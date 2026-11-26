# WebM for Lintards

**WebM for Lintards** is a Linux-first, cross-platform desktop front-end for `ffmpeg`.
It wraps the fiddly parts of video conversion — trimming, cropping, resizing,
two-pass encoding, subtitle burn-in, downloads — in a plain Qt 6 Widgets UI so
you can go from a file (or a URL) to a finished WebM/MP4 in a handful of clicks.

It is a C++17 port of the Windows-only C#/WinForms tool
[`argorar/WebMConverter`](https://github.com/argorar/WebMConverter). The port
drops AviSynth entirely: every edit is expressed as a pure `ffmpeg` filter
graph, which means no plug-ins to install and identical output on Linux, Windows
and macOS.

> **Independent port.** This is not an official release of the original project.
> See [License & credits](#license--credits).

---

## Just give me the exe, nerd

This is the nerd version of the tool, sorry. You could try heading to releases and grabbing/running the windows version but it requires ffmpeg and yt-dlp installed which is easier on linux.

## Usage:

Install ffmpeg and yt-dlp and make them available on PATH. 

Run the appimage.

Done.

---

## Status

| Area | State |
| --- | --- |
| Qt-free core (`FilterModel`, `VideoInfo`, `CommandBuilder`, easing, size calc, progress parser) | **Implemented**, 57 unit checks passing |
| Qt I/O adapters (`MediaProbe`, `Encoder`, `YtdlpRunner`) | **Implemented**, compile-checked against Qt 6.8 |
| Filter dialogs (16 of them, see `src/ui/dialogs/`) | **Implemented**, compile-checked |
| Preview engine, `PreviewCanvas`, `TimelineWidget`, panels, main window shell | **Implemented**, compile-checked |

All 32 translation units under `src/` compile cleanly against Qt 6.8 headers
(`scripts/check_qt_compile.sh`), and the Qt-free core's 57 unit checks pass
(`scripts/build_core_tests.sh`). A native link/run and the AppImage are produced
by CI (`.github/workflows/build.yml`), since the development sandbox used to
author this port only had headers for a different CPU architecture.

Concretely:

* `CommandBuilder` covers WebM (VP8/VP9) and MP4 (H.264/H.265), constant and
  variable rate control, two-pass, target filesize, trim/multi-trim, crop,
  dynamic crop, resize, rotate, rate change, dynamic speed, captions, subtitle
  burn-in, overlay, fade, EQ, denoise, deinterlace, audio delay, loop, reverse and
  the multi-input grid path.
* `Encoder` runs the resulting pass plans sequentially through `QProcess` and
  emits parseable progress.
* Every `.cpp` under `src/` compiles cleanly with
  `scripts/check_qt_compile.sh` (32 translation units).

Every filter listed above is emitted by `CommandBuilder` today. Stabilisation in
particular produces a dedicated analysis pass (`vidstabdetect` → `transforms.trf`)
that is inserted ahead of the encoding pass(es), which the unit tests assert.

---

## Features

**Encoding**

* WebM: VP8 / VP9 video, Opus / Vorbis audio
* MP4: H.264 / H.265 video, AAC audio
* Constant bitrate or variable quality (CRF)
* Two-pass encoding
* Target filesize ("make it fit") with automatic bitrate planning
* HQ mode (`-lag-in-frames 16 -auto-alt-ref 1`), alpha (`yuva420p`), HDR (10-bit / BT.2020)

**Editing**

* Trim, and multi-trim (several ranges concatenated in one pass)
* Crop, and **dynamic crop** — keyframed tracking of a moving region
* Dynamic speed — keyframed speed ramps with easing
* Resize (bicubic/bilinear/nearest/lanczos), rotate 90/180/270, change frame rate
* Captions (`drawtext`), subtitle burn-in, image overlay
* Fade in/out, EQ (gamma/saturation/contrast), denoise, deinterlace
* Audio: delay, loudness normalisation, amplify, mono
* Loop, reverse
* Stabilisation (vid.stab) — detect pass + `vidstabtransform` encode pass
* Grid — side-by-side or stacked composition of two files (`hstack` / `vstack`)
* Merge/convert several files in a batch, drag & drop input

**Downloading**

* `yt-dlp` integration, including the `URL@*start-end` clip trick (see below)

**Not carried over from the original** (deliberately): Gfycat/ShareX upload
(services dead / Windows-only), the self-updater (deferred), AviSynth and FFMS2
(not needed), and bundled binaries (everything comes from `PATH`).

---

## Screenshots

_Placeholders — to be captured once the UI shell lands._

| Main window | Crop & dynamic crop | Timeline |
| --- | --- | --- |
| `docs/screenshots/main-window.png` | `docs/screenshots/crop.png` | `docs/screenshots/timeline.png` |

---

## Requirements

**Runtime**

* `ffmpeg` and `ffprobe` on `PATH`
* `yt-dlp` on `PATH` (only for the Download tab)

Nothing is bundled. Install them from your distribution, Homebrew, winget or the
official static builds.

**Build**

* CMake ≥ 3.21
* A C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
* Qt 6.5 or newer — `Core`, `Gui`, `Widgets` only (no QML, no `QtMultimedia`
  requirement)

Debian/Ubuntu:

```bash
sudo apt install build-essential cmake qt6-base-dev ffmpeg yt-dlp
```

Fedora:

```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel ffmpeg yt-dlp
```

Arch:

```bash
sudo pacman -S base-devel cmake qt6-base ffmpeg yt-dlp
```

---

## Build

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/webmforlazies
```

If Qt is installed in a non-standard prefix, point CMake at it:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/Qt/6.7.0/gcc_64"
```

> The top-level `CMakeLists.txt` that defines the Qt application target lands
> together with the UI shell. Source snapshots that predate it can still build and
> test the core with the scripts in [`Run the core tests`](#run-the-core-tests).

### Windows

Use the "Qt 6.x MSVC 64-bit" kit from the Qt Online Installer, then:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_PREFIX_PATH=C:\Qt\6.7.0\msvc2019_64
cmake --build build --config Release
```

Deploy the Qt DLLs next to the executable with `windeployqt`:

```powershell
C:\Qt\6.7.0\msvc2019_64\bin\windeployqt.exe build\Release\webmforlazies.exe
```

Put `ffmpeg.exe`, `ffprobe.exe` and `yt-dlp.exe` on `PATH` (or in the same
directory as the binary, if you launch it from there).

### macOS

```bash
brew install qt cmake ffmpeg yt-dlp
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j"$(sysctl -n hw.ncpu)"
```

For a redistributable bundle, run `macdeployqt` on the generated `.app`. Note
that Apple Silicon and Intel builds must be built separately (universal binaries
need a universal Qt).

---

## Run the core tests

The core library is Qt-free, so it can be built and tested with nothing but a
C++17 compiler:

```bash
scripts/build_core_tests.sh          # picks zig c++ / g++ / clang++ automatically
scripts/build_core_tests.sh g++      # or force a compiler
```

Expected tail:

```
57 checks, 0 failures
```

To syntax/codegen-check every Qt translation unit without linking (useful on
machines where the Qt tree is for a different CPU architecture):

```bash
QTINC=/path/to/qt6/include ZIG=zig scripts/check_qt_compile.sh
# -> ALL 23 TU(s) COMPILED OK
```

---

## Usage tips

* **Drop a file anywhere** on the window to load it; drop several to fill the
  batch/merge list.
* **Trim** by typing timecodes; **multi-trim** takes a list of ranges and
  concatenates them in a single ffmpeg run.
* **Dynamic crop / dynamic speed** are keyframed: set at least two keyframes and
  the core emits interpolated ffmpeg expressions (`crop=x='...'`,
  `setpts='PTS/(...)'`).
* **Target filesize** overrides the bitrate fields: enter e.g. `8M` and the
  builder solves video bitrate = (target × 0.98 − audio) for the clip duration,
  and adds `-fs` as a hard cap.
* **Two-pass** is worth it for VP9 at low bitrates; it writes `<output>.2pass`
  files next to the output and cleans up after pass 2 (delete them manually if a
  run is cancelled).
* **Grid** is a special path: it takes two inputs and ignores most other filters.
* Subtitle burn-in needs a path `ffmpeg`'s `subtitles` filter can open; on
  Windows, prefer forward slashes in the path.

### Downloading a clip with `yt-dlp`

`YtdlpRunner` understands the original tool's shorthand — append
`@*start-end` to the URL to download only that section:

```
https://www.youtube.com/watch?v=dQw4w9WgXcQ@*1:30-1:45
```

That is passed to `yt-dlp` as a download range, so you get just the 15-second
excerpt instead of the whole video.

---

## Keyboard shortcuts

### Crop dialog

| Keys | Action |
| --- | --- |
| Arrow keys | Move the crop rectangle by 1 px |
| Shift + arrows | Move fast (10 px) |
| Alt + arrows | Resize the crop rectangle |
| Ctrl + arrows | Resize while keeping the aspect ratio |

### Timeline

| Input | Action |
| --- | --- |
| Mouse wheel | Step one frame forward/back |
| Shift + wheel | Step one second |
| Click / drag | Scrub; the frame preview follows |

---

## Architecture

The code is split into three layers with a strict rule: **the UI never builds
command lines, and the core never touches widgets.** The shared currency is
`FilterModel` + `VideoInfo` + `QImage`.

```
src/core/     Qt-free models and the ffmpeg argv compiler (unit-tested)
src/ui/       Qt Widgets main window, panels, custom widgets and dialogs
src/preview/  frame extraction + LRU cache, PreviewEngine
```

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the data flow, the exact
filter ordering, the two-pass/size-targeting maths and the preview strategy.

---

## AI Disclaimer

Entirely slop ported. By deepseek4.1-flash doing the coding and I used GLM5.4 on the recon/planning. 

---


## License & credits

This project is released under the **MIT License** — see [`LICENSE`](LICENSE) and
[`LICENSES.md`](LICENSES.md) for third-party components.

It is an independent port inspired by
[`argorar/WebMConverter`](https://github.com/argorar/WebMConverter) (MIT), which
in turn is a GUI around `ffmpeg`. The original's filter model, argument
templates and UI workflow informed this port; the C++ implementation, the
AviSynth-free filter graphs and the Qt UI are new work. Not affiliated with or
endorsed by the original author.
