#include "ui/dialogs/denoisedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVariant>
#include <QVBoxLayout>

namespace wc {

DenoiseDialog::DenoiseDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    Q_UNUSED(info_);
    setWindowTitle(tr("Denoise"));
    setModal(true);

    enableCheck_ = new QCheckBox(tr("Enable denoise"), this);

    strengthCombo_ = new QComboBox(this);
    strengthCombo_->addItem(tr("Light"), 0);
    strengthCombo_->addItem(tr("Medium"), 1);
    strengthCombo_->addItem(tr("Strong"), 2);

    // Pre-fill from the model.
    enableCheck_->setChecked(model_.denoise);
    const int idx = strengthCombo_->findData(model_.denoiseStrength);
    strengthCombo_->setCurrentIndex(idx >= 0 ? idx : 0);

    auto* form = new QFormLayout;
    form->addRow(tr("Strength:"), strengthCombo_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DenoiseDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DenoiseDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enableCheck_);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void DenoiseDialog::accept() {
    model_.denoise = enableCheck_->isChecked();

    const QVariant data = strengthCombo_->currentData();
    model_.denoiseStrength = data.isValid() ? data.toInt() : 0;

    QDialog::accept();
}

} // namespace wc
