#!/usr/bin/env python3
"""Генерирует source.json (формат AltStore/SideStore) для LiveContainer.

Использование: make_source.py <ipa> <version> <build> <download_url> <out>
"""
import json, os, sys
from datetime import datetime, timezone

ipa, version, build, url, out = sys.argv[1:6]
repo = os.environ.get("GITHUB_REPOSITORY", "B760m670/GameStay")
raw = f"https://raw.githubusercontent.com/{repo}/main/distribution"

source = {
    "name": "Lanternfall",
    "identifier": "com.gamestay.lanternfall.source",
    "subtitle": "Сборки Lanternfall: Хранители Мглы",
    "iconURL": f"{raw}/icon.png",
    "website": f"https://github.com/{repo}",
    "tintColor": "#FFB84A",
    "apps": [{
        "name": "Lanternfall",
        "bundleIdentifier": "com.gamestay.lanternfall",
        "developerName": "GameStay",
        "subtitle": "Стратегия о свете и Мгле",
        "localizedDescription": "Строй поселение в круге света, гаси чужие Светочи и переживи ночь Мглы.",
        "iconURL": f"{raw}/icon.png",
        "tintColor": "#FFB84A",
        "category": "games",
        "versions": [{
            "version": version,
            "buildVersion": build,
            "date": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "downloadURL": url,
            "size": os.path.getsize(ipa),
            "minOSVersion": "15.0",
        }],
        "appPermissions": {"entitlements": [], "privacy": {}},
    }],
    "news": [],
}
with open(out, "w", encoding="utf-8") as f:
    json.dump(source, f, ensure_ascii=False, indent=2)
print(f"source.json: {version} ({build}) -> {url}")
