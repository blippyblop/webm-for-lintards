#include "core/encoder.h"

#include "core/progressparser.h"

#include <QByteArray>
#include <QProcess>
#include <QSignalBlocker>
#include <QString>
#include <QStringList>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <string>
#include <vector>

namespace wc {

namespace {

// Split raw stderr bytes into complete lines. ffmpeg terminates ordinary log
// lines with '\n' and in-place progress updates with '\r'. An incomplete
// trailing fragment is kept in `carry` until the next read so that a token
// such as "frame=  12" is never fed to the parser in two pieces.
QStringList takeCompleteLines(QByteArray& carry, const QByteArray& chunk) {
    carry.append(chunk);

    QStringList lines;
    int start = 0;
    const int n = static_cast<int>(carry.size());
    for (int i = 0; i < n; ++i) {
        const char c = carry.at(i);
        if (c != '\n' && c != '\r')
            continue;

        if (i > start) {
            const QString line =
                QString::fromLocal8Bit(carry.constData() + start, i - start).trimmed();
            if (!line.isEmpty())
                lines << line;
        }
        start = i + 1;
    }

    if (start > 0)
        carry.remove(0, start);

    // Safety valve: never let a delimiter-less stream grow without bound.
    if (carry.size() > 1024 * 1024) {
        const QString line = QString::fromLocal8Bit(carry).trimmed();
        if (!line.isEmpty())
            lines << line;
        carry.clear();
    }

    return lines;
}

} // namespace

// Defined here (instead of the header) to keep encoder.h free of the
// progress-parser dependency. Owns the parser plus the partial-line buffer
// that cannot be added to the fixed header member list.
struct Encoder::ParserHolder {
    ProgressParser p;
    QByteArray carry;
};

Encoder::Encoder(QObject* parent)
    : QObject(parent)
{
    parser_ = new ParserHolder();
}

Encoder::~Encoder()
{
    cleanupProcess();
    delete parser_;
    parser_ = nullptr;
}

void Encoder::setFfmpegPath(const QString& path)
{
    ffmpegPath_ = path;
}

void Encoder::start(const std::vector<PassPlan>& passes, double durationSeconds)
{
    if (running_)
        return;

    passes_ = passes;
    duration_ = durationSeconds;

    if (!parser_)
        parser_ = new ParserHolder();
    parser_->p.reset();
    parser_->p.setDuration(duration_);
    parser_->carry.clear();

    currentPass_ = 0;
    cancelled_ = false;
    running_ = true;
    lastError_.clear();

    launchPass();
}

void Encoder::cancel()
{
    cancelled_ = true;
    if (proc_ && running_)
        proc_->kill();
}

void Encoder::cleanupProcess()
{
    if (!proc_)
        return;

    disconnect(proc_, nullptr, this, nullptr);

    if (proc_->state() != QProcess::NotRunning) {
        proc_->kill();
        proc_->waitForFinished(2000);
    }

    proc_->deleteLater();
    proc_ = nullptr;

    passes_.clear();
    if (parser_)
        parser_->carry.clear();
}

void Encoder::launchPass()
{
    if (currentPass_ >= static_cast<int>(passes_.size())) {
        cleanupArtifacts();
        running_ = false;
        emit finished(true, QString());
        return;
    }

    if (!proc_) {
        proc_ = new QProcess(this);
        proc_->setProcessChannelMode(QProcess::MergedChannels);

        connect(proc_, &QProcess::readyRead, this, &Encoder::onStdErr);
        connect(proc_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                this, &Encoder::onProcFinished);

        // A failed exec() never emits finished(); report it ourselves so the
        // Encoder does not stay stuck in the running state.
        connect(proc_, &QProcess::errorOccurred, this,
                [this](QProcess::ProcessError error) {
                    if (error != QProcess::FailedToStart || !running_)
                        return;
                    lastError_ = QStringLiteral("Failed to start ffmpeg: %1").arg(ffmpegPath_);
                    running_ = false;
                    emit logLine(lastError_);
                    emit finished(false, lastError_);
                });
    }

    // Normally NotRunning here (passes are sequential), but be defensive and
    // block signals while replacing a stale process to avoid a re-entrant
    // finished() advancing currentPass_.
    if (proc_->state() != QProcess::NotRunning) {
        QSignalBlocker blocker(proc_);
        proc_->kill();
        proc_->waitForFinished(2000);
    }
    proc_->readAll();   // drop anything left over from a previous pass
    if (parser_)
        parser_->carry.clear();

    const PassPlan& plan = passes_[currentPass_];

    QStringList args;
    args.reserve(static_cast<int>(plan.args.size()));
    for (const std::string& arg : plan.args)
        args << QString::fromStdString(arg);

    QString program = ffmpegPath_;
#ifdef Q_OS_UNIX
    // Run the encoder at low priority so it doesn't choke the desktop:
    // nice -n 19 ffmpeg <args...>
    program = QStringLiteral("nice");
    args.prepend(ffmpegPath_);
    args.prepend(QStringLiteral("19"));
    args.prepend(QStringLiteral("-n"));
#endif
    proc_->setProgram(program);
    proc_->setArguments(args);
    proc_->setProcessChannelMode(QProcess::MergedChannels);
    proc_->start();
}

void Encoder::cleanupArtifacts()
{
    // Delete any two-pass log files referenced by the pass plans, plus the
    // vid.stab transform file, so encoding leaves no stray *.log files behind.
    QSet<QString> bases;
    bool usedVidstab = false;
    for (const PassPlan& plan : passes_) {
        for (std::size_t i = 0; i + 1 < plan.args.size(); ++i) {
            if (plan.args[i] == "-passlogfile")
                bases.insert(QString::fromStdString(plan.args[i + 1]));
            if (plan.args[i].find("vidstabdetect") != std::string::npos ||
                plan.args[i].find("vidstabtransform") != std::string::npos)
                usedVidstab = true;
        }
    }
    for (const QString& base : bases) {
        QFile::remove(base);
        const QFileInfo fi(base);
        const QDir dir = fi.absoluteDir();
        const QString prefix = fi.fileName();
        // Literal prefix matching (no glob metacharacters) and only ffmpeg's
        // own "-N.log" style suffixes, so a file name containing '*'/'?' cannot
        // cause deletion of unrelated files.
        const QFileInfoList entries = dir.entryInfoList(QDir::Files);
        for (const QFileInfo& e : entries) {
            const QString name = e.fileName();
            if (!name.startsWith(prefix)) continue;
            const QString rest = name.mid(prefix.size());
            if (rest.isEmpty() || rest == QStringLiteral(".log") || rest.startsWith(QLatin1Char('-')))
                QFile::remove(e.absoluteFilePath());
        }
    }
    if (usedVidstab)
        QFile::remove(QStringLiteral(".webmforlintards_transforms.trf"));
}

void Encoder::onStdErr()
{
    if (!proc_ || !parser_)
        return;

    const QByteArray chunk = proc_->readAll();
    if (chunk.isEmpty())
        return;

    const QStringList lines = takeCompleteLines(parser_->carry, chunk);
    for (const QString& line : lines) {
        emit logLine(line);

        if (parser_->p.feed(line.toStdString())) {
            const EncodeProgress& ep = parser_->p.last();
            emit progress(static_cast<qint64>(ep.frame),
                          ep.timeSeconds,
                          ep.percent,
                          ep.fps,
                          ep.speed);
        }
    }
}

void Encoder::onProcFinished(int exitCode, QProcess::ExitStatus status)
{
    // Ignore signals from a QProcess instance that has already been replaced.
    if (sender() && sender() != proc_)
        return;

    // Drain whatever ffmpeg wrote between the last readyRead and exiting.
    QStringList lines;
    if (proc_ && parser_) {
        lines = takeCompleteLines(parser_->carry, proc_->readAll());
        if (!parser_->carry.trimmed().isEmpty()) {
            lines << QString::fromLocal8Bit(parser_->carry).trimmed();
            parser_->carry.clear();
        }
    }

    QString lastLine;
    for (const QString& line : lines) {
        if (line.isEmpty())
            continue;
        emit logLine(line);
        lastLine = line;
    }

    if (cancelled_) {
        cleanupArtifacts();
        running_ = false;
        emit finished(false, QStringLiteral("Cancelled"));
        return;
    }

    if (exitCode != 0 || status == QProcess::CrashExit) {
        cleanupArtifacts();
        running_ = false;

        if (!lastLine.isEmpty())
            lastError_ = lastLine;
        if (lastError_.isEmpty()) {
            lastError_ = QStringLiteral("ffmpeg failed (exit code %1)").arg(exitCode);
        } else if (status == QProcess::CrashExit) {
            lastError_ = QStringLiteral("ffmpeg crashed: %1").arg(lastLine);
        }
        if (lastError_.size() > 800)
            lastError_ = lastError_.left(800);

        emit finished(false, lastError_);
        return;
    }

    ++currentPass_;
    launchPass();
}

} // namespace wc
