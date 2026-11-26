#include "ui/dialogs/trimdialog.h"

#include "ui/previewcanvas.h"
#include "ui/timelinewidget.h"
#include "preview/previewengine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QSignalBlocker>

#include <algorithm>

namespace wc {

TrimDialog::TrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent,
                       const QString& inputPath, const QString& ffmpegPath)
    : QDialog(parent), model_(model), info_(info), inputPath_(inputPath) {
    setWindowTitle(tr("Trim"));
    resize(960, 680);

    auto* layout = new QVBoxLayout(this);

    canvas_ = new PreviewCanvas(this);
    canvas_->setCropEditable(false);
    canvas_->setMinimumHeight(360);
    layout->addWidget(canvas_, 1);

    timeline_ = new TimelineWidget(this);
    timeline_->setDuration(info_.durationSeconds);
    timeline_->setFrameRate(info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0);
    layout->addWidget(timeline_);

    // Current time / duration readouts.
    currentLabel_ = new QLabel(tr("0.00 s"), this);
    durationLabel_ = new QLabel(tr("0.00 s"), this);
    auto* timeRow = new QHBoxLayout;
    timeRow->addWidget(new QLabel(tr("Current:"), this));
    timeRow->addWidget(currentLabel_);
    timeRow->addSpacing(24);
    timeRow->addWidget(new QLabel(tr("Duration:"), this));
    timeRow->addWidget(durationLabel_);
    timeRow->addStretch(1);
    layout->addLayout(timeRow);

    // Range controls.
    const double maxT = std::max(1.0, info_.durationSeconds);
    startSpin_ = new QDoubleSpinBox(this);
    endSpin_ = new QDoubleSpinBox(this);
    for (QDoubleSpinBox* sp : {startSpin_, endSpin_}) {
        sp->setRange(0.0, maxT);
        sp->setDecimals(3);
        sp->setSuffix(tr(" s"));
        sp->setSingleStep(0.1);
    }
    wholeClip_ = new QCheckBox(tr("Use whole clip"), this);

    auto* setStartBtn = new QPushButton(tr("Set Start:"), this);
    auto* setEndBtn = new QPushButton(tr("Set End:"), this);

    auto* rangeRow = new QHBoxLayout;
    rangeRow->addWidget(new QLabel(tr("Start:"), this));
    rangeRow->addWidget(startSpin_);
    rangeRow->addWidget(setStartBtn);
    rangeRow->addSpacing(16);
    rangeRow->addWidget(new QLabel(tr("End:"), this));
    rangeRow->addWidget(endSpin_);
    rangeRow->addWidget(setEndBtn);
    rangeRow->addSpacing(16);
    rangeRow->addWidget(wholeClip_);
    rangeRow->addStretch(1);
    layout->addLayout(rangeRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &TrimDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Initial range from the model.
    double s = 0.0;
    double e = info_.durationSeconds;
    const bool whole = !model_.trim.has_value();
    if (model_.trim) {
        s = std::clamp(model_.trim->start, 0.0, maxT);
        e = (model_.trim->end > 0.0) ? std::clamp(model_.trim->end, 0.0, maxT) : info_.durationSeconds;
    }
    startSpin_->setValue(s);
    endSpin_->setValue(e);
    wholeClip_->setChecked(whole);
    startSpin_->setEnabled(!whole);
    endSpin_->setEnabled(!whole);

    connect(startSpin_, &QDoubleSpinBox::valueChanged, this, &TrimDialog::syncFromSpins);
    connect(endSpin_, &QDoubleSpinBox::valueChanged, this, &TrimDialog::syncFromSpins);
    connect(wholeClip_, &QCheckBox::toggled, this, [this](bool on) {
        startSpin_->setEnabled(!on);
        endSpin_->setEnabled(!on);
        if (on) {
            QSignalBlocker b1(startSpin_), b2(endSpin_);
            startSpin_->setValue(0.0);
            endSpin_->setValue(info_.durationSeconds);
        }
        updateRangeUi();
    });
    connect(setStartBtn, &QPushButton::clicked, this, &TrimDialog::setStartHere);
    connect(setEndBtn, &QPushButton::clicked, this, &TrimDialog::setEndHere);
    connect(timeline_, &TimelineWidget::seekRequested, this, &TrimDialog::onSeek);
    connect(timeline_, &TimelineWidget::frameStepRequested, this, &TrimDialog::onFrameStep);

    // Preview engine (optional: only if we have a source path).
    if (!inputPath_.isEmpty()) {
        preview_ = new PreviewEngine(this);
        preview_->setFfmpegPath(ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : ffmpegPath);
        connect(preview_, &PreviewEngine::frameReady, this, &TrimDialog::onFrameReady);
        preview_->setSource(inputPath_, info_);
    }

    updateRangeUi();
    gotoTime(s);
}

TrimDialog::~TrimDialog() {
    if (preview_) preview_->cancel();
}

void TrimDialog::gotoTime(double seconds) {
    const double dur = info_.durationSeconds;
    if (seconds < 0.0) seconds = 0.0;
    if (dur > 0.0 && seconds > dur) seconds = dur;
    currentTime_ = seconds;
    timeline_->setCurrentTime(seconds);
    const int frame = info_.frameAt(seconds);
    const double fps = info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0;
    currentLabel_->setText(tr("%1 s  (frame %2 @ %3 fps)")
                               .arg(seconds, 0, 'f', 2)
                               .arg(frame)
                               .arg(fps, 0, 'f', 2));
    if (preview_ && preview_->hasSource()) preview_->requestFrame(frame);
}

void TrimDialog::onSeek(double seconds) { gotoTime(seconds); }

void TrimDialog::onFrameStep(int delta) {
    const double fps = info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0;
    gotoTime(currentTime_ + delta / fps);
}

void TrimDialog::onFrameReady(int frame, const QImage& image) {
    Q_UNUSED(frame);
    if (canvas_) canvas_->setImage(image);
}

void TrimDialog::setStartHere() {
    wholeClip_->setChecked(false);
    startSpin_->setValue(currentTime_);
}

void TrimDialog::setEndHere() {
    wholeClip_->setChecked(false);
    endSpin_->setValue(currentTime_);
}

void TrimDialog::syncFromSpins() {
    double s = startSpin_->value();
    double e = endSpin_->value();
    if (e < s) {
        QSignalBlocker b(endSpin_);
        endSpin_->setValue(s);
    }
    updateRangeUi();
}

void TrimDialog::updateRangeUi() {
    const double s = wholeClip_->isChecked() ? 0.0 : startSpin_->value();
    const double e = wholeClip_->isChecked() ? info_.durationSeconds : endSpin_->value();
    const double clampedE = std::max(s, e);
    timeline_->setTrimRange(s, clampedE, true);
    durationLabel_->setText(tr("%1 s").arg(clampedE - s, 0, 'f', 2));
}

void TrimDialog::accept() {
    if (wholeClip_->isChecked()) {
        model_.trim.reset();
    } else {
        const double s = startSpin_->value();
        const double e = endSpin_->value();
        if (e <= s)
            model_.trim.reset();
        else
            model_.trim = TrimRange{s, e};
    }
    QDialog::accept();
}

} // namespace wc
