#pragma once
// Scrubbing trim popup: preview + timeline (with filter preview), frame-based
// start/end, "Set Start"/"Set End", and a Play/Stop preview of the trim.

#include <QDialog>
#include <QImage>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QTimer;

namespace wc {

class PreviewCanvas;
class TimelineWidget;
class PreviewEngine;

class TrimDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr,
                        const QString& inputPath = QString(), const QString& ffmpegPath = QString());
    ~TrimDialog() override;

    void accept() override;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSeek(double seconds);
    void onFrameStep(int delta);
    void onFrameReady(int frame, const QImage& image);
    void setStartHere();
    void setEndHere();
    void syncFromSpins();
    void togglePlay();
    void playTick();

private:
    void gotoFrame(int frame);
    void updateRangeUi();
    int maxFrame() const;

    FilterModel& model_;
    const VideoInfo& info_;
    QString inputPath_;

    PreviewEngine* preview_ = nullptr;
    PreviewCanvas* canvas_ = nullptr;
    TimelineWidget* timeline_ = nullptr;
    QSpinBox* startFrame_ = nullptr;
    QSpinBox* endFrame_ = nullptr;
    QCheckBox* wholeClip_ = nullptr;
    QLabel* currentLabel_ = nullptr;
    QLabel* durationLabel_ = nullptr;
    QPushButton* playBtn_ = nullptr;
    QTimer* playTimer_ = nullptr;

    bool playing_ = false;
    int savedFrame_ = 0;
    int currentFrameIndex_ = 0;
};

} // namespace wc
