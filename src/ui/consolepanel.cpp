#include "ui/consolepanel.h"

#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFont>
#include <QFontDatabase>

namespace wc {

ConsolePanel::ConsolePanel(QWidget* parent) : QWidget(parent) {
    log_ = new QPlainTextEdit(this);
    log_->setReadOnly(true);
    log_->setMaximumBlockCount(5000);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    log_->setFont(mono);

    progress_ = new QProgressBar(this);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setTextVisible(true);

    cancel_ = new QPushButton(tr("Cancel"), this);
    cancel_->setEnabled(false);

    auto* bar = new QHBoxLayout;
    bar->addWidget(progress_, 1);
    bar->addWidget(cancel_);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(log_, 1);
    layout->addLayout(bar);
    layout->setContentsMargins(0, 0, 0, 0);

    connect(cancel_, &QPushButton::clicked, this, &ConsolePanel::cancelRequested);
}

void ConsolePanel::appendLog(const QString& line) {
    log_->appendPlainText(line);
}

void ConsolePanel::clearLog() {
    log_->clear();
}

void ConsolePanel::setProgress(double percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    progress_->setValue(static_cast<int>(percent));
}

void ConsolePanel::setBusy(bool busy) {
    cancel_->setEnabled(busy);
    if (!busy) progress_->setValue(0);
}

} // namespace wc
