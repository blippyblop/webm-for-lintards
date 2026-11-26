#pragma once
#include <QDialog>
#include <QString>

#include "core/filtermodel.h"
#include "core/videoinfo.h"

class QCheckBox;
class QFontComboBox;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;

namespace wc {

class ScrubPreview;

// Burned-in text overlay (FilterModel::caption): single-run text with font,
// size, position, timing and colours.
class CaptionDialog : public QDialog {
    Q_OBJECT

public:
    explicit CaptionDialog(FilterModel& model, const VideoInfo& info,
                           QWidget* parent = nullptr, const QString& inputPath = QString(),
                           const QString& ffmpegPath = QString());

    void accept() override;

private:
    void pickColor(std::string& target, QPushButton* button, const QString& title);

    FilterModel& model_;
    const VideoInfo& info_;

    ScrubPreview* scrub_ = nullptr;
    QCheckBox* enabledBox_ = nullptr;
    QPlainTextEdit* textEdit_ = nullptr;
    QFontComboBox* fontCombo_ = nullptr;
    QSpinBox* sizeSpin_ = nullptr;
    QSpinBox* xSpin_ = nullptr;
    QSpinBox* ySpin_ = nullptr;
    QDoubleSpinBox* startSpin_ = nullptr;
    QDoubleSpinBox* endSpin_ = nullptr;
    QPushButton* colorButton_ = nullptr;
    QPushButton* borderColorButton_ = nullptr;

    std::string color_;
    std::string borderColor_;
};

} // namespace wc
