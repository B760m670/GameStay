// Тесты игрового ядра. Запуск: ctest или ./build/lf_tests
#include <cmath>
#include <cstdio>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "game/core.h"

using namespace lf;

static int failures = 0;
static std::vector<std::pair<const char*, std::function<void()>>>& registry() {
    static std::vector<std::pair<const char*, std::function<void()>>> r;
    return r;
}
struct Reg { Reg(const char* n, std::function<void()> f) { registry().push_back({n, f}); } };
#define TEST(name) static void name(); static Reg reg_##name(#name, name); static void name()
#define CHECK(cond, ...) do { if (!(cond)) { failures++; std::printf("  FAIL %s:%d: %s ", __FILE__, __LINE__, #cond); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

TEST(start_village_valid) {
    auto S = village::create(0);
    for (const auto& b : S.buildings) {
        auto r = village::canPlace(S, b.type, b.x, b.y, b.id);
        CHECK(r.ok, "%s: %s", B(b.type).id, r.reason.c_str());
    }
}

TEST(no_building_in_darkness) {
    auto S = village::create(0);
    CHECK(!village::canPlace(S, BType::Wall, 3, 3).ok, "");
    CHECK(village::canPlace(S, BType::Wall, 19, 19).ok, "");
}

TEST(build_upgrade_collect) {
    auto S = village::create(0);
    CHECK(village::place(S, BType::Wall, 19, 19, 0).ok, "");
    int lantern = S.buildings[0].id;
    S.ore = 4000;
    CHECK(village::startUpgrade(S, lantern, 0).ok, "");
    village::tick(S, 61000);
    CHECK(village::lanternLevel(S) == 2, "LL=%d", village::lanternLevel(S));
    int mine = -1;
    for (auto& b : S.buildings) if (b.type == BType::OreMine) mine = b.id;
    CHECK(village::collect(S, mine, 120000) > 0, "");
}

TEST(train_troops) {
    auto S = village::create(0);
    S.army = {};
    CHECK(village::train(S, TroopType::Torch, 0).ok, "");
    village::tick(S, 10000);
    CHECK(S.army[(int)TroopType::Torch] == 1, "");
}

TEST(save_load_roundtrip) {
    auto S = village::create(123);
    S.campaign["m1"] = 3;
    S.codex.push_back("ford");
    village::train(S, TroopType::Torch, 123);
    VillageState L;
    CHECK(village::load(village::save(S), L), "");
    CHECK(L.buildings.size() == S.buildings.size(), "");
    CHECK(L.ore == S.ore && L.campaign["m1"] == 3 && L.codex.size() == 2 && L.trainQueue.size() == 1, "");
    CHECK(!village::load("garbage", L), "");
}

TEST(basegen_valid_all_levels) {
    for (int LL = 1; LL <= 6; LL++)
        for (int seed = 1; seed <= 5; seed++) {
            auto layout = basegen::generate(LL, seed);
            std::set<int> occ;
            std::vector<LightSource> src;
            for (auto& b : layout)
                if (B(b.type).isLight()) src.push_back({b.x + B(b.type).size / 2.f, b.y + B(b.type).size / 2.f, B(b.type).lv(B(b.type).light, b.level), b.id});
            bool hasLantern = false;
            for (auto& b : layout) {
                int s = B(b.type).size;
                for (int j = b.y; j < b.y + s; j++)
                    for (int i = b.x; i < b.x + s; i++) CHECK(occ.insert(j * kGrid + i).second, "overlap LL%d seed%d %s", LL, seed, B(b.type).id);
                CHECK(isLit(b.x + s / 2.f, b.y + s / 2.f, src), "unlit %s LL%d", B(b.type).id, LL);
                CHECK(b.level >= 1 && b.level <= B(b.type).maxLevel, "");
                hasLantern |= b.type == BType::Lantern;
            }
            CHECK(hasLantern, "");
        }
}

TEST(deploy_only_in_darkness) {
    BattleOptions o;
    o.layout = basegen::generate(2, 3);
    o.army[TroopType::Torch] = 5;
    Battle B(o);
    CHECK(!B.canDeploy(22, 22), "");
    CHECK(B.canDeploy(1, 1), "");
    CHECK(B.deploy(TroopType::Torch, 1, 1), "");
}

TEST(strong_army_wins_weak_does_not) {
    auto layout = basegen::generate(1, 7);
    BattleOptions o;
    o.layout = layout;
    o.army[TroopType::Torch] = 60;
    Battle B(o);
    for (int k = 0; k < 60; k++) B.deploy(TroopType::Torch, 1 + (k % 10) * 0.3f, 1 + (k / 10) * 0.3f);
    auto r = B.simulate();
    CHECK(r.stars >= 2, "stars=%d pct=%d", r.stars, r.pct);
    CHECK(r.lootOre > 0, "");
    BattleOptions o2;
    o2.layout = layout;
    o2.army[TroopType::Torch] = 3;
    Battle B2(o2);
    for (int k = 0; k < 3; k++) B2.deploy(TroopType::Torch, 1, 1);
    CHECK(B2.simulate().stars < 3, "");
}

TEST(simulation_deterministic) {
    auto run = [] {
        BattleOptions o;
        o.layout = basegen::generate(3, 11);
        o.seed = 5;
        o.army[TroopType::Torch] = 30;
        o.army[TroopType::Archer] = 20;
        Battle B(o);
        for (int k = 0; k < 30; k++) B.deploy(TroopType::Torch, 1 + k * 0.1f, 2);
        for (int k = 0; k < 20; k++) B.deploy(TroopType::Archer, 2, 1 + k * 0.1f);
        return B.simulate();
    };
    auto a = run(), b = run();
    CHECK(a.pct == b.pct && a.lootOre == b.lootOre && a.stars == b.stars, "");
}

TEST(first_night_survivable) {
    int wins = 0;
    for (int seed = 1; seed <= 6; seed++) {
        auto S = village::create(0);
        auto n = basegen::nightWaves(1, 0, seed);
        BattleOptions o;
        o.mode = BattleMode::Defense;
        o.layout = village::layout(S);
        o.waves = n.waves;
        o.hazeMult = n.hazeMult;
        o.duration = kNightTime;
        o.seed = seed;
        Battle B(o);
        if (B.simulate().win) wins++;
    }
    CHECK(wins >= 4, "wins=%d", wins);
}

TEST(beacons_power_defenses) {
    BattleOptions o;
    o.layout = basegen::generate(4, 2);
    Battle B(o);
    int powered = 0, beacons = 0;
    for (auto& b : B.buildings) { powered += b.isDefense() && b.powered; beacons += b.type == BType::Beacon; }
    CHECK(beacons > 0 && powered > 0, "");
    for (auto& b : B.buildings) if (b.isLight()) b.dead = true;
    B.relight();
    int after = 0;
    for (auto& b : B.buildings) after += b.isDefense() && b.powered;
    CHECK(after == 0, "after=%d", after);
}

TEST(campaign_generates) {
    CHECK(campaign::missions().size() == 12, "");
    for (const auto& m : campaign::missions()) {
        auto l = basegen::generate(m.LL, m.seed, &m.counts, m.richness, m.levelBias);
        CHECK(!l.empty() && l[0].type == BType::Lantern, "%s", m.id);
    }
}

TEST(xp_from_building_and_level_up) {
    auto S = village::create(0);
    long long shards0 = S.shards;
    int lantern = S.buildings[0].id;
    S.ore = 100000;
    CHECK(village::startUpgrade(S, lantern, 0).ok, "");
    village::tick(S, 61000);  // 60 с стройки -> корень = 7 опыта
    CHECK(S.level == 1 && S.xp == 7, "lvl=%d xp=%lld", S.level, S.xp);
    village::addXp(S, 100);
    CHECK(S.level >= 2 && S.shards > shards0 && S.levelUps >= 1, "lvl=%d", S.level);
    VillageState L;
    CHECK(village::load(village::save(S), L) && L.level == S.level && L.xp == S.xp, "");
}

TEST(tasks_progress_and_claim) {
    auto S = village::create(0);
    const auto& t = village::taskDefs()[0];  // постройки
    CHECK(!village::taskReady(S, t), "");
    S.ore = 100000;
    for (int k = 0; k < 5; k++) village::place(S, BType::Wall, 19 + k, 19, 0);
    CHECK(S.stats.built == 5 && village::taskReady(S, t), "built=%lld", S.stats.built);
    long long sh = S.shards;
    CHECK(village::claimTask(S, t) && S.shards == sh + t.shards[0] && village::taskClaimed(S, t) == 1, "");
    CHECK(!village::claimTask(S, t), "");
    VillageState L;
    CHECK(village::load(village::save(S), L) && village::taskClaimed(L, t) == 1 && L.stats.built == 5, "");
}

TEST(workshop_research_and_battle_levels) {
    auto S = village::create(0);
    CHECK(!village::canResearch(S, TroopType::Torch, 0).ok, "");
    S.buildings[0].level = 3;  // Фонарь 3 открывает Кузницу
    S.ore = S.aether = 1000000;
    village::addRes(S, Res::Aether, 0);
    S.buildings.push_back(BuildingRec{999, BType::Workshop, 30, 30, 1});
    CHECK(village::troopMaxLevel(S) == 2, "max=%d", village::troopMaxLevel(S));
    S.aether = 100000;
    CHECK(village::startResearch(S, TroopType::Torch, 0).ok, "");
    CHECK(!village::canResearch(S, TroopType::Archer, 1000).ok, "busy");
    village::tick(S, 10'000'000);
    CHECK(S.troopLevel[0] == 2 && S.researchType == -1, "lvl=%d", S.troopLevel[0]);
    VillageState L;
    CHECK(village::load(village::save(S), L) && L.troopLevel[0] == 2, "");
    // уровень отряда усиливает его в бою
    auto run = [](int lvl) {
        BattleOptions o;
        o.layout = basegen::generate(2, 4);
        o.army[TroopType::Torch] = 25;
        o.troopLevels[TroopType::Torch] = lvl;
        Battle B(o);
        for (int k = 0; k < 25; k++) B.deploy(TroopType::Torch, 1 + k * 0.2f, 1);
        return B.simulate().pct;
    };
    CHECK(run(5) >= run(1), "");
}

TEST(spells_heal_and_rage) {
    BattleOptions o;
    o.layout = basegen::generate(2, 3);
    o.army[TroopType::Shield] = 2;
    o.spells[Spell::Heal] = 1;
    Battle B(o);
    CHECK(B.deploy(TroopType::Shield, 1, 1), "");
    B.troops[0].hp = B.troops[0].maxHp * 0.3f;
    CHECK(B.castSpell(Spell::Heal, 1, 1), "");
    CHECK(!B.castSpell(Spell::Heal, 1, 1), "no charges left");
    CHECK(!B.castSpell(Spell::Rage, 1, 1), "rage not given");
    for (int i = 0; i < 60; i++) B.step(1.f / 30);
    CHECK(B.troops[0].hp > B.troops[0].maxHp * 0.4f, "hp=%f", B.troops[0].hp);
    // ярость: войска в зоне бегут быстрее
    auto dist = [](bool rage) {
        BattleOptions o;
        o.layout = basegen::generate(2, 3);
        o.army[TroopType::Torch] = 1;
        o.spells[Spell::Rage] = 1;
        Battle B(o);
        B.deploy(TroopType::Torch, 1, 1);
        if (rage) B.castSpell(Spell::Rage, 1, 1);
        for (int i = 0; i < 15; i++) B.step(1.f / 30);
        return std::hypot(B.troops[0].x - 1, B.troops[0].y - 1);
    };
    CHECK(dist(true) > dist(false) * 1.3f, "%f vs %f", dist(true), dist(false));
}

TEST(trap_triggers_and_not_counted) {
    BattleOptions o;
    o.layout = {{BType::Lantern, 20, 20, 1, 0}, {BType::Trap, 3, 3, 1, 1}};
    o.army[TroopType::Torch] = 1;
    Battle B(o);
    CHECK(B.total == 1, "total=%d", B.total);
    CHECK(B.deploy(TroopType::Torch, 3.5f, 3.6f), "can deploy over a hidden trap");
    B.step(1.f / 30);
    CHECK(B.buildings[1].dead, "trap fired");
    CHECK(B.troops[0].hp < B.troops[0].maxHp, "hp=%f", B.troops[0].hp);
    CHECK(B.percent() == 0, "pct=%d", B.percent());
    // в базах соперников ловушки есть
    int traps = 0;
    for (const auto& it : basegen::generate(3, 9)) traps += it.type == BType::Trap;
    CHECK(traps > 0, "");
}

TEST(hero_deploy_flash_and_upgrade) {
    BattleOptions o;
    o.layout = basegen::generate(2, 5);
    o.heroLevel = 3;
    Battle B(o);
    CHECK(!B.heroAbility(), "not deployed yet");
    CHECK(B.deployHero(1, 1), "");
    CHECK(!B.deployHero(1.5f, 1), "only once");
    CHECK(B.troops[B.heroIdx].maxHp > T(TroopType::Hero).hp * 1.15f, "level scales hp");
    // подвести героя к обороне и ослепить её
    int def = -1;
    for (size_t i = 0; i < B.buildings.size(); i++) if (B.buildings[i].isDefense() && !B.buildings[i].isTrap()) def = (int)i;
    CHECK(def >= 0, "");
    B.troops[B.heroIdx].x = B.buildings[def].cx + 1;
    B.troops[B.heroIdx].y = B.buildings[def].cy;
    CHECK(B.heroAbility(), "");
    CHECK(B.buildings[def].stunT > 0, "stunned");
    CHECK(!B.heroAbility(), "once per battle");
    // прокачка в деревне
    auto S = village::create(0);
    CHECK(!village::upgradeHero(S).ok, "locked at lantern 1");
    S.buildings[0].level = 3;
    S.aether = 1000000;
    CHECK(village::heroMaxLevel(S) == 5, "max=%d", village::heroMaxLevel(S));
    CHECK(village::upgradeHero(S).ok && S.heroLevel == 2, "");
    VillageState L;
    CHECK(village::load(village::save(S), L) && L.heroLevel == 2, "");
}

TEST(replay_restores_battle_exactly) {
    replay::Record rec;
    rec.opts.layout = basegen::generate(3, 21);
    rec.opts.army[TroopType::Torch] = 20;
    rec.opts.army[TroopType::Archer] = 10;
    rec.opts.spells[Spell::Rage] = 1;
    rec.opts.heroLevel = 2;
    rec.opts.eclipseCharges = 1;
    rec.opts.seed = 9;
    const float dt = 1.f / 30;
    Battle live(rec.opts);
    auto act = [&](replay::Act k, int arg, float x, float y) {
        replay::Action a{rec.steps, k, arg, x, y};
        if (replay::apply(live, a)) rec.actions.push_back(a);
    };
    for (int s = 0; s < 400; s++) {
        if (s % 7 == 0 && s < 140) act(replay::Act::Deploy, s % 2 ? 0 : 1, 1.f + s * 0.05f, 1.2f);
        if (s == 30) act(replay::Act::Hero, 0, 2.f, 1.f);
        if (s == 90) act(replay::Act::Spell, (int)Spell::Rage, 6.f, 6.f);
        if (s == 120) act(replay::Act::HeroAbility, 0, 0, 0);
        if (s == 150) act(replay::Act::Eclipse, 0, 20.f, 20.f);
        live.step(dt);
        rec.steps++;
    }
    replay::Record back;
    CHECK(replay::load(replay::save(rec), back), "");
    Battle again(back.opts);
    replay::fastForward(again, back, dt);
    CHECK(again.troops.size() == live.troops.size(), "%zu vs %zu", again.troops.size(), live.troops.size());
    CHECK(again.percent() == live.percent() && again.time == live.time, "pct %d vs %d", again.percent(), live.percent());
    bool same = true;
    for (size_t i = 0; i < live.troops.size() && i < again.troops.size(); i++)
        same = same && live.troops[i].x == again.troops[i].x && live.troops[i].hp == again.troops[i].hp;
    CHECK(same, "troop state differs");
}

TEST(obstacles_block_clear_and_grow) {
    auto S = village::create(0);
    CHECK(S.obstacles.size() >= 12, "n=%zu", S.obstacles.size());
    const auto o = S.obstacles[0];
    // нельзя строить поверх препятствия (свет не важен: проверяем, что причина — препятствие или тьма)
    auto r = village::canPlace(S, BType::Wall, o.x, o.y, -1);
    CHECK(!r.ok, "");
    S.ore = 100000;
    long long shards = S.shards;
    int got = -1;
    CHECK(village::clearObstacle(S, o.id, &got).ok, "");
    CHECK(S.shards == shards + got && got >= 0, "");
    CHECK(village::obstacleAtCell(S, o.x, o.y) == nullptr, "");
    size_t n = S.obstacles.size();
    village::tick(S, kObstacleEveryMs * 3 + 10);
    CHECK(S.obstacles.size() > n, "grew %zu -> %zu", n, S.obstacles.size());
    VillageState L;
    CHECK(village::load(village::save(S), L) && L.obstacles.size() == S.obstacles.size(), "");
}

int main() {
    for (auto& [name, fn] : registry()) {
        int before = failures;
        fn();
        std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", name);
    }
    std::printf("%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
