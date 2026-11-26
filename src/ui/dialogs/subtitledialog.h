#pragma once
#include <QDialog>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;

namespace wc {

// Subtitle selection: pick an internal subtitle stream or an external file and
// choose whether subtitles are burned in (FilterModel::subtitle).
class SubtitleDialog : public QDialog {
    Q_OBJECT

public:
    explicit SubtitleDialog(FilterModel& model, const VideoInfo& info,
                            QWidget* parent = nullptr);

    void accept() override;

private:
    void selectExternal();

    FilterModel& model_;
    const VideoInfo& info_;

    QCheckBox* enabledBox_ = nullptr;
    QCheckBox* burnInBox_ = nullptr;
    QComboBox* streamCombo_ = nullptr;
    QLineEdit* pathEdit_ = nullptr;
    QPushButton* browseButton_ = nullptr;
    int externalIndex_ = -1;   // combo row holding the "External file…" entry
};

} // namespace wc
