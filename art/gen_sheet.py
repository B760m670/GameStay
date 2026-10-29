#!/usr/bin/env python3
"""Лист сгенерированного декора (несколько отдельных объектов на пурпурном фоне) -> отдельные спрайты.

Использование: python3 art/gen_sheet.py <raw> <префикс> <ширина_px> [out_dir] [масштабы через запятую]
Объекты нумеруются слева направо, сверху вниз: <out>/buildings/decor_<префикс><N>.png/.json.
<ширина_px> — ширина самого широкого объекта в игре (128 px = клетка); остальные масштабируются так же.
Pivot — точка касания земли: середина нижней кромки объекта.
"""
import json
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_sprite import cut  # noqa: E402


def components(alpha, min_area, erode=13):
    """Отдельные объекты листа. Маску сжимаем (рвём тонкие перемычки между соседними кронами) и находим ядра
    заливкой PIL; затем ядра одновременно разрастаются по исходной непрозрачности — каждый пиксель достаётся
    ближайшему по пути ядру (тонкие стволы возвращаются к своему дереву, а не к соседнему)."""
    from PIL import ImageFilter
    solid = alpha > 200  # ядра — только сам объект; мягкие тени (alpha до 0.6) присоединяются при разрастании
    mask = Image.fromarray(np.where(solid, 255, 0).astype(np.uint8)).filter(ImageFilter.MinFilter(erode)).copy()
    labels = np.zeros(alpha.shape, np.int32)
    n = 0
    a = np.asarray(mask)
    for y, x in zip(*np.nonzero(a[::4, ::4] == 255)):
        y, x = int(y * 4), int(x * 4)
        if mask.getpixel((x, y)) != 255:
            continue
        ImageDraw.floodfill(mask, (x, y), 100)  # fromarray в Pillow 12 — только чтение, поэтому .copy() выше
        core = np.asarray(mask) == 100
        ImageDraw.floodfill(mask, (x, y), 0)
        if core.sum() >= min_area // 4:
            n += 1
            labels[core] = n
    area = alpha > 0
    while True:
        grown = labels.copy()
        for dst, src in (((slice(1, None),), (slice(None, -1),)), ((slice(None, -1),), (slice(1, None),)),
                         ((slice(None), slice(1, None)), (slice(None), slice(None, -1))),
                         ((slice(None), slice(None, -1)), (slice(None), slice(1, None)))):
            take = (grown[dst] == 0) & (labels[src] > 0) & area[dst]
            grown[dst] = np.where(take, labels[src], grown[dst])
        if (grown == labels).all():
            break
        labels = grown
    found = []
    for k in range(1, n + 1):
        m = labels == k
        ys, xs = np.nonzero(m)
        found.append((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1, m))
    return found


def main():
    raw, prefix, width = sys.argv[1], sys.argv[2], float(sys.argv[3])
    out = sys.argv[4] if len(sys.argv) > 4 else "art/out"
    rel = [float(v) for v in sys.argv[5].split(",")] if len(sys.argv) > 5 else []  # свой масштаб каждого объекта
    im = cut(Image.open(raw), shadows=True)
    rgba = np.asarray(im)
    comps = components(rgba[..., 3], min_area=rgba.shape[0] * rgba.shape[1] // 400)
    # сначала строки (по нижней кромке с допуском), потом слева направо
    comps.sort(key=lambda c: (round(c[3] / (rgba.shape[0] / 4)), c[0]))
    scale = width / max(c[2] - c[0] for c in comps)
    os.makedirs(os.path.join(out, "buildings"), exist_ok=True)
    for i, (x0, y0, x1, y1, m) in enumerate(comps, 1):
        part = rgba[y0:y1, x0:x1].copy()
        part[..., 3] = np.where(m[y0:y1, x0:x1], part[..., 3], 0)
        sp = Image.fromarray(part, "RGBA")
        k = scale * (rel[i - 1] if i - 1 < len(rel) else 1.0)
        sp = sp.resize((max(1, round(sp.width * k)), max(1, round(sp.height * k))), Image.LANCZOS)
        al = np.asarray(sp)[..., 3]
        ys, xs = np.nonzero(al > 200)  # без мягкой тени
        bottom = ys.max()
        foot = xs[ys > bottom - max(3, sp.height * 0.08)]
        px, py = (foot.min() + foot.max()) / 2, bottom - sp.height * 0.03
        name = f"decor_{prefix}{i}"
        sp.save(os.path.join(out, "buildings", name + ".png"))
        with open(os.path.join(out, "buildings", name + ".json"), "w") as f:
            json.dump({"size": 1, "pivot": [round(float(px), 1), round(float(py), 1)], "tilePx": 128, "w": sp.width, "h": sp.height,
                       "source": "generated"}, f)
        print(f"{name}: {sp.size} pivot=({px:.0f},{py:.0f})")


if __name__ == "__main__":
    main()
