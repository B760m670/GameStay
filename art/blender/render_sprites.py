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
    img = bpy.data.images.load(path)
    px = list(img.pixels)
    for i in range(3, len(px), 4):
        a = px[i]
        px[i] = 0.0 if a < floor else min(1.0, (a - floor) / (1 - floor))
    img.pixels[:] = px
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def render_building(name, recipe, opts):
    sc = reset_scene(opts["samples"])
    objs, height = build(recipe, opts["assets"])
    cam, pivot = setup_camera(recipe["size"], height)
    out = os.path.join(opts["out"], "buildings", name + ".png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    clean_alpha(out)
    meta = {"size": recipe["size"], "pivot": [round(pivot[0], 1), round(pivot[1], 1)], "tilePx": TILE_PX,
            "w": sc.render.resolution_x, "h": sc.render.resolution_y}
    with open(out.replace(".png", ".json"), "w") as f:
        json.dump(meta, f)
    print(f"[sprites] {name}: {meta}")


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
            arm.animation_data_create()
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
        for anim, frames in recipe["anims"].items():
            act = next((a for a in bpy.data.actions if a.name.split("_")[0] == anim), None)
            if arm is not None and act is not None:
                arm.animation_data.action = act
            for fi, frame in enumerate(frames):
                if act is not None:
                    sc.frame_set(int(act.frame_range[0] + (act.frame_range[1] - act.frame_range[0]) * frame))
                out = os.path.join(out_dir, f"{anim}_{d}_{fi}.png")
                sc.render.filepath = out
                bpy.ops.render.render(write_still=True)
                clean_alpha(out)
    with open(os.path.join(out_dir, "meta.json"), "w") as f:
        json.dump({"pivot": [round(pivot[0], 1), round(pivot[1], 1)], "anims": {k: len(v) for k, v in recipe["anims"].items()},
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


main()
