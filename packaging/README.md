# Packaging

Linux packaging assets for **WebM for Lazys** (`webmconverter-qt`), a Qt 6
(Core/Gui/Widgets) GUI that converts videos to WebM/MP4 with ffmpeg.

| File | Purpose |
| --- | --- |
| `webmconverter-qt.desktop` | freedesktop desktop entry (installed to `share/applications`) |
| `io.github.webmforlazies.webmconverter.svg` | 256×256 scalable app icon (installed to the hicolor theme) |
| `make-appimage.sh` | builds a self-contained AppImage with linuxdeploy + linuxdeploy-plugin-qt |

## Build an AppImage

The script only *installs* an existing build — configure and compile first:

```bash
cd webmconverter-qt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j"$(nproc)"

chmod +x packaging/make-appimage.sh
./packaging/make-appimage.sh
```

The result is written to `dist/webmconverter-qt-<version>-x86_64.AppImage`
(version comes from `git describe --tags --always`, falling back to `0.1.0`).
Overridable via environment: `BUILD_DIR`, `APPDIR`, `OUTPUT_DIR`,
`TOOLS_DIR`, `VERSION`, `QMAKE`.

linuxdeploy and its Qt plugin are downloaded on first run into
`.cache/linuxdeploy/` and reused afterwards.

## Prerequisites

- `cmake` (≥ 3.16), a C++17 compiler, Ninja or Make
- Qt 6 development packages — `Core`, `Gui`, `Widgets` — with `qmake6` on `PATH`
  (Debian/Ubuntu: `qt6-base-dev`; Fedora: `qt6-qtbase-devel`)
- `patchelf`, `file`, `binutils`
- `curl` for the linuxdeploy download
- FUSE (`libfuse2` on Debian/Ubuntu) to run linuxdeploy's AppImages, **or**
  `APPIMAGE_EXTRACT_AND_RUN=1` when FUSE is unavailable (containers/CI).
  The script sets this automatically when unset.

## Runtime dependencies (not bundled)

The AppImage ships Qt and the GUI itself, but relies on tools from the host
`PATH`:

- `ffmpeg` and `ffprobe` — encoding, probing, and size estimation
- `yt-dlp` — optional, for downloading source videos from URLs

Install them with your distribution's package manager; the app reports missing
tools instead of bundling them.

## Notes

- The desktop entry uses `Icon=io.github.webmforlazies.webmconverter`, matching
  the SVG file name, so theme-based installs resolve the icon correctly.
- Licensed under the MIT License (see `../LICENSE`).
