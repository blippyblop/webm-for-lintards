#include "preview/previewengine.h"
#include "preview/framecache.h"

#include <QByteArray>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace wc {

PreviewEngine::PreviewEngine(QObject* parent)
    : QObject(parent), cache_(new FrameCache) {}

PreviewEngine::~PreviewEngine() {
    cancel();
    delete cache_;
}

void PreviewEngine::setSource(const QString& path, const VideoInfo& info) {
    cancel();
    sourcePath_ = path;
    info_ = info;
    cache_->clear();
    prefetchQueue_.clear();
    pendingFrame_ = -1;
}

void PreviewEngine::clear() {
    cancel();
    sourcePath_.clear();
    info_ = VideoInfo{};
    cache_->clear();
    prefetchQueue_.clear();
}

void PreviewEngine::clearCache() {
    cache_->clear();
}

void PreviewEngine::setFilterProvider(std::function<QString(double)> fn) {
    filterProvider_ = std::move(fn);
    cache_->clear();
}

void PreviewEngine::requestFrame(int frame) {
    if (!hasSource()) return;
    if (frame < 0) frame = 0;

    if (cache_->contains(frame)) {
        emit frameReady(frame, cache_->get(frame));
        schedulePrefetch(frame);
        return;
    }
    schedulePrefetch(frame);
    if (busy_) {
        pendingFrame_ = frame;   // coalesce to the latest user seek
        return;
    }
    startDecode(frame, true);
}

void PreviewEngine::schedulePrefetch(int frame) {
    prefetchQueue_.clear();
    const long long total = info_.frameCount();
    if (total <= 1) return;

    // ~5% of the clip either side of the cursor, nearest-first, capped.
    const int radius = static_cast<int>(std::max<long long>(1, total / 20));
    const int cap = 200;
    for (int d = 1; d <= radius && prefetchQueue_.size() < cap; ++d) {
        const int ahead = frame + d;
        const int behind = frame - d;
        if (ahead < total && ahead != pendingFrame_ && ahead != currentFrame_ &&
            !cache_->contains(ahead))
            prefetchQueue_ << ahead;
        if (behind >= 0 && behind != pendingFrame_ && behind != currentFrame_ &&
            !cache_->contains(behind))
            prefetchQueue_ << behind;
    }
}

void PreviewEngine::startDecode(int frame, bool forDisplay) {
    busy_ = true;
    currentFrame_ = frame;
    currentForDisplay_ = forDisplay;

    if (proc_) {
        proc_->disconnect(this);
        proc_->deleteLater();
        proc_ = nullptr;
    }
    proc_ = new QProcess(this);
    proc_->setProgram(ffmpegPath_);

    QStringList args;
    args << "-loglevel" << "error"
         << "-ss" << QString::number(info_.timeOfFrame(frame), 'f', 6)
         << "-i" << sourcePath_;

    QString vf;
    if (filterProvider_) vf = filterProvider_(info_.timeOfFrame(frame));
    // Cap preview resolution: hugely cheaper to decode/encode/paint.
    if (info_.width > 1280) {
        if (!vf.isEmpty()) vf += QLatin1Char(',');
        vf += QStringLiteral("scale=1280:-2");
    }
    if (!vf.isEmpty()) args << "-vf" << vf;

    args << "-frames:v" << "1"
         << "-f" << "image2pipe"
         << "-vcodec" << "mjpeg"
         << "-q:v" << "4"
         << "-";
    proc_->setArguments(args);
    proc_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(proc_, &QProcess::finished, this, &PreviewEngine::onProcFinished);
    proc_->start();
}

void PreviewEngine::onProcFinished(int exitCode, QProcess::ExitStatus status) {
    QByteArray data;
    if (proc_) data = proc_->readAllStandardOutput();

    const int decodedFrame = currentFrame_;
    const bool wasDisplay = currentForDisplay_;

    if (proc_) {
        proc_->disconnect(this);
        proc_->deleteLater();
        proc_ = nullptr;
    }
    busy_ = false;

    if (status == QProcess::NormalExit && exitCode == 0 && !data.isEmpty()) {
        QImage img = QImage::fromData(data, "JPEG");
        if (!img.isNull()) {
            cache_->insert(decodedFrame, img);
            if (wasDisplay) emit frameReady(decodedFrame, img);
        }
    } else if (wasDisplay && !data.isEmpty() && exitCode != 0) {
        emit error(tr("Failed to decode preview frame %1").arg(decodedFrame));
    }

    // A user seek always takes priority over background prefetch.
    if (pendingFrame_ >= 0) {
        const int next = pendingFrame_;
        pendingFrame_ = -1;
        startDecode(next, true);
        return;
    }
    kickNext();
}

void PreviewEngine::kickNext() {
    while (!prefetchQueue_.isEmpty()) {
        const int f = prefetchQueue_.takeFirst();
        if (cache_->contains(f)) continue;
        startDecode(f, false);
        return;
    }
}

void PreviewEngine::cancel() {
    pendingFrame_ = -1;
    prefetchQueue_.clear();
    if (proc_) {
        proc_->disconnect(this);
        if (proc_->state() != QProcess::NotRunning) {
            proc_->kill();
            proc_->waitForFinished(500);
        }
        proc_->deleteLater();
        proc_ = nullptr;
    }
    busy_ = false;
    currentFrame_ = -1;
}

} // namespace wc
