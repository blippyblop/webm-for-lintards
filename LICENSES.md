# Licenses

WebM for Lintards itself is MIT-licensed. It depends on Qt 6 at build/link time and
on `ffmpeg`, `ffprobe` and `yt-dlp` as separate runtime programs. This file
summarises those licenses; it is not legal advice.

## This project — MIT

```
MIT License

Copyright (c) 2026 WebMConverter-Qt contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

The full text is in [`LICENSE`](LICENSE).

### Upstream inspiration

The feature set and UI workflow are inspired by
[`argorar/WebMConverter`](https://github.com/argorar/WebMConverter), also
MIT-licensed. No source code is copied verbatim; the C# implementation was used
as a behavioural reference. The original copyright notice should be retained in
any distribution that includes derived material from it.

---

## Qt 6 — LGPLv3 (or commercial)

Qt is used under the **GNU Lesser General Public License, version 3**
(LGPLv3), with the Qt Company's additional terms where applicable.

What this means for distribution:

* **Dynamic linking (recommended, and how the release builds are made).** Ship
  the unmodified Qt shared libraries (`.so`, `.dll`, `.dylib`) alongside the
  executable, include the LGPLv3 text and a notice that Qt is used, and state
  where to obtain the exact Qt source version used. Users must be able to
  replace the Qt libraries with a modified build and still run the application.
  `windeployqt` / `macdeployqt` produce exactly this layout.
* **Static linking.** If you link Qt statically, the whole resulting binary
  falls under LGPLv3 obligations: you must provide the application in a form
  that can be relinked against a modified Qt (object files or equivalent), or
  release the application under a GPL-compatible license. For most
  redistributors this is more trouble than it is worth; prefer shared Qt.
* **Commercial license.** If neither LGPLv3 nor GPL terms suit you, the Qt
  Company sells a commercial license that removes these obligations.

Only the Qt modules `Core`, `Gui` and `Widgets` are used. No GPL-only Qt
module is linked.

Qt copyright: © The Qt Company Ltd. and contributors.
License texts: <https://www.qt.io/licensing/open-source-lgpl-obligations>

---

## ffmpeg / ffprobe — LGPL or GPL, depending on the build

`ffmpeg` and `ffprobe` are **not** linked into this application and are **not
bundled**. They are invoked as external processes and must be installed by the
user (or by the packager) on `PATH`.

* Upstream ffmpeg is licensed under the **LGPLv2.1+** by default.
* Builds with `--enable-gpl` (which enables `libx264`, `libx265`, `vid.stab` and
  other GPL components) are **GPLv2+**. Almost every distribution package and
  every "static build" download is a GPL build.
* Some optional external libraries (`libfdk_aac`, for example) impose their own
  terms.

Because the binaries are separate programs, their licenses do not change the
license of this project. However, if you **redistribute** a package that bundles
an ffmpeg binary, you must comply with that binary's license (including offering
the corresponding source for a GPL build) and with the licenses of its codecs.
The safest route for downstream packagers is to depend on the distribution's
`ffmpeg` package rather than shipping your own.

Encoders used by this project and their homes:

| Encoder | Provided by | License note |
| --- | --- | --- |
| `libvpx`, `libvpx-vp9` | libvpx | BSD-style |
| `libopus` | libopus | BSD-style |
| `libvorbis` | libvorbis | BSD-style |
| `libx264` | x264 | GPLv2 (requires a GPL ffmpeg build) |
| `libx265` | x265 | GPLv2 (or commercial) |
| `aac` | ffmpeg native | LGPL (part of ffmpeg) |
| `vid.stab` | vid.stab | GPLv2 |

ffmpeg project: <https://ffmpeg.org/legal.html>

---

## yt-dlp — Unlicense

`yt-dlp` is released into the public domain under **The Unlicense**. It is also
not bundled; `YtdlpRunner` merely executes a `yt-dlp` binary found on `PATH`.
Its optional dependencies (e.g. `ffmpeg` for muxing, `phantomjs`/`deno` for
some extractors) carry their own licenses.

<https://github.com/yt-dlp/yt-dlp/blob/master/LICENSE>

---

## Third-party code in this repository

The repository contains no vendored third-party source. All core code is
original C++17; the only external API surface used at compile time is Qt.

If you add a dependency, add it to this file **and** to the build's license
documentation in the same pull request — including the license name, the
version, whether it is linked or invoked as a subprocess, and any notice
requirements attached to it.
