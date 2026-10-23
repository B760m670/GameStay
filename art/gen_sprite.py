#!/usr/bin/env python3
"""Сгенерированный рисунок постройки -> игровой спрайт: вырезать фон, найти ромб основания, подогнать масштаб.

Использование: python3 art/gen_sprite.py <raw.png> <имя> <клеток> [out_dir]
Фон — однотонный или плавный градиент (берётся по углам картинки). Результат: <out>/buildings/<имя>.png и .json
(pivot — центр ромба основания, как у спрайтов из Blender; 128 px на клетку).
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

TILE_PX = 128


def background_model(a):
    """Билинейная модель фона по четырём угловым окнам 6% картинки."""
    h, w, _ = a.shape
    k = max(8, int(min(h, w) * 0.06))
    c = [a[:k, :k].reshape(-1, 3).mean(0), a[:k, -k:].reshape(-1, 3).mean(0), a[-k:, :k].reshape(-1, 3).mean(0), a[-k:, -k:].reshape(-1, 3).mean(0)]
    yy, xx = np.mgrid[0:h, 0:w]
    u, v = (xx / (w - 1))[..., None], (yy / (h - 1))[..., None]
    top = c[0] * (1 - u) + c[1] * u
    bot = c[2] * (1 - u) + c[3] * u
    return top * (1 - v) + bot * v


def poly_model(a, mask):
    """Квадратичная модель фона по пикселям маски (устойчивая: два прохода с отбрасыванием выбросов)."""
    h, w, _ = a.shape
    yy, xx = np.mgrid[0:h, 0:w]
    u, v = xx / (w - 1) - 0.5, yy / (h - 1) - 0.5
    basis = np.stack([np.ones_like(u), u, v, u * u, v * v, u * v, u * u * v, u * v * v], -1)
    sel = mask.copy()
    for _ in range(2):
        idx = np.flatnonzero(sel)[::7]
        A = basis.reshape(-1, basis.shape[-1])[idx]
        coef, *_ = np.linalg.lstsq(A, a.reshape(-1, 3)[idx], rcond=None)
        model = basis @ coef
        err = np.sqrt(((a - model) ** 2).sum(2))
        sel = mask & (err < max(12.0, np.percentile(err[mask], 80)))
    return model


def flood_from_border(near):
    """Связная с краем картинки часть маски near (bool)."""
    from PIL import ImageDraw
    h, w = near.shape
    im = Image.fromarray(np.where(near, 0, 255).astype(np.uint8)).copy()  # fromarray в Pillow 12 — только чтение
    seeds = [(x, 0) for x in range(0, w, 24)] + [(x, h - 1) for x in range(0, w, 24)]
    seeds += [(0, y) for y in range(0, h, 24)] + [(w - 1, y) for y in range(0, h, 24)]
    for sx, sy in seeds:
        if im.getpixel((sx, sy)) == 0:
            ImageDraw.floodfill(im, (sx, sy), 128)
    return np.asarray(im) == 128


def hue_sat(x):
    mx, mn = x.max(2), x.min(2)
    d = mx - mn + 1e-6
    r, g, b = x[..., 0], x[..., 1], x[..., 2]
    hh = np.where(mx == r, (g - b) / d % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) * 60
    return hh, d / (mx + 1e-6)


def cut(img):
    """Фон = область, связанная с краем картинки и близкая к модели фона. Генератор рисует фон с градиентом и виньеткой,
    поэтому модель уточняется итерациями: заливка -> квадратичная модель по залитому -> снова заливка.
    Ореолы света — пиксели фонового оттенка светлее фона — восстанавливаются как полупрозрачное тёплое свечение."""
    rgb = img.convert("RGB")
    a = np.asarray(rgb, dtype=np.float32)
    sm = np.asarray(rgb.filter(ImageFilter.GaussianBlur(1.0)), dtype=np.float32)
    model = background_model(a)
    _, ssm = hue_sat(sm)
    for thr in (48, 40, 36):
        dist = np.sqrt(((sm - model) ** 2).sum(2))
        _, smod = hue_sat(model)
        # насыщенность не ниже фоновой: серый камень того же тона фоном не считается
        filled = flood_from_border((dist < thr) & (ssm > smod * 0.6))
        model = poly_model(sm, filled)
    dist = np.sqrt(((a - model) ** 2).sum(2))
    hb, sb = hue_sat(model)
    # проверка исходника: фон должен быть насыщенно-пурпурным везде, иначе он сливается с землёй и камнем
    off = np.abs((hb[filled] - 305 + 180) % 360 - 180)
    if np.percentile(off, 95) > 40 or np.percentile(sb[filled], 5) < 0.3:
        print(f"ВНИМАНИЕ: фон не пурпурный (отклонение оттенка {np.percentile(off, 95):.0f}°, насыщенность {np.percentile(sb[filled], 5):.2f})"
              " — вырезка будет грязной, лучше перегенерировать", file=sys.stderr)
    ha, sa = hue_sat(a)
    hd = np.abs((ha - hb + 180) % 360 - 180)
    vb_, va_ = model.max(2), a.max(2)
    # «карманы» фона внутри контура: точно цвет и оттенок фона (серый камень с розоватым отливом не трогаем)
    base = filled | ((dist < 14) & (hd < 30) & (sa > 0.25))
    background = base.copy()
    # ореол: цвет лежит на отрезке «фон -> тёплый свет» (свет фонаря, наложенный на фон)
    warm = np.array([255, 205, 130], dtype=np.float32)
    dv = warm - model
    tt = ((a - model) * dv).sum(2) / np.maximum((dv ** 2).sum(2), 1)
    resid = np.sqrt(((a - model - tt[..., None] * dv) ** 2).sum(2))
    cand = ((hd < 28) & (sa > 0.22) & (va_ > vb_ + 10)) | ((tt > 0.06) & (tt < 0.9) & (resid < 26))
    for _ in range(600):
        grown = background.copy()
        grown[1:] |= background[:-1]
        grown[:-1] |= background[1:]
        grown[:, 1:] |= background[:, :-1]
        grown[:, :-1] |= background[:, 1:]
        grown &= cand | background
        if (grown == background).all():
            break
        background = grown
    halo = background & (va_ > vb_ + 12)  # свет поверх фона — и внутри залитой области, и в разросшемся ореоле
    # бахрома: пиксели фонового оттенка у самой границы (кромка, смешанная с фоном)
    near_bg = background.copy()
    for _ in range(3):
        g2 = near_bg.copy()
        g2[1:] |= near_bg[:-1]; g2[:-1] |= near_bg[1:]; g2[:, 1:] |= near_bg[:, :-1]; g2[:, :-1] |= near_bg[:, 1:]
        near_bg = g2
    fringe = near_bg & ~background & (hd < 40) & (sa > 0.18) & (dist < 70)
    background = background | fringe
    alpha = np.where(background, 0.0, 1.0)
    m = Image.fromarray((alpha * 255).astype(np.uint8)).filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(0.8))
    alpha = np.asarray(m, dtype=np.float32) / 255
    edge = (alpha > 0) & (alpha < 1)
    a2 = a.copy()
    a2[edge] = np.clip((a[edge] - model[edge] * (1 - alpha[edge, None])) / np.maximum(alpha[edge, None], 0.3), 0, 255)
    dvec = warm - model[halo]
    ha_ = np.clip(((a[halo] - model[halo]) * dvec).sum(1) / np.maximum((dvec ** 2).sum(1), 1), 0, 1) * 0.8
    a2[halo] = warm
    alpha[halo] = np.maximum(alpha[halo], ha_)
    rgba = np.dstack([a2, alpha * 255]).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def base_diamond(alpha):
    """Левый и правый край основания: самые крайние непрозрачные точки в нижних 45% рисунка."""
    h, w = alpha.shape
    ys, xs = np.nonzero(alpha > 128)
    low = ys > ys.min() + (ys.max() - ys.min()) * 0.55
    ys, xs = ys[low], xs[low]
    li, ri = xs.argmin(), xs.argmax()
    return (xs[li], ys[li]), (xs[ri], ys[ri]), ys.max()


def main():
    raw, name, size = sys.argv[1], sys.argv[2], float(sys.argv[3])
    out = sys.argv[4] if len(sys.argv) > 4 else "art/out"
    fill = float(os.environ.get("FILL", "1.0"))
    im = cut(Image.open(raw))
    al = np.asarray(im)[..., 3]
    (lx, ly), (rx, ry), bottom = base_diamond(al)
    width = rx - lx
    scale = size * TILE_PX * fill / width
    cx, cy = (lx + rx) / 2, (ly + ry) / 2
    # проверка проекции: у ромба 2:1 высота от линии краёв до нижней вершины = половина полуширины
    ratio = (bottom - cy) / (width / 2)
    im = im.resize((max(1, int(im.width * scale)), max(1, int(im.height * scale))), Image.LANCZOS)
    bbox = im.getchannel("A").getbbox()
    im = im.crop(bbox)
    px, py = cx * scale - bbox[0], cy * scale - bbox[1]
    os.makedirs(os.path.join(out, "buildings"), exist_ok=True)
    path = os.path.join(out, "buildings", name + ".png")
    im.save(path)
    with open(path[:-4] + ".json", "w") as f:
        json.dump({"size": size, "pivot": [round(px, 1), round(py, 1)], "tilePx": TILE_PX, "w": im.width, "h": im.height, "source": "generated"}, f)
    print(f"{name}: {im.size} pivot=({px:.0f},{py:.0f}) diamond ratio={ratio:.2f} (2:1 -> 0.50)")


if __name__ == "__main__":
    main()
