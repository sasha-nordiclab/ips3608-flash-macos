#!/bin/sh
# Build standalone "IPS3608 Flasher.app" (universal: Apple Silicon + Intel) and a release zip.
set -e
cd "$(dirname "$0")/.."
OUT=dist
APP="$OUT/IPS3608 Flasher.app"
rm -rf "$OUT" build && mkdir -p "$OUT" build
clang -O2 -Wall -Wextra -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
  -o build/ips3608flash src/ips3608flash.c
osacompile -o "$APP" "app/IPS3608 Flasher.applescript"
cp build/ips3608flash "$APP/Contents/Resources/"
codesign --force --deep -s - "$APP"
(cd "$OUT" && ditto -c -k --keepParent "IPS3608 Flasher.app" "IPS3608-Flasher-macOS.zip")
echo "Built $APP and $OUT/IPS3608-Flasher-macOS.zip"
