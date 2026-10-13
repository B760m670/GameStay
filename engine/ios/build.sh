#!/usr/bin/env bash
# Сборка Lanternfall для iOS без Xcode-проекта: clang + iOS SDK -> .app -> неподписанный .ipa.
# Запускать на macOS: engine/ios/build.sh <version> <build-number> <out.ipa>
set -euo pipefail
VERSION="${1:-0.2.0}"
BUILD="${2:-1}"
OUT="${3:-Lanternfall.ipa}"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
WORK="$(mktemp -d)"
APP="$WORK/Payload/Lanternfall.app"
mkdir -p "$APP" "$WORK/obj"

SDK="$(xcrun --sdk iphoneos --show-sdk-path)"
CC="$(xcrun --sdk iphoneos --find clang)"
CXX="$(xcrun --sdk iphoneos --find clang++)"
COMMON=(-arch arm64 -isysroot "$SDK" -miphoneos-version-min=16.0 -Os -DNDEBUG -DSOKOL_METAL
        -I"$ROOT/src" -I"$ROOT/third_party/sokol" -I"$ROOT/third_party/stb" -fvisibility=hidden -ffunction-sections -fdata-sections)

objs=()
compile() {  # file lang
  local src="$1" lang="$2" obj="$WORK/obj/$(echo "$1" | tr '/' '_').o"
  case "$lang" in
    cxx)   "$CXX" "${COMMON[@]}" -std=c++17 -c "$src" -o "$obj" ;;
    objcxx) "$CXX" "${COMMON[@]}" -std=c++17 -x objective-c++ -fobjc-arc -c "$src" -o "$obj" ;;
  esac
  objs+=("$obj")
}

for f in "$ROOT"/src/game/*.cc "$ROOT"/src/engine/*.cc "$ROOT"/src/client/*.cc "$ROOT"/src/main.cc "$ROOT"/src/platform/platform_common.cc; do
  compile "$f" cxx
done
compile "$ROOT/src/platform/sokol_impl.cc" objcxx
compile "$ROOT/src/platform/platform_ios.mm" objcxx

"$CXX" "${COMMON[@]}" -o "$APP/Lanternfall" "${objs[@]}" -Wl,-dead_strip \
  -framework Foundation -framework UIKit -framework Metal -framework MetalKit -framework QuartzCore -framework CoreGraphics \
  -framework AudioToolbox -framework AVFoundation
xcrun strip -x "$APP/Lanternfall" || true

# ресурсы
mkdir -p "$APP/assets"
cp -R "$ROOT/assets/atlas" "$ROOT/assets/fonts" "$APP/assets/"
rm -f "$APP/assets/fonts/"*.txt
sed -e "s/@VERSION@/$VERSION/" -e "s/@BUILD@/$BUILD/" "$HERE/Info.plist.in" > "$APP/Info.plist"
plutil -convert binary1 "$APP/Info.plist"
ICON="$ROOT/../distribution/icon.png"
sips -z 120 120 "$ICON" --out "$APP/AppIcon60x60@2x.png" >/dev/null
sips -z 180 180 "$ICON" --out "$APP/AppIcon60x60@3x.png" >/dev/null
sips -z 152 152 "$ICON" --out "$APP/AppIcon76x76@2x~ipad.png" >/dev/null

# ad-hoc подпись (LiveContainer всё равно переподписывает)
codesign --force --sign - --timestamp=none "$APP" || true

(cd "$WORK" && zip -qry ipa.zip Payload)
mv "$WORK/ipa.zip" "$OUT"
echo "binary: $(stat -f%z "$APP/Lanternfall") bytes"
echo "ipa:    $(stat -f%z "$OUT") bytes -> $OUT"
