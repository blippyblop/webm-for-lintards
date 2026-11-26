#include "ui/previewcanvas.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>

namespace wc {

PreviewCanvas::PreviewCanvas(QWidget* parent) : QWidget(parent) {
    setMinimumSize(320, 180);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
}

void PreviewCanvas::setImage(const QImage& img) {
    image_ = img;
    update();
}
void PreviewCanvas::clearImage() {
    image_ = QImage();
    update();
}
void PreviewCanvas::setCropRect(const QRect& r) {
    crop_ = r;
    update();
}
void PreviewCanvas::setCropEditable(bool editable) {
    cropEditable_ = editable;
    setCursor(editable ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

QRect PreviewCanvas::imageDrawRect() const {
    const QRect area = contentsRect();
    if (image_.isNull() || image_.width() <= 0 || image_.height() <= 0) return area;
    QSize scaled = image_.size();
    scaled.scale(area.size(), Qt::KeepAspectRatio);
    const int x = area.x() + (area.width() - scaled.width()) / 2;
    const int y = area.y() + (area.height() - scaled.height()) / 2;
    return QRect(x, y, scaled.width(), scaled.height());
}

QRect PreviewCanvas::cropWidgetRect() const {
    const QRect d = imageDrawRect();
    if (image_.isNull() || d.width() <= 0 || crop_.width() <= 0 || crop_.height() <= 0)
        return QRect();
    const double sx = static_cast<double>(d.width()) / image_.width();
    const double sy = static_cast<double>(d.height()) / image_.height();
    return QRect(static_cast<int>(d.x() + crop_.x() * sx),
                 static_cast<int>(d.y() + crop_.y() * sy),
                 static_cast<int>(crop_.width() * sx),
                 static_cast<int>(crop_.height() * sy));
}

QPoint PreviewCanvas::widgetToImage(const QPoint& w) const {
    const QRect d = imageDrawRect();
    if (image_.isNull() || d.width() <= 0 || d.height() <= 0) return QPoint(0, 0);
    const double sx = static_cast<double>(image_.width()) / d.width();
    const double sy = static_cast<double>(image_.height()) / d.height();
    int ix = static_cast<int>(std::lround((w.x() - d.x()) * sx));
    int iy = static_cast<int>(std::lround((w.y() - d.y()) * sy));
    ix = std::clamp(ix, 0, image_.width() - 1);
    iy = std::clamp(iy, 0, image_.height() - 1);
    return QPoint(ix, iy);
}

void PreviewCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(18, 18, 20));

    if (image_.isNull()) {
        p.setPen(QColor(140, 140, 145));
        p.drawText(rect(), Qt::AlignCenter, tr("Drop a video here or use File > Open"));
        return;
    }

    const QRect d = imageDrawRect();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(d, image_);

    if (crop_.width() <= 0 || crop_.height() <= 0) return;
    const QRect cr = cropWidgetRect().intersected(d);
    if (cr.width() <= 0 || cr.height() <= 0) return;

    // Shade outside the crop rectangle.
    const QColor shade(0, 0, 0, 150);
    p.fillRect(QRect(d.topLeft(), QPoint(d.right(), cr.top() - 1)), shade);
    p.fillRect(QRect(QPoint(d.left(), cr.bottom() + 1), d.bottomRight()), shade);
    p.fillRect(QRect(QPoint(d.left(), cr.top()), QPoint(cr.left() - 1, cr.bottom())), shade);
    p.fillRect(QRect(QPoint(cr.right() + 1, cr.top()), QPoint(d.right(), cr.bottom())), shade);

    // Border + handles.
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(QColor(0, 200, 120), 2));
    p.setBrush(Qt::NoBrush);
    p.drawRect(cr);

    p.setBrush(QColor(0, 200, 120));
    p.setPen(Qt::NoPen);
    const int h = 7;
    const QPoint corners[8] = {
        cr.topLeft(), cr.topRight(), cr.bottomLeft(), cr.bottomRight(),
        QPoint(cr.center().x(), cr.top()), QPoint(cr.center().x(), cr.bottom()),
        QPoint(cr.left(), cr.center().y()), QPoint(cr.right(), cr.center().y())};
    for (const QPoint& c : corners) p.drawRect(QRect(c.x() - h / 2, c.y() - h / 2, h, h));
}

PreviewCanvas::Handle PreviewCanvas::hitTest(const QPoint& w) const {
    const QRect r = cropWidgetRect();
    if (r.isNull()) return None;
    const int tol = 8;
    const bool nearL = std::abs(w.x() - r.left()) <= tol && w.y() >= r.top() - tol && w.y() <= r.bottom() + tol;
    const bool nearR = std::abs(w.x() - r.right()) <= tol && w.y() >= r.top() - tol && w.y() <= r.bottom() + tol;
    const bool nearT = std::abs(w.y() - r.top()) <= tol && w.x() >= r.left() - tol && w.x() <= r.right() + tol;
    const bool nearB = std::abs(w.y() - r.bottom()) <= tol && w.x() >= r.left() - tol && w.x() <= r.right() + tol;
    if (nearL && nearT) return TopLeft;
    if (nearR && nearT) return TopRight;
    if (nearL && nearB) return BottomLeft;
    if (nearR && nearB) return BottomRight;
    if (nearL) return Left;
    if (nearR) return Right;
    if (nearT) return Top;
    if (nearB) return Bottom;
    if (r.contains(w)) return Move;
    return None;
}

void PreviewCanvas::mousePressEvent(QMouseEvent* e) {
    if (!cropEditable_ || image_.isNull() || e->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(e);
        return;
    }
    handle_ = hitTest(e->pos());
    if (handle_ == None) return;
    dragging_ = true;
    pressImg_ = widgetToImage(e->pos());
    pressRect_ = crop_;
}

void PreviewCanvas::mouseMoveEvent(QMouseEvent* e) {
    if (!cropEditable_ || image_.isNull()) { QWidget::mouseMoveEvent(e); return; }
    if (!dragging_) {
        switch (hitTest(e->pos())) {
            case Left: case Right: setCursor(Qt::SizeHorCursor); break;
            case Top: case Bottom: setCursor(Qt::SizeVerCursor); break;
            case TopLeft: case BottomRight: setCursor(Qt::SizeFDiagCursor); break;
            case TopRight: case BottomLeft: setCursor(Qt::SizeBDiagCursor); break;
            case Move: setCursor(Qt::SizeAllCursor); break;
            default: setCursor(Qt::CrossCursor); break;
        }
        return;
    }

    const QPoint cur = widgetToImage(e->pos());
    const QPoint delta = cur - pressImg_;
    QRect nr = pressRect_;
    const int dx = delta.x();
    const int dy = delta.y();
    switch (handle_) {
        case Move: nr.translate(dx, dy); break;
        case Left: nr.setLeft(pressRect_.left() + dx); break;
        case Right: nr.setRight(pressRect_.right() + dx); break;
        case Top: nr.setTop(pressRect_.top() + dy); break;
        case Bottom: nr.setBottom(pressRect_.bottom() + dy); break;
        case TopLeft: nr.setLeft(pressRect_.left() + dx); nr.setTop(pressRect_.top() + dy); break;
        case TopRight: nr.setRight(pressRect_.right() + dx); nr.setTop(pressRect_.top() + dy); break;
        case BottomLeft: nr.setLeft(pressRect_.left() + dx); nr.setBottom(pressRect_.bottom() + dy); break;
        case BottomRight: nr.setRight(pressRect_.right() + dx); nr.setBottom(pressRect_.bottom() + dy); break;
        default: return;
    }
    nr = nr.normalized().intersected(QRect(0, 0, image_.width(), image_.height()));
    if (nr.width() < 8 || nr.height() < 8) return;
    crop_ = nr;
    update();
}

void PreviewCanvas::mouseReleaseEvent(QMouseEvent* e) {
    if (dragging_) {
        dragging_ = false;
        handle_ = None;
        emit cropChanged(crop_);
    }
    QWidget::mouseReleaseEvent(e);
}

void PreviewCanvas::clampCrop() {
    if (image_.isNull() || crop_.width() <= 0 || crop_.height() <= 0) return;
    crop_ = crop_.normalized().intersected(QRect(0, 0, image_.width(), image_.height()));
}

void PreviewCanvas::keyPressEvent(QKeyEvent* e) {
    if (!cropEditable_ || image_.isNull() || crop_.width() <= 0) {
        QWidget::keyPressEvent(e);
        return;
    }
    const int step = (e->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    const bool resize = e->modifiers() & Qt::AltModifier;
    const bool keepAspect = e->modifiers() & Qt::ControlModifier;

    int dx = 0, dy = 0;
    switch (e->key()) {
        case Qt::Key_Left: dx = -step; break;
        case Qt::Key_Right: dx = step; break;
        case Qt::Key_Up: dy = -step; break;
        case Qt::Key_Down: dy = step; break;
        default: QWidget::keyPressEvent(e); return;
    }

    QRect nr = crop_;
    if (resize) {
        nr.setWidth(std::max(8, nr.width() + dx));
        if (keepAspect && crop_.width() > 0) {
            const double aspect = static_cast<double>(crop_.height()) / crop_.width();
            nr.setHeight(std::max(8, static_cast<int>(std::lround(nr.width() * aspect))));
        } else {
            nr.setHeight(std::max(8, nr.height() + dy));
        }
    } else {
        nr.translate(dx, dy);
    }
    crop_ = nr;
    clampCrop();
    update();
    emit cropChanged(crop_);
}

} // namespace wc
