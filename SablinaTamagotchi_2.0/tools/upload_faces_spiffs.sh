#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR=$(cd "$SCRIPT_DIR/.." && pwd)
PARTITIONS_FILE="$PROJECT_DIR/partitions.csv"
DATA_DIR="$PROJECT_DIR/data"
BUILD_DIR="$PROJECT_DIR/.tmp_spiffs"
IMAGE_PATH="$BUILD_DIR/sablina-faces.spiffs.bin"
find_latest_tool() {
  local base_dir="$1"
  shift
  local names=("$@")
  local results=()
  local name
  for name in "${names[@]}"; do
    while IFS= read -r match; do
      results+=("$match")
    done < <(find "$base_dir" -mindepth 2 -maxdepth 2 -type f -name "$name" 2>/dev/null)
  done
  if [[ ${#results[@]} -eq 0 ]]; then
    return 1
  fi
  printf '%s\n' "${results[@]}" | sort -V | tail -n 1
}

MK_SPIFFS=${MK_SPIFFS:-$(find_latest_tool /home/nexland/.arduino15/packages/esp32/tools/mkspiffs mkspiffs)}
ESPTOOL_PY=${ESPTOOL_PY:-$(find_latest_tool /home/nexland/.arduino15/packages/esp32/tools/esptool_py esptool.py esptool)}
PORT=${PORT:-/dev/ttyACM0}
BAUD=${BAUD:-921600}
BUILD_ONLY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-only)
      BUILD_ONLY=1
      shift
      ;;
    --port)
      PORT="$2"
      shift 2
      ;;
    --baud)
      BAUD="$2"
      shift 2
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ ! -f "$PARTITIONS_FILE" ]]; then
  echo "Missing partitions file: $PARTITIONS_FILE" >&2
  exit 1
fi

if [[ ! -d "$DATA_DIR/faces" ]]; then
  echo "Missing face assets in $DATA_DIR/faces" >&2
  exit 1
fi

if [[ ! -x "$MK_SPIFFS" ]]; then
  echo "mkspiffs not found: $MK_SPIFFS" >&2
  exit 1
fi

if [[ ! -f "$ESPTOOL_PY" ]]; then
  echo "esptool.py not found: $ESPTOOL_PY" >&2
  exit 1
fi

SPIFFS_OFFSET=$(awk -F',' '/^spiffs/ {gsub(/ /, "", $4); print $4}' "$PARTITIONS_FILE")
SPIFFS_SIZE=$(awk -F',' '/^spiffs/ {gsub(/ /, "", $5); print $5}' "$PARTITIONS_FILE")

if [[ -z "$SPIFFS_OFFSET" || -z "$SPIFFS_SIZE" ]]; then
  echo "Could not parse SPIFFS offset/size from $PARTITIONS_FILE" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"

"$MK_SPIFFS" -c "$DATA_DIR" -b 4096 -p 256 -s "$SPIFFS_SIZE" "$IMAGE_PATH"
echo "Built SPIFFS image: $IMAGE_PATH"

if [[ "$BUILD_ONLY" -eq 1 ]]; then
  exit 0
fi

if [[ -x "$ESPTOOL_PY" ]]; then
  "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" --baud "$BAUD" write-flash "$SPIFFS_OFFSET" "$IMAGE_PATH"
else
  python3 "$ESPTOOL_PY" --chip esp32s3 --port "$PORT" --baud "$BAUD" write-flash "$SPIFFS_OFFSET" "$IMAGE_PATH"
fi