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
    {1, 1, 1, 1, 1, 1},           // Lantern — Острог
    {0, 1, 1, 1, 2, 2},           // Beacon — Торг
    {2, 3, 4, 5, 6, 6},           // OreMine — Лесосека
    {2, 3, 4, 5, 6, 6},           // AetherWell — Мельница
    {1, 1, 2, 2, 3, 3},           // OreStore — Лесной двор
    {1, 1, 2, 2, 3, 3},           // AetherStore — Амбар
    {1, 1, 2, 2, 2, 3},           // Barracks — Ратная изба
    {1, 2, 2, 3, 3, 3},           // Camp — Стан
    {2, 3, 3, 4, 4, 5},           // BuilderHut — Изба плотника
    {1, 2, 3, 3, 4, 5},           // Ballista — Самострел
    {0, 1, 1, 2, 2, 3},           // Brazier — Смоляной котёл
    {0, 0, 1, 1, 2, 2},           // Catapult — Пушка
    {0, 0, 0, 1, 1, 2},           // Prism — Затинная пищаль
    {20, 35, 55, 80, 110, 150},   // Wall — Частокол
    {0, 0, 1, 1, 1, 1},           // Workshop — Кузница
    {0, 2, 3, 4, 5, 6},           // Trap — Волчья яма
};

std::vector<BuildingDef> makeBuildings() {
    std::vector<BuildingDef> v(kBTypes);
    auto& L = v[(int)BType::Lantern];
    L.type = BType::Lantern; L.id = "lantern"; L.name = "Острог"; L.size = 4; L.maxLevel = 5; L.category = Category::Core; L.costRes = Res::Ore;
    L.desc = "Деревянная крепость на старом каменном фундаменте — сердце поселения. Уровень острога открывает новые постройки и поднимает их предел.";
    L.cost = {300, 1200, 4000, 12000, 30000, 70000}; L.time = {30, 60, 240, 600, 1200, 2400};  // [0] — восстановление из руин
    L.hp = {1500, 2100, 2800, 3600, 4500, 5500}; L.storage = {1000, 1500, 2000, 2500, 3000, 4000};

    auto& Tg = v[(int)BType::Beacon];
    Tg.type = BType::Beacon; Tg.id = "beacon"; Tg.name = "Торг"; Tg.size = 2; Tg.maxLevel = 5; Tg.category = Category::Economy; Tg.costRes = Res::Ore;
    Tg.desc = "Лавка, где пушнину и мёд меняют на серебро. Серебро копится медленно — его можно собрать раз в несколько часов.";
    Tg.cost = {1500, 4000, 10000, 24000, 50000}; Tg.time = {60, 300, 900, 1800, 3000};
    Tg.hp = {400, 480, 570, 670, 780}; Tg.rate = {1, 2, 3, 4, 5}; Tg.cap = {6, 10, 15, 20, 26}; Tg.produces = Res::Shards;  // серебро в час

    for (auto [t, id, name, res, prod, desc] : {
             std::make_tuple(BType::OreMine, "oreMine", "Лесосека", Res::Aether, Res::Ore, "Лесорубы валят и разделывают лес. Брёвна идут на все постройки и частокол."),
             std::make_tuple(BType::AetherWell, "aetherWell", "Мельница", Res::Ore, Res::Aether, "Мелет зерно с окрестных полей. Хлеб кормит войско.")}) {
        auto& d = v[(int)t];
        d.type = t; d.id = id; d.name = name; d.size = 3; d.maxLevel = 6; d.category = Category::Economy; d.costRes = res; d.desc = desc;
        d.cost = {150, 400, 1200, 3500, 8000, 16000}; d.time = {10, 45, 150, 400, 900, 1500};
        d.hp = {400, 460, 530, 610, 700, 800}; d.rate = {12, 20, 30, 42, 56, 72}; d.cap = {500, 900, 1500, 2400, 3600, 5200}; d.produces = prod;
    }
    for (auto [t, id, name, res, stores, desc] : {
             std::make_tuple(BType::OreStore, "oreStore", "Лесной двор", Res::Aether, Res::Ore, "Брёвна сохнут в штабелях под навесом. Чем выше уровень, тем больше леса можно запасти."),
             std::make_tuple(BType::AetherStore, "aetherStore", "Амбар", Res::Ore, Res::Aether, "Амбар на сваях: зерно не достанут ни сырость, ни мыши.")}) {
        auto& d = v[(int)t];
        d.type = t; d.id = id; d.name = name; d.size = 3; d.maxLevel = 6; d.category = Category::Economy; d.costRes = res; d.desc = desc;
        d.cost = {300, 1000, 3000, 8000, 20000, 45000}; d.time = {15, 60, 200, 500, 1000, 1800};
        d.hp = {700, 850, 1000, 1200, 1450, 1700}; d.storage = {3000, 7000, 15000, 35000, 75000, 150000}; d.stores = stores;
    }
    auto& Ba = v[(int)BType::Barracks];
    Ba.type = BType::Barracks; Ba.id = "barracks"; Ba.name = "Ратная изба"; Ba.size = 3; Ba.maxLevel = 6; Ba.category = Category::Army; Ba.costRes = Res::Aether;
    Ba.desc = "Здесь учат ратному делу. Каждый уровень открывает новый отряд; несколько изб учат быстрее.";
    Ba.cost = {200, 800, 2500, 7000, 18000, 40000}; Ba.time = {10, 60, 200, 500, 1000, 1800}; Ba.hp = {450, 520, 600, 690, 790, 900};

    auto& Ca = v[(int)BType::Camp];
    Ca.type = BType::Camp; Ca.id = "camp"; Ca.name = "Стан"; Ca.size = 3; Ca.maxLevel = 6; Ca.category = Category::Army; Ca.costRes = Res::Aether;
    Ca.desc = "Шатры и костры, где отдыхает войско. Чем больше станов, тем больше войско.";
    Ca.cost = {250, 900, 2800, 7500, 19000, 42000}; Ca.time = {10, 60, 200, 500, 1000, 1800};
    Ca.hp = {350, 400, 460, 530, 610, 700}; Ca.capacity = {20, 30, 40, 55, 70, 85};

    auto& H = v[(int)BType::BuilderHut];
    H.type = BType::BuilderHut; H.id = "builderHut"; H.name = "Изба плотника"; H.size = 2; H.maxLevel = 1; H.category = Category::Economy; H.costRes = Res::Shards;
    H.desc = "Каждый плотник — ещё одна стройка одновременно."; H.cost = {0}; H.time = {0}; H.hp = {250};

    auto& Bl = v[(int)BType::Ballista];
    Bl.type = BType::Ballista; Bl.id = "ballista"; Bl.name = "Самострел"; Bl.size = 3; Bl.maxLevel = 6; Bl.category = Category::Defense; Bl.costRes = Res::Ore;
    Bl.desc = "Тяжёлый станковый арбалет на помосте. Бьёт точно и далеко, по одной цели.";
    Bl.cost = {250, 900, 2800, 7500, 18000, 40000}; Bl.time = {15, 60, 200, 500, 1000, 1800}; Bl.hp = {420, 520, 640, 780, 940, 1120};
    Bl.range = 8.5f; Bl.cd = 0.9f; Bl.dmg = {20, 27, 36, 47, 60, 76}; Bl.ground = true; Bl.air = true; Bl.proj = Proj::Bolt; Bl.projSpeed = 16;

    auto& Br = v[(int)BType::Brazier];
    Br.type = BType::Brazier; Br.id = "brazier"; Br.name = "Смоляной котёл"; Br.size = 2; Br.maxLevel = 5; Br.category = Category::Defense; Br.costRes = Res::Ore;
    Br.desc = "Кипящая смола, которую льют на тех, кто подошёл близко. Жжёт всех в луже — гроза толпы.";
    Br.cost = {600, 1800, 5000, 12000, 28000}; Br.time = {30, 120, 400, 800, 1500}; Br.hp = {550, 650, 770, 900, 1050};
    Br.range = 4.5f; Br.cd = 0.5f; Br.dmg = {9, 12, 16, 21, 27}; Br.splash = 1.4f; Br.ground = true; Br.proj = Proj::Pitch;

    auto& Cp = v[(int)BType::Catapult];
    Cp.type = BType::Catapult; Cp.id = "catapult"; Cp.name = "Пушка"; Cp.size = 3; Cp.maxLevel = 4; Cp.category = Category::Defense; Cp.costRes = Res::Ore;
    Cp.desc = "Чугунное ядро летит очень далеко и разбрасывает всех вокруг. Тех, кто подошёл вплотную, не достать.";
    Cp.cost = {2500, 7000, 16000, 35000}; Cp.time = {120, 400, 900, 1600}; Cp.hp = {700, 820, 960, 1120};
    Cp.range = 11; Cp.minRange = 3.5f; Cp.cd = 3.5f; Cp.dmg = {55, 72, 94, 120}; Cp.splash = 1.6f; Cp.ground = true; Cp.proj = Proj::Ball; Cp.projSpeed = 7;

    auto& Pi = v[(int)BType::Prism];
    Pi.type = BType::Prism; Pi.id = "prism"; Pi.name = "Затинная пищаль"; Pi.size = 2; Pi.maxLevel = 3; Pi.category = Category::Defense; Pi.costRes = Res::Ore;
    Pi.desc = "Тяжёлое ружьё на стене. Перезаряжается долго, но пуля валит даже щитника.";
    Pi.cost = {9000, 22000, 48000}; Pi.time = {600, 1200, 2000}; Pi.hp = {800, 950, 1120};
    Pi.range = 7.5f; Pi.cd = 2.4f; Pi.dmg = {110, 140, 175}; Pi.ground = true; Pi.air = true; Pi.proj = Proj::Bullet; Pi.projSpeed = 30;

    auto& W = v[(int)BType::Wall];
    W.type = BType::Wall; W.id = "wall"; W.name = "Частокол"; W.size = 1; W.maxLevel = 6; W.category = Category::Wall; W.costRes = Res::Ore;
    W.desc = "Звено из заострённых брёвен. Пешие обходят его или рубят; лучники и стрельцы бьют поверх.";
    W.cost = {50, 150, 500, 1200, 3000, 7000}; W.time = {0, 0, 0, 0, 0, 0}; W.hp = {300, 550, 900, 1400, 2100, 3000};
    auto& K = v[(int)BType::Workshop];
    K.type = BType::Workshop; K.id = "workshop"; K.name = "Кузница"; K.size = 3; K.maxLevel = 4; K.category = Category::Army; K.costRes = Res::Aether;
    K.desc = "Кузнец куёт оружие и доспехи: каждый уровень отряда даёт +12% к здоровью и урону. Уровень кузницы ограничивает уровень отрядов.";
    K.cost = {3000, 9000, 25000, 60000}; K.time = {120, 600, 1500, 3000}; K.hp = {800, 950, 1120, 1300};

    auto& Tr = v[(int)BType::Trap];
    Tr.type = BType::Trap; Tr.id = "trap"; Tr.name = "Волчья яма"; Tr.size = 1; Tr.maxLevel = 4; Tr.category = Category::Defense; Tr.costRes = Res::Ore;
    Tr.desc = "Яма с кольями, прикрытая ветками. Кто наступит рядом — проваливается; срабатывает один раз за бой.";
    Tr.cost = {400, 1500, 5000, 14000}; Tr.time = {5, 60, 240, 600}; Tr.hp = {1, 1, 1, 1};
    Tr.range = 0.9f; Tr.dmg = {60, 85, 115, 150}; Tr.splash = 1.3f; Tr.ground = true; Tr.trap = true;
    return v;
}

std::vector<TroopDef> makeTroops() {
    std::vector<TroopDef> v(kTroopTypes);
    auto set = [&](TroopDef d) { v[(int)d.type] = d; };
    TroopDef d;
    d = {}; d.type = TroopType::Torch; d.trait = "Рубит частокол"; d.id = "torch"; d.name = "Ополченец"; d.housing = 1; d.cost = 25; d.time = 4; d.unlock = 1;
    d.hp = 80; d.dmg = 11; d.cd = 0.9f; d.range = 0.6f; d.speed = 1.45f; d.wallMult = 1.6f; d.color = hex(0xb08a5a);
    d.desc = "Мужик в тулупе с топором и деревянным щитом. Дёшев, бьёт что ближе; топором рубит частокол в полтора раза быстрее."; set(d);
    d = {}; d.type = TroopType::Archer; d.trait = "Бьёт через стены"; d.id = "archer"; d.name = "Лучник"; d.housing = 1; d.cost = 50; d.time = 6; d.unlock = 1;
    d.hp = 38; d.dmg = 9; d.cd = 0.85f; d.range = 3.6f; d.speed = 1.55f; d.ranged = Proj::Arrow; d.color = hex(0x7a9a5a);
    d.desc = "Стреляет из-за частокола. Хрупок — держи его за спинами пехоты."; set(d);
    d = {}; d.type = TroopType::Shield; d.trait = "Щит от стрел"; d.id = "shield"; d.name = "Щитник"; d.housing = 5; d.cost = 250; d.time = 20; d.unlock = 2;
    d.hp = 760; d.dmg = 28; d.cd = 1.5f; d.range = 0.75f; d.speed = 0.85f; d.target = Target::Defense; d.rangedArmor = 0.5f;
    d.color = hex(0x8a94a0);
    d.desc = "В кольчуге и шлеме, с большим щитом и копьём. Идёт прямо на оборону; щит вдвое ослабляет стрелы, болты и пули."; set(d);
    d = {}; d.type = TroopType::Sapper; d.trait = "Ломает частокол"; d.id = "sapper"; d.name = "Таран"; d.housing = 4; d.cost = 300; d.time = 18; d.unlock = 3;
    d.hp = 420; d.dmg = 40; d.cd = 2.2f; d.range = 0.7f; d.speed = 0.8f; d.target = Target::Wall; d.wallMult = 10; d.color = hex(0x6a5038);
    d.desc = "Четверо несут окованное бревно. Ломает частокол с нескольких ударов, по остальному бьёт слабо."; set(d);
    d = {}; d.type = TroopType::Shade; d.trait = "Натиск ×3"; d.id = "shade"; d.name = "Всадник"; d.housing = 3; d.cost = 350; d.time = 18; d.unlock = 4;
    d.hp = 260; d.dmg = 26; d.cd = 0.8f; d.range = 0.7f; d.speed = 2.8f; d.target = Target::Economy; d.econMult = 2; d.chargeMult = 3;
    d.color = hex(0x9a6a40);
    d.desc = "Конный налётчик с саблей. Быстро уходит вглубь и громит хозяйство — двойной урон; первый удар с налёта по каждой цели тройной."; set(d);
    d = {}; d.type = TroopType::Owl; d.trait = "Пробивает оборону"; d.id = "owl"; d.name = "Стрелец"; d.housing = 3; d.cost = 400; d.time = 22; d.unlock = 5;
    d.hp = 120; d.dmg = 60; d.cd = 2.6f; d.range = 4.8f; d.speed = 1.2f; d.ranged = Proj::Bullet; d.defenseMult = 1.5f; d.color = hex(0xb04038);
    d.desc = "В красном кафтане, с пищалью и бердышом. Бьёт дальше всех и пробивает оборону (полуторный урон), но долго перезаряжается."; set(d);

    d = {}; d.type = TroopType::Morok; d.id = "morok"; d.name = "Разбойник"; d.raider = true; d.housing = 1; d.unlock = 1;
    d.hp = 60; d.dmg = 8; d.cd = 0.8f; d.range = 0.6f; d.speed = 1.7f; d.color = hex(0x6a5a48); d.desc = "Лихой человек с кистенём."; set(d);
    d = {}; d.type = TroopType::Brute; d.id = "brute"; d.name = "Атаман"; d.raider = true; d.housing = 5; d.unlock = 2;
    d.hp = 600; d.dmg = 35; d.cd = 1.6f; d.range = 0.7f; d.speed = 0.85f; d.target = Target::Defense; d.color = hex(0x4a3a30);
    d.desc = "Вожак ватаги в трофейной кольчуге. Идёт на оборону."; set(d);
    d = {}; d.type = TroopType::Leech; d.id = "leech"; d.name = "Поджигатель"; d.raider = true; d.housing = 3; d.unlock = 3;
    d.hp = 150; d.dmg = 18; d.cd = 0.6f; d.range = 0.7f; d.speed = 1.8f; d.target = Target::Economy; d.econMult = 3; d.color = hex(0xa05a30);
    d.desc = "С факелом и смолой бежит к амбарам и складам."; set(d);
    d = {}; d.type = TroopType::Wraith; d.id = "wraith"; d.name = "Лихой стрелок"; d.raider = true; d.housing = 2; d.unlock = 4;
    d.hp = 90; d.dmg = 14; d.cd = 1.0f; d.range = 3.2f; d.speed = 1.5f; d.ranged = Proj::Arrow; d.color = hex(0x5a6048);
    d.desc = "Бьёт из лука из-за деревьев."; set(d);
    d = {}; d.type = TroopType::Hero; d.id = "hero"; d.name = "Воевода"; d.housing = 0; d.unlock = 1;
    d.hp = 900; d.dmg = 42; d.cd = 1.1f; d.range = 0.75f; d.speed = 1.35f; d.color = hex(0xc8a050);
    d.desc = "Герой в доспехе с саблей. «Боевой клич» подлечивает отряды вокруг и на время делает их яростнее."; set(d);
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

}  // namespace lf
