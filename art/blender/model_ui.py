"""Рамки, плашки и кнопки интерфейса как игровые предметы: объёмные, с фаской, глянцем и золотым ободом — в одном свете
с иконками (model_icons.rig). Каждый спрайт режется движком на 9 частей (draw9), поэтому размеры и толщина рамок
совпадают с границами в коде: панель 192/70, окно 192/66, карточка 128/40, кнопка 160/56, полоска 96/30.

Запуск: blender -b -P art/blender/model_ui.py -- --out art/out --samples 48 [--only btn_gold,paper]
Результат: <out>/ui3d/<имя>.png; pack_atlas.py кладёт их в атлас как ui/<имя> вместо процедурных.
"""
import math
import os
import sys

import bmesh
import bpy
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import model_icons as I  # noqa: E402
import render_sprites as R  # noqa: E402

PX = 0.01  # 1 пиксель спрайта = 0.01 единицы сцены


def rrect(w, h, r, segs=10):
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90), (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for i in range(segs + 1):
            a = math.radians(a0 + 90 * i / segs)
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
    return pts


def plate(pts, z0, z1, m, bevel, name="plate"):
    """Плоская фигура pts [(x, y)] толщиной от z0 до z1 со скруглённой фаской по верху."""
    bm = bmesh.new()
    lo = [bm.verts.new((x, y, z0)) for x, y in pts]
    hi = [bm.verts.new((x, y, z1)) for x, y in pts]
    bm.faces.new(lo[::-1])
    bm.faces.new(hi)
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new((lo[i], lo[j], hi[j], hi[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(m)
    mod = o.modifiers.new("bevel", "BEVEL")
    mod.width = bevel
    mod.segments = 5
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(60)
    o.modifiers.new("wn", "WEIGHTED_NORMAL")
    for p in o.data.polygons:
        p.use_smooth = True
    return o


def glossy(name, top, bottom, rough=0.25, coat=0.7):
    """Лаковое покрытие с вертикальным градиентом (светлее сверху)."""
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    b = N["Principled BSDF"]
    b.inputs["Roughness"].default_value = rough
    b.inputs["Coat Weight"].default_value = coat
    b.inputs["Coat Roughness"].default_value = 0.06
    tc = N.new("ShaderNodeTexCoord")
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(tc.outputs["Generated"], sep.inputs[0])
    ramp = N.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (*I.lin(bottom), 1)
    ramp.color_ramp.elements[1].color = (*I.lin(top), 1)
    L.new(sep.outputs["Y"], ramp.inputs["Fac"])
    L.new(ramp.outputs["Color"], b.inputs["Base Color"])
    return m


GOLD = None


def mats():
    global GOLD
    GOLD = glossy("gold_rim", (255, 214, 92), (214, 140, 36), rough=0.3, coat=0.9)
    return {
        "navy": glossy("navy", (40, 74, 130), (22, 42, 82), rough=0.4, coat=0.5),
        "navy_dark": glossy("navy_dark", (26, 44, 80), (16, 28, 54), rough=0.45, coat=0.4),
        "card": glossy("card", (70, 118, 186), (44, 82, 142), rough=0.35, coat=0.6),
        "card_dark": glossy("card_dark", (44, 58, 88), (32, 42, 66), rough=0.4, coat=0.4),
        "btn": glossy("btn", (84, 156, 250), (36, 92, 206)),
        "btn_gold": glossy("btn_gold", (255, 200, 54), (232, 128, 14)),
        "btn_green": glossy("btn_green", (118, 214, 64), (40, 146, 40)),
        "btn_red": glossy("btn_red", (246, 92, 74), (190, 32, 36)),
        "btn_dark": glossy("btn_dark", (82, 94, 124), (50, 58, 84)),
        "lip_btn": I.mat("lip_btn", (24, 58, 140), rough=0.4),
        "lip_btn_gold": I.mat("lip_btn_gold", (170, 90, 10), rough=0.4),
        "lip_btn_green": I.mat("lip_btn_green", (24, 100, 32), rough=0.4),
        "lip_btn_red": I.mat("lip_btn_red", (130, 20, 26), rough=0.4),
        "lip_btn_dark": I.mat("lip_btn_dark", (32, 36, 52), rough=0.4),
        "outline": I.mat("outline", (14, 18, 32), rough=0.6),
        "groove": I.mat("groove", (10, 16, 32), rough=0.6),
        "white": I.mat("white_fill", (240, 240, 240), rough=0.3, coat=0.5),
        "ribbon": glossy("ribbon", (236, 70, 60), (176, 30, 34)),
        "ribbon_back": I.mat("ribbon_back", (120, 18, 24), rough=0.4),
    }


# ---------- элементы ----------
def window(M, w, h, rim, r, inner, inner_m, trim=True):
    """Рамка: тёмный кант, золотой обод с фаской, утопленное поле; тонкая золотая нить по полю."""
    plate(rrect(w, h, r), 0.0, 0.06, M["outline"], 0.02, "edge")
    plate(rrect(w - 8 * PX, h - 8 * PX, r - 4 * PX), 0.0, 0.14, GOLD, 0.06, "rim")
    plate(rrect(w - 2 * rim, h - 2 * rim, max(0.02, r - rim)), 0.0, 0.15, M["outline"], 0.01, "inner_edge")
    plate(rrect(w - 2 * rim - 6 * PX, h - 2 * rim - 6 * PX, max(0.02, r - rim - 3 * PX)), 0.0, 0.16, inner_m, 0.03, "field")
    if trim:
        t = inner
        plate(rrect(w - 2 * t, h - 2 * t, max(0.02, r - t)), 0.0, 0.165, GOLD, 0.002, "trim")
        plate(rrect(w - 2 * t - 4 * PX, h - 2 * t - 4 * PX, max(0.02, r - t - 2 * PX)), 0.0, 0.168, inner_m, 0.002, "trim_in")


def button(M, key, w, h, r, lip=14 * PX):
    plate(rrect(w, h, r), 0.0, 0.2, M["outline"], 0.03, "edge")
    plate(rrect(w - 8 * PX, h - 8 * PX, r - 4 * PX), 0.0, 0.22, M["lip_" + key], 0.05, "lip")
    o = plate(rrect(w - 8 * PX, h - 8 * PX - lip, r - 4 * PX), 0.0, 0.3, M[key], 0.07, "face")
    o.location.y += lip / 2


def medallion(M, key, d):
    """Круглая кнопка: цветное поле с глянцем в толстом золотом кольце."""
    circ = lambda rr: [(math.cos(a) * rr, math.sin(a) * rr) for a in (i * 2 * math.pi / 64 for i in range(64))]
    plate(circ(d / 2), 0.0, 0.12, M["outline"], 0.03, "edge")
    plate(circ(d / 2 - 5 * PX), 0.0, 0.2, GOLD, 0.06, "ring")
    plate(circ(d / 2 - 24 * PX), 0.0, 0.18, M["outline"], 0.01, "ring_in")
    plate(circ(d / 2 - 28 * PX), 0.0, 0.26, M[key], 0.08, "face")


def bar(M, w, h, fill=False):
    if fill:
        plate(rrect(w, h, h / 2 - PX), 0.0, 0.14, M["white"], 0.05, "fill")
        return
    plate(rrect(w, h, h / 2 - PX), 0.0, 0.08, M["outline"], 0.02, "edge")
    plate(rrect(w - 8 * PX, h - 8 * PX, h / 2 - 5 * PX), 0.0, 0.1, GOLD, 0.03, "rim")
    plate(rrect(w - 16 * PX, h - 16 * PX, h / 2 - 9 * PX), 0.0, 0.11, M["groove"], 0.02, "groove")


def ribbon(M, w, h):
    """Лента-заголовок: красное полотнище с золотой каймой и раздвоенными хвостами (по 120 px не тянутся)."""
    tail = 120 * PX
    for s in (-1, 1):
        x0 = s * (w / 2)
        x1 = s * (w / 2 - tail)
        pts = [(x0, h / 2 - 16 * PX), (x1, h / 2 - 22 * PX), (x1, -h / 2 + 4 * PX), (x0, -h / 2 + 10 * PX), (x0 - s * 30 * PX, -2 * PX)]
        if s < 0:
            pts = pts[::-1]
        plate(pts, 0.0, 0.1, M["ribbon_back"], 0.02, "tail")
    plate(rrect(w - 2 * 40 * PX, h - 18 * PX, 8 * PX), 0.0, 0.12, M["outline"], 0.02, "edge")
    o = plate(rrect(w - 2 * 44 * PX, h - 26 * PX, 6 * PX), 0.0, 0.18, M["ribbon"], 0.04, "cloth")
    for y in (h / 2 - 22 * PX, -h / 2 + 22 * PX):
        g = plate(rrect(w - 2 * 52 * PX, 5 * PX, 2 * PX), 0.18, 0.19, GOLD, 0.002, "trim")
        g.location.y = y * 0.82


def badge(M, d):
    """Знак уровня: синий самоцвет-щит в золотом венце с зубцами."""
    pts = []
    for i in range(48):
        a = i * 2 * math.pi / 48
        rr = d / 2 - (0 if i % 4 < 2 else 10 * PX)
        pts.append((math.cos(a) * rr, math.sin(a) * rr))
    plate(pts, 0.0, 0.14, M["outline"], 0.02, "edge")
    plate([(x * 0.94, y * 0.94) for x, y in pts], 0.0, 0.2, GOLD, 0.05, "crown")
    circ = lambda rr: [(math.cos(a) * rr, math.sin(a) * rr) for a in (i * 2 * math.pi / 64 for i in range(64))]
    plate(circ(d / 2 - 30 * PX), 0.0, 0.2, M["outline"], 0.01, "in")
    plate(circ(d / 2 - 34 * PX), 0.0, 0.3, M["btn"], 0.1, "gem")


PIECES = {
    # имя: (ширина, высота, построение, наклон камеры)
    "panel": (192, 192, lambda M: window(M, 1.92, 1.92, 14 * PX, 0.4, 24 * PX, M["navy"]), 0),
    "panel_dark": (192, 192, lambda M: window(M, 1.92, 1.92, 10 * PX, 0.4, 0, M["navy_dark"], trim=False), 0),
    "paper": (192, 192, lambda M: window(M, 1.92, 1.92, 18 * PX, 0.42, 30 * PX, M["navy"]), 0),
    "card": (128, 128, lambda M: window(M, 1.28, 1.28, 8 * PX, 0.24, 0, M["card"], trim=False), 0),
    "card_dark": (128, 128, lambda M: window(M, 1.28, 1.28, 8 * PX, 0.24, 0, M["card_dark"], trim=False), 0),
    "bar_bg": (96, 60, lambda M: bar(M, 0.96, 0.6), 0),
    "bar_fill": (96, 52, lambda M: bar(M, 0.96, 0.52, fill=True), 0),
    "ribbon": (360, 96, lambda M: ribbon(M, 3.6, 0.96), 0),
    "badge": (192, 192, lambda M: badge(M, 1.92), 0),
}
for _k in ("btn", "btn_gold", "btn_green", "btn_red", "btn_dark"):
    PIECES[_k] = (160, 160, (lambda k: lambda M: button(M, k, 1.6, 1.6, 0.42))(_k), 0)
for _k, _m in (("round", "btn"), ("round_gold", "btn_gold"), ("round_green", "btn_green"), ("round_red", "btn_red")):
    PIECES[_k] = (192, 192, (lambda k: lambda M: medallion(M, k, 1.92))(_m), 0)


def shoot(name, w, h, opts):
    sc = bpy.context.scene
    sc.render.resolution_x, sc.render.resolution_y = w, h
    sc.view_settings.exposure = -0.45
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = max(w, h) * PX
    cam_data.sensor_fit = "AUTO"
    cam = bpy.data.objects.new("Cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.location = (0, 0, 10)
    out = os.path.join(opts["out"], "ui3d", name + ".png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    print(f"[ui] {name} -> {out}")


def light(sc):
    I.rig(sc)
    # для плоских деталей ключевой свет круче сверху, контровой — по верхней кромке
    for o in sc.objects:
        if o.type == "LIGHT":
            if o.name == "Key":
                o.rotation_euler = (math.radians(35), 0, math.radians(-30))
            if o.name == "Rim":
                o.rotation_euler = (math.radians(55), 0, math.radians(180))
                o.data.energy = 3.0


def main():
    opts = R.args()
    only = opts.get("only") or set()
    for name, (w, h, fn, _tilt) in PIECES.items():
        if only and name not in only:
            continue
        sc = R.reset_scene(int(opts["samples"]))
        light(sc)
        fn(mats())
        shoot(name, w, h, opts)


if __name__ == "__main__":
    main()
