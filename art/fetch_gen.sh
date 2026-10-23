#!/usr/bin/env bash
# Скачать сгенерированный рисунок постройки, сохранить исходник (1024 px, WebP без потерь) и сделать игровой спрайт.
# Использование: art/fetch_gen.sh <имя_спрайта> <url> <клеток>
set -euo pipefail
NAME="$1"; URL="$2"; SIZE="$3"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d)"
curl -sS -o "$TMP/raw.png" "$URL"
python3 -I -c "
from PIL import Image
im = Image.open('$TMP/raw.png').convert('RGB').resize((1024, 1024), Image.LANCZOS)
im.save('$ROOT/art/source/generated/$NAME.webp', lossless=True)
"
python3 -I "$ROOT/art/gen_sprite.py" "$ROOT/art/source/generated/$NAME.webp" "$NAME" "$SIZE" "$ROOT/art/out"
rm -rf "$TMP"
