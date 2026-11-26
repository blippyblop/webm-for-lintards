#include "ui/appliedfilterspanel.h"
#include "ui/filterjson.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QListView>
#include <QAbstractItemView>
#include <QStackedWidget>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPixmap>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileDialog>
#include <QFile>

namespace wc {

namespace {
QIcon makeIcon(const QString& text, const QColor& color) {
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(color);
    p.setPen(color.darker(140));
    p.drawRoundedRect(QRectF(4, 4, 56, 56), 12, 12);
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(16);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, text.left(4).toUpper());
    return QIcon(pm);
}
} // namespace

AppliedFiltersPanel::AppliedFiltersPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* header = new QHBoxLayout;
    header->addWidget(new QLabel(tr("Applied filters  (right-click to modify or delete)"), this));
    header->addStretch(1);
    jsonToggle_ = new QToolButton(this);
    jsonToggle_->setText(tr("JSON"));
    jsonToggle_->setCheckable(true);
    auto* saveBtn = new QPushButton(tr("Save…"), this);
    auto* loadBtn = new QPushButton(tr("Load…"), this);
    header->addWidget(jsonToggle_);
    header->addWidget(saveBtn);
    header->addWidget(loadBtn);
    layout->addLayout(header);

    stack_ = new QStackedWidget(this);

    list_ = new QListWidget(stack_);
    list_->setViewMode(QListView::IconMode);
    list_->setIconSize(QSize(64, 64));
    list_->setGridSize(QSize(104, 104));
    list_->setResizeMode(QListView::Adjust);
    list_->setMovement(QListView::Static);
    list_->setWordWrap(true);
    list_->setSpacing(4);
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setContextMenuPolicy(Qt::CustomContextMenu);
    stack_->addWidget(list_);

    auto* jsonPage = new QWidget(stack_);
    auto* jl = new QVBoxLayout(jsonPage);
    jsonView_ = new QPlainTextEdit(jsonPage);
    auto* applyBtn = new QPushButton(tr("Apply JSON"), jsonPage);
    jl->addWidget(jsonView_, 1);
    jl->addWidget(applyBtn, 0, Qt::AlignRight);
    stack_->addWidget(jsonPage);

    stack_->setCurrentIndex(0);
    layout->addWidget(stack_, 1);

    connect(jsonToggle_, &QToolButton::toggled, this, [this](bool on) {
        if (on) updateJsonView();
        stack_->setCurrentIndex(on ? 1 : 0);
    });
    connect(saveBtn, &QPushButton::clicked, this, &AppliedFiltersPanel::saveJson);
    connect(loadBtn, &QPushButton::clicked, this, &AppliedFiltersPanel::loadJson);
    connect(applyBtn, &QPushButton::clicked, this, &AppliedFiltersPanel::applyJson);

    connect(list_, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = list_->itemAt(pos);
        if (!item) return;
        const QVariant d = item->data(Qt::UserRole);
        if (!d.isValid()) return;
        const auto kind = static_cast<FilterKind>(d.toInt());
        QMenu menu(this);
        QAction* mod = menu.addAction(tr("Modify…"));
        QAction* del = menu.addAction(tr("Delete"));
        QAction* chosen = menu.exec(list_->viewport()->mapToGlobal(pos));
        if (chosen == mod) emit modifyRequested(kind);
        else if (chosen == del) emit deleteRequested(kind);
    });
    connect(list_, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        const QVariant d = item->data(Qt::UserRole);
        if (d.isValid()) emit modifyRequested(static_cast<FilterKind>(d.toInt()));
    });
}

void AppliedFiltersPanel::addItem(FilterKind kind, const QString& name, const QColor& color) {
    auto* item = new QListWidgetItem(makeIcon(name, color), name);
    item->setData(Qt::UserRole, static_cast<int>(kind));
    item->setToolTip(tr("%1 — right-click to modify or delete").arg(name));
    item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
    item->setSizeHint(QSize(100, 96));
    item->setFlags(item->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    list_->addItem(item);
}

void AppliedFiltersPanel::setModel(const FilterModel& f) {
    lastModel_ = f;
    list_->clear();

    if (f.trim) addItem(FilterKind::Trim, tr("Trim"), QColor(0x2e, 0x7d, 0x32));
    if (!f.multiTrim.empty()) addItem(FilterKind::MultiTrim, tr("Multi-Trim"), QColor(0x38, 0x8e, 0x3c));
    if (f.crop.enabled) addItem(FilterKind::Crop, tr("Crop"), QColor(0x6a, 0x1b, 0x9a));
    if (f.resizeEnabled) addItem(FilterKind::Resize, tr("Resize"), QColor(0x00, 0x69, 0x5c));
    if (f.rotateDegrees % 360 != 0) addItem(FilterKind::Rotate, tr("Rotate"), QColor(0x00, 0x69, 0x5c));
    if (f.rateFps) addItem(FilterKind::Rate, tr("Rate"), QColor(0x45, 0x27, 0xa0));
    if (f.dynamicRateEnabled && f.dynamicRate.size() >= 2)
        addItem(FilterKind::DynamicRate, tr("Dyn. Speed"), QColor(0x5e, 0x35, 0xb1));
    if (f.caption.enabled) addItem(FilterKind::Caption, tr("Caption"), QColor(0xc6, 0x28, 0x28));
    if (f.subtitle.enabled) addItem(FilterKind::Subtitle, tr("Subtitle"), QColor(0xc6, 0x28, 0x28));
    if (f.overlay.enabled) addItem(FilterKind::Overlay, tr("Overlay"), QColor(0xc6, 0x28, 0x28));
    if (f.fade.enabled) addItem(FilterKind::Fade, tr("Fade"), QColor(0x8e, 0x24, 0xaa));
    if (f.eq.enabled) addItem(FilterKind::Eq, tr("EQ"), QColor(0xef, 0x6c, 0x00));
    if (f.denoise) addItem(FilterKind::Denoise, tr("Denoise"), QColor(0x55, 0x6b, 0x2f));
    if (f.deinterlace) addItem(FilterKind::Deinterlace, tr("Deinterlace"), QColor(0x55, 0x6b, 0x2f));
    if (f.stabilization.enabled) addItem(FilterKind::Stabilization, tr("Stabilize"), QColor(0x00, 0x60, 0x7d));
    if (f.delayAudioMs != 0) addItem(FilterKind::DelayAudio, tr("Delay Audio"), QColor(0x45, 0x27, 0xa0));
    if (f.loop) addItem(FilterKind::Loop, tr("Loop"), QColor(0x6d, 0x4c, 0x41));
    if (f.reverse) addItem(FilterKind::Reverse, tr("Reverse"), QColor(0x6d, 0x4c, 0x41));

    if (list_->count() == 0) {
        auto* item = new QListWidgetItem(tr("(no filters applied)"));
        item->setFlags(Qt::NoItemFlags);
        list_->addItem(item);
    }
    updateJsonView();
}

void AppliedFiltersPanel::updateJsonView() {
    if (!jsonView_) return;
    const QJsonDocument doc(filterToJson(lastModel_));
    jsonView_->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
}

void AppliedFiltersPanel::saveJson() {
    const QString f = QFileDialog::getSaveFileName(this, tr("Save filters"), QString(),
                                                   tr("JSON (*.json);;All files (*)"));
    if (f.isEmpty()) return;
    QFile file(f);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    file.write(QJsonDocument(filterToJson(lastModel_)).toJson(QJsonDocument::Indented));
}

void AppliedFiltersPanel::loadJson() {
    const QString f = QFileDialog::getOpenFileName(this, tr("Load filters"), QString(),
                                                   tr("JSON (*.json);;All files (*)"));
    if (f.isEmpty()) return;
    QFile file(f);
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    FilterModel m;
    if (filterFromJson(doc.object(), m)) emit modelLoaded(m);
}

void AppliedFiltersPanel::applyJson() {
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonView_->toPlainText().toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return;
    FilterModel m;
    if (filterFromJson(doc.object(), m)) emit modelLoaded(m);
}

} // namespace wc
