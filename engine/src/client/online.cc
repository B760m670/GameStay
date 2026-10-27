#include "online.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "engine/net.h"
#include "platform/platform.h"

#ifndef LF_SERVER_URL
#define LF_SERVER_URL ""
#endif

namespace client {
namespace {
std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    while (!s.empty() && (s.front() == ' ')) s.erase(0, 1);
    return s;
}
std::string credsPath() { return platform::saveDir() + "/online.txt"; }
std::string errorOf(const net::Response& r) {
    if (r.status == 0) return "Нет связи с сервером";
    auto v = json::parse(r.body);
    return v["error"].s("Ошибка сервера");
}
}  // namespace

void Online::init() {
    std::vector<unsigned char> data;
    url_ = LF_SERVER_URL;
    if (platform::readFile(platform::saveDir() + "/server.txt", data)) url_ = trim(std::string(data.begin(), data.end()));
    while (!url_.empty() && url_.back() == '/') url_.pop_back();
    if (platform::readFile(credsPath(), data)) {
        std::istringstream in(std::string(data.begin(), data.end()));
        std::string k;
        while (in >> k) {
            if (k == "id") in >> id_;
            else if (k == "token") in >> token_;
            else if (k == "name") { std::getline(in, name_); name_ = trim(name_); }
        }
    }
}

void Online::saveCreds() {
    platform::writeFile(credsPath(), "id " + std::to_string(id_) + "\ntoken " + token_ + "\nname " + name_ + "\n");
}

void Online::ensureAccount() {
    if (!enabled() || ready() || registering_) return;
    registering_ = true;
    net::request("POST", api("/v1/register"), "{\"name\":\"Приказчик\"}", "", [this](const net::Response& r) {
        registering_ = false;
        if (r.status != 200) return;
        auto v = json::parse(r.body);
        id_ = v["id"].i64();
        token_ = v["token"].s();
        name_ = "Приказчик";
        if (!token_.empty()) saveCreds();
    });
}

void Online::pushSave(const std::string& save, std::function<void(bool, long long, long long, int)> onDone) {
    if (!ready()) { if (onDone) onDone(false, 0, 0, 0); return; }
    net::request("PUT", api("/v1/save"), "{\"save\":" + json::quote(save) + "}", token_, [onDone](const net::Response& r) {
        auto v = json::parse(r.body);
        if (onDone) onDone(r.status == 200, v["debt_ore"].i64(), v["debt_aether"].i64(), (int)v["rating"].i64());
    });
}

void Online::findMatch(std::function<void(bool, const Match&, const std::string&)> cb) {
    if (!ready()) { cb(false, {}, "Нет связи с сервером"); return; }
    net::request("POST", api("/v1/match"), "", token_, [cb](const net::Response& r) {
        Match m;
        if (r.status != 200) {
            cb(false, m, r.status == 404 ? "Соперников пока нет — загляни позже" : errorOf(r));
            return;
        }
        auto v = json::parse(r.body);
        m.id = v["match"].i64();
        m.seed = (int)v["seed"].i64();
        m.snapshot = v["snapshot"].s();
        m.name = v["defender"]["name"].s();
        m.rating = (int)v["defender"]["rating"].i64();
        m.level = (int)v["defender"]["level"].i64();
        m.lantern = (int)v["defender"]["lantern"].i64(1);
        cb(true, m, "");
    });
}

void Online::submitAttack(long long match, const std::string& replay, std::function<void(bool, const json::Value&)> cb) {
    net::request("POST", api("/v1/attack"), "{\"match\":" + std::to_string(match) + ",\"replay\":" + json::quote(replay) + "}", token_,
                 [cb](const net::Response& r) { cb(r.status == 200, json::parse(r.body)); });
}

void Online::defenses(std::function<void(const json::Value&)> cb) {
    if (!ready()) return;
    net::request("GET", api("/v1/defenses"), "", token_, [cb](const net::Response& r) { if (r.status == 200) cb(json::parse(r.body)); });
}

void Online::leaderboard(std::function<void(const json::Value&)> cb) {
    if (!enabled()) return;
    net::request("GET", api("/v1/leaderboard"), "", token_, [cb](const net::Response& r) { if (r.status == 200) cb(json::parse(r.body)); });
}

void Online::me(std::function<void(const json::Value&)> cb) {
    if (!ready()) return;
    net::request("GET", api("/v1/me"), "", token_, [cb](const net::Response& r) { if (r.status == 200) cb(json::parse(r.body)); });
}

void Online::clanSearch(const std::string& q, std::function<void(const json::Value&)> cb) {
    if (!ready()) return;
    std::string enc;
    for (unsigned char c : q) {
        if (std::isalnum(c) || c == '-' || c == '_') enc += (char)c;
        else { char b[4]; std::snprintf(b, sizeof b, "%%%02X", c); enc += b; }
    }
    net::request("GET", api("/v1/clans?q=" + enc), "", token_, [cb](const net::Response& r) { if (r.status == 200) cb(json::parse(r.body)); });
}

void Online::clanInfo(long long id, std::function<void(const json::Value&)> cb) {
    net::request("GET", api("/v1/clans/" + std::to_string(id)), "", token_, [cb](const net::Response& r) { if (r.status == 200) cb(json::parse(r.body)); });
}

void Online::clanCreate(const std::string& name, const std::string& desc, std::function<void(bool, const std::string&)> cb) {
    net::request("POST", api("/v1/clans"), "{\"name\":" + json::quote(name) + ",\"description\":" + json::quote(desc) + "}", token_,
                 [this, cb](const net::Response& r) {
                     if (r.status == 200) chatConnect();  // переподключиться: появился клановый канал
                     cb(r.status == 200, r.status == 409 ? "Такое имя занято" : errorOf(r));
                 });
}

void Online::clanJoin(long long id, std::function<void(bool, const std::string&)> cb) {
    net::request("POST", api("/v1/clans/" + std::to_string(id) + "/join"), "", token_, [this, cb](const net::Response& r) {
        if (r.status == 200) chatConnect();
        cb(r.status == 200, r.status == 403 ? "Мало рейтинга для этого клана" : r.status == 409 ? "Клан полон или ты уже в клане" : errorOf(r));
    });
}

void Online::clanLeave(std::function<void(bool)> cb) {
    net::request("POST", api("/v1/clans/leave"), "", token_, [this, cb](const net::Response& r) {
        if (r.status == 200) { clan.clear(); chatConnect(); }
        cb(r.status == 200);
    });
}

void Online::report(long long messageId) {
    net::request("POST", api("/v1/report"), "{\"message\":" + std::to_string(messageId) + "}", token_, [](const net::Response&) {});
}

void Online::chatConnect() {
    if (!ready()) return;
    std::string ws = url_;
    if (ws.rfind("https://", 0) == 0) ws = "wss://" + ws.substr(8);
    else if (ws.rfind("http://", 0) == 0) ws = "ws://" + ws.substr(7);
    global.clear();
    clan.clear();
    net::wsOpen(ws + "/v1/ws?token=" + token_,
                [this](const std::string& text) {
                    auto v = json::parse(text);
                    std::string type = v["type"].s();
                    if (type == "error") { lastChatError = v["text"].s(); return; }
                    ChatLine l;
                    l.id = v["id"].i64();
                    l.from = v["from"].i64();
                    l.at = v["at"].i64();
                    l.channel = v["channel"].s();
                    l.name = v["name"].s();
                    l.text = v["text"].s();
                    l.system = type == "system";
                    auto& list = l.channel == "global" ? global : clan;
                    list.push_back(l);
                    if (list.size() > 120) list.erase(list.begin());
                    if (l.from != id_) unread++;
                },
                [](bool) {});
}

void Online::say(const std::string& channel, const std::string& text) {
    net::wsSend("{\"type\":\"say\",\"channel\":" + json::quote(channel) + ",\"text\":" + json::quote(text) + "}");
}

bool Online::chatConnected() const { return net::wsConnected(); }

}  // namespace client
