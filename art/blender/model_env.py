"""Природа вокруг острога из CC0-сканов Poly Haven: сосны и ели, молодняк, кусты, валуны, пни, папоротник, валежник.
Те же камера и свет, что у построек; каждый спрайт — один объект (или маленькая группа) на клетке.

Запуск: blender -b -P art/blender/model_env.py -- --out art/out --samples 48 [--only tree1,rock1]
Результат: <out>/buildings/env_<имя>.png/.json (в атласе — b/env_<имя>).
"""
import json
import math
import os
import random
import sys

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_hq as HQ  # noqa: E402
import model_kit as K  # noqa: E402
import render_sprites as R  # noqa: E402
import toon_kit as T  # noqa: E402
import scan_kit as S  # noqa: E402

TILE_M = 2.5


def real(m):
    """Метры -> клетки."""
    return m / TILE_M


def fit(o, height_m):
    """Подогнать скан к реальной высоте (в метрах) и поставить на землю."""
    from mathutils import Vector
    bpy.context.view_layer.update()
    s = real(height_m) / max(o.dimensions.z, 1e-3)
    o.scale = tuple(v * s for v in o.scale)
    bpy.context.view_layer.update()
    o.location.z -= min((o.matrix_world @ Vector(c)).z for c in o.bound_box)
    return real(height_m)


def tree(asset, part, height_m, rot=0.0):
    o = S.place(asset, part, (0, 0, 0), scale=1.0, rot_z=rot)
    bpy.context.view_layer.update()
    h = o.dimensions.z
    s = real(height_m) / max(h, 1e-3)
    o.scale = (s, s, s)
    bpy.context.view_layer.update()
    zmin = min((o.matrix_world @ __import__("mathutils").Vector(c)).z for c in o.bound_box)
    o.location.z -= zmin
    return real(height_m)


def ground_bits(rnd, r, n=8):
    for k in range(n):
        a = rnd.uniform(0, 6.28)
        d = rnd.uniform(0.1, r)
        S.place("grass_medium_01", HQ.GRASS[k % len(HQ.GRASS)], (math.cos(a) * d, math.sin(a) * d, 0.0), scale=rnd.uniform(1.0, 1.5),
                rot_z=rnd.uniform(0, 6.28))


def make(name, rnd):
    """Возвращает (размер в клетках, высота в клетках)."""
    if name.startswith("pine"):
        i = int(name[-1])
        h = tree("pine_tree_01", "pine_tree_01_" + "abc"[i - 1], (10, 12, 8.5)[i - 1], rot=i)
        ground_bits(rnd, 0.5, 6)
        return 2, h
    if name.startswith("fir"):
        i = int(name[-1])
        h = tree("fir_tree_01", "fir_tree_01_" + "abc"[i - 1], (8, 9.5, 6.5)[i - 1], rot=i * 2)
        ground_bits(rnd, 0.5, 6)
        return 2, h
    if name.startswith("sapling"):
        i = int(name[-1])
        h = tree("pine_sapling_small" if i == 1 else "pine_sapling_medium", "pine_sapling", 1.4 if i == 1 else 2.6, rot=i)
        ground_bits(rnd, 0.3, 4)
        return 1, h
    if name.startswith("bush"):
        i = int(name[-1])
        asset, part, sc = (("shrub_01", "shrub_01_a", 0.32), ("shrub_02", "shrub_02_a", 0.4), ("shrub_03", "shrub_03_b", 0.45),
                           ("shrub_02", "shrub_02_c", 0.4))[i - 1]
        fit(S.place(asset, part, (0, 0, 0), scale=sc, rot_z=i), (1.4, 1.1, 1.7, 1.2)[i - 1])
        ground_bits(rnd, 0.35, 4)
        return 1, 0.7
    if name.startswith("rock"):
        i = int(name[-1])
        if i == 1:
            fit(S.place("boulder_01", "boulder_01", (0, 0, 0), scale=0.28, rot_z=0.5), 1.6)
            ground_bits(rnd, 0.7, 8)
            return 2, 0.8
        if i == 2:
            fit(S.place("rock_07", "rock_07", (0, 0, 0), scale=0.22, rot_z=1.3), 1.2)
            ground_bits(rnd, 0.6, 6)
            return 2, 0.6
        fit(S.place("rock_moss_set_01", "rock_moss_set_01_rock0" + str(i - 1), (0, 0, 0), scale=0.3, rot_z=i), 0.6)
        ground_bits(rnd, 0.35, 4)
        return 1, 0.4
    if name == "stump":
        fit(S.place("tree_stump_01", "tree_stump_01", (0, 0, 0), scale=0.3, rot_z=0.7), 0.7)
        ground_bits(rnd, 0.35, 4)
        return 1, 0.4
    if name == "deadlog":
        fit(S.place("dead_tree_trunk_02", "dead_tree_trunk_02", (0, 0, 0), scale=0.4, rot_z=0.6), 0.7)
        ground_bits(rnd, 0.6, 6)
        return 2, 0.4
    if name == "fern":
        for k in range(3):
            a = k * 2.1
            S.place("fern_02", "fern_02_" + "abcd"[k], (math.cos(a) * 0.12, math.sin(a) * 0.12, 0), scale=0.7, rot_z=a)
        return 1, 0.4
    if name == "nettle":
        for k, part in enumerate(("nettle_plant_tall_a", "nettle_plant_medium_a", "nettle_plant_small_b")):
            a = k * 2.1
            fit(S.place("nettle_plant", part, (math.cos(a) * 0.12, math.sin(a) * 0.12, 0), scale=0.5, rot_z=a), (1.1, 0.8, 0.5)[k])
        return 1, 0.45
    if name == "branches":
        S.place("dry_branches_medium_01", "dry_branches_medium_01", (0, 0, 0), scale=0.4, rot_z=0.3)
        return 1, 0.3
    if name == "roots":
        fit(S.place("root_cluster_02", "root_cluster_02", (0, 0, 0), scale=0.35, rot_z=1.0), 0.9)
        ground_bits(rnd, 0.4, 5)
        return 1, 0.4
    raise SystemExit("нет такого: " + name)


NAMES = ["pine1", "pine2", "pine3", "fir1", "fir2", "fir3", "sapling1", "sapling2", "bush1", "bush2", "bush3", "bush4", "rock1", "rock2",
         "rock3", "rock4", "rock5", "stump", "deadlog", "fern", "nettle", "branches", "roots"]


def main():
    opts = R.args()
    only = opts.get("only") or set()
    out_dir = os.path.join(opts["out"], "buildings")
    os.makedirs(out_dir, exist_ok=True)
    for name in NAMES:
        if only and name not in only:
            continue
        sc = R.reset_scene(int(opts["samples"]))
        S.reset()
        K.palette()
        size, height = make(name, random.Random(name))
        K.light_rig(sc)
        sc.view_settings.exposure = T.exposure(0.5)
        cam, pivot = R.setup_camera(size, height, extra_w=int(height * R.PPU * 0.6))
        path = os.path.join(out_dir, f"env_{name}.png")
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        R.clean_alpha(path)
        K.grade(path)
        with open(path[:-4] + ".json", "w") as f:
            json.dump({"size": size, "pivot": [round(pivot[0], 1), round(pivot[1], 1)], "tilePx": R.TILE_PX,
                       "w": sc.render.resolution_x, "h": sc.render.resolution_y, "source": "blender:model_env"}, f)
        print(f"[env] {name} -> {path}")


if __name__ == "__main__":
    main()
