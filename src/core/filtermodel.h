#pragma once
// Qt-free representation of the editing state. This mirrors the original
// WebMConverter.Filters registry: an ordered set of optional edits that the
// CommandBuilder turns into an ffmpeg invocation.

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <cstdint>

namespace wc {

struct TrimRange {
    double start = 0.0;   // seconds
    double end = 0.0;     // seconds (0 or <= start means "to end")
};

struct Crop {
    bool enabled = false;
    int x = 0, y = 0, w = 0, h = 0;
};

struct KeyframeCrop {
    double time = 0.0;    // seconds
    int x = 0, y = 0, w = 0, h = 0;
};

struct DynamicCrop {
    bool enabled = false;
    int w = 0, h = 0;
    std::vector<KeyframeCrop> keyframes;   // >= 2 for animation
};

struct KeyframeRate {
    double time = 0.0;    // seconds
    double rate = 1.0;    // speed multiplier (>1 faster, <1 slower)
};

struct Caption {
    bool enabled = false;
    std::string text;
    std::string fontFamily = "sans";
    int fontSize = 24;
    int x = 0, y = 0;
    double start = 0.0, end = 0.0;
    std::string color = "white";
    std::string borderColor = "black";
};

struct SubtitleTrack {
    bool enabled = false;
    int streamIndex = -1;          // internal track index (or -1 for external)
    std::string path;              // external subtitle file
    bool burnIn = true;
};

struct OverlaySpec {
    bool enabled = false;
    std::string imagePath;
    int x = 0, y = 0;
    double start = 0.0, end = 0.0; // 0,0 = whole clip
};

struct Fade {
    bool enabled = false;
    double inDuration = 0.0;   // seconds
    double outDuration = 0.0;  // seconds
};

struct Stabilization {
    bool enabled = false;
    int shakiness = 5;         // 1..10
    int accuracy = 15;
    int smoothing = 10;
    int zoom = 0;
    int interpol = 2;
};

struct GridSpec {
    bool enabled = false;
    std::string otherFile;
    bool vertical = false;
};

struct EqFilter {
    bool enabled = false;
    double gamma = 1.0;
    double saturation = 1.0;
    double contrast = 1.0;
};

struct FilterModel {
    std::optional<TrimRange> trim;
    std::vector<TrimRange> multiTrim;          // if non-empty, overrides single trim

    Crop crop;
    DynamicCrop dynamicCrop;

    bool resizeEnabled = false;
    int resizeW = 0, resizeH = 0;
    std::string resizeAlgo = "bicubic";

    int rotateDegrees = 0;                     // 0/90/180/270
    std::optional<int> rateFps;                // change frame rate
    bool dynamicRateEnabled = false;
    std::vector<KeyframeRate> dynamicRate;     // speed ramp keyframes
    bool dynamicRateSmoothing = true;          // blend rates near each keyframe
    double dynamicRateSmoothingOverlap = 0.5;  // seconds of blend before each keyframe

    Caption caption;
    SubtitleTrack subtitle;
    OverlaySpec overlay;
    Fade fade;
    EqFilter eq;

    bool denoise = false;
    int denoiseStrength = 0;                   // 0 = light ... 3 = strong (hqdn3d preset)
    bool deinterlace = false;
    int delayAudioMs = 0;
    bool reverse = false;
    bool loop = false;
    Stabilization stabilization;
    GridSpec grid;                             // specialized multi-input path
};

// ---- Encoding options -------------------------------------------------------

enum class OutputFormat { WebM, MP4 };
enum class VideoCodec { VP8, VP9, H264, H265 };
enum class RateControl { Constant, Variable };
enum class AudioCodec { Opus, Vorbis, AAC };

struct EncodingOptions {
    OutputFormat format = OutputFormat::WebM;
    VideoCodec codec = VideoCodec::VP9;
    RateControl rateControl = RateControl::Variable;
    AudioCodec audioCodec = AudioCodec::Opus;
    int audioStreamIndex = 0;      // ordinal among the source's audio streams

    int videoBitrateKbps = 1500;   // constant rate-control mode
    int audioBitrateKbps = 128;
    int crf = 30;                  // variable rate-control mode

    bool twoPass = true;           // two-pass encode by default
    bool hq = false;               // -lag-in-frames 16 -auto-alt-ref 1
    bool useAlpha = false;         // yuva420p
    bool hdr = false;              // 10-bit / bt.2020
    long long sizeLimitBytes = 4LL * 1024 * 1024;  // 4 MB default; 0 = unlimited
    int threads = 0;               // 0 = auto
    int slices = 0;                // 0 = auto

    bool disableAudio = true;      // audio off by default
    bool normalizeAudio = false;
    bool amplifyAudio = false;
    bool monoAudio = false;

    int targetFps = 0;             // 0 = keep source
    std::string title;             // container title metadata
};

} // namespace wc
