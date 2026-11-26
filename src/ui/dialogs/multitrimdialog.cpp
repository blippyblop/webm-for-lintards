#include "ui/dialogs/multitrimdialog.h"
#include "ui/scrubpreview.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace wc {

namespace {
constexpr double kFallbackMaxSeconds = 1e6;

double maxSecondsFor(const VideoInfo& info) {
    return info.durationSeconds > 0.0 ? info.durationSeconds : kFallbackMaxSeconds;
}
int maxFrameFor(const VideoInfo& info) {
    const long long c = info.frameCount();
    return c > 0 ? static_cast<int>(c - 1) : 0;
}
} // namespace

MultiTrimDialog::MultiTrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent,
                                 const QString& inputPath, const QString& ffmpegPath)
    : QDialog(parent), model_(model), info_(info), maxSeconds_(maxSecondsFor(info)) {
    setWindowTitle(tr("Multi-Trim"));
    resize(600, 700);

    auto* layout = new QVBoxLayout(this);

    scrub_ = new ScrubPreview(this);
    if (!inputPath.isEmpty()) scrub_->setSource(inputPath, info_, ffmpegPath);
    scrub_->setFilters(&model_);
    scrub_->setFilters(&model_);
    layout->addWidget(scrub_);

    if (info_.durationSeconds > 0.0) {
        layout->addWidget(new QLabel(
            tr("Source: %1 frames @ %2 fps").arg(maxFrameFor(info) + 1)
                .arg(info_.frameRateValue(), 0, 'f', 2), this));
    }

    table_ = new QTableWidget(0, 2, this);
    table_->setHorizontalHeaderLabels({tr("Start frame"), tr("End frame")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);

    auto* buttonRow = new QHBoxLayout;
    addButton_ = new QPushButton(tr("Add segment"), this);
    removeButton_ = new QPushButton(tr("Remove segment"), this);
    buttonRow->addWidget(addButton_);
    buttonRow->addWidget(removeButton_);
    auto* setStartBtn = new QPushButton(tr("Start at playhead"), this);
    auto* setEndBtn = new QPushButton(tr("End at playhead"), this);
    buttonRow->addWidget(setStartBtn);
    buttonRow->addWidget(setEndBtn);
    buttonRow->addStretch(1);
    layout->addLayout(buttonRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &MultiTrimDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &MultiTrimDialog::reject);

    connect(addButton_, &QPushButton::clicked, this, [this]() {
        double startSec = 0.0;
        if (table_->rowCount() > 0) {
            if (auto* last = qobject_cast<QSpinBox*>(table_->cellWidget(table_->rowCount() - 1, 1)))
                startSec = info_.timeOfFrame(last->value());
        }
        if (startSec < 0.0 || startSec > maxSeconds_) startSec = 0.0;
        addRow(startSec, info_.durationSeconds);
    });
    connect(removeButton_, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentRow();
        if (row >= 0) table_->removeRow(row);
    });

    auto setAtPlayhead = [this](int column) {
        int row = table_->currentRow();
        if (row < 0 && table_->rowCount() > 0) row = table_->rowCount() - 1;
        if (row < 0) return;
        if (auto* spin = qobject_cast<QSpinBox*>(table_->cellWidget(row, column)))
            spin->setValue(info_.frameAt(scrub_ ? scrub_->currentTime() : 0.0));
    };
    connect(setStartBtn, &QPushButton::clicked, this, [setAtPlayhead]() { setAtPlayhead(0); });
    connect(setEndBtn, &QPushButton::clicked, this, [setAtPlayhead]() { setAtPlayhead(1); });

    for (const TrimRange& range : model_.multiTrim)
        addRow(range.start, range.end > range.start ? range.end : maxSeconds_);
}

void MultiTrimDialog::addRow(double start, double end) {
    const int maxF = maxFrameFor(info_);
    const int row = table_->rowCount();
    table_->insertRow(row);

    auto makeSpin = [this, maxF](double valueSec) {
        auto* spin = new QSpinBox(table_);
        spin->setRange(0, maxF);
        spin->setSuffix(tr(" f"));
        spin->setValue(std::clamp(info_.frameAt(valueSec), 0, maxF));
        spin->setFrame(false);
        return spin;
    };

    table_->setCellWidget(row, 0, makeSpin(start));
    table_->setCellWidget(row, 1, makeSpin(end));
}

void MultiTrimDialog::accept() {
    std::vector<TrimRange> ranges;
    const int maxF = maxFrameFor(info_);

    for (int row = 0; row < table_->rowCount(); ++row) {
        auto* startSpin = qobject_cast<QSpinBox*>(table_->cellWidget(row, 0));
        auto* endSpin = qobject_cast<QSpinBox*>(table_->cellWidget(row, 1));
        if (!startSpin || !endSpin) continue;

        const int sF = std::clamp(startSpin->value(), 0, maxF);
        const int eF = std::clamp(endSpin->value(), 0, maxF);
        if (eF <= sF) continue;
        const double startSec = info_.timeOfFrame(sF);
        const double endSec = (eF >= maxF) ? info_.durationSeconds : info_.timeOfFrame(eF + 1);
        if (endSec > startSec) ranges.push_back(TrimRange{startSec, endSec});
    }

    std::sort(ranges.begin(), ranges.end(),
              [](const TrimRange& a, const TrimRange& b) { return a.start < b.start; });
    model_.multiTrim = std::move(ranges);
    QDialog::accept();
}

} // namespace wc
