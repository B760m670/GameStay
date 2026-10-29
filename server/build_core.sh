#!/usr/bin/env bash
# Собирает ядро игры для сервера (без клиента и графики).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cmake -S "$ROOT/engine" -B "$ROOT/engine/build-server" -DLF_CLIENT=OFF -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$ROOT/engine/build-server" --target lf_server -j"$(nproc)"
