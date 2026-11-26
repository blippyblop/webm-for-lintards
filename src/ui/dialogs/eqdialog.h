#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QDoubleSpinBox;

namespace wc {

// Edits FilterModel::eq (gamma / saturation / contrast) in place.
class EqDialog : public QDialog {
    Q_OBJECT
public:
    explicit EqDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enableCheck_ = nullptr;
    QDoubleSpinBox* gammaSpin_ = nullptr;
    QDoubleSpinBox* saturationSpin_ = nullptr;
    QDoubleSpinBox* contrastSpin_ = nullptr;
};

} // namespace wc
