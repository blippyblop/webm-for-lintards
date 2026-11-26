#pragma once
// Custom-painted timeline: ruler, current-frame marker, trim-range shading.
// Click/drag seeks; the mouse wheel steps one frame at a time.

#include <QWidget>

namespace wc {

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setDuration(double seconds);
    void setFrameRate(double fps);
    void setCurrentTime(double seconds);
    void setTrimRange(double start, double end, bool enabled);

    double currentTime() const { return current_; }
    double duration() const { return duration_; }

signals:
    void seekRequested(double seconds);
    void frameStepRequested(int delta);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    QRect trackRect() const;
    double timeAtX(int x) const;
    int xAtTime(double s) const;

    double duration_ = 0.0;
    double fps_ = 30.0;
    double current_ = 0.0;
    double trimStart_ = 0.0;
    double trimEnd_ = 0.0;
    bool trimEnabled_ = false;
};

} // namespace wc
