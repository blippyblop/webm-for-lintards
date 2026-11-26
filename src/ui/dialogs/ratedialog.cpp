#include "ui/dialogs/ratedialog.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>

namespace wc {

namespace {
constexpr int kMinFps = 1;
constexpr int kMaxFps = 240;
constexpr int kFallbackFps = 30;
} // namespace

RateDialog::RateDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Frame Rate"));

    const double sourceFps = info_.frameRateValue();
    int initialFps = model_.rateFps.has_value() ? *model_.rateFps
                                                : static_cast<int>(std::lround(sourceFps));
    if (initialFps <= 0)
        initialFps = kFallbackFps;
    initialFps = std::clamp(initialFps, kMinFps, kMaxFps);

    keepSource_ = new QCheckBox(tr("Keep source rate"), this);
    keepSource_->setChecked(!model_.rateFps.has_value());

    fps_ = new QSpinBox(this);
    fps_->setRange(kMinFps, kMaxFps);
    fps_->setValue(initialFps);
    fps_->setSuffix(tr(" fps"));

    auto* form = new QFormLayout(this);
    if (sourceFps > 0.0) {
        form->addRow(tr("Source rate:"),
                     new QLabel(QString::number(sourceFps, 'f', 3) + tr(" fps"), this));
    } else {
        form->addRow(tr("Source rate:"), new QLabel(tr("unknown"), this));
    }
    form->addRow(keepSource_);
    form->addRow(tr("Target rate:"), fps_);

    percent_ = new QSlider(Qt::Horizontal, this);
    percent_->setRange(10, 400);
    percent_->setSingleStep(5);
    percentLabel_ = new QLabel(this);
    auto* pctRow = new QHBoxLayout;
    pctRow->addWidget(percent_, 1);
    pctRow->addWidget(percentLabel_);
    form->addRow(tr("Rate %:"), pctRow);

    if (sourceFps > 0.0) {
        const int pct = std::clamp(static_cast<int>(std::lround(initialFps / sourceFps * 100.0)), 10, 400);
        percent_->setValue(pct);
    } else {
        percent_->setValue(100);
    }
    percentLabel_->setText(QString::number(percent_->value()) + QLatin1Char('%'));

    connect(percent_, &QSlider::valueChanged, this, [this, sourceFps](int pct) {
        percentLabel_->setText(QString::number(pct) + QLatin1Char('%'));
        if (sourceFps > 0.0) {
            QSignalBlocker b(fps_);
            fps_->setValue(std::clamp(static_cast<int>(std::lround(sourceFps * pct / 100.0)),
                                      kMinFps, kMaxFps));
        }
    });
    connect(fps_, &QSpinBox::valueChanged, this, [this, sourceFps](int value) {
        if (sourceFps <= 0.0) return;
        QSignalBlocker b(percent_);
        percent_->setValue(std::clamp(static_cast<int>(std::lround(value / sourceFps * 100.0)), 10, 400));
        percentLabel_->setText(QString::number(percent_->value()) + QLatin1Char('%'));
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &RateDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &RateDialog::reject);

    connect(keepSource_, &QCheckBox::toggled, fps_, &QSpinBox::setDisabled);
    connect(keepSource_, &QCheckBox::toggled, percent_, &QSlider::setDisabled);
    fps_->setDisabled(keepSource_->isChecked());
    percent_->setDisabled(keepSource_->isChecked());
}

void RateDialog::accept() {
    if (keepSource_->isChecked()) {
        model_.rateFps.reset();
    } else {
        model_.rateFps = std::clamp(fps_->value(), kMinFps, kMaxFps);
    }

    QDialog::accept();
}

} // namespace wc
