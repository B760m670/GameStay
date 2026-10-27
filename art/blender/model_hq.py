"""Острог — главное здание, собранное из реалистичных деталей (scan_kit: CC0-сканы и фототекстуры Poly Haven).

Уровень 0 — руины, с которых начинается игра: старый каменный фундамент, поросший мхом, травой и молодыми елями.
Уровень 1 — бревенчатая изба на восстановленном фундаменте, крыша из тёса на слегах.
Уровни 2–5 добавляют к избе укрепления (частокол, вышку, ворота, башни).

Запуск: blender -b -P art/blender/model_hq.py -- --out art/out --levels 0,1 --samples 64
Результат: <out>/buildings/lantern_l<уровень>.png/.json (id главного здания в игре — lantern).
"""
import json
import math
import os
import random
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_kit as K  # noqa: E402
import scan_kit as S  # noqa: E402
import render_sprites as R  # noqa: E402
import toon_kit as T  # noqa: E402

SIZE = 4  # клеток; координаты Blender: 1 единица = 1 клетка (≈ 2,5 м)
GRASS = ["grass_medium_01_large_a", "grass_medium_01_large_b", "grass_medium_01_mid_a", "grass_medium_01_tall_a",
         "grass_medium_01_mid_b", "grass_medium_01_large_c"]
ROCKS = ["rock_moss_set_02_rock07", "rock_moss_set_02_rock08", "rock_moss_set_02_rock09", "rock_moss_set_02_rock10",
         "rock_moss_set_02_rock11", "rock_moss_set_02_rock12", "rock_moss_set_02_rock13"]


def frame(x_axis, z_axis, center):
    """Матрица с локальной X вдоль x_axis и Z вдоль z_axis (правая тройка)."""
    x = Vector(x_axis).normalized()
    z = Vector(z_axis)
    z = (z - x * z.dot(x)).normalized()
    y = z.cross(x)
    m = Matrix((x, y, z)).transposed().to_4x4()
    m.translation = Vector(center)
    return m


def grass_ring(rnd, x0, x1, y0, y1, n, spread=0.25, scale=(1.5, 2.2)):
    """Трава у подножия вдоль периметра прямоугольника."""
    for k in range(n):
        side = k % 4
        t = rnd.uniform(0, 1)
        x = (x0 + (x1 - x0) * t) if side < 2 else (x0 if side == 2 else x1)
        y = (y0 if side == 0 else y1) if side < 2 else (y0 + (y1 - y0) * t)
        x += rnd.uniform(-spread, spread)
        y += rnd.uniform(-spread, spread)
        S.place("grass_medium_01", GRASS[k % len(GRASS)], (x, y, 0.0), scale=rnd.uniform(*scale), rot_z=rnd.uniform(0, 6.28))


# ---------- уровень 0: руины ----------
def ruins():
    rnd = random.Random(11)
    wall = S.pbr("mossy_stone_wall", tile_m=0.9)
    floor = S.pbr("forest_ground_04", tile_m=1.2)
    x0, x1, y0, y1 = -1.75, 1.75, -1.55, 1.65  # руины занимают почти весь участок 4×4
    th = 0.34
    # стены: ближние к камере (перед и правая) почти ушли в землю, дальние уцелели выше; проём бывшей двери слева
    S.ruined_wall((x0, y0), (x1, y0), th, lambda t: 0.28 if 0.12 < t < 0.4 or t > 0.78 else 0.08, wall, seed=1, jag=0.07)
    S.ruined_wall((x1, y0), (x1, y1), th, lambda t: 0.25 + 0.5 * t, wall, seed=2)
    S.ruined_wall((x1, y1), (x0, y1), th, lambda t: 0.75 + 0.55 * t - (0.45 if 0.35 < t < 0.5 else 0), wall, seed=3)
    S.ruined_wall((x0, y1), (x0, y0), th, lambda t: 1.3 - 0.9 * t - (0.95 if 0.45 < t < 0.68 else 0), wall, seed=4)
    # старый земляной пол внутри стен
    K.box((x1 - x0 - 0.1, y1 - y0 - 0.1, 0.04), floor, loc=((x0 + x1) / 2, (y0 + y1) / 2, 0.02), bevel=0.0)
    # обломки кладки и камни со мхом
    spots = [(x0 + 0.3, y0 - 0.25), (x0 + 1.2, y0 - 0.3), (x1 + 0.3, y0 + 0.4), (x1 + 0.28, y0 + 1.3), (x0 - 0.3, y0 + 0.5),
             (x0 + 0.5, y0 + 0.5), (x1 - 0.6, y1 - 0.5), (x0 + 1.6, y0 + 0.15), (x1 - 0.3, y0 + 0.2), (x0 + 0.3, y1 - 0.6)]
    for i, (x, y) in enumerate(spots):
        S.place("rock_moss_set_02", ROCKS[i % len(ROCKS)], (x, y, 0.0), scale=rnd.uniform(0.16, 0.26), rot_z=rnd.uniform(0, 6.28))
    S.place("rock_moss_set_01", "rock_moss_set_01_rock03", (x1 + 0.2, y0 - 0.2, 0.0), scale=0.2, rot_z=0.7)
    # поваленный замшелый ствол поперёк руин, опирается на низкую переднюю стену
    S.place("dead_tree_trunk", "dead_tree_trunk", (0.1, 0.1, 0.02), scale=0.78, rot_z=-0.6, tilt=(0, 0.08, 0))
    S.place("tree_stump_01", "tree_stump_01", (x1 + 0.25, y1 - 0.15, 0.0), scale=0.38, rot_z=1.2)
    for k in range(3):
        S.place("bark_debris_01", "bark_debris_01_" + "abc"[k], (rnd.uniform(-0.8, 0.9), rnd.uniform(-0.7, 0.8), 0.04),
                scale=0.7, rot_z=rnd.uniform(0, 6.28))
    # молодые ели проросли сквозь руины
    S.place("fir_sapling", "fir_sapling_a", (0.75, 0.75, 0.04), scale=1.35, rot_z=0.4)
    S.place("fir_sapling", "fir_sapling_c", (x0 - 0.35, y1 - 0.4, 0.0), scale=1.15, rot_z=2.0)
    S.place("fir_sapling", "fir_sapling_b", (x1 + 0.35, y1 + 0.1, 0.0), scale=0.9, rot_z=4.0)
    for k, (fx, fy) in enumerate([(-0.85, 0.85), (1.0, -0.6), (-0.3, -0.7), (x0 - 0.2, y0 + 0.1), (x1 + 0.35, y0 + 0.9),
                                  (-1.0, -0.2)]):
        S.place("fern_02", "fern_02_" + "abcd"[k % 4], (fx, fy, 0.04 if x0 < fx < x1 and y0 < fy < y1 else 0.0),
                scale=rnd.uniform(0.6, 0.8), rot_z=rnd.uniform(0, 6.28))
    for k in range(26):  # трава на полу руин
        S.place("grass_medium_01", GRASS[k % len(GRASS)], (rnd.uniform(x0 + 0.25, x1 - 0.25), rnd.uniform(y0 + 0.25, y1 - 0.25), 0.04),
                scale=rnd.uniform(1.4, 2.0), rot_z=rnd.uniform(0, 6.28))
    grass_ring(rnd, x0, x1, y0, y1, 34)
    return 2.2


# ---------- уровень 1+: изба ----------
class Izba:
    """Сруб «в чашу»: брёвна соседних стен сдвинуты на полбревна по высоте и выходят за угол остатками.
    Конёк вдоль X; фронтоны (±X) — из укорачивающихся брёвен-«самцов», на них лежат слеги, по слегам — тёс."""

    def __init__(self, x0, x1, y0, y1, base, rows=9, r=0.075, step=0.135, rise=1.05, over=0.3, seed=1):
        self.x0, self.x1, self.y0, self.y1 = x0, x1, y0, y1
        self.base, self.rows, self.r, self.step = base, rows, r, step
        self.rise, self.over = rise, over
        self.rnd = random.Random(seed)
        self.logs = S.grain_mat("weathered_planks")
        self.tes = S.grain_mat("wood_planks_grey", name="grain_tes", moss=0.6)
        self.boards = S.grain_mat("wood_planks_grey", name="grain_boards")  # двери, ставни, дымник — без мха
        self.openings = []  # (стена, a0, a1, z0, z1): стена 'front' (y0), 'right' (x1), 'back' (y1), 'left' (x0)
        self.corner = 0.17  # остаток бревна за углом
        self.z_eave = base + (rows - 1) * step + 2 * r  # верх последнего венца продольных стен
        self.yc = (y0 + y1) / 2
        self.half = (y1 - y0) / 2

    def opening(self, wall, a0, a1, z0, z1):
        self.openings.append((wall, a0, a1, z0, z1))

    def _segments(self, wall, a0, a1, z):
        """Куски бревна вдоль стены wall от a0 до a1 на высоте z за вычетом проёмов."""
        cuts = sorted((o[1], o[2]) for o in self.openings if o[0] == wall and o[3] - 0.02 < z < o[4] + 0.02)
        segs, cur = [], a0
        for c0, c1 in cuts:
            if c0 > cur:
                segs.append((cur, c0))
            cur = max(cur, c1)
        if cur < a1:
            segs.append((cur, a1))
        return segs

    def _log_x(self, wall, y, z, xa, xb):
        for a, b in self._segments(wall, xa, xb, z):
            if b - a > 0.05:
                rr = self.r * self.rnd.uniform(0.95, 1.08)
                ends = (a, b) if self.rnd.random() < 0.5 else (b, a)  # комель то слева, то справа
                S.log((ends[0], y, z), (ends[1], y, z), rr, self.logs, self.rnd.randrange(1 << 30), r_end=rr * 0.9)

    def _log_y(self, wall, x, z, ya, yb):
        for a, b in self._segments(wall, ya, yb, z):
            if b - a > 0.05:
                rr = self.r * self.rnd.uniform(0.95, 1.08)
                ends = (a, b) if self.rnd.random() < 0.5 else (b, a)
                S.log((x, ends[0], z), (x, ends[1], z), rr, self.logs, self.rnd.randrange(1 << 30), r_end=rr * 0.9)

    def walls(self):
        c, r, b = self.corner, self.r, self.base
        for k in range(self.rows):
            z = b + r + k * self.step
            self._log_x("front", self.y0, z, self.x0 - c, self.x1 + c)
            self._log_x("back", self.y1, z, self.x0 - c, self.x1 + c)
        for k in range(-1, self.rows):
            z = b + r + k * self.step + self.step / 2
            z = max(z, b + r * 0.55)  # нижний венец торцевых стен утоплен в фундамент
            self._log_y("left", self.x0, z, self.y0 - c, self.y1 + c)
            self._log_y("right", self.x1, z, self.y0 - c, self.y1 + c)

    def roof_z(self, y):
        """Нижняя плоскость кровли (по верху слег) над точкой y."""
        return self.z_eave + self.rise * (1 - abs(y - self.yc) / self.half) + 0.06

    def gables(self):
        """Фронтоны из «самцов» и слеги, выходящие за фронтоны на свес."""
        k = 0
        z0 = self.base + self.r + (self.rows - 1) * self.step + self.step * 1.5
        while True:
            z = z0 + k * self.step
            h = z + self.r - self.z_eave
            hl = self.half * (1 - h / self.rise)
            if hl < 0.12:
                break
            for x in (self.x0, self.x1):
                self._log_y("gable", x, z, self.yc - hl, self.yc + hl)
            k += 1
        # слеги: по две на скат и князевая
        for t in (0.15, 0.55, 1.0):
            for side in (-1, 1) if t < 1 else (0,):
                y = self.yc + side * self.half * (1 - t)
                z = self.z_eave + self.rise * t - 0.015
                S.log((self.x0 - self.over, y, z), (self.x1 + self.over, y, z), 0.06, self.logs, self.rnd.randrange(1 << 30))

    def roof(self):
        """Тёс в два слоя: нижние доски вразбежку, верхние перекрывают щели; по коньку — охлупень, по краю — причелины."""
        rnd = self.rnd
        eave = 0.32  # свес над стеной
        span = self.half + eave
        slope_len = math.hypot(span, self.rise * span / self.half)
        xa, xb = self.x0 - self.over - 0.04, self.x1 + self.over + 0.04
        width = 0.13
        n = int((xb - xa) / width)
        for side in (-1, 1):
            d = Vector((0, side * span, -self.rise * span / self.half)).normalized()  # от конька вниз по скату
            normal = Vector((0, side * self.rise / self.half, 1)).normalized()  # наружу от ската
            ridge = Vector((0, self.yc, self.z_eave + self.rise + 0.06))
            for layer in (0, 1):
                off = width / 2 if layer else 0
                th = 0.022
                for i in range(n + (0 if layer else 1)):
                    x = xa + off + i * width + rnd.uniform(-0.008, 0.008)
                    w = (width * rnd.uniform(0.82, 0.92)) if layer == 0 else width * rnd.uniform(0.5, 0.62)
                    L = slope_len * rnd.uniform(0.97, 1.0)
                    c = ridge + d * (L / 2) + normal * (th / 2 + layer * th * 0.9) + Vector((x, 0, 0))
                    m = frame(d, normal, c) @ Matrix.Rotation(rnd.uniform(-0.012, 0.012), 4, "Z")
                    S.board((L, w, th), m, self.tes, rnd.randrange(1 << 30))
            # причелины по краям фронтонов
            for x in (xa - 0.01, xb + 0.01):
                c = ridge + d * (slope_len / 2) - normal * 0.04 + Vector((x, 0, 0))
                S.board((slope_len, 0.18, 0.03), frame(d, (1, 0, 0), c), self.logs, rnd.randrange(1 << 30))
        # охлупень — тяжёлое бревно по коньку, прижимает верх тёса
        zr = self.z_eave + self.rise + 0.06 + 0.06
        S.log((xa - 0.1, self.yc, zr), (xb + 0.1, self.yc, zr), 0.085, self.logs, rnd.randrange(1 << 30), sag=0.0)
        return zr + 0.09

    def door(self, a0, a1, h):
        """Дверь в передней стене: косяки, порог, притолока, полотно из досок на кованых петлях."""
        rnd = self.rnd
        y, b, r = self.y0, self.base, self.r
        self.opening("front", a0, a1, b, b + h)
        dark = K.material("iron_dark", [(48, 44, 40), (58, 52, 46)], rough=0.6, metal=0.7, scale=20, bump=0.2)
        for x in (a0 - 0.035, a1 + 0.035):  # косяки
            S.board((h + 0.06, 0.07, 2 * r + 0.02), frame((0, 0, 1), (0, -1, 0), (x, y, b + h / 2)), self.logs, rnd.randrange(1 << 30))
        S.board((a1 - a0 + 0.2, 2 * r + 0.02, 0.07), frame((1, 0, 0), (0, 0, 1), ((a0 + a1) / 2, y, b + h + 0.035)), self.logs,
                rnd.randrange(1 << 30))
        S.board((a1 - a0 + 0.14, 2 * r + 0.06, 0.05), frame((1, 0, 0), (0, 0, 1), ((a0 + a1) / 2, y - 0.01, b + 0.025)), self.logs,
                rnd.randrange(1 << 30))
        nb = 4
        bw = (a1 - a0) / nb
        for i in range(nb):  # полотно чуть утоплено в проём
            S.board((h - 0.06, bw * 0.97, 0.035), frame((0, 0, 1), (0, -1, 0), (a0 + bw * (i + 0.5), y + 0.02, b + 0.05 + (h - 0.06) / 2)),
                    self.boards, rnd.randrange(1 << 30))
        for z in (b + 0.18, b + h - 0.16):  # петли
            K.box(((a1 - a0) * 0.62, 0.012, 0.035), dark, loc=(a0 + (a1 - a0) * 0.33, y - 0.002, z), bevel=0.004)
        K.box((0.03, 0.03, 0.03), dark, loc=(a1 - 0.07, y - 0.005, b + h * 0.5), bevel=0.006)  # скоба

    def window(self, wall, a, z, w=0.3, h=0.3, shutters=True):
        """Окно с наличником, тёмным стеклом (слюдой) и распахнутыми ставнями."""
        rnd = self.rnd
        a0, a1 = a - w / 2, a + w / 2
        self.opening(wall, a0, a1, z - h / 2, z + h / 2)
        if wall == "front":
            pos = lambda u, out: Vector((u, self.y0 - out, z))  # noqa: E731
            along, n = Vector((1, 0, 0)), Vector((0, -1, 0))
        else:
            pos = lambda u, out: Vector((self.x1 + out, u, z))  # noqa: E731
            along, n = Vector((0, 1, 0)), Vector((1, 0, 0))
        glass = K.material("mica", [(40, 46, 52), (58, 62, 64)], rough=0.25, scale=30, bump=0.1, per_part=False)
        K.box((w if wall == "front" else 0.02, 0.02 if wall == "front" else w, h), glass, loc=pos(a, -0.01), bevel=0.0)
        t = 0.05
        for u in (a0 - t / 2, a1 + t / 2):  # стойки наличника
            S.board((h + 2 * t, t, 2 * self.r + 0.04), frame((0, 0, 1), n, pos(u, 0)), self.logs, rnd.randrange(1 << 30))
        for dz in (-(h / 2 + t / 2), h / 2 + t / 2):
            c = pos(a, 0) + Vector((0, 0, dz))
            S.board((w + 2 * t + 0.06, t * (1.4 if dz > 0 else 1.0), 2 * self.r + 0.06), frame(along, n, c), self.logs,
                    rnd.randrange(1 << 30))
        if shutters:
            for sgn in (-1, 1):  # ставни распахнуты и прижаты к стене
                edge = a + sgn * (w / 2 + t)
                for i in range(3):
                    u = edge + sgn * (w / 6) * (i + 0.5)
                    S.board((h + 0.02, w / 3 * 0.96, 0.025), frame((0, 0, 1), n, pos(u, self.r + 0.015)), self.boards,
                            rnd.randrange(1 << 30))
                for dz in (-h * 0.3, h * 0.3):  # поперечные планки
                    c = pos(edge + sgn * w / 4, self.r + 0.035) + Vector((0, 0, dz))
                    S.board((w / 2, 0.04, 0.015), frame(along, n, c), self.boards, rnd.randrange(1 << 30))

    def smoke_hole(self, x):
        """Дымник на коньке — дощатый короб с двускатной крышкой (изба топится по-чёрному)."""
        rnd = self.rnd
        zr = self.z_eave + self.rise + 0.06
        s, h = 0.2, 0.32
        for (dx, dy, ax) in ((0, -s / 2, (1, 0, 0)), (0, s / 2, (1, 0, 0)), (-s / 2, 0, (0, 1, 0)), (s / 2, 0, (0, 1, 0))):
            nrm = Vector((dx, dy, 0)).normalized()
            S.board((s + 0.02, h, 0.022), frame(ax, nrm, (x + dx, self.yc + dy, zr + h / 2 - 0.05)), self.boards, rnd.randrange(1 << 30))
        for sgn in (-1, 1):
            nrm = Vector((0, sgn * 0.8, 1)).normalized()
            S.board((s + 0.12, 0.17, 0.02), frame((1, 0, 0), nrm, (x, self.yc + sgn * 0.065, zr + h - 0.02)), self.tes,
                    rnd.randrange(1 << 30))


def woodpile(x0, y, z, rows, cols, rnd):
    """Поленница: короткие поленья торцами к зрителю (−Y), сложены рядами со сдвигом."""
    logs = S.grain_mat("weathered_planks")
    r = 0.04
    for k in range(rows):
        for i in range(cols - (k % 2)):
            x = x0 + (i + 0.5 * (k % 2)) * 2 * r * 1.02 + rnd.uniform(-0.006, 0.006)
            zz = z + r + k * 2 * r * 0.88
            L = rnd.uniform(0.2, 0.26)
            S.log((x, y - L / 2, zz), (x, y + L / 2, zz), r * rnd.uniform(0.85, 1.1), logs, rnd.randrange(1 << 30), sag=0.0)


def canopy(x0, x1, y0, y1, z_eave, rise, mat, logs, rnd, ridge_axis="y"):
    """Двускатный навес из тёса (крыльцо, вышка, ворота): конёк вдоль ridge_axis, по коньку — бревно."""
    if ridge_axis == "y":
        c, half, a0, a1 = (x0 + x1) / 2, (x1 - x0) / 2, y0, y1
    else:
        c, half, a0, a1 = (y0 + y1) / 2, (y1 - y0) / 2, x0, x1
    slope_len = math.hypot(half, rise)
    width = 0.12
    n = max(2, int((a1 - a0) / width))
    zr = z_eave + rise
    for side in (-1, 1):
        if ridge_axis == "y":
            d = Vector((side * half, 0, -rise)).normalized()
            normal = Vector((side * rise / half, 0, 1)).normalized()
        else:
            d = Vector((0, side * half, -rise)).normalized()
            normal = Vector((0, side * rise / half, 1)).normalized()
        for layer in (0, 1):
            for i in range(n + (0 if layer else 1)):
                a = a0 + (width / 2 if layer else 0) + i * (a1 - a0) / n + rnd.uniform(-0.006, 0.006)
                w = width * (rnd.uniform(0.82, 0.92) if layer == 0 else rnd.uniform(0.5, 0.62))
                along = Vector((0, a, 0)) if ridge_axis == "y" else Vector((a, 0, 0))
                ridge = (Vector((c, 0, zr)) if ridge_axis == "y" else Vector((0, c, zr))) + along
                ctr = ridge + d * (slope_len * 0.5 + 0.03) + normal * (0.011 + layer * 0.02)
                S.board((slope_len + 0.08, w, 0.022), frame(d, normal, ctr), mat, rnd.randrange(1 << 30))
    p0 = (c, a0 - 0.04, zr + 0.05) if ridge_axis == "y" else (a0 - 0.04, c, zr + 0.05)
    p1 = (c, a1 + 0.04, zr + 0.05) if ridge_axis == "y" else (a1 + 0.04, c, zr + 0.05)
    S.log(p0, p1, 0.055, logs, rnd.randrange(1 << 30), sag=0.0)
    return zr + 0.1


def post(x, y, z0, z1, logs, rnd, r=0.055):
    S.log((x, y, z0), (x, y, z1), r, logs, rnd.randrange(1 << 30), sag=0.004)


def porch(iz, a0, a1, depth, rnd):
    """Крыльцо: дощатый настил перед дверью, два столба и навес с коньком поперёк стены."""
    y_wall = iz.y0 - iz.r
    y_out = y_wall - depth
    x0, x1 = a0 - 0.16, a1 + 0.16
    n = int((x1 - x0) / 0.1)
    for i in range(n):  # настил
        x = x0 + (i + 0.5) * (x1 - x0) / n
        S.board((depth, (x1 - x0) / n * 0.95, 0.03), frame((0, -1, 0), (0, 0, 1), (x, y_wall - depth / 2, iz.base + 0.015)),
                iz.boards, rnd.randrange(1 << 30))
    top = iz.base + 1.08
    for x in (x0 + 0.05, x1 - 0.05):
        post(x, y_out + 0.05, 0.0, top, iz.logs, rnd, r=0.045)
    S.log((x0 - 0.04, y_out + 0.05, top + 0.03), (x1 + 0.04, y_out + 0.05, top + 0.03), 0.045, iz.logs, rnd.randrange(1 << 30))
    canopy(x0 - 0.08, x1 + 0.08, y_out - 0.12, y_wall + 0.02, top + 0.06, 0.32, iz.tes, iz.logs, rnd, ridge_axis="y")


def chimney(iz, x, stone):
    """Печная труба из камня — изба топится «по-белому»: выходит из заднего ската выше конька."""
    y = iz.yc + 0.32
    z0 = iz.z_eave + 0.2
    z1 = iz.z_eave + iz.rise + 0.42
    S.stone_block((0.3, 0.3, z1 - z0), (x, y, (z0 + z1) / 2), stone, seed=31, disp=0.012)
    S.stone_block((0.38, 0.38, 0.07), (x, y, z1 + 0.02), stone, seed=32, disp=0.008)
    return z1 + 0.06


def watchtower(cx, cy, rnd, logs, tes, boards, h=2.75, s=0.52):
    """Сторожевая вышка: четыре столба с раскосами, площадка с перилами и двускатная крыша."""
    hs = s / 2
    corners = [(cx - hs, cy - hs), (cx + hs, cy - hs), (cx + hs, cy + hs), (cx - hs, cy + hs)]
    for x, y in corners:
        post(x, y, 0.0, h + 0.62, logs, rnd, r=0.055)
    for i in range(4):  # раскосы крест-накрест по граням и обвязка
        (xa, ya), (xb, yb) = corners[i], corners[(i + 1) % 4]
        for z0, z1 in ((0.25, h * 0.5), (h * 0.5, h - 0.1)):
            S.log((xa, ya, z0), (xb, yb, z1), 0.03, logs, rnd.randrange(1 << 30), sag=0.0)
        S.log((xa, ya, h * 0.5), (xb, yb, h * 0.5), 0.035, logs, rnd.randrange(1 << 30), sag=0.0)
    n = 6
    for i in range(n):  # площадка
        y = cy - hs - 0.08 + (i + 0.5) * (s + 0.16) / n
        S.board((s + 0.2, (s + 0.16) / n * 0.95, 0.03), frame((1, 0, 0), (0, 0, 1), (cx, y, h)), boards, rnd.randrange(1 << 30))
    for i in range(4):  # перила и зашивка снизу перил
        (xa, ya), (xb, yb) = corners[i], corners[(i + 1) % 4]
        S.log((xa, ya, h + 0.3), (xb, yb, h + 0.3), 0.025, logs, rnd.randrange(1 << 30), sag=0.0)
        mid = Vector(((xa + xb) / 2, (ya + yb) / 2, h + 0.1))
        along = Vector((xb - xa, yb - ya, 0)).normalized()
        out = Vector((along.y, -along.x, 0))
        S.board((s, 0.17, 0.02), frame(along, out, mid + out * 0.06), boards, rnd.randrange(1 << 30))
    return canopy(cx - hs - 0.16, cx + hs + 0.16, cy - hs - 0.16, cy + hs + 0.16, h + 0.62, 0.3, tes, logs, rnd, ridge_axis="x")


def stake(x, y, h, logs, rnd, r=0.06):
    """Заострённое бревно частокола."""
    hh = h * rnd.uniform(0.95, 1.04)
    tip = r * 2.4
    S.log((x, y, -0.05), (x, y, hh - tip), r, logs, rnd.randrange(1 << 30), sag=0.0, segs=10)
    S.log((x, y, hh - tip), (x, y, hh), r, logs, rnd.randrange(1 << 30), r_end=0.008, sag=0.0, segs=10, ends=False)


def palisade(p0, p1, h, logs, rnd, gaps=()):
    """Частокол от p0 до p1; gaps — промежутки (в долях пути) под ворота и башни."""
    p0, p1 = Vector((*p0, 0)), Vector((*p1, 0))
    L = (p1 - p0).length
    n = int(L / 0.125)
    for i in range(n + 1):
        t = i / n
        if any(g0 <= t <= g1 for g0, g1 in gaps):
            continue
        q = p0 + (p1 - p0) * t
        stake(q.x + rnd.uniform(-0.01, 0.01), q.y + rnd.uniform(-0.01, 0.01), h, logs, rnd)


def gate(x0, x1, y, h, logs, tes, rnd):
    """Проезжие ворота частокола: два столба, перекладина и навес."""
    for x in (x0, x1):
        post(x, y, -0.05, h + 0.3, logs, rnd, r=0.08)
    S.log((x0 - 0.12, y, h + 0.25), (x1 + 0.12, y, h + 0.25), 0.07, logs, rnd.randrange(1 << 30))
    canopy(x0 - 0.2, x1 + 0.2, y - 0.2, y + 0.2, h + 0.33, 0.22, tes, logs, rnd, ridge_axis="x")


def corner_tower(cx, cy, s, seed, roof_rise=0.45):
    """Угловая башня острога: малый сруб с бойницами и двускатной кровлей."""
    t = Izba(cx - s / 2, cx + s / 2, cy - s / 2, cy + s / 2, 0.0, rows=15, r=0.065, step=0.12, rise=roof_rise, over=0.16, seed=seed)
    t.corner = 0.12
    for k, z in enumerate((0.75, 1.45)):
        t.window("front", cx, z, w=0.11, h=0.1, shutters=False)
        t.window("right", cy, z + 0.12 * (k - 0.5), w=0.11, h=0.1, shutters=False)
    t.walls()
    t.gables()
    return t.roof()


def ostrog(level):
    """Уровни 1–5: изба (1), крыльцо с навесом и печная труба (2), сторожевая вышка (3),
    второй ярус и частокол с тыла (4), частокол кругом с воротами и угловыми башнями (5)."""
    rnd = random.Random(21)
    stone = S.pbr("mossy_stone_wall", tile_m=0.9)
    # восстановленный фундамент — тот же старый камень, что в руинах
    fx0, fx1, fy0, fy1 = -1.72, 1.72, -1.52, 1.62
    S.stone_block((fx1 - fx0, fy1 - fy0, 0.36), ((fx0 + fx1) / 2, (fy0 + fy1) / 2, 0.16), stone, seed=1)
    base = 0.34
    two_storey = level >= 4
    iz = Izba(-1.48, 1.48, -1.28, 1.42, base, rows=15 if two_storey else 9, seed=1)
    iz.door(-0.62, -0.18, 0.82)
    iz.window("front", 0.75, base + 0.6)
    iz.window("right", 0.05, base + 0.6)
    if two_storey:  # окна верхнего яруса
        for x in (-0.4, 0.75):
            iz.window("front", x, base + 1.42)
        for y in (-0.6, 0.6):
            iz.window("right", y, base + 1.42)
    iz.walls()
    iz.gables()
    top = iz.roof()
    if level >= 2:
        porch(iz, -0.62, -0.18, 0.42, rnd)
        top = max(top, chimney(iz, 0.55, stone))
    else:
        iz.smoke_hole(-0.7)
        # каменные ступени к двери
        for k, (dy, h) in enumerate(((-1.78, 0.12), (-1.63, 0.24))):
            S.stone_block((0.62 - 0.04 * k, 0.17, h), (-0.4, dy, h / 2 - 0.01), stone, seed=10 + k, disp=0.015)
    if level >= 2:  # ступени крыльца — под навесом
        for k, (dy, h) in enumerate(((-1.86, 0.12), (-1.74, 0.24))):
            S.stone_block((0.62 - 0.04 * k, 0.13, h), (-0.4, dy, h / 2 - 0.01), stone, seed=10 + k, disp=0.015)
    if level >= 3:
        top = max(top, watchtower(-1.62, 1.68, rnd, iz.logs, iz.tes, iz.boards, h=iz.z_eave + 0.9))
    # хозяйство у избы
    woodpile(0.55, -1.8, 0.0, 4, 11, rnd)
    S.place("tree_stump_01", "tree_stump_01", (-1.15, -1.85, 0.0), scale=0.2, rot_z=0.4)
    S.place("wooden_axe", "wooden_axe", (-1.12, -1.84, 0.13), scale=0.45, rot_z=0.9, tilt=(0.0, 1.1, 0), ground=False)
    S.place("wooden_barrels_01", "wooden_barrels_01_barrel01", (1.8, -0.95, 0.0), scale=0.42, rot_z=0.3)
    S.place_group("wooden_bucket_01", (1.8, -0.55, 0.0), scale=0.45, rot_z=1.1)
    if level >= 4:  # частокол с тыла и слева
        ph = 0.95
        palisade((-1.97, -1.4), (-1.97, 1.97), ph, iz.logs, rnd, gaps=((0.82, 1.0),) if level >= 3 else ())
        palisade((-1.97, 1.97), (1.97, 1.97), ph, iz.logs, rnd, gaps=((0.0, 0.14),))
    if level >= 5:  # кругом: справа и спереди, ворота против крыльца, башни по флангам
        palisade((1.97, 1.97), (1.97, -1.97), ph, iz.logs, rnd, gaps=((0.0, 0.1),))
        palisade((1.97, -1.97), (-1.97, -1.97), ph, iz.logs, rnd, gaps=((0.53, 0.77), (0.86, 1.0)))
        gate(-0.78, -0.02, -1.97, ph + 0.15, iz.logs, iz.tes, rnd)
        top = max(top, corner_tower(-1.72, -1.72, 0.52, seed=7))
        top = max(top, corner_tower(1.72, 1.72, 0.52, seed=8))
    else:
        # природа по краям участка (на 5-м уровне участок обнесён частоколом)
        for i, (x, y) in enumerate([(-1.92, -1.2), (1.95, 0.6), (-1.9, 1.5), (1.4, 1.85), (-0.6, 1.88)]):
            if level >= 4 and (x < -1.8 or y > 1.8):
                continue
            S.place("rock_moss_set_02", ROCKS[i % len(ROCKS)], (x, y, 0.0), scale=rnd.uniform(0.14, 0.2), rot_z=rnd.uniform(0, 6.28))
        if level < 3:
            S.place("fir_sapling", "fir_sapling_c", (-1.95, 1.8, 0.0), scale=1.0, rot_z=2.0)
        for k, (fx, fy) in enumerate([(-1.95, 0.2), (1.92, 1.3), (0.6, 1.9)]):
            S.place("fern_02", "fern_02_" + "abcd"[k % 4], (fx, fy, 0.0), scale=rnd.uniform(0.5, 0.65), rot_z=rnd.uniform(0, 6.28))
    grass_ring(rnd, fx0, fx1, fy0, fy1, 30, spread=0.2, scale=(1.2, 1.8))
    return top + 0.15


def main():
    opts = R.args()
    levels = [int(v) for v in str(opts.get("levels", "0")).split(",")]
    for level in levels:
        sc = R.reset_scene(int(opts["samples"]))
        S.reset()
        K.palette()  # сцена сброшена: кэши материалов и заготовок model_kit тоже
        height = ruins() if level == 0 else ostrog(level)
        K.light_rig(sc)
        sc.view_settings.exposure = T.exposure(0.45 if level == 0 else 0.62)  # фототекстуры темнее процедурных цветов
        cam, pivot = R.setup_camera(SIZE, height, extra_w=int(height * R.PPU * 0.9))
        out_dir = os.path.join(opts["out"], "buildings")
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, f"lantern_l{level}.png")
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        R.clean_alpha(path)
        K.grade(path)
        with open(path[:-4] + ".json", "w") as f:
            json.dump({"size": SIZE, "pivot": [round(pivot[0], 1), round(pivot[1], 1)], "tilePx": R.TILE_PX,
                       "w": sc.render.resolution_x, "h": sc.render.resolution_y, "source": "blender:model_hq"}, f)
        print(f"[hq] level {level} -> {path}")


if __name__ == "__main__":
    main()
