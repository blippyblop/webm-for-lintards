#include "preview/previewengine.h"
#include "preview/framecache.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QStandardPaths>
#include <QStringList>
#include <QTimer>

#include <algorithm>
#include <cstdlib>

namespace wc {

PreviewEngine::PreviewEngine(QObject* parent)
    : QObject(parent), cache_(new FrameCache) {
    // Transient, per-engine cache under the system temp dir. It is deleted when
    // a new input is loaded and when the engine is destroyed (app/dialog close).
    tempRoot_ = QDir::tempPath() + QStringLiteral("/webmforlintards_preview_") +
                QString::number(static_cast<qulonglong>(QCoreApplication::applicationPid())) +
                QLatin1Char('_') +
                QString::number(reinterpret_cast<quintptr>(this), 16);
    QDir(tempRoot_).removeRecursively();
    QDir().mkpath(tempRoot_);

    burstTimer_ = new QTimer(this);
    burstTimer_->setSingleShot(true);
    burstTimer_->setInterval(220);
    connect(burstTimer_, &QTimer::timeout, this, [this]() {
        if (!burstRunning_) startBurst(burstCenter_);
    });
}

PreviewEngine::~PreviewEngine() {
    cancel();
    delete cache_;
    if (!tempRoot_.isEmpty()) QDir(tempRoot_).removeRecursively();
}

void PreviewEngine::setSource(const QString& path, const VideoInfo& info) {
    cancel();
    // New input -> discard the previous file's transient cache.
    if (!cacheRoot_.isEmpty()) QDir(cacheRoot_).removeRecursively();
    if (!burstDir_.isEmpty()) QDir(burstDir_).removeRecursively();
    cacheRoot_.clear();
    burstDir_.clear();

    sourcePath_ = path;
    info_ = info;
    cache_->clear();
    pendingFrame_ = -1;
    expansion_ = 1;

    // Per-source cache inside the transient temp root, keyed by path+size+mtime.
    const QFileInfo fi(path);
    const QString key = QString::fromLatin1(
        QCryptographicHash::hash((path + QLatin1Char('|') + QString::number(fi.size()) +
                                  QLatin1Char('|') + QString::number(fi.lastModified().toSecsSinceEpoch()))
                                     .toUtf8(),
                                 QCryptographicHash::Md5)
            .toHex());
    cacheRoot_ = tempRoot_ + QLatin1Char('/') + key;
    QDir().mkpath(cacheRoot_);
}

void PreviewEngine::clear() {
    cancel();
    if (!cacheRoot_.isEmpty()) QDir(cacheRoot_).removeRecursively();
    if (!burstDir_.isEmpty()) QDir(burstDir_).removeRecursively();
    sourcePath_.clear();
    info_ = VideoInfo{};
    cache_->clear();
    cacheRoot_.clear();
    burstDir_.clear();
}

void PreviewEngine::clearCache() { cache_->clear(); }

void PreviewEngine::setFilterProvider(std::function<QString(double)> fn) {
    filterProvider_ = std::move(fn);
    cache_->clear();
}

QString PreviewEngine::buildVf(double t) const {
    QString vf;
    if (filterProvider_) vf = filterProvider_(t);
    if (info_.width > 1280) {
        if (!vf.isEmpty()) vf += QLatin1Char(',');
        vf += QStringLiteral("scale=1280:-2");
    }
    return vf;
}

QString PreviewEngine::diskPath(const QString& vf, int frame) const {
    if (cacheRoot_.isEmpty()) return QString();
    const QString h = QString::fromLatin1(
        QCryptographicHash::hash(vf.toUtf8(), QCryptographicHash::Md5).toHex());
    return cacheRoot_ + QLatin1Char('/') + h + QLatin1Char('/') + QString::number(frame) +
           QStringLiteral(".jpg");
}

bool PreviewEngine::loadFromDisk(const QString& vf, int frame, QImage& out) const {
    const QString p = diskPath(vf, frame);
    if (p.isEmpty() || !QFile::exists(p)) return false;
    QImage img(p);
    if (img.isNull()) return false;
    out = img;
    return true;
}

void PreviewEngine::saveToDisk(const QString& vf, int frame, const QImage& img) const {
    const QString p = diskPath(vf, frame);
    if (p.isEmpty() || img.isNull()) return;
    QDir().mkpath(QFileInfo(p).absolutePath());
    img.save(p, "JPEG", 90);
}

void PreviewEngine::requestFrame(int frame) {
    if (!hasSource()) return;
    if (frame < 0) frame = 0;

    if (cache_->contains(frame)) {
        emit frameReady(frame, cache_->get(frame));
        scheduleBurst(frame);
        return;
    }

    // Persistent disk cache hit? (index-like: no ffmpeg spawn needed)
    const QString vf = buildVf(info_.timeOfFrame(frame));
    QImage diskImg;
    if (loadFromDisk(vf, frame, diskImg)) {
        cache_->insert(frame, diskImg);
        emit frameReady(frame, diskImg);
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
    if (std::abs(centerFrame - burstCenter_) > 2) expansion_ = 1;
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

    const QString vf = buildVf(info_.timeOfFrame(frame));
    currentVf_ = vf;

    QStringList args;
    args << "-loglevel" << "error"
         << "-ss" << QString::number(info_.timeOfFrame(frame), 'f', 6)
         << "-i" << sourcePath_;
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
    const QString vf = currentVf_;
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
            saveToDisk(vf, decodedFrame, img);
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
    int baseCount = std::min(2 * radius + 1, 80);
    int count = std::min(baseCount * expansion_, 320);
    int start = centerFrame - count / 2;
    start = std::clamp(start, 0, static_cast<int>(std::max<long long>(0, total - count)));
    burstStart_ = start;
    burstCount_ = count;

    burstDir_ = tempRoot_ + QStringLiteral("/burst");
    QDir dir(burstDir_);
    if (!dir.exists()) QDir().mkpath(burstDir_);
    for (const QString& f : dir.entryList({QStringLiteral("f_*.jpg")}, QDir::Files))
        QFile::remove(dir.filePath(f));

    burstVf_ = buildVf(info_.timeOfFrame(start));

    QStringList args;
    args << "-loglevel" << "error"
         << "-ss" << QString::number(info_.timeOfFrame(start), 'f', 6)
         << "-i" << sourcePath_;
    if (!burstVf_.isEmpty()) args << "-vf" << burstVf_;
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
    QString program = ffmpegPath_;
#ifdef Q_OS_UNIX
    program = QStringLiteral("nice");
    args.prepend(ffmpegPath_);
    args.prepend(QStringLiteral("19"));
    args.prepend(QStringLiteral("-n"));
#endif
    burstProc_->setProgram(program);
    burstProc_->setArguments(args);
    burstProc_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(burstProc_, &QProcess::finished, this, &PreviewEngine::onBurstFinished);
    burstRunning_ = true;
    emit indexingActive(true);
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
    emit indexingActive(false);

    QDir dir(burstDir_);
    const QStringList files = dir.entryList({QStringLiteral("f_*.jpg")}, QDir::Files, QDir::Name);
    for (int i = 0; i < files.size() && i < burstCount_; ++i) {
        const int frame = burstStart_ + i;
        QImage img(dir.filePath(files[i]));
        if (!img.isNull()) {
            if (!cache_->contains(frame)) cache_->insert(frame, img);
            saveToDisk(burstVf_, frame, img);
        }
        QFile::remove(dir.filePath(files[i]));
    }
    for (const QString& f : dir.entryList({QStringLiteral("f_*.jpg")}, QDir::Files))
        QFile::remove(dir.filePath(f));

    // Keep widening the background window while the user sits still (~indexing
    // outward from the cursor). Reset happens on the next distant seek.
    if (!busy_ && pendingFrame_ < 0 && burstCenter_ == currentFrame_ && expansion_ < 4) {
        ++expansion_;
        if (burstTimer_) burstTimer_->start(120);
    }
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
