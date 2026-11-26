#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QSpinBox;
class QSlider;
class QLabel;

namespace wc {

// Edits FilterModel::rateFps. Checking "Keep source rate" clears the optional
// frame-rate override.
class RateDialog : public QDialog {
    Q_OBJECT
public:
    explicit RateDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* keepSource_ = nullptr;
    QSpinBox* fps_ = nullptr;
    QSlider* percent_ = nullptr;
    QLabel* percentLabel_ = nullptr;
};

} // namespace wc
