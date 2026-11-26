#pragma once
#include <QDialog>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QDoubleSpinBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace wc {

class ScrubPreview;

// Image overlay placement/timing (FilterModel::overlay).
class OverlayDialog : public QDialog {
    Q_OBJECT

public:
    explicit OverlayDialog(FilterModel& model, const VideoInfo& info,
                           QWidget* parent = nullptr, const QString& inputPath = QString(),
                           const QString& ffmpegPath = QString());

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    ScrubPreview* scrub_ = nullptr;
    QCheckBox* enabledBox_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    QSpinBox* xSpin_ = nullptr;
    QSpinBox* ySpin_ = nullptr;
    QDoubleSpinBox* startSpin_ = nullptr;
    QDoubleSpinBox* endSpin_ = nullptr;
};

} // namespace wc
