package main

import (
	"bytes"
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"path/filepath"
	"strings"
	"testing"
	"time"

	"github.com/coder/websocket"
)

type env struct {
	t   *testing.T
	srv *Server
	ts  *httptest.Server
}

func newEnv(t *testing.T) *env {
	st, err := OpenStore(filepath.Join(t.TempDir(), "test.db"))
	if err != nil {
		t.Fatal(err)
	}
	srv := &Server{store: st, now: time.Now}
	srv.hub = NewHub(st, srv.now)
	ts := httptest.NewServer(srv.Routes())
	t.Cleanup(ts.Close)
	return &env{t, srv, ts}
}

func (e *env) do(method, path, token string, body any, out any) int {
	e.t.Helper()
	var rd *bytes.Reader
	if body != nil {
		b, _ := json.Marshal(body)
		rd = bytes.NewReader(b)
	} else {
		rd = bytes.NewReader(nil)
	}
	req, _ := http.NewRequest(method, e.ts.URL+path, rd)
	if token != "" {
		req.Header.Set("Authorization", "Bearer "+token)
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		e.t.Fatal(err)
	}
	defer resp.Body.Close()
	if out != nil {
		_ = json.NewDecoder(resp.Body).Decode(out)
	}
	return resp.StatusCode
}

type account struct {
	ID    int64  `json:"id"`
	Token string `json:"token"`
	Save  string `json:"save"`
}

func (e *env) register(name string) account {
	var a account
	if c := e.do("POST", "/v1/register", "", map[string]string{"name": name}, &a); c != 200 || a.Token == "" {
		e.t.Fatalf("register: %d", c)
	}
	return a
}

type matchResp struct {
	Match    int64  `json:"match"`
	Seed     int    `json:"seed"`
	Snapshot string `json:"snapshot"`
	Defender struct {
		ID int64 `json:"id"`
	} `json:"defender"`
}

func TestRegisterAndCloudSave(t *testing.T) {
	e := newEnv(t)
	a := e.register("Ярена")
	var me map[string]any
	if c := e.do("GET", "/v1/me", a.Token, nil, &me); c != 200 || me["name"] != "Ярена" || me["save"] == "" {
		t.Fatalf("me: %d %v", c, me)
	}
	if c := e.do("GET", "/v1/me", "wrong", nil, nil); c != 401 {
		t.Fatalf("bad token should be 401, got %d", c)
	}
	// облачное сохранение нельзя использовать, чтобы накрутить рейтинг
	forged := strings.Replace(a.Save, "\nrating 0\n", "\nrating 5000\n", 1)
	var put map[string]any
	if c := e.do("PUT", "/v1/save", a.Token, map[string]string{"save": forged}, &put); c != 200 {
		t.Fatalf("put: %d", c)
	}
	var me2 map[string]any
	e.do("GET", "/v1/me", a.Token, nil, &me2)
	if me2["rating"].(float64) != 0 {
		t.Fatalf("rating must stay server-side, got %v", me2["rating"])
	}
	if c := e.do("PUT", "/v1/save", a.Token, map[string]string{"save": "garbage"}, nil); c != 400 {
		t.Fatalf("garbage save should be rejected, got %d", c)
	}
}

func TestVerifiedAttack(t *testing.T) {
	e := newEnv(t)
	a := e.register("Атакующий")
	b := e.register("Защитник")
	var m matchResp
	if c := e.do("POST", "/v1/match", a.Token, nil, &m); c != 200 || m.Defender.ID != b.ID || m.Snapshot == "" {
		t.Fatalf("match: %d %+v", c, m)
	}
	now := time.Now().UnixMilli()
	replay := AutoReplay(a.Save, m.Snapshot, m.Seed, now)
	if replay == "" {
		t.Fatal("no replay")
	}
	// подделки отклоняются
	forged := strings.Replace(replay, "\narmy 0 10\n", "\narmy 0 999\n", 1)
	if c := e.do("POST", "/v1/attack", a.Token, map[string]any{"match": m.Match, "replay": forged}, nil); c != 422 {
		t.Fatalf("forged army must be rejected, got %d", c)
	}
	wrongSeed := strings.Replace(replay, "opts 0 ", "opts 0 1", 1)
	if c := e.do("POST", "/v1/attack", a.Token, map[string]any{"match": m.Match, "replay": wrongSeed}, nil); c != 422 {
		t.Fatalf("seed mismatch must be rejected, got %d", c)
	}
	// честная атака проходит, итог считает сервер
	var res struct {
		Result map[string]int64 `json:"result"`
		Save   string           `json:"save"`
	}
	if c := e.do("POST", "/v1/attack", a.Token, map[string]any{"match": m.Match, "replay": replay}, &res); c != 200 {
		t.Fatalf("attack: %d", c)
	}
	if res.Result["pct"] < 0 || res.Save == "" {
		t.Fatalf("result: %+v", res)
	}
	if !strings.Contains(res.Save, "\narmy 0 0") {
		t.Fatalf("used troops must be spent:\n%s", res.Save)
	}
	// повторно тот же матч не засчитывается
	if c := e.do("POST", "/v1/attack", a.Token, map[string]any{"match": m.Match, "replay": replay}, nil); c != 409 {
		t.Fatalf("reused match must be 409, got %d", c)
	}
	// у защитника — запись в журнале обороны и щит
	var log []map[string]any
	e.do("GET", "/v1/defenses", b.Token, nil, &log)
	if len(log) != 1 || log[0]["name"] != "Атакующий" {
		t.Fatalf("defense log: %v", log)
	}
	if c := e.do("POST", "/v1/match", a.Token, nil, nil); c != 404 {
		t.Fatalf("defender under shield must not be matched, got %d", c)
	}
	var lb []map[string]any
	e.do("GET", "/v1/leaderboard", "", nil, &lb)
	if len(lb) != 2 {
		t.Fatalf("leaderboard: %v", lb)
	}
}

func TestClans(t *testing.T) {
	e := newEnv(t)
	a := e.register("Лидер")
	b := e.register("Боец")
	var cl map[string]int64
	if c := e.do("POST", "/v1/clans", a.Token, map[string]any{"name": "Орден Света", "description": "Свет не гаснет"}, &cl); c != 200 {
		t.Fatalf("create: %d", c)
	}
	if c := e.do("POST", "/v1/clans", b.Token, map[string]any{"name": "орден света"}, nil); c != 409 {
		t.Fatalf("duplicate name must be 409, got %d", c)
	}
	id := cl["clan"]
	if c := e.do("POST", "/v1/clans/"+itoa(id)+"/join", b.Token, nil, nil); c != 200 {
		t.Fatalf("join: %d", c)
	}
	var info struct {
		Members []map[string]any `json:"members"`
	}
	e.do("GET", "/v1/clans/"+itoa(id), a.Token, nil, &info)
	if len(info.Members) != 2 {
		t.Fatalf("members: %v", info.Members)
	}
	if c := e.do("POST", "/v1/clans/leave", a.Token, nil, nil); c != 200 {
		t.Fatalf("leave: %d", c)
	}
	e.do("GET", "/v1/clans/"+itoa(id), b.Token, nil, &info)
	if len(info.Members) != 1 || info.Members[0]["role"] != "leader" {
		t.Fatalf("leadership must pass on: %v", info.Members)
	}
}

func TestChat(t *testing.T) {
	e := newEnv(t)
	a := e.register("Аня")
	b := e.register("Боря")
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	wsURL := "ws" + strings.TrimPrefix(e.ts.URL, "http") + "/v1/ws?token="
	ca, _, err := websocket.Dial(ctx, wsURL+a.Token, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer ca.CloseNow()
	cb, _, err := websocket.Dial(ctx, wsURL+b.Token, nil)
	if err != nil {
		t.Fatal(err)
	}
	defer cb.CloseNow()
	time.Sleep(100 * time.Millisecond)
	say, _ := json.Marshal(map[string]string{"type": "say", "channel": "global", "text": "привет, блядь"})
	if err := ca.Write(ctx, websocket.MessageText, say); err != nil {
		t.Fatal(err)
	}
	_, data, err := cb.Read(ctx)
	if err != nil {
		t.Fatal(err)
	}
	var m ChatMsg
	_ = json.Unmarshal(data, &m)
	if m.Name != "Аня" || strings.Contains(m.Text, "бляд") || !strings.HasPrefix(m.Text, "привет") {
		t.Fatalf("message: %+v", m)
	}
	// слишком часто — отказ
	_ = ca.Write(ctx, websocket.MessageText, say)
	_, data, _ = ca.Read(ctx) // своё первое сообщение
	_, data, _ = ca.Read(ctx)
	_ = json.Unmarshal(data, &m)
	if m.Type != "error" {
		t.Fatalf("rate limit expected, got %+v", m)
	}
	// три жалобы скрывают сообщение из истории
	c := e.register("Вова")
	d := e.register("Гена")
	var first ChatMsg
	var hist []ChatMsg
	e.do("GET", "/v1/chat/global", a.Token, nil, &hist)
	if len(hist) != 1 {
		t.Fatalf("history: %v", hist)
	}
	first = hist[0]
	for _, tok := range []string{b.Token, c.Token, d.Token} {
		e.do("POST", "/v1/report", tok, map[string]int64{"message": first.ID}, nil)
	}
	e.do("GET", "/v1/chat/global", a.Token, nil, &hist)
	if len(hist) != 0 {
		t.Fatalf("reported message must be hidden: %v", hist)
	}
}
