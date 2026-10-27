// Детерминированная симуляция боя (поход игрока и отпор набегу разбойников) + поиск пути A*.
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

#include "core.h"

namespace lf {

// ---------- A* ----------
std::vector<int> findPath(int sx, int sy, int gx, int gy, int gs, const std::vector<float>& cost, int maxIter) {
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static const float DC[8] = {1, 1, 1, 1, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f};
    const int N = kGrid * kGrid;
    const float INF = std::numeric_limits<float>::infinity();
    std::vector<float> g(N, INF);
    std::vector<int> from(N, -1);
    std::vector<uint8_t> closed(N, 0);
    auto inGoal = [&](int x, int y) { return x >= gx && x < gx + gs && y >= gy && y < gy + gs; };
    auto H = [&](int x, int y) {
        int dx = std::max({gx - x, 0, x - (gx + gs - 1)});
        int dy = std::max({gy - y, 0, y - (gy + gs - 1)});
        return std::max(dx, dy) + 0.41421356f * std::min(dx, dy);
    };
    sx = std::clamp(sx, 0, kGrid - 1);
    sy = std::clamp(sy, 0, kGrid - 1);
    int start = sy * kGrid + sx;
    g[start] = 0;
    using Node = std::pair<float, int>;
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    open.push({H(sx, sy), start});
    int iter = 0;
    while (!open.empty() && iter++ < maxIter) {
        int i = open.top().second;
        open.pop();
        if (closed[i]) continue;
        closed[i] = 1;
        int x = i % kGrid, y = i / kGrid;
        if (inGoal(x, y)) {
            std::vector<int> path;
            for (int c = i; c != start; c = from[c]) path.push_back(c);
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (int d = 0; d < 8; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= kGrid || ny >= kGrid) continue;
            int ni = ny * kGrid + nx;
            if (closed[ni]) continue;
            float c = inGoal(nx, ny) ? 1.f : cost[ni];
            if (std::isinf(c)) continue;
            if (DX[d] && DY[d]) {
                int a = y * kGrid + nx, b = ny * kGrid + x;
                if ((!inGoal(nx, y) && cost[a] > 1.f) || (!inGoal(x, ny) && cost[b] > 1.f)) continue;
            }
            float ng = g[i] + DC[d] * c;
            if (ng < g[ni]) {
                g[ni] = ng;
                from[ni] = i;
                open.push({ng + H(nx, ny), ni});
            }
        }
    }
    return {};
}

// ---------- Бой ----------
namespace {
constexpr float kWallCost = 10.f;
}

Battle::Battle(const BattleOptions& o)
    : mode(o.mode), duration(o.duration), rng(o.seed), spells(o.spells), heroLevel(o.heroLevel), pool(o.army),
      tileOwner(kGrid * kGrid, -1), waves(o.waves), raidMult(o.raidMult), troopLevels(o.troopLevels) {
    for (size_t idx = 0; idx < o.layout.size(); idx++) {
        const auto& it = o.layout[idx];
        const BuildingDef& d = B(it.type);
        BBuilding b;
        b.id = (int)idx;
        b.type = it.type;
        b.x = it.x;
        b.y = it.y;
        b.level = it.level;
        b.size = d.size;
        b.def = &d;
        b.hp = b.maxHp = (float)d.lv(d.hp, it.level);
        b.cx = it.x + d.size / 2.f;
        b.cy = it.y + d.size / 2.f;
        b.lootOre = it.lootOre;
        b.lootAether = it.lootAether;
        if (b.isStructure()) total++;
        lootTotalOre += b.lootOre;
        lootTotalAether += b.lootAether;
        if (!b.isTrap())  // по ловушкам ходят — они не занимают клетку
            for (int j = b.y; j < b.y + b.size; j++)
            for (int i = b.x; i < b.x + b.size; i++) tileOwner[j * kGrid + i] = (int)idx;
        buildings.push_back(b);
    }
    updateDeployZone();
}

void Battle::updateDeployZone() {
    // Высаживаться можно на открытой земле: не на постройках и не вплотную к ним (клетка зазора).
    noDeploy.assign(kGrid * kGrid, 0);
    for (const auto& b : buildings) {
        if (b.dead || b.isTrap()) continue;
        for (int j = std::max(0, b.y - 1); j < std::min(kGrid, b.y + b.size + 1); j++)
            for (int i = std::max(0, b.x - 1); i < std::min(kGrid, b.x + b.size + 1); i++) noDeploy[j * kGrid + i] = 1;
    }
}

bool Battle::canDeploy(float x, float y) const {
    if (x < 0 || y < 0 || x >= kGrid || y >= kGrid) return false;
    return !noDeploy[(int)y * kGrid + (int)x];
}

int Battle::spawn(TroopType type, float x, float y) {
    const TroopDef& d = T(type);
    float hpMult = mode == BattleMode::Defense ? raidMult : 1.f;
    if (!d.raider) {
        auto it = troopLevels.find(type);
        if (it != troopLevels.end()) hpMult *= village::troopMult(it->second);
    }
    BTroop t;
    t.id = nextId++;
    t.type = type;
    t.def = &d;
    t.x = x;
    t.y = y;
    t.hp = t.maxHp = d.hp * hpMult;
    t.dmgMult = mode == BattleMode::Defense ? 1.f + (raidMult - 1.f) * 0.5f : 1.f;
    if (!d.raider && troopLevels.count(type)) t.dmgMult *= village::troopMult(troopLevels.at(type));
    t.air = d.air;
    t.jx = (rng.nextf() - 0.5f) * 0.6f;
    t.jy = (rng.nextf() - 0.5f) * 0.6f;
    t.bob = rng.nextf() * 6.28f;
    troops.push_back(std::move(t));
    return (int)troops.size() - 1;
}

bool Battle::deploy(TroopType type, float x, float y) {
    auto it = pool.find(type);
    if (ended || it == pool.end() || it->second <= 0) return false;
    if (!canDeploy(x, y)) return false;
    spawn(type, x, y);
    events.push_back({Ev::Deploy, x, y, BType::Lantern});
    it->second--;
    used[type]++;
    lastDeployAt = time;
    return true;
}

bool Battle::deployHero(float x, float y) {
    if (ended || heroLevel <= 0 || heroIdx >= 0 || !canDeploy(x, y)) return false;
    heroIdx = spawn(TroopType::Hero, x, y);
    BTroop& h = troops[heroIdx];
    h.hp = h.maxHp = h.maxHp * village::heroMult(heroLevel);
    h.dmgMult *= village::heroMult(heroLevel);
    events.push_back({Ev::Deploy, x, y, BType::Lantern});
    lastDeployAt = time;
    return true;
}

bool Battle::heroAbility() {
    if (ended || heroIdx < 0 || heroAbilityUsed || troops[heroIdx].dead) return false;
    heroAbilityUsed = true;
    const BTroop& h = troops[heroIdx];
    // Боевой клич: отряды вокруг подлечиваются и на время свирепеют
    for (auto& t : troops)
        if (!t.dead && !t.def->raider && std::hypot(t.x - h.x, t.y - h.y) <= kHeroCryRadius) {
            t.hp = std::min(t.maxHp, t.hp + t.maxHp * kHeroCryHeal);
            t.rageT = kHeroCryTime;
        }
    effects.push_back({Fx::Cry, h.x, h.y, 0, 0, 0.9f, 0.9f, kHeroCryRadius});
    events.push_back({Ev::HeroCry, h.x, h.y, BType::Lantern});
    return true;
}

bool Battle::castSpell(Spell s, float x, float y) {
    auto it = spells.find(s);
    if (ended || it == spells.end() || it->second <= 0) return false;
    it->second--;
    zones.push_back({s, x, y, kSpellRadius, time + kSpellDuration});
    events.push_back({Ev::SpellCast, x, y, BType::Lantern});
    return true;
}

int Battle::chooseTarget(const BTroop& t) const {
    auto pick = [&](auto pred) {
        int best = -1;
        float bd = std::numeric_limits<float>::max();
        for (size_t i = 0; i < buildings.size(); i++) {
            const auto& b = buildings[i];
            if (b.dead || b.isTrap() || !pred(b)) continue;
            float d = distToRect(t.x, t.y, (float)b.x, (float)b.y, (float)b.size);
            if (d < bd) { bd = d; best = (int)i; }
        }
        return best;
    };
    int r = -1;
    switch (t.def->target) {
        case Target::Defense: r = pick([](const BBuilding& b) { return b.isDefense(); }); break;
        case Target::Economy: r = pick([](const BBuilding& b) { return b.isEconomy(); }); break;
        case Target::Wall: r = pick([](const BBuilding& b) { return b.isWall(); }); break;
        default: break;
    }
    if (r < 0) r = pick([](const BBuilding& b) { return !b.isWall(); });
    return r;
}

float Battle::cellCost(int i) const {
    int o = tileOwner[i];
    if (o < 0) return 1.f;
    const auto& b = buildings[o];
    if (b.dead) return 1.f;
    return b.isWall() ? 1.f + kWallCost : std::numeric_limits<float>::infinity();
}

void Battle::damageBuilding(int bi, float dmg, int attacker) {
    auto& b = buildings[bi];
    if (b.dead) return;
    if (attacker >= 0) {
        const TroopDef& d = *troops[attacker].def;
        if (b.isEconomy()) dmg *= d.econMult;
        if (b.isWall()) dmg *= d.wallMult;
        if (b.isDefense()) dmg *= d.defenseMult;
    }
    float dealt = std::min(dmg, b.hp);
    if (b.lootOre > 0 || b.lootAether > 0) {
        float f = dealt / b.maxHp;
        lootOre += b.lootOre * f;
        lootAether += b.lootAether * f;
    }
    b.hp -= dealt;
    b.hitT = 0.15f;
    if (b.hp <= 0.001f) destroyBuilding(bi);
}

void Battle::destroyBuilding(int bi) {
    auto& b = buildings[bi];
    b.dead = true;
    b.hp = 0;
    effects.push_back({Fx::Boom, b.cx, b.cy, 0, 0, 0.8f, 0.8f, (float)b.size});
    events.push_back({Ev::Destroyed, b.cx, b.cy, b.type});
    if (b.isStructure()) destroyed++;
    if (b.type == BType::Lantern) {
        hqDead = true;
        events.push_back({Ev::Hq, b.cx, b.cy, b.type});
    }
    updateDeployZone();
}

void Battle::damageTroop(int ti, float dmg) {
    auto& t = troops[ti];
    if (t.dead) return;
    t.hp -= dmg;
    t.hitT = 0.12f;
    if (t.hp <= 0) {
        t.dead = true;
        Effect e{Fx::Poof, t.x, t.y, 0, 0, 0.5f, 0.5f};
        e.air = t.air;
        e.color = t.def->color;
        effects.push_back(e);
    }
}

void Battle::troopAttack(int ti, int bi) {
    auto& t = troops[ti];
    const auto& b = buildings[bi];
    t.attacking = true;
    t.angle = std::atan2(b.cy - t.y, b.cx - t.x);
    if (t.cd > 0) return;
    const TroopDef& d = *t.def;
    t.cd = d.cd;
    float dmg = d.dmg * t.dmgMult;
    if (!t.charged) {  // натиск: первый удар по новой цели
        dmg *= d.chargeMult;
        t.charged = true;
    }
    if (d.ranged != Proj::None) {
        Projectile p;
        p.kind = d.ranged;
        p.sx = p.x = t.x;
        p.sy = p.y = t.y;
        p.tx = b.cx;
        p.ty = b.cy;
        p.building = bi;
        p.dur = std::max(0.15f, std::hypot(b.cx - t.x, b.cy - t.y) / 9.f);
        p.dmg = dmg;
        p.attacker = ti;
        p.fromAir = t.air;
        projectiles.push_back(p);
    } else {
        float ax = t.x + std::cos(t.angle) * 0.4f, ay = t.y + std::sin(t.angle) * 0.4f;
        damageBuilding(bi, dmg, ti);
        effects.push_back({Fx::Hit, ax, ay, 0, 0, 0.2f, 0.2f});
        events.push_back({Ev::Hit, ax, ay, b.type});
    }
}

static bool moveToward(BTroop& t, float x, float y, float step) {
    float dx = x - t.x, dy = y - t.y, d = std::hypot(dx, dy);
    t.angle = std::atan2(dy, dx);
    if (d <= step) { t.x = x; t.y = y; return true; }
    t.x += dx / d * step;
    t.y += dy / d * step;
    return false;
}

void Battle::updateTroop(int ti, float dt) {
    BTroop& t = troops[ti];
    float rage = 1;
    if (t.rageT > 0) { t.rageT -= dt; rage = kRageMult; }
    for (const auto& z : zones)
        if (z.kind == Spell::Rage && !t.def->raider && std::hypot(t.x - z.x, t.y - z.y) <= z.r) rage = kRageMult;
    if (t.cd > 0) t.cd -= dt * rage;
    if (t.hitT > 0) t.hitT -= dt;
    t.animT += dt;
    t.attacking = false;
    if (t.target < 0 || buildings[t.target].dead) {
        t.target = chooseTarget(t);
        t.charged = false;
        t.hasPath = false;
        t.wall = -1;
        if (t.target < 0) return;
    }
    const BBuilding& b = buildings[t.target];
    if (distToRect(t.x, t.y, (float)b.x, (float)b.y, (float)b.size) <= t.def->range) { troopAttack(ti, t.target); return; }
    float step = t.def->speed * dt * rage;
    if (t.air) { moveToward(t, b.cx, b.cy, step); return; }
    float reach = std::max(t.def->range, 0.6f);
    if (t.wall >= 0) {
        const auto& w = buildings[t.wall];
        if (w.dead) t.wall = -1;
        else if (distToRect(t.x, t.y, (float)w.x, (float)w.y, 1) <= reach) { troopAttack(ti, t.wall); return; }
    }
    if (!t.hasPath) {
        std::vector<float> cost(kGrid * kGrid);
        for (int i = 0; i < kGrid * kGrid; i++) cost[i] = cellCost(i);
        t.path = findPath((int)t.x, (int)t.y, b.x, b.y, b.size, cost);
        t.hasPath = true;
        t.pi = 0;
    }
    if (t.pi >= (int)t.path.size()) { moveToward(t, b.cx, b.cy, step); return; }
    int idx = t.path[t.pi];
    int o = tileOwner[idx];
    if (o >= 0 && buildings[o].isWall() && !buildings[o].dead && o != t.target) {
        const auto& w = buildings[o];
        t.wall = o;
        if (distToRect(t.x, t.y, (float)w.x, (float)w.y, 1) <= reach) { troopAttack(ti, o); return; }
        moveToward(t, w.cx, w.cy, step);
        return;
    }
    float px = idx % kGrid + 0.5f + t.jx, py = idx / kGrid + 0.5f + t.jy;
    if (moveToward(t, px, py, step)) t.pi++;
}

bool Battle::canHit(const BBuilding& b, const BTroop& t) const {
    if (t.dead) return false;
    if (t.air ? !b.def->air : !b.def->ground) return false;
    float d = std::hypot(t.x - b.cx, t.y - b.cy);
    if (d > b.def->range + 0.3f) return false;
    if (b.def->minRange > 0 && d < b.def->minRange) return false;
    return true;
}

void Battle::updateDefense(int bi, float dt) {
    BBuilding& b = buildings[bi];
    if (b.cd > 0) b.cd -= dt;
    if (b.target >= 0 && !canHit(b, troops[b.target])) b.target = -1;
    if (b.target < 0) {
        int best = -1;
        float bd = std::numeric_limits<float>::max();
        for (size_t i = 0; i < troops.size(); i++) {
            if (!canHit(b, troops[i])) continue;
            float d = std::hypot(troops[i].x - b.cx, troops[i].y - b.cy);
            if (d < bd) { bd = d; best = (int)i; }
        }
        b.target = best;
    }
    if (b.target < 0) return;
    const BTroop& tr = troops[b.target];
    b.angle = std::atan2(tr.y - b.cy, tr.x - b.cx);
    if (b.cd > 0) return;
    b.cd = b.def->cd;
    events.push_back({Ev::Shot, b.cx, b.cy, b.type});
    float dmg = (float)b.def->lv(b.def->dmg, b.level);
    if (b.def->proj == Proj::Pitch) {  // смола льётся сразу, без полёта снаряда
        float tx = tr.x, ty = tr.y;
        splashTroops(tx, ty, b.def->splash, dmg, b.def);
        effects.push_back({Fx::Pitch, tx, ty, b.cx, b.cy, 0.35f, 0.35f});
        return;
    }
    Projectile p;
    p.kind = b.def->proj;
    p.sx = p.x = b.cx;
    p.sy = p.y = b.cy;
    p.tx = tr.x;
    p.ty = tr.y;
    p.troop = b.def->proj == Proj::Ball ? -1 : b.target;  // ядро летит в точку, болт и пуля — в цель
    p.dur = std::max(0.1f, std::hypot(tr.x - b.cx, tr.y - b.cy) / b.def->projSpeed);
    p.dmg = dmg;
    p.splash = b.def->splash;
    p.src = b.def;
    projectiles.push_back(p);
}

void Battle::updateTrap(int bi) {
    BBuilding& b = buildings[bi];
    bool near = false;
    for (const auto& t : troops)
        if (!t.dead && !t.air && std::hypot(t.x - b.cx, t.y - b.cy) <= b.def->range) { near = true; break; }
    if (!near) return;
    b.dead = true;
    b.hp = 0;
    splashTroops(b.cx, b.cy, b.def->splash, (float)b.def->lv(b.def->dmg, b.level), b.def);
    effects.push_back({Fx::Blast, b.cx, b.cy, 0, 0, 0.6f, 0.6f, b.def->splash});
    effects.push_back({Fx::Boom, b.cx, b.cy, 0, 0, 0.8f, 0.8f, 1.2f});
    events.push_back({Ev::Trap, b.cx, b.cy, b.type});
}

void Battle::splashTroops(float x, float y, float r, float dmg, const BuildingDef* src) {
    for (size_t i = 0; i < troops.size(); i++) {
        const auto& t = troops[i];
        if (t.dead) continue;
        if (t.air ? !src->air : !src->ground) continue;
        if (std::hypot(t.x - x, t.y - y) <= r) damageTroop((int)i, dmg);
    }
}

void Battle::updateProjectiles(float dt) {
    for (size_t i = 0; i < projectiles.size(); i++) {
        Projectile& p = projectiles[i];
        p.t += dt;
        if (p.troop >= 0 && !troops[p.troop].dead) { p.tx = troops[p.troop].x; p.ty = troops[p.troop].y; }
        p.k = std::min(1.f, p.t / p.dur);
        p.x = p.sx + (p.tx - p.sx) * p.k;
        p.y = p.sy + (p.ty - p.sy) * p.k;
        if (p.k < 1.f) continue;
        p.done = true;
        Projectile c = p;  // копия: следующие вызовы могут добавить снаряды и сдвинуть вектор
        if (c.building >= 0) damageBuilding(c.building, c.dmg, c.attacker);
        else if (c.splash > 0) {
            splashTroops(c.tx, c.ty, c.splash, c.dmg, c.src);
            effects.push_back({Fx::Blast, c.tx, c.ty, 0, 0, 0.5f, 0.5f, c.splash});
        } else if (c.troop >= 0) damageTroop(c.troop, c.dmg * troops[c.troop].def->rangedArmor);  // стрелы, болты, пули
    }
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return p.done; }), projectiles.end());
}

void Battle::spawnWave(Wave& w) {
    int side = rng.range(4);
    float along = 4 + rng.nextf() * (kGrid - 8), edge = 0.8f, cx, cy;
    if (side == 0) { cx = along; cy = edge; }
    else if (side == 1) { cx = kGrid - edge; cy = along; }
    else if (side == 2) { cx = along; cy = kGrid - edge; }
    else { cx = edge; cy = along; }
    for (const auto& [type, n] : w.units)
        for (int k = 0; k < n; k++) {
            float x = std::clamp(cx + (rng.nextf() - 0.5f) * 3, 0.2f, kGrid - 0.2f);
            float y = std::clamp(cy + (rng.nextf() - 0.5f) * 3, 0.2f, kGrid - 0.2f);
            spawn(type, x, y);
        }
    effects.push_back({Fx::Arrival, cx, cy, 0, 0, 1.5f, 1.5f});
    events.push_back({Ev::Wave, cx, cy, BType::Lantern});
}

void Battle::step(float dt) {
    if (ended) return;
    time += dt;
    for (auto& w : waves)
        if (!w.done && time >= w.t) { w.done = true; spawnWave(w); }
    if (!zones.empty()) {
        zones.erase(std::remove_if(zones.begin(), zones.end(), [&](const SpellZone& z) { return z.until <= time; }), zones.end());
        for (const auto& z : zones) {
            if (z.kind != Spell::Heal) continue;
            for (auto& t : troops)
                if (!t.dead && !t.def->raider && std::hypot(t.x - z.x, t.y - z.y) <= z.r) t.hp = std::min(t.maxHp, t.hp + t.maxHp * kHealPerSec * dt);
        }
    }
    size_t n = troops.size();
    for (size_t i = 0; i < n; i++)
        if (!troops[i].dead) updateTroop((int)i, dt);
    for (size_t i = 0; i < buildings.size(); i++) {
        if (buildings[i].hitT > 0) buildings[i].hitT -= dt;
        if (buildings[i].dead) continue;
        if (buildings[i].isTrap()) updateTrap((int)i);
        else if (buildings[i].isDefense()) updateDefense((int)i, dt);
    }
    updateProjectiles(dt);
    for (auto& e : effects) e.life -= dt;
    effects.erase(std::remove_if(effects.begin(), effects.end(), [](const Effect& e) { return e.life <= 0; }), effects.end());
    checkEnd();
}

int Battle::aliveTroops() const { return (int)std::count_if(troops.begin(), troops.end(), [](const BTroop& t) { return !t.dead; }); }
int Battle::poolLeft() const {
    int n = 0;
    for (const auto& [k, v] : pool) n += v;
    return n;
}
int Battle::percent() const { return total > 0 ? destroyed * 100 / total : 0; }

void Battle::checkEnd() {
    if (mode == BattleMode::Attack) {
        bool noForces = aliveTroops() == 0 && poolLeft() == 0 && time - lastDeployAt > 1 && projectiles.empty();
        if (time >= duration || destroyed >= total || noForces) finish();
    } else {
        bool allSpawned = std::all_of(waves.begin(), waves.end(), [](const Wave& w) { return w.done; });
        if (hqDead || time >= duration || (allSpawned && aliveTroops() == 0)) finish();
    }
}

void Battle::finish() {
    if (ended) return;
    ended = true;
    int pct = percent(), stars = 0;
    if (pct >= 50) stars++;
    if (hqDead) stars++;
    if (pct >= 100) stars++;
    result = {};
    result.stars = mode == BattleMode::Attack ? stars : 0;
    result.pct = pct;
    result.lootOre = (long long)std::floor(lootOre);
    result.lootAether = (long long)std::floor(lootAether);
    result.win = mode == BattleMode::Attack ? stars > 0 : !hqDead;
    result.hqDead = hqDead;
    result.used = used;
}

BattleResult Battle::simulate(float maxTime, float dt) {
    while (!ended && time < maxTime) { step(dt); events.clear(); }
    if (!ended) finish();
    return result;
}

}  // namespace lf
