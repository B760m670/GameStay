#include "net.h"

#include <map>
#include <mutex>
#include <vector>

#include "../platform/platform.h"

namespace net {
namespace {
std::mutex mu;
int nextId = 1;
std::map<int, std::function<void(const Response&)>> pending;
std::vector<std::pair<int, Response>> done;
std::vector<std::pair<int, std::string>> wsEvents;
std::function<void(const std::string&)> wsMsg;
std::function<void(bool)> wsState;
bool connected = false;
}  // namespace

void request(const std::string& method, const std::string& url, const std::string& body, const std::string& token,
             std::function<void(const Response&)> cb) {
    int id;
    {
        std::lock_guard<std::mutex> l(mu);
        id = nextId++;
        pending[id] = std::move(cb);
    }
    platform::httpRequest(id, method, url, body, token);
}

void deliver(int id, int status, std::string body) {
    std::lock_guard<std::mutex> l(mu);
    done.push_back({id, Response{status, std::move(body)}});
}

void wsDeliver(int kind, std::string text) {
    std::lock_guard<std::mutex> l(mu);
    wsEvents.push_back({kind, std::move(text)});
}

void wsOpen(const std::string& url, std::function<void(const std::string&)> onMessage, std::function<void(bool)> onState) {
    wsMsg = std::move(onMessage);
    wsState = std::move(onState);
    platform::wsOpen(url);
}
void wsSend(const std::string& text) { if (connected) platform::wsSend(text); }
void wsClose() { platform::wsClose(); connected = false; }
bool wsConnected() { return connected; }

void poll() {
    std::vector<std::pair<int, Response>> d;
    std::vector<std::pair<int, std::string>> ev;
    {
        std::lock_guard<std::mutex> l(mu);
        d.swap(done);
        ev.swap(wsEvents);
    }
    for (auto& [id, r] : d) {
        std::function<void(const Response&)> cb;
        {
            std::lock_guard<std::mutex> l(mu);
            auto it = pending.find(id);
            if (it == pending.end()) continue;
            cb = std::move(it->second);
            pending.erase(it);
        }
        if (cb) cb(r);
    }
    for (auto& [kind, text] : ev) {
        if (kind == 0 && wsMsg) wsMsg(text);
        else if (kind == 1 || kind == 2) {
            connected = kind == 1;
            if (wsState) wsState(connected);
        }
    }
}

}  // namespace net
