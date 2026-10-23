package main

// Хранилище: SQLite (чистый Go). Одна запись на игрока, матчи, бои, кланы, чат.

import (
	"database/sql"
	"strconv"
	"strings"

	_ "modernc.org/sqlite"
)

const schema = `
CREATE TABLE IF NOT EXISTS players (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  token_hash TEXT NOT NULL UNIQUE,
  save TEXT NOT NULL,
  level INTEGER NOT NULL DEFAULT 1,
  rating INTEGER NOT NULL DEFAULT 0,
  lantern INTEGER NOT NULL DEFAULT 1,
  clan_id INTEGER,
  clan_role TEXT NOT NULL DEFAULT '',
  shield_until INTEGER NOT NULL DEFAULT 0,
  debt_ore INTEGER NOT NULL DEFAULT 0,
  debt_aether INTEGER NOT NULL DEFAULT 0,
  muted_until INTEGER NOT NULL DEFAULT 0,
  created INTEGER NOT NULL,
  seen INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS players_rating ON players(rating);
CREATE TABLE IF NOT EXISTS matches (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  attacker INTEGER NOT NULL,
  defender INTEGER NOT NULL,
  snapshot TEXT NOT NULL,
  seed INTEGER NOT NULL,
  created INTEGER NOT NULL,
  used INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS battles (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  attacker INTEGER NOT NULL,
  defender INTEGER NOT NULL,
  stars INTEGER NOT NULL,
  pct INTEGER NOT NULL,
  ore INTEGER NOT NULL,
  aether INTEGER NOT NULL,
  attacker_rating INTEGER NOT NULL,
  defender_rating INTEGER NOT NULL,
  replay TEXT NOT NULL,
  snapshot TEXT NOT NULL,
  created INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS battles_defender ON battles(defender, created);
CREATE TABLE IF NOT EXISTS clans (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  name TEXT NOT NULL,
  name_key TEXT NOT NULL UNIQUE,
  description TEXT NOT NULL DEFAULT '',
  leader INTEGER NOT NULL,
  min_rating INTEGER NOT NULL DEFAULT 0,
  created INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS messages (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  channel TEXT NOT NULL,
  player INTEGER NOT NULL,
  name TEXT NOT NULL,
  text TEXT NOT NULL,
  created INTEGER NOT NULL,
  hidden INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS messages_channel ON messages(channel, id);
CREATE TABLE IF NOT EXISTS reports (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  message INTEGER NOT NULL,
  reporter INTEGER NOT NULL,
  created INTEGER NOT NULL,
  UNIQUE(message, reporter)
);
`

type Store struct{ db *sql.DB }

func OpenStore(path string) (*Store, error) {
	db, err := sql.Open("sqlite", path+"?_pragma=busy_timeout(5000)&_pragma=journal_mode(WAL)&_pragma=foreign_keys(1)")
	if err != nil {
		return nil, err
	}
	db.SetMaxOpenConns(1) // SQLite: один писатель; для старта этого хватает
	if _, err := db.Exec(schema); err != nil {
		return nil, err
	}
	return &Store{db}, nil
}

type Player struct {
	ID, Level, Rating, Lantern int64
	Name, Save, ClanRole       string
	ClanID                     sql.NullInt64
	ShieldUntil, MutedUntil    int64
}

const playerCols = `id, name, save, level, rating, lantern, clan_id, clan_role, shield_until, muted_until`

func scanPlayer(row interface{ Scan(...any) error }) (*Player, error) {
	p := &Player{}
	err := row.Scan(&p.ID, &p.Name, &p.Save, &p.Level, &p.Rating, &p.Lantern, &p.ClanID, &p.ClanRole, &p.ShieldUntil, &p.MutedUntil)
	if err != nil {
		return nil, err
	}
	return p, nil
}

func (s *Store) PlayerByToken(hash string) (*Player, error) {
	return scanPlayer(s.db.QueryRow(`SELECT `+playerCols+` FROM players WHERE token_hash = ?`, hash))
}

func (s *Store) PlayerByID(id int64) (*Player, error) {
	return scanPlayer(s.db.QueryRow(`SELECT `+playerCols+` FROM players WHERE id = ?`, id))
}

// SetSave сохраняет деревню и обновляет столбцы для поиска (уровень, рейтинг, Фонарь).
func (s *Store) SetSave(tx execer, id int64, save string, now int64) error {
	sum, err := Summary(save)
	if err != nil {
		return err
	}
	_, err = tx.Exec(`UPDATE players SET save = ?, level = ?, rating = ?, lantern = ?, seen = ? WHERE id = ?`,
		save, sum["level"], sum["rating"], sum["lantern"], now, id)
	return err
}

type execer interface {
	Exec(query string, args ...any) (sql.Result, error)
}

func parseKV(text string) map[string]int64 {
	out := map[string]int64{}
	for _, line := range strings.Split(text, "\n") {
		f := strings.Fields(line)
		if len(f) == 2 {
			v, _ := strconv.ParseInt(f[1], 10, 64)
			out[f[0]] = v
		}
	}
	return out
}
