"""Реалистичные детали из CC0-сканов Poly Haven (art/fetch_polyhaven.py): модели (камни, стволы, пни, папоротник,
молодые ели, трава), PBR-материалы из фототекстур и стены с неровно обрушенным верхом.

Масштаб: клетка игры ≈ 2,5 реальных метра, поэтому сканы обычно уменьшаются в 0,3–0,5 раза.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import toon_kit as T

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "source", "polyhaven")

_LIB = {}  # имя ассета -> {имя части: объект-образец}
_ORIG = {}  # имя части -> её матрица в исходной сборке glTF


def _library(asset):
    """Импортирует glTF ассета один раз в скрытую коллекцию; возвращает его части по именам."""
    if asset in _LIB:
        return _LIB[asset]
    path = os.path.join(ROOT, asset, asset + ".gltf")
    if not os.path.exists(path):
        raise SystemExit(f"нет ассета {asset}: python3 art/fetch_polyhaven.py 1k {asset}")
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    lib = bpy.data.collections.get("ScanLibrary") or bpy.data.collections.new("ScanLibrary")
    for o in new:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        lib.objects.link(o)
        o.hide_render = True
        _ORIG[o.name] = o.matrix_world.copy()  # исходное положение части в сборке (для place_group)
        o.parent = None
        o.matrix_world = Matrix.Identity(4)
    parts = {o.name: o for o in new if o.type == "MESH"}
    if T.on():
        T.restyle_scan(asset, list(parts.values()))
    _LIB[asset] = parts
    return parts


def reset():
    _LIB.clear()
    _ORIG.clear()


def place(asset, part, loc, scale=1.0, rot_z=0.0, tilt=(0.0, 0.0), ground=True):
    """Копия части скана (общие данные меша). ground — поставить нижней точкой на землю (z = loc.z)."""
    parts = _library(asset)
    src = next((o for n, o in parts.items() if n.startswith(part)), None)
    if src is None:
        raise SystemExit(f"в {asset} нет части {part}: {sorted(parts)}")
    o = src.copy()
    bpy.context.scene.collection.objects.link(o)
    o.hide_render = False
    o.rotation_euler = (tilt[0], tilt[1], rot_z)
    o.scale = (scale, scale, scale)
    o.location = loc
    bpy.context.view_layer.update()
    if ground:
        zmin = min((o.matrix_world @ Vector(c)).z for c in o.bound_box)
        o.location.z += loc[2] - zmin
    return o


def pbr(asset, tile_m=1.0, name=None):
    """PBR-материал из фототекстуры ассета: цвет, нормали, шероховатость (и AO из ARM, если есть).
    Проекция «коробкой» по координатам объекта без швов; tile_m — сколько единиц игры на один повтор текстуры."""
    name = name or f"pbr_{asset}"
    if T.on() and asset in T.SOLID:
        return T.solid("toon_" + name, **T.SOLID[asset])
    if T.on() and asset in T.WOOD:
        return T.wood("toon_" + name, T.WOOD[asset])
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    tex_dir = os.path.join(ROOT, asset, "textures")
    files = {f.split(asset + "_")[1].rsplit("_", 1)[0]: os.path.join(tex_dir, f) for f in os.listdir(tex_dir)}
    for alias in ("albedo", "col"):  # у части ассетов карта цвета названа иначе
        if alias in files and "diff" not in files:
            files["diff"] = files[alias]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    bsdf = N["Principled BSDF"]
    co = N.new("ShaderNodeTexCoord")
    mp = N.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1 / tile_m, 1 / tile_m, 1 / tile_m)
    L.new(co.outputs["Object"], mp.inputs["Vector"])

    def image(key, colorspace):
        t = N.new("ShaderNodeTexImage")
        t.image = bpy.data.images.load(files[key], check_existing=True)
        t.image.colorspace_settings.name = colorspace
        t.projection = "BOX"
        t.projection_blend = 0.25
        L.new(mp.outputs["Vector"], t.inputs["Vector"])
        return t

    diff = image("diff", "sRGB")
    color = diff.outputs["Color"]
    if "arm" in files:
        arm = image("arm", "Non-Color")
        sep = N.new("ShaderNodeSeparateColor")
        L.new(arm.outputs["Color"], sep.inputs["Color"])
        ao = N.new("ShaderNodeMix")
        ao.data_type = "RGBA"
        ao.blend_type = "MULTIPLY"
        ao.inputs["Factor"].default_value = 0.8
        L.new(color, ao.inputs["A"])
        L.new(sep.outputs["Red"], ao.inputs["B"])
        color = ao.outputs["Result"]
        L.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
    elif "rough" in files:
        L.new(image("rough", "Non-Color").outputs["Color"], bsdf.inputs["Roughness"])
    L.new(color, bsdf.inputs["Base Color"])
    if "nor_gl" in files:
        nm = N.new("ShaderNodeNormalMap")
        nm.inputs["Strength"].default_value = 1.2
        L.new(image("nor_gl", "Non-Color").outputs["Color"], nm.inputs["Color"])
        L.new(nm.outputs["Normal"], bsdf.inputs["Normal"])
    return m


def ruined_wall(p0, p1, thickness, height, mat, seed, segments=24, jag=0.12):
    """Стена от p0 до p1 (в плане) толщиной thickness; height(t) — высота уцелевшей части в долях пути t∈[0,1].
    Верх рваный: высота и толщина гуляют, камни «выкрошены»."""
    rnd = random.Random(seed)
    # крупные обломы: плавная случайная кривая по 6 опорным точкам + мелкая выкрошка
    knots = [rnd.uniform(-1, 1) for _ in range(7)]

    def broken(t):
        x = t * 6
        i = min(5, int(x))
        f = x - i
        f = f * f * (3 - 2 * f)
        return knots[i] * (1 - f) + knots[i + 1] * f
    p0, p1 = Vector((p0[0], p0[1], 0)), Vector((p1[0], p1[1], 0))
    d = (p1 - p0)
    length = d.length
    u = d.normalized()
    n = Vector((-u.y, u.x, 0))
    bm = bmesh.new()
    cols = []
    for i in range(segments + 1):
        t = i / segments
        h = max(0.03, height(t) + broken(t) * jag * 1.6 + rnd.uniform(-jag, jag) * 0.35)
        base = p0 + u * (t * length)
        tf, tb = thickness / 2 * rnd.uniform(0.85, 1.1), thickness / 2 * rnd.uniform(0.85, 1.1)
        cols.append([bm.verts.new(base + n * tf), bm.verts.new(base + n * tf * rnd.uniform(0.8, 1.0) + Vector((0, 0, h))),
                     bm.verts.new(base - n * tb * rnd.uniform(0.8, 1.0) + Vector((0, 0, h * rnd.uniform(0.9, 1.0)))),
                     bm.verts.new(base - n * tb)])
    for a, b in zip(cols, cols[1:]):
        bm.faces.new((a[0], b[0], b[1], a[1]))  # лицевая
        bm.faces.new((a[1], b[1], b[2], a[2]))  # верх
        bm.faces.new((a[2], b[2], b[3], a[3]))  # тыльная
    bm.faces.new(cols[0][::-1])
    bm.faces.new(cols[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=2, use_grid_fill=True)
    me = bpy.data.meshes.new("wall")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("wall", me)
    o.data.materials.append(mat)
    bpy.context.scene.collection.objects.link(o)
    # неровная поверхность кладки
    tex = bpy.data.textures.new("wall_disp", "CLOUDS")
    tex.noise_scale = 0.25
    md = o.modifiers.new("Displace", "DISPLACE")
    md.texture = tex
    md.strength = 0.04
    md.texture_coords = "OBJECT"
    return o


# ---------- брёвна и доски с фототекстурой вдоль волокон ----------
# Текстуры досок Poly Haven: доски вертикальные, волокна идут по оси V. Для бревна или доски берём одну полосу шириной
# в доску (без стыков) — так у каждого бревна свой рисунок. Центры досок по оси U измерены по стыкам на текстуре.
GRAIN = {
    "weathered_planks": {"centers": [0.117, 0.229, 0.344, 0.459, 0.573, 0.688, 0.803, 0.917], "width": 0.08, "len_m": 0.9},
    "wood_planks_grey": {"centers": [0.1, 0.3, 0.5, 0.7, 0.9], "width": 0.14, "len_m": 1.1},
}


def grain_mat(asset, vary=0.12, name=None, moss=0.0):
    """PBR по UV-развёртке (её задают log/board); vary — разброс яркости между брёвнами;
    moss — доля поверхности под мхом и лишайником (пятнами, по мировым координатам: старый тёс, нижние венцы)."""
    name = name or f"grain_{asset}"
    if T.on():
        m = T.wood("toon_" + name, T.ROOF if name == "grain_tes" else T.WOOD.get(asset, T.WOOD["wood_planks_grey"]), moss=moss * 0.8)
        m["grain"] = asset
        return m
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = pbr(asset, name=name)
    m["grain"] = asset  # какую полосу текстуры брать (GRAIN)
    N, L = m.node_tree.nodes, m.node_tree.links
    co = next(n for n in N if n.type == "TEX_COORD")
    mp = next(n for n in N if n.type == "MAPPING")
    mp.inputs["Scale"].default_value = (1, 1, 1)
    L.new(co.outputs["UV"], mp.inputs["Vector"])
    for n in N:
        if n.type == "TEX_IMAGE":
            n.projection = "FLAT"
    bsdf = N["Principled BSDF"]
    src = bsdf.inputs["Base Color"].links[0].from_socket
    info = N.new("ShaderNodeObjectInfo")
    rng = N.new("ShaderNodeMapRange")
    rng.inputs["To Min"].default_value = 1 - vary
    rng.inputs["To Max"].default_value = 1 + vary * 0.5
    L.new(info.outputs["Random"], rng.inputs["Value"])
    mul = N.new("ShaderNodeMix")
    mul.data_type = "RGBA"
    mul.blend_type = "MULTIPLY"
    mul.inputs["Factor"].default_value = 1.0
    L.new(src, mul.inputs["A"])
    gray = N.new("ShaderNodeCombineColor")
    for k in ("Red", "Green", "Blue"):
        L.new(rng.outputs["Result"], gray.inputs[k])
    L.new(gray.outputs["Color"], mul.inputs["B"])
    color = mul.outputs["Result"]
    if moss > 0:
        geo = N.new("ShaderNodeNewGeometry")
        nz = N.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = 2.2
        nz.inputs["Detail"].default_value = 10.0
        nz.inputs["Roughness"].default_value = 0.65
        L.new(geo.outputs["Position"], nz.inputs["Vector"])
        ramp = N.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.62 - moss * 0.2
        ramp.color_ramp.elements[1].position = 0.7 - moss * 0.2
        L.new(nz.outputs["Fac"], ramp.inputs["Fac"])
        green = N.new("ShaderNodeMix")
        green.data_type = "RGBA"
        green.blend_type = "MULTIPLY"
        green.inputs["Factor"].default_value = 1.0
        green.inputs["B"].default_value = (0.42, 0.62, 0.16, 1)
        L.new(color, green.inputs["A"])
        mix = N.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        L.new(ramp.outputs["Color"], mix.inputs["Factor"])
        L.new(color, mix.inputs["A"])
        L.new(green.outputs["Result"], mix.inputs["B"])
        color = mix.outputs["Result"]
    L.new(color, bsdf.inputs["Base Color"])
    return m


def end_grain_mat():
    """Торец бревна: годичные кольца, трещины, посеревшая древесина. UV торца — круг с центром (0.5, 0.5)."""
    name = "end_grain"
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    bsdf = N["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = 0.9
    co = N.new("ShaderNodeTexCoord")
    noise = N.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 6.0
    noise.inputs["Detail"].default_value = 6.0
    L.new(co.outputs["UV"], noise.inputs["Vector"])
    # расстояние от сердцевины, искажённое шумом — кольца неровные
    sub = N.new("ShaderNodeVectorMath")
    sub.operation = "SUBTRACT"
    sub.inputs[1].default_value = (0.5, 0.5, 0)
    L.new(co.outputs["UV"], sub.inputs[0])
    ln = N.new("ShaderNodeVectorMath")
    ln.operation = "LENGTH"
    L.new(sub.outputs["Vector"], ln.inputs[0])
    warp = N.new("ShaderNodeMath")
    warp.operation = "MULTIPLY_ADD"
    warp.inputs[1].default_value = 0.06
    L.new(noise.outputs["Fac"], warp.inputs[0])
    L.new(ln.outputs["Value"], warp.inputs[2])
    rings = N.new("ShaderNodeMath")
    rings.operation = "SINE"
    scale = N.new("ShaderNodeMath")
    scale.operation = "MULTIPLY"
    scale.inputs[1].default_value = 120.0
    L.new(warp.outputs["Value"], scale.inputs[0])
    L.new(scale.outputs["Value"], rings.inputs[0])
    ramp = N.new("ShaderNodeValToRGB")
    if T.on():  # светлый свежий срез с тёплыми кольцами
        ramp.color_ramp.elements[0].color = T.lin((198, 150, 96))
        ramp.color_ramp.elements[1].color = T.lin((232, 192, 134))
    else:
        ramp.color_ramp.elements[0].color = (0.16, 0.13, 0.10, 1)
        ramp.color_ramp.elements[1].color = (0.30, 0.26, 0.21, 1)
    L.new(rings.outputs["Value"], ramp.inputs["Fac"])
    # трещины-лучи и потемневший край
    crack = N.new("ShaderNodeTexVoronoi")
    crack.feature = "DISTANCE_TO_EDGE"
    crack.inputs["Scale"].default_value = 5.0
    L.new(co.outputs["UV"], crack.inputs["Vector"])
    ck = N.new("ShaderNodeMapRange")
    ck.inputs["From Min"].default_value = 0.0
    ck.inputs["From Max"].default_value = 0.025
    ck.inputs["To Min"].default_value = 0.45
    L.new(crack.outputs["Distance"], ck.inputs["Value"])
    edge = N.new("ShaderNodeMapRange")
    edge.inputs["From Min"].default_value = 0.5
    edge.inputs["From Max"].default_value = 0.36
    edge.inputs["To Min"].default_value = 0.5
    L.new(ln.outputs["Value"], edge.inputs["Value"])
    m1 = N.new("ShaderNodeMix")
    m1.data_type = "RGBA"
    m1.blend_type = "MULTIPLY"
    m1.inputs["Factor"].default_value = 1.0
    L.new(ramp.outputs["Color"], m1.inputs["A"])
    L.new(ck.outputs["Result"], m1.inputs["B"])
    m2 = N.new("ShaderNodeMix")
    m2.data_type = "RGBA"
    m2.blend_type = "MULTIPLY"
    m2.inputs["Factor"].default_value = 1.0
    L.new(m1.outputs["Result"], m2.inputs["A"])
    L.new(edge.outputs["Result"], m2.inputs["B"])
    L.new(m2.outputs["Result"], bsdf.inputs["Base Color"])
    return m


def _uv_strip(asset, rnd):
    g = GRAIN[asset]
    c = rnd.choice(g["centers"])
    return c - g["width"] / 2, g["width"], g["len_m"], rnd.uniform(0, 1)


def log(p0, p1, r, mat, seed, r_end=None, sag=0.01, ends=True, segs=14, name="log"):
    """Бревно от p0 до p1 радиусом r (у вершинного конца r_end): слегка неровное и кривое, кора снята.
    Волокна текстуры идут вдоль бревна; торцы — кольца (end_grain_mat)."""
    rnd = random.Random(seed)
    asset = mat.get("grain", "wood_planks_grey")  # у не-древесных материалов UV всё равно нужна
    u0, uw, len_m, v0 = _uv_strip(asset, rnd)
    p0, p1 = Vector(p0), Vector(p1)
    axis = p1 - p0
    length = axis.length
    w = axis.normalized()
    helper = Vector((0, 0, 1)) if abs(w.z) < 0.9 else Vector((1, 0, 0))
    a = w.cross(helper).normalized()
    b = w.cross(a).normalized()
    r_end = r if r_end is None else r_end
    nl = max(2, int(length / 0.25))
    bumps = [rnd.uniform(-1, 1) for _ in range(nl + 1)]
    bend = rnd.uniform(-1, 1) * sag
    phase = rnd.uniform(0, 6.28)
    bm = bmesh.new()
    rings = []
    for i in range(nl + 1):
        t = i / nl
        rr = (r + (r_end - r) * t) * (1 + 0.035 * bumps[i])
        c = p0 + axis * t + b * (bend * math.sin(math.pi * t))
        ring = []
        for j in range(segs):
            ang = 2 * math.pi * j / segs + phase
            k = 1 + 0.025 * math.sin(3 * ang + bumps[i] * 2)  # бревно не идеально круглое
            ring.append(bm.verts.new(c + (a * math.cos(ang) + b * math.sin(ang)) * rr * k))
        rings.append(ring)
    uv = bm.loops.layers.uv.new("UVMap")
    for i in range(nl):
        for j in range(segs):
            jn = (j + 1) % segs
            f = bm.faces.new((rings[i][j], rings[i][jn], rings[i + 1][jn], rings[i + 1][j]))
            f.smooth = True
            for loop, (ii, jj) in zip(f.loops, ((i, j), (i, j + 1), (i + 1, j + 1), (i + 1, j))):
                loop[uv].uv = (u0 + uw * jj / segs, v0 + ii / nl * length / len_m)
    if ends:
        for ring, rev in ((rings[0], True), (rings[-1], False)):
            f = bm.faces.new(ring[::-1] if rev else ring)
            f.material_index = 1
            cx, cy = rnd.uniform(0.45, 0.55), rnd.uniform(0.45, 0.55)
            for loop in f.loops:
                ang = 2 * math.pi * ring.index(loop.vert) / segs + phase
                loop[uv].uv = (cx + 0.5 * math.cos(ang), cy + 0.5 * math.sin(ang))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    o.data.materials.append(mat)
    o.data.materials.append(end_grain_mat())
    bpy.context.scene.collection.objects.link(o)
    return o


def board(size, matrix, mat, seed, name="board"):
    """Доска size=(длина по X, ширина по Y, толщина по Z) с волокнами вдоль длины; matrix — положение и поворот."""
    rnd = random.Random(seed)
    asset = mat.get("grain", "wood_planks_grey")  # у не-древесных материалов UV всё равно нужна
    u0, uw, len_m, v0 = _uv_strip(asset, rnd)
    sx, sy, sz = size
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * sx, v.co.y * sy, v.co.z * sz))
    uv = bm.loops.layers.uv.new("UVMap")
    for f in bm.faces:
        n = f.normal
        for loop in f.loops:
            co = loop.vert.co
            across = co.y / sy + 0.5 if abs(n.z) > 0.5 else co.z / max(sz, 1e-4) + 0.5
            along = co.x if abs(n.x) < 0.5 else co.y
            loop[uv].uv = (u0 + uw * across, v0 + along / len_m)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    o.matrix_world = matrix
    o.data.materials.append(mat)
    bpy.context.scene.collection.objects.link(o)
    md = o.modifiers.new("Bevel", "BEVEL")
    md.width = min(0.006, sz * 0.4)
    md.segments = 1
    return o


def stone_block(size, loc, mat, seed, disp=0.03):
    """Каменный массив (фундамент, ступень): подразделённый брус с неровной поверхностью кладки."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=8, use_grid_fill=True)
    me = bpy.data.meshes.new("stone")
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("stone", me)
    o.location = loc
    o.data.materials.append(mat)
    bpy.context.scene.collection.objects.link(o)
    tex = bpy.data.textures.new(f"stone_disp{seed}", "CLOUDS")
    tex.noise_scale = 0.2
    md = o.modifiers.new("Displace", "DISPLACE")
    md.texture = tex
    md.strength = disp
    md.texture_coords = "OBJECT"
    md.mid_level = 0.5
    bev = o.modifiers.new("Bevel", "BEVEL")
    bev.width = 0.02
    bev.segments = 2
    o.modifiers.move(1, 0)
    return o


def place_group(asset, loc, scale=1.0, rot_z=0.0, parts=None):
    """Все части скана (ведро с дужкой, ящик с крышкой) с общим положением; нижняя точка — на земле (z = loc.z)."""
    lib = _library(asset)
    objs = []
    for n, src in lib.items():
        if parts and not any(n.startswith(p) for p in parts):
            continue
        o = src.copy()
        bpy.context.scene.collection.objects.link(o)
        o.hide_render = False
        o.matrix_world = (Matrix.Translation(loc) @ Matrix.Rotation(rot_z, 4, "Z") @ Matrix.Diagonal((scale, scale, scale, 1))
                          @ _ORIG.get(n, Matrix.Identity(4)))
        objs.append(o)
    bpy.context.view_layer.update()
    zmin = min((o.matrix_world @ Vector(c)).z for o in objs for c in o.bound_box)
    for o in objs:
        o.location.z += loc[2] - zmin
    return objs
