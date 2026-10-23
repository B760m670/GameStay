"""Пробный состав (концепт «Состав»): пути и три вагона — крытый грузовой, бронированный с картечницей, цистерна для воды.
Собирается из частей model_kit, рендерится той же изометрической камерой, что и постройки.

Запуск: blender -b -P art/blender/model_wagon.py -- --out art/out --samples 64
Результат: <out>/buildings/decor_train.png/.json (центр путей — точка привязки).
"""
import json
import math
import os
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_kit as K  # noqa: E402
import render_sprites as R  # noqa: E402

CAR_L, CAR_W = 3.1, 1.12   # длина и ширина кузова, м (клетка = 1 м)
GAP = 0.45                  # промежуток между вагонами
RAIL_Y = 0.36               # полуколея
DECK = 0.66                 # верх рамы


def materials():
    M = K.palette()
    M.update({
        "paint_red": K.material("paint_red", [(156, 62, 42), (142, 54, 38), (168, 70, 48)], rough=0.75, kind="wave", scale=3, bump=0.3),
        "steel": K.material("steel", [(108, 116, 110), (96, 104, 100), (118, 124, 116)], rough=0.5, metal=0.6, scale=6, bump=0.15),
        "gravel": K.material("gravel", [(124, 112, 98), (108, 98, 86), (136, 124, 108)], rough=0.95, scale=40, bump=0.8),
        "rail": K.material("rail", [(92, 88, 88), (80, 76, 78)], rough=0.35, metal=0.9, kind="none"),
        "dark": K.material("dark", [(30, 24, 22), (26, 20, 18)], rough=0.9, kind="none"),
        "sandbag": K.material("sandbag", [(184, 160, 116), (170, 146, 104), (196, 172, 128)], rough=0.95, scale=18, bump=0.3),
    })
    return M


def track(M, length):
    """Насыпь из щебня, шпалы, два рельса."""
    K.box((length, 1.7, 0.1), M["gravel"], loc=(0, 0, 0.05), bevel=0.06)
    n = int(length / 0.34)
    for i in range(n):
        x = -length / 2 + (i + 0.5) * length / n
        K.box((0.16, 1.32, 0.07), M["timber"], loc=(x, 0, 0.12), bevel=0.012)
    for y in (-RAIL_Y, RAIL_Y):
        K.box((length, 0.06, 0.05), M["rail"], loc=(0, y, 0.175), bevel=0.008)
        K.box((length, 0.1, 0.02), M["rail"], loc=(0, y, 0.16), bevel=0.004)


def bogie(M, x):
    """Тележка: боковины, рессоры, две колёсные пары."""
    for y in (-RAIL_Y - 0.08, RAIL_Y + 0.08):
        K.box((1.0, 0.06, 0.14), M["iron"], loc=(x, y, 0.42), bevel=0.02)
        K.box((0.24, 0.07, 0.1), M["iron"], loc=(x, y, 0.54), bevel=0.02)
        for k in range(3):  # витки рессоры
            K.cylinder(0.035, 0.02, M["rail"], loc=(x - 0.06 + k * 0.06, y, 0.5), verts=10, bevel=0)
    for dx in (-0.32, 0.32):
        K.cylinder(0.03, 2 * RAIL_Y + 0.1, M["iron"], loc=(x + dx, 0, 0.38), rot=(math.pi / 2, 0, 0), verts=10, bevel=0)
        for y in (-RAIL_Y, RAIL_Y):
            K.cylinder(0.2, 0.06, M["iron"], loc=(x + dx, y - 0.02 * (1 if y > 0 else -1), 0.38), rot=(math.pi / 2, 0, 0), verts=28, bevel=0.01)
            K.cylinder(0.215, 0.015, M["rail"], loc=(x + dx, y + 0.03 * (1 if y > 0 else -1), 0.38), rot=(math.pi / 2, 0, 0), verts=28, bevel=0)
            K.cylinder(0.06, 0.08, M["rail"], loc=(x + dx, y - 0.04 * (1 if y > 0 else -1), 0.38), rot=(math.pi / 2, 0, 0), verts=14, bevel=0)


def chassis(M, xc):
    """Рама, сцепки и буфера, тележки."""
    K.box((CAR_L, CAR_W - 0.1, 0.16), M["timber"], loc=(xc, 0, DECK - 0.08), bevel=0.02)
    K.box((CAR_L + 0.02, 0.08, 0.2), M["iron"], loc=(xc, -(CAR_W - 0.1) / 2, DECK - 0.1), bevel=0.015)
    for s in (-1, 1):
        xe = xc + s * (CAR_L / 2 + 0.06)
        K.box((0.12, CAR_W - 0.2, 0.22), M["timber"], loc=(xe, 0, DECK - 0.1), bevel=0.02)
        for y in (-0.3, 0.3):
            K.cylinder(0.055, 0.16, M["iron"], loc=(xe + s * 0.1, y, DECK - 0.1), rot=(0, math.pi / 2, 0), verts=14, bevel=0.01)
            K.cylinder(0.08, 0.025, M["rail"], loc=(xe + s * 0.18, y, DECK - 0.1), rot=(0, math.pi / 2, 0), verts=16, bevel=0)
        K.box((0.18, 0.1, 0.08), M["iron"], loc=(xe + s * 0.12, 0, DECK - 0.14), bevel=0.01)
        bogie(M, xc + s * 1.0)


def side_faces(xc, z0, height):
    """Видимые грани кузова: длинный бок (-Y) и торец (+X)."""
    side = K.Face((xc - CAR_L / 2, -CAR_W / 2, z0), (1, 0, 0), (0, -1, 0), CAR_L, height)
    end = K.Face((xc + CAR_L / 2, -CAR_W / 2, z0), (0, 1, 0), (1, 0, 0), CAR_W, height)
    return side, end


def planks(face, z0, z1, M, mat, width=0.13):
    n = max(1, round(face.L / width))
    for i in range(n):
        u0 = i * face.L / n
        face.panel(u0 + 0.004, u0 + face.L / n - 0.004, z0, z1, 0.04, M[mat], out=0.0, bevel=0.008)


def boxcar(M, xc):
    """Крытый грузовой вагон: доски в красной краске, раскосы, сдвижная дверь, крыша с ходовой доской."""
    z0, h = DECK, 1.08
    K.box((CAR_L - 0.02, CAR_W - 0.02, h), M["paint_red"], loc=(xc, 0, z0 + h / 2), bevel=0.0)
    side, end = side_faces(xc, 0, h)
    planks(side, z0, z0 + h, M, "paint_red")
    planks(end, z0, z0 + h, M, "paint_red")
    for f in (side, end):  # уголки, обвязки
        for u in (0.0, f.L):
            f.beam(u, z0, u, z0 + h, M["iron"], thick=0.07, depth=0.03, out=0.04)
        f.beam(0, z0 + 0.04, f.L, z0 + 0.04, M["timber"], thick=0.08, depth=0.04, out=0.04)
        f.beam(0, z0 + h - 0.04, f.L, z0 + h - 0.04, M["timber"], thick=0.08, depth=0.04, out=0.04)
    for u0, u1 in ((0.08, 0.95), (CAR_L - 0.08, CAR_L - 0.95)):  # раскосы по бокам от двери
        side.beam(u0, z0 + 0.08, u1, z0 + h - 0.08, M["timber"], thick=0.07, depth=0.035, out=0.04)
        side.beam(u1, z0 + 0.08, u0, z0 + h - 0.08, M["timber"], thick=0.07, depth=0.035, out=0.04)
    dc, dw = CAR_L / 2, 0.86  # сдвижная дверь на рельсах
    side.panel(dc - dw / 2, dc + dw / 2, z0 + 0.04, z0 + h - 0.06, 0.05, M["plank"], out=0.05)
    for zz in (0.3, 0.7):
        side.beam(dc - dw / 2, z0 + h * zz, dc + dw / 2, z0 + h * zz, M["iron"], thick=0.035, depth=0.02, out=0.1)
    side.beam(dc - dw / 2 - 0.2, z0 + h + 0.02, dc + dw / 2 + 0.2, z0 + h + 0.02, M["iron"], thick=0.04, depth=0.04, out=0.06)
    side.beam(dc + dw * 0.32, z0 + h * 0.45, dc + dw * 0.32, z0 + h * 0.55, M["iron"], thick=0.05, depth=0.03, out=0.11)
    # крыша: пологий скат, ходовая доска, тормозной штурвал на торце
    K.box((CAR_L + 0.1, CAR_W + 0.12, 0.07), M["timber"], loc=(xc, 0, z0 + h + 0.05), bevel=0.02)
    for s in (-1, 1):
        K.box((CAR_L + 0.1, (CAR_W + 0.12) / 2, 0.05), M["paint_red"], loc=(xc, s * (CAR_W + 0.12) / 4, z0 + h + 0.12),
              rot=(s * -0.12, 0, 0), bevel=0.012)
    K.box((CAR_L - 0.1, 0.22, 0.04), M["plank"], loc=(xc, 0, z0 + h + 0.18), bevel=0.008)
    K.cylinder(0.02, 0.4, M["iron"], loc=(xc + CAR_L / 2 + 0.08, 0.25, z0 + h + 0.02), verts=8, bevel=0)
    K.cylinder(0.13, 0.02, M["iron"], loc=(xc + CAR_L / 2 + 0.08, 0.25, z0 + h + 0.22), verts=20, bevel=0)
    for k in range(4):  # лесенка на торце
        end.beam(0.12, z0 + 0.15 + k * 0.22, 0.32, z0 + 0.15 + k * 0.22, M["iron"], thick=0.02, depth=0.02, out=0.08)


def armored(M, xc):
    """Бронированный вагон: стальные листы с заклёпками, бойницы, мешки с песком, картечница на поворотной площадке."""
    z0, h = DECK, 0.95
    K.box((CAR_L - 0.02, CAR_W - 0.02, h), M["steel"], loc=(xc, 0, z0 + h / 2), bevel=0.0)
    side, end = side_faces(xc, 0, h)
    for f in (side, end):
        n = max(1, round(f.L / 0.62))
        for i in range(n):  # листы брони
            u0, u1 = i * f.L / n, (i + 1) * f.L / n
            f.panel(u0 + 0.01, u1 - 0.01, z0 + 0.02, z0 + h - 0.02, 0.05, M["steel"], out=0.0, bevel=0.012)
            for zz in (0.08, h - 0.08):  # заклёпки по краям листа
                for k in range(int((u1 - u0) / 0.1)):
                    K.sphere(0.014, M["rail"], f.point(u0 + 0.05 + k * 0.1, z0 + zz, 0.055))
        for k in range(1, max(2, round(f.L / 0.62))):  # бойницы
            uc = k * f.L / max(2, round(f.L / 0.62))
            f.panel(uc - 0.11, uc + 0.11, z0 + h * 0.58, z0 + h * 0.66, 0.02, M["dark"], out=0.05, bevel=0)
    # крыша из стальных листов, мешки с песком по краю, картечница на поворотной площадке
    for i in range(5):
        x = xc - CAR_L / 2 + (i + 0.5) * CAR_L / 5
        K.box((CAR_L / 5 - 0.02, CAR_W, 0.06), M["steel"], loc=(x, 0, z0 + h + 0.03), bevel=0.012)
    for i in range(8):
        x = xc - CAR_L / 2 + 0.22 + i * (CAR_L - 0.44) / 7
        for y in (-CAR_W / 2 + 0.12, CAR_W / 2 - 0.12):
            if abs(x - (xc + 0.4)) < 0.45:
                continue
            K.box((0.32, 0.18, 0.11), M["sandbag"], loc=(x, y, z0 + h + 0.12), rot=(0, 0, 0.08 * ((i % 3) - 1)), bevel=0.05)
    gx, gz = xc + 0.4, z0 + h
    K.cylinder(0.34, 0.08, M["iron"], loc=(gx, 0, gz + 0.1), verts=24)
    K.box((0.08, 0.08, 0.32), M["iron"], loc=(gx, 0, gz + 0.3))
    K.box((0.5, 0.04, 0.32), M["steel"], loc=(gx, -0.2, gz + 0.38), bevel=0.01)  # щиток
    d = (math.sin(math.radians(20)), -math.cos(math.radians(20)))  # куда смотрит ствол: к зрителю, чуть вправо
    rot = (math.pi / 2, 0, math.radians(20))
    K.cylinder(0.09, 0.42, M["steel"], loc=(gx, 0.02, gz + 0.5), rot=rot, verts=16)
    for k in range(6):  # блок стволов
        a = k * math.pi / 3
        off = 0.045
        K.cylinder(0.02, 0.8, M["rail"], loc=(gx + d[0] * 0.45 + off * math.cos(a) * d[1], 0.02 + d[1] * 0.45 - off * math.cos(a) * d[0] * 0,
                                               gz + 0.5 + off * math.sin(a)), rot=rot, verts=8, bevel=0)
    K.cylinder(0.075, 0.05, M["iron"], loc=(gx + d[0] * 0.8, 0.02 + d[1] * 0.8, gz + 0.5), rot=rot, verts=14, bevel=0)
    K.cylinder(0.06, 0.14, M["gold"], loc=(gx - d[0] * 0.28, 0.02 - d[1] * 0.28, gz + 0.5), rot=rot, verts=12)


def tank(M, xc):
    """Цистерна для воды: деревянная бочка из клёпок на ложементах, обручи, люк, лестница."""
    r, length = 0.52, CAR_L - 0.25
    K.box((CAR_L, CAR_W - 0.05, 0.06), M["plank"], loc=(xc, 0, DECK + 0.03), bevel=0.012)
    for dx in (-1.0, 0.0, 1.0):
        K.box((0.14, CAR_W - 0.15, 0.22), M["timber"], loc=(xc + dx, 0, DECK + 0.14), bevel=0.02)
    zc = DECK + 0.08 + r
    staves = 22
    for i in range(staves):
        a = i * 2 * math.pi / staves
        K.box((length, 2 * r * math.sin(math.pi / staves) * 1.02, 0.05), M["plank"],
              loc=(xc, math.cos(a) * (r - 0.02), zc + math.sin(a) * (r - 0.02)), rot=(a + math.pi / 2, 0, 0), bevel=0.006)
    K.cylinder(r - 0.03, length - 0.02, M["timber"], loc=(xc, 0, zc), rot=(0, math.pi / 2, 0), verts=32, bevel=0)
    for k in range(5):
        x = xc - length / 2 + 0.15 + k * (length - 0.3) / 4
        K.cylinder(r + 0.02, 0.045, M["iron"], loc=(x, 0, zc), rot=(0, math.pi / 2, 0), verts=32, bevel=0)
    K.cylinder(0.15, 0.14, M["iron"], loc=(xc, 0, zc + r + 0.04), verts=20)
    K.cylinder(0.17, 0.03, M["rail"], loc=(xc, 0, zc + r + 0.12), verts=20, bevel=0)
    K.cylinder(0.04, 0.3, M["iron"], loc=(xc + length / 2 - 0.1, -r - 0.05, zc - 0.25), verts=10)  # кран слива


def build():
    M = materials()
    pitch = CAR_L + GAP + 0.24
    track(M, 3 * pitch + 1.6)
    for xc, kind in ((-pitch, boxcar), (0.0, armored), (pitch, tank)):
        chassis(M, xc)
        kind(M, xc)
    for i in range(2):  # сцепки между вагонами
        x = -pitch / 2 + i * pitch
        K.box((GAP + 0.3, 0.06, 0.05), M["iron"], loc=(x, 0, DECK - 0.14), bevel=0.01)
    return 3 * pitch + 1.6, 2.4


def main():
    opts = R.args()
    sc = R.reset_scene(int(opts["samples"]))
    length, height = build()
    K.light_rig(sc)
    K.outlines(sc)
    tiles = math.ceil(length)
    cam, pivot = R.setup_camera(tiles, height)
    out_dir = os.path.join(opts["out"], "buildings")
    os.makedirs(out_dir, exist_ok=True)
    path = os.path.join(out_dir, "decor_train.png")
    sc.render.filepath = path
    bpy.ops.render.render(write_still=True)
    R.clean_alpha(path)
    K.grade(path)
    with open(path[:-4] + ".json", "w") as f:
        json.dump({"size": tiles, "pivot": [round(pivot[0], 1), round(pivot[1], 1)], "tilePx": R.TILE_PX,
                   "w": sc.render.resolution_x, "h": sc.render.resolution_y, "source": "blender:model_wagon"}, f)
    print(f"[train] {path} {tiles} tiles")


if __name__ == "__main__":
    main()
