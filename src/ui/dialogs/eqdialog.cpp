#include "ui/dialogs/eqdialog.h"
#include "ui/scrubpreview.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QVBoxLayout>

namespace wc {

EqDialog::EqDialog(FilterModel& model, const VideoInfo& info, QWidget* parent,
                   const QString& inputPath, const QString& ffmpegPath)
    : QDialog(parent), model_(model), info_(info) {
    Q_UNUSED(info_);
    setWindowTitle(tr("Equalizer"));
    setModal(true);

    enableCheck_ = new QCheckBox(tr("Enable color adjustment"), this);

    gammaSpin_ = new QDoubleSpinBox(this);
    gammaSpin_->setRange(0.1, 10.0);
    gammaSpin_->setSingleStep(0.1);
    gammaSpin_->setDecimals(2);
    gammaSpin_->setValue(1.0);

    saturationSpin_ = new QDoubleSpinBox(this);
    saturationSpin_->setRange(0.0, 4.0);
    saturationSpin_->setSingleStep(0.1);
    saturationSpin_->setDecimals(2);
    saturationSpin_->setValue(1.0);

    contrastSpin_ = new QDoubleSpinBox(this);
    contrastSpin_->setRange(0.5, 2.0);
    contrastSpin_->setSingleStep(0.05);
    contrastSpin_->setDecimals(2);
    contrastSpin_->setValue(1.0);

    // Pre-fill from the model.
    const EqFilter& eq = model_.eq;
    enableCheck_->setChecked(eq.enabled);
    gammaSpin_->setValue(eq.gamma);
    saturationSpin_->setValue(eq.saturation);
    contrastSpin_->setValue(eq.contrast);

    auto* form = new QFormLayout;
    form->addRow(tr("Gamma:"), gammaSpin_);
    form->addRow(tr("Saturation:"), saturationSpin_);
    form->addRow(tr("Contrast:"), contrastSpin_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &EqDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &EqDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enableCheck_);
    layout->addLayout(form);
    layout->addWidget(buttons);

    scrub_ = new ScrubPreview(this);
    if (!inputPath.isEmpty()) scrub_->setSource(inputPath, info_, ffmpegPath);
    scrub_->setEditableModel(model_);
    layout->insertWidget(0, scrub_);

    auto syncPreview = [this]() {
        EqFilter& e = scrub_->previewModel().eq;
        e.enabled = enableCheck_->isChecked();
        e.gamma = gammaSpin_->value();
        e.saturation = saturationSpin_->value();
        e.contrast = contrastSpin_->value();
        scrub_->refreshPreview();
    };
    connect(enableCheck_, &QCheckBox::toggled, this, [syncPreview](bool) { syncPreview(); });
    connect(gammaSpin_, &QDoubleSpinBox::valueChanged, this, [syncPreview](double) { syncPreview(); });
    connect(saturationSpin_, &QDoubleSpinBox::valueChanged, this, [syncPreview](double) { syncPreview(); });
    connect(contrastSpin_, &QDoubleSpinBox::valueChanged, this, [syncPreview](double) { syncPreview(); });
    resize(900, 780);
}

void EqDialog::accept() {
    model_.eq.enabled = enableCheck_->isChecked();
    model_.eq.gamma = gammaSpin_->value();
    model_.eq.saturation = saturationSpin_->value();
    model_.eq.contrast = contrastSpin_->value();

    QDialog::accept();
}

} // namespace wc
