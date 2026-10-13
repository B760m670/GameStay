// Сцена деревни: строительство, улучшение, сбор, армия, переходы в бой.
import { GRID, BUILDINGS, TROOPS, RES, DEFENSES, RAID_SEARCH_COST } from '../core/config.js';
import { centerOf, lightSources } from '../core/grid.js';
import * as V from '../core/village.js';
import { MISSIONS, CODEX, INTRO, missionUnlocked } from '../core/campaign.js';
import {
  drawBackground, drawGround, drawFog, drawBuilding, drawBar, glow,
} from './render.js';
import { el, $, clear, fmt, fmtTime, toast, modal, costLabel, starsEl, confirmBox, dialogue, anyModalOpen } from './ui.js';

export const ICONS = {
  lantern: '🏮', beacon: '💎', oreMine: '⛏', aetherWell: '⛲', oreStore: '📦', aetherStore: '🧪',
  barracks: '⚔', camp: '⛺', builderHut: '🔨', ballista: '🏹', brazier: '🔥', catapult: '☄', prism: '🔮', wall: '🧱',
};
const SHOP = [
  ['Оборона', ['ballista', 'brazier', 'catapult', 'prism']],
  ['Свет', ['beacon']],
  ['Стены', ['wall']],
  ['Экономика', ['oreMine', 'aetherWell', 'oreStore', 'aetherStore', 'builderHut']],
  ['Армия', ['barracks', 'camp']],
];

export class VillageScene {
  constructor(game) {
    this.g = game;
    this.selected = null;
    this.placing = null; // {type, x, y, moveId, valid}
    this.dragMove = null;
    this.sig = '';
    this.hudRefs = {};
  }
  get S() { return this.g.S; }

  enter() {
    this.buildHud();
    const cam = this.g.cam;
    const lantern = this.S.buildings.find((b) => b.type === 'lantern');
    const R = BUILDINGS.lantern.light[(lantern?.level || 1) - 1] + 4;
    const c = lantern ? centerOf(lantern) : { x: GRID / 2, y: GRID / 2 };
    cam.focus(c.x, c.y, R);
  }
  exit() {
    clear($('hud-top')); clear($('hud-bottom')); clear($('panel'));
  }

  // ---------- HUD ----------
  buildHud() {
    const top = clear($('hud-top'));
    const left = el('div', { class: 'group' });
    const right = el('div', { class: 'group' });
    const r = this.hudRefs;
    r.lvl = el('span', {}, '');
    left.append(el('div', { class: 'chip lvl-chip' }, '🏮 ', r.lvl));
    r.builders = el('span', {}, '');
    left.append(el('div', { class: 'chip', title: 'Строители' }, el('span', { class: 'ico' }, '🔨'), r.builders));
    r.army = el('span', {}, '');
    left.append(el('div', { class: 'chip', title: 'Армия' }, el('span', { class: 'ico' }, '⚔'), r.army));
    for (const res of ['ore', 'aether']) {
      const val = el('span', {}, ''), cap = el('span', { style: { color: 'var(--muted)', fontSize: '11px' } }, '');
      const bar = el('i', { style: { background: RES[res].color, width: '0%' } });
      right.append(el('div', { class: 'chip res-chip' },
        el('div', { class: 'row' }, el('span', { style: { color: RES[res].color } }, `${RES[res].icon} ${RES[res].name}`), val),
        el('div', { class: 'bar' }, bar), el('div', { class: 'row' }, el('span'), cap)));
      r[res] = { val, cap, bar };
    }
    r.shards = el('span', {}, '');
    right.append(el('div', { class: 'chip', title: 'Осколки' }, el('span', { class: 'ico', style: { color: RES.shards.color } }, '◆'), r.shards));
    top.append(left, right);

    const bot = clear($('hud-bottom'));
    const mb = (ico, label, fn) => {
      const lab = el('span', {}, label);
      const b = el('button', { class: 'main-btn', onclick: fn }, el('span', { class: 'ico' }, ico), lab);
      b.lab = lab;
      return b;
    };
    bot.append(
      mb('🏗', 'Строить', () => this.openShop()),
      mb('⚔', 'Армия', () => this.openArmy()),
      mb('🗺', 'Поход', () => this.openMap()),
    );
    r.night = mb('🌙', 'Ночь', () => this.openNight());
    bot.append(r.night, mb('📜', 'Хроника', () => this.openCodex()), mb('⚙', 'Меню', () => this.openSettings()));
    this.refreshHud();
  }

  refreshHud() {
    const S = this.S, r = this.hudRefs, now = Date.now();
    if (!r.lvl) return;
    r.lvl.textContent = `Фонарь ${V.lanternLevel(S)}`;
    const bl = V.builders(S, now);
    r.builders.textContent = `${bl.free}/${bl.total}`;
    r.army.textContent = `${V.armyHousing(S)}/${V.campCapacity(S)}`;
    const cap = V.storageCap(S);
    for (const res of ['ore', 'aether']) {
      r[res].val.textContent = fmt(S.res[res]);
      r[res].cap.textContent = `макс ${fmt(cap[res])}`;
      r[res].bar.style.width = `${Math.min(100, (S.res[res] / Math.max(1, cap[res])) * 100)}%`;
    }
    r.shards.textContent = fmt(S.res.shards);
    const due = V.nightDue(S, now);
    r.night.lab.textContent = due ? 'Мгла идёт!' : `Ночь ${fmtTime((S.nightAt - now) / 1000)}`;
    r.night.classList.toggle('pulse', due);
  }

  // ---------- Панель выбранной постройки ----------
  refreshPanel() {
    const S = this.S, now = Date.now();
    const p = $('panel');
    if (this.placing && !this.placing.moveId) {
      const sig = `place|${this.placing.type}|${this.placing.valid}`;
      if (sig === this.sig) return;
      this.sig = sig;
      clear(p);
      const def = BUILDINGS[this.placing.type];
      const c = V.buildCost(S, this.placing.type);
      p.append(el('div', { class: 'panel' },
        el('h3', {}, `${ICONS[this.placing.type]} ${def.name}`),
        el('div', { class: 'desc' }, this.placing.valid ? 'Перетащи или нажми на карту, чтобы выбрать место. Строить можно только в свете.' : this.placing.reason || 'Здесь нельзя строить'),
        el('div', { class: 'actions' },
          el('button', { class: 'green', disabled: !this.placing.valid, onclick: () => this.confirmPlace() }, '✔ Построить ', costLabel(c.res, c.amount)),
          el('button', { class: 'red', onclick: () => this.cancelPlace() }, '✕ Отмена'))));
      return;
    }
    const b = this.selected != null ? V.getB(S, this.selected) : null;
    if (!b) { if (this.sig !== '') { clear(p); this.sig = ''; } return; }
    const def = BUILDINGS[b.type];
    const up = V.upgradeInfo(S, b.id, now);
    const busy = b.busyUntil > now;
    const amount = V.collectorAmount(b, now);
    const sig = `${b.id}|${b.level}|${busy}|${up.ok}|${up.reason}|${Math.floor(amount)}|${S.res.shards >= (busy ? V.speedUpCost(b, now) : 0)}`;
    if (sig === this.sig) { this.updatePanelTimers(b, now); return; }
    this.sig = sig;
    clear(p);
    const lv = Math.max(1, b.level);
    const stats = el('div', { class: 'stats' });
    const st = (k, v) => stats.append(el('span', {}, el('span', { style: { color: 'var(--muted)' } }, k + ': '), v));
    st('Прочность', fmt(def.hp[Math.min(lv, def.hp.length) - 1]));
    if (def.isLight) st('Радиус света', def.light[lv - 1]);
    if (def.produces) {
      st('Добыча', `${def.rate[lv - 1]}/мин`);
      st('Накоплено', `${Math.floor(amount)}/${def.cap[lv - 1]}`);
    }
    if (def.storage && b.type !== 'lantern') st('Вместимость', fmt(def.storage[lv - 1]));
    if (b.type === 'lantern') st('Хранит', `${fmt(def.storage[lv - 1])} каждого`);
    if (def.dmg) st('Урон', `${def.dmg[lv - 1]} / ${def.cd}с`);
    if (def.dps) st('Луч', `${def.dps[lv - 1]}/с ×${def.rampMax}`);
    if (def.range) st('Дальность', def.range);
    if (DEFENSES.includes(b.type)) st('Цели', `${def.ground ? 'земля' : ''}${def.ground && def.air ? ' + ' : ''}${def.air ? 'воздух' : ''}`);
    if (def.capacity) st('Мест', def.capacity[lv - 1]);
    if (b.type === 'barracks') {
      const unl = Object.values(TROOPS).filter((t) => t.unlock <= lv).map((t) => t.name).join(', ');
      st('Обучает', unl);
    }
    const actions = el('div', { class: 'actions' });
    this.timerSpan = null;
    if (busy) {
      this.timerSpan = el('span', {}, fmtTime((b.busyUntil - now) / 1000));
      actions.append(el('span', { class: 'chip' }, '⏳ ', this.timerSpan));
      const sc = V.speedUpCost(b, now);
      actions.append(el('button', { class: 'gold', disabled: S.res.shards < sc, onclick: () => this.doSpeedUp(b.id) }, `Ускорить ◆${sc}`));
    } else if (!up.max) {
      const btn = el('button', { class: up.ok ? 'gold' : '', disabled: !up.ok, onclick: () => this.doUpgrade(b.id) },
        `⬆ Улучшить до ${b.level + 1} `, up.cost != null ? costLabel(up.res, up.cost) : '', up.time ? ` · ${fmtTime(up.time)}` : '');
      actions.append(btn);
      if (!up.ok) actions.append(el('span', { style: { color: 'var(--muted)', alignSelf: 'center', fontSize: '12px' } }, up.reason));
    } else {
      actions.append(el('span', { class: 'chip' }, '★ Максимальный уровень'));
    }
    if (def.produces && b.level >= 1) {
      actions.append(el('button', { disabled: amount < 1, onclick: () => this.doCollect(b.id) }, `Собрать ${RES[def.produces].icon} ${Math.floor(amount)}`));
    }
    if (b.type === 'barracks' || b.type === 'camp') actions.append(el('button', { onclick: () => this.openArmy() }, '⚔ Армия'));
    if (b.type === 'lantern' && !up.max) {
      const nextLL = b.level + 1;
      const unlocks = Object.keys(BUILDINGS).filter((t) => V.limitOf(t, nextLL) > V.limitOf(t, b.level)).map((t) => BUILDINGS[t].name);
      if (unlocks.length) stats.append(el('div', { style: { width: '100%', color: 'var(--muted)', fontSize: '12px' } }, `Ур. ${nextLL} даст: ${unlocks.join(', ')}`));
    }
    p.append(el('div', { class: 'panel' },
      el('h3', {}, `${ICONS[b.type]} ${def.name} — ${b.level ? `ур. ${b.level}` : 'строится'}`),
      el('div', { class: 'desc' }, def.desc, ' ', el('i', {}, 'Перетащи, чтобы переместить.')),
      stats, actions));
  }

  updatePanelTimers(b, now) {
    if (this.timerSpan && b.busyUntil > now) this.timerSpan.textContent = fmtTime((b.busyUntil - now) / 1000);
  }

  doUpgrade(id) {
    const r = V.startUpgrade(this.S, id);
    if (!r.ok) toast(r.reason, 'bad'); else { toast('Строители взялись за работу', 'good'); this.g.save(); }
    this.sig = '';
  }
  doSpeedUp(id) {
    const r = V.speedUp(this.S, id);
    if (!r.ok) toast(r.reason || 'Нельзя ускорить', 'bad'); else { toast('Готово!', 'good'); this.g.save(); }
    this.sig = '';
  }
  doCollect(id) {
    const b = V.getB(this.S, id);
    const got = V.collect(this.S, id);
    if (got > 0) toast(`+${got} ${RES[BUILDINGS[b.type].produces].name}`, 'good');
    else toast('Хранилища полны', 'bad');
    this.sig = '';
  }

  // ---------- Размещение ----------
  findSpot(type, near) {
    const s = BUILDINGS[type].size;
    const cx = Math.round(near.x - s / 2), cy = Math.round(near.y - s / 2);
    for (let r = 0; r < 20; r++) {
      for (let dy = -r; dy <= r; dy++) for (let dx = -r; dx <= r; dx++) {
        if (Math.max(Math.abs(dx), Math.abs(dy)) !== r) continue;
        if (V.canPlace(this.S, type, cx + dx, cy + dy).ok) return { x: cx + dx, y: cy + dy };
      }
    }
    return { x: cx, y: cy };
  }

  startPlacing(type, near) {
    const center = near || this.g.cam.toWorld(this.g.cam.w / 2, this.g.cam.h / 2);
    const spot = this.findSpot(type, center);
    this.selected = null;
    this.placing = { type, x: spot.x, y: spot.y };
    this.validatePlacing();
  }
  validatePlacing() {
    const pl = this.placing;
    const r = V.canPlace(this.S, pl.type, pl.x, pl.y, pl.moveId ?? null);
    pl.valid = r.ok; pl.reason = r.reason;
  }
  confirmPlace() {
    const pl = this.placing;
    const r = V.placeBuilding(this.S, pl.type, pl.x, pl.y);
    if (!r.ok) { toast(r.reason, 'bad'); return; }
    this.g.save();
    if (pl.type === 'wall' && V.canBuild(this.S, 'wall').ok) {
      // продолжаем ставить стену рядом
      const d = pl.lastDir || [1, 0];
      pl.x += d[0]; pl.y += d[1];
      this.validatePlacing();
      if (!pl.valid) { const s = this.findSpot('wall', { x: pl.x + 0.5, y: pl.y + 0.5 }); pl.x = s.x; pl.y = s.y; this.validatePlacing(); }
      this.sig = '';
      return;
    }
    this.placing = null;
    this.selected = r.building.id;
    this.sig = '';
  }
  cancelPlace() { this.placing = null; this.sig = ''; }

  buildingAt(w) {
    const tx = Math.floor(w.x), ty = Math.floor(w.y);
    // приоритет — то, что «ближе к камере»
    let best = null, bd = -1;
    for (const b of this.S.buildings) {
      const s = BUILDINGS[b.type].size;
      if (tx >= b.x && tx < b.x + s && ty >= b.y && ty < b.y + s) {
        const d = b.x + b.y + s;
        if (d > bd) { bd = d; best = b; }
      }
    }
    return best;
  }

  // ---------- Ввод ----------
  onDown(w) {
    if (anyModalOpen()) return false;
    if (this.placing && !this.placing.moveId) {
      const s = BUILDINGS[this.placing.type].size;
      if (w.x >= this.placing.x && w.x < this.placing.x + s && w.y >= this.placing.y && w.y < this.placing.y + s) {
        this.dragMove = { kind: 'ghost', ox: w.x - this.placing.x, oy: w.y - this.placing.y };
        return true;
      }
      return false;
    }
    if (this.selected != null) {
      const b = this.buildingAt(w);
      if (b && b.id === this.selected) {
        this.dragMove = { kind: 'move', id: b.id, ox: w.x - b.x, oy: w.y - b.y, fromX: b.x, fromY: b.y };
        this.placing = { type: b.type, x: b.x, y: b.y, moveId: b.id, valid: true };
        return true;
      }
    }
    return false;
  }
  onDrag(w) {
    if (!this.dragMove || !this.placing) return;
    const nx = Math.round(w.x - this.dragMove.ox), ny = Math.round(w.y - this.dragMove.oy);
    if (nx !== this.placing.x || ny !== this.placing.y) {
      this.placing.lastDir = [Math.sign(nx - this.placing.x), Math.sign(ny - this.placing.y)];
      if (this.placing.lastDir[0] && this.placing.lastDir[1]) this.placing.lastDir[1] = 0;
      this.placing.x = nx; this.placing.y = ny;
      this.validatePlacing();
    }
  }
  onDragEnd(drag) {
    const dm = this.dragMove;
    this.dragMove = null;
    if (!dm) return;
    if (dm.kind === 'move') {
      const pl = this.placing;
      this.placing = null;
      if (!drag.moved) return;
      if (pl.x === dm.fromX && pl.y === dm.fromY) return;
      const r = V.moveBuilding(this.S, dm.id, pl.x, pl.y);
      if (!r.ok) toast(r.reason, 'bad'); else this.g.save();
      this.sig = '';
    }
  }
  onTap(w) {
    if (anyModalOpen()) return;
    if (this.placing && !this.placing.moveId) {
      const s = BUILDINGS[this.placing.type].size;
      const nx = Math.round(w.x - s / 2), ny = Math.round(w.y - s / 2);
      this.placing.x = nx; this.placing.y = ny;
      this.validatePlacing();
      return;
    }
    const b = this.buildingAt(w);
    if (!b) { this.selected = null; return; }
    const def = BUILDINGS[b.type];
    if (def.produces && V.collectorAmount(b) >= 1 && this.selected !== b.id) this.doCollect(b.id);
    this.selected = b.id;
    this.sig = '';
  }

  // ---------- Модальные окна ----------
  openShop() {
    const S = this.S;
    modal({
      title: '🏗 Строительство',
      body: (body, m) => {
        const LL = V.lanternLevel(S);
        for (const [cat, types] of SHOP) {
          body.append(el('h3', { style: { fontFamily: 'var(--title)', color: 'var(--muted)', margin: '6px 0' } }, cat));
          const grid = el('div', { class: 'grid-cards' });
          for (const type of types) {
            const def = BUILDINGS[type];
            const can = V.canBuild(S, type);
            const c = V.buildCost(S, type);
            const lim = V.limitOf(type, LL);
            const card = el('div', { class: `card ${can.ok ? '' : 'locked'}` },
              el('div', { class: 'swatch', style: { background: 'rgba(0,0,0,0.3)' } }, ICONS[type]),
              el('div', { class: 'name' }, def.name),
              el('div', { class: 'meta' }, `${V.countOf(S, type)}/${lim} · ${def.size}×${def.size}${def.time[0] ? ' · ' + fmtTime(def.time[0]) : ''}`),
              el('div', { class: 'cost' }, costLabel(c.res, c.amount)),
              can.ok ? null : el('div', { class: 'meta', style: { color: '#e88' } }, can.reason));
            card.addEventListener('click', () => {
              if (!can.ok) { toast(can.reason, 'bad'); return; }
              m.close();
              this.startPlacing(type);
            });
            grid.append(card);
          }
          body.append(grid);
        }
      },
    });
  }

  openArmy() {
    const S = this.S;
    let timer = null;
    modal({
      title: '⚔ Армия',
      width: 680,
      onClose: () => clearInterval(timer),
      body: (body) => {
        const info = el('div', { class: 'big-num' });
        const queue = el('div', { class: 'queue' });
        const army = el('div', { class: 'queue' });
        const grid = el('div', { class: 'grid-cards' });
        const cards = {};
        for (const [type, t] of Object.entries(TROOPS)) {
          const cnt = el('div', { class: 'meta' });
          const reason = el('div', { class: 'meta', style: { color: '#e88' } });
          const card = el('div', { class: 'card' },
            el('div', { class: 'swatch', style: { background: t.color + '55' } }, el('div', { style: { width: '26px', height: '26px', borderRadius: '50%', background: t.color, border: '2px solid #0006' } })),
            el('div', { class: 'name' }, t.name),
            el('div', { class: 'meta' }, t.desc),
            el('div', { class: 'meta' }, `❤ ${t.hp} · 🏠 ${t.housing} · ⏱ ${t.time}с`),
            el('div', { class: 'cost' }, costLabel('aether', t.cost)), cnt, reason);
          card.addEventListener('click', () => {
            const r = V.train(S, type);
            if (!r.ok) toast(r.reason, 'bad');
            refresh();
          });
          cards[type] = { card, cnt, reason };
          grid.append(card);
        }
        const refresh = () => {
          V.tick(S);
          const cap = V.campCapacity(S);
          info.textContent = `Лагеря: ${V.armyHousing(S)} + ${V.queueHousing(S)} в обучении / ${cap}`;
          clear(queue);
          if (!S.trainQueue.length) queue.append(el('span', { class: 'meta', style: { color: 'var(--muted)' } }, 'Очередь пуста. Нажми на отряд, чтобы обучить.'));
          S.trainQueue.slice(0, 30).forEach((q, i) => queue.append(el('span', { class: 'q', title: 'Отменить', onclick: () => { V.cancelTrain(S, i); refresh(); } },
            `${TROOPS[q.type].name}${i === 0 ? ` · ${Math.ceil(q.left / Math.max(1, V.barracksCount(S)))}с` : ''} ✕`)));
          clear(army);
          const entries = Object.entries(S.army).filter(([, c]) => c > 0);
          if (!entries.length) army.append(el('span', { class: 'meta', style: { color: 'var(--muted)' } }, 'Армия пуста'));
          for (const [t, c] of entries) army.append(el('span', { class: 'q', style: { borderColor: TROOPS[t].color } }, `${TROOPS[t].name} ×${c}`));
          for (const [type] of Object.entries(TROOPS)) {
            const c = V.canTrain(S, type);
            cards[type].card.classList.toggle('locked', !c.ok);
            cards[type].reason.textContent = c.ok ? '' : c.reason;
            cards[type].cnt.textContent = `В армии: ${S.army[type] || 0}`;
          }
          this.refreshHud();
        };
        body.append(info, el('div', { class: 'meta', style: { color: 'var(--muted)' } }, 'Готовая армия:'), army,
          el('div', { class: 'meta', style: { color: 'var(--muted)' } }, 'Очередь обучения (казарм: ' + V.barracksCount(S) + '):'), queue, grid);
        refresh();
        timer = setInterval(refresh, 500);
      },
    });
  }

  openMap() {
    const S = this.S;
    modal({
      title: '🗺 Поход',
      width: 700,
      body: (body, m) => {
        const tabs = el('div', { class: 'tabs' });
        const content = el('div');
        const show = (which) => {
          [...tabs.children].forEach((b) => b.classList.toggle('active', b.dataset.k === which));
          clear(content);
          if (which === 'campaign') this.renderCampaign(content, m);
          else this.renderRaid(content, m);
        };
        tabs.append(
          el('button', { 'data-k': 'campaign', onclick: () => show('campaign') }, '📖 Хроника Мглы'),
          el('button', { 'data-k': 'raid', onclick: () => show('raid') }, '⚔ Набег на соперника'));
        body.append(tabs, content);
        show('campaign');
      },
    });
  }

  renderCampaign(content, m) {
    const S = this.S;
    const list = el('div', { class: 'list' });
    MISSIONS.forEach((ms, i) => {
      const open = missionUnlocked(S, i);
      const stars = S.campaign[ms.id] || 0;
      const row = el('div', { class: `row-item ${open ? '' : 'locked'}` },
        el('div', { class: 'big-num', style: { width: '32px', textAlign: 'center', color: 'var(--muted)' } }, String(i + 1)),
        el('div', { class: 'grow' },
          el('div', { class: 'title' }, ms.title),
          el('div', { class: 'sub' }, `Фонарь врага: ${ms.LL} · Награда за первую победу: `,
            costLabel('ore', ms.reward.ore), ' ', costLabel('aether', ms.reward.aether), ' ', costLabel('shards', ms.reward.shards))),
        starsEl(stars),
        el('button', { class: open ? 'gold' : '', disabled: !open, onclick: () => {
          if (V.armyHousing(S) === 0) { toast('Армия пуста — обучи войска', 'bad'); return; }
          m.close();
          dialogue(ms.intro, () => this.g.startMission(ms));
        } }, open ? 'В бой' : '🔒'));
      list.append(row);
    });
    content.append(list);
  }

  renderRaid(content, m) {
    const S = this.S;
    const LL = V.lanternLevel(S);
    const cost = RAID_SEARCH_COST[LL - 1];
    content.append(
      el('p', {}, 'Другие Хранители и Угасшие копят руду и Эфир. Найди соперника своего уровня, погаси его свет и забери добычу.'),
      el('p', { class: 'sub', style: { color: 'var(--muted)' } }, `Армия: ${V.armyHousing(S)} мест · Затмений в бою: ${Math.floor(LL / 2)}`),
      el('button', { class: 'gold big', onclick: () => {
        if (V.armyHousing(S) === 0) { toast('Армия пуста — обучи войска', 'bad'); return; }
        if (S.res.ore < cost) { toast('Не хватает руды на поиск', 'bad'); return; }
        S.res.ore -= cost;
        m.close();
        this.g.startRaid();
      } }, '🔍 Искать соперника ', costLabel('ore', cost)));
  }

  openNight() {
    const S = this.S;
    const due = V.nightDue(S);
    modal({
      title: '🌙 Ночь Мглы',
      body: el('div', {},
        el('p', {}, 'Каждую ночь Мгла проверяет твой свет. Порождения выходят из тьмы со всех сторон. Твоя оборона сражается сама — ты наблюдаешь.'),
        el('p', {}, 'Если Фонарь устоит до рассвета — получишь Осколки и ресурсы. Разрушенные хранилища теряют часть запасов.'),
        el('p', { style: { color: 'var(--muted)' } }, `Пережито ночей: ${S.nights}. ${due ? 'Мгла уже у порога!' : `Следующая ночь через ${fmtTime((S.nightAt - Date.now()) / 1000)}.`}`)),
      buttons: [
        { text: 'Позже', onClick: (mm) => mm.close() },
        { text: due ? 'Встретить ночь' : 'Призвать ночь сейчас', cls: 'gold', onClick: (mm) => { mm.close(); this.g.startNight(); } },
      ],
    });
  }

  openCodex() {
    const S = this.S;
    modal({
      title: '📜 Хроника',
      body: (body) => {
        const list = el('div', { class: 'list' });
        for (const key of S.codex) {
          const c = CODEX[key];
          if (!c) continue;
          list.append(el('div', { class: 'row-item' }, el('div', { class: 'grow' }, el('div', { class: 'title' }, c.title), el('div', {}, c.text))));
        }
        const locked = Object.keys(CODEX).length - S.codex.length;
        if (locked > 0) list.append(el('div', { class: 'sub', style: { color: 'var(--muted)' } }, `Ещё ${locked} записей скрыто во Мгле. Проходи главы кампании.`));
        body.append(el('button', { onclick: () => dialogue(INTRO) }, '▶ Пролог'), el('div', { style: { height: '10px' } }), list);
      },
    });
  }

  openSettings() {
    const S = this.S;
    modal({
      title: '⚙ Меню',
      body: el('div', { class: 'list' },
        el('div', { class: 'row-item' }, el('div', { class: 'grow' },
          el('div', { class: 'title' }, 'Статистика'),
          el('div', { class: 'sub' }, `Атак: ${S.stats.attacks}, побед: ${S.stats.wins}. Ночей пережито: ${S.stats.nightsWon}, проиграно: ${S.stats.nightsLost}.`))),
        el('div', { class: 'row-item' }, el('div', { class: 'grow' },
          el('div', { class: 'title' }, 'Режим разработчика'),
          el('div', { class: 'sub' }, 'Для тестирования баланса: ресурсы, осколки, мгновенная стройка.')),
        el('button', { onclick: () => {
          V.addRes(S, 'ore', 1e9); V.addRes(S, 'aether', 1e9); S.res.shards += 500;
          for (const b of S.buildings) if (b.busyUntil) b.busyUntil = Date.now();
          V.tick(S); this.g.save(); toast('Казна полна', 'good'); this.sig = '';
        } }, '+ Ресурсы')),
        el('div', { class: 'row-item' }, el('div', { class: 'grow' },
          el('div', { class: 'title' }, 'Начать заново'),
          el('div', { class: 'sub' }, 'Удалит весь прогресс.')),
        el('button', { class: 'red', onclick: () => confirmBox('Начать заново?', 'Весь прогресс будет потерян.', 'Стереть', () => this.g.reset()) }, 'Сброс'))),
    });
  }

  // ---------- Цикл ----------
  update(dt) {
    this.hudT = (this.hudT || 0) + dt;
    if (this.hudT > 0.25) {
      this.hudT = 0;
      V.tick(this.S);
      this.refreshHud();
      this.refreshPanel();
    }
  }

  render(ctx, t) {
    const { cam } = this.g;
    const S = this.S, now = Date.now();
    drawBackground(ctx, cam.w, cam.h, t);
    drawGround(ctx, cam);
    const moveId = this.placing?.moveId;
    const view = S.buildings.map((b) => (b.id === moveId ? { ...b, x: this.placing.x, y: this.placing.y } : b));
    drawFog(ctx, cam, lightSources(view), [], t, { dpr: this.g.dpr });
    const items = view.map((b) => ({ b, d: b.x + b.y + BUILDINGS[b.type].size }));
    if (this.placing && !moveId) items.push({ ghost: true, b: { id: -1, type: this.placing.type, x: this.placing.x, y: this.placing.y, level: 1 }, d: 1e9 });
    items.sort((a, c) => a.d - c.d);
    const cap = V.storageCap(S);
    for (const it of items) {
      const b = it.b;
      const def = BUILDINGS[b.type];
      const opt = { selected: b.id === this.selected && !this.placing };
      if (it.ghost || b.id === moveId) { opt.ghost = this.placing.valid ? 'ok' : 'bad'; opt.alpha = 0.8; }
      if (def.stores) opt.fill = S.res[def.stores] / Math.max(1, cap[def.stores]);
      drawBuilding(ctx, cam, b, t, opt);
    }
    // индикаторы: стройка и сбор
    for (const b of S.buildings) {
      const def = BUILDINGS[b.type];
      const s = def.size;
      if (b.busyUntil > now) {
        const total = (def.time[Math.min(b.level, def.time.length - 1)] || 1) * 1000;
        const p = cam.toScreen(b.x + s / 2, b.y + s / 2, 70 + s * 6);
        drawBar(ctx, p.x, p.y, 44 * cam.zoom + 10, 1 - (b.busyUntil - now) / total, '#ffd86b');
        ctx.fillStyle = '#fff'; ctx.font = `${Math.round(10 + 3 * cam.zoom)}px system-ui`; ctx.textAlign = 'center';
        ctx.fillText(fmtTime((b.busyUntil - now) / 1000), p.x, p.y - 4);
      } else if (def.produces && b.level >= 1) {
        const a = V.collectorAmount(b, now);
        if (a >= def.cap[b.level - 1] * 0.1) {
          const p = cam.toScreen(b.x + s / 2, b.y + s / 2, 70 + Math.sin(t * 3 + b.id) * 4);
          const r = 11 * cam.zoom + 5;
          glow(ctx, p.x, p.y, r * 2, def.produces === 'ore' ? 'rgba(232,149,74,0.5)' : 'rgba(134,220,255,0.5)');
          ctx.fillStyle = 'rgba(20,15,35,0.9)'; ctx.beginPath(); ctx.arc(p.x, p.y, r, 0, 7); ctx.fill();
          ctx.strokeStyle = RES[def.produces].color; ctx.lineWidth = 2; ctx.stroke();
          ctx.fillStyle = RES[def.produces].color; ctx.font = `${Math.round(r * 1.1)}px system-ui`; ctx.textAlign = 'center'; ctx.textBaseline = 'middle';
          ctx.fillText(RES[def.produces].icon, p.x, p.y + 1);
          ctx.textBaseline = 'alphabetic';
        }
      }
    }
  }
}
