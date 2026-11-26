#pragma once
// Decodes single preview frames on demand via ffmpeg, with an LRU cache and
// coalescing of rapid seek requests. Emits frames as QImage on the UI thread.
//
// Strategy (see docs/ARCHITECTURE.md): `ffmpeg -ss <t> -i <in> -frames:v 1
// -f image2pipe -vcodec png -` -> QImage::fromData. A future optimisation is to
// decode directly with libavcodec behind this same interface.

#include "core/videoinfo.h"

#include <QObject>
#include <QImage>
#include <QProcess>
#include <QString>

namespace wc {

class FrameCache;

class PreviewEngine : public QObject {
    Q_OBJECT
public:
    explicit PreviewEngine(QObject* parent = nullptr);
    ~PreviewEngine() override;

    void setFfmpegPath(const QString& path) { ffmpegPath_ = path; }
    void setSource(const QString& path, const VideoInfo& info);
    void clear();

    void requestFrame(int frame);
    void cancel();

    bool hasSource() const { return !sourcePath_.isEmpty(); }
    double frameToSeconds(int frame) const { return info_.timeOfFrame(frame); }
    int secondsToFrame(double s) const { return info_.frameAt(s); }

signals:
    void frameReady(int frame, const QImage& image);
    void error(const QString& message);

private slots:
    void onProcFinished(int exitCode, QProcess::ExitStatus status);

private:
    void startDecode(int frame);

    QString ffmpegPath_ = QStringLiteral("ffmpeg");
    QString sourcePath_;
    VideoInfo info_;
    FrameCache* cache_ = nullptr;
    QProcess* proc_ = nullptr;
    bool busy_ = false;
    int currentFrame_ = -1;
    int pendingFrame_ = -1;
};

} // namespace wc
