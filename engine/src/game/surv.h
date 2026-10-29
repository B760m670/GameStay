#pragma once
// «Острог: 99 ночей тайги» — симуляция кооперативного выживания. Детерминированная, с фиксированным тактом,
// без отрисовки: её ведёт клиент (одиночная игра) или сервер (игра с друзьями), клиенты присылают ввод.
//
// Мир — поляна тайги kMap×kMap метров (x, y — земля). В центре сторожевой огонь. Днём игроки рубят лес и строят
// частокол и факелы, ночью приходят волки, а на третью ночь (и каждую десятую потом) — Лихо, которое боится огня.
#include <cstdint>
#include <string>
#include <vector>

namespace surv {

constexpr float kDt = 0.05f;        // такт — 20 раз в секунду
constexpr int kMap = 72;            // метров по стороне поляны
constexpr int kMaxPlayers = 6;
constexpr float kDayLen = 150.f;    // секунд дня
constexpr float kNightLen = 90.f;   // секунд ночи

enum class Piece : uint8_t { Palisade, Gate, Torch, Count };
enum class EnemyKind : uint8_t { Wolf, Likho };
enum class Role : uint8_t { Carpenter, Hunter, Healer, Warrior, Scout, Smith };

struct PieceDef {
    const char* id;
    const char* name;
    int wood;        // цена в брёвнах
    float hp;
    float light;     // радиус света (0 — не светит)
};
const PieceDef& piece(Piece p);

struct Tree {
    int id;
    float x, y;
    float hp = 4;
    uint8_t kind = 0;   // 0 — ель, 1 — сосна, 2 — берёза
    float regrow = 0;   // >0 — пень, отрастёт через столько секунд
    float shake = 0;    // для отрисовки: недавно рубили
    bool alive() const { return regrow <= 0; }
};

struct Structure {
    int id;
    Piece kind;
    int cx, cy;           // клетка 1×1 м
    float hp, maxHp;
    float hitT = 0;       // для отрисовки: недавно били
};

struct Player {
    int id;
    std::string name;
    Role role = Role::Carpenter;
    float x, y, facing = 0;
    float vx = 0, vy = 0;
    float hp = 10, maxHp = 10;
    int wood = 0;
    float swing = 0;       // >0 — идёт замах (секунды до конца)
    float cd = 0;
    float respawn = 0;     // >0 — погиб, вернётся у огня
    float hurtT = 0;
    bool alive() const { return respawn <= 0; }
};

struct Enemy {
    int id;
    EnemyKind kind;
    float x, y, facing = 0;
    float hp, maxHp;
    float cd = 0, hurtT = 0, attackT = 0;
    float fear = 0;        // Лихо: сколько ещё отступает от огня
    bool alive = true;
    float deadT = 0;
};

struct Input {
    float mx = 0, my = 0;  // желаемое направление движения (в мире), длина ≤ 1
    bool act = false;      // рубить / бить
    bool feed = false;     // подкинуть бревно в огонь
    bool build = false;    // поставить деталь на клетку bx, by
    Piece piece = Piece::Palisade;
    int bx = 0, by = 0;
};

// События такта — для звуков и частиц на клиенте.
enum class Ev : uint8_t { Chop, TreeFell, Hit, PlayerHurt, PlayerDied, EnemyDied, Built, Broken, Fed, NightBegins, DayBegins, LikhoComes, FireOut };
struct Event {
    Ev type;
    float x, y;
    int who = -1;
};

struct World {
    uint32_t rng = 1;
    int tick = 0;
    float time = 0;        // секунды с начала забега
    int day = 1;           // номер дня (ночь n идёт после дня n)
    bool night = false;
    float phaseT = 0;      // сколько прошло в текущей фазе
    float fireX = kMap / 2.f, fireY = kMap / 2.f;
    float fuel = 160;      // запас дров в «секундах ночного горения» (днём горит вдвое медленнее)
    int fireLevel = 1;
    int nextId = 1;
    int spawned = 0, toSpawn = 0;
    float spawnT = 0;
    int nightsSurvived = 0;
    int fed = 0;           // сколько брёвен скормлено огню (растёт уровень)
    bool lost = false;     // все погибли разом, огня нет
    std::vector<Tree> trees;
    std::vector<Structure> structures;
    std::vector<Player> players;
    std::vector<Enemy> enemies;
    std::vector<Event> events;
};

void init(World& w, uint32_t seed);
int addPlayer(World& w, const std::string& name, Role role = Role::Carpenter);
// Один такт. inputs[i] — ввод игрока players[i] (может быть короче — остальные стоят).
void step(World& w, const std::vector<Input>& inputs);

// Справочные функции для клиента и сервера.
float fireRadius(const World& w);          // радиус света огня, 0 — погас
float phaseLeft(const World& w);           // секунд до смены дня и ночи
const Structure* structureAt(const World& w, int cx, int cy);
bool canBuild(const World& w, const Player& p, Piece piece, int cx, int cy, std::string* why = nullptr);
const Tree* treeInReach(const World& w, const Player& p);
float lightAt(const World& w, float x, float y);  // 0..1, насколько светло (огонь и факелы)
const char* roleName(Role r);

}  // namespace surv
