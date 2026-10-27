#pragma once
// Онлайн: аккаунт, облачное сохранение, набеги на реальных игроков, журнал обороны, чат, кланы.
// Адрес сервера: файл server.txt в папке сохранений или LF_SERVER_URL при сборке; пусто — игра офлайн.
#include <functional>
#include <string>
#include <vector>

#include "engine/json.h"

namespace client {

struct ChatLine { long long id = 0, from = 0, at = 0; std::string channel, name, text; bool system = false; };

class Online {
public:
    void init();
    bool enabled() const { return !url_.empty(); }
    bool ready() const { return enabled() && !token_.empty(); }
    long long playerId() const { return id_; }
    const std::string& name() const { return name_; }

    // Аккаунт создаётся при первом запуске с сетью.
    void ensureAccount();
    // Облачное сохранение; onDebt — сколько леса и хлеба отняли налётчики, пока игрок был в игре; rating — серверный.
    void pushSave(const std::string& save, std::function<void(bool ok, long long ore, long long aether, int rating)> onDone);
    // Соперник для набега: снимок базы и seed.
    struct Match { long long id = 0; int seed = 0; std::string snapshot, name; int rating = 0, level = 0, lantern = 1; };
    void findMatch(std::function<void(bool ok, const Match&, const std::string& error)> cb);
    // Отправить запись боя; сервер возвращает итог и новое сохранение.
    void submitAttack(long long match, const std::string& replay, std::function<void(bool ok, const json::Value& resp)> cb);
    void defenses(std::function<void(const json::Value&)> cb);
    void leaderboard(std::function<void(const json::Value&)> cb);

    // Кланы.
    void me(std::function<void(const json::Value&)> cb);
    void clanSearch(const std::string& q, std::function<void(const json::Value&)> cb);
    void clanInfo(long long id, std::function<void(const json::Value&)> cb);
    void clanCreate(const std::string& name, const std::string& desc, std::function<void(bool ok, const std::string& err)> cb);
    void clanJoin(long long id, std::function<void(bool ok, const std::string& err)> cb);
    void clanLeave(std::function<void(bool ok)> cb);
    void report(long long messageId);

    // Чат.
    void chatConnect();
    void say(const std::string& channel, const std::string& text);
    bool chatConnected() const;
    std::vector<ChatLine> global, clan;
    std::string lastChatError;
    int unread = 0;

private:
    std::string url_, token_, name_;
    long long id_ = 0;
    bool registering_ = false;
    void saveCreds();
    std::string api(const std::string& path) const { return url_ + path; }
};

}  // namespace client
