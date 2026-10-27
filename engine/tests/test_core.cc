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

// Деревня с набором построек (как была старая стартовая): для проверок стройки, армии, обороны.
static VillageState testVillage(long long now) {
    auto S = village::create(now);
    S.buildings[0].level = 1;  // главное здание восстановлено
    auto add = [&](BType t, int x, int y) {
        BuildingRec b;
        b.id = S.nextId++;
        b.type = t;
        b.x = x;
        b.y = y;
        b.level = 1;
        b.accAt = now;
        S.buildings.push_back(b);
    };
    add(BType::OreMine, 15, 19);
    add(BType::AetherWell, 26, 21);
    add(BType::OreStore, 20, 25);
    add(BType::AetherStore, 21, 15);
    add(BType::Barracks, 24, 24);
    add(BType::Camp, 16, 23);
    add(BType::Ballista, 24, 17);
    add(BType::BuilderHut, 18, 17);
    add(BType::BuilderHut, 18, 26);
    S.ore = S.aether = 1500;
    return S;
}

TEST(start_village_valid) {
    auto S = testVillage(0);
    for (const auto& b : S.buildings) {
        auto r = village::canPlace(S, b.type, b.x, b.y, b.id);
        CHECK(r.ok, "%s: %s", B(b.type).id, r.reason.c_str());
    }
}

TEST(build_anywhere_on_clearing) {
    auto S = testVillage(0);
    S.obstacles.clear();
    CHECK(village::canPlace(S, BType::Wall, 3, 3).ok, "far corner of the clearing is fine");
    CHECK(!village::canPlace(S, BType::Wall, 1, 1).ok, "border");
    CHECK(!village::canPlace(S, BType::Wall, 21, 21).ok, "occupied by the fort");
}

TEST(build_upgrade_collect) {
    auto S = testVillage(0);
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
    auto S = testVillage(0);
    S.army = {};
    CHECK(village::train(S, TroopType::Torch, 0).ok, "");
    village::tick(S, 10000);
    CHECK(S.army[(int)TroopType::Torch] == 1, "");
}

TEST(save_load_roundtrip) {
    auto S = testVillage(123);
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
    for (int LL = 1; LL <= kMaxLantern; LL++)
        for (int seed = 1; seed <= 5; seed++) {
            auto layout = basegen::generate(LL, seed);
            std::set<int> occ;
            bool hasLantern = false;
            for (auto& b : layout) {
                int s = B(b.type).size;
                for (int j = b.y; j < b.y + s; j++)
                    for (int i = b.x; i < b.x + s; i++) CHECK(occ.insert(j * kGrid + i).second, "overlap LL%d seed%d %s", LL, seed, B(b.type).id);
                CHECK(b.x >= kBorder && b.y >= kBorder && b.x + s <= kGrid - kBorder && b.y + s <= kGrid - kBorder, "outside %s", B(b.type).id);
                CHECK(b.level >= 1 && b.level <= B(b.type).maxLevel, "");
                hasLantern |= b.type == BType::Lantern;
            }
            CHECK(hasLantern, "");
        }
}

TEST(deploy_on_open_ground_only) {
    BattleOptions o;
    o.layout = basegen::generate(2, 3);
    o.army[TroopType::Torch] = 5;
    Battle B(o);
    CHECK(!B.canDeploy(22, 22), "on the fort");
    CHECK(B.canDeploy(1, 1), "");
    const auto& hq = B.buildings[0];
    CHECK(!B.canDeploy(hq.x - 0.5f, hq.y + 0.5f), "right next to a building");
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
        auto S = testVillage(0);
        auto n = basegen::nightWaves(1, 0, seed);
        BattleOptions o;
        o.mode = BattleMode::Defense;
        o.layout = village::layout(S);
        o.waves = n.waves;
        o.raidMult = n.raidMult;
        o.duration = kNightTime;
        o.seed = seed;
        Battle B(o);
        if (B.simulate().win) wins++;
    }
    CHECK(wins >= 4, "wins=%d", wins);
}

TEST(ram_breaks_palisade_rider_hits_economy) {
    BattleOptions o;
    o.layout = basegen::generate(3, 4);
    Battle B(o);
    int wall = -1, mill = -1;
    for (size_t i = 0; i < B.buildings.size(); i++) {
        if (B.buildings[i].isWall() && wall < 0) wall = (int)i;
        if (B.buildings[i].type == BType::AetherWell && mill < 0) mill = (int)i;
    }
    CHECK(wall >= 0 && mill >= 0, "");
    // таран: удар по частоколу в разы сильнее обычного
    int ram = B.troops.size();
    B.troops.push_back({});
    B.troops[ram].def = &T(TroopType::Sapper);
    B.troops[ram].type = TroopType::Sapper;
    B.troops[ram].hp = B.troops[ram].maxHp = 400;
    float before = B.buildings[wall].hp;
    B.troops[ram].x = B.buildings[wall].cx + 0.6f;
    B.troops[ram].y = B.buildings[wall].cy;
    for (int k = 0; k < 3 && !B.buildings[wall].dead; k++) { B.troops[ram].cd = 0; B.step(0.0f); B.troops[ram].target = wall; }
    float wallHit = before - B.buildings[wall].hp;
    CHECK(wallHit >= T(TroopType::Sapper).dmg * 9 || B.buildings[wall].dead, "ram hit=%f", wallHit);
    // всадник выбирает хозяйство
    BTroop r{};
    r.def = &T(TroopType::Shade);
    r.type = TroopType::Shade;
    r.hp = r.maxHp = 260;
    r.x = 1; r.y = 1;
    B.troops.push_back(r);
    B.step(1.f / 30);
    int tgt = B.troops.back().target;
    CHECK(tgt >= 0 && B.buildings[tgt].isEconomy(), "rider target is economy");
}

TEST(campaign_generates) {
    CHECK(campaign::missions().size() == 12, "");
    for (const auto& m : campaign::missions()) {
        auto l = basegen::generate(m.LL, m.seed, &m.counts, m.richness, m.levelBias);
        CHECK(!l.empty() && l[0].type == BType::Lantern, "%s", m.id);
    }
}

TEST(xp_from_building_and_level_up) {
    auto S = testVillage(0);
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
    auto S = testVillage(0);
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
    auto S = testVillage(0);
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

TEST(hero_deploy_cry_and_upgrade) {
    BattleOptions o;
    o.layout = basegen::generate(2, 5);
    o.heroLevel = 3;
    Battle B(o);
    CHECK(!B.heroAbility(), "not deployed yet");
    CHECK(B.deployHero(1, 1), "");
    CHECK(!B.deployHero(1.5f, 1), "only once");
    CHECK(B.troops[B.heroIdx].maxHp > T(TroopType::Hero).hp * 1.15f, "level scales hp");
    // боевой клич: раненый ополченец рядом подлечивается и свирепеет
    o.army[TroopType::Torch] = 1;
    Battle C(o);
    CHECK(C.deployHero(1, 1) && C.deploy(TroopType::Torch, 1.5f, 1.2f), "");
    BTroop& m = C.troops.back();
    m.hp = m.maxHp * 0.3f;
    CHECK(C.heroAbility(), "");
    CHECK(m.hp > m.maxHp * 0.6f && m.rageT > 0, "hp=%f rage=%f", m.hp, m.rageT);
    CHECK(B.heroAbility(), "");
    CHECK(!B.heroAbility(), "once per battle");
    // прокачка в деревне
    auto S = testVillage(0);
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
    auto S = testVillage(0);
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

TEST(new_game_starts_with_hq_ruins) {
    auto S = village::create(0);
    CHECK(S.buildings.size() == 1 && S.buildings[0].type == BType::Lantern && S.buildings[0].level == 0, "");
    CHECK(village::hqRuined(S) && village::builders(S, 0).total == 1, "ruins and one builder");
    CHECK(B(BType::Lantern).maxLevel == kMaxLantern && kMaxLantern == 5, "");
    CHECK(!village::canBuild(S, BType::Ballista, 0).ok, "nothing can be built before the restoration");
    // ночи ждут первой обороны
    village::tick(S, 10LL * 3600 * 1000);
    CHECK(!village::nightsActive(S) && !village::nightDue(S, 10LL * 3600 * 1000), "");
    // восстановление: стоит руду и время, после — уровень 1 и первые постройки
    long long ore = S.ore;
    CHECK(village::startUpgrade(S, S.buildings[0].id, 0).ok, "");
    CHECK(S.ore == ore - B(BType::Lantern).cost[0], "");
    village::tick(S, 60 * 1000);
    CHECK(!village::hqRuined(S) && village::lanternLevel(S) == 1, "level=%d", S.buildings[0].level);
    CHECK(village::canBuild(S, BType::Ballista, 60 * 1000).ok, "first defence is available after restoration");
}
