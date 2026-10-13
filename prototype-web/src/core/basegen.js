// Процедурная генерация вражеских баз и ночных волн Мглы.
import { GRID, BORDER, BUILDINGS, LIMITS, BASE_LOOT, HAZE } from './config.js';
import { mulberry32, isPointLit, footprintLit } from './grid.js';
import { maxLevelAllowed } from './village.js';

const NAMES_A = ['Серый', 'Угасший', 'Безмолвный', 'Хромой', 'Пепельный', 'Слепой', 'Ржавый', 'Последний', 'Одинокий', 'Тусклый'];
const NAMES_B = ['Ольгерд', 'Мирава', 'Тарн', 'Ивица', 'Бранко', 'Ведана', 'Корвин', 'Ярена', 'Стигг', 'Лучезар', 'Агния', 'Ратибор'];

export function rivalName(rng) {
  return `${NAMES_A[Math.floor(rng() * NAMES_A.length)]} ${NAMES_B[Math.floor(rng() * NAMES_B.length)]}`;
}

/**
 * @param LL уровень Фонаря базы
 * @param seed зерно
 * @param counts переопределение количества построек {type: n}
 * @param richness множитель добычи
 */
export function generateBase({ LL, seed = 1, counts = {}, richness = 1, levelBias = 0 }) {
  const rng = mulberry32(seed);
  const layout = [];
  const occ = new Int8Array(GRID * GRID); // 0 пусто, 1 здание, 2 стена
  const sources = [];
  const lvl = (type) => {
    const max = maxLevelAllowed(type, LL);
    if (max <= 0) return 1;
    const l = max - (rng() < 0.4 ? 1 : 0) + levelBias;
    return Math.max(1, Math.min(BUILDINGS[type].maxLevel, max, l));
  };
  const count = (type) => (counts[type] != null ? counts[type] : LIMITS[type][LL - 1]);

  const free = (x, y, s, margin) => {
    if (x < BORDER || y < BORDER || x + s > GRID - BORDER || y + s > GRID - BORDER) return false;
    for (let j = y - margin; j < y + s + margin; j++)
      for (let i = x - margin; i < x + s + margin; i++) {
        if (i < 0 || j < 0 || i >= GRID || j >= GRID) continue;
        const inside = i >= x && i < x + s && j >= y && j < y + s;
        const v = occ[j * GRID + i];
        if (v === 1 || (inside && v === 2)) return false;
      }
    return true;
  };
  const put = (type, x, y, level) => {
    const s = BUILDINGS[type].size;
    for (let j = y; j < y + s; j++) for (let i = x; i < x + s; i++) occ[j * GRID + i] = type === 'wall' ? 2 : 1;
    const item = { type, x, y, level };
    layout.push(item);
    if (BUILDINGS[type].isLight) sources.push({ x: x + s / 2, y: y + s / 2, r: BUILDINGS[type].light[level - 1] });
    return item;
  };

  const C = GRID / 2;
  const lantern = put('lantern', C - 2, C - 2, LL);

  // Светочи — по кругу у края света Фонаря.
  const nBeacons = count('beacon');
  const lr = BUILDINGS.lantern.light[LL - 1];
  const a0 = rng() * Math.PI * 2;
  for (let k = 0; k < nBeacons; k++) {
    const level = lvl('beacon');
    let placed = false;
    for (let tries = 0; tries < 40 && !placed; tries++) {
      const a = a0 + (k * Math.PI * 2) / Math.max(1, nBeacons) + (rng() - 0.5) * 0.6;
      const d = lr - 1.8 - rng() * 1.5 - tries * 0.05;
      const x = Math.round(C + Math.cos(a) * d - 1), y = Math.round(C + Math.sin(a) * d - 1);
      if (free(x, y, 2, 1) && footprintLit(x, y, 2, sources)) { put('beacon', x, y, level); placed = true; }
    }
  }

  // Стены — квадратные кольца вокруг Фонаря.
  let walls = count('wall');
  const wallLevel = lvl('wall');
  const ring = (k) => {
    const x0 = lantern.x - k, y0 = lantern.y - k, x1 = lantern.x + 3 + k, y1 = lantern.y + 3 + k;
    const tiles = [];
    for (let i = x0; i <= x1; i++) { tiles.push([i, y0]); tiles.push([i, y1]); }
    for (let j = y0 + 1; j < y1; j++) { tiles.push([x0, j]); tiles.push([x1, j]); }
    return tiles;
  };
  const tryRing = (k) => {
    const all = ring(k);
    if (!all.every(([i, j]) => isPointLit(i + 0.5, j + 0.5, sources))) return false;
    const tiles = all.filter(([i, j]) => free(i, j, 1, 0));
    if (tiles.length === 0 || tiles.length > walls) return false;
    for (const [i, j] of tiles) put('wall', i, j, wallLevel);
    walls -= tiles.length;
    return true;
  };
  const rings = [];
  if (walls >= 100) { if (tryRing(3)) rings.push(3); }
  for (let k = 9; k >= 1; k--) {
    if (rings.some((r) => Math.abs(r - k) < 3)) continue;
    if (tryRing(k)) { rings.push(k); break; }
  }

  // Остальные постройки.
  const order = [
    ['oreStore', 'core'], ['aetherStore', 'core'],
    ['prism', 'def'], ['catapult', 'def'], ['brazier', 'def'], ['ballista', 'def'],
    ['barracks', 'out'], ['camp', 'out'], ['oreMine', 'out'], ['aetherWell', 'out'], ['builderHut', 'out'],
  ];
  const placedByType = {};
  for (const [type, role] of order) {
    const n = count(type);
    for (let k = 0; k < n; k++) {
      const s = BUILDINGS[type].size;
      let best = null, bestScore = Infinity;
      for (let c = 0; c < 260; c++) {
        const x = Math.floor(BORDER + rng() * (GRID - 2 * BORDER - s));
        const y = Math.floor(BORDER + rng() * (GRID - 2 * BORDER - s));
        if (!free(x, y, s, 1) || !footprintLit(x, y, s, sources)) continue;
        const d = Math.hypot(x + s / 2 - C, y + s / 2 - C);
        let score;
        if (role === 'core') score = d;
        else if (role === 'def') score = Math.abs(d - (3 + ((k * 2.7) % (lr + 1)))) * 0.7;
        else score = -d * 0.6;
        for (const o of placedByType[type] || []) {
          const od = Math.hypot(o.x - x, o.y - y);
          if (od < 7) score += (7 - od) * 1.2;
        }
        score += rng() * 2.5;
        if (score < bestScore) { bestScore = score; best = [x, y]; }
      }
      if (best) {
        const it = put(type, best[0], best[1], type === 'builderHut' ? 1 : lvl(type));
        (placedByType[type] = placedByType[type] || []).push(it);
      }
    }
  }

  // Добыча.
  const pool = BASE_LOOT[LL - 1] * richness;
  const share = (items, frac) => {
    if (!items.length) return;
    for (const it of items) it.lootShare = (it.lootShare || 0) + frac / items.length;
  };
  const by = (t) => layout.filter((b) => b.type === t);
  share(by('oreStore'), 0.6); share(by('oreMine'), 0.25);
  for (const b of layout) if (b.lootShare) b.loot = { ore: pool * b.lootShare }, delete b.lootShare;
  share(by('aetherStore'), 0.6); share(by('aetherWell'), 0.25);
  for (const b of layout) if (b.lootShare) b.loot = { aether: pool * b.lootShare }, delete b.lootShare;
  lantern.loot = { ore: pool * 0.15, aether: pool * 0.15 };
  return layout;
}

// Ночь: состав волн по уровню Фонаря и числу пережитых ночей.
export function nightWaves(LL, nights, seed = 1) {
  const rng = mulberry32(seed);
  const budget = 10 + 14 * (LL - 1) + 2 * Math.min(nights, 20);
  const types = Object.keys(HAZE).filter((t) => HAZE[t].unlockLL <= LL);
  const waves = [{ t: 2, units: {} }, { t: 22, units: {} }, { t: 45, units: {} }];
  const shares = [0.3, 0.35, 0.35];
  waves.forEach((w, i) => {
    let left = Math.round(budget * shares[i]);
    while (left > 0) {
      const opts = types.filter((t) => HAZE[t].housing <= left);
      if (!opts.length) break;
      const t = rng() < 0.45 ? 'morok' : opts[Math.floor(rng() * opts.length)];
      if (HAZE[t].housing > left) break;
      w.units[t] = (w.units[t] || 0) + 1;
      left -= HAZE[t].housing;
    }
  });
  return { waves, hazeMult: 1 + 0.05 * Math.min(nights, 30) };
}
