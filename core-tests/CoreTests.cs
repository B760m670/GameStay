using System.Collections.Generic;
using System.Linq;
using Lanternfall.Core;
using Xunit;

public class CoreTests
{
    [Fact]
    public void StartVillage_AllBuildingsLitAndNonOverlapping()
    {
        var S = Village.Create(0);
        foreach (var b in S.buildings)
        {
            var r = Village.CanPlace(S, b.type, b.x, b.y, b.id);
            Assert.True(r.Ok, $"{b.type}: {r.Reason}");
        }
    }

    [Fact]
    public void CannotBuildInDarkness()
    {
        var S = Village.Create(0);
        Assert.False(Village.CanPlace(S, BType.Wall, 3, 3).Ok);
        Assert.True(Village.CanPlace(S, BType.Wall, 19, 19).Ok);
    }

    [Fact]
    public void BuildUpgradeCollect()
    {
        var S = Village.Create(0);
        Assert.True(Village.Place(S, BType.Wall, 19, 19, 0, out _).Ok);
        var lantern = S.buildings.First(b => b.type == BType.Lantern);
        S.ore = 4000;
        Assert.True(Village.StartUpgrade(S, lantern.id, 0).Ok);
        Village.Tick(S, 61000);
        Assert.Equal(2, Village.LanternLevel(S));
        var mine = S.buildings.First(b => b.type == BType.OreMine);
        Assert.True(Village.Collect(S, mine.id, 120000) > 0);
    }

    [Fact]
    public void TrainTroops()
    {
        var S = Village.Create(0);
        S.army = new int[S.army.Length];
        Assert.True(Village.Train(S, TroopType.Torch, 0).Ok);
        Village.Tick(S, 10000);
        Assert.Equal(1, S.army[Village.TroopIndex(TroopType.Torch)]);
    }

    [Fact]
    public void BaseGen_AllLevels_LitAndNonOverlapping()
    {
        for (int LL = 1; LL <= 6; LL++)
            for (int seed = 1; seed <= 5; seed++)
            {
                var layout = BaseGen.Generate(LL, seed);
                var occ = new HashSet<int>();
                var src = Geo.Sources(layout);
                foreach (var b in layout)
                {
                    int s = GameConfig.B(b.Type).Size;
                    for (int j = b.Y; j < b.Y + s; j++)
                        for (int i = b.X; i < b.X + s; i++)
                            Assert.True(occ.Add(j * GameConfig.Grid + i), $"overlap LL{LL} seed{seed} {b.Type}");
                    Assert.True(Geo.IsLit(b.X + s / 2f, b.Y + s / 2f, src), $"unlit {b.Type} LL{LL}");
                    Assert.InRange(b.Level, 1, GameConfig.B(b.Type).MaxLevel);
                }
                Assert.Contains(layout, b => b.Type == BType.Lantern);
            }
    }

    [Fact]
    public void Deploy_OnlyInDarkness()
    {
        var B = new Battle(new BattleOptions { Layout = BaseGen.Generate(2, 3), Army = new Dictionary<TroopType, int> { [TroopType.Torch] = 5 } });
        Assert.False(B.CanDeploy(22, 22));
        Assert.True(B.CanDeploy(1, 1));
        Assert.True(B.Deploy(TroopType.Torch, 1, 1));
    }

    [Fact]
    public void StrongArmyWins_WeakArmyDoesNot()
    {
        var layout = BaseGen.Generate(1, 7);
        var B = new Battle(new BattleOptions { Layout = layout, Army = new Dictionary<TroopType, int> { [TroopType.Torch] = 60 } });
        for (int k = 0; k < 60; k++) B.Deploy(TroopType.Torch, 1 + (k % 10) * 0.3f, 1 + (k / 10) * 0.3f);
        var r = B.Simulate();
        Assert.True(r.Stars >= 2, $"stars={r.Stars} pct={r.Pct}");
        Assert.True(r.LootOre > 0);

        var B2 = new Battle(new BattleOptions { Layout = layout, Army = new Dictionary<TroopType, int> { [TroopType.Torch] = 3 } });
        for (int k = 0; k < 3; k++) B2.Deploy(TroopType.Torch, 1, 1);
        Assert.True(B2.Simulate().Stars < 3);
    }

    [Fact]
    public void Simulation_IsDeterministic()
    {
        BattleResult Run()
        {
            var B = new Battle(new BattleOptions { Layout = BaseGen.Generate(3, 11), Seed = 5, Army = new Dictionary<TroopType, int> { [TroopType.Torch] = 30, [TroopType.Archer] = 20 } });
            for (int k = 0; k < 30; k++) B.Deploy(TroopType.Torch, 1 + k * 0.1f, 2);
            for (int k = 0; k < 20; k++) B.Deploy(TroopType.Archer, 2, 1 + k * 0.1f);
            return B.Simulate();
        }
        var a = Run(); var b = Run();
        Assert.Equal(a.Pct, b.Pct);
        Assert.Equal(a.LootOre, b.LootOre);
        Assert.Equal(a.Stars, b.Stars);
    }

    [Fact]
    public void FirstNight_UsuallySurvivable()
    {
        int wins = 0;
        for (int seed = 1; seed <= 6; seed++)
        {
            var S = Village.Create(0);
            var (waves, mult) = BaseGen.NightWaves(1, 0, seed);
            var B = new Battle(new BattleOptions { Mode = BattleMode.Defense, Layout = Village.Layout(S), Waves = waves, HazeMult = mult, Duration = GameConfig.NightTime, Seed = seed });
            if (B.Simulate().Win) wins++;
        }
        Assert.True(wins >= 4, $"wins={wins}");
    }

    [Fact]
    public void KillingBeacon_UnpowersDefensesInItsSector()
    {
        var layout = BaseGen.Generate(4, 2);
        var B = new Battle(new BattleOptions { Layout = layout });
        var beacons = B.Buildings.Where(b => b.Type == BType.Beacon).ToList();
        Assert.NotEmpty(beacons);
        int poweredBefore = B.Buildings.Count(b => b.IsDefense && b.Powered);
        var lantern = B.Buildings.First(b => b.Type == BType.Lantern);
        lantern.Dead = true;
        foreach (var bc in beacons) bc.Dead = true;
        B.Relight();
        Assert.True(poweredBefore > 0);
        Assert.Equal(0, B.Buildings.Count(b => b.IsDefense && b.Powered));
    }

    [Fact]
    public void CampaignMissionsGenerate()
    {
        Assert.Equal(12, Campaign.Missions.Length);
        foreach (var m in Campaign.Missions)
            Assert.Contains(BaseGen.Generate(m.LL, m.Seed, m.Counts, m.Richness, m.LevelBias), b => b.Type == BType.Lantern);
    }
}
