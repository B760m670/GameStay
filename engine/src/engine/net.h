#pragma once
// Сеть: асинхронные HTTP-запросы и WebSocket. Ответы и сообщения приходят в поток игры через poll().
#include <functional>
#include <string>

namespace net {

struct Response {
    int status = 0;     // 0 — нет связи
    std::string body;
};

// Запрос к серверу; token — Bearer-токен (может быть пустым). cb вызывается из poll().
void request(const std::string& method, const std::string& url, const std::string& body, const std::string& token,
             std::function<void(const Response&)> cb);

// WebSocket: одно соединение (чат). onMessage — текстовые сообщения, onState — подключено/отключено.
void wsOpen(const std::string& url, std::function<void(const std::string&)> onMessage, std::function<void(bool)> onState);
void wsSend(const std::string& text);
void wsClose();
bool wsConnected();

// Вызывать каждый кадр.
void poll();

// Для платформенного кода (любой поток).
void deliver(int id, int status, std::string body);
void wsDeliver(int kind, std::string text);  // 0 — сообщение, 1 — подключено, 2 — отключено

}  // namespace net
