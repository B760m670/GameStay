// Логика деревни.
#include <algorithm>
#include <cmath>
#include <sstream>

#include "core.h"

namespace lf::village {

namespace {
BuildingRec make(VillageState& S, BType t, int x, int y, int level, long long now) {
    BuildingRec b;
    b.id = S.nextId++;
    b.type = t;
    b.x = x;
    b.y = y;
    b.level = level;
    b.accAt = now;
    return b;
}

// Опыт за стройку: растёт с длительностью (как корень из секунд), минимум 1.
long long buildXp(const BuildingRec& b) {
    const auto& d = b.def();
    int idx = std::min(b.level, (int)d.time.size() - 1);
    return std::max(1LL, (long long)std::sqrt((double)std::max(0, d.time[idx])));
}

void finishBuild(BuildingRec& b, long long now) {
    if (b.def().produces != Res::None) {
        b.acc = collectorAmount(b, now);
        b.accAt = now;
    }
    b.level += 1;
    b.busyUntil = 0;
}
}  // namespace

VillageState create(long long now) {
    VillageState S;
    S.created = now;
    S.ore = 1500;
    S.aether = 1500;
    S.shards = 15;
    S.trainTickAt = now;
    S.nightAt = now + kFirstNightMs;
    auto add = [&](BType t, int x, int y) { S.buildings.push_back(make(S, t, x, y, 1, now)); };
    add(BType::Lantern, 20, 20);
    add(BType::OreMine, 15, 19);
    add(BType::AetherWell, 26, 21);
    add(BType::OreStore, 20, 25);
    add(BType::AetherStore, 21, 15);
    add(BType::Barracks, 24, 24);
    add(BType::Camp, 16, 23);
    add(BType::Ballista, 24, 17);
    add(BType::BuilderHut, 18, 17);
    add(BType::BuilderHut, 18, 26);
    S.army[(int)TroopType::Torch] = 10;
    return S;
}

BuildingRec* get(VillageState& S, int id) {
    for (auto& b : S.buildings) if (b.id == id) return &b;
    return nullptr;
}

int lanternLevel(const VillageState& S) {
    for (const auto& b : S.buildings) if (b.type == BType::Lantern) return std::max(1, b.level);
    return 1;
}

int countOf(const VillageState& S, BType t) {
    return (int)std::count_if(S.buildings.begin(), S.buildings.end(), [&](const BuildingRec& b) { return b.type == t; });
}

int maxLevelAllowed(BType t, int lantern) {
    if (t == BType::Lantern) return kMaxLantern;
    return std::max(0, std::min(B(t).maxLevel, lantern - unlockLantern(t) + 1));
}

long long cap(const VillageState& S, Res r) {
    if (r == Res::Shards) return (1LL << 60);
    long long c = 0;
    for (const auto& b : S.buildings) {
        if (b.level < 1) continue;
        const auto& d = b.def();
        if (b.type == BType::Lantern || d.stores == r) c += d.lv(d.storage, b.level);
    }
    return c;
}

long long addRes(VillageState& S, Res r, long long amount) {
    long long before = S.res(r);
    long long after = std::max(0LL, before + amount);
    if (r != Res::Shards) after = std::min(cap(S, r), after);
    S.setRes(r, after);
    return after - before;
}

Builders builders(const VillageState& S, long long now) {
    int total = 0, busy = 0;
    for (const auto& b : S.buildings) {
        if (b.type == BType::BuilderHut && b.level >= 1) total++;
        if (b.busyUntil > now) busy++;
    }
    return {total, busy, total - busy};
}

int armyHousing(const VillageState& S) {
    int n = 0;
    for (int i = 0; i < kPlayerTroops; i++) n += T((TroopType)i).housing * S.army[i];
    return n;
}
int queueHousing(const VillageState& S) {
    int n = 0;
    for (const auto& q : S.trainQueue) n += T(q.type).housing;
    return n;
}
int campCapacity(const VillageState& S) {
    int n = 0;
    for (const auto& b : S.buildings)
        if (b.type == BType::Camp && b.level >= 1) n += b.def().lv(b.def().capacity, b.level);
    return n;
}
int barracksLevel(const VillageState& S) {
    int m = 0;
    for (const auto& b : S.buildings) if (b.type == BType::Barracks) m = std::max(m, b.level);
    return m;
}
int barracksCount(const VillageState& S) {
    int n = 0;
    for (const auto& b : S.buildings) if (b.type == BType::Barracks && b.level >= 1) n++;
    return n;
}

double collectorAmount(const BuildingRec& b, long long now) {
    const auto& d = b.def();
    if (d.produces == Res::None || b.level < 1) return 0;
    double gained = d.lv(d.rate, b.level) * (double)std::max(0LL, now - b.accAt) / 60000.0;
    return std::min((double)d.lv(d.cap, b.level), b.acc + gained);
}

long long collect(VillageState& S, int id, long long now) {
    BuildingRec* b = get(S, id);
    if (!b || b->def().produces == Res::None) return 0;
    double amount = collectorAmount(*b, now);
    long long got = addRes(S, b->def().produces, (long long)std::floor(amount));
    b->acc = amount - (double)got;
    b->accAt = now;
    if (got >= 50) addXp(S, 1 + got / 1000);
    (b->def().produces == Res::Ore ? S.stats.oreCollected : S.stats.aetherCollected) += got;
    return got;
}

std::vector<LightSource> sources(const VillageState& S, int excludeId) {
    std::vector<LightSource> out;
    for (const auto& b : S.buildings) {
        if (b.id == excludeId || b.level < 1 || !b.def().isLight()) continue;
        float s = (float)b.def().size;
        out.push_back({b.x + s / 2, b.y + s / 2, b.def().lv(b.def().light, b.level), b.id});
    }
    return out;
}

Result canPlace(const VillageState& S, BType t, int x, int y, int ignoreId) {
    const auto& def = B(t);
    int s = def.size;
    if (x < kBorder || y < kBorder || x + s > kGrid - kBorder || y + s > kGrid - kBorder) return Result::no("За краем мира");
    for (const auto& b : S.buildings)
        if (b.id != ignoreId && rectsOverlap(x, y, s, b.x, b.y, b.def().size)) return Result::no("Место занято");
    auto src = sources(S, ignoreId);
    if (t != BType::Lantern && !footprintLit(x, y, s, src)) return Result::no("Здесь тьма — нужен свет");
    if (def.isLight() && ignoreId >= 0) {
        // переносим источник света — остальные постройки не должны остаться во тьме
        VillageState moved = S;
        for (auto& b : moved.buildings) if (b.id == ignoreId) { b.x = x; b.y = y; }
        auto ms = sources(moved);
        for (const auto& b : S.buildings) {
            if (b.id == ignoreId) continue;
            float half = b.def().size / 2.f;
            if (!isLit(b.x + half, b.y + half, ms)) return Result::no("Другие постройки останутся во тьме");
        }
    }
    return Result::yes();
}

std::pair<Res, int> buildCost(const VillageState& S, BType t) {
    if (t == BType::BuilderHut) return {Res::Shards, kHutCost[std::min(countOf(S, t), 4)]};
    return {B(t).costRes, B(t).cost[0]};
}

Result canBuild(const VillageState& S, BType t, long long now) {
    int LL = lanternLevel(S);
    int lim = limit(t, LL);
    if (lim == 0) return Result::no("Нужен Фонарь ур. " + std::to_string(unlockLantern(t)));
    if (countOf(S, t) >= lim) return Result::no("Лимит " + std::to_string(lim) + " — улучши Фонарь");
    auto c = buildCost(S, t);
    if (S.res(c.first) < c.second) return Result::no("Не хватает ресурсов");
    if (B(t).time[0] > 0 && builders(S, now).free < 1) return Result::no("Все строители заняты");
    return Result::yes();
}

Result place(VillageState& S, BType t, int x, int y, long long now, int* placedId) {
    auto can = canBuild(S, t, now);
    if (!can.ok) return can;
    auto pl = canPlace(S, t, x, y);
    if (!pl.ok) return pl;
    auto c = buildCost(S, t);
    S.setRes(c.first, S.res(c.first) - c.second);
    int time = B(t).time[0];
    BuildingRec b = make(S, t, x, y, time > 0 ? 0 : 1, now);
    if (time > 0) b.busyUntil = now + time * 1000LL;
    if (placedId) *placedId = b.id;
    S.buildings.push_back(b);
    S.stats.built++;
    if (time == 0) addXp(S, 1);
    return Result::yes();
}

Result move(VillageState& S, int id, int x, int y) {
    BuildingRec* b = get(S, id);
    if (!b) return Result::no("Нет постройки");
    auto pl = canPlace(S, b->type, x, y, id);
    if (!pl.ok) return pl;
    b->x = x;
    b->y = y;
    return Result::yes();
}

UpgradeInfo upgradeInfo(const VillageState& S, int id, long long now) {
    UpgradeInfo info;
    const BuildingRec* b = nullptr;
    for (const auto& x : S.buildings) if (x.id == id) b = &x;
    if (!b) { info.reason = "Нет постройки"; return info; }
    const auto& d = b->def();
    int next = b->level + 1;
    if (b->busyUntil > now) { info.reason = "Уже строится"; return info; }
    if (next > d.maxLevel) { info.reason = "Максимальный уровень"; info.max = true; return info; }
    if (next > maxLevelAllowed(b->type, lanternLevel(S))) { info.reason = "Нужен Фонарь выше"; return info; }
    info.cost = d.cost[next - 1];
    info.time = d.time[next - 1];
    info.res = d.costRes;
    info.next = next;
    if (S.res(info.res) < info.cost) { info.reason = "Не хватает ресурсов"; return info; }
    if (info.time > 0 && builders(S, now).free < 1) { info.reason = "Все строители заняты"; return info; }
    info.ok = true;
    return info;
}

Result startUpgrade(VillageState& S, int id, long long now) {
    auto info = upgradeInfo(S, id, now);
    if (!info.ok) return Result::no(info.reason);
    BuildingRec* b = get(S, id);
    S.setRes(info.res, S.res(info.res) - info.cost);
    if (info.time > 0) b->busyUntil = now + info.time * 1000LL;
    else { finishBuild(*b, now); addXp(S, 1); S.stats.upgrades++; }
    return Result::yes();
}

int speedUpCost(const BuildingRec& b, long long now) {
    return std::max(1, (int)std::ceil(std::max(0LL, b.busyUntil - now) / 60000.0));
}

Result speedUp(VillageState& S, int id, long long now) {
    BuildingRec* b = get(S, id);
    if (!b || b->busyUntil <= now) return Result::no("Нечего ускорять");
    int cost = speedUpCost(*b, now);
    if (S.shards < cost) return Result::no("Не хватает осколков");
    S.shards -= cost;
    b->busyUntil = now;
    tick(S, now);
    return Result::yes();
}

Result canTrain(const VillageState& S, TroopType t) {
    const auto& d = T(t);
    if (barracksLevel(S) < d.unlock) return Result::no("Нужна казарма ур. " + std::to_string(d.unlock));
    if (barracksCount(S) < 1) return Result::no("Нет готовой казармы");
    if (armyHousing(S) + queueHousing(S) + d.housing > campCapacity(S)) return Result::no("Лагеря полны");
    if (S.aether < d.cost) return Result::no("Не хватает Эфира");
    return Result::yes();
}

Result train(VillageState& S, TroopType t, long long now) {
    auto c = canTrain(S, t);
    if (!c.ok) return c;
    if (S.trainQueue.empty()) S.trainTickAt = now;
    S.aether -= T(t).cost;
    S.trainQueue.push_back({t, (float)T(t).time});
    return Result::yes();
}

void cancelTrain(VillageState& S, int index) {
    if (index < 0 || index >= (int)S.trainQueue.size()) return;
    TroopType t = S.trainQueue[index].type;
    S.trainQueue.erase(S.trainQueue.begin() + index);
    addRes(S, Res::Aether, T(t).cost);
}

long long xpToNext(int level) { return 30 + 25LL * (level - 1) + 4LL * (level - 1) * (level - 1); }
int levelReward(int level) { return 2 + level / 3; }

int spellUnlockLevel(Spell s) { return s == Spell::Heal ? 3 : 5; }

std::map<Spell, int> spellCharges(int level) {
    std::map<Spell, int> m;
    if (level >= spellUnlockLevel(Spell::Heal)) m[Spell::Heal] = level >= 10 ? 2 : 1;
    if (level >= spellUnlockLevel(Spell::Rage)) m[Spell::Rage] = level >= 12 ? 2 : 1;
    return m;
}

int addXp(VillageState& S, long long amount) {
    if (amount <= 0) return 0;
    S.xp += amount;
    S.lastXpGain = amount;
    int ups = 0;
    while (S.xp >= xpToNext(S.level)) {
        S.xp -= xpToNext(S.level);
        S.level++;
        S.shards += levelReward(S.level);
        ups++;
    }
    S.levelUps += ups;
    return ups;
}

float troopMult(int level) { return 1.f + 0.12f * (std::max(1, level) - 1); }
int troopMaxLevel(const VillageState& S) {
    int w = 0;
    for (const auto& b : S.buildings) if (b.type == BType::Workshop) w = std::max(w, b.level);
    return w > 0 ? std::min(kMaxTroopLevel, w + 1) : 1;
}
int researchCost(TroopType t, int toLevel) { return T(t).cost * 30 * (toLevel - 1) * (toLevel - 1) + 500; }
int researchTime(TroopType t, int toLevel) { return 120 * (toLevel - 1) * (toLevel - 1); }

Result canResearch(const VillageState& S, TroopType t, long long now) {
    int i = (int)t;
    if (troopMaxLevel(S) <= 1) return Result::no("Построй Кузницу");
    if (S.researchType >= 0 && S.researchUntil > now) return Result::no("Кузница занята");
    if (barracksLevel(S) < T(t).unlock) return Result::no("Отряд ещё не открыт");
    int next = S.troopLevel[i] + 1;
    if (next > troopMaxLevel(S)) return Result::no(S.troopLevel[i] >= kMaxTroopLevel ? "Максимальный уровень" : "Улучши Кузницу");
    if (S.aether < researchCost(t, next)) return Result::no("Не хватает Эфира");
    return Result::yes();
}

Result startResearch(VillageState& S, TroopType t, long long now) {
    auto c = canResearch(S, t, now);
    if (!c.ok) return c;
    int next = S.troopLevel[(int)t] + 1;
    S.aether -= researchCost(t, next);
    S.researchType = (int)t;
    S.researchUntil = now + researchTime(t, next) * 1000LL;
    return Result::yes();
}

bool heroUnlocked(const VillageState& S) { return lanternLevel(S) >= kHeroUnlockLantern; }
int heroMaxLevel(const VillageState& S) { return heroUnlocked(S) ? std::min(kHeroMaxLevel, lanternLevel(S) * 2 - 1) : 0; }
int heroUpgradeCost(int toLevel) { return 1500 * (toLevel - 1) * (toLevel - 1) + 1000 * (toLevel - 1); }
float heroMult(int level) { return 1.f + 0.1f * (std::max(1, level) - 1); }

Result upgradeHero(VillageState& S) {
    if (!heroUnlocked(S)) return Result::no("Герой откроется на Фонаре " + std::to_string(kHeroUnlockLantern));
    int next = S.heroLevel + 1;
    if (next > heroMaxLevel(S)) return Result::no(S.heroLevel >= kHeroMaxLevel ? "Максимальный уровень" : "Улучши Фонарь");
    if (S.aether < heroUpgradeCost(next)) return Result::no("Не хватает Эфира");
    S.aether -= heroUpgradeCost(next);
    S.heroLevel = next;
    addXp(S, 10 + next * 3);
    return Result::yes();
}

namespace {
const League kLeagues[] = {
    {"Медь", 0, 0.00f, 0xff5a8ad0u}, {"Бронза", 100, 0.05f, 0xff3a7ac8u}, {"Серебро", 300, 0.10f, 0xffd8d0c8u},
    {"Золото", 600, 0.15f, 0xff40c8ffu}, {"Платина", 1000, 0.20f, 0xffe8e0a0u}, {"Алмаз", 1600, 0.30f, 0xffffe080u},
};
}
int leagueIndex(int rating) {
    int i = 0;
    for (int k = 0; k < 6; k++) if (rating >= kLeagues[k].minRating) i = k;
    return i;
}
const League& league(int rating) { return kLeagues[leagueIndex(rating)]; }
int ratingDelta(bool win, int stars) { return win ? 8 + stars * 6 : -6; }

const std::vector<TaskDef>& taskDefs() {
    static const std::vector<TaskDef> v = {
        {"build", "Строитель", "Построй новых построек: %s", Metric::Built, {5, 20, 60}, {3, 6, 12}, {10, 30, 80}},
        {"upgrade", "Мастер на все руки", "Заверши улучшений: %s", Metric::Upgrades, {3, 15, 50}, {3, 6, 12}, {10, 30, 80}},
        {"ore", "Рудокоп", "Собери руды из шахт: %s", Metric::Ore, {2000, 25000, 250000}, {2, 5, 10}, {10, 25, 60}},
        {"aether", "Собиратель Эфира", "Собери Эфира из колодцев: %s", Metric::Aether, {2000, 25000, 250000}, {2, 5, 10}, {10, 25, 60}},
        {"beacons", "Гаситель", "Погаси вражеских Светочей: %s", Metric::Beacons, {3, 20, 100}, {3, 8, 20}, {15, 40, 100}},
        {"stars", "Звёздный налёт", "Получи звёзд в боях: %s", Metric::Stars, {5, 30, 150}, {3, 8, 20}, {15, 40, 100}},
        {"nights", "Страж ночи", "Переживи ночей Мглы: %s", Metric::Nights, {1, 10, 50}, {3, 8, 20}, {15, 40, 100}},
        {"trained", "Наставник", "Обучи войск: %s", Metric::Trained, {20, 200, 1000}, {2, 5, 10}, {10, 25, 60}},
        {"lantern", "Ярче Мглы", "Улучши Фонарь до уровня %s", Metric::Lantern, {2, 4, 6}, {5, 12, 30}, {25, 60, 150}},
    };
    return v;
}

long long taskProgress(const VillageState& S, const TaskDef& t) {
    switch (t.metric) {
        case Metric::Built: return S.stats.built;
        case Metric::Upgrades: return S.stats.upgrades;
        case Metric::Ore: return S.stats.oreCollected;
        case Metric::Aether: return S.stats.aetherCollected;
        case Metric::Beacons: return S.stats.beaconsDestroyed;
        case Metric::Stars: return S.stats.stars;
        case Metric::Nights: return S.stats.nightsWon;
        case Metric::Trained: return S.stats.trained;
        case Metric::Lantern: return lanternLevel(S);
    }
    return 0;
}

int taskClaimed(const VillageState& S, const TaskDef& t) {
    auto it = S.tasks.find(t.id);
    return it == S.tasks.end() ? 0 : it->second;
}

bool taskReady(const VillageState& S, const TaskDef& t) {
    int c = taskClaimed(S, t);
    return c < 3 && taskProgress(S, t) >= t.goal[c];
}

int tasksReady(const VillageState& S) {
    int n = 0;
    for (const auto& t : taskDefs()) n += taskReady(S, t);
    return n;
}

bool claimTask(VillageState& S, const TaskDef& t) {
    if (!taskReady(S, t)) return false;
    int c = taskClaimed(S, t);
    S.shards += t.shards[c];
    S.tasks[t.id] = c + 1;
    addXp(S, t.xp[c]);
    return true;
}

void tick(VillageState& S, long long now) {
    for (auto& b : S.buildings)
        if (b.busyUntil != 0 && b.busyUntil <= now) {
            long long xp = buildXp(b);
            if (b.level >= 1) S.stats.upgrades++;
            finishBuild(b, std::min(now, b.busyUntil));
            addXp(S, xp);
        }
    if (S.researchType >= 0 && S.researchUntil <= now) {
        S.troopLevel[S.researchType] = std::min(kMaxTroopLevel, S.troopLevel[S.researchType] + 1);
        S.researchType = -1;
        S.researchUntil = 0;
        S.stats.upgrades++;
        addXp(S, 15);
    }
    int nb = barracksCount(S);
    if (!S.trainQueue.empty() && nb > 0) {
        double budget = (now - S.trainTickAt) / 1000.0 * nb;
        while (!S.trainQueue.empty() && budget > 0) {
            auto& q = S.trainQueue.front();
            double use = std::min((double)q.left, budget);
            q.left -= (float)use;
            budget -= use;
            if (q.left <= 1e-4f) {
                S.army[(int)q.type]++;
                S.stats.trained++;
                S.trainQueue.erase(S.trainQueue.begin());
            }
        }
    }
    S.trainTickAt = now;
}

bool nightDue(const VillageState& S, long long now) { return now >= S.nightAt; }
void scheduleNextNight(VillageState& S, long long now) { S.nightAt = now + kNightIntervalMs; }

std::vector<LayoutItem> layout(const VillageState& S, float lootFrac) {
    long long capOre = cap(S, Res::Ore), capAe = cap(S, Res::Aether);
    std::vector<LayoutItem> out;
    for (const auto& b : S.buildings) {
        if (b.level < 1) continue;
        LayoutItem it{b.type, b.x, b.y, b.level, (int)out.size()};
        const auto& d = b.def();
        if (lootFrac > 0) {
            float s = (float)d.lv(d.storage, b.level);
            if (d.stores == Res::Ore || b.type == BType::Lantern) it.lootOre = (float)(S.ore * lootFrac * s / std::max(1LL, capOre));
            if (d.stores == Res::Aether || b.type == BType::Lantern) it.lootAether = (float)(S.aether * lootFrac * s / std::max(1LL, capAe));
        }
        out.push_back(it);
    }
    return out;
}

// --- Сохранение: простой построчный формат, устойчивый к новым полям ---
std::string save(const VillageState& S) {
    std::ostringstream o;
    o << "version " << VillageState::kVersion << "\n";
    o << "created " << S.created << "\n";
    o << "res " << S.ore << " " << S.aether << " " << S.shards << "\n";
    o << "nextId " << S.nextId << "\n";
    o << "trainTickAt " << S.trainTickAt << "\n";
    o << "night " << S.nightAt << " " << S.nights << "\n";
    o << "stats " << S.stats.attacks << " " << S.stats.wins << " " << S.stats.nightsWon << " " << S.stats.nightsLost << "\n";
    o << "started " << (S.started ? 1 : 0) << "\n";
    o << "xp " << S.level << " " << S.xp << "\n";
    o << "rating " << S.rating << "\n";
    o << "hero " << S.heroLevel << "\n";
    o << "troops";
    for (int n : S.troopLevel) o << " " << n;
    o << " " << S.researchType << " " << S.researchUntil << "\n";
    o << "stats2 " << S.stats.built << " " << S.stats.upgrades << " " << S.stats.oreCollected << " " << S.stats.aetherCollected << " "
      << S.stats.beaconsDestroyed << " " << S.stats.stars << " " << S.stats.trained << "\n";
    for (const auto& [id, tier] : S.tasks) o << "task " << id << " " << tier << "\n";
    o << "army";
    for (int n : S.army) o << " " << n;
    o << "\n";
    for (const auto& b : S.buildings)
        o << "b " << b.id << " " << (int)b.type << " " << b.x << " " << b.y << " " << b.level << " " << b.busyUntil << " " << b.acc << " " << b.accAt << "\n";
    for (const auto& q : S.trainQueue) o << "q " << (int)q.type << " " << q.left << "\n";
    for (const auto& [id, st] : S.campaign) o << "mission " << id << " " << st << "\n";
    for (const auto& c : S.codex) o << "codex " << c << "\n";
    return o.str();
}

bool load(const std::string& text, VillageState& out) {
    VillageState S;
    S.buildings.clear();
    S.codex.clear();
    std::istringstream in(text);
    std::string line;
    bool versionOk = false;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string k;
        l >> k;
        if (k == "version") { int v; l >> v; versionOk = v == VillageState::kVersion; }
        else if (k == "created") l >> S.created;
        else if (k == "res") l >> S.ore >> S.aether >> S.shards;
        else if (k == "nextId") l >> S.nextId;
        else if (k == "trainTickAt") l >> S.trainTickAt;
        else if (k == "night") l >> S.nightAt >> S.nights;
        else if (k == "stats") l >> S.stats.attacks >> S.stats.wins >> S.stats.nightsWon >> S.stats.nightsLost;
        else if (k == "started") { int v; l >> v; S.started = v != 0; }
        else if (k == "xp") l >> S.level >> S.xp;
        else if (k == "hero") l >> S.heroLevel;
        else if (k == "rating") l >> S.rating;
        else if (k == "troops") { for (auto& n : S.troopLevel) l >> n; l >> S.researchType >> S.researchUntil; }
        else if (k == "stats2") l >> S.stats.built >> S.stats.upgrades >> S.stats.oreCollected >> S.stats.aetherCollected >> S.stats.beaconsDestroyed >> S.stats.stars >> S.stats.trained;
        else if (k == "task") { std::string id; int tier; l >> id >> tier; S.tasks[id] = tier; }
        else if (k == "army") for (auto& n : S.army) l >> n;
        else if (k == "b") {
            BuildingRec b;
            int t;
            l >> b.id >> t >> b.x >> b.y >> b.level >> b.busyUntil >> b.acc >> b.accAt;
            if (t < 0 || t >= kBTypes) return false;
            b.type = (BType)t;
            S.buildings.push_back(b);
        } else if (k == "q") {
            int t; float left;
            l >> t >> left;
            if (t >= 0 && t < kPlayerTroops) S.trainQueue.push_back({(TroopType)t, left});
        } else if (k == "mission") { std::string id; int st; l >> id >> st; S.campaign[id] = st; }
        else if (k == "codex") { std::string c; l >> c; S.codex.push_back(c); }
    }
    if (!versionOk || S.buildings.empty()) return false;
    out = std::move(S);
    return true;
}

}  // namespace lf::village
