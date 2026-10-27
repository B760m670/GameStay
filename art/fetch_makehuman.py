#!/usr/bin/env python3
"""Загрузка данных MakeHuman (CC0) для сборки людей-войск в Blender (art/blender/human_kit.py).

Берутся только данные: базовый меш, таргеты пропорций, риг и веса из пакета MPFB и системные ассеты (кожа, причёски,
брови). Код аддона не устанавливается и не запускается. Файлы ложатся в art/source/makehuman/ (в git не хранятся).

Использование: python3 art/fetch_makehuman.py
"""
import os
import re
import urllib.request
import zipfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "source", "makehuman")
MPFB = ("https://extensions.blender.org/download/sha256:c075b3e9b94b8e5556a5f03f4653602d02ea306de08126e8f55ed6dee28a2dad/"
        "add-on-mpfb-v2.0.18.zip?repository=%2Fapi%2Fv1%2Fextensions%2F&blender_version_min=4.2.0")
SYSTEM = "https://files.makehumancommunity.org/asset_packs/makehuman_system_assets/makehuman_system_assets_cc0.zip"
DATA_FILES = re.compile(r"data/(3dobjs/base\.obj|rigs/standard/(rig|weights)\.default\.json|targets/macrodetails/.*|poses/default_fk/.*)$")
SYS_DIRS = ("skins", "eyes", "eyebrows", "hair")


def download(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return
    req = urllib.request.Request(url, headers={"User-Agent": "ostrog-art-pipeline/1.0"})
    with urllib.request.urlopen(req, timeout=900) as r, open(path + ".part", "wb") as f:
        f.write(r.read())
    os.replace(path + ".part", path)


def extract(zpath, pick, dest):
    with zipfile.ZipFile(zpath) as z:
        for n in z.namelist():
            out = pick(n)
            if not out or n.endswith("/"):
                continue
            target = os.path.normpath(os.path.join(dest, out))
            if not target.startswith(os.path.normpath(dest)):  # пути из архива — только внутрь папки
                continue
            os.makedirs(os.path.dirname(target), exist_ok=True)
            with open(target, "wb") as f:
                f.write(z.read(n))


def main():
    os.makedirs(ROOT, exist_ok=True)
    mpfb, system = os.path.join(ROOT, "mpfb.zip"), os.path.join(ROOT, "system_assets.zip")
    download(MPFB, mpfb)
    download(SYSTEM, system)
    extract(mpfb, lambda n: "data/" + n[5:] if DATA_FILES.match(n) else None, ROOT)
    extract(system, lambda n: "sys/" + n if n.split("/")[0] in SYS_DIRS else None, ROOT)
    print("makehuman data ->", ROOT)


if __name__ == "__main__":
    main()
