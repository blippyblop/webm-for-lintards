#pragma once

#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QSpinBox;

namespace wc {

// Edits FilterModel::delayAudioMs in place.
class DelayAudioDialog : public QDialog {
    Q_OBJECT
public:
    explicit DelayAudioDialog(FilterModel& model, const VideoInfo& info, QWidget* parent = nullptr);

    void accept() override;

private:
    FilterModel& model_;
    const VideoInfo& info_;

    QSpinBox* delaySpin_ = nullptr;
};

} // namespace wc
