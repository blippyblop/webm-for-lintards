#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QDoubleSpinBox;

namespace wc {

// Edits FilterModel::trim. Accepting with "Use whole clip" checked clears the
// optional trim range entirely.
class TrimDialog : public QDialog {
    Q_OBJECT
public:
    explicit TrimDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* wholeClip_ = nullptr;
    QDoubleSpinBox* start_ = nullptr;
    QDoubleSpinBox* end_ = nullptr;
};

} // namespace wc
