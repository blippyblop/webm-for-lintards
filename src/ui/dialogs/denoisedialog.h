#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QComboBox;

namespace wc {

// Edits FilterModel::denoise / denoiseStrength in place.
class DenoiseDialog : public QDialog {
    Q_OBJECT
public:
    explicit DenoiseDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enableCheck_ = nullptr;
    QComboBox* strengthCombo_ = nullptr;
};

} // namespace wc
