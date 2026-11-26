#include "core/ytdlprunner.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

namespace wc {

namespace {

// yt-dlp progress lines look like:
//   [download]  12.3% of 100.00MiB at 1.23MiB/s ETA 00:34
// Best-effort extraction of the transfer speed token (".../s").
QString extractSpeed(const QString& line)
{
    const qsizetype at = line.indexOf(QStringLiteral(" at "));
    if (at < 0)
        return QString();

    const QStringList tokens = line.mid(at + 4).trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < tokens.size() && i < 3; ++i) {
        if (tokens.at(i).contains(QStringLiteral("/s")))
            return tokens.at(i);
    }
    return QString();
}

// Best-effort extraction of the token following "ETA".
QString extractEta(const QString& line)
{
    const qsizetype eta = line.indexOf(QStringLiteral("ETA"));
    if (eta < 0)
        return QString();

    const QString rest = line.mid(eta + 3).trimmed();
    if (rest.isEmpty())
        return QString();
    return rest.section(QLatin1Char(' '), 0, 0);
}

// Quote an argument for a human-readable command summary.
QString quoteArg(const QString& arg)
{
    if (arg.contains(QLatin1Char(' ')) || arg.contains(QLatin1Char('"'))
        || arg.contains(QLatin1Char('\'')) || arg.isEmpty()) {
        QString escaped = arg;
        escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        return QStringLiteral("\"%1\"").arg(escaped);
    }
    return arg;
}

} // namespace

YtdlpRunner::YtdlpRunner(QObject* parent)
    : QObject(parent)
{
}

YtdlpRunner::~YtdlpRunner()
{
    if (proc_) {
        proc_->disconnect(this);
        if (proc_->state() != QProcess::NotRunning) {
            proc_->kill();
            proc_->waitForFinished(1000);
        }
        delete proc_;
        proc_ = nullptr;
    }
}

bool YtdlpRunner::isAvailable()
{
    return !QStandardPaths::findExecutable(QStringLiteral("yt-dlp")).isEmpty();
}

void YtdlpRunner::setBinary(const QString& path)
{
    binary_ = path;
}

void YtdlpRunner::download(const QString& url, const QString& outputDir, const QString& format)
{
    if (running_)
        return;

    outputDir_ = outputDir;
    cancelled_ = false;
    lastPath_.clear();
    tailBuffer_.clear();

    // Clipping convention: "<url>@*<start>-<end>" e.g. "https://...@*5:35-5:45".
    QString base = url;
    QString section;
    const qsizetype marker = url.indexOf(QStringLiteral("@*"));
    if (marker >= 0) {
        base = url.left(marker);
        section = url.mid(marker + 2);
    }

    // Normalize the output directory (strip trailing slashes, keep "/").
    QString dir = outputDir;
    while (dir.size() > 1 && dir.endsWith(QLatin1Char('/')))
        dir.chop(1);
    if (dir.isEmpty())
        dir = QStringLiteral(".");

    QStringList args;
    args << QStringLiteral("--newline");
    args << QStringLiteral("-o")
         << (dir + QStringLiteral("/%(title)s.%(ext)s"));
    if (!section.isEmpty())
        args << QStringLiteral("--download-sections") << (QStringLiteral("*") + section);
    if (!format.isEmpty())
        args << QStringLiteral("-f") << format;
    args << base;

    // Re-create the process object for every run.
    if (proc_) {
        proc_->disconnect(this);
        proc_->deleteLater();
        proc_ = nullptr;
    }

    proc_ = new QProcess(this);
    proc_->setProgram(binary_);
    proc_->setArguments(args);
    proc_->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc_, &QProcess::readyRead, this, &YtdlpRunner::onReadyRead);
    connect(proc_, &QProcess::finished, this, &YtdlpRunner::onProcFinished);

    // Handle "binary not found / not executable" reliably: the failed-start
    // error is emitted before (and sometimes instead of) finished().
    QProcess* const proc = proc_;
    connect(proc_, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart || proc != proc_)
                    return;
                running_ = false;
                proc_ = nullptr;
                proc->deleteLater();
                emit finished(false, QString(), QStringLiteral("failed to start yt-dlp"));
            });

    running_ = true;

    QString summary = binary_;
    for (const QString& arg : args) {
        summary += QLatin1Char(' ');
        summary += quoteArg(arg);
    }
    emit logLine(summary);

    proc_->start();
}

void YtdlpRunner::cancel()
{
    cancelled_ = true;
    if (proc_ && running_)
        proc_->kill();
}

void YtdlpRunner::onReadyRead()
{
    QProcess* const p = qobject_cast<QProcess*>(sender());
    if (!p || p != proc_)
        return;

    tailBuffer_ += QString::fromUtf8(p->readAll());

    qsizetype start = 0;
    for (qsizetype i = 0; i < tailBuffer_.size(); ++i) {
        const QChar ch = tailBuffer_.at(i);
        if (ch != QLatin1Char('\n') && ch != QLatin1Char('\r'))
            continue;

        const QString line = tailBuffer_.mid(start, i - start);
        start = i + 1;

        if (line.trimmed().isEmpty())
            continue;

        emit logLine(line);

        // Destination tracking.
        static const QString destKey = QStringLiteral("Destination:");
        const qsizetype dest = line.indexOf(destKey);
        if (dest >= 0) {
            const QString path = line.mid(dest + destKey.size()).trimmed();
            if (!path.isEmpty())
                lastPath_ = path;
        } else {
            static const QRegularExpression alreadyRe(
                QStringLiteral("\\[download\\]\\s+(.+?)\\s+has already been downloaded"));
            const QRegularExpressionMatch m = alreadyRe.match(line);
            if (m.hasMatch()) {
                const QString path = m.captured(1).trimmed();
                if (!path.isEmpty())
                    lastPath_ = path;
            }
        }

        // Progress tracking.
        if (line.contains(QStringLiteral("[download]")) && line.contains(QLatin1Char('%'))) {
            static const QRegularExpression pctRe(
                QStringLiteral("([0-9]+(?:\\.[0-9]+)?)%"));
            const QRegularExpressionMatch pm = pctRe.match(line);
            if (pm.hasMatch())
                emit progress(pm.captured(1).toDouble(), extractSpeed(line), extractEta(line));
        }
    }

    tailBuffer_ = tailBuffer_.mid(start);
}

void YtdlpRunner::onProcFinished(int exitCode, QProcess::ExitStatus status)
{
    QProcess* const p = qobject_cast<QProcess*>(sender());
    if (!proc_ || (p && p != proc_))
        return; // already handled (e.g. failed start) or stale sender

    running_ = false;

    const bool failedToStart = (proc_->error() == QProcess::FailedToStart);
    const QString path = lastPath_;

    proc_->disconnect(this);
    proc_->deleteLater();
    proc_ = nullptr;

    if (failedToStart) {
        emit finished(false, QString(), QStringLiteral("failed to start yt-dlp"));
        return;
    }

    if (cancelled_) {
        emit finished(false, QString(), QStringLiteral("Cancelled"));
        return;
    }

    if (exitCode == 0 && status == QProcess::NormalExit)
        emit finished(true, path, QString());
    else
        emit finished(false, path, QStringLiteral("yt-dlp failed (exit %1)").arg(exitCode));
}

} // namespace wc
