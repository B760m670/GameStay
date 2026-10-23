"""Казарма, собранная из частей (model_kit). Уровни: 1 — фахверковый дом; 2 — каменный первый этаж, знамёна, ящики;
3 — угловая башня с шатровой крышей и флагом, золочёные навершия.

Запуск: blender -b -P art/blender/model_barracks.py -- --out art/out --levels 1,2,3 --samples 64
"""
import math
import os
import random
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_kit as K  # noqa: E402
import render_sprites as R  # noqa: E402

SIZE = 3  # клеток


def spear(M, base, lean_axis, lean):
    """Копьё: древко и наконечник, наклонено."""
    rot = (lean if lean_axis == "x" else 0, lean if lean_axis == "y" else 0, 0)
    K.cylinder(0.018, 0.95, M["plank"], loc=(base[0], base[1], base[2] + 0.47), rot=rot, verts=10, bevel=0)
    tip_z = base[2] + 0.95 * math.cos(lean)
    off = 0.95 * math.sin(lean)
    tip = (base[0] + (off if lean_axis == "y" else 0), base[1] - (off if lean_axis == "x" else 0), tip_z + 0.05)
    K.cylinder(0.035, 0.12, M["iron"], loc=tip, rot=rot, radius2=0.0, verts=10, bevel=0)


def weapon_rack(M, x, y):
    for dx in (-0.22, 0.22):
        K.box((0.06, 0.06, 0.62), M["timber"], loc=(x + dx, y, 0.31))
    K.box((0.56, 0.07, 0.06), M["timber"], loc=(x, y, 0.5))
    K.box((0.56, 0.07, 0.05), M["timber"], loc=(x, y, 0.12))
    for i, dx in enumerate((-0.15, -0.05, 0.05, 0.15)):
        spear(M, (x + dx, y + 0.05, 0.02), "x", math.radians(-10 + i * 2))
    K.box((0.2, 0.05, 0.24), M["cloth_red"], loc=(x - 0.12, y - 0.045, 0.32), bevel=0.01)  # щит на стойке
    K.cylinder(0.1, 0.03, M["iron"], loc=(x + 0.12, y - 0.05, 0.3), rot=(math.pi / 2, 0, 0), verts=18)


def dummy(M, x, y):
    """Соломенное чучело для тренировок."""
    K.cylinder(0.035, 0.95, M["timber"], loc=(x, y, 0.47), verts=10)
    K.box((0.5, 0.05, 0.05), M["timber"], loc=(x, y, 0.72), rot=(0, 0, math.radians(45)))
    K.cylinder(0.13, 0.36, M["straw"], loc=(x, y, 0.62), radius2=0.11, verts=18)
    for z in (0.5, 0.74):
        K.cylinder(0.135, 0.025, M["sack"], loc=(x, y, z), verts=18, bevel=0)
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.1, location=(x, y, 0.92), segments=16, ring_count=10)
    head = bpy.context.active_object
    head.data.materials.append(M["sack"])
    for p in head.data.polygons:
        p.use_smooth = True


def barrel(M, x, y, h=0.36, r=0.15):
    K.cylinder(r, h, M["plank"], loc=(x, y, h / 2), radius2=r, verts=20, bevel=0.01)
    for z in (0.07, h - 0.07):
        K.cylinder(r + 0.01, 0.025, M["iron"], loc=(x, y, z), verts=20, bevel=0)
    K.cylinder(r - 0.01, 0.01, M["timber"], loc=(x, y, h + 0.003), verts=20, bevel=0)


def crate(M, x, y, s=0.28, rot=0.0):
    K.box((s, s, s), M["plank"], loc=(x, y, s / 2), rot=(0, 0, rot), bevel=0.02)
    for dz in (0.03, s - 0.03):
        K.box((s + 0.012, s + 0.012, 0.04), M["timber"], loc=(x, y, dz), rot=(0, 0, rot), bevel=0.01)


def banner(M, x, y, h=1.5, flag="cloth_red"):
    K.cylinder(0.03, h, M["timber"], loc=(x, y, h / 2), verts=10)
    K.cylinder(0.045, 0.06, M["gold"], loc=(x, y, h + 0.03), radius2=0.0, verts=10, bevel=0)
    K.box((0.04, 0.32, 0.03), M["timber"], loc=(x, y - 0.14, h - 0.08))
    K.box((0.02, 0.28, 0.5), M[flag], loc=(x, y - 0.15, h - 0.36), bevel=0.01)
    K.box((0.025, 0.12, 0.12), M["gold"], loc=(x - 0.005, y - 0.15, h - 0.36), rot=(math.radians(45), 0, 0), bevel=0.01)


def tower(M, x, y, level):
    """Угловая башня (3-й уровень): кладка, бойницы, шатровая крыша из сланца, флаг."""
    r, h = 0.42, 2.5
    K.cylinder(r, h, M["masonry"], loc=(x, y, h / 2), verts=28, bevel=0.01)
    K.cylinder(r + 0.05, 0.14, M["stone"], loc=(x, y, h - 0.07), verts=28, bevel=0.02)
    for a in range(-80, 10, 30):  # бойницы на видимой стороне
        ang = math.radians(a - 45)
        K.box((0.06, 0.04, 0.2), M["glass"], loc=(x + math.cos(ang) * r, y + math.sin(ang) * r, 1.7), rot=(0, 0, ang + math.pi / 2), bevel=0)
    K.cylinder(r + 0.12, 1.1, M["slate"], loc=(x, y, h + 0.55), radius2=0.02, verts=28, bevel=0.01)
    K.cylinder(0.03, 0.55, M["gold"], loc=(x, y, h + 1.3), verts=8, bevel=0)
    K.box((0.02, 0.36, 0.22), M["cloth_red"], loc=(x, y - 0.19, h + 1.42), bevel=0.01)


def build(level):
    M = K.palette()
    rnd = random.Random(7)
    x0, x1, y0, y1 = -1.38, 1.0, -0.72, 1.18
    wx, wy = x1 - x0, y1 - y0
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    zp, zw = 0.24, 1.58            # верх цоколя и верх стен
    rise, over = 0.8, 0.13
    front = K.Face((x0, y0, 0), (1, 0, 0), (0, -1, 0), wx, zw)
    right = K.Face((x1, y0, 0), (0, 1, 0), (1, 0, 0), wy, zw)
    # цоколь: сердцевина и камни по видимым граням
    K.box((wx + 0.08, wy + 0.08, zp), M["stone_dark"], loc=(cx, cy, zp / 2), bevel=0.02)
    stone_top = zp if level == 1 else 0.78   # на 2+ уровне первый этаж каменный
    for i, f in enumerate((front, right)):
        K.stone_course(f, 0.0, stone_top, M, seed=10 + i, rows=2 if level == 1 else 4, out=0.0)
    # стены: штукатурка над камнем
    K.box((wx, wy, zw - stone_top), M["plaster"], loc=(cx, cy, (zw + stone_top) / 2), bevel=0.0)
    if level > 1:
        K.box((wx - 0.02, wy - 0.02, stone_top - zp), M["stone_dark"], loc=(cx, cy, (stone_top + zp) / 2), bevel=0.0)
    # фахверк: угловые стойки, обвязки, раскосы
    for f in (front, right):
        for u in (0.0, f.L):
            f.beam(u, stone_top, u, zw, M["timber"], thick=0.12, depth=0.08)
        f.beam(-0.02, stone_top + 0.05, f.L + 0.02, stone_top + 0.05, M["timber"], thick=0.1, depth=0.08)
        f.beam(-0.02, zw - 0.05, f.L + 0.02, zw - 0.05, M["timber"], thick=0.11, depth=0.08)
    # фасад: окна по бокам, дверь в центре, раскосы под окнами
    door_z = zp
    for uc in (0.42, wx - 0.42):
        K.window(front, uc, 1.08, 0.26, 0.36, M)
        K.flower_box(front, uc, 0.84, 0.36, M, seed=int(uc * 100))
        if level == 1:
            front.beam(uc - 0.3, stone_top + 0.08, uc + 0.3, 0.66, M["timber"], thick=0.07, depth=0.06)
            front.beam(uc + 0.3, stone_top + 0.08, uc - 0.3, 0.66, M["timber"], thick=0.07, depth=0.06)
    front.beam(wx / 2 - 0.36, stone_top, wx / 2 - 0.36, zw, M["timber"], thick=0.09, depth=0.07)
    front.beam(wx / 2 + 0.36, stone_top, wx / 2 + 0.36, zw, M["timber"], thick=0.09, depth=0.07)
    K.door(front, wx / 2, 0.44, 0.8, M, z0=door_z)
    # вывеска казармы над дверью: щит и скрещённые мечи
    front.panel(wx / 2 - 0.2, wx / 2 + 0.2, 1.2, 1.46, 0.04, M["plank"], out=0.02)
    front.panel(wx / 2 - 0.07, wx / 2 + 0.07, 1.23, 1.43, 0.03, M["cloth_red"], out=0.06)
    for sgn in (-1, 1):
        front.beam(wx / 2 - 0.16 * sgn, 1.22, wx / 2 + 0.16 * sgn, 1.44, M["iron"], thick=0.025, depth=0.02, out=0.09)
    K.box((0.66, 0.26, 0.08), M["stone"], loc=(x0 + wx / 2, y0 - 0.14, 0.04), bevel=0.025)  # порог
    K.box((0.56, 0.22, 0.08), M["stone"], loc=(x0 + wx / 2, y0 - 0.12, 0.12), bevel=0.025)
    # торец: окно, раскосы, фронтон
    K.window(right, wy / 2, 1.08, 0.3, 0.38, M)
    K.flower_box(right, wy / 2, 0.84, 0.4, M, seed=77)
    if level == 1:
        right.beam(0.06, stone_top + 0.06, 0.4, zw - 0.08, M["timber"], thick=0.07, depth=0.06)
        right.beam(wy - 0.06, stone_top + 0.06, wy - 0.4, zw - 0.08, M["timber"], thick=0.07, depth=0.06)
    K.gable_wall(right, zw, rise, M)
    K.window(right, wy / 2, zw + rise * 0.38, 0.17, 0.17, M, shutters=False)
    if level >= 2:  # герб на фронтоне
        right.panel(wy / 2 - 0.13, wy / 2 + 0.13, zw + 0.02, zw + 0.2, 0.03, M["cloth_red"], out=0.05)
    # крыша
    pitch, run = K.gable_roof(cx, cy, zw, wx, wy, rise, over, M, seed=3)
    z_low = zw - over * math.tan(pitch)
    K.verge(right, z_low, zw + rise + 0.06, over, M)
    if level >= 3:  # золочёные навершия конька
        for x in (cx - wx / 2 - over, cx + wx / 2 + over):
            K.cylinder(0.06, 0.2, M["gold"], loc=(x, cy, zw + rise + 0.2), radius2=0.0, verts=12, bevel=0)
    # труба на заднем скате
    chx, chy = x0 + 0.45, cy + 0.45
    K.box((0.3, 0.3, 1.25), M["masonry"], loc=(chx, chy, zw + 0.7), bevel=0.02)
    K.box((0.38, 0.38, 0.08), M["stone"], loc=(chx, chy, zw + 1.36), bevel=0.02)
    # двор
    weapon_rack(M, x0 + 0.35, y0 - 0.55)
    dummy(M, x1 + 0.2, y0 - 0.6)
    barrel(M, x1 + 0.28, y1 - 0.25)
    for i in range(3):  # поленница у торца
        for j in range(3 - i):
            K.cylinder(0.055, 0.5, M["plank"], loc=(x1 + 0.12 + 0.11 * j + 0.055 * i, y1 - 0.62, 0.06 + 0.1 * i), rot=(math.pi / 2, 0, 0), verts=12)
    for k, (gx, gy) in enumerate([(x0 + 0.1, y0 - 0.06), (x1 + 0.06, y0 + 0.4), (x0 + 1.3, y0 - 0.07), (x1 + 0.07, y0 + 1.2), (x1 - 0.2, y0 - 0.08)]):
        K.grass_tuft(M, gx, gy, seed=k)
    if level >= 2:
        crate(M, x1 + 0.3, y1 - 0.75, rot=0.3)
        banner(M, x0 - 0.05, y0 - 0.25)
        banner(M, x1 + 0.12, y0 - 0.12)
    if level >= 3:
        tower(M, x1 - 0.05, y1 + 0.02, level)
    return 2.7 if level < 3 else 4.2


def main():
    opts = R.args()
    levels = [int(v) for v in str(opts.get("levels", "1,2,3")).split(",")]
    for level in levels:
        sc = R.reset_scene(int(opts["samples"]))
        height = build(level)
        K.light_rig(sc)
        K.outlines(sc)
        cam, pivot = R.setup_camera(SIZE, height, extra_w=int(height * R.PPU * 0.9))  # запас под тень; пустые поля срежет упаковщик
        out_dir = os.path.join(opts["out"], "buildings")
        os.makedirs(out_dir, exist_ok=True)
        path = os.path.join(out_dir, f"barracks_t{level}.png")
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        R.clean_alpha(path)
        K.grade(path)
        import json
        with open(path[:-4] + ".json", "w") as f:
            json.dump({"size": SIZE, "pivot": [round(pivot[0], 1), round(pivot[1], 1)], "tilePx": R.TILE_PX,
                       "w": sc.render.resolution_x, "h": sc.render.resolution_y, "source": "blender:model_barracks"}, f)
        print(f"[barracks] level {level} -> {path}")


if __name__ == "__main__":
    main()
