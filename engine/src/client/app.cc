// Общая часть клиента: цикл кадра, сохранения, тосты, диалоги, запуск и итоги боёв.
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "engine/atlas.h"
#include "engine/audio.h"
#include "engine/font.h"
#include "engine/gfx.h"
#include "platform/platform.h"
#include "world.h"

namespace client {
using gfx::rgba;

std::string fmtNum(long long n) {
    char buf[32];
    if (n >= 10000000) std::snprintf(buf, sizeof buf, "%lldM", n / 1000000);
    else if (n >= 1000000) std::snprintf(buf, sizeof buf, "%.1fM", n / 1e6);
    else if (n >= 100000) std::snprintf(buf, sizeof buf, "%lldk", n / 1000);
    else if (n >= 10000) std::snprintf(buf, sizeof buf, "%.1fk", n / 1e3);
    else std::snprintf(buf, sizeof buf, "%lld", n);
    return buf;
}

std::string fmtTime(double sec) {
    long long s = (long long)std::ceil(std::max(0.0, sec));
    char buf[32];
    if (s < 60) std::snprintf(buf, sizeof buf, "%lldс", s);
    else if (s < 3600) std::snprintf(buf, sizeof buf, s % 60 ? "%lldм %lldс" : "%lldм", s / 60, s % 60);
    else std::snprintf(buf, sizeof buf, "%lldч %lldм", s / 3600, (s / 60) % 60);
    return buf;
}

long long App::now() const { return platform::nowMs(); }

static std::string savePath() { return platform::saveDir() + "/lanternfall.sav"; }

void App::init() {
    std::vector<unsigned char> data;
    bool loaded = false;
    if (platform::readFile(savePath(), data)) loaded = lf::village::load(std::string(data.begin(), data.end()), S);
    if (!loaded) S = lf::village::create(now());
    lf::village::tick(S, now());
    enterVillage();
    if (!S.started) {
        S.started = true;
        dialogue(lf::campaign::intro(), [this] { toast("Нажми на постройку, чтобы улучшить её"); });
        save();
    }
}

void App::save() { platform::writeFile(savePath(), lf::village::save(S)); }

void App::toast(const std::string& s, uint32_t color) {
    if (color == rgba(255, 140, 130)) audio::play("error", 0.5f);
    toasts.push_back({s, 2.6f, color});
    if (toasts.size() > 4) toasts.erase(toasts.begin());
}

void App::drawToasts(float dt) {
    float y = 70;
    for (auto& tt : toasts) {
        tt.t -= dt;
        float a = std::clamp(std::min(tt.t / 0.4f, (2.6f - tt.t) / 0.2f), 0.f, 1.f);
        float w = font::measure(tt.text, 17) + 36;
        float x = cam.w / 2 - w / 2;
        atlas::draw9(atlas::get("ui/panel_dark"), x, y, w, 34, 28, 12, rgba(255, 255, 255, (int)(235 * a)));
        font::draw(tt.text, cam.w / 2, y + 7, 17, gfx::withAlpha(tt.color, a), font::Center);
        y += 40;
    }
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(), [](const Toast& t) { return t.t <= 0; }), toasts.end());
}

void App::spawnFlyers(lf::Res r, Vec2 from, long long amount) {
    int n = (int)std::clamp(amount / 40 + 3, 3LL, 10LL);
    for (int i = 0; i < n; i++) {
        float a = i * 2.4f;
        Vec2 f{from.x + std::cos(a) * 14, from.y + std::sin(a) * 8};
        flyers.push_back({r, f, resAnchor[(int)r], 0, i * 0.05f, i == n - 1 ? amount : 0});
    }
}

void App::drawFlyers(float dt) {
    for (auto& f : flyers) {
        f.delay -= dt;
        if (f.delay > 0) continue;
        f.t += dt / 0.55f;
        float k = std::min(1.f, f.t);
        float e = k * k * (3 - 2 * k);
        float x = f.from.x + (f.to.x - f.from.x) * e;
        float y = f.from.y + (f.to.y - f.from.y) * e - std::sin(k * 3.14159f) * 60;
        const char* ic = f.res == lf::Res::Ore ? "icon/ore" : f.res == lf::Res::Aether ? "icon/aether" : "icon/shard";
        ui::icon(ic, x, y, 30 * (1.f - 0.3f * k));
    }
    flyers.erase(std::remove_if(flyers.begin(), flyers.end(), [](const Flyer& f) { return f.t >= 1; }), flyers.end());
}

void App::dialogue(const std::vector<lf::Line>& lines, std::function<void()> done) {
    dlg = lines;
    dlgIdx = 0;
    dlgT = 0;
    dlgDone = std::move(done);
}

// Возвращает true, пока диалог на экране (перехватывает весь ввод).
bool App::drawDialogue() {
    if (dlg.empty()) return false;
    ui::block({0, 0, cam.w, cam.h});
    gfx::fillRect(0, 0, cam.w, cam.h, rgba(5, 3, 14, 110));
    const lf::Line& line = dlg[dlgIdx];
    auto sp = lf::campaign::speaker(line.who);
    std::string text = line.text;
    float bw = std::min(cam.w - 2 * safeX - 20, 760.f), bh = 150;
    float bx = cam.w / 2 - bw / 2, by = cam.h - bh - safeB - 8;
    atlas::draw9(atlas::get("ui/panel"), bx, by, bw, bh, 28, 14);
    float tx = bx + 22;
    bool named = sp.name && *sp.name;
    if (named) {
        float r = 34;
        atlas::drawRect(atlas::get("fx/dot"), bx + 20, by - 26, r * 2, r * 2, rgba(20, 12, 30));
        atlas::drawRect(atlas::get("fx/dot"), bx + 24, by - 22, r * 2 - 8, r * 2 - 8, sp.color);
        std::string initial = std::string(sp.name).substr(0, 2);  // первая буква в UTF-8 (кириллица — 2 байта)
        font::draw(initial, bx + 20 + r, by - 26 + r - 15, 28, rgba(30, 20, 40), font::Center, false);
        font::draw(sp.name, bx + 100, by + 10, 19, sp.color);
        tx = bx + 22;
    }
    // печатная машинка
    dlgT += 1.f / 60.f;
    size_t shown = std::min(text.size(), (size_t)(dlgT * 90));
    while (shown < text.size() && (text[shown] & 0xC0) == 0x80) shown++;
    auto lines = font::wrap(text.substr(0, shown), 18, bw - 44);
    float ly = by + (named ? 42 : 22);
    for (auto& l : lines) {
        font::draw(l, tx, ly, 18, named ? 0xffffffffu : rgba(230, 220, 200));
        ly += font::lineHeight(18);
    }
    font::draw("нажми, чтобы продолжить »", bx + bw - 18, by + bh - 26, 13, rgba(170, 160, 200), font::Right, false);
    if (ui::button({cam.w - safeX - 130, 14, 120, 40}, "Пропустить", ui::Style::Dark)) {
        dlg.clear();
        if (dlgDone) { auto d = dlgDone; dlgDone = nullptr; d(); }
        return true;
    }
    if (ui::tapped({0, 0, cam.w, cam.h})) {
        if (shown < text.size()) dlgT = 1e3;
        else if (++dlgIdx >= dlg.size()) {
            dlg.clear();
            if (dlgDone) { auto d = dlgDone; dlgDone = nullptr; d(); }
        } else dlgT = 0;
    }
    return true;
}

void App::frame(float dt, float width, float height) {
    t += dt;
    cam.w = width;
    cam.h = height;
    // запас под вырез и «чёлку» на широких айфонах
    safeX = width / height > 1.9f ? 46.f : 14.f;
    safeB = width / height > 1.9f ? 14.f : 8.f;
    if (focus.pending) { cam.focus(focus.x, focus.y, focus.r, focus.maxZ); focus.pending = false; }
    auto events = input::drain();
    auto world = ui::beginFrame(events);
    if (!dlg.empty() || modal != Modal::None) world.clear();
    gfx::beginFrame(width, height);
    ui::setTime(t);
    if (modal != shownModal) { if (modal != Modal::None) modalT = t; shownModal = modal; }
    if (scene == Scene::Village) villageFrame(dt, world);
    else battleFrame(dt, world);
    if (S.levelUps > 0 && dlg.empty() && modal == Modal::None) {
        int reward = lf::village::levelReward(S.level);
        toast("Новый уровень " + std::to_string(S.level) + "!  +" + std::to_string(reward) + " осколков", gfx::rgba(255, 216, 107));
        audio::play("win", 0.7f);
        spawnFlyers(lf::Res::Shards, {cam.w / 2, cam.h / 2}, reward * 40);
        S.levelUps = 0;
        save();
    }
    drawFlyers(dt);
    drawDialogue();
    drawToasts(dt);
    ui::endFrame();
    gfx::endFrame(rgba(13, 10, 28));
    audio::update();
    saveT += dt;
    if (saveT > 5 && scene == Scene::Village) { saveT = 0; save(); }
}

// ---------- запуск боёв ----------
static std::map<lf::TroopType, int> armyMap(const lf::VillageState& S) {
    std::map<lf::TroopType, int> m;
    for (int i = 0; i < lf::kPlayerTroops; i++) if (S.army[i] > 0) m[(lf::TroopType)i] = S.army[i];
    return m;
}

void App::startMission(int idx) {
    const auto& ms = lf::campaign::missions()[idx];
    lf::BattleOptions o;
    o.layout = lf::basegen::generate(ms.LL, ms.seed, &ms.counts, ms.richness, ms.levelBias);
    o.army = armyMap(S);
    o.eclipseCharges = lf::eclipseCharges(lf::village::lanternLevel(S));
    o.seed = ms.seed;
    enterBattle(std::make_unique<lf::Battle>(o), BattleKind::Mission, ms.title, idx);
}

void App::startRaid() {
    int LL = lf::village::lanternLevel(S);
    int seed = (int)(now() & 0x7fffffff);
    lf::Rng rng(seed);
    int eLL = std::clamp(LL + (rng.next() < 0.25 ? -1 : 0) + (rng.next() < 0.15 && LL < 6 ? 1 : 0), 1, 6);
    lf::BattleOptions o;
    o.layout = lf::basegen::generate(eLL, seed, nullptr, 0.7f + rng.nextf() * 0.6f);
    o.army = armyMap(S);
    o.eclipseCharges = lf::eclipseCharges(LL);
    o.seed = seed;
    std::string name = lf::basegen::rivalName(rng) + " · Фонарь " + std::to_string(eLL);
    enterBattle(std::make_unique<lf::Battle>(o), BattleKind::Raid, name);
}

void App::startNight() {
    lf::village::tick(S, now());
    int LL = lf::village::lanternLevel(S);
    int seed = (int)(now() & 0x7fffffff);
    auto night = lf::basegen::nightWaves(LL, S.nights, seed);
    lf::BattleOptions o;
    o.mode = lf::BattleMode::Defense;
    o.layout = lf::village::layout(S, 0.25f);
    o.waves = night.waves;
    o.hazeMult = night.hazeMult;
    o.seed = seed;
    o.duration = lf::kNightTime;
    enterBattle(std::make_unique<lf::Battle>(o), BattleKind::Night, "Ночь " + std::to_string(S.nights + 1));
}

void App::finishBattle() {
    auto& r = bt.b->result;
    bt.res = r;
    bt.gotOre = bt.gotAether = bt.gotShards = bt.lostOre = bt.lostAether = 0;
    bt.firstWin = false;
    if (bt.kind == BattleKind::Night) {
        bt.lostOre = std::min(S.ore, r.lootOre);
        bt.lostAether = std::min(S.aether, r.lootAether);
        S.ore -= bt.lostOre;
        S.aether -= bt.lostAether;
        int LL = lf::village::lanternLevel(S);
        if (r.win) {
            lf::village::addXp(S, 10LL * LL);
            S.nights++;
            S.stats.nightsWon++;
            bt.gotOre = lf::village::addRes(S, lf::Res::Ore, 150LL * LL * LL);
            bt.gotAether = lf::village::addRes(S, lf::Res::Aether, 150LL * LL * LL);
            bt.gotShards = 2 + LL / 2;
            S.shards += bt.gotShards;
        } else S.stats.nightsLost++;
        lf::village::scheduleNextNight(S, now());
    } else {
        for (auto& [type, n] : r.used) S.army[(int)type] = std::max(0, S.army[(int)type] - n);
        bt.gotOre = lf::village::addRes(S, lf::Res::Ore, r.lootOre);
        bt.gotAether = lf::village::addRes(S, lf::Res::Aether, r.lootAether);
        S.stats.attacks++;
        if (r.win) S.stats.wins++;
        lf::village::addXp(S, 2 + 6LL * r.stars);
        if (bt.kind == BattleKind::Mission) {
            const auto& ms = lf::campaign::missions()[bt.missionIdx];
            int prev = S.campaign.count(ms.id) ? S.campaign[ms.id] : 0;
            if (r.stars > prev) S.campaign[ms.id] = r.stars;
            if (prev == 0 && r.stars > 0) {
                bt.firstWin = true;
                lf::village::addXp(S, 20);
                bt.gotOre += lf::village::addRes(S, lf::Res::Ore, ms.rewardOre);
                bt.gotAether += lf::village::addRes(S, lf::Res::Aether, ms.rewardAether);
                bt.gotShards += ms.rewardShards;
                S.shards += ms.rewardShards;
                if (std::find(S.codex.begin(), S.codex.end(), ms.codex) == S.codex.end()) S.codex.push_back(ms.codex);
            }
        } else if (r.stars > 0) {
            bt.gotShards = r.stars;
            S.shards += r.stars;
        }
    }
    save();
    modal = Modal::Result;
    audio::play(bt.res.win ? "win" : "lose", 0.8f);
}

}  // namespace client
