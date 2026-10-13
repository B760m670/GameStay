// Сцена боя: атака игрока или ночная оборона от Мглы.
import { GRID, TROOPS, ECLIPSE } from '../core/config.js';
import * as BT from '../core/battle.js';
import {
  drawBackground, drawGround, drawFog, drawBuilding, drawHpBar, drawTroop, drawProjectile, drawEffect, drawBeam, footprint,
} from './render.js';
import { el, $, clear, fmt, toast, costLabel, anyModalOpen } from './ui.js';

const SCOUT_TIME = 30;
const DT = 1 / 30;

export class BattleScene {
  /**
   * opts: { mode, battle, title, onFinish(result, battle) }
   */
  constructor(game, opts) {
    this.g = game;
    this.opts = opts;
    this.B = opts.battle;
    this.mode = this.B.mode;
    this.started = this.mode === 'defense';
    this.scout = SCOUT_TIME;
    this.sel = null;
    this.acc = 0;
    this.speed = 1;
    this.holding = null;
    this.flash = [];
    this.finishedShown = false;
  }

  enter() {
    const B = this.B;
    const C = GRID / 2;
    let R = 8;
    for (const s of B.sources) R = Math.max(R, Math.hypot(s.x - C, s.y - C) + s.r);
    this.g.cam.focus(C, C, Math.min(GRID / 2 + 2, R + 4));
    this.buildHud();
    const first = Object.keys(this.B.pool).find((k) => this.B.pool[k] > 0);
    if (first) this.select(first);
  }
  exit() { clear($('hud-top')); clear($('hud-bottom')); clear($('panel')); }

  buildHud() {
    const top = clear($('hud-top'));
    const B = this.B;
    this.r = {};
    this.r.timer = el('span', { class: 'big-num' }, '');
    this.r.pct = el('span', { class: 'big-num' }, '');
    this.r.loot = el('span', {}, '');
    this.r.status = el('span', { style: { color: 'var(--muted)', fontSize: '12px' } }, '');
    top.append(
      el('div', { class: 'group' },
        el('div', { class: 'chip' }, el('span', { style: { fontFamily: 'var(--title)', color: 'var(--gold)', fontSize: '16px' } }, this.opts.title)),
        el('div', { class: 'chip' }, '⏱ ', this.r.timer),
        el('div', { class: 'chip' }, this.mode === 'attack' ? '💥 ' : '🏮 ', this.r.pct)),
      el('div', { class: 'group' },
        el('div', { class: 'chip' }, this.mode === 'attack' ? 'Добыча: ' : 'Потери: ', this.r.loot),
        el('div', { class: 'chip' }, this.r.status)));

    const bot = clear($('hud-bottom'));
    this.cards = {};
    if (this.mode === 'attack') {
      for (const type of Object.keys(B.pool)) {
        if (!(B.pool[type] > 0)) continue;
        const t = TROOPS[type];
        const cnt = el('span', { class: 'cnt' }, '');
        const c = el('button', { class: 'troop-card', onclick: () => this.select(type) },
          el('div', { class: 'dot', style: { background: t.color } }), el('span', {}, t.name), cnt);
        c.cnt = cnt;
        this.cards[type] = c;
        bot.append(c);
      }
      if (B.eclipseCharges > 0) {
        const cnt = el('span', { class: 'cnt' }, '');
        const c = el('button', { class: 'troop-card', onclick: () => this.select('eclipse'), title: 'Гасит свет в области на 10 секунд' },
          el('div', { class: 'dot', style: { background: 'radial-gradient(#000 40%, #9a6ae0)' } }), el('span', {}, ECLIPSE.name), cnt);
        c.cnt = cnt;
        this.cards.eclipse = c;
        bot.append(c);
      }
    }
    this.r.speed = el('button', { class: 'main-btn', onclick: () => { this.speed = this.speed === 1 ? 2 : this.speed === 2 ? 4 : 1; this.refreshHud(); } }, el('span', { class: 'ico' }, '⏩'), el('span', {}, 'x1'));
    bot.append(this.r.speed);
    bot.append(el('button', { class: 'main-btn red', onclick: () => this.surrender() }, el('span', { class: 'ico' }, '🏳'), el('span', {}, this.mode === 'attack' ? 'Завершить' : 'Сдаться')));
    this.refreshHud();
  }

  select(type) {
    this.sel = type;
    for (const [k, c] of Object.entries(this.cards)) c.classList.toggle('sel', k === type);
  }

  refreshHud() {
    const B = this.B, r = this.r;
    if (!r) return;
    if (!this.started) r.timer.textContent = `Разведка ${Math.ceil(this.scout)}`;
    else r.timer.textContent = `${Math.max(0, Math.ceil(B.duration - B.time))}с`;
    if (this.mode === 'attack') {
      r.pct.textContent = `${BT.percent(B)}% ${'★'.repeat((BT.percent(B) >= 50) + B.lanternDead + (BT.percent(B) >= 100))}`;
    } else {
      const l = B.buildings.find((b) => b.type === 'lantern');
      r.pct.textContent = l ? `${Math.ceil((l.hp / l.maxHp) * 100)}%` : '—';
    }
    clear(r.loot).append(costLabel('ore', B.loot.ore), ' ', costLabel('aether', B.loot.aether));
    if (this.mode === 'attack') {
      r.status.textContent = this.sel === 'eclipse' ? 'Нажми на базу — свет погаснет' : 'Высаживай войска во тьме';
    } else {
      const left = B.waves.filter((w) => !w.done).length;
      r.status.textContent = left ? `Волн впереди: ${left}` : `Мгла на базе: ${BT.aliveTroops(B)}`;
    }
    r.speed.lastChild.textContent = `x${this.speed}`;
    for (const [k, c] of Object.entries(this.cards)) {
      const n = k === 'eclipse' ? B.eclipseCharges : B.pool[k];
      c.cnt.textContent = `×${n}`;
      c.classList.toggle('empty', !(n > 0));
    }
  }

  surrender() {
    if (this.B.ended) return;
    if (this.mode === 'defense') this.B.lanternDead = true;
    BT.finish(this.B);
  }

  // ---------- Ввод ----------
  tryAction(w) {
    const B = this.B;
    if (B.ended || this.mode !== 'attack' || !this.sel) return false;
    if (this.sel === 'eclipse') {
      if (B.eclipseCharges <= 0) return false;
      if (w.x < 0 || w.y < 0 || w.x > GRID || w.y > GRID) return false;
      BT.castEclipse(B, w.x, w.y);
      this.started = true;
      toast('Затмение! Свет погас', 'good');
      return true;
    }
    if (!(B.pool[this.sel] > 0)) return false;
    if (BT.deploy(B, this.sel, w.x, w.y)) { this.started = true; return true; }
    this.flash.push({ x: Math.floor(w.x), y: Math.floor(w.y), t: 0.5 });
    if (!this.warned || performance.now() - this.warned > 1500) {
      toast('Высадка только во тьме и не на постройках', 'bad');
      this.warned = performance.now();
    }
    return false;
  }

  onDown(w) {
    if (anyModalOpen() || this.B.ended || this.mode !== 'attack' || !this.sel) return false;
    if (this.sel !== 'eclipse' && !(this.B.pool[this.sel] > 0)) return false;
    if (this.sel === 'eclipse') return false;
    this.tryAction(w);
    this.holding = { w, t: 0 };
    return true;
  }
  onDrag(w) { if (this.holding) this.holding.w = w; }
  onDragEnd() { this.holding = null; }
  onTap(w) {
    if (anyModalOpen()) return;
    if (this.sel === 'eclipse') this.tryAction(w);
  }

  // ---------- Цикл ----------
  update(dt) {
    const B = this.B;
    if (!this.started) {
      this.scout -= dt;
      if (this.scout <= 0) this.started = true;
    }
    if (this.holding && !B.ended) {
      this.holding.t += dt;
      if (this.holding.t > 0.11) { this.holding.t = 0; this.tryAction(this.holding.w); }
    }
    if (this.started && !B.ended) {
      this.acc += Math.min(dt, 0.1) * this.speed;
      while (this.acc >= DT) { BT.step(B, DT); this.acc -= DT; }
    }
    for (const ev of B.events.splice(0)) {
      if (ev.kind === 'lightsOut') toast('Светоч погас — сектор во тьме!', 'good');
      if (ev.kind === 'lantern') toast(this.mode === 'attack' ? 'Фонарь врага разрушен!' : 'Наш Фонарь пал!', this.mode === 'attack' ? 'good' : 'bad');
      if (ev.kind === 'wave') toast('Мгла выходит из тьмы!', 'bad');
    }
    for (const f of this.flash) f.t -= dt;
    this.flash = this.flash.filter((f) => f.t > 0);
    this.hudT = (this.hudT || 0) + dt;
    if (this.hudT > 0.2) { this.hudT = 0; this.refreshHud(); }
    if (B.ended && !this.finishedShown) {
      this.finishedShown = true;
      setTimeout(() => this.opts.onFinish(B.result, B), 900);
    }
  }

  render(ctx, t) {
    const { cam } = this.g;
    const B = this.B;
    drawBackground(ctx, cam.w, cam.h, t);
    drawGround(ctx, cam);
    const tint = this.mode === 'attack' && this.sel && this.sel !== 'eclipse' && !B.ended;
    drawFog(ctx, cam, B.sources, B.eclipses, t, { dpr: this.g.dpr, forbidTint: tint });
    for (const f of this.flash) footprint(ctx, cam, f.x, f.y, 1, `rgba(255,60,60,${f.t})`, null);
    const items = [];
    for (const b of B.buildings) items.push({ k: 0, o: b, d: b.cx + b.cy });
    for (const tr of B.troops) if (!tr.dead) items.push({ k: 1, o: tr, d: tr.x + tr.y + (tr.air ? 3 : 0) });
    items.sort((a, b) => a.d - b.d);
    const haze = this.mode === 'defense';
    for (const it of items) {
      if (it.k === 0) drawBuilding(ctx, cam, it.o, t);
      else drawTroop(ctx, cam, it.o, t, haze);
    }
    for (const b of B.buildings) if (b.def.beam && !b.dead && b.target) drawBeam(ctx, cam, b, t);
    for (const p of B.projectiles) drawProjectile(ctx, cam, p);
    for (const e of B.effects) drawEffect(ctx, cam, e, t);
    for (const b of B.buildings) if (!b.isWall || b.hp < b.maxHp) drawHpBar(ctx, cam, b);
  }
}
