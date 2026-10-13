// A* по сетке с диагоналями. cost(i) -> Infinity для непроходимых клеток.
import { GRID } from './config.js';

const DIRS = [
  [1, 0, 1], [-1, 0, 1], [0, 1, 1], [0, -1, 1],
  [1, 1, Math.SQRT2], [1, -1, Math.SQRT2], [-1, 1, Math.SQRT2], [-1, -1, Math.SQRT2],
];

class Heap {
  constructor() { this.a = []; }
  get size() { return this.a.length; }
  push(n) {
    const a = this.a; a.push(n);
    let i = a.length - 1;
    while (i > 0) {
      const p = (i - 1) >> 1;
      if (a[p].f <= a[i].f) break;
      [a[p], a[i]] = [a[i], a[p]]; i = p;
    }
  }
  pop() {
    const a = this.a; const top = a[0]; const last = a.pop();
    if (a.length) {
      a[0] = last; let i = 0;
      for (;;) {
        const l = 2 * i + 1, r = l + 1; let m = i;
        if (l < a.length && a[l].f < a[m].f) m = l;
        if (r < a.length && a[r].f < a[m].f) m = r;
        if (m === i) break;
        [a[m], a[i]] = [a[i], a[m]]; i = m;
      }
    }
    return top;
  }
}

/**
 * @param sx,sy стартовая клетка
 * @param goal {x,y,size} прямоугольник цели — любая его клетка считается достигнутой
 * @param cost (idx) => стоимость входа в клетку (Infinity = стена/здание)
 * @returns массив индексов клеток (без стартовой) или null
 */
export function findPath(sx, sy, goal, cost, maxIter = 6000) {
  const N = GRID * GRID;
  const g = new Float32Array(N).fill(Infinity);
  const from = new Int32Array(N).fill(-1);
  const closed = new Uint8Array(N);
  const inGoal = (x, y) => x >= goal.x && x < goal.x + goal.size && y >= goal.y && y < goal.y + goal.size;
  const h = (x, y) => {
    const dx = Math.max(goal.x - x, 0, x - (goal.x + goal.size - 1));
    const dy = Math.max(goal.y - y, 0, y - (goal.y + goal.size - 1));
    return Math.max(dx, dy) + (Math.SQRT2 - 1) * Math.min(dx, dy);
  };
  sx = Math.max(0, Math.min(GRID - 1, sx));
  sy = Math.max(0, Math.min(GRID - 1, sy));
  const start = sy * GRID + sx;
  g[start] = 0;
  const open = new Heap();
  open.push({ i: start, f: h(sx, sy) });
  let iter = 0;
  while (open.size && iter++ < maxIter) {
    const { i } = open.pop();
    if (closed[i]) continue;
    closed[i] = 1;
    const x = i % GRID, y = (i / GRID) | 0;
    if (inGoal(x, y)) {
      const path = [];
      let c = i;
      while (c !== start) { path.push(c); c = from[c]; }
      return path.reverse();
    }
    for (const [dx, dy, dc] of DIRS) {
      const nx = x + dx, ny = y + dy;
      if (nx < 0 || ny < 0 || nx >= GRID || ny >= GRID) continue;
      const ni = ny * GRID + nx;
      if (closed[ni]) continue;
      const goalCell = inGoal(nx, ny);
      const c = goalCell ? 1 : cost(ni);
      if (c === Infinity) continue;
      if (dx && dy) {
        // не срезаем углы через препятствия
        const a = y * GRID + nx, b = ny * GRID + x;
        if ((!inGoal(nx, y) && cost(a) > 1) || (!inGoal(x, ny) && cost(b) > 1)) continue;
      }
      const ng = g[i] + dc * c;
      if (ng < g[ni]) {
        g[ni] = ng; from[ni] = i;
        open.push({ i: ni, f: ng + h(nx, ny) });
      }
    }
  }
  return null;
}
