#include "ui/dialogs/multitrimdialog.h"
#include "ui/dialogs/trimdialog.h"

#include <algorithm>
#include <utility>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace wc {

MultiTrimDialog::MultiTrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent,
                                 const QString& inputPath, const QString& ffmpegPath)
    : QDialog(parent), model_(model), info_(info), inputPath_(inputPath), ffmpegPath_(ffmpegPath) {
    setWindowTitle(tr("Multi-Trim"));
    resize(560, 480);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Segments are concatenated in the order shown. Use \"Add segment…\" to configure one in the trim scrubber."),
        this));

    list_ = new QListWidget(this);
    layout->addWidget(list_, 1);

    auto* row = new QHBoxLayout;
    addBtn_ = new QPushButton(tr("Add segment…"), this);
    editBtn_ = new QPushButton(tr("Edit…"), this);
    removeBtn_ = new QPushButton(tr("Remove"), this);
    upBtn_ = new QPushButton(tr("Move up"), this);
    downBtn_ = new QPushButton(tr("Move down"), this);
    row->addWidget(addBtn_);
    row->addWidget(editBtn_);
    row->addWidget(removeBtn_);
    row->addSpacing(12);
    row->addWidget(upBtn_);
    row->addWidget(downBtn_);
    row->addStretch(1);
    layout->addLayout(row);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &MultiTrimDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &MultiTrimDialog::reject);

    segments_ = model_.multiTrim;
    refreshList();

    connect(addBtn_, &QPushButton::clicked, this, &MultiTrimDialog::addSegmentFromTrim);
    connect(editBtn_, &QPushButton::clicked, this, &MultiTrimDialog::editSelected);
    connect(removeBtn_, &QPushButton::clicked, this, [this]() {
        const int r = list_->currentRow();
        if (r >= 0 && r < static_cast<int>(segments_.size())) {
            segments_.erase(segments_.begin() + r);
            refreshList();
            if (r < list_->count()) list_->setCurrentRow(r);
        }
    });
    connect(upBtn_, &QPushButton::clicked, this, [this]() {
        const int r = list_->currentRow();
        if (r > 0 && r < static_cast<int>(segments_.size())) {
            std::swap(segments_[r], segments_[r - 1]);
            refreshList();
            list_->setCurrentRow(r - 1);
        }
    });
    connect(downBtn_, &QPushButton::clicked, this, [this]() {
        const int r = list_->currentRow();
        if (r >= 0 && r + 1 < static_cast<int>(segments_.size())) {
            std::swap(segments_[r], segments_[r + 1]);
            refreshList();
            list_->setCurrentRow(r + 1);
        }
    });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem*) { editSelected(); });
}

void MultiTrimDialog::refreshList() {
    const int cur = list_->currentRow();
    list_->clear();
    for (size_t i = 0; i < segments_.size(); ++i) {
        const TrimRange& s = segments_[i];
        const int sf = info_.frameAt(s.start);
        const int ef = info_.frameAt(s.end > s.start ? s.end : s.start);
        list_->addItem(tr("%1. frames %2–%3  (%4 s)")
                           .arg(static_cast<int>(i) + 1)
                           .arg(sf)
                           .arg(std::max(sf, ef - 1))
                           .arg(s.end - s.start, 0, 'f', 2));
    }
    if (cur >= 0 && cur < list_->count()) list_->setCurrentRow(cur);
}

void MultiTrimDialog::addSegmentFromTrim() {
    FilterModel tmp;
    TrimDialog dlg(tmp, info_, this, inputPath_, ffmpegPath_);
    dlg.setWindowTitle(tr("Add segment"));
    if (dlg.exec() == QDialog::Accepted && tmp.trim && tmp.trim->end > tmp.trim->start) {
        segments_.push_back(*tmp.trim);
        refreshList();
        list_->setCurrentRow(list_->count() - 1);
    }
}

void MultiTrimDialog::editSelected() {
    const int r = list_->currentRow();
    if (r < 0 || r >= static_cast<int>(segments_.size())) return;
    FilterModel tmp;
    tmp.trim = segments_[r];
    TrimDialog dlg(tmp, info_, this, inputPath_, ffmpegPath_);
    dlg.setWindowTitle(tr("Edit segment"));
    if (dlg.exec() == QDialog::Accepted) {
        if (tmp.trim && tmp.trim->end > tmp.trim->start)
            segments_[r] = *tmp.trim;
        else
            segments_.erase(segments_.begin() + r);
        refreshList();
    }
}

void MultiTrimDialog::accept() {
    model_.multiTrim = segments_;
    QDialog::accept();
}

} // namespace wc
