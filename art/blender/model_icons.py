"""Иконки интерфейса как игровые иллюстрации: у каждой свой предмет, собранный в Blender, — монеты, каравай, брёвна,
шестерня, замок, рог, стяг... Объём, глянец, контровой свет по краю и отражения неба на металле, чтобы значок читался
даже мелким. Те же правила, что у построек: формы утрированы, цвета чистые.

Запуск: blender -b -P art/blender/model_icons.py -- --out art/out --samples 48 [--only star,menu]
Результат: <out>/icons/<имя>.png (256×256, прозрачный фон); pack_atlas.py обводит и кладёт в атлас как icon/<имя>.
"""
import math
import os
import random
import sys

import bmesh
import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import render_sprites as R  # noqa: E402

FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "engine", "assets", "fonts", "Rubik-Bold.ttf")


def lin(c):
    return tuple((v / 255) ** 2.2 for v in c[:3])


# ---------- материалы: чистый цвет, глянец, лак ----------
def mat(name, rgb, rough=0.35, metal=0.0, coat=0.0, emit=0.0, noise=0.0, rgb2=None, alpha=1.0, transmission=0.0):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    b = N["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*lin(rgb), 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Coat Roughness"].default_value = 0.08
    if transmission:
        b.inputs["Transmission Weight"].default_value = transmission
        b.inputs["IOR"].default_value = 1.45
    if emit:
        b.inputs["Emission Color"].default_value = (*lin(rgb), 1)
        b.inputs["Emission Strength"].default_value = emit
    if noise:  # мягкие пятна второго тона (корка, кора, кожа)
        nz = N.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = noise
        nz.inputs["Detail"].default_value = 4
        ramp = N.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.35
        ramp.color_ramp.elements[1].position = 0.7
        ramp.color_ramp.elements[0].color = (*lin(rgb), 1)
        ramp.color_ramp.elements[1].color = (*lin(rgb2 or rgb), 1)
        L.new(nz.outputs["Fac"], ramp.inputs["Fac"])
        L.new(ramp.outputs["Color"], b.inputs["Base Color"])
        bump = N.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.15
        L.new(nz.outputs["Fac"], bump.inputs["Height"])
        L.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


def rings_mat(name, light, dark):
    """Торец бревна: годичные кольца."""
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    b = N["Principled BSDF"]
    b.inputs["Roughness"].default_value = 0.6
    w = N.new("ShaderNodeTexWave")
    w.wave_type = "RINGS"
    w.inputs["Scale"].default_value = 6
    w.inputs["Distortion"].default_value = 1.5
    tc = N.new("ShaderNodeTexCoord")
    L.new(tc.outputs["Object"], w.inputs["Vector"])
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (*lin(dark), 1)
    ramp.color_ramp.elements[1].color = (*lin(light), 1)
    ramp.color_ramp.elements[0].position = 0.25
    L.new(w.outputs["Fac"], ramp.inputs["Fac"])
    L.new(ramp.outputs["Color"], b.inputs["Base Color"])
    return m


# ---------- примитивы ----------
def _obj(bm, name, m, smooth=True, bevel=0.0, segs=3):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = smooth
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(m)
    if bevel:
        mod = o.modifiers.new("bevel", "BEVEL")
        mod.width = bevel
        mod.segments = segs
        mod.limit_method = "ANGLE"
        o.modifiers.new("wn", "WEIGHTED_NORMAL")
    return o


def place(o, loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    o.location = loc
    o.rotation_euler = rot
    o.scale = scale if isinstance(scale, tuple) else (scale, scale, scale)
    return o


def box(size, m, bevel=0.04, name="box", **kw):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=Vector(size), verts=bm.verts)
    return place(_obj(bm, name, m, smooth=False, bevel=bevel), **kw)


def cyl(r, h, m, bevel=0.03, segs=48, name="cyl", r2=None, **kw):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segs, radius1=r, radius2=r if r2 is None else r2, depth=h)
    o = _obj(bm, name, m, smooth=True, bevel=bevel)
    return place(o, **kw)


def ball(r, m, name="ball", sub=3, **kw):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=r)
    return place(_obj(bm, name, m), **kw)


def lathe(profile, m, segs=48, name="lathe", **kw):
    """Тело вращения по профилю [(r, z)] вокруг Z."""
    bm = bmesh.new()
    rows = []
    for r, z in profile:
        rows.append([bm.verts.new((math.cos(a) * r, math.sin(a) * r, z)) for a in (i * 2 * math.pi / segs for i in range(segs))])
    for a, b in zip(rows, rows[1:]):
        for i in range(segs):
            j = (i + 1) % segs
            bm.faces.new((a[i], a[j], b[j], b[i]))
    if profile[0][0] > 0:
        bm.faces.new(rows[0][::-1])
    if profile[-1][0] > 0:
        bm.faces.new(rows[-1])
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-5)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_obj(bm, name, m), **kw)


def slab(pts, depth, m, bevel=0.03, name="slab", **kw):
    """Плоская фигура (контур [(x, y)] в плоскости XZ, толщина по Y) с фаской — звезда, стрела, крест, шестерня."""
    bm = bmesh.new()
    front = [bm.verts.new((x, -depth / 2, y)) for x, y in pts]
    back = [bm.verts.new((x, depth / 2, y)) for x, y in pts]
    bm.faces.new(front[::-1])
    bm.faces.new(back)
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((front[i], front[j], back[j], back[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_obj(bm, name, m, smooth=False, bevel=bevel, segs=4), **kw)


def tube(path, r, m, segs=16, name="tube", r_end=None, **kw):
    """Трубка по ломаной path [(x, y, z)] — рукояти, древки, дужки."""
    bm = bmesh.new()
    pts = [Vector(p) for p in path]
    rows = []
    a = None
    for k, p in enumerate(pts):
        t = (pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]).normalized()
        if a is None:
            a = t.cross(Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))).normalized()
        else:  # параллельный перенос: рамка не перекручивается на изгибах
            a = (a - t * a.dot(t)).normalized()
        b = t.cross(a)
        rr = r if r_end is None else r + (r_end - r) * k / (len(pts) - 1)
        rows.append([bm.verts.new(p + (a * math.cos(q) + b * math.sin(q)) * rr) for q in (i * 2 * math.pi / segs for i in range(segs))])
    for a_, b_ in zip(rows, rows[1:]):
        for i in range(segs):
            j = (i + 1) % segs
            bm.faces.new((a_[i], a_[j], b_[j], b_[i]))
    bm.faces.new(rows[0][::-1])
    bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return place(_obj(bm, name, m), **kw)


def text3d(s, size, depth, m, name="text", **kw):
    cu = bpy.data.curves.new(name, "FONT")
    cu.body = s
    cu.font = bpy.data.fonts.load(FONT)
    cu.size = size
    cu.extrude = depth
    cu.bevel_depth = depth * 0.35
    cu.align_x = "CENTER"
    cu.align_y = "CENTER"
    o = bpy.data.objects.new(name, cu)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(m)
    return place(o, **kw)


def star_pts(ro, ri, n=5, rot=90):
    return [(math.cos(math.radians(rot + i * 180 / n)) * (ro if i % 2 == 0 else ri),
             math.sin(math.radians(rot + i * 180 / n)) * (ro if i % 2 == 0 else ri)) for i in range(2 * n)]


# ---------- палитра предметов ----------
def P():
    return {
        "gold": mat("gold", (255, 210, 80), rough=0.2, metal=1.0),
        "gold_dark": mat("gold_dark", (200, 130, 30), rough=0.3, metal=1.0),
        "silver": mat("silver", (226, 232, 240), rough=0.18, metal=1.0),
        "steel": mat("steel", (200, 210, 222), rough=0.22, metal=1.0),
        "iron": mat("iron", (92, 100, 112), rough=0.35, metal=0.9),
        "red": mat("red", (214, 46, 40), rough=0.3, coat=0.6),
        "red_dark": mat("red_dark", (140, 24, 24), rough=0.35, coat=0.4),
        "green": mat("green", (96, 200, 60), rough=0.28, coat=0.6),
        "blue": mat("blue", (52, 120, 232), rough=0.28, coat=0.6),
        "white": mat("white", (250, 248, 240), rough=0.3, coat=0.5),
        "wood": mat("wood", (176, 104, 50), rough=0.55, noise=6, rgb2=(140, 78, 36)),
        "wood_light": mat("wood_light", (224, 160, 92), rough=0.5, noise=5, rgb2=(196, 132, 70)),
        "bark": mat("bark", (112, 66, 36), rough=0.75, noise=9, rgb2=(84, 48, 26)),
        "leather": mat("leather", (150, 82, 40), rough=0.5, noise=7, rgb2=(118, 62, 30)),
        "cream": mat("cream", (246, 232, 196), rough=0.6),
        "dark": mat("dark", (40, 30, 26), rough=0.6),
    }


# ---------- иконки ----------
ICONS = {}


def icon(name, elev=28, yaw=0, fit=1.0):
    def deco(fn):
        ICONS[name] = (fn, elev, yaw, fit)
        return fn
    return deco


@icon("shard", elev=34)
def silver(M):
    """Серебро: две стопки монет-чешуек и россыпь перед ними, сзади кожаный кошель."""
    rnd = random.Random(5)
    coin = lambda loc, rot: lathe([(0.0, 0.0), (0.27, 0.0), (0.3, 0.025), (0.3, 0.065), (0.27, 0.09), (0.2, 0.09), (0.19, 0.075),
                                   (0.0, 0.075)], M["silver"], segs=40, name="coin", loc=loc, rot=rot)
    for sx, n in ((-0.3, 5), (0.32, 3)):
        for i in range(n):
            coin((sx + rnd.uniform(-0.02, 0.02), 0.15 + rnd.uniform(-0.02, 0.02), i * 0.088), (0, 0, rnd.uniform(0, 6)))
    for x, y, r in ((-0.05, -0.38, 0.3), (0.45, -0.3, -0.25), (-0.55, -0.25, 0.4)):
        coin((x, y, 0.06), (r, 0.15, 0))
    pouch = lathe([(0.0, 0.0), (0.3, 0.04), (0.42, 0.22), (0.4, 0.42), (0.22, 0.6), (0.13, 0.66), (0.2, 0.76), (0.24, 0.82), (0.0, 0.8)],
                  M["leather"], name="pouch", loc=(0.0, 0.62, 0.0))
    tube([(-0.17, 0.62, 0.66), (0.0, 0.47, 0.67), (0.17, 0.62, 0.66)], 0.03, M["red"], name="string")
    return pouch


@icon("ore", elev=24, yaw=-20)
def logs(M):
    """Лес: три бревна пирамидкой и топор, воткнутый в верхнее."""
    ends = rings_mat("rings", (238, 196, 132), (196, 140, 80))
    for x, z in ((-0.29, 0.27), (0.29, 0.27), (0.0, 0.76)):
        cyl(0.27, 1.25, M["bark"], bevel=0.04, loc=(x, 0, z), rot=(math.pi / 2, 0, 0), name="log")
        for s in (-1, 1):
            cyl(0.235, 0.02, ends, bevel=0.0, loc=(x, s * 0.627, z), rot=(math.pi / 2, 0, 0), name="end")
    tube([(0.25, 0.05, 1.0), (0.55, 0.05, 1.6)], 0.045, M["wood_light"], name="haft")
    slab([(-0.05, -0.18), (0.22, -0.26), (0.26, 0.12), (-0.05, 0.08)], 0.06, M["steel"], bevel=0.015, loc=(0.18, 0.05, 1.03),
         rot=(0, math.radians(-30), 0), name="blade")


@icon("aether", elev=42)
def bread(M):
    """Хлеб: каравай с глянцевой коркой и тремя надрезами, за ним два колоса."""
    crust = mat("crust", (186, 104, 36), rough=0.5, coat=0.15, noise=3, rgb2=(164, 88, 30))
    lathe([(0.0, 0.0), (0.66, 0.0), (0.72, 0.1), (0.7, 0.28), (0.58, 0.45), (0.36, 0.56), (0.0, 0.6)], crust, name="loaf")
    crumb = mat("crumb", (255, 236, 186), rough=0.7)
    for i in (-1, 0, 1):  # надрезы — светлые полосы поперёк верха, по склону каравая
        y = i * 0.24
        z = 0.6 - 0.06 * abs(i)
        half = 0.42 - 0.12 * abs(i)
        tube([(-half, y - 0.04, z - 0.08), (0.0, y, z + 0.06), (half, y + 0.04, z - 0.08)], 0.075, crumb, name="score")
    straw = mat("straw", (246, 200, 84), rough=0.4, coat=0.3)
    for ang in (-32, 0, 32):  # пучок колосьев веером за караваем
        r = math.radians(ang)
        base = Vector((0.0, 0.5, 0.15))
        top = base + Vector((math.sin(r), 0.1, math.cos(r))) * 1.25
        tube([base, top], 0.03, straw, name="stalk")
        for k in range(7):
            p = base.lerp(top, 0.5 + k * 0.075)
            for side in (-1, 1):
                ball(0.085, straw, loc=p + Vector((math.cos(r) * side * 0.07, 0, -math.sin(r) * side * 0.07)), scale=(0.7, 0.7, 1.4),
                     rot=(0, r + side * 0.5, 0), name="grain")


@icon("star", elev=12)
def star(M, m=None):
    slab(star_pts(0.95, 0.42), 0.32, m or M["gold"], bevel=0.12, name="star")


@icon("star_off", elev=12)
def star_off(M):
    star(M, mat("stone", (110, 104, 112), rough=0.6))


@icon("menu", elev=20, yaw=15)
def gear(M):
    pts = []
    n = 10
    for i in range(n * 4):
        a = i * 2 * math.pi / (n * 4)
        r = 0.92 if (i % 4) in (1, 2) else 0.7
        pts.append((math.cos(a) * r, math.sin(a) * r))
    slab(pts, 0.32, M["steel"], bevel=0.05, name="gear")
    cyl(0.3, 0.42, M["iron"], bevel=0.04, rot=(math.pi / 2, 0, 0), name="hub")
    cyl(0.14, 0.46, M["dark"], bevel=0.02, rot=(math.pi / 2, 0, 0), name="hole")


@icon("lock", elev=18, yaw=-15)
def padlock(M):
    box((1.2, 0.5, 0.95), M["iron"], bevel=0.12, loc=(0, 0, 0.47), name="body")
    tube([(-0.36, 0, 0.9)] + [(math.cos(a) * -0.36, 0, 1.25 + math.sin(a) * 0.36) for a in [i * math.pi / 12 for i in range(13)]]
         + [(0.36, 0, 0.9)], 0.1, M["steel"], name="shackle")
    cyl(0.24, 0.06, M["gold"], bevel=0.02, loc=(0, -0.26, 0.5), rot=(math.pi / 2, 0, 0), name="plate")
    box((0.1, 0.06, 0.24), M["dark"], bevel=0.02, loc=(0, -0.29, 0.44), name="slot")
    cyl(0.07, 0.06, M["dark"], bevel=0.0, loc=(0, -0.29, 0.56), rot=(math.pi / 2, 0, 0), name="hole")


@icon("builder", elev=26, yaw=-15)
def builder(M):
    """Плотник: топор, всаженный в чурбак."""
    cyl(0.62, 0.7, M["bark"], bevel=0.05, loc=(0, 0, 0.35), name="stump")
    cyl(0.57, 0.02, rings_mat("rings", (238, 196, 132), (196, 140, 80)), bevel=0.0, loc=(0, 0, 0.71), name="top")
    tube([(0.05, 0, 0.72), (0.62, 0, 1.75)], 0.055, M["wood_light"], name="haft")
    slab([(-0.5, -0.12), (0.06, -0.2), (0.14, 0.26), (-0.5, 0.4)], 0.09, M["steel"], bevel=0.025, loc=(0.05, 0, 0.92),
         rot=(0, math.radians(-30), 0), name="blade")


@icon("build", elev=20)
def build(M):
    """Строить: молоток и пила крест-накрест."""
    teeth = [(-0.9, -0.12)] + [(-0.9 + i * 0.09 + (0.045 if i % 2 else 0), -0.12 - (0.06 if i % 2 else 0)) for i in range(1, 18)] + \
        [(0.65, -0.12), (0.65, 0.18), (-0.9, 0.1)]
    saw = slab(teeth, 0.03, M["steel"], bevel=0.008, name="saw", rot=(0, math.radians(35), 0), loc=(0, 0.05, 0))
    box((0.4, 0.12, 0.3), M["wood"], bevel=0.06, loc=(0.62, 0.05, -0.43), rot=(0, math.radians(35), 0), name="saw_grip")
    tube([(0.55, -0.1, -0.75), (-0.45, -0.1, 0.6)], 0.075, M["wood_light"], name="handle")
    box((0.62, 0.22, 0.24), M["iron"], bevel=0.05, loc=(-0.48, -0.1, 0.66), rot=(0, math.radians(-36), 0), name="head")


@icon("army", elev=20)
def army(M):
    """Войско: круглый червлёный щит с золотым умбоном, за ним копьё и сабля."""
    tube([(-0.95, 0.3, -0.95), (0.8, 0.3, 0.8)], 0.045, M["wood"], name="spear")
    slab([(0, 0), (0.13, 0.1), (0, 0.45), (-0.13, 0.1)], 0.05, M["steel"], bevel=0.012, loc=(0.78, 0.3, 0.78),
         rot=(0, math.radians(-45), 0), name="tip")
    slab([(-0.1, 0), (0.1, 0), (0.16, 1.5), (0.0, 1.68), (-0.06, 1.5)], 0.05, M["steel"], bevel=0.012, loc=(0.95, 0.3, -0.95),
         rot=(0, math.radians(-45), 0), scale=(1.1, 1, 1.45), name="sabre")
    tube([(1.0, 0.3, -1.0), (1.22, 0.3, -1.22)], 0.065, M["red_dark"], name="grip")
    box((0.42, 0.08, 0.08), M["gold"], bevel=0.03, loc=(0.97, 0.3, -0.97), rot=(0, math.radians(45), 0), name="guard")
    lathe([(0.0, 0.0), (0.82, 0.0), (0.84, 0.06), (0.8, 0.12), (0.0, 0.16)], M["red"], segs=64, name="shield", rot=(math.pi / 2, 0, 0))
    lathe([(0.84, -0.01), (0.9, 0.04), (0.88, 0.13), (0.8, 0.14), (0.8, 0.0)], M["gold"], segs=64, name="rim", rot=(math.pi / 2, 0, 0))
    lathe([(0.0, 0.14), (0.26, 0.14), (0.24, 0.24), (0.12, 0.33), (0.0, 0.35)], M["gold"], segs=40, name="boss", rot=(math.pi / 2, 0, 0))


@icon("sword", elev=14)
def sabre(M):
    """Сабля: широкий изогнутый клинок, золотая гарда, красная рукоять."""
    back, edge = [], []
    for i in range(13):
        t = i / 12
        x = 0.35 * t * t
        z = -0.1 + 1.9 * t
        w = 0.2 * (1 - 0.6 * t)
        back.append((x - w * 0.3, z))
        edge.append((x + w, z - 0.04))
    blade = back + [(0.35 + 0.1, 1.95)] + edge[::-1]
    slab(blade, 0.06, M["steel"], bevel=0.018, rot=(0, math.radians(-40), 0), name="blade")
    box((0.55, 0.12, 0.1), M["gold"], bevel=0.04, loc=(0.05, 0, -0.12), rot=(0, math.radians(-40), 0), name="guard")
    tube([(0.12, 0, -0.2), (0.36, 0, -0.5)], 0.075, M["red"], name="grip")
    ball(0.1, M["gold"], loc=(0.4, 0, -0.56), name="pommel")


@icon("map", elev=48)
def map_(M):
    """Поход: развёрнутая карта с загнутыми краями, лес и река, красный путь к крестику, флажок."""
    paper = mat("paper", (244, 226, 180), rough=0.7)
    box((1.6, 1.1, 0.03), paper, bevel=0.01, name="sheet")
    for s in (-1, 1):
        cyl(0.09, 1.1, paper, bevel=0.0, loc=(s * 0.82, 0, 0.07), rot=(math.pi / 2, 0, 0), name="roll")
    river = mat("river", (70, 150, 230), rough=0.4)
    forest = mat("forest", (80, 160, 70), rough=0.5)
    tube([(-0.7, 0.4, 0.02), (-0.3, 0.15, 0.02), (0.0, 0.3, 0.02), (0.4, -0.1, 0.02), (0.7, -0.45, 0.02)], 0.035, river, name="river")
    for x, y in ((-0.45, -0.25), (-0.3, -0.38), (-0.55, -0.4), (0.35, 0.3), (0.5, 0.2)):
        lathe([(0.0, 0.0), (0.08, 0.01), (0.0, 0.16)], forest, segs=12, loc=(x, y, 0.015), name="tree")
    for k in range(6):
        x = -0.55 + k * 0.17
        y = 0.25 - k * 0.05 + math.sin(k) * 0.05
        box((0.08, 0.035, 0.012), M["red"], bevel=0.0, loc=(x, y, 0.022), rot=(0, 0, -0.3), name="route")
    for r in (0.6, -0.6):
        box((0.22, 0.05, 0.02), M["red"], bevel=0.0, loc=(0.48, 0.1, 0.03), rot=(0, 0, r), name="x")
    tube([(0.48, 0.1, 0.0), (0.48, 0.1, 0.5)], 0.018, M["wood"], name="pin")
    slab([(0, 0), (0.25, -0.07), (0, -0.16)], 0.02, M["red"], bevel=0.005, loc=(0.48, 0.1, 0.5), name="pennant")


@icon("codex", elev=30, yaw=-20)
def codex(M):
    """Летопись: толстая книга в красной коже с золотыми углами, застёжкой и ромбом на крышке."""
    cover = mat("cover", (178, 38, 36), rough=0.4, coat=0.3)
    box((1.2, 1.5, 0.08), cover, bevel=0.03, loc=(0, 0, 0.04), name="back")
    box((1.12, 1.44, 0.3), M["cream"], bevel=0.02, loc=(0.02, 0, 0.23), name="pages")
    box((1.2, 1.5, 0.08), cover, bevel=0.03, loc=(0, 0, 0.42), name="front")
    cyl(0.2, 1.5, cover, bevel=0.02, loc=(-0.6, 0, 0.23), rot=(math.pi / 2, 0, 0), name="spine")
    for x in (-0.52, 0.52):
        for y in (-0.67, 0.67):
            box((0.2, 0.2, 0.1), M["gold"], bevel=0.04, loc=(x, y, 0.43), name="corner")
    slab([(0, -0.3), (0.24, 0), (0, 0.3), (-0.24, 0)], 0.06, M["gold"], bevel=0.02, loc=(0.02, 0, 0.47), rot=(math.pi / 2, 0, 0), name="emblem")
    box((0.3, 0.22, 0.42), M["gold"], bevel=0.04, loc=(0.6, 0, 0.23), name="clasp")


@icon("chat", elev=10)
def chat(M):
    """Чат: глянцевое облачко с хвостиком и тремя точками."""
    pts = []
    for i in range(48):
        a = i * 2 * math.pi / 48
        pts.append((math.cos(a) * 0.95, 0.15 + math.sin(a) * 0.68))
    k = min(range(48), key=lambda i: abs(math.atan2(pts[i][1] - 0.15, pts[i][0]) + 2.2))
    pts = pts[:k] + [(-0.62, -0.75)] + pts[k + 2:]
    slab(pts, 0.32, M["white"], bevel=0.12, name="bubble")
    for x in (-0.38, 0.0, 0.38):
        ball(0.12, M["blue"], loc=(x, -0.2, 0.15), scale=(1, 0.6, 1), name="dot")


@icon("clock", elev=16, yaw=-10)
def hourglass(M):
    for z in (0.0, 1.5):
        cyl(0.55, 0.14, M["wood"], bevel=0.04, loc=(0, 0, z), name="cap")
    for a in (0.3, 2.4, 4.5):
        tube([(math.cos(a) * 0.45, math.sin(a) * 0.45, 0.05), (math.cos(a) * 0.45, math.sin(a) * 0.45, 1.45)], 0.05, M["wood_light"], name="post")
    glass = mat("glass", (220, 240, 255), rough=0.05, transmission=1.0)
    lathe([(0.05, 0.07), (0.36, 0.2), (0.38, 0.45), (0.07, 0.75), (0.38, 1.05), (0.36, 1.3), (0.05, 1.43)], glass, name="glass")
    sand = mat("sand", (246, 190, 70), rough=0.5)
    lathe([(0.0, 0.08), (0.33, 0.2), (0.3, 0.38), (0.0, 0.42)], sand, name="sand_low")
    lathe([(0.0, 0.85), (0.2, 0.92), (0.32, 1.1), (0.0, 1.12)], sand, name="sand_up")
    tube([(0, 0, 0.42), (0, 0, 0.8)], 0.02, sand, name="stream")


@icon("upgrade", elev=10)
def upgrade(M):
    slab([(0, 1.0), (0.85, 0.15), (0.38, 0.15), (0.38, -0.85), (-0.38, -0.85), (-0.38, 0.15), (-0.85, 0.15)], 0.36, M["green"], bevel=0.12,
         name="arrow")


@icon("heal", elev=22)
def potion(M):
    """Лекарь: пузатая склянка с зелёным снадобьем, пробка и лист."""
    glass = mat("glass", (220, 240, 255), rough=0.05, transmission=1.0)
    lathe([(0.0, 0.0), (0.5, 0.05), (0.66, 0.4), (0.55, 0.78), (0.2, 0.95), (0.17, 1.3), (0.21, 1.34), (0.0, 1.34)], glass, name="flask")
    brew = mat("brew", (90, 220, 90), rough=0.15, emit=0.6, transmission=0.3)
    lathe([(0.0, 0.03), (0.46, 0.08), (0.6, 0.4), (0.52, 0.66), (0.0, 0.67)], brew, name="brew")
    cyl(0.16, 0.3, M["wood_light"], bevel=0.03, loc=(0, 0, 1.4), name="cork")
    slab([(0, 0), (0.25, 0.12), (0.45, 0.0), (0.25, -0.1)], 0.03, M["green"], bevel=0.01, loc=(0.18, -0.05, 1.42), rot=(0, -0.5, 0.3), name="leaf")


@icon("rage", elev=18, yaw=10)
def horn(M):
    """Боевой рог: изогнутый турий рог с золотыми обоймами и мундштуком."""
    ivory = mat("ivory", (246, 234, 206), rough=0.35, coat=0.4)
    path, radii = [], []
    for i in range(17):
        t = i / 16
        a = math.pi * (1.1 - 0.85 * t)
        path.append((math.cos(a) * 0.85, 0, math.sin(a) * 0.75))
    for k in range(len(path) - 1):
        r0 = 0.06 + 0.3 * (k / 16) ** 1.6
        r1 = 0.06 + 0.3 * ((k + 1) / 16) ** 1.6
        tube([path[k], path[k + 1]], r0, ivory, r_end=r1, name="horn")
    for k in (5, 10):
        p0, p1 = Vector(path[k]), Vector(path[k + 1])
        r = 0.06 + 0.3 * (k / 16) ** 1.6 + 0.03
        tube([p0, p0 + (p1 - p0) * 0.35], r, M["gold"], name="band")
    p0, p1 = Vector(path[-2]), Vector(path[-1])
    tube([p1 - (p1 - p0) * 0.1, p1 + (p1 - p0) * 0.15], 0.38, M["gold"], r_end=0.39, name="bell")
    tube([Vector(path[0]) + Vector((0.02, 0, -0.05)), Vector(path[0]) + Vector((0.06, 0, -0.25))], 0.07, M["gold"], name="mouth")


@icon("flash", elev=18)
def helmet(M):
    """Воевода: золочёный шишак с наносником и красным султаном."""
    lathe([(0.0, 1.3), (0.08, 1.2), (0.3, 0.9), (0.55, 0.5), (0.62, 0.2), (0.62, 0.0), (0.0, 0.0)], M["gold"], segs=64, name="dome")
    lathe([(0.6, -0.02), (0.7, 0.0), (0.7, 0.14), (0.6, 0.16)], M["steel"], segs=64, name="band")
    box((0.12, 0.12, 0.6), M["steel"], bevel=0.04, loc=(0, -0.66, -0.12), name="nasal")
    lathe([(0.66, 0.0), (0.74, -0.35), (0.72, -0.55), (0.0, -0.55)], M["iron"], segs=64, name="mail")
    tube([(0, 0, 1.2), (0, 0, 1.38)], 0.04, M["gold"], name="spike")
    ball(0.2, M["red"], loc=(0.0, 0.0, 1.5), scale=(0.85, 0.85, 1.9), name="plume")


@icon("night", elev=14)
def axes(M):
    """Набег: два скрещённых топора."""
    for s in (1, -1):
        rz = math.radians(38 * s)
        tube([(-0.75 * s, 0.0, -0.9), (0.55 * s, 0.0, 0.85)], 0.06, M["wood"], name="haft")
        slab([(0, -0.2), (0.42, -0.36), (0.5, 0.18), (0.0, 0.12)], 0.08, M["steel"], bevel=0.02, loc=(0.42 * s, 0.05 * s, 0.6),
             rot=(0, -rz if s > 0 else -rz, 0), scale=(s, 1, 1), name="blade")


@icon("lantern", elev=22, yaw=-20)
def tower(M):
    """Острог: рубленая башня с красным шатром и стягом, у подножия частокол."""
    for k in range(7):
        z = 0.08 + k * 0.17
        for y in (-0.42, 0.42):
            cyl(0.09, 0.95, M["wood_light"], bevel=0.02, loc=(0, y, z), rot=(0, math.pi / 2, 0), name="log")
        for x in (-0.42, 0.42):
            cyl(0.09, 0.95, M["wood_light"], bevel=0.02, loc=(x, 0, z + 0.085), rot=(math.pi / 2, 0, 0), name="log")
    lathe([(0.0, 1.95), (0.78, 1.25), (0.0, 1.25)], M["red"], segs=4, rot=(0, 0, math.pi / 4), name="roof")
    tube([(0, 0, 1.9), (0, 0, 2.35)], 0.025, M["wood"], name="pole")
    slab([(0, 0), (0.34, -0.06), (0, -0.2)], 0.02, M["gold"], bevel=0.005, loc=(0, 0, 2.33), name="flag")
    box((0.2, 0.05, 0.3), M["dark"], bevel=0.02, loc=(0, -0.5, 0.8), name="window")
    for k in range(-4, 5):
        x = k * 0.2
        tube([(x, -0.78, -0.05), (x, -0.78, 0.45)], 0.085, M["wood"], name="stake")
        lathe([(0.085, 0.45), (0.0, 0.62)], M["wood"], segs=12, loc=(x, -0.78, 0), name="tip")


@icon("flag", elev=16)
def banner(M):
    tube([(-0.6, 0, -1.0), (-0.6, 0, 1.15)], 0.055, M["wood"], name="pole")
    ball(0.11, M["gold"], loc=(-0.6, 0, 1.22), name="finial")
    slab([(0, 0), (1.2, 0.06), (0.95, -0.38), (1.2, -0.8), (0, -0.74)], 0.06, M["red"], bevel=0.02, loc=(-0.55, 0, 1.05), name="cloth")
    slab([(0, -0.17), (0.17, 0), (0, 0.17), (-0.17, 0)], 0.08, M["gold"], bevel=0.02, loc=(-0.05, -0.02, 0.68), name="emblem")


@icon("person", elev=10)
def person(M):
    """Профиль, люди: голова с бородой в меховой шапке, плечи в красном кафтане."""
    skin = mat("skin", (240, 184, 140), rough=0.5)
    fur = mat("fur", (120, 78, 44), rough=0.85, noise=18, rgb2=(92, 58, 32))
    beard = mat("beard", (150, 90, 46), rough=0.7, noise=10, rgb2=(120, 70, 36))
    lathe([(0.0, -0.6), (0.85, -0.6), (0.9, -0.4), (0.75, -0.1), (0.35, 0.05), (0.0, 0.06)], M["red"], name="shoulders")
    lathe([(0.75, -0.12), (0.8, -0.2), (0.7, -0.1), (0.0, 0.02)], M["gold"], name="collar")
    ball(0.45, skin, loc=(0, 0, 0.45), scale=(1, 0.95, 1.05), name="head")
    ball(0.36, beard, loc=(0, -0.2, 0.18), scale=(1.0, 0.7, 1.0), name="beard")
    ball(0.1, beard, loc=(0, -0.38, 0.38), scale=(1.6, 0.6, 0.6), name="moustache")
    for x in (-0.15, 0.15):
        ball(0.055, M["dark"], loc=(x, -0.4, 0.55), name="eye")
    ball(0.09, skin, loc=(0, -0.45, 0.45), name="nose")
    lathe([(0.48, 0.62), (0.56, 0.66), (0.58, 0.85), (0.5, 0.95), (0.4, 0.9), (0.4, 0.62)], fur, name="hat_band")
    lathe([(0.42, 0.85), (0.38, 1.1), (0.2, 1.22), (0.0, 1.24)], M["red"], name="hat_top")


@icon("info", elev=8)
def info(M):
    lathe([(0.0, -0.18), (0.9, -0.18), (0.98, -0.1), (0.98, 0.1), (0.9, 0.18), (0.0, 0.18)], M["blue"], segs=64, rot=(math.pi / 2, 0, 0), name="disc")
    box((0.3, 0.2, 0.85), M["white"], bevel=0.1, loc=(0, -0.22, -0.18), name="i_stem")
    ball(0.19, M["white"], loc=(0, -0.24, 0.5), name="i_dot")


@icon("close", elev=8)
def close(M):
    w, L = 0.24, 0.9
    pts = []
    for k in range(4):
        a = k * math.pi / 2 + math.pi / 4
        c, s_ = math.cos(a), math.sin(a)
        n = (-s_, c)
        pts += [(c * w * 1.41 * 0.0 + n[0] * w * -1 + c * L, s_ * L + n[1] * -w), (c * L + n[0] * w, s_ * L + n[1] * w)]
        a2 = a + math.pi / 4
        pts.append((math.cos(a2) * w * 1.41, math.sin(a2) * w * 1.41))
    slab(pts, 0.42, M["red"], bevel=0.14, name="x")


@icon("heart", elev=10)
def heart(M):
    pts = []
    for i in range(64):
        t = i / 64 * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((x / 17, y / 17))
    slab(pts, 0.4, M["red"], bevel=0.14, name="heart")


# ---------- съёмка ----------
def rig(sc):
    """Свет «как у иконок в играх»: тёплый ключ слева-сверху, яркий холодный контровой сзади-справа, мягкая заливка;
    небо — градиент для отражений металла (сам фон прозрачный)."""
    for o in list(bpy.data.objects):
        if o.type == "LIGHT" or o.name == "ShadowCatcher":
            bpy.data.objects.remove(o, do_unlink=True)
    w = sc.world.node_tree
    N, L = w.nodes, w.links
    bg = N["Background"]
    tc = N.new("ShaderNodeTexCoord")
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(tc.outputs["Generated"], sep.inputs[0])
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.42, 0.34, 0.26, 1)
    ramp.color_ramp.elements[0].position = 0.42
    ramp.color_ramp.elements[1].color = (0.9, 0.95, 1.0, 1)
    ramp.color_ramp.elements[1].position = 0.62
    L.new(sep.outputs["Z"], ramp.inputs["Fac"])
    L.new(ramp.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 1.0
    for name, rot, energy, color, ang in (("Key", (52, 0, -40), 4.8, (1.0, 0.94, 0.84), 12),
                                          ("Rim", (60, 0, 150), 6.0, (0.85, 0.92, 1.0), 4),
                                          ("Fill", (75, 0, 45), 1.2, (0.9, 0.9, 1.0), 30)):
        ld = bpy.data.lights.new(name, "SUN")
        ld.energy = energy
        ld.color = color
        ld.angle = math.radians(ang)
        lo = bpy.data.objects.new(name, ld)
        lo.rotation_euler = tuple(math.radians(v) for v in rot)
        sc.collection.objects.link(lo)


def shoot(name, elev, yaw, fit, opts):
    sc = bpy.context.scene
    sc.render.resolution_x = sc.render.resolution_y = 256
    sc.view_settings.exposure = 0.0
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 85
    cam = bpy.data.objects.new("Cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    bpy.context.view_layer.update()
    deps = bpy.context.evaluated_depsgraph_get()
    pts = []
    for o in sc.objects:
        if o.type in ("MESH", "FONT"):
            ev = o.evaluated_get(deps)
            pts += [o.matrix_world @ Vector(c) for c in ev.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    center = (lo + hi) / 2
    radius = max((p - center).length for p in pts)
    cam.rotation_euler = (math.radians(90 - elev), 0, math.radians(yaw))
    fwd = Vector((0, 0, -1))
    fwd.rotate(cam.rotation_euler)
    dist = radius / math.tan(math.atan(18 / cam_data.lens)) * 1.02 / fit
    cam.location = center - fwd * dist
    out = os.path.join(opts["out"], "icons", name + ".png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print(f"[icons] {name} -> {out}")


def main():
    opts = R.args()
    only = opts.get("only") or set()
    for name, (fn, elev, yaw, fit) in ICONS.items():
        if only and name not in only:
            continue
        sc = R.reset_scene(int(opts["samples"]))
        rig(sc)
        fn(P())
        shoot(name, elev, yaw, fit, opts)


if __name__ == "__main__":
    main()
