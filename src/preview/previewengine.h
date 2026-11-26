#pragma once
// Preview decoder with a persistent on-disk frame cache and a background
// "indexer" that expands a prefetch window around the cursor (our ffmpeg-only
// analogue of FFMS2's .ffindex: no frame data in FFMS's case, but here we cache
// decoded preview JPEGs so revisits/seeks are instant).

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
    void indexingActive(bool active);

private slots:
    void onProcFinished(int exitCode, QProcess::ExitStatus status);
    void onBurstFinished(int exitCode, QProcess::ExitStatus status);

private:
    void startDecode(int frame);
    void scheduleBurst(int centerFrame);
    void startBurst(int centerFrame);
    QString buildVf(double t) const;
    QString diskPath(const QString& vf, int frame) const;
    bool loadFromDisk(const QString& vf, int frame, QImage& out) const;
    void saveToDisk(const QString& vf, int frame, const QImage& img) const;

    QString ffmpegPath_ = QStringLiteral("ffmpeg");
    QString sourcePath_;
    VideoInfo info_;
    QString cacheRoot_;          // per-source disk cache directory
    FrameCache* cache_ = nullptr;
    QProcess* proc_ = nullptr;
    QProcess* burstProc_ = nullptr;
    QTimer* burstTimer_ = nullptr;
    QString burstDir_;           // temp dir for the current burst
    bool busy_ = false;
    int currentFrame_ = -1;
    int pendingFrame_ = -1;
    int burstCenter_ = 0;
    int burstStart_ = 0;
    int burstCount_ = 0;
    int expansion_ = 1;          // grows the background window while idle
    bool burstRunning_ = false;
    QString currentVf_;
    QString burstVf_;
    std::function<QString(double)> filterProvider_;
};

} // namespace wc
