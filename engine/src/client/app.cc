// Общая часть клиента: цикл кадра, сохранения, тосты, диалоги, запуск и итоги боёв.
#include "app.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

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
    splash = gfx::loadTexture(platform::assetDir() + "/art/splash.jpg", true);
    std::vector<unsigned char> settings;
    if (platform::readFile(platform::saveDir() + "/settings.txt", settings) && std::string(settings.begin(), settings.end()).find("mute 1") != std::string::npos)
        audio::setMuted(true);
    audio::music("village", 0.42f);
    std::vector<unsigned char> data;
    bool loaded = false;
    if (platform::readFile(savePath(), data)) loaded = lf::village::load(std::string(data.begin(), data.end()), S);
    if (!loaded) S = lf::village::create(now());
    lf::village::tick(S, now());
    enterVillage();
    if (S.started && resumeBattle()) return;
    if (!S.started) {
        S.started = true;
        dialogue(lf::campaign::intro(), [this] { toast("Нажми на постройку, чтобы улучшить её"); });
        save();
    }
}

static std::string battlePath() { return platform::saveDir() + "/battle.sav"; }

void App::save() {
    platform::writeFile(savePath(), lf::village::save(S));
    saveBattle();
}

// Незаконченный бой сохраняется записью действий; пустой файл — боя нет.
void App::saveBattle() {
    if (scene != Scene::Battle || !bt.b || bt.b->ended || !bt.started) { platform::writeFile(battlePath(), ""); return; }
    std::string head = "kind " + std::to_string((int)bt.kind) + "\nmission " + std::to_string(bt.missionIdx) + "\ntitle " + bt.title + "\n";
    platform::writeFile(battlePath(), head + lf::replay::save(bt.rec));
}

bool App::resumeBattle() {
    std::vector<unsigned char> data;
    if (!platform::readFile(battlePath(), data) || data.empty()) return false;
    std::string s(data.begin(), data.end());
    lf::replay::Record rec;
    if (!lf::replay::load(s, rec)) return false;
    int kind = 0, mission = -1;
    std::string title;
    std::istringstream in(s);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("kind ", 0) == 0) kind = std::atoi(line.c_str() + 5);
        else if (line.rfind("mission ", 0) == 0) mission = std::atoi(line.c_str() + 8);
        else if (line.rfind("title ", 0) == 0) title = line.substr(6);
    }
    if (kind < 0 || kind > 2 || (kind == (int)BattleKind::Mission && (mission < 0 || mission >= (int)lf::campaign::missions().size()))) return false;
    enterBattle(rec.opts, (BattleKind)kind, title, mission, &rec);
    toast("Бой продолжается с того же места", rgba(255, 216, 107));
    return true;
}

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
        static const char* ids[] = {"", "portrait/vira", "portrait/keeper", "portrait/haze", "portrait/king"};
        const atlas::Sprite* pic = atlas::get(ids[(int)line.who]);
        float ps = std::min(150.f, by - 12);
        if (pic) {
            // портрет выезжает при смене говорящего
            float k = dlgIdx > 0 && dlg[dlgIdx - 1].who == line.who ? 1.f : std::min(1.f, dlgT * 6);
            float back = 1 + 2.70158f * (k - 1) * (k - 1) * (k - 1) + 1.70158f * (k - 1) * (k - 1);
            gfx::pushScale(bx + 16 + ps / 2, by, back, 1);
            atlas::drawRect(pic, bx + 16, by - ps + 30, ps, ps);
            gfx::popScale();
            font::draw(sp.name, bx + 30 + ps, by + 10, 19, sp.color);
        } else {
            font::draw(sp.name, bx + 22, by + 10, 19, sp.color);
        }
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
    if (!dlg.empty() || modal != Modal::None || title) world.clear();
    gfx::beginFrame(width, height);
    ui::setTime(t);
    if (title && titleOut < 0) {  // пока заставка закрывает экран, мир не рисуем
        drawTitle(dt);
        ui::endFrame();
        gfx::endFrame(rgba(13, 10, 28));
        audio::update();
        return;
    }
    if (modal != shownModal) { if (modal != Modal::None) modalT = t; shownModal = modal; }
    if (scene == Scene::Village) villageFrame(dt, world);
    else battleFrame(dt, world);
    if (S.levelUps > 0 && dlg.empty() && modal == Modal::None) {
        int reward = lf::village::levelReward(S.level);
        toast("Новый уровень " + std::to_string(S.level) + "!  +" + std::to_string(reward) + " осколков", gfx::rgba(255, 216, 107));
        audio::play("win", 0.7f);
        spawnFlyers(lf::Res::Shards, {cam.w / 2, cam.h / 2}, reward * 40);
        if (S.level == lf::village::spellUnlockLevel(lf::Spell::Heal)) toast("Открыто заклинание «Исцеление» — лечит отряды в бою", gfx::rgba(160, 240, 140));
        if (S.level == lf::village::spellUnlockLevel(lf::Spell::Rage)) toast("Открыто заклинание «Ярость» — отряды быстрее бегут и бьют", gfx::rgba(255, 150, 120));
        S.levelUps = 0;
        save();
    }
    drawFlyers(dt);
    if (title) drawTitle(dt);
    else drawDialogue();
    drawToasts(dt);
    ui::endFrame();
    gfx::endFrame(rgba(13, 10, 28));
    audio::update();
    saveT += dt;
    if (saveT > 5) { saveT = 0; save(); }
}

// ---------- титульный экран ----------
void App::drawTitle(float dt) {
    float W = cam.w, H = cam.h;
    float fade = titleOut < 0 ? 1.f : std::max(0.f, 1 - titleOut / 0.5f);
    if (titleOut >= 0) { titleOut += dt; if (titleOut > 0.5f) { title = false; return; } }
    ui::block({0, 0, W, H});
    uint32_t white = rgba(255, 255, 255, (int)(255 * fade));
    if (splash) {
        // «эффект Кена Бёрнса»: медленный наезд на картину
        float zoom = 1.04f + 0.04f * std::sin(t * 0.08f);
        float k = std::max(W / splash->w, H / splash->h) * zoom;
        float w = splash->w * k, h = splash->h * k;
        gfx::rect(splash, W / 2 - w / 2 + std::sin(t * 0.05f) * 8, H / 2 - h / 2, w, h, 0, 0, 1, 1, white);
    } else {
        gfx::fillRect(0, 0, W, H, rgba(13, 10, 28, (int)(255 * fade)));
    }
    // угольки от Фонаря
    if (embers.size() < 60) {
        float x = W * (0.3f + (float)(std::rand() % 1000) / 2500.f);
        embers.push_back({x, H * 0.65f, 0, ((std::rand() % 100) - 50) * 0.3f, -30.f - std::rand() % 40, 0, 3.f + (std::rand() % 100) / 50.f, 5.f, 3.f + std::rand() % 4,
                          rgba(255, 180 + std::rand() % 50, 90, 220), true});
    }
    gfx::setBlend(gfx::Blend::Add);
    for (auto& e : embers) {
        e.life -= dt;
        e.x += (e.vx + std::sin(t * 2 + e.size) * 10) * dt;
        e.y += e.vy * dt;
        float a = std::min(1.f, e.life / 1.5f) * fade;
        atlas::drawRect(atlas::get("fx/glow"), e.x - e.size * 2, e.y - e.size * 2, e.size * 4, e.size * 4, gfx::withAlpha(e.color, a));
    }
    gfx::setBlend(gfx::Blend::Alpha);
    embers.erase(std::remove_if(embers.begin(), embers.end(), [](const world::Particle& p) { return p.life <= 0; }), embers.end());
    // затемнение сверху под заголовок
    {
        uint32_t top = rgba(8, 5, 20, (int)(170 * fade)), bot = rgba(8, 5, 20, 0);
        gfx::Vtx v[4] = {{0, 0, 0, 0, top}, {W, 0, 1, 0, top}, {W, H * 0.42f, 1, 1, bot}, {0, H * 0.42f, 0, 1, bot}};
        gfx::quad(gfx::white(), v);
    }
    float ty = H * 0.08f;
    float pulse = 1 + 0.015f * std::sin(t * 2.2f);
    gfx::pushScale(W / 2, ty + 34, pulse, fade);
    font::draw("LANTERNFALL", W / 2, ty, 64, rgba(255, 216, 107), font::Center);
    gfx::popScale();
    font::draw("Пока горит Фонарь — Мгла не пройдёт", W / 2, ty + 78, 19, rgba(235, 225, 255, (int)(235 * fade)), font::Center);
    float blink = 0.55f + 0.45f * std::sin(t * 3.5f);
    font::draw("Нажми, чтобы начать", W / 2, H - safeB - 44, 22, rgba(255, 240, 200, (int)(255 * blink * fade)), font::Center);
    if (titleOut < 0 && ui::tapped({0, 0, W, H})) {
        titleOut = 0;
        audio::play("open", 0.8f);
    }
}

// ---------- запуск боёв ----------
static std::map<lf::TroopType, int> levelMap(const lf::VillageState& S) {
    std::map<lf::TroopType, int> m;
    for (int i = 0; i < lf::kPlayerTroops; i++) m[(lf::TroopType)i] = S.troopLevel[i];
    return m;
}

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
    o.troopLevels = levelMap(S);
    o.eclipseCharges = lf::eclipseCharges(lf::village::lanternLevel(S));
    o.spells = lf::village::spellCharges(S.level);
    o.heroLevel = lf::village::heroUnlocked(S) ? S.heroLevel : 0;
    o.seed = ms.seed;
    enterBattle(o, BattleKind::Mission, ms.title, idx);
}

void App::startRaid() {
    int LL = lf::village::lanternLevel(S);
    int seed = (int)(now() & 0x7fffffff);
    lf::Rng rng(seed);
    int eLL = std::clamp(LL + (rng.next() < 0.25 ? -1 : 0) + (rng.next() < 0.15 && LL < 6 ? 1 : 0), 1, 6);
    lf::BattleOptions o;
    o.layout = lf::basegen::generate(eLL, seed, nullptr, 0.7f + rng.nextf() * 0.6f);
    o.army = armyMap(S);
    o.troopLevels = levelMap(S);
    o.eclipseCharges = lf::eclipseCharges(LL);
    o.spells = lf::village::spellCharges(S.level);
    o.heroLevel = lf::village::heroUnlocked(S) ? S.heroLevel : 0;
    o.seed = seed;
    std::string name = lf::basegen::rivalName(rng) + " · Фонарь " + std::to_string(eLL);
    enterBattle(o, BattleKind::Raid, name);
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
    enterBattle(o, BattleKind::Night, "Ночь " + std::to_string(S.nights + 1));
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
        float bonus = bt.kind == BattleKind::Raid ? 1.f + lf::village::league(S.rating).lootBonus : 1.f;
        bt.gotOre = lf::village::addRes(S, lf::Res::Ore, (long long)(r.lootOre * bonus));
        bt.gotAether = lf::village::addRes(S, lf::Res::Aether, (long long)(r.lootAether * bonus));
        S.stats.attacks++;
        if (r.win) S.stats.wins++;
        S.stats.stars += r.stars;
        for (const auto& b : bt.b->buildings) if (b.dead && b.type == lf::BType::Beacon) S.stats.beaconsDestroyed++;
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
        } else {
            int old = S.rating, before = lf::village::leagueIndex(old);
            S.rating = std::max(0, old + lf::village::ratingDelta(r.win, r.stars));
            bt.ratingDelta = S.rating - old;
            if (r.stars > 0) {
                bt.gotShards = r.stars;
                S.shards += r.stars;
            }
            if (lf::village::leagueIndex(S.rating) > before) toast(std::string("Новая лига: ") + lf::village::league(S.rating).name + "!", gfx::rgba(255, 216, 107));
        }
    }
    save();
    modal = Modal::Result;
    audio::play(bt.res.win ? "win" : "lose", 0.8f);
}

}  // namespace client
