#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_DIR="$ROOT_DIR/third_party/esp32-llm/data"

stories="$MODEL_DIR/stories260K.bin"
tok="$MODEL_DIR/tok512.bin"

if [[ ! -f "$stories" || ! -f "$tok" ]]; then
  echo "Missing esp32-llm assets in: $MODEL_DIR"
  exit 1
fi

echo "esp32-llm assets found:"
ls -lh "$stories" "$tok"
