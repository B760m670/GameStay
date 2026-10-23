"""Главное здание, собранное из частей (model_kit). Уровень 0 — руины, с которых начинается игра: старый каменный
фундамент, уцелевший угол стены, обвалившиеся балки, всё поросло мхом, травой и молодой берёзой. Уровни 1–5 — после
выбора названия и облика здания.

Запуск: blender -b -P art/blender/model_hq.py -- --out art/out --levels 0 --samples 64
Результат: <out>/buildings/lantern_l<уровень>.png/.json (id главного здания в игре — lantern).
"""
import json
import math
import os
import random
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_kit as K  # noqa: E402
import scan_kit as S  # noqa: E402
import render_sprites as R  # noqa: E402

SIZE = 4  # клеток


def materials():
    M = K.palette()
    M.update({
        "ruin_stone": K.material("ruin_stone", [(138, 132, 122), (118, 114, 108), (150, 142, 128), (106, 102, 98)], rough=0.9, scale=9,
                                 bump=0.6, moss=0.9),
        "old_wood": K.material("old_wood", [(112, 96, 80), (96, 82, 68), (124, 106, 88)], rough=0.9, kind="wave", scale=3, bump=0.5,
                               moss=0.7),
        "bark": K.material("bark", [(226, 222, 210), (214, 208, 196)], rough=0.8, scale=14, bump=0.3),
        "fern": K.material("fern", [(70, 128, 52), (60, 112, 46), (84, 144, 58)], rough=0.8, scale=18, bump=0.2),
        "ivy": K.material("ivy", [(52, 104, 46), (44, 92, 40), (62, 118, 52)], rough=0.7, scale=20, bump=0.2),
        "earth": K.material("earth", [(112, 92, 64), (100, 84, 58), (122, 102, 70)], rough=1.0, scale=7, bump=0.5, moss=1.0),
        "moss_pad": K.material("moss_pad", [(98, 140, 54), (86, 126, 48)], rough=0.95, scale=24, bump=0.4),
    })
    return M


def ruined_wall(face, M, seed, profile, row_h=0.2, gap=0.12, depth=0.24):
    """Стена из камней, разрушенная по профилю: profile(u) — сколько рядов уцелело в точке u; часть камней выпала."""
    rnd = random.Random(seed)
    u = -0.05
    r_off = 0
    while u < face.L + 0.05:
        w = rnd.uniform(0.2, 0.36)
        u1 = min(u + w, face.L + 0.05)
        rows = max(0, int(round(profile((u + u1) / 2) + rnd.uniform(-0.6, 0.6))))
        for r in range(rows):
            if r > 0 and rnd.random() < gap:
                continue  # выпавший камень
            sh = rnd.uniform(-0.05, 0.05) if r % 2 else 0
            hh = row_h * rnd.uniform(0.82, 1.0)
            uc, zc = (u + u1) / 2 + sh, r * row_h + hh / 2 + 0.01
            m = face.matrix(uc, zc, rnd.uniform(-0.025, 0.025), rnd.uniform(-0.06, 0.06))  # камни лежат не идеально ровно
            K.box((u1 - u - 0.03, depth + rnd.uniform(-0.04, 0.04), hh - 0.02), M["ruin_stone"], matrix=m, bevel=0.035)
        u = u1
        r_off += 1


def loose_stone(M, x, y, s, seed):
    rnd = random.Random(seed)
    K.box((s * rnd.uniform(0.9, 1.4), s * rnd.uniform(0.8, 1.2), s * rnd.uniform(0.5, 0.8)), M["ruin_stone"], loc=(x, y, s * 0.28),
          rot=(rnd.uniform(-0.2, 0.2), rnd.uniform(-0.2, 0.2), rnd.uniform(0, 3.14)), bevel=min(0.04, s * 0.3))


def fern(M, x, y, seed, size=0.3):
    """Папоротник: веер вытянутых листьев."""
    rnd = random.Random(seed)
    for i in range(7):
        a = i * 2 * math.pi / 7 + rnd.uniform(-0.2, 0.2)
        L = size * rnd.uniform(0.8, 1.2)
        K.box((L, 0.06, 0.012), M["fern"], loc=(x + math.cos(a) * L * 0.45, y + math.sin(a) * L * 0.45, 0.06 + L * 0.2),
              rot=(0, -0.5, a), bevel=0.004)


def birch(M, x, y, h=2.0, seed=3):
    """Молодая берёза, проросшая сквозь руины: тонкий белый ствол и рыхлая крона."""
    rnd = random.Random(seed)
    K.cylinder(0.055, h, M["bark"], loc=(x, y, h / 2), radius2=0.035, verts=12, bevel=0, rot=(0.04, -0.05, 0))
    for k in range(6):  # тёмные полоски на коре
        K.cylinder(0.058 - k * 0.003, 0.02, M["iron"], loc=(x + 0.002 * k, y, 0.25 + k * 0.28), verts=12, bevel=0)
    for i in range(70):  # рыхлая крона из мелких листовых пучков
        a = rnd.uniform(0, 2 * math.pi)
        r = rnd.uniform(0.0, 0.5) * (1.2 - rnd.random() * 0.4)
        z = h * rnd.uniform(0.55, 1.12)
        K.sphere(rnd.uniform(0.05, 0.09), M["leaf"], (x + math.cos(a) * r, y + math.sin(a) * r, z), segments=8, rings=6, scale=(1, 1, 0.7))
    for k in range(4):  # ветки
        a = k * 1.6 + 0.4
        K.cylinder(0.014, 0.5, M["bark"], loc=(x + math.cos(a) * 0.18, y + math.sin(a) * 0.18, h * 0.78), rot=(math.sin(a) * 0.9, -math.cos(a) * 0.9, 0),
                   verts=6, bevel=0)


def ivy(M, face, u0, u1, z0, z1, seed, n=26):
    rnd = random.Random(seed)
    for _ in range(n):
        u = rnd.uniform(u0, u1)
        z = rnd.uniform(z0, z1) * rnd.uniform(0.6, 1.0)
        K.sphere(rnd.uniform(0.04, 0.07), M["ivy"], face.point(u, z, 0.16 + rnd.uniform(-0.01, 0.03)), scale=(1, 0.5, 1))


def ruins(M):
    """Уровень 0: руины из реалистичных деталей (scan_kit, CC0-сканы Poly Haven)."""
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
    rocks = ["rock_moss_set_02_rock07", "rock_moss_set_02_rock08", "rock_moss_set_02_rock09", "rock_moss_set_02_rock10",
             "rock_moss_set_02_rock11", "rock_moss_set_02_rock12", "rock_moss_set_02_rock13"]
    spots = [(x0 + 0.3, y0 - 0.25), (x0 + 1.2, y0 - 0.3), (x1 + 0.3, y0 + 0.4), (x1 + 0.28, y0 + 1.3), (x0 - 0.3, y0 + 0.5),
             (x0 + 0.5, y0 + 0.5), (x1 - 0.6, y1 - 0.5), (x0 + 1.6, y0 + 0.15), (x1 - 0.3, y0 + 0.2), (x0 + 0.3, y1 - 0.6)]
    for i, (x, y) in enumerate(spots):
        S.place("rock_moss_set_02", rocks[i % len(rocks)], (x, y, 0.0), scale=rnd.uniform(0.16, 0.26), rot_z=rnd.uniform(0, 6.28))
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
    grass = ["grass_medium_01_large_a", "grass_medium_01_large_b", "grass_medium_01_mid_a", "grass_medium_01_tall_a",
             "grass_medium_01_mid_b", "grass_medium_01_large_c"]
    for k in range(26):  # трава на полу руин
        S.place("grass_medium_01", grass[k % len(grass)], (rnd.uniform(x0 + 0.25, x1 - 0.25), rnd.uniform(y0 + 0.25, y1 - 0.25), 0.04),
                scale=rnd.uniform(1.4, 2.0), rot_z=rnd.uniform(0, 6.28))
    for k in range(34):  # трава у подножия стен
        side = k % 4
        t = rnd.uniform(0, 1)
        x = (x0 + (x1 - x0) * t) if side < 2 else (x0 if side == 2 else x1)
        y = (y0 if side == 0 else y1) if side < 2 else (y0 + (y1 - y0) * t)
        x += rnd.uniform(-0.25, 0.25)
        y += rnd.uniform(-0.25, 0.25)
        S.place("grass_medium_01", grass[k % len(grass)], (x, y, 0.0), scale=rnd.uniform(1.5, 2.2), rot_z=rnd.uniform(0, 6.28))
    return 2.2


def main():
    opts = R.args()
    levels = [int(v) for v in str(opts.get("levels", "0")).split(",")]
    for level in levels:
        sc = R.reset_scene(int(opts["samples"]))
        M = materials()
        S.reset()
        if level == 0:
            height = ruins(M)
        else:
            raise SystemExit("уровни 1–5 появятся после выбора облика главного здания")
        K.light_rig(sc)
        if level > 0:
            K.outlines(sc)  # у реалистичных руин контура нет
        else:
            sc.view_settings.exposure = 0.45  # фототекстуры темнее процедурных цветов
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
