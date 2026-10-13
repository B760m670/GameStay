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
constexpr int kEclipseSel = 100;
constexpr float kDT = 1.f / 30.f;
}

void App::enterBattle(std::unique_ptr<lf::Battle> b, BattleKind kind, const std::string& title, int missionIdx) {
    bt = BattleState{};
    bt.b = std::move(b);
    bt.kind = kind;
    bt.title = title;
    bt.missionIdx = missionIdx;
    bt.started = kind == BattleKind::Night;
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
    gest.onTap = [this](Vec2 w, Vec2) { if (bt.sel == kEclipseSel) tryBattleAction(w); };
}

bool App::tryBattleAction(Vec2 w) {
    auto& B = *bt.b;
    if (B.ended || bt.kind == BattleKind::Night) return false;
    if (bt.sel == kEclipseSel) {
        if (B.eclipseChargesLeft <= 0 || w.x < 0 || w.y < 0 || w.x > lf::kGrid || w.y > lf::kGrid) return false;
        B.castEclipse(w.x, w.y);
        bt.started = true;
        toast("Затмение! Свет погас", kGood);
        return true;
    }
    if (B.deploy((lf::TroopType)bt.sel, w.x, w.y)) { bt.started = true; return true; }
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
        for (int i = 0; i < cnt; i++, k++) B.deploy(type, 1.0f + (k % 20) * 0.4f, 1.0f + (k / 20) * 0.4f);
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
        while (bt.acc >= kDT) { B.step(kDT); bt.acc -= kDT; }
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
        }
        if (ev.kind == lf::Ev::LightsOut) toast("Светоч погас — сектор во тьме!", kGood);
        else if (ev.kind == lf::Ev::Lantern) toast(bt.kind == BattleKind::Night ? "Наш Фонарь пал!" : "Фонарь врага разрушен!", bt.kind == BattleKind::Night ? kBad : kGood);
        else if (ev.kind == lf::Ev::Wave) toast("Мгла выходит из тьмы!", kBad);
    }
    B.events.clear();
    for (auto& f : bt.flashes) f.second -= dt;
    bt.flashes.erase(std::remove_if(bt.flashes.begin(), bt.flashes.end(), [](auto& f) { return f.second <= 0; }), bt.flashes.end());
    if (B.ended && !bt.resultShown) {
        bt.endDelay += dt;
        if (bt.endDelay > 1.0f) { bt.resultShown = true; finishBattle(); }
    }
    drawBattleWorld();
    drawBattleHud();
    if (modal == Modal::Result) drawResult();
}

void App::drawBattleWorld() {
    auto& B = *bt.b;
    world::drawGround(cam);
    for (const auto& b : B.buildings) if (!b.isWall()) world::drawDirt(cam, (float)b.x, (float)b.y, (float)b.size);
    world::drawMeadow(cam);
    bool tint = bt.kind != BattleKind::Night && bt.sel >= 0 && bt.sel != kEclipseSel && !B.ended;
    world::drawFog(cam, B.sources, B.eclipses, t, 0.76f, tint);
    for (auto& f : bt.flashes) world::drawFootprint(cam, (float)(f.first % lf::kGrid), (float)(f.first / lf::kGrid), 1, rgba(255, 60, 60, (int)(f.second * 400)));
    struct Item { float depth; int kind, idx; };
    std::vector<Item> items;
    const auto& dec = world::decor();
    for (size_t i = 0; i < dec.size(); i++) items.push_back({dec[i].depth, 2, (int)i});
    for (size_t i = 0; i < B.buildings.size(); i++) items.push_back({B.buildings[i].cx + B.buildings[i].cy, 0, (int)i});
    for (size_t i = 0; i < B.troops.size(); i++)
        if (!B.troops[i].dead) items.push_back({B.troops[i].x + B.troops[i].y + (B.troops[i].air ? 3.f : 0.f), 1, (int)i});
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });
    for (const auto& it : items) {
        if (it.kind == 2) { world::drawDecor(cam, dec[it.idx]); continue; }
        if (it.kind == 0) {
            const auto& b = B.buildings[it.idx];
            world::BuildingLook look;
            look.dead = b.dead;
            look.unpowered = b.isDefense() && !b.powered && !b.dead;
            look.dim = look.unpowered ? 0.55f : 1.f;
            look.hitFlash = b.hitT;
            world::drawBuilding(cam, b.type, (float)b.x, (float)b.y, b.level, look, t);
        } else {
            world::drawTroop(cam, B.troops[it.idx], t);
        }
    }
    for (const auto& b : B.buildings)
        if (b.def->beam && !b.dead && b.target >= 0 && !B.troops[b.target].dead) world::drawBeam(cam, b, B.troops[b.target]);
    for (const auto& p : B.projectiles) world::drawProjectile(cam, p);
    for (const auto& e : B.effects) world::drawEffect(cam, e, t);
    for (const auto& b : B.buildings)
        if (!b.dead && b.hp < b.maxHp)
            world::drawBarAt(cam, b.cx, b.cy, b.isWall() ? 30.f : 60.f + b.size * 16, b.hp / b.maxHp,
                             b.hp / b.maxHp > 0.5f ? rgba(130, 220, 80) : b.hp / b.maxHp > 0.25f ? rgba(240, 200, 60) : rgba(230, 70, 60), b.isWall() ? 22 : 36);
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
        font::draw(bt.sel == kEclipseSel ? "Нажми на базу — свет погаснет" : "Высаживай войска во тьме", cam.w - safeX, y + 48, 14, kMuted, font::Right);
    }
    // нижняя панель: войска, затмение, скорость, выход
    float bh = 74, bw = 70, gap = 6;
    std::vector<std::pair<int, int>> cards;
    if (bt.kind != BattleKind::Night) {
        for (const auto& [type, n] : B.pool) if (n > 0 || B.used.count(type)) cards.push_back({(int)type, n});
        if (B.eclipseChargesLeft > 0 || bt.sel == kEclipseSel) cards.push_back({kEclipseSel, B.eclipseChargesLeft});
    }
    float total = cards.size() * (bw + gap) + 2 * (64 + gap);
    float bx = cam.w / 2 - total / 2, by = cam.h - bh - safeB;
    for (auto [id, n] : cards) {
        ui::Rect r{bx, by, bw, bh};
        bool sel = bt.sel == id;
        ui::panel(r, sel ? ui::Style::Normal : ui::Style::Dark);
        if (sel) atlas::draw9(atlas::get("ui/glow_frame"), r.x - 6, r.y - 6, r.w + 12, r.h + 12, 30, 18, rgba(255, 220, 120, 230));
        if (id == kEclipseSel) ui::icon("icon/eclipse", bx + bw / 2, by + 30, 40);
        else world::drawTroopThumb((lf::TroopType)id, bx + bw / 2, by + 30, 52);
        font::draw("x" + std::to_string(n), bx + bw / 2, by + bh - 24, 16, n > 0 ? 0xffffffffu : kMuted, font::Center);
        if (n <= 0) gfx::fillRect(bx + 3, by + 3, bw - 6, bh - 6, rgba(10, 6, 20, 120));
        if (ui::tapped(r)) bt.sel = id;
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
    ui::block({0, 0, cam.w, cam.h});
    gfx::fillRect(0, 0, cam.w, cam.h, rgba(5, 3, 14, 160));
    float w = std::min(560.f, cam.w - 2 * safeX), h = std::min(320.f, cam.h - 20);
    float x = cam.w / 2 - w / 2, y = cam.h / 2 - h / 2;
    ui::panel({x, y, w, h});
    font::draw(title, x + w / 2, y + 14, 28, kGold, font::Center);
    float cy = y + 60;
    if (!night) {
        for (int s = 0; s < 3; s++) ui::icon(s < bt.res.stars ? "icon/star" : "icon/star_off", x + w / 2 - 70 + s * 70, cy + 30, 64);
        cy += 72;
        font::draw("Разрушено: " + std::to_string(bt.res.pct) + "%", x + w / 2, cy, 19, 0xffffffffu, font::Center);
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
    std::string got = "Получено: " + fmtNum(bt.gotOre) + " руды, " + fmtNum(bt.gotAether) + " Эфира";
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
