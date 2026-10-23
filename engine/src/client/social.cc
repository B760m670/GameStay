// Онлайн-экраны: чат (общий и клановый) и кланы.
#include <algorithm>

#include "app.h"
#include "engine/atlas.h"
#include "engine/audio.h"
#include "engine/font.h"
#include "engine/gfx.h"

namespace client {
using gfx::rgba;

namespace {
const uint32_t kInk = rgba(242, 236, 255), kInkSoft = rgba(176, 164, 214), kGold = rgba(255, 216, 107), kBad = rgba(255, 140, 130);
}

bool App::onlineOffline(const ui::Rect& body) {
    if (online.enabled()) {
        if (online.ready()) return false;
        font::draw("Подключаемся к серверу…", body.x + body.w / 2, body.y + body.h / 2 - 12, 18, kInk, font::Center, false);
        return true;
    }
    float y = body.y + body.h / 2 - 40;
    for (const auto& l : font::wrap("Онлайн заработает, когда будет запущен сервер игры: общий чат, кланы, набеги на живых игроков и лиги.", 17,
                                    body.w - 60)) {
        font::draw(l, body.x + body.w / 2, y, 17, kInk, font::Center, false);
        y += font::lineHeight(17);
    }
    return true;
}

void App::drawChat() {
    ui::Rect body;
    if (!modalFrame("Чат", 760, 370, body)) return;
    if (onlineOffline(body)) return;
    online.unread = 0;
    const char* tabs[3] = {"Общий", "Клан", "Кланы"};
    float tx = body.x;
    for (int i = 0; i < 3; i++) {
        float w = font::measure(tabs[i], 16) + 30;
        if (ui::button({tx, body.y, w, 34}, tabs[i], chatTab == i ? ui::Style::Gold : ui::Style::Dark, true, 16)) {
            if (i == 2) { modal = Modal::Clan; clanLoaded = false; return; }
            chatTab = i;
        }
        tx += w + 6;
    }
    font::draw(online.chatConnected() ? "в сети" : "нет связи", body.x + body.w, body.y + 8, 13, online.chatConnected() ? rgba(120, 220, 110) : kBad,
               font::Right, false);
    // сообщения снизу вверх
    const auto& list = chatTab == 0 ? online.global : online.clan;
    ui::Rect area{body.x, body.y + 42, body.w, body.h - 42 - 52};
    atlas::draw9(atlas::get("ui/card"), area.x, area.y, area.w, area.h, 40, 12);
    if (chatTab == 1 && myClan == 0 && list.empty()) {
        font::draw("Ты пока не в клане — загляни во вкладку «Кланы»", area.x + area.w / 2, area.y + area.h / 2 - 10, 16, kInkSoft, font::Center, false);
    }
    float y = area.y + area.h - 12;
    const float ts = 15;
    for (int i = (int)list.size() - 1; i >= 0 && y > area.y + 20; i--) {
        const auto& m = list[i];
        std::string head = m.system ? "" : m.name + ": ";
        auto lines = font::wrap(head + m.text, ts, area.w - 70);
        float h = lines.size() * font::lineHeight(ts);
        y -= h + 4;
        if (y < area.y + 6) break;
        float ly = y;
        for (const auto& l : lines) {
            font::draw(l, area.x + 14, ly, ts, m.system ? kInkSoft : m.from == online.playerId() ? rgba(140, 200, 255) : kInk, font::Left, false);
            ly += font::lineHeight(ts);
        }
        if (!m.system && m.from != online.playerId() && m.id > 0) {
            ui::Rect rb{area.x + area.w - 40, y - 2, 30, 24};
            font::draw("!", rb.x + rb.w / 2, rb.y + 2, 15, rgba(255, 140, 120), font::Center, false);
            if (ui::tapped(rb)) { online.report(m.id); toast("Жалоба отправлена — спасибо", kGold); }
        }
    }
    // ввод
    ui::Rect field{body.x, body.y + body.h - 44, body.w - 140, 44};
    bool submit = ui::textField(field, chatInput, chatTab == 0 ? "Написать всем…" : "Написать клану…", 200);
    if (ui::button({body.x + body.w - 130, body.y + body.h - 46, 130, 46}, "Отправить", ui::Style::Green, !chatInput.empty(), 16) || submit) {
        if (!chatInput.empty()) {
            online.say(chatTab == 0 ? "global" : "clan", chatInput);
            chatInput.clear();
        }
    }
    if (!online.lastChatError.empty()) {
        toast(online.lastChatError, kBad);
        online.lastChatError.clear();
    }
}

void App::refreshClan() {
    clanLoaded = true;
    online.me([this](const json::Value& v) {
        myClan = v["clan"].i64();
        if (myClan > 0) online.clanInfo(myClan, [this](const json::Value& c) { clanView = c; });
        else online.clanSearch(clanQuery, [this](const json::Value& l) { clanList = l; });
    });
}

void App::drawClan() {
    ui::Rect body;
    if (!modalFrame("Кланы", 780, 380, body)) return;
    if (onlineOffline(body)) return;
    if (!clanLoaded) refreshClan();
    if (myClan > 0) {
        // свой клан: описание, участники, выход
        font::draw(clanView["name"].s("…"), body.x + 4, body.y, 22, kInk, font::Left, false);
        font::draw(clanView["description"].s(), body.x + 4, body.y + 30, 14, kInkSoft, font::Left, false);
        const auto& mem = clanView["members"];
        float y = body.y + 58, colW = (body.w - 10) / 2;
        for (size_t i = 0; i < mem.size() && i < 16; i++) {
            float x = body.x + (i % 2) * (colW + 10), yy = y + (i / 2) * 34;
            atlas::draw9(atlas::get("ui/card"), x, yy, colW, 30, 40, 10);
            std::string role = mem[i]["role"].s() == "leader" ? " · глава" : "";
            font::draw(std::to_string(i + 1) + ". " + mem[i]["name"].s() + role, x + 10, yy + 6, 14, kInk, font::Left, false);
            font::draw("рейтинг " + std::to_string(mem[i]["rating"].i64()), x + colW - 10, yy + 7, 13, kInkSoft, font::Right, false);
        }
        if (ui::button({body.x, body.y + body.h - 48, 200, 48}, "Чат клана", ui::Style::Green, true, 16)) { chatTab = 1; modal = Modal::Chat; }
        if (ui::button({body.x + body.w - 180, body.y + body.h - 48, 180, 48}, "Покинуть", ui::Style::Red, true, 16))
            online.clanLeave([this](bool ok) {
                if (ok) { toast("Ты покинул клан", kGold); myClan = 0; clanLoaded = false; }
            });
        return;
    }
    // без клана: поиск и создание
    ui::Rect qf{body.x, body.y, body.w - 300, 42};
    bool go = ui::textField(qf, clanQuery, "Название клана…", 20);
    if (ui::button({body.x + body.w - 290, body.y, 130, 42}, "Найти", ui::Style::Normal, true, 16) || go)
        online.clanSearch(clanQuery, [this](const json::Value& l) { clanList = l; });
    if (ui::button({body.x + body.w - 150, body.y, 150, 42}, "Создать", ui::Style::Gold, utf8Len(clanQuery) >= 3, 16))
        online.clanCreate(clanQuery, "", [this](bool ok, const std::string& err) {
            if (ok) { toast("Клан «" + clanQuery + "» основан!", kGold); audio::play("win", 0.7f); clanLoaded = false; }
            else toast(err, kBad);
        });
    float y = body.y + 52;
    if (clanList.size() == 0) font::draw("Кланов не найдено — впиши название и нажми «Создать»", body.x + 6, y + 6, 15, kInkSoft, font::Left, false);
    for (size_t i = 0; i < clanList.size() && y < body.y + body.h - 40; i++, y += 46) {
        const auto& c = clanList[i];
        atlas::draw9(atlas::get("ui/card"), body.x, y, body.w, 42, 40, 12);
        font::draw(c["name"].s(), body.x + 14, y + 4, 17, kInk, font::Left, false);
        font::draw(std::to_string(c["members"].i64()) + "/30 · рейтинг " + std::to_string(c["rating"].i64()) +
                       (c["min_rating"].i64() > 0 ? " · от " + std::to_string(c["min_rating"].i64()) : ""),
                   body.x + 14, y + 24, 12, kInkSoft, font::Left, false);
        long long id = c["id"].i64();
        if (ui::button({body.x + body.w - 140, y + 4, 130, 34}, "Вступить", ui::Style::Green, true, 15))
            online.clanJoin(id, [this](bool ok, const std::string& err) {
                if (ok) { toast("Добро пожаловать в клан!", kGold); clanLoaded = false; }
                else toast(err, kBad);
            });
    }
}

int App::utf8Len(const std::string& s) {
    int n = 0;
    for (char c : s) n += (c & 0xC0) != 0x80;
    return n;
}

}  // namespace client
