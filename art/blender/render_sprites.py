"""Пререндер спрайтов (путь Clash of Clans 2012): 3D-модели -> 2D-спрайты в изометрии 2:1.

Запуск:
  blender -b --factory-startup -P art/blender/render_sprites.py -- --assets /opt/assets --out art/out [--only lantern,ballista]

Сопоставление осей: игровая клетка (x, y) = Blender (X, -Y). Камера смотрит с угла +X/-Y,
так что экранное «вправо-вниз» = +x игры, «влево-вниз» = +y игры — как в изометрии клиента.
"""
import json
import math
import os
import sys

import bpy
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import recipes  # noqa: E402

TILE_PX = 128                  # ширина ромба клетки в пикселях спрайта (2x для Retina)
ELEV = math.atan(0.5)          # 26.565° — честная изометрия 2:1
PPU = TILE_PX / math.sqrt(2)   # пикселей на метр по горизонтали экрана
PAD = 24


def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opts = {"assets": "/opt/assets", "out": "art/out", "only": None, "samples": 48}
    i = 0
    while i < len(a):
        k = a[i].lstrip("-")
        opts[k] = a[i + 1]
        i += 2
    if opts["only"]:
        opts["only"] = set(opts["only"].split(","))
    opts["samples"] = int(opts["samples"])
    return opts


def reset_scene(samples):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    sc.render.film_transparent = True
    sc.render.image_settings.file_format = "PNG"
    sc.render.image_settings.color_mode = "RGBA"
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.look = "None"
    world = bpy.data.worlds.new("World")
    sc.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes["Background"]
    bg.inputs[0].default_value = (0.62, 0.68, 0.85, 1.0)   # холодный небесный амбиент
    bg.inputs[1].default_value = 0.12                      # слабый: иначе ловец теней ловит «небесную» тень по всему кадру
    # Заполняющий свет без теней — вместо сильного неба.
    for name, rot, energy, color in [("FillTop", (0, 0, 0), 1.6, (0.75, 0.8, 1.0)),
                                     ("FillBack", (math.radians(60), 0, math.radians(150)), 0.9, (0.7, 0.75, 1.0))]:
        ld = bpy.data.lights.new(name, "SUN")
        ld.energy = energy
        ld.color = color
        ld.cycles.cast_shadow = False
        lo = bpy.data.objects.new(name, ld)
        lo.rotation_euler = rot
        sc.collection.objects.link(lo)
    # Солнце сверху-слева (как в CoC): тени падают вправо-вниз по экрану.
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 3.2
    sun_data.angle = math.radians(8)
    sun_data.color = (1.0, 0.95, 0.86)
    sun = bpy.data.objects.new("Sun", sun_data)
    sun.rotation_euler = (math.radians(30), 0, math.radians(-25))
    sc.collection.objects.link(sun)
    # Плоскость-ловец теней: тень попадает в прозрачный PNG.
    bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0))
    plane = bpy.context.active_object
    plane.name = "ShadowCatcher"
    plane.is_shadow_catcher = True
    return sc


def import_part(path, loc, rot_z, scale):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=path)
    new = [o for o in bpy.data.objects if o not in before]
    roots = [o for o in new if o.parent is None]
    for r in roots:
        r.rotation_mode = "XYZ"
        r.rotation_euler.z += math.radians(rot_z)
        r.scale = Vector(r.scale) * scale
        r.location = Vector(r.location) * scale + Vector(loc)
    bpy.context.view_layer.update()
    return new


def bounds(objs):
    mn = Vector((1e9, 1e9, 1e9))
    mx = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        if o.type != "MESH":
            continue
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c)
            mn = Vector(map(min, mn, w))
            mx = Vector(map(max, mx, w))
    return mn, mx


def bounds_evaluated(objs):
    """Габариты по деформированной геометрии (для анимированных персонажей bound_box врёт)."""
    dg = bpy.context.evaluated_depsgraph_get()
    mn = Vector((1e9, 1e9, 1e9))
    mx = Vector((-1e9, -1e9, -1e9))
    for o in objs:
        if o.type != "MESH":
            continue
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        for v in me.vertices:
            w = ev.matrix_world @ v.co
            mn = Vector(map(min, mn, w))
            mx = Vector(map(max, mx, w))
        ev.to_mesh_clear()
    return mn, mx


def emission_material(name, color, strength):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    em = nt.nodes.new("ShaderNodeEmission")
    em.inputs[0].default_value = (*color, 1)
    em.inputs[1].default_value = strength
    nt.links.new(em.outputs[0], out.inputs[0])
    return m


TURRETS = []  # корневые объекты поворотных частей последней собранной постройки


def build(recipe, assets):
    """Собирает модель по рецепту, нормирует под размер клетки. Возвращает (объекты, высота в метрах)."""
    objs = []
    top = 0.0
    for p in recipe["parts"]:
        kit, name = p["kit"], p["model"]
        path = os.path.join(assets, recipes.KITS[kit], name + ".glb")
        x, y = p.get("at", (0, 0))   # координаты Blender; камера смотрит с +X/-Y
        z = p.get("z", 0)
        if z == "top":
            z = top
        new = import_part(path, (x, y, z), p.get("rot", 0), p.get("scale", 1))
        if p.get("turret"):
            TURRETS.extend(o for o in new if o.parent is None)
        objs += new
        _, mx = bounds(new)
        top = max(top, mx.z)
    # Подогнать по площади клетки
    mn, mx = bounds(objs)
    ext = max(mx.x - mn.x, mx.y - mn.y)
    s = recipe["size"] * recipe.get("fill", 0.88) / ext
    cx, cy = (mn.x + mx.x) / 2, (mn.y + mx.y) / 2
    for o in objs:
        if o.parent is None:
            o.location = (Vector(o.location) - Vector((cx, cy, mn.z))) * s
            o.scale = Vector(o.scale) * s
    bpy.context.view_layer.update()
    # Свечения (огонь Фонаря, кристаллы) — в координатах уже нормированной модели, в долях высоты
    _, mx = bounds(objs)
    height = mx.z
    for g in recipe.get("glow", []):
        bpy.ops.mesh.primitive_uv_sphere_add(radius=g["r"], location=(g.get("x", 0), g.get("y", 0), g["h"] * height))
        sph = bpy.context.active_object
        sph.data.materials.append(emission_material("Glow", g["color"], g.get("strength", 8)))
        light = bpy.data.lights.new("GlowLight", "POINT")
        light.energy = g.get("light", 60)
        light.color = g["color"]
        lo = bpy.data.objects.new("GlowLight", light)
        lo.location = sph.location
        bpy.context.scene.collection.objects.link(lo)
        objs.append(sph)
    return objs, height


def setup_camera(size_tiles, height_m, extra_w=0):
    sc = bpy.context.scene
    shadow = int(0.45 * height_m * PPU)                       # запас под тень от солнца
    w = int(size_tiles * TILE_PX + 2 * PAD + 2 * shadow + extra_w)
    h_px = height_m * math.cos(ELEV) * PPU
    hgt = int(size_tiles * TILE_PX / 2 + h_px + 2 * PAD + shadow)
    sc.render.resolution_x = w
    sc.render.resolution_y = hgt
    sc.render.resolution_percentage = 100
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.sensor_fit = "HORIZONTAL"
    cam_data.ortho_scale = w / PPU
    cam = bpy.data.objects.new("Cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.rotation_euler = (math.pi / 2 - ELEV, 0, math.radians(45))
    # Точка, на которую смотрит центр кадра: подбираем высоту так, чтобы ромб клетки был внизу кадра.
    pivot_y_from_bottom = PAD + shadow + size_tiles * TILE_PX / 4
    up_px = hgt / 2 - pivot_y_from_bottom                 # насколько центр кадра выше центра клетки
    target = Vector((0, 0, up_px / PPU / math.cos(ELEV)))
    fwd = Vector((0, 0, -1))
    fwd.rotate(cam.rotation_euler)
    cam.location = target - fwd * 50
    bpy.context.view_layer.update()
    p = world_to_camera_view(sc, cam, Vector((0, 0, 0)))
    return cam, (p.x * w, (1 - p.y) * hgt)


def clean_alpha(path, floor=0.07):
    """Ловец теней оставляет почти прозрачную «подложку» по всему кадру — срезаем её."""
    import numpy as np
    img = bpy.data.images.load(path)
    px = np.empty(len(img.pixels), dtype=np.float32)
    img.pixels.foreach_get(px)
    a = px[3::4]
    px[3::4] = np.where(a < floor, 0.0, np.minimum(1.0, (a - floor) / (1 - floor)))
    img.pixels.foreach_set(px)
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def render_building(name, recipe, opts):
    sc = reset_scene(opts["samples"])
    TURRETS.clear()
    objs, height = build(recipe, opts["assets"])
    cam, pivot = setup_camera(recipe["size"], height)
    os.makedirs(os.path.join(opts["out"], "buildings"), exist_ok=True)
    meta = {"size": recipe["size"], "pivot": [round(pivot[0], 1), round(pivot[1], 1)], "tilePx": TILE_PX,
            "w": sc.render.resolution_x, "h": sc.render.resolution_y}
    # основной кадр + 8 направлений поворотной части (d — поворот на 45° против часовой, вид сверху)
    variants = [("", 0)] + ([(f"_d{d}", d * 45) for d in range(8)] if TURRETS else [])
    base_rot = [r.rotation_euler.z for r in TURRETS]
    for suffix, deg in variants:
        for r, b0 in zip(TURRETS, base_rot):
            r.rotation_euler.z = b0 + math.radians(deg)
        bpy.context.view_layer.update()
        out = os.path.join(opts["out"], "buildings", name + suffix + ".png")
        sc.render.filepath = out
        bpy.ops.render.render(write_still=True)
        clean_alpha(out)
        with open(out.replace(".png", ".json"), "w") as f:
            json.dump(meta, f)
    print(f"[sprites] {name}: {meta}")


def pbr(name, color, metallic=0.0, rough=0.55, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes.get("Principled BSDF")
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    if emit:
        b.inputs["Emission Color"].default_value = (*color, 1)
        b.inputs["Emission Strength"].default_value = emit
    return m


def _prim(kind, mat, loc=(0, 0, 0), scale=(1, 1, 1), rot=(0, 0, 0), **kw):
    getattr(bpy.ops.mesh, "primitive_" + kind + "_add")(location=loc, rotation=rot, **kw)
    o = bpy.context.active_object
    o.scale = scale
    o.data.materials.append(mat)
    bpy.ops.object.shade_smooth() if kind in ("uv_sphere", "cylinder", "torus") else None
    return o


def _curve(points, radius, mat):
    cd = bpy.data.curves.new("crv", "CURVE")
    cd.dimensions = "3D"
    cd.bevel_depth = radius
    cd.bevel_resolution = 2
    sp = cd.splines.new("POLY")
    sp.points.add(len(points) - 1)
    for i, p in enumerate(points):
        sp.points[i].co = (*p, 1)
    o = bpy.data.objects.new("crv", cd)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(mat)
    return o


METAL = (0.78, 0.8, 0.86)
WOOD = (0.42, 0.26, 0.13)


def make_item(kind, assets, color=None):
    """Предмет в руку. Начало координат — точка хвата, предмет тянется вдоль +Z. Возвращает корневой объект."""
    parts = []
    if kind == "sword" or kind == "dagger":
        L = 0.16 if kind == "sword" else 0.09
        steel, hilt = pbr("Steel", METAL, 0.9, 0.25), pbr("Hilt", color or (0.55, 0.36, 0.12), 0.3, 0.5)
        parts += [_prim("cube", steel, (0, 0, 0.03 + L / 2), (0.016, 0.005, L / 2)),
                  _prim("cube", hilt, (0, 0, 0.026), (0.034, 0.008, 0.006)),
                  _prim("cylinder", pbr("Grip", WOOD), (0, 0, 0), (0.008, 0.008, 0.024), vertices=8),
                  _prim("uv_sphere", hilt, (0, 0, -0.026), (0.011, 0.011, 0.011), segments=8, ring_count=6)]
    elif kind == "shield":
        face = pbr("ShieldFace", color or (0.16, 0.32, 0.72), 0.1, 0.5)
        rim = pbr("ShieldRim", (0.82, 0.62, 0.2), 0.85, 0.3)
        parts += [_prim("cylinder", face, (0, 0, 0), (0.095, 0.095, 0.009), (math.pi / 2, 0, 0), vertices=24),
                  _prim("torus", rim, (0, 0, 0), (1, 1, 1), (math.pi / 2, 0, 0), major_radius=0.095, minor_radius=0.009,
                        major_segments=24, minor_segments=6),
                  _prim("uv_sphere", rim, (0, -0.012, 0), (0.024, 0.016, 0.024), segments=10, ring_count=6)]
    elif kind == "bow":
        wood = pbr("Bow", (0.5, 0.3, 0.12), 0.0, 0.6)
        pts = [(0.05 * (1 - math.cos(a)) * 1.2, 0, 0.17 * math.sin(a)) for a in [(-1 + 2 * i / 10) * 1.0 for i in range(11)]]
        parts += [_curve([(x - 0.03, y, z) for x, y, z in pts], 0.012, wood),
                  _curve([(pts[0][0] - 0.03, 0, pts[0][2]), (pts[-1][0] - 0.03, 0, pts[-1][2])], 0.0025, pbr("Str", (0.95, 0.92, 0.85)))]
    elif kind in ("staff", "darkstaff"):
        c = color or ((0.35, 0.85, 1.0) if kind == "staff" else (0.7, 0.3, 1.0))
        parts += [_prim("cylinder", pbr("Staff", WOOD), (0, 0, 0.08), (0.009, 0.009, 0.2), vertices=8),
                  _prim("uv_sphere", pbr("Orb", c, 0, 0.2, emit=2.5), (0, 0, 0.3), (0.03, 0.03, 0.03), segments=12, ring_count=8)]
        ld = bpy.data.lights.new("OrbLight", "POINT")
        ld.energy, ld.color, ld.shadow_soft_size = 4, c, 0.02
        lo = bpy.data.objects.new("OrbLight", ld)
        lo.location = (0, 0, 0.3)
        bpy.context.scene.collection.objects.link(lo)
        parts.append(lo)
    elif kind == "lanternstaff":
        parts.append(_prim("cylinder", pbr("Staff", WOOD), (0, 0, 0.12), (0.009, 0.009, 0.2), vertices=8))
        parts.append(_prim("cube", pbr("Hook", (0.25, 0.22, 0.2), 0.7, 0.4), (0, 0, 0.31), (0.035, 0.006, 0.006)))
        new = import_part(os.path.join(assets, recipes.KITS["grave"], "lantern-candle.glb"), (0, 0, 0), 0, 0.38)
        tint_all(new, (0.42, 0.34, 0.26))
        for o in new:
            if o.parent is None:
                o.location = (0.03, 0, 0.17)
        parts += [o for o in new if o.parent is None]
        parts.append(_prim("uv_sphere", pbr("Flame", (1.0, 0.6, 0.18), 0, 0.3, emit=4), (0.03, 0, 0.235), (0.02, 0.02, 0.03), segments=8, ring_count=6))
        ld = bpy.data.lights.new("HeroLight", "POINT")
        ld.energy, ld.color, ld.shadow_soft_size = 8, (1.0, 0.65, 0.3), 0.02
        lo = bpy.data.objects.new("HeroLight", ld)
        lo.location = (0.03, 0, 0.235)
        bpy.context.scene.collection.objects.link(lo)
        parts.append(lo)
    elif kind == "lantern":
        new = import_part(os.path.join(assets, recipes.KITS["grave"], "lantern-candle.glb"), (0, 0, 0), 0, 0.42)
        for o in new:
            if o.parent is None:
                o.location.z -= 0.16
        parts += [o for o in new if o.parent is None]
        parts.append(_prim("uv_sphere", pbr("Flame", (1.0, 0.55, 0.15), 0, 0.3, emit=4), (0, 0, -0.085), (0.02, 0.02, 0.03), segments=8, ring_count=6))
        ld = bpy.data.lights.new("LanternLight", "POINT")
        ld.energy, ld.color, ld.shadow_soft_size = 6, (1.0, 0.6, 0.25), 0.02
        lo = bpy.data.objects.new("LanternLight", ld)
        lo.location = (0, 0, -0.085)
        bpy.context.scene.collection.objects.link(lo)
        parts.append(lo)
    elif kind == "torch":
        parts += [_prim("cylinder", pbr("TorchStick", WOOD), (0, 0, 0.05), (0.011, 0.011, 0.11), vertices=8),
                  _prim("cylinder", pbr("TorchWrap", (0.3, 0.2, 0.12)), (0, 0, 0.15), (0.017, 0.017, 0.025), vertices=8),
                  _prim("uv_sphere", pbr("TorchFire", (1.0, 0.45, 0.08), 0, 0.3, emit=3), (0, 0, 0.19), (0.026, 0.026, 0.04), segments=10, ring_count=6),
                  _prim("uv_sphere", pbr("TorchCore", (1.0, 0.8, 0.3), 0, 0.3, emit=5), (0, 0, 0.18), (0.014, 0.014, 0.024), segments=8, ring_count=6)]
        ld = bpy.data.lights.new("TorchLight", "POINT")
        ld.energy, ld.color, ld.shadow_soft_size = 5, (1.0, 0.55, 0.2), 0.02
        lo = bpy.data.objects.new("TorchLight", ld)
        lo.location = (0, 0, 0.2)
        bpy.context.scene.collection.objects.link(lo)
        parts.append(lo)
    elif kind == "bomb":
        parts += [_prim("uv_sphere", pbr("Bomb", (0.08, 0.08, 0.1), 0.6, 0.35), (0, 0, 0.03), (0.045, 0.045, 0.045), segments=14, ring_count=8),
                  _prim("cylinder", pbr("Fuse", (0.8, 0.7, 0.5)), (0, 0, 0.085), (0.005, 0.005, 0.015), vertices=6),
                  _prim("uv_sphere", pbr("Spark", (1, 0.7, 0.2), 0, 0.3, emit=5), (0, 0, 0.1), (0.012, 0.012, 0.012), segments=6, ring_count=4)]
    else:  # предмет из набора: "survival:tool-hammer"
        kit, model = kind.split(":")
        new = import_part(os.path.join(assets, recipes.KITS[kit], model + ".glb"), (0, 0, 0), 0, 1)
        for o in new:
            if o.parent is None:
                o.location.z -= 0.03
        parts += [o for o in new if o.parent is None]
    root = bpy.data.objects.new("Item_" + kind, None)
    bpy.context.scene.collection.objects.link(root)
    for p in parts:
        p.parent = root
    return root


def hand_pos(target, bone):
    """Кисть в позе покоя: самые дальние от плеча вершины руки (кость скелета или жёсткая деталь)."""
    pts = []
    if target.type == "ARMATURE":
        for o in target.children:
            if o.type != "MESH" or bone not in o.vertex_groups:
                continue
            gi = o.vertex_groups[bone].index
            for v in o.data.vertices:
                if any(g.group == gi and g.weight > 0.5 for g in v.groups):
                    pts.append(o.matrix_world @ v.co)
    elif target.type == "MESH":
        pts = [target.matrix_world @ v.co for v in target.data.vertices]
    if not pts:
        return Vector((-0.36 if "right" in bone else 0.36, -0.03, 0.27))
    sign = -1 if "right" in bone else 1
    ext = max(p.x * sign for p in pts)
    near = [p for p in pts if p.x * sign > ext - 0.04]
    c = sum(near, Vector()) / len(near)
    return Vector((c.x - sign * 0.01, c.y, c.z))


def attach_items(new, holds, assets):
    """Вложить предметы в руки. Персонаж в позе покоя (Т-поза Kenney: руки в стороны, вперёд = -Y).
    Скелетные персонажи — привязка к кости, «жёсткие» (кладбищенские) — к детали руки."""
    from mathutils import Euler, Matrix
    arm = next((o for o in new if o.type == "ARMATURE"), None)
    if arm is not None:
        arm.data.pose_position = "REST"
    bpy.context.view_layer.update()
    out = []
    for h in holds:
        bone = h.get("bone", "arm-right")
        root = make_item(h["item"], assets, h.get("color"))
        if arm is not None:
            target = arm
            root.parent = arm
            root.parent_type = "BONE"
            root.parent_bone = bone
        else:
            target = next(o for o in new if o.name.split(".")[0] == bone)
            root.parent = target
        bpy.context.view_layer.update()
        rot = Euler([math.radians(v) for v in h.get("rot", (90, 0, 0))])
        sc = h.get("scale", 1)
        loc = Vector(h["loc"]) if "loc" in h else hand_pos(target, bone) + Vector(h.get("off", (0, 0, 0)))
        root.matrix_world = Matrix.LocRotScale(loc, rot, Vector((sc, sc, sc)))
        out.append(root)
    if arm is not None:
        arm.data.pose_position = "POSE"
    bpy.context.view_layer.update()
    return out


def set_anim(new, src):
    """Включить анимацию src. Возвращает (начало, конец) диапазона кадров или None."""
    rng = None
    for o in new:
        if o.type == "ARMATURE":
            act = next((a for a in bpy.data.actions if a.name.split("_")[0] == src), None)
        else:
            act = bpy.data.actions.get(src + "_" + o.name.split(".")[0])
        if act is None:
            continue
        if o.animation_data is None:
            o.animation_data_create()
        o.animation_data.action = act
        r = act.frame_range
        rng = (min(rng[0], r[0]), max(rng[1], r[1])) if rng else (r[0], r[1])
    return rng

def render_unit(name, recipe, opts):
    """Юнит: 8 направлений × кадры анимаций. Модель импортируется один раз на направление."""
    assets = opts["assets"]
    out_dir = os.path.join(opts["out"], "units", name)
    os.makedirs(out_dir, exist_ok=True)
    scale = None
    pivot = None
    for d in range(recipe.get("dirs", 8)):
        sc = reset_scene(opts["samples"])
        path = os.path.join(assets, recipes.KITS[recipe["kit"]], recipe["model"] + ".glb")
        new = import_part(path, (0, 0, 0), 0, 1)
        arm = next((o for o in new if o.type == "ARMATURE"), None)
        if arm is not None:
            # в персонажах Kenney лежит служебная сфера без родителя — убираем
            stray = [o for o in new if o.type == "MESH" and o.parent is None]
            new = [o for o in new if o not in stray]
            for o in stray:
                bpy.data.objects.remove(o, do_unlink=True)
        set_anim(new, "static")
        attach_items(new, recipe.get("hold", []), assets)
        roots = [o for o in new if o.parent is None]
        for r in roots:
            r.rotation_mode = "XYZ"
            r.rotation_euler.z = math.radians(45 - d * 45 + recipe.get("yaw", 0))
        for mat_name, color in recipe.get("tint", {}).items():
            m = bpy.data.materials.get(mat_name)
            if m and m.use_nodes:
                bsdf = m.node_tree.nodes.get("Principled BSDF")
                if bsdf:
                    bsdf.inputs["Base Color"].default_value = (*color, 1)
        bpy.context.view_layer.update()
        if scale is None:
            mn, mx = bounds_evaluated(new)
            scale = (recipe["height"] / max(0.01, mx.z - mn.z), mn.z)
        for r in roots:
            r.scale = Vector(r.scale) * scale[0]
            r.location = (Vector(r.location) - Vector((0, 0, scale[1]))) * scale[0] + Vector((0, 0, recipe.get("lift", 0)))
        bpy.context.view_layer.update()
        cam, pivot = setup_camera(1, recipe["height"] * 1.15 + recipe.get("lift", 0))
        for anim, (src, frames) in recipe["anims"].items():
            rng = set_anim(new, src)
            for fi, frame in enumerate(frames):
                if rng is not None:
                    f = rng[0] + (rng[1] - rng[0]) * frame
                    sc.frame_set(int(f), subframe=f - int(f))
                out = os.path.join(out_dir, f"{anim}_{d}_{fi}.png")
                sc.render.filepath = out
                bpy.ops.render.render(write_still=True)
                clean_alpha(out)
    with open(os.path.join(out_dir, "meta.json"), "w") as f:
        json.dump({"pivot": [round(pivot[0], 1), round(pivot[1], 1)], "anims": {k: len(v[1]) for k, v in recipe["anims"].items()},
                   "dirs": recipe.get("dirs", 8)}, f)
    print(f"[sprites] unit {name} done")


def tint_all(objs, color, emission=0.0):
    """Перекрасить материалы: текстура -> обесцветить -> умножить на цвет (тени и блики сохраняются)."""
    done = set()
    for o in objs:
        for slot in getattr(o, "material_slots", []):
            m = slot.material
            if not m or m.name in done or not m.use_nodes:
                continue
            done.add(m.name)
            nt = m.node_tree
            b = nt.nodes.get("Principled BSDF")
            if not b:
                continue
            inp = b.inputs["Base Color"]
            mul = nt.nodes.new("ShaderNodeMix")
            mul.data_type = "RGBA"
            mul.blend_type = "MULTIPLY"
            mul.inputs["Factor"].default_value = 1.0
            mul.inputs[7].default_value = (color[0] * 1.15, color[1] * 1.15, color[2] * 1.15, 1)
            if inp.is_linked:
                src = inp.links[0].from_socket
                hs = nt.nodes.new("ShaderNodeHueSaturation")
                hs.inputs["Saturation"].default_value = 0.0
                hs.inputs["Value"].default_value = 1.0
                nt.links.new(src, hs.inputs["Color"])
                nt.links.new(hs.outputs["Color"], mul.inputs[6])
            else:
                v = inp.default_value
                g = 0.3 * v[0] + 0.59 * v[1] + 0.11 * v[2]
                mul.inputs[6].default_value = (0.4 + g, 0.4 + g, 0.4 + g, 1)
            nt.links.new(mul.outputs[2], inp)
            if emission:
                b.inputs["Emission Color"].default_value = (*color, 1)
                b.inputs["Emission Strength"].default_value = emission


def render_icon(name, recipe, opts):
    """Иконка UI: модель крупным планом 3/4, прозрачный фон, без теней на земле."""
    sc = reset_scene(opts["samples"])
    bpy.data.objects.remove(bpy.data.objects["ShadowCatcher"], do_unlink=True)
    objs = []
    for p in recipe["parts"]:
        path = os.path.join(opts["assets"], recipes.KITS[p["kit"]], p["model"] + ".glb")
        new = import_part(path, (p["at"][0], p["at"][1], p["z"] if p["z"] != "top" else 0), p.get("rot", 0), p.get("scale", 1))
        if "tint" in p:
            tint_all(new, p["tint"], p.get("emit", 0))
        objs += new
    mn, mx = bounds(objs)
    size = 256
    sc.render.resolution_x = sc.render.resolution_y = size
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("Cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.rotation_euler = (math.radians(62), 0, math.radians(recipe.get("yaw", 30)))
    center = (mn + mx) / 2
    fwd = Vector((0, 0, -1))
    fwd.rotate(cam.rotation_euler)
    cam.location = center - fwd * 30
    bpy.context.view_layer.update()
    # подогнать масштаб по проекции габаритов
    corners = [Vector((x, y, z)) for x in (mn.x, mx.x) for y in (mn.y, mx.y) for z in (mn.z, mx.z)]
    inv = cam.matrix_world.inverted()
    pts = [inv @ c for c in corners]
    w = max(p.x for p in pts) - min(p.x for p in pts)
    h = max(p.y for p in pts) - min(p.y for p in pts)
    cam_data.ortho_scale = max(w, h) * 1.12
    out = os.path.join(opts["out"], "icons", name + ".png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print(f"[sprites] icon {name}")


def main():
    opts = args()
    for name, r in getattr(recipes, "ICONS", {}).items():
        if opts["only"] and name not in opts["only"]:
            continue
        render_icon(name, r, opts)
    for name, r in recipes.BUILDINGS.items():
        if opts["only"] and name not in opts["only"]:
            continue
        render_building(name, r, opts)
    for name, r in recipes.UNITS.items():
        if opts["only"] and name not in opts["only"]:
            continue
        render_unit(name, r, opts)


if __name__ == "__main__":
    main()
