"""Трава из настоящих травинок: плитка 4×4 клетки рендерится под игровой изометрической камерой
и разворачивается в квадратную бесшовную текстуру (изометрия аффинна, так что развёртка точная).

Запуск:
  blender -b --factory-startup -P art/blender/render_grass.py -- --out engine/assets/art [--samples 48]

Плитка повторяется 3×3, рендерится центральная — травинки соседей заходят на края, поэтому швов нет.
Освещение то же, что у спрайтов построек: солнце сверху-слева.
"""
import math
import os
import random
import sys

import bmesh
import bpy
import numpy as np
from bpy_extras.object_utils import world_to_camera_view
from mathutils import Vector

ELEV = math.atan(0.5)
TILE = 4.0          # клеток в плитке
OUT_PX = 512        # сторона итоговой текстуры (128 px на клетку — как у спрайтов)
BLADES = 1500       # травинок на клетку


def args():
    a = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    o = {"out": "engine/assets/art", "samples": 48}
    for i in range(0, len(a) - 1, 2):
        o[a[i].lstrip("-")] = a[i + 1]
    o["samples"] = int(o["samples"])
    return o


def scene(samples):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    sc.cycles.device = "CPU"
    sc.cycles.samples = samples
    sc.cycles.use_denoising = True
    sc.view_settings.view_transform = "Standard"
    w = bpy.data.worlds.new("W")
    sc.world = w
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.62, 0.74, 1.0, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 0.55
    sun_data = bpy.data.lights.new("Sun", "SUN")
    sun_data.energy = 3.4
    sun_data.angle = math.radians(10)
    sun_data.color = (1.0, 0.95, 0.85)
    sun = bpy.data.objects.new("Sun", sun_data)
    sun.rotation_euler = (math.radians(30), 0, math.radians(-25))
    sc.collection.objects.link(sun)
    return sc


def vcol_material():
    m = bpy.data.materials.new("Grass")
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    attr = nt.nodes.new("ShaderNodeVertexColor")
    attr.layer_name = "Col"
    nt.links.new(attr.outputs["Color"], b.inputs["Base Color"])
    b.inputs["Roughness"].default_value = 0.65
    b.inputs["Subsurface Weight"].default_value = 0.15
    return m


def build_field(rng):
    me = bpy.data.meshes.new("Field")
    bm = bmesh.new()
    col = bm.loops.layers.color.new("Col")
    greens = [(0.20, 0.52, 0.08), (0.28, 0.62, 0.10), (0.36, 0.68, 0.12), (0.16, 0.44, 0.07), (0.44, 0.66, 0.14)]
    # пятна поля: где-то трава гуще и сочнее, где-то светлее и суше
    def patch(x, y):
        return 0.5 + 0.25 * math.sin(x * 1.7 + 0.3) * math.sin(y * 1.3 + 1.1) + 0.25 * math.sin(x * 0.9 - y * 1.1)

    blades = []
    n = int(BLADES * TILE * TILE)
    for _ in range(n):
        x, y = rng.uniform(0, TILE), rng.uniform(0, TILE)
        p = patch(x, y)
        h = rng.uniform(0.05, 0.11) * (0.8 + 0.5 * p)
        w = rng.uniform(0.012, 0.02)
        ang = rng.uniform(0, math.tau)
        lean = rng.uniform(0.15, 0.55)
        base = greens[rng.randrange(len(greens))]
        k = 0.85 + 0.3 * p
        blades.append((x, y, h, w, ang, lean, tuple(min(1, c * k) for c in base)))
    # клевер и мелкие цветы
    extras = []
    for _ in range(int(TILE * TILE * 10)):
        x, y = rng.uniform(0, TILE), rng.uniform(0, TILE)
        kind = rng.random()
        c = (0.95, 0.95, 0.88) if kind < 0.45 else (1.0, 0.82, 0.15) if kind < 0.75 else (0.78, 0.45, 0.95) if kind < 0.9 else (1.0, 0.45, 0.35)
        extras.append((x, y, c))

    for ox in (-TILE, 0, TILE):
        for oy in (-TILE, 0, TILE):
            for (x, y, h, w, ang, lean, c) in blades:
                bx, by = x + ox, y + oy
                dx, dy = math.cos(ang), math.sin(ang)
                px, py = -dy * w, dx * w
                verts = []
                segs = 3
                for s in range(segs + 1):
                    t = s / segs
                    off = lean * h * t * t
                    cx, cy, cz = bx + dx * off, -(by + dy * off), h * t
                    ww = 1 - t * 0.9
                    verts.append(bm.verts.new((cx - px * ww, cy + py * ww, cz)))
                    verts.append(bm.verts.new((cx + px * ww, cy - py * ww, cz)))
                for s in range(segs):
                    f = bm.faces.new((verts[2 * s], verts[2 * s + 1], verts[2 * s + 3], verts[2 * s + 2]))
                    t0 = s / segs
                    for loop in f.loops:
                        tt = loop.vert.co.z / max(h, 1e-4)
                        shade = 0.55 + 0.6 * tt          # у корня темнее, к кончику светлее
                        loop[col] = (min(1, c[0] * shade + 0.04 * tt), min(1, c[1] * shade + 0.05 * tt), c[2] * shade, 1)
            for (x, y, c) in extras:
                bx, by = x + ox, -(y + oy)
                ring = []
                for k in range(6):
                    a = k * math.tau / 6
                    ring.append(bm.verts.new((bx + math.cos(a) * 0.022, by + math.sin(a) * 0.022, 0.07)))
                f = bm.faces.new(ring)
                for loop in f.loops:
                    loop[col] = (*c, 1)
    bm.to_mesh(me)
    bm.free()
    o = bpy.data.objects.new("Field", me)
    bpy.context.scene.collection.objects.link(o)
    o.data.materials.append(vcol_material())
    # земля под травой
    bpy.ops.mesh.primitive_plane_add(size=TILE * 3, location=(TILE / 2, -TILE / 2, 0))
    g = bpy.context.active_object
    gm = bpy.data.materials.new("Soil")
    gm.use_nodes = True
    gm.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.1, 0.22, 0.04, 1)
    g.data.materials.append(gm)


def camera(sc, px_per_cell):
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.type = "ORTHO"
    diag = TILE * math.sqrt(2)
    cam_data.ortho_scale = diag * 1.08
    cam = bpy.data.objects.new("Cam", cam_data)
    sc.collection.objects.link(cam)
    sc.camera = cam
    cam.rotation_euler = (math.pi / 2 - ELEV, 0, math.radians(45))
    target = Vector((TILE / 2, -TILE / 2, 0))
    fwd = Vector((0, 0, -1))
    fwd.rotate(cam.rotation_euler)
    cam.location = target - fwd * 30
    w = int(diag * 1.08 * px_per_cell / math.sqrt(2) * 2)  # 2x — запас резкости для развёртки
    sc.render.resolution_x = w
    sc.render.resolution_y = int(w * 0.6)
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = False
    bpy.context.view_layer.update()
    return cam


def unwarp(sc, cam, path_png, out_path):
    W, H = sc.render.resolution_x, sc.render.resolution_y
    img = bpy.data.images.load(path_png)
    px = np.array(img.pixels[:], dtype=np.float32).reshape(H, W, 4)[::-1]  # верх изображения — строка 0
    def proj(x, y):
        p = world_to_camera_view(sc, cam, Vector((x, -y, 0)))
        return np.array([p.x * W, (1 - p.y) * H])
    o, ex, ey = proj(0, 0), proj(TILE, 0), proj(0, TILE)
    u = (np.arange(OUT_PX) + 0.5) / OUT_PX
    U, V = np.meshgrid(u, u)
    sx = o[0] + (ex[0] - o[0]) * U + (ey[0] - o[0]) * V
    sy = o[1] + (ex[1] - o[1]) * U + (ey[1] - o[1]) * V
    x0, y0 = np.clip(np.floor(sx).astype(int), 0, W - 2), np.clip(np.floor(sy).astype(int), 0, H - 2)
    fx, fy = (sx - x0)[..., None], (sy - y0)[..., None]
    c = (px[y0, x0] * (1 - fx) * (1 - fy) + px[y0, x0 + 1] * fx * (1 - fy) + px[y0 + 1, x0] * (1 - fx) * fy + px[y0 + 1, x0 + 1] * fx * fy)
    # ярче и сочнее — стилизация под солнечный день
    rgb = c[..., :3] * 1.32
    gray = rgb.mean(axis=2, keepdims=True)
    c[..., :3] = np.clip(gray + (rgb - gray) * 1.15, 0, 1)
    c[..., 3] = 1
    out = bpy.data.images.new("Grass", OUT_PX, OUT_PX, alpha=False)
    out.pixels[:] = c[::-1].ravel()
    out.filepath_raw = out_path
    out.file_format = "PNG"
    out.save()
    print("[grass]", out_path)


def main():
    o = args()
    sc = scene(o["samples"])
    build_field(random.Random(5))
    cam = camera(sc, 128)
    tmp = os.path.join(bpy.app.tempdir or "/tmp", "grass_iso.png")
    sc.render.filepath = tmp
    bpy.ops.render.render(write_still=True)
    os.makedirs(o["out"], exist_ok=True)
    unwarp(sc, cam, tmp, os.path.join(os.path.abspath(o["out"]), "grass_tile.png"))


if __name__ == "__main__":
    main()
