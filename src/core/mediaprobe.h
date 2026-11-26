#pragma once
// Probes a media file with ffprobe and fills a Qt-free VideoInfo.
// CONTRACT: implementation lives in mediaprobe.cpp.

#include "core/videoinfo.h"

#include <QString>

namespace wc {

class MediaProbe {
public:
    explicit MediaProbe(QString ffprobePath = QStringLiteral("ffprobe"));

    // Runs ffprobe synchronously. Returns true on success.
    // On failure, *error (if non-null) is set to a human readable message.
    bool probe(const QString& filePath, VideoInfo& info, QString* error = nullptr) const;

    QString ffprobePath() const { return ffprobePath_; }
    void setFfprobePath(const QString& p) { ffprobePath_ = p; }

private:
    QString ffprobePath_;
};

} // namespace wc
