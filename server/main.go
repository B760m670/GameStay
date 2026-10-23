// Сервер Lanternfall: аккаунты, облачные сохранения, PvP с проверкой боёв ядром игры, кланы, чат.
//
// Сборка: ./build_core.sh && go build -o lanternfall-server .
// Запуск: ./lanternfall-server -addr :8080 -db lanternfall.db
package main

import (
	"flag"
	"log"
	"net/http"
	"time"
)

func main() {
	addr := flag.String("addr", ":8080", "адрес HTTP")
	dbPath := flag.String("db", "lanternfall.db", "файл базы SQLite")
	flag.Parse()
	st, err := OpenStore(*dbPath)
	if err != nil {
		log.Fatal(err)
	}
	srv := &Server{store: st, now: time.Now}
	srv.hub = NewHub(st, srv.now)
	h := &http.Server{Addr: *addr, Handler: srv.Routes(), ReadHeaderTimeout: 10 * time.Second}
	log.Printf("lanternfall server on %s", *addr)
	log.Fatal(h.ListenAndServe())
}
