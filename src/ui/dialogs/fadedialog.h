#pragma once
#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QDoubleSpinBox;

namespace wc {

// Video fade-in / fade-out durations (FilterModel::fade).
class FadeDialog : public QDialog {
    Q_OBJECT

public:
    explicit FadeDialog(FilterModel& model, const VideoInfo& info,
                        QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enabledBox_ = nullptr;
    QDoubleSpinBox* inSpin_ = nullptr;
    QDoubleSpinBox* outSpin_ = nullptr;
};

} // namespace wc
