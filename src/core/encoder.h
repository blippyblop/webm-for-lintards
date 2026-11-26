#pragma once
// Sequential ffmpeg runner for one or more PassPlans (two-pass aware).
// CONTRACT: implementation lives in encoder.cpp.

#include "core/commandbuilder.h"

#include <QObject>
#include <QProcess>
#include <QString>

#include <vector>

namespace wc {

class Encoder : public QObject {
    Q_OBJECT
public:
    explicit Encoder(QObject* parent = nullptr);
    ~Encoder() override;

    void setFfmpegPath(const QString& path);
    QString ffmpegPath() const { return ffmpegPath_; }

    // Run passes sequentially. durationSeconds is used for progress percent.
    void start(const std::vector<PassPlan>& passes, double durationSeconds);
    void cancel();
    bool isRunning() const { return running_; }

signals:
    void logLine(const QString& line);
    void progress(qint64 frame, double timeSeconds, double percent, double fps, double speed);
    void finished(bool success, const QString& errorString);

private slots:
    void onStdErr();
    void onProcFinished(int exitCode, QProcess::ExitStatus status);

private:
    void launchPass();
    void cleanupProcess();
    void cleanupArtifacts();   // remove two-pass logs / vid.stab transform file

    QString ffmpegPath_ = QStringLiteral("ffmpeg");
    QProcess* proc_ = nullptr;
    std::vector<PassPlan> passes_;
    int currentPass_ = 0;
    bool running_ = false;
    bool cancelled_ = false;
    QString lastError_;
    double duration_ = 0.0;
    // ProgressParser lives here; declared in cpp to keep this header light.
    struct ParserHolder;
    ParserHolder* parser_ = nullptr;
};

} // namespace wc
