#!/usr/bin/env bash
# Compile-check (syntax + codegen, no link) every .cpp in src/ against Qt6 headers.
# We can't link here because the only Qt tree available is a different CPU arch,
# but a successful -c compile catches the overwhelming majority of mistakes.
#
# Usage:
#   QTINC=/path/to/qt6/include ZIG=/path/to/zig scripts/check_qt_compile.sh
#   (ZIG defaults to `zig` on PATH; QTINC is required)
set -uo pipefail
cd "$(dirname "$0")/.."

QTINC="${QTINC:?Set QTINC to the qt6 include dir (contains QtCore, QtGui, QtWidgets)}"
ZIG="${ZIG:-zig}"
CXXFLAGS=(
  -std=c++17 -fPIC
  -DQT_WIDGETS_LIB -DQT_GUI_LIB -DQT_CORE_LIB -DQT_NO_DEBUG
  -I src
  -I"$QTINC" -I"$QTINC/QtCore" -I"$QTINC/QtGui"
  -I"$QTINC/QtWidgets" -I"$QTINC/QtMultimedia"
)

mkdir -p build/check
fail=0
count=0
while IFS= read -r f; do
  count=$((count+1))
  obj="build/check/$(echo "$f" | tr '/' '_').o"
  if ! out=$("$ZIG" c++ "${CXXFLAGS[@]}" -c "$f" -o "$obj" 2>&1); then
    echo "FAIL  $f"
    echo "$out" | head -40
    fail=1
  else
    echo "ok    $f"
  fi
done < <(find src -name '*.cpp' | sort)

echo "------------------------------------------------------------"
if [ "$fail" -eq 0 ]; then echo "ALL $count TU(s) COMPILED OK"; else echo "COMPILE FAILURES PRESENT"; fi
exit "$fail"
