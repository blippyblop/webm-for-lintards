#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace wc {

// Edits FilterModel::stabilization (vidstab parameters) in place.
class StabilizationDialog : public QDialog {
    Q_OBJECT
public:
    explicit StabilizationDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enableCheck_ = nullptr;
    QSpinBox* shakinessSpin_ = nullptr;
    QSpinBox* accuracySpin_ = nullptr;
    QSpinBox* smoothingSpin_ = nullptr;
    QSpinBox* zoomSpin_ = nullptr;
    QComboBox* interpolCombo_ = nullptr;
};

} // namespace wc
