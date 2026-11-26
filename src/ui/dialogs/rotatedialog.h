#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QButtonGroup;

namespace wc {

// Edits FilterModel::rotateDegrees (0/90/180/270).
class RotateDialog : public QDialog {
    Q_OBJECT
public:
    explicit RotateDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QButtonGroup* group_ = nullptr;
};

} // namespace wc
