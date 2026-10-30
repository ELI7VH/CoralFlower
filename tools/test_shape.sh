#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Run after tools/build.py --gen-only (Pillow + fontTools) or a firmware build.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host
CC=${CC:-cc}
"$CC" -O1 -w -Ifirmware/src -Itests tests/arp_shape_test.c -o build/host/arp_shape_test
build/host/arp_shape_test
for name in cf_backup_test cf_shape_test; do
    "$CC" -std=gnu11 -O1 -w -Itools/emu/web/compat -Ibuild/gen -Ifirmware/src -Itests "tests/$name.c" -lm -o "build/host/$name"
    "build/host/$name"
done
