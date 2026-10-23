import { GRID, BUILDINGS } from './config.js';

export function mulberry32(seed) {
  let a = seed >>> 0;
  return function () {
    a = (a + 0x6D2B79F5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function sizeOf(type) { return BUILDINGS[type].size; }

export function centerOf(b) {
  const s = sizeOf(b.type);
  return { x: b.x + s / 2, y: b.y + s / 2 };
}

// Евклидово расстояние от точки до прямоугольника постройки.
export function distToRect(px, py, x, y, size) {
  const dx = Math.max(x - px, 0, px - (x + size));
  const dy = Math.max(y - py, 0, py - (y + size));
  return Math.hypot(dx, dy);
}

export function lightRadius(b) {
  const def = BUILDINGS[b.type];
  if (!def.isLight || !b.level) return 0;
  return def.light[b.level - 1];
}

// Источники света: [{x, y, r}] в координатах клеток.
export function lightSources(buildings, excludeId = null) {
  const out = [];
  for (const b of buildings) {
    if (b.id === excludeId || b.dead) continue;
    const r = lightRadius(b);
    if (r > 0) { const c = centerOf(b); out.push({ x: c.x, y: c.y, r, id: b.id }); }
  }
  return out;
}

export function isPointLit(px, py, sources, eclipses = []) {
  for (const e of eclipses) if (Math.hypot(px - e.x, py - e.y) <= e.r) return false;
  for (const s of sources) if (Math.hypot(px - s.x, py - s.y) <= s.r) return true;
  return false;
}

export function computeLitGrid(sources, eclipses = []) {
  const g = new Uint8Array(GRID * GRID);
  for (let j = 0; j < GRID; j++) {
    for (let i = 0; i < GRID; i++) {
      if (isPointLit(i + 0.5, j + 0.5, sources, eclipses)) g[j * GRID + i] = 1;
    }
  }
  return g;
}

export function footprintLit(x, y, size, sources) {
  for (let j = y; j < y + size; j++)
    for (let i = x; i < x + size; i++)
      if (!isPointLit(i + 0.5, j + 0.5, sources)) return false;
  return true;
}

export function rectsOverlap(ax, ay, as, bx, by, bs) {
  return ax < bx + bs && bx < ax + as && ay < by + bs && by < ay + as;
}
