#include "ui/scrubpreview.h"

#include "ui/previewcanvas.h"
#include "ui/timelinewidget.h"
#include "preview/previewengine.h"
#include "core/previewfilter.h"

#include <QVBoxLayout>
#include <QLabel>

#include <algorithm>

namespace wc {

ScrubPreview::ScrubPreview(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    canvas_ = new PreviewCanvas(this);
    canvas_->setCropEditable(false);
    canvas_->setMinimumHeight(220);
    timeline_ = new TimelineWidget(this);
    label_ = new QLabel(tr("0.00 s"), this);

    layout->addWidget(canvas_, 1);
    layout->addWidget(timeline_);
    layout->addWidget(label_);

    preview_ = new PreviewEngine(this);
    connect(preview_, &PreviewEngine::frameReady, this, &ScrubPreview::onFrame);
    connect(timeline_, &TimelineWidget::seekRequested, this, &ScrubPreview::onSeek);
    connect(timeline_, &TimelineWidget::frameStepRequested, this, &ScrubPreview::onStep);
}

ScrubPreview::~ScrubPreview() {
    if (preview_) preview_->cancel();
}

void ScrubPreview::setSource(const QString& path, const VideoInfo& info, const QString& ffmpegPath) {
    info_ = info;
    duration_ = info.durationSeconds;
    timeline_->setDuration(duration_);
    timeline_->setFrameRate(info.frameRateValue() > 0.0 ? info.frameRateValue() : 30.0);
    if (!path.isEmpty()) {
        preview_->setFfmpegPath(ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : ffmpegPath);
        preview_->setSource(path, info);
    }
    gotoTime(0.0);
}

void ScrubPreview::setFilters(const FilterModel* model) {
    model_ = model;
    if (preview_) {
        preview_->setFilterProvider([this](double t) -> QString {
            if (!model_) return QString();
            return QString::fromStdString(buildPreviewVideoFilter(*model_, info_, t));
        });
    }
    gotoTime(current_);
}

void ScrubPreview::setDuration(double seconds) {
    duration_ = std::max(0.0, seconds);
    timeline_->setDuration(duration_);
}

void ScrubPreview::gotoTime(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    if (duration_ > 0.0 && seconds > duration_) seconds = duration_;
    current_ = seconds;
    timeline_->setCurrentTime(seconds);
    const double fps = info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0;
    const int frame = info_.frameAt(seconds);
    label_->setText(tr("%1 s  (frame %2 @ %3 fps)")
                        .arg(seconds, 0, 'f', 2)
                        .arg(frame)
                        .arg(fps, 0, 'f', 2));
    if (preview_ && preview_->hasSource())
        preview_->requestFrame(frame);
}

void ScrubPreview::onSeek(double seconds) {
    gotoTime(seconds);
    emit timeChanged(current_);
}

void ScrubPreview::onStep(int delta) {
    const double fps = info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0;
    gotoTime(current_ + delta / fps);
    emit timeChanged(current_);
}

void ScrubPreview::onFrame(int frame, const QImage& image) {
    Q_UNUSED(frame);
    if (canvas_) canvas_->setImage(image);
}

} // namespace wc
