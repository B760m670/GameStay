"""Человек для спрайтов войск: базовый меш MakeHuman (CC0), пропорции по макро-таргетам, скелет и веса из JSON.

Используются только данные (OBJ, таргеты, JSON рига и весов) из архивов MakeHuman/MPFB — сам аддон не нужен.
Данные кладёт art/fetch_makehuman.py в art/source/makehuman/ (в git не хранятся).

Координаты MakeHuman: дециметры, Y вверх, лицо к +Z. В Blender: метры, Z вверх, лицо к −Y (как у камеры игры: видно
грань −Y слева и +X справа). Потом весь человек масштабируется под рост юнита в клетках (клетка ≈ 2,5 м).
"""
import gzip
import itertools
import json
import os

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "source", "makehuman")
DATA = os.path.join(ROOT, "data")
SYS = os.path.join(ROOT, "sys")

_OBJ = None  # кэш разобранного base.obj


def _base_obj():
    """base.obj: вершины, UV и грани по группам (body, joint-*, helper-*)."""
    global _OBJ
    if _OBJ is not None:
        return _OBJ
    verts, uvs, groups = [], [], {}
    cur = None
    with open(os.path.join(DATA, "3dobjs", "base.obj")) as f:
        for line in f:
            if line.startswith("v "):
                verts.append([float(x) for x in line.split()[1:4]])
            elif line.startswith("vt "):
                uvs.append([float(x) for x in line.split()[1:3]])
            elif line.startswith("g "):
                cur = groups.setdefault(line.split(None, 1)[1].strip(), [])
            elif line.startswith("f ") and cur is not None:
                face = []
                for tok in line.split()[1:]:
                    p = tok.split("/")
                    face.append((int(p[0]) - 1, int(p[1]) - 1 if len(p) > 1 and p[1] else -1))
                cur.append(face)
    _OBJ = (verts, uvs, groups)
    return _OBJ


def _read_target(path):
    with gzip.open(path, "rt") if path.endswith(".gz") else open(path) as f:
        for line in f:
            p = line.split()
            if len(p) == 4 and not line.startswith("#"):
                yield int(p[0]), float(p[1]), float(p[2]), float(p[3])


def _split(value, parts):
    """Значение макро-ползунка -> {метка: вес} по частям из macro.json."""
    for part in parts:
        if part["lowest"] <= value <= part["highest"]:
            t = (value - max(0.0, part["lowest"])) / (min(1.0, part["highest"]) - max(0.0, part["lowest"]))
            t = min(1.0, max(0.0, t))
            out = {}
            if part["low"]:
                out[part["low"]] = out.get(part["low"], 0) + 1 - t
            if part["high"]:
                out[part["high"]] = out.get(part["high"], 0) + t
            return out
    return {}


def macro_weights(gender=1.0, age=0.5, muscle=0.5, weight=0.5, height=0.5, proportions=0.5, race="caucasian"):
    """Таргеты MakeHuman и их веса для заданных макро-параметров (как в самом MakeHuman)."""
    macro = json.load(open(os.path.join(DATA, "targets", "macrodetails", "macro.json")))["macrotargets"]
    vals = {"gender": gender, "age": age, "muscle": muscle, "weight": weight, "height": height, "proportions": proportions}
    split = {k: _split(v, macro[k]["parts"]) for k, v in vals.items()}
    split["race"] = {race: 1.0}
    out = {}

    def combos(keys):
        for combo in itertools.product(*[split[k].items() for k in keys]):
            w = 1.0
            for _, x in combo:
                w *= x
            if w > 1e-4:
                yield [c[0] for c in combo], w

    d = os.path.join(DATA, "targets", "macrodetails")
    for labels, w in combos(["gender", "age", "muscle", "weight"]):
        out[os.path.join(d, "universal-" + "-".join(labels) + ".target.gz")] = w
    for labels, w in combos(["race", "gender", "age"]):
        out[os.path.join(d, "-".join(labels) + ".target.gz")] = w
    for labels, w in combos(["gender", "age", "muscle", "weight", "height"]):
        if labels[-1]:
            out[os.path.join(d, "height", "-".join(labels) + ".target.gz")] = w
    for labels, w in combos(["gender", "age", "muscle", "weight", "proportions"]):
        if labels[-1]:
            out[os.path.join(d, "proportions", "-".join(labels) + ".target.gz")] = w
    return {p: w for p, w in out.items() if os.path.exists(p)}


def _to_blender(v):
    return Vector((v[0] * 0.1, -v[2] * 0.1, v[1] * 0.1))


class Human:
    """Меш тела с UV, скелет (риг default без лицевых костей не нужен — берём все) и веса.

    После build(): self.body — меш, self.arm — арматура, self.coords — исходные координаты MakeHuman после таргетов
    (для подгонки причёсок), self.ground — сдвиг, ставящий ступни на землю."""

    def __init__(self, name="human", targets=(), **macro):
        self.name = name
        self.macro = macro
        self.targets = list(targets)  # дополнительные таргеты: (путь относительно data/targets, вес) — например, лошадь

    def build(self, skin="young_caucasian_male"):
        verts, uvs, groups = _base_obj()
        coords = [list(v) for v in verts]
        extra = {os.path.join(DATA, "targets", p): w for p, w in self.targets}
        for path, w in list(macro_weights(**self.macro).items()) + list(extra.items()):
            for i, dx, dy, dz in _read_target(path):
                c = coords[i]
                c[0] += dx * w
                c[1] += dy * w
                c[2] += dz * w
        self.coords = coords
        body_faces = groups["body"]
        used = sorted({vi for f in body_faces for vi, _ in f})
        zmin = min(coords[i][1] for i in used)
        self.ground = -zmin * 0.1
        self.jgroups = {g: sorted({vi for f in fs for vi, _ in f}) for g, fs in groups.items() if g.startswith("joint-")}
        # меш: все вершины (индексы совпадают с весами), грани только тела; лишние вершины удаляются в конце
        me = bpy.data.meshes.new(self.name)
        bm = bmesh.new()
        uvl = bm.loops.layers.uv.new("UVMap")  # слои — до вершин: добавление слоя пересоздаёт их
        dl = bm.verts.layers.deform.verify()
        bv = [bm.verts.new(self.pos(i)) for i in range(len(coords))]
        bm.verts.ensure_lookup_table()
        for f in body_faces:
            try:
                face = bm.faces.new([bv[vi] for vi, _ in f])
            except ValueError:
                continue
            face.smooth = True
            for loop, (_, ti) in zip(face.loops, f):
                if ti >= 0:
                    loop[uvl].uv = uvs[ti]
        weights = json.load(open(os.path.join(DATA, "rigs", "standard", "weights.default.json")))["weights"]
        self.bone_names = list(weights)
        for gi, bone in enumerate(self.bone_names):
            for vi, w in weights[bone]:
                bv[vi][dl][gi] = w
        loose = [v for v in bm.verts if not v.link_faces]
        self.weights = weights
        bmesh.ops.delete(bm, geom=loose, context="VERTS")
        bm.to_mesh(me)
        bm.free()
        self.body = bpy.data.objects.new(self.name, me)
        for b in self.bone_names:
            self.body.vertex_groups.new(name=b)
        bpy.context.scene.collection.objects.link(self.body)
        self.body.data.materials.append(skin_material(skin))
        self._armature()
        return self

    def pos(self, i):
        p = _to_blender(self.coords[i])
        p.z += self.ground
        return p

    def _joint(self, spec):
        s = spec["strategy"]
        if s == "CUBE":
            idx = self.jgroups[spec["cube_name"]]
        elif s == "VERTEX":
            idx = [spec["vertex_index"]]
        else:
            idx = spec["vertex_indices"]
        acc = Vector()
        for i in idx:
            acc += self.pos(i)
        return acc / len(idx)

    def _armature(self):
        rig = json.load(open(os.path.join(DATA, "rigs", "standard", "rig.default.json")))
        data = bpy.data.armatures.new(self.name + "_rig")
        arm = bpy.data.objects.new(self.name + "_rig", data)
        bpy.context.scene.collection.objects.link(arm)
        bpy.context.view_layer.objects.active = arm
        bpy.ops.object.mode_set(mode="EDIT")
        eb = data.edit_bones
        for name, d in rig.items():
            b = eb.new(name)
            b.head = self._joint(d["head"])
            b.tail = self._joint(d["tail"])
            if (b.tail - b.head).length < 1e-4:
                b.tail = b.head + Vector((0, 0, 0.01))
            b.roll = d.get("roll", 0)
        for name, d in rig.items():
            if d.get("parent"):
                eb[name].parent = eb[d["parent"]]
                eb[name].use_connect = bool(d.get("use_connect"))
        bpy.ops.object.mode_set(mode="OBJECT")
        for pb in arm.pose.bones:
            pb.rotation_mode = "QUATERNION"
        self.body.parent = arm
        md = self.body.modifiers.new("Armature", "ARMATURE")
        md.object = arm
        self.arm = arm

    # ---------- подгонка ассетов MakeHuman (причёски, брови) ----------
    def fit(self, kind, name, material=None):
        """Причёска/брови из системных ассетов (.mhclo): каждая вершина — взвешенная сумма трёх вершин тела + смещение."""
        d = os.path.join(SYS, kind, name)
        mhclo = open(os.path.join(d, name + ".mhclo")).read().splitlines()
        scales, refs, obj_file = {}, [], None
        in_verts = False
        for line in mhclo:
            p = line.split()
            if not p or p[0].startswith("#"):
                continue
            if in_verts and not p[0].isalpha():  # после «verts» числовые строки — опорные вершины (ключи могут идти вперемешку)
                refs.append(p)
                continue
            if p[0] in ("x_scale", "y_scale", "z_scale"):
                a, b, dist = int(p[1]), int(p[2]), float(p[3])
                ax = "xyz".index(p[0][0])
                scales[ax] = abs(self.coords[a][ax] - self.coords[b][ax]) / dist
            elif p[0] == "obj_file":
                obj_file = p[1]
            elif p[0] == "verts":
                in_verts = True
        verts, uvs, faces = [], [], []
        for line in open(os.path.join(d, obj_file)):
            if line.startswith("v "):
                verts.append(line)
            elif line.startswith("vt "):
                uvs.append([float(x) for x in line.split()[1:3]])
            elif line.startswith("f "):
                faces.append([(int(t.split("/")[0]) - 1, int(t.split("/")[1]) - 1 if "/" in t and t.split("/")[1] else -1)
                              for t in line.split()[1:]])
        pos, wsum = [], []
        for r in refs:
            if len(r) == 1:
                v = self.coords[int(r[0])]
                pos.append(list(v))
                wsum.append({int(r[0]): 1.0})
                continue
            i = [int(x) for x in r[:3]]
            w = [float(x) for x in r[3:6]]
            off = [float(x) for x in r[6:9]]
            p = [sum(w[k] * self.coords[i[k]][ax] for k in range(3)) + off[ax] * scales.get(ax, 1.0) for ax in range(3)]
            pos.append(p)
            wsum.append({i[k]: w[k] for k in range(3)})
        me = bpy.data.meshes.new(name)
        bm = bmesh.new()
        uvl = bm.loops.layers.uv.new("UVMap")
        bv = []
        for p in pos:
            v = _to_blender(p)
            v.z += self.ground
            bv.append(bm.verts.new(v))
        for f in faces:
            try:
                face = bm.faces.new([bv[vi] for vi, _ in f])
            except ValueError:
                continue
            face.smooth = True
            for loop, (_, ti) in zip(face.loops, f):
                if ti >= 0:
                    loop[uvl].uv = uvs[ti]
        bm.to_mesh(me)
        bm.free()
        o = bpy.data.objects.new(name, me)
        bpy.context.scene.collection.objects.link(o)
        # веса — от опорных вершин тела
        idx = {b: i for i, b in enumerate(self.bone_names)}
        per_vert = {}
        for b, lst in self.weights.items():
            for vi, w in lst:
                per_vert.setdefault(vi, []).append((b, w))
        groups = {}
        for vi, ref in enumerate(wsum):
            acc = {}
            for bvi, w in ref.items():
                for b, bw in per_vert.get(bvi, []):
                    acc[b] = acc.get(b, 0) + bw * w
            for b, w in acc.items():
                if w > 0.01:
                    if b not in groups:
                        groups[b] = o.vertex_groups.new(name=b)
                    groups[b].add([vi], w, "REPLACE")
        del idx
        o.parent = self.arm
        md = o.modifiers.new("Armature", "ARMATURE")
        md.object = self.arm
        o.data.materials.append(material or image_material(name, os.path.join(d, _mhmat_texture(d, name)), alpha=True))
        return o

    # ---------- поза ----------
    def bone(self, name):
        return self.arm.pose.bones[name]

    def rest_axis(self, name, axis):
        """Ось в локальных координатах кости, соответствующая мировой оси axis в позе покоя."""
        m = self.arm.data.bones[name].matrix_local.to_3x3()
        return (m.inverted() @ Vector(axis)).normalized()

    def rotate(self, name, axis, angle):
        """Повернуть кость вокруг мировой оси (в позе покоя) на angle радиан — поверх уже заданного поворота."""
        pb = self.bone(name)
        q = Quaternion(self.rest_axis(name, axis), angle)
        pb.rotation_quaternion = q @ pb.rotation_quaternion

    def reset_pose(self):
        for pb in self.arm.pose.bones:
            pb.rotation_quaternion = (1, 0, 0, 0)
            pb.location = (0, 0, 0)

    def key(self, frame):
        for pb in self.arm.pose.bones:
            pb.keyframe_insert("rotation_quaternion", frame=frame)
            pb.keyframe_insert("location", frame=frame)

    def world_head(self, bone):
        return self.arm.matrix_world @ self.arm.pose.bones[bone].head

    def bone_matrix(self, bone):
        return self.arm.matrix_world @ self.arm.pose.bones[bone].matrix


def _mhmat_texture(d, name):
    for line in open(os.path.join(d, name + ".mhmat")):
        if line.startswith("diffuseTexture"):
            return line.split()[1]
    return name + "_diffuse.png"


def image_material(name, path, alpha=False, rough=0.7, tint=None):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    N, L = m.node_tree.nodes, m.node_tree.links
    bsdf = N["Principled BSDF"]
    bsdf.inputs["Roughness"].default_value = rough
    t = N.new("ShaderNodeTexImage")
    t.image = bpy.data.images.load(path, check_existing=True)
    col = t.outputs["Color"]
    if tint is not None:
        mix = N.new("ShaderNodeMix")
        mix.data_type = "RGBA"
        mix.blend_type = "MULTIPLY"
        mix.inputs["Factor"].default_value = 1.0
        mix.inputs["B"].default_value = (*tint, 1)
        L.new(col, mix.inputs["A"])
        col = mix.outputs["Result"]
    L.new(col, bsdf.inputs["Base Color"])
    if alpha:
        L.new(t.outputs["Alpha"], bsdf.inputs["Alpha"])
        m.blend_method = "HASHED"
    return m


def skin_material(skin):
    d = os.path.join(SYS, "skins", skin)
    tex = next(f for f in os.listdir(d) if f.endswith(".png"))
    m = image_material("skin_" + skin, os.path.join(d, tex), rough=0.55)
    bsdf = m.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Subsurface Weight"].default_value = 0.08
    bsdf.inputs["Subsurface Radius"].default_value = (0.4, 0.15, 0.08)
    return m


def axis_angle_matrix(axis, angle):
    return Matrix.Rotation(angle, 4, Vector(axis))
