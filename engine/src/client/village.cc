// Сцена деревни.
#include <algorithm>
#include <cmath>

#include "app.h"
#include "engine/atlas.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "world.h"

namespace client {
using gfx::rgba;
namespace V = lf::village;

namespace {
const uint32_t kGold = rgba(255, 216, 107), kMuted = rgba(190, 178, 215), kOre = rgba(240, 160, 80), kAether = rgba(134, 220, 255), kBad = rgba(255, 140, 130);

const char* resIcon(lf::Res r) { return r == lf::Res::Ore ? "icon/ore" : r == lf::Res::Aether ? "icon/aether" : "icon/shard"; }

// «⛏ 1200» — иконка ресурса и число.
float costLabel(lf::Res r, long long amount, float x, float y, float size, uint32_t color = 0xffffffffu, int align = 0) {
    std::string s = fmtNum(amount);
    float w = size + 4 + font::measure(s, size);
    if (align == 1) x -= w / 2;
    else if (align == 2) x -= w;
    ui::icon(resIcon(r), x + size / 2, y + size * 0.62f, size * 1.15f);
    font::draw(s, x + size + 4, y, size, color);
    return w;
}

const lf::BType kShopTabs[4][6] = {
    {lf::BType::Ballista, lf::BType::Brazier, lf::BType::Catapult, lf::BType::Prism, lf::BType::Count, lf::BType::Count},
    {lf::BType::Beacon, lf::BType::Wall, lf::BType::Count, lf::BType::Count, lf::BType::Count, lf::BType::Count},
    {lf::BType::OreMine, lf::BType::AetherWell, lf::BType::OreStore, lf::BType::AetherStore, lf::BType::BuilderHut, lf::BType::Count},
    {lf::BType::Barracks, lf::BType::Camp, lf::BType::Count, lf::BType::Count, lf::BType::Count, lf::BType::Count},
};
const char* kShopTabNames[4] = {"Оборона", "Свет и стены", "Экономика", "Армия"};
}  // namespace

void App::enterVillage() {
    scene = Scene::Village;
    modal = Modal::None;
    selected = -1;
    placing = Placing{};
    gest.reset();
    int LL = V::lanternLevel(S);
    float R = lf::B(lf::BType::Lantern).lv(lf::B(lf::BType::Lantern).light, LL) + 4.5f;
    requestFocus(22, 22, R, 1.25f);
    gest.onDown = [this](Vec2 w, Vec2) {
        if (placing.active && placing.moveId < 0) {
            int s = lf::B(placing.type).size;
            if (w.x >= placing.x && w.x < placing.x + s && w.y >= placing.y && w.y < placing.y + s) {
                placing.dragging = true;
                placing.grabX = w.x - placing.x;
                placing.grabY = w.y - placing.y;
                return true;
            }
            return false;
        }
        if (selected >= 0) {
            int b = buildingAt(w.x, w.y);
            if (b == selected) {
                auto* rec = V::get(S, selected);
                placing = Placing{true, rec->type, rec->x, rec->y, rec->id, true, "", 1, 0, w.x - rec->x, w.y - rec->y, true};
                return true;
            }
        }
        return false;
    };
    gest.onDrag = [this](Vec2 w, Vec2) {
        if (!placing.active || !placing.dragging) return;
        int nx = (int)std::lround(w.x - placing.grabX), ny = (int)std::lround(w.y - placing.grabY);
        if (nx != placing.x || ny != placing.y) {
            int dx = nx - placing.x, dy = ny - placing.y;
            placing.dirX = dx > 0 ? 1 : dx < 0 ? -1 : 0;
            placing.dirY = placing.dirX ? 0 : (dy > 0 ? 1 : -1);
            placing.x = nx;
            placing.y = ny;
            validatePlacing();
        }
    };
    gest.onDragEnd = [this](bool moved, Vec2) {
        if (!placing.active) return;
        placing.dragging = false;
        if (placing.moveId >= 0) {
            int id = placing.moveId;
            auto* rec = V::get(S, id);
            bool changed = rec && (rec->x != placing.x || rec->y != placing.y);
            if (moved && changed) {
                auto r = V::move(S, id, placing.x, placing.y);
                if (!r.ok) toast(r.reason, kBad);
                else save();
            }
            placing = Placing{};
        }
    };
    gest.onTap = [this](Vec2 w, Vec2) {
        if (placing.active && placing.moveId < 0) {
            int s = lf::B(placing.type).size;
            placing.x = (int)std::lround(w.x - s / 2.f);
            placing.y = (int)std::lround(w.y - s / 2.f);
            validatePlacing();
            return;
        }
        int b = buildingAt(w.x, w.y);
        if (b < 0) { selected = -1; return; }
        auto* rec = V::get(S, b);
        if (rec->def().produces != lf::Res::None && V::collectorAmount(*rec, now()) >= 1 && selected != b) {
            long long got = V::collect(S, b, now());
            if (got > 0) toast("+" + fmtNum(got) + (rec->def().produces == lf::Res::Ore ? " руды" : " Эфира"), rec->def().produces == lf::Res::Ore ? kOre : kAether);
            else toast("Хранилища полны", kBad);
        }
        selected = b;
    };
}

int App::buildingAt(float wx, float wy) const {
    int tx = (int)std::floor(wx), ty = (int)std::floor(wy), best = -1, bd = -1;
    for (const auto& b : S.buildings) {
        int s = b.def().size;
        if (tx >= b.x && tx < b.x + s && ty >= b.y && ty < b.y + s && b.x + b.y + s > bd) { bd = b.x + b.y + s; best = b.id; }
    }
    return best;
}

void App::startPlacing(lf::BType type) {
    Vec2 c = cam.toWorld(cam.w / 2, cam.h / 2);
    int s = lf::B(type).size;
    int cx = (int)std::lround(c.x - s / 2.f), cy = (int)std::lround(c.y - s / 2.f);
    placing = Placing{};
    placing.active = true;
    placing.type = type;
    placing.x = cx;
    placing.y = cy;
    for (int r = 0; r < 20; r++) {
        bool found = false;
        for (int dy = -r; dy <= r && !found; dy++)
            for (int dx = -r; dx <= r && !found; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                if (V::canPlace(S, type, cx + dx, cy + dy).ok) { placing.x = cx + dx; placing.y = cy + dy; found = true; }
            }
        if (found) break;
    }
    selected = -1;
    validatePlacing();
}

void App::validatePlacing() {
    auto r = V::canPlace(S, placing.type, placing.x, placing.y, placing.moveId);
    placing.valid = r.ok;
    placing.reason = r.reason;
}

void App::confirmPlacing() {
    int id = -1;
    auto r = V::place(S, placing.type, placing.x, placing.y, now(), &id);
    if (!r.ok) { toast(r.reason, kBad); return; }
    save();
    if (placing.type == lf::BType::Wall && V::canBuild(S, lf::BType::Wall, now()).ok) {
        placing.x += placing.dirX;
        placing.y += placing.dirY;
        validatePlacing();
        return;
    }
    placing = Placing{};
    selected = id;
}

void App::villageFrame(float dt, const std::vector<input::Event>& worldEvents) {
    gest.process(worldEvents, cam);
    tickT += dt;
    if (tickT > 0.25f) { tickT = 0; V::tick(S, now()); }
    drawVillageWorld();
    drawVillageHud();
    switch (modal) {
        case Modal::Shop: drawShop(); break;
        case Modal::Army: drawArmy(); break;
        case Modal::Map: drawMap(); break;
        case Modal::Night: drawNight(); break;
        case Modal::Codex: drawCodex(); break;
        case Modal::Menu: drawMenu(); break;
        default: break;
    }
}

void App::drawVillageWorld() {
    world::drawGround(cam);
    // свет считаем с учётом переносимой постройки
    lf::VillageState view = S;
    if (placing.active && placing.moveId >= 0)
        for (auto& b : view.buildings) if (b.id == placing.moveId) { b.x = placing.x; b.y = placing.y; }
    world::drawFog(cam, V::sources(view), {}, t);
    struct Item { float depth; const lf::BuildingRec* b; bool ghost; };
    std::vector<Item> items;
    for (const auto& b : view.buildings) items.push_back({(float)(b.x + b.y + b.def().size), &b, false});
    lf::BuildingRec ghost;
    if (placing.active && placing.moveId < 0) {
        ghost.type = placing.type;
        ghost.x = placing.x;
        ghost.y = placing.y;
        ghost.level = 1;
        items.push_back({1e9f, &ghost, true});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });
    long long tnow = now();
    for (const auto& it : items) {
        const auto& b = *it.b;
        world::BuildingLook look;
        look.selected = b.id == selected && !placing.active;
        look.constructing = b.level == 0;
        if (it.ghost || (placing.active && b.id == placing.moveId)) { look.ghost = placing.valid ? 1 : -1; look.alpha = 0.85f; }
        world::drawBuilding(cam, b.type, (float)b.x, (float)b.y, std::max(1, b.level), look, t);
    }
    // индикаторы стройки и сбора
    for (const auto& b : S.buildings) {
        const auto& d = b.def();
        float s = (float)d.size;
        if (b.busyUntil > tnow) {
            int idx = std::min(b.level, (int)d.time.size() - 1);
            float total = std::max(1, d.time[idx]) * 1000.f;
            float frac = 1 - (b.busyUntil - tnow) / total;
            world::drawBarAt(cam, b.x + s / 2, b.y + s / 2, 80 + s * 14, frac, kGold, 46);
            Vec2 p = cam.toScreen(b.x + s / 2, b.y + s / 2, 80 + s * 14);
            font::draw(fmtTime((b.busyUntil - tnow) / 1000.0), p.x, p.y - 22, 14, 0xffffffffu, font::Center);
        } else if (d.produces != lf::Res::None && b.level >= 1) {
            double a = V::collectorAmount(b, tnow);
            if (a >= d.lv(d.cap, b.level) * 0.1) {
                Vec2 p = cam.toScreen(b.x + s / 2, b.y + s / 2, 85 + std::sin(t * 3 + b.id) * 5);
                float r = 15 + 4 * cam.zoom;
                atlas::drawRect(atlas::get("fx/dot"), p.x - r - 2, p.y - r - 2, r * 2 + 4, r * 2 + 4, rgba(20, 12, 30, 230));
                atlas::drawRect(atlas::get("fx/dot"), p.x - r, p.y - r, r * 2, r * 2, rgba(255, 245, 220));
                ui::icon(resIcon(d.produces), p.x, p.y, r * 1.5f);
            }
        }
    }
}

void App::drawVillageHud() {
    long long tnow = now();
    // слева: Фонарь, строители, армия
    float x = safeX, y = 10;
    auto chip = [&](const char* ic, const std::string& text, float w) {
        ui::panel({x, y, w, 40}, ui::Style::Dark);
        ui::icon(ic, x + 22, y + 20, 30);
        font::draw(text, x + 42, y + 10, 17, 0xffffffffu);
        x += w + 8;
    };
    chip("icon/lantern", "Фонарь " + std::to_string(V::lanternLevel(S)), 122);
    auto bl = V::builders(S, tnow);
    chip("icon/builder", std::to_string(bl.free) + "/" + std::to_string(bl.total), 76);
    chip("icon/army", std::to_string(V::armyHousing(S)) + "/" + std::to_string(V::campCapacity(S)), 92);
    // справа: ресурсы
    float rx = cam.w - safeX;
    auto resBar = [&](lf::Res r, uint32_t color) {
        float w = 146;
        rx -= w;
        long long v = S.res(r), c = V::cap(S, r);
        ui::panel({rx, y, w, 40}, ui::Style::Dark);
        ui::bar({rx + 30, y + 22, w - 40, 12}, c > 0 ? (float)v / c : 0, color);
        font::draw(fmtNum(v), rx + w - 10, y + 3, 16, 0xffffffffu, font::Right);
        ui::icon(resIcon(r), rx + 18, y + 20, 32);
        rx -= 8;
    };
    {
        float w = 76;
        rx -= w;
        ui::panel({rx, y, w, 40}, ui::Style::Dark);
        ui::icon("icon/shard", rx + 20, y + 20, 28);
        font::draw(fmtNum(S.shards), rx + w - 12, y + 10, 17, kGold, font::Right);
        rx -= 8;
    }
    resBar(lf::Res::Aether, kAether);
    resBar(lf::Res::Ore, kOre);

    if (placing.active && placing.moveId < 0) { drawPlacingPanel(); return; }
    if (selected >= 0 && V::get(S, selected)) drawSelectedPanel();

    // нижняя панель
    const float bw = 70, bh = 62, gap = 8;
    const int n = 6;
    float total = n * bw + (n - 1) * gap;
    float bx = cam.w / 2 - total / 2, by = cam.h - bh - safeB;
    bool due = V::nightDue(S, tnow);
    struct B { const char* icon; std::string label; Modal m; };
    B btns[n] = {{"icon/build", "Строить", Modal::Shop}, {"icon/army", "Армия", Modal::Army}, {"icon/map", "Поход", Modal::Map},
                 {"icon/night", due ? "Мгла!" : fmtTime((S.nightAt - tnow) / 1000.0), Modal::Night}, {"icon/codex", "Хроника", Modal::Codex},
                 {"icon/menu", "Меню", Modal::Menu}};
    for (int i = 0; i < n; i++) {
        bool hl = i == 3 && due && std::fmod(t, 1.2f) < 0.8f;
        if (ui::iconButton({bx + i * (bw + gap), by, bw, bh}, btns[i].icon, btns[i].label, i == 2 ? ui::Style::Gold : ui::Style::Normal, hl)) {
            modal = btns[i].m;
            selected = -1;
            resetArmed = false;
        }
    }
}

void App::drawSelectedPanel() {
    long long tnow = now();
    auto* b = V::get(S, selected);
    const auto& d = b->def();
    auto up = V::upgradeInfo(S, b->id, tnow);
    bool busy = b->busyUntil > tnow;
    float pw = std::min(560.f, cam.w - 2 * safeX), ph = 120;
    float px = cam.w / 2 - pw / 2, py = cam.h - safeB - 62 - 10 - ph;
    ui::panel({px, py, pw, ph});
    world::drawBuildingThumb(b->type, px + 52, py + ph / 2, 84, 100, std::max(1, b->level));
    float tx = px + 104;
    std::string title = std::string(d.name) + (b->level ? " — ур. " + std::to_string(b->level) : " — строится");
    font::draw(title, tx, py + 10, 20, kGold);
    // строка характеристик
    int lv = std::max(1, b->level);
    std::string stats = "Прочность " + std::to_string(d.lv(d.hp, lv));
    if (d.isLight()) { char buf[32]; std::snprintf(buf, sizeof buf, " · Свет %.1f", d.lv(d.light, lv)); stats += buf; }
    if (d.produces != lf::Res::None) stats += " · " + std::to_string(d.lv(d.rate, lv)) + "/мин";
    if (!d.dmg.empty()) stats += " · Урон " + std::to_string(d.lv(d.dmg, lv));
    if (!d.dps.empty()) stats += " · Луч " + std::to_string((int)d.lv(d.dps, lv)) + "/с";
    if (!d.capacity.empty()) stats += " · Мест " + std::to_string(d.lv(d.capacity, lv));
    if (!d.storage.empty() && b->type != lf::BType::Lantern) stats += " · Вмещает " + fmtNum(d.lv(d.storage, lv));
    font::draw(stats, tx, py + 36, 14, kMuted, font::Left, false);
    // кнопки
    float bx = tx, by = py + ph - 52, bh = 42;
    if (busy) {
        int sc = V::speedUpCost(*b, tnow);
        font::draw("Готово через " + fmtTime((b->busyUntil - tnow) / 1000.0), bx, by + 10, 16, 0xffffffffu);
        float w = 170;
        if (ui::button({px + pw - w - 14, by, w, bh}, "", ui::Style::Gold, S.shards >= sc)) {
            auto r = V::speedUp(S, b->id, tnow);
            if (!r.ok) toast(r.reason, kBad); else { toast("Готово!", kGold); save(); }
        }
        font::draw("Ускорить", px + pw - w - 14 + 16, by + 11, 16, 0xffffffffu);
        costLabel(lf::Res::Shards, sc, px + pw - 14 - 60, by + 11, 16);
    } else if (!up.max) {
        float w = 220;
        if (ui::button({bx, by, w, bh}, "", up.ok ? ui::Style::Gold : ui::Style::Dark, up.ok)) {
            auto r = V::startUpgrade(S, b->id, tnow);
            if (!r.ok) toast(r.reason, kBad); else { toast("Строители взялись за работу", kGold); save(); }
        }
        font::draw("Улучшить", bx + 14, by + 11, 16, 0xffffffffu);
        if (up.cost > 0 || up.next) costLabel(up.res, up.cost, bx + w - 12, by + 11, 16, S.res(up.res) >= up.cost ? 0xffffffffu : kBad, 2);
        if (!up.ok) font::draw(up.reason, bx + w + 12, by + 13, 13, kBad, font::Left, false);
    } else {
        font::draw("Максимальный уровень", bx, by + 12, 16, kGold);
    }
    if (d.produces != lf::Res::None && b->level >= 1 && !busy) {
        double a = V::collectorAmount(*b, tnow);
        float w = 130;
        if (ui::button({px + pw - w - 14, by, w, bh}, "", ui::Style::Green, a >= 1)) {
            long long got = V::collect(S, b->id, tnow);
            toast(got > 0 ? "+" + fmtNum(got) : "Хранилища полны", got > 0 ? kGold : kBad);
        }
        costLabel(d.produces, (long long)a, px + pw - w / 2 - 14, by + 11, 16, 0xffffffffu, 1);
    }
    if (b->type == lf::BType::Barracks || b->type == lf::BType::Camp) {
        if (ui::button({px + pw - 144, by, 130, bh}, "Армия", ui::Style::Normal)) { modal = Modal::Army; selected = -1; }
    }
    if (ui::button({px + pw - 40, py + 8, 32, 32}, "×", ui::Style::Dark, true, 16)) selected = -1;
}

void App::drawPlacingPanel() {
    const auto& d = lf::B(placing.type);
    auto cost = V::buildCost(S, placing.type);
    float pw = std::min(520.f, cam.w - 2 * safeX), ph = 96;
    float px = cam.w / 2 - pw / 2, py = cam.h - safeB - ph;
    ui::panel({px, py, pw, ph});
    world::drawBuildingThumb(placing.type, px + 46, py + ph / 2, 70, 80);
    font::draw(d.name, px + 92, py + 10, 19, kGold);
    font::draw(placing.valid ? "Перетащи на место. Строить можно только в свете." : placing.reason, px + 92, py + 36, 14,
               placing.valid ? kMuted : kBad, font::Left, false);
    if (ui::button({px + 92, py + ph - 46, 190, 38}, "", ui::Style::Green, placing.valid)) confirmPlacing();
    font::draw("Построить", px + 104, py + ph - 37, 16, 0xffffffffu);
    costLabel(cost.first, cost.second, px + 270, py + ph - 37, 16, 0xffffffffu, 2);
    if (ui::button({px + 292, py + ph - 46, 120, 38}, "Отмена", ui::Style::Red)) placing = Placing{};
}

// ---------- модальные окна ----------
bool App::modalFrame(const char* title, float w, float h, ui::Rect& body) {
    ui::block({0, 0, cam.w, cam.h});
    gfx::fillRect(0, 0, cam.w, cam.h, rgba(5, 3, 14, 150));
    w = std::min(w, cam.w - 2 * safeX);
    h = std::min(h, cam.h - 16);
    float x = cam.w / 2 - w / 2, y = cam.h / 2 - h / 2;
    ui::panel({x, y, w, h});
    font::draw(title, x + 22, y + 12, 22, kGold);
    body = {x + 16, y + 50, w - 32, h - 62};
    if (ui::button({x + w - 50, y + 10, 38, 34}, "×", ui::Style::Red, true, 17)) { modal = Modal::None; return false; }
    return true;
}

void App::drawShop() {
    ui::Rect body;
    if (!modalFrame("Строительство", 780, 380, body)) return;
    float tx = body.x;
    for (int i = 0; i < 4; i++) {
        float w = font::measure(kShopTabNames[i], 16) + 28;
        if (ui::button({tx, body.y, w, 34}, kShopTabNames[i], shopTab == i ? ui::Style::Gold : ui::Style::Dark, true, 16)) shopTab = i;
        tx += w + 6;
    }
    int LL = V::lanternLevel(S);
    float cw = 140, ch = body.h - 50, cx = body.x, cy = body.y + 46;
    for (int i = 0; i < 6; i++) {
        lf::BType type = kShopTabs[shopTab][i];
        if (type == lf::BType::Count) break;
        const auto& d = lf::B(type);
        auto can = V::canBuild(S, type, now());
        auto cost = V::buildCost(S, type);
        ui::Rect r{cx, cy, cw, ch};
        ui::panel(r, ui::Style::Dark);
        world::drawBuildingThumb(type, cx + cw / 2, cy + ch * 0.36f, cw - 20, ch * 0.55f);
        font::draw(d.name, cx + cw / 2, cy + ch * 0.66f, 15, can.ok ? kGold : kMuted, font::Center);
        font::draw(std::to_string(V::countOf(S, type)) + "/" + std::to_string(lf::limit(type, LL)) + (d.time[0] ? " · " + fmtTime(d.time[0]) : ""),
                   cx + cw / 2, cy + ch * 0.66f + 20, 12, kMuted, font::Center, false);
        costLabel(cost.first, cost.second, cx + cw / 2, cy + ch - 28, 15, S.res(cost.first) >= cost.second ? 0xffffffffu : kBad, 1);
        if (!can.ok) gfx::fillRect(cx + 4, cy + 4, cw - 8, ch - 8, rgba(10, 6, 20, 110));
        if (ui::tapped(r)) {
            if (!can.ok) toast(can.reason, kBad);
            else { modal = Modal::None; startPlacing(type); }
        }
        cx += cw + 8;
    }
}

void App::drawArmy() {
    ui::Rect body;
    if (!modalFrame("Армия", 800, 380, body)) return;
    long long tnow = now();
    V::tick(S, tnow);
    int cap = V::campCapacity(S), have = V::armyHousing(S), q = V::queueHousing(S);
    font::draw("Лагеря: " + std::to_string(have) + " + " + std::to_string(q) + " в обучении / " + std::to_string(cap), body.x, body.y, 16, 0xffffffffu);
    ui::bar({body.x + 330, body.y + 4, body.w - 330, 14}, cap ? (float)(have + q) / cap : 0, kGold);
    // очередь
    float qx = body.x, qy = body.y + 28;
    if (S.trainQueue.empty()) font::draw("Очередь пуста — нажми на отряд, чтобы обучить", qx, qy + 6, 14, kMuted, font::Left, false);
    for (size_t i = 0; i < S.trainQueue.size() && i < 14; i++) {
        ui::Rect r{qx, qy, 46, 40};
        ui::panel(r, ui::Style::Dark);
        world::drawTroopThumb(S.trainQueue[i].type, qx + 23, qy + 20, 34);
        if (i == 0) font::draw(fmtTime(S.trainQueue[0].left / std::max(1, V::barracksCount(S))), qx + 23, qy + 26, 11, 0xffffffffu, font::Center);
        if (ui::tapped(r)) V::cancelTrain(S, (int)i);
        qx += 50;
    }
    // карточки
    float cw = (body.w - 5 * 8) / 6, ch = body.h - 80, cx = body.x, cy = body.y + 76;
    for (int i = 0; i < lf::kPlayerTroops; i++) {
        auto type = (lf::TroopType)i;
        const auto& d = lf::T(type);
        auto can = V::canTrain(S, type);
        bool unlocked = V::barracksLevel(S) >= d.unlock;
        ui::Rect r{cx, cy, cw, ch};
        ui::panel(r, ui::Style::Dark);
        world::drawTroopThumb(type, cx + cw / 2, cy + ch * 0.3f, ch * 0.42f);
        font::draw(d.name, cx + cw / 2, cy + ch * 0.55f, 14, unlocked ? kGold : kMuted, font::Center);
        font::draw("x" + std::to_string(S.army[i]), cx + 10, cy + 8, 16, 0xffffffffu);
        font::draw("HP " + std::to_string((int)d.hp) + "  место " + std::to_string(d.housing), cx + cw / 2, cy + ch * 0.55f + 20, 11, kMuted, font::Center, false);
        if (unlocked) costLabel(lf::Res::Aether, d.cost, cx + cw / 2, cy + ch - 26, 14, S.aether >= d.cost ? 0xffffffffu : kBad, 1);
        else font::draw("Казарма " + std::to_string(d.unlock), cx + cw / 2, cy + ch - 26, 13, kBad, font::Center, false);
        if (!can.ok) gfx::fillRect(cx + 4, cy + 4, cw - 8, ch - 8, rgba(10, 6, 20, 90));
        if (ui::tapped(r)) {
            auto res = V::train(S, type, tnow);
            if (!res.ok) toast(res.reason, kBad);
        }
        cx += cw + 8;
    }
}

void App::drawMap() {
    ui::Rect body;
    if (!modalFrame("Поход", 820, 384, body)) return;
    const char* tabs[2] = {"Хроника Мглы", "Набег на соперника"};
    float tx = body.x;
    for (int i = 0; i < 2; i++) {
        float w = font::measure(tabs[i], 16) + 28;
        if (ui::button({tx, body.y, w, 34}, tabs[i], mapTab == i ? ui::Style::Gold : ui::Style::Dark, true, 16)) mapTab = i;
        tx += w + 6;
    }
    float y0 = body.y + 44;
    if (mapTab == 0) {
        const auto& ms = lf::campaign::missions();
        int cols = 4;
        float cw = (body.w - (cols - 1) * 8) / cols, ch = (body.h - 44 - 2 * 8) / 3;
        for (size_t i = 0; i < ms.size(); i++) {
            float cx = body.x + (i % cols) * (cw + 8), cy = y0 + (i / cols) * (ch + 8);
            bool open = lf::campaign::unlocked(S, (int)i);
            int stars = S.campaign.count(ms[i].id) ? S.campaign[ms[i].id] : 0;
            ui::Rect r{cx, cy, cw, ch};
            ui::panel(r, open ? ui::Style::Normal : ui::Style::Dark);
            font::draw(std::to_string(i + 1) + ". " + ms[i].title, cx + 10, cy + 8, 14, open ? kGold : kMuted, font::Left);
            font::draw("Фонарь врага " + std::to_string(ms[i].LL), cx + 10, cy + 30, 12, kMuted, font::Left, false);
            for (int s = 0; s < 3; s++) ui::icon(s < stars ? "icon/star" : "icon/star_off", cx + cw - 64 + s * 20, cy + ch - 18, 20);
            if (!open) font::draw("закрыто", cx + 10, cy + ch - 26, 12, kMuted, font::Left, false);
            if (ui::tapped(r) && open) {
                if (V::armyHousing(S) == 0) { toast("Армия пуста — обучи войска", kBad); continue; }
                modal = Modal::None;
                int idx = (int)i;
                dialogue(ms[i].intro, [this, idx] { startMission(idx); });
            }
        }
    } else {
        int LL = V::lanternLevel(S);
        int cost = lf::kRaidSearchCost[LL - 1];
        float y = y0 + 6;
        for (const auto& l : font::wrap("Другие Хранители и Угасшие копят руду и Эфир. Найди соперника своего уровня, погаси его свет и забери добычу. "
                                        "Высаженные войска расходуются.", 17, body.w - 20)) {
            font::draw(l, body.x + 6, y, 17, 0xffffffffu, font::Left, false);
            y += font::lineHeight(17);
        }
        y += 10;
        font::draw("Армия: " + std::to_string(V::armyHousing(S)) + " мест · Затмений в бою: " + std::to_string(lf::eclipseCharges(LL)), body.x + 6, y, 15, kMuted, font::Left, false);
        ui::Rect br{body.x + body.w / 2 - 150, body.y + body.h - 70, 300, 56};
        if (ui::button(br, "", ui::Style::Gold)) {
            if (V::armyHousing(S) == 0) toast("Армия пуста — обучи войска", kBad);
            else if (S.ore < cost) toast("Не хватает руды на поиск", kBad);
            else { S.ore -= cost; modal = Modal::None; startRaid(); }
        }
        font::draw("Искать соперника", br.x + 22, br.y + 16, 19, 0xffffffffu);
        costLabel(lf::Res::Ore, cost, br.x + br.w - 18, br.y + 17, 18, 0xffffffffu, 2);
    }
}

void App::drawNight() {
    ui::Rect body;
    if (!modalFrame("Ночь Мглы", 620, 330, body)) return;
    bool due = V::nightDue(S, now());
    float y = body.y + 4;
    std::string text = "Каждую ночь Мгла проверяет твой свет. Порождения выходят из тьмы со всех сторон, а оборона сражается сама. "
                       "Если Фонарь устоит до рассвета — получишь Осколки и ресурсы. Разрушенные хранилища теряют часть запасов.";
    for (const auto& l : font::wrap(text, 17, body.w - 10)) { font::draw(l, body.x + 4, y, 17, 0xffffffffu, font::Left, false); y += font::lineHeight(17); }
    y += 8;
    font::draw("Пережито ночей: " + std::to_string(S.nights) + (due ? ". Мгла уже у порога!" : ". Следующая через " + fmtTime((S.nightAt - now()) / 1000.0)),
               body.x + 4, y, 15, kMuted, font::Left, false);
    if (ui::button({body.x + body.w - 260, body.y + body.h - 52, 260, 48}, due ? "Встретить ночь" : "Призвать ночь сейчас", ui::Style::Gold)) {
        modal = Modal::None;
        startNight();
    }
}

void App::drawCodex() {
    ui::Rect body;
    if (!modalFrame("Хроника", 760, 380, body)) return;
    const auto& cx = lf::campaign::codex();
    std::vector<const lf::CodexEntry*> entries;
    for (const auto& k : S.codex) { auto it = cx.find(k); if (it != cx.end()) entries.push_back(&it->second); }
    int perPage = 3, pages = std::max(1, (int)(entries.size() + perPage - 1) / perPage);
    codexPage = std::clamp(codexPage, 0, pages - 1);
    float y = body.y;
    for (int i = codexPage * perPage; i < std::min((int)entries.size(), (codexPage + 1) * perPage); i++) {
        font::draw(entries[i]->title, body.x + 4, y, 18, kGold);
        y += 24;
        for (const auto& l : font::wrap(entries[i]->text, 15, body.w - 10)) { font::draw(l, body.x + 4, y, 15, 0xffffffffu, font::Left, false); y += font::lineHeight(15); }
        y += 10;
    }
    int locked = (int)cx.size() - (int)entries.size();
    float by = body.y + body.h - 40;
    if (locked > 0) font::draw("Ещё " + std::to_string(locked) + " записей скрыто во Мгле", body.x + 4, by + 10, 13, kMuted, font::Left, false);
    if (pages > 1) {
        if (ui::button({body.x + body.w - 200, by, 44, 38}, "<", ui::Style::Dark, codexPage > 0)) codexPage--;
        font::draw(std::to_string(codexPage + 1) + "/" + std::to_string(pages), body.x + body.w - 128, by + 9, 16, 0xffffffffu, font::Center);
        if (ui::button({body.x + body.w - 100, by, 44, 38}, ">", ui::Style::Dark, codexPage < pages - 1)) codexPage++;
    }
    if (ui::button({body.x + body.w - 50, by, 50, 38}, "Пролог", ui::Style::Normal)) { modal = Modal::None; dialogue(lf::campaign::intro()); }
}

void App::drawMenu() {
    ui::Rect body;
    if (!modalFrame("Меню", 560, 330, body)) return;
    float y = body.y;
    font::draw("Атак: " + std::to_string(S.stats.attacks) + ", побед: " + std::to_string(S.stats.wins), body.x + 4, y, 16, 0xffffffffu);
    font::draw("Ночей пережито: " + std::to_string(S.stats.nightsWon) + ", проиграно: " + std::to_string(S.stats.nightsLost), body.x + 4, y + 24, 16, 0xffffffffu);
    font::draw("Lanternfall 0.2 · свой движок на C++", body.x + 4, y + 52, 13, kMuted, font::Left, false);
    if (ui::button({body.x, body.y + body.h - 112, body.w, 50}, "Режим разработчика: ресурсы и стройка", ui::Style::Normal, true, 16)) {
        V::addRes(S, lf::Res::Ore, 1000000000);
        V::addRes(S, lf::Res::Aether, 1000000000);
        S.shards += 500;
        for (auto& b : S.buildings) if (b.busyUntil) b.busyUntil = now();
        V::tick(S, now());
        save();
        toast("Казна полна", kGold);
    }
    if (ui::button({body.x, body.y + body.h - 54, body.w, 50}, resetArmed ? "Точно стереть весь прогресс?" : "Начать заново", ui::Style::Red, true, 16)) {
        if (!resetArmed) resetArmed = true;
        else {
            S = V::create(now());
            S.started = true;
            save();
            enterVillage();
            dialogue(lf::campaign::intro());
        }
    }
}

}  // namespace client
