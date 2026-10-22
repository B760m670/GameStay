#!/usr/bin/env python3
"""Ландшафт под поселением: одна цельная текстура земли (вид сверху) + плитка мелкой травы.

Использование: python3 art/make_terrain.py engine/assets/art
Выход: ground.jpg (вся карта вместе с краем, вид сверху) и grass_detail.png (бесшовная плитка 4×4 клетки).

Текстура кладётся на ромб карты одним квадом (изометрия — аффинное преобразование), так что швов нет.
Ручей, пруд, тропинки и мосты должны совпадать с world.cc (Terrain) — меняй их в обоих местах.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

DST = sys.argv[1] if len(sys.argv) > 1 else "engine/assets/art"
GRID, PAD = 44, 9
N = GRID + 2 * PAD          # клеток по стороне вместе с краем
S = 2048                    # пикселей текстуры по стороне
PPC = S / N
rng = np.random.default_rng(7)


def noise(cells, seed, size=S):
    """Гладкий шум: случайная сетка cells×cells, растянутая бикубически."""
    r = np.random.default_rng(seed).random((cells, cells)).astype(np.float32)
    im = Image.fromarray((r * 255).astype(np.uint8)).resize((size, size), Image.BICUBIC)
    return np.asarray(im, dtype=np.float32) / 255.0


def fbm(seed, octaves=((6, 0.5), (14, 0.25), (34, 0.15), (90, 0.1))):
    return sum(noise(c, seed + i) * w for i, (c, w) in enumerate(octaves))


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def mix(c1, c2, t):
    return c1 * (1 - t[..., None]) + c2 * t[..., None]


def col(r, g, b):
    return np.array([r, g, b], dtype=np.float32)


# координаты мира для каждого пикселя: u — ось x игры, v — ось y
ax = (np.arange(S, dtype=np.float32) + 0.5) / PPC - PAD
wx, wy = np.meshgrid(ax, ax)

# ---------- трава ----------
n1, n2, n3 = fbm(1), fbm(20), fbm(40)
lush, light, dark, sun = col(72, 150, 48), col(108, 178, 58), col(46, 116, 42), col(150, 182, 66)
img = mix(lush, dark, smooth(0.45, 0.75, n1))
img = mix(img, light, smooth(0.55, 0.8, n2) * 0.8)
img = mix(img, sun, smooth(0.68, 0.85, n3) * 0.6)
# пологие холмики: крупная светотень
hill = fbm(60, ((4, 0.6), (9, 0.4)))
img *= (0.9 + 0.2 * hill)[..., None]

# ---------- лесная подстилка за краем поля ----------
out = np.maximum.reduce([-wx, -wy, wx - GRID, wy - GRID, np.zeros_like(wx)])
forest = mix(col(44, 66, 42), col(70, 60, 40), smooth(0.4, 0.8, fbm(80)))
needles = (noise(400, 90) > 0.72).astype(np.float32) * 0.25
forest = forest * (1 - needles[..., None]) + col(96, 76, 48) * needles[..., None]
tf = smooth(0.2, 3.5, out + (fbm(100) - 0.5) * 3)
img = mix(img, forest, tf)

# ---------- тропинки к мостам ----------
def path_dist(px, py, along_x):
    # тропа от края поля наружу: по y у x=px (along_x=False) или по x у y=py
    if along_x:
        c = py + 0.5 * np.sin(wx * 0.7)
        d = np.abs(wy - c)
        d = np.where(wx < GRID - 1, 99, d)
    else:
        c = px + 0.5 * np.sin(wy * 0.7)
        d = np.abs(wx - c)
        d = np.where(wy < GRID - 1, 99, d)
    return d


pd = np.minimum(path_dist(22, 0, False), path_dist(0, 22, True))
pw = 0.65 + (fbm(120) - 0.5) * 0.5
dirt = mix(col(128, 100, 66), col(150, 124, 84), fbm(130))
pebble = (noise(300, 140) > 0.8).astype(np.float32)
dirt = mix(dirt, col(170, 160, 150), pebble * 0.5)
pt = 1 - smooth(pw - 0.25, pw + 0.15, pd)
img = mix(img, dirt, pt)

# ---------- ручей вдоль двух ближних сторон и пруд в углу ----------
def meander(t):
    # то же самое в world.cc: streamCenter()
    return 47.6 + 1.1 * np.sin(t * 0.21) + 0.45 * np.sin(t * 0.53 + 1.0)


def stream_dist():
    da = np.where(wx <= 48.5, np.abs(wy - meander(wx)), 99)
    db = np.where(wy <= 48.5, np.abs(wx - meander(wy)), 99)
    width = 0.75 + 0.3 * np.sin(np.minimum(wx, wy) * 0.37)
    ds = np.minimum(da, db) - width
    ang = np.arctan2(wy - 48.5, wx - 48.5)
    dp = np.hypot(wx - 48.5, wy - 48.5) - (3.6 + 0.5 * np.sin(ang * 3 + 0.7))
    k = 0.8  # плавное слияние ручья с прудом
    h = np.clip(0.5 + 0.5 * (dp - ds) / k, 0, 1)
    d = dp * (1 - h) + ds * h - k * h * (1 - h)
    return d + (fbm(150) - 0.5) * 0.3


sd = stream_dist()
bank = mix(col(150, 136, 98), col(110, 100, 80), smooth(0.4, 0.7, fbm(160)))
img = mix(img, bank, 1 - smooth(0.05, 0.6, sd))                    # песок и галька
img = mix(img, col(70, 66, 54), (1 - smooth(-0.05, 0.12, sd)) * 0.7)  # мокрая кромка
deep, shallow = col(22, 58, 76), col(48, 108, 112)
water = mix(shallow, deep, smooth(-0.05, -0.75, sd))
ripple = fbm(170, ((40, 0.5), (120, 0.5)))
water *= (0.92 + 0.16 * ripple)[..., None]
img = mix(img, water, 1 - smooth(-0.1, 0.0, sd))

# ---------- клевер, цветы, тёмные куртинки (только на траве поля) ----------
inside = (1 - tf) * smooth(0.0, 0.3, sd) * (1 - pt)
spots = noise(700, 200)
for thr, c, a in ((0.95, col(240, 236, 210), 0.5), (0.96, col(250, 220, 90), 0.55), (0.97, col(200, 140, 220), 0.5)):
    m = (spots > thr).astype(np.float32) * inside * a * smooth(0.5, 0.7, noise(12, int(thr * 1000)))
    img = mix(img, c, m)
    spots = noise(700, int(thr * 777))
clumps = smooth(0.6, 0.85, fbm(300, ((30, 0.6), (70, 0.4)))) * inside * 0.3
img = mix(img, col(44, 96, 42), clumps)

# ---------- край мира уходит в темноту ----------
edge = np.maximum.reduce([-wx - PAD + 2.5, -wy - PAD + 2.5, wx - GRID - PAD + 2.5, wy - GRID - PAD + 2.5])
img = mix(img, col(14, 12, 24), smooth(-0.5, 2.5, edge))

os.makedirs(DST, exist_ok=True)
Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).save(os.path.join(DST, "ground.jpg"), quality=88)

# ---------- плитка травинок (бесшовная, 4×4 клетки) ----------
T = 256
tile = Image.new("RGBA", (T * 3, T * 3), (0, 0, 0, 0))
from PIL import ImageDraw  # noqa: E402

d = ImageDraw.Draw(tile)
r = np.random.default_rng(11)
for _ in range(900):
    x, y = r.uniform(T, 2 * T), r.uniform(T, 2 * T)
    ln = r.uniform(3, 7)
    ang = r.uniform(-0.6, 0.6) - math.pi / 2
    if r.random() < 0.55:
        c = (190, 230, 120, int(r.uniform(60, 120)))
    else:
        c = (20, 60, 20, int(r.uniform(50, 100)))
    for ox in (-T, 0, T):          # рисуем с переносом — плитка бесшовная
        for oy in (-T, 0, T):
            d.line((x + ox, y + oy, x + ox + math.cos(ang) * ln, y + oy + math.sin(ang) * ln), fill=c, width=2)
tile = tile.crop((T, T, 2 * T, 2 * T)).filter(ImageFilter.GaussianBlur(0.4))
tile.save(os.path.join(DST, "grass_detail.png"))
print("terrain:", os.path.join(DST, "ground.jpg"))
