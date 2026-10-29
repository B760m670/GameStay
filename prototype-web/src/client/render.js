// Процедурная изометрическая графика: земля, туман, постройки, войска, эффекты.
import { GRID, BORDER, BUILDINGS } from '../core/config.js';
import { mulberry32 } from '../core/grid.js';
import { TW, TH } from './camera.js';

const SQ2 = Math.SQRT2;

// ---------- Земля ----------
let groundCache = null;
export function getGround() {
  if (groundCache) return groundCache;
  const c = document.createElement('canvas');
  c.width = GRID * TW; c.height = GRID * TH;
  const g = c.getContext('2d');
  const rng = mulberry32(42);
  const ox = (GRID * TW) / 2;
  for (let j = 0; j < GRID; j++) {
    for (let i = 0; i < GRID; i++) {
      const x = ox + (i - j) * (TW / 2), y = (i + j) * (TH / 2);
      const edge = i < BORDER || j < BORDER || i >= GRID - BORDER || j >= GRID - BORDER;
      const n = rng();
      const checker = (i + j) % 2 ? 0 : 2;
      const col = edge
        ? `hsl(${255 + n * 10}, 14%, ${13 + n * 3}%)`
        : `hsl(${105 + n * 25}, ${24 + n * 8}%, ${23 + checker + n * 4}%)`;
      g.beginPath();
      g.moveTo(x, y); g.lineTo(x + TW / 2, y + TH / 2); g.lineTo(x, y + TH); g.lineTo(x - TW / 2, y + TH / 2);
      g.closePath();
      g.fillStyle = col; g.fill();
      if (!edge && rng() < 0.18) {
        // травинки и камушки
        g.fillStyle = rng() < 0.5 ? 'rgba(160,200,120,0.35)' : 'rgba(30,40,30,0.35)';
        for (let k = 0; k < 3; k++) {
          const px = x + (rng() - 0.5) * TW * 0.5, py = y + TH / 2 + (rng() - 0.5) * TH * 0.5;
          g.fillRect(px, py, 2, 3);
        }
      }
      if (!edge && rng() < 0.03) {
        g.fillStyle = 'rgba(120,120,130,0.6)';
        g.beginPath(); g.ellipse(x + (rng() - 0.5) * 20, y + TH / 2, 4, 2, 0, 0, 7); g.fill();
      }
    }
  }
  groundCache = c;
  return c;
}

export function drawBackground(ctx, w, h, t) {
  const grd = ctx.createLinearGradient(0, 0, 0, h);
  grd.addColorStop(0, '#0d0a1c');
  grd.addColorStop(1, '#1a1330');
  ctx.fillStyle = grd;
  ctx.fillRect(0, 0, w, h);
}

export function drawGround(ctx, cam) {
  const g = getGround();
  const z = cam.zoom;
  ctx.imageSmoothingEnabled = true;
  ctx.drawImage(g, cam.ox - (GRID * TW * z) / 2, cam.oy, GRID * TW * z, GRID * TH * z);
}

// ---------- Туман и свет ----------
let fogCanvas = null;
export function drawFog(ctx, cam, sources, eclipses, t, opts = {}) {
  const w = ctx.canvas.width, h = ctx.canvas.height;
  if (!fogCanvas) fogCanvas = document.createElement('canvas');
  if (fogCanvas.width !== w || fogCanvas.height !== h) { fogCanvas.width = w; fogCanvas.height = h; }
  const f = fogCanvas.getContext('2d');
  const dpr = opts.dpr || 1;
  f.setTransform(1, 0, 0, 1, 0, 0);
  f.globalCompositeOperation = 'source-over';
  f.clearRect(0, 0, w, h);
  f.fillStyle = opts.fogColor || 'rgba(14, 9, 30, 0.74)';
  f.fillRect(0, 0, w, h);
  f.setTransform(dpr, 0, 0, dpr, 0, 0);
  f.globalCompositeOperation = 'destination-out';
  for (const s of sources) {
    const p = cam.toScreen(s.x, s.y);
    const flick = 1 + Math.sin(t * 3 + s.x) * 0.012;
    const rx = (s.r * TW * cam.zoom * flick) / SQ2;
    f.save();
    f.translate(p.x, p.y);
    f.scale(1, 0.5);
    const grd = f.createRadialGradient(0, 0, rx * 0.2, 0, 0, rx * 1.04);
    grd.addColorStop(0, 'rgba(0,0,0,1)');
    grd.addColorStop(0.86, 'rgba(0,0,0,0.95)');
    grd.addColorStop(1, 'rgba(0,0,0,0)');
    f.fillStyle = grd;
    f.beginPath(); f.arc(0, 0, rx * 1.04, 0, Math.PI * 2); f.fill();
    f.restore();
  }
  f.globalCompositeOperation = 'source-over';
  for (const e of eclipses || []) {
    const p = cam.toScreen(e.x, e.y);
    const rx = (e.r * TW * cam.zoom) / SQ2;
    f.save(); f.translate(p.x, p.y); f.scale(1, 0.5);
    const grd = f.createRadialGradient(0, 0, 0, 0, 0, rx);
    grd.addColorStop(0, 'rgba(20,0,40,0.85)');
    grd.addColorStop(0.9, 'rgba(30,5,60,0.75)');
    grd.addColorStop(1, 'rgba(60,20,110,0)');
    f.fillStyle = grd;
    f.beginPath(); f.arc(0, 0, rx, 0, Math.PI * 2); f.fill();
    f.restore();
  }
  ctx.save();
  ctx.setTransform(1, 0, 0, 1, 0, 0);
  ctx.drawImage(fogCanvas, 0, 0);
  ctx.restore();
  // тёплое свечение
  ctx.save();
  ctx.globalCompositeOperation = 'lighter';
  for (const s of sources) {
    const p = cam.toScreen(s.x, s.y);
    const rx = (s.r * TW * cam.zoom) / SQ2;
    ctx.save(); ctx.translate(p.x, p.y); ctx.scale(1, 0.5);
    const grd = ctx.createRadialGradient(0, 0, 0, 0, 0, rx);
    grd.addColorStop(0, 'rgba(255,190,90,0.12)');
    grd.addColorStop(1, 'rgba(255,170,60,0)');
    ctx.fillStyle = grd;
    ctx.beginPath(); ctx.arc(0, 0, rx, 0, Math.PI * 2); ctx.fill();
    ctx.restore();
  }
  ctx.restore();
  if (opts.forbidTint) {
    ctx.save();
    ctx.globalAlpha = 0.16;
    ctx.fillStyle = '#ff3a3a';
    ctx.beginPath();
    for (const s of sources) {
      const p = cam.toScreen(s.x, s.y);
      const rx = (s.r * TW * cam.zoom) / SQ2;
      ctx.moveTo(p.x + rx, p.y);
      ctx.ellipse(p.x, p.y, rx, rx / 2, 0, 0, Math.PI * 2);
    }
    ctx.fill('nonzero');
    ctx.restore();
  }
}

// ---------- Примитивы ----------
function poly(ctx, pts, fill, stroke) {
  ctx.beginPath();
  ctx.moveTo(pts[0].x, pts[0].y);
  for (let i = 1; i < pts.length; i++) ctx.lineTo(pts[i].x, pts[i].y);
  ctx.closePath();
  if (fill) { ctx.fillStyle = fill; ctx.fill(); }
  if (stroke) { ctx.strokeStyle = stroke; ctx.stroke(); }
}

function shade(hex, k) {
  const n = parseInt(hex.slice(1), 16);
  let r = (n >> 16) & 255, g = (n >> 8) & 255, b = n & 255;
  r = Math.max(0, Math.min(255, Math.round(r * k)));
  g = Math.max(0, Math.min(255, Math.round(g * k)));
  b = Math.max(0, Math.min(255, Math.round(b * k)));
  return `rgb(${r},${g},${b})`;
}

export function isoBox(ctx, cam, x, y, w, d, z0, h, color) {
  const P = (a, b, z) => cam.toScreen(a, b, z);
  const t = [P(x, y, z0 + h), P(x + w, y, z0 + h), P(x + w, y + d, z0 + h), P(x, y + d, z0 + h)];
  const bL = P(x, y + d, z0), bB = P(x + w, y + d, z0), bR = P(x + w, y, z0);
  ctx.lineWidth = Math.max(0.5, cam.zoom);
  poly(ctx, [bL, bB, t[2], t[3]], shade(color, 0.72), 'rgba(0,0,0,0.25)');
  poly(ctx, [bB, bR, t[1], t[2]], shade(color, 0.52), 'rgba(0,0,0,0.25)');
  poly(ctx, t, color, 'rgba(0,0,0,0.2)');
}

export function isoPyramid(ctx, cam, x, y, w, d, z0, h, color) {
  const P = (a, b, z) => cam.toScreen(a, b, z);
  const apex = P(x + w / 2, y + d / 2, z0 + h);
  const L = P(x, y + d, z0), Bt = P(x + w, y + d, z0), R = P(x + w, y, z0), T = P(x, y, z0);
  ctx.lineWidth = Math.max(0.5, cam.zoom);
  poly(ctx, [T, L, apex], shade(color, 1.1), 'rgba(0,0,0,0.2)');
  poly(ctx, [T, R, apex], shade(color, 0.95), 'rgba(0,0,0,0.2)');
  poly(ctx, [L, Bt, apex], shade(color, 0.8), 'rgba(0,0,0,0.25)');
  poly(ctx, [Bt, R, apex], shade(color, 0.6), 'rgba(0,0,0,0.25)');
}

export function isoCylinder(ctx, cam, cx, cy, r, z0, h, color, topColor) {
  const p0 = cam.toScreen(cx, cy, z0), p1 = cam.toScreen(cx, cy, z0 + h);
  const rx = (r * TW * cam.zoom) / SQ2, ry = rx / 2;
  const grd = ctx.createLinearGradient(p0.x - rx, 0, p0.x + rx, 0);
  grd.addColorStop(0, shade(color, 0.85));
  grd.addColorStop(0.5, shade(color, 0.7));
  grd.addColorStop(1, shade(color, 0.45));
  ctx.beginPath();
  ctx.moveTo(p0.x - rx, p1.y);
  ctx.lineTo(p0.x - rx, p0.y);
  ctx.ellipse(p0.x, p0.y, rx, ry, 0, Math.PI, 0, true);
  ctx.lineTo(p0.x + rx, p1.y);
  ctx.closePath();
  ctx.fillStyle = grd; ctx.fill();
  ctx.strokeStyle = 'rgba(0,0,0,0.25)'; ctx.lineWidth = Math.max(0.5, cam.zoom); ctx.stroke();
  ctx.beginPath(); ctx.ellipse(p1.x, p1.y, rx, ry, 0, 0, Math.PI * 2);
  ctx.fillStyle = topColor || color; ctx.fill(); ctx.stroke();
}

export function glow(ctx, x, y, r, color, alpha = 1) {
  ctx.save();
  ctx.globalCompositeOperation = 'lighter';
  ctx.globalAlpha = alpha;
  const g = ctx.createRadialGradient(x, y, 0, x, y, r);
  g.addColorStop(0, color);
  g.addColorStop(1, 'rgba(0,0,0,0)');
  ctx.fillStyle = g;
  ctx.beginPath(); ctx.arc(x, y, r, 0, Math.PI * 2); ctx.fill();
  ctx.restore();
}

export function footprint(ctx, cam, x, y, s, fill, stroke) {
  const pts = [cam.toScreen(x, y), cam.toScreen(x + s, y), cam.toScreen(x + s, y + s), cam.toScreen(x, y + s)];
  ctx.lineWidth = 2;
  poly(ctx, pts, fill, stroke);
}

function flame(ctx, x, y, size, t, seed = 0) {
  const f = 1 + Math.sin(t * 12 + seed) * 0.12 + Math.sin(t * 7.3 + seed * 2) * 0.08;
  glow(ctx, x, y, size * 3.2 * f, 'rgba(255,170,60,0.55)');
  ctx.beginPath();
  ctx.moveTo(x - size * 0.6, y);
  ctx.quadraticCurveTo(x - size * 0.5, y - size * 1.2 * f, x, y - size * 2 * f);
  ctx.quadraticCurveTo(x + size * 0.5, y - size * 1.2 * f, x + size * 0.6, y);
  ctx.closePath();
  ctx.fillStyle = '#ffb347'; ctx.fill();
  ctx.beginPath();
  ctx.moveTo(x - size * 0.3, y);
  ctx.quadraticCurveTo(x, y - size * 1.3 * f, x + size * 0.3, y);
  ctx.fillStyle = '#fff2b0'; ctx.fill();
}

// Направление в мире -> экранный единичный вектор.
function screenDir(a) {
  const dx = (Math.cos(a) - Math.sin(a)) * (TW / 2), dy = (Math.cos(a) + Math.sin(a)) * (TH / 2);
  const l = Math.hypot(dx, dy) || 1;
  return { x: dx / l, y: dy / l };
}

const WALL_COLORS = ['#8a6a48', '#8d8d96', '#6a6a80', '#5a6470', '#b8a060', '#7ab8d0'];

// ---------- Постройки ----------
export function drawBuilding(ctx, cam, b, t, opt = {}) {
  const def = BUILDINGS[b.type];
  const s = def.size;
  const { x, y } = b;
  const z = cam.zoom;
  ctx.save();
  if (opt.alpha != null) ctx.globalAlpha = opt.alpha;
  if (opt.ghost) footprint(ctx, cam, x, y, s, opt.ghost === 'ok' ? 'rgba(80,255,120,0.25)' : 'rgba(255,60,60,0.3)', opt.ghost === 'ok' ? '#6f6' : '#f55');
  if (opt.selected) footprint(ctx, cam, x, y, s, 'rgba(255,230,120,0.12)', 'rgba(255,230,120,0.9)');

  if (b.dead) { drawRubble(ctx, cam, b, s); ctx.restore(); return; }
  if (b.level === 0) { drawConstruction(ctx, cam, b, s, t); ctx.restore(); return; }

  // тень
  const c = cam.toScreen(x + s / 2, y + s / 2);
  ctx.fillStyle = 'rgba(0,0,0,0.25)';
  ctx.beginPath(); ctx.ellipse(c.x, c.y, (s * TW * z) / 2.6, (s * TH * z) / 2.6, 0, 0, 7); ctx.fill();

  const lv = b.level;
  switch (b.type) {
    case 'lantern': {
      isoBox(ctx, cam, x + 0.3, y + 0.3, s - 0.6, s - 0.6, 0, 16, '#6e6a80');
      isoBox(ctx, cam, x + 0.9, y + 0.9, s - 1.8, s - 1.8, 16, 14, '#8a85a0');
      isoBox(ctx, cam, x + 1.3, y + 1.3, s - 2.6, s - 2.6, 30, 30 + lv * 3, '#9b96b0');
      const top = 60 + lv * 3;
      isoBox(ctx, cam, x + 1.15, y + 1.15, s - 2.3, s - 2.3, top, 4, '#c7a35a');
      const p = cam.toScreen(x + s / 2, y + s / 2, top + 8);
      flame(ctx, p.x, p.y + 8 * z, 9 * z, t);
      isoPyramid(ctx, cam, x + 1.15, y + 1.15, s - 2.3, s - 2.3, top + 22, 18, '#3e3a55');
      for (let k = 0; k < lv; k++) {
        const q = cam.toScreen(x + 0.45 + k * 0.5, y + s - 0.35, 6);
        ctx.fillStyle = '#ffd86b'; ctx.fillRect(q.x - 2 * z, q.y - 2 * z, 4 * z, 4 * z);
      }
      break;
    }
    case 'beacon': {
      isoBox(ctx, cam, x + 0.35, y + 0.35, s - 0.7, s - 0.7, 0, 8, '#5d5a70');
      isoBox(ctx, cam, x + 0.7, y + 0.7, s - 1.4, s - 1.4, 8, 30 + lv * 3, '#8d8aa8');
      const h = 44 + lv * 3;
      const p = cam.toScreen(x + s / 2, y + s / 2, h);
      const pulse = 1 + Math.sin(t * 2 + x) * 0.1;
      glow(ctx, p.x, p.y, 26 * z * pulse, 'rgba(160,230,255,0.7)');
      ctx.beginPath();
      ctx.moveTo(p.x, p.y - 12 * z); ctx.lineTo(p.x + 6 * z, p.y); ctx.lineTo(p.x, p.y + 10 * z); ctx.lineTo(p.x - 6 * z, p.y);
      ctx.closePath(); ctx.fillStyle = '#d8f6ff'; ctx.fill(); ctx.strokeStyle = '#7ad'; ctx.stroke();
      break;
    }
    case 'oreMine': {
      isoBox(ctx, cam, x + 0.2, y + 0.2, s - 0.4, s - 0.4, 0, 10, '#6b5038');
      for (let k = 0; k < 3; k++) {
        const p = cam.toScreen(x + 0.8 + k * 0.6, y + s - 0.8, 10);
        ctx.beginPath(); ctx.moveTo(p.x - 7 * z, p.y); ctx.lineTo(p.x, p.y - 9 * z); ctx.lineTo(p.x + 7 * z, p.y); ctx.closePath();
        ctx.fillStyle = '#e8954a'; ctx.fill(); ctx.strokeStyle = '#7a4a20'; ctx.stroke();
      }
      const a = cam.toScreen(x + 0.6, y + 0.6, 10), b2 = cam.toScreen(x + s - 0.6, y + 0.6, 10), top = cam.toScreen(x + s / 2, y + 0.6, 44);
      ctx.strokeStyle = '#a07850'; ctx.lineWidth = 3 * z;
      ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(top.x, top.y); ctx.lineTo(b2.x, b2.y); ctx.stroke();
      ctx.beginPath(); ctx.arc(top.x, top.y + 4 * z, 7 * z, 0, 7); ctx.strokeStyle = '#c8a070'; ctx.lineWidth = 2 * z; ctx.stroke();
      const ang = t * 2;
      ctx.beginPath(); ctx.moveTo(top.x, top.y + 4 * z); ctx.lineTo(top.x + Math.cos(ang) * 7 * z, top.y + 4 * z + Math.sin(ang) * 7 * z); ctx.stroke();
      break;
    }
    case 'aetherWell': {
      isoCylinder(ctx, cam, x + s / 2, y + s / 2, 1.1, 0, 14, '#77738a', '#2a5a70');
      const p = cam.toScreen(x + s / 2, y + s / 2, 14);
      glow(ctx, p.x, p.y, 30 * z, 'rgba(120,220,255,0.6)', 0.8 + Math.sin(t * 2.5) * 0.2);
      for (let k = 0; k < 3; k++) {
        const ph = (t * 0.7 + k / 3) % 1;
        ctx.fillStyle = `rgba(190,240,255,${1 - ph})`;
        ctx.beginPath(); ctx.arc(p.x + (k - 1) * 8 * z, p.y - ph * 30 * z, 2.5 * z, 0, 7); ctx.fill();
      }
      break;
    }
    case 'oreStore': {
      isoBox(ctx, cam, x + 0.3, y + 0.3, s - 0.6, s - 0.6, 0, 22 + lv * 2, '#7a5a3a');
      const h = 22 + lv * 2;
      for (let k = 1; k <= 2; k++) {
        const a = cam.toScreen(x + 0.3, y + s - 0.3, (h * k) / 3), b2 = cam.toScreen(x + s - 0.3, y + s - 0.3, (h * k) / 3), c2 = cam.toScreen(x + s - 0.3, y + 0.3, (h * k) / 3);
        ctx.strokeStyle = '#3a3030'; ctx.lineWidth = 2 * z;
        ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(b2.x, b2.y); ctx.lineTo(c2.x, c2.y); ctx.stroke();
      }
      isoPyramid(ctx, cam, x + 0.6, y + 0.6, s - 1.2, s - 1.2, h, 10 + (opt.fill || 0.5) * 14, '#e8954a');
      break;
    }
    case 'aetherStore': {
      isoCylinder(ctx, cam, x + s / 2, y + s / 2, 1.15, 0, 6, '#5d5a70', '#4d4a60');
      isoCylinder(ctx, cam, x + s / 2, y + s / 2, 0.95, 6, 28 + lv * 2, '#5ab8d8', '#a8ecff');
      const p = cam.toScreen(x + s / 2, y + s / 2, 20);
      glow(ctx, p.x, p.y, 28 * z, 'rgba(120,220,255,0.35)');
      isoBox(ctx, cam, x + 0.95, y + 0.95, s - 1.9, s - 1.9, 34 + lv * 2, 4, '#c7a35a');
      break;
    }
    case 'barracks': {
      isoBox(ctx, cam, x + 0.25, y + 0.25, s - 0.5, s - 0.5, 0, 20, '#7b7280');
      isoPyramid(ctx, cam, x + 0.15, y + 0.15, s - 0.3, s - 0.3, 20, 18 + lv, '#a8443a');
      const p = cam.toScreen(x + s - 0.4, y + s - 0.4, 20);
      ctx.strokeStyle = '#ddd'; ctx.lineWidth = 2 * z;
      ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(p.x, p.y - 30 * z); ctx.stroke();
      ctx.fillStyle = '#e8a04a';
      ctx.beginPath(); ctx.moveTo(p.x, p.y - 30 * z); ctx.lineTo(p.x + 12 * z, p.y - 25 * z + Math.sin(t * 4) * 2 * z); ctx.lineTo(p.x, p.y - 20 * z); ctx.fill();
      break;
    }
    case 'camp': {
      const cc = cam.toScreen(x + s / 2, y + s / 2);
      ctx.fillStyle = 'rgba(90,70,50,0.6)';
      ctx.beginPath(); ctx.ellipse(cc.x, cc.y, (s * TW * z) / 2.4, (s * TH * z) / 2.4, 0, 0, 7); ctx.fill();
      isoPyramid(ctx, cam, x + 0.2, y + 0.2, 1.2, 1.2, 0, 22, '#8d7a5a');
      isoPyramid(ctx, cam, x + s - 1.4, y + 0.3, 1.2, 1.2, 0, 22, '#7a8d5a');
      isoPyramid(ctx, cam, x + 0.3, y + s - 1.4, 1.2, 1.2, 0, 22, '#8d5a5a');
      flame(ctx, cc.x + 6 * z, cc.y + 6 * z, 5 * z, t, x);
      break;
    }
    case 'builderHut': {
      isoBox(ctx, cam, x + 0.3, y + 0.3, s - 0.6, s - 0.6, 0, 12, '#9a8060');
      isoPyramid(ctx, cam, x + 0.2, y + 0.2, s - 0.4, s - 0.4, 12, 14, '#c89a40');
      break;
    }
    case 'ballista': {
      isoBox(ctx, cam, x + 0.4, y + 0.4, s - 0.8, s - 0.8, 0, 20 + lv * 2, '#7c7890');
      isoBox(ctx, cam, x + 0.3, y + 0.3, s - 0.6, s - 0.6, 20 + lv * 2, 4, '#625e75');
      const p = cam.toScreen(x + s / 2, y + s / 2, 30 + lv * 2);
      const d = screenDir(b.angle ?? 0.8);
      const L = 20 * z;
      ctx.strokeStyle = '#5a3a20'; ctx.lineWidth = 4 * z;
      ctx.beginPath(); ctx.moveTo(p.x - d.x * L * 0.6, p.y - d.y * L * 0.6); ctx.lineTo(p.x + d.x * L, p.y + d.y * L); ctx.stroke();
      ctx.strokeStyle = '#c8b090'; ctx.lineWidth = 3 * z;
      ctx.beginPath();
      ctx.moveTo(p.x + d.x * L * 0.5 - d.y * L * 0.8, p.y + d.y * L * 0.5 + d.x * L * 0.8);
      ctx.quadraticCurveTo(p.x + d.x * L * 1.0, p.y + d.y * L * 1.0, p.x + d.x * L * 0.5 + d.y * L * 0.8, p.y + d.y * L * 0.5 - d.x * L * 0.8);
      ctx.stroke();
      break;
    }
    case 'brazier': {
      isoBox(ctx, cam, x + 0.3, y + 0.3, s - 0.6, s - 0.6, 0, 10, '#5a5060');
      isoCylinder(ctx, cam, x + s / 2, y + s / 2, 0.65, 10, 10, '#3a3238', '#1a1010');
      const p = cam.toScreen(x + s / 2, y + s / 2, 20);
      if (b.powered !== false) flame(ctx, p.x, p.y, (6 + lv) * z, t, x * 3);
      break;
    }
    case 'catapult': {
      isoBox(ctx, cam, x + 0.3, y + 0.3, s - 0.6, s - 0.6, 0, 10, '#6a5040');
      const p = cam.toScreen(x + s / 2, y + s / 2, 14);
      const d = screenDir(b.angle ?? 0.8);
      const swing = b.cd > 0 && b.def ? Math.min(1, b.cd / b.def.cd) : 0;
      const L = 30 * z;
      const ex = p.x - d.x * L * (0.6 - swing), ey = p.y - d.y * L * 0.3 - L * (0.7 + swing * 0.2);
      ctx.strokeStyle = '#8a6040'; ctx.lineWidth = 4 * z;
      ctx.beginPath(); ctx.moveTo(p.x, p.y); ctx.lineTo(ex, ey); ctx.stroke();
      ctx.fillStyle = '#888';
      ctx.beginPath(); ctx.arc(ex, ey, 5 * z, 0, 7); ctx.fill();
      break;
    }
    case 'prism': {
      isoBox(ctx, cam, x + 0.3, y + 0.3, s - 0.6, s - 0.6, 0, 14, '#4e4a66');
      const p = cam.toScreen(x + s / 2, y + s / 2, 40 + Math.sin(t * 2) * 3);
      glow(ctx, p.x, p.y, 30 * z, 'rgba(255,120,220,0.5)');
      const r = 10 * z;
      ctx.beginPath(); ctx.moveTo(p.x, p.y - r * 1.6); ctx.lineTo(p.x + r, p.y); ctx.lineTo(p.x, p.y + r * 1.6); ctx.lineTo(p.x - r, p.y); ctx.closePath();
      const g = ctx.createLinearGradient(p.x - r, p.y, p.x + r, p.y);
      g.addColorStop(0, '#ffd0f4'); g.addColorStop(1, '#b050d0');
      ctx.fillStyle = g; ctx.fill(); ctx.strokeStyle = '#fff'; ctx.lineWidth = 1; ctx.stroke();
      break;
    }
    case 'wall': {
      isoBox(ctx, cam, x + 0.12, y + 0.12, 0.76, 0.76, 0, 14 + lv, WALL_COLORS[lv - 1] || '#888');
      break;
    }
  }

  if (b.hitT > 0) {
    const p = cam.toScreen(x + s / 2, y + s / 2, 20);
    glow(ctx, p.x, p.y, s * 14 * z, 'rgba(255,255,255,0.4)');
  }
  if (b.powered === false && !b.dead) {
    const p = cam.toScreen(x + s / 2, y + s / 2, 50);
    ctx.fillStyle = 'rgba(20,10,40,0.85)';
    ctx.beginPath(); ctx.arc(p.x, p.y, 9 * z + 3, 0, 7); ctx.fill();
    ctx.strokeStyle = '#b59ae0'; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.arc(p.x, p.y, 5 * z + 1, 0, 7); ctx.stroke();
    ctx.beginPath(); ctx.moveTo(p.x - 7 * z - 2, p.y + 7 * z + 2); ctx.lineTo(p.x + 7 * z + 2, p.y - 7 * z - 2); ctx.stroke();
  }
  ctx.restore();
}

function drawRubble(ctx, cam, b, s) {
  const rng = mulberry32(b.x * 131 + b.y * 7 + 3);
  if (b.type === 'wall') {
    isoBox(ctx, cam, b.x + 0.3, b.y + 0.3, 0.4, 0.4, 0, 3, '#4a4550');
    return;
  }
  const c = cam.toScreen(b.x + s / 2, b.y + s / 2);
  ctx.fillStyle = 'rgba(20,15,20,0.55)';
  ctx.beginPath(); ctx.ellipse(c.x, c.y, (s * TW * cam.zoom) / 2.4, (s * TH * cam.zoom) / 2.4, 0, 0, 7); ctx.fill();
  for (let k = 0; k < s * 3; k++) {
    const px = b.x + 0.3 + rng() * (s - 0.8), py = b.y + 0.3 + rng() * (s - 0.8);
    isoBox(ctx, cam, px, py, 0.35, 0.35, 0, 3 + rng() * 6, rng() < 0.5 ? '#5a5560' : '#4a3a30');
  }
}

function drawConstruction(ctx, cam, b, s, t) {
  footprint(ctx, cam, b.x + 0.15, b.y + 0.15, s - 0.3, 'rgba(120,90,60,0.6)', 'rgba(60,40,20,0.8)');
  const h = 26;
  ctx.strokeStyle = '#c8a070'; ctx.lineWidth = 2 * cam.zoom;
  const corners = [[0.25, 0.25], [s - 0.25, 0.25], [s - 0.25, s - 0.25], [0.25, s - 0.25]];
  for (const [cx, cy] of corners) {
    const a = cam.toScreen(b.x + cx, b.y + cy, 0), c = cam.toScreen(b.x + cx, b.y + cy, h);
    ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(c.x, c.y); ctx.stroke();
  }
  const top = corners.map(([cx, cy]) => cam.toScreen(b.x + cx, b.y + cy, h));
  ctx.beginPath(); ctx.moveTo(top[0].x, top[0].y);
  for (const p of top) ctx.lineTo(p.x, p.y);
  ctx.closePath(); ctx.stroke();
  const p = cam.toScreen(b.x + s / 2, b.y + s / 2, 14);
  const k = Math.abs(Math.sin(t * 6));
  ctx.fillStyle = '#ddd';
  ctx.fillRect(p.x - 2 * cam.zoom, p.y - 10 * cam.zoom * k, 4 * cam.zoom, 8 * cam.zoom);
}

export function drawBar(ctx, x, y, w, frac, color, bg = 'rgba(0,0,0,0.6)') {
  ctx.fillStyle = bg;
  ctx.fillRect(x - w / 2 - 1, y - 1, w + 2, 6);
  ctx.fillStyle = color;
  ctx.fillRect(x - w / 2, y, w * Math.max(0, Math.min(1, frac)), 4);
}

export function drawHpBar(ctx, cam, b) {
  if (b.dead || b.hp >= b.maxHp) return;
  const s = b.size;
  const p = cam.toScreen(b.x + s / 2, b.y + s / 2, b.isWall ? 24 : 40 + s * 10);
  const f = b.hp / b.maxHp;
  drawBar(ctx, p.x, p.y, Math.max(18, s * 14 * cam.zoom), f, f > 0.5 ? '#7c4' : f > 0.25 ? '#ec4' : '#e44');
}

// ---------- Войска ----------
export function drawTroop(ctx, cam, tr, t, isHaze) {
  const z = cam.zoom;
  const air = tr.air ? 46 + Math.sin(t * 4 + tr.bob) * 4 : 0;
  const g = cam.toScreen(tr.x, tr.y);
  const p = cam.toScreen(tr.x, tr.y, air);
  const big = tr.type === 'shield' || tr.type === 'brute' ? 1.6 : tr.type === 'owl' || tr.type === 'wraith' ? 1.3 : 1;
  const r = 5 * z * big + 1;
  ctx.save();
  if (tr.hidden) ctx.globalAlpha = 0.35;
  ctx.fillStyle = 'rgba(0,0,0,0.35)';
  ctx.beginPath(); ctx.ellipse(g.x, g.y, r * 1.1, r * 0.55, 0, 0, 7); ctx.fill();
  const step = tr.air ? 0 : Math.abs(Math.sin(t * 10 + tr.bob)) * 2 * z;
  const by = p.y - r - step;
  const col = tr.def.color;
  if (isHaze) {
    glow(ctx, p.x, by, r * 2.5, 'rgba(140,90,200,0.35)');
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.moveTo(p.x - r, by + r);
    ctx.quadraticCurveTo(p.x - r * 1.2, by - r * 1.4, p.x, by - r * 1.3);
    ctx.quadraticCurveTo(p.x + r * 1.2, by - r * 1.4, p.x + r, by + r);
    ctx.quadraticCurveTo(p.x, by + r * 0.4 + Math.sin(t * 8 + tr.bob) * r * 0.3, p.x - r, by + r);
    ctx.fill();
    ctx.fillStyle = tr.type === 'leech' ? '#7cf' : '#f6d';
    ctx.fillRect(p.x - r * 0.45, by - r * 0.3, r * 0.3, r * 0.3);
    ctx.fillRect(p.x + r * 0.15, by - r * 0.3, r * 0.3, r * 0.3);
  } else {
    if (tr.type === 'owl') {
      const flap = Math.sin(t * 14 + tr.bob) * r;
      ctx.fillStyle = '#b8a47a';
      ctx.beginPath(); ctx.moveTo(p.x, by); ctx.lineTo(p.x - r * 2.2, by - flap); ctx.lineTo(p.x - r * 0.5, by + r * 0.5); ctx.fill();
      ctx.beginPath(); ctx.moveTo(p.x, by); ctx.lineTo(p.x + r * 2.2, by - flap); ctx.lineTo(p.x + r * 0.5, by + r * 0.5); ctx.fill();
    }
    ctx.fillStyle = col;
    ctx.strokeStyle = 'rgba(0,0,0,0.6)'; ctx.lineWidth = 1;
    ctx.beginPath(); ctx.arc(p.x, by, r, 0, 7); ctx.fill(); ctx.stroke();
    ctx.fillStyle = '#f0d8b8';
    ctx.beginPath(); ctx.arc(p.x, by - r * 1.05, r * 0.55, 0, 7); ctx.fill(); ctx.stroke();
    const d = screenDir(tr.angle);
    if (tr.type === 'torch') {
      const hx = p.x + d.x * r * 1.2, hy = by + d.y * r * 0.6 - r * 0.8;
      ctx.strokeStyle = '#6a4a2a'; ctx.lineWidth = 2 * z;
      ctx.beginPath(); ctx.moveTo(p.x + d.x * r * 0.6, by); ctx.lineTo(hx, hy); ctx.stroke();
      glow(ctx, hx, hy, 8 * z, 'rgba(255,170,60,0.9)');
    } else if (tr.type === 'shield') {
      ctx.fillStyle = '#c8d0e8'; ctx.strokeStyle = '#445';
      ctx.beginPath(); ctx.ellipse(p.x + d.x * r * 0.9, by + d.y * r * 0.5, r * 0.5, r * 0.9, 0, 0, 7); ctx.fill(); ctx.stroke();
    } else if (tr.type === 'sapper') {
      ctx.fillStyle = '#222';
      ctx.beginPath(); ctx.arc(p.x + r * 0.9, by - r * 0.2, r * 0.6, 0, 7); ctx.fill();
      if (Math.sin(t * 20) > 0) glow(ctx, p.x + r * 1.2, by - r * 0.9, 5 * z, 'rgba(255,200,80,1)');
    } else if (tr.type === 'archer') {
      ctx.strokeStyle = '#7a5a30'; ctx.lineWidth = 1.5 * z;
      ctx.beginPath(); ctx.arc(p.x + d.x * r, by, r * 0.9, -1.2, 1.2); ctx.stroke();
    } else if (tr.type === 'shade') {
      ctx.fillStyle = 'rgba(60,30,100,0.8)';
      ctx.beginPath(); ctx.moveTo(p.x - r, by + r); ctx.lineTo(p.x, by - r * 2); ctx.lineTo(p.x + r, by + r); ctx.fill();
    }
  }
  if (tr.hp < tr.maxHp) drawBar(ctx, p.x, by - r * 2.6, 14 * z + 6, tr.hp / tr.maxHp, isHaze ? '#c6f' : '#7c4');
  ctx.restore();
}

// ---------- Снаряды и эффекты ----------
export function drawProjectile(ctx, cam, p) {
  const z = cam.zoom;
  const h0 = p.fromAir ? 46 : p.src ? 32 : 10;
  let zz = h0 * (1 - p.k) + 10 * p.k;
  if (p.kind === 'rock') zz += Math.sin(p.k * Math.PI) * 90;
  const s = cam.toScreen(p.x, p.y, zz);
  if (p.kind === 'rock') {
    ctx.fillStyle = '#9a9090'; ctx.beginPath(); ctx.arc(s.x, s.y, 5 * z + 1, 0, 7); ctx.fill();
    const g = cam.toScreen(p.x, p.y);
    ctx.fillStyle = 'rgba(0,0,0,0.25)'; ctx.beginPath(); ctx.ellipse(g.x, g.y, 5 * z, 2.5 * z, 0, 0, 7); ctx.fill();
  } else if (p.kind === 'bolt' || p.kind === 'arrow') {
    const a = Math.atan2(p.ty - p.sy, p.tx - p.sx);
    const d = screenDir(a);
    const L = (p.kind === 'bolt' ? 12 : 7) * z + 2;
    ctx.strokeStyle = p.kind === 'bolt' ? '#e8d8b0' : '#cfe8a0'; ctx.lineWidth = Math.max(1, 2 * z);
    ctx.beginPath(); ctx.moveTo(s.x - d.x * L, s.y - d.y * L); ctx.lineTo(s.x, s.y); ctx.stroke();
  } else {
    glow(ctx, s.x, s.y, 8 * z + 2, p.kind === 'wisp' ? 'rgba(200,140,255,0.9)' : 'rgba(255,240,200,0.9)');
  }
}

export function drawBeam(ctx, cam, b, t) {
  const tr = b.target;
  if (!tr || tr.dead) return;
  const s = b.size;
  const a = cam.toScreen(b.x + s / 2, b.y + s / 2, 40);
  const bz = tr.air ? 46 : 6;
  const c = cam.toScreen(tr.x, tr.y, bz);
  const k = Math.min(1, b.beamT / 1.5);
  ctx.save();
  ctx.globalCompositeOperation = 'lighter';
  ctx.strokeStyle = `rgba(255,120,230,${0.4 + k * 0.4})`;
  ctx.lineWidth = (2 + k * 5) * cam.zoom + 1;
  ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(c.x, c.y); ctx.stroke();
  ctx.strokeStyle = 'rgba(255,255,255,0.9)'; ctx.lineWidth = 1.2;
  ctx.beginPath(); ctx.moveTo(a.x, a.y); ctx.lineTo(c.x, c.y); ctx.stroke();
  ctx.restore();
  glow(ctx, c.x, c.y, 12 * cam.zoom + 4, 'rgba(255,150,240,0.8)');
}

export function drawEffect(ctx, cam, e, t) {
  const z = cam.zoom;
  const k = 1 - e.life / e.max;
  const p = cam.toScreen(e.x, e.y, e.air ? 46 : 0);
  ctx.save();
  switch (e.kind) {
    case 'boom': {
      const r = (e.size || 1) * 20 * z * (0.4 + k);
      glow(ctx, p.x, p.y - 10 * z, r * 1.6, `rgba(255,150,60,${1 - k})`);
      ctx.fillStyle = `rgba(70,60,70,${0.7 * (1 - k)})`;
      for (let i = 0; i < 6; i++) {
        const a = i * 1.05;
        ctx.beginPath(); ctx.arc(p.x + Math.cos(a) * r * 0.7, p.y - 10 * z - k * 30 * z + Math.sin(a) * r * 0.35, r * 0.4, 0, 7); ctx.fill();
      }
      break;
    }
    case 'blast': {
      const rx = ((e.r || 1) * TW * z) / SQ2;
      ctx.strokeStyle = `rgba(255,200,120,${1 - k})`; ctx.lineWidth = 3;
      ctx.beginPath(); ctx.ellipse(p.x, p.y, rx * (0.3 + k * 0.7), rx * 0.5 * (0.3 + k * 0.7), 0, 0, 7); ctx.stroke();
      glow(ctx, p.x, p.y, rx * 0.6, `rgba(255,170,80,${0.8 * (1 - k)})`);
      break;
    }
    case 'hit':
      glow(ctx, p.x, p.y - 8 * z, 7 * z + 2, `rgba(255,240,200,${1 - k})`);
      break;
    case 'poof':
      ctx.fillStyle = `rgba(150,140,160,${0.6 * (1 - k)})`;
      ctx.beginPath(); ctx.arc(p.x, p.y - 6 * z - k * 12 * z, (4 + k * 8) * z, 0, 7); ctx.fill();
      break;
    case 'flame': {
      const a = cam.toScreen(e.sx, e.sy, 20);
      const n = 6;
      for (let i = 0; i <= n; i++) {
        const f = i / n;
        const x = a.x + (p.x - a.x) * f, y = a.y + (p.y - 6 * z - a.y) * f;
        glow(ctx, x, y, (6 + f * 12) * z + 2, `rgba(255,${140 - f * 60},40,${(1 - k) * 0.9})`);
      }
      break;
    }
    case 'eclipse': {
      const rx = (3.5 * TW * z) / SQ2;
      ctx.strokeStyle = `rgba(190,140,255,${1 - k})`; ctx.lineWidth = 3;
      ctx.beginPath(); ctx.ellipse(p.x, p.y, rx * (0.2 + k), rx * 0.5 * (0.2 + k), 0, 0, 7); ctx.stroke();
      break;
    }
    case 'rift': {
      const rx = 40 * z;
      ctx.strokeStyle = `rgba(170,110,240,${1 - k})`; ctx.lineWidth = 3;
      for (let i = 0; i < 3; i++) {
        ctx.beginPath(); ctx.ellipse(p.x, p.y, rx * (0.4 + i * 0.3), rx * 0.25 * (0.4 + i * 0.3), t * 2 + i, 0, 5); ctx.stroke();
      }
      break;
    }
  }
  ctx.restore();
}
