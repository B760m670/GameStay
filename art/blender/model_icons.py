"""Иконки интерфейса, собранные из тех же реалистичных деталей, что постройки и войска:
лес (брёвна), хлеб (каравай), серебро (чешуйки-копейки), плотник (топор в чурбаке), летопись (книга), поход (свиток),
войско (щит и топор), острог.

Запуск: blender -b -P art/blender/model_icons.py -- --out art/out --samples 64
Результат: <out>/icons/<имя>.png (256×256, прозрачный фон); pack_atlas.py обводит и кладёт в атлас как icon/<имя>.
"""
import math
import os
import random
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_kit as K  # noqa: E402
import outfit_kit as O  # noqa: E402
import render_sprites as R  # noqa: E402
import toon_kit as T  # noqa: E402
import scan_kit as S  # noqa: E402


def lin(c):
    return tuple((v / 255) ** 2.2 for v in c)


def crust():
    """Корка хлеба: тёплый коричневый с пятнами и бугорками."""
    m = bpy.data.materials.new("crust")
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    b = N["Principled BSDF"]
    b.inputs["Roughness"].default_value = 0.6
    nz = N.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 18
    nz.inputs["Detail"].default_value = 8
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (*lin((120, 60, 22)), 1)
    ramp.color_ramp.elements[1].color = (*lin((214, 150, 70)), 1)
    L.new(nz.outputs["Fac"], ramp.inputs["Fac"])
    L.new(ramp.outputs["Color"], b.inputs["Base Color"])
    bump = N.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.4
    L.new(nz.outputs["Fac"], bump.inputs["Height"])
    L.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


def wood_icon():
    rnd = random.Random(3)
    mat = S.grain_mat("weathered_planks")
    objs = []
    for k, n in enumerate((3, 2, 1)):
        for i in range(n):
            y = (i - (n - 1) / 2) * 0.24
            objs.append(S.log((-0.45, y, 0.12 + k * 0.205), (0.45, y, 0.12 + k * 0.205), 0.12, mat, rnd.randrange(1 << 30), sag=0.0))
    return objs, 25


def bread_icon():
    loaf = O.revolve("loaf", [(0.5, 0.0), (0.52, 0.06), (0.5, 0.2), (0.38, 0.33), (0.2, 0.39), (0.0, 0.4)], crust(), Vector((0, 0, 0)), segs=40)
    cut = O.plain("crumb", lin((240, 214, 160)), rough=0.9)
    objs = [loaf]
    for i in range(3):  # надрезы
        o = O._prism_obj("score", [(-0.25, 0.0), (0.25, 0.0), (0.25, 0.03), (-0.25, 0.03)], 0.04, cut,
                         Matrix.Translation((0, -0.15 + i * 0.15, 0.37 - abs(i - 1) * 0.03)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
        objs.append(o)
    return objs, 30


def silver_icon():
    rnd = random.Random(5)
    silver = O.plain("silver", lin((200, 200, 205)), rough=0.3, metal=1.0)
    objs = []
    for i in range(14):
        a = rnd.uniform(0, 6.28)
        r = rnd.uniform(0.0, 0.32) * (1 - i / 20)
        o = O.revolve("coin", [(0.14, 0.0), (0.145, 0.012), (0.14, 0.025), (0.0, 0.026)], silver, Vector((0, 0, 0)), segs=20,
                      scale=(1.0, rnd.uniform(0.75, 0.9), 1))
        o.matrix_world = Matrix.Translation((math.cos(a) * r, math.sin(a) * r, 0.02 * i * 0.6)) @ Matrix.Rotation(rnd.uniform(-0.25, 0.25), 4, "X") \
            @ Matrix.Rotation(rnd.uniform(0, 6.28), 4, "Z")
        objs.append(o)
    return objs, 20


def builder_icon():
    S.place("tree_stump_01", "tree_stump_01", (0, 0, 0.0), scale=0.35, rot_z=0.4)
    steel = O.plain("steel", lin((160, 160, 160)), rough=0.35, metal=0.9)
    axe = O.axe(S.grain_mat("wood_planks_grey", name="grain_shaft"), steel, length=0.75)
    axe.matrix_world = Matrix.Translation((0.05, 0.0, 0.33)) @ Matrix.Rotation(0.5, 4, "Y") @ Matrix.Rotation(math.pi, 4, "X") @ Matrix.Translation((0, 0, -0.5))
    return [o for o in bpy.context.scene.objects if o.type == "MESH" and not o.hide_render and o.name != "ShadowCatcher"], 40


def codex_icon():
    leather = O.fabric("leather", lin((110, 50, 30)), scale=2, sat=0.6)
    pages = O.plain("pages", lin((236, 222, 190)), rough=0.9)
    cover = O._prism_obj("cover", [(-0.32, -0.42), (0.32, -0.42), (0.32, 0.42), (-0.32, 0.42)], 0.03, leather, Matrix.Rotation(math.pi / 2, 4, "X"))
    block = O._prism_obj("block", [(-0.3, -0.4), (0.29, -0.4), (0.29, 0.4), (-0.3, 0.4)], 0.12, pages,
                         Matrix.Translation((0, 0, 0.075)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
    top = O._prism_obj("cover2", [(-0.32, -0.42), (0.32, -0.42), (0.32, 0.42), (-0.32, 0.42)], 0.03, leather,
                       Matrix.Translation((0, 0, 0.15)) @ Matrix.Rotation(math.pi / 2, 4, "X"))
    gold = O.plain("brass", lin((210, 170, 90)), rough=0.35, metal=1.0)
    clasp = O._prism_obj("clasp", [(-0.05, -0.1), (0.05, -0.1), (0.05, 0.1), (-0.05, 0.1)], 0.02, gold,
                         Matrix.Translation((0.31, 0, 0.09)) @ Matrix.Rotation(math.pi / 2, 4, "Y"))
    return [cover, block, top, clasp], 30


def map_icon():
    bark = O.plain("birch_bark", lin((226, 214, 186)), rough=0.85)
    roll = [S.log((-0.5, 0, 0.09), (0.5, 0, 0.09), 0.09, bark, 1, sag=0.0)]
    sheet = O._prism_obj("sheet", [(-0.48, 0.0), (0.48, 0.0), (0.48, 0.6), (-0.48, 0.6)], 0.01, bark,
                         Matrix.Translation((0, 0.05, 0.005)) @ Matrix.Rotation(-math.pi / 2, 4, "X"))
    ink = O.plain("ink", lin((60, 40, 30)), rough=0.8)
    lines = [O._prism_obj("line", [(-0.3 + i * 0.12, 0.15), (-0.25 + i * 0.12, 0.15), (-0.15 + i * 0.1, 0.5), (-0.2 + i * 0.1, 0.5)], 0.004, ink,
                          Matrix.Translation((0, 0.05, 0.012)) @ Matrix.Rotation(-math.pi / 2, 4, "X")) for i in range(3)]
    return roll + [sheet] + lines, 25


def army_icon():
    wood = S.grain_mat("weathered_planks")
    iron = O.plain("dark_iron", lin((70, 68, 66)), rough=0.5, metal=0.8)
    steel = O.plain("steel", lin((170, 170, 170)), rough=0.35, metal=0.9)
    sh = O.round_shield(0.45, wood, iron)
    sh.matrix_world = Matrix.Rotation(-0.25, 4, "X")
    axe = O.axe(S.grain_mat("wood_planks_grey", name="grain_shaft"), steel, length=1.0)
    axe.matrix_world = Matrix.Translation((0.05, -0.2, -0.1)) @ Matrix.Rotation(0.7, 4, "Y")
    return [sh, axe], 35


ICONS = {"ore": wood_icon, "aether": bread_icon, "shard": silver_icon, "builder": builder_icon, "codex": codex_icon, "map": map_icon,
         "army": army_icon}


def render(name, objs, yaw, opts):
    sc = bpy.context.scene
    sc.render.resolution_x = sc.render.resolution_y = 256
    sc.view_settings.exposure = T.exposure(0.5)
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("Cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.rotation_euler = (math.radians(58), 0, math.radians(yaw))
    bpy.context.view_layer.update()
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    center = sum(pts, Vector()) / len(pts)
    fwd = Vector((0, 0, -1))
    fwd.rotate(cam.rotation_euler)
    cam.location = center - fwd * 30
    bpy.context.view_layer.update()
    inv = cam.matrix_world.inverted()
    q = [inv @ p for p in pts]
    cam_data.ortho_scale = max(max(p.x for p in q) - min(p.x for p in q), max(p.y for p in q) - min(p.y for p in q)) * 1.1
    cam.location = center - fwd * 30
    out = os.path.join(opts["out"], "icons", name + ".png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print(f"[icons] {name} -> {out}")


def main():
    opts = R.args()
    only = opts.get("only") or set()
    for name, fn in ICONS.items():
        if only and name not in only:
            continue
        sc = R.reset_scene(int(opts["samples"]))
        bpy.data.objects.remove(bpy.data.objects["ShadowCatcher"], do_unlink=True)
        S.reset()
        K.palette()
        K.light_rig(sc)
        objs, yaw = fn()
        render(name, objs, yaw, opts)


if __name__ == "__main__":
    main()
