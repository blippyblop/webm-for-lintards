#include "ui/dialogs/multitrimdialog.h"
#include "ui/scrubpreview.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <QAbstractItemView>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QVBoxLayout>

namespace wc {

namespace {
constexpr double kFallbackMaxSeconds = 1e6;

double maxSecondsFor(const VideoInfo& info) {
    return info.durationSeconds > 0.0 ? info.durationSeconds : kFallbackMaxSeconds;
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
    layout->addWidget(scrub_);

    if (info_.durationSeconds > 0.0) {
        layout->addWidget(new QLabel(
            tr("Source duration: %1 s").arg(info_.durationSeconds, 0, 'f', 2), this));
    }

    table_ = new QTableWidget(0, 2, this);
    table_->setHorizontalHeaderLabels({tr("Start (s)"), tr("End (s)")});
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

    connect(setStartBtn, &QPushButton::clicked, this, [this]() {
        int row = table_->currentRow();
        if (row < 0 && table_->rowCount() > 0) row = table_->rowCount() - 1;
        if (row >= 0)
            if (auto* s = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row, 0)))
                s->setValue(scrub_ ? scrub_->currentTime() : 0.0);
    });
    connect(setEndBtn, &QPushButton::clicked, this, [this]() {
        int row = table_->currentRow();
        if (row < 0 && table_->rowCount() > 0) row = table_->rowCount() - 1;
        if (row >= 0)
            if (auto* s = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row, 1)))
                s->setValue(scrub_ ? scrub_->currentTime() : 0.0);
    });

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &MultiTrimDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &MultiTrimDialog::reject);

    connect(addButton_, &QPushButton::clicked, this, [this]() {
        double start = 0.0;
        if (table_->rowCount() > 0) {
            if (auto* last = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(table_->rowCount() - 1, 1)))
                start = last->value();
        }
        if (start < 0.0 || start > maxSeconds_)
            start = 0.0;
        addRow(start, maxSeconds_);
    });
    connect(removeButton_, &QPushButton::clicked, this, [this]() {
        const int row = table_->currentRow();
        if (row >= 0)
            table_->removeRow(row);
    });

    for (const TrimRange& range : model_.multiTrim) {
        addRow(range.start, range.end > range.start ? range.end : maxSeconds_);
    }
}

void MultiTrimDialog::addRow(double start, double end) {
    const int row = table_->rowCount();
    table_->insertRow(row);

    auto makeSpin = [this](double value) {
        auto* spin = new QDoubleSpinBox(table_);
        spin->setRange(0.0, maxSeconds_);
        spin->setDecimals(2);
        spin->setSingleStep(0.1);
        spin->setValue(std::clamp(value, 0.0, maxSeconds_));
        spin->setFrame(false);
        return spin;
    };

    table_->setCellWidget(row, 0, makeSpin(start));
    table_->setCellWidget(row, 1, makeSpin(end));
}

void MultiTrimDialog::accept() {
    std::vector<TrimRange> ranges;

    for (int row = 0; row < table_->rowCount(); ++row) {
        double start = 0.0;
        double end = 0.0;

        if (auto* spin = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row, 0)))
            start = spin->value();
        else if (QTableWidgetItem* item = table_->item(row, 0))
            start = item->text().toDouble();

        if (auto* spin = qobject_cast<QDoubleSpinBox*>(table_->cellWidget(row, 1)))
            end = spin->value();
        else if (QTableWidgetItem* item = table_->item(row, 1))
            end = item->text().toDouble();

        start = std::clamp(start, 0.0, maxSeconds_);
        end = std::clamp(end, 0.0, maxSeconds_);

        if (end > start)
            ranges.push_back(TrimRange{start, end});
    }

    std::sort(ranges.begin(), ranges.end(),
              [](const TrimRange& a, const TrimRange& b) { return a.start < b.start; });

    model_.multiTrim = std::move(ranges);

    QDialog::accept();
}

} // namespace wc
