#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;

namespace wc {

// Edits FilterModel::deinterlace in place.
class DeinterlaceDialog : public QDialog {
    Q_OBJECT
public:
    explicit DeinterlaceDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enableCheck_ = nullptr;
};

} // namespace wc
