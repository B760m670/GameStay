// Логика деревни: постройка, улучшение, добыча, армия. Состояние сериализуемо (JsonUtility-совместимо).
using System;
using System.Collections.Generic;
using System.Linq;

namespace Lanternfall.Core
{
    [Serializable]
    public class BuildingRec : IPlaced
    {
        public int id;
        public BType type;
        public int x, y, level;
        public long busyUntil;   // мс, 0 = не строится
        public double acc;       // накоплено в шахте
        public long accAt;
        public BType Type => type;
        public int X => x;
        public int Y => y;
        public int Level => level;
        public int Id => id;
        public BuildingDef Def => GameConfig.B(type);
    }

    [Serializable] public class TrainItem { public TroopType type; public float left; }
    [Serializable] public class MissionStars { public string id; public int stars; }
    [Serializable] public class Stats { public int attacks, wins, nightsWon, nightsLost; }

    [Serializable]
    public class VillageState
    {
        public const int SaveVersion = 1;
        public int version = SaveVersion;
        public long created;
        public long ore, aether, shards;
        public List<BuildingRec> buildings = new List<BuildingRec>();
        public int nextId = 1;
        public int[] army = new int[GameConfig.PlayerTroops.Length];
        public List<TrainItem> trainQueue = new List<TrainItem>();
        public long trainTickAt;
        public List<MissionStars> campaign = new List<MissionStars>();
        public List<string> codex = new List<string> { "intro" };
        public long nightAt;
        public int nights;
        public Stats stats = new Stats();
        public bool started;

        public long GetRes(Res r) => r == Res.Ore ? ore : r == Res.Aether ? aether : shards;
        public void SetRes(Res r, long v) { if (r == Res.Ore) ore = v; else if (r == Res.Aether) aether = v; else shards = v; }
        public int Stars(string missionId) => campaign.FirstOrDefault(c => c.id == missionId)?.stars ?? 0;
        public void SetStars(string missionId, int s)
        {
            var e = campaign.FirstOrDefault(c => c.id == missionId);
            if (e == null) campaign.Add(new MissionStars { id = missionId, stars = s });
            else e.stars = Math.Max(e.stars, s);
        }
    }

    public struct Result
    {
        public bool Ok;
        public string Reason;
        public static Result Yes => new Result { Ok = true };
        public static Result No(string r) => new Result { Ok = false, Reason = r };
    }

    public struct UpgradeInfo
    {
        public bool Ok, Max;
        public string Reason;
        public int Cost, Time, Next;
        public Res Res;
    }

    public static class Village
    {
        public static int TroopIndex(TroopType t) => Array.IndexOf(GameConfig.PlayerTroops, t);

        public static VillageState Create(long now)
        {
            var S = new VillageState { created = now, ore = 1500, aether = 1500, shards = 15, trainTickAt = now, nightAt = now + GameConfig.FirstNightMs };
            void Add(BType t, int x, int y) => S.buildings.Add(Make(S, t, x, y, 1, now));
            Add(BType.Lantern, 20, 20);
            Add(BType.OreMine, 15, 19);
            Add(BType.AetherWell, 26, 21);
            Add(BType.OreStore, 20, 25);
            Add(BType.AetherStore, 21, 15);
            Add(BType.Barracks, 24, 24);
            Add(BType.Camp, 16, 23);
            Add(BType.Ballista, 24, 17);
            Add(BType.BuilderHut, 18, 17);
            Add(BType.BuilderHut, 18, 26);
            S.army[TroopIndex(TroopType.Torch)] = 10;
            return S;
        }

        static BuildingRec Make(VillageState S, BType t, int x, int y, int level, long now) =>
            new BuildingRec { id = S.nextId++, type = t, x = x, y = y, level = level, accAt = now };

        public static BuildingRec Get(VillageState S, int id) => S.buildings.FirstOrDefault(b => b.id == id);
        public static int LanternLevel(VillageState S) => S.buildings.FirstOrDefault(b => b.type == BType.Lantern)?.level ?? 1;
        public static int CountOf(VillageState S, BType t) => S.buildings.Count(b => b.type == t);

        public static int MaxLevelAllowed(BType t, int lantern)
        {
            if (t == BType.Lantern) return GameConfig.MaxLantern;
            return Math.Max(0, Math.Min(GameConfig.B(t).MaxLevel, lantern - GameConfig.UnlockLantern(t) + 1));
        }

        public static (long ore, long aether) StorageCap(VillageState S)
        {
            long ore = 0, aether = 0;
            foreach (var b in S.buildings)
            {
                if (b.level < 1) continue;
                var d = b.Def;
                if (b.type == BType.Lantern) { ore += d.Lv(d.Storage, b.level); aether += d.Lv(d.Storage, b.level); }
                else if (d.Stores == Res.Ore) ore += d.Lv(d.Storage, b.level);
                else if (d.Stores == Res.Aether) aether += d.Lv(d.Storage, b.level);
            }
            return (ore, aether);
        }

        public static long Cap(VillageState S, Res r)
        {
            if (r == Res.Shards) return long.MaxValue;
            var c = StorageCap(S);
            return r == Res.Ore ? c.ore : c.aether;
        }

        /// Добавляет ресурс с учётом вместимости. Возвращает фактически добавленное.
        public static long AddRes(VillageState S, Res r, long amount)
        {
            long before = S.GetRes(r);
            long after = Math.Max(0, Math.Min(Cap(S, r), before + amount));
            if (r == Res.Shards) after = Math.Max(0, before + amount);
            S.SetRes(r, after);
            return after - before;
        }

        public static (int total, int busy, int free) Builders(VillageState S, long now)
        {
            int total = S.buildings.Count(b => b.type == BType.BuilderHut && b.level >= 1);
            int busy = S.buildings.Count(b => b.busyUntil > now);
            return (total, busy, total - busy);
        }

        public static int ArmyHousing(VillageState S)
        {
            int n = 0;
            for (int i = 0; i < S.army.Length; i++) n += GameConfig.T(GameConfig.PlayerTroops[i]).Housing * S.army[i];
            return n;
        }
        public static int QueueHousing(VillageState S) => S.trainQueue.Sum(q => GameConfig.T(q.type).Housing);
        public static int CampCapacity(VillageState S) =>
            S.buildings.Where(b => b.type == BType.Camp && b.level >= 1).Sum(b => b.Def.Lv(b.Def.Capacity, b.level));
        public static int BarracksLevel(VillageState S) =>
            S.buildings.Where(b => b.type == BType.Barracks).Select(b => b.level).DefaultIfEmpty(0).Max();
        public static int BarracksCount(VillageState S) => S.buildings.Count(b => b.type == BType.Barracks && b.level >= 1);

        // --- Добыча ---
        public static double CollectorAmount(BuildingRec b, long now)
        {
            var d = b.Def;
            if (d.Produces == null || b.level < 1) return 0;
            double gained = d.Lv(d.Rate, b.level) * Math.Max(0, now - b.accAt) / 60000.0;
            return Math.Min(d.Lv(d.Cap, b.level), b.acc + gained);
        }

        public static long Collect(VillageState S, int id, long now)
        {
            var b = Get(S, id);
            if (b == null || b.Def.Produces == null) return 0;
            double amount = CollectorAmount(b, now);
            long got = AddRes(S, b.Def.Produces.Value, (long)Math.Floor(amount));
            b.acc = amount - got;
            b.accAt = now;
            return got;
        }

        // --- Размещение ---
        public static Result CanPlace(VillageState S, BType t, int x, int y, int ignoreId = int.MinValue)
        {
            var def = GameConfig.B(t);
            int n = GameConfig.Grid, br = GameConfig.Border, s = def.Size;
            if (x < br || y < br || x + s > n - br || y + s > n - br) return Result.No("За краем мира");
            foreach (var b in S.buildings)
                if (b.id != ignoreId && Geo.RectsOverlap(x, y, s, b.x, b.y, b.Def.Size)) return Result.No("Место занято");
            var sources = Geo.Sources(S.buildings, ignoreId);
            if (t != BType.Lantern && !Geo.FootprintLit(x, y, s, sources)) return Result.No("Здесь тьма — нужен свет");
            if (def.IsLight && ignoreId != int.MinValue)
            {
                // источник света переносят — остальные постройки не должны остаться во тьме
                var moved = S.buildings.Select(b => b.id == ignoreId ? new BuildingRec { id = b.id, type = b.type, x = x, y = y, level = b.level } : b);
                var src = Geo.Sources(moved);
                foreach (var b in S.buildings)
                {
                    if (b.id == ignoreId) continue;
                    float half = b.Def.Size / 2f;
                    if (!Geo.IsLit(b.x + half, b.y + half, src)) return Result.No("Другие постройки останутся во тьме");
                }
            }
            return Result.Yes;
        }

        public static (Res res, int amount) BuildCost(VillageState S, BType t)
        {
            if (t == BType.BuilderHut)
                return (Res.Shards, GameConfig.HutCost[Math.Min(CountOf(S, t), GameConfig.HutCost.Length - 1)]);
            var d = GameConfig.B(t);
            return (d.CostRes, d.Cost[0]);
        }

        public static Result CanBuild(VillageState S, BType t, long now)
        {
            int LL = LanternLevel(S);
            int lim = GameConfig.Limit(t, LL);
            if (lim == 0) return Result.No($"Нужен Фонарь ур. {GameConfig.UnlockLantern(t)}");
            if (CountOf(S, t) >= lim) return Result.No($"Лимит {lim} — улучши Фонарь");
            var c = BuildCost(S, t);
            if (S.GetRes(c.res) < c.amount) return Result.No("Не хватает ресурсов");
            if (GameConfig.B(t).Time[0] > 0 && Builders(S, now).free < 1) return Result.No("Все строители заняты");
            return Result.Yes;
        }

        public static Result Place(VillageState S, BType t, int x, int y, long now, out BuildingRec placed)
        {
            placed = null;
            var can = CanBuild(S, t, now);
            if (!can.Ok) return can;
            var pl = CanPlace(S, t, x, y);
            if (!pl.Ok) return pl;
            var c = BuildCost(S, t);
            S.SetRes(c.res, S.GetRes(c.res) - c.amount);
            int time = GameConfig.B(t).Time[0];
            placed = Make(S, t, x, y, time > 0 ? 0 : 1, now);
            if (time > 0) placed.busyUntil = now + time * 1000L;
            S.buildings.Add(placed);
            return Result.Yes;
        }

        public static Result Move(VillageState S, int id, int x, int y)
        {
            var b = Get(S, id);
            if (b == null) return Result.No("Нет постройки");
            var pl = CanPlace(S, b.type, x, y, id);
            if (!pl.Ok) return pl;
            b.x = x; b.y = y;
            return Result.Yes;
        }

        // --- Улучшение ---
        public static UpgradeInfo GetUpgradeInfo(VillageState S, int id, long now)
        {
            var b = Get(S, id);
            var d = b.Def;
            int next = b.level + 1;
            if (b.busyUntil > now) return new UpgradeInfo { Reason = "Уже строится" };
            if (next > d.MaxLevel) return new UpgradeInfo { Reason = "Максимальный уровень", Max = true };
            if (next > MaxLevelAllowed(b.type, LanternLevel(S))) return new UpgradeInfo { Reason = "Нужен Фонарь выше" };
            var info = new UpgradeInfo { Cost = d.Cost[next - 1], Time = d.Time[next - 1], Res = d.CostRes, Next = next };
            if (S.GetRes(info.Res) < info.Cost) { info.Reason = "Не хватает ресурсов"; return info; }
            if (info.Time > 0 && Builders(S, now).free < 1) { info.Reason = "Все строители заняты"; return info; }
            info.Ok = true;
            return info;
        }

        public static Result StartUpgrade(VillageState S, int id, long now)
        {
            var info = GetUpgradeInfo(S, id, now);
            if (!info.Ok) return Result.No(info.Reason);
            var b = Get(S, id);
            S.SetRes(info.Res, S.GetRes(info.Res) - info.Cost);
            if (info.Time > 0) b.busyUntil = now + info.Time * 1000L;
            else FinishBuild(b, now);
            return Result.Yes;
        }

        static void FinishBuild(BuildingRec b, long now)
        {
            if (b.Def.Produces != null) { b.acc = CollectorAmount(b, now); b.accAt = now; }
            b.level += 1;
            b.busyUntil = 0;
        }

        public static int SpeedUpCost(BuildingRec b, long now) =>
            Math.Max(1, (int)Math.Ceiling(Math.Max(0, b.busyUntil - now) / 1000.0 / 60.0));

        public static Result SpeedUp(VillageState S, int id, long now)
        {
            var b = Get(S, id);
            if (b == null || b.busyUntil <= now) return Result.No("Нечего ускорять");
            int cost = SpeedUpCost(b, now);
            if (S.shards < cost) return Result.No("Не хватает осколков");
            S.shards -= cost;
            b.busyUntil = now;
            Tick(S, now);
            return Result.Yes;
        }

        // --- Армия ---
        public static Result CanTrain(VillageState S, TroopType t)
        {
            var d = GameConfig.T(t);
            if (BarracksLevel(S) < d.Unlock) return Result.No($"Нужна казарма ур. {d.Unlock}");
            if (BarracksCount(S) < 1) return Result.No("Нет готовой казармы");
            if (ArmyHousing(S) + QueueHousing(S) + d.Housing > CampCapacity(S)) return Result.No("Лагеря полны");
            if (S.aether < d.Cost) return Result.No("Не хватает Эфира");
            return Result.Yes;
        }

        public static Result Train(VillageState S, TroopType t, long now)
        {
            var c = CanTrain(S, t);
            if (!c.Ok) return c;
            if (S.trainQueue.Count == 0) S.trainTickAt = now;
            S.aether -= GameConfig.T(t).Cost;
            S.trainQueue.Add(new TrainItem { type = t, left = GameConfig.T(t).Time });
            return Result.Yes;
        }

        public static void CancelTrain(VillageState S, int index)
        {
            if (index < 0 || index >= S.trainQueue.Count) return;
            var q = S.trainQueue[index];
            S.trainQueue.RemoveAt(index);
            AddRes(S, Res.Aether, GameConfig.T(q.type).Cost);
        }

        // --- Время ---
        public static void Tick(VillageState S, long now)
        {
            foreach (var b in S.buildings)
                if (b.busyUntil != 0 && b.busyUntil <= now) FinishBuild(b, Math.Min(now, b.busyUntil));
            int nb = BarracksCount(S);
            if (S.trainQueue.Count > 0 && nb > 0)
            {
                double budget = (now - S.trainTickAt) / 1000.0 * nb;
                while (S.trainQueue.Count > 0 && budget > 0)
                {
                    var q = S.trainQueue[0];
                    double use = Math.Min(q.left, budget);
                    q.left -= (float)use;
                    budget -= use;
                    if (q.left <= 1e-4f)
                    {
                        S.trainQueue.RemoveAt(0);
                        S.army[TroopIndex(q.type)]++;
                    }
                }
            }
            S.trainTickAt = now;
        }

        public static bool NightDue(VillageState S, long now) => now >= S.nightAt;
        public static void ScheduleNextNight(VillageState S, long now) => S.nightAt = now + GameConfig.NightIntervalMs;

        /// Снимок для боя: только достроенные постройки; frac — доля запасов, которую может унести Мгла.
        public static List<LayoutItem> Layout(VillageState S, float lootFrac = 0f)
        {
            var cap = StorageCap(S);
            var list = new List<LayoutItem>();
            foreach (var b in S.buildings)
            {
                if (b.level < 1) continue;
                var it = new LayoutItem { Type = b.type, X = b.x, Y = b.y, Level = b.level };
                var d = b.Def;
                if (lootFrac > 0)
                {
                    if (d.Stores == Res.Ore) it.LootOre = S.ore * lootFrac * d.Lv(d.Storage, b.level) / Math.Max(1, cap.ore);
                    if (d.Stores == Res.Aether) it.LootAether = S.aether * lootFrac * d.Lv(d.Storage, b.level) / Math.Max(1, cap.aether);
                    if (b.type == BType.Lantern)
                    {
                        float s = d.Lv(d.Storage, b.level);
                        it.LootOre = S.ore * lootFrac * s / Math.Max(1, cap.ore);
                        it.LootAether = S.aether * lootFrac * s / Math.Max(1, cap.aether);
                    }
                }
                list.Add(it);
            }
            return list;
        }
    }

    public class LayoutItem : IPlaced
    {
        public BType Type { get; set; }
        public int X { get; set; }
        public int Y { get; set; }
        public int Level { get; set; }
        public int Id { get; set; }
        public float LootOre, LootAether;
    }
}
