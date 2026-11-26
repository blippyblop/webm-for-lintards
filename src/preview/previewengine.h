#pragma once
// Preview frame decoder with cache and *batched* background prefetch: after a
// seek settles, one ffmpeg process decodes a neighbourhood around the cursor to
// temp JPEGs which are loaded into the cache.

#include "core/videoinfo.h"

#include <QObject>
#include <QImage>
#include <QProcess>
#include <QString>

#include <functional>

class QTimer;

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

    void setFilterProvider(std::function<QString(double)> fn);

    bool hasSource() const { return !sourcePath_.isEmpty(); }
    double frameToSeconds(int frame) const { return info_.timeOfFrame(frame); }
    int secondsToFrame(double s) const { return info_.frameAt(s); }

signals:
    void frameReady(int frame, const QImage& image);
    void error(const QString& message);

private slots:
    void onProcFinished(int exitCode, QProcess::ExitStatus status);
    void onBurstFinished(int exitCode, QProcess::ExitStatus status);

private:
    void startDecode(int frame);
    void scheduleBurst(int centerFrame);
    void startBurst(int centerFrame);

    QString ffmpegPath_ = QStringLiteral("ffmpeg");
    QString sourcePath_;
    VideoInfo info_;
    FrameCache* cache_ = nullptr;
    QProcess* proc_ = nullptr;
    QProcess* burstProc_ = nullptr;
    QTimer* burstTimer_ = nullptr;
    QString burstDir_;
    bool busy_ = false;
    int currentFrame_ = -1;
    int pendingFrame_ = -1;
    int burstCenter_ = 0;
    int burstStart_ = 0;
    int burstCount_ = 0;
    bool burstRunning_ = false;
    std::function<QString(double)> filterProvider_;
};

} // namespace wc
