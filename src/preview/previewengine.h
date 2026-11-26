#pragma once
// Decodes preview frames via ffmpeg with an optional per-frame filter chain,
// caches them, and prefetches a neighbourhood around the cursor in the
// background so scrubbing stays smooth.

#include "core/videoinfo.h"

#include <QObject>
#include <QImage>
#include <QProcess>
#include <QString>
#include <QList>

#include <functional>

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
    void clearCache();

    // Per-frame filter chain provider (seconds -> ffmpeg -vf chain). Setting it
    // invalidates the cache because the same frame now renders differently.
    void setFilterProvider(std::function<QString(double)> fn);

    bool hasSource() const { return !sourcePath_.isEmpty(); }
    double frameToSeconds(int frame) const { return info_.timeOfFrame(frame); }
    int secondsToFrame(double s) const { return info_.frameAt(s); }

signals:
    void frameReady(int frame, const QImage& image);
    void error(const QString& message);

private slots:
    void onProcFinished(int exitCode, QProcess::ExitStatus status);

private:
    void startDecode(int frame, bool forDisplay);
    void schedulePrefetch(int frame);
    void kickNext();

    QString ffmpegPath_ = QStringLiteral("ffmpeg");
    QString sourcePath_;
    VideoInfo info_;
    FrameCache* cache_ = nullptr;
    QProcess* proc_ = nullptr;
    bool busy_ = false;
    bool currentForDisplay_ = false;
    int currentFrame_ = -1;
    int pendingFrame_ = -1;
    QList<int> prefetchQueue_;
    std::function<QString(double)> filterProvider_;
};

} // namespace wc
