#include "surv.h"

#include <algorithm>
#include <cmath>

namespace surv {
namespace {

const PieceDef kPieces[] = {
    {"palisade", "Частокол", 2, 10.f, 0.f},
    {"gate", "Ворота", 4, 14.f, 0.f},
    {"torch", "Факел", 1, 3.f, 4.5f},
};

struct EnemyDef { float speed, hp, damage, range, cd, radius; };
const EnemyDef kWolf{4.3f, 3.f, 1.f, 1.0f, 1.0f, 0.4f};
const EnemyDef kLikho{2.6f, 28.f, 3.f, 1.5f, 1.6f, 0.75f};
const EnemyDef& edef(EnemyKind k) { return k == EnemyKind::Wolf ? kWolf : kLikho; }

constexpr float kPlayerR = 0.4f, kTreeR = 0.45f, kFirePitR = 0.9f;
constexpr float kMaxFuel = 240.f;
constexpr int kMaxWood = 30;

uint32_t rnd(World& w) {
    uint32_t x = w.rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return w.rng = x ? x : 0x9e3779b9u;
}
float frand(World& w) { return (rnd(w) & 0xffffff) / (float)0x1000000; }

float dist(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }

void emit(World& w, Ev t, float x, float y, int who = -1) { w.events.push_back({t, x, y, who}); }

bool blocksEnemies(const Structure& s) { return s.kind == Piece::Palisade || s.kind == Piece::Gate; }
bool blocksPlayers(const Structure& s) { return s.kind == Piece::Palisade; }

// Окружность (x, y, r) против клетки-бруска: вытолкнуть наружу. Возвращает true, если было пересечение.
bool pushOutOfCell(float& x, float& y, float r, int cx, int cy, float inset = 0.f) {
    float x0 = cx + inset, x1 = cx + 1 - inset, y0 = cy + inset, y1 = cy + 1 - inset;
    float nx = std::clamp(x, x0, x1), ny = std::clamp(y, y0, y1);
    float dx = x - nx, dy = y - ny, d2 = dx * dx + dy * dy;
    if (d2 >= r * r) return false;
    if (d2 < 1e-8f) {  // центр внутри клетки — вытолкнуть к ближайшей стороне
        float l = x - x0, rr = x1 - x, t = y - y0, b = y1 - y;
        float m = std::min(std::min(l, rr), std::min(t, b));
        if (m == l) x = x0 - r; else if (m == rr) x = x1 + r; else if (m == t) y = y0 - r; else y = y1 + r;
        return true;
    }
    float d = std::sqrt(d2);
    x = nx + dx / d * r;
    y = ny + dy / d * r;
    return true;
}

bool pushOutOfCircle(float& x, float& y, float r, float cx, float cy, float cr) {
    float dx = x - cx, dy = y - cy, d = std::hypot(dx, dy), m = r + cr;
    if (d >= m) return false;
    if (d < 1e-5f) { dx = 1; dy = 0; d = 1; }
    x = cx + dx / d * m;
    y = cy + dy / d * m;
    return true;
}

// Передвинуть тело с учётом деревьев, построек, ямы костра и краёв поляны. hitStructure — первая постройка, в которую упёрлись.
void moveBody(World& w, float& x, float& y, float dx, float dy, float r, bool enemy, int* hitStructure) {
    x += dx;
    y += dy;
    for (int pass = 0; pass < 2; pass++) {
        for (const Tree& t : w.trees)
            if (t.alive() && std::fabs(t.x - x) < 2 && std::fabs(t.y - y) < 2) pushOutOfCircle(x, y, r, t.x, t.y, kTreeR);
        for (const Structure& s : w.structures) {
            if (std::abs(s.cx - (int)std::floor(x)) > 2 || std::abs(s.cy - (int)std::floor(y)) > 2) continue;
            bool solid = enemy ? blocksEnemies(s) : blocksPlayers(s);
            if (s.kind == Piece::Torch) {
                if (pushOutOfCircle(x, y, r, s.cx + 0.5f, s.cy + 0.5f, 0.15f) && hitStructure && *hitStructure < 0 && enemy) *hitStructure = s.id;
                continue;
            }
            if (!solid) continue;
            if (pushOutOfCell(x, y, r, s.cx, s.cy, 0.05f) && hitStructure && *hitStructure < 0) *hitStructure = s.id;
        }
        pushOutOfCircle(x, y, r, w.fireX, w.fireY, kFirePitR);
    }
    x = std::clamp(x, 1.f, kMap - 1.f);
    y = std::clamp(y, 1.f, kMap - 1.f);
}

Structure* findStructure(World& w, int id) {
    for (auto& s : w.structures) if (s.id == id) return &s;
    return nullptr;
}

void hurtPlayer(World& w, Player& p, float dmg) {
    if (!p.alive()) return;
    p.hp -= dmg;
    p.hurtT = 0.35f;
    emit(w, Ev::PlayerHurt, p.x, p.y, p.id);
    if (p.hp <= 0) {
        p.hp = 0;
        p.respawn = 10.f;
        p.wood = 0;
        p.swing = 0;
        emit(w, Ev::PlayerDied, p.x, p.y, p.id);
    }
}

void spawnEnemy(World& w, EnemyKind k) {
    // с края поляны, со случайной стороны
    float a = frand(w) * 6.2831853f;
    float r = kMap / 2.f - 2.f;
    Enemy e;
    e.id = w.nextId++;
    e.kind = k;
    e.x = std::clamp(w.fireX + std::cos(a) * r, 1.5f, kMap - 1.5f);
    e.y = std::clamp(w.fireY + std::sin(a) * r, 1.5f, kMap - 1.5f);
    e.hp = e.maxHp = edef(k).hp;
    e.facing = std::atan2(w.fireY - e.y, w.fireX - e.x);
    w.enemies.push_back(e);
}

void beginNight(World& w) {
    w.night = true;
    w.phaseT = 0;
    w.toSpawn = std::min(2 + 2 * (w.day - 1), 16);
    w.spawned = 0;
    w.spawnT = 2.f;
    emit(w, Ev::NightBegins, w.fireX, w.fireY);
    if (w.day == 3 || w.day % 10 == 0) {
        spawnEnemy(w, EnemyKind::Likho);
        emit(w, Ev::LikhoComes, w.enemies.back().x, w.enemies.back().y);
    }
}

void beginDay(World& w) {
    w.night = false;
    w.phaseT = 0;
    w.nightsSurvived++;
    w.day++;
    for (auto& e : w.enemies) if (e.alive) { e.alive = false; e.deadT = 0; }  // с рассветом уходят в лес
    emit(w, Ev::DayBegins, w.fireX, w.fireY);
}

void stepPlayer(World& w, Player& p, const Input& in) {
    if (!p.alive()) {
        p.respawn -= kDt;
        if (p.respawn <= 0) {
            p.respawn = 0;
            p.hp = p.maxHp;
            float a = p.id * 1.7f;
            p.x = w.fireX + std::cos(a) * 2.2f;
            p.y = w.fireY + std::sin(a) * 2.2f;
        }
        return;
    }
    p.cd = std::max(0.f, p.cd - kDt);
    p.swing = std::max(0.f, p.swing - kDt);
    p.hurtT = std::max(0.f, p.hurtT - kDt);
    float mx = in.mx, my = in.my, ml = std::hypot(mx, my);
    if (ml > 1) { mx /= ml; my /= ml; ml = 1; }
    float speed = p.role == Role::Scout ? 6.f : 5.f;
    if (p.swing > 0) speed *= 0.5f;
    p.vx = mx * speed;
    p.vy = my * speed;
    if (ml > 0.1f) p.facing = std::atan2(my, mx);
    moveBody(w, p.x, p.y, p.vx * kDt, p.vy * kDt, kPlayerR, false, nullptr);

    if (in.act && p.cd <= 0) {
        p.swing = 0.35f;
        p.cd = p.role == Role::Carpenter ? 0.45f : 0.55f;
        float fx = std::cos(p.facing), fy = std::sin(p.facing);
        // сначала враг перед собой
        Enemy* best = nullptr;
        float bestD = 2.0f;
        for (auto& e : w.enemies) {
            if (!e.alive) continue;
            float dx = e.x - p.x, dy = e.y - p.y, d = std::hypot(dx, dy) - edef(e.kind).radius;
            if (d < bestD && (dx * fx + dy * fy) > 0.2f * std::hypot(dx, dy)) { bestD = d; best = &e; }
        }
        if (best) {
            float dmg = p.role == Role::Warrior ? 2.f : 1.f;
            best->hp -= dmg;
            best->hurtT = 0.3f;
            emit(w, Ev::Hit, best->x, best->y, p.id);
            if (best->hp <= 0) {
                best->alive = false;
                best->deadT = 0;
                emit(w, Ev::EnemyDied, best->x, best->y, p.id);
            }
        } else if (const Tree* tc = treeInReach(w, p)) {
            Tree& t = const_cast<Tree&>(*tc);
            t.hp -= 1;
            t.shake = 0.3f;
            if (t.hp <= 0) {
                t.regrow = 180.f;
                p.wood = std::min(kMaxWood, p.wood + 3);
                emit(w, Ev::TreeFell, t.x, t.y, p.id);
            } else {
                emit(w, Ev::Chop, t.x, t.y, p.id);
            }
        }
    }
    if (in.feed && p.cd <= 0 && p.wood > 0 && dist(p.x, p.y, w.fireX, w.fireY) < 3.5f) {
        p.wood--;
        p.cd = 0.25f;
        w.fuel = std::min(kMaxFuel, w.fuel + 20.f);
        w.fed++;
        int lvl = w.fed >= 60 ? 4 : w.fed >= 30 ? 3 : w.fed >= 10 ? 2 : 1;
        w.fireLevel = std::max(w.fireLevel, lvl);
        emit(w, Ev::Fed, w.fireX, w.fireY, p.id);
    }
    if (in.build && p.cd <= 0 && canBuild(w, p, in.piece, in.bx, in.by)) {
        const PieceDef& d = piece(in.piece);
        p.wood -= d.wood;
        p.cd = 0.25f;
        Structure s;
        s.id = w.nextId++;
        s.kind = in.piece;
        s.cx = in.bx;
        s.cy = in.by;
        s.hp = s.maxHp = d.hp;
        w.structures.push_back(s);
        emit(w, Ev::Built, in.bx + 0.5f, in.by + 0.5f, p.id);
    }
    // у огня раны затягиваются
    if (p.hp < p.maxHp && dist(p.x, p.y, w.fireX, w.fireY) < fireRadius(w) * 0.8f) {
        float k = p.role == Role::Healer ? 1.2f : 0.5f;
        p.hp = std::min(p.maxHp, p.hp + k * kDt);
    }
}

void stepEnemy(World& w, Enemy& e) {
    if (!e.alive) {
        e.deadT += kDt;
        return;
    }
    const EnemyDef& d = edef(e.kind);
    e.cd = std::max(0.f, e.cd - kDt);
    e.hurtT = std::max(0.f, e.hurtT - kDt);
    e.attackT = std::max(0.f, e.attackT - kDt);
    float light = lightAt(w, e.x, e.y);
    // цель — ближайший живой игрок (Лихо выбирает тех, кто в темноте)
    Player* target = nullptr;
    float best = 1e9f;
    for (auto& p : w.players) {
        if (!p.alive()) continue;
        float dd = dist(p.x, p.y, e.x, e.y);
        if (e.kind == EnemyKind::Likho && lightAt(w, p.x, p.y) > 0.3f) dd += 40;
        if (dd < best) { best = dd; target = &p; }
    }
    float gx = w.fireX, gy = w.fireY;
    if (target) { gx = target->x; gy = target->y; }
    float dx = gx - e.x, dy = gy - e.y, dl = std::hypot(dx, dy);
    float speed = d.speed;
    if (e.kind == EnemyKind::Likho) {
        if (light > 0.35f) { e.fear = 2.5f; e.hp -= 1.f * kDt; e.hurtT = 0.1f; }
        if (e.hp <= 0) { e.alive = false; e.deadT = 0; emit(w, Ev::EnemyDied, e.x, e.y); return; }
        if (e.fear > 0) {  // отступает от огня
            e.fear -= kDt;
            dx = e.x - w.fireX; dy = e.y - w.fireY; dl = std::hypot(dx, dy);
            speed *= 1.2f;
        }
    } else {
        // волки к самому огню не подходят: ходят по кромке света
        float fr = fireRadius(w) * 0.55f, fd = dist(e.x, e.y, w.fireX, w.fireY);
        if (fr > 0 && fd < fr + 0.5f && dl > d.range) {
            float ox = e.x - w.fireX, oy = e.y - w.fireY, ol = std::max(0.01f, fd);
            dx = -oy / ol + ox / ol * 0.6f;  // в обход, по касательной
            dy = ox / ol + oy / ol * 0.6f;
            dl = std::hypot(dx, dy);
        }
    }
    bool inReach = target && dist(target->x, target->y, e.x, e.y) < d.range + kPlayerR + d.radius * 0.5f;
    if (inReach && e.fear <= 0) {
        e.facing = std::atan2(target->y - e.y, target->x - e.x);
        if (e.cd <= 0) {
            e.cd = d.cd;
            e.attackT = 0.3f;
            hurtPlayer(w, *target, d.damage);
        }
        return;
    }
    if (dl > 0.01f) {
        e.facing = std::atan2(dy, dx);
        int hit = -1;
        moveBody(w, e.x, e.y, dx / dl * speed * kDt, dy / dl * speed * kDt, d.radius, true, &hit);
        if (hit >= 0 && e.cd <= 0) {  // упёрся в частокол — ломает
            if (Structure* s = findStructure(w, hit)) {
                e.cd = d.cd;
                e.attackT = 0.3f;
                s->hp -= e.kind == EnemyKind::Likho ? 5.f : 1.f;
                s->hitT = 0.3f;
                emit(w, Ev::Hit, s->cx + 0.5f, s->cy + 0.5f);
            }
        }
    }
}

}  // namespace

const PieceDef& piece(Piece p) { return kPieces[(int)p]; }

const char* roleName(Role r) {
    switch (r) {
        case Role::Carpenter: return "Плотник";
        case Role::Hunter: return "Охотник";
        case Role::Healer: return "Знахарка";
        case Role::Warrior: return "Ратник";
        case Role::Scout: return "Разведчик";
        case Role::Smith: return "Кузнец";
    }
    return "";
}

void init(World& w, uint32_t seed) {
    w = World{};
    w.rng = seed ? seed : 1;
    for (int i = 0; i < 8; i++) rnd(w);
    // лес: плотнее к краям, в центре поляна под острог
    int attempts = 0;
    while ((int)w.trees.size() < 300 && attempts++ < 20000) {
        float x = 2 + frand(w) * (kMap - 4), y = 2 + frand(w) * (kMap - 4);
        float dc = dist(x, y, w.fireX, w.fireY);
        if (dc < 10.f) continue;
        if (dc < 16.f && frand(w) < 0.7f) continue;
        bool ok = true;
        for (const Tree& t : w.trees)
            if (std::fabs(t.x - x) < 2.2f && std::fabs(t.y - y) < 2.2f && dist(t.x, t.y, x, y) < 2.2f) { ok = false; break; }
        if (!ok) continue;
        Tree t;
        t.id = w.nextId++;
        t.x = x;
        t.y = y;
        uint32_t r = rnd(w) % 10;
        t.kind = r < 5 ? 0 : r < 8 ? 1 : 2;
        w.trees.push_back(t);
    }
}

int addPlayer(World& w, const std::string& name, Role role) {
    Player p;
    p.id = w.nextId++;
    p.name = name;
    p.role = role;
    float a = (float)w.players.size() * 1.1f + 0.6f;
    p.x = w.fireX + std::cos(a) * 3.f;
    p.y = w.fireY + std::sin(a) * 3.f;
    p.facing = std::atan2(w.fireY - p.y, w.fireX - p.x);
    w.players.push_back(p);
    return p.id;
}

void step(World& w, const std::vector<Input>& inputs) {
    w.events.clear();
    w.tick++;
    w.time += kDt;
    w.phaseT += kDt;
    if (!w.night && w.phaseT >= kDayLen) beginNight(w);
    else if (w.night && w.phaseT >= kNightLen) beginDay(w);

    if (w.fuel > 0) {
        w.fuel -= w.night ? kDt : kDt * 0.5f;
        if (w.fuel <= 0) { w.fuel = 0; emit(w, Ev::FireOut, w.fireX, w.fireY); }
    }
    if (w.night && w.spawned < w.toSpawn) {
        w.spawnT -= kDt;
        if (w.spawnT <= 0) {
            spawnEnemy(w, EnemyKind::Wolf);
            w.spawned++;
            w.spawnT = 3.f;
        }
    }
    for (auto& t : w.trees) {
        t.shake = std::max(0.f, t.shake - kDt);
        if (t.regrow > 0) {
            t.regrow -= kDt;
            if (t.regrow <= 0) { t.regrow = 0; t.hp = 4; }
        }
    }
    for (size_t i = 0; i < w.players.size(); i++) stepPlayer(w, w.players[i], i < inputs.size() ? inputs[i] : Input{});
    for (auto& e : w.enemies) stepEnemy(w, e);
    for (auto& s : w.structures) s.hitT = std::max(0.f, s.hitT - kDt);
    for (auto it = w.structures.begin(); it != w.structures.end();) {
        if (it->hp <= 0) { emit(w, Ev::Broken, it->cx + 0.5f, it->cy + 0.5f); it = w.structures.erase(it); }
        else ++it;
    }
    w.enemies.erase(std::remove_if(w.enemies.begin(), w.enemies.end(), [](const Enemy& e) { return !e.alive && e.deadT > 2.5f; }),
                    w.enemies.end());
    bool anyAlive = false;
    for (const auto& p : w.players) anyAlive |= p.alive();
    w.lost = !w.players.empty() && !anyAlive && w.fuel <= 0;
}

float fireRadius(const World& w) { return w.fuel > 0 ? 7.f + 2.f * (w.fireLevel - 1) : 0.f; }

float phaseLeft(const World& w) { return std::max(0.f, (w.night ? kNightLen : kDayLen) - w.phaseT); }

const Structure* structureAt(const World& w, int cx, int cy) {
    for (const auto& s : w.structures) if (s.cx == cx && s.cy == cy) return &s;
    return nullptr;
}

bool canBuild(const World& w, const Player& p, Piece pc, int cx, int cy, std::string* why) {
    auto no = [&](const char* m) { if (why) *why = m; return false; };
    if (pc >= Piece::Count) return no("Нет такой детали");
    if (cx < 1 || cy < 1 || cx >= kMap - 1 || cy >= kMap - 1) return no("Слишком далеко");
    float x = cx + 0.5f, y = cy + 0.5f;
    if (dist(p.x, p.y, x, y) > 4.5f) return no("Подойди ближе");
    if (p.wood < piece(pc).wood) return no("Не хватает брёвен");
    if (dist(x, y, w.fireX, w.fireY) < 1.8f) return no("Слишком близко к огню");
    if (structureAt(w, cx, cy)) return no("Тут уже стоит");
    for (const auto& t : w.trees) if (t.alive() && dist(t.x, t.y, x, y) < 0.95f) return no("Мешает дерево");
    for (const auto& o : w.players) if (o.alive() && dist(o.x, o.y, x, y) < 0.75f) return no("Мешает человек");
    for (const auto& e : w.enemies) if (e.alive && dist(e.x, e.y, x, y) < 0.9f) return no("Мешает зверь");
    return true;
}

const Tree* treeInReach(const World& w, const Player& p) {
    float fx = p.x + std::cos(p.facing) * 0.8f, fy = p.y + std::sin(p.facing) * 0.8f;
    const Tree* best = nullptr;
    float bd = 1.3f;
    for (const auto& t : w.trees) {
        if (!t.alive()) continue;
        float d = dist(t.x, t.y, fx, fy);
        if (d < bd) { bd = d; best = &t; }
    }
    return best;
}

float lightAt(const World& w, float x, float y) {
    float best = 0;
    float r = fireRadius(w);
    if (r > 0) best = std::clamp((r - dist(x, y, w.fireX, w.fireY)) / 2.f, 0.f, 1.f);
    for (const auto& s : w.structures) {
        float lr = piece(s.kind).light;
        if (lr <= 0) continue;
        best = std::max(best, std::clamp((lr - dist(x, y, s.cx + 0.5f, s.cy + 0.5f)) / 1.5f, 0.f, 1.f));
    }
    return best;
}

}  // namespace surv
