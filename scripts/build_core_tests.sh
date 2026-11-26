#!/usr/bin/env bash
# Build & run the Qt-free core unit tests with any C++17 compiler.
# Usage: scripts/build_core_tests.sh [CXX]
#   default CXX = zig c++ (portable, no root) if available, else g++/clang++.
set -euo pipefail
cd "$(dirname "$0")/.."

CXX="${1:-}"
if [ -z "$CXX" ]; then
  if command -v zig >/dev/null 2>&1; then CXX="zig c++";
  elif command -v g++ >/dev/null 2>&1; then CXX="g++";
  elif command -v clang++ >/dev/null 2>&1; then CXX="clang++";
  else echo "No C++ compiler found (need zig, g++, or clang++)" >&2; exit 1; fi
fi

mkdir -p build
echo "== compiling core tests with: $CXX"
$CXX -std=c++17 -O2 -Wall -Wextra \
  -I src \
  src/core/easing.cpp \
  src/core/sizecalc.cpp \
  src/core/progressparser.cpp \
  src/core/commandbuilder.cpp \
  tests/core_tests.cpp \
  -o build/core_tests

echo "== running core tests"
./build/core_tests
