"""Чертёж ополченца: вид спереди, сбоку и сзади, сетка пропорций, детали, палитра, проверка силуэта.
Персонаж придуман с нуля (не реальная анатомия): голова с шапкой — почти половина роста, короткие ноги в валенках,
тулуп колоколом, крупные кисти, топор с большим клином.

Запуск: python3 art/concept/militia_sheet.py  ->  art/concept/militia_sheet.svg (PNG рисует render_sheet.js).
"""
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "militia_sheet.svg")
C = dict(ol="#3A2416", skin="#F2B48C", skin_s="#D98E68", beard="#9A5A2A", beard_s="#74421F", coat="#D29A5E", coat_s="#AE7640",
         fleece="#F6ECD8", fleece_s="#DCCAA6", sash="#C8352B", sash_s="#97231D", boot="#77716C", boot_s="#57524E", fur="#6B4A33",
         fur_s="#4E3524", cloth="#C8352B", cloth_s="#97231D", gold="#E3A93A", steel="#D3DBE2", steel_s="#8E9AA5", haft="#A8693A",
         haft_s="#7E4C28", wood="#C08A52", wood_s="#986638", iron="#5E646C", eye="#2A1A12", cheek="#E8746A")
N = [0]
defs = []


def part(d, base, shade, off=(16, 12), sw=5, extra=""):
    """Заливка с «целлюлоидной» тенью: та же форма, сдвинутая к свету, оставляет тень снизу-справа."""
    N[0] += 1
    cid = f"c{N[0]}"
    defs.append(f'<clipPath id="{cid}"><path d="{d}"/></clipPath>')
    return (f'<path d="{d}" fill="{shade}"/>'
            f'<path d="{d}" fill="{base}" transform="translate({-off[0]},{-off[1]})" clip-path="url(#{cid})"/>'
            f'<path d="{d}" fill="none" stroke="{C["ol"]}" stroke-width="{sw}" stroke-linejoin="round" {extra}/>')


def circ(cx, cy, r):
    return f"M {cx - r} {cy} a {r} {r} 0 1 0 {2 * r} 0 a {r} {r} 0 1 0 {-2 * r} 0 Z"


def fleece_line(pts, r=15):
    """Овчина: цепочка завитков по линии."""
    out = ""
    for x, y in pts:
        out += f'<circle cx="{x}" cy="{y}" r="{r}" fill="{C["fleece"]}" stroke="{C["ol"]}" stroke-width="3.5"/>'
    for x, y in pts:
        out += f'<path d="M {x - r * 0.4} {y + r * 0.1} q {r * 0.4} {-r * 0.6} {r * 0.8} 0" fill="none" stroke="{C["fleece_s"]}" stroke-width="3"/>'
    return out


def lerp_pts(a, b, n):
    return [(a[0] + (b[0] - a[0]) * i / (n - 1), a[1] + (b[1] - a[1]) * i / (n - 1)) for i in range(n)]


def arc_pts(cx, cy, rx, ry, a0, a1, n):
    import math
    return [(cx + math.cos(math.radians(a0 + (a1 - a0) * i / (n - 1))) * rx, cy + math.sin(math.radians(a0 + (a1 - a0) * i / (n - 1))) * ry)
            for i in range(n)]


# ---------- детали, общие для видов ----------
def axe(hx, hy, tx, ty, flip=1):
    """Топор: рукоять от кисти (hx, hy) до (tx, ty), большой клин-лезвие у вершины."""
    s = part(f"M {hx - 9} {hy} L {tx - 9} {ty} L {tx + 9} {ty} L {hx + 9} {hy} Z", C["haft"], C["haft_s"], off=(6, 0))
    bx = tx - 6 * flip
    s += part(f"M {bx} {ty + 8} L {bx - 78 * flip} {ty - 30} Q {bx - 98 * flip} {ty + 32} {bx - 80 * flip} {ty + 78} L {bx} {ty + 44} Z",
              C["steel"], C["steel_s"], off=(10 * flip, 8))
    s += f'<path d="M {bx - 80 * flip} {ty - 26} Q {bx - 96 * flip} {ty + 32} {bx - 78 * flip} {ty + 74}" fill="none" stroke="#FFFFFF" stroke-width="5" opacity="0.8"/>'
    for k in range(3):  # обмотка рукояти
        y = hy - 30 - k * 14
        x = hx + (tx - hx) * (hy - y) / (hy - ty)
        s += f'<path d="M {x - 11} {y} L {x + 11} {y - 6}" stroke="{C["sash_s"]}" stroke-width="6"/>'
    return s


def shield_face(cx, cy, r=90):
    s = part(circ(cx, cy, r), C["wood"], C["wood_s"], off=(14, 12))
    for k in (-2, -1, 1, 2):
        x = cx + k * r * 0.36
        import math
        h = math.sqrt(max(0, r * r - (x - cx) ** 2)) - 6
        s += f'<line x1="{x}" y1="{cy - h}" x2="{x}" y2="{cy + h}" stroke="{C["wood_s"]}" stroke-width="3"/>'
    s += f'<circle cx="{cx}" cy="{cy}" r="{r * 0.62}" fill="none" stroke="{C["sash"]}" stroke-width="13"/>'
    s += f'<circle cx="{cx}" cy="{cy}" r="{r - 6}" fill="none" stroke="{C["iron"]}" stroke-width="12"/>'
    s += f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{C["ol"]}" stroke-width="5"/>'
    s += part(circ(cx, cy, r * 0.22), C["steel"], C["steel_s"], off=(5, 4))
    for a in range(0, 360, 45):
        import math
        x, y = cx + math.cos(math.radians(a)) * (r - 6), cy + math.sin(math.radians(a)) * (r - 6)
        s += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.5" fill="{C["steel"]}"/>'
    return s


def boots(xs, toe=0):
    s = ""
    for x in xs:
        s += part(f"M {x - 34} -88 L {x + 34} -88 L {x + 36} -20 Q {x + 36 + toe} 2 {x + toe * 0.5} 2 L {x - 36} 2 Q {x - 42} -10 {x - 36} -24 Z",
                  C["boot"], C["boot_s"], off=(10, 8))
        s += f'<path d="M {x - 30} -70 L {x + 30} -70" stroke="{C["boot_s"]}" stroke-width="4"/>'
    return s


def hat(cx, back=False, side=0):
    """Меховая шапка: бугристый околыш, красный суконный верх с кистью."""
    s = part(f"M {cx - 82} -448 Q {cx - 88 + side} -548 {cx + side * 1.4} -560 Q {cx + 88 + side} -548 {cx + 82} -448 Z", C["cloth"], C["cloth_s"],
             off=(14, 10))
    s += f'<path d="M {cx + side * 1.4} -558 Q {cx - 10 + side} -500 {cx} -450" fill="none" stroke="{C["cloth_s"]}" stroke-width="4"/>'
    s += f'<circle cx="{cx + side * 1.5}" cy="-566" r="13" fill="{C["gold"]}" stroke="{C["ol"]}" stroke-width="4"/>'
    s += part(f"M {cx - 104} -462 Q {cx} -486 {cx + 104} -462 L {cx + 108} -420 Q {cx} -398 {cx - 108} -420 Z", C["fur"], C["fur_s"], off=(12, 8))
    for k in range(-5, 6):  # ворс
        x = cx + k * 19
        s += f'<path d="M {x - 6} -468 q 6 -10 12 0" fill="none" stroke="{C["fur_s"]}" stroke-width="3.5"/>'
    return s


def coat(cx=0, w=1.0, back=False):
    d = (f"M {cx - 105 * w} -345 C {cx - 135 * w} -300 {cx - 160 * w} -200 {cx - 172 * w} -96 Q {cx} -70 {cx + 172 * w} -96 "
         f"C {cx + 160 * w} -200 {cx + 135 * w} -300 {cx + 105 * w} -345 Q {cx} -372 {cx - 105 * w} -345 Z")
    s = part(d, C["coat"], C["coat_s"], off=(30, 14))
    if not back:
        s += f'<rect x="{cx - 22}" y="-345" width="44" height="250" fill="{C["fleece"]}" stroke="{C["ol"]}" stroke-width="4"/>'
        s += fleece_line(lerp_pts((cx - 22, -330), (cx - 22, -110), 9), 10) + fleece_line(lerp_pts((cx + 22, -330), (cx + 22, -110), 9), 10)
    else:
        s += f'<path d="M {cx} -340 L {cx} -100" stroke="{C["coat_s"]}" stroke-width="5"/>'
        for k in (-1, 1):  # заплата
            pass
        s += part(f"M {cx + 40} -200 l 46 -4 l 4 40 l -46 4 Z", "#C68A4E", C["coat_s"], off=(4, 4), sw=4)
        s += f'<path d="M {cx + 44} -196 l 38 36 M {cx + 84} -198 l -36 36" stroke="{C["ol"]}" stroke-width="2.5" stroke-dasharray="5 4"/>'
    s += fleece_line(lerp_pts((cx - 168 * w, -96), (cx + 168 * w, -96), 15), 16)
    return s


def sash(cx=0, w=1.0, knot=-60, back=False):
    d = f"M {cx - 150 * w} -252 Q {cx} -232 {cx + 150 * w} -252 L {cx + 153 * w} -218 Q {cx} -198 {cx - 153 * w} -218 Z"
    s = part(d, C["sash"], C["sash_s"], off=(10, 6))
    k = cx + knot
    s += part(f"M {k - 8} -228 Q {k - 26} -190 {k - 30} -150 L {k - 6} -152 Q {k} -190 {k + 4} -226 Z", C["sash"], C["sash_s"], off=(5, 3), sw=4)
    s += part(f"M {k + 4} -228 Q {k + 12} -190 {k + 22} -160 L {k + 42} -166 Q {k + 26} -200 {k + 16} -230 Z", C["sash"], C["sash_s"], off=(5, 3), sw=4)
    s += part(circ(k + 4, -234, 17), C["sash"], C["sash_s"], off=(5, 4), sw=4)
    return s


# ---------- виды ----------
def front():
    s = boots((-46, 46))
    # рука со щитом (зритель справа) — за щитом, щит рисуется позже
    s += part("M 95 -345 Q 150 -320 172 -230 L 124 -214 Q 110 -290 70 -320 Z", C["coat"], C["coat_s"], off=(12, 8))
    s += coat()
    s += sash()
    # рука с топором (зритель слева)
    s += part("M -95 -345 Q -150 -320 -176 -232 L -126 -214 Q -110 -290 -70 -320 Z", C["coat"], C["coat_s"], off=(12, 8))
    s += fleece_line(arc_pts(-150, -224, 30, 10, 160, 380, 5), 12)
    s += axe(-152, -190, -186, -480)
    s += part(circ(-152, -194, 34), C["skin"], C["skin_s"], off=(8, 6))
    s += f'<path d="M -178 -200 q 14 -6 26 2 M -178 -186 q 14 -6 26 2" fill="none" stroke="{C["skin_s"]}" stroke-width="4"/>'
    # ворот овчиной
    s += fleece_line(arc_pts(0, -342, 110, 26, 180, 360, 11), 17)
    # голова
    s += part(circ(-82, -382, 19), C["skin"], C["skin_s"], off=(4, 3)) + part(circ(82, -382, 19), C["skin"], C["skin_s"], off=(4, 3))
    s += part(circ(0, -382, 80), C["skin"], C["skin_s"], off=(18, 12))
    s += f'<ellipse cx="-48" cy="-352" rx="17" ry="10" fill="{C["cheek"]}" opacity="0.55"/><ellipse cx="48" cy="-352" rx="17" ry="10" fill="{C["cheek"]}" opacity="0.55"/>'
    s += part("M -74 -350 Q -86 -250 0 -206 Q 86 -250 74 -350 Q 40 -322 0 -330 Q -40 -322 -74 -350 Z", C["beard"], C["beard_s"], off=(14, 10))
    s += f'<path d="M -30 -300 q 8 30 0 60 M 0 -300 q 6 34 0 76 M 30 -300 q -8 30 0 60" fill="none" stroke="{C["beard_s"]}" stroke-width="4"/>'
    s += part("M -4 -344 Q -40 -360 -62 -334 Q -40 -326 -4 -332 Z", C["beard"], C["beard_s"], off=(4, 3), sw=4)
    s += part("M 4 -344 Q 40 -360 62 -334 Q 40 -326 4 -332 Z", C["beard"], C["beard_s"], off=(4, 3), sw=4)
    s += part(circ(0, -362, 19), C["skin"], C["skin_s"], off=(5, 4), sw=4)
    for x in (-30, 30):
        s += f'<ellipse cx="{x}" cy="-396" rx="8" ry="11" fill="{C["eye"]}"/><circle cx="{x - 2}" cy="-400" r="3" fill="#fff"/>'
    s += f'<path d="M -48 -418 L -14 -408 M 48 -418 L 14 -408" stroke="{C["beard_s"]}" stroke-width="9" stroke-linecap="round"/>'
    s += hat(0)
    # щит на левой руке
    s += shield_face(176, -222)
    return s


def side():
    """Профиль вправо: видна правая сторона — рука с топором; щит на дальней руке выглядывает ребром."""
    s = part("M 60 -330 Q 92 -330 96 -230 Q 92 -120 60 -112 Q 48 -220 60 -330 Z", C["wood"], C["wood_s"], off=(4, 4))
    s += f'<path d="M 66 -320 Q 86 -230 66 -122" fill="none" stroke="{C["iron"]}" stroke-width="10"/>'
    s += boots((-6,), toe=26)
    d = "M -62 -345 C -96 -280 -122 -180 -128 -96 Q 0 -78 122 -96 C 116 -180 96 -280 70 -345 Q 0 -368 -62 -345 Z"
    s += part(d, C["coat"], C["coat_s"], off=(26, 14))
    s += fleece_line(lerp_pts((-124, -96), (118, -96), 11), 16)
    s += fleece_line(lerp_pts((56, -330), (82, -110), 9), 10)
    s += part("M -116 -252 Q 0 -236 108 -252 L 112 -218 Q 0 -202 -120 -218 Z", C["sash"], C["sash_s"], off=(8, 5))
    s += part("M -112 -228 Q -132 -190 -138 -152 L -116 -152 Q -108 -190 -96 -226 Z", C["sash"], C["sash_s"], off=(4, 3), sw=4)
    # рука с топором, вынесена вперёд
    s += part("M -40 -345 Q 10 -330 18 -230 L -30 -220 Q -34 -290 -52 -318 Z", C["coat"], C["coat_s"], off=(12, 8))
    s += fleece_line(arc_pts(-6, -226, 28, 10, 160, 380, 5), 12)
    s += part("M -14 -192 L -44 -476 L -28 -476 L 4 -192 Z", C["haft"], C["haft_s"], off=(5, 0))
    s += part("M -36 -470 L 50 -506 Q 68 -444 48 -390 L -36 -428 Z", C["steel"], C["steel_s"], off=(10, 8))
    s += f'<path d="M 50 -502 Q 66 -444 46 -394" fill="none" stroke="#FFFFFF" stroke-width="5" opacity="0.8"/>'
    s += part(circ(-6, -194, 33), C["skin"], C["skin_s"], off=(8, 6))
    s += fleece_line(arc_pts(-10, -340, 82, 20, 190, 350, 8), 16)
    s += part(circ(-30, -384, 19), C["skin"], C["skin_s"], off=(4, 3))
    s += part(circ(8, -382, 80), C["skin"], C["skin_s"], off=(18, 12))
    s += f'<ellipse cx="44" cy="-352" rx="15" ry="10" fill="{C["cheek"]}" opacity="0.55"/>'
    s += part("M -6 -340 Q 40 -318 92 -326 Q 96 -262 60 -222 Q 20 -236 -10 -300 Z", C["beard"], C["beard_s"], off=(12, 8))
    s += f'<path d="M 30 -310 q 14 30 12 60 M 60 -318 q 8 30 0 64" fill="none" stroke="{C["beard_s"]}" stroke-width="4"/>'
    s += part("M 60 -346 Q 100 -356 112 -334 Q 92 -326 60 -332 Z", C["beard"], C["beard_s"], off=(4, 3), sw=4)
    s += part(circ(86, -366, 20), C["skin"], C["skin_s"], off=(5, 4), sw=4)
    s += f'<ellipse cx="52" cy="-396" rx="7" ry="11" fill="{C["eye"]}"/><circle cx="51" cy="-400" r="3" fill="#fff"/>'
    s += f'<path d="M 34 -420 L 70 -412" stroke="{C["beard_s"]}" stroke-width="9" stroke-linecap="round"/>'
    s += hat(4, side=-14)
    return s


def back():
    """Сзади: шапка с околышем по кругу, спинка тулупа с заплатой, узел кушака, топор справа, изнанка щита слева."""
    s = boots((-46, 46))
    s += part("M -95 -345 Q -150 -320 -172 -230 L -124 -214 Q -110 -290 -70 -320 Z", C["coat"], C["coat_s"], off=(12, 8))
    s += part(circ(-176, -222, 90), C["wood"], C["wood_s"], off=(14, 12))
    s += f'<rect x="-196" y="-262" width="40" height="80" rx="8" fill="{C["haft"]}" stroke="{C["ol"]}" stroke-width="4"/>'
    s += f'<path d="M -262 -232 L -90 -232 M -262 -206 L -90 -206" stroke="{C["haft_s"]}" stroke-width="10"/>'
    s += f'<circle cx="-176" cy="-222" r="84" fill="none" stroke="{C["iron"]}" stroke-width="12"/>'
    s += f'<circle cx="-176" cy="-222" r="90" fill="none" stroke="{C["ol"]}" stroke-width="5"/>'
    s += coat(back=True)
    s += sash(knot=50)
    s += part("M 95 -345 Q 150 -320 176 -232 L 126 -214 Q 110 -290 70 -320 Z", C["coat"], C["coat_s"], off=(12, 8))
    s += fleece_line(arc_pts(150, -224, 30, 10, 160, 380, 5), 12)
    s += axe(152, -190, 186, -480, flip=-1)
    s += part(circ(152, -194, 34), C["skin"], C["skin_s"], off=(8, 6))
    s += fleece_line(arc_pts(0, -342, 110, 26, 180, 360, 11), 17)
    s += part(circ(-82, -382, 19), C["skin"], C["skin_s"], off=(4, 3)) + part(circ(82, -382, 19), C["skin"], C["skin_s"], off=(4, 3))
    s += part(circ(0, -382, 80), C["beard"], C["beard_s"], off=(18, 12))
    s += f'<path d="M -40 -440 q -6 50 0 100 M 0 -446 q -4 56 0 112 M 40 -440 q 6 50 0 100" fill="none" stroke="{C["beard_s"]}" stroke-width="4"/>'
    s += part("M -50 -312 Q 0 -296 50 -312 L 46 -336 Q 0 -324 -46 -336 Z", C["skin"], C["skin_s"], off=(4, 3), sw=4)
    s += hat(0, back=True)
    return s


# ---------- лист ----------
W, H = 2000, 1240
GROUND = 880
body = []
body.append(f'<rect width="{W}" height="{H}" fill="#EFE6D4"/>')
for x in range(0, W, 40):
    body.append(f'<line x1="{x}" y1="0" x2="{x}" y2="{H}" stroke="#DCCFB6" stroke-width="1"/>')
for y in range(0, H, 40):
    body.append(f'<line x1="0" y1="{y}" x2="{W}" y2="{y}" stroke="#DCCFB6" stroke-width="1"/>')
body.append('<text x="60" y="80" font-family="Rubik" font-weight="700" font-size="46" fill="#3A2416">ОПОЛЧЕНЕЦ · чертёж</text>')
body.append('<text x="60" y="122" font-family="Rubik" font-size="22" fill="#6B5444">Дешёвая пехота острога: мужик в тулупе с топором и щитом. Особенность — рубит частокол ×1,6. '
            'Придуман с нуля, не по реальной анатомии.</text>')
# сетка пропорций: голова с шапкой ≈ 0,46 роста
HEAD = 120
marks = [(0, "земля"), (-92, "подол / верх валенок"), (-235, "кушак"), (-302, "низ бороды"), (-345, "плечи"),
         (-462, "околыш"), (-566, "макушка")]
for y, label in marks:
    yy = GROUND + y
    body.append(f'<line x1="60" y1="{yy}" x2="1570" y2="{yy}" stroke="#B49A78" stroke-width="2" stroke-dasharray="10 8"/>')
    body.append(f'<text x="64" y="{yy - 8}" font-family="Rubik" font-size="17" fill="#8A6F55">{label}</text>')
views = [(450, "СПЕРЕДИ", front()), (885, "СБОКУ", side()), (1300, "СЗАДИ", back())]
for x, title, g in views:
    body.append(f'<ellipse cx="{x}" cy="{GROUND + 4}" rx="190" ry="26" fill="#000" opacity="0.12"/>')
    body.append(f'<g transform="translate({x},{GROUND})">{g}</g>')
    body.append(f'<text x="{x}" y="{GROUND + 70}" text-anchor="middle" font-family="Rubik" font-weight="700" font-size="26" fill="#3A2416">{title}</text>')
# пропорции словами
notes = ["Рост 566 = 4,7 «головы»; голова с шапкой — 46% роста",
         "Кисти крупные (Ø 68 — почти пол-лица): топор читается издалека",
         "Ноги короткие, валенки тяжёлые — устойчивый «мешок» внизу",
         "Тулуп колоколом: силуэт — треугольник, отличается от всех",
         "Овчина крупными завитками: ворот, подол, обшлага, пола",
         "Красный кушак с узлом и концами — главный цветовой акцент",
         "Шапка: бурый мех и красный верх с золотой кистью",
         "Топор: клин больше головы, светлая кромка, обмотка рукояти",
         "Щит: доски, красное кольцо, железный обод с заклёпками",
         "Сзади: заплата на тулупе, узел кушака — характер «мужика»"]
for i, t in enumerate(notes):
    body.append(f'<text x="1590" y="{200 + i * 46}" font-family="Rubik" font-size="19" fill="#3A2416">• {t}</text>') if False else None
body.append('<rect x="1595" y="160" width="385" height="560" rx="16" fill="#F7F0E2" stroke="#B49A78" stroke-width="2"/>')
body.append('<text x="1605" y="198" font-family="Rubik" font-weight="700" font-size="22" fill="#3A2416">Детали и пропорции</text>')
import textwrap
yy = 232
for t in notes:
    lines = textwrap.wrap(t, 34)
    for j, ln in enumerate(lines):
        body.append(f'<text x="{1605 if j == 0 else 1621}" y="{yy}" font-family="Rubik" font-size="16" fill="#3A2416">{"• " if j == 0 else ""}{ln}</text>')
        yy += 21
    yy += 8
# палитра
pal = [("Тулуп", C["coat"]), ("Овчина", C["fleece"]), ("Кушак", C["sash"]), ("Мех", C["fur"]), ("Валенки", C["boot"]),
       ("Кожа", C["skin"]), ("Борода", C["beard"]), ("Сталь", C["steel"]), ("Дерево", C["wood"]), ("Контур", C["ol"])]
body.append('<text x="1605" y="762" font-family="Rubik" font-weight="700" font-size="22" fill="#3A2416">Палитра</text>')
for i, (n, c) in enumerate(pal):
    x = 1605 + (i % 5) * 74
    y = 778 + (i // 5) * 86
    body.append(f'<rect x="{x}" y="{y}" width="60" height="44" rx="8" fill="{c}" stroke="#3A2416" stroke-width="3"/>')
    body.append(f'<text x="{x + 30}" y="{y + 64}" text-anchor="middle" font-family="Rubik" font-size="13" fill="#3A2416">{n}</text>')
# проверка силуэта: как он выглядит на поле боя
body.append('<text x="1605" y="985" font-family="Rubik" font-weight="700" font-size="22" fill="#3A2416">Силуэт на поле</text>')
for i, sc in enumerate((0.28, 0.16, 0.09)):
    x = (1690, 1835, 1930)[i]
    body.append(f'<g transform="translate({x},{1170}) scale({sc})" filter="url(#sil)">{front()}</g>')
defs.append('<filter id="sil"><feColorMatrix type="matrix" values="0 0 0 0 0.16  0 0 0 0 0.11  0 0 0 0 0.08  0 0 0 1 0"/></filter>')
svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}"><defs>{"".join(defs)}</defs>'
       + "".join(b for b in body if b) + "</svg>")
with open(OUT, "w") as f:
    f.write(svg)
print("->", OUT)
