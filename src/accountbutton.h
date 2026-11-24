#pragma once
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>

class AccountButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
protected:
    void paintEvent(QPaintEvent *event) override {
        if (!property("accountAvatar").toBool()) { QPushButton::paintEvent(event); return; }
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF bounds = QRectF(rect()).adjusted(1, 1, -1, -1);
        QPainterPath circle; circle.addEllipse(bounds);
        painter.save(); painter.setClipPath(circle);
        painter.fillRect(bounds, palette().color(QPalette::Button));
        const auto image = property("accountImage").value<QImage>();
        if (!image.isNull()) {
            const double side = qMin(image.width(), image.height());
            const QRectF source((image.width()-side)/2, (image.height()-side)/2, side, side);
            painter.drawImage(bounds, image, source);
        } else {
            painter.setPen(palette().color(QPalette::ButtonText));
            painter.drawText(bounds, Qt::AlignCenter, text());
        }
        painter.restore();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(palette().color(hasFocus() ? QPalette::Highlight : QPalette::Mid), hasFocus() ? 2 : 1));
        painter.drawEllipse(bounds);
    }
};
