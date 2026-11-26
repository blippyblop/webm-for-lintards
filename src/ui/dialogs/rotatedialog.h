#pragma once

#include <QDialog>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QButtonGroup;

namespace wc {

class ScrubPreview;

// Edits FilterModel::rotateDegrees (0/90/180/270).
class RotateDialog : public QDialog {
    Q_OBJECT
public:
    explicit RotateDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr,
                          const QString& inputPath = QString(), const QString& ffmpegPath = QString());

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    ScrubPreview* scrub_ = nullptr;
    QButtonGroup* group_ = nullptr;
};

} // namespace wc
