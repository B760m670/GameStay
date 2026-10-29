// Все балансные данные игры. Массивы по уровням: индекс 0 = уровень 1.

export const GRID = 44;          // карта GRID x GRID клеток
export const BORDER = 2;         // крайние клетки — только для высадки
export const MAX_LANTERN = 6;

export const RES = {
  ore: { name: 'Руда', icon: '⛏', color: '#e8954a' },
  aether: { name: 'Эфир', icon: '✦', color: '#86dcff' },
  shards: { name: 'Осколки', icon: '◆', color: '#ffd86b' },
};

// Лимит построек по уровню Фонаря (индекс = уровень Фонаря - 1)
export const LIMITS = {
  lantern:     [1, 1, 1, 1, 1, 1],
  beacon:      [1, 2, 2, 3, 4, 5],
  oreMine:     [2, 3, 4, 5, 6, 6],
  aetherWell:  [2, 3, 4, 5, 6, 6],
  oreStore:    [1, 1, 2, 2, 3, 3],
  aetherStore: [1, 1, 2, 2, 3, 3],
  barracks:    [1, 1, 2, 2, 2, 3],
  camp:        [1, 2, 2, 3, 3, 3],
  builderHut:  [2, 3, 3, 4, 4, 5],
  ballista:    [1, 2, 3, 3, 4, 5],
  brazier:     [0, 1, 1, 2, 2, 3],
  catapult:    [0, 0, 1, 1, 2, 2],
  prism:       [0, 0, 0, 1, 1, 2],
  wall:        [20, 35, 55, 80, 110, 150],
};

export const BUILDINGS = {
  lantern: {
    name: 'Фонарь', size: 4, maxLevel: 6, category: 'core', costRes: 'ore',
    desc: 'Сердце поселения. Держит Мглу на расстоянии. Уровень Фонаря открывает новые постройки.',
    cost: [0, 1200, 4000, 12000, 30000, 70000], time: [0, 60, 240, 600, 1200, 2400],
    hp: [1500, 2100, 2800, 3600, 4500, 5500],
    light: [7, 8, 9, 10, 11, 12], storage: [1000, 1500, 2000, 2500, 3000, 4000],
    isLight: true,
  },
  beacon: {
    name: 'Светоч', size: 2, maxLevel: 6, category: 'light', costRes: 'ore',
    desc: 'Раздвигает тьму: расширяет зону строительства и питает оборону светом. Главная цель врага.',
    cost: [500, 2000, 6000, 15000, 35000, 70000], time: [20, 90, 300, 600, 1200, 2000],
    hp: [350, 450, 560, 680, 820, 980],
    light: [5, 5.5, 6, 6.5, 7, 7.5],
    isLight: true,
  },
  oreMine: {
    name: 'Рудная шахта', size: 3, maxLevel: 6, category: 'economy', costRes: 'aether',
    desc: 'Добывает руду из-под Мглы.',
    cost: [150, 400, 1200, 3500, 8000, 16000], time: [10, 45, 150, 400, 900, 1500],
    hp: [400, 460, 530, 610, 700, 800],
    rate: [12, 20, 30, 42, 56, 72], cap: [500, 900, 1500, 2400, 3600, 5200],
    produces: 'ore',
  },
  aetherWell: {
    name: 'Эфирный колодец', size: 3, maxLevel: 6, category: 'economy', costRes: 'ore',
    desc: 'Конденсирует Эфир из тумана. Говорят, в нём слышны чужие голоса.',
    cost: [150, 400, 1200, 3500, 8000, 16000], time: [10, 45, 150, 400, 900, 1500],
    hp: [400, 460, 530, 610, 700, 800],
    rate: [12, 20, 30, 42, 56, 72], cap: [500, 900, 1500, 2400, 3600, 5200],
    produces: 'aether',
  },
  oreStore: {
    name: 'Рудный склад', size: 3, maxLevel: 6, category: 'economy', costRes: 'aether',
    desc: 'Хранит руду.',
    cost: [300, 1000, 3000, 8000, 20000, 45000], time: [15, 60, 200, 500, 1000, 1800],
    hp: [700, 850, 1000, 1200, 1450, 1700],
    storage: [3000, 7000, 15000, 35000, 75000, 150000], stores: 'ore',
  },
  aetherStore: {
    name: 'Эфирный резервуар', size: 3, maxLevel: 6, category: 'economy', costRes: 'ore',
    desc: 'Хранит Эфир.',
    cost: [300, 1000, 3000, 8000, 20000, 45000], time: [15, 60, 200, 500, 1000, 1800],
    hp: [700, 850, 1000, 1200, 1450, 1700],
    storage: [3000, 7000, 15000, 35000, 75000, 150000], stores: 'aether',
  },
  barracks: {
    name: 'Казарма', size: 3, maxLevel: 6, category: 'army', costRes: 'aether',
    desc: 'Тренирует войска. Каждый уровень открывает новый отряд. Несколько казарм ускоряют обучение.',
    cost: [200, 800, 2500, 7000, 18000, 40000], time: [10, 60, 200, 500, 1000, 1800],
    hp: [450, 520, 600, 690, 790, 900],
  },
  camp: {
    name: 'Лагерь', size: 3, maxLevel: 6, category: 'army', costRes: 'aether',
    desc: 'Здесь отдыхает армия. Определяет её размер.',
    cost: [250, 900, 2800, 7500, 19000, 42000], time: [10, 60, 200, 500, 1000, 1800],
    hp: [350, 400, 460, 530, 610, 700],
    capacity: [20, 30, 40, 55, 70, 85],
  },
  builderHut: {
    name: 'Хижина строителя', size: 2, maxLevel: 1, category: 'economy', costRes: 'shards',
    desc: 'Каждая хижина — ещё один строитель.',
    cost: [0], time: [0], hp: [250],
  },
  ballista: {
    name: 'Арбалет', size: 3, maxLevel: 6, category: 'defense', costRes: 'ore',
    desc: 'Точный выстрел по одной цели. Бьёт и наземных, и летающих.',
    cost: [250, 900, 2800, 7500, 18000, 40000], time: [15, 60, 200, 500, 1000, 1800],
    hp: [420, 520, 640, 780, 940, 1120],
    range: 8.5, cd: 0.9, dmg: [20, 27, 36, 47, 60, 76], ground: true, air: true,
    proj: { kind: 'bolt', speed: 16 },
  },
  brazier: {
    name: 'Жаровня', size: 2, maxLevel: 5, category: 'defense', costRes: 'ore',
    desc: 'Выплёвывает пламя по площади вблизи. Гроза толп.',
    cost: [600, 1800, 5000, 12000, 28000], time: [30, 120, 400, 800, 1500],
    hp: [550, 650, 770, 900, 1050],
    range: 4.5, cd: 0.5, dmg: [9, 12, 16, 21, 27], splash: 1.4, ground: true, air: false,
    proj: { kind: 'flame', speed: 0 },
  },
  catapult: {
    name: 'Катапульта', size: 3, maxLevel: 4, category: 'defense', costRes: 'ore',
    desc: 'Мечет камни очень далеко. Не попадает по тем, кто подошёл вплотную.',
    cost: [2500, 7000, 16000, 35000], time: [120, 400, 900, 1600],
    hp: [700, 820, 960, 1120],
    range: 11, minRange: 3.5, cd: 3.5, dmg: [55, 72, 94, 120], splash: 1.6, ground: true, air: false,
    proj: { kind: 'rock', speed: 7 },
  },
  prism: {
    name: 'Призма', size: 2, maxLevel: 3, category: 'defense', costRes: 'ore',
    desc: 'Фокусирует свет в луч. Чем дольше держит цель — тем сильнее жжёт.',
    cost: [9000, 22000, 48000], time: [600, 1200, 2000],
    hp: [800, 950, 1120],
    range: 7, dps: [25, 33, 43], rampMax: 4, rampRate: 2, ground: true, air: true,
    beam: true,
  },
  wall: {
    name: 'Стена', size: 1, maxLevel: 6, category: 'wall', costRes: 'ore',
    desc: 'Задерживает наземных врагов.',
    cost: [50, 150, 500, 1200, 3000, 7000], time: [0, 0, 0, 0, 0, 0],
    hp: [300, 550, 900, 1400, 2100, 3000],
  },
};

export const DEFENSES = ['ballista', 'brazier', 'catapult', 'prism'];
export const HUT_COST = [0, 0, 20, 40, 80]; // осколков за N-ю хижину

// Войска Хранителя. unlock = нужный уровень казармы.
export const TROOPS = {
  torch: {
    name: 'Факельщик', housing: 1, cost: 25, time: 4, unlock: 1,
    hp: 70, dmg: 9, cd: 0.8, range: 0.6, speed: 1.5, target: 'any',
    color: '#e8a04a', desc: 'Дешёвый и смелый. Бьёт всё, что ближе.',
  },
  archer: {
    name: 'Стрелок', housing: 1, cost: 50, time: 6, unlock: 2,
    hp: 35, dmg: 8, cd: 0.8, range: 3.5, speed: 1.5, target: 'any', ranged: 'arrow',
    color: '#9be06a', desc: 'Стреляет через стены. Хрупок.',
  },
  shield: {
    name: 'Щитоносец', housing: 5, cost: 250, time: 20, unlock: 3,
    hp: 700, dmg: 30, cd: 1.5, range: 0.7, speed: 0.9, target: 'defense',
    color: '#8aa0c8', desc: 'Медленный танк. Идёт прямо к обороне.',
  },
  sapper: {
    name: 'Подрывник', housing: 2, cost: 300, time: 15, unlock: 4,
    hp: 45, dmg: 450, cd: 0, range: 0.6, speed: 2.2, target: 'wall', suicide: true, splash: 1.3,
    color: '#d06060', desc: 'Бежит к ближайшей стене и взрывается.',
  },
  shade: {
    name: 'Тенекрад', housing: 3, cost: 400, time: 20, unlock: 5,
    hp: 200, dmg: 22, cd: 0.6, range: 0.7, speed: 2.0, target: 'light', lightMult: 3, stealth: true,
    color: '#9a7ad8', desc: 'Невидим во тьме. Тройной урон по Фонарю и Светочам.',
  },
  owl: {
    name: 'Филин', housing: 4, cost: 500, time: 25, unlock: 6,
    hp: 260, dmg: 24, cd: 0.9, range: 2.5, speed: 1.8, target: 'defense', air: true, ranged: 'feather',
    color: '#d8c49a', desc: 'Летает над стенами. Уязвим для Арбалетов и Призм.',
  },
};

// Порождения Мглы — атакуют по ночам.
export const HAZE = {
  morok: {
    name: 'Морок', housing: 1, unlockLL: 1,
    hp: 60, dmg: 8, cd: 0.8, range: 0.6, speed: 1.8, target: 'any', color: '#6b5a8a',
  },
  brute: {
    name: 'Громила Мглы', housing: 5, unlockLL: 2,
    hp: 600, dmg: 35, cd: 1.6, range: 0.7, speed: 0.85, target: 'defense', color: '#4a3d63',
  },
  leech: {
    name: 'Светоед', housing: 3, unlockLL: 3,
    hp: 160, dmg: 18, cd: 0.6, range: 0.7, speed: 1.6, target: 'light', lightMult: 3, stealth: true, color: '#3a2a55',
  },
  wraith: {
    name: 'Призрак', housing: 4, unlockLL: 4,
    hp: 200, dmg: 18, cd: 1.0, range: 2.2, speed: 1.6, target: 'any', air: true, ranged: 'wisp', color: '#a090c8',
  },
};

export const ECLIPSE = { name: 'Затмение', radius: 3.5, duration: 10 };
export function eclipseCharges(LL) { return LL >= 2 ? Math.floor(LL / 2) : 0; }

export const BATTLE_TIME = 180;
export const NIGHT_TIME = 90;
export const NIGHT_INTERVAL_MS = 20 * 60 * 1000;
export const FIRST_NIGHT_MS = 8 * 60 * 1000;

export const RAID_SEARCH_COST = [50, 150, 400, 900, 1800, 3000];
export const BASE_LOOT = [800, 2000, 5000, 12000, 25000, 45000];
