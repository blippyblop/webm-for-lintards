#pragma once
#include <QDialog>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QTableWidget;
class QDoubleSpinBox;

namespace wc {

class ScrubPreview;

// Editing UI for FilterModel::dynamicRate (speed-ramp keyframes). The table
// holds one row per keyframe with an editable Time(s) / Rate pair; rows are
// committed on accept() (sorted by time, non-positive rates dropped).
class DynamicRateDialog : public QDialog {
    Q_OBJECT

public:
    explicit DynamicRateDialog(FilterModel& model, const VideoInfo& info,
                               QWidget* parent = nullptr, const QString& inputPath = QString(),
                               const QString& ffmpegPath = QString());

    void accept() override;

private:
    void addRow(double time, double rate);

    FilterModel& model_;
    const VideoInfo& info_;

    ScrubPreview* scrub_ = nullptr;
    QCheckBox* enabledBox_ = nullptr;
    QCheckBox* smoothBox_ = nullptr;
    QDoubleSpinBox* overlapSpin_ = nullptr;
    QTableWidget* table_ = nullptr;
    double maxTime_ = 1.0;
};

} // namespace wc
