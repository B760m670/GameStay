#!/usr/bin/env bash
# Веб-сборка Lanternfall (Emscripten, WebGL2) -> engine/build-web/
# Нужен активированный emsdk (source emsdk_env.sh). Ресурсы раскладываются на пакеты до 14 МБ:
# хостинг страниц ограничивает размер одного файла.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=build-web
rm -rf "$OUT" && mkdir -p "$OUT"
SRC=$(ls src/main.cc src/engine/*.cc src/client/*.cc src/game/*.cc src/platform/platform_common.cc src/platform/sokol_impl.cc src/platform/platform_web.cc)
em++ -std=c++17 -O2 -DSOKOL_GLES3 -Isrc -Ithird_party/sokol -Ithird_party/stb $SRC \
  -sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 \
  -sFORCE_FILESYSTEM=1 -lidbfs.js -sEXPORTED_RUNTIME_METHODS=FS,addRunDependency,removeRunDependency \
  -sENVIRONMENT=web --pre-js web/pre.js -o "$OUT/lanternfall.js"
# ресурсы: копия папки assets и список файлов с размерами (загружает web/pre.js)
cp -r assets "$OUT/assets"
python3 - "$OUT" <<'PY'
import json, os, sys
out = sys.argv[1]
files = []
for root, _, names in os.walk(os.path.join(out, "assets")):
    for n in sorted(names):
        p = os.path.join(root, n)
        files.append({"path": os.path.relpath(p, out), "size": os.path.getsize(p)})
json.dump(sorted(files, key=lambda f: f["path"]), open(os.path.join(out, "assets.json"), "w"))
print(len(files), "files", sum(f["size"] for f in files) // 1048576, "MB")
PY
cp web/index.html "$OUT/page.html"
ls -la "$OUT"
