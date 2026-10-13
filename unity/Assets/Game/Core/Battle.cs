// Детерминированная симуляция боя: атака игрока и ночная оборона от Мглы.
// Клиент только рисует состояние и шлёт команды Deploy/CastEclipse — та же симуляция пригодна для сервера.
using System;
using System.Collections.Generic;
using System.Linq;

namespace Lanternfall.Core
{
    public enum BattleMode { Attack, Defense }

    public class BBuilding : IPlaced
    {
        public int Id { get; set; }
        public BType Type { get; set; }
        public int X { get; set; }
        public int Y { get; set; }
        public int Level { get; set; }
        public BuildingDef Def;
        public int Size;
        public float Hp, MaxHp, Cx, Cy, LightR, Cd, BeamT, HitT, Angle = 2.35f;
        public float LootOre, LootAether;
        public bool Dead, Powered = true;
        public BTroop Target;
        public bool IsDefense => Def.IsDefense;
        public bool IsWall => Type == BType.Wall;
        public bool IsLight => Def.IsLight;
        public bool HasLoot => LootOre > 0 || LootAether > 0;
    }

    public class BTroop
    {
        public int Id;
        public TroopType Type;
        public TroopDef Def;
        public float X, Y, Hp, MaxHp, DmgMult = 1f, Cd, Jx, Jy, Angle, HitT, Bob;
        public bool Air, Dead, Attacking, Hidden;
        public BBuilding Target, Wall;
        public List<int> Path;
        public int Pi;
    }

    public class Projectile
    {
        public ProjKind Kind;
        public float Sx, Sy, X, Y, Tx, Ty, T, Dur, Dmg, Splash, K;
        public BTroop Troop;          // цель-войско (оборона стреляет)
        public BBuilding Building;    // цель-здание (войска стреляют)
        public BTroop Attacker;
        public BuildingDef Src;
        public bool FromAir, Done;
    }

    public enum FxKind { Boom, Blast, Hit, Poof, Flame, Eclipse, Rift }
    public class Effect { public FxKind Kind; public float X, Y, Sx, Sy, Life, Max, R; public bool Air; public string Color; }

    public enum EventKind { LightsOut, Lantern, Wave, Eclipse, Destroyed }
    public struct BattleEvent { public EventKind Kind; public float X, Y; public BType Type; }

    public class Wave { public float T; public Dictionary<TroopType, int> Units = new Dictionary<TroopType, int>(); public bool Done; }

    public class BattleResult
    {
        public int Stars, Pct;
        public long LootOre, LootAether;
        public bool Win, LanternDead;
        public Dictionary<TroopType, int> Used = new Dictionary<TroopType, int>();
    }

    public class BattleOptions
    {
        public BattleMode Mode = BattleMode.Attack;
        public List<LayoutItem> Layout;
        public Dictionary<TroopType, int> Army = new Dictionary<TroopType, int>();
        public int EclipseCharges;
        public int Seed = 1;
        public float Duration = GameConfig.BattleTime;
        public List<Wave> Waves = new List<Wave>();
        public float HazeMult = 1f;
    }

    public class Battle
    {
        const float WallCost = 10f;
        static int G => GameConfig.Grid;

        public readonly BattleMode Mode;
        public float Time, Duration;
        public readonly Rng Rng;
        public readonly List<BBuilding> Buildings = new List<BBuilding>();
        public List<BTroop> Troops = new List<BTroop>();
        public List<Projectile> Projectiles = new List<Projectile>();
        public List<Effect> Effects = new List<Effect>();
        public List<Eclipse> Eclipses = new List<Eclipse>();
        public readonly List<BattleEvent> Events = new List<BattleEvent>();
        public int EclipseCharges;
        public readonly Dictionary<TroopType, int> Pool;
        public readonly Dictionary<TroopType, int> Used = new Dictionary<TroopType, int>();
        public float LootOre, LootAether, LootTotalOre, LootTotalAether;
        public readonly int[] TileOwner;
        public bool[] Lit;
        public List<LightSource> Sources = new List<LightSource>();
        public int Destroyed, Total;
        public bool LanternDead, Ended;
        public BattleResult Result;
        public readonly List<Wave> Waves;
        public readonly float HazeMult;
        float lastDeployAt = -99f;
        int nextId = 1;

        public Battle(BattleOptions o)
        {
            Mode = o.Mode; Duration = o.Duration; Rng = new Rng(o.Seed);
            EclipseCharges = o.EclipseCharges;
            Pool = new Dictionary<TroopType, int>(o.Army);
            Waves = o.Waves; HazeMult = o.HazeMult;
            TileOwner = Enumerable.Repeat(-1, G * G).ToArray();
            for (int idx = 0; idx < o.Layout.Count; idx++)
            {
                var it = o.Layout[idx];
                var d = GameConfig.B(it.Type);
                float hp = d.Lv(d.Hp, it.Level);
                var b = new BBuilding
                {
                    Id = idx, Type = it.Type, X = it.X, Y = it.Y, Level = it.Level, Def = d, Size = d.Size,
                    Hp = hp, MaxHp = hp, Cx = it.X + d.Size / 2f, Cy = it.Y + d.Size / 2f,
                    LightR = d.IsLight ? d.Lv(d.Light, it.Level) : 0f, LootOre = it.LootOre, LootAether = it.LootAether,
                };
                Buildings.Add(b);
                if (!b.IsWall) Total++;
                LootTotalOre += b.LootOre; LootTotalAether += b.LootAether;
                for (int j = b.Y; j < b.Y + b.Size; j++)
                    for (int i = b.X; i < b.X + b.Size; i++) TileOwner[j * G + i] = idx;
            }
            Relight();
        }

        public void Relight()
        {
            Sources = Buildings.Where(b => b.IsLight && !b.Dead).Select(b => new LightSource(b.Cx, b.Cy, b.LightR, b.Id)).ToList();
            Lit = Geo.LitGrid(Sources, Eclipses);
            foreach (var b in Buildings)
                if (b.IsDefense) b.Powered = Lit[(int)b.Cy * G + (int)b.Cx];
        }

        public bool LitAt(float x, float y)
        {
            int i = Math.Max(0, Math.Min(G - 1, (int)Math.Floor(x)));
            int j = Math.Max(0, Math.Min(G - 1, (int)Math.Floor(y)));
            return Lit[j * G + i];
        }

        public bool CanDeploy(float x, float y)
        {
            if (x < 0 || y < 0 || x >= G || y >= G) return false;
            int i = (int)x, j = (int)y;
            if (Lit[j * G + i]) return false;
            int o = TileOwner[j * G + i];
            return o < 0 || Buildings[o].Dead;
        }

        BTroop Spawn(TroopType type, float x, float y)
        {
            var def = GameConfig.T(type);
            float hpMult = Mode == BattleMode.Defense ? HazeMult : 1f;
            var t = new BTroop
            {
                Id = nextId++, Type = type, Def = def, X = x, Y = y,
                Hp = def.Hp * hpMult, MaxHp = def.Hp * hpMult,
                DmgMult = Mode == BattleMode.Defense ? 1f + (HazeMult - 1f) * 0.5f : 1f,
                Air = def.Air, Jx = (Rng.NextF() - 0.5f) * 0.6f, Jy = (Rng.NextF() - 0.5f) * 0.6f, Bob = Rng.NextF() * 6.28f,
            };
            Troops.Add(t);
            return t;
        }

        public bool Deploy(TroopType type, float x, float y)
        {
            if (Ended || !Pool.TryGetValue(type, out int left) || left <= 0) return false;
            if (!CanDeploy(x, y)) return false;
            Spawn(type, x, y);
            Pool[type] = left - 1;
            Used[type] = (Used.TryGetValue(type, out int u) ? u : 0) + 1;
            lastDeployAt = Time;
            return true;
        }

        public bool CastEclipse(float x, float y)
        {
            if (Ended || EclipseCharges <= 0) return false;
            EclipseCharges--;
            Eclipses.Add(new Eclipse { X = x, Y = y, R = GameConfig.EclipseRadius, Until = Time + GameConfig.EclipseDuration });
            Effects.Add(new Effect { Kind = FxKind.Eclipse, X = x, Y = y, Life = 1, Max = 1 });
            Relight();
            Events.Add(new BattleEvent { Kind = EventKind.Eclipse, X = x, Y = y });
            return true;
        }

        // --- Цели ---
        BBuilding ChooseTarget(BTroop t)
        {
            IEnumerable<BBuilding> alive = Buildings.Where(b => !b.Dead);
            List<BBuilding> pool = null;
            switch (t.Def.Target)
            {
                case TargetPref.Defense: pool = alive.Where(b => b.IsDefense).ToList(); break;
                case TargetPref.Light: pool = alive.Where(b => b.IsLight).ToList(); break;
                case TargetPref.Wall: pool = alive.Where(b => b.IsWall).ToList(); break;
            }
            if (pool == null || pool.Count == 0) pool = alive.Where(b => !b.IsWall).ToList();
            BBuilding best = null; float bd = float.MaxValue;
            foreach (var b in pool)
            {
                float d = Geo.DistToRect(t.X, t.Y, b.X, b.Y, b.Size);
                if (d < bd) { bd = d; best = b; }
            }
            return best;
        }

        float CellCost(int i)
        {
            int o = TileOwner[i];
            if (o < 0) return 1f;
            var b = Buildings[o];
            if (b.Dead) return 1f;
            return b.IsWall ? 1f + WallCost : float.PositiveInfinity;
        }

        // --- Урон ---
        void DamageBuilding(BBuilding b, float dmg, BTroop attacker)
        {
            if (b.Dead) return;
            if (attacker != null && b.IsLight) dmg *= attacker.Def.LightMult;
            float dealt = Math.Min(dmg, b.Hp);
            if (b.HasLoot)
            {
                float f = dealt / b.MaxHp;
                LootOre += b.LootOre * f;
                LootAether += b.LootAether * f;
            }
            b.Hp -= dealt;
            b.HitT = 0.15f;
            if (b.Hp <= 0.001f) DestroyBuilding(b);
        }

        void DestroyBuilding(BBuilding b)
        {
            b.Dead = true; b.Hp = 0;
            Effects.Add(new Effect { Kind = FxKind.Boom, X = b.Cx, Y = b.Cy, Life = 0.8f, Max = 0.8f, R = b.Size });
            Events.Add(new BattleEvent { Kind = EventKind.Destroyed, X = b.Cx, Y = b.Cy, Type = b.Type });
            if (!b.IsWall) Destroyed++;
            if (b.Type == BType.Lantern) { LanternDead = true; Events.Add(new BattleEvent { Kind = EventKind.Lantern, X = b.Cx, Y = b.Cy }); }
            if (b.IsLight)
            {
                Relight();
                if (b.Type == BType.Beacon) Events.Add(new BattleEvent { Kind = EventKind.LightsOut, X = b.Cx, Y = b.Cy });
            }
        }

        void DamageTroop(BTroop t, float dmg)
        {
            if (t.Dead) return;
            t.Hp -= dmg;
            t.HitT = 0.12f;
            if (t.Hp <= 0)
            {
                t.Dead = true;
                Effects.Add(new Effect { Kind = FxKind.Poof, X = t.X, Y = t.Y, Life = 0.5f, Max = 0.5f, Color = t.Def.Color, Air = t.Air });
            }
        }

        // --- Войска ---
        void TroopAttack(BTroop t, BBuilding b)
        {
            t.Attacking = true;
            t.Angle = (float)Math.Atan2(b.Cy - t.Y, b.Cx - t.X);
            if (t.Cd > 0) return;
            var def = t.Def;
            if (def.Suicide)
            {
                foreach (var o in Buildings.ToList())
                {
                    if (o.Dead) continue;
                    if (Geo.DistToRect(t.X, t.Y, o.X, o.Y, o.Size) <= def.Splash) DamageBuilding(o, o.IsWall ? def.Dmg : def.Dmg * 0.25f, t);
                }
                Effects.Add(new Effect { Kind = FxKind.Blast, X = t.X, Y = t.Y, Life = 0.6f, Max = 0.6f, R = def.Splash });
                t.Dead = true;
                return;
            }
            t.Cd = def.Cd;
            float dmg = def.Dmg * t.DmgMult;
            if (def.Ranged != ProjKind.None)
            {
                Projectiles.Add(new Projectile
                {
                    Kind = def.Ranged, Sx = t.X, Sy = t.Y, X = t.X, Y = t.Y, Tx = b.Cx, Ty = b.Cy, Building = b,
                    Dur = Math.Max(0.15f, Geo.Hypot(b.Cx - t.X, b.Cy - t.Y) / 9f), Dmg = dmg, Attacker = t, FromAir = t.Air,
                });
            }
            else
            {
                DamageBuilding(b, dmg, t);
                Effects.Add(new Effect { Kind = FxKind.Hit, X = t.X + (float)Math.Cos(t.Angle) * 0.4f, Y = t.Y + (float)Math.Sin(t.Angle) * 0.4f, Life = 0.2f, Max = 0.2f });
            }
        }

        static bool MoveToward(BTroop t, float x, float y, float step)
        {
            float dx = x - t.X, dy = y - t.Y, d = Geo.Hypot(dx, dy);
            t.Angle = (float)Math.Atan2(dy, dx);
            if (d <= step) { t.X = x; t.Y = y; return true; }
            t.X += dx / d * step; t.Y += dy / d * step;
            return false;
        }

        void UpdateTroop(BTroop t, float dt)
        {
            if (t.Cd > 0) t.Cd -= dt;
            if (t.HitT > 0) t.HitT -= dt;
            t.Attacking = false;
            t.Hidden = t.Def.Stealth && !LitAt(t.X, t.Y);
            if (t.Target == null || t.Target.Dead)
            {
                t.Target = ChooseTarget(t);
                t.Path = null; t.Wall = null;
                if (t.Target == null) return;
            }
            var b = t.Target;
            if (Geo.DistToRect(t.X, t.Y, b.X, b.Y, b.Size) <= t.Def.Range) { TroopAttack(t, b); return; }
            float step = t.Def.Speed * dt;
            if (t.Air) { MoveToward(t, b.Cx, b.Cy, step); return; }
            float reach = Math.Max(t.Def.Range, 0.6f);
            if (t.Wall != null)
            {
                if (t.Wall.Dead) t.Wall = null;
                else if (Geo.DistToRect(t.X, t.Y, t.Wall.X, t.Wall.Y, 1) <= reach) { TroopAttack(t, t.Wall); return; }
            }
            if (t.Path == null)
            {
                t.Path = Pathfinding.Find((int)t.X, (int)t.Y, b.X, b.Y, b.Size, CellCost) ?? new List<int>();
                t.Pi = 0;
            }
            if (t.Pi >= t.Path.Count) { MoveToward(t, b.Cx, b.Cy, step); return; }
            int idx = t.Path[t.Pi];
            int o = TileOwner[idx];
            if (o >= 0 && Buildings[o].IsWall && !Buildings[o].Dead && Buildings[o] != b)
            {
                var w = Buildings[o];
                t.Wall = w;
                if (Geo.DistToRect(t.X, t.Y, w.X, w.Y, 1) <= reach) { TroopAttack(t, w); return; }
                MoveToward(t, w.Cx, w.Cy, step);
                return;
            }
            float px = idx % G + 0.5f + t.Jx, py = idx / G + 0.5f + t.Jy;
            if (MoveToward(t, px, py, step)) t.Pi++;
        }

        // --- Оборона ---
        static bool CanHit(BBuilding b, BTroop t)
        {
            if (t.Dead || t.Hidden) return false;
            if (t.Air ? !b.Def.Air : !b.Def.Ground) return false;
            float d = Geo.Hypot(t.X - b.Cx, t.Y - b.Cy);
            if (d > b.Def.Range + 0.3f) return false;
            if (b.Def.MinRange > 0 && d < b.Def.MinRange) return false;
            return true;
        }

        void UpdateDefense(BBuilding b, float dt)
        {
            if (b.Cd > 0) b.Cd -= dt;
            if (!b.Powered) { b.Target = null; b.BeamT = 0; return; }
            if (b.Target != null && !CanHit(b, b.Target)) { b.Target = null; b.BeamT = 0; }
            if (b.Target == null)
            {
                BTroop best = null; float bd = float.MaxValue;
                foreach (var t in Troops)
                {
                    if (!CanHit(b, t)) continue;
                    float d = Geo.Hypot(t.X - b.Cx, t.Y - b.Cy);
                    if (d < bd) { bd = d; best = t; }
                }
                b.Target = best; b.BeamT = 0;
            }
            var tr = b.Target;
            if (tr == null) return;
            b.Angle = (float)Math.Atan2(tr.Y - b.Cy, tr.X - b.Cx);
            if (b.Def.Beam)
            {
                b.BeamT += dt;
                float mult = Math.Min(b.Def.RampMax, 1 + b.BeamT * b.Def.RampRate);
                DamageTroop(tr, b.Def.Lv(b.Def.Dps, b.Level) * mult * dt);
                return;
            }
            if (b.Cd > 0) return;
            b.Cd = b.Def.Cd;
            float dmg = b.Def.Lv(b.Def.Dmg, b.Level);
            if (b.Def.Proj == ProjKind.Flame)
            {
                SplashTroops(tr.X, tr.Y, b.Def.Splash, dmg, b.Def);
                Effects.Add(new Effect { Kind = FxKind.Flame, X = tr.X, Y = tr.Y, Sx = b.Cx, Sy = b.Cy, Life = 0.35f, Max = 0.35f });
                return;
            }
            float dist = Geo.Hypot(tr.X - b.Cx, tr.Y - b.Cy);
            Projectiles.Add(new Projectile
            {
                Kind = b.Def.Proj, Sx = b.Cx, Sy = b.Cy, X = b.Cx, Y = b.Cy, Tx = tr.X, Ty = tr.Y,
                Troop = b.Def.Proj == ProjKind.Rock ? null : tr, Dur = Math.Max(0.1f, dist / b.Def.ProjSpeed),
                Dmg = dmg, Splash = b.Def.Splash, Src = b.Def,
            });
        }

        void SplashTroops(float x, float y, float r, float dmg, BuildingDef src)
        {
            foreach (var t in Troops)
            {
                if (t.Dead) continue;
                if (t.Air ? !src.Air : !src.Ground) continue;
                if (Geo.Hypot(t.X - x, t.Y - y) <= r) DamageTroop(t, dmg);
            }
        }

        void UpdateProjectiles(float dt)
        {
            foreach (var p in Projectiles)
            {
                p.T += dt;
                if (p.Troop != null && !p.Troop.Dead) { p.Tx = p.Troop.X; p.Ty = p.Troop.Y; }
                p.K = Math.Min(1f, p.T / p.Dur);
                p.X = p.Sx + (p.Tx - p.Sx) * p.K;
                p.Y = p.Sy + (p.Ty - p.Sy) * p.K;
                if (p.K < 1f) continue;
                p.Done = true;
                if (p.Building != null) DamageBuilding(p.Building, p.Dmg, p.Attacker);
                else if (p.Splash > 0)
                {
                    SplashTroops(p.Tx, p.Ty, p.Splash, p.Dmg, p.Src);
                    Effects.Add(new Effect { Kind = FxKind.Blast, X = p.Tx, Y = p.Ty, Life = 0.5f, Max = 0.5f, R = p.Splash });
                }
                else if (p.Troop != null) DamageTroop(p.Troop, p.Dmg);
            }
            Projectiles.RemoveAll(p => p.Done);
        }

        // --- Волны Мглы ---
        void SpawnWave(Wave w)
        {
            int side = Rng.Range(4);
            float along = 4 + Rng.NextF() * (G - 8), edge = 0.8f, cx, cy;
            if (side == 0) { cx = along; cy = edge; }
            else if (side == 1) { cx = G - edge; cy = along; }
            else if (side == 2) { cx = along; cy = G - edge; }
            else { cx = edge; cy = along; }
            foreach (var kv in w.Units.OrderBy(k => (int)k.Key))
                for (int k = 0; k < kv.Value; k++)
                {
                    float x = Math.Max(0.2f, Math.Min(G - 0.2f, cx + (Rng.NextF() - 0.5f) * 3));
                    float y = Math.Max(0.2f, Math.Min(G - 0.2f, cy + (Rng.NextF() - 0.5f) * 3));
                    Spawn(kv.Key, x, y);
                }
            Effects.Add(new Effect { Kind = FxKind.Rift, X = cx, Y = cy, Life = 1.5f, Max = 1.5f });
            Events.Add(new BattleEvent { Kind = EventKind.Wave, X = cx, Y = cy });
        }

        // --- Главный шаг ---
        public void Step(float dt)
        {
            if (Ended) return;
            Time += dt;
            if (Eclipses.Count > 0 && Eclipses.RemoveAll(e => e.Until <= Time) > 0) Relight();
            foreach (var w in Waves) if (!w.Done && Time >= w.T) { w.Done = true; SpawnWave(w); }
            foreach (var t in Troops.ToList()) if (!t.Dead) UpdateTroop(t, dt);
            foreach (var b in Buildings)
            {
                if (b.HitT > 0) b.HitT -= dt;
                if (b.IsDefense && !b.Dead) UpdateDefense(b, dt);
            }
            UpdateProjectiles(dt);
            foreach (var e in Effects) e.Life -= dt;
            Effects.RemoveAll(e => e.Life <= 0);
            if (Troops.Count > 400) Troops.RemoveAll(t => t.Dead);
            CheckEnd();
        }

        public int AliveTroops => Troops.Count(t => !t.Dead);
        public int PoolLeft => Pool.Values.Sum();
        public int Percent => Total > 0 ? (int)Math.Floor(Destroyed * 100.0 / Total) : 0;

        void CheckEnd()
        {
            if (Mode == BattleMode.Attack)
            {
                bool noForces = AliveTroops == 0 && PoolLeft == 0 && Time - lastDeployAt > 1 && Projectiles.Count == 0;
                if (Time >= Duration || Destroyed >= Total || noForces) Finish();
            }
            else
            {
                bool allSpawned = Waves.All(w => w.Done);
                if (LanternDead || Time >= Duration || (allSpawned && AliveTroops == 0)) Finish();
            }
        }

        public void Finish()
        {
            if (Ended) return;
            Ended = true;
            int pct = Percent, stars = 0;
            if (pct >= 50) stars++;
            if (LanternDead) stars++;
            if (pct >= 100) stars++;
            Result = new BattleResult
            {
                Stars = Mode == BattleMode.Attack ? stars : 0, Pct = pct,
                LootOre = (long)Math.Floor(LootOre), LootAether = (long)Math.Floor(LootAether),
                Win = Mode == BattleMode.Attack ? stars > 0 : !LanternDead, LanternDead = LanternDead,
                Used = new Dictionary<TroopType, int>(Used),
            };
        }

        /// Прогон до конца (тесты, автобой, серверная проверка).
        public BattleResult Simulate(float maxTime = 400f, float dt = 1f / 30f)
        {
            while (!Ended && Time < maxTime) Step(dt);
            if (!Ended) Finish();
            return Result;
        }
    }
}
