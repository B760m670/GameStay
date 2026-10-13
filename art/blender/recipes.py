"""Рецепты спрайтов: из каких деталей Kenney (CC0) собирается каждая постройка и юнит.

Координаты деталей — в метрах Blender (1 м = клетка набора Kenney), камера смотрит с угла +X/-Y,
то есть видимые фасады — стороны +X и -Y. Модель потом масштабируется под размер постройки в клетках.
"""

KITS = {
    "castle": "kenney_castle-kit/Models/GLB format",
    "town": "kenney_fantasy-town-kit_2.0/Models/GLB format",
    "td": "kenney_tower-defense-kit/Models/GLB format",
    "survival": "kenney_survival-kit/Models/GLB format",
    "grave": "kenney_graveyard-kit_5.0/Models/GLB format",
    "chars": "kenney_mini-characters/Models/GLB format",
}

WARM = (1.0, 0.72, 0.32)
AETHER = (0.45, 0.85, 1.0)


def P(kit, model, at=(0, 0), z=0, rot=0, scale=1.0):
    return {"kit": kit, "model": model, "at": at, "z": z, "rot": rot, "scale": scale}


def house(cells_x=2, cells_y=2, storeys=1, front="town", roof="roof-gable"):
    """Дом из стен fantasy-town: фасады +X и -Y с окнами и дверью, крыша по ячейкам."""
    parts = []
    xs = [i - (cells_x - 1) / 2 for i in range(cells_x)]
    ys = [j - (cells_y - 1) / 2 for j in range(cells_y)]
    for s in range(storeys):
        z = s * 1.0
        for x in xs:
            # -Y фасад (виден) и +Y (задний)
            parts.append(P(front, "wall-window-shutters" if s else ("wall-door" if x == xs[-1] else "wall-window-glass"), (x, ys[0]), z, 270))
            parts.append(P(front, "wall", (x, ys[-1]), z, 90))
        for y in ys:
            parts.append(P(front, "wall-window-shutters" if (s or y != ys[0]) else "wall-window-glass", (xs[-1], y), z, 0))
            parts.append(P(front, "wall", (xs[0], y), z, 180))
    for x in xs:
        for y in ys:
            parts.append(P(front, roof, (x, y), storeys * 1.0, 90))
    return parts


BUILDINGS = {
    "lantern": {
        "size": 4, "fill": 0.95,
        "parts": [
            P("castle", "tower-square-base", (0.5, 0.5)), P("castle", "tower-square-base", (-0.5, 0.5)),
            P("castle", "tower-square-base", (0.5, -0.5)), P("castle", "tower-square-base", (-0.5, -0.5)),
            P("castle", "tower-square-mid-windows", (0, 0), 1.0, 0, 1.55),
            P("castle", "tower-square-top-roof-high-windows", (0, 0), "top", 0, 1.55),
            P("grave", "lightpost-single", (1.35, -1.35), 0, 0, 0.9),
            P("grave", "lightpost-single", (-1.35, -1.35), 0, 0, 0.9),
            P("grave", "lightpost-single", (1.35, 1.35), 0, 0, 0.9),
            P("grave", "lightpost-single", (-1.35, 1.35), 0, 0, 0.9),
            P("castle", "flag-banner-short", (0.62, -1.02), 1.0, -90, 1.0),
        ],
        "glow": [{"h": 0.5, "r": 0.1, "x": 0.55, "y": -0.55, "color": WARM, "strength": 25, "light": 200}],
    },
    "beacon": {
        "size": 2, "fill": 0.85,
        "parts": [P("td", "tower-round-bottom-a"), P("td", "tower-round-middle-a", z="top"), P("td", "tower-round-crystals", z="top")],
        "glow": [{"h": 0.9, "r": 0.12, "color": AETHER, "strength": 10, "light": 60}],
    },
    "ballista": {
        "size": 3, "fill": 0.86,
        "parts": [P("castle", "tower-hexagon-base"), P("castle", "tower-hexagon-mid", z="top"),
                  P("castle", "siege-ballista", z="top", rot=-45, scale=0.75)],
    },
    "barracks": {"size": 3, "fill": 0.9, "parts": house(2, 2, 2, roof="roof-gable") + [P("castle", "flag-banner-long", (1.15, -1.15))]},
    "aetherWell": {
        "size": 3, "fill": 0.9,
        "parts": [P("town", "fountain-round"), P("town", "fountain-center", z=0)],
        "glow": [{"h": 0.35, "r": 0.05, "color": AETHER, "strength": 4, "light": 40}],
    },
    "wall": {"size": 1, "fill": 1.0, "parts": [P("castle", "wall-half")]},
}

UNITS = {
    "torch": {"kit": "chars", "model": "character-male-a", "height": 0.95, "dirs": 8, "anims": {"walk": [0.0, 0.5]}},
    "ghost": {"kit": "grave", "model": "character-ghost", "height": 0.95, "dirs": 8, "anims": {"idle": [0.0]}},
}
