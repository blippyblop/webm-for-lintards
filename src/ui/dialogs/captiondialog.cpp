#include "captiondialog.h"
#include "ui/scrubpreview.h"

#include <QCheckBox>
#include <QColor>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFont>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

namespace wc {

namespace {

// Resolve a colour name (e.g. "white", "#112233") to a QColor, falling back to
// the supplied default when the model holds something Qt cannot parse.
QColor parseColor(const std::string& name, const QColor& fallback) {
    QColor c(QString::fromStdString(name));
    return c.isValid() ? c : fallback;
}

} // namespace

CaptionDialog::CaptionDialog(FilterModel& model, const VideoInfo& info,
                             QWidget* parent, const QString& inputPath,
                             const QString& ffmpegPath)
    : QDialog(parent),
      model_(model),
      info_(info),
      color_(model.caption.color),
      borderColor_(model.caption.borderColor) {
    setWindowTitle(tr("Caption"));

    const double maxTime = (info_.durationSeconds > 0.0) ? info_.durationSeconds : 1.0e6;

    enabledBox_ = new QCheckBox(tr("Enable caption"), this);
    enabledBox_->setChecked(model_.caption.enabled);

    textEdit_ = new QPlainTextEdit(this);
    textEdit_->setPlainText(QString::fromStdString(model_.caption.text));
    textEdit_->setTabChangesFocus(true);
    textEdit_->setMinimumHeight(70);

    fontCombo_ = new QFontComboBox(this);
    fontCombo_->setCurrentFont(QFont(QString::fromStdString(model_.caption.fontFamily)));

    sizeSpin_ = new QSpinBox(this);
    sizeSpin_->setRange(1, 300);
    sizeSpin_->setValue(model_.caption.fontSize > 0 ? model_.caption.fontSize : 24);

    xSpin_ = new QSpinBox(this);
    xSpin_->setRange(-4096, 4096);
    xSpin_->setValue(model_.caption.x);

    ySpin_ = new QSpinBox(this);
    ySpin_->setRange(-4096, 4096);
    ySpin_->setValue(model_.caption.y);

    startSpin_ = new QDoubleSpinBox(this);
    startSpin_->setDecimals(3);
    startSpin_->setSingleStep(0.1);
    startSpin_->setRange(0.0, maxTime);
    startSpin_->setValue(std::min(std::max(0.0, model_.caption.start), maxTime));

    endSpin_ = new QDoubleSpinBox(this);
    endSpin_->setDecimals(3);
    endSpin_->setSingleStep(0.1);
    endSpin_->setRange(0.0, maxTime);
    endSpin_->setValue(std::min(std::max(0.0, model_.caption.end), maxTime));

    colorButton_ = new QPushButton(this);
    colorButton_->setMinimumWidth(120);
    auto* colorRow = new QHBoxLayout;
    colorRow->addWidget(colorButton_);
    colorRow->addStretch(1);
    connect(colorButton_, &QPushButton::clicked, this, [this]() {
        pickColor(color_, colorButton_, tr("Text color"));
    });

    borderColorButton_ = new QPushButton(this);
    borderColorButton_->setMinimumWidth(120);
    auto* borderRow = new QHBoxLayout;
    borderRow->addWidget(borderColorButton_);
    borderRow->addStretch(1);
    connect(borderColorButton_, &QPushButton::clicked, this, [this]() {
        pickColor(borderColor_, borderColorButton_, tr("Border color"));
    });

    auto* form = new QFormLayout;
    form->addRow(tr("Text:"), textEdit_);
    form->addRow(tr("Font:"), fontCombo_);
    form->addRow(tr("Font size:"), sizeSpin_);
    form->addRow(tr("X:"), xSpin_);
    form->addRow(tr("Y:"), ySpin_);
    form->addRow(tr("Start (s):"), startSpin_);
    form->addRow(tr("End (s):"), endSpin_);
    form->addRow(tr("Text color:"), colorRow);
    form->addRow(tr("Border color:"), borderRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &CaptionDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &CaptionDialog::reject);

    auto* layout = new QVBoxLayout(this);
    scrub_ = new ScrubPreview(this);
    if (!inputPath.isEmpty()) scrub_->setSource(inputPath, info_, ffmpegPath);
    layout->addWidget(scrub_);
    layout->addWidget(enabledBox_);
    layout->addLayout(form);

    auto* scrubRow = new QHBoxLayout;
    auto* sBtn = new QPushButton(tr("Start = playhead"), this);
    auto* eBtn = new QPushButton(tr("End = playhead"), this);
    scrubRow->addWidget(sBtn);
    scrubRow->addWidget(eBtn);
    scrubRow->addStretch(1);
    layout->addLayout(scrubRow);
    connect(sBtn, &QPushButton::clicked, this, [this]() { startSpin_->setValue(scrub_->currentTime()); });
    connect(eBtn, &QPushButton::clicked, this, [this]() { endSpin_->setValue(scrub_->currentTime()); });

    layout->addWidget(buttons);

    // Reflect the stored colours on the buttons.
    auto applySwatch = [](QPushButton* button, const QColor& c, const QString& label) {
        const bool dark = c.lightness() < 128;
        button->setText(QStringLiteral("%1 (%2)").arg(label, c.name()));
        button->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                                  .arg(c.name(), dark ? QStringLiteral("white") : QStringLiteral("black")));
    };
    applySwatch(colorButton_, parseColor(color_, Qt::white), tr("Text color"));
    applySwatch(borderColorButton_, parseColor(borderColor_, Qt::black),
                tr("Border color"));

    resize(640, 780);
}

void CaptionDialog::pickColor(std::string& target, QPushButton* button,
                              const QString& title) {
    const QColor initial = parseColor(target, Qt::white);
    const QColor chosen = QColorDialog::getColor(initial, this, title);
    if (!chosen.isValid()) return;

    target = chosen.name().toStdString();

    const bool dark = chosen.lightness() < 128;
    button->setText(QStringLiteral("%1 (%2)").arg(title, chosen.name()));
    button->setStyleSheet(QStringLiteral("background-color: %1; color: %2;")
                              .arg(chosen.name(), dark ? QStringLiteral("white") : QStringLiteral("black")));
}

void CaptionDialog::accept() {
    const QString text = textEdit_->toPlainText();

    Caption& c = model_.caption;
    c.text = text.toStdString();
    c.fontFamily = fontCombo_->currentFont().family().toStdString();
    if (c.fontFamily.empty()) c.fontFamily = "sans";
    c.fontSize = sizeSpin_->value();
    c.x = xSpin_->value();
    c.y = ySpin_->value();
    c.start = startSpin_->value();
    c.end = endSpin_->value();
    c.color = color_.empty() ? std::string("white") : color_;
    c.borderColor = borderColor_.empty() ? std::string("black") : borderColor_;
    c.enabled = enabledBox_->isChecked() && !text.isEmpty();

    QDialog::accept();
}

} // namespace wc
