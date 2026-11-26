#pragma once
// Scrubbing trim popup: preview + timeline, "Set Start"/"Set End" buttons,
// duration readout. Mirrors the original WebMConverter Trim window.

#include <QDialog>
#include <QImage>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QDoubleSpinBox;
class QCheckBox;
class QLabel;

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

private slots:
    void onSeek(double seconds);
    void onFrameStep(int delta);
    void onFrameReady(int frame, const QImage& image);
    void setStartHere();
    void setEndHere();
    void syncFromSpins();

private:
    void gotoTime(double seconds);
    void updateRangeUi();

    FilterModel& model_;
    const VideoInfo& info_;
    QString inputPath_;

    PreviewEngine* preview_ = nullptr;
    PreviewCanvas* canvas_ = nullptr;
    TimelineWidget* timeline_ = nullptr;
    QDoubleSpinBox* startSpin_ = nullptr;
    QDoubleSpinBox* endSpin_ = nullptr;
    QCheckBox* wholeClip_ = nullptr;
    QLabel* currentLabel_ = nullptr;
    QLabel* durationLabel_ = nullptr;

    double currentTime_ = 0.0;
};

} // namespace wc
