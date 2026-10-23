"""Набор для сборки построек из частей в Blender (без генерации): материалы, брусья, камни, черепица, окна, двери.

Модель здания — функция Python, которая расставляет части; уровни улучшения — та же модель с дополнительными частями.
Камера, солнце и ловец теней — общие с render_sprites.py, так что спрайты встают в игру в том же масштабе (128 px на клетку).
Координаты: метр = клетка. Камера смотрит с угла (+X, -Y): видны грань -Y (слева на экране) и грань +X (справа).
"""
import math
import random

import bpy
import bmesh
from mathutils import Matrix, Vector


def lin(c):
    """sRGB 0..255 -> линейный цвет Blender."""
    return tuple((v / 255) ** 2.2 for v in c[:3]) + (1.0,)


# ---------- материалы ----------
_MATS = {}


def material(name, colors, rough=0.65, metal=0.0, kind="noise", scale=6.0, bump=0.25, emit=0.0, per_part=True):
    """colors — палитра sRGB. При per_part цвет каждой части берётся из палитры по Object Info.Random (камни,
    черепица, доски — разные). Поверх — процедурная фактура: noise (штукатурка, камень), wave (дерево) или none."""
    if name in _MATS:
        return _MATS[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    bsdf = N["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = rough
    bsdf.inputs["Metallic"].default_value = metal
    ramp = N.new("ShaderNodeValToRGB")
    els = ramp.color_ramp.elements
    els[0].color = lin(colors[0])
    els[1].color = lin(colors[-1])
    for i, c in enumerate(colors[1:-1], 1):
        els.new(i / (len(colors) - 1)).color = lin(c)
    if per_part:
        L.new(N.new("ShaderNodeObjectInfo").outputs["Random"], ramp.inputs["Fac"])
    else:
        ramp.inputs["Fac"].default_value = 0.5
    color = ramp.outputs["Color"]
    if kind != "none":
        if kind == "brick":  # каменная кладка на крупных поверхностях (башня, труба)
            tex = N.new("ShaderNodeTexBrick")
            tex.inputs["Color1"].default_value = lin(colors[0])
            tex.inputs["Color2"].default_value = lin(colors[-1])
            tex.inputs["Mortar"].default_value = lin((70, 64, 60))
            tex.inputs["Mortar Size"].default_value = 0.025
            tex.inputs["Brick Width"].default_value = 0.6
            tex.inputs["Row Height"].default_value = 0.3
            tex.offset = 0.5
        elif kind == "wave":
            tex = N.new("ShaderNodeTexWave")
            tex.bands_direction = "X"
            tex.inputs["Distortion"].default_value = 6.0
            tex.inputs["Detail"].default_value = 3.0
        else:
            tex = N.new("ShaderNodeTexNoise")
            tex.inputs["Detail"].default_value = 6.0
            tex.inputs["Roughness"].default_value = 0.6
        tex.inputs["Scale"].default_value = scale
        obj_co = N.new("ShaderNodeTexCoord").outputs["Object"]
        if kind == "brick":
            sep = N.new("ShaderNodeSeparateXYZ")
            add = N.new("ShaderNodeMath")
            comb = N.new("ShaderNodeCombineXYZ")
            L.new(obj_co, sep.inputs[0])
            L.new(sep.outputs["X"], add.inputs[0])
            L.new(sep.outputs["Y"], add.inputs[1])
            L.new(add.outputs[0], comb.inputs["X"])
            L.new(sep.outputs["Z"], comb.inputs["Y"])
            L.new(comb.outputs[0], tex.inputs["Vector"])
        else:
            L.new(obj_co, tex.inputs["Vector"])
        mix = N.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "OVERLAY"
        mix.inputs["Factor"].default_value = 0.9 if kind == "brick" else 0.35
        L.new(color, mix.inputs["A"])
        L.new(tex.outputs["Color"] if kind == "brick" else tex.outputs["Fac"], mix.inputs["B"])
        color = mix.outputs["Result"]
        if bump > 0:
            b = N.new("ShaderNodeBump")
            b.inputs["Strength"].default_value = bump
            b.inputs["Distance"].default_value = 0.02
            L.new(tex.outputs["Fac"], b.inputs["Height"])
            L.new(b.outputs["Normal"], bsdf.inputs["Normal"])
    L.new(color, bsdf.inputs["Base Color"])
    if emit > 0:
        L.new(color, bsdf.inputs["Emission Color"])
        bsdf.inputs["Emission Strength"].default_value = emit
    _MATS[name] = m
    return m


def palette():
    """Общая палитра построек: тёплая и насыщенная, как в рисованных стратегиях."""
    global _CUBE
    _MATS.clear()
    _CUBE = None  # сцена сброшена: прежние материалы и заготовка удалены вместе с ней
    return {
        "plaster": material("plaster", [(246, 230, 196), (236, 214, 176)], rough=0.9, scale=14, bump=0.15),
        "timber": material("timber", [(104, 62, 36), (88, 52, 30), (118, 72, 40)], rough=0.7, kind="wave", scale=3, bump=0.35),
        "plank": material("plank", [(150, 98, 56), (134, 86, 48), (166, 110, 62)], rough=0.7, kind="wave", scale=3, bump=0.3),
        "stone": material("stone", [(156, 150, 142), (136, 132, 126), (170, 164, 152), (124, 120, 116)], rough=0.85, scale=9, bump=0.5),
        "masonry": material("masonry", [(170, 164, 152), (140, 136, 130)], rough=0.85, kind="brick", scale=3.5, bump=0.6),
        "stone_dark": material("stone_dark", [(112, 108, 106), (98, 94, 92)], rough=0.9, scale=9, bump=0.4),
        "tile": material("tile", [(200, 76, 46), (184, 66, 40), (214, 92, 54), (172, 60, 38)], rough=0.55, scale=10, bump=0.3),
        "tile_dark": material("tile_dark", [(150, 50, 34), (136, 44, 30)], rough=0.6, scale=10, bump=0.2),
        "slate": material("slate", [(72, 84, 112), (62, 74, 100), (82, 94, 124)], rough=0.5, scale=10, bump=0.3),
        "glass": material("glass", [(255, 196, 108), (255, 214, 140)], rough=0.3, kind="none", emit=2.6, per_part=False),
        "shutter": material("shutter", [(62, 112, 92), (54, 100, 82)], rough=0.6, kind="wave", scale=4, bump=0.2),
        "iron": material("iron", [(58, 56, 62), (48, 46, 52)], rough=0.4, metal=0.85, kind="none"),
        "gold": material("gold", [(240, 186, 70), (226, 168, 56)], rough=0.3, metal=0.9, kind="none"),
        "cloth_red": material("cloth_red", [(196, 46, 46), (178, 38, 40)], rough=0.8, scale=20, bump=0.1),
        "straw": material("straw", [(226, 186, 92), (210, 168, 76), (236, 200, 110)], rough=0.9, kind="wave", scale=12, bump=0.4),
        "leaf": material("leaf", [(84, 150, 58), (70, 132, 50), (100, 168, 66)], rough=0.8, scale=18, bump=0.2),
        "flower": material("flower", [(236, 66, 60), (250, 196, 60), (246, 236, 220), (236, 120, 50)], rough=0.7, kind="none"),
        "sack": material("sack", [(196, 166, 120), (182, 152, 108)], rough=0.95, scale=16, bump=0.2),
    }


# ---------- геометрия ----------
_CUBE = None


def _unit_cube():
    global _CUBE
    if _CUBE is None:
        _CUBE = bpy.data.meshes.new("unit_cube")
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        bm.to_mesh(_CUBE)
        bm.free()
    return _CUBE


def _finish(obj, mat, bevel, segments=2):
    obj.data.materials.append(mat)
    bpy.context.scene.collection.objects.link(obj)
    if bevel > 0:
        md = obj.modifiers.new("Bevel", "BEVEL")
        md.width = bevel
        md.segments = segments
        md.limit_method = "ANGLE"
    return obj


def box(size, mat, loc=(0, 0, 0), rot=(0, 0, 0), matrix=None, bevel=0.015, name="box"):
    """Брусок size=(x, y, z) метров с фаской. Размер «запекается» в вершины, поэтому фаска одинакова на всех брусках.
    matrix — готовая матрица положения и поворота (иначе loc и rot)."""
    me = _unit_cube().copy()
    me.transform(Matrix.Diagonal(Vector(size).to_4d()))
    obj = bpy.data.objects.new(name, me)
    if matrix is not None:
        obj.matrix_world = matrix
    else:
        obj.location = loc
        obj.rotation_euler = rot
    return _finish(obj, mat, min(bevel, min(size) * 0.45))


def cylinder(radius, depth, mat, loc=(0, 0, 0), rot=(0, 0, 0), radius2=None, verts=20, bevel=0.01, name="cyl"):
    """Цилиндр или усечённый конус (radius2) по локальной Z."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=verts, radius1=radius,
                          radius2=radius if radius2 is None else radius2, depth=depth)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new(name, me)
    obj.location = loc
    obj.rotation_euler = rot
    return _finish(obj, mat, bevel)


def prism(points, depth, mat, matrix, bevel=0.01, name="prism"):
    """Многоугольник points=[(x, z), ...] в локальной плоскости XZ толщиной depth по локальной Y."""
    bm = bmesh.new()
    front = [bm.verts.new((x, -depth / 2, z)) for x, z in points]
    back = [bm.verts.new((x, depth / 2, z)) for x, z in points]
    bm.faces.new(front[::-1])
    bm.faces.new(back)
    for i in range(len(points)):
        j = (i + 1) % len(points)
        bm.faces.new((front[i], front[j], back[j], back[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    obj = bpy.data.objects.new(name, me)
    obj.matrix_world = matrix
    return _finish(obj, mat, bevel)


# ---------- стена как система координат ----------
class Face:
    """Плоскость стены: origin — нижний левый угол снаружи, u — вдоль стены, n — наружу, z — вверх."""

    def __init__(self, origin, u, n, length, height):
        self.o, self.u, self.n = Vector(origin), Vector(u).normalized(), Vector(n).normalized()
        self.z = Vector((0, 0, 1))
        self.L, self.H = length, height

    def matrix(self, u, z, out=0.0, angle=0.0):
        """Положение точки (u, z) стены, вынесенной наружу на out; angle — поворот в плоскости стены."""
        basis = Matrix((self.u, -self.n, self.z)).transposed().to_4x4()  # правая тройка: детали симметричны по толщине
        return Matrix.Translation(self.o + self.u * u + self.z * z + self.n * out) @ basis @ Matrix.Rotation(angle, 4, "Y")

    def point(self, u, z, out=0.0):
        return self.o + self.u * u + self.z * z + self.n * out

    def beam(self, u0, z0, u1, z1, mat, thick=0.1, depth=0.06, out=0.0, bevel=0.012):
        """Брус от (u0, z0) до (u1, z1): горизонтальный, вертикальный или раскос."""
        du, dz = u1 - u0, z1 - z0
        m = self.matrix((u0 + u1) / 2, (z0 + z1) / 2, out + depth / 2, -math.atan2(dz, du))
        return box((math.hypot(du, dz), depth, thick), mat, matrix=m, bevel=bevel)

    def panel(self, u0, u1, z0, z1, depth, mat, out=0.0, bevel=0.01):
        """Прямоугольная пластина на стене."""
        return box((u1 - u0, depth, z1 - z0), mat, matrix=self.matrix((u0 + u1) / 2, (z0 + z1) / 2, out + depth / 2), bevel=bevel)


def window(face, uc, zc, w, h, M, shutters=True):
    """Окно: тёплое стекло, рама, крестовина, подоконник, ставни на кованых петлях."""
    face.panel(uc - w / 2, uc + w / 2, zc - h / 2, zc + h / 2, 0.02, M["glass"], out=-0.005)
    t = 0.06
    face.beam(uc - w / 2 - t, zc + h / 2 + t / 2, uc + w / 2 + t, zc + h / 2 + t / 2, M["timber"], thick=t, depth=0.07)
    face.beam(uc - w / 2 - t, zc - h / 2 - t / 2, uc + w / 2 + t, zc - h / 2 - t / 2, M["timber"], thick=t, depth=0.07)
    face.beam(uc - w / 2 - t / 2, zc - h / 2, uc - w / 2 - t / 2, zc + h / 2, M["timber"], thick=t, depth=0.07)
    face.beam(uc + w / 2 + t / 2, zc - h / 2, uc + w / 2 + t / 2, zc + h / 2, M["timber"], thick=t, depth=0.07)
    face.beam(uc, zc - h / 2, uc, zc + h / 2, M["timber"], thick=0.028, depth=0.04)
    face.beam(uc - w / 2, zc + h * 0.08, uc + w / 2, zc + h * 0.08, M["timber"], thick=0.028, depth=0.04)
    face.panel(uc - w / 2 - 0.09, uc + w / 2 + 0.09, zc - h / 2 - 0.11, zc - h / 2 - 0.06, 0.13, M["plank"])
    if shutters:
        sw = w * 0.5
        for side in (-1, 1):
            u = uc + side * (w / 2 + t + sw / 2 + 0.01)
            face.panel(u - sw / 2, u + sw / 2, zc - h / 2, zc + h / 2, 0.035, M["shutter"], out=0.01)
            for zz in (0.27, -0.27):
                face.beam(u - sw / 2, zc + h * zz, u + sw / 2, zc + h * zz, M["iron"], thick=0.02, depth=0.015, out=0.045)


def door(face, uc, w, h, M, z0=0.0):
    """Дверь из досок с коваными полосами в раме."""
    n = 5
    for i in range(n):
        u = uc - w / 2 + (i + 0.5) * w / n
        face.panel(u - w / n / 2 + 0.004, u + w / n / 2 - 0.004, z0, z0 + h, 0.05, M["plank"], out=-0.01)
    t = 0.08
    face.beam(uc - w / 2 - t / 2, z0, uc - w / 2 - t / 2, z0 + h + t, M["timber"], thick=t, depth=0.09)
    face.beam(uc + w / 2 + t / 2, z0, uc + w / 2 + t / 2, z0 + h + t, M["timber"], thick=t, depth=0.09)
    face.beam(uc - w / 2 - t, z0 + h + t / 2, uc + w / 2 + t, z0 + h + t / 2, M["timber"], thick=t * 1.3, depth=0.1)
    for zz in (0.25, 0.75):
        face.beam(uc - w / 2, z0 + h * zz, uc + w / 2, z0 + h * zz, M["iron"], thick=0.035, depth=0.02, out=0.04)
    p = face.point(uc + w * 0.3, z0 + h * 0.5, 0.07)
    cylinder(0.03, 0.025, M["iron"], loc=p, rot=(math.pi / 2, 0, math.atan2(face.n.y, face.n.x) - math.pi / 2), verts=12, bevel=0)


def stone_course(face, z0, z1, M, seed, rows=2, stone_w=(0.18, 0.32), depth=0.12, out=0.0, mat="stone", extend=0.04):
    """Каменная кладка по грани: ряды со смещением, камни разной длины, толщины и цвета."""
    rnd = random.Random(seed)
    hrow = (z1 - z0) / rows
    for r in range(rows):
        u = -extend - (rnd.uniform(0.05, 0.13) if r % 2 else 0)
        while u < face.L + extend:
            u1 = min(u + rnd.uniform(*stone_w), face.L + extend)
            if u1 - u > 0.06:
                face.panel(max(u, -extend) + 0.01, u1 - 0.01, z0 + r * hrow + 0.01, z0 + (r + 1) * hrow - 0.01,
                           depth + rnd.uniform(-0.015, 0.025), M[mat], out=out, bevel=0.028)
            u = u1


def gable_roof(cx, cy, z_eave, wx, wy, rise, over, M, seed, tile="tile", base="tile_dark"):
    """Двускатная крыша с коньком вдоль X: подложка на оба ската, отдельная черепица на видимом скате (-Y),
    округлый конёк. Возвращает (уклон, вынос свеса по Y)."""
    rnd = random.Random(seed)
    pitch = math.atan2(rise, wy / 2)
    run = wy / 2 + over
    slope = run / math.cos(pitch)
    length = wx + 2 * over
    z_low = z_eave - over * math.tan(pitch)  # высота кромки свеса
    for side in (-1, 1):
        box((length, slope, 0.07), M[base], loc=(cx, cy + side * run / 2, z_low + run * math.tan(pitch) / 2),
            rot=(-side * pitch, 0, 0), bevel=0.01)
    rows = max(4, round(slope / 0.14))
    cols = max(6, round(length / 0.17))
    tw, tl = length / cols, slope / rows * 1.5
    nrm = Vector((0, -math.sin(pitch), math.cos(pitch)))
    for r in range(rows):
        s = (r + 0.5) / rows * slope
        stagger = 0.5 if r % 2 else 0.0
        for c in range(cols + 1):
            x = cx - length / 2 + (c + 0.5 - stagger) * tw
            if x - tw * 0.45 < cx - length / 2 - 0.01 or x + tw * 0.45 > cx + length / 2 + 0.01:
                continue
            p = Vector((x + rnd.uniform(-0.005, 0.005), cy - run + s * math.cos(pitch), z_low + s * math.sin(pitch))) + nrm * 0.05
            o = box((tw * 0.93, tl, 0.06), M[tile], loc=p, rot=(pitch + math.radians(13), 0, rnd.uniform(-0.04, 0.04)), bevel=0.026,
                    name="tile")
            o.modifiers["Bevel"].segments = 3
    # конёк: валик из коротких цилиндров
    n = max(3, round(length / 0.21))
    for i in range(n):
        x = cx - length / 2 + (i + 0.5) * length / n
        cylinder(0.08, length / n * 0.98, M[base], loc=(x, cy, z_eave + rise + 0.05), rot=(0, math.pi / 2, 0), verts=16, bevel=0.01)
    return pitch, run


def gable_wall(face, z_base, rise, M, timber=True):
    """Треугольный фронтон на торце (грань с длиной face.L) от z_base до конька."""
    L = face.L
    prism([(0, z_base), (L, z_base), (L / 2, z_base + rise)], 0.05, M["plaster"], face.matrix(0, 0, -0.025), bevel=0.0)
    if timber:
        face.beam(0, z_base, L / 2, z_base + rise, M["timber"], thick=0.09)
        face.beam(L, z_base, L / 2, z_base + rise, M["timber"], thick=0.09)
        face.beam(L / 2, z_base, L / 2, z_base + rise * 0.82, M["timber"], thick=0.08)


def verge(face, z_low, z_top, over, M):
    """Ветровые доски по краям торца крыши."""
    L = face.L
    face.beam(-over, z_low, L / 2, z_top, M["timber"], thick=0.1, depth=0.07, out=over + 0.02)
    face.beam(L + over, z_low, L / 2, z_top, M["timber"], thick=0.1, depth=0.07, out=over + 0.02)


def outlines(scene, thickness=1.3, color=(0.07, 0.045, 0.035)):
    """Тонкий тёмный контур (Freestyle) по силуэту, как у рисованных спрайтов. Ловец теней без контура."""
    scene.render.use_freestyle = True
    scene.render.line_thickness_mode = "ABSOLUTE"
    scene.render.line_thickness = thickness
    vl = scene.view_layers[0]
    vl.use_freestyle = True
    fs = vl.freestyle_settings
    ls = fs.linesets[0] if fs.linesets else fs.linesets.new("Lines")
    ls.select_by_visibility = True
    ls.select_by_edge_types = True
    ls.select_silhouette = True
    ls.select_border = True
    ls.select_crease = False
    ls.select_by_collection = True
    ls.collection_negation = "EXCLUSIVE"
    ex = bpy.data.collections.new("NoLines")
    scene.collection.children.link(ex)
    ls.collection = ex
    if ls.linestyle is None:
        ls.linestyle = bpy.data.linestyles.new("Outline")
    ls.linestyle.color = color
    ls.linestyle.thickness = thickness
    ls.linestyle.alpha = 0.8
    plane = bpy.data.objects.get("ShadowCatcher")
    if plane:
        for c in list(plane.users_collection):
            c.objects.unlink(plane)
        ex.objects.link(plane)


def sphere(radius, mat, loc, segments=12, rings=8, scale=(1, 1, 1)):
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=segments, v_segments=rings, radius=radius)
    me = bpy.data.meshes.new("sphere")
    bm.to_mesh(me)
    bm.free()
    for p in me.polygons:
        p.use_smooth = True
    obj = bpy.data.objects.new("sphere", me)
    obj.location = loc
    obj.scale = scale
    return _finish(obj, mat, 0)


def flower_box(face, uc, z, w, M, seed):
    """Ящик с цветами под окном: доски, листва, цветки разных цветов."""
    rnd = random.Random(seed)
    face.panel(uc - w / 2, uc + w / 2, z - 0.1, z, 0.12, M["plank"], out=0.0)
    for i in range(int(w / 0.06)):
        u = uc - w / 2 + 0.04 + i * 0.06 + rnd.uniform(-0.01, 0.01)
        sphere(0.045, M["leaf"], face.point(u, z + 0.02, 0.07 + rnd.uniform(-0.02, 0.02)), scale=(1, 1, 0.8))
        if i % 2 == 0:
            sphere(0.024, M["flower"], face.point(u + 0.01, z + 0.06, 0.09 + rnd.uniform(-0.02, 0.01)))


def grass_tuft(M, x, y, seed, n=5, h=0.12):
    """Пучок травы у фундамента: несколько вытянутых конусов."""
    rnd = random.Random(seed)
    for _ in range(n):
        a = rnd.uniform(-0.5, 0.5)
        cylinder(0.022, h * rnd.uniform(0.7, 1.2), M["leaf"], loc=(x + rnd.uniform(-0.05, 0.05), y + rnd.uniform(-0.05, 0.05), h * 0.45),
                 rot=(rnd.uniform(-0.4, 0.4), a, 0), radius2=0.0, verts=6, bevel=0)


def light_rig(scene):
    """Свет для собранных построек: солнце ниже и левее, чем у старых спрайтов, — освещён главный фасад (-Y),
    торец (+X) в мягкой полутени; тени ложатся вправо-назад."""
    sun = bpy.data.objects.get("Sun")
    if sun:
        sun.rotation_euler = (math.radians(42), 0, math.radians(-38))
        sun.data.energy = 3.8
        sun.data.angle = math.radians(9)
    fill = bpy.data.objects.get("FillBack")
    if fill:
        fill.data.energy = 0.6


def grade(path, saturation=1.14, contrast=1.06):
    """Цветокоррекция готового спрайта (насыщенность и контраст, как у рисованных). numpy есть в Python Blender."""
    import numpy as np
    img = bpy.data.images.load(path)
    px = np.empty(len(img.pixels), dtype=np.float32)
    img.pixels.foreach_get(px)
    p = px.reshape(-1, 4)
    rgb = p[:, :3]
    lum = (rgb * np.array([0.299, 0.587, 0.114], np.float32)).sum(1, keepdims=True)
    rgb = lum + (rgb - lum) * saturation
    p[:, :3] = np.clip((rgb - 0.5) * contrast + 0.5, 0, 1)
    img.pixels.foreach_set(p.ravel())
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)
