// Балансные данные и геометрия.
#include <algorithm>
#include <cmath>

#include "core.h"

namespace lf {

const int kRaidSearchCost[6] = {50, 150, 400, 900, 1800, 3000};
const int kBaseLoot[6] = {800, 2000, 5000, 12000, 25000, 45000};
const int kHutCost[5] = {0, 0, 20, 40, 80};

namespace {

constexpr uint32_t hex(uint32_t rgb) { return ((rgb >> 16) & 255) | (rgb & 0xff00) | ((rgb & 255) << 16) | 0xff000000u; }

const int kLimits[kBTypes][6] = {
    {1, 1, 1, 1, 1, 1},           // Lantern
    {1, 2, 2, 3, 4, 5},           // Beacon
    {2, 3, 4, 5, 6, 6},           // OreMine
    {2, 3, 4, 5, 6, 6},           // AetherWell
    {1, 1, 2, 2, 3, 3},           // OreStore
    {1, 1, 2, 2, 3, 3},           // AetherStore
    {1, 1, 2, 2, 2, 3},           // Barracks
    {1, 2, 2, 3, 3, 3},           // Camp
    {2, 3, 3, 4, 4, 5},           // BuilderHut
    {1, 2, 3, 3, 4, 5},           // Ballista
    {0, 1, 1, 2, 2, 3},           // Brazier
    {0, 0, 1, 1, 2, 2},           // Catapult
    {0, 0, 0, 1, 1, 2},           // Prism
    {20, 35, 55, 80, 110, 150},   // Wall
    {0, 0, 1, 1, 1, 1},           // Workshop
    {0, 2, 3, 4, 5, 6},           // Trap
};

std::vector<BuildingDef> makeBuildings() {
    std::vector<BuildingDef> v(kBTypes);
    auto& L = v[(int)BType::Lantern];
    L.type = BType::Lantern; L.id = "lantern"; L.name = "Штаб"; L.size = 4; L.maxLevel = 5; L.category = Category::Core; L.costRes = Res::Ore;
    L.desc = "Центр поселения: отсюда управляют стройкой и обороной. Уровень штаба открывает новые постройки и поднимает их предел.";
    L.cost = {300, 1200, 4000, 12000, 30000, 70000}; L.time = {30, 60, 240, 600, 1200, 2400};  // [0] — восстановление из руин
    L.hp = {1500, 2100, 2800, 3600, 4500, 5500}; L.light = {7, 8, 9, 10, 11, 12}; L.storage = {1000, 1500, 2000, 2500, 3000, 4000};

    auto& Bc = v[(int)BType::Beacon];
    Bc.type = BType::Beacon; Bc.id = "beacon"; Bc.name = "Светоч"; Bc.size = 2; Bc.maxLevel = 6; Bc.category = Category::Light; Bc.costRes = Res::Ore;
    Bc.desc = "Раздвигает тьму: расширяет зону строительства и питает оборону светом. Главная цель врага.";
    Bc.cost = {500, 2000, 6000, 15000, 35000, 70000}; Bc.time = {20, 90, 300, 600, 1200, 2000};
    Bc.hp = {350, 450, 560, 680, 820, 980}; Bc.light = {5, 5.5f, 6, 6.5f, 7, 7.5f};

    for (auto [t, id, name, res, prod, desc] : {
             std::make_tuple(BType::OreMine, "oreMine", "Рудная шахта", Res::Aether, Res::Ore, "Добывает руду из-под Мглы."),
             std::make_tuple(BType::AetherWell, "aetherWell", "Эфирный колодец", Res::Ore, Res::Aether, "Конденсирует Эфир из тумана. Говорят, в нём слышны чужие голоса.")}) {
        auto& d = v[(int)t];
        d.type = t; d.id = id; d.name = name; d.size = 3; d.maxLevel = 6; d.category = Category::Economy; d.costRes = res; d.desc = desc;
        d.cost = {150, 400, 1200, 3500, 8000, 16000}; d.time = {10, 45, 150, 400, 900, 1500};
        d.hp = {400, 460, 530, 610, 700, 800}; d.rate = {12, 20, 30, 42, 56, 72}; d.cap = {500, 900, 1500, 2400, 3600, 5200}; d.produces = prod;
    }
    for (auto [t, id, name, res, stores, desc] : {
             std::make_tuple(BType::OreStore, "oreStore", "Рудный склад", Res::Aether, Res::Ore, "Хранит руду."),
             std::make_tuple(BType::AetherStore, "aetherStore", "Эфирный резервуар", Res::Ore, Res::Aether, "Хранит Эфир.")}) {
        auto& d = v[(int)t];
        d.type = t; d.id = id; d.name = name; d.size = 3; d.maxLevel = 6; d.category = Category::Economy; d.costRes = res; d.desc = desc;
        d.cost = {300, 1000, 3000, 8000, 20000, 45000}; d.time = {15, 60, 200, 500, 1000, 1800};
        d.hp = {700, 850, 1000, 1200, 1450, 1700}; d.storage = {3000, 7000, 15000, 35000, 75000, 150000}; d.stores = stores;
    }
    auto& Ba = v[(int)BType::Barracks];
    Ba.type = BType::Barracks; Ba.id = "barracks"; Ba.name = "Казарма"; Ba.size = 3; Ba.maxLevel = 6; Ba.category = Category::Army; Ba.costRes = Res::Aether;
    Ba.desc = "Тренирует войска. Каждый уровень открывает новый отряд. Несколько казарм ускоряют обучение.";
    Ba.cost = {200, 800, 2500, 7000, 18000, 40000}; Ba.time = {10, 60, 200, 500, 1000, 1800}; Ba.hp = {450, 520, 600, 690, 790, 900};

    auto& Ca = v[(int)BType::Camp];
    Ca.type = BType::Camp; Ca.id = "camp"; Ca.name = "Лагерь"; Ca.size = 3; Ca.maxLevel = 6; Ca.category = Category::Army; Ca.costRes = Res::Aether;
    Ca.desc = "Здесь отдыхает армия. Определяет её размер.";
    Ca.cost = {250, 900, 2800, 7500, 19000, 42000}; Ca.time = {10, 60, 200, 500, 1000, 1800};
    Ca.hp = {350, 400, 460, 530, 610, 700}; Ca.capacity = {20, 30, 40, 55, 70, 85};

    auto& H = v[(int)BType::BuilderHut];
    H.type = BType::BuilderHut; H.id = "builderHut"; H.name = "Хижина строителя"; H.size = 2; H.maxLevel = 1; H.category = Category::Economy; H.costRes = Res::Shards;
    H.desc = "Каждая хижина — ещё один строитель."; H.cost = {0}; H.time = {0}; H.hp = {250};

    auto& Bl = v[(int)BType::Ballista];
    Bl.type = BType::Ballista; Bl.id = "ballista"; Bl.name = "Арбалет"; Bl.size = 3; Bl.maxLevel = 6; Bl.category = Category::Defense; Bl.costRes = Res::Ore;
    Bl.desc = "Точный выстрел по одной цели. Бьёт и наземных, и летающих.";
    Bl.cost = {250, 900, 2800, 7500, 18000, 40000}; Bl.time = {15, 60, 200, 500, 1000, 1800}; Bl.hp = {420, 520, 640, 780, 940, 1120};
    Bl.range = 8.5f; Bl.cd = 0.9f; Bl.dmg = {20, 27, 36, 47, 60, 76}; Bl.ground = true; Bl.air = true; Bl.proj = Proj::Bolt; Bl.projSpeed = 16;

    auto& Br = v[(int)BType::Brazier];
    Br.type = BType::Brazier; Br.id = "brazier"; Br.name = "Жаровня"; Br.size = 2; Br.maxLevel = 5; Br.category = Category::Defense; Br.costRes = Res::Ore;
    Br.desc = "Выплёвывает пламя по площади вблизи. Гроза толп.";
    Br.cost = {600, 1800, 5000, 12000, 28000}; Br.time = {30, 120, 400, 800, 1500}; Br.hp = {550, 650, 770, 900, 1050};
    Br.range = 4.5f; Br.cd = 0.5f; Br.dmg = {9, 12, 16, 21, 27}; Br.splash = 1.4f; Br.ground = true; Br.proj = Proj::Flame;

    auto& Cp = v[(int)BType::Catapult];
    Cp.type = BType::Catapult; Cp.id = "catapult"; Cp.name = "Катапульта"; Cp.size = 3; Cp.maxLevel = 4; Cp.category = Category::Defense; Cp.costRes = Res::Ore;
    Cp.desc = "Мечет камни очень далеко. Не попадает по тем, кто подошёл вплотную.";
    Cp.cost = {2500, 7000, 16000, 35000}; Cp.time = {120, 400, 900, 1600}; Cp.hp = {700, 820, 960, 1120};
    Cp.range = 11; Cp.minRange = 3.5f; Cp.cd = 3.5f; Cp.dmg = {55, 72, 94, 120}; Cp.splash = 1.6f; Cp.ground = true; Cp.proj = Proj::Rock; Cp.projSpeed = 7;

    auto& Pr = v[(int)BType::Prism];
    Pr.type = BType::Prism; Pr.id = "prism"; Pr.name = "Призма"; Pr.size = 2; Pr.maxLevel = 3; Pr.category = Category::Defense; Pr.costRes = Res::Ore;
    Pr.desc = "Фокусирует свет в луч. Чем дольше держит цель — тем сильнее жжёт.";
    Pr.cost = {9000, 22000, 48000}; Pr.time = {600, 1200, 2000}; Pr.hp = {800, 950, 1120};
    Pr.range = 7; Pr.dps = {25, 33, 43}; Pr.rampMax = 4; Pr.rampRate = 2; Pr.ground = true; Pr.air = true; Pr.beam = true;

    auto& W = v[(int)BType::Wall];
    W.type = BType::Wall; W.id = "wall"; W.name = "Стена"; W.size = 1; W.maxLevel = 6; W.category = Category::Wall; W.costRes = Res::Ore;
    W.desc = "Задерживает наземных врагов.";
    W.cost = {50, 150, 500, 1200, 3000, 7000}; W.time = {0, 0, 0, 0, 0, 0}; W.hp = {300, 550, 900, 1400, 2100, 3000};
    auto& K = v[(int)BType::Workshop];
    K.type = BType::Workshop; K.id = "workshop"; K.name = "Кузница"; K.size = 3; K.maxLevel = 4; K.category = Category::Army; K.costRes = Res::Aether;
    K.desc = "Здесь куют оружие и доспехи: каждый уровень отряда даёт +12% к здоровью и урону. Уровень Кузницы ограничивает уровень отрядов.";
    K.cost = {3000, 9000, 25000, 60000}; K.time = {120, 600, 1500, 3000}; K.hp = {800, 950, 1120, 1300};

    auto& Tr = v[(int)BType::Trap];
    Tr.type = BType::Trap; Tr.id = "trap"; Tr.name = "Огненная ловушка"; Tr.size = 1; Tr.maxLevel = 4; Tr.category = Category::Defense; Tr.costRes = Res::Ore;
    Tr.desc = "Спрятана в земле. Когда рядом проходит враг — взрывается и обжигает всех вокруг. Срабатывает один раз за бой.";
    Tr.cost = {400, 1500, 5000, 14000}; Tr.time = {5, 60, 240, 600}; Tr.hp = {1, 1, 1, 1};
    Tr.range = 0.9f; Tr.dmg = {60, 85, 115, 150}; Tr.splash = 1.7f; Tr.ground = true; Tr.trap = true;
    return v;
}

std::vector<TroopDef> makeTroops() {
    std::vector<TroopDef> v(kTroopTypes);
    auto set = [&](TroopDef d) { v[(int)d.type] = d; };
    TroopDef d;
    d = {}; d.type = TroopType::Torch; d.id = "torch"; d.name = "Факельщик"; d.housing = 1; d.cost = 25; d.time = 4; d.unlock = 1;
    d.hp = 70; d.dmg = 9; d.cd = 0.8f; d.range = 0.6f; d.speed = 1.5f; d.color = hex(0xe8a04a); d.desc = "Дешёвый и смелый. Бьёт всё, что ближе."; set(d);
    d = {}; d.type = TroopType::Archer; d.id = "archer"; d.name = "Стрелок"; d.housing = 1; d.cost = 50; d.time = 6; d.unlock = 2;
    d.hp = 35; d.dmg = 8; d.cd = 0.8f; d.range = 3.5f; d.speed = 1.5f; d.ranged = Proj::Arrow; d.color = hex(0x9be06a); d.desc = "Стреляет через стены. Хрупок."; set(d);
    d = {}; d.type = TroopType::Shield; d.id = "shield"; d.name = "Щитоносец"; d.housing = 5; d.cost = 250; d.time = 20; d.unlock = 3;
    d.hp = 700; d.dmg = 30; d.cd = 1.5f; d.range = 0.7f; d.speed = 0.9f; d.target = Target::Defense; d.color = hex(0x8aa0c8); d.desc = "Медленный танк. Идёт прямо к обороне."; set(d);
    d = {}; d.type = TroopType::Sapper; d.id = "sapper"; d.name = "Подрывник"; d.housing = 2; d.cost = 300; d.time = 15; d.unlock = 4;
    d.hp = 45; d.dmg = 450; d.cd = 0; d.range = 0.6f; d.speed = 2.2f; d.target = Target::Wall; d.suicide = true; d.splash = 1.3f; d.color = hex(0xd06060); d.desc = "Бежит к ближайшей стене и взрывается."; set(d);
    d = {}; d.type = TroopType::Shade; d.id = "shade"; d.name = "Тенекрад"; d.housing = 3; d.cost = 400; d.time = 20; d.unlock = 5;
    d.hp = 200; d.dmg = 22; d.cd = 0.6f; d.range = 0.7f; d.speed = 2.0f; d.target = Target::Light; d.lightMult = 3; d.stealth = true; d.color = hex(0x9a7ad8); d.desc = "Невидим во тьме. Тройной урон по Фонарю и Светочам."; set(d);
    d = {}; d.type = TroopType::Owl; d.id = "owl"; d.name = "Филин"; d.housing = 4; d.cost = 500; d.time = 25; d.unlock = 6;
    d.hp = 260; d.dmg = 24; d.cd = 0.9f; d.range = 2.5f; d.speed = 1.8f; d.target = Target::Defense; d.air = true; d.ranged = Proj::Feather; d.color = hex(0xd8c49a); d.desc = "Летает над стенами. Уязвим для Арбалетов и Призм."; set(d);

    d = {}; d.type = TroopType::Morok; d.id = "morok"; d.name = "Морок"; d.haze = true; d.housing = 1; d.unlock = 1;
    d.hp = 60; d.dmg = 8; d.cd = 0.8f; d.range = 0.6f; d.speed = 1.8f; d.color = hex(0x6b5a8a); d.desc = ""; set(d);
    d = {}; d.type = TroopType::Brute; d.id = "brute"; d.name = "Громила Мглы"; d.haze = true; d.housing = 5; d.unlock = 2;
    d.hp = 600; d.dmg = 35; d.cd = 1.6f; d.range = 0.7f; d.speed = 0.85f; d.target = Target::Defense; d.color = hex(0x4a3d63); d.desc = ""; set(d);
    d = {}; d.type = TroopType::Leech; d.id = "leech"; d.name = "Светоед"; d.haze = true; d.housing = 3; d.unlock = 3;
    d.hp = 160; d.dmg = 18; d.cd = 0.6f; d.range = 0.7f; d.speed = 1.6f; d.target = Target::Light; d.lightMult = 3; d.stealth = true; d.color = hex(0x3a2a55); d.desc = ""; set(d);
    d = {}; d.type = TroopType::Wraith; d.id = "wraith"; d.name = "Призрак"; d.haze = true; d.housing = 4; d.unlock = 4;
    d.hp = 200; d.dmg = 18; d.cd = 1.0f; d.range = 2.2f; d.speed = 1.6f; d.air = true; d.ranged = Proj::Wisp; d.color = hex(0xa090c8); d.desc = ""; set(d);
    d = {}; d.type = TroopType::Hero; d.id = "hero"; d.name = "Фонарщик"; d.housing = 0; d.unlock = 1;
    d.hp = 900; d.dmg = 42; d.cd = 1.1f; d.range = 0.75f; d.speed = 1.35f; d.color = hex(0xffd36b);
    d.desc = "Герой. Бьёт посохом с фонарём, а «Вспышкой» ослепляет оборону и подлечивает отряды."; set(d);
    return v;
}

const std::vector<BuildingDef>& buildings() { static const auto v = makeBuildings(); return v; }
const std::vector<TroopDef>& troops() { static const auto v = makeTroops(); return v; }

}  // namespace

const BuildingDef& B(BType t) { return buildings()[(int)t]; }
const TroopDef& T(TroopType t) { return troops()[(int)t]; }
int limit(BType t, int lantern) { return kLimits[(int)t][std::clamp(lantern, 1, kMaxLantern) - 1]; }
int unlockLantern(BType t) {
    for (int i = 0; i < 6; i++) if (kLimits[(int)t][i] > 0) return i + 1;
    return kMaxLantern + 1;
}
int eclipseCharges(int lantern) { return lantern >= 2 ? lantern / 2 : 0; }

// ---------- Геометрия ----------
double Rng::next() {
    a_ += 0x6D2B79F5u;
    uint32_t t = (a_ ^ (a_ >> 15)) * (1u | a_);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

float distToRect(float px, float py, float x, float y, float size) {
    float dx = std::max({x - px, 0.f, px - (x + size)});
    float dy = std::max({y - py, 0.f, py - (y + size)});
    return std::hypot(dx, dy);
}

bool rectsOverlap(int ax, int ay, int as, int bx, int by, int bs) {
    return ax < bx + bs && bx < ax + as && ay < by + bs && by < ay + as;
}

bool isLit(float px, float py, const std::vector<LightSource>& src, const std::vector<Eclipse>* ecl) {
    if (ecl)
        for (const auto& e : *ecl) if (std::hypot(px - e.x, py - e.y) <= e.r) return false;
    for (const auto& s : src) if (std::hypot(px - s.x, py - s.y) <= s.r) return true;
    return false;
}

std::vector<uint8_t> litGrid(const std::vector<LightSource>& src, const std::vector<Eclipse>* ecl) {
    std::vector<uint8_t> g(kGrid * kGrid);
    for (int j = 0; j < kGrid; j++)
        for (int i = 0; i < kGrid; i++) g[j * kGrid + i] = isLit(i + 0.5f, j + 0.5f, src, ecl) ? 1 : 0;
    return g;
}

bool footprintLit(int x, int y, int size, const std::vector<LightSource>& src) {
    for (int j = y; j < y + size; j++)
        for (int i = x; i < x + size; i++)
            if (!isLit(i + 0.5f, j + 0.5f, src)) return false;
    return true;
}

}  // namespace lf
