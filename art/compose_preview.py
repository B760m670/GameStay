"""Собирает превью сцены из отрендеренных спрайтов в той же изометрии, что и игра (клетка 128x64)."""
import json, math, random, sys
from PIL import Image, ImageDraw, ImageFilter

OUT = sys.argv[1] if len(sys.argv) > 1 else "art/out/preview.png"
SP = "art/out"
TW, TH = 128, 64
N = 20                        # клеток по стороне
W, H = N * TW + 200, N * TH + 700
ox, oy = W // 2, 560


def scr(gx, gy):
    return ox + (gx - gy) * TW / 2, oy + (gx + gy) * TH / 2


def sprite(name):
    im = Image.open(f"{SP}/buildings/{name}.png").convert("RGBA")
    meta = json.load(open(f"{SP}/buildings/{name}.json"))
    return im, meta


rnd = random.Random(3)
img = Image.new("RGBA", (W, H), (14, 10, 28, 255))
ground = Image.new("RGBA", (W, H), (0, 0, 0, 0))
g = ImageDraw.Draw(ground)
for j in range(N):
    for i in range(N):
        x, y = scr(i, j)
        v = rnd.random()
        col = (int(78 + v * 18), int(128 + v * 22), int(62 + v * 12), 255)
        g.polygon([(x, y), (x + TW / 2, y + TH / 2), (x, y + TH), (x - TW / 2, y + TH / 2)], fill=col)
# мягкая трава: размыть шахматку, добавить пятна
ground = ground.filter(ImageFilter.GaussianBlur(6))
d = ImageDraw.Draw(ground)
for _ in range(900):
    i, j = rnd.uniform(0, N), rnd.uniform(0, N)
    x, y = scr(i, j)
    r = rnd.uniform(3, 9)
    d.ellipse((x - r, y - r / 2, x + r, y + r / 2), fill=(70 + rnd.randint(0, 40), 120 + rnd.randint(0, 50), 55, 140))
mask = Image.new("L", (W, H), 0)
x0, y0 = scr(0, 0); x1, y1 = scr(N, 0); x2, y2 = scr(N, N); x3, y3 = scr(0, N)
ImageDraw.Draw(mask).polygon([(x0, y0), (x1, y1), (x2, y2), (x3, y3)], fill=255)
img.paste(ground, (0, 0), mask)

items = []
def place(name, gx, gy):
    im, meta = sprite(name)
    s = meta["size"]
    cx, cy = scr(gx + s / 2, gy + s / 2)
    items.append((gx + gy + s, im, int(cx - meta["pivot"][0]), int(cy - meta["pivot"][1])))

def unit(name, anim, d, f, gx, gy):
    im = Image.open(f"{SP}/units/{name}/{anim}_{d}_{f}.png").convert("RGBA")
    meta = json.load(open(f"{SP}/units/{name}/meta.json"))
    cx, cy = scr(gx, gy)
    items.append((gx + gy + 0.01, im, int(cx - meta["pivot"][0]), int(cy - meta["pivot"][1])))

L = 8
place("lantern", L, L)
for k in range(-1, 5):
    for (a, b) in [(L + k, L - 1), (L + k, L + 4), (L - 1, L + k), (L + 4, L + k)]:
        place("wall", a, b)
place("wall", L + 4, L + 4); place("wall", L - 1, L + 4)
place("ballista", 13, 6)
place("barracks", 4, 13)
place("beacon", 14, 13)
place("aetherWell", 4, 5)
place("ballista", 9, 14)
for k, (gx, gy, d) in enumerate([(12.5, 11, 3), (11.5, 12.2, 2), (6.5, 7, 5), (16, 9, 6), (15.2, 10.4, 6)]):
    unit("torch", "walk", d, k % 2, gx, gy)
for gx, gy, d in [(18.6, 3, 5), (18.2, 4.4, 6), (1.5, 18.5, 1), (2.6, 19, 2)]:
    unit("ghost", "idle", d, 0, gx, gy)
items.sort(key=lambda t: t[0])

# Туман Мглы: темнота везде, кроме кругов света Фонаря и Светоча (эллипсы в изометрии)
fog = Image.new("L", (W, H), 200)
fd = ImageDraw.Draw(fog)
for gx, gy, r in [(L + 2, L + 2, 8.2), (15, 14, 5.0)]:
    cx, cy = scr(gx, gy)
    rx = r * TW / math.sqrt(2)
    fd.ellipse((cx - rx, cy - rx / 2, cx + rx, cy + rx / 2), fill=0)
fog = fog.filter(ImageFilter.GaussianBlur(28))
dark = Image.new("RGBA", (W, H), (16, 10, 36, 255))

for _, im, x, y in items:
    img.alpha_composite(im, (x, y))
img = Image.composite(dark, img, fog.point(lambda v: int(v * 0.85)))
# тёплое свечение света
glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
gd = ImageDraw.Draw(glow)
cx, cy = scr(L + 2, L + 2)
gd.ellipse((cx - 600, cy - 300, cx + 600, cy + 300), fill=(255, 170, 70, 38))
glow = glow.filter(ImageFilter.GaussianBlur(60))
img.alpha_composite(glow)
bb = (int(scr(0, N)[0]) - 60, 300, int(scr(N, 0)[0]) + 60, int(scr(N, N)[1]) + 40)
img = img.crop(bb).convert("RGB")
img.thumbnail((1600, 1600), Image.LANCZOS)
img.save(OUT)
print(OUT, img.size)
