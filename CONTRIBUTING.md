# Contributing to WebM for Lintards

Thanks for wanting to help. This is a small, deliberately unglamorous project:
the goal is **feature parity with the original Windows tool on Linux, Windows and
macOS**, not a pretty UI. Plain, readable, well-tested code beats clever code.

---

## Getting set up

### Prerequisites

* CMake ≥ 3.21
* C++17 compiler (GCC 9+, Clang 10+, MSVC 2019+)
* Qt 6.5+ — `Core`, `Gui`, `Widgets`
* `ffmpeg` + `ffprobe` on `PATH` for manual testing (and `yt-dlp` for the
  download tab)

Debian/Ubuntu:

```bash
sudo apt install build-essential cmake qt6-base-dev ffmpeg yt-dlp
```

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
./build/webmforlazies
```

### Tests

The core is Qt-free and builds in a second or two:

```bash
scripts/build_core_tests.sh          # zig c++ / g++ / clang++, whichever exists
scripts/build_core_tests.sh clang++  # or name a compiler
```

Expect `53 checks, 0 failures`. There is also a compile-only pass over every Qt
translation unit:

```bash
QTINC=/path/to/qt6/include scripts/check_qt_compile.sh
```

Run both before opening a pull request. CI (once it exists) runs exactly these
two commands.

---

## Project rules

These three rules are what keep the codebase testable; PRs that break them will
be asked to change.

1. **`src/core/` stays Qt-free.** No `#include <QString>`, no `QObject`, no
   widgets in `filtermodel.h`, `videoinfo.h`, `easing.*`, `sizecalc.*`,
   `progressparser.*` or `commandbuilder.*`. That is why the 53 core tests can
   run without a Qt toolchain. `QString` ↔ `std::string` conversion happens in
   the adapters (`mediaprobe.cpp`, `encoder.cpp`), at the boundary.
2. **The UI never builds command lines.** Dialogs write into `FilterModel` /
   `EncodingOptions`; `CommandBuilder` turns those into `argv`. Never assemble a
   filter string in a dialog, even "just for a preview".
3. **The core returns argv, not a shell string.** `PassPlan::args` is a
   `std::vector<std::string>` passed straight to `QProcess::start(program,
   arguments)` — no quoting, no `system()`, no shell. Keep it that way; it is
   what makes Windows path handling sane.

Also: the UI is intentionally plain Qt Widgets ("Qt Lite"). Do not introduce
QML, animations or custom theming. A dated look is acceptable; a missing feature
is not.

---

## Code style

* **Language**: C++17. No exceptions for control flow, no RTTI tricks, no
  `std::filesystem` in the core (paths arrive as strings; the adapters use
  `QFileInfo`).
* **Formatting**: 4 spaces, no tabs, ~100 column limit, K&R braces. Match the
  surrounding file; don't reformat untouched code in a feature PR.
* **Naming**: `snake_case`-free — the codebase uses `camelCase` for functions and
  locals, `PascalCase` for types, trailing `_` for private members
  (`ffmpegPath_`, `duration_`). Macros are `SCREAMING_SNAKE_CASE` and rare.
* **Headers**: `#pragma once`. Keep headers light — forward-declare Qt classes
  in dialog headers, put `ProgressParser` behind a pimpl-ish holder in
  `Encoder` rather than including it.
* **Namespaces**: everything in `namespace wc { ... }`.
* **Comments**: explain *why*, not *what*. Where behaviour is inherited from the
  original C# tool, say so — a lot of the argument templates look arbitrary
  until you know they came from `MainForm.cs`.
* **No new dependencies** without discussing it in an issue first. Every
  dependency has to be justified in `LICENSES.md`.

---

## Adding or changing a filter

The pattern is fixed; follow it and the change is easy to review.

1. **Model** — add the fields to the relevant struct in
   `src/core/filtermodel.h` (most already exist; keep flags `enabled` +
   parameters, so "off" is representable).
2. **Builder** — emit the ffmpeg syntax in `CommandBuilder::videoFilterGraph()`
   (or `audioFilterArgs()`), at the correct position in the chain (see the order
   table in `docs/ARCHITECTURE.md`), and update `hasVideoFilters()` so the
   no-filter fast path stays correct.
3. **Test** — extend `tests/core_tests.cpp` with a case that builds a
   `CommandBuilder` for the new filter and asserts on the argv/filter graph.
   Command-builder changes without a test will not be merged.
4. **Dialog** — add or extend a dialog under `src/ui/dialogs/`, following the
   existing ones (`apply to model on accept`, no ffmpeg knowledge).
5. **Docs** — add the feature to `README.md` and, if the ordering matters, to
   the filter-order table in `docs/ARCHITECTURE.md`.

If a filter needs a *keyframed* parameter, use `easingExpression()` /
`piecewiseLinear()` rather than inventing interpolation; dynamic crop and dynamic
speed are the reference implementations.

## Changing encoding behaviour

Anything that affects the output bytes (codec flags, rate control, pixel format,
container) needs:

* a test in `tests/core_tests.cpp` (the existing VP9/H.264/two-pass/alpha/HDR
  tests are the template),
* a note in `docs/ARCHITECTURE.md` if the pass structure changes,
* a manual check that `ffmpeg` accepts the graph: run the printed command by hand
  on a short clip.

---

## Pull requests

* One logical change per PR. Separate "add filter X" from "reformat dialogs".
* Write a PR description that says **what the user can now do** and **how you
  verified it** (which test, which ffmpeg version, which sample file).
* Keep commits focused; a message like `core: emit adelay for audio delay` is
  ideal. Squash fixups before asking for review.
* New user-visible strings should be plain and short; there is no translation
  infrastructure yet, so don't add `tr()` wrappers in new code (existing ones can
  stay).
* Don't commit build artefacts (`build/` is git-ignored), sample videos, or
  downloaded media.
* By contributing you agree that your work is licensed under the project's MIT
  license (see `LICENSES.md`).

## Bug reports

Include:

* OS and version, Qt version, `ffmpeg -version` (first line), `yt-dlp --version`,
* the exact input file properties (`ffprobe -v error -show_streams file`),
* the settings used (a screenshot of the dialogs is fine),
* the ffmpeg stderr from the log pane — this is almost always enough to diagnose
  it.

If the generated command itself is wrong, a minimal reproduction is the
`CommandBuilder` inputs and the bad `-filter_complex` string; that can become a
unit test directly.

## Scope

Accepted: bug fixes, new filters that exist in the original tool, platform
fixes, tests, performance work that doesn't complicate the core.

Out of scope by design: AviSynth/FFMS2, bundled ffmpeg binaries, cloud upload
integrations, QML/GPU preview, and a self-updater (deferred, not rejected).
