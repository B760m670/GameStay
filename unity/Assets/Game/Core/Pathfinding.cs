using System;
using System.Collections.Generic;

namespace Lanternfall.Core
{
    /// A* по сетке с диагоналями. cost(i) = float.PositiveInfinity для непроходимых клеток.
    public static class Pathfinding
    {
        static readonly int[] DX = { 1, -1, 0, 0, 1, 1, -1, -1 };
        static readonly int[] DY = { 0, 0, 1, -1, 1, -1, 1, -1 };
        static readonly float[] DC = { 1, 1, 1, 1, 1.41421356f, 1.41421356f, 1.41421356f, 1.41421356f };

        struct Node { public int I; public float F; }

        sealed class Heap
        {
            readonly List<Node> a = new List<Node>();
            public int Count => a.Count;
            public void Push(Node n)
            {
                a.Add(n);
                int i = a.Count - 1;
                while (i > 0)
                {
                    int p = (i - 1) >> 1;
                    if (a[p].F <= a[i].F) break;
                    (a[p], a[i]) = (a[i], a[p]); i = p;
                }
            }
            public Node Pop()
            {
                var top = a[0];
                var last = a[a.Count - 1];
                a.RemoveAt(a.Count - 1);
                if (a.Count > 0)
                {
                    a[0] = last;
                    int i = 0;
                    for (;;)
                    {
                        int l = 2 * i + 1, r = l + 1, m = i;
                        if (l < a.Count && a[l].F < a[m].F) m = l;
                        if (r < a.Count && a[r].F < a[m].F) m = r;
                        if (m == i) break;
                        (a[m], a[i]) = (a[i], a[m]); i = m;
                    }
                }
                return top;
            }
        }

        /// Путь от (sx,sy) до любой клетки прямоугольника цели. Возвращает индексы клеток (без стартовой) или null.
        public static List<int> Find(int sx, int sy, int gx, int gy, int gs, Func<int, float> cost, int maxIter = 6000)
        {
            int n = GameConfig.Grid, N = n * n;
            var g = new float[N];
            var from = new int[N];
            var closed = new bool[N];
            for (int k = 0; k < N; k++) { g[k] = float.PositiveInfinity; from[k] = -1; }
            bool InGoal(int x, int y) => x >= gx && x < gx + gs && y >= gy && y < gy + gs;
            float H(int x, int y)
            {
                int dx = Math.Max(Math.Max(gx - x, 0), x - (gx + gs - 1));
                int dy = Math.Max(Math.Max(gy - y, 0), y - (gy + gs - 1));
                return Math.Max(dx, dy) + 0.41421356f * Math.Min(dx, dy);
            }
            sx = Math.Max(0, Math.Min(n - 1, sx));
            sy = Math.Max(0, Math.Min(n - 1, sy));
            int start = sy * n + sx;
            g[start] = 0;
            var open = new Heap();
            open.Push(new Node { I = start, F = H(sx, sy) });
            int iter = 0;
            while (open.Count > 0 && iter++ < maxIter)
            {
                int i = open.Pop().I;
                if (closed[i]) continue;
                closed[i] = true;
                int x = i % n, y = i / n;
                if (InGoal(x, y))
                {
                    var path = new List<int>();
                    for (int c = i; c != start; c = from[c]) path.Add(c);
                    path.Reverse();
                    return path;
                }
                for (int d = 0; d < 8; d++)
                {
                    int nx = x + DX[d], ny = y + DY[d];
                    if (nx < 0 || ny < 0 || nx >= n || ny >= n) continue;
                    int ni = ny * n + nx;
                    if (closed[ni]) continue;
                    float c = InGoal(nx, ny) ? 1f : cost(ni);
                    if (float.IsPositiveInfinity(c)) continue;
                    if (DX[d] != 0 && DY[d] != 0)
                    {
                        // не срезаем углы через препятствия
                        int a = y * n + nx, b = ny * n + x;
                        if ((!InGoal(nx, y) && cost(a) > 1f) || (!InGoal(x, ny) && cost(b) > 1f)) continue;
                    }
                    float ng = g[i] + DC[d] * c;
                    if (ng < g[ni])
                    {
                        g[ni] = ng; from[ni] = i;
                        open.Push(new Node { I = ni, F = ng + H(nx, ny) });
                    }
                }
            }
            return null;
        }
    }
}
