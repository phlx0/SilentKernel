#!/usr/bin/env bash
# build.sh — build SilentKernel.iso
# Requires: i686-elf cross-compiler, nasm, grub-mkrescue, xorriso

set -e

# ── Dependency check ────────────────────────────────────────────────
check() {
    command -v "$1" >/dev/null 2>&1 || {
        echo "ERROR: '$1' not found. See README.md for setup instructions."
        exit 1
    }
}

check i686-elf-gcc
check i686-elf-ld
check nasm
check i686-elf-grub-mkrescue
check xorriso

echo "==> All tools found"
make clean
make all
echo ""
echo "==> Build complete! Drop SilentKernel.iso into your VM."
