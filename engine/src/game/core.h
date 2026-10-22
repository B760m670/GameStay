#pragma once
// Игровое ядро Lanternfall: конфигурация, деревня, детерминированный бой, генерация баз, кампания.
// Не зависит от движка — тестируется отдельно и пригодно для будущего сервера.
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace lf {

constexpr int kGrid = 44;
constexpr int kBorder = 2;
constexpr int kMaxLantern = 6;
constexpr float kBattleTime = 180.f;
constexpr float kNightTime = 90.f;
constexpr long long kNightIntervalMs = 20LL * 60 * 1000;
constexpr long long kFirstNightMs = 8LL * 60 * 1000;
constexpr float kEclipseRadius = 3.5f;
constexpr float kEclipseDuration = 10.f;
// Заклинания атакующего: зона на земле на несколько секунд.
enum class Spell { Heal, Rage };
constexpr float kSpellRadius = 3.f;
constexpr float kSpellDuration = 8.f;
constexpr float kHealPerSec = 0.12f;  // доля здоровья в секунду
constexpr float kRageMult = 1.6f;     // скорость и темп атак
// Герой (Фонарщик): один на бой, способность «Вспышка Фонаря» — ослепляет оборону и подлечивает отряды вокруг.
constexpr int kHeroUnlockLantern = 2;
constexpr int kHeroMaxLevel = 10;
constexpr float kHeroFlashRadius = 5.f;
constexpr float kHeroStun = 5.f;
constexpr float kHeroFlashHeal = 0.35f;

enum class BType { Lantern, Beacon, OreMine, AetherWell, OreStore, AetherStore, Barracks, Camp, BuilderHut, Ballista, Brazier, Catapult, Prism, Wall, Workshop, Trap, Count };
enum class Res { Ore, Aether, Shards, None };
enum class TroopType { Torch, Archer, Shield, Sapper, Shade, Owl, Morok, Brute, Leech, Wraith, Hero, Count };
enum class Target { Any, Defense, Light, Wall };
enum class Proj { None, Bolt, Flame, Rock, Arrow, Feather, Wisp };
enum class Category { Core, Light, Economy, Army, Defense, Wall };

constexpr int kBTypes = (int)BType::Count;
constexpr int kTroopTypes = (int)TroopType::Count;
constexpr int kPlayerTroops = 6;  // Torch..Owl

struct BuildingDef {
    BType type;
    const char* id;
    const char* name;
    const char* desc;
    int size, maxLevel;
    Category category;
    Res costRes;
    std::vector<int> cost, time, hp, storage, rate, cap, capacity, dmg;
    std::vector<float> light, dps;
    Res produces = Res::None, stores = Res::None;
    float range = 0, minRange = 0, cd = 0, splash = 0, rampMax = 0, rampRate = 0, projSpeed = 0;
    bool ground = false, air = false, beam = false;
    bool trap = false;  // скрытая ловушка: срабатывает один раз, не считается в разрушении
    Proj proj = Proj::None;

    bool isLight() const { return !light.empty(); }
    bool isDefense() const { return category == Category::Defense; }
    template <class T> T lv(const std::vector<T>& a, int level) const {
        if (a.empty()) return T{};
        int i = level - 1;
        if (i < 0) i = 0;
        if (i >= (int)a.size()) i = (int)a.size() - 1;
        return a[i];
    }
};

struct TroopDef {
    TroopType type;
    const char* id;
    const char* name;
    const char* desc;
    uint32_t color;  // RGBA
    bool haze = false;
    int housing = 1, cost = 0, time = 0, unlock = 1;
    float hp = 1, dmg = 0, cd = 1, range = 0.6f, speed = 1, splash = 0, lightMult = 1;
    Target target = Target::Any;
    Proj ranged = Proj::None;
    bool air = false, suicide = false, stealth = false;
};

const BuildingDef& B(BType t);
const TroopDef& T(TroopType t);
int limit(BType t, int lantern);
int unlockLantern(BType t);
int eclipseCharges(int lantern);
extern const int kRaidSearchCost[6];
extern const int kBaseLoot[6];
extern const int kHutCost[5];

// ---------- Утилиты ----------
class Rng {
public:
    explicit Rng(int32_t seed) : a_((uint32_t)seed) {}
    double next();
    float nextf() { return (float)next(); }
    int range(int n) { return (int)(next() * n); }
private:
    uint32_t a_;
};

struct LightSource { float x, y, r; int id; };
struct Eclipse { float x, y, r, until; };
struct SpellZone { Spell kind; float x, y, r, until; };

float distToRect(float px, float py, float x, float y, float size);
bool rectsOverlap(int ax, int ay, int as, int bx, int by, int bs);
bool isLit(float px, float py, const std::vector<LightSource>& src, const std::vector<Eclipse>* ecl = nullptr);
std::vector<uint8_t> litGrid(const std::vector<LightSource>& src, const std::vector<Eclipse>* ecl = nullptr);
bool footprintLit(int x, int y, int size, const std::vector<LightSource>& src);

// ---------- Деревня ----------
struct BuildingRec {
    int id = 0;
    BType type = BType::Lantern;
    int x = 0, y = 0, level = 0;
    long long busyUntil = 0;
    double acc = 0;
    long long accAt = 0;
    const BuildingDef& def() const { return B(type); }
};

struct TrainItem { TroopType type; float left; };

struct Stats {
    int attacks = 0, wins = 0, nightsWon = 0, nightsLost = 0;
    long long built = 0, upgrades = 0, oreCollected = 0, aetherCollected = 0, beaconsDestroyed = 0, stars = 0, trained = 0;
};

// Задания: три ступени, награда за каждую.
enum class Metric { Built, Upgrades, Ore, Aether, Beacons, Stars, Nights, Trained, Lantern };
struct TaskDef {
    const char* id;
    const char* title;
    const char* desc;   // %s — цель
    Metric metric;
    long long goal[3];
    int shards[3], xp[3];
};

struct VillageState {
    static constexpr int kVersion = 1;
    long long created = 0;
    long long ore = 0, aether = 0, shards = 0;
    std::vector<BuildingRec> buildings;
    int nextId = 1;
    std::array<int, kPlayerTroops> army{};
    std::vector<TrainItem> trainQueue;
    long long trainTickAt = 0;
    std::map<std::string, int> campaign;
    std::vector<std::string> codex{"intro"};
    long long nightAt = 0;
    int nights = 0;
    Stats stats;
    bool started = false;
    std::map<std::string, int> tasks;  // id -> сколько ступеней уже забрано
    int rating = 0;        // рейтинг набегов
    std::array<int, kPlayerTroops> troopLevel{1, 1, 1, 1, 1, 1};
    int heroLevel = 1;
    int researchType = -1;  // какой отряд сейчас улучшается в Кузнице
    long long researchUntil = 0;
    long long xp = 0;      // опыт внутри текущего уровня
    int level = 1;         // уровень игрока
    int levelUps = 0;      // не сохраняется: сколько повышений клиент ещё не показал
    long long lastXpGain = 0;

    long long res(Res r) const { return r == Res::Ore ? ore : r == Res::Aether ? aether : shards; }
    void setRes(Res r, long long v) { (r == Res::Ore ? ore : r == Res::Aether ? aether : shards) = v; }
};

struct Result {
    bool ok = true;
    std::string reason;
    static Result yes() { return {}; }
    static Result no(std::string r) { return {false, std::move(r)}; }
};

struct UpgradeInfo {
    bool ok = false, max = false;
    std::string reason;
    int cost = 0, time = 0, next = 0;
    Res res = Res::Ore;
};

struct LayoutItem {
    BType type;
    int x, y, level;
    int id = 0;
    float lootOre = 0, lootAether = 0;
};

namespace village {
VillageState create(long long now);
BuildingRec* get(VillageState& S, int id);
int lanternLevel(const VillageState& S);
int countOf(const VillageState& S, BType t);
int maxLevelAllowed(BType t, int lantern);
long long cap(const VillageState& S, Res r);
long long addRes(VillageState& S, Res r, long long amount);
struct Builders { int total, busy, free; };
Builders builders(const VillageState& S, long long now);
int armyHousing(const VillageState& S);
int queueHousing(const VillageState& S);
int campCapacity(const VillageState& S);
int barracksLevel(const VillageState& S);
int barracksCount(const VillageState& S);
double collectorAmount(const BuildingRec& b, long long now);
long long collect(VillageState& S, int id, long long now);
std::vector<LightSource> sources(const VillageState& S, int excludeId = -1);
Result canPlace(const VillageState& S, BType t, int x, int y, int ignoreId = -1);
std::pair<Res, int> buildCost(const VillageState& S, BType t);
Result canBuild(const VillageState& S, BType t, long long now);
Result place(VillageState& S, BType t, int x, int y, long long now, int* placedId = nullptr);
Result move(VillageState& S, int id, int x, int y);
UpgradeInfo upgradeInfo(const VillageState& S, int id, long long now);
Result startUpgrade(VillageState& S, int id, long long now);
int speedUpCost(const BuildingRec& b, long long now);
Result speedUp(VillageState& S, int id, long long now);
Result canTrain(const VillageState& S, TroopType t);
Result train(VillageState& S, TroopType t, long long now);
void cancelTrain(VillageState& S, int index);
void tick(VillageState& S, long long now);
bool nightDue(const VillageState& S, long long now);
// Опыт: сколько нужно до следующего уровня, начислить (с наградой за уровни).
long long xpToNext(int level);
int addXp(VillageState& S, long long amount);
int levelReward(int level);
// Заряды заклинаний в бою по уровню игрока: Исцеление с 3-го, Ярость с 5-го, второй заряд позже.
std::map<Spell, int> spellCharges(int level);
int spellUnlockLevel(Spell s);
// Кузница: улучшение отрядов.
constexpr int kMaxTroopLevel = 5;
float troopMult(int level);  // множитель здоровья и урона
int troopMaxLevel(const VillageState& S);
int researchCost(TroopType t, int toLevel);
int researchTime(TroopType t, int toLevel);
Result canResearch(const VillageState& S, TroopType t, long long now);
Result startResearch(VillageState& S, TroopType t, long long now);
// Герой.
bool heroUnlocked(const VillageState& S);
int heroMaxLevel(const VillageState& S);  // ограничен уровнем Фонаря
int heroUpgradeCost(int toLevel);         // Эфир
float heroMult(int level);
Result upgradeHero(VillageState& S);
// Рейтинг и лиги.
struct League { const char* name; int minRating; float lootBonus; uint32_t color; };
const League& league(int rating);
int leagueIndex(int rating);
int ratingDelta(bool win, int stars);
const std::vector<TaskDef>& taskDefs();
long long taskProgress(const VillageState& S, const TaskDef& t);
int taskClaimed(const VillageState& S, const TaskDef& t);
bool taskReady(const VillageState& S, const TaskDef& t);   // текущая ступень выполнена и не забрана
int tasksReady(const VillageState& S);
bool claimTask(VillageState& S, const TaskDef& t);
void scheduleNextNight(VillageState& S, long long now);
std::vector<LayoutItem> layout(const VillageState& S, float lootFrac = 0.f);
std::string save(const VillageState& S);
bool load(const std::string& text, VillageState& out);
}  // namespace village

// ---------- Бой ----------
enum class BattleMode { Attack, Defense };

struct BTroop;
struct BBuilding {
    int id;
    BType type;
    int x, y, level, size;
    const BuildingDef* def;
    float hp, maxHp, cx, cy, lightR, cd = 0, beamT = 0, hitT = 0, angle = 2.35f, stunT = 0;
    float lootOre = 0, lootAether = 0;
    bool dead = false, powered = true;
    int target = -1;  // индекс войска
    bool isDefense() const { return def->isDefense(); }
    bool isWall() const { return type == BType::Wall; }
    bool isLight() const { return def->isLight(); }
    bool isTrap() const { return def->trap; }
    bool isStructure() const { return !isWall() && !isTrap(); }  // то, что считается в проценте разрушения
};

struct BTroop {
    int id;
    TroopType type;
    const TroopDef* def;
    float x, y, hp, maxHp, dmgMult = 1, cd = 0, jx, jy, angle = 0, hitT = 0, bob, animT = 0;
    bool air, dead = false, attacking = false, hidden = false;
    int target = -1, wall = -1;  // индексы построек
    std::vector<int> path;
    bool hasPath = false;
    int pi = 0;
};

struct Projectile {
    Proj kind;
    float sx, sy, x, y, tx, ty, t = 0, dur, dmg, splash = 0, k = 0;
    int troop = -1, building = -1, attacker = -1;
    const BuildingDef* src = nullptr;
    bool fromAir = false, done = false;
};

enum class Fx { Boom, Blast, Hit, Poof, Flame, EclipseFx, Rift, Flash };
struct Effect { Fx kind; float x, y, sx = 0, sy = 0, life, max, r = 0; bool air = false; uint32_t color = 0; };

enum class Ev { LightsOut, Lantern, Wave, EclipseCast, Destroyed, Shot, Hit, Deploy, SpellCast, Trap, HeroFlash };
struct BattleEvent { Ev kind; float x, y; BType type; };

struct Wave { float t; std::map<TroopType, int> units; bool done = false; };

struct BattleResult {
    int stars = 0, pct = 0;
    long long lootOre = 0, lootAether = 0;
    bool win = false, lanternDead = false;
    std::map<TroopType, int> used;
};

struct BattleOptions {
    BattleMode mode = BattleMode::Attack;
    std::vector<LayoutItem> layout;
    std::map<TroopType, int> army;
    std::map<TroopType, int> troopLevels;  // уровни отрядов атакующего (1 по умолчанию)
    int eclipseCharges = 0;
    std::map<Spell, int> spells;  // заряды заклинаний атакующего
    int heroLevel = 0;            // 0 — героя нет
    int seed = 1;
    float duration = kBattleTime;
    std::vector<Wave> waves;
    float hazeMult = 1.f;
};

class Battle {
public:
    explicit Battle(const BattleOptions& o);
    void step(float dt);
    bool canDeploy(float x, float y) const;
    bool deploy(TroopType t, float x, float y);
    bool castEclipse(float x, float y);
    bool castSpell(Spell s, float x, float y);
    bool deployHero(float x, float y);
    bool heroAbility();
    void finish();
    BattleResult simulate(float maxTime = 400.f, float dt = 1.f / 30.f);
    bool litAt(float x, float y) const;
    int aliveTroops() const;
    int poolLeft() const;
    int percent() const;
    void relight();

    BattleMode mode;
    float time = 0, duration;
    Rng rng;
    std::vector<BBuilding> buildings;
    std::vector<BTroop> troops;
    std::vector<Projectile> projectiles;
    std::vector<Effect> effects;
    std::vector<Eclipse> eclipses;
    std::vector<SpellZone> zones;
    std::map<Spell, int> spells;
    int heroLevel = 0, heroIdx = -1;  // индекс героя в troops после высадки
    bool heroAbilityUsed = false;
    std::vector<BattleEvent> events;
    int eclipseChargesLeft;
    std::map<TroopType, int> pool, used;
    float lootOre = 0, lootAether = 0, lootTotalOre = 0, lootTotalAether = 0;
    std::vector<int> tileOwner;
    std::vector<uint8_t> lit;
    std::vector<LightSource> sources;
    int destroyed = 0, total = 0;
    bool lanternDead = false, ended = false;
    BattleResult result;
    std::vector<Wave> waves;
    float hazeMult;
    std::map<TroopType, int> troopLevels;

private:
    float lastDeployAt = -99.f;
    int nextId = 1;
    int spawn(TroopType t, float x, float y);
    int chooseTarget(const BTroop& t) const;
    float cellCost(int i) const;
    void damageBuilding(int bi, float dmg, int attacker);
    void destroyBuilding(int bi);
    void damageTroop(int ti, float dmg);
    void troopAttack(int ti, int bi);
    void updateTroop(int ti, float dt);
    bool canHit(const BBuilding& b, const BTroop& t) const;
    void updateDefense(int bi, float dt);
    void updateTrap(int bi);
    void splashTroops(float x, float y, float r, float dmg, const BuildingDef* src);
    void updateProjectiles(float dt);
    void spawnWave(Wave& w);
    void checkEnd();
};

std::vector<int> findPath(int sx, int sy, int gx, int gy, int gs, const std::vector<float>& cost, int maxIter = 6000);

// ---------- Генерация ----------
namespace basegen {
std::string rivalName(Rng& rng);
std::vector<LayoutItem> generate(int LL, int seed, const std::map<BType, int>* counts = nullptr, float richness = 1.f, int levelBias = 0);
struct Night { std::vector<Wave> waves; float hazeMult; };
Night nightWaves(int LL, int nights, int seed);
}

// ---------- Запись боя (продолжение после закрытия приложения) ----------
namespace replay {
enum class Act { Deploy, Eclipse, Spell, Hero, HeroAbility };
struct Action { int step = 0; Act kind = Act::Deploy; int arg = 0; float x = 0, y = 0; };
struct Record { BattleOptions opts; std::vector<Action> actions; int steps = 0; };
bool apply(Battle& B, const Action& a);
// Повторить действия и шаги записи на свежем бою, созданном из r.opts.
void fastForward(Battle& B, const Record& r, float dt);
std::string save(const Record& r);
bool load(const std::string& s, Record& r);
}

// ---------- Кампания ----------
enum class Speaker { Narrator, Vira, Keeper, Haze, King };
struct Line { Speaker who; const char* text; };
struct Mission {
    const char* id;
    const char* title;
    int LL, seed;
    float richness;
    int levelBias;
    const char* codex;
    int rewardOre, rewardAether, rewardShards;
    std::map<BType, int> counts;
    std::vector<Line> intro, outro;
};
struct CodexEntry { const char* title; const char* text; };
struct SpeakerInfo { const char* name; uint32_t color; };

namespace campaign {
const std::vector<Mission>& missions();
const std::vector<Line>& intro();
const std::map<std::string, CodexEntry>& codex();
SpeakerInfo speaker(Speaker s);
bool unlocked(const VillageState& S, int idx);
}

}  // namespace lf
