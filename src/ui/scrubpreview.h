#pragma once
// Reusable scrubbing surface (preview + timeline) embedded by time-based
// filter dialogs so every time selection can be made by scrubbing.

#include <QWidget>
#include <QImage>
#include <QString>

#include "core/videoinfo.h"
#include "core/filtermodel.h"

class QLabel;

namespace wc {

class PreviewCanvas;
class TimelineWidget;
class PreviewEngine;

class ScrubPreview : public QWidget {
    Q_OBJECT
public:
    explicit ScrubPreview(QWidget* parent = nullptr);
    ~ScrubPreview() override;

    void setSource(const QString& path, const VideoInfo& info, const QString& ffmpegPath = QString());
    void setDuration(double seconds);
    double currentTime() const { return current_; }
    int currentFrame() const { return info_.frameAt(current_); }
    double duration() const { return duration_; }
    void gotoTime(double seconds);
    void gotoFrame(int frame) { gotoTime(info_.timeOfFrame(frame)); }

    // Provide the filter state so scrub frames render with filters applied.
    void setFilters(const FilterModel* model);

signals:
    void timeChanged(double seconds);

private slots:
    void onSeek(double seconds);
    void onStep(int delta);
    void onFrame(int frame, const QImage& image);

private:
    PreviewCanvas* canvas_ = nullptr;
    TimelineWidget* timeline_ = nullptr;
    QLabel* label_ = nullptr;
    PreviewEngine* preview_ = nullptr;
    VideoInfo info_;
    const FilterModel* model_ = nullptr;
    double current_ = 0.0;
    double duration_ = 0.0;
};

} // namespace wc
