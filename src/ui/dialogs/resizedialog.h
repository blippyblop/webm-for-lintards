#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QComboBox;
class QSpinBox;

namespace wc {

// Edits FilterModel::{resizeEnabled, resizeW, resizeH, resizeAlgo}. Accepting
// always enables resizing; the aspect-ratio checkbox keeps the height in sync
// with the width using the source dimensions.
class ResizeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ResizeDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    void syncHeightToWidth(int width);

    FilterModel& model_;
    const VideoInfo& info_;

    QSpinBox* width_ = nullptr;
    QSpinBox* height_ = nullptr;
    QCheckBox* keepAspect_ = nullptr;
    QComboBox* algo_ = nullptr;
};

} // namespace wc
