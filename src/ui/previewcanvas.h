#pragma once
// Displays the current preview frame letterboxed, and in crop mode draws a
// draggable/resizable crop rectangle. Mirrors the original PreviewFrame widget
// (mouse drag + keyboard modifiers).

#include <QWidget>
#include <QImage>
#include <QRect>
#include <QPoint>

namespace wc {

class PreviewCanvas : public QWidget {
    Q_OBJECT
public:
    explicit PreviewCanvas(QWidget* parent = nullptr);

    void setImage(const QImage& img);
    void clearImage();
    void setCropRect(const QRect& r);
    void setCropEditable(bool editable);
    QRect cropRect() const { return crop_; }

signals:
    void cropChanged(const QRect& rect);

protected:
    void paintEvent(QPaintEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;

private:
    enum Handle { None, Move, Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };
    QRect imageDrawRect() const;
    QRect cropWidgetRect() const;
    QPoint widgetToImage(const QPoint& w) const;
    Handle hitTest(const QPoint& w) const;
    void clampCrop();

    QImage image_;
    QRect crop_;
    bool cropEditable_ = false;
    bool dragging_ = false;
    Handle handle_ = None;
    QPoint pressImg_;
    QRect pressRect_;
};

} // namespace wc
