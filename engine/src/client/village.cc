// Сцена деревни.
#include <algorithm>
#include <cmath>

#include "app.h"
#include "engine/atlas.h"
#include "engine/audio.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "world.h"

namespace client {
using gfx::rgba;
namespace V = lf::village;

namespace {
const uint32_t kInk = rgba(70, 44, 30), kInkSoft = rgba(120, 92, 66);
const uint32_t kGold = rgba(255, 216, 107), kMuted = rgba(190, 178, 215), kOre = rgba(240, 160, 80), kAether = rgba(134, 220, 255), kBad = rgba(255, 140, 130);

const char* resIcon(lf::Res r) { return r == lf::Res::Ore ? "icon/ore" : r == lf::Res::Aether ? "icon/aether" : "icon/shard"; }

// «⛏ 1200» — иконка ресурса и число.
float costLabel(lf::Res r, long long amount, float x, float y, float size, uint32_t color = 0xffffffffu, int align = 0) {
    std::string s = fmtNum(amount);
    float w = size + 4 + font::measure(s, size);
    if (align == 1) x -= w / 2;
    else if (align == 2) x -= w;
    ui::icon(resIcon(r), x + size / 2, y + size * 0.62f, size * 1.25f);
    bool light = ((color & 255) + ((color >> 8) & 255) + ((color >> 16) & 255)) > 450;
    font::draw(s, x + size + 4, y, size, color, font::Left, light);
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
            if (got > 0) {
                audio::play(rec->def().produces == lf::Res::Ore ? "collect_ore" : "collect_aether");
                float s = (float)rec->def().size;
                spawnFlyers(rec->def().produces, cam.toScreen(rec->x + s / 2, rec->y + s / 2, 60), got);
            }
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
    audio::play("build", 0.8f, 0.9f + (id % 5) * 0.05f);
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
    if (modal != Modal::None) {
        ui::block({0, 0, cam.w, cam.h});
        gfx::fillRect(0, 0, cam.w, cam.h, rgba(5, 3, 14, 150));
        float k = std::min(1.f, (t - modalT) / 0.28f);
        float back = 1 + 2.2f * (k - 1) * (k - 1) * (k - 1) + 1.2f * (k - 1) * (k - 1);  // easeOutBack
        gfx::pushScale(cam.w / 2, cam.h / 2, 0.75f + 0.25f * back, std::min(1.f, k * 2.5f));
    }
    switch (modal) {
        case Modal::Info: drawInfo(); break;
        case Modal::Shop: drawShop(); break;
        case Modal::Army: drawArmy(); break;
        case Modal::Map: drawMap(); break;
        case Modal::Night: drawNight(); break;
        case Modal::Codex: drawCodex(); break;
        case Modal::Menu: drawMenu(); break;
        default: break;
    }
    if (modal != Modal::None) gfx::popScale();
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
    auto shown = [&](lf::Res r) -> double& { return r == lf::Res::Ore ? shownOre : r == lf::Res::Aether ? shownAether : shownShards; };
    auto resBar = [&](lf::Res r, uint32_t color) {
        float w = 150;
        rx -= w;
        long long c = V::cap(S, r);
        double& sv = shown(r);
        bool flying = std::any_of(flyers.begin(), flyers.end(), [&](const Flyer& f) { return f.res == r; });
        if (sv < 0) sv = (double)S.res(r);
        else if (!flying) sv += ((double)S.res(r) - sv) * 0.12;
        long long v = (long long)std::llround(sv);
        ui::panel({rx, y, w, 40}, ui::Style::Dark);
        ui::bar({rx + 10, y + 22, w - 46, 12}, c > 0 ? (float)v / c : 0, color);
        font::draw(fmtNum(v), rx + w - 42, y + 2, 17, 0xffffffffu, font::Right);
        ui::icon(resIcon(r), rx + w - 18, y + 20, 38);
        resAnchor[(int)r] = {rx + w - 18, y + 20};
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
    if (selected >= 0 && V::get(S, selected) && modal == Modal::None) { drawSelectedPanel(); return; }

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
            audio::play("open", 0.6f);
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
    float by = cam.h - safeB - 50;
    // заголовок над кнопками
    std::string title = std::string(d.name) + (b->level ? "  (ур. " + std::to_string(b->level) + ")" : "  (строится)");
    font::draw(title, cam.w / 2, by - 68, 22, kGold, font::Center);
    if (busy) font::draw("Готово через " + fmtTime((b->busyUntil - tnow) / 1000.0), cam.w / 2, by - 38, 15, 0xffffffffu, font::Center);
    struct Btn { const char* icon; std::string label; ui::Style style; bool enabled; int action; lf::Res res; int cost; };
    std::vector<Btn> btns;
    btns.push_back({"icon/codex", "Инфо", ui::Style::Normal, true, 0, lf::Res::None, 0});
    if (busy) btns.push_back({"icon/shard", std::to_string(V::speedUpCost(*b, tnow)), ui::Style::Gold, S.shards >= V::speedUpCost(*b, tnow), 2, lf::Res::None, 0});
    else if (!up.max) btns.push_back({"icon/upgrade", up.cost ? fmtNum(up.cost) : "Улучшить", up.ok ? ui::Style::Gold : ui::Style::Normal, true, 1, up.res, up.cost});
    double amount = d.produces != lf::Res::None && b->level >= 1 ? V::collectorAmount(*b, tnow) : 0;
    if (d.produces != lf::Res::None && b->level >= 1 && !busy)
        btns.push_back({d.produces == lf::Res::Ore ? "icon/ore" : "icon/aether", fmtNum((long long)amount), ui::Style::Green, amount >= 1, 3, lf::Res::None, 0});
    if (b->type == lf::BType::Barracks || b->type == lf::BType::Camp) btns.push_back({"icon/army", "Армия", ui::Style::Normal, true, 4, lf::Res::None, 0});
    float gap = 86, x0 = cam.w / 2 - (btns.size() - 1) * gap / 2;
    for (size_t i = 0; i < btns.size(); i++) {
        const auto& bt = btns[i];
        float cx = x0 + i * gap;
        if (ui::roundButton(cx, by, 32, bt.icon, bt.label, bt.style, bt.enabled)) {
            switch (bt.action) {
                case 0: modal = Modal::Info; break;
                case 1: {
                    auto r = V::startUpgrade(S, b->id, tnow);
                    if (!r.ok) toast(r.reason, kBad);
                    else { audio::play("upgrade"); toast("Строители взялись за работу", kGold); save(); }
                    break;
                }
                case 2: {
                    auto r = V::speedUp(S, b->id, tnow);
                    if (!r.ok) toast(r.reason, kBad); else { audio::play("done"); toast("Готово!", kGold); save(); }
                    break;
                }
                case 3: {
                    long long got = V::collect(S, b->id, tnow);
                    if (got > 0) {
                        audio::play(d.produces == lf::Res::Ore ? "collect_ore" : "collect_aether");
                        float s = (float)d.size;
                        spawnFlyers(d.produces, cam.toScreen(b->x + s / 2, b->y + s / 2, 60), got);
                    } else toast("Хранилища полны", kBad);
                    break;
                }
                case 4: modal = Modal::Army; selected = -1; break;
            }
        }
        if (bt.res != lf::Res::None && bt.cost > 0) {
            float w = font::measure(bt.label, 14);
            ui::icon(bt.res == lf::Res::Ore ? "icon/ore" : "icon/aether", cx - w / 2 - 11, by + 32 + 8, 18);
        }
    }
    if (!up.ok && !up.max && !busy) font::draw(up.reason, cam.w / 2, by - 40, 15, kBad, font::Center);
}

void App::drawInfo() {
    auto* b = V::get(S, selected >= 0 ? selected : -1);
    if (!b) { modal = Modal::None; return; }
    const auto& d = b->def();
    ui::Rect body;
    std::string title = std::string(d.name) + " — ур. " + std::to_string(std::max(1, b->level));
    if (!modalFrame(title.c_str(), 640, 340, body)) return;
    world::drawBuildingThumb(b->type, body.x + 90, body.y + body.h / 2, 170, body.h - 10, std::max(1, b->level));
    float tx = body.x + 190, ty = body.y + 4, tw = body.w - 200;
    for (const auto& l : font::wrap(d.desc, 15, tw)) { font::draw(l, tx, ty, 15, kInk, font::Left, false); ty += font::lineHeight(15); }
    ty += 10;
    int lv = std::max(1, b->level), nx = std::min(d.maxLevel, lv + 1);
    bool hasNext = nx > lv;
    auto row = [&](const std::string& name, const std::string& cur, const std::string& nxt) {
        ui::panel({tx, ty, tw, 30}, ui::Style::Card);
        font::draw(name, tx + 12, ty + 6, 15, kInk, font::Left, false);
        font::draw(cur, tx + tw * 0.62f, ty + 6, 15, kInk, font::Left, false);
        if (hasNext && nxt != cur) font::draw("» " + nxt, tx + tw * 0.78f, ty + 6, 15, rgba(40, 130, 40), font::Left, false);
        ty += 34;
    };
    char a[32], c[32];
    row("Прочность", std::to_string(d.lv(d.hp, lv)), std::to_string(d.lv(d.hp, nx)));
    if (d.isLight()) { std::snprintf(a, 32, "%.1f", d.lv(d.light, lv)); std::snprintf(c, 32, "%.1f", d.lv(d.light, nx)); row("Радиус света", a, c); }
    if (d.produces != lf::Res::None) row("Добыча в минуту", std::to_string(d.lv(d.rate, lv)), std::to_string(d.lv(d.rate, nx)));
    if (d.produces != lf::Res::None) row("Вместимость", fmtNum(d.lv(d.cap, lv)), fmtNum(d.lv(d.cap, nx)));
    if (!d.storage.empty()) row("Хранит", fmtNum(d.lv(d.storage, lv)), fmtNum(d.lv(d.storage, nx)));
    if (!d.dmg.empty()) row("Урон за выстрел", std::to_string(d.lv(d.dmg, lv)), std::to_string(d.lv(d.dmg, nx)));
    if (!d.dps.empty()) row("Урон луча в секунду", std::to_string((int)d.lv(d.dps, lv)), std::to_string((int)d.lv(d.dps, nx)));
    if (d.range > 0) { std::snprintf(a, 32, "%.1f", d.range); row("Дальность", a, a); }
    if (!d.capacity.empty()) row("Мест для армии", std::to_string(d.lv(d.capacity, lv)), std::to_string(d.lv(d.capacity, nx)));
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
    w = std::min(w, cam.w - 2 * safeX);
    h = std::min(h, cam.h - 26);
    float x = cam.w / 2 - w / 2, y = cam.h / 2 - h / 2 + 8;
    ui::panel({x, y, w, h}, ui::Style::Paper);
    ui::ribbon(title, cam.w / 2, y - 18, std::max(220.f, font::measure(title, 21) + 70));
    body = {x + 18, y + 32, w - 36, h - 46};
    if (ui::button({x + w - 34, y - 14, 46, 46}, "×", ui::Style::Red, true, 22)) { modal = Modal::None; audio::play("close", 0.6f); return false; }
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
        atlas::draw9(atlas::get(can.ok ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 24, 12);
        world::drawBuildingThumb(type, cx + cw / 2, cy + ch * 0.36f, cw - 8, ch * 0.62f);
        font::draw(d.name, cx + cw / 2, cy + ch * 0.68f, 16, kInk, font::Center, false);
        font::draw(std::to_string(V::countOf(S, type)) + "/" + std::to_string(lf::limit(type, LL)) + (d.time[0] ? " · " + fmtTime(d.time[0]) : ""),
                   cx + cw / 2, cy + ch * 0.68f + 20, 12, kInkSoft, font::Center, false);
        costLabel(cost.first, cost.second, cx + cw / 2, cy + ch - 28, 16, S.res(cost.first) >= cost.second ? kInk : rgba(200, 50, 40), 1);
        if (!can.ok) font::draw(can.reason, cx + cw / 2, cy + 8, 11, rgba(180, 40, 30), font::Center, false);
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
    font::draw("Лагеря: " + std::to_string(have) + " + " + std::to_string(q) + " в обучении / " + std::to_string(cap), body.x, body.y, 16, kInk, font::Left, false);
    ui::bar({body.x + 330, body.y + 4, body.w - 330, 14}, cap ? (float)(have + q) / cap : 0, kGold);
    // очередь
    float qx = body.x, qy = body.y + 28;
    if (S.trainQueue.empty()) font::draw("Очередь пуста — нажми на отряд, чтобы обучить", qx, qy + 6, 14, kInkSoft, font::Left, false);
    for (size_t i = 0; i < S.trainQueue.size() && i < 14; i++) {
        ui::Rect r{qx, qy, 46, 40};
        atlas::draw9(atlas::get("ui/card"), qx, qy, 46, 40, 24, 10);
        world::drawTroopThumb(S.trainQueue[i].type, qx + 23, qy + 20, 34);
        if (i == 0) font::draw(fmtTime(S.trainQueue[0].left / std::max(1, V::barracksCount(S))), qx + 23, qy + 26, 11, kInk, font::Center, false);
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
        atlas::draw9(atlas::get(unlocked ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 24, 12);
        world::drawTroopThumb(type, cx + cw / 2, cy + ch * 0.32f, ch * 0.5f);
        font::draw(d.name, cx + cw / 2, cy + ch * 0.58f, 15, kInk, font::Center, false);
        font::draw("x" + std::to_string(S.army[i]), cx + 10, cy + 8, 17, rgba(255, 255, 255));
        font::draw("HP " + std::to_string((int)d.hp) + " · место " + std::to_string(d.housing), cx + cw / 2, cy + ch * 0.58f + 20, 11, kInkSoft, font::Center, false);
        if (unlocked) costLabel(lf::Res::Aether, d.cost, cx + cw / 2, cy + ch - 26, 15, S.aether >= d.cost ? kInk : rgba(200, 50, 40), 1);
        else font::draw("Казарма ур. " + std::to_string(d.unlock), cx + cw / 2, cy + ch - 26, 13, rgba(180, 40, 30), font::Center, false);
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
            atlas::draw9(atlas::get(open ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 24, 12);
            font::draw(std::to_string(i + 1) + ". " + ms[i].title, cx + 10, cy + 8, 14, kInk, font::Left, false);
            font::draw("Фонарь врага " + std::to_string(ms[i].LL), cx + 10, cy + 30, 12, kInkSoft, font::Left, false);
            for (int s = 0; s < 3; s++) ui::icon(s < stars ? "icon/star" : "icon/star_off", cx + cw - 64 + s * 20, cy + ch - 18, 20);
            if (!open) font::draw("закрыто", cx + 10, cy + ch - 26, 12, kInkSoft, font::Left, false);
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
            font::draw(l, body.x + 6, y, 17, kInk, font::Left, false);
            y += font::lineHeight(17);
        }
        y += 10;
        font::draw("Армия: " + std::to_string(V::armyHousing(S)) + " мест · Затмений в бою: " + std::to_string(lf::eclipseCharges(LL)), body.x + 6, y, 15, kInkSoft, font::Left, false);
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
    for (const auto& l : font::wrap(text, 17, body.w - 10)) { font::draw(l, body.x + 4, y, 17, kInk, font::Left, false); y += font::lineHeight(17); }
    y += 8;
    font::draw("Пережито ночей: " + std::to_string(S.nights) + (due ? ". Мгла уже у порога!" : ". Следующая через " + fmtTime((S.nightAt - now()) / 1000.0)),
               body.x + 4, y, 15, kInkSoft, font::Left, false);
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
        font::draw(entries[i]->title, body.x + 4, y, 19, rgba(150, 80, 30), font::Left, false);
        y += 24;
        for (const auto& l : font::wrap(entries[i]->text, 15, body.w - 10)) { font::draw(l, body.x + 4, y, 15, kInk, font::Left, false); y += font::lineHeight(15); }
        y += 10;
    }
    int locked = (int)cx.size() - (int)entries.size();
    float by = body.y + body.h - 40;
    if (locked > 0) font::draw("Ещё " + std::to_string(locked) + " записей скрыто во Мгле", body.x + 130, by + 10, 13, kInkSoft, font::Left, false);
    if (pages > 1) {
        if (ui::button({body.x + body.w - 200, by, 44, 38}, "<", ui::Style::Dark, codexPage > 0)) codexPage--;
        font::draw(std::to_string(codexPage + 1) + "/" + std::to_string(pages), body.x + body.w - 128, by + 9, 16, kInk, font::Center, false);
        if (ui::button({body.x + body.w - 100, by, 44, 38}, ">", ui::Style::Dark, codexPage < pages - 1)) codexPage++;
    }
    if (ui::button({body.x, by, 120, 38}, "Пролог", ui::Style::Normal)) { modal = Modal::None; dialogue(lf::campaign::intro()); }
}

void App::drawMenu() {
    ui::Rect body;
    if (!modalFrame("Меню", 560, 330, body)) return;
    float y = body.y;
    font::draw("Атак: " + std::to_string(S.stats.attacks) + ", побед: " + std::to_string(S.stats.wins), body.x + 4, y, 16, kInk, font::Left, false);
    font::draw("Ночей пережито: " + std::to_string(S.stats.nightsWon) + ", проиграно: " + std::to_string(S.stats.nightsLost), body.x + 4, y + 24, 16, kInk, font::Left, false);
    font::draw("Lanternfall 0.2 · свой движок на C++", body.x + 4, y + 52, 13, kInkSoft, font::Left, false);
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
