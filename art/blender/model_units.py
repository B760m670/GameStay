"""Войска и разбойники: реалистичные люди (human_kit) в одежде и со снаряжением (outfit_kit), позы и анимации —
процедурные. Каждый кадр: сбросить позу, выставить её функцией анимации, отрендерить. Спрайты — как у прежних юнитов:
<out>/units/<id>/<anim>_<dir>_<frame>.png (+ meta.json), направления 0..4 (5..7 движок зеркалит).

Запуск: blender -b -P art/blender/model_units.py -- --out art/out --units torch,archer --samples 32 [--preview]
"""
import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import human_kit as H  # noqa: E402
import model_kit as K  # noqa: E402
import anim_kit as A  # noqa: E402
import outfit_kit as O  # noqa: E402
import render_sprites as R  # noqa: E402
import scan_kit as S  # noqa: E402

TILE_M = 2.5  # метров в клетке


def lin(c):
    return tuple((v / 255) ** 2.2 for v in c)


# ---------- облик ----------
def materials():
    return {
        "wood": S.grain_mat("weathered_planks"),
        "shaft": S.grain_mat("wood_planks_grey", name="grain_shaft"),
        "steel": O.plain("steel", lin((150, 150, 150)), rough=0.35, metal=0.9),
        "iron": O.plain("dark_iron", lin((70, 68, 66)), rough=0.5, metal=0.8),
        "string": O.plain("string", lin((210, 200, 170)), rough=0.8),
    }


def dress_peasant(h, coat_col, hat_fur, hat_top=None, belt=(150, 40, 34), long_coat=True):
    """Порты, сапоги, кафтан или тулуп с полами, кушак, шапка."""
    O.garment(h, O.UPPER_LEGS + O.LOWER_LEGS, 0.006, O.fabric("linen", lin((92, 84, 70))), name="pants", cover=0.9)
    O.garment(h, O.LOWER_LEGS, 0.016, O.fabric("leather", lin((62, 44, 32)), scale=6, sat=0.5), prefixes=O.FEET_PREFIX, thr=0.45,
              name="boots", cover=0.9, grow=lambda v, co: 0.006 if co.z < 0.08 else 0.0)
    coat = O.fabric(coat_col[0], lin(coat_col[1]), scale=7)
    O.garment(h, O.SPINE + O.UPPER_ARMS + O.LOWER_ARMS, 0.02, coat, name="coat", cover=0.9)
    if long_coat:
        O.skirt(h, h.world_head("upperleg02.L").z + 0.08, h.world_head("lowerleg01.L").z - 0.02, coat, flare=0.05, name="coat_skirt")
    waist = h.world_head("upperleg02.L").z + 0.1
    c, rx, ry = O.body_section(h, waist, O.SPINE)
    sash = O.revolve("sash", [(1.0, -0.035), (1.0, 0.035)], O.fabric("wool", lin(belt), scale=5), Vector((c.x, c.y, waist)),
                     scale=(rx + 0.03, ry + 0.03, 1))
    O.transfer_weights(h, sash, O.SPINE)
    O._attach(h, sash)
    O.fur_hat(h, O.fabric("fur", lin(hat_fur), scale=5), top=O.fabric("wool", lin(hat_top), scale=5) if hat_top else None)


def guard_shield(h, shield, lift=0.0):
    """Щит на левой руке: руку ставим в позу «щит перед собой», щит — вертикально перед грудью слева, лицом вперёд."""
    h.reset_pose()
    A.arm(h, "L", fwd=0.55, elbow=1.15, out=-0.05)
    bpy.context.view_layer.update()
    a = h.world_head("lowerarm01.L")
    b = h.world_head("wrist.L")
    c = (a + b) / 2
    fwd = Vector((0.25, -1, 0)).normalized()
    up = Vector((0, 0, 1))
    m = Matrix((fwd.cross(up), fwd, up)).transposed().to_4x4()  # лицевая сторона щита (+Y) — вперёд
    m.translation = c + fwd * 0.13 + Vector((0, 0, 0.04 + lift))
    shield.matrix_world = m @ shield.matrix_world
    O.bone_attach(h, shield, "lowerarm02.L")
    h.reset_pose()
    bpy.context.view_layer.update()


def aim_bow(h, bow, st):
    """Лук в левой руке: ставим руку в позу выстрела, лук — вертикально, выгибом к цели; потом привязываем к кисти."""
    h.reset_pose()
    A.shoot_bow(h, 0.5, st)
    bpy.context.view_layer.update()
    wr = h.world_head("wrist.L")
    mid = h.world_head("finger3-1.L")
    along = (mid - wr).normalized()
    x = Vector((along.x, along.y, 0)).normalized()
    z = Vector((0, 0, 1))
    m = Matrix((x, z.cross(x), z)).transposed().to_4x4()
    m.translation = wr + (mid - wr) * 0.6
    bow.matrix_world = m @ bow.matrix_world
    O.bone_attach(h, bow, "wrist.L")
    h.reset_pose()
    bpy.context.view_layer.update()


def aim_gun(h, gun, st):
    """Пищаль в правой руке: ставим позу прицеливания, ствол — от правой кисти к левой, ложе вниз."""
    h.reset_pose()
    A.shoot_gun(h, 0.4, st)
    bpy.context.view_layer.update()
    wr = h.world_head("wrist.R")
    wl = h.world_head("wrist.L")
    z = (wl - wr).normalized()
    up = Vector((0, 0, 1))
    x = (up - z * up.dot(z)).normalized()
    m = Matrix((x, z.cross(x), z)).transposed().to_4x4()
    m.translation = wr
    gun.matrix_world = m @ gun.matrix_world
    O.bone_attach(h, gun, "wrist.R")
    h.reset_pose()
    bpy.context.view_layer.update()


def militia():
    """Ополченец: тулуп из овчины, меховая шапка, кушак, порты и сапоги; топор и круглый щит."""
    h = H.Human("militia", gender=1.0, age=0.62, muscle=0.62, weight=0.55, height=0.45).build("middleage_caucasian_male")
    h.fit("hair", "short02")
    h.fit("eyebrows", "eyebrow001")
    M = materials()
    dress_peasant(h, ("fleece", (206, 176, 132)), hat_fur=(96, 70, 50), belt=(158, 44, 36))
    O.hold(h, O.axe(M["shaft"], M["steel"]), "R")
    guard_shield(h, O.round_shield(0.28, M["wood"], M["iron"]))
    return h, {"grip": "shield", "attack": "chop", "cam": 0.85}


def archer():
    """Лучник: короткий зелёный кафтан, войлочный колпак; лук и колчан за спиной."""
    h = H.Human("archer", gender=1.0, age=0.45, muscle=0.5, weight=0.4, height=0.55).build("young_caucasian_male")
    h.fit("hair", "short03")
    h.fit("eyebrows", "eyebrow003")
    M = materials()
    dress_peasant(h, ("wool", (92, 112, 74)), hat_fur=(70, 60, 50), hat_top=(120, 60, 40), belt=(70, 50, 36), long_coat=False)
    st = {"grip": "bow", "attack": "bow", "cam": 0.85}
    aim_bow(h, O.bow(M["shaft"], M["string"]), st)
    O.on_back(h, O.quiver(O.fabric("leather", lin((90, 60, 40)), scale=4, sat=0.5), M["shaft"]), tilt=-0.35, offset=(0.05, 0.14, -0.12))
    return h, st


def shieldman():
    """Щитник: кольчуга поверх стёганого кафтана, шишак с бармицей; большой каплевидный щит и копьё."""
    h = H.Human("shieldman", gender=1.0, age=0.55, muscle=0.85, weight=0.6, height=0.65).build("middleage_caucasian_male")
    h.fit("eyebrows", "eyebrow002")
    M = materials()
    O.garment(h, O.UPPER_LEGS + O.LOWER_LEGS, 0.007, O.fabric("wool", lin((70, 54, 44))), name="pants", cover=0.9)
    O.garment(h, O.LOWER_LEGS, 0.016, O.fabric("leather", lin((52, 38, 28)), scale=6, sat=0.5), prefixes=O.FEET_PREFIX, thr=0.45,
              name="boots", cover=0.9, grow=lambda v, co: 0.006 if co.z < 0.08 else 0.0)
    gambeson = O.fabric("wool", lin((120, 44, 36)), scale=8)
    O.garment(h, O.SPINE + O.UPPER_ARMS + O.LOWER_ARMS, 0.02, gambeson, name="gambeson", cover=0.9)
    O.skirt(h, h.world_head("upperleg02.L").z + 0.08, h.world_head("lowerleg01.L").z + 0.05, gambeson, flare=0.04, name="gambeson_skirt")
    mail = O.fabric("fleece", lin((150, 150, 150)), scale=26, metal=0.75, rough=0.45, sat=0.0)
    O.garment(h, O.SPINE + O.UPPER_ARMS, 0.032, mail, name="mail", thr=0.6)
    O.skirt(h, h.world_head("upperleg02.L").z + 0.1, h.world_head("upperleg02.L").z - 0.22, mail, flare=0.03, margin=0.04, name="mail_skirt")
    O.helmet(h, O.plain("helm_steel", lin((160, 160, 160)), rough=0.3, metal=0.9), mail=mail)
    O.hold(h, O.spear(M["shaft"], M["steel"]), "R", slide=0.1)
    guard_shield(h, O.kite_shield(O.fabric("leather2", lin((150, 36, 28)), scale=3, sat=0.4, rough=0.55)), lift=-0.08)
    return h, {"grip": "shield", "attack": "thrust", "cam": 1.0}


def musketeer():
    """Стрелец: длинный красный кафтан, шапка с меховым околышем; пищаль, бердыш за спиной."""
    h = H.Human("musketeer", gender=1.0, age=0.58, muscle=0.6, weight=0.5, height=0.55).build("middleage_caucasian_male")
    h.fit("hair", "short02")
    h.fit("eyebrows", "eyebrow001")
    M = materials()
    dress_peasant(h, ("wool", (168, 40, 34)), hat_fur=(60, 46, 36), hat_top=(168, 40, 34), belt=(214, 180, 90))
    st = {"grip": "pole", "attack": "gun", "cam": 0.95}
    aim_gun(h, O.musket(M["wood"], M["iron"]), st)
    O.on_back(h, O.berdysh(M["shaft"], M["steel"]), tilt=0.45, offset=(0.0, 0.15, -0.1))
    return h, st


def voevoda():
    """Воевода: кольчуга, красное корзно, шишак; сабля."""
    h = H.Human("voevoda", gender=1.0, age=0.7, muscle=0.75, weight=0.6, height=0.7).build("middleage_caucasian_male")
    h.fit("eyebrows", "eyebrow002")
    M = materials()
    O.garment(h, O.UPPER_LEGS + O.LOWER_LEGS, 0.007, O.fabric("wool", lin((40, 52, 80))), name="pants", cover=0.9)
    O.garment(h, O.LOWER_LEGS, 0.017, O.fabric("leather", lin((140, 40, 30)), scale=6, sat=0.6), prefixes=O.FEET_PREFIX, thr=0.45,
              name="boots", cover=0.9, grow=lambda v, co: 0.006 if co.z < 0.08 else 0.0)
    kaftan = O.fabric("wool", lin((40, 52, 90)), scale=8)
    O.garment(h, O.SPINE + O.UPPER_ARMS + O.LOWER_ARMS, 0.02, kaftan, name="kaftan", cover=0.9)
    O.skirt(h, h.world_head("upperleg02.L").z + 0.08, h.world_head("lowerleg01.L").z - 0.05, kaftan, flare=0.05, name="kaftan_skirt")
    mail = O.fabric("fleece", lin((175, 170, 160)), scale=26, metal=0.8, rough=0.4, sat=0.0)
    O.garment(h, O.SPINE + O.UPPER_ARMS, 0.032, mail, name="mail", thr=0.6)
    O.cape(h, O.fabric("wool", lin((176, 34, 30)), scale=6))
    O.helmet(h, O.plain("helm_gold", lin((200, 170, 100)), rough=0.3, metal=0.9), mail=mail)
    O.hold(h, O.sabre(M["steel"], M["wood"]), "R")
    return h, {"grip": "one", "attack": "chop", "cam": 1.0}


def worker():
    """Плотник: рубаха, порты, фартук; топор."""
    h = H.Human("worker", gender=1.0, age=0.5, muscle=0.6, weight=0.5, height=0.5).build("young_caucasian_male")
    h.fit("hair", "short04")
    h.fit("eyebrows", "eyebrow004")
    M = materials()
    O.garment(h, O.UPPER_LEGS + O.LOWER_LEGS, 0.006, O.fabric("linen", lin((120, 110, 92))), name="pants", cover=0.9)
    O.garment(h, O.LOWER_LEGS, 0.014, O.fabric("burlap", lin((150, 130, 100)), scale=8), prefixes=O.FEET_PREFIX, thr=0.45, name="onuchi",
              cover=0.9)
    O.garment(h, O.SPINE + O.UPPER_ARMS + O.LOWER_ARMS, 0.012, O.fabric("linen", lin((224, 214, 192))), name="shirt", cover=0.9)
    O.skirt(h, h.world_head("upperleg02.L").z + 0.12, h.world_head("upperleg02.L").z - 0.18, O.fabric("linen", lin((224, 214, 192))),
            flare=0.02, name="shirt_tail")
    O.hold(h, O.axe(M["shaft"], M["steel"], length=0.6), "R")
    return h, {"grip": "one", "attack": "chop", "cam": 0.85}


def bandit():
    """Разбойник: рваная рубаха, капюшон, кистень."""
    h = H.Human("bandit", gender=1.0, age=0.5, muscle=0.55, weight=0.35, height=0.5).build("young_caucasian_male2")
    h.fit("eyebrows", "eyebrow005")
    M = materials()
    O.garment(h, O.UPPER_LEGS + O.LOWER_LEGS, 0.006, O.fabric("burlap", lin((96, 86, 70)), scale=8), name="pants", cover=0.9)
    O.garment(h, O.LOWER_LEGS, 0.014, O.fabric("burlap", lin((120, 104, 80)), scale=8), prefixes=O.FEET_PREFIX, thr=0.45, name="onuchi",
              cover=0.9)
    shirt = O.fabric("burlap", lin((140, 126, 100)), scale=8)
    O.garment(h, O.SPINE + O.UPPER_ARMS + O.LOWER_ARMS, 0.012, shirt, name="shirt", cover=0.9)
    O.skirt(h, h.world_head("upperleg02.L").z + 0.12, h.world_head("upperleg02.L").z - 0.15, shirt, flare=0.02, name="shirt_tail")
    O.hood(h, O.fabric("wool", lin((62, 58, 50)), scale=6))
    O.hold(h, O.club(M["shaft"], M["iron"]), "R")
    return h, {"grip": "one", "attack": "chop", "cam": 0.85}


def ataman():
    """Атаман: крупный, в тёмной шубе и трофейной кольчуге, с секирой."""
    h = H.Human("ataman", gender=1.0, age=0.7, muscle=0.9, weight=0.8, height=0.85).build("middleage_caucasian_male")
    h.fit("hair", "long01")
    h.fit("eyebrows", "eyebrow006")
    M = materials()
    dress_peasant(h, ("fleece", (70, 52, 40)), hat_fur=(40, 32, 26), belt=(30, 26, 24))
    mail = O.fabric("fleece", lin((110, 108, 104)), scale=26, metal=0.7, rough=0.5, sat=0.0)
    O.garment(h, O.SPINE, 0.04, mail, name="mail", thr=0.7)
    O.hold(h, O.axe(M["shaft"], M["steel"], length=1.0), "R")
    return h, {"grip": "one", "attack": "chop", "cam": 1.0}


def arsonist():
    """Поджигатель: в капюшоне, с горящим факелом."""
    h = H.Human("arsonist", gender=1.0, age=0.45, muscle=0.5, weight=0.4, height=0.45).build("young_caucasian_male")
    h.fit("eyebrows", "eyebrow007")
    M = materials()
    dress_peasant(h, ("burlap", (110, 92, 70)), hat_fur=(50, 40, 30), belt=(60, 40, 30), long_coat=False)
    O.hood(h, O.fabric("wool", lin((52, 46, 40)), scale=6))
    fire = O.emissive("fire", (1.0, 0.45, 0.08), 9.0)
    O.hold(h, O.torch(M["shaft"], fire, O.plain("rag", lin((40, 32, 26)))), "R")
    return h, {"grip": "one", "attack": "chop", "cam": 0.9}


def outlaw():
    """Лихой стрелок: тёмная одежда, капюшон, лук."""
    h = H.Human("outlaw", gender=1.0, age=0.5, muscle=0.5, weight=0.4, height=0.55).build("young_caucasian_male")
    h.fit("eyebrows", "eyebrow008")
    M = materials()
    dress_peasant(h, ("wool", (60, 64, 50)), hat_fur=(50, 40, 30), belt=(40, 34, 30), long_coat=False)
    O.hood(h, O.fabric("wool", lin((48, 52, 42)), scale=6))
    st = {"grip": "bow", "attack": "bow", "cam": 0.85}
    aim_bow(h, O.bow(M["shaft"], M["string"]), st)
    O.on_back(h, O.quiver(O.fabric("leather", lin((60, 44, 32)), scale=4, sat=0.5), M["shaft"]), tilt=-0.35, offset=(0.05, 0.14, -0.12))
    return h, st


PEOPLE = {"torch": militia, "archer": archer, "shield": shieldman, "owl": musketeer, "hero": voevoda, "worker": worker,
          "morok": bandit, "brute": ataman, "leech": arsonist, "wraith": outlaw}
MOVE8 = [i / 8 for i in range(8)]
ATK6 = [0.0, 0.18, 0.34, 0.5, 0.66, 0.84]
IDLE4 = [0.0, 0.25, 0.5, 0.75]
DIE5 = [0.0, 0.3, 0.55, 0.8, 1.0]


class Unit:
    """Отряд на одном спрайте: фигуры (люди, конь) со смещениями и общий реквизит (бревно тарана).
    Все фигуры и реквизит поворачиваются вместе на направление; каждая фигура позируется своей анимацией."""

    def __init__(self, st):
        self.st = st
        self.members = []  # (human, offset Vector в метрах, сдвиг фазы, свой стиль)
        self.props = []    # (объект, базовая матрица в осях отряда)
        self.ride = None   # (всадник, конь): всадник сидит в седле и качается вместе с конём

    def add(self, h, off=(0, 0, 0), phase=0.0, st=None):
        self.members.append((h, Vector(off), phase, dict(self.st, **(st or {}))))
        return h


def single(builder):
    def make():
        h, st = builder()
        u = Unit(st)
        u.add(h)
        return u
    return make


def pose(u, anim, t, direction):
    """Поза кадра: кости каждой фигуры, её покачивание/падение/рывок, поворот отряда на направление."""
    rz = Matrix.Rotation(math.radians(45 - direction * 45), 4, "Z")
    s = 1.0 / TILE_M
    fwd = Vector((0, -1, 0))
    slides, bobs = [], []
    for h, off, phase, st in u.members:
        h.reset_pose()
        h.bob = h.fall = h.slide = h.roll = 0.0
        tt = (t + phase) % 1.0 if anim in ("move", "idle") else t
        fn = st.get("anims", {}).get(anim) or {"move": A.walk, "idle": A.idle, "die": A.die}.get(anim) or A.ATTACKS[st["attack"]]
        fn(h, tt, st)
        slides.append(h.slide)
        bobs.append(h.bob)
    if u.ride:  # всадник качается с конём
        rider, horse = u.ride
        rider.bob += horse.bob
        rider.slide += horse.slide
    for h, off, phase, st in u.members:
        hs = st.get("scale", 1.0)
        local = (Matrix.Translation((off + fwd * h.slide + Vector((0, 0, h.bob))) * s) @ Matrix.Rotation(-h.fall, 4, "X")
                 @ Matrix.Rotation(h.roll, 4, "Y") @ Matrix.Diagonal((s * hs, s * hs, s * hs, 1)))
        h.arm.matrix_world = rz @ local
    k_slide = sum(slides) / max(1, len(slides))
    k_bob = sum(bobs) / max(1, len(bobs))
    for o, base in u.props:
        drop = u.st.get("prop_drop", 0.0) * (A.keys(t, [(0.0, 0.0), (0.5, 0.2), (0.9, 1.0), (1.0, 1.0)]) if anim == "die" else 0.0)
        o.matrix_world = rz @ Matrix.Translation((fwd * k_slide + Vector((0, 0, k_bob - drop))) * s) @ Matrix.Diagonal((s, s, s, 1)) @ base
    bpy.context.view_layer.update()


def render_frames(u, sc, out_dir, dirs, anims, tag=""):
    paths = []
    for d in dirs:
        for anim, frames in anims.items():
            for fi, t in enumerate(frames):
                pose(u, anim, t, d)
                p = os.path.join(out_dir, f"{tag}{anim}_{d}_{fi}.png")
                sc.render.filepath = p
                bpy.ops.render.render(write_still=True)
                R.clean_alpha(p)
                paths.append(p)
    return paths


# ---------- таран и всадник ----------
def carrier(i):
    """Носильщик тарана: в тулупе или рубахе, без оружия."""
    h = H.Human(f"ram{i}", gender=1.0, age=0.5 + 0.06 * i, muscle=0.7, weight=0.5 + 0.05 * (i % 2), height=0.45 + 0.08 * (i % 3)).build(
        ["middleage_caucasian_male", "young_caucasian_male", "young_caucasian_male2", "old_caucasian_male"][i % 4])
    h.fit("hair", ["short02", "short01", "short03", "short04"][i % 4])
    coats = [("fleece", (190, 160, 120)), ("wool", (110, 90, 70)), ("linen", (200, 188, 160)), ("fleece", (150, 120, 90))]
    dress_peasant(h, coats[i % 4], hat_fur=(80 + 10 * i, 64, 48), belt=[(150, 40, 34), (60, 70, 110), (120, 100, 50), (90, 40, 30)][i % 4],
                  long_coat=i % 2 == 0)
    return h


def ram():
    """Таран: четверо несут окованное бревно, двое с каждой стороны."""
    u = Unit({"grip": "carry", "attack": "ram", "cam": 0.95, "size": 2, "prop_drop": 1.05})
    spots = [(-0.42, -0.75), (0.42, -0.65), (-0.42, 0.55), (0.42, 0.65)]
    for i, (x, y) in enumerate(spots):
        u.add(carrier(i), off=(x, y, 0), phase=(0.0, 0.12, 0.06, 0.18)[i])
    M = materials()
    log = S.log((0, 1.5, 1.18), (0, -1.6, 1.18), 0.15, S.grain_mat("weathered_planks"), 5, r_end=0.13, sag=0.0)
    cap = O.revolve("ram_cap", [(0.15, 0.0), (0.155, 0.22), (0.12, 0.3), (0.0, 0.33)], M["iron"], Vector((0, 0, 0)), segs=16)
    cap.matrix_world = Matrix.Translation((0, -1.48, 1.18)) @ Matrix.Rotation(math.pi / 2, 4, "X")
    for b in (1.2, 0.0):  # железные обручи
        ring = O.revolve("ram_band", [(0.155, -0.03), (0.16, 0.0), (0.155, 0.03)], M["iron"], Vector((0, 0, 0)), segs=16)
        ring.matrix_world = Matrix.Translation((0, -b, 1.18)) @ Matrix.Rotation(math.pi / 2, 4, "X")
        u.props.append((ring, ring.matrix_world.copy()))
    u.props += [(log, log.matrix_world.copy()), (cap, cap.matrix_world.copy())]
    return u


def horse_walk(h, t, st):
    """Рысь: диагональные пары ног (левая передняя с правой задней). Передние ноги коня — руки рига."""
    run = st.get("run", 1.0)
    a = 2 * math.pi * t
    amp = 0.35 + 0.15 * run
    for sd, ph in (("L", 0.0), ("R", math.pi)):
        front = math.sin(a + ph)
        hind = math.sin(a + ph + math.pi)
        h.rotate(f"upperarm01.{sd}", A.X, -amp * front)
        h.rotate(f"lowerarm01.{sd}", A.X, (0.2 + 1.0 * max(0.0, math.cos(a + ph))))
        h.rotate(f"upperleg01.{sd}", A.X, -amp * hind)
        h.rotate(f"lowerleg01.{sd}", A.X, -(0.15 + 0.6 * max(0.0, math.cos(a + ph + math.pi))))
    h.rotate("neck01", A.X, 0.08 * math.sin(2 * a))
    h.bob = 0.05 * abs(math.cos(a)) * (1 + run)


def horse_idle(h, t, st):
    a = 2 * math.pi * t
    h.rotate("neck01", A.X, 0.06 + 0.05 * math.sin(a))
    h.rotate("upperarm01.L", A.X, -0.05)
    h.bob = 0.0


def horse_rear(h, t, st):
    """Конь во время удара всадника: переступает и вскидывает голову."""
    k = A.keys(t, [(0.0, 0.0), (0.4, 1.0), (0.7, 0.6), (1.0, 0.0)])
    h.rotate("upperarm01.L", A.X, -0.4 * k)
    h.rotate("lowerarm01.L", A.X, 0.9 * k)
    h.rotate("neck01", A.X, -0.25 * k)
    h.bob = 0.02 * k


def horse_die(h, t, st):
    k = A.keys(t, [(0.0, 0.0), (0.4, 0.4), (0.9, 1.0), (1.0, 1.0)])
    for sd in ("L", "R"):
        h.rotate(f"lowerarm01.{sd}", A.X, 1.2 * k)
        h.rotate(f"lowerleg01.{sd}", A.X, -1.0 * k)
    h.rotate("neck01", A.X, 0.4 * k)
    h.roll = 1.45 * A.keys(t, [(0.0, 0.0), (0.3, 0.05), (0.9, 1.0), (1.0, 1.0)])  # конь валится на бок
    h.bob = -0.05 * k


def seated(h, t, st, upper=None):
    """Всадник в седле: бёдра вперёд и в стороны, колени согнуты; верх тела — от upper (удар) или спокойно."""
    for sd in ("L", "R"):
        sign = 1 if sd == "L" else -1
        h.rotate(f"upperleg01.{sd}", A.Y, -sign * 0.32)
        h.rotate(f"upperleg01.{sd}", A.X, -1.25)
        h.rotate(f"lowerleg01.{sd}", A.X, 1.35)
        h.rotate(f"foot.{sd}", A.X, -0.2)
    if upper:
        upper(h, t, st)
        for sd in ("L", "R"):  # ноги после удара — снова в седле
            pass
    else:
        A.spine(h, lean=0.12)
        A.arm(h, "L", fwd=0.7, elbow=1.2)
        A.arm(h, "R", fwd=0.35, elbow=0.9, out=0.05)


def rider_attack(h, t, st):
    up = A.keys(t, [(0.0, 0.3), (0.38, 2.6), (0.58, 0.1), (1.0, 0.3)])
    elbow = A.keys(t, [(0.0, 0.9), (0.38, 1.3), (0.58, 0.1), (1.0, 0.9)])
    A.spine(h, lean=A.keys(t, [(0.0, 0.1), (0.38, -0.1), (0.58, 0.35), (1.0, 0.1)]), twist=A.keys(t, [(0.0, 0.0), (0.38, 0.35), (0.58, -0.3), (1.0, 0.0)]))
    A.arm(h, "R", fwd=up, elbow=elbow, out=0.15)
    A.arm(h, "L", fwd=0.7, elbow=1.2)


def rider_die(h, t, st):
    A.die(h, t, st)
    h.fall *= 0.7
    h.slide = -0.5 * A.keys(t, [(0.0, 0.0), (0.4, 0.3), (1.0, 1.0)])
    h.bob = -1.0 * A.keys(t, [(0.0, 0.0), (0.3, 0.1), (0.9, 1.0), (1.0, 1.0)])


def rider():
    """Всадник: конь рыжей масти с седлом, на нём — человек в кафтане с саблей."""
    M = materials()
    horse = H.Human("horse", targets=[("animal/jaldmic_horse.target", 1.0)], gender=1.0, age=0.5, muscle=0.7, weight=0.5).build()
    coat = O.fabric("leather2", lin((120, 70, 38)), scale=4, sat=0.35, rough=0.45, name="horse_coat")
    horse.body.data.materials.clear()
    horse.body.data.materials.append(coat)
    hs = 1.75  # конь из базового меша — пони; до роста настоящей лошади
    back = max((horse.body.matrix_world @ v.co).z for v in horse.body.data.vertices if abs(v.co.y - 0.1) < 0.08 and abs(v.co.x) < 0.05)
    # грива и хвост
    neck = horse.world_head("neck02")
    mane = O.revolve("mane", [(0.03, 0.0), (0.035, 0.2), (0.0, 0.32)], O.fabric("fur", lin((40, 28, 20)), scale=6), Vector((0, 0, 0)), segs=6,
                     scale=(0.6, 1.0, 1.0))
    mane.matrix_world = Matrix.Translation(neck + Vector((0, 0.06, 0.05))) @ Matrix.Rotation(-0.9, 4, "X")
    O.bone_attach(horse, mane, "neck02")
    tail_root = horse.world_head("spine05") + Vector((0, 0.32, 0.05))
    tail = O.revolve("tail", [(0.045, 0.0), (0.06, -0.25), (0.03, -0.55), (0.0, -0.62)], O.fabric("fur", lin((40, 28, 20)), scale=6),
                     Vector((0, 0, 0)), segs=8)
    tail.matrix_world = Matrix.Translation(tail_root) @ Matrix.Rotation(-0.35, 4, "X")
    O.bone_attach(horse, tail, "spine05")
    saddle = O.revolve("saddle", [(0.16, -0.02), (0.18, 0.03), (0.12, 0.07), (0.0, 0.075)], O.fabric("leather", lin((90, 46, 30)), scale=4, sat=0.6),
                       Vector((0, 0.08, back - 0.03)), segs=16, scale=(0.95, 1.4, 1.0))
    blanket = O.revolve("blanket", [(0.24, -0.12), (0.25, -0.02), (0.2, 0.01)], O.fabric("wool", lin((150, 36, 30)), scale=5),
                        Vector((0, 0.08, back - 0.03)), segs=16, scale=(1.0, 1.5, 1.0))
    for o in (saddle, blanket):
        O.bone_attach(horse, o, "spine03")
    man = H.Human("rider", gender=1.0, age=0.5, muscle=0.6, weight=0.45, height=0.5).build("young_caucasian_male")
    man.fit("hair", "short01")
    dress_peasant(man, ("wool", (70, 80, 110)), hat_fur=(60, 44, 34), hat_top=(150, 40, 34), belt=(200, 170, 80))
    O.hold(man, O.sabre(M["steel"], M["wood"]), "R")
    seat = (back - 0.03 + 0.075) * hs - man.world_head("upperleg01.L").z + 0.02
    u = Unit({"grip": "one", "attack": "chop", "cam": 1.25, "size": 2})
    u.add(horse, st={"scale": hs, "anims": {"move": horse_walk, "idle": horse_idle, "attack": horse_rear, "die": horse_die}})
    u.add(man, off=(0, 0.08 * hs, seat), st={"anims": {"move": lambda h, t, st: seated(h, t, st), "idle": lambda h, t, st: seated(h, t, st),
                                                        "attack": lambda h, t, st: seated(h, t, st, rider_attack),
                                                        "die": lambda h, t, st: (seated(h, t, st), rider_die(h, t, st))}})
    u.ride = (man, horse)
    return u


UNITS = {k: single(f) for k, f in PEOPLE.items()}
UNITS.update({"sapper": ram, "shade": rider})


def main():
    opts = R.args()
    names = str(opts.get("units", "torch")).split(",")
    for name in names:
        sc = R.reset_scene(int(opts["samples"]))
        S.reset()
        K.palette()
        u = UNITS[name]()
        K.light_rig(sc)
        sc.view_settings.exposure = 0.55
        pose(u, "idle", 0.0, 0)
        cam, pivot = R.setup_camera(u.st.get("size", 1), u.st.get("cam", 0.85))
        if opts.get("preview"):  # лист проверки: крупно, два направления, все анимации
            sc.render.resolution_percentage = 200
            out_dir = os.path.join(opts["out"], "preview_" + name)
            os.makedirs(out_dir, exist_ok=True)
            render_frames(u, sc, out_dir, [0, 1], {"move": MOVE8, "attack": ATK6, "idle": IDLE4[:2], "die": DIE5})
            print("[units]", name, "preview ->", out_dir)
            continue
        out_dir = os.path.join(opts["out"], "units", name)
        os.makedirs(out_dir, exist_ok=True)
        render_frames(u, sc, out_dir, range(5), {"move": MOVE8, "attack": ATK6, "idle": IDLE4, "die": DIE5})
        with open(os.path.join(out_dir, "meta.json"), "w") as f:
            json.dump({"pivot": [round(pivot[0], 1), round(pivot[1], 1)], "anims": {"move": 8, "attack": 6, "idle": 4, "die": 5},
                       "dirs": 8, "source": "blender:model_units"}, f)
        print("[units]", name, "->", out_dir)


if __name__ == "__main__":
    main()
