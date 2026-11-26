#include "ui/dialogs/resizedialog.h"

#include <algorithm>
#include <cmath>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSpinBox>

namespace wc {

namespace {
constexpr int kMinDimension = 2;
constexpr int kMaxDimension = 7680;
constexpr int kFallbackDimension = 1280;
} // namespace

ResizeDialog::ResizeDialog(FilterModel& model, const VideoInfo& info, QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Resize"));

    const int sourceW = info_.width > 0 ? info_.width : kFallbackDimension;
    const int sourceH = info_.height > 0 ? info_.height : kFallbackDimension;
    const int initW = model_.resizeW > 0 ? model_.resizeW : sourceW;
    const int initH = model_.resizeH > 0 ? model_.resizeH : sourceH;

    width_ = new QSpinBox(this);
    width_->setRange(kMinDimension, kMaxDimension);
    width_->setValue(std::clamp(initW, kMinDimension, kMaxDimension));
    width_->setSuffix(tr(" px"));

    height_ = new QSpinBox(this);
    height_->setRange(kMinDimension, kMaxDimension);
    height_->setValue(std::clamp(initH, kMinDimension, kMaxDimension));
    height_->setSuffix(tr(" px"));

    keepAspect_ = new QCheckBox(tr("Keep aspect ratio"), this);
    keepAspect_->setChecked(true);

    algo_ = new QComboBox(this);
    algo_->addItem(QStringLiteral("bicubic"), QStringLiteral("bicubic"));
    algo_->addItem(QStringLiteral("bilinear"), QStringLiteral("bilinear"));
    algo_->addItem(QStringLiteral("lanczos"), QStringLiteral("lanczos"));
    algo_->addItem(QStringLiteral("neighbor"), QStringLiteral("neighbor"));
    const int algoIndex = algo_->findData(QString::fromStdString(model_.resizeAlgo));
    algo_->setCurrentIndex(algoIndex >= 0 ? algoIndex : 0);

    auto* form = new QFormLayout(this);
    if (info_.width > 0 && info_.height > 0) {
        form->addRow(tr("Source size:"),
                     new QLabel(tr("%1 x %2 px").arg(info_.width).arg(info_.height), this));
    }
    form->addRow(tr("Width:"), width_);
    form->addRow(tr("Height:"), height_);
    form->addRow(keepAspect_);
    form->addRow(tr("Algorithm:"), algo_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ResizeDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ResizeDialog::reject);

    connect(width_, &QSpinBox::valueChanged, this,
            [this](int value) { syncHeightToWidth(value); });
}

void ResizeDialog::syncHeightToWidth(int width) {
    if (!keepAspect_->isChecked() || info_.width <= 0 || info_.height <= 0)
        return;

    const double ratio = static_cast<double>(info_.height) / static_cast<double>(info_.width);
    const int height = static_cast<int>(std::lround(static_cast<double>(width) * ratio));

    const QSignalBlocker blocker(height_);
    height_->setValue(std::clamp(height, height_->minimum(), height_->maximum()));
}

void ResizeDialog::accept() {
    model_.resizeEnabled = true;
    model_.resizeW = width_->value();
    model_.resizeH = height_->value();

    const QString algo = algo_->currentData().toString();
    model_.resizeAlgo = (algo.isEmpty() ? algo_->currentText() : algo).toStdString();

    QDialog::accept();
}

} // namespace wc
