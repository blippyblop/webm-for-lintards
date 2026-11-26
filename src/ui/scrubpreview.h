#pragma once
// Reusable scrubbing surface (preview + timeline) for filter dialogs. Supports
// live filter preview, interactive crop overlay, before/after split, and
// window-wide scrubbing hotkeys.

#include <QWidget>
#include <QImage>
#include <QString>
#include <QSize>
#include <QRect>

#include <functional>

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

    // Restrict scrubbing to the trimmed span (or multi-trim bounds).
    void setLimits(const FilterModel& filters);

    // Preview against an external (applied) model.
    void setFilters(const FilterModel* model);
    // Preview against a dialog-owned copy that the dialog can mutate live.
    void setEditableModel(const FilterModel& initial);
    FilterModel& previewModel() { return previewModel_; }
    void refreshPreview();

    QSize imageSize() const { return lastImageSize_; }

    // Interactive crop overlay.
    void setCropEditable(bool editable);
    void setCropRect(const QRect& rect);
    QRect cropRect() const;

    // Before/after split (raw left, effect right) with a centre divider.
    void setBeforeAfter(bool on, std::function<QImage(const QImage&)> effect = nullptr);

signals:
    void timeChanged(double seconds);
    void cropChanged(const QRect& rect);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void onSeek(double seconds);
    void onStep(int delta);
    void onFrame(int frame, const QImage& image);

private:
    void setupProvider();
    const FilterModel& effectiveModel() const;

    PreviewCanvas* canvas_ = nullptr;
    TimelineWidget* timeline_ = nullptr;
    QLabel* label_ = nullptr;
    PreviewEngine* preview_ = nullptr;

    VideoInfo info_;
    QString sourcePath_;
    QString ffmpegPath_ = QStringLiteral("ffmpeg");

    const FilterModel* external_ = nullptr;
    FilterModel previewModel_;

    double current_ = 0.0;
    double duration_ = 0.0;
    double rangeStart_ = 0.0;
    double rangeEnd_ = 0.0;
    bool hasRange_ = false;
    QSize lastImageSize_;

    bool beforeAfter_ = false;
    std::function<QImage(const QImage&)> effect_;
};

} // namespace wc
