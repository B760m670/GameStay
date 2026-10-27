"""Рисованный стиль (как в стратегиях вроде Clash of Clans): чистые тёплые цвета, мягкие переходы, крупная фактура
вместо фототекстур. Геометрия остаётся собранной из деталей; меняются только материалы.

Включён по умолчанию; LF_STYLE=photo возвращает фототекстуры Poly Haven (для сравнения).
"""
import os

import bpy

STYLE = os.environ.get("LF_STYLE", "toon")


def on():
    return STYLE == "toon"


def lin(c):
    return tuple((v / 255) ** 2.2 for v in c[:3]) + (1.0,)


def _ramp(N, colors):
    r = N.new("ShaderNodeValToRGB")
    els = r.color_ramp.elements
    els[0].color = lin(colors[0])
    els[1].color = lin(colors[-1])
    for i, c in enumerate(colors[1:-1], 1):
        els.new(i / (len(colors) - 1)).color = lin(c)
    return r


def _mix(N, L, a, b, fac, blend="MIX"):
    m = N.new("ShaderNodeMix")
    m.data_type = "RGBA"
    m.blend_type = blend
    if isinstance(fac, float):
        m.inputs["Factor"].default_value = fac
    else:
        L.new(fac, m.inputs["Factor"])
    for sock, v in (("A", a), ("B", b)):
        if isinstance(v, tuple):
            m.inputs[sock].default_value = v
        else:
            L.new(v, m.inputs[sock])
    return m.outputs["Result"]


def _top_light(N, L, color, amount=0.18):
    """Грани, смотрящие вверх, чуть светлее — как подсвеченные края на рисованных спрайтах."""
    geo = N.new("ShaderNodeNewGeometry")
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(geo.outputs["Normal"], sep.inputs[0])
    mr = N.new("ShaderNodeMapRange")
    mr.inputs["From Min"].default_value = 0.2
    mr.inputs["From Max"].default_value = 1.0
    mr.inputs["To Min"].default_value = 0.0
    mr.inputs["To Max"].default_value = amount
    L.new(sep.outputs["Z"], mr.inputs["Value"])
    return _mix(N, L, color, (1.0, 0.96, 0.86, 1), mr.outputs["Result"], "SCREEN")


def _moss(N, L, color, moss):
    geo = N.new("ShaderNodeNewGeometry")
    nz = N.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = 2.5
    nz.inputs["Detail"].default_value = 3.0
    L.new(geo.outputs["Position"], nz.inputs["Vector"])
    sep = N.new("ShaderNodeSeparateXYZ")
    L.new(geo.outputs["Normal"], sep.inputs[0])
    up = N.new("ShaderNodeMath")
    up.operation = "MULTIPLY"
    L.new(nz.outputs["Fac"], up.inputs[0])
    L.new(sep.outputs["Z"], up.inputs[1])
    r = N.new("ShaderNodeValToRGB")
    r.color_ramp.elements[0].position = 0.6 - moss * 0.2
    r.color_ramp.elements[1].position = 0.66 - moss * 0.2
    L.new(up.outputs["Value"], r.inputs["Fac"])
    return _mix(N, L, color, lin((112, 158, 58)), r.outputs["Color"])


def _new(name):
    if name in bpy.data.materials:
        return None, bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    return m, m


def wood(name, colors, moss=0.0, grain=60.0, rough=0.75):
    """Дерево по UV брёвен/досок: цвет детали — из палитры (у каждой своя), крупные волокна вдоль, светлый верх."""
    fresh, m = _new(name)
    if not fresh:
        return m
    N, L = m.node_tree.nodes, m.node_tree.links
    b = N["Principled BSDF"]
    b.inputs["Roughness"].default_value = rough
    r = _ramp(N, colors)
    L.new(N.new("ShaderNodeObjectInfo").outputs["Random"], r.inputs["Fac"])
    wave = N.new("ShaderNodeTexWave")
    wave.bands_direction = "X"
    wave.inputs["Scale"].default_value = grain
    wave.inputs["Distortion"].default_value = 3.0
    wave.inputs["Detail"].default_value = 1.0
    L.new(N.new("ShaderNodeTexCoord").outputs["UV"], wave.inputs["Vector"])
    g = N.new("ShaderNodeMapRange")
    g.inputs["To Min"].default_value = 0.86
    g.inputs["To Max"].default_value = 1.06
    L.new(wave.outputs["Fac"], g.inputs["Value"])
    gc = N.new("ShaderNodeCombineColor")
    for k in ("Red", "Green", "Blue"):
        L.new(g.outputs["Result"], gc.inputs[k])
    color = _mix(N, L, r.outputs["Color"], gc.outputs["Color"], 1.0, "MULTIPLY")
    if moss > 0:
        color = _moss(N, L, color, moss)
    color = _top_light(N, L, color)
    L.new(color, b.inputs["Base Color"])
    bump = N.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.25
    L.new(wave.outputs["Fac"], bump.inputs["Height"])
    L.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


def solid(name, colors, rough=0.7, metal=0.0, noise=8.0, bump=0.3, moss=0.0, per_part=True, coords="Object"):
    """Камень, земля, ткань, металл: цвет из палитры, мягкие пятна шума, светлый верх."""
    fresh, m = _new(name)
    if not fresh:
        return m
    N, L = m.node_tree.nodes, m.node_tree.links
    b = N["Principled BSDF"]
    b.inputs["Roughness"].default_value = rough
    b.inputs["Metallic"].default_value = metal
    r = _ramp(N, colors)
    if per_part:
        L.new(N.new("ShaderNodeObjectInfo").outputs["Random"], r.inputs["Fac"])
    else:
        r.inputs["Fac"].default_value = 0.5
    color = r.outputs["Color"]
    if noise > 0:
        nz = N.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = noise
        nz.inputs["Detail"].default_value = 3.0
        L.new(N.new("ShaderNodeTexCoord").outputs[coords], nz.inputs["Vector"])
        g = N.new("ShaderNodeMapRange")
        g.inputs["To Min"].default_value = 0.88
        g.inputs["To Max"].default_value = 1.08
        L.new(nz.outputs["Fac"], g.inputs["Value"])
        gc = N.new("ShaderNodeCombineColor")
        for k in ("Red", "Green", "Blue"):
            L.new(g.outputs["Result"], gc.inputs[k])
        color = _mix(N, L, color, gc.outputs["Color"], 1.0, "MULTIPLY")
        if bump > 0:
            bp = N.new("ShaderNodeBump")
            bp.inputs["Strength"].default_value = bump
            L.new(nz.outputs["Fac"], bp.inputs["Height"])
            L.new(bp.outputs["Normal"], b.inputs["Normal"])
    if moss > 0:
        color = _moss(N, L, color, moss)
    color = _top_light(N, L, color, 0.12)
    L.new(color, b.inputs["Base Color"])
    return m


# Палитры вместо фототекстур Poly Haven (по имени ассета)
WOOD = {
    "weathered_planks": [(204, 146, 86), (188, 132, 76), (214, 156, 94), (196, 140, 82)],  # брёвна сруба
    "wood_planks_grey": [(218, 172, 112), (204, 158, 100), (226, 182, 120)],               # доски
}
ROOF = [(126, 78, 52), (112, 68, 46), (136, 86, 58)]  # тёс крыш — темнее стен, чтобы силуэт читался
SOLID = {
    "mossy_stone_wall": dict(colors=[(170, 160, 146), (152, 144, 134), (182, 170, 152), (160, 152, 142)], noise=10, bump=0.5, moss=0.4),
    "forest_ground_04": dict(colors=[(150, 116, 76), (138, 106, 70)], noise=6, bump=0.3),
}
SCAN_STONE = ("rock", "boulder", "stone_fire")
SCAN_BARK = ("tree_stump", "dead_tree", "bark_debris", "root_cluster", "dry_branches")
SCAN_LEAF = {"fern": (96, 168, 56), "grass": (124, 178, 62), "nettle": (90, 150, 52), "shrub": (88, 150, 58), "sapling": (70, 128, 66),
             "fir": (60, 116, 64), "pine": (74, 126, 60), "moss": (110, 160, 60)}


def restyle_scan(asset, objs):
    """Материалы скана -> рисованные: камень и кора — свои цвета; листва — цвет палитры с прозрачностью листьев из
    исходной текстуры; утварь (бочки, топоры, пушка) — исходная окраска, но насыщеннее и светлее."""
    for o in objs:
        for slot in o.material_slots:
            src = slot.material
            if src is None:
                continue
            key = f"toon_{asset}_{src.name}"
            if key in bpy.data.materials:
                slot.material = bpy.data.materials[key]
                continue
            if asset.startswith(SCAN_STONE):
                slot.material = solid(key, [(176, 168, 156), (160, 154, 144), (188, 178, 160)], noise=12, bump=0.4, moss=0.3)
                continue
            if asset.startswith(SCAN_BARK):
                slot.material = solid(key, [(138, 98, 64), (122, 86, 56), (150, 108, 70)], noise=10, bump=0.4, moss=0.2)
                continue
            m = src.copy()
            m.name = key
            N, L = m.node_tree.nodes, m.node_tree.links
            bsdf = next((n for n in N if n.type == "BSDF_PRINCIPLED"), None)
            link = bsdf.inputs["Base Color"].links[0] if bsdf and bsdf.inputs["Base Color"].links else None
            if link is not None:
                leaf = next((v for k, v in SCAN_LEAF.items() if k in asset), None)
                if leaf:  # листва: яркость текстуры -> оттенки одного зелёного
                    bw = N.new("ShaderNodeRGBToBW")
                    L.new(link.from_socket, bw.inputs["Color"])
                    mr = N.new("ShaderNodeMapRange")
                    mr.inputs["To Min"].default_value = 0.55
                    mr.inputs["To Max"].default_value = 1.35
                    mr.inputs["From Max"].default_value = 0.6
                    L.new(bw.outputs["Val"], mr.inputs["Value"])
                    gc = N.new("ShaderNodeCombineColor")
                    for k in ("Red", "Green", "Blue"):
                        L.new(mr.outputs["Result"], gc.inputs[k])
                    col = _mix(N, L, lin(leaf), gc.outputs["Color"], 1.0, "MULTIPLY")
                else:  # утварь: насыщеннее и светлее
                    hs = N.new("ShaderNodeHueSaturation")
                    hs.inputs["Saturation"].default_value = 1.35
                    hs.inputs["Value"].default_value = 1.2
                    L.new(link.from_socket, hs.inputs["Color"])
                    col = hs.outputs["Color"]
                L.new(col, bsdf.inputs["Base Color"])
            slot.material = m
