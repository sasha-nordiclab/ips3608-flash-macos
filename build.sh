#!/bin/sh
# Build the universal (Apple Silicon + Intel) ips3608flash binary.
set -e
cd "$(dirname "$0")"
mkdir -p build
clang -O2 -Wall -Wextra -arch arm64 -arch x86_64 -mmacosx-version-min=11.0 \
  -o build/ips3608flash src/ips3608flash.c
echo "Built build/ips3608flash"
