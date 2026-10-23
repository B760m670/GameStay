// Сеть для Linux (разработка и автотесты): HTTP и WebSocket поверх обычных сокетов, только без TLS (http://, ws://).
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <thread>

#include "../engine/net.h"
#include "platform.h"

namespace platform {
namespace {
struct Url { std::string scheme, host, port, path; };

bool parseUrl(const std::string& u, Url& out) {
    size_t p = u.find("://");
    if (p == std::string::npos) return false;
    out.scheme = u.substr(0, p);
    std::string rest = u.substr(p + 3);
    size_t slash = rest.find('/');
    std::string hostport = rest.substr(0, slash);
    out.path = slash == std::string::npos ? "/" : rest.substr(slash);
    size_t colon = hostport.find(':');
    out.host = hostport.substr(0, colon);
    out.port = colon == std::string::npos ? (out.scheme == "https" || out.scheme == "wss" ? "443" : "80") : hostport.substr(colon + 1);
    return out.scheme == "http" || out.scheme == "ws";  // TLS на Linux не поддерживаем
}

int dial(const Url& u) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(u.host.c_str(), u.port.c_str(), &hints, &res) != 0) return -1;
    int fd = -1;
    for (addrinfo* a = res; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        if (connect(fd, a->ai_addr, a->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

bool sendAll(int fd, const std::string& s) {
    size_t off = 0;
    while (off < s.size()) {
        ssize_t n = ::send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
        if (n <= 0) return false;
        off += n;
    }
    return true;
}

std::string dechunk(const std::string& b) {
    std::string out;
    size_t i = 0;
    while (i < b.size()) {
        size_t eol = b.find("\r\n", i);
        if (eol == std::string::npos) break;
        size_t len = std::strtoul(b.substr(i, eol - i).c_str(), nullptr, 16);
        if (len == 0) break;
        out += b.substr(eol + 2, len);
        i = eol + 2 + len + 2;
    }
    return out;
}

// ---------- WebSocket ----------
std::mutex wsMu;
int wsFd = -1;
std::atomic<int> wsGen{0};

void wsFrame(int fd, int opcode, const std::string& payload) {
    std::string f;
    f += (char)(0x80 | opcode);
    size_t n = payload.size();
    if (n < 126) f += (char)(0x80 | n);
    else if (n < 65536) { f += (char)(0x80 | 126); f += (char)(n >> 8); f += (char)(n & 255); }
    else { f += (char)(0x80 | 127); for (int k = 7; k >= 0; k--) f += (char)((n >> (8 * k)) & 255); }
    static std::mt19937 rng{std::random_device{}()};
    unsigned char mask[4];
    for (auto& m : mask) m = (unsigned char)rng();
    f.append((const char*)mask, 4);
    for (size_t i = 0; i < n; i++) f += (char)(payload[i] ^ mask[i % 4]);
    sendAll(fd, f);
}

bool readN(int fd, std::string& buf, size_t n) {
    char tmp[4096];
    while (buf.size() < n) {
        ssize_t r = recv(fd, tmp, sizeof tmp, 0);
        if (r <= 0) return false;
        buf.append(tmp, r);
    }
    return true;
}
}  // namespace

void httpRequest(int id, const std::string& method, const std::string& url, const std::string& body, const std::string& token) {
    std::thread([=] {
        Url u;
        if (!parseUrl(url, u)) { net::deliver(id, 0, ""); return; }
        int fd = dial(u);
        if (fd < 0) { net::deliver(id, 0, ""); return; }
        std::string req = method + " " + u.path + " HTTP/1.1\r\nHost: " + u.host + "\r\nConnection: close\r\nContent-Type: application/json\r\n";
        if (!token.empty()) req += "Authorization: Bearer " + token + "\r\n";
        req += "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        std::string resp;
        if (sendAll(fd, req)) {
            char tmp[8192];
            ssize_t r;
            while ((r = recv(fd, tmp, sizeof tmp, 0)) > 0) resp.append(tmp, r);
        }
        close(fd);
        int status = 0;
        std::sscanf(resp.c_str(), "HTTP/%*s %d", &status);
        size_t hdrEnd = resp.find("\r\n\r\n");
        std::string headers = hdrEnd == std::string::npos ? resp : resp.substr(0, hdrEnd);
        std::string payload = hdrEnd == std::string::npos ? "" : resp.substr(hdrEnd + 4);
        for (auto& c : headers) c = (char)std::tolower((unsigned char)c);
        if (headers.find("transfer-encoding: chunked") != std::string::npos) payload = dechunk(payload);
        net::deliver(id, status, payload);
    }).detach();
}

void wsOpen(const std::string& url) {
    wsClose();
    int gen = ++wsGen;
    std::thread([url, gen] {
        Url u;
        if (!parseUrl(url, u)) { net::wsDeliver(2, ""); return; }
        int fd = dial(u);
        if (fd < 0) { net::wsDeliver(2, ""); return; }
        std::string req = "GET " + u.path + " HTTP/1.1\r\nHost: " + u.host + "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                          "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
        std::string buf;
        if (!sendAll(fd, req)) { close(fd); net::wsDeliver(2, ""); return; }
        size_t end;
        while ((end = buf.find("\r\n\r\n")) == std::string::npos)
            if (!readN(fd, buf, buf.size() + 1)) { close(fd); net::wsDeliver(2, ""); return; }
        if (buf.find(" 101 ") == std::string::npos) { close(fd); net::wsDeliver(2, ""); return; }
        buf.erase(0, end + 4);
        {
            std::lock_guard<std::mutex> l(wsMu);
            if (gen != wsGen) { close(fd); return; }
            wsFd = fd;
        }
        net::wsDeliver(1, "");
        std::string msg;
        for (;;) {
            if (!readN(fd, buf, 2)) break;
            unsigned char b0 = buf[0], b1 = buf[1];
            size_t len = b1 & 0x7F, hdr = 2;
            if (len == 126) { if (!readN(fd, buf, 4)) break; len = ((unsigned char)buf[2] << 8) | (unsigned char)buf[3]; hdr = 4; }
            else if (len == 127) { if (!readN(fd, buf, 10)) break; len = 0; for (int k = 0; k < 8; k++) len = (len << 8) | (unsigned char)buf[2 + k]; hdr = 10; }
            if (!readN(fd, buf, hdr + len)) break;
            std::string payload = buf.substr(hdr, len);
            buf.erase(0, hdr + len);
            int op = b0 & 0x0F;
            if (op == 8) break;
            if (op == 9) { std::lock_guard<std::mutex> l(wsMu); wsFrame(fd, 10, payload); continue; }
            msg += payload;
            if (b0 & 0x80) { if (op == 1 || op == 0) net::wsDeliver(0, msg); msg.clear(); }
        }
        {
            std::lock_guard<std::mutex> l(wsMu);
            if (wsFd == fd) wsFd = -1;
        }
        close(fd);
        if (gen == wsGen) net::wsDeliver(2, "");
    }).detach();
}

void wsSend(const std::string& text) {
    std::lock_guard<std::mutex> l(wsMu);
    if (wsFd >= 0) wsFrame(wsFd, 1, text);
}

void wsClose() {
    std::lock_guard<std::mutex> l(wsMu);
    if (wsFd >= 0) { shutdown(wsFd, SHUT_RDWR); wsFd = -1; }
    ++wsGen;
}

void showKeyboard(bool) {}
}  // namespace platform
