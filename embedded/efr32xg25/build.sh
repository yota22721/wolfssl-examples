#!/usr/bin/env bash
# Generate and build the EFR32xG25 wolfCrypt test/benchmark project headlessly.
# Requires the Silicon Labs toolchain installed via SLT (see README.md).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BOARD="${BOARD:-brd4270b}"
BUILD_DIR="${BUILD_DIR:-$HERE/build}"

# wolfSSL sources are pulled in by relative path from wolfcrypt_test.slcp, which
# assumes a wolfssl checkout sitting beside wolfssl-examples. Point WOLFSSL_ROOT
# somewhere else and the project file is rewritten with absolute paths into the
# build directory, leaving the checked-in one untouched.
WOLFSSL_ROOT="${WOLFSSL_ROOT:-$HERE/../../../wolfssl}"
if [ ! -f "$WOLFSSL_ROOT/wolfcrypt/src/aes.c" ]; then
    echo "No wolfSSL checkout at '$WOLFSSL_ROOT'." >&2
    echo "Clone wolfSSL beside wolfssl-examples, or set WOLFSSL_ROOT." >&2
    exit 1
fi
WOLFSSL_ROOT="$(cd "$WOLFSSL_ROOT" && pwd -P)"

# Resolve the SLT-managed toolchain from its own registries rather than
# hardcoding conan hash directories, which change on reinstall.
SILABS_HOME="${SILABS_HOME:-$HOME/.silabs}"
SLT_HINT="Install the toolchain with 'slt install' (see README.md), or set SDK, SLC, GCC_DIR and JAVA_DIR explicitly."

# A missing registry, a truncated one, or a tool that was never installed all
# have the same fix, so say what it is instead of letting python traceback.
json_path() { # json_path <file> <key>
    local out
    if [ ! -r "$1" ]; then
        echo "Cannot read SLT registry '$1'." >&2
        echo "$SLT_HINT" >&2
        return 1
    fi
    out="$(python3 -c "import json,sys
try:
    d = json.load(open(sys.argv[1]))
except ValueError as e:
    sys.exit('%s: not valid JSON: %s' % (sys.argv[1], e))
try:
    print(d[sys.argv[2]][0]['path'] if isinstance(d, dict)
          else d[0]['extensions'][0]['path'])
except (KeyError, IndexError, TypeError):
    sys.exit('%s: no entry for %s' % (sys.argv[1], sys.argv[2]))" "$1" "$2")" || {
        echo "$SLT_HINT" >&2
        return 1
    }
    printf '%s\n' "$out"
}

SDK="${SDK:-$(json_path "$SILABS_HOME/sdks.json" simplicity-sdk)}"
SLC="${SLC:-$(json_path "$SILABS_HOME/tools.json" slc-cli)/slc}"
GCC_DIR="${GCC_DIR:-$(json_path "$SILABS_HOME/tools.json" gcc-arm-none-eabi)/bin}"
JAVA_DIR="${JAVA_DIR:-$(json_path "$SILABS_HOME/tools.json" java21)/bin}"

export PATH="$JAVA_DIR:$GCC_DIR:$PATH"
ARM_GCC_DIR="$(dirname "$GCC_DIR")"
export ARM_GCC_DIR

echo "SDK   : $SDK"
echo "slc   : $SLC"
echo "gcc   : $(arm-none-eabi-gcc -dumpversion)"
echo "board : $BOARD"

# BUILD_DIR is deleted below, so refuse anything that is not clearly ours.
# It must resolve beneath this example directory: that rules out "/", $HOME,
# the repository root and any unrelated project someone points the variable at.
BUILD_PARENT="$(cd "$(dirname "$BUILD_DIR")" 2>/dev/null && pwd -P || true)"
if [ -z "$BUILD_PARENT" ]; then
    echo "BUILD_DIR parent does not exist: $BUILD_DIR" >&2
    exit 1
fi
BUILD_ABS="$BUILD_PARENT/$(basename "$BUILD_DIR")"
case "$BUILD_ABS" in
    "$HERE"/?*) ;;
    *)
        echo "Refusing to delete '$BUILD_ABS'" >&2
        echo "BUILD_DIR must be inside $HERE" >&2
        exit 1
        ;;
esac

rm -rf "$BUILD_ABS"
mkdir -p "$BUILD_ABS"
BUILD_DIR="$BUILD_ABS"

# Point the project file at WOLFSSL_ROOT. slc resolves every path: entry
# relative to the .slcp itself, so a non-default root means rewriting that one
# prefix to a path relative to this directory - and the copy has to stay here,
# because app.c and user_settings.h are relative to it too.
SLCP="$HERE/wolfcrypt_test.slcp"
WOLFSSL_REL="$(realpath --relative-to="$HERE" "$WOLFSSL_ROOT")"
if [ "$WOLFSSL_REL" != "../../../wolfssl" ]; then
    SLCP="$HERE/.wolfcrypt_test.local.slcp"
    sed "s|\.\./\.\./\.\./wolfssl|$WOLFSSL_REL|g" \
        "$HERE/wolfcrypt_test.slcp" > "$SLCP"
    echo "wolfSSL: $WOLFSSL_ROOT"
fi

"$SLC" signature trust --sdk "$SDK" >/dev/null 2>&1 || true

"$SLC" generate \
    --sdk "$SDK" \
    --project-file "$SLCP" \
    --output-type makefile \
    --with "$BOARD" \
    --destination "$BUILD_DIR" \
    --new-project --force

# slc only emits include directories that live inside the project, so add the
# wolfSSL root here. Appending to the generated fragment keeps the SDK's own
# INCLUDES intact, which passing INCLUDES= on the make command line would not.
printf '\nINCLUDES += -I%s\n' "$WOLFSSL_ROOT" >> "$BUILD_DIR/wolfcrypt_test.project.mak"

make -C "$BUILD_DIR" -f wolfcrypt_test.Makefile -j"$(nproc)"

echo
echo "Artifacts:"
find "$BUILD_DIR" -name "*.hex" -o -name "*.s37" -o -name "*.bin" | sed 's/^/  /'
