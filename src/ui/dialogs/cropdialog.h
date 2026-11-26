#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QSpinBox;

namespace wc {

// Edits FilterModel::crop. Width/height are rounded down to even numbers and
// clamped to the source dimensions on accept.
class CropDialog : public QDialog {
    Q_OBJECT
public:
    explicit CropDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enable_ = nullptr;
    QSpinBox* x_ = nullptr;
    QSpinBox* y_ = nullptr;
    QSpinBox* w_ = nullptr;
    QSpinBox* h_ = nullptr;
};

} // namespace wc
