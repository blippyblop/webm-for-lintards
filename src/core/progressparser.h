#pragma once
// Qt-free parser for ffmpeg's stderr progress lines. Keeping it separate from
// the QProcess runner makes it unit-testable.

#include <string>

namespace wc {

struct EncodeProgress {
    long long frame = -1;
    double timeSeconds = -1.0;
    double fps = -1.0;
    double bitrateKbps = -1.0;
    double speed = -1.0;      // multiple of realtime
    double percent = -1.0;    // 0..100 (needs duration)
};

class ProgressParser {
public:
    void setDuration(double seconds) { duration_ = seconds; }
    double duration() const { return duration_; }

    // Feed one stderr line. Returns true if any field in the progress state
    // changed as a result.
    bool feed(const std::string& line);

    const EncodeProgress& last() const { return last_; }
    void reset() { last_ = EncodeProgress{}; }

private:
    static bool parseTimestamp(const std::string& s, double& outSeconds);
    double duration_ = 0.0;
    EncodeProgress last_;
};

} // namespace wc
