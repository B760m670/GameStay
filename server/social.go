package main

// Кланы и чат: глобальный канал и канал своего клана через WebSocket, история, жалобы, фильтр, ограничение частоты.

import (
	"context"
	"database/sql"
	"encoding/json"
	"net/http"
	"regexp"
	"strconv"
	"strings"
	"sync"
	"time"
	"unicode/utf8"

	"github.com/coder/websocket"
)

const (
	maxMessageRunes = 200
	messageEvery    = 1500 * time.Millisecond // не чаще сообщения в полторы секунды
	reportsToHide   = 3                       // столько жалоб — и сообщение скрыто
	clanMax         = 30
)

// ---------- фильтр слов ----------

var badWords = regexp.MustCompile(`(?i)(х[уy][йияеёю]\p{L}*|п[иi][зz]д\p{L}*|бл[яa]д\p{L}*|бля\b|еб[аaуoлн]\p{L}*|ёб\p{L}*|сук[аи]\b|муд[аo]к\p{L}*|f+u+c+k+\w*|sh[i1]t\w*|bitch\w*)`)

func filterText(s string) string {
	return badWords.ReplaceAllStringFunc(s, func(w string) string { return strings.Repeat("*", utf8.RuneCountInString(w)) })
}

// ---------- кланы ----------

func (s *Server) createClan(w http.ResponseWriter, r *http.Request, p *Player) {
	var req struct {
		Name        string `json:"name"`
		Description string `json:"description"`
		MinRating   int64  `json:"min_rating"`
	}
	if err := readJSON(r, &req); err != nil {
		fail(w, 400, "bad json")
		return
	}
	name := strings.TrimSpace(req.Name)
	if n := utf8.RuneCountInString(name); n < 3 || n > 20 {
		fail(w, 400, "name must be 3-20 characters")
		return
	}
	if filterText(name) != name {
		fail(w, 400, "name not allowed")
		return
	}
	if p.ClanID.Valid {
		fail(w, 409, "already in a clan")
		return
	}
	desc := filterText(req.Description)
	if utf8.RuneCountInString(desc) > 160 {
		desc = string([]rune(desc)[:160])
	}
	tx, err := s.store.db.Begin()
	if err != nil {
		fail(w, 500, "db")
		return
	}
	defer tx.Rollback()
	res, err := tx.Exec(`INSERT INTO clans(name, name_key, description, leader, min_rating, created) VALUES(?, ?, ?, ?, ?, ?)`,
		name, strings.ToLower(strings.Join(strings.Fields(name), " ")), desc, p.ID, req.MinRating, s.nowMs())
	if err != nil {
		fail(w, 409, "name taken")
		return
	}
	id, _ := res.LastInsertId()
	if _, err := tx.Exec(`UPDATE players SET clan_id = ?, clan_role = 'leader' WHERE id = ?`, id, p.ID); err != nil {
		fail(w, 500, "db")
		return
	}
	if err := tx.Commit(); err != nil {
		fail(w, 500, "db")
		return
	}
	writeJSON(w, 200, map[string]any{"clan": id})
}

func (s *Server) listClans(w http.ResponseWriter, r *http.Request, p *Player) {
	q := "%" + strings.TrimSpace(r.URL.Query().Get("q")) + "%"
	rows, err := s.store.db.Query(`SELECT c.id, c.name, c.description, c.min_rating, count(pl.id), coalesce(sum(pl.rating), 0)
		FROM clans c LEFT JOIN players pl ON pl.clan_id = c.id WHERE c.name LIKE ? GROUP BY c.id ORDER BY 6 DESC LIMIT 30`, q)
	if err != nil {
		fail(w, 500, "db")
		return
	}
	defer rows.Close()
	out := []map[string]any{}
	for rows.Next() {
		var id, minR, members, total int64
		var name, desc string
		if rows.Scan(&id, &name, &desc, &minR, &members, &total) == nil {
			out = append(out, map[string]any{"id": id, "name": name, "description": desc, "min_rating": minR, "members": members, "rating": total})
		}
	}
	writeJSON(w, 200, out)
}

func (s *Server) clanInfo(w http.ResponseWriter, r *http.Request, p *Player) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	var name, desc string
	var leader, minR int64
	if err := s.store.db.QueryRow(`SELECT name, description, leader, min_rating FROM clans WHERE id = ?`, id).Scan(&name, &desc, &leader, &minR); err != nil {
		fail(w, 404, "no such clan")
		return
	}
	rows, err := s.store.db.Query(`SELECT id, name, rating, level, clan_role FROM players WHERE clan_id = ? ORDER BY rating DESC`, id)
	if err != nil {
		fail(w, 500, "db")
		return
	}
	defer rows.Close()
	members := []map[string]any{}
	for rows.Next() {
		var pid, rating, level int64
		var pname, role string
		if rows.Scan(&pid, &pname, &rating, &level, &role) == nil {
			members = append(members, map[string]any{"id": pid, "name": pname, "rating": rating, "level": level, "role": role})
		}
	}
	writeJSON(w, 200, map[string]any{"id": id, "name": name, "description": desc, "leader": leader, "min_rating": minR, "members": members})
}

func (s *Server) joinClan(w http.ResponseWriter, r *http.Request, p *Player) {
	id, _ := strconv.ParseInt(r.PathValue("id"), 10, 64)
	if p.ClanID.Valid {
		fail(w, 409, "already in a clan")
		return
	}
	var minR, members int64
	if err := s.store.db.QueryRow(`SELECT min_rating, (SELECT count(*) FROM players WHERE clan_id = clans.id) FROM clans WHERE id = ?`, id).Scan(&minR, &members); err != nil {
		fail(w, 404, "no such clan")
		return
	}
	if p.Rating < minR {
		fail(w, 403, "rating too low")
		return
	}
	if members >= clanMax {
		fail(w, 409, "clan is full")
		return
	}
	if _, err := s.store.db.Exec(`UPDATE players SET clan_id = ?, clan_role = 'member' WHERE id = ?`, id, p.ID); err != nil {
		fail(w, 500, "db")
		return
	}
	s.hub.system("clan:"+strconv.FormatInt(id, 10), p.Name+" вступает в клан")
	writeJSON(w, 200, map[string]any{"ok": true})
}

func (s *Server) leaveClan(w http.ResponseWriter, r *http.Request, p *Player) {
	if !p.ClanID.Valid {
		fail(w, 409, "not in a clan")
		return
	}
	cid := p.ClanID.Int64
	tx, err := s.store.db.Begin()
	if err != nil {
		fail(w, 500, "db")
		return
	}
	defer tx.Rollback()
	if _, err := tx.Exec(`UPDATE players SET clan_id = NULL, clan_role = '' WHERE id = ?`, p.ID); err != nil {
		fail(w, 500, "db")
		return
	}
	if p.ClanRole == "leader" {
		// лидерство переходит к самому сильному; пустой клан распускается
		var next int64
		err := tx.QueryRow(`SELECT id FROM players WHERE clan_id = ? ORDER BY rating DESC LIMIT 1`, cid).Scan(&next)
		if err == sql.ErrNoRows {
			_, _ = tx.Exec(`DELETE FROM clans WHERE id = ?`, cid)
		} else if err == nil {
			_, _ = tx.Exec(`UPDATE players SET clan_role = 'leader' WHERE id = ?`, next)
			_, _ = tx.Exec(`UPDATE clans SET leader = ? WHERE id = ?`, next, cid)
		}
	}
	if err := tx.Commit(); err != nil {
		fail(w, 500, "db")
		return
	}
	s.hub.system("clan:"+strconv.FormatInt(cid, 10), p.Name+" покидает клан")
	writeJSON(w, 200, map[string]any{"ok": true})
}

// ---------- чат ----------

type ChatMsg struct {
	Type    string `json:"type"`
	ID      int64  `json:"id,omitempty"`
	Channel string `json:"channel"`
	From    int64  `json:"from,omitempty"`
	Name    string `json:"name,omitempty"`
	Text    string `json:"text"`
	At      int64  `json:"at"`
}

type client struct {
	id     int64
	name   string
	clan   string // "clan:<id>" или ""
	send   chan ChatMsg
	lastAt time.Time
}

type Hub struct {
	mu      sync.Mutex
	clients map[*client]bool
	store   *Store
	now     func() time.Time
}

func NewHub(st *Store, now func() time.Time) *Hub {
	return &Hub{clients: map[*client]bool{}, store: st, now: now}
}

func (h *Hub) broadcast(m ChatMsg) {
	h.mu.Lock()
	defer h.mu.Unlock()
	for c := range h.clients {
		if m.Channel == "global" || m.Channel == c.clan {
			select {
			case c.send <- m:
			default: // медленный клиент — пропускает сообщение, но не тормозит остальных
			}
		}
	}
}

func (h *Hub) system(channel, text string) {
	h.broadcast(ChatMsg{Type: "system", Channel: channel, Text: text, At: h.now().UnixMilli()})
}

func (h *Hub) save(c *client, channel, text string) (ChatMsg, error) {
	now := h.now().UnixMilli()
	res, err := h.store.db.Exec(`INSERT INTO messages(channel, player, name, text, created) VALUES(?, ?, ?, ?, ?)`, channel, c.id, c.name, text, now)
	if err != nil {
		return ChatMsg{}, err
	}
	id, _ := res.LastInsertId()
	return ChatMsg{Type: "msg", ID: id, Channel: channel, From: c.id, Name: c.name, Text: text, At: now}, nil
}

func (s *Server) history(channel string, limit int) []ChatMsg {
	rows, err := s.store.db.Query(`SELECT id, player, name, text, created FROM messages WHERE channel = ? AND hidden = 0 ORDER BY id DESC LIMIT ?`, channel, limit)
	if err != nil {
		return nil
	}
	defer rows.Close()
	var out []ChatMsg
	for rows.Next() {
		m := ChatMsg{Type: "msg", Channel: channel}
		if rows.Scan(&m.ID, &m.From, &m.Name, &m.Text, &m.At) == nil {
			out = append(out, m)
		}
	}
	for i, j := 0, len(out)-1; i < j; i, j = i+1, j-1 { // старые сверху
		out[i], out[j] = out[j], out[i]
	}
	return out
}

func (s *Server) chatHistory(w http.ResponseWriter, r *http.Request, p *Player) {
	ch := r.PathValue("channel")
	if ch == "clan" {
		if !p.ClanID.Valid {
			fail(w, 403, "not in a clan")
			return
		}
		ch = "clan:" + strconv.FormatInt(p.ClanID.Int64, 10)
	} else if ch != "global" {
		fail(w, 404, "no such channel")
		return
	}
	writeJSON(w, 200, s.history(ch, 50))
}

func (s *Server) report(w http.ResponseWriter, r *http.Request, p *Player) {
	var req struct {
		Message int64 `json:"message"`
	}
	if err := readJSON(r, &req); err != nil {
		fail(w, 400, "bad json")
		return
	}
	if _, err := s.store.db.Exec(`INSERT OR IGNORE INTO reports(message, reporter, created) VALUES(?, ?, ?)`, req.Message, p.ID, s.nowMs()); err != nil {
		fail(w, 500, "db")
		return
	}
	var n int64
	_ = s.store.db.QueryRow(`SELECT count(*) FROM reports WHERE message = ?`, req.Message).Scan(&n)
	if n >= reportsToHide {
		_, _ = s.store.db.Exec(`UPDATE messages SET hidden = 1 WHERE id = ?`, req.Message)
	}
	writeJSON(w, 200, map[string]any{"ok": true, "reports": n})
}

// ws — подключение к чату: ?token=... ; клиент шлёт {"type":"say","channel":"global"|"clan","text":"..."}.
func (s *Server) ws(w http.ResponseWriter, r *http.Request) {
	p, err := s.auth(r)
	if err != nil {
		fail(w, http.StatusUnauthorized, "unauthorized")
		return
	}
	conn, err := websocket.Accept(w, r, &websocket.AcceptOptions{InsecureSkipVerify: true})
	if err != nil {
		return
	}
	defer conn.CloseNow()
	conn.SetReadLimit(4096)
	c := &client{id: p.ID, name: p.Name, send: make(chan ChatMsg, 64)}
	if p.ClanID.Valid {
		c.clan = "clan:" + strconv.FormatInt(p.ClanID.Int64, 10)
	}
	s.hub.mu.Lock()
	s.hub.clients[c] = true
	s.hub.mu.Unlock()
	defer func() {
		s.hub.mu.Lock()
		delete(s.hub.clients, c)
		s.hub.mu.Unlock()
	}()
	ctx, cancel := context.WithCancel(r.Context())
	defer cancel()
	// история при входе
	for _, m := range s.history("global", 30) {
		c.send <- m
	}
	if c.clan != "" {
		for _, m := range s.history(c.clan, 30) {
			c.send <- m
		}
	}
	go func() {
		for {
			select {
			case <-ctx.Done():
				return
			case m := <-c.send:
				b, _ := json.Marshal(m)
				wctx, wc := context.WithTimeout(ctx, 5*time.Second)
				err := conn.Write(wctx, websocket.MessageText, b)
				wc()
				if err != nil {
					cancel()
					return
				}
			}
		}
	}()
	for {
		_, data, err := conn.Read(ctx)
		if err != nil {
			return
		}
		var in struct {
			Type, Channel, Text string
		}
		if json.Unmarshal(data, &in) != nil || in.Type != "say" {
			continue
		}
		reply := func(text string) { c.send <- ChatMsg{Type: "error", Channel: in.Channel, Text: text, At: s.nowMs()} }
		ch := "global"
		if in.Channel == "clan" {
			if c.clan == "" {
				reply("Ты не в клане")
				continue
			}
			ch = c.clan
		}
		var muted int64
		_ = s.store.db.QueryRow(`SELECT muted_until FROM players WHERE id = ?`, c.id).Scan(&muted)
		if muted > s.nowMs() {
			reply("Чат временно недоступен")
			continue
		}
		if now := s.now(); now.Sub(c.lastAt) < messageEvery {
			reply("Слишком часто")
			continue
		} else {
			c.lastAt = now
		}
		text := strings.TrimSpace(in.Text)
		if text == "" {
			continue
		}
		if utf8.RuneCountInString(text) > maxMessageRunes {
			text = string([]rune(text)[:maxMessageRunes])
		}
		m, err := s.hub.save(c, ch, filterText(text))
		if err != nil {
			reply("Ошибка сервера")
			continue
		}
		s.hub.broadcast(m)
	}
}
