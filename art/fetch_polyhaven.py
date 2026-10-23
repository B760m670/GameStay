#!/usr/bin/env python3
"""Загрузка CC0-ассетов Poly Haven (фотосканы моделей и фототекстуры) для сборки построек в Blender.

Использование: python3 art/fetch_polyhaven.py <res> <asset> [<asset> ...]
  res — 1k/2k/4k. И модели, и текстуры качаются в формате glTF с картами (цвет, нормали, шероховатость) в textures/.
Файлы ложатся в art/source/polyhaven/<asset>/ (в git не хранятся: скрипт воспроизводит их по списку).
Лицензия Poly Haven — CC0: можно использовать как угодно, в том числе в коммерческой игре.
"""
import json
import os
import sys
import urllib.request

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "source", "polyhaven")
API = "https://api.polyhaven.com"
HEADERS = {"User-Agent": "lanternfall-art-pipeline/1.0"}  # без него сервер отвечает 403


def _open(url, timeout):
    return urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=timeout)


def get_json(url):
    with _open(url, 60) as r:
        return json.load(r)


def download(url, path):
    if os.path.exists(path) and os.path.getsize(path) > 0:
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with _open(url, 300) as r, open(path + ".part", "wb") as f:
        f.write(r.read())
    os.replace(path + ".part", path)


def fetch(asset, res):
    """Модель или текстура — всё приходит как glTF (у текстуры это плоскость) с картами в textures/."""
    files = get_json(f"{API}/files/{asset}")
    entry = files["gltf"][res]["gltf"]
    out = os.path.join(ROOT, asset)
    download(entry["url"], os.path.join(out, f"{asset}.gltf"))
    for rel, inc in entry.get("include", {}).items():
        download(inc["url"], os.path.join(out, rel))
    print(f"{asset}: {res} -> {out}")


def main():
    res, assets = sys.argv[1], sys.argv[2:]
    for a in assets:
        fetch(a, res)


if __name__ == "__main__":
    main()
