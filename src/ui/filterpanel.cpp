#include "ui/filterpanel.h"

#include <QPushButton>
#include <QCheckBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QSignalBlocker>

namespace wc {

FilterPanel::FilterPanel(QWidget* parent) : QWidget(parent) {
    auto* group = new QGroupBox(tr("Filters"), this);
    auto* grid = new QGridLayout(group);

    struct Item { const char* label; FilterKind kind; };
    const Item items[] = {
        {"Trim", FilterKind::Trim},
        {"Multi-Trim", FilterKind::MultiTrim},
        {"Crop", FilterKind::Crop},
        {"Resize", FilterKind::Resize},
        {"Rotate", FilterKind::Rotate},
        {"Change Rate", FilterKind::Rate},
        {"Dynamic Speed", FilterKind::DynamicRate},
        {"Caption", FilterKind::Caption},
        {"Subtitle", FilterKind::Subtitle},
        {"Overlay", FilterKind::Overlay},
        {"Fade", FilterKind::Fade},
        {"Color / EQ", FilterKind::Eq},
        {"Denoise", FilterKind::Denoise},
        {"Deinterlace", FilterKind::Deinterlace},
        {"Stabilize", FilterKind::Stabilization},
        {"Delay Audio", FilterKind::DelayAudio},
    };

    int row = 0, col = 0;
    for (const Item& it : items) {
        auto* b = new QPushButton(QString::fromLatin1(it.label), group);
        b->setMinimumHeight(28);
        const FilterKind k = it.kind;
        connect(b, &QPushButton::clicked, this, [this, k]() { emit filterClicked(k); });
        grid->addWidget(b, row, col);
        if (++col == 2) { col = 0; ++row; }
    }

    loop_ = new QCheckBox(tr("Loop (forward + reverse)"), group);
    reverse_ = new QCheckBox(tr("Reverse"), group);
    grid->addWidget(loop_, row + 1, 0, 1, 2);
    grid->addWidget(reverse_, row + 2, 0, 1, 2);

    connect(loop_, &QCheckBox::toggled, this, &FilterPanel::loopToggled);
    connect(reverse_, &QCheckBox::toggled, this, &FilterPanel::reverseToggled);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(group);
    layout->setContentsMargins(0, 0, 0, 0);
}

void FilterPanel::setLoop(bool on) {
    QSignalBlocker b(loop_);
    loop_->setChecked(on);
}
void FilterPanel::setReverse(bool on) {
    QSignalBlocker b(reverse_);
    reverse_->setChecked(on);
}
bool FilterPanel::loop() const { return loop_ && loop_->isChecked(); }
bool FilterPanel::reverse() const { return reverse_ && reverse_->isChecked(); }

} // namespace wc
