"""Одежда и снаряжение для людей из human_kit: ткани и кожа из фототекстур Poly Haven (CC0).

Одежда — копии участков тела, приподнятые над кожей: у них те же веса костей, поэтому она двигается с телом без
симуляции. Полы кафтана — отдельный «колокол», веса берутся у ближайших вершин бёдер. Шапки, шлемы, щиты и оружие —
жёсткие детали, привязанные к костям.
"""
import math
import os

import bmesh
import bpy
from mathutils import Matrix, Vector, kdtree

import scan_kit as S
import toon_kit as T

TEX = {
    "wool": "poly_wool_herringbone", "felt": "caban", "fleece": "knitted_fleece", "linen": "rough_linen",
    "burlap": "hessian_380", "cotton": "cotton_jersey", "leather": "brown_leather", "leather2": "fabric_leather_01",
    "fur": "curly_teddy_natural", "steel": "metal_plate_02", "iron": "rusty_metal_03",
}

# Группы костей рига default
SPINE = ["spine01", "spine02", "spine03", "spine04", "spine05", "root", "breast.L", "breast.R", "clavicle.L", "clavicle.R",
         "shoulder01.L", "shoulder01.R", "pelvis.L", "pelvis.R", "neck01"]
UPPER_ARMS = ["upperarm01.L", "upperarm01.R", "upperarm02.L", "upperarm02.R"]
LOWER_ARMS = ["lowerarm01.L", "lowerarm01.R", "lowerarm02.L", "lowerarm02.R"]
HANDS_PREFIX = ("wrist", "metacarpal", "finger")
UPPER_LEGS = ["upperleg01.L", "upperleg01.R", "upperleg02.L", "upperleg02.R"]
LOWER_LEGS = ["lowerleg01.L", "lowerleg01.R", "lowerleg02.L", "lowerleg02.R"]
FEET_PREFIX = ("foot", "toe")


def fabric(kind, tint, scale=9.0, metal=0.0, rough=None, sat=0.25, name=None):
    """Ткань/кожа/металл по UV тела: фототекстура обесцвечивается (sat) и окрашивается в tint (линейный RGB)."""
    asset = TEX.get(kind, kind)
    name = name or f"fab_{kind}_{'_'.join(f'{c:.2f}' for c in tint)}_{scale}"
    if T.on():  # рисованная ткань/кожа/металл: чистый цвет, мягкая фактура
        base = tuple(int(255 * max(0.0, v) ** (1 / 2.2)) for v in tint[:3])
        shade = tuple(int(v * 0.86) for v in base)
        return T.solid("toon_" + name, [base, shade], rough=rough if rough is not None else (0.35 if metal else 0.8), metal=metal,
                       noise=scale * 1.5, bump=0.15, per_part=False, coords="UV")
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = S.pbr(asset, name=name)
    N, L = m.node_tree.nodes, m.node_tree.links
    co = next(n for n in N if n.type == "TEX_COORD")
    mp = next(n for n in N if n.type == "MAPPING")
    mp.inputs["Scale"].default_value = (scale, scale, scale)
    L.new(co.outputs["UV"], mp.inputs["Vector"])
    for n in N:
        if n.type == "TEX_IMAGE":
            n.projection = "FLAT"
    bsdf = N["Principled BSDF"]
    src = bsdf.inputs["Base Color"].links[0].from_socket
    hs = N.new("ShaderNodeHueSaturation")
    hs.inputs["Saturation"].default_value = sat
    # фототекстуры бывают тёмными: нормируем среднюю яркость к 1, чтобы ткань была цвета tint
    diff = next(n for n in N if n.type == "TEX_IMAGE" and n.image.colorspace_settings.name == "sRGB")
    g = 1.0 / max(0.03, _mean_luminance(diff.image))
    gain = N.new("ShaderNodeMix")
    gain.data_type = "RGBA"
    gain.blend_type = "MULTIPLY"
    gain.inputs["Factor"].default_value = 1.0
    gain.inputs["B"].default_value = (g, g, g, 1)
    L.new(src, gain.inputs["A"])
    L.new(gain.outputs["Result"], hs.inputs["Color"])
    mul = N.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    mul.inputs["B"].default_value = (*tint, 1)
    L.new(hs.outputs["Color"], mul.inputs["A"])
    L.new(mul.outputs["Result"], bsdf.inputs["Base Color"])
    bsdf.inputs["Metallic"].default_value = metal
    if rough is not None:
        for l in list(bsdf.inputs["Roughness"].links):
            L.remove(l)
        bsdf.inputs["Roughness"].default_value = rough
    return m


_LUM = {}


def _mean_luminance(img):
    """Средняя линейная яркость картинки (по прореженной выборке пикселей)."""
    if img.name in _LUM:
        return _LUM[img.name]
    import numpy as np
    px = np.empty(len(img.pixels), dtype=np.float32)
    img.pixels.foreach_get(px)
    p = px.reshape(-1, 4)[::97, :3]
    lum = float((p * np.array([0.2126, 0.7152, 0.0722], np.float32)).sum(1).mean())
    _LUM[img.name] = lum
    return lum


def plain(name, color, rough=0.7, metal=0.0):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    return m


def _membership(h, bm, bones, prefixes=()):
    """Доля веса вершины, приходящаяся на кости bones (и на кости с префиксами prefixes)."""
    dl = bm.verts.layers.deform.active
    names = h.bone_names
    want = {i for i, n in enumerate(names) if n in bones or n.startswith(tuple(prefixes))} if (bones or prefixes) else set()
    out = {}
    for v in bm.verts:
        d = v[dl]
        tot = sum(d.values()) or 1.0
        out[v] = sum(w for g, w in d.items() if g in want) / tot
    return out


def _attach(h, o):
    o.parent = h.arm
    md = o.modifiers.new("Armature", "ARMATURE")
    md.object = h.arm
    if o.name not in bpy.context.scene.collection.objects:
        bpy.context.scene.collection.objects.link(o)
    return o


def garment(h, bones, offset, mat, prefixes=(), thr=0.5, name="garment", cover=0.0, grow=None):
    """Одежда из участка тела: вершины, где доля весов bones ≥ thr, приподнятые на offset метров (в масштабе человека).
    grow(v, co) -> добавочный сдвиг по нормали (расширить рукав книзу, распустить штанину).
    cover > 0 — удалить под одеждой грани тела, полностью закрытые (доля > cover): кожа не пролезет при движении."""
    me = h.body.data.copy()
    me.materials.clear()
    me.materials.append(mat)
    bm = bmesh.new()
    bm.from_mesh(me)
    mem = _membership(h, bm, set(bones), prefixes)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if mem[v] < thr], context="VERTS")
    bm.normal_update()
    for v in bm.verts:
        extra = grow(v, v.co) if grow else 0.0
        v.co += v.normal * (offset + extra)
    for f in bm.faces:
        f.material_index = 0
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    for vg in h.body.vertex_groups:
        o.vertex_groups.new(name=vg.name)
    _attach(h, o)
    if cover > 0:
        hide_body(h, bones, prefixes, cover)
    return o


def hide_body(h, bones, prefixes=(), thr=0.95):
    """Убрать грани тела, целиком принадлежащие костям bones (их закрывает одежда)."""
    bm = bmesh.new()
    bm.from_mesh(h.body.data)
    mem = _membership(h, bm, set(bones), prefixes)
    dead = [f for f in bm.faces if all(mem[v] >= thr for v in f.verts)]
    bmesh.ops.delete(bm, geom=dead, context="FACES_ONLY")
    bm.to_mesh(h.body.data)
    bm.free()


def _kd_body(h, bones=None, prefixes=()):
    me = h.body.data
    bm = bmesh.new()
    bm.from_mesh(me)
    mem = _membership(h, bm, set(bones or []), prefixes) if (bones or prefixes) else None
    pts = [(v.index, h.body.matrix_world @ v.co) for v in bm.verts if mem is None or mem[v] > 0.3]
    bm.free()
    kd = kdtree.KDTree(len(pts))
    for i, p in pts:
        kd.insert(p, i)
    kd.balance()
    return kd


def transfer_weights(h, o, bones=None, prefixes=(), k=4):
    """Веса для детали o — по ближайшим вершинам тела (среднее k соседей с весом по расстоянию)."""
    kd = _kd_body(h, bones, prefixes)
    body = h.body.data
    groups = {vg.index: vg.name for vg in h.body.vertex_groups}
    for vg in h.body.vertex_groups:
        if vg.name not in o.vertex_groups:
            o.vertex_groups.new(name=vg.name)
    mw = o.matrix_world
    for v in o.data.vertices:
        acc, tot = {}, 0.0
        for co, idx, dist in kd.find_n(mw @ v.co, k):
            w0 = 1.0 / (dist + 1e-3)
            tot += w0
            for g in body.vertices[idx].groups:
                acc[g.group] = acc.get(g.group, 0) + g.weight * w0
        for gi, w in acc.items():
            if w / tot > 0.01:
                o.vertex_groups[groups[gi]].add([v.index], w / tot, "REPLACE")


def body_section(h, z, bones=None, prefixes=()):
    """Сечение тела на высоте z (мировые координаты): центр и полуоси по X и Y."""
    pts = [h.body.matrix_world @ v.co for v in h.body.data.vertices]
    if bones or prefixes:
        bm = bmesh.new()
        bm.from_mesh(h.body.data)
        mem = _membership(h, bm, set(bones or []), prefixes)
        keep = {v.index for v in bm.verts if mem[v] > 0.3}
        bm.free()
        pts = [p for i, p in enumerate(pts) if i in keep]
    band = [p for p in pts if abs(p.z - z) < 0.025]
    xs, ys = [p.x for p in band], [p.y for p in band]
    return Vector(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2, z)), (max(xs) - min(xs)) / 2, (max(ys) - min(ys)) / 2


def skirt(h, z_top, z_bottom, mat, flare=0.06, margin=0.025, front_open=0.0, name="skirt", segs=28, rings=7):
    """Полы кафтана/тулупа: колокол от пояса вниз. front_open — разрез спереди (доля окружности)."""
    c, rx, ry = body_section(h, z_top, SPINE + UPPER_LEGS)
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    grid = []
    for r in range(rings + 1):
        t = r / rings
        z = z_top + (z_bottom - z_top) * t
        k = 1 + flare * t / max(rx, 0.05) * 1.6
        row = []
        for s in range(segs + 1):
            a = -math.pi / 2 + (s / segs) * 2 * math.pi * (1 - front_open) + math.pi * front_open
            row.append(bm.verts.new((c.x + math.cos(a) * (rx + margin) * k, c.y + math.sin(a) * (ry + margin) * k * 1.05, z)))
        grid.append(row)
    for r in range(rings):
        for s in range(segs):
            f = bm.faces.new((grid[r][s], grid[r][s + 1], grid[r + 1][s + 1], grid[r + 1][s]))
            f.smooth = True
            for loop, (rr, ss) in zip(f.loops, ((r, s), (r, s + 1), (r + 1, s + 1), (r + 1, s))):
                loop[uvl].uv = (ss / segs * 2.0, rr / rings * 0.6)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    transfer_weights(h, o, SPINE + UPPER_LEGS + LOWER_LEGS)
    bpy.context.scene.collection.objects.unlink(o)
    return _attach(h, o)


# ---------- жёсткие детали ----------
def bone_attach(h, o, bone):
    """Привязать деталь к кости, сохранив её положение в позе покоя."""
    bpy.context.view_layer.update()
    mw = o.matrix_world.copy()
    o.parent = h.arm
    o.parent_type = "BONE"
    o.parent_bone = bone
    bpy.context.view_layer.update()
    o.matrix_world = mw
    return o


def head_frame(h):
    """Центр головы, радиус и макушка (по вершинам с весом кости head)."""
    bm = bmesh.new()
    bm.from_mesh(h.body.data)
    mem = _membership(h, bm, {"head"}, ("oculi", "orbicularis", "temporalis", "levator", "oris", "risorius", "jaw", "special"))
    pts = [h.body.matrix_world @ v.co for v in bm.verts if mem[v] > 0.6]
    bm.free()
    top = max(p.z for p in pts)
    xs = [p.x for p in pts]
    ys = [p.y for p in pts]
    c = Vector(((min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2 + 0.01, top - 0.11))
    return c, (max(xs) - min(xs)) / 2, top


def _mesh_obj(name, bm, mat):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    me.materials.append(mat)
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    return o


def revolve(name, profile, mat, center, segs=24, scale=(1, 1, 1), uv_scale=(1, 1)):
    """Тело вращения по профилю [(r, z), ...] вокруг вертикали через center."""
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    rows = []
    for r, z in profile:
        rows.append([bm.verts.new((center.x + math.cos(a) * r * scale[0], center.y + math.sin(a) * r * scale[1], center.z + z * scale[2]))
                     for a in (2 * math.pi * s / segs for s in range(segs))])
    for i in range(len(rows) - 1):
        for s in range(segs):
            sn = (s + 1) % segs
            f = bm.faces.new((rows[i][s], rows[i][sn], rows[i + 1][sn], rows[i + 1][s]))
            for loop, (ii, ss) in zip(f.loops, ((i, s), (i, s + 1), (i + 1, s + 1), (i + 1, s))):
                loop[uvl].uv = (ss / segs * uv_scale[0], ii / (len(rows) - 1) * uv_scale[1])
    if profile[-1][0] < 1e-4:
        pass
    else:
        bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return _mesh_obj(name, bm, mat)


def fur_hat(h, fur, top=None, height=0.13):
    """Меховая шапка: валик меха и (если top) суконный верх."""
    c, r, crown = head_frame(h)
    base = crown - 0.075
    band = revolve("hat_fur", [(r * 1.18, 0.0), (r * 1.3, 0.03), (r * 1.3, 0.07), (r * 1.15, 0.095)], fur, Vector((c.x, c.y, base)),
                   scale=(1, 1.08, 1), uv_scale=(3, 1))
    parts = [band]
    if top is not None:
        parts.append(revolve("hat_top", [(r * 1.12, 0.07), (r * 1.05, height * 0.8), (r * 0.7, height), (0.0, height + 0.012)], top,
                             Vector((c.x, c.y, base)), scale=(1, 1.08, 1), uv_scale=(3, 1)))
    for p in parts:
        bone_attach(h, p, "head")
    return parts


def helmet(h, steel, mail=None):
    """Шишак: высокая коническая тулья со шпилем; кольчужная бармица закрывает шею."""
    c, r, crown = head_frame(h)
    base = crown - 0.085
    parts = [revolve("helmet", [(r * 1.16, 0.0), (r * 1.16, 0.035), (r * 1.08, 0.08), (r * 0.7, 0.15), (r * 0.2, 0.2), (0.02, 0.24), (0.0, 0.25)],
                     steel, Vector((c.x, c.y, base)), scale=(1, 1.06, 1), uv_scale=(2, 1))]
    if mail is not None:
        parts.append(revolve("aventail", [(r * 1.17, 0.005), (r * 1.25, -0.06), (r * 1.45, -0.13)], mail, Vector((c.x, c.y + 0.012, base)),
                             scale=(1, 1.05, 1), uv_scale=(4, 1)))
    for p in parts:
        bone_attach(h, p, "head")
    return parts


def hand_frame(h, side="R"):
    """Точка хвата в кулаке и оси кисти в позе покоя: along — к пальцам, grip — ось древка (от мизинца к большому)."""
    wr = h.world_head(f"wrist.{side}")
    mid = h.world_head(f"finger3-1.{side}")
    thumb = h.world_head(f"finger1-2.{side}")
    along = (mid - wr).normalized()
    grip = thumb - wr
    grip = (grip - along * grip.dot(along)).normalized()
    return wr + along * 0.6 * (mid - wr).length, along, grip


def hold(h, o, side="R", roll=0.0, slide=0.0):
    """Вложить предмет в руку: у предмета хват в начале координат, древко по +Z. roll — поворот вокруг древка."""
    p, along, grip = hand_frame(h, side)
    z = grip
    x = along
    y = z.cross(x)
    m = Matrix((x, y, z)).transposed().to_4x4() @ Matrix.Rotation(roll, 4, "Z")
    m.translation = p + z * slide
    o.matrix_world = m @ o.matrix_world
    return bone_attach(h, o, f"wrist.{side}")


def join(objs, name):
    """Склеить детали предмета в один объект (у всех — общее начало координат первого)."""
    ctx = bpy.context
    for o in ctx.view_layer.objects.selected:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    ctx.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    objs[0].name = name
    return objs[0]


def scan_item(asset, part, scale, rot=(0, 0, 0)):
    """Предмет-скан (топор, щит): копия части, начало координат — в центре её габаритов."""
    o = S.place(asset, part, (0, 0, 0), scale=scale, ground=False)
    o.rotation_euler = rot
    bpy.context.view_layer.update()
    bb = [o.matrix_world @ Vector(c) for c in o.bound_box]
    mid = sum(bb, Vector()) / 8
    o.location -= mid
    bpy.context.view_layer.update()
    me = o.data.copy()
    me.transform(o.matrix_world)
    o.data = me
    o.matrix_world = Matrix.Identity(4)
    return o


def round_shield(radius, wood, iron, seed=1):
    """Круглый щит из досок с железной оковкой и умбоном; лицевая сторона — к +Y, центр в начале координат."""
    import random
    rnd = random.Random(seed)
    parts = []
    n = 6
    w = 2 * radius / n
    for i in range(n):
        x = -radius + (i + 0.5) * w
        L = 2 * math.sqrt(max(0.0, radius ** 2 - x ** 2)) * 0.98
        m = Matrix(((0, 1, 0), (0, 0, 1), (1, 0, 0))).to_4x4()  # доска стоймя: длина по Z, ширина по X, толщина по Y
        m.translation = (x, 0, 0)
        o = S.board((L, w * 0.97, 0.018), m, wood, rnd.randrange(1 << 30))
        parts.append(o)
    # ровный круглый край: обрезаем доски цилиндром (Boolean) и надеваем обод
    cut = revolve("cut", [(radius, -0.05), (radius, 0.05)], iron, Vector((0, 0, 0)), segs=32)
    cut.rotation_euler = (math.pi / 2, 0, 0)
    bpy.context.view_layer.update()
    for o in parts:
        md = o.modifiers.new("cut", "BOOLEAN")
        md.operation = "INTERSECT"
        md.object = cut
        bpy.context.view_layer.objects.active = o
        bpy.ops.object.modifier_apply(modifier=md.name)
    bpy.data.objects.remove(cut, do_unlink=True)
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=False, segments=32, radius1=radius + 0.006, radius2=radius + 0.006, depth=0.03)
    rim = _mesh_obj("rim", bm, iron)
    rim.rotation_euler = (math.pi / 2, 0, 0)
    boss = revolve("boss", [(0.07, 0.0), (0.065, 0.025), (0.03, 0.045), (0.0, 0.05)], iron, Vector((0, 0, 0)))
    boss.rotation_euler = (-math.pi / 2, 0, 0)
    parts += [rim, boss]
    bpy.context.view_layer.update()
    for o in parts:
        me = o.data.copy()
        me.transform(o.matrix_world)
        o.data = me
        o.matrix_world = Matrix.Identity(4)
    return join(parts, "shield")


def strap(h, shield, side="L", out=0.07):
    """Надеть щит на предплечье: лицевая сторона наружу, верх щита — к кисти."""
    a = h.world_head(f"lowerarm01.{side}")
    b = h.world_head(f"wrist.{side}")
    f = (b - a).normalized()
    lat = Vector((1 if side == "L" else -1, 0, 0))
    n = (lat - f * lat.dot(f)).normalized()
    x = n.cross(f)
    m = Matrix((x, n, f)).transposed().to_4x4()
    m.translation = (a + b) / 2 + n * out
    shield.matrix_world = m @ shield.matrix_world
    return bone_attach(h, shield, f"lowerarm02.{side}")


# ---------- оружие (собирается из простых частей; хват — в начале координат, древко по +Z, «вперёд» — +X) ----------
def _prism_obj(name, pts, depth, mat, m=Matrix.Identity(4)):
    """Плоская деталь (лезвие): многоугольник pts [(x, z)] толщиной depth по Y."""
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    front = [bm.verts.new((x, -depth / 2, z)) for x, z in pts]
    back = [bm.verts.new((x, depth / 2, z)) for x, z in pts]
    bm.faces.new(front[::-1])
    bm.faces.new(back)
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        bm.faces.new((front[i], front[j], back[j], back[i]))
    for f in bm.faces:
        for loop in f.loops:
            loop[uvl].uv = (loop.vert.co.x * 3, loop.vert.co.z * 3)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.transform(m)
    o = _mesh_obj(name, bm, mat)
    for p in o.data.polygons:
        p.use_smooth = False
    return o


def shaft(length, r, wood, z0=0.0, seed=3, r_end=None):
    return S.log((0, 0, z0), (0, 0, z0 + length), r, wood, seed, r_end=r_end, sag=0.0, segs=8)


def axe(wood, steel, length=0.75):
    """Боевой топор: топорище и бородовидное лезвие лезвием вперёд (+X)."""
    parts = [shaft(length, 0.018, wood, z0=-0.18)]
    top = length - 0.24
    parts.append(_prism_obj("axe_head", [(-0.03, top - 0.03), (0.05, top - 0.02), (0.15, top - 0.11), (0.19, top - 0.1), (0.19, top + 0.08),
                                          (0.1, top + 0.07), (0.05, top + 0.035), (-0.03, top + 0.04)], 0.022, steel))
    return join(parts, "axe")


def sabre(steel, grip):
    blade = _prism_obj("blade", [(-0.016, 0.09), (0.016, 0.09), (0.03, 0.55), (0.06, 0.82), (0.035, 0.84), (0.0, 0.56)], 0.006, steel)
    hilt = _prism_obj("guard", [(-0.05, 0.075), (0.05, 0.075), (0.05, 0.095), (-0.05, 0.095)], 0.02, steel)
    handle = shaft(0.16, 0.015, grip, z0=-0.07)
    return join([blade, hilt, handle], "sabre")


def spear(wood, steel, length=2.1):
    parts = [shaft(length, 0.016, wood, z0=-0.9)]
    tip = length - 0.9
    parts.append(revolve("tip", [(0.02, 0.0), (0.035, 0.05), (0.028, 0.18), (0.0, 0.3)], steel, Vector((0, 0, tip)), segs=6, scale=(1.0, 0.35, 1.0)))
    return join(parts, "spear")


def bow(wood, string):
    """Лук: дуга из двух плеч (изогнутые бруски) и тетива. Хват в центре, плечи по ±Z, выгиб — вперёд (+X)."""
    parts = []
    n = 8
    for sgn in (1, -1):
        for i in range(n):
            a0, a1 = i / n, (i + 1) / n
            z0, z1 = sgn * 0.65 * a0, sgn * 0.65 * a1
            x0, x1 = 0.12 * (a0 ** 2) * -1 + 0.04, 0.12 * (a1 ** 2) * -1 + 0.04
            parts.append(S.log((x0, 0, z0), (x1, 0, z1), 0.016 * (1 - 0.4 * a0), wood, 7 + i, r_end=0.016 * (1 - 0.4 * a1), sag=0.0,
                               segs=6, ends=False))
    parts.append(S.log((-0.08, 0, 0.65), (-0.08, 0, -0.65), 0.003, string, 1, sag=0.0, segs=4, ends=False))
    return join(parts, "bow")


def musket(wood, steel, length=1.5):
    """Пищаль: ложе и ствол. Хват правой — у шейки приклада; ствол вперёд по +Z."""
    stock = _prism_obj("stock", [(-0.05, -0.32), (0.03, -0.3), (0.025, -0.05), (0.02, 0.2), (0.01, 0.75), (-0.02, 0.75), (-0.02, 0.1),
                                 (-0.03, -0.08), (-0.075, -0.3)], 0.045, wood)
    barrel = S.log((0.0, 0, 0.0), (0.0, 0, length - 0.32), 0.014, steel, 2, sag=0.0, segs=8)
    barrel.location.x = 0.012
    return join([stock, barrel], "musket")


def berdysh(wood, steel):
    parts = [shaft(1.6, 0.017, wood, z0=-0.8)]
    parts.append(_prism_obj("berdysh_blade", [(0.02, 0.35), (0.06, 0.3), (0.2, 0.42), (0.24, 0.62), (0.22, 0.85), (0.14, 0.95), (0.08, 0.72),
                                              (0.02, 0.6)], 0.01, steel))
    return join(parts, "berdysh")


def quiver(leather, wood):
    q = revolve("quiver", [(0.05, 0.0), (0.055, 0.25), (0.06, 0.5), (0.0, 0.5)], leather, Vector((0, 0, 0)), segs=10)
    arrows = [S.log((math.cos(a) * 0.025, math.sin(a) * 0.025, 0.45), (math.cos(a) * 0.03, math.sin(a) * 0.03, 0.62), 0.004, wood, i, sag=0.0,
                    segs=4) for i, a in enumerate((0.3, 1.9, 3.6, 5.0))]
    return join([q] + arrows, "quiver")


def on_back(h, o, bone="spine02", tilt=0.5, offset=(0.0, 0.16, 0.0)):
    """Колчан/бердыш за спиной: наклон по диагонали спины."""
    p = h.world_head(bone) + Vector(offset)
    m = Matrix.Translation(p) @ Matrix.Rotation(tilt, 4, "Y")
    o.matrix_world = m @ o.matrix_world
    return bone_attach(h, o, bone)


def hood(h, cloth):
    """Капюшон: колокол над головой, свисающий на плечи."""
    c, r, crown = head_frame(h)
    o = revolve("hood", [(0.0, 0.035), (r * 0.9, 0.02), (r * 1.25, -0.06), (r * 1.32, -0.16), (r * 1.6, -0.27)], cloth,
                Vector((c.x, c.y + 0.015, crown)), scale=(1, 1.1, 1), uv_scale=(3, 1))
    # открыть лицо: срезаем переднюю нижнюю часть
    bm = bmesh.new()
    bm.from_mesh(o.data)
    dead = [v for v in bm.verts if v.co.y < c.y - r * 0.55 and v.co.z < crown - 0.03 and v.co.z > crown - 0.2]
    bmesh.ops.delete(bm, geom=dead, context="VERTS")
    bm.to_mesh(o.data)
    bm.free()
    bpy.context.scene.collection.objects.unlink(o)
    bpy.context.scene.collection.objects.link(o)
    transfer_weights(h, o, ["head", "neck01", "neck02", "neck03"] + SPINE)
    return _attach(h, o)


def cape(h, cloth, length=0.95):
    """Плащ (корзно): от плеч до колен по спине, веса — от спины и плеч."""
    top = h.world_head("neck01").z - 0.02
    c, rx, ry = body_section(h, top - 0.06, SPINE)
    me = bpy.data.meshes.new("cape")
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVMap")
    rows, segs = 8, 14
    grid = []
    for r in range(rows + 1):
        t = r / rows
        z = top - length * t
        row = []
        for s in range(segs + 1):
            a = math.pi * (0.05 + 0.9 * s / segs)  # задняя полуокружность (+Y — спина)
            w = (rx + 0.06) * (1 + 0.5 * t)
            row.append(bm.verts.new((c.x + math.cos(a) * w, c.y + 0.04 + math.sin(a) * (ry + 0.07) * (1 + 0.3 * t), z)))
        grid.append(row)
    for r in range(rows):
        for s in range(segs):
            f = bm.faces.new((grid[r][s], grid[r][s + 1], grid[r + 1][s + 1], grid[r + 1][s]))
            f.smooth = True
            for loop, (rr, ss) in zip(f.loops, ((r, s), (r, s + 1), (r + 1, s + 1), (r + 1, s))):
                loop[uvl].uv = (ss / segs * 2, rr / rows * 2)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(cloth)
    o = bpy.data.objects.new("cape", me)
    bpy.context.scene.collection.objects.link(o)
    transfer_weights(h, o, SPINE + UPPER_LEGS)
    return _attach(h, o)


def torch(wood, fire, rag):
    """Факел: палка, намотанная просмолённая тряпка и пламя (светящееся)."""
    stick = shaft(0.62, 0.016, wood, z0=-0.2)
    wrap = revolve("rag", [(0.03, 0.34), (0.036, 0.38), (0.034, 0.44), (0.0, 0.45)], rag, Vector((0, 0, 0)), segs=10)
    flame = revolve("flame", [(0.045, 0.43), (0.05, 0.5), (0.03, 0.58), (0.0, 0.66)], fire, Vector((0, 0, 0)), segs=10)
    return join([stick, wrap, flame], "torch")


def club(wood, iron):
    """Кистень/палица: рукоять и окованная железом головка."""
    handle = shaft(0.55, 0.017, wood, z0=-0.15)
    head = revolve("club_head", [(0.025, 0.3), (0.05, 0.34), (0.055, 0.4), (0.0, 0.44)], iron, Vector((0, 0, 0)), segs=10)
    return join([handle, head], "club")


def emissive(name, color, strength=6.0):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Emission Color"].default_value = (*color, 1)
    b.inputs["Emission Strength"].default_value = strength
    return m


def kite_shield(paint=None):
    """Каплевидный щит (скан). paint — материал лицевой доски вместо исходной росписи (червлёный щит)."""
    parts = S.place_group("kite_shield", (0, 0, 0), scale=0.85)
    if paint is not None:
        for o in parts:
            if o.name.startswith("kite_shield_body"):
                o.data = o.data.copy()
                o.data.materials.clear()
                o.data.materials.append(paint)
    bpy.context.view_layer.update()
    for o in parts:
        me = o.data.copy()
        me.transform(o.matrix_world)
        o.data = me
        o.matrix_world = Matrix.Identity(4)
    o = join(parts, "kite")
    bb = [Vector(c) for c in o.bound_box]
    mid = sum(bb, Vector()) / 8
    o.data.transform(Matrix.Rotation(math.pi, 4, "Z") @ Matrix.Translation(-mid))  # лицевая сторона — к +Y, как у круглого
    return o
