"""Интерфейс из «материалов»: кнопки, панели, рамки рендерятся в Blender как настоящие 3D-предметы
(эмаль с золотой окантовкой, доски с прожилками и железными уголками, пергамент, ткань, камень).

Запуск:
  blender -b --factory-startup -P art/blender/render_ui.py -- --out art/out [--only btn_gold,panel]

Каждый элемент рендерится спереди ортокамерой на прозрачном фоне в art/out/ui/<имя>.png.
Размеры — в пикселях Retina (2x). Растягиваемые элементы рассчитаны на 9-slice: края с деталями
(уголки, заклёпки, скругления) лежат в полях шириной BORDER[имя], середина однородна.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

PX = 0.01  # 1 пиксель = 0.01 единицы Blender


def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    o = {"out": "art/out", "only": None, "samples": 64}
    for i in range(0, len(a) - 1, 2):
        o[a[i].lstrip("-")] = a[i + 1]
    if o["only"]:
        o["only"] = set(o["only"].split(","))
    o["samples"] = int(o["samples"])
    return o


# ---------- сцена ----------
def reset(samples, w, h):
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
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.render.resolution_percentage = 100
    world = bpy.data.worlds.new("W")
    sc.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = nt.nodes["Background"]
    # «студийное небо»: светлый верх, тёплый низ — металлу и эмали есть что отражать
    grad = nt.nodes.new("ShaderNodeTexGradient")
    coord = nt.nodes.new("ShaderNodeTexCoord")
    mapn = nt.nodes.new("ShaderNodeMapping")
    mapn.inputs["Rotation"].default_value = (0, math.radians(-90), 0)
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.25, 0.18, 0.12, 1)
    ramp.color_ramp.elements[1].color = (1.0, 0.97, 0.92, 1)
    nt.links.new(coord.outputs["Generated"], mapn.inputs["Vector"])
    nt.links.new(mapn.outputs["Vector"], grad.inputs["Vector"])
    nt.links.new(grad.outputs["Fac"], ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 0.45
    for name, loc, energy, size in [("Key", (-3, -5, 6), 380, 4), ("Fill", (4, -4, 1), 90, 5), ("Rim", (0, 3, 6), 160, 3)]:
        ld = bpy.data.lights.new(name, "AREA")
        ld.energy = energy
        ld.size = size
        lo = bpy.data.objects.new(name, ld)
        lo.location = loc
        direction = Vector((0, 0, 0)) - Vector(loc)
        lo.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
        sc.collection.objects.link(lo)
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = max(w, h) * PX
    cam = bpy.data.objects.new("Cam", cam_data)
    cam.location = (0, -10, 0)
    cam.rotation_euler = (math.radians(90), 0, 0)
    sc.collection.objects.link(cam)
    sc.camera = cam
    return sc


# ---------- материалы ----------
def mat(name, color, metallic=0.0, rough=0.5, coat=0.0, emit=0.0):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1)
    b.inputs["Metallic"].default_value = metallic
    b.inputs["Roughness"].default_value = rough
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Coat Roughness"].default_value = 0.05
    if emit:
        b.inputs["Emission Color"].default_value = (*color, 1)
        b.inputs["Emission Strength"].default_value = emit
    return m


def textured(name, c1, c2, kind="wood", scale=4.0, rough=0.6, bump=0.3, metallic=0.0):
    """Процедурная текстура: wood — прожилки, noise — пятна (пергамент, камень)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metallic
    coord = nt.nodes.new("ShaderNodeTexCoord")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (*c1, 1)
    ramp.color_ramp.elements[1].color = (*c2, 1)
    if kind == "wood":
        tex = nt.nodes.new("ShaderNodeTexWave")
        tex.wave_type = "BANDS"
        tex.bands_direction = "Z"   # волокна вдоль X
        tex.inputs["Scale"].default_value = scale
        tex.inputs["Distortion"].default_value = 6
        tex.inputs["Detail"].default_value = 3
        mapn = nt.nodes.new("ShaderNodeMapping")
        mapn.inputs["Scale"].default_value = (0.15, 1, 1)
        nt.links.new(coord.outputs["Object"], mapn.inputs["Vector"])
        nt.links.new(mapn.outputs["Vector"], tex.inputs["Vector"])
        fac = tex.outputs["Fac"]
    else:
        tex = nt.nodes.new("ShaderNodeTexNoise")
        tex.inputs["Scale"].default_value = scale
        tex.inputs["Detail"].default_value = 8
        nt.links.new(coord.outputs["Object"], tex.inputs["Vector"])
        fac = tex.outputs["Fac"]
    nt.links.new(fac, ramp.inputs["Fac"])
    nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    bn = nt.nodes.new("ShaderNodeBump")
    bn.inputs["Strength"].default_value = bump
    nt.links.new(fac, bn.inputs["Height"])
    nt.links.new(bn.outputs["Normal"], b.inputs["Normal"])
    return m


GOLD = lambda: mat("Gold", (1.0, 0.72, 0.28), 1.0, 0.22)
IRON = lambda: mat("Iron", (0.32, 0.32, 0.36), 0.9, 0.42)


# ---------- формы ----------
def rbox(w, h, d, r, material, x=0, y=0, z=0):
    """Брусок со скруглёнными углами в плоскости фронта (радиус r) и мягкой фаской по краю."""
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x * PX, y * PX, z * PX))
    o = bpy.context.active_object
    o.scale = (w * PX, d * PX, h * PX)
    bpy.ops.object.transform_apply(scale=True)
    # скругление вертикальных рёбер (углы фронта): выделяем рёбра вдоль Y
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="DESELECT")
    bpy.ops.object.mode_set(mode="OBJECT")
    for e in o.data.edges:
        v0, v1 = o.data.vertices[e.vertices[0]].co, o.data.vertices[e.vertices[1]].co
        e.select = abs(v0.x - v1.x) < 1e-6 and abs(v0.z - v1.z) < 1e-6
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.bevel(offset=min(r, w / 2 - 1, h / 2 - 1) * PX, segments=10, affect="EDGES")
    bpy.ops.object.mode_set(mode="OBJECT")
    bev = o.modifiers.new("b", "BEVEL")
    bev.width = min(d * 0.35, 10) * PX
    bev.segments = 4
    bev.limit_method = "ANGLE"
    o.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    return o


def disc(r, d, material, x=0, y=0, z=0):
    bpy.ops.mesh.primitive_cylinder_add(vertices=64, radius=r * PX, depth=d * PX, location=(x * PX, y * PX, z * PX),
                                        rotation=(math.radians(90), 0, 0))
    o = bpy.context.active_object
    bev = o.modifiers.new("b", "BEVEL")
    bev.width = min(d * 0.4, 12) * PX
    bev.segments = 5
    bev.limit_method = "ANGLE"
    o.data.materials.append(material)
    bpy.ops.object.shade_smooth()
    return o


def rivet(x, z, r=7, y=-30):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r * PX, location=(x * PX, y * PX, z * PX), segments=16, ring_count=8)
    o = bpy.context.active_object
    o.scale = (1, 0.6, 1)
    o.data.materials.append(IRON())
    bpy.ops.object.shade_smooth()


def render(sc, out):
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print("[ui]", out)


# ---------- элементы ----------
ENAMEL = {"btn_green": (0.25, 0.72, 0.12), "btn_gold": (1.0, 0.62, 0.08), "btn_red": (0.86, 0.16, 0.12),
          "btn": (0.22, 0.42, 0.9), "btn_dark": (0.2, 0.17, 0.3)}


def button(name, color, o):
    W, H = 256, 144
    sc = reset(o["samples"], W, H)
    dark = tuple(c * 0.45 for c in color)
    rbox(W - 8, H - 14, 40, 40, mat("Lip", dark, 0, 0.5), 0, 10, -8)                  # нижняя «губа»
    rbox(W - 8, H - 14, 40, 40, GOLD(), 0, 0, 4)                                        # золотая окантовка
    rbox(W - 30, H - 36, 40, 30, mat("Enamel", color, 0, 0.25, coat=1.0), 0, -8, 6)    # эмаль
    # блик: полупрозрачная светлая полоса сверху
    glint = tuple(c + (1 - c) * 0.55 for c in color)
    rbox(W - 70, 12, 4, 6, mat("Gloss", glint, 0, 0.1, emit=0.15), 0, -29, 30)
    render(sc, os.path.join(o["out"], "ui", name + ".png"))


def round_button(name, color, o):
    S = 192
    sc = reset(o["samples"], S, S)
    disc(92, 40, mat("Lip", tuple(c * 0.45 for c in color), 0, 0.5), 0, 10, -6)
    disc(92, 40, GOLD(), 0, 0, 3)
    disc(76, 40, mat("Enamel", color, 0, 0.25, coat=1.0), 0, -8, 4)
    render(sc, os.path.join(o["out"], "ui", name + ".png"))


def wood_panel(name, o, light=False):
    W = H = 256
    sc = reset(o["samples"], W, H)
    frame = textured("Frame", (0.16, 0.08, 0.04), (0.3, 0.16, 0.07), "wood", 6, 0.6, 0.4)
    rbox(W - 6, H - 6, 30, 26, frame, 0, 0, 0)
    inner = textured("Planks", (0.36, 0.2, 0.09), (0.56, 0.34, 0.16), "wood", 9, 0.55, 0.35) if light else \
        textured("Planks", (0.2, 0.12, 0.06), (0.34, 0.2, 0.1), "wood", 9, 0.6, 0.35)
    rbox(W - 50, H - 50, 30, 14, inner, 0, -6, 0)
    # железные уголки с заклёпками
    for sx in (-1, 1):
        for sz in (-1, 1):
            cx, cz = sx * (W / 2 - 26), sz * (H / 2 - 26)
            rbox(40, 40, 10, 8, IRON(), cx, -18, cz)
            rivet(cx, cz, 6, -25)
    render(sc, os.path.join(o["out"], "ui", name + ".png"))


def paper(o):
    W = H = 320
    sc = reset(o["samples"], W, H)
    rbox(W - 6, H - 6, 34, 34, textured("Frame", (0.28, 0.15, 0.06), (0.46, 0.27, 0.12), "wood", 6, 0.55, 0.4), 0, 0, 0)
    rbox(W - 56, H - 56, 30, 20, textured("Parchment", (0.86, 0.74, 0.52), (0.98, 0.91, 0.74), "noise", 3.5, 0.9, 0.08), 0, -6, 0)
    for sx in (-1, 1):
        for sz in (-1, 1):
            cx, cz = sx * (W / 2 - 22), sz * (H / 2 - 22)
            disc(16, 14, GOLD(), cx, -20, cz)
    render(sc, os.path.join(o["out"], "ui", "paper.png"))


def card(name, o, dark=False):
    W, H = 192, 192
    sc = reset(o["samples"], W, H)
    rbox(W - 6, H - 6, 24, 26, textured("Frame", (0.42, 0.26, 0.11), (0.6, 0.4, 0.18), "wood", 7, 0.55, 0.3), 0, 0, 0)
    c1, c2 = ((0.62, 0.55, 0.45), (0.72, 0.65, 0.54)) if dark else ((0.93, 0.84, 0.64), (1.0, 0.95, 0.82))
    rbox(W - 26, H - 26, 24, 18, textured("Paper", c1, c2, "noise", 4, 0.85, 0.05), 0, -4, 0)
    render(sc, os.path.join(o["out"], "ui", name + ".png"))


def ribbon(o):
    W, H = 512, 112
    sc = reset(o["samples"], W, H)
    cloth = mat("Cloth", (0.62, 0.1, 0.12), 0, 0.75)
    dark = mat("ClothDark", (0.36, 0.05, 0.07), 0, 0.8)
    # хвосты ленты (сзади, с вырезом «ласточкин хвост» — клинья)
    for sx in (-1, 1):
        b = rbox(110, 70, 18, 6, dark, sx * (W / 2 - 60), 14, -12)
        b.rotation_euler = (0, math.radians(-8 * sx), 0)
    rbox(W - 90, 80, 22, 10, cloth, 0, 0, 4)
    rbox(W - 90, 8, 24, 3, GOLD(), 0, -2, 4 + 34)
    rbox(W - 90, 8, 24, 3, GOLD(), 0, -2, 4 - 34)
    render(sc, os.path.join(o["out"], "ui", "ribbon.png"))


def bars(o):
    W, H = 192, 56
    sc = reset(o["samples"], W, H)
    rbox(W - 4, H - 4, 20, 26, textured("Slot", (0.12, 0.07, 0.04), (0.22, 0.13, 0.07), "wood", 8, 0.6, 0.3), 0, 0, 0)
    rbox(W - 18, H - 18, 20, 19, mat("Hollow", (0.05, 0.04, 0.06), 0, 0.7), 0, -3, 0)
    render(sc, os.path.join(o["out"], "ui", "bar_bg.png"))
    sc = reset(o["samples"], W, H - 16)
    rbox(W - 10, H - 26, 20, 15, mat("Glass", (0.92, 0.92, 0.92), 0, 0.15, coat=1.0), 0, 0, 0)
    rbox(W - 40, 6, 4, 3, mat("Gloss", (1, 1, 1), 0, 0.1, emit=1.0), 0, -12, 6)
    render(sc, os.path.join(o["out"], "ui", "bar_fill.png"))


def badge(o):
    S = 224
    sc = reset(o["samples"], S, S)
    # щит: скруглённый прямоугольник + клин снизу
    disc(100, 40, GOLD(), 0, 0, 0)
    disc(84, 42, mat("Enamel", (0.16, 0.34, 0.85), 0, 0.25, coat=1.0), 0, -6, 0)
    for k in range(12):
        a = k * math.pi / 6
        rivet(math.cos(a) * 92, math.sin(a) * 92, 6, -22)
    render(sc, os.path.join(o["out"], "ui", "badge.png"))


def shelf(o):
    W, H = 256, 128
    sc = reset(o["samples"], W, H)
    rbox(W - 6, H - 20, 30, 20, textured("Shelf", (0.3, 0.17, 0.08), (0.5, 0.3, 0.14), "wood", 7, 0.55, 0.4), 0, 0, -4)
    rbox(W - 6, 14, 34, 6, IRON(), 0, -2, H / 2 - 18)
    for sx in (-1, 1):
        rivet(sx * (W / 2 - 22), H / 2 - 18, 6, -22)
    render(sc, os.path.join(o["out"], "ui", "shelf.png"))


ITEMS = {**{n: (lambda o, n=n, c=c: button(n, c, o)) for n, c in ENAMEL.items()},
         "round": lambda o: round_button("round", (0.22, 0.42, 0.9), o),
         "round_gold": lambda o: round_button("round_gold", (1.0, 0.62, 0.08), o),
         "round_green": lambda o: round_button("round_green", (0.25, 0.72, 0.12), o),
         "round_red": lambda o: round_button("round_red", (0.86, 0.16, 0.12), o),
         "panel": lambda o: wood_panel("panel", o, True),
         "panel_dark": lambda o: wood_panel("panel_dark", o, False),
         "paper": paper, "card": lambda o: card("card", o), "card_dark": lambda o: card("card_dark", o, True),
         "ribbon": ribbon, "bars": bars, "badge": badge, "shelf": shelf}


def main():
    o = args()
    for name, fn in ITEMS.items():
        if o["only"] and name not in o["only"]:
            continue
        fn(o)


if __name__ == "__main__":
    main()
