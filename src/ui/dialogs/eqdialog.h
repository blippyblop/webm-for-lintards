#pragma once

#include <QDialog>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QDoubleSpinBox;

namespace wc {

class ScrubPreview;

// Edits FilterModel::eq (gamma / saturation / contrast) in place.
class EqDialog : public QDialog {
    Q_OBJECT
public:
    explicit EqDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr,
                      const QString& inputPath = QString(), const QString& ffmpegPath = QString());

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    ScrubPreview* scrub_ = nullptr;
    QCheckBox* enableCheck_ = nullptr;
    QDoubleSpinBox* gammaSpin_ = nullptr;
    QDoubleSpinBox* saturationSpin_ = nullptr;
    QDoubleSpinBox* contrastSpin_ = nullptr;
};

} // namespace wc
