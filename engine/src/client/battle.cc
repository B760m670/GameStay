// Сцена боя: атака игрока и ночная оборона.
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

namespace {
const uint32_t kGold = rgba(255, 216, 107), kMuted = rgba(190, 178, 215), kBad = rgba(255, 140, 130), kGood = rgba(160, 240, 140);
constexpr int kEclipseSel = 100, kHealSel = 101, kRageSel = 102, kHeroSel = 103;
bool isSpellSel(int sel) { return sel >= kEclipseSel; }
constexpr float kDT = 1.f / 30.f;

// Свой генератор для эффектов: ГСЧ боя трогать нельзя — он отвечает за детерминизм симуляции.
uint32_t fxSeed = 12345;
float frand() {
    fxSeed = fxSeed * 1664525u + 1013904223u;
    return (fxSeed >> 8) / 16777216.f;
}

float easeOutBack(float k) {
    k = std::clamp(k, 0.f, 1.f);
    return 1 + 2.70158f * (k - 1) * (k - 1) * (k - 1) + 1.70158f * (k - 1) * (k - 1);
}

void dust(std::vector<world::Particle>& ps, float x, float y, int n, float spread, uint32_t col) {
    for (int i = 0; i < n; i++) {
        float a = frand() * 6.283f, v = spread * (0.5f + frand());
        ps.push_back({x, y, 4, std::cos(a) * v, std::sin(a) * v, 10 + frand() * 25, 0.5f + frand() * 0.4f, 0.9f, 6 + frand() * 5, col, false});
    }
}

void debris(std::vector<world::Particle>& ps, float x, float y, float size, int n) {
    static const uint32_t cols[] = {rgba(120, 110, 120), rgba(150, 110, 70), rgba(90, 80, 90), rgba(170, 150, 120)};
    for (int i = 0; i < n; i++) {
        float a = frand() * 6.283f, v = (0.6f + frand() * 1.6f) * (0.6f + size * 0.3f);
        world::Particle p{x + (frand() - 0.5f) * size * 0.6f, y + (frand() - 0.5f) * size * 0.6f, 20 + frand() * 30 * size,
                          std::cos(a) * v, std::sin(a) * v, 120 + frand() * 160, 1.6f + frand() * 0.8f, 2.4f, 2.5f + frand() * 3.f,
                          cols[i % 4], false};
        p.chunk = true;
        ps.push_back(p);
    }
    for (int i = 0; i < n / 2; i++)  // дым
        ps.push_back({x + (frand() - 0.5f) * size, y + (frand() - 0.5f) * size, 15 + frand() * 20, (frand() - 0.5f) * 0.4f,
                      (frand() - 0.5f) * 0.4f, 30 + frand() * 30, 1.4f + frand(), 2.4f, 10 + frand() * 8 * size, rgba(110, 100, 115, 170), false});
}
}

void App::enterBattle(const lf::BattleOptions& o, BattleKind kind, const std::string& title, int missionIdx, const lf::replay::Record* resume) {
    bt = BattleState{};
    audio::music("battle", 0.38f);
    bt.b = std::make_unique<lf::Battle>(o);
    bt.rec.opts = o;
    if (resume) {  // продолжение: повторяем записанные действия и шаги
        bt.rec = *resume;
        lf::replay::fastForward(*bt.b, bt.rec, kDT);
    }
    bt.kind = kind;
    bt.title = title;
    bt.missionIdx = missionIdx;
    bt.started = kind == BattleKind::Night || (resume && resume->steps > 0);
    scene = Scene::Battle;
    modal = Modal::None;
    gest.reset();
    float R = 8;
    for (const auto& s : bt.b->sources) R = std::max(R, std::hypot(s.x - 22, s.y - 22) + s.r);
    requestFocus(22, 22, std::min(24.f, R + 3.5f), 1.2f);
    for (const auto& [type, n] : bt.b->pool) if (n > 0) { bt.sel = (int)type; break; }
    gest.onDown = [this](Vec2 w, Vec2) {
        if (bt.b->ended || bt.kind == BattleKind::Night || bt.sel < 0 || bt.sel == kEclipseSel) return false;
        auto it = bt.b->pool.find((lf::TroopType)bt.sel);
        if (it == bt.b->pool.end() || it->second <= 0) return false;
        tryBattleAction(w);
        bt.holding = true;
        bt.holdPos = w;
        bt.holdT = 0;
        return true;
    };
    gest.onDrag = [this](Vec2 w, Vec2) { bt.holdPos = w; };
    gest.onDragEnd = [this](bool, Vec2) { bt.holding = false; };
    gest.onTap = [this](Vec2 w, Vec2) { if (isSpellSel(bt.sel)) tryBattleAction(w); };
}

bool App::act(lf::replay::Act kind, int arg, Vec2 w) {
    lf::replay::Action a{bt.rec.steps, kind, arg, w.x, w.y};
    if (!lf::replay::apply(*bt.b, a)) return false;
    bt.rec.actions.push_back(a);  // для продолжения боя после закрытия приложения
    return true;
}

bool App::tryBattleAction(Vec2 w) {
    auto& B = *bt.b;
    if (B.ended || bt.kind == BattleKind::Night) return false;
    if (bt.sel == kEclipseSel) {
        if (B.eclipseChargesLeft <= 0 || w.x < 0 || w.y < 0 || w.x > lf::kGrid || w.y > lf::kGrid) return false;
        act(lf::replay::Act::Eclipse, 0, w);
        bt.started = true;
        toast("Затмение! Свет погас", kGood);
        return true;
    }
    if (bt.sel == kHeroSel) {
        if (B.heroIdx >= 0) return false;
        if (act(lf::replay::Act::Hero, 0, w)) {
            bt.started = true;
            toast("Фонарщик в бою! Нажми на его карточку — Вспышка", kGold);
            return true;
        }
        if (bt.warnT <= 0) { toast("Высадка только во тьме и не на постройках", kBad); bt.warnT = 1.5f; }
        return false;
    }
    if (bt.sel == kHealSel || bt.sel == kRageSel) {
        if (w.x < 0 || w.y < 0 || w.x > lf::kGrid || w.y > lf::kGrid) return false;
        if (!act(lf::replay::Act::Spell, (int)(bt.sel == kHealSel ? lf::Spell::Heal : lf::Spell::Rage), w)) return false;
        bt.started = true;
        return true;
    }
    if (act(lf::replay::Act::Deploy, bt.sel, w)) { bt.started = true; return true; }
    if (w.x >= 0 && w.y >= 0 && w.x < lf::kGrid && w.y < lf::kGrid) bt.flashes.push_back({(int)w.y * lf::kGrid + (int)w.x, 0.5f});
    if (bt.warnT <= 0) { toast("Высадка только во тьме и не на постройках", kBad); bt.warnT = 1.5f; }
    return false;
}

void App::autoDeploy() {
    // для автотестов: высадить всё по краю карты
    auto& B = *bt.b;
    int k = 0;
    for (auto& [type, n] : B.pool) {
        int cnt = n;
        for (int i = 0; i < cnt; i++, k++) act(lf::replay::Act::Deploy, (int)type, {1.0f + (k % 20) * 0.4f, 1.0f + (k / 20) * 0.4f});
    }
    bt.started = true;
}

void App::battleFrame(float dt, const std::vector<input::Event>& worldEvents) {
    auto& B = *bt.b;
    gest.process(worldEvents, cam);
    if (bt.warnT > 0) bt.warnT -= dt;
    if (!bt.started) {
        bt.scout -= dt;
        if (bt.scout <= 0) bt.started = true;
    }
    if (bt.holding && !B.ended) {
        bt.holdT += dt;
        if (bt.holdT > 0.11f) { bt.holdT = 0; tryBattleAction(bt.holdPos); }
    }
    if (bt.started && !B.ended && modal == Modal::None && dlg.empty()) {
        bt.acc += std::min(dt, 0.1f) * bt.speed;
        while (bt.acc >= kDT) { B.step(kDT); bt.rec.steps++; bt.acc -= kDT; }
    }
    for (const auto& ev : B.events) {
        float pv = 0.9f + (float)((int)(ev.x * 7 + ev.y * 3) % 5) * 0.05f;  // разброс высоты без ГСЧ боя
        switch (ev.kind) {
            case lf::Ev::Shot:
                audio::play(ev.type == lf::BType::Catapult ? "catapult" : ev.type == lf::BType::Brazier ? "flame" : "shoot", 0.45f, pv);
                break;
            case lf::Ev::Hit: audio::play(pv > 1.0f ? "slash" : "hit", 0.35f, pv); break;
            case lf::Ev::Deploy: audio::play("deploy", 0.5f, pv); break;
            case lf::Ev::Destroyed: audio::play(ev.type == lf::BType::Wall ? "hit" : "destroy", ev.type == lf::BType::Wall ? 0.5f : 0.9f, pv); break;
            case lf::Ev::Lantern: audio::play("boom", 1.0f, 0.8f); break;
            case lf::Ev::EclipseCast: audio::play("eclipse", 0.8f); break;
            case lf::Ev::Wave: audio::play("wave", 0.7f); break;
            case lf::Ev::LightsOut: audio::play("eclipse", 0.7f, 0.8f); break;
            case lf::Ev::SpellCast: audio::play("upgrade", 0.8f, 0.8f); break;
            case lf::Ev::Trap: audio::play("boom", 0.9f, 1.2f); break;
            case lf::Ev::HeroFlash: audio::play("eclipse", 1.0f, 1.4f); break;
        }
        switch (ev.kind) {
            case lf::Ev::Deploy: dust(bt.parts, ev.x, ev.y, 6, 0.9f, rgba(150, 125, 90, 170)); break;
            case lf::Ev::Destroyed: {
                bool wall = ev.type == lf::BType::Wall;
                float size = (float)lf::B(ev.type).size;
                debris(bt.parts, ev.x, ev.y, size, wall ? 5 : 10 + (int)size * 4);
                if (!wall) {
                    bt.shake = std::min(1.f, bt.shake + 0.22f + size * 0.06f);
                    for (int i = 0; i < 10; i++)
                        bt.parts.push_back({ev.x, ev.y, 30, (frand() - 0.5f) * 3, (frand() - 0.5f) * 3, 60 + frand() * 120, 0.5f + frand() * 0.4f, 0.9f, 7, rgba(255, 170, 80, 230), true});
                }
                break;
            }
            case lf::Ev::HeroFlash:
                bt.shake = std::min(1.f, bt.shake + 0.3f);
                for (int i = 0; i < 30; i++)
                    bt.parts.push_back({ev.x, ev.y, 30, (frand() - 0.5f) * 8, (frand() - 0.5f) * 8, 40 + frand() * 120, 0.7f + frand() * 0.5f, 1.2f, 7, rgba(255, 230, 140, 240), true});
                break;
            case lf::Ev::Trap:
                bt.shake = std::min(1.f, bt.shake + 0.35f);
                dust(bt.parts, ev.x, ev.y, 10, 2.2f, rgba(90, 70, 60, 200));
                if (bt.kind != BattleKind::Night) toast("Ловушка!", kBad);
                break;
            case lf::Ev::Lantern:
                bt.shake = 1.f;
                for (int i = 0; i < 40; i++)
                    bt.parts.push_back({ev.x, ev.y, 60, (frand() - 0.5f) * 5, (frand() - 0.5f) * 5, 80 + frand() * 200, 0.8f + frand() * 0.8f, 1.6f, 8, rgba(255, 190, 90, 240), true});
                break;
            default: break;
        }
        if (ev.kind == lf::Ev::LightsOut) toast("Светоч погас — сектор во тьме!", kGood);
        else if (ev.kind == lf::Ev::Lantern) toast(bt.kind == BattleKind::Night ? "Наш Фонарь пал!" : "Фонарь врага разрушен!", bt.kind == BattleKind::Night ? kBad : kGood);
        else if (ev.kind == lf::Ev::Wave) toast("Мгла выходит из тьмы!", kBad);
    }
    B.events.clear();
    // гибель юнитов и построек: сколько секунд прошло (для анимации смерти и обрушения)
    bt.troopDeadT.resize(B.troops.size(), -1.f);
    for (size_t i = 0; i < B.troops.size(); i++)
        if (B.troops[i].dead) bt.troopDeadT[i] = bt.troopDeadT[i] < 0 ? 0.f : bt.troopDeadT[i] + dt;
    bt.bDeadT.resize(B.buildings.size(), -1.f);
    for (size_t i = 0; i < B.buildings.size(); i++)
        if (B.buildings[i].dead) bt.bDeadT[i] = bt.bDeadT[i] < 0 ? 0.f : bt.bDeadT[i] + dt;
    bt.shake = std::max(0.f, bt.shake - dt * 1.6f);
    world::updateParticles(bt.parts, dt);
    for (auto& f : bt.flashes) f.second -= dt;
    bt.flashes.erase(std::remove_if(bt.flashes.begin(), bt.flashes.end(), [](auto& f) { return f.second <= 0; }), bt.flashes.end());
    if (B.ended && !bt.resultShown) {
        bt.endDelay += dt;
        if (bt.endDelay > 1.0f) { bt.resultShown = true; finishBattle(); }
    }
    drawBattleWorld();
    drawBattleHud();
    if (modal == Modal::Result) {
        bt.resultT += dt;
        drawResult();
    }
}

void App::drawBattleWorld() {
    auto& B = *bt.b;
    const float ox = cam.ox, oy = cam.oy;
    if (bt.shake > 0) {
        float k = bt.shake * bt.shake * 9;
        cam.ox += std::sin(t * 61) * k;
        cam.oy += std::cos(t * 47) * k * 0.7f;
    }
    world::drawGround(cam);
    for (const auto& b : B.buildings) if (b.isStructure()) world::drawDirt(cam, (float)b.x, (float)b.y, (float)b.size);
    world::drawMeadow(cam, t);
    bool tint = bt.kind != BattleKind::Night && bt.sel >= 0 && !isSpellSel(bt.sel) && !B.ended;
    world::drawFog(cam, B.sources, B.eclipses, t, 0.72f, tint);
    world::drawWaterFx(cam, t);
    for (const auto& z : B.zones) world::drawSpellZone(cam, z, B.time, t);
    for (auto& f : bt.flashes) world::drawFootprint(cam, (float)(f.first % lf::kGrid), (float)(f.first / lf::kGrid), 1, rgba(255, 60, 60, (int)(f.second * 400)));
    struct Item { float depth; int kind, idx; };
    std::vector<Item> items;
    const auto& dec = world::decor();
    for (size_t i = 0; i < dec.size(); i++) items.push_back({dec[i].depth, 2, (int)i});
    for (size_t i = 0; i < B.buildings.size(); i++) {
        const auto& b = B.buildings[i];
        // ловушки врага невидимы до срабатывания, сработавшие исчезают; свои (ночью) видны
        if (b.isTrap() && (b.dead || bt.kind != BattleKind::Night)) continue;
        items.push_back({b.cx + b.cy, 0, (int)i});
    }
    for (size_t i = 0; i < B.troops.size(); i++) {
        bool dead = B.troops[i].dead;
        if (dead && (i >= bt.troopDeadT.size() || bt.troopDeadT[i] > 2.8f)) continue;
        items.push_back({B.troops[i].x + B.troops[i].y + (B.troops[i].air && !dead ? 3.f : 0.f) - (dead ? 0.5f : 0.f), 1, (int)i});
    }
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });
    for (const auto& it : items) {
        if (it.kind == 2) { world::drawDecor(cam, dec[it.idx], t); continue; }
        if (it.kind == 0) {
            const auto& b = B.buildings[it.idx];
            world::BuildingLook look;
            look.dead = b.dead;
            look.unpowered = b.isDefense() && !b.powered && !b.dead;
            look.dim = look.unpowered ? 0.55f : 1.f;
            look.hitFlash = b.hitT;
            look.angle = b.angle;
            look.stunned = b.stunT;
            if (b.dead && it.idx < (int)bt.bDeadT.size()) look.collapse = std::min(1.f, bt.bDeadT[it.idx] / 0.7f);
            world::drawBuilding(cam, b.type, (float)b.x, (float)b.y, b.level, look, t);
        } else {
            const auto& tr = B.troops[it.idx];
            world::drawTroop(cam, tr, t, tr.dead ? bt.troopDeadT[it.idx] : -1.f);
        }
    }
    for (const auto& b : B.buildings)
        if (b.def->beam && !b.dead && b.target >= 0 && !B.troops[b.target].dead) world::drawBeam(cam, b, B.troops[b.target]);
    for (const auto& p : B.projectiles) world::drawProjectile(cam, p);
    for (const auto& e : B.effects) world::drawEffect(cam, e, t);
    for (const auto& p : bt.parts) if (!p.additive) world::drawParticle(cam, p);
    gfx::setBlend(gfx::Blend::Add);
    for (const auto& p : bt.parts) if (p.additive) world::drawParticle(cam, p);
    gfx::setBlend(gfx::Blend::Alpha);
    for (const auto& b : B.buildings)
        if (!b.dead && !b.isTrap() && b.hp < b.maxHp)
            world::drawBarAt(cam, b.cx, b.cy, b.isWall() ? 30.f : 60.f + b.size * 16, b.hp / b.maxHp,
                             b.hp / b.maxHp > 0.5f ? rgba(130, 220, 80) : b.hp / b.maxHp > 0.25f ? rgba(240, 200, 60) : rgba(230, 70, 60), b.isWall() ? 22 : 36);
    cam.ox = ox;
    cam.oy = oy;
}

void App::drawBattleHud() {
    auto& B = *bt.b;
    float x = safeX, y = 10;
    // название и таймер
    float tw = std::min(320.f, font::measure(bt.title, 17) + 30);
    ui::panel({x, y, tw, 40}, ui::Style::Dark);
    font::draw(bt.title, x + 14, y + 10, 17, kGold);
    x += tw + 8;
    std::string timer = bt.started ? fmtTime(B.duration - B.time) : "Разведка " + std::to_string((int)std::ceil(bt.scout));
    ui::panel({x, y, 140, 40}, ui::Style::Dark);
    ui::icon("icon/clock", x + 20, y + 20, 26);
    font::draw(timer, x + 38, y + 10, 17, 0xffffffffu);
    x += 148;
    if (bt.kind == BattleKind::Night) {
        const lf::BBuilding* l = nullptr;
        for (const auto& b : B.buildings) if (b.type == lf::BType::Lantern) l = &b;
        ui::panel({x, y, 170, 40}, ui::Style::Dark);
        ui::icon("icon/lantern", x + 20, y + 20, 28);
        ui::bar({x + 40, y + 14, 120, 14}, l ? l->hp / l->maxHp : 0, rgba(255, 200, 90));
        int left = 0;
        for (const auto& w : B.waves) left += !w.done;
        std::string st = left ? "Волн впереди: " + std::to_string(left) : "Мгла на базе: " + std::to_string(B.aliveTroops());
        font::draw(st, cam.w - safeX, y + 10, 16, kMuted, font::Right);
        font::draw("Потери: " + fmtNum((long long)B.lootOre) + " руды, " + fmtNum((long long)B.lootAether) + " Эфира", cam.w - safeX, y + 34, 13, kBad, font::Right, false);
    } else {
        int pct = B.percent();
        int stars = (pct >= 50) + B.lanternDead + (pct >= 100);
        ui::panel({x, y, 150, 40}, ui::Style::Dark);
        font::draw(std::to_string(pct) + "%", x + 14, y + 9, 19, 0xffffffffu);
        for (int s = 0; s < 3; s++) ui::icon(s < stars ? "icon/star" : "icon/star_off", x + 80 + s * 22, y + 20, 22);
        // добыча справа
        float rx = cam.w - safeX - 230;
        ui::panel({rx, y, 230, 40}, ui::Style::Dark);
        ui::icon("icon/ore", rx + 20, y + 20, 26);
        font::draw(fmtNum((long long)B.lootOre), rx + 38, y + 10, 16, 0xffffffffu);
        ui::icon("icon/aether", rx + 125, y + 20, 26);
        font::draw(fmtNum((long long)B.lootAether), rx + 143, y + 10, 16, 0xffffffffu);
        const char* hint = bt.sel == kEclipseSel ? "Нажми на базу — свет погаснет"
                           : bt.sel == kHealSel ? "Нажми на карту — отряды в круге лечатся"
                           : bt.sel == kRageSel ? "Нажми на карту — отряды в круге звереют"
                           : bt.sel == kHeroSel && B.heroIdx < 0 ? "Нажми во тьме — высадить Фонарщика"
                                                : "Высаживай войска во тьме";
        font::draw(hint, cam.w - safeX, y + 48, 14, kMuted, font::Right);
    }
    // нижняя панель: войска, затмение, скорость, выход
    float bh = 74, bw = 70, gap = 6;
    std::vector<std::pair<int, int>> cards;
    if (bt.kind != BattleKind::Night) {
        for (const auto& [type, n] : B.pool) if (n > 0 || B.used.count(type)) cards.push_back({(int)type, n});
        if (B.eclipseChargesLeft > 0 || bt.sel == kEclipseSel) cards.push_back({kEclipseSel, B.eclipseChargesLeft});
        for (auto [sp, n] : B.spells) cards.push_back({sp == lf::Spell::Heal ? kHealSel : kRageSel, n});
        if (B.heroLevel > 0) cards.insert(cards.begin(), {kHeroSel, B.heroIdx < 0 || !B.heroAbilityUsed ? 1 : 0});
    }
    float total = cards.size() * (bw + gap) + 2 * (64 + gap);
    float bx = cam.w / 2 - total / 2, by = cam.h - bh - safeB;
    for (auto [id, n] : cards) {
        ui::Rect r{bx, by, bw, bh};
        bool sel = bt.sel == id;
        ui::panel(r, sel ? ui::Style::Normal : ui::Style::Dark);
        if (sel) atlas::draw9(atlas::get("ui/glow_frame"), r.x - 6, r.y - 6, r.w + 12, r.h + 12, 30, 18, rgba(255, 220, 120, 230));
        if (id == kEclipseSel) ui::icon("icon/eclipse", bx + bw / 2, by + 30, 40);
        else if (id == kHeroSel && B.heroIdx >= 0) {
            bool alive = !B.troops[B.heroIdx].dead;
            ui::icon("icon/flash", bx + bw / 2, by + 30, 44, alive && !B.heroAbilityUsed ? 0xffffffffu : rgba(120, 110, 130, 200));
            if (alive && !B.heroAbilityUsed) atlas::draw9(atlas::get("ui/glow_frame"), r.x - 6, r.y - 6, r.w + 12, r.h + 12, 30, 18, rgba(255, 220, 120, (int)(150 + 100 * std::sin(t * 6))));
        } else if (id == kHeroSel) world::drawTroopThumb(lf::TroopType::Hero, bx + bw / 2, by + 30, 56);
        else if (id == kHealSel) ui::icon("icon/heal", bx + bw / 2, by + 30, 42);
        else if (id == kRageSel) ui::icon("icon/rage", bx + bw / 2, by + 30, 42);
        else world::drawTroopThumb((lf::TroopType)id, bx + bw / 2, by + 30, 52);
        std::string label = id == kHeroSel ? (B.heroIdx < 0 ? "ур. " + std::to_string(B.heroLevel) : "Вспышка") : "x" + std::to_string(n);
        font::draw(label, bx + bw / 2, by + bh - 24, id == kHeroSel ? 14 : 16, n > 0 ? 0xffffffffu : kMuted, font::Center);
        if (n <= 0) gfx::fillRect(bx + 3, by + 3, bw - 6, bh - 6, rgba(10, 6, 20, 120));
        if (ui::tapped(r)) {
            if (id == kHeroSel && B.heroIdx >= 0) act(lf::replay::Act::HeroAbility, 0, {0, 0});
            else bt.sel = id;
        }
        bx += bw + gap;
    }
    if (ui::iconButton({bx, by + 6, 64, bh - 6}, "icon/clock", "x" + std::to_string(bt.speed))) bt.speed = bt.speed == 1 ? 2 : bt.speed == 2 ? 4 : 1;
    bx += 64 + gap;
    if (ui::iconButton({bx, by + 6, 64, bh - 6}, "icon/flag", bt.kind == BattleKind::Night ? "Сдаться" : "Выйти", ui::Style::Red) && !B.ended) {
        if (bt.kind == BattleKind::Night) B.lanternDead = true;
        B.finish();
    }
}

void App::drawResult() {
    bool night = bt.kind == BattleKind::Night;
    const char* title = night ? (bt.res.win ? "Рассвет" : "Фонарь пал") : (bt.res.win ? "Победа!" : "Поражение");
    const float rt = bt.resultT;
    auto count = [&](long long v) { return (long long)(v * std::clamp((rt - 0.4f) / 1.3f, 0.f, 1.f)); };
    ui::block({0, 0, cam.w, cam.h});
    gfx::fillRect(0, 0, cam.w, cam.h, rgba(5, 3, 14, 160));
    float w = std::min(560.f, cam.w - 2 * safeX), h = std::min(320.f, cam.h - 20);
    float x = cam.w / 2 - w / 2, y = cam.h / 2 - h / 2;
    ui::panel({x, y, w, h});
    font::draw(title, x + w / 2, y + 14, 28, kGold, font::Center);
    float cy = y + 60;
    if (!night) {
        for (int s = 0; s < 3; s++) {
            float appear = rt - 0.35f - s * 0.38f;
            bool on = s < bt.res.stars && appear > 0;
            if (on && bt.starsShown <= s) { bt.starsShown = s + 1; audio::play("upgrade", 0.8f, 1.f + s * 0.12f); }
            float sz = on ? 64 * easeOutBack(appear / 0.35f) : 64;
            if (on && appear < 0.5f) world::glowSpot(x + w / 2 - 70 + s * 70, cy + 30, 80 * (1 - appear * 2), rgba(255, 220, 120, 200));
            ui::icon("icon/star_off", x + w / 2 - 70 + s * 70, cy + 30, 64);
            if (on) ui::icon("icon/star", x + w / 2 - 70 + s * 70, cy + 30, sz);
        }
        cy += 72;
        font::draw("Разрушено: " + std::to_string((int)count(bt.res.pct)) + "%", x + w / 2, cy, 19, 0xffffffffu, font::Center);
        cy += 30;
    } else {
        for (const auto& l : font::wrap(bt.res.win ? "Ты удержал свет до рассвета. Мгла отступила — до следующей ночи."
                                                   : "Мгла захлестнула поселение. Фонарь разгорится снова, но часть запасов утеряна.", 17, w - 40)) {
            font::draw(l, x + w / 2, cy, 17, 0xffffffffu, font::Center, false);
            cy += font::lineHeight(17);
        }
        cy += 6;
        font::draw("Похищено Мглой: " + fmtNum(bt.lostOre) + " руды, " + fmtNum(bt.lostAether) + " Эфира", x + w / 2, cy, 15, kBad, font::Center, false);
        cy += 26;
    }
    if (bt.kind == BattleKind::Raid) {
        std::string rd = std::string("Рейтинг: ") + (bt.ratingDelta >= 0 ? "+" : "") + std::to_string(bt.ratingDelta) + "  (" +
                         std::to_string(S.rating) + ", лига " + lf::village::league(S.rating).name + ")";
        font::draw(rd, x + w / 2, cy, 15, bt.ratingDelta >= 0 ? kGood : kBad, font::Center, false);
        cy += 22;
    }
    std::string got = "Получено: " + fmtNum(count(bt.gotOre)) + " руды, " + fmtNum(count(bt.gotAether)) + " Эфира";
    if (bt.gotShards) got += ", " + std::to_string(bt.gotShards) + " осколков";
    font::draw(got, x + w / 2, cy, 16, kGood, font::Center, false);
    cy += 24;
    if (bt.firstWin) {
        const auto& ms = lf::campaign::missions()[bt.missionIdx];
        auto it = lf::campaign::codex().find(ms.codex);
        if (it != lf::campaign::codex().end()) font::draw(std::string("Новая запись в Хронике: «") + it->second.title + "»", x + w / 2, cy, 15, kGold, font::Center, false);
    } else if (!night && !bt.res.win) {
        font::draw("Совет: сначала гаси Светочи — оборона в их секторе ослепнет.", x + w / 2, cy, 14, kMuted, font::Center, false);
    }
    if (ui::button({x + w / 2 - 100, y + h - 62, 200, 48}, "Домой", ui::Style::Gold)) {
        bool firstWin = bt.firstWin;
        int idx = bt.missionIdx;
        enterVillage();
        if (firstWin && idx >= 0) dialogue(lf::campaign::missions()[idx].outro);
    }
}

}  // namespace client
