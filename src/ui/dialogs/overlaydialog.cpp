#include "overlaydialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace wc {

OverlayDialog::OverlayDialog(FilterModel& model, const VideoInfo& info,
                             QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Overlay image"));

    const double maxTime = (info_.durationSeconds > 0.0) ? info_.durationSeconds : 1.0e6;

    enabledBox_ = new QCheckBox(tr("Enable image overlay"), this);
    enabledBox_->setChecked(model_.overlay.enabled);

    pathEdit_ = new QLineEdit(this);
    pathEdit_->setText(QString::fromStdString(model_.overlay.imagePath));
    pathEdit_->setPlaceholderText(tr("Path to .png / .jpg / .bmp"));

    browseButton_ = new QPushButton(tr("Browse…"), this);

    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browseButton_);

    connect(browseButton_, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Select overlay image"), pathEdit_->text(),
            tr("Images (*.png *.jpg *.jpeg *.bmp)"));
        if (!file.isEmpty()) pathEdit_->setText(file);
    });

    xSpin_ = new QSpinBox(this);
    xSpin_->setRange(-4096, 4096);
    xSpin_->setValue(model_.overlay.x);

    ySpin_ = new QSpinBox(this);
    ySpin_->setRange(-4096, 4096);
    ySpin_->setValue(model_.overlay.y);

    startSpin_ = new QDoubleSpinBox(this);
    startSpin_->setDecimals(3);
    startSpin_->setSingleStep(0.1);
    startSpin_->setRange(0.0, maxTime);
    startSpin_->setValue(std::min(std::max(0.0, model_.overlay.start), maxTime));

    endSpin_ = new QDoubleSpinBox(this);
    endSpin_->setDecimals(3);
    endSpin_->setSingleStep(0.1);
    endSpin_->setRange(0.0, maxTime);
    endSpin_->setValue(std::min(std::max(0.0, model_.overlay.end), maxTime));

    auto* form = new QFormLayout;
    form->addRow(tr("Image:"), pathRow);
    form->addRow(tr("X:"), xSpin_);
    form->addRow(tr("Y:"), ySpin_);
    form->addRow(tr("Start (s):"), startSpin_);
    form->addRow(tr("End (s):"), endSpin_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &OverlayDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &OverlayDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enabledBox_);
    layout->addLayout(form);
    layout->addWidget(buttons);

    resize(500, 260);
}

void OverlayDialog::accept() {
    OverlaySpec& o = model_.overlay;

    o.imagePath = pathEdit_->text().trimmed().toStdString();
    o.x = xSpin_->value();
    o.y = ySpin_->value();
    o.start = startSpin_->value();
    o.end = endSpin_->value();
    o.enabled = enabledBox_->isChecked() && !o.imagePath.empty();

    QDialog::accept();
}

} // namespace wc
