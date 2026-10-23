#!/usr/bin/env python3
"""Собирает атласы для движка: спрайты из Blender (art/out) + процедурная графика UI и земли.

Использование: python3 art/pack_atlas.py art/out engine/assets/atlas
Выход: pageN.png + atlas.txt (строки `page N file` и `sprite name page x y w h pivotX pivotY`).
"""
import glob
import json
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter

SRC = sys.argv[1] if len(sys.argv) > 1 else "art/out"
DST = sys.argv[2] if len(sys.argv) > 2 else "engine/assets/atlas"
PAGE = 2048
PAD = 2
SS = 4  # суперсэмплинг для процедурной графики

sprites = []  # (name, image, pivot_x, pivot_y)


UI3D = {}  # ui/<имя> -> элемент интерфейса, отрендеренный в Blender (заменяет процедурный)


def add(name, img, px=None, py=None, trim=True):
    if name in UI3D:
        img, px, py, trim = UI3D[name], None, None, False
    img = img.convert("RGBA")
    if px is None:
        px, py = img.width / 2, img.height / 2
    if trim:
        bbox = img.getchannel("A").point(lambda a: 255 if a > 3 else 0).getbbox()
        if bbox is None:
            return
        img = img.crop(bbox)
        px, py = px - bbox[0], py - bbox[1]
    sprites.append((name, img, px, py))


def smooth(draw_fn, w, h):
    """Рисуем в увеличенном размере и уменьшаем — сглаживание для PIL."""
    big = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(big), SS, big)
    return big.resize((w, h), Image.LANCZOS)


# ---------- спрайты из Blender ----------
for png in sorted(glob.glob(os.path.join(SRC, "buildings", "*.png"))):
    meta = json.load(open(png[:-4] + ".json"))
    add("b/" + os.path.basename(png)[:-4], Image.open(png), *meta["pivot"])
for udir in sorted(glob.glob(os.path.join(SRC, "units", "*"))):
    meta = json.load(open(os.path.join(udir, "meta.json")))
    unit = os.path.basename(udir)
    for png in sorted(glob.glob(os.path.join(udir, "*.png"))):
        stem = os.path.basename(png)[:-4]
        parts = stem.rsplit("_", 2)  # anim_dir_frame
        if len(parts) == 3 and parts[1].isdigit() and int(parts[1]) > 4:
            continue  # направления 5..7 — зеркала 3..1, движок рисует их отражёнными
        add(f"u/{unit}/{stem}", Image.open(png), *meta["pivot"])

# ---------- 3D-иконки с обводкой (переопределяют плоские ниже) ----------
def outlined(img, width=7, color=(28, 16, 36)):
    img = img.convert("RGBA")
    pad = width + 2
    big = Image.new("RGBA", (img.width + pad * 2, img.height + pad * 2), (0, 0, 0, 0))
    big.paste(img, (pad, pad))
    a = big.getchannel("A").point(lambda v: 255 if v > 40 else 0)
    ring = a.filter(ImageFilter.MaxFilter(width * 2 + 1)).filter(ImageFilter.GaussianBlur(0.8))
    out = Image.new("RGBA", big.size, color + (0,))
    out.putalpha(ring)
    out.alpha_composite(big)
    return out


from PIL import ImageEnhance  # noqa: E402

for png in sorted(glob.glob(os.path.join(SRC, "ui", "*.png"))):
    im = Image.open(png).convert("RGBA")
    rgb = ImageEnhance.Contrast(ImageEnhance.Color(im.convert("RGB")).enhance(1.25)).enhance(1.06)
    rgb.putalpha(im.getchannel("A"))
    UI3D["ui/" + os.path.basename(png)[:-4]] = outlined(rgb, width=3)
for extra in ("badge", "shelf", "round_red"):  # новые элементы, которых нет в процедурном наборе
    if "ui/" + extra in UI3D:
        add("ui/" + extra, UI3D["ui/" + extra])

ICON3D = {}
for png in sorted(glob.glob(os.path.join(SRC, "icons", "*.png"))):
    name = os.path.basename(png)[:-4]
    ICON3D[name] = outlined(Image.open(png))

# ---------- земля ----------
rnd = random.Random(7)
TW, TH = 128, 64


def grass_tile(seed, base, spread, dark=False):
    r = random.Random(seed)
    w, h = TW + 4, TH + 2  # чуть больше ромба — без щелей между тайлами
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).polygon([(w / 2, 0), (w, h / 2), (w / 2, h), (0, h / 2)], fill=255)
    tex = Image.new("RGBA", (w, h), base)
    d = ImageDraw.Draw(tex)
    for _ in range(140):
        x, y = r.uniform(0, w), r.uniform(0, h)
        k = r.uniform(-spread, spread)
        c = tuple(max(0, min(255, int(v + k))) for v in base[:3]) + (255,)
        rr = r.uniform(2, 7)
        d.ellipse((x - rr, y - rr / 2, x + rr, y + rr / 2), fill=c)
    tex = tex.filter(ImageFilter.GaussianBlur(1.6))
    d = ImageDraw.Draw(tex)
    if not dark:
        for _ in range(26):  # травинки
            x, y = r.uniform(8, w - 8), r.uniform(6, h - 4)
            c = (min(255, base[0] + 40), min(255, base[1] + 50), base[2] + 10, 255)
            d.line((x, y, x + r.uniform(-1.5, 1.5), y - r.uniform(3, 6)), fill=c, width=1)
        if r.random() < 0.35:  # цветок/камушек
            x, y = r.uniform(30, w - 30), r.uniform(18, h - 18)
            col = r.choice([(240, 220, 120), (230, 150, 180), (200, 200, 210)])
            d.ellipse((x - 2, y - 1.5, x + 2, y + 1.5), fill=col + (255,))
    img.paste(tex, (0, 0), mask)
    return img


for i in range(6):
    add(f"g/grass{i}", grass_tile(100 + i, (86 + rnd.randint(-6, 6), 140 + rnd.randint(-8, 8), 66, 255), 16), trim=False)
for i in range(3):
    add(f"g/edge{i}", grass_tile(200 + i, (52, 64, 58, 255), 10, dark=True), trim=False)


# ---------- эффекты ----------
def radial(size, power=1.6):
    img = Image.new("RGBA", (size, size))
    px = img.load()
    c = (size - 1) / 2
    for y in range(size):
        for x in range(size):
            d = math.hypot(x - c, y - c) / c
            a = max(0.0, 1 - d) ** power
            px[x, y] = (255, 255, 255, int(a * 255))
    return img


add("fx/glow", radial(128, 1.8), trim=False)
add("fx/soft", radial(64, 1.0), trim=False)
shadow = Image.new("RGBA", (128, 64), (0, 0, 0, 0))
ImageDraw.Draw(shadow).ellipse((4, 4, 124, 60), fill=(0, 0, 0, 255))
add("fx/shadow", shadow.filter(ImageFilter.GaussianBlur(6)), trim=False)
add("fx/dot", smooth(lambda d, s, im: d.ellipse((0, 0, 16 * s - 1, 16 * s - 1), fill=(255, 255, 255, 255)), 16, 16), trim=False)
diamond = Image.new("RGBA", (TW, TH), (0, 0, 0, 0))
ImageDraw.Draw(diamond).polygon([(TW / 2, 0), (TW, TH / 2), (TW / 2, TH), (0, TH / 2)], fill=(255, 255, 255, 255))
add("fx/tile", diamond, trim=False)
ring = smooth(lambda d, s, im: d.ellipse((2 * s, 2 * s, 126 * s, 62 * s), outline=(255, 255, 255, 255), width=3 * s), 128, 64)
add("fx/ring", ring, trim=False)


# ---------- UI ----------
def rounded_box(w, h, r, top, bottom, border, border_w=3, inner=None, gloss=True):
    def fn(d, s, im):
        grad = Image.new("RGBA", (w * s, h * s))
        gd = ImageDraw.Draw(grad)
        for y in range(h * s):
            t = y / (h * s - 1)
            c = tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,)
            gd.line((0, y, w * s, y), fill=c)
        mask = Image.new("L", (w * s, h * s), 0)
        ImageDraw.Draw(mask).rounded_rectangle((0, 0, w * s - 1, h * s - 1), r * s, fill=255)
        im.paste(grad, (0, 0), mask)
        d.rounded_rectangle((0, 0, w * s - 1, h * s - 1), r * s, outline=border + (255,), width=border_w * s)
        if inner:
            d.rounded_rectangle((border_w * s, border_w * s, w * s - 1 - border_w * s, h * s - 1 - border_w * s), (r - 2) * s,
                                outline=inner + (255,), width=max(1, s))
        if gloss:
            gl = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
            ImageDraw.Draw(gl).rounded_rectangle((border_w * 2 * s, border_w * 2 * s, w * s - border_w * 2 * s, h * s * 0.45),
                                                 (r - 3) * s, fill=(255, 255, 255, 46))
            im.alpha_composite(gl)
    return smooth(fn, w, h)


add("ui/panel", rounded_box(96, 96, 20, (58, 44, 92), (34, 25, 58), (20, 12, 34), 4, (120, 100, 170), gloss=False), trim=False)
add("ui/panel_dark", rounded_box(96, 96, 20, (26, 20, 44), (16, 12, 30), (8, 5, 16), 4, (70, 60, 110), gloss=False), trim=False)
def chunky_button(top, bottom, lip, w=112, h=112, r=26):
    """Кнопка в стиле мобильных стратегий: толстая обводка, «бортик» снизу, глянцевый блик."""
    def fn(d, s, im):
        O = 5 * s
        # тень-бортик (толщина кнопки)
        d.rounded_rectangle((0, 8 * s, w * s - 1, h * s - 1), r * s, fill=(20, 12, 28, 255))
        d.rounded_rectangle((O, 8 * s + O - 2 * s, w * s - 1 - O, h * s - 1 - O + 2 * s), (r - 4) * s, fill=lip + (255,))
        # лицевая часть с градиентом
        face = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
        g = Image.new("RGBA", (w * s, h * s))
        gd = ImageDraw.Draw(g)
        for y in range(h * s):
            t = y / (h * s - 1)
            gd.line((0, y, w * s, y), fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
        m = Image.new("L", (w * s, h * s), 0)
        ImageDraw.Draw(m).rounded_rectangle((O, O, w * s - 1 - O, h * s - 1 - 12 * s), (r - 4) * s, fill=255)
        face.paste(g, (0, 0), m)
        ImageDraw.Draw(face).rounded_rectangle((0, 0, w * s - 1, h * s - 1 - 8 * s), r * s, outline=(20, 12, 28, 255), width=O)
        im.alpha_composite(face)
        # блик
        gl = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
        ImageDraw.Draw(gl).rounded_rectangle((O + 6 * s, O + 4 * s, w * s - O - 6 * s, int(h * s * 0.38)), (r - 8) * s, fill=(255, 255, 255, 70))
        im.alpha_composite(gl)
    return smooth(fn, w, h)


add("ui/btn", chunky_button((128, 112, 200), (82, 66, 150), (52, 38, 100)), trim=False)
add("ui/btn_gold", chunky_button((255, 222, 96), (240, 150, 30), (170, 90, 20)), trim=False)
add("ui/btn_green", chunky_button((168, 236, 96), (82, 176, 56), (40, 110, 34)), trim=False)
add("ui/btn_red", chunky_button((250, 120, 100), (206, 60, 50), (130, 30, 30)), trim=False)
add("ui/btn_dark", chunky_button((92, 82, 124), (58, 50, 86), (34, 28, 54)), trim=False)


def round_button(top, bottom, lip, size=128):
    def fn(d, s, im):
        S = size * s
        d.ellipse((0, 8 * s, S - 1, S - 1), fill=(20, 12, 28, 255))
        d.ellipse((5 * s, 10 * s, S - 5 * s, S - 6 * s), fill=lip + (255,))
        g = Image.new("RGBA", (S, S))
        gd = ImageDraw.Draw(g)
        for y in range(S):
            t = y / (S - 1)
            gd.line((0, y, S, y), fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)) + (255,))
        m = Image.new("L", (S, S), 0)
        ImageDraw.Draw(m).ellipse((5 * s, 5 * s, S - 5 * s, S - 14 * s), fill=255)
        im.paste(g, (0, 0), m)
        d.ellipse((0, 0, S - 1, S - 9 * s), outline=(20, 12, 28, 255), width=5 * s)
        gl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        ImageDraw.Draw(gl).ellipse((16 * s, 9 * s, S - 16 * s, int(S * 0.42)), fill=(255, 255, 255, 70))
        im.alpha_composite(gl)
    return smooth(fn, size, size)


add("ui/round", round_button((128, 112, 200), (82, 66, 150), (52, 38, 100)), trim=False)
add("ui/round_gold", round_button((255, 222, 96), (240, 150, 30), (170, 90, 20)), trim=False)
add("ui/round_green", round_button((168, 236, 96), (82, 176, 56), (40, 110, 34)), trim=False)


def paper_panel(w=128, h=128, r=26):
    """Пергаментное окно: тёплая бумага, двойная рамка, лёгкая текстура."""
    rr = random.Random(5)
    def fn(d, s, im):
        d.rounded_rectangle((0, 0, w * s - 1, h * s - 1), r * s, fill=(36, 22, 30, 255))
        d.rounded_rectangle((5 * s, 5 * s, w * s - 1 - 5 * s, h * s - 1 - 5 * s), (r - 4) * s, fill=(146, 98, 58, 255))
        inner = (11 * s, 11 * s, w * s - 1 - 11 * s, h * s - 1 - 11 * s)
        d.rounded_rectangle(inner, (r - 9) * s, fill=(248, 232, 196, 255))
        tex = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
        td = ImageDraw.Draw(tex)
        for _ in range(500):
            x, y = rr.uniform(inner[0], inner[2]), rr.uniform(inner[1], inner[3])
            c = rr.choice([(210, 180, 130, 40), (255, 250, 230, 50)])
            rad = rr.uniform(2, 7) * s
            td.ellipse((x - rad, y - rad, x + rad, y + rad), fill=c)
        m = Image.new("L", (w * s, h * s), 0)
        ImageDraw.Draw(m).rounded_rectangle(inner, (r - 9) * s, fill=255)
        tex = tex.filter(ImageFilter.GaussianBlur(3 * s))
        clipped = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
        clipped.paste(tex, (0, 0), m)
        im.alpha_composite(clipped)
        d.rounded_rectangle((8 * s, 8 * s, w * s - 1 - 8 * s, h * s - 1 - 8 * s), (r - 6) * s, outline=(196, 148, 90, 255), width=2 * s)
    return smooth(fn, w, h)


add("ui/paper", paper_panel(), trim=False)
add("ui/card", rounded_box(96, 96, 18, (255, 246, 222), (232, 214, 176), (120, 84, 50), 3, (255, 255, 240), gloss=False), trim=False)
add("ui/card_dark", rounded_box(96, 96, 18, (190, 176, 150), (160, 146, 120), (90, 66, 44), 3, gloss=False), trim=False)
add("ui/ribbon", rounded_box(128, 48, 14, (120, 70, 180), (80, 40, 130), (24, 12, 40), 4, (190, 150, 240)), trim=False)
add("ui/bar_bg", rounded_box(48, 24, 12, (20, 14, 30), (34, 26, 50), (10, 6, 16), 2, gloss=False), trim=False)
add("ui/bar_fill", rounded_box(48, 20, 10, (255, 255, 255), (200, 200, 200), (255, 255, 255), 1), trim=False)
frame = smooth(lambda d, s, im: d.rounded_rectangle((6 * s, 6 * s, 90 * s, 90 * s), 22 * s, outline=(255, 255, 255, 255), width=5 * s), 96, 96)
add("ui/glow_frame", frame.filter(ImageFilter.GaussianBlur(3)), trim=False)


def icon(name, fn, size=128):
    if name in ICON3D:
        return
    add("icon/" + name, smooth(fn, size, size), trim=False)


for _n, _img in ICON3D.items():
    add("icon/" + _n, _img)


def outlined_poly(d, pts, fill, s, outline=(30, 18, 40), w=7):
    d.polygon(pts, fill=outline + (255,))
    cx = sum(p[0] for p in pts) / len(pts)
    cy = sum(p[1] for p in pts) / len(pts)
    inner = [(cx + (x - cx) * 0.86, cy + (y - cy) * 0.86) for x, y in pts]
    d.polygon(inner, fill=fill + (255,))


def ore(d, s, im):
    S = 128 * s
    pts = [(S * .2, S * .55), (S * .35, S * .25), (S * .62, S * .18), (S * .84, S * .42), (S * .78, S * .76), (S * .45, S * .86), (S * .18, S * .74)]
    outlined_poly(d, pts, (238, 150, 70), s)
    d.polygon([(S * .38, S * .3), (S * .6, S * .25), (S * .55, S * .45), (S * .36, S * .5)], fill=(255, 205, 140, 255))
    d.polygon([(S * .5, S * .62), (S * .72, S * .55), (S * .7, S * .72), (S * .5, S * .78)], fill=(190, 100, 40, 255))


def aether(d, s, im):
    S = 128 * s
    d.ellipse((S * .18, S * .36, S * .82, S * .92), fill=(30, 18, 40, 255))
    d.polygon([(S * .5, S * .04), (S * .8, S * .58), (S * .2, S * .58)], fill=(30, 18, 40, 255))
    d.ellipse((S * .25, S * .42, S * .75, S * .86), fill=(110, 220, 255, 255))
    d.polygon([(S * .5, S * .14), (S * .72, S * .58), (S * .28, S * .58)], fill=(110, 220, 255, 255))
    d.ellipse((S * .34, S * .5, S * .48, S * .66), fill=(230, 250, 255, 255))


def shard(d, s, im):
    S = 128 * s
    outlined_poly(d, [(S * .5, S * .06), (S * .86, S * .45), (S * .5, S * .94), (S * .14, S * .45)], (255, 214, 90), s)
    d.polygon([(S * .5, S * .16), (S * .74, S * .45), (S * .5, S * .45)], fill=(255, 246, 200, 255))


def star(d, s, im, col=(255, 214, 90)):
    S = 128 * s
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = S * (0.46 if i % 2 == 0 else 0.2)
        pts.append((S / 2 + math.cos(a) * r, S * 0.53 + math.sin(a) * r))
    outlined_poly(d, pts, col, s)


def hammer(d, s, im):
    S = 128 * s
    d.line((S * .3, S * .85, S * .62, S * .42), fill=(30, 18, 40, 255), width=int(22 * s))
    d.line((S * .3, S * .85, S * .62, S * .42), fill=(170, 110, 60, 255), width=int(12 * s))
    outlined_poly(d, [(S * .4, S * .3), (S * .62, S * .1), (S * .9, S * .38), (S * .68, S * .58)], (190, 196, 214), s)


def sword(d, s, im):
    S = 128 * s
    d.line((S * .22, S * .82, S * .8, S * .2), fill=(30, 18, 40, 255), width=int(24 * s))
    d.line((S * .26, S * .78, S * .8, S * .2), fill=(220, 226, 240, 255), width=int(12 * s))
    d.line((S * .2, S * .58, S * .44, S * .84), fill=(30, 18, 40, 255), width=int(22 * s))
    d.line((S * .22, S * .6, S * .42, S * .82), fill=(230, 170, 60, 255), width=int(12 * s))


def house(d, s, im):
    S = 128 * s
    outlined_poly(d, [(S * .2, S * .5), (S * .5, S * .16), (S * .8, S * .5), (S * .8, S * .88), (S * .2, S * .88)], (240, 220, 180), s)
    d.polygon([(S * .14, S * .52), (S * .5, S * .12), (S * .86, S * .52), (S * .76, S * .52), (S * .5, S * .26), (S * .24, S * .52)], fill=(200, 70, 60, 255))
    d.rectangle((S * .44, S * .62, S * .58, S * .86), fill=(120, 70, 40, 255))


def scroll(d, s, im):
    S = 128 * s
    outlined_poly(d, [(S * .2, S * .18), (S * .78, S * .18), (S * .8, S * .84), (S * .22, S * .84)], (240, 222, 176), s)
    for k in range(4):
        y = S * (.34 + k * .12)
        d.line((S * .32, y, S * .68, y), fill=(150, 120, 80, 255), width=int(5 * s))


def moon(d, s, im):
    S = 128 * s
    d.ellipse((S * .14, S * .12, S * .86, S * .84), fill=(30, 18, 40, 255))
    d.ellipse((S * .2, S * .18, S * .8, S * .78), fill=(220, 210, 255, 255))
    d.ellipse((S * .38, S * .08, S * .98, S * .68), fill=(0, 0, 0, 0))


def mapi(d, s, im):
    S = 128 * s
    outlined_poly(d, [(S * .12, S * .24), (S * .38, S * .14), (S * .62, S * .24), (S * .88, S * .14), (S * .88, S * .78), (S * .62, S * .88), (S * .38, S * .78), (S * .12, S * .88)], (200, 220, 160), s)
    d.line((S * .3, S * .6, S * .45, S * .45, S * .6, S * .55, S * .72, S * .36), fill=(200, 60, 60, 255), width=int(5 * s))


def gear(d, s, im):
    S = 128 * s
    pts = []
    for i in range(16):
        a = i * math.pi / 8
        r = S * (0.42 if i % 2 == 0 else 0.32)
        pts.append((S / 2 + math.cos(a) * r, S / 2 + math.sin(a) * r))
    outlined_poly(d, pts, (190, 196, 214), s)
    d.ellipse((S * .38, S * .38, S * .62, S * .62), fill=(30, 18, 40, 255))


def up(d, s, im):
    S = 128 * s
    outlined_poly(d, [(S * .5, S * .1), (S * .88, S * .5), (S * .64, S * .5), (S * .64, S * .9), (S * .36, S * .9), (S * .36, S * .5), (S * .12, S * .5)], (120, 230, 90), s)


def clock(d, s, im):
    S = 128 * s
    d.ellipse((S * .1, S * .1, S * .9, S * .9), fill=(30, 18, 40, 255))
    d.ellipse((S * .17, S * .17, S * .83, S * .83), fill=(240, 236, 220, 255))
    d.line((S * .5, S * .5, S * .5, S * .26), fill=(30, 18, 40, 255), width=int(7 * s))
    d.line((S * .5, S * .5, S * .68, S * .58), fill=(30, 18, 40, 255), width=int(7 * s))


def close(d, s, im):
    S = 128 * s
    for w, c in ((26, (30, 18, 40)), (14, (255, 255, 255))):
        d.line((S * .22, S * .22, S * .78, S * .78), fill=c + (255,), width=int(w * s))
        d.line((S * .78, S * .22, S * .22, S * .78), fill=c + (255,), width=int(w * s))


def eclipse(d, s, im):
    S = 128 * s
    d.ellipse((S * .1, S * .1, S * .9, S * .9), fill=(160, 110, 240, 255))
    d.ellipse((S * .2, S * .2, S * .8, S * .8), fill=(12, 6, 24, 255))


def flag(d, s, im):
    S = 128 * s
    d.line((S * .28, S * .1, S * .28, S * .92), fill=(30, 18, 40, 255), width=int(14 * s))
    outlined_poly(d, [(S * .3, S * .14), (S * .84, S * .26), (S * .3, S * .52)], (240, 240, 250), s)


def lantern(d, s, im):
    S = 128 * s
    outlined_poly(d, [(S * .3, S * .3), (S * .7, S * .3), (S * .74, S * .82), (S * .26, S * .82)], (255, 200, 90), s)
    d.ellipse((S * .4, S * .44, S * .6, S * .72), fill=(255, 250, 220, 255))
    d.polygon([(S * .24, S * .32), (S * .5, S * .1), (S * .76, S * .32)], fill=(90, 70, 140, 255))


def heal(d, s, im):
    S = 128 * s
    d.ellipse((S * .08, S * .08, S * .92, S * .92), fill=(30, 18, 40, 255))
    d.ellipse((S * .15, S * .15, S * .85, S * .85), fill=(110, 220, 120, 255))
    outlined_poly(d, [(S * .42, S * .24), (S * .58, S * .24), (S * .58, S * .42), (S * .76, S * .42), (S * .76, S * .58), (S * .58, S * .58),
                      (S * .58, S * .76), (S * .42, S * .76), (S * .42, S * .58), (S * .24, S * .58), (S * .24, S * .42), (S * .42, S * .42)], (250, 255, 240), s)


def rage(d, s, im):
    S = 128 * s
    d.ellipse((S * .08, S * .08, S * .92, S * .92), fill=(30, 18, 40, 255))
    d.ellipse((S * .15, S * .15, S * .85, S * .85), fill=(200, 50, 60, 255))
    pts = [(S * .5, S * .18), (S * .64, S * .4), (S * .7, S * .3), (S * .76, S * .56), (S * .66, S * .8), (S * .34, S * .8), (S * .24, S * .56),
           (S * .32, S * .36), (S * .4, S * .46)]
    outlined_poly(d, pts, (255, 190, 70), s)
    d.polygon([(S * .5, S * .48), (S * .6, S * .66), (S * .54, S * .78), (S * .46, S * .78), (S * .4, S * .66)], fill=(255, 245, 200, 255))


def flash(d, s, im):
    S = 128 * s
    d.ellipse((S * .08, S * .08, S * .92, S * .92), fill=(30, 18, 40, 255))
    d.ellipse((S * .15, S * .15, S * .85, S * .85), fill=(255, 196, 70, 255))
    pts = []
    for i in range(16):
        a = i * math.pi / 8
        r = S * (0.33 if i % 2 == 0 else 0.15)
        pts.append((S / 2 + math.cos(a) * r, S / 2 + math.sin(a) * r))
    outlined_poly(d, pts, (255, 252, 225), s)


def chat(d, s, im):
    S = 128 * s
    outlined_poly(d, [(S * .14, S * .2), (S * .86, S * .2), (S * .86, S * .66), (S * .46, S * .66), (S * .26, S * .86), (S * .3, S * .66),
                      (S * .14, S * .66)], (245, 240, 225), s)
    for k in range(3):
        x = S * (.34 + k * .16)
        d.ellipse((x - S * .045, S * .4, x + S * .045, S * .49), fill=(70, 60, 110, 255))


for n, f in [("flash", flash), ("chat", chat), ("heal", heal), ("rage", rage), ("ore", ore), ("aether", aether), ("shard", shard), ("star", star), ("builder", hammer), ("army", sword),
             ("build", house), ("codex", scroll), ("night", moon), ("map", mapi), ("menu", gear), ("upgrade", up),
             ("clock", clock), ("close", close), ("eclipse", eclipse), ("flag", flag), ("lantern", lantern)]:
    icon(n, f)
icon("star_off", lambda d, s, im: star(d, s, im, (70, 60, 90)))

# ---------- портреты персонажей (art/source/portraits, сгенерированы в Higgsfield) ----------
def portrait(path, size=300, radius=40):
    src = Image.open(path).convert("RGBA").resize((size - 18, size - 18), Image.LANCZOS)
    out = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    big = size * SS

    def rr(inset, fill):
        m = Image.new("L", (big, big), 0)
        ImageDraw.Draw(m).rounded_rectangle((inset * SS, inset * SS, big - inset * SS, big - inset * SS), radius * SS - inset * SS, fill=255)
        layer = Image.new("RGBA", (size, size), fill)
        layer.putalpha(m.resize((size, size), Image.LANCZOS))
        return layer
    out.alpha_composite(rr(0, (30, 18, 40, 255)))
    out.alpha_composite(rr(3, (232, 182, 84, 255)))
    out.alpha_composite(rr(7, (60, 36, 20, 255)))
    pic = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    pic.paste(src, (9, 9))
    m = rr(9, (0, 0, 0, 255)).getchannel("A")
    pic.putalpha(Image.composite(pic.getchannel("A"), Image.new("L", (size, size), 0), m))
    out.alpha_composite(pic)
    return out


PORTRAITS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "source", "portraits")
for png in sorted(glob.glob(os.path.join(PORTRAITS, "*.png"))):
    add("portrait/" + os.path.basename(png)[:-4], portrait(png), trim=False)

# ---------- упаковка (полки по высоте) ----------
sprites.sort(key=lambda t: (-t[1].height, -t[1].width))
pages = []
placements = []
cur = None
x = y = shelf_h = 0
for name, img, px, py in sprites:
    w, h = img.width + PAD * 2, img.height + PAD * 2
    if cur is None or x + w > PAGE:
        x = 0
        y += shelf_h
        shelf_h = 0
    if cur is None or y + h > PAGE:
        cur = Image.new("RGBA", (PAGE, PAGE), (0, 0, 0, 0))
        pages.append(cur)
        x = y = shelf_h = 0
    cur.paste(img, (x + PAD, y + PAD))
    placements.append((name, len(pages) - 1, x + PAD, y + PAD, img.width, img.height, px, py))
    x += w
    shelf_h = max(shelf_h, h)

os.makedirs(DST, exist_ok=True)
for f in glob.glob(os.path.join(DST, "page*.png")):
    os.remove(f)
with open(os.path.join(DST, "atlas.txt"), "w") as f:
    for i, p in enumerate(pages):
        # последняя страница обрезается по высоте занятого места
        if i == len(pages) - 1:
            bbox = p.getbbox()
            if bbox:
                hgt = 1 << math.ceil(math.log2(max(64, bbox[3])))
                p = p.crop((0, 0, PAGE, min(PAGE, hgt)))
        p.save(os.path.join(DST, f"page{i}.png"), optimize=True)
        f.write(f"page {i} page{i}.png\n")
    for name, pg, sx, sy, w, h, px, py in placements:
        f.write(f"sprite {name} {pg} {sx} {sy} {w} {h} {px:.1f} {py:.1f}\n")
total = sum(os.path.getsize(p) for p in glob.glob(os.path.join(DST, "page*.png")))
print(f"{len(placements)} sprites, {len(pages)} pages, {total / 1e6:.2f} MB")
