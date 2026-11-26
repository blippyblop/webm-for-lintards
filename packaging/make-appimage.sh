#!/usr/bin/env bash
#
# make-appimage.sh -- build a self-contained AppImage for WebM for Lazys
#
# The application ("WebM for Lazys", executable `webmconverter-qt`) is a Qt 6
# Widgets GUI. This script bundles the Qt libraries and the executable into a
# single relocatable AppImage using linuxdeploy + linuxdeploy-plugin-qt.
#
# USAGE
#   chmod +x packaging/make-appimage.sh      # once, to make it executable
#   ./packaging/make-appimage.sh
#
# BUILD FIRST (the script does *not* configure or compile anything):
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
#   cmake --build build -j"$(nproc)"
#
# PREREQUISITES
#   * cmake + Ninja/Make, a C++17 compiler
#   * Qt 6 development packages (Core, Gui, Widgets) and qmake6/QMAKE on PATH
#   * patchelf, file, and the usual binutils
#   * FUSE (libfuse2) if you want to *run* linuxdeploy's AppImages directly;
#     otherwise export APPIMAGE_EXTRACT_AND_RUN=1 (handled transparently below)
#   * curl (to fetch linuxdeploy on first run), network access
#
# OPTIONAL ENVIRONMENT OVERRIDES
#   BUILD_DIR=build            directory used for `cmake --install`
#   APPDIR=AppDir              AppDir staging directory (recreated)
#   OUTPUT_DIR=dist            where the final .AppImage is written
#   TOOLS_DIR=.cache/linuxdeploy   where linuxdeploy tools are cached
#   VERSION=1.2.3              override the version taken from `git describe`
#   QMAKE=/usr/lib/qt6/bin/qmake   hint for the Qt plugin
#
set -euo pipefail

# ---------------------------------------------------------------------------
# Paths and basic metadata
# ---------------------------------------------------------------------------
cd "$(dirname "$0")/.."

APP="webmconverter-qt"
APPNAME="WebM for Lazys"
ICON_NAME="io.github.webmforlazies.webmconverter"
ARCH="x86_64"

BUILD_DIR="${BUILD_DIR:-build}"
APPDIR="${APPDIR:-AppDir}"
OUTPUT_DIR="${OUTPUT_DIR:-dist}"
TOOLS_DIR="${TOOLS_DIR:-.cache/linuxdeploy}"

VERSION="${VERSION:-}"
if [[ -z "$VERSION" ]]; then
	if command -v git >/dev/null 2>&1 && git rev-parse --git-dir >/dev/null 2>&1; then
		VERSION="$(git describe --tags --always 2>/dev/null || true)"
	fi
	VERSION="${VERSION:-0.1.0}"
fi

LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
LINUXDEPLOY_QT_URL="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ARCH}.AppImage"

log()  { printf '[make-appimage] %s\n' "$*"; }
die()  { printf '[make-appimage] error: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Sanity checks
# ---------------------------------------------------------------------------
command -v curl >/dev/null 2>&1 || die "curl is required to download linuxdeploy"
command -v cmake >/dev/null 2>&1 || die "cmake is required"
[[ -d "$BUILD_DIR" ]] || die "build directory '$BUILD_DIR' not found -- configure and build the project first (see header)"
[[ -f packaging/webmconverter-qt.desktop ]] || die "packaging/webmconverter-qt.desktop is missing"
[[ -f "packaging/${ICON_NAME}.svg" ]] || die "packaging/${ICON_NAME}.svg is missing"

# AppImages need FUSE, or extraction when FUSE is unavailable (CI, containers).
if [[ -z "${APPIMAGE_EXTRACT_AND_RUN:-}" ]]; then
	export APPIMAGE_EXTRACT_AND_RUN=1
fi

# ---------------------------------------------------------------------------
# Fetch linuxdeploy + Qt plugin (cached, executable)
# ---------------------------------------------------------------------------
fetch_tool() {
	local url="$1" dest="$2"
	if [[ ! -x "$dest" ]]; then
		log "downloading $(basename "$dest")"
		curl -fL --retry 3 --connect-timeout 20 -o "$dest.part" "$url" \
			|| die "failed to download $url"
		mv -f "$dest.part" "$dest"
	fi
	chmod +x "$dest"
}

mkdir -p "$TOOLS_DIR"
if [[ "$TOOLS_DIR" = /* ]]; then
	TOOLS_ABS="$TOOLS_DIR"
else
	TOOLS_ABS="$PWD/$TOOLS_DIR"
fi

LINUXDEPLOY="$TOOLS_ABS/linuxdeploy-${ARCH}.AppImage"
LINUXDEPLOY_QT="$TOOLS_ABS/linuxdeploy-plugin-qt-${ARCH}.AppImage"
fetch_tool "$LINUXDEPLOY_URL" "$LINUXDEPLOY"
fetch_tool "$LINUXDEPLOY_QT_URL" "$LINUXDEPLOY_QT"

# linuxdeploy discovers plugins via PATH; make sure ours is found first.
export PATH="$TOOLS_ABS:$PATH"
export ARCH

# Optional qmake hint for the Qt plugin.
if [[ -n "${QMAKE:-}" ]]; then
	export QMAKE
	log "using QMAKE=$QMAKE"
fi

# ---------------------------------------------------------------------------
# Stage the AppDir
# ---------------------------------------------------------------------------
log "recreating $APPDIR"
rm -rf -- "$APPDIR"
mkdir -p "$APPDIR/usr/bin" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/scalable/apps"

log "installing project into $APPDIR/usr"
cmake --install "$BUILD_DIR" --prefix "$APPDIR/usr"

[[ -x "$APPDIR/usr/bin/$APP" ]] || log "warning: $APPDIR/usr/bin/$APP not found after install"

# Desktop entry: AppDir root (required by linuxdeploy) + system location.
install -m 0644 packaging/webmconverter-qt.desktop "$APPDIR/$APP.desktop"
install -m 0644 packaging/webmconverter-qt.desktop "$APPDIR/usr/share/applications/$APP.desktop"

# Icon: AppDir root (picked up by linuxdeploy, name from the Icon= key) + theme dir.
install -m 0644 "packaging/${ICON_NAME}.svg" "$APPDIR/${ICON_NAME}.svg"
install -m 0644 "packaging/${ICON_NAME}.svg" "$APPDIR/usr/share/icons/hicolor/scalable/apps/${ICON_NAME}.svg"

# ---------------------------------------------------------------------------
# Build the AppImage
# ---------------------------------------------------------------------------
mkdir -p "$OUTPUT_DIR"
OUTPUT_ABS="$(cd "$OUTPUT_DIR" && pwd)"
OUT_FILE="$OUTPUT_ABS/${APP}-${VERSION}-${ARCH}.AppImage"
rm -f -- "$OUT_FILE"
export OUTPUT="$OUT_FILE"

log "running linuxdeploy (Qt plugin) for ${APPNAME} ${VERSION}"
"$LINUXDEPLOY" \
	--appdir "$APPDIR" \
	--desktop-file "$APPDIR/$APP.desktop" \
	--icon-file "$APPDIR/${ICON_NAME}.svg" \
	--plugin qt \
	--output appimage

[[ -f "$OUT_FILE" ]] || die "linuxdeploy did not produce $OUT_FILE"

log "done: $OUT_FILE"
printf '%s\n' "$OUT_FILE"
