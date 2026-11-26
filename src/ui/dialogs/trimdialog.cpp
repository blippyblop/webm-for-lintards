#include "ui/dialogs/trimdialog.h"

#include "ui/previewcanvas.h"
#include "ui/timelinewidget.h"
#include "preview/previewengine.h"
#include "core/previewfilter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTimer>
#include <QSignalBlocker>

#include <algorithm>

namespace wc {

TrimDialog::TrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent,
                       const QString& inputPath, const QString& ffmpegPath)
    : QDialog(parent), model_(model), info_(info), inputPath_(inputPath) {
    setWindowTitle(tr("Trim"));
    resize(960, 700);

    auto* layout = new QVBoxLayout(this);

    canvas_ = new PreviewCanvas(this);
    canvas_->setCropEditable(false);
    canvas_->setMinimumHeight(360);
    layout->addWidget(canvas_, 1);

    timeline_ = new TimelineWidget(this);
    timeline_->setDuration(info_.durationSeconds);
    timeline_->setFrameRate(info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0);
    layout->addWidget(timeline_);

    currentLabel_ = new QLabel(tr("frame 0"), this);
    durationLabel_ = new QLabel(tr("0 frames"), this);
    auto* timeRow = new QHBoxLayout;
    timeRow->addStretch(1);
    timeRow->addWidget(new QLabel(tr("Current:"), this));
    timeRow->addWidget(currentLabel_);
    timeRow->addSpacing(24);
    timeRow->addWidget(new QLabel(tr("Trim length:"), this));
    timeRow->addWidget(durationLabel_);
    timeRow->addStretch(1);
    layout->addLayout(timeRow);

    // Frame-based range controls, centred.
    startFrame_ = new QSpinBox(this);
    endFrame_ = new QSpinBox(this);
    for (QSpinBox* sp : {startFrame_, endFrame_}) {
        sp->setRange(0, std::max(0, maxFrame()));
        sp->setSuffix(tr(" f"));
    }
    wholeClip_ = new QCheckBox(tr("Use whole clip"), this);

    auto* setStartBtn = new QPushButton(tr("Set Start:"), this);
    auto* setEndBtn = new QPushButton(tr("Set End:"), this);
    playBtn_ = new QPushButton(tr("Play preview"), this);

    auto* rangeRow = new QHBoxLayout;
    rangeRow->addStretch(1);
    rangeRow->addWidget(new QLabel(tr("Start frame:"), this));
    rangeRow->addWidget(startFrame_);
    rangeRow->addWidget(setStartBtn);
    rangeRow->addSpacing(16);
    rangeRow->addWidget(new QLabel(tr("End frame:"), this));
    rangeRow->addWidget(endFrame_);
    rangeRow->addWidget(setEndBtn);
    rangeRow->addSpacing(16);
    rangeRow->addWidget(playBtn_);
    rangeRow->addSpacing(8);
    rangeRow->addWidget(wholeClip_);
    rangeRow->addStretch(1);
    layout->addLayout(rangeRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &TrimDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Initial range from the model (converted to frames).
    const int total = maxFrame();
    const bool whole = !model_.trim.has_value();
    int sFrame = 0;
    int eFrame = total;
    if (model_.trim) {
        sFrame = std::clamp(info_.frameAt(model_.trim->start), 0, total);
        const double endSec = model_.trim->end > 0.0 ? model_.trim->end : info_.durationSeconds;
        eFrame = std::clamp(info_.frameAt(endSec), sFrame, total);
    }
    startFrame_->setValue(sFrame);
    endFrame_->setValue(eFrame);
    wholeClip_->setChecked(whole);
    startFrame_->setEnabled(!whole);
    endFrame_->setEnabled(!whole);

    connect(startFrame_, &QSpinBox::valueChanged, this, &TrimDialog::syncFromSpins);
    connect(endFrame_, &QSpinBox::valueChanged, this, &TrimDialog::syncFromSpins);
    connect(wholeClip_, &QCheckBox::toggled, this, [this](bool on) {
        startFrame_->setEnabled(!on);
        endFrame_->setEnabled(!on);
        if (on) {
            QSignalBlocker b1(startFrame_), b2(endFrame_);
            startFrame_->setValue(0);
            endFrame_->setValue(maxFrame());
        }
        updateRangeUi();
    });
    connect(setStartBtn, &QPushButton::clicked, this, &TrimDialog::setStartHere);
    connect(setEndBtn, &QPushButton::clicked, this, &TrimDialog::setEndHere);
    connect(playBtn_, &QPushButton::clicked, this, &TrimDialog::togglePlay);
    connect(timeline_, &TimelineWidget::seekRequested, this, &TrimDialog::onSeek);
    connect(timeline_, &TimelineWidget::frameStepRequested, this, &TrimDialog::onFrameStep);

    playTimer_ = new QTimer(this);
    connect(playTimer_, &QTimer::timeout, this, &TrimDialog::playTick);

    // Preview with the current filters rendered.
    if (!inputPath_.isEmpty()) {
        preview_ = new PreviewEngine(this);
        preview_->setFfmpegPath(ffmpegPath.isEmpty() ? QStringLiteral("ffmpeg") : ffmpegPath);
        preview_->setFilterProvider([this](double t) -> QString {
            return QString::fromStdString(buildPreviewVideoFilter(model_, info_, t));
        });
        connect(preview_, &PreviewEngine::frameReady, this, &TrimDialog::onFrameReady);
        preview_->setSource(inputPath_, info_);
    }

    updateRangeUi();
    gotoFrame(sFrame);
}

TrimDialog::~TrimDialog() {
    if (playTimer_) playTimer_->stop();
    if (preview_) preview_->cancel();
}

int TrimDialog::maxFrame() const {
    const long long c = info_.frameCount();
    if (c <= 0) return 0;
    return static_cast<int>(c - 1);
}

void TrimDialog::gotoFrame(int frame) {
    const int total = maxFrame();
    frame = std::clamp(frame, 0, total);
    currentFrameIndex_ = frame;
    const double seconds = info_.timeOfFrame(frame);
    timeline_->setCurrentTime(seconds);
    const double fps = info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0;
    currentLabel_->setText(tr("%1  (%2 s @ %3 fps)")
                               .arg(frame)
                               .arg(seconds, 0, 'f', 2)
                               .arg(fps, 0, 'f', 2));
    if (preview_ && preview_->hasSource()) preview_->requestFrame(frame);
}

void TrimDialog::onSeek(double seconds) {
    gotoFrame(info_.frameAt(seconds));
}

void TrimDialog::onFrameStep(int delta) {
    gotoFrame(currentFrameIndex_ + delta);
}

void TrimDialog::onFrameReady(int frame, const QImage& image) {
    Q_UNUSED(frame);
    if (canvas_) canvas_->setImage(image);
}

void TrimDialog::setStartHere() {
    wholeClip_->setChecked(false);
    startFrame_->setValue(currentFrameIndex_);
}

void TrimDialog::setEndHere() {
    wholeClip_->setChecked(false);
    endFrame_->setValue(currentFrameIndex_);
}

void TrimDialog::syncFromSpins() {
    if (endFrame_->value() < startFrame_->value()) {
        QSignalBlocker b(endFrame_);
        endFrame_->setValue(startFrame_->value());
    }
    updateRangeUi();
}

void TrimDialog::updateRangeUi() {
    const int sF = wholeClip_->isChecked() ? 0 : startFrame_->value();
    const int eF = wholeClip_->isChecked() ? maxFrame() : std::max(sF, endFrame_->value());
    timeline_->setTrimRange(info_.timeOfFrame(sF), info_.timeOfFrame(eF), true);
    const double secs = info_.timeOfFrame(eF - sF + 1);
    durationLabel_->setText(tr("%1 frames (%2 s)").arg(eF - sF + 1).arg(secs, 0, 'f', 2));
}

void TrimDialog::togglePlay() {
    if (!playing_) {
        savedFrame_ = currentFrameIndex_;
        playing_ = true;
        playBtn_->setText(tr("Stop"));
        const int start = wholeClip_->isChecked() ? 0 : startFrame_->value();
        gotoFrame(start);
        const double fps = info_.frameRateValue() > 0.0 ? info_.frameRateValue() : 30.0;
        playTimer_->start(static_cast<int>(std::max(10.0, 1000.0 / fps)));
    } else {
        playing_ = false;
        playBtn_->setText(tr("Play preview"));
        playTimer_->stop();
        gotoFrame(savedFrame_);
    }
}

void TrimDialog::playTick() {
    const int end = wholeClip_->isChecked() ? maxFrame() : endFrame_->value();
    if (currentFrameIndex_ >= end) {
        togglePlay();   // stop and reset
        return;
    }
    gotoFrame(currentFrameIndex_ + 1);
}

void TrimDialog::accept() {
    if (playing_) togglePlay();
    if (wholeClip_->isChecked()) {
        model_.trim.reset();
    } else {
        const int sF = startFrame_->value();
        const int eF = endFrame_->value();
        if (eF <= sF) {
            model_.trim.reset();
        } else {
            const double startSec = info_.timeOfFrame(sF);
            double endSec = (eF >= maxFrame()) ? info_.durationSeconds
                                               : info_.timeOfFrame(eF + 1);
            if (endSec <= startSec) endSec = info_.timeOfFrame(eF);
            model_.trim = TrimRange{startSec, endSec};
        }
    }
    QDialog::accept();
}

} // namespace wc
