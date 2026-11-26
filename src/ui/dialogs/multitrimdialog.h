#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QPushButton;
class QTableWidget;

namespace wc {

// Edits FilterModel::multiTrim. Each table row is a [start, end] segment in
// seconds; rows with end <= start are dropped and the result is sorted by start.
class MultiTrimDialog : public QDialog {
    Q_OBJECT
public:
    explicit MultiTrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    void addRow(double start, double end);

    FilterModel& model_;
    const VideoInfo& info_;
    double maxSeconds_ = 1.0;

    QTableWidget* table_ = nullptr;
    QPushButton* addButton_ = nullptr;
    QPushButton* removeButton_ = nullptr;
};

} // namespace wc
