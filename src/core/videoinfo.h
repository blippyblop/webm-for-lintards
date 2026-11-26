#pragma once
// Plain (Qt-free) media metadata model produced by MediaProbe and consumed by
// CommandBuilder. Keeping this Qt-free lets the command-building logic be unit
// tested without a Qt toolchain.

#include <string>
#include <vector>
#include <cmath>

namespace wc {

struct Rational {
    int num = 0;
    int den = 1;
    double value() const { return den ? static_cast<double>(num) / den : 0.0; }
};

struct StreamInfo {
    int index = -1;
    std::string codecName;   // e.g. "h264", "vp9", "aac"
    std::string codecType;   // "video" | "audio" | "subtitle" | "attachment"
    std::string language;
    std::string title;
    int width = 0;
    int height = 0;
    Rational frameRate;
    int sampleRate = 0;
    int channels = 0;
    double durationSeconds = 0.0;
};

struct VideoInfo {
    std::string path;
    double durationSeconds = 0.0;
    Rational frameRate;              // nominal frame rate
    int width = 0;
    int height = 0;
    std::string pixelFormat;         // "yuv420p", "yuv420p10le", ...
    std::string colorRange;          // "tv" | "pc"
    std::string colorPrimaries;      // "bt709" | "bt2020" | ...
    std::string colorTransfer;
    bool interlaced = false;
    bool hasAudio = false;
    std::vector<StreamInfo> streams;

    double frameRateValue() const { return frameRate.value(); }

    // Frame <-> time helpers. Round-trips through the nominal frame rate; the
    // original tool used FFMS2 PTS data, which is more precise at the edges but
    // this is sufficient for timeline navigation.
    int frameAt(double seconds) const {
        const double fps = frameRateValue();
        if (fps <= 0.0) return 0;
        const int f = static_cast<int>(std::floor(seconds * fps));
        return f < 0 ? 0 : f;
    }
    double timeOfFrame(int frame) const {
        const double fps = frameRateValue();
        if (fps <= 0.0) return 0.0;
        return frame / fps;
    }
    long long frameCount() const {
        const double fps = frameRateValue();
        if (fps <= 0.0) return 0;
        return static_cast<long long>(std::llround(durationSeconds * fps));
    }
};

} // namespace wc
