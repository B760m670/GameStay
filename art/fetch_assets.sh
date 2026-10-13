#!/usr/bin/env bash
# Скачивает CC0-наборы Kenney, из которых рендерятся спрайты. Использование: art/fetch_assets.sh /opt/assets
set -euo pipefail
DEST="${1:-/opt/assets}"
mkdir -p "$DEST"
for u in \
  https://kenney.nl/media/pages/assets/castle-kit/a395102d20-1711543616/kenney_castle-kit.zip \
  https://kenney.nl/media/pages/assets/fantasy-town-kit/efe948d309-1754222374/kenney_fantasy-town-kit_2.0.zip \
  https://kenney.nl/media/pages/assets/tower-defense-kit/a402493eaa-1726471567/kenney_tower-defense-kit.zip \
  https://kenney.nl/media/pages/assets/nature-kit/37ac38a37b-1677698939/kenney_nature-kit.zip \
  https://kenney.nl/media/pages/assets/survival-kit/4065a8185b-1712149243/kenney_survival-kit.zip \
  https://kenney.nl/media/pages/assets/mini-characters/bfc7e272b4-1774770718/kenney_mini-characters.zip \
  https://kenney.nl/media/pages/assets/graveyard-kit/ba8d4b4517-1760691807/kenney_graveyard-kit_5.0.zip; do
  n=$(basename "$u" .zip)
  [ -d "$DEST/$n" ] && continue
  curl -sSL "$u" -o "$DEST/$n.zip"
  unzip -q -o "$DEST/$n.zip" -d "$DEST/$n"
  rm "$DEST/$n.zip"
  echo "$n"
done
