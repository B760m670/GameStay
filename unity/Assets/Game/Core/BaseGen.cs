// Процедурная генерация вражеских баз и ночных волн Мглы.
using System;
using System.Collections.Generic;
using System.Linq;

namespace Lanternfall.Core
{
    public static class BaseGen
    {
        static readonly string[] NamesA = { "Серый", "Угасший", "Безмолвный", "Хромой", "Пепельный", "Слепой", "Ржавый", "Последний", "Одинокий", "Тусклый" };
        static readonly string[] NamesB = { "Ольгерд", "Мирава", "Тарн", "Ивица", "Бранко", "Ведана", "Корвин", "Ярена", "Стигг", "Лучезар", "Агния", "Ратибор" };

        public static string RivalName(Rng rng) => $"{NamesA[rng.Range(NamesA.Length)]} {NamesB[rng.Range(NamesB.Length)]}";

        public static List<LayoutItem> Generate(int LL, int seed, Dictionary<BType, int> counts = null, float richness = 1f, int levelBias = 0)
        {
            var rng = new Rng(seed);
            int G = GameConfig.Grid, BR = GameConfig.Border;
            var layout = new List<LayoutItem>();
            var occ = new byte[G * G]; // 0 пусто, 1 здание, 2 стена
            var sources = new List<LightSource>();

            int Lvl(BType t)
            {
                int max = Village.MaxLevelAllowed(t, LL);
                if (max <= 0) return 1;
                int l = max - (rng.Next() < 0.4 ? 1 : 0) + levelBias;
                return Math.Max(1, Math.Min(Math.Min(GameConfig.B(t).MaxLevel, max), l));
            }
            int Count(BType t) => counts != null && counts.TryGetValue(t, out int c) ? c : GameConfig.Limit(t, LL);

            bool Free(int x, int y, int s, int margin)
            {
                if (x < BR || y < BR || x + s > G - BR || y + s > G - BR) return false;
                for (int j = y - margin; j < y + s + margin; j++)
                    for (int i = x - margin; i < x + s + margin; i++)
                    {
                        if (i < 0 || j < 0 || i >= G || j >= G) continue;
                        bool inside = i >= x && i < x + s && j >= y && j < y + s;
                        byte v = occ[j * G + i];
                        if (v == 1 || (inside && v == 2)) return false;
                    }
                return true;
            }
            LayoutItem Put(BType t, int x, int y, int level)
            {
                int s = GameConfig.B(t).Size;
                for (int j = y; j < y + s; j++) for (int i = x; i < x + s; i++) occ[j * G + i] = (byte)(t == BType.Wall ? 2 : 1);
                var it = new LayoutItem { Type = t, X = x, Y = y, Level = level, Id = layout.Count };
                layout.Add(it);
                var d = GameConfig.B(t);
                if (d.IsLight) sources.Add(new LightSource(x + s / 2f, y + s / 2f, d.Lv(d.Light, level), it.Id));
                return it;
            }

            int C = G / 2;
            var lantern = Put(BType.Lantern, C - 2, C - 2, LL);
            float lr = GameConfig.B(BType.Lantern).Lv(GameConfig.B(BType.Lantern).Light, LL);

            // Светочи — по кругу у края света Фонаря.
            int nBeacons = Count(BType.Beacon);
            double a0 = rng.Next() * Math.PI * 2;
            for (int k = 0; k < nBeacons; k++)
            {
                int level = Lvl(BType.Beacon);
                for (int tries = 0; tries < 40; tries++)
                {
                    double a = a0 + k * Math.PI * 2 / Math.Max(1, nBeacons) + (rng.Next() - 0.5) * 0.6;
                    double d = lr - 1.8 - rng.Next() * 1.5 - tries * 0.05;
                    int x = (int)Math.Round(C + Math.Cos(a) * d - 1), y = (int)Math.Round(C + Math.Sin(a) * d - 1);
                    if (Free(x, y, 2, 1) && Geo.FootprintLit(x, y, 2, sources)) { Put(BType.Beacon, x, y, level); break; }
                }
            }

            // Стены — квадратные кольца вокруг Фонаря.
            int walls = Count(BType.Wall);
            int wallLevel = Lvl(BType.Wall);
            List<(int, int)> Ring(int k)
            {
                int x0 = lantern.X - k, y0 = lantern.Y - k, x1 = lantern.X + 3 + k, y1 = lantern.Y + 3 + k;
                var tiles = new List<(int, int)>();
                for (int i = x0; i <= x1; i++) { tiles.Add((i, y0)); tiles.Add((i, y1)); }
                for (int j = y0 + 1; j < y1; j++) { tiles.Add((x0, j)); tiles.Add((x1, j)); }
                return tiles;
            }
            bool TryRing(int k)
            {
                var all = Ring(k);
                if (!all.All(p => Geo.IsLit(p.Item1 + 0.5f, p.Item2 + 0.5f, sources))) return false;
                var tiles = all.Where(p => Free(p.Item1, p.Item2, 1, 0)).ToList();
                if (tiles.Count == 0 || tiles.Count > walls) return false;
                foreach (var p in tiles) Put(BType.Wall, p.Item1, p.Item2, wallLevel);
                walls -= tiles.Count;
                return true;
            }
            var rings = new List<int>();
            if (walls >= 100 && TryRing(3)) rings.Add(3);
            for (int k = 9; k >= 1; k--)
            {
                if (rings.Any(r => Math.Abs(r - k) < 3)) continue;
                if (TryRing(k)) { rings.Add(k); break; }
            }

            // Остальные постройки: core — ближе к центру, def — равномерно, out — на окраине.
            var order = new (BType, string)[]
            {
                (BType.OreStore, "core"), (BType.AetherStore, "core"),
                (BType.Prism, "def"), (BType.Catapult, "def"), (BType.Brazier, "def"), (BType.Ballista, "def"),
                (BType.Barracks, "out"), (BType.Camp, "out"), (BType.OreMine, "out"), (BType.AetherWell, "out"), (BType.BuilderHut, "out"),
            };
            var placedByType = new Dictionary<BType, List<LayoutItem>>();
            foreach (var (type, role) in order)
            {
                int n = Count(type), s = GameConfig.B(type).Size;
                for (int k = 0; k < n; k++)
                {
                    (int x, int y)? best = null; double bestScore = double.MaxValue;
                    for (int c = 0; c < 260; c++)
                    {
                        int x = (int)Math.Floor(BR + rng.Next() * (G - 2 * BR - s));
                        int y = (int)Math.Floor(BR + rng.Next() * (G - 2 * BR - s));
                        if (!Free(x, y, s, 1) || !Geo.FootprintLit(x, y, s, sources)) continue;
                        double d = Geo.Hypot(x + s / 2f - C, y + s / 2f - C), score;
                        if (role == "core") score = d;
                        else if (role == "def") score = Math.Abs(d - (3 + (k * 2.7) % (lr + 1))) * 0.7;
                        else score = -d * 0.6;
                        if (placedByType.TryGetValue(type, out var same))
                            foreach (var o in same)
                            {
                                double od = Geo.Hypot(o.X - x, o.Y - y);
                                if (od < 7) score += (7 - od) * 1.2;
                            }
                        score += rng.Next() * 2.5;
                        if (score < bestScore) { bestScore = score; best = (x, y); }
                    }
                    if (best == null) continue;
                    var it = Put(type, best.Value.x, best.Value.y, type == BType.BuilderHut ? 1 : Lvl(type));
                    if (!placedByType.ContainsKey(type)) placedByType[type] = new List<LayoutItem>();
                    placedByType[type].Add(it);
                }
            }

            // Добыча.
            float pool = GameConfig.BaseLoot[LL - 1] * richness;
            void Share(BType t, float frac, bool ore)
            {
                var items = layout.Where(b => b.Type == t).ToList();
                foreach (var it in items)
                    if (ore) it.LootOre += pool * frac / items.Count; else it.LootAether += pool * frac / items.Count;
            }
            Share(BType.OreStore, 0.6f, true); Share(BType.OreMine, 0.25f, true);
            Share(BType.AetherStore, 0.6f, false); Share(BType.AetherWell, 0.25f, false);
            lantern.LootOre = pool * 0.15f; lantern.LootAether = pool * 0.15f;
            return layout;
        }

        /// Ночь: состав волн по уровню Фонаря и числу пережитых ночей.
        public static (List<Wave> waves, float hazeMult) NightWaves(int LL, int nights, int seed)
        {
            var rng = new Rng(seed);
            int budget = 10 + 14 * (LL - 1) + 2 * Math.Min(nights, 20);
            var types = GameConfig.HazeTroops.Where(t => GameConfig.T(t).Unlock <= LL).ToList();
            var waves = new List<Wave> { new Wave { T = 2 }, new Wave { T = 22 }, new Wave { T = 45 } };
            float[] shares = { 0.3f, 0.35f, 0.35f };
            for (int i = 0; i < waves.Count; i++)
            {
                int left = (int)Math.Round(budget * shares[i]);
                while (left > 0)
                {
                    var opts = types.Where(t => GameConfig.T(t).Housing <= left).ToList();
                    if (opts.Count == 0) break;
                    var t = rng.Next() < 0.45 ? TroopType.Morok : opts[rng.Range(opts.Count)];
                    if (GameConfig.T(t).Housing > left) break;
                    waves[i].Units[t] = (waves[i].Units.TryGetValue(t, out int c) ? c : 0) + 1;
                    left -= GameConfig.T(t).Housing;
                }
            }
            return (waves, 1f + 0.05f * Math.Min(nights, 30));
        }
    }
}
