#include "dynamicratedialog.h"
#include "ui/scrubpreview.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace wc {

DynamicRateDialog::DynamicRateDialog(FilterModel& model, const VideoInfo& info,
                                     QWidget* parent, const QString& inputPath,
                                     const QString& ffmpegPath)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Dynamic rate"));

    maxTime_ = (info_.durationSeconds > 0.0) ? info_.durationSeconds : 1.0e6;

    enabledBox_ = new QCheckBox(tr("Enable dynamic rate (speed ramp)"), this);
    enabledBox_->setChecked(model_.dynamicRateEnabled);

    table_ = new QTableWidget(0, 2, this);
    table_->setHorizontalHeaderLabels({tr("Time (s)"), tr("Rate")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto* addBtn = new QPushButton(tr("Add"), this);
    auto* removeBtn = new QPushButton(tr("Remove"), this);

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        // Default new keyframe to the current end of the table (or t=0).
        double t = 0.0;
        if (table_->rowCount() > 0) {
            if (auto* last = qobject_cast<QDoubleSpinBox*>(
                    table_->cellWidget(table_->rowCount() - 1, 0))) {
                t = std::min(last->value() + 1.0, maxTime_);
            }
        }
        addRow(t, 1.0);
        table_->selectRow(table_->rowCount() - 1);
    });
    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentRow();
        if (row >= 0) table_->removeRow(row);
    });

    auto* addAtPlayhead = new QPushButton(tr("Add at current time"), this);
    connect(addAtPlayhead, &QPushButton::clicked, this, [this]() {
        addRow(scrub_ ? scrub_->currentTime() : 0.0, 1.0);
        table_->selectRow(table_->rowCount() - 1);
    });

    auto* rowButtons = new QHBoxLayout;
    rowButtons->addWidget(addBtn);
    rowButtons->addWidget(removeBtn);
    rowButtons->addWidget(addAtPlayhead);
    rowButtons->addStretch(1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DynamicRateDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DynamicRateDialog::reject);

    smoothBox_ = new QCheckBox(tr("Smooth between keyframes"), this);
    smoothBox_->setChecked(model_.dynamicRateSmoothing);
    overlapSpin_ = new QDoubleSpinBox(this);
    overlapSpin_->setRange(0.0, 10.0);
    overlapSpin_->setDecimals(2);
    overlapSpin_->setSingleStep(0.1);
    overlapSpin_->setSuffix(tr(" s"));
    overlapSpin_->setValue(model_.dynamicRateSmoothingOverlap);
    auto* smoothRow = new QHBoxLayout;
    smoothRow->addWidget(smoothBox_);
    smoothRow->addWidget(new QLabel(tr("Overlap:"), this));
    smoothRow->addWidget(overlapSpin_);
    smoothRow->addStretch(1);

    auto* layout = new QVBoxLayout(this);
    scrub_ = new ScrubPreview(this);
    if (!inputPath.isEmpty()) scrub_->setSource(inputPath, info_, ffmpegPath);
    scrub_->setFilters(&model_);
    layout->addWidget(scrub_);
    layout->addWidget(enabledBox_);
    layout->addWidget(table_, 1);
    layout->addLayout(smoothRow);
    layout->addLayout(rowButtons);
    layout->addWidget(buttons);

    table_->setEnabled(model_.dynamicRateEnabled);
    connect(enabledBox_, &QCheckBox::toggled, table_, &QWidget::setEnabled);
    // Row buttons should follow the checkbox as well.
    connect(enabledBox_, &QCheckBox::toggled, addBtn, &QWidget::setEnabled);
    connect(enabledBox_, &QCheckBox::toggled, removeBtn, &QWidget::setEnabled);
    connect(enabledBox_, &QCheckBox::toggled, addAtPlayhead, &QWidget::setEnabled);
    addBtn->setEnabled(model_.dynamicRateEnabled);
    removeBtn->setEnabled(model_.dynamicRateEnabled);
    addAtPlayhead->setEnabled(model_.dynamicRateEnabled);

    for (const auto& kf : model_.dynamicRate) {
        addRow(kf.time, kf.rate);
    }

    resize(600, 700);
}

void DynamicRateDialog::addRow(double time, double rate) {
    const int row = table_->rowCount();
    table_->insertRow(row);

    auto* timeSpin = new QDoubleSpinBox(table_);
    timeSpin->setDecimals(3);
    timeSpin->setSingleStep(0.1);
    timeSpin->setRange(0.0, maxTime_);
    timeSpin->setValue(std::max(0.0, std::min(time, maxTime_)));

    auto* rateSpin = new QDoubleSpinBox(table_);
    rateSpin->setDecimals(3);
    rateSpin->setSingleStep(0.1);
    rateSpin->setRange(0.01, 100.0);
    rateSpin->setValue(rate > 0.0 ? rate : 1.0);

    table_->setCellWidget(row, 0, timeSpin);
    table_->setCellWidget(row, 1, rateSpin);
}

void DynamicRateDialog::accept() {
    std::vector<KeyframeRate> result;
    result.reserve(static_cast<size_t>(table_->rowCount()));
    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* timeSpin = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row, 0));
        auto* rateSpin = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row, 1));
        if (!timeSpin || !rateSpin) continue;
        const double rate = rateSpin->value();
        if (rate <= 0.0) continue;
        KeyframeRate kf;
        kf.time = timeSpin->value();
        kf.rate = rate;
        result.push_back(kf);
    }

    std::stable_sort(result.begin(), result.end(),
                     [](const KeyframeRate& a, const KeyframeRate& b) {
                         return a.time < b.time;
                     });

    model_.dynamicRateEnabled = enabledBox_->isChecked();
    model_.dynamicRate = std::move(result);
    model_.dynamicRateSmoothing = smoothBox_->isChecked();
    model_.dynamicRateSmoothingOverlap = overlapSpin_->value();

    QDialog::accept();
}

} // namespace wc
