#!/usr/bin/env bash
# Build the single-file eui.exe with MinGW-w64 gcc (MSYS2 / Git Bash).
set -euo pipefail
cd "$(dirname "$0")"

if ! command -v gcc >/dev/null 2>&1; then
  export PATH="/c/msys64/ucrt64/bin:/c/msys64/mingw64/bin:$PATH"
fi
if ! command -v gcc >/dev/null 2>&1; then
  echo "[error] gcc not found. Install MSYS2 (ucrt64 or mingw64) and retry." >&2
  exit 1
fi

mkdir -p build

CFLAGS=(-std=c11 -Os -Wall -Wextra -Ivendor/zstd -DZSTD_DISABLE_ASM -DZSTD_LEGACY_SUPPORT=0 \
        -DZSTD_STRIP_ERROR_STRINGS)
LDFLAGS=(-static -s -ffunction-sections -fdata-sections -Wl,--gc-sections -lshell32)

echo "[build] gcc src/*.c vendor/zstd/*.c -> build/eui.exe"
gcc "${CFLAGS[@]}" src/*.c vendor/zstd/common/*.c vendor/zstd/decompress/*.c \
    -o build/eui.exe "${LDFLAGS[@]}"

echo "[done] build/eui.exe ($(stat -c %s build/eui.exe 2>/dev/null || echo '?') bytes)"
