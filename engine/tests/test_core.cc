// Тесты игрового ядра. Запуск: ctest или ./build/lf_tests
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

int main() {
    for (auto& [name, fn] : registry()) {
        int before = failures;
        fn();
        std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", name);
    }
    std::printf("%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
