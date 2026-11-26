# Architecture

WebM for Lazys is a thin, testable compiler from *editing intent* to an
`ffmpeg` invocation, plus a Qt Widgets shell around it.

The whole design follows from one decision: **the part that is easy to get wrong
(the filter graph) must be testable without a GUI and without Qt.** Everything
else is plumbing.

---

## 1. Layers

| Layer | Location | Qt? | Responsibility |
| --- | --- | --- | --- |
| Core model + compiler | `src/core/` | **No** | `FilterModel`, `VideoInfo`, argv compilation, easing maths, size planning, progress parsing |
| I/O adapters | `src/core/mediaprobe.*`, `src/core/encoder.*`, `src/core/ytdlprunner.*` | Yes (`QProcess`) | Run `ffprobe`, `ffmpeg`, `yt-dlp`; convert to/from the Qt-free types |
| Preview | `src/preview/` | Yes | Frame extraction + cache, `PreviewCanvas`, `TimelineWidget` |
| UI shell | `src/ui/` | Yes | Main window, menus, drag & drop, and the filter dialogs in `src/ui/dialogs/` |

```
+---------------------------------------------------------------+
|  src/ui/            MainWindow (WIP), dialogs/*               |
|    writes v            reads ^            reads ^             |
+-------|------------------|-------------------|-----------------+
        v                  |                   |
+----------------+   +-------------+   +------------------+
|  FilterModel   |   |  VideoInfo  |   |   QImage frames  |
| EncodingOptions|   |             |   |  (preview only)  |
+-------+--------+   +------+------+   +------------------+
        |                   ^
        v                   |
+----------------+   +------+------+        +------------------+
| CommandBuilder |   | MediaProbe  |        |   YtdlpRunner    |
|   -> PassPlan[]|   | (ffprobe)   |        |   (yt-dlp)       |
+-------+--------+   +-------------+        +------------------+
        |
        v
+----------------+   stderr    +-----------------+
|    Encoder     |------------>| ProgressParser  |
|  (QProcess)    |             |  frame/time/fps |
+----------------+             +-----------------+
```

**Shared currency:** all three layers speak `FilterModel` + `VideoInfo` +
`QImage`. There is no second representation of an edit anywhere.

### The three-layer rule

1. **UI never builds command lines.** A dialog's `accept()` handler writes
   fields into the shared `FilterModel`; it never knows a filter is called
   `hqdn3d` or that two-pass needs `-passlogfile`.
2. **Core never touches widgets.** `src/core/commandbuilder.*`,
   `filtermodel.h`, `videoinfo.h`, `easing.*`, `sizecalc.*` and
   `progressparser.*` include only the standard library. `QString` conversions
   happen in `mediaprobe.cpp` / `encoder.cpp`.
3. **Adapters own processes.** Only `Encoder`, `MediaProbe` and `YtdlpRunner`
   spawn subprocesses; only they know about `QProcess`, exit codes and stderr.

This is why the core test binary (57 checks) builds with plain `g++` and no Qt
at all — see `tests/core_tests.cpp` and `scripts/build_core_tests.sh`.

---

## 2. Core types

### `VideoInfo` — `src/core/videoinfo.h`

A flat, Qt-free snapshot of the input as reported by `ffprobe`:

* `durationSeconds`, nominal `Rational frameRate`, `width`/`height`,
  `pixelFormat`, `colorRange`, `colorPrimaries`, `colorTransfer`, `interlaced`,
  `hasAudio`, and a `std::vector<StreamInfo>` with per-stream codec/language/
  dimensions/sample rate.
* Frame mathematics: `frameAt(seconds)`, `timeOfFrame(frame)`,
  `frameCount()`. These round-trip through the *nominal* frame rate. The original
  tool used FFMS2 presentation timestamps, which is more accurate at variable
  frame rate edges; the nominal-rate approach is sufficient for timeline
  navigation and is deterministic, which is more valuable here.

`VideoInfo` is **read-only during editing** — it describes the source, not the
result. After a trim, the output duration is computed by
`CommandBuilder::clipDuration()` rather than written back into `VideoInfo`.

### `FilterModel` + `EncodingOptions` — `src/core/filtermodel.h`

`FilterModel` is an *ordered set of optional edits*, directly mirroring the
original `WebMConverter.Filters` registry. Each feature is a small struct with an
`enabled` flag (or a `std::optional`), so "not applied" is representable without
sentinel values:

| Field | Type | Maps to |
| --- | --- | --- |
| `trim` | `std::optional<TrimRange>` | `-ss` / `-t` on the input |
| `multiTrim` | `std::vector<TrimRange>` | `split` + `trim` + `concat` |
| `crop` | `Crop{x,y,w,h,enabled}` | `crop` |
| `dynamicCrop` | `DynamicCrop{w,h,keyframes}` | `crop` with time expressions |
| `resizeEnabled`, `resizeW/H`, `resizeAlgo` | | `scale` |
| `rotateDegrees` | `int` (0/90/180/270) | `transpose` |
| `rateFps` | `std::optional<int>` | `-r` |
| `dynamicRate` | `std::vector<KeyframeRate>` | `setpts` + `atempo` |
| `caption` | `Caption` | `drawtext` |
| `subtitle` | `SubtitleTrack` | `subtitles` (burn-in) |
| `overlay` | `OverlaySpec` | `movie` + `overlay` |
| `fade` | `Fade{inDuration,outDuration}` | `fade` ×2 |
| `eq` | `EqFilter{gamma,saturation,contrast}` | `eq` |
| `denoise`, `denoiseStrength` | `bool`, `int` | `hqdn3d` preset |
| `deinterlace` | `bool` | `yadif` |
| `delayAudioMs` | `int` | `adelay` |
| `reverse`, `loop` | `bool` | `reverse`, `loop` |
| `stabilization` | `Stabilization` | vid.stab: an analysis pass (`vidstabdetect` → `transforms.trf`) is inserted before the encode pass, whose chain starts with `vidstabtransform` |
| `grid` | `GridSpec{otherFile,vertical}` | `hstack` / `vstack` special path |

`EncodingOptions` holds everything about the *output*: container/codec enums
(`OutputFormat`, `VideoCodec`, `RateControl`, `AudioCodec`), bitrates, `crf`,
`twoPass`, `hq`, `useAlpha`, `hdr`, `sizeLimitBytes`, `threads`, `slices`,
audio flags (`disableAudio`, `normalizeAudio`, `amplifyAudio`, `monoAudio`),
`targetFps` and container `title`.

### Pure helpers

* `easing.h` — `easingExpression(name, a, b, p)` builds an ffmpeg expression in
  `t` for the named easing (linear/quad/cubic in/out/in-out), and `lerp()`.
  Used by the keyframed filters. Degenerate durations are clamped, never
  divide by zero.
* `sizecalc.h` — `planForSize(duration, targetBytes, audioKbps) -> SizePlan`
  (see §7).
* `progressparser.h` — `ProgressParser::feed(line)` scans one stderr line for
  `frame=`, `fps=`, `bitrate=`, `speed=` and `time=` (or `out_time=`), returning
  `EncodeProgress` with `percent` computed against a supplied duration.

---

## 3. Data flow

```
file / URL
   |  drag & drop, File>Open, or Download tab
   v
MediaProbe::probe()        ffprobe -print_format json -show_format -show_streams
   |                        (defensive JSON parsing: numbers are sometimes strings)
   v
VideoInfo                  duration, fps, size, pix_fmt, streams
   |
   v
UI (MainWindow + dialogs)  user edits; dialogs write FilterModel / EncodingOptions
   |                        preview requests frames for the current time
   v
CommandBuilder::build()    FilterModel + EncodingOptions -> std::vector<PassPlan>
   |                        each PassPlan = argv tokens (program name excluded)
   v
Encoder::start(passes, duration)
   |                        one QProcess per pass, run sequentially
   |  ffmpeg stderr  ----> ProgressParser ----> EncodeProgress
   v
signals: progress(frame, time, percent, fps, speed)
         logLine(QString)
         finished(success, errorString)
   |
   v
UI: progress bar, ETA-ish readout, log pane, re-enable controls
```

Important detail: **the encoder never sees a `FilterModel`.** It only sees
already-built argv. That means two-pass and the grid path need no special
handling in the encoder — they are just "run these N passes in order".

---

## 4. `CommandBuilder` — the argv compiler

`src/core/commandbuilder.h` / `.cpp`. Constructed with
`(const VideoInfo&, const FilterModel&, const EncodingOptions&)` and exposes:

```cpp
std::vector<PassPlan> build(const std::string& inputPath,
                            const std::string& outputPath) const;
```

`PassPlan` holds `std::vector<std::string> args` and `isAnalysisPass`. Args are
tokens, not a shell string, so they go straight into
`QProcess::start(program, arguments)` — no quoting bugs, no `cmd.exe` mangling on
Windows. `CommandBuilder` also exposes `videoFilterGraph()`, `videoCodecArgs()`,
`audioFilterArgs()`, `effectiveVideoKbps()` and `effectiveAudioKbps()` for the
diagnostics/settings UI and for tests.

### 4.1 Two paths

* **Fast path** — no filters at all (`hasVideoFilters() == false`): the command
  is `-i in -map 0:v:0 <codec args> -map 0:a:0? <audio args> -f <fmt> out`.
  No `-filter_complex` is emitted, because an empty `-filter_complex` still costs
  a frame copy.
* **Filter path** — any filter enabled: a single `-filter_complex` graph ending
  in `[vout]`, mapped with `-map [vout]`.

### 4.2 Filter application order

Order is not arbitrary: geometry first, then timing, then text/overlays, then
temporal effects. The sequence below is exactly the order in
`CommandBuilder::videoFilterGraph()`:

| # | Filter | ffmpeg syntax | Why here |
| --- | --- | --- | --- |
| 0 | Multi-trim | `split=n` → per-range `trim,setpts=PTS-STARTPTS` → `concat=n=N:v=1:a=0` | Must precede everything: later filters see the concatenated timeline |
| 1 | Denoise | `hqdn3d=1.5:1.5:6:6` … `3:3:10:10` (3 presets) | Clean before spatial scaling so scaling doesn't amplify noise |
| 2 | Deinterlace | `yadif` | Before crop/scale, so fields are combined at source resolution |
| 3 | Crop | `crop=w:h:x:y` | Geometry in source pixels |
| 4 | Dynamic crop | `crop=w:h:x='expr':y='expr'` | Same stage as static crop; expressions use `t` |
| 5 | Resize | `scale=w:h:flags=<algo>` | After cropping, so the user's output size is exact |
| 6 | Rotate | `transpose=1` (90°), `transpose=2` (270°), 2× for 180° | After scaling; a 90° rotation swaps the output dimensions |
| 7 | Dynamic speed | `setpts='(PTS)/(<piecewise-linear rate>)'` | Timing change; must be after geometry and before text |
| 8 | EQ | `eq=gamma=…:saturation=…:contrast=…` | Colour, independent of geometry |
| 9 | Subtitles | `subtitles='path'` | Burned in at output resolution |
| 10 | Caption | `drawtext=text='…':fontcolor=…:fontsize=…:borderw=2:bordercolor=…:x=…:y=…[:enable='between(t,a,b)']` | Pixel coordinates refer to the final frame |
| 11 | Fade | `fade=t=in:st=0:d=…`, `fade=t=out:st=<dur-d>:d=…` | Uses `clipDuration()`, so it must be computed post-trim |
| 12 | Reverse | `reverse` | Buffers the whole (already shortened) clip; keep it late |
| 13 | Overlay | `movie='img'[wm];[cur][wm]overlay=x:y:shortest=1` | Second input chain; composited on the finished frame |
| 14 | Loop | `split[lf][lb];[lb]reverse[rv];[lf][rv]concat=n=2[lp];[lp]loop=loop=1:size=<frames>` | Terminal: builds forward+reverse palindrome and repeats. Returns the graph immediately with `[vout]` |
| 15 | Normalise | `[cur]copy[vout]` | Guarantees a single, stable output label |

Notes:

* A single (non-multi) trim is **not** a filter. It becomes `-ss <start>` before
  `-i` and `-t <end-start>` after it (`appendInputArgs`), which is much faster
  than trimming in the graph and keeps the filter graph representative of the
  edit, not the seek.
* `multiTrim` overrides `trim`; `clipDuration()` sums the ranges for
  size/bitrate planning and fade-out timing.
* Text escaping is centralised: `escapeDrawtext()` handles `\ ' : %`, and
  `quoteFilterValue()` single-quotes file paths for `subtitles`/`movie`.
* The graph is only generated when a filter is actually active; each compiled
  feature also updates `hasVideoFilters()`, which is what keeps the fast path
  honest (and what the tests assert).
* **Stabilisation is a multi-pass feature.** When `FilterModel::stabilization` is
  enabled, `CommandBuilder::build()` inserts a leading analysis pass
  (`-vf vidstabdetect ... result=transforms.trf -f null -`) before the normal
  encode pass(es); the encode pass's filter chain then begins with
  `vidstabtransform=input=transforms.trf` (+ a light `unsharp`). `Encoder` runs
  the passes sequentially in the same working directory, so the `.trf` is
  available to the encode pass. This composes with two-pass encoding (detect →
  encode-pass-1 → encode-pass-2).

### 4.3 Video encoding arguments

`videoCodecArgs()` composes:

1. `-c:v` from `codecName()`:
   `VP8 → libvpx`, `VP9 → libvpx-vp9`, `H264 → libx264`, `H265 → libx265`.
2. Rate-control flags from `rateControlArgs()` (see §5).
3. `-pix_fmt` from `pixelFormat()`: `yuv420p` normally, `yuva420p` with alpha,
   `yuv420p10le` with HDR.
4. HQ extras: `-lag-in-frames 16`, and `-auto-alt-ref 1` unless alpha is on
   (alt-ref frames are incompatible with `yuva420p`).
5. VP9 only: `-tile-columns 1 -row-mt 1`.
6. `-threads` / `-slices` when non-zero.
7. `-r <fps>` when `EncodingOptions::targetFps` is set, otherwise when the
   "change rate" filter (`FilterModel::rateFps`) is set.
8. HDR: `-color_primaries bt2020 -color_trc smpte2084 -colorspace bt2020nc`.

### 4.4 Audio arguments

`appendAudioArgs()` emits `-an`, or `-map 0:a:0?` (optional — silent inputs do
not fail), `-c:a`, `-b:a`, and optionally `-ac 1` for mono. The `-af` chain from
`audioFilterArgs()` is applied in this order:

| Order | Filter | Source |
| --- | --- | --- |
| 1 | `loudnorm=I=-16:TP=-1.5:LRA=11` | `EncodingOptions::normalizeAudio` |
| 2 | `volume=3dB` | `EncodingOptions::amplifyAudio` |
| 3 | `adelay=<ms>|<ms>` | `FilterModel::delayAudioMs` |
| 4 | `atempo=<mean rate>` (clamped 0.5–2.0) | `FilterModel::dynamicRate` |

`atempo` is an approximation: dynamic speed is applied exactly on video via
`setpts`, while audio gets the arithmetic mean of the keyframe rates, clamped to
`atempo`'s supported range. Exact per-segment audio speed would need a chain of
`atempo`/`asegment` filters and is a known limitation, inherited from the
original's behaviour.

Encoder names follow the container: WebM uses `libopus` (or `libvorbis` when
selected); MP4 always uses `aac`.

---

## 5. Container, codec and rate control

`containerFormat()` is derived from the codec, not chosen independently:
VP8/VP9 → `webm`, H.264/H.265 → `mp4`. This avoids invalid combinations such as
H.264 in WebM.

`rateControlArgs()` has two modes:

**Constant bitrate** (`RateControl::Constant`, or any target-filesize run):

```
-b:v <kbps>k  [-qcomp 0 (webm)]  [-fs <bytes> (size limit only)]
```

**Variable quality** (`RateControl::Variable`):

* VP8/VP9: `-qmin 0 -crf <crf> -qmax 63 -qcomp 1 -b:v 0` (libvpx needs
  `-b:v 0` to run in true constant-quality mode).
* H.264/H.265: `-crf <crf>`.

A target filesize always forces the constant-bitrate branch, because the size is
the constraint and quality is the dependent variable.

---

## 6. Two-pass encoding

Enabled by `EncodingOptions::twoPass`. `build()` returns two `PassPlan`s:

**Pass 1 (analysis)** — `isAnalysisPass = true`

```
[input args] [video args] -an -pass 1 -passlogfile <output>.2pass -f null -
```

Video settings must match pass 2 exactly, or libvpx/x264 will produce a worse
result than a single pass; that is why both passes call the same
`appendVideoArgs()`. Audio is omitted (`-an`) because it does not influence the
video statistics.

**Pass 2** — the real encode

```
[input args] [video args] [audio args] -pass 2 -passlogfile <output>.2pass
-f <webm|mp4> [metadata] -y <output>
```

The pass log is written next to the output as `<output>.2pass` and left for
ffmpeg to consume. `Encoder` runs the plans strictly sequentially: a pass only
starts from `onProcFinished` when the previous one exited successfully, and
`finished(false, …)` aborts the chain. Cancelling kills the current `QProcess`
and stops; no further passes start. If a run is cancelled, stale `.2pass` files
may remain (they are in `.gitignore` and harmless, but should be deleted before
re-running to avoid confusing libvpx statistics).

Progress: the same `durationSeconds` is used for every pass, so the UI sees the
progress bar restart for pass 2. That is deliberate — it keeps each pass's
throughput legible, and the UI can label "pass 1 / 2" itself because it knows it
started a two-pass run.

---

## 7. Target filesize

`src/core/sizecalc.cpp`, used via `CommandBuilder::effectiveVideoKbps()` and
`effectiveAudioKbps()`.

```
totalKbps = targetBytes * 8 / 1000 / clipDuration
usable    = totalKbps * 0.98 - audioKbps        // 2% muxing/container headroom
videoKbps = floor(usable)
```

* `clipDuration` is `CommandBuilder::clipDuration()` — the sum of multi-trim
  ranges, the single trim length, or the full source duration.
* If `usable < 8 kbps` the plan is marked `feasible = false` and the bitrate is
  clamped to an 8 kbps floor so the command still runs (the UI should warn
  instead of producing a 4 kbps video).
* The result feeds `-b:v` and, additionally, `-fs <targetBytes>` as a hard cap.
  Two mechanisms are used on purpose: the bitrate plan aims at the target, `-fs`
  guarantees it even when the encoder overshoots on a difficult clip.
* Audio is subtracted at its declared bitrate; disabling audio
  (`effectiveAudioKbps() == 0`) gives the whole budget to video.

Because a size target implies a bitrate target, it overrides `crf`/variable mode
entirely — a CRF encode cannot honour a byte budget.

---

## 8. I/O adapters

### `MediaProbe` — `src/core/mediaprobe.{h,cpp}`

Synchronous `ffprobe -v error -print_format json -show_format -show_streams`.
The JSON parsing is deliberately defensive: ffprobe emits numbers as strings in
some builds (`"duration": "12.345000"`), omits absent keys, and reports frame
rates as `"30000/1001"`. Helpers (`toDouble`, `toInt`, rational parsing,
`qstr`) normalise all of that into `VideoInfo`, including
`interlaced` (from field order / `field_order`) and per-stream metadata. Errors
are returned as a human-readable `QString*`, never as an exception.

### `Encoder` — `src/core/encoder.{h,cpp}`

A `QObject` owning one `QProcess` at a time and a `ProgressParser`. Public
surface:

```cpp
void start(const std::vector<PassPlan>& passes, double durationSeconds);
void cancel();
bool isRunning() const;
signals:
  void logLine(const QString&);
  void progress(qint64 frame, double timeSeconds, double percent, double fps, double speed);
  void finished(bool success, const QString& errorString);
```

Details worth knowing:

* The process runs with `QProcess::MergedChannels`, so stdout and stderr arrive
  on one stream, interleaved with ffmpeg's in-place progress updates (`\r`).
  The reader splits on both `\n` and `\r` and keeps a carry buffer for partial
  lines.
* Progress-looking lines are consumed by `ProgressParser`; everything else is
  forwarded as `logLine`, so the UI's log pane shows the full ffmpeg output
  (including warnings and the final error, if any).
* `ProgressParser` is hidden behind a `ParserHolder` pimpl so the header does not
  drag in the parser (and stays light for UI translation units).
* Cancellation is cooperative: `cancel()` kills the process and suppresses the
  `finished(true)` that a clean exit would emit.
* The binary path is configurable (`setFfmpegPath`), defaulting to `ffmpeg` on
  `PATH`.

### `YtdlpRunner` — `src/core/ytdlprunner.{h,cpp}`

Mirror of `Encoder` for downloads: `isAvailable()` checks `PATH`,
`download(url, outputDir, format)` starts `yt-dlp`, `progress(percent, speed,
eta)` is parsed from its output, and `finished(success, outputPath, error)` hands
back the downloaded file for immediate loading.

The `URL@*start-end` syntax is handled here: a URL such as
`https://…/watch?v=ID@*1:30-1:45` is split at `@*`, and the range is passed to
`yt-dlp` as a download section (`--download-sections "*1:30-1:45"`). This is the
same convenience the original tool offered, without downloading the full video
first.

---

## 9. Preview subsystem

Location: `src/preview/` — `PreviewCanvas` (custom-painted `QWidget`) and
`TimelineWidget` (custom-painted scrubber with frame-accurate wheel steps).

Strategy, chosen for correctness-per-line-of-code rather than speed:

1. **Single-frame extraction with ffmpeg.** For a requested time `t`, run
   `ffmpeg -ss <t> -i <input> -frames:v 1 -f image2pipe -vcodec png -` and decode
   the PNG into a `QImage` (or write to a temp file and load it, on platforms
   where the pipe is awkward). `-ss` before `-i` uses keyframe seeking plus
   decode, which is fast enough for scrubbing.
2. **LRU cache.** Frames are cached keyed by (file, frame index) with a bounded
   count (a few hundred frames of the *scaled preview* size, not full
   resolution). Scrubbing back and forth is instant after the first visit;
   stepping one frame forward reuses the cache until it misses.
3. **Frame ↔ time** goes through `VideoInfo::frameAt()` / `timeOfFrame()`, so the
   preview, the timeline and the trim spin boxes agree on frame numbers. Wheel
   steps on the timeline are one frame; Shift accelerates.
4. **Crop overlay** is drawn by `PreviewCanvas`, not baked into the image: the
   crop rectangle, keyframe handles and caption placement are painted over the
   cached frame with `QPainter`, so dragging is smooth and never re-extracts.
5. **Future upgrade path:** link `libavcodec`/`libavformat` (or use
   `QMediaPlayer` with a frame grab) and keep a decoder open per input, seeking
   by PTS. That removes the process spawn per miss and makes long-GOP seeking
   cheap. The interface (`requestFrame(file, frame) -> QImage`) is designed so
   this swap does not touch `PreviewCanvas` or the dialogs.

The preview is *advisory*: it shows the source frame, not the filtered output.
Rendering the filter graph for preview would require running ffmpeg per edit —
the original tool's AviSynth preview is exactly the complexity this port
deliberately drops.

---

## 10. UI shell and dialogs

`src/ui/dialogs/` currently contains one dialog per editable feature, all
following the same contract: construct with the current model value, let the
user edit, and on `accept()` write back and set `enabled` accordingly. No dialog
builds ffmpeg syntax.

| Dialog | Model field |
| --- | --- |
| `TrimDialog`, `MultiTrimDialog` | `trim`, `multiTrim` |
| `CropDialog` | `crop` (arrow-key nudging, Shift/Alt/Ctrl modifiers) |
| `DynamicRateDialog` | `dynamicRate` keyframes |
| `ResizeDialog`, `RotateDialog`, `RateDialog` | resize / rotate / `rateFps` |
| `CaptionDialog`, `SubtitleDialog`, `OverlayDialog` | text, burn-in, image overlay |
| `FadeDialog`, `EqDialog`, `DenoiseDialog`, `DeinterlaceDialog` | temporal/colour/cleanup |
| `DelayAudioDialog` | `delayAudioMs` |
| `StabilizationDialog` | `stabilization` |

The main window (`MainWindow`, menus, toolbar, drag & drop, batch list, download
tab) is the remaining piece. It will own:

* the current `VideoInfo` and a `FilterModel`/`EncodingOptions` pair,
* the `Encoder`, `MediaProbe` and `YtdlpRunner` instances,
* the "show command" panel, fed by `CommandBuilder::videoFilterGraph()`,
  `videoCodecArgs()` and `audioFilterArgs()` — which is also the best debugging
  tool users have.

Threading: `MediaProbe` is synchronous and called only on user action (it is
sub-second for normal files); `Encoder` and `YtdlpRunner` are asynchronous
`QObject`s driven by signals, so the event loop is never blocked during an
encode. All UI updates happen on the main thread; the adapters emit from it too
(`QProcess` signals are delivered there).

---

## 11. Testing

`tests/core_tests.cpp` — 13 test functions, 53 `CHECK`s, no framework:

* model maths (`VideoInfo` frame conversions, `frameCount`),
* easing expressions (including degenerate duration),
* size planning (feasibility, floors, audio subtraction),
* progress parsing (partial lines, missing fields),
* argv snapshots for: default VP9, constant bitrate + `-fs`, trim+crop,
  two-pass structure, MP4/H.264 with audio disabled, alpha and HDR pixel formats,
  resize/rotate/rate, dynamic crop expressions, and the grid path.

The tests assert on *tokens* and *substrings of the filter graph*, not on exact
whole command strings, so adding an unrelated flag does not break every test.
This is the reason `CommandBuilder` exposes `videoFilterGraph()` and
`videoCodecArgs()` separately.

`scripts/check_qt_compile.sh` compile-checks every Qt translation unit (23 at the
time of writing) with `-c` only, which is how the adapters and dialogs are
verified on machines where the available Qt tree targets another architecture.

---

## 12. Deliberate non-parity

| Original feature | Status | Reason |
| --- | --- | --- |
| Gfycat / ShareX upload | Dropped | Gfycat is dead; ShareX is Windows-only |
| Self-updater | Deferred | Needs signing/hosting; out of scope for a source port |
| AviSynth scripts + FFMS2 source | Dropped | Replaced by pure ffmpeg filter graphs |
| Bundled ffmpeg/yt-dlp | Not shipped | Licensing and size; rely on `PATH` |
| Exact per-segment audio speed | Approximated | `atempo` mean value; see §4.4 |

Everything else in `FilterModel` is intended to reach parity with the original's
filter list. New features should follow the pattern in `CONTRIBUTING.md`.
