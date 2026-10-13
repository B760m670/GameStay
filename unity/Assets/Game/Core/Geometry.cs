using System;
using System.Collections.Generic;

namespace Lanternfall.Core
{
    /// Детерминированный ГСЧ (mulberry32) — одинаковые результаты на клиенте и сервере.
    public sealed class Rng
    {
        uint a;
        public Rng(int seed) { a = unchecked((uint)seed); }
        public double Next()
        {
            unchecked
            {
                a += 0x6D2B79F5;
                uint t = (a ^ (a >> 15)) * (1 | a);
                t = (t + ((t ^ (t >> 7)) * (61 | t))) ^ t;
                return (t ^ (t >> 14)) / 4294967296.0;
            }
        }
        public float NextF() => (float)Next();
        public int Range(int maxExclusive) => (int)(Next() * maxExclusive);
    }

    public struct LightSource
    {
        public float X, Y, R;
        public int Id;
        public LightSource(float x, float y, float r, int id) { X = x; Y = y; R = r; Id = id; }
    }

    public struct Eclipse
    {
        public float X, Y, R, Until;
    }

    public interface IPlaced
    {
        BType Type { get; }
        int X { get; }
        int Y { get; }
        int Level { get; }
        int Id { get; }
    }

    public static class Geo
    {
        public static float Hypot(float x, float y) => (float)Math.Sqrt(x * x + y * y);

        public static float DistToRect(float px, float py, float x, float y, float size)
        {
            float dx = Math.Max(Math.Max(x - px, 0f), px - (x + size));
            float dy = Math.Max(Math.Max(y - py, 0f), py - (y + size));
            return Hypot(dx, dy);
        }

        public static bool RectsOverlap(int ax, int ay, int a, int bx, int by, int b) =>
            ax < bx + b && bx < ax + a && ay < by + b && by < ay + a;

        public static float LightRadius(BType t, int level)
        {
            var d = GameConfig.B(t);
            if (!d.IsLight || level < 1) return 0f;
            return d.Lv(d.Light, level);
        }

        public static List<LightSource> Sources<T>(IEnumerable<T> items, int excludeId = int.MinValue) where T : IPlaced
        {
            var list = new List<LightSource>();
            foreach (var b in items)
            {
                if (b.Id == excludeId) continue;
                float r = LightRadius(b.Type, b.Level);
                if (r <= 0) continue;
                float s = GameConfig.B(b.Type).Size;
                list.Add(new LightSource(b.X + s / 2f, b.Y + s / 2f, r, b.Id));
            }
            return list;
        }

        public static bool IsLit(float px, float py, List<LightSource> sources, List<Eclipse> eclipses = null)
        {
            if (eclipses != null)
                foreach (var e in eclipses) if (Hypot(px - e.X, py - e.Y) <= e.R) return false;
            foreach (var s in sources) if (Hypot(px - s.X, py - s.Y) <= s.R) return true;
            return false;
        }

        public static bool[] LitGrid(List<LightSource> sources, List<Eclipse> eclipses = null)
        {
            int n = GameConfig.Grid;
            var g = new bool[n * n];
            for (int j = 0; j < n; j++)
                for (int i = 0; i < n; i++)
                    g[j * n + i] = IsLit(i + 0.5f, j + 0.5f, sources, eclipses);
            return g;
        }

        public static bool FootprintLit(int x, int y, int size, List<LightSource> sources)
        {
            for (int j = y; j < y + size; j++)
                for (int i = x; i < x + size; i++)
                    if (!IsLit(i + 0.5f, j + 0.5f, sources)) return false;
            return true;
        }
    }
}
