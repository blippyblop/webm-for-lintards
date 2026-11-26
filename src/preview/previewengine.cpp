#include "preview/previewengine.h"
#include "preview/framecache.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStringList>
#include <QTimer>

#include <algorithm>

namespace wc {

PreviewEngine::PreviewEngine(QObject* parent)
    : QObject(parent), cache_(new FrameCache) {
    burstTimer_ = new QTimer(this);
    burstTimer_->setSingleShot(true);
    burstTimer_->setInterval(220);   // debounce rapid scrubbing
    connect(burstTimer_, &QTimer::timeout, this, [this]() {
        if (!burstRunning_) startBurst(burstCenter_);
    });
}

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

void PreviewEngine::clearCache() { cache_->clear(); }

void PreviewEngine::setFilterProvider(std::function<QString(double)> fn) {
    filterProvider_ = std::move(fn);
    cache_->clear();
}

void PreviewEngine::requestFrame(int frame) {
    if (!hasSource()) return;
    if (frame < 0) frame = 0;

    if (cache_->contains(frame)) {
        emit frameReady(frame, cache_->get(frame));
        scheduleBurst(frame);
        return;
    }
    scheduleBurst(frame);
    if (busy_) {
        pendingFrame_ = frame;
        return;
    }
    startDecode(frame);
}

void PreviewEngine::scheduleBurst(int centerFrame) {
    burstCenter_ = centerFrame;
    if (burstTimer_ && !burstRunning_) burstTimer_->start();
}

void PreviewEngine::startDecode(int frame) {
    busy_ = true;
    currentFrame_ = frame;

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
            emit frameReady(decodedFrame, img);
        }
    } else if (!data.isEmpty() && exitCode != 0) {
        emit error(tr("Failed to decode preview frame %1").arg(decodedFrame));
    }

    if (pendingFrame_ >= 0) {
        const int next = pendingFrame_;
        pendingFrame_ = -1;
        startDecode(next);
    }
}

void PreviewEngine::startBurst(int centerFrame) {
    if (!hasSource() || burstRunning_) return;
    const long long total = info_.frameCount();
    if (total <= 1) return;

    const int radius = static_cast<int>(std::max<long long>(1, total / 20));  // ~5%
    int count = std::min(2 * radius + 1, 80);                                 // batch cap
    if (count < 2) return;
    int start = centerFrame - count / 2;
    start = std::clamp(start, 0, static_cast<int>(std::max<long long>(0, total - count)));
    burstStart_ = start;
    burstCount_ = count;

    burstDir_ = QDir::tempPath() + QStringLiteral("/wc_preview_") +
                QString::number(static_cast<qulonglong>(QCoreApplication::applicationPid()));
    QDir dir(burstDir_);
    if (!dir.exists()) QDir().mkpath(burstDir_);
    for (const QString& f : dir.entryList({QStringLiteral("f_*.jpg")}, QDir::Files))
        QFile::remove(dir.filePath(f));

    QStringList args;
    args << "-loglevel" << "error"
         << "-ss" << QString::number(info_.timeOfFrame(start), 'f', 6)
         << "-i" << sourcePath_;
    QString vf;
    if (filterProvider_) vf = filterProvider_(info_.timeOfFrame(start));
    if (info_.width > 1280) {
        if (!vf.isEmpty()) vf += QLatin1Char(',');
        vf += QStringLiteral("scale=1280:-2");
    }
    if (!vf.isEmpty()) args << "-vf" << vf;
    args << "-frames:v" << QString::number(count)
         << "-f" << "image2"
         << "-vcodec" << "mjpeg"
         << "-q:v" << "5"
         << "-start_number" << "0"
         << dir.filePath(QStringLiteral("f_%05d.jpg"));

    if (burstProc_) {
        burstProc_->disconnect(this);
        burstProc_->deleteLater();
    }
    burstProc_ = new QProcess(this);
    burstProc_->setProgram(ffmpegPath_);
    burstProc_->setArguments(args);
    burstProc_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(burstProc_, &QProcess::finished, this, &PreviewEngine::onBurstFinished);
    burstRunning_ = true;
    burstProc_->start();
}

void PreviewEngine::onBurstFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
    if (burstProc_) {
        burstProc_->disconnect(this);
        burstProc_->deleteLater();
        burstProc_ = nullptr;
    }
    burstRunning_ = false;

    QDir dir(burstDir_);
    QStringList files = dir.entryList({QStringLiteral("f_*.jpg")}, QDir::Files, QDir::Name);
    for (int i = 0; i < files.size() && i < burstCount_; ++i) {
        const int frame = burstStart_ + i;
        if (cache_->contains(frame)) continue;
        QImage img(dir.filePath(files[i]));
        if (!img.isNull()) cache_->insert(frame, img);
        QFile::remove(dir.filePath(files[i]));
    }
    // remove any leftovers
    for (const QString& f : dir.entryList({QStringLiteral("f_*.jpg")}, QDir::Files))
        QFile::remove(dir.filePath(f));
}

void PreviewEngine::cancel() {
    pendingFrame_ = -1;
    if (burstTimer_) burstTimer_->stop();
    if (proc_) {
        proc_->disconnect(this);
        if (proc_->state() != QProcess::NotRunning) {
            proc_->kill();
            proc_->waitForFinished(500);
        }
        proc_->deleteLater();
        proc_ = nullptr;
    }
    if (burstProc_) {
        burstProc_->disconnect(this);
        if (burstProc_->state() != QProcess::NotRunning) {
            burstProc_->kill();
            burstProc_->waitForFinished(500);
        }
        burstProc_->deleteLater();
        burstProc_ = nullptr;
    }
    burstRunning_ = false;
    busy_ = false;
    currentFrame_ = -1;
}

} // namespace wc
