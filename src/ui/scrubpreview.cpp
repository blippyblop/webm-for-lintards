#include "ui/scrubpreview.h"

#include "ui/previewcanvas.h"
#include "ui/timelinewidget.h"
#include "preview/previewengine.h"
#include "core/previewfilter.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QApplication>
#include <QAbstractSpinBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QKeyEvent>

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
    connect(canvas_, &PreviewCanvas::cropChanged, this, &ScrubPreview::cropChanged);

    // Window-wide scrubbing hotkeys while this dialog is active.
    qApp->installEventFilter(this);
}

ScrubPreview::~ScrubPreview() {
    if (qApp) qApp->removeEventFilter(this);
    if (preview_) preview_->cancel();
}

void ScrubPreview::setupProvider() {
    if (!preview_) return;
    preview_->setFilterProvider([this](double t) -> QString {
        if (beforeAfter_) return QString();
        return QString::fromStdString(buildPreviewVideoFilter(effectiveModel(), info_, t));
    });
}

const FilterModel& ScrubPreview::effectiveModel() const {
    return external_ ? *external_ : previewModel_;
}

void ScrubPreview::setSource(const QString& path, const VideoInfo& info, const QString& ffmpegPath) {
    info_ = info;
    sourcePath_ = path;
    if (!ffmpegPath.isEmpty()) ffmpegPath_ = ffmpegPath;
    duration_ = info.durationSeconds;
    timeline_->setDuration(duration_);
    timeline_->setFrameRate(info.frameRateValue() > 0.0 ? info.frameRateValue() : 30.0);
    setupProvider();
    preview_->setFfmpegPath(ffmpegPath_);
    if (!path.isEmpty()) preview_->setSource(path, info);
    gotoTime(0.0);
}

void ScrubPreview::setFilters(const FilterModel* model) {
    external_ = model;
    setupProvider();
    refreshPreview();
}

void ScrubPreview::setEditableModel(const FilterModel& initial) {
    previewModel_ = initial;
    external_ = nullptr;
    setupProvider();
    refreshPreview();
}

void ScrubPreview::refreshPreview() {
    if (preview_) preview_->clearCache();
    gotoTime(current_);
}

void ScrubPreview::setDuration(double seconds) {
    duration_ = std::max(0.0, seconds);
    timeline_->setDuration(duration_);
}

void ScrubPreview::setCropEditable(bool editable) {
    if (canvas_) canvas_->setCropEditable(editable);
}
void ScrubPreview::setCropRect(const QRect& rect) {
    if (canvas_) canvas_->setCropRect(rect);
}
QRect ScrubPreview::cropRect() const {
    return canvas_ ? canvas_->cropRect() : QRect();
}

void ScrubPreview::setBeforeAfter(bool on, std::function<QImage(const QImage&)> effect) {
    beforeAfter_ = on;
    effect_ = std::move(effect);
    setupProvider();
    refreshPreview();
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
    lastImageSize_ = image.size();
    if (!beforeAfter_) {
        if (canvas_) canvas_->setImage(image);
        return;
    }

    QImage processed = effect_ ? effect_(image) : image;
    if (processed.size() != image.size())
        processed = processed.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QImage composed(image.size(), QImage::Format_ARGB32);
    composed.fill(Qt::black);
    QPainter p(&composed);
    const int mid = image.width() / 2;
    p.drawImage(QRect(0, 0, mid, image.height()), image,
                QRect(0, 0, mid, image.height()));
    p.drawImage(QRect(mid, 0, image.width() - mid, image.height()), processed,
                QRect(mid, 0, image.width() - mid, image.height()));
    p.setPen(QPen(QColor(255, 60, 60), 2));
    p.drawLine(mid, 0, mid, image.height());
    p.end();
    if (canvas_) canvas_->setImage(composed);
}

bool ScrubPreview::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        if (QApplication::activeWindow() == window()) {
            QWidget* fw = QApplication::focusWidget();
            const bool editing = qobject_cast<QAbstractSpinBox*>(fw) ||
                                 qobject_cast<QLineEdit*>(fw) ||
                                 qobject_cast<QPlainTextEdit*>(fw) ||
                                 qobject_cast<QTextEdit*>(fw);
            if (!editing) {
                auto* ke = static_cast<QKeyEvent*>(event);
                int delta = 0;
                switch (ke->key()) {
                    case Qt::Key_Left: delta = -1; break;
                    case Qt::Key_Right: delta = 1; break;
                    case Qt::Key_PageUp: delta = -5; break;
                    case Qt::Key_PageDown: delta = 5; break;
                    default: break;
                }
                if (delta != 0) {
                    onStep(delta);
                    return true;
                }
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

} // namespace wc
