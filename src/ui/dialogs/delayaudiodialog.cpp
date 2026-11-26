#include "ui/dialogs/delayaudiodialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

namespace wc {

DelayAudioDialog::DelayAudioDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    Q_UNUSED(info_);
    setWindowTitle(tr("Delay Audio"));
    setModal(true);

    delaySpin_ = new QSpinBox(this);
    delaySpin_->setRange(0, 10000);
    delaySpin_->setSingleStep(50);
    delaySpin_->setSuffix(tr(" ms"));
    delaySpin_->setToolTip(tr("Positive values delay the audio relative to the video."));

    // Pre-fill from the model, clamping negative delays to zero.
    const int initial = model_.delayAudioMs < 0 ? 0 : model_.delayAudioMs;
    delaySpin_->setValue(initial);

    auto* form = new QFormLayout;
    form->addRow(tr("Delay:"), delaySpin_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DelayAudioDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DelayAudioDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void DelayAudioDialog::accept() {
    model_.delayAudioMs = delaySpin_->value();
    QDialog::accept();
}

} // namespace wc
