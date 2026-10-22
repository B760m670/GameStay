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
    "nature": "kenney_nature-kit/Models/GLTF format",
}

WARM = (1.0, 0.72, 0.32)
AETHER = (0.45, 0.85, 1.0)


def P(kit, model, at=(0, 0), z=0, rot=0, scale=1.0, turret=False):
    """Деталь постройки. turret=True — поворотная часть: рендерится в 8 направлениях (_d0.._d7)."""
    return {"kit": kit, "model": model, "at": at, "z": z, "rot": rot, "scale": scale, "turret": turret}


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


def lantern(t):
    parts = [P("castle", "tower-square-base", (x, y)) for x in (0.5, -0.5) for y in (0.5, -0.5)]
    parts.append(P("castle", "tower-square-mid-windows", (0, 0), 1.0, 0, 1.55))
    if t >= 2:
        parts.append(P("castle", "tower-square-mid-windows", (0, 0), "top", 0, 1.55))
    parts.append(P("castle", "tower-square-top-roof-high-windows", (0, 0), "top", 0, 1.55))
    posts = [(1.35, -1.35), (-1.35, -1.35)] + ([(1.35, 1.35), (-1.35, 1.35)] if t >= 2 else [])
    parts += [P("grave", "lightpost-double" if t >= 3 else "lightpost-single", p, 0, 0, 0.9) for p in posts]
    parts.append(P("castle", "flag-banner-short", (0.62, -1.02), 1.0, -90, 1.0))
    if t >= 3:
        parts += [P("castle", "tower-top", (x, y), 1.0, 0, 0.55) for x, y in ((0.85, 0.85), (-0.85, -0.85), (0.85, -0.85), (-0.85, 0.85))]
        parts.append(P("castle", "flag", (0, 0), "top", 0, 1.0))
    return {"size": 4, "fill": 0.95, "parts": parts,
            "glow": [{"h": 0.5, "r": 0.1, "x": 0.55, "y": -0.55, "color": WARM, "strength": 25, "light": 200}]}


def beacon(t):
    parts = [P("td", "tower-round-bottom-a")]
    parts += [P("td", "tower-round-middle-a", z="top") for _ in range(t - 1)]
    parts.append(P("td", "tower-round-crystals", z="top"))
    return {"size": 2, "fill": 0.85, "parts": parts, "glow": [{"h": 0.9, "r": 0.12, "color": AETHER, "strength": 10, "light": 60 + 30 * t}]}


def ballista(t):
    if t == 1:
        parts = [P("survival", "structure-floor", (0, 0), 0, 0, 1.6), P("castle", "siege-ballista", (0, 0), "top", -45, 1.1, True)]
    elif t == 2:
        parts = [P("castle", "tower-square-base"), P("castle", "tower-square-mid", z="top"), P("castle", "siege-ballista", z="top", rot=-45, scale=0.8, turret=True)]
    else:
        parts = [P("castle", "tower-hexagon-base"), P("castle", "tower-hexagon-mid", z="top"), P("castle", "tower-hexagon-mid", z="top"),
                 P("castle", "siege-ballista", z="top", rot=-45, scale=0.75, turret=True), P("castle", "flag-pennant", (0.35, 0.35), "top", 0, 0.8)]
    return {"size": 3, "fill": 0.86, "parts": parts}


def brazier(t):
    base = {1: [P("grave", "pillar-small", (0, 0), 0, 0, 1.4)], 2: [P("grave", "pillar-square", (0, 0), 0, 0, 1.4)],
            3: [P("castle", "tower-base", (0, 0), 0, 0, 1.0)]}[t]
    return {"size": 2, "fill": 0.8, "parts": base + [P("grave", "fire-basket", (0, 0), "top", 0, 1.4 + 0.3 * t)],
            "glow": [{"h": 0.92, "r": 0.09, "color": WARM, "strength": 20, "light": 60 + 25 * t}]}


def catapult(t):
    if t == 1:
        parts = [P("castle", "siege-catapult", (0, 0), 0, -45, 1.0, True)]
    elif t == 2:
        parts = [P("survival", "structure-floor", (0, 0), 0, 0, 1.4), P("castle", "siege-catapult", (0, 0), "top", -45, 1.0, True)]
    else:
        parts = [P("survival", "structure-floor", (0, 0), 0, 0, 1.6), P("castle", "siege-trebuchet", (0, 0), "top", -45, 1.0, True),
                 P("castle", "flag-pennant", (0.8, 0.8), 0, 0, 0.8)]
    return {"size": 3, "fill": 0.88, "parts": parts}


def prism(t):
    base = {1: [P("grave", "pillar-obelisk")], 2: [P("grave", "pillar-large")],
            3: [P("td", "tower-round-bottom-c"), P("td", "tower-round-middle-c", z="top")]}[t]
    return {"size": 2, "fill": 0.75, "parts": base + [P("td", "detail-crystal-large", (0, 0), "top", 0, 1.4 + 0.3 * t)],
            "glow": [{"h": 0.85, "r": 0.08, "color": (1.0, 0.45, 0.9), "strength": 10, "light": 50 + 20 * t}]}


def wall(t):
    model = {1: ("survival", "fence-fortified"), 2: ("castle", "wall-half"), 3: ("castle", "wall")}[t]
    return {"size": 1, "fill": 1.0, "parts": [P(model[0], model[1])]}


def ore_mine(t):
    parts = [P("survival", "structure-metal" if t >= 2 else "structure", (0, 0.15), 0, 0, 1.6),
             P("survival", "resource-stone-large", (0.45, -0.45), 0, 20, 2.2), P("survival", "resource-stone-large", (-0.4, -0.5), 0, 70, 1.9),
             P("survival", "tool-pickaxe", (0.05, -0.75), 0, 30, 2.0)]
    if t >= 2:
        parts.append(P("town", "cart", (-0.85, 0.6), 0, 200, 1.0))
    if t >= 3:
        parts += [P("survival", "resource-stone", (0.9, 0.5), 0, 0, 1.4), P("castle", "flag-pennant", (0.5, 0.6), 0, 0, 0.9)]
    return {"size": 3, "fill": 0.92, "parts": parts}


def aether_well(t):
    parts = [P("town", {1: "fountain-round", 2: "fountain-round-detail", 3: "fountain-square-detail"}[t])]
    if t >= 2:
        parts.append(P("td", "detail-crystal", (0, 0), 0.1, 0, 1.2 + 0.3 * t))
    return {"size": 3, "fill": 0.9, "parts": parts, "glow": [{"h": 0.35, "r": 0.05, "color": AETHER, "strength": 4, "light": 30 + 15 * t}]}


def ore_store(t):
    s = 1.5
    parts = [P("survival", "box-large", (0.35, 0.35), 0, 0, s), P("survival", "box-large", (-0.35, -0.35), 0, 0, s),
             P("survival", "barrel", (0.55, -0.55), 0, 0, 1.6)]
    if t >= 2:
        parts += [P("survival", "box-large", (0.35, -0.35), 0, 0, s), P("survival", "box-large", (-0.35, 0.35), 0, 0, s)]
    if t >= 3:
        parts += [P("survival", "box-large", (0, 0), "top", 0, s), P("survival", "resource-stone-large", (-0.7, 0.75), 0, 0, 1.4)]
    return {"size": 3, "fill": 0.88, "parts": parts}


def aether_store(t):
    parts = [P("town", "fountain-square-detail"), P("td", "detail-crystal-large", (0, 0), 0.15, 0, 1.4 + 0.3 * t)]
    if t >= 2:
        parts.append(P("td", "detail-crystal", (0.4, -0.3), 0.15, 30, 1.3))
    if t >= 3:
        parts.append(P("td", "detail-crystal", (-0.35, 0.35), 0.15, 70, 1.3))
    return {"size": 3, "fill": 0.9, "parts": parts, "glow": [{"h": 0.45, "r": 0.06, "color": AETHER, "strength": 6, "light": 40 + 15 * t}]}


def barracks(t):
    parts = {1: house(1, 2, 1), 2: house(2, 2, 1), 3: house(2, 2, 2)}[t]
    if t >= 2:
        parts = parts + [P("castle", "flag-banner-long", (1.15, -1.15))]
    return {"size": 3, "fill": 0.9, "parts": parts}


def camp(t):
    parts = [P("survival", "tent-canvas", (0.25, 0.3), 0, 200, 2.2), P("survival", "campfire-pit", (-0.35, -0.4), 0, 0, 1.8)]
    if t >= 2:
        parts.append(P("survival", "tent-canvas", (-0.45, 0.45), 0, 140, 1.8))
    if t >= 3:
        parts += [P("survival", "tent-canvas", (0.5, -0.4), 0, 250, 1.7), P("castle", "flag", (-0.8, -0.8), 0, 0, 0.9)]
    return {"size": 3, "fill": 0.92, "parts": parts,
            "glow": [{"h": 0.06, "r": 0.04, "x": -0.35, "y": -0.35, "color": WARM, "strength": 15, "light": 30}]}


def workshop(t):
    parts = [P("survival", "structure-roof" if t >= 2 else "structure", (0, 0.25), 0, 0, 1.5),
             P("survival", "workbench-anvil", (0.45, -0.45), 0, 30, 1.6), P("survival", "workbench", (-0.45, -0.4), 0, 0, 1.4),
             P("survival", "barrel", (0.75, 0.55), 0, 0, 1.2)]
    if t >= 3:
        parts += [P("castle", "flag-banner-short", (-0.8, 0.7), 0, 0, 0.9), P("survival", "tool-hammer-upgraded", (0.2, -0.85), 0, 60, 1.4)]
    return {"size": 3, "fill": 0.9, "parts": parts,
            "glow": [{"h": 0.25, "r": 0.04, "x": 0.45, "y": -0.45, "color": WARM, "strength": 10, "light": 25}]}


def trap(t):
    """Огненная ловушка: кострище в земле, тлеющие угли; выше уровень — железная решётка и шипы."""
    parts = [P("survival", "campfire-pit", (0, 0), 0, 0, 1.0)]
    if t >= 2:
        parts.append(P("survival", "metal-panel-screws-half", (0, 0), 0.05, 0, 0.55))
    if t >= 3:
        parts += [P("survival", "rock-a", (0.38, 0.3), 0, 30, 0.5), P("survival", "rock-c", (-0.35, -0.3), 0, 120, 0.5)]
    return {"size": 1, "fill": 0.85, "parts": parts,
            "glow": [{"h": 0.4, "r": 0.06, "color": (1.0, 0.45, 0.12), "strength": 12, "light": 25 + 10 * t}]}


GEN = {"workshop": workshop, "trap": trap, "lantern": lantern, "beacon": beacon, "ballista": ballista, "brazier": brazier, "catapult": catapult, "prism": prism,
       "wall": wall, "oreMine": ore_mine, "aetherWell": aether_well, "oreStore": ore_store, "aetherStore": aether_store,
       "barracks": barracks, "camp": camp}

BUILDINGS = {f"{name}_t{t}": fn(t) for name, fn in GEN.items() for t in (1, 2, 3)}
BUILDINGS["builderHut_t1"] = {"size": 2, "fill": 0.9, "parts": house(1, 1, 1, roof="roof-point") + [P("survival", "workbench-anvil", (0.95, -0.2), scale=0.7)]}

# Декор вокруг карты: деревья, скалы, кусты (рендерятся как постройки 1-2 клетки).
DECOR = {
    "oak": ("tree_oak", 2, 1.0), "oak_dark": ("tree_oak_dark", 2, 1.0), "fat": ("tree_fat", 2, 1.0),
    "detailed": ("tree_detailed", 2, 1.0), "pine": ("tree_pineTallA_detailed", 2, 0.9), "pine2": ("tree_pineRoundC", 2, 0.9),
    "cone": ("tree_cone_dark", 2, 0.8), "tall": ("tree_tall_dark", 2, 0.9),
    "rock_large": ("rock_largeA", 2, 1.0), "rock_tall": ("rock_tallB", 1, 1.0), "rock_small": ("rock_smallC", 1, 0.9),
    "bush": ("plant_bushLarge", 1, 1.0), "bush2": ("plant_bushDetailed", 1, 0.9), "stump": ("stump_oldTall", 1, 0.8),
    "mushrooms": ("mushroom_redGroup", 1, 0.7), "flowers": ("flower_yellowA", 1, 0.6), "logs": ("log_stack", 1, 1.0),
}
# Мелкие детали луга: трава, цветы, камешки, грибы.
DECOR.update({
    "grass": ("grass_large", 1, 0.45), "grass2": ("grass_leafsLarge", 1, 0.45), "grass3": ("grass", 1, 0.35),
    "fl_purple": ("flower_purpleA", 1, 0.3), "fl_red": ("flower_redB", 1, 0.3), "fl_yellow": ("flower_yellowC", 1, 0.3),
    "pebble": ("rock_smallFlatA", 1, 0.35), "pebble2": ("rock_smallB", 1, 0.3), "shroom": ("mushroom_tanGroup", 1, 0.35),
    "bush_small": ("plant_bushSmall", 1, 0.5),
})
# Обустройство окрестностей: мосты, кувшинки, поле, стоянка, руины (4-й элемент — поворот модели).
DECOR.update({
    "bridge_a": ("bridge_wood", 2, 0.95, 0), "bridge_b": ("bridge_wood", 2, 0.95, 90),
    "lily": ("lily_large", 1, 0.55), "lily_small": ("lily_small", 1, 0.4),
    "wheat": ("crops_wheatStageB", 1, 0.95), "corn": ("crops_cornStageD", 1, 0.9), "pumpkin": ("crop_pumpkin", 1, 0.45),
    "crop_row": ("crops_dirtRow", 1, 1.0), "fence_a": ("fence_simple", 1, 1.0, 0), "fence_b": ("fence_simple", 1, 1.0, 90),
    "tent": ("tent_detailedOpen", 2, 0.85, 200), "campfire": ("campfire_stones", 1, 0.7), "log_seat": ("log_large", 1, 0.9, 30),
    "ruin_ring": ("statue_ring", 2, 0.8), "ruin_column": ("statue_columnDamaged", 1, 0.6), "ruin_obelisk": ("statue_obelisk", 1, 0.55),
    "ruin_head": ("statue_head", 1, 0.6, 160), "sign": ("sign", 1, 0.5, 225), "stones": ("stone_tallC", 1, 0.6),
})

for _k, _v in DECOR.items():
    _m, _s, _f = _v[:3]
    BUILDINGS[f"decor_{_k}"] = {"size": _s, "fill": _f, "parts": [P("nature", _m, rot=_v[3] if len(_v) > 3 else 0)]}

MOVE8 = [i / 8 for i in range(8)]
ATK6 = [0.0, 0.18, 0.34, 0.5, 0.66, 0.84]
IDLE4 = [0.0, 0.25, 0.5, 0.75]
DIE5 = [0.0, 0.3, 0.55, 0.8, 1.0]


def unit(kit, model, height, move, attack, hold=(), **kw):
    """Стандартный набор анимаций юнита: move 8 кадров, attack 6, idle 4, die 5."""
    idle = move if move == "fall" else "idle"
    r = {"kit": kit, "model": model, "height": height, "hold": list(hold),
         "anims": {"move": (move, MOVE8), "attack": (attack, ATK6), "idle": (idle, IDLE4), "die": ("die", DIE5)}}
    r.update(kw)
    return r


R = (0, -90, 0)   # хват в правой руке: предмет вперёд-вниз в покое, заносится над головой при ударе
L = (0, 90, 0)
UNITS = {
    "worker": unit("chars", "character-male-b", 0.9, "walk", "interact-right", [{"item": "survival:tool-hammer", "rot": R, "scale": 1.7}]),
    "torch": unit("chars", "character-male-a", 0.95, "walk", "attack-melee-right", [{"item": "torch", "rot": R, "scale": 1.6}]),
    "archer": unit("chars", "character-female-a", 0.9, "walk", "holding-right-shoot", [{"item": "bow", "rot": R, "scale": 2.0}]),
    "shield": unit("chars", "character-male-c", 1.15, "walk", "attack-melee-right",
                   [{"item": "sword", "rot": R, "scale": 1.7}, {"item": "shield", "bone": "arm-left", "rot": (0, 0, 0), "off": (0, -0.04, 0), "scale": 1.5}]),
    "sapper": unit("chars", "character-male-e", 0.85, "sprint", "interact-right", [{"item": "bomb", "rot": R, "scale": 1.6}]),
    "shade": unit("grave", "character-vampire", 1.0, "sprint", "attack-melee-right",
                  [{"item": "dagger", "rot": R, "scale": 1.7, "color": (0.5, 0.1, 0.15)}]),
    "owl": unit("chars", "character-female-c", 0.9, "fall", "holding-both-shoot", [{"item": "staff", "rot": R, "scale": 1.5}], lift=0.0),
    "morok": unit("grave", "character-ghost", 0.85, "walk", "attack-melee-right"),
    "brute": unit("grave", "character-zombie", 1.35, "walk", "attack-melee-right", [{"item": "survival:tool-axe", "rot": R, "scale": 1.9}]),
    "leech": unit("grave", "character-skeleton", 0.95, "sprint", "attack-melee-left",
                  [{"item": "sword", "bone": "arm-left", "rot": L, "scale": 1.5, "color": (0.35, 0.3, 0.28)}]),
    "hero": unit("grave", "character-keeper", 1.2, "walk", "attack-melee-right", [{"item": "lanternstaff", "rot": (-90, 0, 0), "scale": 1.6}]),
    "wraith": unit("grave", "character-ghost", 1.25, "fall", "holding-both-shoot", [{"item": "darkstaff", "rot": R, "scale": 1.5}]),
}

ORE = (0.95, 0.42, 0.1)
GOLD = (1.0, 0.68, 0.1)
CYAN = (0.15, 0.72, 1.0)


def T(kit, model, at=(0, 0), z=0, rot=0, scale=1.0, tint=None, emit=0.0):
    d = P(kit, model, at, z, rot, scale)
    if tint:
        d["tint"] = tint
        d["emit"] = emit
    return d


ICONS = {
    "ore": {"parts": [T("survival", "resource-stone-large", tint=ORE, scale=1.0), T("survival", "resource-stone", (0.18, -0.2), tint=ORE, scale=0.7)]},
    "aether": {"parts": [T("td", "detail-crystal-large", tint=CYAN, emit=0.15)]},
    "shard": {"parts": [T("td", "detail-crystal", tint=GOLD, emit=0.1)], "yaw": 10},
    "builder": {"parts": [P("survival", "tool-hammer-upgraded", rot=0)], "yaw": 60},
    "army": {"parts": [P("survival", "tool-axe-upgraded")], "yaw": 60},
    "codex": {"parts": [P("survival", "chest")]},
    "map": {"parts": [P("castle", "flag-wide")], "yaw": 50},
    "lantern": {"parts": [T("grave", "lantern-glass", tint=GOLD, emit=0.05)]},
}
