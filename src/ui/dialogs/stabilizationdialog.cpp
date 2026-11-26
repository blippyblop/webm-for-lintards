#include "ui/dialogs/stabilizationdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVariant>
#include <QVBoxLayout>

namespace wc {

StabilizationDialog::StabilizationDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    Q_UNUSED(info_);
    setWindowTitle(tr("Stabilization"));
    setModal(true);

    enableCheck_ = new QCheckBox(tr("Enable stabilization"), this);

    shakinessSpin_ = new QSpinBox(this);
    shakinessSpin_->setRange(1, 10);
    shakinessSpin_->setValue(5);

    accuracySpin_ = new QSpinBox(this);
    accuracySpin_->setRange(1, 15);
    accuracySpin_->setValue(15);

    smoothingSpin_ = new QSpinBox(this);
    smoothingSpin_->setRange(1, 30);
    smoothingSpin_->setValue(10);

    zoomSpin_ = new QSpinBox(this);
    zoomSpin_->setRange(0, 20);
    zoomSpin_->setValue(0);

    interpolCombo_ = new QComboBox(this);
    interpolCombo_->addItem(tr("Nearest"), 0);
    interpolCombo_->addItem(tr("Bilinear"), 1);
    interpolCombo_->addItem(tr("Bicubic"), 2);

    // Pre-fill from the model.
    const Stabilization& s = model_.stabilization;
    enableCheck_->setChecked(s.enabled);
    shakinessSpin_->setValue(s.shakiness);
    accuracySpin_->setValue(s.accuracy);
    smoothingSpin_->setValue(s.smoothing);
    zoomSpin_->setValue(s.zoom);
    const int interpolIdx = interpolCombo_->findData(s.interpol);
    interpolCombo_->setCurrentIndex(interpolIdx >= 0 ? interpolIdx : 2);

    auto* form = new QFormLayout;
    form->addRow(tr("Shakiness:"), shakinessSpin_);
    form->addRow(tr("Accuracy:"), accuracySpin_);
    form->addRow(tr("Smoothing:"), smoothingSpin_);
    form->addRow(tr("Zoom:"), zoomSpin_);
    form->addRow(tr("Interpolation:"), interpolCombo_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &StabilizationDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &StabilizationDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enableCheck_);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void StabilizationDialog::accept() {
    model_.stabilization.enabled = enableCheck_->isChecked();
    model_.stabilization.shakiness = shakinessSpin_->value();
    model_.stabilization.accuracy = accuracySpin_->value();
    model_.stabilization.smoothing = smoothingSpin_->value();
    model_.stabilization.zoom = zoomSpin_->value();

    const QVariant data = interpolCombo_->currentData();
    model_.stabilization.interpol = data.isValid() ? data.toInt() : 2;

    QDialog::accept();
}

} // namespace wc
