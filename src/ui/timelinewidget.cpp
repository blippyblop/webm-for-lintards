#include "ui/timelinewidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QToolTip>
#include <algorithm>
#include <cmath>

namespace wc {

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(56);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::StrongFocus);
}

void TimelineWidget::setDuration(double seconds) {
    duration_ = std::max(0.0, seconds);
    update();
}
void TimelineWidget::setFrameRate(double fps) {
    fps_ = fps > 0.0 ? fps : 30.0;
    update();
}
void TimelineWidget::setCurrentTime(double seconds) {
    current_ = std::max(0.0, seconds);
    update();
}
void TimelineWidget::setTrimRange(double start, double end, bool enabled) {
    trimStart_ = start;
    trimEnd_ = end;
    trimEnabled_ = enabled;
    update();
}

QRect TimelineWidget::trackRect() const {
    return contentsRect().adjusted(8, 10, -8, -22);
}

double TimelineWidget::timeAtX(int x) const {
    const QRect r = trackRect();
    if (duration_ <= 0.0 || r.width() <= 0) return 0.0;
    const double f = static_cast<double>(x - r.left()) / r.width();
    return std::clamp(f, 0.0, 1.0) * duration_;
}

int TimelineWidget::xAtTime(double s) const {
    const QRect r = trackRect();
    if (duration_ <= 0.0) return r.left();
    const double f = std::clamp(s / duration_, 0.0, 1.0);
    return r.left() + static_cast<int>(std::lround(f * r.width()));
}

void TimelineWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = trackRect();
    p.fillRect(rect(), QColor(28, 28, 30));

    // Track background.
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(52, 52, 58));
    p.drawRoundedRect(r, 4, 4);

    if (duration_ <= 0.0) {
        p.setPen(QColor(150, 150, 150));
        p.drawText(rect(), Qt::AlignCenter, tr("No media loaded"));
        return;
    }

    // Trim shading.
    if (trimEnabled_ && trimEnd_ > trimStart_) {
        const int x0 = xAtTime(trimStart_);
        const int x1 = xAtTime(trimEnd_);
        QRect tr(QPoint(x0, r.top()), QPoint(x1, r.bottom()));
        p.fillRect(tr, QColor(60, 130, 90, 150));
    }

    // Elapsed portion.
    {
        QRect el(r.topLeft(), QPoint(xAtTime(current_), r.bottom()));
        p.fillRect(el, QColor(70, 110, 160, 120));
    }

    // Ruler ticks.
    double step = 1.0;
    const double candidates[] = {0.04, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600};
    for (double c : candidates) {
        if (duration_ / c <= 12.0) { step = c; break; }
        step = c;
    }
    p.setPen(QColor(140, 140, 145));
    QFont f = p.font();
    f.setPointSizeF(8.0);
    p.setFont(f);
    for (double t = 0.0; t <= duration_ + 1e-9; t += step) {
        const int x = xAtTime(t);
        p.drawLine(x, r.top(), x, r.top() + 5);
        const int total = static_cast<int>(t + 0.5);
        const QString label = QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QChar('0'));
        p.drawText(x - 16, r.bottom() + 16, 40, 14, Qt::AlignCenter, label);
    }

    // Current-position marker.
    const int cx = xAtTime(current_);
    p.setPen(QPen(QColor(240, 90, 80), 2));
    p.drawLine(cx, r.top(), cx, r.bottom() + 2);
    p.setBrush(QColor(240, 90, 80));
    p.setPen(Qt::NoPen);
    QPolygon tri;
    tri << QPoint(cx - 5, r.bottom() + 2) << QPoint(cx + 5, r.bottom() + 2)
        << QPoint(cx, r.bottom() - 4);
    p.drawPolygon(tri);
}

void TimelineWidget::mousePressEvent(QMouseEvent* e) {
    setFocus(Qt::MouseFocusReason);
    if (e->button() == Qt::LeftButton && duration_ > 0.0) {
        emit seekRequested(timeAtX(e->pos().x()));
    }
}

void TimelineWidget::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton && duration_ > 0.0) {
        emit seekRequested(timeAtX(e->pos().x()));
    }
    if (duration_ > 0.0) {
        QToolTip::showText(e->globalPosition().toPoint(),
                           tr("%1 s (frame ~%2)")
                               .arg(timeAtX(e->pos().x()), 0, 'f', 2)
                               .arg(static_cast<int>(timeAtX(e->pos().x()) * fps_)),
                           this);
    }
}

void TimelineWidget::wheelEvent(QWheelEvent* e) {
    const int dy = e->angleDelta().y();
    if (dy != 0) {
        const int step = (e->modifiers() & Qt::ShiftModifier) ? 5 : 1;
        emit frameStepRequested(dy > 0 ? step : -step);
        e->accept();
    } else {
        QWidget::wheelEvent(e);
    }
}

void TimelineWidget::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
        case Qt::Key_Left:  emit frameStepRequested(-1); e->accept(); return;
        case Qt::Key_Right: emit frameStepRequested(1);  e->accept(); return;
        case Qt::Key_PageUp:   emit frameStepRequested(-5); e->accept(); return;
        case Qt::Key_PageDown: emit frameStepRequested(5);  e->accept(); return;
        case Qt::Key_Home:  emit seekRequested(0.0); e->accept(); return;
        case Qt::Key_End:   emit seekRequested(duration_); e->accept(); return;
        default: break;
    }
    QWidget::keyPressEvent(e);
}

} // namespace wc
