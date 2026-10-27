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

for png in (sorted(glob.glob(os.path.join(SRC, "ui", "*.png"))) if os.environ.get("LF_UI3D") else []):  # старый набор: дерево и пергамент
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
    if name == "army":
        continue  # армию обозначает меч (рисуется ниже), а не молот
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


frame = smooth(lambda d, s, im: d.rounded_rectangle((6 * s, 6 * s, 90 * s, 90 * s), 22 * s, outline=(255, 255, 255, 255), width=5 * s), 96, 96)
add("ui/glow_frame", frame.filter(ImageFilter.GaussianBlur(3)), trim=False)


# ---------- интерфейс «ночное стекло» ----------
# Тёмно-индиговые полупрозрачные панели с тонкой светлой кромкой, тёплое золото как акцент,
# объёмные кнопки с нижней гранью. Размеры спрайтов согласованы с рамками draw9 в коде (панель 70, карточка 40, кнопка 56).
def vgrad(w, h, top, bottom):
    g = Image.new("RGBA", (w, h))
    gd = ImageDraw.Draw(g)
    for y in range(h):
        t = y / max(1, h - 1)
        gd.line((0, y, w, y), fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(4)))
    return g


def glass(w, h, r, top, bottom, rim, inset_line=None, shadow=True):
    def fn(d, s, im):
        W, H, R = w * s, h * s, r * s
        pad = 6 * s if shadow else 0
        if shadow:  # мягкая тень под панелью
            sh = Image.new("RGBA", (W, H), (0, 0, 0, 0))
            ImageDraw.Draw(sh).rounded_rectangle((pad, pad + 3 * s, W - pad, H - 1), R, fill=(4, 2, 12, 150))
            im.alpha_composite(sh.filter(ImageFilter.GaussianBlur(4 * s)))
        m = Image.new("L", (W, H), 0)
        ImageDraw.Draw(m).rounded_rectangle((pad, pad, W - pad - 1, H - pad - 1), R, fill=255)
        body = vgrad(W, H, top, bottom)
        layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        layer.paste(body, (0, 0), m)
        im.alpha_composite(layer)
        dd = ImageDraw.Draw(im)
        dd.rounded_rectangle((pad, pad, W - pad - 1, H - pad - 1), R, outline=(6, 4, 14, 230), width=2 * s)
        dd.rounded_rectangle((pad + 2 * s, pad + 2 * s, W - pad - 2 * s - 1, H - pad - 2 * s - 1), R - 2 * s, outline=rim, width=2 * s)
        # верхний блик стекла
        hl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        ImageDraw.Draw(hl).rounded_rectangle((pad + 4 * s, pad + 4 * s, W - pad - 4 * s, pad + R), R - 4 * s, fill=(255, 255, 255, 14))
        im.alpha_composite(hl)
        if inset_line:
            dd.rounded_rectangle((pad + 9 * s, pad + 9 * s, W - pad - 9 * s - 1, H - pad - 9 * s - 1), R - 8 * s, outline=inset_line, width=s)
    return smooth(fn, w, h)


def candy(top, bottom, lip, w=160, h=160, r=44, circle=False):
    """Объёмная кнопка: тёмный контур, нижняя грань (толщина), градиентное лицо, мягкий блик."""
    def fn(d, s, im):
        W, H, R, L = w * s, h * s, r * s, 12 * s
        shape = (lambda dr, box, **k: dr.ellipse(box, **k)) if circle else (lambda dr, box, **k: dr.rounded_rectangle(box, R, **k))
        shape(d, (0, L, W - 1, H - 1), fill=(10, 6, 20, 255))
        shape(d, (3 * s, L, W - 3 * s - 1, H - 3 * s - 1), fill=lip + (255,))
        m = Image.new("L", (W, H), 0)
        shape(ImageDraw.Draw(m), (3 * s, 3 * s, W - 3 * s - 1, H - L - 3 * s), fill=255)
        face = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        face.paste(vgrad(W, H, top + (255,), bottom + (255,)), (0, 0), m)
        im.alpha_composite(face)
        shape(ImageDraw.Draw(im), (0, 0, W - 1, H - L), outline=(10, 6, 20, 255), width=3 * s)
        gl = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        if circle:
            ImageDraw.Draw(gl).ellipse((W * 0.2, 7 * s, W * 0.8, H * 0.4), fill=(255, 255, 255, 60))
        else:
            ImageDraw.Draw(gl).rounded_rectangle((9 * s, 7 * s, W - 9 * s, H * 0.36), R - 8 * s, fill=(255, 255, 255, 52))
        im.alpha_composite(gl.filter(ImageFilter.GaussianBlur(2 * s)))
    return smooth(fn, w, h)


BLUE = ((126, 178, 255), (66, 110, 226), (38, 62, 150))
GOLD = ((255, 222, 120), (246, 166, 44), (168, 96, 18))
GREEN = ((150, 232, 110), (58, 178, 74), (30, 112, 46))
RED = ((255, 128, 112), (222, 62, 64), (140, 28, 38))
DARK = ((88, 78, 128), (56, 48, 90), (32, 26, 56))

add("ui/panel", glass(192, 192, 46, (44, 36, 80, 238), (26, 20, 50, 238), (150, 132, 230, 110)), trim=False)
add("ui/panel_dark", glass(192, 192, 46, (26, 21, 48, 228), (16, 12, 32, 228), (120, 104, 190, 80)), trim=False)
add("ui/paper", glass(192, 192, 46, (40, 33, 74, 246), (22, 17, 44, 246), (160, 140, 240, 110), inset_line=(255, 200, 110, 70)), trim=False)
add("ui/card", glass(128, 128, 30, (64, 53, 110, 255), (46, 37, 82, 255), (170, 150, 240, 90), shadow=False), trim=False)
add("ui/card_dark", glass(128, 128, 30, (34, 28, 58, 255), (26, 21, 46, 255), (100, 88, 150, 70), shadow=False), trim=False)
add("ui/ribbon", glass(128, 48, 20, (52, 42, 94, 240), (32, 26, 62, 240), (255, 200, 110, 120), shadow=False), trim=False)
add("ui/bar_bg", glass(96, 60, 28, (10, 7, 22, 235), (22, 17, 40, 235), (110, 96, 170, 70), shadow=False), trim=False)
def bar_fill(d, s, im):
    W, H = 96 * s, 52 * s
    d.rounded_rectangle((0, 0, W - 1, H - 1), 26 * s, fill=(236, 236, 236, 255))
    d.rounded_rectangle((6 * s, 5 * s, W - 6 * s, H * 0.42), 18 * s, fill=(255, 255, 255, 255))


add("ui/bar_fill", smooth(bar_fill, 96, 52), trim=False)
for nm, c in (("btn", BLUE), ("btn_gold", GOLD), ("btn_green", GREEN), ("btn_red", RED), ("btn_dark", DARK)):
    add("ui/" + nm, candy(*c), trim=False)
for nm, c in (("round", BLUE), ("round_gold", GOLD), ("round_green", GREEN), ("round_red", RED)):
    add("ui/" + nm, candy(*c, w=192, h=192, circle=True), trim=False)


def level_badge(d, s, im):
    """Значок уровня: золотое кольцо и синий самоцвет."""
    S = 192 * s
    d.ellipse((4 * s, 4 * s, S - 4 * s, S - 4 * s), fill=(10, 6, 20, 255))
    d.ellipse((10 * s, 10 * s, S - 10 * s, S - 10 * s), fill=(246, 176, 54, 255))
    d.ellipse((18 * s, 14 * s, S - 18 * s, S - 30 * s), fill=(255, 226, 130, 255))
    d.ellipse((26 * s, 26 * s, S - 26 * s, S - 26 * s), fill=(10, 6, 20, 255))
    g = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    m = Image.new("L", (S, S), 0)
    ImageDraw.Draw(m).ellipse((31 * s, 31 * s, S - 31 * s, S - 31 * s), fill=255)
    g.paste(vgrad(S, S, (110, 170, 255, 255), (52, 82, 200, 255)), (0, 0), m)
    im.alpha_composite(g)
    hl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(hl).ellipse((50 * s, 38 * s, S - 50 * s, S * 0.45), fill=(255, 255, 255, 60))
    im.alpha_composite(hl.filter(ImageFilter.GaussianBlur(3 * s)))


add("ui/badge", smooth(level_badge, 192, 192), trim=False)


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


def flag(d, s, im):
    S = 128 * s
    d.line((S * .28, S * .1, S * .28, S * .92), fill=(30, 18, 40, 255), width=int(14 * s))
    outlined_poly(d, [(S * .3, S * .14), (S * .84, S * .26), (S * .3, S * .52)], (240, 240, 250), s)


def heal(d, s, im):
    S = 128 * s
    d.ellipse((S * .08, S * .08, S * .92, S * .92), fill=(30, 18, 40, 255))
    d.ellipse((S * .15, S * .15, S * .85, S * .85), fill=(110, 220, 120, 255))
    outlined_poly(d, [(S * .42, S * .24), (S * .58, S * .24), (S * .58, S * .42), (S * .76, S * .42), (S * .76, S * .58), (S * .58, S * .58),
                      (S * .58, S * .76), (S * .42, S * .76), (S * .42, S * .58), (S * .24, S * .58), (S * .24, S * .42), (S * .42, S * .42)], (250, 255, 240), s)


def rage(d, s, im):
    """Боевой рог: изогнутый турий рог с окованным раструбом."""
    S = 128 * s
    d.ellipse((S * .08, S * .08, S * .92, S * .92), fill=(30, 18, 40, 255))
    d.ellipse((S * .15, S * .15, S * .85, S * .85), fill=(200, 50, 60, 255))
    top, bot = [], []
    for i in range(13):  # рог по дуге: узкий конец слева внизу, раструб справа вверху
        k = i / 12
        a = math.pi * (1.05 - 0.75 * k)
        cx, cy = S * .5 + math.cos(a) * S * .26, S * .66 - math.sin(a) * S * .3
        w = S * (.025 + .1 * k * k)
        nx, ny = math.cos(a), -math.sin(a)
        top.append((cx + nx * w, cy + ny * w))
        bot.append((cx - nx * w, cy - ny * w))
    outlined_poly(d, top + bot[::-1], (240, 222, 176), s)
    for k in (4, 8):  # оковки
        d.line((top[k], bot[k]), fill=(150, 110, 60, 255), width=int(6 * s))
    d.line((top[12], bot[12]), fill=(230, 180, 70, 255), width=int(9 * s))


def raid(d, s, im):
    """Набег: два скрещённых топора."""
    S = 128 * s
    for flip in (1, -1):
        x0, x1 = (S * .2, S * .8) if flip == 1 else (S * .8, S * .2)
        d.line((x0, S * .86, x1, S * .18), fill=(30, 18, 40, 255), width=int(20 * s))
        d.line((x0, S * .86, x1, S * .18), fill=(170, 110, 60, 255), width=int(9 * s))
        hx = S * .5 + (S * .2 if flip == 1 else -S * .2)
        outlined_poly(d, [(hx, S * .2), (hx + flip * S * .2, S * .1), (hx + flip * S * .24, S * .36), (hx + flip * S * .04, S * .38)], (200, 206, 222), s)


def fort(d, s, im):
    """Острог: рубленая башня с шатровой кровлей над частоколом."""
    S = 128 * s
    outlined_poly(d, [(S * .1, S * .9), (S * .1, S * .6), (S * .9, S * .6), (S * .9, S * .9)], (200, 140, 80), s)
    for k in range(5):
        x = S * (.1 + k * .2)
        d.polygon([(x, S * .62), (x + S * .1, S * .5), (x + S * .2, S * .62)], fill=(200, 140, 80, 255))
    outlined_poly(d, [(S * .32, S * .9), (S * .32, S * .36), (S * .68, S * .36), (S * .68, S * .9)], (214, 156, 94), s)
    outlined_poly(d, [(S * .26, S * .4), (S * .5, S * .06), (S * .74, S * .4)], (134, 86, 54), s)
    d.rectangle((S * .44, S * .5, S * .56, S * .62), fill=(40, 26, 20, 255))


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


def info(d, s, im):
    S = 128 * s
    d.ellipse((S * .08, S * .08, S * .92, S * .92), fill=(30, 18, 40, 255))
    d.ellipse((S * .15, S * .15, S * .85, S * .85), fill=(90, 150, 230, 255))
    d.ellipse((S * .43, S * .24, S * .57, S * .38), fill=(255, 255, 255, 255))
    d.rounded_rectangle((S * .43, S * .44, S * .57, S * .76), S * .04, fill=(255, 255, 255, 255))


def lock(d, s, im):
    S = 128 * s
    d.rounded_rectangle((S * .3, S * .1, S * .7, S * .6), S * .2, outline=(30, 18, 40, 255), width=int(22 * s))
    d.rounded_rectangle((S * .3, S * .1, S * .7, S * .6), S * .2, outline=(200, 205, 220, 255), width=int(10 * s))
    d.rounded_rectangle((S * .16, S * .42, S * .84, S * .92), S * .1, fill=(30, 18, 40, 255))
    d.rounded_rectangle((S * .22, S * .48, S * .78, S * .86), S * .07, fill=(240, 190, 80, 255))
    d.ellipse((S * .44, S * .56, S * .56, S * .68), fill=(60, 40, 30, 255))
    d.rectangle((S * .475, S * .62, S * .525, S * .78), fill=(60, 40, 30, 255))


def heart(d, s, im):
    S = 128 * s
    pts = []
    for i in range(64):
        t = i / 64 * 2 * math.pi
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((S / 2 + x * S * 0.026, S * 0.48 - y * S * 0.026))
    outlined_poly(d, pts, (235, 70, 80), s)
    d.ellipse((S * .3, S * .3, S * .42, S * .42), fill=(255, 200, 200, 255))


def person(d, s, im):
    S = 128 * s
    d.ellipse((S * .32, S * .08, S * .68, S * .44), fill=(30, 18, 40, 255))
    d.ellipse((S * .37, S * .13, S * .63, S * .39), fill=(240, 226, 200, 255))
    d.pieslice((S * .14, S * .46, S * .86, S * 1.3), 180, 360, fill=(30, 18, 40, 255))
    d.pieslice((S * .2, S * .52, S * .8, S * 1.22), 180, 360, fill=(120, 170, 230, 255))


for n, f in [("sword", sword), ("info", info), ("lock", lock), ("heart", heart), ("person", person), ("flash", flash), ("chat", chat), ("heal", heal), ("rage", rage), ("ore", ore), ("aether", aether), ("shard", shard), ("star", star), ("builder", hammer), ("army", sword),
             ("build", house), ("codex", scroll), ("night", raid), ("map", mapi), ("menu", gear), ("upgrade", up),
             ("clock", clock), ("close", close), ("flag", flag), ("lantern", fort)]:
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
