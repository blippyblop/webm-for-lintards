#include "subtitledialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QVariant>
#include <QVBoxLayout>

namespace wc {

SubtitleDialog::SubtitleDialog(FilterModel& model, const VideoInfo& info,
                               QWidget* parent)
    : QDialog(parent), model_(model), info_(info) {
    setWindowTitle(tr("Subtitles"));

    enabledBox_ = new QCheckBox(tr("Enable subtitles"), this);
    enabledBox_->setChecked(model_.subtitle.enabled);

    burnInBox_ = new QCheckBox(tr("Burn in"), this);
    burnInBox_->setChecked(model_.subtitle.burnIn);

    streamCombo_ = new QComboBox(this);
    for (const auto& stream : info_.streams) {
        if (stream.codecType != "subtitle") continue;

        QString label = tr("Stream %1").arg(stream.index);
        if (!stream.codecName.empty())
            label += QStringLiteral(" ") + QString::fromStdString(stream.codecName);
        if (!stream.language.empty())
            label += QStringLiteral(" ") + QString::fromStdString(stream.language);

        streamCombo_->addItem(label, QVariant(stream.index));
    }
    externalIndex_ = streamCombo_->count();
    streamCombo_->addItem(tr("External file…"), QVariant(-1));

    pathEdit_ = new QLineEdit(this);
    pathEdit_->setText(QString::fromStdString(model_.subtitle.path));
    pathEdit_->setPlaceholderText(tr("Path to .srt / .ass / .vtt file"));

    browseButton_ = new QPushButton(tr("Browse…"), this);

    auto* pathRow = new QHBoxLayout;
    pathRow->addWidget(pathEdit_, 1);
    pathRow->addWidget(browseButton_);

    auto* form = new QFormLayout;
    form->addRow(tr("Track:"), streamCombo_);
    form->addRow(tr("External:"), pathRow);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok |
                                             QDialogButtonBox::Cancel,
                                         this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SubtitleDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SubtitleDialog::reject);

    connect(browseButton_, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(
            this, tr("Select subtitle file"), pathEdit_->text(),
            tr("Subtitles (*.srt *.ass *.ssa *.vtt)"));
        if (file.isEmpty()) return;
        pathEdit_->setText(file);
        selectExternal();
    });

    // Choosing the external entry (or any stream) keeps the path in sync.
    connect(streamCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                if (index == externalIndex_) {
                    pathEdit_->setFocus();
                    pathEdit_->selectAll();
                }
            });

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(enabledBox_);
    layout->addLayout(form);
    layout->addWidget(burnInBox_);
    layout->addWidget(buttons);

    // Prefill: an external path wins; otherwise match the stored stream index.
    if (!model_.subtitle.path.empty()) {
        selectExternal();
    } else if (model_.subtitle.streamIndex >= 0) {
        const int idx = streamCombo_->findData(QVariant(model_.subtitle.streamIndex));
        if (idx >= 0) streamCombo_->setCurrentIndex(idx);
    } else {
        selectExternal();
    }

    resize(500, 240);
}

void SubtitleDialog::selectExternal() {
    if (externalIndex_ >= 0) streamCombo_->setCurrentIndex(externalIndex_);
}

void SubtitleDialog::accept() {
    SubtitleTrack& s = model_.subtitle;

    const QString path = pathEdit_->text().trimmed();
    if (!path.isEmpty()) {
        s.streamIndex = -1;
        s.path = path.toStdString();
    } else {
        s.path.clear();
        s.streamIndex = streamCombo_->currentData().toInt();
    }

    s.enabled = enabledBox_->isChecked();
    s.burnIn = burnInBox_->isChecked();

    QDialog::accept();
}

} // namespace wc
