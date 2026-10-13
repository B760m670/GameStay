// Все балансные данные игры. Массивы по уровням: индекс 0 = уровень 1.
// Чистый C# без зависимостей от UnityEngine — тестируется в dotnet.
using System;

namespace Lanternfall.Core
{
    public enum BType { Lantern, Beacon, OreMine, AetherWell, OreStore, AetherStore, Barracks, Camp, BuilderHut, Ballista, Brazier, Catapult, Prism, Wall }
    public enum Res { Ore, Aether, Shards }
    public enum TroopType { Torch, Archer, Shield, Sapper, Shade, Owl, Morok, Brute, Leech, Wraith }
    public enum TargetPref { Any, Defense, Light, Wall }
    public enum ProjKind { None, Bolt, Flame, Rock, Arrow, Feather, Wisp }
    public enum Category { Core, Light, Economy, Army, Defense, Wall }

    public sealed class BuildingDef
    {
        public BType Type;
        public string Name, Desc;
        public int Size, MaxLevel;
        public Category Category;
        public Res CostRes;
        public int[] Cost, Time, Hp;
        public float[] Light;           // радиус света (Фонарь, Светоч)
        public int[] Storage;           // вместимость хранилищ / Фонаря
        public int[] Rate, Cap;         // добыча в минуту и вместимость шахт
        public int[] Capacity;          // мест в лагере
        public Res? Produces, Stores;
        // оборона
        public float Range, MinRange, Cd, Splash, RampMax, RampRate, ProjSpeed;
        public int[] Dmg;
        public float[] Dps;
        public bool Ground, Air, Beam;
        public ProjKind Proj;

        public bool IsLight => Light != null;
        public bool IsDefense => Category == Category.Defense;
        public int Lv(int[] arr, int level) => arr[Math.Max(0, Math.Min(arr.Length - 1, level - 1))];
        public float Lv(float[] arr, int level) => arr[Math.Max(0, Math.Min(arr.Length - 1, level - 1))];
    }

    public sealed class TroopDef
    {
        public TroopType Type;
        public string Name, Desc, Color;
        public bool IsHaze;
        public int Housing, Cost, Time, Unlock; // Unlock: уровень казармы (войска) или Фонаря (Мгла)
        public float Hp, Dmg, Cd, Range, Speed, Splash, LightMult = 1f;
        public TargetPref Target;
        public ProjKind Ranged;
        public bool Air, Suicide, Stealth;
    }

    public static class GameConfig
    {
        public const int Grid = 44;
        public const int Border = 2;
        public const int MaxLantern = 6;

        public const float BattleTime = 180f;
        public const float NightTime = 90f;
        public const long NightIntervalMs = 20L * 60 * 1000;
        public const long FirstNightMs = 8L * 60 * 1000;

        public const float EclipseRadius = 3.5f;
        public const float EclipseDuration = 10f;
        public static int EclipseCharges(int lantern) => lantern >= 2 ? lantern / 2 : 0;

        public static readonly int[] RaidSearchCost = { 50, 150, 400, 900, 1800, 3000 };
        public static readonly int[] BaseLoot = { 800, 2000, 5000, 12000, 25000, 45000 };
        public static readonly int[] HutCost = { 0, 0, 20, 40, 80 };

        public static readonly BType[] AllBuildings = (BType[])Enum.GetValues(typeof(BType));
        public static readonly TroopType[] PlayerTroops = { TroopType.Torch, TroopType.Archer, TroopType.Shield, TroopType.Sapper, TroopType.Shade, TroopType.Owl };
        public static readonly TroopType[] HazeTroops = { TroopType.Morok, TroopType.Brute, TroopType.Leech, TroopType.Wraith };

        static readonly int[][] limits = new int[14][];
        static readonly BuildingDef[] buildings = new BuildingDef[14];
        static readonly TroopDef[] troops = new TroopDef[10];

        public static BuildingDef B(BType t) => buildings[(int)t];
        public static TroopDef T(TroopType t) => troops[(int)t];
        public static int Limit(BType t, int lantern) => limits[(int)t][Math.Max(1, Math.Min(MaxLantern, lantern)) - 1];
        public static int UnlockLantern(BType t)
        {
            var l = limits[(int)t];
            for (int i = 0; i < l.Length; i++) if (l[i] > 0) return i + 1;
            return MaxLantern + 1;
        }

        static void L(BType t, params int[] v) => limits[(int)t] = v;
        static void Def(BuildingDef d) => buildings[(int)d.Type] = d;
        static void Tr(TroopDef d) => troops[(int)d.Type] = d;

        static GameConfig()
        {
            L(BType.Lantern, 1, 1, 1, 1, 1, 1);
            L(BType.Beacon, 1, 2, 2, 3, 4, 5);
            L(BType.OreMine, 2, 3, 4, 5, 6, 6);
            L(BType.AetherWell, 2, 3, 4, 5, 6, 6);
            L(BType.OreStore, 1, 1, 2, 2, 3, 3);
            L(BType.AetherStore, 1, 1, 2, 2, 3, 3);
            L(BType.Barracks, 1, 1, 2, 2, 2, 3);
            L(BType.Camp, 1, 2, 2, 3, 3, 3);
            L(BType.BuilderHut, 2, 3, 3, 4, 4, 5);
            L(BType.Ballista, 1, 2, 3, 3, 4, 5);
            L(BType.Brazier, 0, 1, 1, 2, 2, 3);
            L(BType.Catapult, 0, 0, 1, 1, 2, 2);
            L(BType.Prism, 0, 0, 0, 1, 1, 2);
            L(BType.Wall, 20, 35, 55, 80, 110, 150);

            Def(new BuildingDef
            {
                Type = BType.Lantern, Name = "Фонарь", Size = 4, MaxLevel = 6, Category = Category.Core, CostRes = Res.Ore,
                Desc = "Сердце поселения. Держит Мглу на расстоянии. Уровень Фонаря открывает новые постройки.",
                Cost = new[] { 0, 1200, 4000, 12000, 30000, 70000 }, Time = new[] { 0, 60, 240, 600, 1200, 2400 },
                Hp = new[] { 1500, 2100, 2800, 3600, 4500, 5500 },
                Light = new[] { 7f, 8f, 9f, 10f, 11f, 12f }, Storage = new[] { 1000, 1500, 2000, 2500, 3000, 4000 },
            });
            Def(new BuildingDef
            {
                Type = BType.Beacon, Name = "Светоч", Size = 2, MaxLevel = 6, Category = Category.Light, CostRes = Res.Ore,
                Desc = "Раздвигает тьму: расширяет зону строительства и питает оборону светом. Главная цель врага.",
                Cost = new[] { 500, 2000, 6000, 15000, 35000, 70000 }, Time = new[] { 20, 90, 300, 600, 1200, 2000 },
                Hp = new[] { 350, 450, 560, 680, 820, 980 },
                Light = new[] { 5f, 5.5f, 6f, 6.5f, 7f, 7.5f },
            });
            foreach (var (t, name, res, prod, desc) in new[] {
                (BType.OreMine, "Рудная шахта", Res.Aether, Res.Ore, "Добывает руду из-под Мглы."),
                (BType.AetherWell, "Эфирный колодец", Res.Ore, Res.Aether, "Конденсирует Эфир из тумана. Говорят, в нём слышны чужие голоса.") })
            {
                Def(new BuildingDef
                {
                    Type = t, Name = name, Size = 3, MaxLevel = 6, Category = Category.Economy, CostRes = res, Desc = desc,
                    Cost = new[] { 150, 400, 1200, 3500, 8000, 16000 }, Time = new[] { 10, 45, 150, 400, 900, 1500 },
                    Hp = new[] { 400, 460, 530, 610, 700, 800 },
                    Rate = new[] { 12, 20, 30, 42, 56, 72 }, Cap = new[] { 500, 900, 1500, 2400, 3600, 5200 }, Produces = prod,
                });
            }
            foreach (var (t, name, res, stores) in new[] {
                (BType.OreStore, "Рудный склад", Res.Aether, Res.Ore),
                (BType.AetherStore, "Эфирный резервуар", Res.Ore, Res.Aether) })
            {
                Def(new BuildingDef
                {
                    Type = t, Name = name, Size = 3, MaxLevel = 6, Category = Category.Economy, CostRes = res,
                    Desc = stores == Res.Ore ? "Хранит руду." : "Хранит Эфир.",
                    Cost = new[] { 300, 1000, 3000, 8000, 20000, 45000 }, Time = new[] { 15, 60, 200, 500, 1000, 1800 },
                    Hp = new[] { 700, 850, 1000, 1200, 1450, 1700 },
                    Storage = new[] { 3000, 7000, 15000, 35000, 75000, 150000 }, Stores = stores,
                });
            }
            Def(new BuildingDef
            {
                Type = BType.Barracks, Name = "Казарма", Size = 3, MaxLevel = 6, Category = Category.Army, CostRes = Res.Aether,
                Desc = "Тренирует войска. Каждый уровень открывает новый отряд. Несколько казарм ускоряют обучение.",
                Cost = new[] { 200, 800, 2500, 7000, 18000, 40000 }, Time = new[] { 10, 60, 200, 500, 1000, 1800 },
                Hp = new[] { 450, 520, 600, 690, 790, 900 },
            });
            Def(new BuildingDef
            {
                Type = BType.Camp, Name = "Лагерь", Size = 3, MaxLevel = 6, Category = Category.Army, CostRes = Res.Aether,
                Desc = "Здесь отдыхает армия. Определяет её размер.",
                Cost = new[] { 250, 900, 2800, 7500, 19000, 42000 }, Time = new[] { 10, 60, 200, 500, 1000, 1800 },
                Hp = new[] { 350, 400, 460, 530, 610, 700 }, Capacity = new[] { 20, 30, 40, 55, 70, 85 },
            });
            Def(new BuildingDef
            {
                Type = BType.BuilderHut, Name = "Хижина строителя", Size = 2, MaxLevel = 1, Category = Category.Economy, CostRes = Res.Shards,
                Desc = "Каждая хижина — ещё один строитель.", Cost = new[] { 0 }, Time = new[] { 0 }, Hp = new[] { 250 },
            });
            Def(new BuildingDef
            {
                Type = BType.Ballista, Name = "Арбалет", Size = 3, MaxLevel = 6, Category = Category.Defense, CostRes = Res.Ore,
                Desc = "Точный выстрел по одной цели. Бьёт и наземных, и летающих.",
                Cost = new[] { 250, 900, 2800, 7500, 18000, 40000 }, Time = new[] { 15, 60, 200, 500, 1000, 1800 },
                Hp = new[] { 420, 520, 640, 780, 940, 1120 },
                Range = 8.5f, Cd = 0.9f, Dmg = new[] { 20, 27, 36, 47, 60, 76 }, Ground = true, Air = true, Proj = ProjKind.Bolt, ProjSpeed = 16f,
            });
            Def(new BuildingDef
            {
                Type = BType.Brazier, Name = "Жаровня", Size = 2, MaxLevel = 5, Category = Category.Defense, CostRes = Res.Ore,
                Desc = "Выплёвывает пламя по площади вблизи. Гроза толп.",
                Cost = new[] { 600, 1800, 5000, 12000, 28000 }, Time = new[] { 30, 120, 400, 800, 1500 },
                Hp = new[] { 550, 650, 770, 900, 1050 },
                Range = 4.5f, Cd = 0.5f, Dmg = new[] { 9, 12, 16, 21, 27 }, Splash = 1.4f, Ground = true, Proj = ProjKind.Flame,
            });
            Def(new BuildingDef
            {
                Type = BType.Catapult, Name = "Катапульта", Size = 3, MaxLevel = 4, Category = Category.Defense, CostRes = Res.Ore,
                Desc = "Мечет камни очень далеко. Не попадает по тем, кто подошёл вплотную.",
                Cost = new[] { 2500, 7000, 16000, 35000 }, Time = new[] { 120, 400, 900, 1600 },
                Hp = new[] { 700, 820, 960, 1120 },
                Range = 11f, MinRange = 3.5f, Cd = 3.5f, Dmg = new[] { 55, 72, 94, 120 }, Splash = 1.6f, Ground = true, Proj = ProjKind.Rock, ProjSpeed = 7f,
            });
            Def(new BuildingDef
            {
                Type = BType.Prism, Name = "Призма", Size = 2, MaxLevel = 3, Category = Category.Defense, CostRes = Res.Ore,
                Desc = "Фокусирует свет в луч. Чем дольше держит цель — тем сильнее жжёт.",
                Cost = new[] { 9000, 22000, 48000 }, Time = new[] { 600, 1200, 2000 },
                Hp = new[] { 800, 950, 1120 },
                Range = 7f, Dps = new[] { 25f, 33f, 43f }, RampMax = 4f, RampRate = 2f, Ground = true, Air = true, Beam = true,
            });
            Def(new BuildingDef
            {
                Type = BType.Wall, Name = "Стена", Size = 1, MaxLevel = 6, Category = Category.Wall, CostRes = Res.Ore,
                Desc = "Задерживает наземных врагов.",
                Cost = new[] { 50, 150, 500, 1200, 3000, 7000 }, Time = new[] { 0, 0, 0, 0, 0, 0 },
                Hp = new[] { 300, 550, 900, 1400, 2100, 3000 },
            });

            Tr(new TroopDef { Type = TroopType.Torch, Name = "Факельщик", Housing = 1, Cost = 25, Time = 4, Unlock = 1, Hp = 70, Dmg = 9, Cd = 0.8f, Range = 0.6f, Speed = 1.5f, Target = TargetPref.Any, Color = "#e8a04a", Desc = "Дешёвый и смелый. Бьёт всё, что ближе." });
            Tr(new TroopDef { Type = TroopType.Archer, Name = "Стрелок", Housing = 1, Cost = 50, Time = 6, Unlock = 2, Hp = 35, Dmg = 8, Cd = 0.8f, Range = 3.5f, Speed = 1.5f, Target = TargetPref.Any, Ranged = ProjKind.Arrow, Color = "#9be06a", Desc = "Стреляет через стены. Хрупок." });
            Tr(new TroopDef { Type = TroopType.Shield, Name = "Щитоносец", Housing = 5, Cost = 250, Time = 20, Unlock = 3, Hp = 700, Dmg = 30, Cd = 1.5f, Range = 0.7f, Speed = 0.9f, Target = TargetPref.Defense, Color = "#8aa0c8", Desc = "Медленный танк. Идёт прямо к обороне." });
            Tr(new TroopDef { Type = TroopType.Sapper, Name = "Подрывник", Housing = 2, Cost = 300, Time = 15, Unlock = 4, Hp = 45, Dmg = 450, Cd = 0, Range = 0.6f, Speed = 2.2f, Target = TargetPref.Wall, Suicide = true, Splash = 1.3f, Color = "#d06060", Desc = "Бежит к ближайшей стене и взрывается." });
            Tr(new TroopDef { Type = TroopType.Shade, Name = "Тенекрад", Housing = 3, Cost = 400, Time = 20, Unlock = 5, Hp = 200, Dmg = 22, Cd = 0.6f, Range = 0.7f, Speed = 2.0f, Target = TargetPref.Light, LightMult = 3, Stealth = true, Color = "#9a7ad8", Desc = "Невидим во тьме. Тройной урон по Фонарю и Светочам." });
            Tr(new TroopDef { Type = TroopType.Owl, Name = "Филин", Housing = 4, Cost = 500, Time = 25, Unlock = 6, Hp = 260, Dmg = 24, Cd = 0.9f, Range = 2.5f, Speed = 1.8f, Target = TargetPref.Defense, Air = true, Ranged = ProjKind.Feather, Color = "#d8c49a", Desc = "Летает над стенами. Уязвим для Арбалетов и Призм." });

            Tr(new TroopDef { Type = TroopType.Morok, IsHaze = true, Name = "Морок", Housing = 1, Unlock = 1, Hp = 60, Dmg = 8, Cd = 0.8f, Range = 0.6f, Speed = 1.8f, Target = TargetPref.Any, Color = "#6b5a8a" });
            Tr(new TroopDef { Type = TroopType.Brute, IsHaze = true, Name = "Громила Мглы", Housing = 5, Unlock = 2, Hp = 600, Dmg = 35, Cd = 1.6f, Range = 0.7f, Speed = 0.85f, Target = TargetPref.Defense, Color = "#4a3d63" });
            Tr(new TroopDef { Type = TroopType.Leech, IsHaze = true, Name = "Светоед", Housing = 3, Unlock = 3, Hp = 160, Dmg = 18, Cd = 0.6f, Range = 0.7f, Speed = 1.6f, Target = TargetPref.Light, LightMult = 3, Stealth = true, Color = "#3a2a55" });
            Tr(new TroopDef { Type = TroopType.Wraith, IsHaze = true, Name = "Призрак", Housing = 4, Unlock = 4, Hp = 200, Dmg = 18, Cd = 1.0f, Range = 2.2f, Speed = 1.6f, Target = TargetPref.Any, Air = true, Ranged = ProjKind.Wisp, Color = "#a090c8" });
        }
    }
}
