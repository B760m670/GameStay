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

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "source", "polyhaven")

_LIB = {}  # имя ассета -> {имя части: объект-образец}


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
        o.parent = None
        o.matrix_world = Matrix.Identity(4)
    parts = {o.name: o for o in new if o.type == "MESH"}
    _LIB[asset] = parts
    return parts


def reset():
    _LIB.clear()


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
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    tex_dir = os.path.join(ROOT, asset, "textures")
    files = {f.split(asset + "_")[1].rsplit("_", 1)[0]: os.path.join(tex_dir, f) for f in os.listdir(tex_dir)}
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
