// Точка входа: сохранение, цикл, переключение сцен, применение результатов боёв.
import { BATTLE_TIME, NIGHT_TIME, eclipseCharges, RES } from '../core/config.js';
import * as V from '../core/village.js';
import { createBattle } from '../core/battle.js';
import { generateBase, nightWaves, rivalName } from '../core/basegen.js';
import { INTRO, CODEX } from '../core/campaign.js';
import { mulberry32 } from '../core/grid.js';
import { Camera, attachInput } from './camera.js';
import { VillageScene } from './villageScene.js';
import { BattleScene } from './battleScene.js';
import { el, $, modal, dialogue, toast, costLabel, starsEl, fmt } from './ui.js';

const SAVE_KEY = 'lanternfall-save-v1';

function load() {
  try {
    const raw = localStorage.getItem(SAVE_KEY);
    if (!raw) return null;
    const S = JSON.parse(raw);
    if (S.version !== V.SAVE_VERSION) return null;
    return S;
  } catch { return null; }
}

const canvas = $('game');
const ctx = canvas.getContext('2d');

const game = {
  S: null,
  cam: new Camera(),
  dpr: 1,
  scene: null,
  save() { try { localStorage.setItem(SAVE_KEY, JSON.stringify(this.S)); } catch { /* приватный режим */ } },
  setScene(sc) {
    this.scene?.exit?.();
    this.scene = sc;
    sc.enter?.();
  },
  toVillage() { this.setScene(new VillageScene(this)); },
  reset() {
    try { localStorage.removeItem(SAVE_KEY); } catch { /* */ }
    this.S = V.createVillage();
    this.save();
    this.toVillage();
    dialogue(INTRO);
  },

  startRaid() {
    const S = this.S;
    const LL = V.lanternLevel(S);
    const seed = (Math.random() * 1e9) | 0;
    const rng = mulberry32(seed);
    const eLL = Math.max(1, Math.min(6, LL + (rng() < 0.25 ? -1 : 0) + (rng() < 0.15 && LL < 6 ? 1 : 0)));
    const layout = generateBase({ LL: eLL, seed, richness: 0.7 + rng() * 0.6 });
    const name = rivalName(rng);
    const battle = createBattle({ mode: 'attack', layout, army: { ...S.army }, eclipseCharges: eclipseCharges(LL), seed, duration: BATTLE_TIME });
    this.setScene(new BattleScene(this, {
      battle, title: `${name} · Фонарь ${eLL}`,
      onFinish: (res, B) => this.finishAttack(res, B, null),
    }));
  },

  startMission(ms) {
    const S = this.S;
    const layout = generateBase({ LL: ms.LL, seed: ms.seed, counts: ms.counts || {}, richness: ms.richness, levelBias: ms.levelBias || 0 });
    const battle = createBattle({ mode: 'attack', layout, army: { ...S.army }, eclipseCharges: eclipseCharges(V.lanternLevel(S)), seed: ms.seed, duration: BATTLE_TIME });
    this.setScene(new BattleScene(this, {
      battle, title: ms.title,
      onFinish: (res, B) => this.finishAttack(res, B, ms),
    }));
  },

  startNight() {
    const S = this.S;
    V.tick(S);
    const LL = V.lanternLevel(S);
    const seed = (Math.random() * 1e9) | 0;
    const { waves, hazeMult } = nightWaves(LL, S.nights, seed);
    const layout = V.lootLayout(S, 0.25);
    const battle = createBattle({ mode: 'defense', layout, waves, hazeMult, seed, duration: NIGHT_TIME });
    this.setScene(new BattleScene(this, {
      battle, title: `Ночь ${S.nights + 1}`,
      onFinish: (res, B) => this.finishNight(res, B),
    }));
  },

  finishAttack(res, B, ms) {
    const S = this.S;
    for (const [t, n] of Object.entries(res.used)) S.army[t] = Math.max(0, (S.army[t] || 0) - n);
    const got = { ore: V.addRes(S, 'ore', res.loot.ore), aether: V.addRes(S, 'aether', res.loot.aether), shards: 0 };
    S.stats.attacks++;
    if (res.win) S.stats.wins++;
    let firstWin = false;
    if (ms) {
      const prev = S.campaign[ms.id] || 0;
      if (res.stars > prev) S.campaign[ms.id] = res.stars;
      if (prev === 0 && res.stars > 0) {
        firstWin = true;
        got.ore += V.addRes(S, 'ore', ms.reward.ore);
        got.aether += V.addRes(S, 'aether', ms.reward.aether);
        S.res.shards += ms.reward.shards; got.shards = ms.reward.shards;
        if (ms.codex && !S.codex.includes(ms.codex)) S.codex.push(ms.codex);
      }
    } else if (res.stars > 0) {
      const sh = res.stars;
      S.res.shards += sh; got.shards = sh;
    }
    this.save();
    const title = res.win ? (ms ? 'Победа!' : 'Набег удался!') : 'Поражение';
    modal({
      title, closable: false,
      body: el('div', { class: 'center' },
        el('div', { class: 'result-stars' }, ...[0, 1, 2].map((i) => el('span', { class: i < res.stars ? '' : 'off' }, '★'))),
        el('div', { class: 'big-num' }, `Разрушено: ${res.pct}%`),
        el('p', {}, 'Получено: ', costLabel('ore', got.ore), '  ', costLabel('aether', got.aether), got.shards ? '  ' : '', got.shards ? costLabel('shards', got.shards) : ''),
        firstWin && ms?.codex ? el('p', { style: { color: 'var(--gold)' } }, `Новая запись в Хронике: «${CODEX[ms.codex].title}»`) : null,
        res.win ? null : el('p', { style: { color: 'var(--muted)' } }, 'Совет: сначала гаси Светочи — оборона в их секторе ослепнет, а высаживаться можно будет ближе.')),
      buttons: [{ text: 'Домой', cls: 'gold', onClick: (m) => {
        m.close();
        this.toVillage();
        if (firstWin && ms.outro) dialogue(ms.outro);
      } }],
    });
  },

  finishNight(res, B) {
    const S = this.S;
    const lost = { ore: Math.min(S.res.ore, res.loot.ore), aether: Math.min(S.res.aether, res.loot.aether) };
    S.res.ore -= lost.ore; S.res.aether -= lost.aether;
    const LL = V.lanternLevel(S);
    let reward = null;
    if (res.win) {
      S.nights++;
      S.stats.nightsWon++;
      reward = { ore: V.addRes(S, 'ore', 150 * LL * LL), aether: V.addRes(S, 'aether', 150 * LL * LL), shards: 2 + Math.floor(LL / 2) };
      S.res.shards += reward.shards;
    } else {
      S.stats.nightsLost++;
    }
    V.scheduleNextNight(S);
    this.save();
    modal({
      title: res.win ? '🌅 Рассвет' : '🌑 Фонарь пал',
      closable: false,
      body: el('div', { class: 'center' },
        el('p', {}, res.win ? 'Ты удержал свет до рассвета. Мгла отступила — до следующей ночи.' : 'Мгла захлестнула поселение. Фонарь разгорится снова, но часть запасов утеряна.'),
        el('p', {}, 'Похищено Мглой: ', costLabel('ore', lost.ore), '  ', costLabel('aether', lost.aether)),
        reward ? el('p', {}, 'Награда: ', costLabel('ore', reward.ore), '  ', costLabel('aether', reward.aether), '  ', costLabel('shards', reward.shards)) : null),
      buttons: [{ text: 'Домой', cls: 'gold', onClick: (m) => { m.close(); this.toVillage(); } }],
    });
  },
};

function resize() {
  const dpr = Math.min(window.devicePixelRatio || 1, 2);
  game.dpr = dpr;
  canvas.width = Math.floor(window.innerWidth * dpr);
  canvas.height = Math.floor(window.innerHeight * dpr);
  game.cam.resize(window.innerWidth, window.innerHeight);
}
window.addEventListener('resize', () => {
  const c = game.cam.toWorld(game.cam.w / 2, game.cam.h / 2);
  resize();
  game.cam.centerOn(c.x, c.y);
});
resize();

attachInput(canvas, game.cam, {
  onDown: (w, p, d) => game.scene?.onDown?.(w, p, d),
  onDrag: (w, p, d) => game.scene?.onDrag?.(w, p, d),
  onDragEnd: (d, w) => game.scene?.onDragEnd?.(d, w),
  onTap: (w, p) => game.scene?.onTap?.(w, p),
});

let last = performance.now();
let saveT = 0;
function frame(now) {
  const dt = Math.min(0.1, (now - last) / 1000);
  last = now;
  const t = now / 1000;
  try {
    game.scene?.update(dt);
    ctx.setTransform(game.dpr, 0, 0, game.dpr, 0, 0);
    game.scene?.render(ctx, t);
  } catch (e) {
    console.error(e);
  }
  saveT += dt;
  if (saveT > 5 && game.scene instanceof VillageScene) { saveT = 0; game.save(); }
  requestAnimationFrame(frame);
}

function titleScreen(onStart) {
  const root = $('modal-root');
  const has = !!game.S.flags.started;
  const scr = el('div', { class: 'title-screen' },
    el('h1', {}, 'LANTERNFALL'),
    el('div', { class: 'subtitle' }, 'Хранители Мглы'),
    el('div', { class: 'tagline' }, 'Мир утонул во Мгле. Свет — твоя земля, твоё оружие и твоя цена. Строй в круге света. Гаси чужие огни. Переживи ночь.'),
    el('button', { class: 'gold big', onclick: () => { scr.remove(); onStart(); } }, has ? 'Продолжить' : 'Начать'));
  root.append(scr);
}

game.S = load() || V.createVillage();
V.tick(game.S);
game.toVillage();
document.addEventListener('visibilitychange', () => { if (document.hidden) game.save(); });
window.addEventListener('beforeunload', () => game.save());
requestAnimationFrame(frame);

titleScreen(() => {
  if (!game.S.flags.started) {
    game.S.flags.started = true;
    game.save();
    dialogue(INTRO, () => toast('Совет: нажми на постройку, чтобы улучшить её. Перетащи — чтобы переместить.'));
  } else if (V.nightDue(game.S)) {
    toast('Мгла у порога! Нажми «Мгла идёт!»', 'bad');
  }
});

window.__game = game; // для отладки из консоли
