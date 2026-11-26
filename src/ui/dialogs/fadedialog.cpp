#include "fadedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>

#include <algorithm>

namespace wc {

FadeDialog::FadeDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Fade"));

    const double maxTime = (info_.durationSeconds > 0.0) ? info_.durationSeconds : 1.0e6;

    enabledBox_ = new QCheckBox(tr("Enable fade"), this);
    enabledBox_->setChecked(model_.fade.enabled);

    inSpin_ = new QDoubleSpinBox(this);
    inSpin_->setDecimals(3);
    inSpin_->setSingleStep(0.1);
    inSpin_->setSuffix(tr(" s"));
    inSpin_->setRange(0.0, maxTime);
    inSpin_->setValue(std::min(std::max(0.0, model_.fade.inDuration), maxTime));

    outSpin_ = new QDoubleSpinBox(this);
    outSpin_->setDecimals(3);
    outSpin_->setSingleStep(0.1);
    outSpin_->setSuffix(tr(" s"));
    outSpin_->setRange(0.0, maxTime);
    outSpin_->setValue(std::min(std::max(0.0, model_.fade.outDuration), maxTime));

    auto* form = new QFormLayout;
    form->addRow(tr("Fade in:"), inSpin_);
    form->addRow(tr("Fade out:"), outSpin_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &FadeDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &FadeDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enabledBox_);
    layout->addLayout(form);
    layout->addWidget(buttons);

    resize(360, 180);
}

void FadeDialog::accept() {
    Fade& f = model_.fade;

    f.inDuration = inSpin_->value();
    f.outDuration = outSpin_->value();
    f.enabled = enabledBox_->isChecked() &&
                (f.inDuration > 0.0 || f.outDuration > 0.0);

    QDialog::accept();
}

} // namespace wc
