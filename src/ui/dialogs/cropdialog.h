#pragma once

#include <QDialog>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QSpinBox;

namespace wc {

class ScrubPreview;

// Edits FilterModel::crop. Width/height are rounded down to even numbers and
// clamped to the source dimensions on accept.
class CropDialog : public QDialog {
    Q_OBJECT
public:
    explicit CropDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr,
                        const QString& inputPath = QString(), const QString& ffmpegPath = QString());

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    ScrubPreview* scrub_ = nullptr;
    QCheckBox* enable_ = nullptr;
    QSpinBox* x_ = nullptr;
    QSpinBox* y_ = nullptr;
    QSpinBox* w_ = nullptr;
    QSpinBox* h_ = nullptr;
};

} // namespace wc
