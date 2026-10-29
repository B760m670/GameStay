"""Природа вокруг острога в рисованном стиле: ели ярусами, сосны с комками хвои, кусты, гранёные валуны, пни, папоротник.
Собрана из простых тел (без сканов); трава у подножия — CC0-скан Poly Haven.
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
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_hq as HQ  # noqa: E402
import model_kit as K  # noqa: E402
import render_sprites as R  # noqa: E402
import toon_kit as T  # noqa: E402
import outfit_kit as O  # noqa: E402
import scan_kit as S  # noqa: E402

TILE_M = 2.5


def real(m):
    """Метры -> клетки."""
    return m / TILE_M


# ---------- рисованная природа: собрана из простых тел, как в стратегиях вроде Clash of Clans ----------
def _obj(bm, name, mat, smooth=True):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for f in me.polygons:
        f.use_smooth = smooth
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat)
    return o


def blob(name, loc, r, mat, rnd, squash=1.0, rough=0.22, subdiv=2, smooth=True):
    """Комок листвы или камень: икосфера с неровной поверхностью."""
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=r)
    seed = rnd.uniform(0, 100)
    for v in bm.verts:
        d = v.co.normalized()
        k = 1 + rough * (math.sin(d.x * 3.1 + seed) * math.sin(d.y * 2.7 + seed * 1.3) * math.cos(d.z * 3.3 + seed * 0.7)
                         + rnd.uniform(-0.35, 0.35))
        v.co = Vector((v.co.x * k, v.co.y * k, v.co.z * k * squash))
    bmesh.ops.translate(bm, verts=bm.verts, vec=Vector(loc))
    return _obj(bm, name, mat, smooth)


def tier(name, z0, r, h, mat, rnd, points=9, droop=0.25):
    """Ярус еловой лапы: конус со звёздчатым зубчатым краем, края слегка опущены."""
    import bmesh
    bm = bmesh.new()
    top = bm.verts.new((0, 0, z0 + h))
    ring, inner = [], []
    off = rnd.uniform(0, 6.28)
    for i in range(points * 2):
        a = off + i * math.pi / points
        rr = r * (1.0 if i % 2 == 0 else 0.72) * rnd.uniform(0.92, 1.06)
        ring.append(bm.verts.new((math.cos(a) * rr, math.sin(a) * rr, z0 - (droop * h if i % 2 == 0 else 0.0))))
        inner.append(bm.verts.new((math.cos(a) * rr * 0.55, math.sin(a) * rr * 0.55, z0 + h * 0.12)))
    n = len(ring)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((ring[i], ring[j], top))
        bm.faces.new((ring[j], ring[i], inner[i], inner[j]))
    bm.faces.new(inner[::-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return _obj(bm, name, mat, smooth=False)


def trunk(h, r, mat, rnd, lean=0.0):
    return S.log((0, 0, -0.02), (lean, 0, h), r, mat, rnd.randrange(1 << 30), r_end=r * 0.55, sag=0.0, name="trunk")


MAT = {}


def mats():
    MAT.clear()
    MAT["fir"] = T.solid("toon_fir", [(54, 112, 62), (46, 100, 56), (62, 124, 68)], rough=0.8, noise=4, bump=0.2)
    MAT["pine"] = T.solid("toon_pine", [(86, 140, 58), (74, 128, 52), (96, 150, 62)], rough=0.8, noise=4, bump=0.2)
    MAT["bush"] = T.solid("toon_bush", [(96, 156, 60), (84, 144, 54), (108, 166, 66)], rough=0.8, noise=5, bump=0.2)
    MAT["leaf"] = T.solid("toon_leaf", [(112, 172, 64), (100, 160, 58)], rough=0.8, noise=3, bump=0.1)
    MAT["bark"] = T.solid("toon_bark", [(112, 78, 52), (100, 70, 48), (122, 86, 56)], rough=0.85, noise=10, bump=0.4)
    MAT["pinebark"] = T.solid("toon_pinebark", [(176, 106, 62), (160, 96, 56)], rough=0.8, noise=10, bump=0.4)
    MAT["stone"] = T.solid("toon_rock", [(122, 126, 136), (108, 112, 122), (134, 136, 144)], rough=0.75, noise=6, bump=0.2, moss=0.35)
    MAT["berry"] = O.plain("toon_berry", lin((200, 40, 40)), rough=0.4)
    MAT["cut"] = S.end_grain_mat()


def lin(c):
    return tuple((v / 255) ** 2.2 for v in c)


def fir(height_m, rnd):
    """Ель: короткий ствол и 5–6 зубчатых ярусов, каждый уже и короче нижнего."""
    H = real(height_m)
    trunk(H * 0.3, H * 0.04, MAT["bark"], rnd)
    n = 6 if height_m > 7 else 5
    z = H * 0.12
    for i in range(n):
        k = 1 - i / n
        r = H * (0.13 + 0.17 * k)
        h = H * (0.2 + 0.05 * (1 - k))
        tier("fir_tier", z, r, h, MAT["fir"], rnd, droop=0.3)
        z += h * 0.62
    return H


def pine(height_m, rnd):
    """Сосна: высокий рыжий ствол, наверху — плоские комки хвои на коротких сучьях."""
    H = real(height_m)
    lean = rnd.uniform(-0.04, 0.04)
    trunk(H * 0.8, H * 0.05, MAT["pinebark"], rnd, lean=lean)
    for i in range(7):  # крона из плотных комков хвои, ниже — шире
        a = i * 2.4 + rnd.uniform(0, 0.5)
        z = H * (0.5 + 0.055 * i)
        d = H * (0.16 - 0.018 * i)
        c = Vector((lean * z / H + math.cos(a) * d, math.sin(a) * d, z))
        S.log((lean * z / H, 0, z - 0.05), c, H * 0.016, MAT["pinebark"], rnd.randrange(1 << 30), sag=0.0)
        blob("pine_crown", c + Vector((0, 0, H * 0.02)), H * (0.16 - 0.012 * i), MAT["pine"], rnd, squash=0.6)
    blob("pine_top", (lean * 0.9, 0, H * 0.9), H * 0.12, MAT["pine"], rnd, squash=0.75)
    return H


def bush(i, rnd):
    """Куст: несколько комков листвы; у второго — ягоды."""
    r = real((0.6, 0.5, 0.75, 0.55)[i - 1])
    n = (5, 4, 6, 4)[i - 1]
    top = 0
    for k in range(n):
        a = k * 2 * math.pi / n + rnd.uniform(0, 0.4)
        d = r * (0.55 if k else 0.0)
        rr = r * rnd.uniform(0.62, 0.8)
        z = rr * 0.7 + (r * 0.35 if k == 0 else 0)
        blob("bush", (math.cos(a) * d, math.sin(a) * d, z), rr, MAT["bush"], rnd, squash=0.85)
        top = max(top, z + rr)
    if i == 2:  # брусника
        for k in range(14):
            a, d = rnd.uniform(0, 6.28), rnd.uniform(0.3, 1.0) * r
            K.sphere(r * 0.07, MAT["berry"], (math.cos(a) * d, math.sin(a) * d, rnd.uniform(0.4, 1.1) * r))
    return top


def rock(i, rnd):
    """Валун: гранёный (как в рисованных стратегиях), сверху мох; мелкие камни — россыпью."""
    if i <= 2:
        r = real((1.0, 0.75)[i - 1])
        blob("rock", (0, 0, r * 0.45), r, MAT["stone"], rnd, squash=0.6, rough=0.18, subdiv=1, smooth=False)
        blob("rock", (r * 0.9, -r * 0.4, r * 0.2), r * 0.4, MAT["stone"], rnd, squash=0.6, rough=0.2, subdiv=1, smooth=False)
        return r * 1.1
    r = real(0.35)
    for k in range(i - 1):
        a = k * 2.3
        blob("rock", (math.cos(a) * r * 0.9 * (k > 0), math.sin(a) * r * 0.9 * (k > 0), r * 0.3), r * rnd.uniform(0.6, 1.0), MAT["stone"], rnd,
             squash=0.6, rough=0.2, subdiv=1, smooth=False)
    return r * 0.9


def stump(rnd, roots=False):
    """Пень со срезом; у корневища — расползшиеся корни."""
    r, h = real(0.3), real(0.45)
    S.log((0, 0, -0.02), (0, 0, h), r, MAT["bark"], rnd.randrange(1 << 30), sag=0.0, ends=False)
    K.cylinder(r * 0.96, 0.01, MAT["cut"], loc=(0, 0, h))
    for k in range(5 if roots else 3):
        a = k * 2 * math.pi / (5 if roots else 3) + rnd.uniform(0, 0.4)
        L = r * (2.6 if roots else 1.5)
        S.log((math.cos(a) * r * 0.6, math.sin(a) * r * 0.6, h * 0.35), (math.cos(a) * L, math.sin(a) * L, -0.01), r * 0.28, MAT["bark"],
              rnd.randrange(1 << 30), r_end=r * 0.08, sag=0.0)
    return h * 1.2


def deadlog(rnd):
    r = real(0.28)
    S.log((-0.7, 0.1, r * 0.9), (0.7, -0.1, r * 0.8), r, MAT["bark"], rnd.randrange(1 << 30), r_end=r * 0.8)
    for x in (-0.3, 0.25):
        S.log((x, 0.0, r * 1.5), (x + 0.1, -0.15, r * 2.6), r * 0.25, MAT["bark"], rnd.randrange(1 << 30), r_end=r * 0.1, sag=0.0)
    blob("moss", (0.4, -0.05, r * 1.6), r * 0.45, MAT["bush"], rnd, squash=0.4)
    return r * 3


def fronds(n, length, rnd, mat, upright=0.5):
    """Листья папоротника или крапивы: изогнутые плоские перья по кругу."""
    for k in range(n):
        a = k * 2 * math.pi / n + rnd.uniform(-0.2, 0.2)
        pts = []
        for t in range(9):
            u = t / 8
            pts.append((u * length, math.sin(u * math.pi) * length * 0.16 + 0.0))
        pts += [(u * length, -math.sin(u * math.pi) * length * 0.16) for u in [t / 8 for t in range(8, -1, -1)]]
        m = Matrix.Rotation(a, 4, "Z") @ Matrix.Rotation(-upright, 4, "Y")
        O._prism_obj("frond", [(x, y) for x, y in pts], 0.01, mat, Matrix.Translation((0, 0, 0.01)) @ m @ Matrix.Rotation(math.pi / 2, 4, "X")
                     @ Matrix.Rotation(math.pi / 2, 4, "X"))


def ground_bits(rnd, r, n=8):
    for k in range(n):
        a = rnd.uniform(0, 6.28)
        d = rnd.uniform(0.1, r)
        S.place("grass_medium_01", HQ.GRASS[k % len(HQ.GRASS)], (math.cos(a) * d, math.sin(a) * d, 0.0), scale=rnd.uniform(1.0, 1.5),
                rot_z=rnd.uniform(0, 6.28))


SMALL = 1.8  # кусты, камни, пни и травы крупнее «по линейке», как убранство поляны


def make(name, rnd):
    """Возвращает (размер в клетках, высота в клетках)."""
    mats()
    before = set(bpy.data.objects)
    size, h = _make(name, rnd)
    if not name.startswith(("pine", "fir")):
        for o in set(bpy.data.objects) - before:
            if o.type == "MESH" and o.name != "ShadowCatcher":
                o.matrix_world = Matrix.Diagonal((SMALL, SMALL, SMALL, 1)) @ o.matrix_world
        h *= SMALL
    return size, h


def _make(name, rnd):
    if name.startswith("pine"):
        h = pine((10, 12, 8.5)[int(name[-1]) - 1], rnd)
        ground_bits(rnd, 0.5, 6)
        return 2, h
    if name.startswith("fir"):
        h = fir((8, 9.5, 6.5)[int(name[-1]) - 1], rnd)
        ground_bits(rnd, 0.5, 6)
        return 2, h
    if name.startswith("sapling"):
        h = fir(1.6 if name[-1] == "1" else 2.8, rnd)
        ground_bits(rnd, 0.3, 4)
        return 1, h
    if name.startswith("bush"):
        h = bush(int(name[-1]), rnd)
        ground_bits(rnd, 0.35, 4)
        return 1, h
    if name.startswith("rock"):
        i = int(name[-1])
        h = rock(i, rnd)
        ground_bits(rnd, 0.6 if i <= 2 else 0.35, 6 if i <= 2 else 3)
        return (2 if i <= 2 else 1), h
    if name == "stump":
        return 1, stump(rnd)
    if name == "roots":
        return 1, stump(rnd, roots=True)
    if name == "deadlog":
        h = deadlog(rnd)
        ground_bits(rnd, 0.6, 6)
        return 2, h
    if name == "fern":
        fronds(7, real(0.7), rnd, MAT["leaf"], upright=0.6)
        return 1, real(0.5)
    if name == "nettle":
        for k in range(3):
            a = k * 2.1
            S.log((math.cos(a) * 0.08, math.sin(a) * 0.08, 0), (math.cos(a) * 0.1, math.sin(a) * 0.1, real(0.9 - 0.15 * k)), 0.008, MAT["leaf"],
                  rnd.randrange(1 << 30), sag=0.0)
            for j in range(3):
                blob("nettle_leaf", (math.cos(a) * 0.1, math.sin(a) * 0.1, real(0.35 + 0.18 * j)), real(0.12), MAT["leaf"], rnd, squash=0.35)
        return 1, real(0.9)
    if name == "branches":
        for k in range(4):
            a = k * 0.8 + rnd.uniform(0, 0.3)
            S.log((math.cos(a) * -0.3, math.sin(a) * -0.3, 0.02), (math.cos(a) * 0.3, math.sin(a) * 0.3, 0.03), 0.02, MAT["bark"],
                  rnd.randrange(1 << 30), r_end=0.008, sag=0.0)
        return 1, 0.15
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
