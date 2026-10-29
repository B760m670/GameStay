import { GRID } from '../core/config.js';

export const TW = 64;
export const TH = 32;

export class Camera {
  constructor() {
    this.zoom = 0.8;
    this.ox = 0; // экранные координаты мировой точки (0,0)
    this.oy = 0;
    this.w = 1; this.h = 1;
    this.minZoom = 0.25; this.maxZoom = 2.2;
  }
  resize(w, h) { this.w = w; this.h = h; }
  centerOn(wx, wy) {
    const s = this.rawToScreen(wx, wy);
    this.ox += this.w / 2 - s.x;
    this.oy += this.h / 2 - s.y;
  }
  fit() {
    // показать карту так, чтобы ромб помещался по ширине или высоте
    const zw = (this.w * 0.95) / (GRID * TW);
    const zh = (this.h * 0.8) / (GRID * TH);
    this.zoom = Math.max(this.minZoom, Math.min(1.1, Math.max(zw, zh) * 1.25));
    this.ox = 0; this.oy = 0;
    this.centerOn(GRID / 2, GRID / 2);
  }
  // Показать круг радиуса R клеток вокруг (cx, cy).
  focus(cx, cy, R, maxZoom = 1.15) {
    this.zoom = Math.max(this.minZoom, Math.min(maxZoom, (this.w * 0.92) / (1.41 * R * TW), (this.h * 0.72) / (0.71 * R * TW)));
    this.ox = 0; this.oy = 0;
    this.centerOn(cx, cy);
  }
  rawToScreen(wx, wy) {
    return { x: this.ox + (wx - wy) * (TW / 2) * this.zoom, y: this.oy + (wx + wy) * (TH / 2) * this.zoom };
  }
  toScreen(wx, wy, z = 0) {
    return { x: this.ox + (wx - wy) * (TW / 2) * this.zoom, y: this.oy + (wx + wy) * (TH / 2) * this.zoom - z * this.zoom };
  }
  toWorld(sx, sy) {
    const a = (sx - this.ox) / ((TW / 2) * this.zoom);
    const b = (sy - this.oy) / ((TH / 2) * this.zoom);
    return { x: (a + b) / 2, y: (b - a) / 2 };
  }
  zoomAt(sx, sy, factor) {
    const before = this.toWorld(sx, sy);
    this.zoom = Math.max(this.minZoom, Math.min(this.maxZoom, this.zoom * factor));
    const after = this.rawToScreen(before.x, before.y);
    this.ox += sx - after.x;
    this.oy += sy - after.y;
  }
  pan(dx, dy) {
    this.ox += dx; this.oy += dy;
    // не даём улететь далеко от карты
    const c = this.toWorld(this.w / 2, this.h / 2);
    const cx = Math.max(-4, Math.min(GRID + 4, c.x));
    const cy = Math.max(-4, Math.min(GRID + 4, c.y));
    if (cx !== c.x || cy !== c.y) this.centerOn(cx, cy);
  }
}

// Жесты: перетаскивание, тап, щипок, колесо.
export function attachInput(canvas, cam, handlers) {
  const pointers = new Map();
  let drag = null;
  let pinch = null;
  const pos = (e) => {
    const r = canvas.getBoundingClientRect();
    return { x: e.clientX - r.left, y: e.clientY - r.top };
  };
  canvas.addEventListener('pointerdown', (e) => {
    canvas.setPointerCapture(e.pointerId);
    const p = pos(e);
    pointers.set(e.pointerId, p);
    if (pointers.size === 2) {
      const [a, b] = [...pointers.values()];
      pinch = { d: Math.hypot(a.x - b.x, a.y - b.y) };
      if (drag && drag.custom) handlers.onDragEnd?.(drag);
      drag = null;
      return;
    }
    drag = { sx: p.x, sy: p.y, x: p.x, y: p.y, moved: false, custom: false, t: performance.now() };
    const w = cam.toWorld(p.x, p.y);
    drag.custom = !!handlers.onDown?.(w, p, drag);
  });
  canvas.addEventListener('pointermove', (e) => {
    const p = pos(e);
    if (!pointers.has(e.pointerId)) { handlers.onHover?.(cam.toWorld(p.x, p.y), p); return; }
    pointers.set(e.pointerId, p);
    if (pinch && pointers.size >= 2) {
      const [a, b] = [...pointers.values()];
      const d = Math.hypot(a.x - b.x, a.y - b.y);
      cam.zoomAt((a.x + b.x) / 2, (a.y + b.y) / 2, d / pinch.d);
      pinch.d = d;
      return;
    }
    if (!drag) return;
    const dx = p.x - drag.x, dy = p.y - drag.y;
    drag.x = p.x; drag.y = p.y;
    if (Math.hypot(p.x - drag.sx, p.y - drag.sy) > 8) drag.moved = true;
    if (drag.custom) handlers.onDrag?.(cam.toWorld(p.x, p.y), p, drag);
    else if (drag.moved) cam.pan(dx, dy);
    handlers.onHover?.(cam.toWorld(p.x, p.y), p);
  });
  const up = (e) => {
    const p = pos(e);
    pointers.delete(e.pointerId);
    if (pointers.size < 2) pinch = null;
    if (!drag) return;
    if (drag.custom) handlers.onDragEnd?.(drag, cam.toWorld(p.x, p.y));
    if (!drag.moved) handlers.onTap?.(cam.toWorld(p.x, p.y), p);
    handlers.onUp?.();
    drag = null;
  };
  canvas.addEventListener('pointerup', up);
  canvas.addEventListener('pointercancel', up);
  canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const p = pos(e);
    cam.zoomAt(p.x, p.y, Math.exp(-e.deltaY * 0.0015));
  }, { passive: false });
}
