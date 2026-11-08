// SPDX-License-Identifier: MIT
#pragma once
#include <QPainter>
namespace Unbound {
inline constexpr int PanePopupDismissDelayMs=2000;
inline Qt::WindowFlags panePopupFlags() {
    return Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint;
}
inline void paintPanePopupSurface(QPainter &painter,const QRect &rect,const QColor &background) {
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(QColor("#555555"),1));painter.setBrush(background);
    painter.drawRoundedRect(QRectF(rect).adjusted(.5,.5,-.5,-.5),4,4);
}
}
