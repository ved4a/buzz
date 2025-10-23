#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BUILD_DIR="$ROOT_DIR/backend/build"
DIST_DIR="$ROOT_DIR/dist"

echo "[pack] Preparing dist at $DIST_DIR"
mkdir -p "$DIST_DIR"

if [[ ! -x "$BUILD_DIR/buzz" ]]; then
  echo "[pack] backend/build/buzz not found. Building..."
  (cd "$ROOT_DIR/backend" && cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j)
fi

echo "[pack] Copying binary"
cp -f "$BUILD_DIR/buzz" "$DIST_DIR/buzz"

chmod +x "$DIST_DIR/buzz" "$DIST_DIR/run.sh"

echo "[pack] Done. You can double-click 'buzz.desktop' or run './dist/run.sh'"
