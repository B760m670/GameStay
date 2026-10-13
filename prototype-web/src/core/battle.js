// Детерминированная симуляция боя. Используется и для атаки игрока, и для ночной обороны от Мглы.
import { GRID, BUILDINGS, DEFENSES, TROOPS, HAZE, ECLIPSE } from './config.js';
import { mulberry32, distToRect, computeLitGrid } from './grid.js';
import { findPath } from './pathfind.js';

const WALL_COST = 10;

function makeBuildingEntity(item, id) {
  const def = BUILDINGS[item.type];
  const lv = item.level - 1;
  const hp = def.hp[Math.min(lv, def.hp.length - 1)];
  return {
    id, type: item.type, def, x: item.x, y: item.y, size: def.size, level: item.level,
    hp, maxHp: hp, dead: false,
    isDefense: DEFENSES.includes(item.type), isWall: item.type === 'wall',
    isLight: !!def.isLight, lightR: def.isLight ? def.light[lv] : 0,
    loot: item.loot ? { ore: item.loot.ore || 0, aether: item.loot.aether || 0 } : null,
    cd: 0, target: null, beamT: 0, angle: Math.PI * 0.75, powered: true,
    cx: item.x + def.size / 2, cy: item.y + def.size / 2,
  };
}

export function createBattle(opts) {
  const {
    mode = 'attack', layout, army = {}, eclipseCharges = 0, seed = 1,
    duration = 180, waves = [], hazeMult = 1, name = '',
  } = opts;
  const B = {
    mode, name, time: 0, duration, rng: mulberry32(seed),
    buildings: [], troops: [], projectiles: [], effects: [], eclipses: [],
    eclipseCharges, pool: { ...army }, used: {},
    loot: { ore: 0, aether: 0 }, lootTotal: { ore: 0, aether: 0 },
    tileOwner: new Int32Array(GRID * GRID).fill(-1),
    lit: null, sources: [],
    destroyed: 0, total: 0, lanternDead: false,
    ended: false, result: null, nextId: 1, events: [],
    waves: waves.map((w) => ({ ...w, done: false })), hazeMult, lastDeployAt: -99,
  };
  layout.forEach((item, idx) => {
    const b = makeBuildingEntity(item, idx);
    B.buildings.push(b);
    if (!b.isWall) B.total++;
    if (b.loot) { B.lootTotal.ore += b.loot.ore; B.lootTotal.aether += b.loot.aether; }
    for (let j = b.y; j < b.y + b.size; j++)
      for (let i = b.x; i < b.x + b.size; i++) B.tileOwner[j * GRID + i] = idx;
  });
  relight(B);
  return B;
}

export function relight(B) {
  B.sources = B.buildings.filter((b) => b.isLight && !b.dead).map((b) => ({ x: b.cx, y: b.cy, r: b.lightR, id: b.id }));
  B.lit = computeLitGrid(B.sources, B.eclipses);
  for (const b of B.buildings) {
    if (b.isDefense) b.powered = !!B.lit[Math.floor(b.cy) * GRID + Math.floor(b.cx)];
  }
}

export function litAt(B, x, y) {
  const i = Math.floor(Math.max(0, Math.min(GRID - 1, x)));
  const j = Math.floor(Math.max(0, Math.min(GRID - 1, y)));
  return !!B.lit[j * GRID + i];
}

export function canDeploy(B, x, y) {
  if (x < 0 || y < 0 || x >= GRID || y >= GRID) return false;
  const i = Math.floor(x), j = Math.floor(y);
  if (B.lit[j * GRID + i]) return false;
  const o = B.tileOwner[j * GRID + i];
  if (o >= 0 && !B.buildings[o].dead) return false;
  return true;
}

function spawnTroop(B, type, x, y, def) {
  const hpMult = B.mode === 'defense' ? B.hazeMult : 1;
  const t = {
    id: B.nextId++, type, def, x, y,
    hp: def.hp * hpMult, maxHp: def.hp * hpMult, dmgMult: B.mode === 'defense' ? 1 + (B.hazeMult - 1) * 0.5 : 1,
    air: !!def.air, target: null, wall: null, path: null, pi: 0, cd: 0, dead: false,
    jx: (B.rng() - 0.5) * 0.6, jy: (B.rng() - 0.5) * 0.6, angle: 0, attacking: false, hidden: false,
    bob: B.rng() * 6.28,
  };
  B.troops.push(t);
  return t;
}

export function deploy(B, type, x, y) {
  if (B.ended || !(B.pool[type] > 0)) return false;
  if (!canDeploy(B, x, y)) return false;
  spawnTroop(B, type, x, y, TROOPS[type]);
  B.pool[type]--;
  B.used[type] = (B.used[type] || 0) + 1;
  B.lastDeployAt = B.time;
  return true;
}

export function castEclipse(B, x, y) {
  if (B.ended || B.eclipseCharges <= 0) return false;
  B.eclipseCharges--;
  B.eclipses.push({ x, y, r: ECLIPSE.radius, until: B.time + ECLIPSE.duration });
  B.effects.push({ kind: 'eclipse', x, y, life: 1, max: 1 });
  relight(B);
  B.events.push({ kind: 'eclipse' });
  return true;
}

// --- Выбор цели ---
function chooseTarget(B, t) {
  const pref = t.def.target;
  const alive = B.buildings.filter((b) => !b.dead);
  let pool = null;
  if (pref === 'defense') pool = alive.filter((b) => b.isDefense);
  else if (pref === 'light') pool = alive.filter((b) => b.isLight);
  else if (pref === 'wall') pool = alive.filter((b) => b.isWall);
  if (!pool || pool.length === 0) pool = alive.filter((b) => !b.isWall);
  let best = null, bd = Infinity;
  for (const b of pool) {
    const d = distToRect(t.x, t.y, b.x, b.y, b.size);
    if (d < bd) { bd = d; best = b; }
  }
  return best;
}

function cellCost(B) {
  return (i) => {
    const o = B.tileOwner[i];
    if (o < 0) return 1;
    const b = B.buildings[o];
    if (b.dead) return 1;
    return b.isWall ? 1 + WALL_COST : Infinity;
  };
}

// --- Урон ---
function damageBuilding(B, b, dmg, attacker) {
  if (b.dead) return;
  if (attacker && attacker.def.lightMult && b.isLight) dmg *= attacker.def.lightMult;
  const dealt = Math.min(dmg, b.hp);
  if (b.loot) {
    const f = dealt / b.maxHp;
    B.loot.ore += b.loot.ore * f;
    B.loot.aether += b.loot.aether * f;
  }
  b.hp -= dealt;
  b.hitT = 0.15;
  if (b.hp <= 0.001) destroyBuilding(B, b);
}

function destroyBuilding(B, b) {
  b.dead = true;
  b.hp = 0;
  B.effects.push({ kind: 'boom', x: b.cx, y: b.cy, life: 0.8, max: 0.8, size: b.size });
  if (!b.isWall) B.destroyed++;
  if (b.type === 'lantern') { B.lanternDead = true; B.events.push({ kind: 'lantern' }); }
  if (b.isLight) {
    relight(B);
    if (b.type === 'beacon') B.events.push({ kind: 'lightsOut', x: b.cx, y: b.cy });
  }
}

function damageTroop(B, t, dmg) {
  if (t.dead) return;
  t.hp -= dmg;
  t.hitT = 0.12;
  if (t.hp <= 0) {
    t.dead = true;
    B.effects.push({ kind: 'poof', x: t.x, y: t.y, life: 0.5, max: 0.5, color: t.def.color, air: t.air });
  }
}

// --- Обновление войск ---
function troopAttack(B, t, b) {
  t.attacking = true;
  t.angle = Math.atan2(b.cy - t.y, b.cx - t.x);
  if (t.cd > 0) return;
  const def = t.def;
  if (def.suicide) {
    for (const o of B.buildings) {
      if (o.dead) continue;
      if (distToRect(t.x, t.y, o.x, o.y, o.size) <= def.splash) damageBuilding(B, o, o.isWall ? def.dmg : def.dmg * 0.25, t);
    }
    B.effects.push({ kind: 'blast', x: t.x, y: t.y, life: 0.6, max: 0.6, r: def.splash });
    t.dead = true;
    return;
  }
  t.cd = def.cd;
  const dmg = def.dmg * t.dmgMult;
  if (def.ranged) {
    B.projectiles.push({
      kind: def.ranged, sx: t.x, sy: t.y, x: t.x, y: t.y, tx: b.cx, ty: b.cy, building: b, troop: null,
      t: 0, dur: Math.max(0.15, Math.hypot(b.cx - t.x, b.cy - t.y) / 9), dmg, attacker: t, fromAir: t.air,
    });
  } else {
    damageBuilding(B, b, dmg, t);
    B.effects.push({ kind: 'hit', x: t.x + Math.cos(t.angle) * 0.4, y: t.y + Math.sin(t.angle) * 0.4, life: 0.2, max: 0.2 });
  }
}

function moveToward(t, x, y, step) {
  const dx = x - t.x, dy = y - t.y;
  const d = Math.hypot(dx, dy);
  t.angle = Math.atan2(dy, dx);
  if (d <= step) { t.x = x; t.y = y; return true; }
  t.x += (dx / d) * step; t.y += (dy / d) * step;
  return false;
}

function updateTroop(B, t, dt) {
  if (t.cd > 0) t.cd -= dt;
  if (t.hitT > 0) t.hitT -= dt;
  t.attacking = false;
  t.hidden = !!t.def.stealth && !litAt(B, t.x, t.y);
  if (!t.target || t.target.dead) {
    t.target = chooseTarget(B, t);
    t.path = null; t.wall = null;
    if (!t.target) return;
  }
  const b = t.target;
  if (distToRect(t.x, t.y, b.x, b.y, b.size) <= t.def.range) { troopAttack(B, t, b); return; }
  const step = t.def.speed * dt;
  if (t.air) { moveToward(t, b.cx, b.cy, step); return; }
  if (t.wall) {
    if (t.wall.dead) t.wall = null;
    else if (distToRect(t.x, t.y, t.wall.x, t.wall.y, 1) <= Math.max(t.def.range, 0.6)) { troopAttack(B, t, t.wall); return; }
  }
  if (!t.path) {
    t.path = findPath(Math.floor(t.x), Math.floor(t.y), { x: b.x, y: b.y, size: b.size }, cellCost(B)) || [];
    t.pi = 0;
  }
  if (t.pi >= t.path.length) { moveToward(t, b.cx, b.cy, step); return; }
  const idx = t.path[t.pi];
  const o = B.tileOwner[idx];
  if (o >= 0 && B.buildings[o].isWall && !B.buildings[o].dead && B.buildings[o] !== b) {
    const w = B.buildings[o];
    t.wall = w;
    if (distToRect(t.x, t.y, w.x, w.y, 1) <= Math.max(t.def.range, 0.6)) { troopAttack(B, t, w); return; }
    moveToward(t, w.cx, w.cy, step);
    return;
  }
  const px = (idx % GRID) + 0.5 + t.jx, py = ((idx / GRID) | 0) + 0.5 + t.jy;
  if (moveToward(t, px, py, step)) t.pi++;
}

// --- Оборона ---
function canHit(b, t) {
  if (t.dead || t.hidden) return false;
  if (t.air ? !b.def.air : !b.def.ground) return false;
  const d = Math.hypot(t.x - b.cx, t.y - b.cy);
  if (d > b.def.range + 0.3) return false;
  if (b.def.minRange && d < b.def.minRange) return false;
  return true;
}

function updateDefense(B, b, dt) {
  if (b.cd > 0) b.cd -= dt;
  if (!b.powered) { b.target = null; b.beamT = 0; return; }
  if (b.target && !canHit(b, b.target)) { b.target = null; b.beamT = 0; }
  if (!b.target) {
    let best = null, bd = Infinity;
    for (const t of B.troops) {
      if (!canHit(b, t)) continue;
      const d = Math.hypot(t.x - b.cx, t.y - b.cy);
      if (d < bd) { bd = d; best = t; }
    }
    b.target = best; b.beamT = 0;
  }
  const t = b.target;
  if (!t) return;
  b.angle = Math.atan2(t.y - b.cy, t.x - b.cx);
  const lv = b.level - 1;
  if (b.def.beam) {
    b.beamT += dt;
    const mult = Math.min(b.def.rampMax, 1 + b.beamT * b.def.rampRate);
    damageTroop(B, t, b.def.dps[lv] * mult * dt);
    return;
  }
  if (b.cd > 0) return;
  b.cd = b.def.cd;
  const dmg = b.def.dmg[lv];
  const kind = b.def.proj.kind;
  if (kind === 'flame') {
    splashTroops(B, t.x, t.y, b.def.splash, dmg, b.def);
    B.effects.push({ kind: 'flame', x: t.x, y: t.y, sx: b.cx, sy: b.cy, life: 0.35, max: 0.35 });
    return;
  }
  const dist = Math.hypot(t.x - b.cx, t.y - b.cy);
  B.projectiles.push({
    kind, sx: b.cx, sy: b.cy, x: b.cx, y: b.cy, tx: t.x, ty: t.y,
    troop: kind === 'rock' ? null : t, building: null, t: 0, dur: Math.max(0.1, dist / b.def.proj.speed),
    dmg, splash: b.def.splash || 0, src: b.def,
  });
}

function splashTroops(B, x, y, r, dmg, srcDef) {
  for (const t of B.troops) {
    if (t.dead) continue;
    if (t.air ? !srcDef.air : !srcDef.ground) continue;
    if (Math.hypot(t.x - x, t.y - y) <= r) damageTroop(B, t, dmg);
  }
}

function updateProjectiles(B, dt) {
  for (const p of B.projectiles) {
    p.t += dt;
    if (p.troop && !p.troop.dead) { p.tx = p.troop.x; p.ty = p.troop.y; }
    const k = Math.min(1, p.t / p.dur);
    p.x = p.sx + (p.tx - p.sx) * k;
    p.y = p.sy + (p.ty - p.sy) * k;
    p.k = k;
    if (k >= 1) {
      p.done = true;
      if (p.building) damageBuilding(B, p.building, p.dmg, p.attacker);
      else if (p.splash) {
        splashTroops(B, p.tx, p.ty, p.splash, p.dmg, p.src);
        B.effects.push({ kind: 'blast', x: p.tx, y: p.ty, life: 0.5, max: 0.5, r: p.splash });
      } else if (p.troop) damageTroop(B, p.troop, p.dmg);
    }
  }
  B.projectiles = B.projectiles.filter((p) => !p.done);
}

// --- Волны Мглы ---
function spawnWave(B, w) {
  const side = Math.floor(B.rng() * 4);
  const along = 4 + B.rng() * (GRID - 8);
  const edge = 0.8;
  let cx, cy;
  if (side === 0) { cx = along; cy = edge; }
  else if (side === 1) { cx = GRID - edge; cy = along; }
  else if (side === 2) { cx = along; cy = GRID - edge; }
  else { cx = edge; cy = along; }
  for (const [type, n] of Object.entries(w.units)) {
    for (let k = 0; k < n; k++) {
      const x = Math.max(0.2, Math.min(GRID - 0.2, cx + (B.rng() - 0.5) * 3));
      const y = Math.max(0.2, Math.min(GRID - 0.2, cy + (B.rng() - 0.5) * 3));
      spawnTroop(B, type, x, y, HAZE[type]);
    }
  }
  B.effects.push({ kind: 'rift', x: cx, y: cy, life: 1.5, max: 1.5 });
  B.events.push({ kind: 'wave' });
}

// --- Главный шаг ---
export function step(B, dt) {
  if (B.ended) return;
  B.time += dt;
  if (B.eclipses.length) {
    const before = B.eclipses.length;
    B.eclipses = B.eclipses.filter((e) => e.until > B.time);
    if (B.eclipses.length !== before) relight(B);
  }
  for (const w of B.waves) if (!w.done && B.time >= w.t) { w.done = true; spawnWave(B, w); }
  for (const t of B.troops) if (!t.dead) updateTroop(B, t, dt);
  for (const b of B.buildings) {
    if (b.hitT > 0) b.hitT -= dt;
    if (b.isDefense && !b.dead) updateDefense(B, b, dt);
  }
  updateProjectiles(B, dt);
  for (const e of B.effects) e.life -= dt;
  B.effects = B.effects.filter((e) => e.life > 0);
  if (B.troops.length > 400) B.troops = B.troops.filter((t) => !t.dead);
  checkEnd(B);
}

export function aliveTroops(B) { return B.troops.filter((t) => !t.dead).length; }
export function poolLeft(B) { return Object.values(B.pool).reduce((a, b) => a + b, 0); }
export function percent(B) { return B.total ? Math.floor((B.destroyed / B.total) * 100) : 0; }

function checkEnd(B) {
  if (B.mode === 'attack') {
    const noForces = aliveTroops(B) === 0 && poolLeft(B) === 0 && B.time - B.lastDeployAt > 1 && B.projectiles.length === 0;
    if (B.time >= B.duration || B.destroyed >= B.total || noForces) finish(B);
  } else {
    const allSpawned = B.waves.every((w) => w.done);
    if (B.lanternDead || B.time >= B.duration || (allSpawned && aliveTroops(B) === 0)) finish(B);
  }
}

export function finish(B) {
  if (B.ended) return;
  B.ended = true;
  const pct = percent(B);
  let stars = 0;
  if (pct >= 50) stars++;
  if (B.lanternDead) stars++;
  if (pct >= 100) stars++;
  const loot = { ore: Math.floor(B.loot.ore), aether: Math.floor(B.loot.aether) };
  B.result = B.mode === 'attack'
    ? { stars, pct, loot, win: stars > 0, used: { ...B.used } }
    : { pct, loot, win: !B.lanternDead, lanternDead: B.lanternDead };
}

// Быстрая прогонка целиком (для тестов/автобоя).
export function simulate(B, maxTime = 400, dt = 1 / 30) {
  while (!B.ended && B.time < maxTime) step(B, dt);
  if (!B.ended) finish(B);
  return B.result;
}
