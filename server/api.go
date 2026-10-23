package main

// HTTP API: аккаунты, облачные сохранения, PvP (подбор соперника, проверенная атака), журнал обороны, лидеры.

import (
	"crypto/rand"
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"encoding/json"
	"errors"
	mrand "math/rand/v2"
	"net/http"
	"strconv"
	"strings"
	"time"
	"unicode/utf8"
)

type Server struct {
	store *Store
	hub   *Hub
	now   func() time.Time
}

const (
	shieldAfterDefense = 10 * time.Minute // после налёта база какое-то время под щитом
	matchTTL           = 10 * time.Minute // сколько живёт выданный соперник
	raidLootFrac       = 0.2              // доля запасов, доступная для добычи
)

func (s *Server) nowMs() int64 { return s.now().UnixMilli() }

func writeJSON(w http.ResponseWriter, code int, v any) {
	w.Header().Set("Content-Type", "application/json; charset=utf-8")
	w.WriteHeader(code)
	_ = json.NewEncoder(w).Encode(v)
}

func fail(w http.ResponseWriter, code int, msg string) { writeJSON(w, code, map[string]string{"error": msg}) }

func readJSON(r *http.Request, v any) error {
	r.Body = http.MaxBytesReader(nil, r.Body, 2<<20)
	return json.NewDecoder(r.Body).Decode(v)
}

func hashToken(t string) string {
	h := sha256.Sum256([]byte(t))
	return hex.EncodeToString(h[:])
}

func newToken() string {
	b := make([]byte, 32)
	_, _ = rand.Read(b)
	return hex.EncodeToString(b)
}

// auth достаёт игрока по токену из заголовка Authorization: Bearer <token> (или ?token= для WebSocket).
func (s *Server) auth(r *http.Request) (*Player, error) {
	tok := strings.TrimPrefix(r.Header.Get("Authorization"), "Bearer ")
	if tok == "" {
		tok = r.URL.Query().Get("token")
	}
	if tok == "" {
		return nil, errors.New("no token")
	}
	return s.store.PlayerByToken(hashToken(tok))
}

func (s *Server) authed(h func(http.ResponseWriter, *http.Request, *Player)) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		p, err := s.auth(r)
		if err != nil {
			fail(w, http.StatusUnauthorized, "unauthorized")
			return
		}
		h(w, r, p)
	}
}

func cleanName(n string) string {
	n = strings.TrimSpace(n)
	if utf8.RuneCountInString(n) > 16 {
		n = string([]rune(n)[:16])
	}
	if n == "" {
		n = "Фонарщик"
	}
	return filterText(n)
}

func (s *Server) Routes() *http.ServeMux {
	m := http.NewServeMux()
	m.HandleFunc("POST /v1/register", s.register)
	m.HandleFunc("GET /v1/me", s.authed(s.me))
	m.HandleFunc("PUT /v1/save", s.authed(s.putSave))
	m.HandleFunc("POST /v1/match", s.authed(s.match))
	m.HandleFunc("POST /v1/attack", s.authed(s.attack))
	m.HandleFunc("GET /v1/defenses", s.authed(s.defenses))
	m.HandleFunc("GET /v1/leaderboard", s.leaderboard)
	m.HandleFunc("POST /v1/clans", s.authed(s.createClan))
	m.HandleFunc("GET /v1/clans", s.authed(s.listClans))
	m.HandleFunc("GET /v1/clans/{id}", s.authed(s.clanInfo))
	m.HandleFunc("POST /v1/clans/{id}/join", s.authed(s.joinClan))
	m.HandleFunc("POST /v1/clans/leave", s.authed(s.leaveClan))
	m.HandleFunc("GET /v1/chat/{channel}", s.authed(s.chatHistory))
	m.HandleFunc("POST /v1/report", s.authed(s.report))
	m.HandleFunc("GET /v1/ws", s.ws)
	m.HandleFunc("GET /healthz", func(w http.ResponseWriter, r *http.Request) { w.Write([]byte("ok")) })
	return m
}

// ---------- аккаунты и сохранения ----------

func (s *Server) register(w http.ResponseWriter, r *http.Request) {
	var req struct {
		Name string `json:"name"`
	}
	_ = readJSON(r, &req)
	tok := newToken()
	now := s.nowMs()
	save := NewVillage(now)
	res, err := s.store.db.Exec(`INSERT INTO players(name, token_hash, save, created, seen) VALUES(?, ?, ?, ?, ?)`,
		cleanName(req.Name), hashToken(tok), save, now, now)
	if err != nil {
		fail(w, 500, "db")
		return
	}
	id, _ := res.LastInsertId()
	_ = s.store.SetSave(s.store.db, id, save, now)
	writeJSON(w, 200, map[string]any{"id": id, "token": tok, "save": save})
}

func (s *Server) me(w http.ResponseWriter, r *http.Request, p *Player) {
	save, err := TickSave(p.Save, s.nowMs())
	if err != nil {
		fail(w, 500, "bad save")
		return
	}
	writeJSON(w, 200, map[string]any{"id": p.ID, "name": p.Name, "save": save, "rating": p.Rating, "level": p.Level,
		"clan": p.ClanID.Int64, "clan_role": p.ClanRole})
}

// putSave — облачное сохранение. Этап 1: экономика считается на телефоне, сервер проверяет формат
// и не даёт подменить то, что считает сам (рейтинг меняется только боями на сервере).
func (s *Server) putSave(w http.ResponseWriter, r *http.Request, p *Player) {
	var req struct {
		Save string `json:"save"`
	}
	if err := readJSON(r, &req); err != nil {
		fail(w, 400, "bad json")
		return
	}
	now := s.nowMs()
	save, err := TickSave(req.Save, now)
	if err != nil {
		fail(w, 400, "bad save")
		return
	}
	// пока игрок был в игре, на него могли напасть: списываем потери из присланного сохранения
	var debtOre, debtAether int64
	_ = s.store.db.QueryRow(`SELECT debt_ore, debt_aether FROM players WHERE id = ?`, p.ID).Scan(&debtOre, &debtAether)
	if debtOre > 0 || debtAether > 0 {
		if adj, _, err := ApplyDefense(save, debtOre, debtAether, 0, false); err == nil {
			save = adj
		}
	}
	sum, _ := Summary(save)
	if sum["rating"] != p.Rating {
		save = strings.Replace(save, "\nrating "+itoa(sum["rating"])+"\n", "\nrating "+itoa(p.Rating)+"\n", 1)
	}
	if err := s.store.SetSave(s.store.db, p.ID, save, now); err != nil {
		fail(w, 500, "db")
		return
	}
	_, _ = s.store.db.Exec(`UPDATE players SET debt_ore = 0, debt_aether = 0 WHERE id = ?`, p.ID)
	writeJSON(w, 200, map[string]any{"ok": true, "save": save, "rating": p.Rating, "debt_ore": debtOre, "debt_aether": debtAether})
}

func (s *Server) match(w http.ResponseWriter, r *http.Request, p *Player) {
	now := s.nowMs()
	var def *Player
	for _, spread := range []int64{150, 400, 1000, 1 << 30} {
		row := s.store.db.QueryRow(`SELECT `+playerCols+` FROM players
			WHERE id != ? AND shield_until < ? AND rating BETWEEN ? AND ?
			ORDER BY abs(rating - ?) + abs(random() % 60) LIMIT 1`, p.ID, now, p.Rating-spread, p.Rating+spread, p.Rating)
		if d, err := scanPlayer(row); err == nil {
			def = d
			break
		}
	}
	if def == nil {
		fail(w, 404, "no opponents yet")
		return
	}
	save, err := TickSave(def.Save, now)
	if err != nil {
		fail(w, 500, "bad defender save")
		return
	}
	snap, err := Snapshot(save, raidLootFrac)
	if err != nil {
		fail(w, 500, "snapshot")
		return
	}
	seed := int(mrand.Int32())
	res, err := s.store.db.Exec(`INSERT INTO matches(attacker, defender, snapshot, seed, created) VALUES(?, ?, ?, ?, ?)`, p.ID, def.ID, snap, seed, now)
	if err != nil {
		fail(w, 500, "db")
		return
	}
	id, _ := res.LastInsertId()
	writeJSON(w, 200, map[string]any{"match": id, "seed": seed, "snapshot": snap,
		"defender": map[string]any{"id": def.ID, "name": def.Name, "rating": def.Rating, "level": def.Level, "lantern": def.Lantern}})
}

// attack принимает запись боя, прогоняет её ядром и применяет итог обоим игрокам. Подделать итог нельзя:
// сервер сам считает звёзды и добычу.
func (s *Server) attack(w http.ResponseWriter, r *http.Request, p *Player) {
	var req struct {
		Match  int64  `json:"match"`
		Replay string `json:"replay"`
	}
	if err := readJSON(r, &req); err != nil {
		fail(w, 400, "bad json")
		return
	}
	now := s.nowMs()
	tx, err := s.store.db.Begin()
	if err != nil {
		fail(w, 500, "db")
		return
	}
	defer tx.Rollback()
	var defID, created, used int64
	var snap string
	var seed int
	err = tx.QueryRow(`SELECT defender, snapshot, seed, created, used FROM matches WHERE id = ? AND attacker = ?`, req.Match, p.ID).
		Scan(&defID, &snap, &seed, &created, &used)
	if err == sql.ErrNoRows {
		fail(w, 404, "no such match")
		return
	} else if err != nil {
		fail(w, 500, "db")
		return
	}
	if used != 0 || now-created > matchTTL.Milliseconds()+int64(4*time.Minute/time.Millisecond) {
		fail(w, 409, "match expired")
		return
	}
	var atkSave, defSave string
	if err := tx.QueryRow(`SELECT save FROM players WHERE id = ?`, p.ID).Scan(&atkSave); err != nil {
		fail(w, 500, "db")
		return
	}
	if err := tx.QueryRow(`SELECT save FROM players WHERE id = ?`, defID).Scan(&defSave); err != nil {
		fail(w, 500, "db")
		return
	}
	result, newAtk, err := VerifyAttack(atkSave, snap, req.Replay, seed, now)
	if err != nil {
		fail(w, 422, "rejected: "+err.Error())
		return
	}
	newDef, defDelta, err := ApplyDefense(defSave, result["ore"], result["aether"], int(result["stars"]), result["win"] == 1)
	if err != nil {
		fail(w, 500, "defender")
		return
	}
	if err := s.store.SetSave(tx, p.ID, newAtk, now); err != nil {
		fail(w, 500, "db")
		return
	}
	if err := s.store.SetSave(tx, defID, newDef, now); err != nil {
		fail(w, 500, "db")
		return
	}
	if _, err := tx.Exec(`UPDATE players SET shield_until = ?, debt_ore = debt_ore + ?, debt_aether = debt_aether + ? WHERE id = ?`,
		now+shieldAfterDefense.Milliseconds(), result["ore"], result["aether"], defID); err != nil {
		fail(w, 500, "db")
		return
	}
	if _, err := tx.Exec(`UPDATE matches SET used = 1 WHERE id = ?`, req.Match); err != nil {
		fail(w, 500, "db")
		return
	}
	if _, err := tx.Exec(`INSERT INTO battles(attacker, defender, stars, pct, ore, aether, attacker_rating, defender_rating, replay, snapshot, created)
		VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`, p.ID, defID, result["stars"], result["pct"], result["ore"], result["aether"],
		result["rating"], defDelta, req.Replay, snap, now); err != nil {
		fail(w, 500, "db")
		return
	}
	if err := tx.Commit(); err != nil {
		fail(w, 500, "db")
		return
	}
	writeJSON(w, 200, map[string]any{"result": result, "save": newAtk, "defender_rating": defDelta})
}

func (s *Server) defenses(w http.ResponseWriter, r *http.Request, p *Player) {
	rows, err := s.store.db.Query(`SELECT b.id, b.attacker, pl.name, b.stars, b.pct, b.ore, b.aether, b.defender_rating, b.created
		FROM battles b JOIN players pl ON pl.id = b.attacker WHERE b.defender = ? ORDER BY b.created DESC LIMIT 20`, p.ID)
	if err != nil {
		fail(w, 500, "db")
		return
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var id, atk, stars, pct, ore, ae, dr, created int64
		var name string
		if rows.Scan(&id, &atk, &name, &stars, &pct, &ore, &ae, &dr, &created) == nil {
			out = append(out, map[string]any{"battle": id, "attacker": atk, "name": name, "stars": stars, "pct": pct,
				"ore": ore, "aether": ae, "rating": dr, "at": created})
		}
	}
	writeJSON(w, 200, out)
}

func (s *Server) leaderboard(w http.ResponseWriter, r *http.Request) {
	rows, err := s.store.db.Query(`SELECT p.id, p.name, p.rating, p.level, coalesce(c.name, '') FROM players p
		LEFT JOIN clans c ON c.id = p.clan_id ORDER BY p.rating DESC, p.id LIMIT 50`)
	if err != nil {
		fail(w, 500, "db")
		return
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var id, rating, level int64
		var name, clan string
		if rows.Scan(&id, &name, &rating, &level, &clan) == nil {
			out = append(out, map[string]any{"id": id, "name": name, "rating": rating, "level": level, "clan": clan})
		}
	}
	writeJSON(w, 200, out)
}

func itoa(v int64) string { return strconv.FormatInt(v, 10) }
