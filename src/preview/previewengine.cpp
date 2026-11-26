#include "preview/previewengine.h"
#include "preview/framecache.h"

#include <QByteArray>

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
    pendingFrame_ = -1;
}

void PreviewEngine::clear() {
    cancel();
    sourcePath_.clear();
    info_ = VideoInfo{};
    cache_->clear();
}

void PreviewEngine::requestFrame(int frame) {
    if (!hasSource()) return;
    if (frame < 0) frame = 0;
    if (cache_->contains(frame)) {
        emit frameReady(frame, cache_->get(frame));
        return;
    }
    if (busy_) {
        pendingFrame_ = frame;   // coalesce: keep only the latest
        return;
    }
    startDecode(frame);
}

void PreviewEngine::startDecode(int frame) {
    busy_ = true;
    currentFrame_ = frame;

    if (proc_) { proc_->disconnect(this); proc_->deleteLater(); }
    proc_ = new QProcess(this);
    proc_->setProgram(ffmpegPath_);

    const QString ss = QString::number(info_.timeOfFrame(frame), 'f', 6);
    QStringList args;
    args << "-ss" << ss
         << "-i" << sourcePath_
         << "-frames:v" << "1"
         << "-f" << "image2pipe"
         << "-vcodec" << "png"
         << "-";
    proc_->setArguments(args);
    proc_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(proc_, &QProcess::finished, this, &PreviewEngine::onProcFinished);
    proc_->start();
}

void PreviewEngine::onProcFinished(int exitCode, QProcess::ExitStatus status) {
    QByteArray data;
    if (proc_) data = proc_->readAllStandardOutput();

    busy_ = false;
    const int decodedFrame = currentFrame_;

    if (status == QProcess::NormalExit && exitCode == 0 && !data.isEmpty()) {
        QImage img = QImage::fromData(data, "PNG");
        if (!img.isNull()) {
            cache_->insert(decodedFrame, img);
            emit frameReady(decodedFrame, img);
        }
    } else if (!data.isEmpty()) {
        emit error(tr("Failed to decode preview frame %1").arg(decodedFrame));
    }

    if (pendingFrame_ >= 0) {
        const int next = pendingFrame_;
        pendingFrame_ = -1;
        startDecode(next);
    }
}

void PreviewEngine::cancel() {
    pendingFrame_ = -1;
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
