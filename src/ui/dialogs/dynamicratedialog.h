#pragma once
#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QTableWidget;

namespace wc {

// Editing UI for FilterModel::dynamicRate (speed-ramp keyframes). The table
// holds one row per keyframe with an editable Time(s) / Rate pair; rows are
// committed on accept() (sorted by time, non-positive rates dropped).
class DynamicRateDialog : public QDialog {
    Q_OBJECT

public:
    explicit DynamicRateDialog(FilterModel& model, const VideoInfo& info,
                               QWidget* parent = nullptr);

    void accept() override;

private:
    void addRow(double time, double rate);

    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enabledBox_ = nullptr;
    QTableWidget* table_ = nullptr;
    double maxTime_ = 1.0;
};

} // namespace wc
