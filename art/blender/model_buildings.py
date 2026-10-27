"""Постройки острога из реалистичных деталей (scan_kit: CC0-сканы и фототекстуры Poly Haven) — в одном стиле с
острогом (model_hq): срубы «в чашу», тёс, частокол, камень старого фундамента.

У каждой постройки три облика по уровню (_t1, _t2, _t3); у самострела и пушки поворотная часть рендерится в восьми
направлениях (_t<k>_d0.._d7). Частокол — столб и звенья к соседям (wall_post, wall_x, wall_y), движок соединяет их.

Запуск: blender -b -P art/blender/model_buildings.py -- --out art/out --only mill,granary --samples 64
"""
import json
import math
import os
import random
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_hq as HQ  # noqa: E402
import model_kit as K  # noqa: E402
import outfit_kit as O  # noqa: E402
import render_sprites as R  # noqa: E402
import scan_kit as S  # noqa: E402

frame = HQ.frame


def lin(c):
    return tuple((v / 255) ** 2.2 for v in c)


class Mats:
    def __init__(self):
        self.logs = S.grain_mat("weathered_planks")
        self.tes = S.grain_mat("wood_planks_grey", name="grain_tes", moss=0.6)
        self.boards = S.grain_mat("wood_planks_grey", name="grain_boards")
        self.stone = S.pbr("mossy_stone_wall", tile_m=0.9)
        self.ground = S.pbr("forest_ground_04", tile_m=1.2)
        self.iron = O.plain("iron_dark", lin((58, 54, 50)), rough=0.55, metal=0.8)
        self.canvas = O.fabric("burlap", lin((150, 138, 112)), scale=3)
        self.red = O.fabric("wool", lin((150, 40, 32)), scale=3)
        self.fur = O.fabric("fur", lin((120, 90, 60)), scale=2)


# ---------- общие детали ----------
def log_stack(x0, y0, z0, length, rows, cols, M, rnd, axis="x", r=0.06):
    """Штабель брёвен: брёвна вдоль axis, ряды друг на друге со сдвигом на полбревна."""
    for k in range(rows):
        n = cols - (k % 2)
        for i in range(n):
            off = (i + 0.5 * (k % 2)) * 2 * r * 1.02
            z = z0 + r + k * 2 * r * 0.86
            rr = r * rnd.uniform(0.85, 1.1)
            L = length * rnd.uniform(0.92, 1.0)
            if axis == "x":
                a, b = (x0, y0 + off, z), (x0 + L, y0 + off, z)
            else:
                a, b = (x0 + off, y0, z), (x0 + off, y0 + L, z)
            S.log(a, b, rr, M.logs, rnd.randrange(1 << 30), r_end=rr * 0.85, sag=0.004)


def post(x, y, z0, z1, M, rnd, r=0.05):
    S.log((x, y, z0), (x, y, z1), r, M.logs, rnd.randrange(1 << 30), sag=0.004)


def deck(x0, x1, y0, y1, z, M, rnd, axis="x", w=0.11, th=0.03, mat=None):
    """Дощатый настил на высоте z (верх досок)."""
    if axis == "x":
        n = max(1, int((y1 - y0) / w))
        for i in range(n):
            y = y0 + (i + 0.5) * (y1 - y0) / n
            S.board((x1 - x0, (y1 - y0) / n * 0.96, th), frame((1, 0, 0), (0, 0, 1), ((x0 + x1) / 2, y, z - th / 2)), mat or M.boards,
                    rnd.randrange(1 << 30))
    else:
        n = max(1, int((x1 - x0) / w))
        for i in range(n):
            x = x0 + (i + 0.5) * (x1 - x0) / n
            S.board((y1 - y0, (x1 - x0) / n * 0.96, th), frame((0, 1, 0), (0, 0, 1), (x, (y0 + y1) / 2, z - th / 2)), mat or M.boards,
                    rnd.randrange(1 << 30))


def platform(x0, x1, y0, y1, z, M, rnd, posts=2):
    """Помост на столбах: столбы по углам (и между), обвязка из брёвен, настил."""
    xs = [x0 + 0.06 + i * (x1 - x0 - 0.12) / (posts - 1) for i in range(posts)]
    ys = [y0 + 0.06 + i * (y1 - y0 - 0.12) / (posts - 1) for i in range(posts)]
    for x in xs:
        for y in ys:
            post(x, y, 0.0, z - 0.05, M, rnd, r=0.055)
    for y in (ys[0], ys[-1]):
        S.log((x0, y, z - 0.08), (x1, y, z - 0.08), 0.05, M.logs, rnd.randrange(1 << 30), sag=0.0)
    for x in (xs[0], xs[-1]):  # раскосы
        S.log((x, ys[0], 0.15), (x, ys[-1], z - 0.15), 0.03, M.logs, rnd.randrange(1 << 30), sag=0.0)
    deck(x0, x1, y0, y1, z, M, rnd)


def lean_to(x0, x1, y0, y1, z_front, z_back, M, rnd, over=0.12):
    """Навес: столбы, прогоны и односкатная кровля из тёса (скат к −Y, к зрителю)."""
    for x in (x0, (x0 + x1) / 2, x1):
        post(x, y0, 0.0, z_front, M, rnd)
        post(x, y1, 0.0, z_back, M, rnd)
    S.log((x0 - over, y0, z_front), (x1 + over, y0, z_front), 0.05, M.logs, rnd.randrange(1 << 30), sag=0.0)
    S.log((x0 - over, y1, z_back), (x1 + over, y1, z_back), 0.05, M.logs, rnd.randrange(1 << 30), sag=0.0)
    d = Vector((0, y0 - y1, z_front - z_back)).normalized()  # вниз по скату, к зрителю
    n = Vector((0, d.z, -d.y))                                 # нормаль ската: вверх и наружу
    L = math.hypot(y1 - y0 + 2 * over, z_back - z_front)
    mid = Vector(((x0 + x1) / 2, (y0 + y1) / 2, (z_front + z_back) / 2 + 0.06))
    w = 0.13
    cnt = int((x1 - x0 + 2 * over) / w)
    for layer in (0, 1):
        for i in range(cnt + (0 if layer else 1)):
            x = x0 - over + (w / 2 if layer else 0) + i * w
            ww = w * (rnd.uniform(0.82, 0.92) if layer == 0 else rnd.uniform(0.5, 0.62))
            S.board((L, ww, 0.022), frame(d, n, mid + Vector((x - (x0 + x1) / 2, 0, 0)) + n * (0.011 + 0.02 * layer)), M.tes, rnd.randrange(1 << 30))


def tent(cx, cy, r, h, M, rnd, cloth=None, rot=0.0):
    """Двускатная палатка из холста: два ската до земли, торцы-треугольники, шесты по концам, конёк-жердь."""
    cloth = cloth or M.canvas
    L, w = 2 * r * 1.15, r * 1.05  # длина вдоль конька и полуширина у земли
    m = Matrix.Translation((cx, cy, 0)) @ Matrix.Rotation(rot, 4, "Z")
    parts = []
    for sgn in (-1, 1):  # скаты
        d = Vector((0, sgn * w, -h)).normalized()
        n = Vector((0, sgn * h, w)).normalized()
        slope = math.hypot(w, h)
        c = Vector((0, sgn * w / 2, h / 2)) + n * 0.005
        parts.append(O._prism_obj("tent_side", [(-L / 2, -slope / 2), (L / 2, -slope / 2), (L / 2, slope / 2), (-L / 2, slope / 2)], 0.012, cloth,
                                  m @ frame((1, 0, 0), n, c) @ Matrix.Rotation(math.pi / 2, 4, "X")))
    dark = O.plain("tent_dark", lin((40, 34, 28)))
    for sx, mat in ((-1, cloth), (1, dark)):  # задний торец закрыт, передний — тёмный вход
        parts.append(O._prism_obj("tent_end", [(-w, 0.0), (w, 0.0), (0.0, h)], 0.01, mat,
                                  m @ Matrix.Translation((sx * L / 2 * 0.98, 0, 0)) @ Matrix.Rotation(math.pi / 2, 4, "Z")))
    for sx in (-1, 1):
        p0 = m @ Vector((sx * (L / 2 + 0.03), 0, 0))
        post(p0.x, p0.y, 0.0, h + 0.08, M, rnd, r=0.018)
    a, b = m @ Vector((-L / 2 - 0.05, 0, h + 0.01)), m @ Vector((L / 2 + 0.05, 0, h + 0.01))
    S.log(a, b, 0.015, M.logs, rnd.randrange(1 << 30), sag=0.0)
    return parts


def campfire(x, y, M, rnd):
    stones = ["rock_moss_set_02_rock07", "rock_moss_set_02_rock08", "rock_moss_set_02_rock09"]
    for i in range(7):
        a = i * 2 * math.pi / 7
        S.place("rock_moss_set_02", stones[i % 3], (x + math.cos(a) * 0.14, y + math.sin(a) * 0.14, 0.0), scale=0.06, rot_z=a)
    for i in range(3):
        a = i * 2.1
        S.log((x + math.cos(a) * 0.12, y + math.sin(a) * 0.12, 0.02), (x - math.cos(a) * 0.02, y - math.sin(a) * 0.02, 0.09), 0.025, M.logs,
              rnd.randrange(1 << 30), sag=0.0)
    fire = O.emissive("fire", (1.0, 0.45, 0.08), 8.0)
    O.revolve("flame", [(0.06, 0.04), (0.05, 0.1), (0.02, 0.17), (0.0, 0.2)], fire, Vector((x, y, 0)), segs=8)


def grass_around(rnd, half, n=24, spread=0.25):
    HQ.grass_ring(rnd, -half, half, -half, half, n, spread=spread, scale=(1.1, 1.6))


def rocks_around(rnd, half, n=3):
    for i in range(n):
        a = rnd.uniform(0, 6.28)
        S.place("rock_moss_set_02", HQ.ROCKS[i % len(HQ.ROCKS)], (math.cos(a) * half * 1.05, math.sin(a) * half * 1.05, 0.0),
                scale=rnd.uniform(0.1, 0.16), rot_z=rnd.uniform(0, 6.28))


# ---------- постройки ----------
def sawmill_site(tier):
    """Лесосека: штабеля брёвен, козлы с бревном, пни с топорами, навес; выше — пильные подмостья и изба."""
    M, rnd = Mats(), random.Random(31)
    half = 1.5
    log_stack(-1.25, -1.3, 0.0, 1.5, 3 + tier, 5 + tier, M, rnd)
    for x, y in ((0.75, -0.95), (1.15, -0.2), (0.5, 0.2)):
        S.place("tree_stump_01", "tree_stump_01", (x, y, 0.0), scale=rnd.uniform(0.2, 0.26), rot_z=rnd.uniform(0, 6.28))
    S.place("wooden_axe", "wooden_axe", (0.77, -0.93, 0.13), scale=0.45, rot_z=0.4, tilt=(0.0, 1.0, 0), ground=False)
    # козлы и бревно на них
    for x in (-0.6, 0.15):
        for s in (-1, 1):
            S.log((x, 0.55 + s * 0.12, 0.0), (x, 0.55 - s * 0.05, 0.42), 0.025, M.logs, rnd.randrange(1 << 30), sag=0.0)
    S.log((-0.85, 0.55, 0.42), (0.45, 0.55, 0.42), 0.075, M.logs, rnd.randrange(1 << 30))
    S.place("bark_debris_01", "bark_debris_01_a", (-0.2, 0.2, 0.0), scale=0.7, rot_z=1.0)
    S.place("bark_debris_01", "bark_debris_01_b", (0.3, -0.5, 0.0), scale=0.7, rot_z=2.0)
    if tier >= 2:  # навес над готовыми брёвнами
        lean_to(-1.35, 0.35, 0.95, 1.4, 0.9, 1.15, M, rnd)
        log_stack(-1.25, 1.0, 0.0, 1.5, 3, 3, M, rnd, r=0.05)
    if tier >= 3:  # пильные подмостья: бревно на высоком помосте, пила
        platform(0.55, 1.4, 0.55, 1.35, 0.55, M, rnd)
        S.log((0.5, 0.95, 0.63), (1.45, 0.95, 0.63), 0.08, M.logs, rnd.randrange(1 << 30))
    grass_around(rnd, half, 20)
    return 1.6 if tier >= 2 else 1.0


def mill(tier):
    """Мельница-столбовка: сруб-амбар на столбе с крестовиной, четыре крыла-решётки, лестница-«хвост» для поворота."""
    M, rnd = Mats(), random.Random(41)
    h = 1.0 + 0.25 * tier          # высота низа сруба
    w = 0.75 + 0.08 * tier
    # основание: камни и крестовина, на ней столб с подкосами
    for a in (0, math.pi / 2):
        S.log((math.cos(a) * -0.75, math.sin(a) * -0.75, 0.08), (math.cos(a) * 0.75, math.sin(a) * 0.75, 0.08), 0.07, M.logs,
              rnd.randrange(1 << 30), sag=0.0)
    post(0, 0, 0.0, h + 0.1, M, rnd, r=0.1)
    for a in (0.5, 2.07, 3.64, 5.21):
        S.log((math.cos(a) * 0.65, math.sin(a) * 0.65, 0.12), (math.cos(a) * 0.08, math.sin(a) * 0.08, h * 0.6), 0.045, M.logs, rnd.randrange(1 << 30),
              sag=0.0)
    if tier >= 2:  # каменный подклет под столбом
        S.stone_block((0.9, 0.9, 0.3), (0, 0, 0.14), M.stone, seed=4)
    # корпус: дощатый сруб, двускатная крыша, конёк вдоль Y (крылья смотрят к −Y)
    iz = HQ.Izba(-w / 2, w / 2, -w / 2 - 0.1, w / 2 + 0.1, h, rows=7 + tier, r=0.05, step=0.09, rise=0.42, over=0.12, seed=7)
    iz.corner = 0.08
    iz.window("right", 0.0, h + 0.35, w=0.16, h=0.16, shutters=False)
    iz.walls()
    iz.gables()
    top = iz.roof()
    # ось и крылья
    hub = Vector((0, -w / 2 - 0.22, h + (7 + tier) * 0.09 * 0.75))
    S.log(hub + Vector((0, 0.4, 0.06)), hub, 0.05, M.logs, 3, sag=0.0)
    L = 1.3 + 0.12 * tier
    for k in range(4):
        a = k * math.pi / 2 + 0.35
        d = Vector((math.cos(a), 0, math.sin(a)))
        S.log(hub, hub + d * L, 0.025, M.logs, rnd.randrange(1 << 30), sag=0.0, segs=6)
        side = Vector((-d.z, 0, d.x))
        for i in range(6):  # решётка крыла
            p = hub + d * (0.25 + i * (L - 0.25) / 5)
            S.log(p, p + side * 0.24, 0.01, M.boards, rnd.randrange(1 << 30), sag=0.0, segs=4)
        S.log(hub + d * 0.25 + side * 0.24, hub + d * L + side * 0.24, 0.012, M.boards, rnd.randrange(1 << 30), sag=0.0, segs=4)
        if True:  # холст на крыльях
            O._prism_obj("sail", [(0.0, 0.25), (0.0, L), (0.23, L), (0.23, 0.25)], 0.004, M.canvas,
                         Matrix.Translation(hub - Vector((0, 0.02, 0))) @ Matrix.Rotation(-a + math.pi / 2, 4, "Y"))
    # «хвост» — водило с лестницей к двери
    S.log((0, w / 2 + 0.1, h + 0.05), (0, w / 2 + 1.1, 0.05), 0.04, M.logs, 5, sag=0.0)
    for i in range(5):
        t = (i + 0.5) / 5
        S.log((-0.12, w / 2 + 0.1 + t, h * (1 - t) + 0.05), (0.12, w / 2 + 0.1 + t, h * (1 - t) + 0.05), 0.015, M.boards, i, sag=0.0)
    sack = O.fabric("burlap", lin((140, 118, 88)), scale=3)
    for i, (x, y) in enumerate(((0.95, -0.9), (1.15, -0.75), (1.05, -0.6))):
        o = O.revolve("sack", [(0.1, 0.0), (0.13, 0.06), (0.12, 0.13), (0.06, 0.17), (0.0, 0.18)], sack, Vector((x, y, 0)), segs=10,
                      scale=(1, 0.8, 1))
        o.rotation_euler.z = i
    grass_around(rnd, 1.5, 24)
    return max(top, hub.z + L) + 0.1


def granary(tier):
    """Амбар на сваях (от сырости и мышей): сруб на столбах, крыльцо-лесенка; выше — больше и с галереей."""
    M, rnd = Mats(), random.Random(51)
    stilt = 0.35
    x0, x1 = -1.1 - 0.1 * tier, 1.1 + 0.1 * tier
    y0, y1 = -0.95, 1.05
    for x in (x0 + 0.1, 0.0, x1 - 0.1):
        for y in (y0 + 0.1, y1 - 0.1):
            post(x, y, 0.0, stilt, M, rnd, r=0.08)
    iz = HQ.Izba(x0, x1, y0, y1, stilt, rows=7 + tier, r=0.07, step=0.125, rise=0.75, over=0.25, seed=12)
    iz.door(-0.2, 0.2, 0.7)
    iz.walls()
    iz.gables()
    top = iz.roof()
    # лесенка к двери
    for i in range(3):
        z = stilt * (1 - (i + 1) / 4)
        S.board((0.5, 0.14, 0.04), frame((1, 0, 0), (0, 0, 1), (0, y0 - 0.15 - i * 0.13, z)), M.boards, i)
    if tier >= 2:  # галерея-навес вдоль передней стены
        for x in (x0 + 0.1, x1 - 0.1):
            post(x, y0 - 0.38, 0.0, stilt + 0.9, M, rnd, r=0.04)
        deck(x0, x1, y0 - 0.45, y0 - 0.05, stilt + 0.02, M, rnd)
    sack = O.fabric("burlap", lin((140, 118, 88)), scale=3)
    for i, (x, y) in enumerate(((x1 + 0.2, -0.6), (x1 + 0.3, -0.35), (x1 + 0.15, -0.15))):
        O.revolve("sack", [(0.1, 0.0), (0.13, 0.06), (0.12, 0.13), (0.06, 0.17), (0.0, 0.18)], sack, Vector((x, y, 0)), segs=10, scale=(1, 0.8, 1))
    grass_around(rnd, 1.45, 22)
    return top + 0.1


def lumber_yard(tier):
    """Лесной двор: штабеля сохнущих брёвен и досок; выше — под общим навесом."""
    M, rnd = Mats(), random.Random(61)
    log_stack(-1.3, -1.25, 0.0, 1.2, 3 + tier, 6, M, rnd, axis="y", r=0.065)
    log_stack(0.1, -1.25, 0.0, 1.2, 2 + tier, 6, M, rnd, axis="y", r=0.065)
    # доски в клетке на подкладках
    for k in range(3 + tier):
        for i in range(6):
            S.board((1.0, 0.1, 0.025), frame((1, 0, 0), (0, 0, 1), (-0.8, 0.35 + i * 0.12, 0.05 + k * 0.05)), M.boards, k * 10 + i)
        S.log((-1.25, 0.3, 0.025 + k * 0.05), (-1.25, 1.05, 0.025 + k * 0.05), 0.012, M.logs, k, sag=0.0)
    if tier >= 2:
        lean_to(-1.4, 1.4, -1.35, 0.1, 1.05, 1.25, M, rnd)
    if tier >= 3:
        lean_to(-1.4, 0.0, 0.25, 1.4, 0.95, 1.15, M, rnd)
    S.place("wooden_axe", "wooden_axe", (0.7, 0.7, 0.0), scale=0.45, rot_z=0.4, tilt=(1.4, 0.0, 0), ground=True)
    grass_around(rnd, 1.5, 18)
    return 1.5 if tier >= 2 else 0.9


def barracks(tier):
    """Ратная изба: сруб с крыльцом, у стены — стойка с копьями, во дворе — соломенное чучело для учения."""
    M, rnd = Mats(), random.Random(71)
    S.stone_block((2.6, 2.3, 0.22), (0, 0.05, 0.1), M.stone, seed=2)
    iz = HQ.Izba(-1.15, 1.15, -0.95, 1.05, 0.2, rows=8 + 2 * (tier - 1), r=0.07, step=0.125, rise=0.85, over=0.25, seed=21)
    iz.door(-0.55, -0.15, 0.78)
    iz.window("front", 0.55, 0.2 + 0.55)
    iz.window("right", 0.0, 0.2 + 0.55)
    if tier >= 3:
        iz.window("front", 0.0, 0.2 + 1.2)
    iz.walls()
    iz.gables()
    top = iz.roof()
    # стойка с копьями у правой стены
    rack_x = 1.4
    S.log((rack_x, -0.6, 0.55), (rack_x, 0.4, 0.55), 0.025, M.logs, 3, sag=0.0)
    for y in (-0.6, 0.4):
        post(rack_x, y, 0.0, 0.6, M, rnd, r=0.03)
    steel = O.plain("steel", lin((150, 150, 150)), rough=0.35, metal=0.9)
    for i in range(4 + tier):
        y = -0.5 + i * 0.85 / (3 + tier)
        sp = O.spear(M.logs, steel, length=1.9)
        sp.matrix_world = Matrix.Translation((rack_x + 0.05, y, 0.0)) @ Matrix.Diagonal((0.42, 0.42, 0.42, 1)) @ Matrix.Translation((0, 0, 0.9)) \
            @ Matrix.Rotation(0.1, 4, "Y")
    # чучело: крест из жердей, соломенный сноп, старый шлем
    straw = O.fabric("burlap", lin((160, 136, 88)), scale=4)
    post(-1.35, -1.3, 0.0, 0.75, M, rnd, r=0.025)
    S.log((-1.55, -1.3, 0.6), (-1.15, -1.3, 0.6), 0.02, M.logs, 2, sag=0.0)
    O.revolve("dummy", [(0.07, 0.3), (0.1, 0.45), (0.09, 0.62), (0.05, 0.7), (0.0, 0.72)], straw, Vector((-1.35, -1.3, 0)), segs=10)
    O.revolve("dummy_head", [(0.05, 0.72), (0.06, 0.78), (0.0, 0.85)], straw, Vector((-1.35, -1.3, 0)), segs=10)
    grass_around(rnd, 1.45, 22)
    return top + 0.1


def workshop(tier):
    """Кузница: открытая к зрителю изба-навес, каменный горн с трубой, наковальня на чурбаке, мехи."""
    M, rnd = Mats(), random.Random(81)
    # задний сруб
    iz = HQ.Izba(-1.2, 1.2, 0.0, 1.2, 0.0, rows=8, r=0.07, step=0.125, rise=0.7, over=0.25, seed=31)
    iz.walls()
    iz.gables()
    top = iz.roof()
    # навес над рабочим двором
    lean_to(-1.2, 1.2, -0.85, -0.1, 1.3, 1.35, M, rnd, over=0.1)
    # горн и труба
    S.stone_block((0.7, 0.55, 0.55), (-0.6, -0.55, 0.27), M.stone, seed=5)
    fire = O.emissive("coals", (1.0, 0.35, 0.05), 5.0)
    O.revolve("coals", [(0.18, 0.55), (0.12, 0.58), (0.0, 0.59)], fire, Vector((-0.6, -0.55, 0)), segs=10)
    S.stone_block((0.32, 0.32, 1.6 + 0.2 * tier), (-0.6, -0.3, 0.8 + 0.1 * tier), M.stone, seed=6)
    # наковальня на чурбаке
    S.log((0.35, -0.6, 0.0), (0.35, -0.6, 0.35), 0.13, M.logs, 9, sag=0.0)
    anvil = O._prism_obj("anvil", [(-0.18, 0.0), (0.12, 0.0), (0.22, 0.05), (0.12, 0.09), (-0.12, 0.09), (-0.16, 0.04)], 0.1, M.iron,
                         Matrix.Translation((0.35, -0.6, 0.36)))
    # мехи
    leather = O.fabric("leather", lin((110, 70, 44)), scale=3, sat=0.5)
    O.revolve("bellows", [(0.0, 0.0), (0.22, 0.05), (0.2, 0.1), (0.0, 0.14)], leather, Vector((-1.0, -0.2, 0.35)), segs=10, scale=(1, 0.6, 1))
    S.place_group("wooden_bucket_01", (0.85, -0.95, 0.0), scale=0.45, rot_z=0.4)
    if tier >= 2:
        S.place("wooden_barrels_01", "wooden_barrels_01_barrel01", (1.1, -0.4, 0.0), scale=0.4)
    grass_around(rnd, 1.45, 18)
    return max(top, 1.7 + 0.2 * tier) + 0.1


def carpenter_hut(tier):
    """Изба плотника: маленький сруб, верстак с бревном, пила и щепа."""
    M, rnd = Mats(), random.Random(91)
    iz = HQ.Izba(-0.75, 0.75, -0.35, 0.85, 0.0, rows=7, r=0.06, step=0.11, rise=0.55, over=0.18, seed=41)
    iz.door(-0.35, 0.0, 0.62)
    iz.walls()
    iz.gables()
    top = iz.roof()
    iz.smoke_hole(0.3)
    # верстак (козлы) с бревном у входа
    for x in (-0.25, 0.45):
        for s in (-1, 1):
            S.log((x, -0.75 + s * 0.08, 0.0), (x, -0.75 - s * 0.03, 0.3), 0.02, M.logs, rnd.randrange(1 << 30), sag=0.0)
    S.log((-0.45, -0.75, 0.31), (0.65, -0.75, 0.31), 0.06, M.logs, 3)
    S.place("bark_debris_01", "bark_debris_01_c", (0.6, -0.5, 0.0), scale=0.6, rot_z=0.4)
    grass_around(rnd, 1.0, 12, spread=0.2)
    return top + 0.1


def trade_post(tier):
    """Торг: лавка с прилавком под навесом, связки пушнины, бочки и короба."""
    M, rnd = Mats(), random.Random(101)
    iz = HQ.Izba(-0.8, 0.8, -0.1, 0.9, 0.0, rows=7 + tier, r=0.06, step=0.11, rise=0.5, over=0.15, seed=51)
    iz.walls()
    iz.gables()
    top = iz.roof()
    lean_to(-0.8, 0.8, -0.85, -0.12, 0.75, 0.85, M, rnd, over=0.08)
    deck(-0.7, 0.7, -0.6, -0.35, 0.42, M, rnd, axis="y")
    for x in (-0.6, 0.6):
        post(x, -0.48, 0.0, 0.4, M, rnd, r=0.03)
    pelt = O.fabric("fur", lin((110, 74, 46)), scale=3)
    for i in range(3 + tier):  # шкурки на жерди под навесом
        x = -0.6 + i * 1.2 / (2 + tier)
        O._prism_obj("pelt", [(-0.06, 0.0), (0.06, 0.0), (0.08, -0.25), (0.0, -0.32), (-0.08, -0.25)], 0.015, pelt,
                     Matrix.Translation((x, -0.75, 0.72)))
    S.place("wooden_barrels_01", "wooden_barrels_01_barrel01", (-0.95, -0.85, 0.0), scale=0.38)
    S.place_group("wooden_crate_01", (0.9, -0.8, 0.0), scale=0.42, rot_z=0.3)
    grass_around(rnd, 1.0, 12, spread=0.2)
    return top + 0.1


def camp(tier):
    """Стан: шатры вокруг костра с котлом, копья в козлах, знамя."""
    M, rnd = Mats(), random.Random(111)
    spots = [(-0.75, 0.7, 0.3), (0.8, 0.75, -0.4), (-0.85, -0.65, 1.2), (0.9, -0.55, 1.9)]
    for i, (x, y, rot) in enumerate(spots[:1 + tier]):
        tent(x, y, 0.4, 0.55, M, rnd, cloth=M.canvas if i % 2 == 0 else O.fabric("burlap", lin((128, 112, 88)), scale=3), rot=rot)
    campfire(0.0, 0.0, M, rnd)
    # котёл на треноге
    for a in (0.3, 2.4, 4.5):
        S.log((math.cos(a) * 0.22, math.sin(a) * 0.22, 0.0), (0, 0, 0.5), 0.015, M.logs, rnd.randrange(1 << 30), sag=0.0)
    O.revolve("kettle", [(0.08, 0.26), (0.11, 0.3), (0.11, 0.36), (0.09, 0.4)], M.iron, Vector((0, 0, 0)), segs=12)
    # копья в козлах
    steel = O.plain("steel", lin((150, 150, 150)), rough=0.35, metal=0.9)
    for i in range(4):
        a = i * math.pi / 2 + 0.4
        sp = O.spear(M.logs, steel, length=1.9)
        sp.matrix_world = Matrix.Translation((0.2 + math.cos(a) * 0.12, -1.1 + math.sin(a) * 0.12, 0.0)) @ Matrix.Diagonal((0.42, 0.42, 0.42, 1)) \
            @ Matrix.Translation((0, 0, 0.9)) @ Matrix.Rotation(0.18, 4, "X") @ Matrix.Rotation(a, 4, "Z")
    if tier >= 2:  # знамя
        post(1.3, -1.25, 0.0, 1.6, M, rnd, r=0.025)
        O._prism_obj("banner", [(0.0, 0.0), (0.45, 0.02), (0.4, -0.18), (0.45, -0.38), (0.0, -0.36)], 0.006, M.red,
                     Matrix.Translation((1.3, -1.25, 1.55)) @ Matrix.Rotation(0.6, 4, "Z"))
    grass_around(rnd, 1.45, 16)
    return 1.7 if tier >= 2 else 1.1


# ---------- оборона ----------
class Turret:
    """Поворотная часть: объекты, которые вращаются вокруг вертикали через pivot; aim — угол (Blender, рад), куда она смотрит в
    собранном виде. Для спрайта _dK часть поворачивается так, чтобы смотреть на угол K·45°."""

    def __init__(self, pivot, aim):
        self.objs, self.base = [], []
        self.pivot, self.aim = Vector(pivot), aim

    def add(self, *objs):
        for o in objs:
            if o is None:
                continue
            for x in (o if isinstance(o, (list, tuple)) else [o]):
                self.objs.append(x)
                self.base.append(x.matrix_world.copy())

    def turn(self, angle):
        r = Matrix.Translation(self.pivot) @ Matrix.Rotation(angle - self.aim, 4, "Z") @ Matrix.Translation(-self.pivot)
        for o, m in zip(self.objs, self.base):
            o.matrix_world = r @ m
        bpy.context.view_layer.update()


def _new_objs(before):
    return [o for o in bpy.context.scene.objects if o not in before and o.type == "MESH"]


def crossbow_tower(tier):
    """Самострел: помост на столбах (выше с уровнем), на вертлюге — тяжёлый станковый арбалет с болтом."""
    M, rnd = Mats(), random.Random(121)
    z = (0.35, 0.7, 1.05)[tier - 1]
    platform(-0.9, 0.9, -0.9, 0.9, z, M, rnd, posts=3 if tier >= 2 else 2)
    if tier >= 3:  # бруствер из брёвен по краю помоста
        for (a, b) in (((-0.9, -0.9), (0.9, -0.9)), ((0.9, -0.9), (0.9, 0.9)), ((0.9, 0.9), (-0.9, 0.9)), ((-0.9, 0.9), (-0.9, -0.9))):
            for k in range(2):
                S.log((a[0], a[1], z + 0.06 + k * 0.11), (b[0], b[1], z + 0.06 + k * 0.11), 0.055, M.logs, rnd.randrange(1 << 30), sag=0.0)
    if tier >= 2:  # лестница
        for x in (-0.2, 0.2):
            S.log((x, -1.4, 0.0), (x, -0.92, z), 0.025, M.logs, rnd.randrange(1 << 30), sag=0.0)
        for i in range(int(z / 0.18)):
            t = (i + 0.5) / int(z / 0.18)
            S.log((-0.2, -1.4 + 0.48 * t, z * t), (0.2, -1.4 + 0.48 * t, z * t), 0.015, M.boards, i, sag=0.0)
    before = set(bpy.context.scene.objects)
    post(0, 0, z, z + 0.42, M, rnd, r=0.07)  # вертлюг
    t = Turret((0, 0, 0), 0.0)
    y = z + 0.48
    stock = S.board((1.25, 0.12, 0.1), frame((1, 0, 0), (0, 0, 1), (0.15, 0, y)), M.logs, 3)
    steel = O.plain("steel", lin((150, 150, 150)), rough=0.35, metal=0.9)
    arms = []
    for sgn in (-1, 1):  # плечи лука: дуга из брёвнышек
        pts = [(0.62 - 0.12 * (k / 4) ** 2, sgn * 0.68 * k / 4, y + 0.02) for k in range(5)]
        for p0, p1 in zip(pts, pts[1:]):
            arms.append(S.log(p0, p1, 0.04 * (1 - 0.3 * abs(p0[1]) / 0.68), M.logs, rnd.randrange(1 << 30), sag=0.0, segs=8))
    string = [S.log((0.5, sgn * 0.68, y + 0.02), (-0.2, 0.0, y + 0.06), 0.006, O.plain("string", lin((210, 200, 170))), 1, sag=0.0, segs=4)
              for sgn in (-1, 1)]
    bolt = S.log((-0.2, 0, y + 0.08), (0.75, 0, y + 0.08), 0.018, M.boards, 9, sag=0.0)
    tip = O.revolve("bolt_tip", [(0.03, 0.0), (0.0, 0.12)], steel, Vector((0, 0, 0)), segs=6)
    tip.matrix_world = Matrix.Translation((0.75, 0, y + 0.08)) @ Matrix.Rotation(math.pi / 2, 4, "Y")
    sc = Matrix.Translation((0, 0, y)) @ Matrix.Diagonal((1.6, 1.6, 1.6, 1)) @ Matrix.Translation((0, 0, -y))  # крупнее, чтобы читался
    for o in _new_objs(before):
        o.matrix_world = sc @ o.matrix_world
    t.add(_new_objs(before))
    grass_around(rnd, 1.45, 16)
    return t, z + 1.2


def cannon_battery(tier):
    """Пушка: орудие на лафете (скан) на бревенчатом настиле, туры из плетня с землёй, ядра и бочонок пороха."""
    M, rnd = Mats(), random.Random(131)
    deck(-1.2, 1.2, -1.2, 1.2, 0.12, M, rnd, axis="y")
    for x in (-1.0, 0.0, 1.0):
        S.log((x, -1.25, 0.05), (x, 1.25, 0.05), 0.05, M.logs, rnd.randrange(1 << 30), sag=0.0)
    wicker = O.fabric("burlap", lin((100, 80, 56)), scale=6)
    earth = M.ground
    gabions = [(-1.15, 1.0), (-0.6, 1.2), (0.6, 1.2), (1.15, 1.0)] + ([(-1.25, 0.3), (1.25, 0.3)] if tier >= 2 else []) \
        + ([(-1.25, -0.4), (1.25, -0.4)] if tier >= 3 else [])
    for x, y in gabions:  # туры: плетёные корзины с землёй
        O.revolve("gabion", [(0.22, 0.0), (0.23, 0.45), (0.0, 0.46)], wicker, Vector((x, y, 0.12)), segs=14, uv_scale=(5, 2))
        O.revolve("gabion_earth", [(0.21, 0.44), (0.12, 0.5), (0.0, 0.52)], earth, Vector((x, y, 0.12)), segs=14)
    for i in range(5 + tier):  # ядра пирамидкой
        a = i * 1.3
        r = 0.0 if i == 0 else 0.1
        O.revolve("ball", [(0.0, -0.05), (0.05, 0.0), (0.0, 0.05)], M.iron, Vector((0.9 + math.cos(a) * r, -0.9 + math.sin(a) * r, 0.17 + (0.07 if i == 0 else 0))),
                  segs=10)
    S.place("wooden_barrels_01", "wooden_barrels_01_barrel01", (-0.95, -0.95, 0.12), scale=0.32)
    before = set(bpy.context.scene.objects)
    S.place_group("cannon_01", (0, 0.1, 0.12), scale=0.5 + 0.05 * tier, parts=["cannon_01_barrel", "cannon_01_frame", "cannon_01_pusherblock",
                                                                                 "cannon_01_wheel"])
    t = Turret((0, 0.1, 0), -math.pi / 2)  # ствол скана смотрит к −Y
    t.add(_new_objs(before))
    grass_around(rnd, 1.45, 14)
    return t, 1.0


def pitch_cauldron(tier):
    """Смоляной котёл: каменный очаг на помосте, чугунный котёл над огнём, бочки смолы, черпак."""
    M, rnd = Mats(), random.Random(141)
    z = (0.2, 0.45, 0.75)[tier - 1]
    if tier >= 2:
        platform(-0.85, 0.85, -0.85, 0.85, z, M, rnd)
    else:
        deck(-0.85, 0.85, -0.85, 0.85, z, M, rnd)
    S.stone_block((0.8, 0.8, 0.25), (0, 0, z + 0.12), M.stone, seed=8, disp=0.02)
    fire = O.emissive("fire", (1.0, 0.45, 0.08), 7.0)
    O.revolve("flame", [(0.2, z + 0.25), (0.14, z + 0.32), (0.0, z + 0.36)], fire, Vector((0, 0, 0)), segs=10)
    O.revolve("cauldron", [(0.12, z + 0.3), (0.28, z + 0.36), (0.32, z + 0.5), (0.3, z + 0.62), (0.33, z + 0.64)], M.iron, Vector((0, 0, 0)), segs=20)
    pitch = O.plain("pitch", lin((22, 16, 12)), rough=0.15)
    O.revolve("pitch", [(0.29, z + 0.6), (0.0, z + 0.6)], pitch, Vector((0, 0, 0)), segs=20)
    S.place("wooden_barrels_01", "wooden_barrels_01_barrel01", (0.55, -0.55, z), scale=0.3)
    if tier >= 2:
        S.place("wooden_barrels_01", "wooden_barrels_01_barrel01", (-0.55, -0.6, z), scale=0.3)
    grass_around(rnd, 0.95, 10, spread=0.2)
    return z + 0.9


def wall_gun_tower(tier):
    """Затинная пищаль: небольшая рубленая вышка, на верхней площадке — тяжёлое ружьё на сошке."""
    M, rnd = Mats(), random.Random(151)
    rows = (8, 10, 12)[tier - 1]
    tw = HQ.Izba(-0.7, 0.7, -0.7, 0.7, 0.0, rows=rows, r=0.065, step=0.12, rise=0.0, over=0.0, seed=61)
    tw.corner = 0.12
    tw.window("front", 0.0, 0.6, w=0.12, h=0.1, shutters=False)
    tw.walls()
    top = rows * 0.12 + 0.1
    deck(-0.85, 0.85, -0.85, 0.85, top + 0.03, M, rnd)
    for x in (-0.8, 0.8):  # перила
        for y in (-0.8, 0.8):
            post(x, y, top, top + 0.35, M, rnd, r=0.03)
    for (a, b) in (((-0.8, -0.8), (0.8, -0.8)), ((0.8, -0.8), (0.8, 0.8)), ((0.8, 0.8), (-0.8, 0.8)), ((-0.8, 0.8), (-0.8, -0.8))):
        S.log((a[0], a[1], top + 0.32), (b[0], b[1], top + 0.32), 0.025, M.logs, rnd.randrange(1 << 30), sag=0.0)
    gun = O.musket(M.logs, M.iron, length=2.1)
    gun.matrix_world = Matrix.Translation((0.1, 0.1, top + 0.3)) @ Matrix.Rotation(-0.25, 4, "Z") @ Matrix.Rotation(1.45, 4, "Y") \
        @ Matrix.Diagonal((0.6, 0.6, 0.6, 1))
    for sgn in (-1, 1):  # сошка
        S.log((0.62 + sgn * 0.05, -0.1, top + 0.03), (0.6, -0.08, top + 0.3), 0.012, M.logs, rnd.randrange(1 << 30), sag=0.0)
    grass_around(rnd, 0.95, 10, spread=0.2)
    return top + 0.6


def wolf_pit(tier):
    """Волчья яма: тёмный провал с заострёнными кольями, края прикрыты ветками."""
    M, rnd = Mats(), random.Random(161 + tier)
    O.revolve("pit", [(0.38, 0.005), (0.3, -0.02), (0.0, -0.02)], O.plain("pit_dark", lin((22, 18, 14)), rough=1.0), Vector((0, 0, 0)), segs=16,
              scale=(1, 1, 1))
    for i in range(3 + 2 * tier):
        a = rnd.uniform(0, 6.28)
        r = rnd.uniform(0.0, 0.22)
        h = 0.18 + 0.05 * tier
        S.log((math.cos(a) * r, math.sin(a) * r, -0.02), (math.cos(a) * r * 1.1, math.sin(a) * r * 1.1, h - 0.06), 0.022, M.logs, rnd.randrange(1 << 30),
              sag=0.0, segs=6)
        S.log((math.cos(a) * r * 1.1, math.sin(a) * r * 1.1, h - 0.06), (math.cos(a) * r * 1.12, math.sin(a) * r * 1.12, h), 0.022, M.logs,
              rnd.randrange(1 << 30), r_end=0.003, sag=0.0, segs=6, ends=False)
    for i in range(5):  # ветки по краю
        a = rnd.uniform(0, 6.28)
        S.log((math.cos(a) * 0.45, math.sin(a) * 0.45, 0.01), (math.cos(a + 0.6) * 0.1, math.sin(a + 0.6) * 0.1, 0.04), 0.012, M.logs,
              rnd.randrange(1 << 30), sag=0.01, segs=5)
    return 0.4


def palisade_piece(tier, part):
    """Звено частокола: столб в центре клетки (post) или колья до центра соседней клетки по +X (x) или +Y (y)."""
    M, rnd = Mats(), random.Random(171 + tier * 7 + len(part))
    h = (0.38, 0.46, 0.55)[tier - 1]
    if part == "post":
        HQ.stake(0, 0, h + 0.05, M.logs, rnd, r=0.05)
        if tier >= 3:
            S.stone_block((0.24, 0.24, 0.08), (0, 0, 0.03), M.stone, seed=3, disp=0.01)
        return h + 0.1
    d = Vector((1, 0, 0)) if part == "x" else Vector((0, 1, 0))
    n = 7
    for i in range(1, n):
        p = d * (i / n)
        HQ.stake(p.x + rnd.uniform(-0.008, 0.008), p.y + rnd.uniform(-0.008, 0.008), h * rnd.uniform(0.94, 1.02), M.logs, rnd, r=0.045)
    if tier >= 2:  # поперечная слега с внутренней стороны
        side = Vector((0, 0.05, 0)) if part == "x" else Vector((0.05, 0, 0))
        S.log(side + Vector((0, 0, h * 0.55)), d + side + Vector((0, 0, h * 0.55)), 0.02, M.logs, 5, sag=0.0)
    if tier >= 3:
        S.stone_block((0.9 if part == "x" else 0.2, 0.2 if part == "x" else 0.9, 0.07), d * 0.5 + Vector((0, 0, 0.03)), M.stone, seed=4, disp=0.01)
    return h + 0.1


TURRETS = {"ballista": (3, crossbow_tower), "catapult": (3, cannon_battery)}
DEFENSE = {"brazier": (2, pitch_cauldron), "prism": (2, wall_gun_tower), "trap": (1, wolf_pit)}


BUILDERS = {
    "oreMine": (3, sawmill_site), "aetherWell": (3, mill), "aetherStore": (3, granary), "oreStore": (3, lumber_yard),
    "barracks": (3, barracks), "workshop": (3, workshop), "builderHut": (2, carpenter_hut), "beacon": (2, trade_post), "camp": (3, camp),
}


def shot(sc, size, height, path, extra=None):
    cam, pivot = R.setup_camera(size, height, extra_w=int(height * R.PPU * 0.9) if extra is None else extra)
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    R.clean_alpha(path)
    K.grade(path)
    with open(path[:-4] + ".json", "w") as f:
        json.dump({"size": size, "pivot": [round(pivot[0], 1), round(pivot[1], 1)], "tilePx": R.TILE_PX,
                   "w": sc.render.resolution_x, "h": sc.render.resolution_y, "source": "blender:model_buildings"}, f)
    print(f"[buildings] -> {path}")


def scene(opts):
    sc = R.reset_scene(int(opts["samples"]))
    S.reset()
    K.palette()
    return sc


def finish(sc):
    K.light_rig(sc)
    sc.view_settings.exposure = 0.62


def main():
    opts = R.args()
    only = opts.get("only") or set()  # render_sprites.args() уже разбирает --only в множество
    tiers = [int(t) for t in str(opts.get("tiers", "1,2,3")).split(",")]
    out_dir = os.path.join(opts["out"], "buildings")
    os.makedirs(out_dir, exist_ok=True)
    for bid, (size, fn) in list(BUILDERS.items()) + list(DEFENSE.items()):
        if only and bid not in only:
            continue
        for tier in tiers:
            if bid == "builderHut" and tier > 1:
                continue
            sc = scene(opts)
            height = fn(tier)
            finish(sc)
            shot(sc, size, height, os.path.join(out_dir, f"{bid}_t{tier}.png"))
    for bid, (size, fn) in TURRETS.items():
        if only and bid not in only:
            continue
        for tier in tiers:
            sc = scene(opts)
            t, height = fn(tier)
            finish(sc)
            t.turn(-math.pi / 4)  # обычный вид (магазин, миниатюры): смотрит на зрителя
            shot(sc, size, height, os.path.join(out_dir, f"{bid}_t{tier}.png"))
            for d in range(8):
                t.turn(d * math.pi / 4)
                shot(sc, size, height, os.path.join(out_dir, f"{bid}_t{tier}_d{d}.png"))
    if not only or "wall" in only:
        for tier in tiers:
            for part in ("post", "x", "y"):
                sc = scene(opts)
                height = palisade_piece(tier, part)
                finish(sc)
                shot(sc, 3, height, os.path.join(out_dir, f"wall_t{tier}_{part}.png"), extra=0)


if __name__ == "__main__":
    main()
