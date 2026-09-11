#!/usr/bin/env bash
# Flash the built image to an attached EFR32xG25 kit and report the device.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$HERE/build}"
SILABS_HOME="${SILABS_HOME:-$HOME/.silabs}"

# Resolve Commander from the SLT registry. The path goes in as an argument
# rather than being pasted into the program text, so a directory holding a
# quote is read as a path and nothing else.
TOOLS_JSON="$SILABS_HOME/tools.json"
if [ -z "${COMMANDER:-}" ]; then
    if [ ! -r "$TOOLS_JSON" ]; then
        echo "Cannot read SLT registry '$TOOLS_JSON'." >&2
        echo "Install the toolchain with 'slt install' (see README.md), or set COMMANDER explicitly." >&2
        exit 1
    fi
    COMMANDER_DIR="$(python3 -c "import json,sys
try:
    d = json.load(open(sys.argv[1]))
except ValueError as e:
    sys.exit('%s: not valid JSON: %s' % (sys.argv[1], e))
try:
    print(d['commander'][0]['path'])
except (KeyError, IndexError, TypeError):
    sys.exit('%s: no entry for commander' % (sys.argv[1],))" "$TOOLS_JSON")" || {
        echo "Install the toolchain with 'slt install' (see README.md), or set COMMANDER explicitly." >&2
        exit 1
    }
    COMMANDER="$COMMANDER_DIR/commander"
fi

# find would end the script under set -e if the directory is not there, before
# the message below could explain why.
if [ -z "${IMAGE:-}" ]; then
    if [ ! -d "$BUILD_DIR" ]; then
        echo "No build directory at $BUILD_DIR - run build.sh first." >&2
        exit 1
    fi
    IMAGE="$(find "$BUILD_DIR" -name '*.hex' | head -1)"
fi
if [ -z "$IMAGE" ]; then
    echo "No .hex found under $BUILD_DIR - run build.sh first." >&2
    exit 1
fi

echo "== device =="
"$COMMANDER" device info

echo
echo "== flashing $IMAGE =="
"$COMMANDER" flash "$IMAGE"

echo
echo "== reset =="
"$COMMANDER" device reset
