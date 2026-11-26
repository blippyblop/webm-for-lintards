#pragma once

#include <QDialog>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QComboBox;

namespace wc {

class ScrubPreview;

// Edits FilterModel::denoise / denoiseStrength in place.
class DenoiseDialog : public QDialog {
    Q_OBJECT
public:
    explicit DenoiseDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr,
                           const QString& inputPath = QString(), const QString& ffmpegPath = QString());

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    ScrubPreview* scrub_ = nullptr;
    QCheckBox* enableCheck_ = nullptr;
    QComboBox* strengthCombo_ = nullptr;
};

} // namespace wc
