#include "ui/dialogs/cropdialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>

namespace wc {

namespace {
constexpr int kFallbackDimension = 7680;

int maxWidthFor(const VideoInfo& info) {
    return info.width > 0 ? info.width : kFallbackDimension;
}

int maxHeightFor(const VideoInfo& info) {
    return info.height > 0 ? info.height : kFallbackDimension;
}
} // namespace

CropDialog::CropDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Crop"));

    const int maxW = maxWidthFor(info_);
    const int maxH = maxHeightFor(info_);

    const bool hasCrop = model_.crop.w > 0 && model_.crop.h > 0;
    const int initX = hasCrop ? model_.crop.x : 0;
    const int initY = hasCrop ? model_.crop.y : 0;
    const int initW = hasCrop ? model_.crop.w : maxW;
    const int initH = hasCrop ? model_.crop.h : maxH;

    enable_ = new QCheckBox(tr("Enable crop"), this);
    enable_->setChecked(model_.crop.enabled);

    x_ = new QSpinBox(this);
    x_->setRange(0, maxW);
    x_->setValue(std::clamp(initX, 0, maxW));
    x_->setSuffix(tr(" px"));

    y_ = new QSpinBox(this);
    y_->setRange(0, maxH);
    y_->setValue(std::clamp(initY, 0, maxH));
    y_->setSuffix(tr(" px"));

    w_ = new QSpinBox(this);
    w_->setRange(1, maxW);
    w_->setValue(std::clamp(initW, 1, maxW));
    w_->setSuffix(tr(" px"));

    h_ = new QSpinBox(this);
    h_->setRange(1, maxH);
    h_->setValue(std::clamp(initH, 1, maxH));
    h_->setSuffix(tr(" px"));

    auto* form = new QFormLayout(this);
    if (info_.width > 0 && info_.height > 0) {
        form->addRow(tr("Source size:"),
                     new QLabel(tr("%1 x %2 px").arg(info_.width).arg(info_.height), this));
    }
    form->addRow(enable_);
    form->addRow(tr("Left (x):"), x_);
    form->addRow(tr("Top (y):"), y_);
    form->addRow(tr("Width:"), w_);
    form->addRow(tr("Height:"), h_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &CropDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CropDialog::reject);

    const auto updateEnabled = [this](bool on) {
        x_->setEnabled(on);
        y_->setEnabled(on);
        w_->setEnabled(on);
        h_->setEnabled(on);
    };
    connect(enable_, &QCheckBox::toggled, this, updateEnabled);
    updateEnabled(enable_->isChecked());
}

void CropDialog::accept() {
    const int maxW = maxWidthFor(info_);
    const int maxH = maxHeightFor(info_);

    int w = w_->value() / 2 * 2;
    int h = h_->value() / 2 * 2;
    if (w < 2)
        w = 2;
    if (h < 2)
        h = 2;
    if (w > maxW)
        w = maxW / 2 * 2;
    if (h > maxH)
        h = maxH / 2 * 2;
    if (w < 2)
        w = 2;
    if (h < 2)
        h = 2;

    int x = std::clamp(x_->value(), 0, std::max(0, maxW - w));
    int y = std::clamp(y_->value(), 0, std::max(0, maxH - h));

    model_.crop.enabled = enable_->isChecked();
    model_.crop.x = x;
    model_.crop.y = y;
    model_.crop.w = w;
    model_.crop.h = h;

    QDialog::accept();
}

} // namespace wc
