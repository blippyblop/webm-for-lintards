#pragma once
// A grid of filter buttons (opens dialogs) plus Loop/Reverse toggles.

#include <QWidget>

class QCheckBox;

namespace wc {

enum class FilterKind {
    Trim, MultiTrim, Crop, Resize, Rotate, Rate, DynamicRate,
    Caption, Subtitle, Overlay, Fade, Eq, Denoise, Deinterlace,
    Stabilization, DelayAudio, Loop, Reverse
};

class FilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit FilterPanel(QWidget* parent = nullptr);

    void setLoop(bool on);
    void setReverse(bool on);
    bool loop() const;
    bool reverse() const;

signals:
    void filterClicked(wc::FilterKind kind);
    void loopToggled(bool on);
    void reverseToggled(bool on);

private:
    QCheckBox* loop_ = nullptr;
    QCheckBox* reverse_ = nullptr;
};

} // namespace wc
