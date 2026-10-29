// Логика деревни: постройка, улучшение, добыча, армия. Без DOM — состояние S сериализуемо в JSON.
import {
  GRID, BORDER, BUILDINGS, LIMITS, TROOPS, HUT_COST, MAX_LANTERN,
  FIRST_NIGHT_MS, NIGHT_INTERVAL_MS,
} from './config.js';
import { lightSources, footprintLit, rectsOverlap, centerOf, isPointLit } from './grid.js';

export const SAVE_VERSION = 1;

export function createVillage(now = Date.now()) {
  const S = {
    version: SAVE_VERSION,
    created: now,
    res: { ore: 1500, aether: 1500, shards: 15 },
    buildings: [],
    nextId: 1,
    army: {},
    trainQueue: [],       // [{type, left}] секунд осталось
    trainTickAt: now,
    campaign: {},         // missionId -> звёзды
    codex: ['intro'],
    nightAt: now + FIRST_NIGHT_MS,
    nights: 0,
    stats: { attacks: 0, wins: 0, nightsWon: 0, nightsLost: 0 },
    flags: {},
  };
  const add = (type, x, y) => S.buildings.push(makeBuilding(S, type, x, y, 1, now));
  add('lantern', 20, 20);
  add('oreMine', 15, 19);
  add('aetherWell', 26, 21);
  add('oreStore', 20, 25);
  add('aetherStore', 21, 15);
  add('barracks', 24, 24);
  add('camp', 16, 23);
  add('ballista', 24, 17);
  add('builderHut', 18, 17);
  add('builderHut', 18, 26);
  S.army = { torch: 10 };
  return S;
}

function makeBuilding(S, type, x, y, level, now) {
  return { id: S.nextId++, type, x, y, level, busyUntil: 0, acc: 0, accAt: now };
}

export function getB(S, id) { return S.buildings.find((b) => b.id === id); }

export function lanternLevel(S) {
  const l = S.buildings.find((b) => b.type === 'lantern');
  return l ? l.level : 1;
}

export function countOf(S, type) { return S.buildings.filter((b) => b.type === type).length; }
export function limitOf(type, LL) { return LIMITS[type][Math.min(LL, MAX_LANTERN) - 1]; }

export function unlockLL(type) {
  const idx = LIMITS[type].findIndex((n) => n > 0);
  return idx + 1;
}

// Макс. уровень постройки при текущем уровне Фонаря.
export function maxLevelAllowed(type, LL) {
  const def = BUILDINGS[type];
  if (type === 'lantern') return MAX_LANTERN;
  return Math.max(0, Math.min(def.maxLevel, LL - unlockLL(type) + 1));
}

export function storageCap(S) {
  const cap = { ore: 0, aether: 0 };
  for (const b of S.buildings) {
    if (b.level < 1) continue;
    const def = BUILDINGS[b.type];
    if (b.type === 'lantern') {
      cap.ore += def.storage[b.level - 1];
      cap.aether += def.storage[b.level - 1];
    } else if (def.stores) {
      cap[def.stores] += def.storage[b.level - 1];
    }
  }
  return cap;
}

export function addRes(S, res, amount) {
  if (res === 'shards') { S.res.shards += amount; return amount; }
  const cap = storageCap(S)[res];
  const before = S.res[res];
  S.res[res] = Math.max(0, Math.min(cap, before + amount));
  return S.res[res] - before;
}

export function builders(S, now = Date.now()) {
  const total = S.buildings.filter((b) => b.type === 'builderHut' && b.level >= 1).length;
  const busy = S.buildings.filter((b) => b.busyUntil > now).length;
  return { total, busy, free: total - busy };
}

export function armyHousing(S) {
  let n = 0;
  for (const [t, c] of Object.entries(S.army)) n += TROOPS[t].housing * c;
  return n;
}
export function queueHousing(S) {
  return S.trainQueue.reduce((n, q) => n + TROOPS[q.type].housing, 0);
}
export function campCapacity(S) {
  return S.buildings
    .filter((b) => b.type === 'camp' && b.level >= 1)
    .reduce((n, b) => n + BUILDINGS.camp.capacity[b.level - 1], 0);
}
export function barracksLevel(S) {
  return S.buildings.filter((b) => b.type === 'barracks').reduce((m, b) => Math.max(m, b.level), 0);
}
export function barracksCount(S) {
  return S.buildings.filter((b) => b.type === 'barracks' && b.level >= 1).length;
}

// --- Добыча ---
export function collectorAmount(b, now = Date.now()) {
  const def = BUILDINGS[b.type];
  if (!def.produces || b.level < 1) return 0;
  const lv = b.level - 1;
  const gained = (def.rate[lv] * Math.max(0, now - b.accAt)) / 60000;
  return Math.min(def.cap[lv], b.acc + gained);
}

function freezeCollector(b, now) {
  b.acc = collectorAmount(b, now);
  b.accAt = now;
}

export function collect(S, id, now = Date.now()) {
  const b = getB(S, id);
  if (!b) return 0;
  const def = BUILDINGS[b.type];
  if (!def.produces) return 0;
  const amount = collectorAmount(b, now);
  const got = addRes(S, def.produces, Math.floor(amount));
  b.acc = amount - got;
  b.accAt = now;
  return got;
}

// --- Размещение ---
export function canPlace(S, type, x, y, ignoreId = null) {
  const def = BUILDINGS[type];
  const size = def.size;
  if (x < BORDER || y < BORDER || x + size > GRID - BORDER || y + size > GRID - BORDER) {
    return { ok: false, reason: 'За краем мира' };
  }
  for (const b of S.buildings) {
    if (b.id === ignoreId) continue;
    if (rectsOverlap(x, y, size, b.x, b.y, BUILDINGS[b.type].size)) return { ok: false, reason: 'Место занято' };
  }
  const sources = lightSources(S.buildings, ignoreId);
  if (type !== 'lantern' && !footprintLit(x, y, size, sources)) return { ok: false, reason: 'Здесь тьма — нужен свет' };
  // Если двигаем источник света — проверяем, что остальные постройки не останутся во тьме.
  if (def.isLight && ignoreId != null) {
    const moved = S.buildings.map((b) => (b.id === ignoreId ? { ...b, x, y } : b));
    const src = lightSources(moved);
    for (const b of S.buildings) {
      if (b.id === ignoreId) continue;
      const c = centerOf(b);
      if (!isPointLit(c.x, c.y, src)) return { ok: false, reason: 'Другие постройки останутся во тьме' };
    }
  }
  return { ok: true };
}

export function buildCost(S, type) {
  const def = BUILDINGS[type];
  if (type === 'builderHut') return { res: 'shards', amount: HUT_COST[Math.min(countOf(S, type), HUT_COST.length - 1)] };
  return { res: def.costRes, amount: def.cost[0] };
}

export function canBuild(S, type, now = Date.now()) {
  const LL = lanternLevel(S);
  const lim = limitOf(type, LL);
  if (lim === 0) return { ok: false, reason: `Нужен Фонарь ур. ${unlockLL(type)}` };
  if (countOf(S, type) >= lim) return { ok: false, reason: `Лимит ${lim} — улучши Фонарь` };
  const c = buildCost(S, type);
  if (S.res[c.res] < c.amount) return { ok: false, reason: 'Не хватает ресурсов' };
  if (BUILDINGS[type].time[0] > 0 && builders(S, now).free < 1) return { ok: false, reason: 'Все строители заняты' };
  return { ok: true };
}

export function placeBuilding(S, type, x, y, now = Date.now()) {
  const can = canBuild(S, type, now);
  if (!can.ok) return can;
  const pl = canPlace(S, type, x, y);
  if (!pl.ok) return pl;
  const c = buildCost(S, type);
  S.res[c.res] -= c.amount;
  const t = BUILDINGS[type].time[0];
  const b = makeBuilding(S, type, x, y, t > 0 ? 0 : 1, now);
  if (t > 0) b.busyUntil = now + t * 1000;
  S.buildings.push(b);
  return { ok: true, building: b };
}

export function moveBuilding(S, id, x, y) {
  const b = getB(S, id);
  if (!b) return { ok: false, reason: 'Нет постройки' };
  const pl = canPlace(S, b.type, x, y, id);
  if (!pl.ok) return pl;
  b.x = x; b.y = y;
  return { ok: true };
}

// --- Улучшение ---
export function upgradeInfo(S, id, now = Date.now()) {
  const b = getB(S, id);
  const def = BUILDINGS[b.type];
  const LL = lanternLevel(S);
  const next = b.level + 1;
  if (b.busyUntil > now) return { ok: false, reason: 'Уже строится' };
  if (next > def.maxLevel) return { ok: false, reason: 'Максимальный уровень', max: true };
  if (next > maxLevelAllowed(b.type, LL)) return { ok: false, reason: 'Нужен Фонарь выше' };
  const cost = def.cost[next - 1];
  const time = def.time[next - 1];
  const res = def.costRes;
  if (S.res[res] < cost) return { ok: false, reason: 'Не хватает ресурсов', cost, time, res };
  if (time > 0 && builders(S, now).free < 1) return { ok: false, reason: 'Все строители заняты', cost, time, res };
  return { ok: true, cost, time, res, next };
}

export function startUpgrade(S, id, now = Date.now()) {
  const info = upgradeInfo(S, id, now);
  if (!info.ok) return info;
  const b = getB(S, id);
  S.res[info.res] -= info.cost;
  if (info.time > 0) b.busyUntil = now + info.time * 1000;
  else finishBuild(b, now);
  return { ok: true };
}

function finishBuild(b, now) {
  if (BUILDINGS[b.type].produces) freezeCollector(b, now);
  b.level += 1;
  b.busyUntil = 0;
}

export function speedUpCost(b, now = Date.now()) {
  const left = Math.max(0, b.busyUntil - now) / 1000;
  return Math.max(1, Math.ceil(left / 60));
}

export function speedUp(S, id, now = Date.now()) {
  const b = getB(S, id);
  if (!b || b.busyUntil <= now) return { ok: false };
  const cost = speedUpCost(b, now);
  if (S.res.shards < cost) return { ok: false, reason: 'Не хватает осколков' };
  S.res.shards -= cost;
  b.busyUntil = now;
  tick(S, now);
  return { ok: true };
}

// --- Армия ---
export function canTrain(S, type) {
  const t = TROOPS[type];
  if (barracksLevel(S) < t.unlock) return { ok: false, reason: `Нужна казарма ур. ${t.unlock}` };
  if (barracksCount(S) < 1) return { ok: false, reason: 'Нет готовой казармы' };
  if (armyHousing(S) + queueHousing(S) + t.housing > campCapacity(S)) return { ok: false, reason: 'Лагеря полны' };
  if (S.res.aether < t.cost) return { ok: false, reason: 'Не хватает Эфира' };
  return { ok: true };
}

export function train(S, type, now = Date.now()) {
  const c = canTrain(S, type);
  if (!c.ok) return c;
  if (S.trainQueue.length === 0) S.trainTickAt = now;
  S.res.aether -= TROOPS[type].cost;
  S.trainQueue.push({ type, left: TROOPS[type].time });
  return { ok: true };
}

export function cancelTrain(S, index) {
  const q = S.trainQueue[index];
  if (!q) return;
  S.trainQueue.splice(index, 1);
  addRes(S, 'aether', TROOPS[q.type].cost);
}

// --- Время ---
export function tick(S, now = Date.now()) {
  for (const b of S.buildings) {
    if (b.busyUntil && b.busyUntil <= now) finishBuild(b, Math.min(now, b.busyUntil));
  }
  const nb = barracksCount(S);
  if (S.trainQueue.length && nb > 0) {
    let budget = ((now - S.trainTickAt) / 1000) * nb;
    while (S.trainQueue.length && budget > 0) {
      const q = S.trainQueue[0];
      const use = Math.min(q.left, budget);
      q.left -= use;
      budget -= use;
      if (q.left <= 1e-6) {
        S.trainQueue.shift();
        S.army[q.type] = (S.army[q.type] || 0) + 1;
      }
    }
  }
  S.trainTickAt = now;
}

export function nightDue(S, now = Date.now()) { return now >= S.nightAt; }
export function scheduleNextNight(S, now = Date.now()) { S.nightAt = now + NIGHT_INTERVAL_MS; }

// Снимок деревни для боя (только достроенные постройки).
export function villageLayout(S) {
  return S.buildings.filter((b) => b.level >= 1).map((b) => ({ type: b.type, x: b.x, y: b.y, level: b.level }));
}

// Доля запасов, которую можно потерять ночью: распределяем по хранилищам для отображения.
export function lootLayout(S, frac) {
  const layout = villageLayout(S);
  const cap = storageCap(S);
  for (const b of layout) {
    const def = BUILDINGS[b.type];
    if (def.stores) b.loot = { [def.stores]: (S.res[def.stores] * frac * def.storage[b.level - 1]) / Math.max(1, cap[def.stores]) };
    if (b.type === 'lantern') {
      const s = def.storage[b.level - 1];
      b.loot = { ore: (S.res.ore * frac * s) / Math.max(1, cap.ore), aether: (S.res.aether * frac * s) / Math.max(1, cap.aether) };
    }
  }
  return layout;
}
