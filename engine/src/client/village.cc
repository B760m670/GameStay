// Сцена деревни.
#include <algorithm>
#include <cmath>

#include "app.h"
#include "engine/atlas.h"
#include "engine/audio.h"
#include "platform/platform.h"
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
    {lf::BType::Ballista, lf::BType::Brazier, lf::BType::Catapult, lf::BType::Prism, lf::BType::Trap, lf::BType::Count},
    {lf::BType::Beacon, lf::BType::Wall, lf::BType::Count, lf::BType::Count, lf::BType::Count, lf::BType::Count},
    {lf::BType::OreMine, lf::BType::AetherWell, lf::BType::OreStore, lf::BType::AetherStore, lf::BType::BuilderHut, lf::BType::Count},
    {lf::BType::Barracks, lf::BType::Camp, lf::BType::Workshop, lf::BType::Count, lf::BType::Count, lf::BType::Count},
};
const char* kShopTabNames[4] = {"Оборона", "Свет и стены", "Экономика", "Армия"};
}  // namespace

void App::enterVillage() {
    audio::music("village", 0.42f);
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
        selectedOb = -1;
        if (b < 0) {
            selected = -1;
            if (const lf::Obstacle* o = V::obstacleAtCell(S, (int)std::floor(w.x), (int)std::floor(w.y))) selectedOb = o->id;
            return;
        }
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

// ---------- жизнь деревни ----------
void App::updateLife(float dt) {
    long long tnow = now();
    static uint32_t seed = 12345;
    auto rnd = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.f; };
    auto center = [](const lf::BuildingRec& b) { float s = b.def().size; return Vec2{b.x + s / 2, b.y + s / 2}; };
    // рабочие: по одному на каждую стройку, остальные у хижин
    std::vector<const lf::BuildingRec*> huts, jobs;
    for (const auto& b : S.buildings) {
        if (b.type == lf::BType::BuilderHut && b.level >= 1) huts.push_back(&b);
        if (b.busyUntil > tnow) jobs.push_back(&b);
    }
    while (workers.size() < huts.size()) {
        Vec2 c = center(*huts[workers.size()]);
        workers.push_back({"worker", c.x + 1.2f, c.y + 1.2f, c.x + 1.2f, c.y + 1.2f});
    }
    if (workers.size() > huts.size()) workers.resize(huts.size());
    for (size_t i = 0; i < workers.size(); i++) {
        Actor& a = workers[i];
        const lf::BuildingRec* job = i < jobs.size() ? jobs[i] : nullptr;
        if (job) {
            Vec2 c = center(*job);
            float s = job->def().size;
            a.tx = c.x + s / 2 + 0.3f;  // у правого угла постройки
            a.ty = c.y;
        } else if (!huts.empty()) {
            Vec2 c = center(*huts[i % huts.size()]);
            if (std::hypot(a.tx - c.x, a.ty - c.y) > 2.5f || (a.wait <= 0 && std::hypot(a.x - a.tx, a.y - a.ty) < 0.1f)) {
                a.tx = c.x + 1.0f + rnd() * 1.2f;
                a.ty = c.y + 0.5f + rnd() * 1.4f;
                a.wait = 1 + rnd() * 3;
            }
        }
        float dx = a.tx - a.x, dy = a.ty - a.y, d = std::hypot(dx, dy);
        a.anim += dt;
        if (d > 0.05f) {
            float step = std::min(d, 1.3f * dt);
            a.x += dx / d * step;
            a.y += dy / d * step;
            a.angle = std::atan2(dy, dx);
            a.working = false;
        } else {
            a.working = job != nullptr;
            if (job) {
                Vec2 c = center(*job);
                a.angle = std::atan2(c.y - a.y, c.x - a.x);
                if (std::fmod(a.anim, 0.5f) < dt) {  // искры от молота
                    particles.push_back({a.x - 0.2f, a.y, 18, (rnd() - 0.5f) * 0.6f, (rnd() - 0.5f) * 0.6f, 40, 0.4f, 0.4f, 6, gfx::rgba(255, 210, 120), true});
                }
            }
            a.wait -= dt;
        }
    }
    // войска у лагеря
    const lf::BuildingRec* camp = nullptr;
    for (const auto& b : S.buildings) if (b.type == lf::BType::Camp && b.level >= 1) { camp = &b; break; }
    size_t want = 0;
    std::vector<int> kinds;
    for (int i = 0; i < lf::kPlayerTroops; i++)
        for (int k = 0; k < std::min(S.army[i], 4); k++) kinds.push_back(i);
    want = camp ? std::min<size_t>(kinds.size(), 12) : 0;
    if (idlers.size() != want) {
        idlers.clear();
        for (size_t i = 0; i < want; i++) {
            Vec2 c = center(*camp);
            Actor a{lf::T((lf::TroopType)kinds[i]).id, c.x + (rnd() - 0.5f) * 3, c.y + (rnd() - 0.5f) * 3, 0, 0};
            a.tx = a.x; a.ty = a.y; a.job = kinds[i]; a.wait = rnd() * 3;
            idlers.push_back(a);
        }
    }
    for (auto& a : idlers) {
        a.anim += dt;
        float dx = a.tx - a.x, dy = a.ty - a.y, d = std::hypot(dx, dy);
        if (d > 0.05f) {
            float step = std::min(d, 0.8f * dt);
            a.x += dx / d * step;
            a.y += dy / d * step;
            a.angle = std::atan2(dy, dx);
        } else if ((a.wait -= dt) <= 0 && camp) {
            Vec2 c = center(*camp);
            a.tx = c.x + (rnd() - 0.5f) * 4.2f;
            a.ty = c.y + (rnd() - 0.5f) * 4.2f;
            a.wait = 1.5f + rnd() * 4;
        }
    }
    // Фонарщик прогуливается у Фонаря
    bool on = V::heroUnlocked(S) && !S.buildings.empty();
    if (on) {
        Vec2 c = center(S.buildings[0]);
        if (!heroOn) { heroA.x = heroA.tx = c.x + 2.8f; heroA.y = heroA.ty = c.y + 1.f; heroA.wait = 2; }
        Actor& a = heroA;
        a.anim += dt;
        float dx = a.tx - a.x, dy = a.ty - a.y, d = std::hypot(dx, dy);
        if (d > 0.05f) {
            float step = std::min(d, 0.7f * dt);
            a.x += dx / d * step;
            a.y += dy / d * step;
            a.angle = std::atan2(dy, dx);
        } else if ((a.wait -= dt) <= 0) {
            float ang = rnd() * 6.283f, r = 2.6f + rnd() * 1.2f;
            a.tx = c.x + std::cos(ang) * r;
            a.ty = c.y + std::sin(ang) * r;
            a.wait = 2.f + rnd() * 4;
        }
    }
    heroOn = on;
    // частицы
    emitT += dt;
    if (emitT > 0.08f) {
        emitT = 0;
        for (const auto& b : S.buildings) {
            if (b.level < 1) continue;
            Vec2 c = center(b);
            switch (b.type) {
                case lf::BType::Lantern:
                    if (rnd() < 0.6f) particles.push_back({c.x + (rnd() - 0.5f) * 0.6f, c.y + (rnd() - 0.5f) * 0.6f, 110, 0, 0, 25 + rnd() * 20, 1.4f, 1.4f, 5, gfx::rgba(255, 190, 90, 220), true});
                    break;
                case lf::BType::Workshop:
                case lf::BType::Camp:
                case lf::BType::Barracks:
                    if (rnd() < 0.35f) particles.push_back({c.x - 0.3f, c.y - 0.3f, b.type == lf::BType::Barracks ? 85.f : 40.f, 0.15f, -0.1f, 22, 2.6f, 2.6f, 9, gfx::rgba(150, 140, 160, 120), false});
                    break;
                case lf::BType::AetherWell:
                case lf::BType::AetherStore:
                case lf::BType::Beacon:
                    if (rnd() < 0.3f) particles.push_back({c.x + (rnd() - 0.5f) * 1.2f, c.y + (rnd() - 0.5f) * 1.2f, 20 + rnd() * 30, 0, 0, 18, 1.2f, 1.2f, 6, gfx::rgba(130, 220, 255, 220), true});
                    break;
                default: break;
            }
        }
        // светлячки во тьме вокруг поселения
        if (particles.size() < 400 && rnd() < 0.7f) {
            float a = rnd() * 6.283f, r = 10 + rnd() * 14;
            particles.push_back({22 + std::cos(a) * r, 22 + std::sin(a) * r, 10 + rnd() * 30, (rnd() - 0.5f) * 0.4f, (rnd() - 0.5f) * 0.4f, (rnd() - 0.5f) * 6, 4, 4, 5, gfx::rgba(200, 255, 140, 200), true});
        }
    }
    world::updateParticles(particles, dt);
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
    updateLife(dt);
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
        case Modal::Tasks: drawTasks(); break;
        case Modal::Workshop: drawWorkshop(); break;
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
    for (const auto& b : view.buildings) if (b.type != lf::BType::Wall && b.type != lf::BType::Trap) world::drawDirt(cam, (float)b.x, (float)b.y, (float)b.def().size);
    world::drawMeadow(cam, t);
    world::drawFog(cam, V::sources(view), {}, t, 0.5f);
    world::drawWaterFx(cam, t);
    if (placing.active) {
        float s = (float)lf::B(placing.type).size;
        world::drawGridAround(cam, placing.x + s / 2, placing.y + s / 2, 7.f);
    }
    struct Item { float depth; const lf::BuildingRec* b; bool ghost; const world::Decor* d = nullptr; int actor = -1; };
    std::vector<Item> items;
    for (size_t i = 0; i < workers.size(); i++) items.push_back({workers[i].x + workers[i].y, nullptr, false, nullptr, (int)i});
    for (size_t i = 0; i < idlers.size(); i++) items.push_back({idlers[i].x + idlers[i].y, nullptr, false, nullptr, 1000 + (int)i});
    if (heroOn) items.push_back({heroA.x + heroA.y, nullptr, false, nullptr, 2000});
    for (size_t i = 0; i < S.obstacles.size(); i++) {
        const auto& o = S.obstacles[i];
        items.push_back({(float)(o.x + o.y + V::obstacleSize(o.kind)), nullptr, false, nullptr, 3000 + (int)i});
    }
    for (const auto& b : view.buildings) items.push_back({(float)(b.x + b.y + b.def().size), &b, false});
    for (const auto& d : world::decor()) items.push_back({d.depth, nullptr, false, &d});
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
        if (it.d) { world::drawDecor(cam, *it.d, t); continue; }
        if (it.actor >= 3000) {
            const auto& o = S.obstacles[it.actor - 3000];
            static const char* trees[] = {"b/decor_oak", "b/decor_fat", "b/decor_pine", "b/decor_detailed"};
            const char* sp = o.kind == lf::ObKind::Tree ? trees[o.id % 4] : o.kind == lf::ObKind::Rock ? "b/decor_rock_large"
                           : o.kind == lf::ObKind::Stump ? "b/decor_stump" : o.kind == lf::ObKind::Flowers ? "b/decor_flowers" : "b/decor_bush2";
            int sz = V::obstacleSize(o.kind);
            if (o.id == selectedOb) {
                float pulse = 0.5f + 0.5f * std::sin(t * 5);
                world::drawFootprint(cam, o.x - 0.1f, o.y - 0.1f, sz + 0.2f, rgba(255, 230, 120, (int)(50 + 50 * pulse)));
            }
            world::drawDecor(cam, world::Decor{sp, (float)o.x, (float)o.y, sz, 0}, t);
            continue;
        }
        if (it.actor >= 0) {
            const Actor& a = it.actor == 2000 ? heroA : it.actor >= 1000 ? idlers[it.actor - 1000] : workers[it.actor];
            bool still = !a.working && std::hypot(a.tx - a.x, a.ty - a.y) <= 0.05f;
            int frame = a.working ? (int)(a.anim * 9) % 6 : still ? (int)(a.anim * 4) % 4 : (int)(a.anim * 12) % 8;
            std::string anim = a.working ? "attack" : still ? "idle" : "move";
            world::drawUnit(cam, a.unit, anim, a.angle, frame, a.x, a.y);
            continue;
        }
        const auto& b = *it.b;
        world::BuildingLook look;
        look.selected = b.id == selected && !placing.active;
        look.constructing = b.level == 0;
        if (it.ghost || (placing.active && b.id == placing.moveId)) { look.ghost = placing.valid ? 1 : -1; look.alpha = 0.85f; }
        world::drawBuilding(cam, b.type, (float)b.x, (float)b.y, std::max(1, b.level), look, t);
    }
    for (const auto& p : particles) if (!p.additive) world::drawParticle(cam, p);
    gfx::setBlend(gfx::Blend::Add);
    for (const auto& p : particles) if (p.additive) world::drawParticle(cam, p);
    gfx::setBlend(gfx::Blend::Alpha);
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
    // слева: щит с уровнем игрока и шкалой опыта, затем Фонарь, строители, армия
    float x = safeX, y = 10;
    {
        float bw = 112;
        long long need = V::xpToNext(S.level);
        ui::bar({x + 40, y + 16, bw - 18, 16}, (float)S.xp / need, rgba(110, 210, 255));
        font::draw(fmtNum(S.xp) + "/" + fmtNum(need), x + 40 + (bw - 18) / 2, y + 15, 12, 0xffffffffu, font::Center);
        atlas::drawRect(atlas::get("ui/badge"), x - 6, y - 6, 54, 54);
        font::draw(std::to_string(S.level), x + 21, y + 6, 22, 0xffffffffu, font::Center);
        x += bw + 34;
    }
    auto chip = [&](const char* ic, const std::string& text, float w) {
        ui::panel({x, y, w, 40}, ui::Style::Dark);
        ui::icon(ic, x + 22, y + 20, 30);
        font::draw(text, x + 42, y + 10, 17, 0xffffffffu);
        x += w + 8;
    };
    chip("icon/lantern", std::to_string(V::lanternLevel(S)), 62);
    auto bl = V::builders(S, tnow);
    chip("icon/builder", std::to_string(bl.free) + "/" + std::to_string(bl.total), 74);
    chip("icon/army", std::to_string(V::armyHousing(S)) + "/" + std::to_string(V::campCapacity(S)), 84);
    // справа: ресурсы
    float rx = cam.w - safeX;
    auto shown = [&](lf::Res r) -> double& { return r == lf::Res::Ore ? shownOre : r == lf::Res::Aether ? shownAether : shownShards; };
    auto resBar = [&](lf::Res r, uint32_t color) {
        float w = 128;
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
        float w = 70;
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
    if (selectedOb >= 0 && modal == Modal::None) {
        bool exists = false;
        for (const auto& o : S.obstacles) exists = exists || o.id == selectedOb;
        if (exists) { drawObstaclePanel(); return; }
        selectedOb = -1;
    }

    // нижняя панель: большие «Поход» и «Строить» по краям, остальное — на деревянной полке
    bool due = V::nightDue(S, tnow);
    int ready = V::tasksReady(S);
    auto open = [&](Modal m) {
        modal = m;
        audio::play("open", 0.6f);
        selected = -1;
        selectedOb = -1;
        resetArmed = false;
    };
    const float R = 38, cyB = cam.h - safeB - R - 14;
    float pulse = 1 + 0.03f * std::sin(t * 3);
    gfx::pushScale(safeX + R + 6, cyB, pulse);
    bool attack = ui::roundButton(safeX + R + 6, cyB, R, "icon/map", "Поход", ui::Style::Red, true);
    gfx::popScale();
    if (attack) open(Modal::Map);
    if (ui::roundButton(cam.w - safeX - R - 6, cyB, R, "icon/build", "Строить", ui::Style::Gold, true)) open(Modal::Shop);
    const float bw = 66, bh = 58, gap = 6;
    const int n = 5;
    float total = n * bw + (n - 1) * gap;
    float bx = cam.w / 2 - total / 2, by = cam.h - bh - safeB - 4;
    atlas::draw9(atlas::get("ui/shelf"), bx - 18, by + bh - 26, total + 36, 34, 50, 16);
    struct B { const char* icon; std::string label; Modal m; ui::Style st; };
    B btns[n] = {{"icon/army", "Армия", Modal::Army, ui::Style::Normal},
                 {"icon/night", due ? "Мгла!" : fmtTime((S.nightAt - tnow) / 1000.0), Modal::Night, due ? ui::Style::Red : ui::Style::Dark},
                 {"icon/star", "Задания", Modal::Tasks, ui::Style::Normal},
                 {"icon/codex", "Хроника", Modal::Codex, ui::Style::Normal},
                 {"icon/menu", "Меню", Modal::Menu, ui::Style::Dark}};
    for (int i = 0; i < n; i++) {
        bool hl = (i == 1 && due && std::fmod(t, 1.2f) < 0.8f) || (i == 2 && ready > 0 && std::fmod(t, 1.4f) < 0.9f);
        ui::Rect r{bx + i * (bw + gap), by, bw, bh};
        if (ui::iconButton(r, btns[i].icon, btns[i].label, btns[i].st, hl)) open(btns[i].m);
        if (i == 2 && ready > 0) {  // значок «есть награда»
            atlas::drawRect(atlas::get("fx/dot"), r.x + r.w - 20, r.y - 8, 26, 26, rgba(30, 14, 20));
            atlas::drawRect(atlas::get("fx/dot"), r.x + r.w - 18, r.y - 6, 22, 22, rgba(230, 60, 50));
            font::draw(std::to_string(ready), r.x + r.w - 7, r.y - 4, 14, 0xffffffffu, font::Center, false);
        }
    }
}

void App::drawWorkshop() {
    ui::Rect body;
    if (!modalFrame("Кузница", 800, 380, body)) return;
    long long tnow = now();
    V::tick(S, tnow);
    int maxLv = V::troopMaxLevel(S);
    std::string head = "Уровень отрядов до " + std::to_string(maxLv) + " (улучши Кузницу, чтобы поднять предел).";
    if (S.researchType >= 0) {
        head = std::string("Сейчас: ") + lf::T((lf::TroopType)S.researchType).name + " — готово через " + fmtTime((S.researchUntil - tnow) / 1000.0);
        int sc = std::max(1, (int)std::ceil((S.researchUntil - tnow) / 60000.0));
        if (ui::button({body.x + body.w - 180, body.y - 4, 180, 36}, "Ускорить  " + std::to_string(sc), ui::Style::Gold, S.shards >= sc, 15)) {
            S.shards -= sc;
            S.researchUntil = tnow;
            V::tick(S, tnow);
            audio::play("done");
            save();
        }
    }
    font::draw(head, body.x, body.y + 4, 15, kInk, font::Left, false);
    float cw = (body.w - 5 * 8) / 6, ch = body.h - 46, cx = body.x, cy = body.y + 42;
    for (int i = 0; i < lf::kPlayerTroops; i++) {
        auto type = (lf::TroopType)i;
        const auto& d = lf::T(type);
        int lv = S.troopLevel[i];
        bool unlocked = V::barracksLevel(S) >= d.unlock;
        auto can = V::canResearch(S, type, tnow);
        ui::Rect r{cx, cy, cw, ch};
        atlas::draw9(atlas::get(unlocked ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 40, 12);
        world::drawTroopThumb(type, cx + cw / 2, cy + ch * 0.27f, ch * 0.42f);
        font::draw(d.name, cx + cw / 2, cy + ch * 0.5f, 14, kInk, font::Center, false);
        for (int s = 0; s < V::kMaxTroopLevel; s++)
            ui::icon(s < lv ? "icon/star" : "icon/star_off", cx + cw / 2 - 36 + s * 18, cy + ch * 0.5f + 30, 16);
        char buf[48];
        std::snprintf(buf, sizeof buf, "+%d%% силы", (int)std::lround((V::troopMult(lv) - 1) * 100));
        font::draw(buf, cx + cw / 2, cy + ch * 0.5f + 42, 12, kInkSoft, font::Center, false);
        if (S.researchType == i) font::draw("куётся...", cx + cw / 2, cy + ch - 30, 14, rgba(40, 130, 40), font::Center, false);
        else if (lv >= V::kMaxTroopLevel) font::draw("максимум", cx + cw / 2, cy + ch - 30, 13, kInkSoft, font::Center, false);
        else if (unlocked) {
            int cost = V::researchCost(type, lv + 1);
            costLabel(lf::Res::Aether, cost, cx + cw / 2, cy + ch - 46, 14, S.aether >= cost ? kInk : rgba(200, 50, 40), 1);
            font::draw(fmtTime(V::researchTime(type, lv + 1)), cx + cw / 2, cy + ch - 24, 12, kInkSoft, font::Center, false);
        } else font::draw("не открыт", cx + cw / 2, cy + ch - 30, 13, kInkSoft, font::Center, false);
        if (ui::tapped(r) && unlocked && S.researchType != i) {
            auto res = V::startResearch(S, type, tnow);
            if (!res.ok) toast(res.reason, kBad);
            else { audio::play("upgrade"); save(); }
        }
        cx += cw + 8;
    }
}

void App::drawTasks() {
    ui::Rect body;
    if (!modalFrame("Задания", 800, 380, body)) return;
    const auto& defs = V::taskDefs();
    // ready первыми, затем по прогрессу
    std::vector<const lf::TaskDef*> list;
    for (const auto& d : defs) list.push_back(&d);
    std::stable_sort(list.begin(), list.end(), [&](const lf::TaskDef* a, const lf::TaskDef* b) {
        auto rank = [&](const lf::TaskDef* t) { return V::taskReady(S, *t) ? 0 : V::taskClaimed(S, *t) >= 3 ? 2 : 1; };
        return rank(a) < rank(b);
    });
    int perPage = 4, pages = ((int)list.size() + perPage - 1) / perPage;
    codexPage = std::clamp(codexPage, 0, pages - 1);
    float rh = (body.h - 46) / perPage - 6, y = body.y;
    for (int i = codexPage * perPage; i < std::min((int)list.size(), (codexPage + 1) * perPage); i++) {
        const auto& d = *list[i];
        int tier = V::taskClaimed(S, d);
        bool done = tier >= 3, ready = V::taskReady(S, d);
        long long goal = d.goal[std::min(tier, 2)], prog = V::taskProgress(S, d);
        atlas::draw9(atlas::get(done ? "ui/card_dark" : "ui/card"), body.x, y, body.w, rh, 40, 12);
        for (int s = 0; s < 3; s++) ui::icon(s < tier ? "icon/star" : "icon/star_off", body.x + 22 + s * 24, y + rh / 2, 24);
        font::draw(d.title, body.x + 96, y + 6, 17, kInk, font::Left, false);
        char buf[160];
        std::snprintf(buf, sizeof buf, d.desc, fmtNum(goal).c_str());
        font::draw(buf, body.x + 96, y + 28, 13, kInkSoft, font::Left, false);
        float bw2 = body.w - 96 - 220;
        ui::bar({body.x + 96, y + rh - 18, bw2, 12}, done ? 1.f : (float)std::min(prog, goal) / goal, ready || done ? rgba(110, 210, 80) : rgba(120, 200, 255));
        font::draw(done ? "выполнено" : fmtNum(std::min(prog, goal)) + " / " + fmtNum(goal), body.x + 96 + bw2 + 8, y + rh - 22, 13, kInk, font::Left, false);
        if (!done) {
            float rx = body.x + body.w - 120;
            if (ready) {
                if (ui::button({rx, y + rh / 2 - 20, 110, 40}, "Забрать", ui::Style::Green)) {
                    int sh = d.shards[tier];
                    V::claimTask(S, d);
                    audio::play("done");
                    spawnFlyers(lf::Res::Shards, {rx + 55, y + rh / 2}, sh * 40);
                    save();
                }
            } else {
                costLabel(lf::Res::Shards, d.shards[tier], rx + 55, y + rh / 2 - 9, 16, kInk, 1);
            }
        }
        y += rh + 6;
    }
    float by = body.y + body.h - 40;
    if (pages > 1) {
        if (ui::button({body.x + body.w / 2 - 110, by, 44, 38}, "<", ui::Style::Dark, codexPage > 0)) codexPage--;
        font::draw(std::to_string(codexPage + 1) + "/" + std::to_string(pages), body.x + body.w / 2, by + 9, 16, kInk, font::Center, false);
        if (ui::button({body.x + body.w / 2 + 66, by, 44, 38}, ">", ui::Style::Dark, codexPage < pages - 1)) codexPage++;
    }
}

namespace {
float vrand() {
    static uint32_t seed = 777;
    seed = seed * 1664525u + 1013904223u;
    return (seed >> 8) / 16777216.f;
}
}  // namespace

void App::drawObstaclePanel() {
    const lf::Obstacle* ob = nullptr;
    for (const auto& o : S.obstacles) if (o.id == selectedOb) ob = &o;
    if (!ob) return;
    float by = cam.h - safeB - 50;
    font::draw(V::obstacleName(ob->kind), cam.w / 2, by - 68, 22, kGold, font::Center);
    int shards = V::obstacleShards(*ob);
    font::draw(shards > 0 ? "Расчисти место — найдёшь " + std::to_string(shards) + " оск." : "Расчисти место для построек", cam.w / 2, by - 40, 15,
               0xffffffffu, font::Center);
    int cost = V::obstacleCost(ob->kind);
    if (ui::roundButton(cam.w / 2, by, 32, "icon/ore", fmtNum(cost), S.ore >= cost ? ui::Style::Gold : ui::Style::Normal, true)) {
        int s = V::obstacleSize(ob->kind);
        float cx = ob->x + s / 2.f, cy = ob->y + s / 2.f;
        int got = 0;
        auto r = V::clearObstacle(S, ob->id, &got);
        if (!r.ok) { toast(r.reason, kBad); return; }
        audio::play("destroy", 0.6f, 1.3f);
        for (int i = 0; i < 18; i++) {  // листья и пыль
            float a = vrand() * 6.283f, v = 0.6f + vrand() * 1.4f;
            world::Particle p{cx, cy, 10 + vrand() * 30, std::cos(a) * v, std::sin(a) * v, 60 + vrand() * 120, 1.f + vrand() * 0.6f, 1.6f, 3 + vrand() * 3,
                              i % 2 ? rgba(110, 160, 70) : rgba(150, 125, 90), false};
            p.chunk = true;
            particles.push_back(p);
        }
        if (got > 0) {
            spawnFlyers(lf::Res::Shards, cam.toScreen(cx, cy, 30), got * 40);
            toast("+" + std::to_string(got) + " оск.", kGold);
        }
        selectedOb = -1;
        save();
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
    if (b->type == lf::BType::Workshop && b->level >= 1) btns.push_back({"icon/builder", "Усилить", ui::Style::Gold, true, 5, lf::Res::None, 0});
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
                case 5: modal = Modal::Workshop; selected = -1; break;
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
        atlas::draw9(atlas::get(can.ok ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 40, 12);
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
    ui::bar({body.x + 330, body.y + 4, body.w - 330 - 180, 14}, cap ? (float)(have + q) / cap : 0, kGold);
    // заклинания: открываются уровнем игрока
    auto spells = V::spellCharges(S.level);
    float sx = body.x + body.w - 170;
    for (lf::Spell sp : {lf::Spell::Heal, lf::Spell::Rage}) {
        bool open = spells.count(sp) > 0;
        ui::icon(sp == lf::Spell::Heal ? "icon/heal" : "icon/rage", sx + 14, body.y + 10, 28, open ? 0xffffffffu : rgba(120, 110, 130, 200));
        font::draw(open ? "x" + std::to_string(spells[sp]) : "ур. " + std::to_string(V::spellUnlockLevel(sp)), sx + 32, body.y + 2, 14,
                   open ? kInk : kInkSoft, font::Left, false);
        sx += 86;
    }
    // очередь
    float qx = body.x, qy = body.y + 28;
    if (S.trainQueue.empty()) font::draw("Очередь пуста — нажми на отряд, чтобы обучить", qx, qy + 6, 14, kInkSoft, font::Left, false);
    for (size_t i = 0; i < S.trainQueue.size() && i < 14; i++) {
        ui::Rect r{qx, qy, 46, 40};
        atlas::draw9(atlas::get("ui/card"), qx, qy, 46, 40, 40, 10);
        world::drawTroopThumb(S.trainQueue[i].type, qx + 23, qy + 20, 34);
        if (i == 0) font::draw(fmtTime(S.trainQueue[0].left / std::max(1, V::barracksCount(S))), qx + 23, qy + 26, 11, kInk, font::Center, false);
        if (ui::tapped(r)) V::cancelTrain(S, (int)i);
        qx += 50;
    }
    // карточки: герой и отряды
    float cw = (body.w - 6 * 8) / 7, ch = body.h - 80, cx = body.x, cy = body.y + 76;
    {
        bool open = V::heroUnlocked(S);
        int maxL = V::heroMaxLevel(S);
        ui::Rect r{cx, cy, cw, ch};
        atlas::draw9(atlas::get(open ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 40, 12, open ? rgba(255, 236, 190) : 0xffffffffu);
        world::drawTroopThumb(lf::TroopType::Hero, cx + cw / 2, cy + ch * 0.32f, ch * 0.55f);
        font::draw("Фонарщик", cx + cw / 2, cy + ch * 0.58f, 15, kInk, font::Center, false);
        font::draw(open ? "Герой · ур. " + std::to_string(S.heroLevel) : "Герой", cx + cw / 2, cy + ch * 0.58f + 20, 11, kInkSoft, font::Center, false);
        if (!open) font::draw("Фонарь ур. " + std::to_string(lf::kHeroUnlockLantern), cx + cw / 2, cy + ch - 26, 13, rgba(180, 40, 30), font::Center, false);
        else if (S.heroLevel >= maxL) font::draw(S.heroLevel >= lf::kHeroMaxLevel ? "Максимум" : "Улучши Фонарь", cx + cw / 2, cy + ch - 26, 13, kInkSoft, font::Center, false);
        else {
            int cost = V::heroUpgradeCost(S.heroLevel + 1);
            costLabel(lf::Res::Aether, cost, cx + cw / 2, cy + ch - 26, 15, S.aether >= cost ? kInk : rgba(200, 50, 40), 1);
        }
        if (ui::tapped(r)) {
            auto res = V::upgradeHero(S);
            if (res.ok) { toast("Фонарщик: уровень " + std::to_string(S.heroLevel) + "!", kGold); audio::play("upgrade", 0.9f); save(); }
            else toast(res.reason, kBad);
        }
        cx += cw + 8;
    }
    for (int i = 0; i < lf::kPlayerTroops; i++) {
        auto type = (lf::TroopType)i;
        const auto& d = lf::T(type);
        auto can = V::canTrain(S, type);
        bool unlocked = V::barracksLevel(S) >= d.unlock;
        ui::Rect r{cx, cy, cw, ch};
        atlas::draw9(atlas::get(unlocked ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 40, 12);
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
            atlas::draw9(atlas::get(open ? "ui/card" : "ui/card_dark"), cx, cy, cw, ch, 40, 12);
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
        y += 30;
        const auto& lg = V::league(S.rating);
        int li = V::leagueIndex(S.rating);
        atlas::draw9(atlas::get("ui/card"), body.x, y, body.w, 56, 40, 12);
        ui::icon("icon/star", body.x + 30, y + 28, 36, lg.color);
        font::draw(std::string("Лига ") + lg.name + " · рейтинг " + std::to_string(S.rating), body.x + 60, y + 8, 18, kInk, font::Left, false);
        int nextMin = 0;
        for (int r = S.rating; r < 100000 && li < 5; r++) if (V::leagueIndex(r) > li) { nextMin = r; break; }
        font::draw(li < 5 ? "Бонус к добыче +" + std::to_string((int)(lg.lootBonus * 100)) + "% · до следующей лиги " + std::to_string(nextMin - S.rating)
                          : "Бонус к добыче +" + std::to_string((int)(lg.lootBonus * 100)) + "% · высшая лига",
                   body.x + 60, y + 32, 13, kInkSoft, font::Left, false);
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
    font::draw("Lanternfall 0.3 · свой движок на C++", body.x + 4, y + 52, 13, kInkSoft, font::Left, false);
    if (ui::button({body.x + body.w - 170, y, 170, 44}, audio::muted() ? "Звук: выкл" : "Звук: вкл", audio::muted() ? ui::Style::Dark : ui::Style::Green, true, 16)) {
        audio::setMuted(!audio::muted());
        platform::writeFile(platform::saveDir() + "/settings.txt", audio::muted() ? "mute 1\n" : "mute 0\n");
    }
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
