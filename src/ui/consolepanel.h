#pragma once
// Read-only conversion log + progress bar + cancel button.

#include <QWidget>

class QPlainTextEdit;
class QProgressBar;
class QPushButton;

namespace wc {

class ConsolePanel : public QWidget {
    Q_OBJECT
public:
    explicit ConsolePanel(QWidget* parent = nullptr);

    void appendLog(const QString& line);
    void clearLog();
    void setProgress(double percent);
    void setBusy(bool busy);

signals:
    void cancelRequested();

private:
    QPlainTextEdit* log_ = nullptr;
    QProgressBar* progress_ = nullptr;
    QPushButton* cancel_ = nullptr;
};

} // namespace wc
