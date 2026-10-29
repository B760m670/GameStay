import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createVillage, canPlace, placeBuilding, tick, collect, train, startUpgrade, villageLayout, lanternLevel } from '../src/core/village.js';
import { centerOf, lightSources, isPointLit } from '../src/core/grid.js';
import { BUILDINGS, LIMITS, GRID } from '../src/core/config.js';
import { createBattle, deploy, simulate, canDeploy } from '../src/core/battle.js';
import { generateBase, nightWaves } from '../src/core/basegen.js';
import { MISSIONS } from '../src/core/campaign.js';

test('стартовая деревня: все постройки в свете и не пересекаются', () => {
  const S = createVillage(0);
  for (const b of S.buildings) {
    const r = canPlace(S, b.type, b.x, b.y, b.id);
    assert.ok(r.ok, `${b.type}: ${r.reason}`);
  }
});

test('нельзя строить во тьме, можно в свете', () => {
  const S = createVillage(0);
  assert.equal(canPlace(S, 'wall', 3, 3).ok, false);
  assert.equal(canPlace(S, 'wall', 19, 19).ok, true);
});

test('стройка, таймеры, добыча', () => {
  const S = createVillage(0);
  const r = placeBuilding(S, 'wall', 19, 19, 0);
  assert.ok(r.ok);
  const lantern = S.buildings.find((b) => b.type === 'lantern');
  S.res.ore = 5000;
  assert.ok(startUpgrade(S, lantern.id, 0).ok);
  tick(S, 61000);
  assert.equal(lanternLevel(S), 2);
  const mine = S.buildings.find((b) => b.type === 'oreMine');
  const got = collect(S, mine.id, 120000);
  assert.ok(got > 0);
});

test('тренировка войск', () => {
  const S = createVillage(0);
  S.army = {};
  assert.ok(train(S, 'torch', 0).ok);
  tick(S, 10000);
  assert.equal(S.army.torch, 1);
});

test('генератор баз: все уровни Фонаря, всё в свете, без пересечений', () => {
  for (let LL = 1; LL <= 6; LL++) {
    for (let seed = 1; seed <= 5; seed++) {
      const layout = generateBase({ LL, seed });
      const occ = new Set();
      const src = lightSources(layout.map((b, i) => ({ ...b, id: i })));
      for (const b of layout) {
        const s = BUILDINGS[b.type].size;
        for (let j = b.y; j < b.y + s; j++) for (let i = b.x; i < b.x + s; i++) {
          const k = j * GRID + i;
          assert.ok(!occ.has(k), `overlap LL${LL} seed${seed} ${b.type}`);
          occ.add(k);
        }
        const c = centerOf(b);
        assert.ok(isPointLit(c.x, c.y, src), `unlit ${b.type} LL${LL}`);
        assert.ok(b.level >= 1 && b.level <= BUILDINGS[b.type].maxLevel);
      }
      const defs = layout.filter((b) => b.type === 'ballista').length;
      assert.ok(defs >= Math.min(1, LIMITS.ballista[LL - 1]));
    }
  }
});

test('бой: высадка запрещена в свете, разрешена во тьме', () => {
  const B = createBattle({ layout: generateBase({ LL: 2, seed: 3 }), army: { torch: 5 } });
  assert.equal(canDeploy(B, 22, 22), false);
  assert.equal(canDeploy(B, 1, 1), true);
  assert.equal(deploy(B, 'torch', 1, 1), true);
});

test('бой: сильная армия разносит базу 1 уровня, пустая — нет', () => {
  const layout = generateBase({ LL: 1, seed: 7 });
  const B = createBattle({ layout, army: { torch: 60 } , seed: 1});
  for (let k = 0; k < 60; k++) deploy(B, 'torch', 1 + (k % 10) * 0.3, 1 + Math.floor(k / 10) * 0.3);
  const r = simulate(B);
  assert.ok(r.stars >= 2, `stars=${r.stars} pct=${r.pct}`);
  assert.ok(r.loot.ore > 0);
  const B2 = createBattle({ layout, army: { torch: 3 } });
  for (let k = 0; k < 3; k++) deploy(B2, 'torch', 1, 1);
  const r2 = simulate(B2);
  assert.ok(r2.stars < 3);
});

test('ночь: стартовая деревня обычно переживает первую ночь', () => {
  let wins = 0;
  for (let seed = 1; seed <= 6; seed++) {
    const S = createVillage(0);
    const { waves, hazeMult } = nightWaves(1, 0, seed);
    const B = createBattle({ mode: 'defense', layout: villageLayout(S), waves, hazeMult, duration: 90, seed });
    const r = simulate(B);
    if (r.win) wins++;
  }
  assert.ok(wins >= 4, `wins=${wins}`);
});

test('миссии кампании генерируются', () => {
  for (const m of MISSIONS) {
    const layout = generateBase({ LL: m.LL, seed: m.seed, counts: m.counts || {}, richness: m.richness, levelBias: m.levelBias || 0 });
    assert.ok(layout.find((b) => b.type === 'lantern'));
  }
});
