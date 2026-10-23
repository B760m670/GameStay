#!/usr/bin/env python3
"""Лист сгенерированного декора (несколько отдельных объектов на пурпурном фоне) -> отдельные спрайты.

Использование: python3 art/gen_sheet.py <raw> <префикс> <ширина_px> [out_dir]
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


def components(alpha, min_area):
    """Связные области непрозрачности (заливкой PIL), крупнее min_area пикселей."""
    mask = Image.fromarray(np.where(alpha > 40, 255, 0).astype(np.uint8)).copy()
    found = []
    label = 1
    a = np.asarray(mask)
    for y, x in zip(*np.nonzero(a[::4, ::4] == 255)):
        y, x = y * 4, x * 4
        if mask.getpixel((int(x), int(y))) != 255:
            continue
        ImageDraw.floodfill(mask, (int(x), int(y)), label)
        m = np.asarray(mask) == label
        if m.sum() >= min_area:
            ys, xs = np.nonzero(m)
            found.append((xs.min(), ys.min(), xs.max() + 1, ys.max() + 1, m))
        label = label + 1 if label < 250 else 1
    return found


def main():
    raw, prefix, width = sys.argv[1], sys.argv[2], float(sys.argv[3])
    out = sys.argv[4] if len(sys.argv) > 4 else "art/out"
    im = cut(Image.open(raw))
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
        sp = sp.resize((max(1, round(sp.width * scale)), max(1, round(sp.height * scale))), Image.LANCZOS)
        al = np.asarray(sp)[..., 3]
        ys, xs = np.nonzero(al > 128)
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
