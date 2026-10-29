"""Процедурные позы и анимации людей (риг default MakeHuman).

Оси в позе покоя: человек смотрит к −Y, его левая сторона — +X, вверх — +Z. Повороты задаются вокруг мировых осей
позы покоя в голове кости (Human.rotate), так что «вперёд» для ноги — поворот вокруг X на отрицательный угол.
Каждая анимация — функция pose(h, t, style) для t ∈ [0, 1); style — словарь особенностей отряда (чем держит оружие).
"""
import math

X, Y, Z = (1, 0, 0), (0, 1, 0), (0, 0, 1)


def smooth(a, b, t):
    t = min(1.0, max(0.0, t))
    t = t * t * (3 - 2 * t)
    return a + (b - a) * t


def keys(t, frames):
    """Интерполяция по ключам [(t0, v0), (t1, v1), ...] с плавными переходами."""
    if t <= frames[0][0]:
        return frames[0][1]
    for (t0, v0), (t1, v1) in zip(frames, frames[1:]):
        if t <= t1:
            k = (t - t0) / max(1e-6, t1 - t0)
            if isinstance(v0, (tuple, list)):
                return [smooth(a, b, k) for a, b in zip(v0, v1)]
            return smooth(v0, v1, k)
    return frames[-1][1]


def side(name, s):
    return name.replace(".L", "." + s).replace(".R", "." + s) if name.endswith((".L", ".R")) else name


def leg(h, s, hip, knee, foot=0.0):
    """Нога s ('L'/'R'): hip — вынос бедра вперёд, knee — сгиб колена, foot — носок вверх (+)."""
    h.rotate(f"upperleg01.{s}", X, -hip)
    h.rotate(f"lowerleg01.{s}", X, knee)
    h.rotate(f"foot.{s}", X, -(foot - hip + knee) * 0.6)


def arm(h, s, down=0.62, fwd=0.0, out=0.0, elbow=0.2, twist=0.0):
    """Рука s: down — опустить из A-позы к телу, fwd — вынести вперёд (подъём), out — отвести в сторону, elbow — сгиб локтя."""
    sign = 1 if s == "L" else -1
    h.rotate(f"upperarm01.{s}", Y, sign * (down - out))
    h.rotate(f"upperarm01.{s}", X, -fwd)
    if twist:
        h.rotate(f"upperarm02.{s}", Z, sign * twist)
    h.rotate(f"lowerarm01.{s}", X, -elbow)


def spine(h, lean=0.0, twist=0.0, bend=0.0):
    """lean — наклон корпуса вперёд, twist — поворот плеч (+ — правое плечо вперёд), bend — наклон вбок."""
    for b, k in (("spine04", 0.35), ("spine03", 0.35), ("spine02", 0.3)):
        h.rotate(b, X, lean * k)
        h.rotate(b, Z, -twist * k)
        h.rotate(b, Y, bend * k)


def head(h, nod=0.0, turn=0.0):
    h.rotate("neck01", X, nod * 0.5)
    h.rotate("head", X, nod * 0.5)
    h.rotate("head", Z, turn)


def bob(h, dz):
    h.bob = dz


# ---------- ходьба и бег ----------
def walk(h, t, st):
    run = st.get("run", 0.0)  # 0 — шаг, 1 — бег
    a = 2 * math.pi * t
    s, c = math.sin(a), math.cos(a)
    stride = 0.42 + 0.25 * run
    for sd, ph in (("L", 0.0), ("R", math.pi)):
        sw = math.sin(a + ph)
        knee = 0.12 + (0.55 + 0.5 * run) * max(0.0, math.cos(a + ph))  # колено сгибается при выносе ноги
        leg(h, sd, stride * sw, knee, foot=0.15 * max(0.0, -math.cos(a + ph)))
    spine(h, lean=0.06 + 0.18 * run, twist=0.08 * s)
    bob(h, -0.025 * abs(c) * (1 + run))
    _arms_moving(h, st, s, run)


def _arms_moving(h, st, s, run):
    grip = st.get("grip", "free")
    if grip == "free" or grip == "one":
        arm(h, "L", fwd=-0.35 * s, elbow=0.25 + 0.4 * run)
    if grip == "free":
        arm(h, "R", fwd=0.35 * s, elbow=0.25 + 0.4 * run)
    elif grip == "one":  # оружие в правой: рука согнута, качается меньше
        arm(h, "R", fwd=0.25 + 0.15 * s, elbow=0.9, out=0.05)
    elif grip == "shield":  # щит перед собой на левой, оружие в правой
        arm(h, "L", fwd=0.55, elbow=1.15, out=-0.05)
        arm(h, "R", fwd=0.2 + 0.15 * s, elbow=0.8, out=0.05)
    elif grip == "pole":  # копьё/пищаль обеими руками у правого плеча
        arm(h, "R", fwd=0.35, elbow=1.2, out=0.12)
        arm(h, "L", fwd=0.75, elbow=0.6, out=-0.15)
    elif grip == "bow":  # лук в левой опущен
        arm(h, "L", fwd=0.3 - 0.1 * s, elbow=0.35)
        arm(h, "R", fwd=0.3 * s, elbow=0.3)
    elif grip == "carry":  # таран: обе руки держат бревно сбоку на уровне груди
        arm(h, "R", fwd=0.95, elbow=0.55, out=-0.15)
        arm(h, "L", fwd=0.95, elbow=0.55, out=-0.15)


def idle(h, t, st):
    a = 2 * math.pi * t
    br = math.sin(a)
    for sd in ("L", "R"):
        leg(h, sd, 0.02, 0.06)
    spine(h, lean=0.03 + 0.012 * br)
    head(h, nod=0.02 * br, turn=0.15 * math.sin(a * 0.5))
    bob(h, -0.004 * (1 + br))
    _arms_moving(h, st, 0.0, 0.0)


# ---------- атаки ----------
def chop(h, t, st):
    """Удар топором/саблей сверху: замах над головой, удар вниз-вперёд, возврат."""
    up = keys(t, [(0.0, 0.3), (0.38, 2.7), (0.58, 0.2), (1.0, 0.3)])
    elbow = keys(t, [(0.0, 0.9), (0.38, 1.4), (0.58, 0.15), (1.0, 0.9)])
    lean = keys(t, [(0.0, 0.05), (0.38, -0.12), (0.58, 0.35), (1.0, 0.05)])
    tw = keys(t, [(0.0, 0.0), (0.38, 0.3), (0.58, -0.25), (1.0, 0.0)])
    spine(h, lean=lean, twist=tw)
    leg(h, "L", 0.35, 0.25)
    leg(h, "R", -0.25, 0.15)
    arm(h, "R", fwd=up, elbow=elbow, out=0.1)
    if st.get("grip") == "shield":
        arm(h, "L", fwd=0.6, elbow=1.15, out=-0.05)
    else:
        arm(h, "L", fwd=0.3, elbow=0.6)
    bob(h, -0.02)


def thrust(h, t, st):
    """Укол копьём из-за щита."""
    ext = keys(t, [(0.0, 0.0), (0.35, -0.25), (0.55, 1.0), (1.0, 0.0)])
    spine(h, lean=0.1 + 0.2 * ext, twist=0.15 * ext)
    leg(h, "L", 0.4, 0.3)
    leg(h, "R", -0.3, 0.2)
    arm(h, "R", fwd=0.55 + 0.45 * ext, elbow=1.25 - 1.0 * ext, out=0.05)
    arm(h, "L", fwd=0.6, elbow=1.15, out=-0.05)
    bob(h, -0.03)


def shoot_bow(h, t, st):
    """Выстрел из лука: вскинуть лук, натянуть тетиву, отпустить."""
    raise_ = keys(t, [(0.0, 0.3), (0.25, 1.45), (1.0, 1.45)])
    draw = keys(t, [(0.0, 0.0), (0.25, 0.2), (0.6, 1.0), (0.66, 0.0), (1.0, 0.0)])
    spine(h, lean=0.02, twist=-0.5)
    head(h, turn=0.45)
    leg(h, "L", 0.25, 0.1)
    leg(h, "R", -0.2, 0.1)
    arm(h, "L", fwd=raise_, elbow=0.1, out=0.25)
    arm(h, "R", fwd=raise_ - 0.1, elbow=0.4 + 1.9 * draw, out=0.15 + 0.3 * draw)
    bob(h, 0.0)


def shoot_gun(h, t, st):
    """Выстрел из пищали: вскинуть к плечу, выстрел (отдача), опустить для перезарядки."""
    aim = keys(t, [(0.0, 0.0), (0.25, 1.0), (0.55, 1.0), (0.85, 0.2), (1.0, 0.0)])
    kick = keys(t, [(0.0, 0.0), (0.44, 0.0), (0.5, 1.0), (0.62, 0.0), (1.0, 0.0)])
    spine(h, lean=0.05 - 0.12 * kick, twist=-0.3 * aim)
    head(h, nod=0.25 * aim, turn=0.3 * aim)
    leg(h, "L", 0.3, 0.15)
    leg(h, "R", -0.25, 0.1)
    arm(h, "R", fwd=0.35 + 0.95 * aim, elbow=1.2 + 0.6 * aim, out=0.12 + 0.35 * aim)
    arm(h, "L", fwd=0.75 + 0.6 * aim, elbow=0.6 - 0.1 * aim, out=-0.15 + 0.1 * aim)


def ram_hit(h, t, st):
    """Удар тараном: оттянуть, рывок вперёд всем телом, отдача."""
    push = keys(t, [(0.0, 0.0), (0.4, -0.6), (0.55, 1.0), (0.7, 0.8), (1.0, 0.0)])
    spine(h, lean=0.25 + 0.2 * push)
    leg(h, "L", 0.3 + 0.2 * push, 0.4)
    leg(h, "R", -0.3 - 0.15 * push, 0.25)
    arm(h, "R", fwd=0.9 + 0.2 * push, elbow=0.6 - 0.3 * push, out=-0.15)
    arm(h, "L", fwd=0.9 + 0.2 * push, elbow=0.6 - 0.3 * push, out=-0.15)
    h.slide = 0.12 * push  # всё тело с тараном — вперёд
    bob(h, -0.03)


def die(h, t, st):
    """Падение назад: колени подгибаются, корпус заваливается."""
    k = keys(t, [(0.0, 0.0), (0.35, 0.35), (0.8, 1.0), (1.0, 1.0)])
    leg(h, "L", 0.2 * k, 0.9 * k)
    leg(h, "R", 0.4 * k, 1.2 * k)
    spine(h, lean=-0.3 * k, twist=0.2 * k)
    head(h, nod=-0.4 * k)
    arm(h, "L", fwd=0.8 * k, out=0.6 * k, elbow=0.3)
    arm(h, "R", fwd=1.0 * k, out=0.5 * k, elbow=0.2)
    h.fall = 1.45 * keys(t, [(0.0, 0.0), (0.25, 0.08), (0.85, 1.0), (1.0, 1.0)])


ATTACKS = {"chop": chop, "thrust": thrust, "bow": shoot_bow, "gun": shoot_gun, "ram": ram_hit}
