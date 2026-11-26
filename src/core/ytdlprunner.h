#pragma once
// Wraps a yt-dlp subprocess for the Download tab.
// CONTRACT: implementation lives in ytdlprunner.cpp.

#include <QObject>
#include <QString>
#include <QProcess>

namespace wc {

class YtdlpRunner : public QObject {
    Q_OBJECT
public:
    explicit YtdlpRunner(QObject* parent = nullptr);
    ~YtdlpRunner() override;

    // True if a working yt-dlp binary is found on PATH.
    static bool isAvailable();

    void setBinary(const QString& path);
    QString binary() const { return binary_; }

    // Download url into outputDir. If format is empty, best quality is used.
    // Supports the "URL@*start-end" clipping trick (e.g. "...@*5:35-5:45").
    void download(const QString& url, const QString& outputDir, const QString& format = QString());
    void cancel();
    bool isRunning() const { return running_; }

signals:
    void logLine(const QString& line);
    void progress(double percent, const QString& speed, const QString& eta);
    void finished(bool success, const QString& outputPath, const QString& errorString);

private slots:
    void onReadyRead();
    void onProcFinished(int exitCode, QProcess::ExitStatus status);

private:
    QString binary_ = QStringLiteral("yt-dlp");
    QProcess* proc_ = nullptr;
    bool running_ = false;
    bool cancelled_ = false;
    QString outputDir_;
    QString lastPath_;
    QString tailBuffer_;
};

} // namespace wc
