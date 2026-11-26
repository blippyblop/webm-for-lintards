#include "ui/dialogs/trimdialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>

namespace wc {

namespace {
constexpr double kFallbackMaxSeconds = 1e6;

double maxSecondsFor(const VideoInfo& info) {
    return info.durationSeconds > 0.0 ? info.durationSeconds : kFallbackMaxSeconds;
}
} // namespace

TrimDialog::TrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Trim"));

    const double maxSeconds = maxSecondsFor(info_);

    wholeClip_ = new QCheckBox(tr("Use whole clip"), this);

    start_ = new QDoubleSpinBox(this);
    start_->setRange(0.0, maxSeconds);
    start_->setDecimals(2);
    start_->setSingleStep(0.1);
    start_->setSuffix(tr(" s"));

    end_ = new QDoubleSpinBox(this);
    end_->setRange(0.0, maxSeconds);
    end_->setDecimals(2);
    end_->setSingleStep(0.1);
    end_->setSuffix(tr(" s"));

    if (model_.trim.has_value()) {
        wholeClip_->setChecked(false);
        const TrimRange& range = *model_.trim;
        start_->setValue(std::clamp(range.start, 0.0, maxSeconds));
        const double endValue = range.end > range.start ? range.end : maxSeconds;
        end_->setValue(std::clamp(endValue, 0.0, maxSeconds));
    } else {
        wholeClip_->setChecked(true);
        start_->setValue(0.0);
        end_->setValue(maxSeconds);
    }

    auto* form = new QFormLayout(this);
    if (info_.durationSeconds > 0.0) {
        form->addRow(tr("Source duration:"),
                     new QLabel(QString::number(info_.durationSeconds, 'f', 2) + tr(" s"), this));
    } else {
        form->addRow(tr("Source duration:"), new QLabel(tr("unknown"), this));
    }
    form->addRow(wholeClip_);
    form->addRow(tr("Start:"), start_);
    form->addRow(tr("End:"), end_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &TrimDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TrimDialog::reject);
    form->addRow(buttons);

    const auto updateEnabled = [this](bool whole) {
        start_->setEnabled(!whole);
        end_->setEnabled(!whole);
    };
    connect(wholeClip_, &QCheckBox::toggled, this, updateEnabled);
    updateEnabled(wholeClip_->isChecked());
}

void TrimDialog::accept() {
    const double maxSeconds = maxSecondsFor(info_);

    if (wholeClip_->isChecked()) {
        model_.trim.reset();
    } else {
        const double start = std::clamp(start_->value(), 0.0, maxSeconds);
        const double end = std::clamp(end_->value(), 0.0, maxSeconds);
        if (end > start) {
            model_.trim = TrimRange{start, end};
        } else {
            model_.trim.reset();
        }
    }

    QDialog::accept();
}

} // namespace wc
