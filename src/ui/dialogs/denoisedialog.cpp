#include "ui/dialogs/denoisedialog.h"
#include "ui/scrubpreview.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVariant>
#include <QVBoxLayout>

namespace wc {

DenoiseDialog::DenoiseDialog(FilterModel& model, const VideoInfo& info, QWidget* parent,
                             const QString& inputPath, const QString& ffmpegPath)
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

    auto* splitBox = new QCheckBox(tr("Before / after split"), this);
    form->addRow(splitBox);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enableCheck_);
    layout->addLayout(form);
    layout->addWidget(buttons);

    scrub_ = new ScrubPreview(this);
    if (!inputPath.isEmpty()) scrub_->setSource(inputPath, info_, ffmpegPath);
    scrub_->setEditableModel(model_);
    layout->insertWidget(0, scrub_);

    auto syncPreview = [this]() {
        FilterModel& m = scrub_->previewModel();
        m.denoise = enableCheck_->isChecked();
        m.denoiseStrength = strengthCombo_->currentData().toInt();
        scrub_->refreshPreview();
    };
    connect(enableCheck_, &QCheckBox::toggled, this, [syncPreview](bool) { syncPreview(); });
    connect(strengthCombo_, &QComboBox::currentIndexChanged, this, [syncPreview](int) { syncPreview(); });

    auto blur = [this](const QImage& src) -> QImage {
        const int s = strengthCombo_->currentData().toInt();   // 0..2
        const int div = std::max(2, 8 - 2 * s);
        QImage small = src.scaled(std::max(1, src.width() / div), std::max(1, src.height() / div),
                                  Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        return small.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    };
    connect(splitBox, &QCheckBox::toggled, this, [this, blur](bool on) {
        scrub_->setBeforeAfter(on, blur);
    });
    resize(900, 780);
}

void DenoiseDialog::accept() {
    model_.denoise = enableCheck_->isChecked();

    const QVariant data = strengthCombo_->currentData();
    model_.denoiseStrength = data.isValid() ? data.toInt() : 0;

    QDialog::accept();
}

} // namespace wc
