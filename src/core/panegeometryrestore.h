// SPDX-License-Identifier: MIT
#pragma once
#include <QWidget>
#include <QScreen>
#include <QGuiApplication>
#include "geometryjson.h"
namespace Unbound {
// Shared main-window recovery policy, independent of native frame coordinates.
inline QRect reachableWindowGeometry(QRect geometry,const QList<QRect> &screens)
{
    if (screens.isEmpty()) return geometry;
    QRect target=screens.first(); int best=-1;
    for (const auto &screen:screens) {
        const auto overlap=screen.intersected(geometry);
        const int area=overlap.width()*overlap.height();
        if (area>best) { best=area; target=screen; }
        if (screen.contains(geometry)) return geometry;
    }
    geometry.setSize(geometry.size().boundedTo(target.size()));
    geometry.moveLeft(qBound(target.left(),geometry.left(),target.right()-geometry.width()+1));
    geometry.moveTop(qBound(target.top(),geometry.top(),target.bottom()-geometry.height()+1));
    return geometry;
}
// Run after QWidget::restoreGeometry on a captionless native host. Qt reserves
// title-bar clearance; preserve exact reachable client geometry on unchanged
// displays while retaining Qt's recovery for removed/reconfigured displays.
inline void restoreReachablePaneClientGeometry(QWidget *window,const QByteArray &bytes)
{
    if(window->isMaximized() || window->isFullScreen()) return;
    const auto saved=qtGeometryJson(bytes);
    QRect client;
    if(!readRectangle(saved.value("clientRect"),client)) return;
    for(auto screen:QGuiApplication::screens()) {
        if(screen->availableGeometry().contains(client)
            && (!saved.value("screenWidth").toInt()
                || saved.value("screenWidth").toInt()==screen->geometry().width())) {
            window->setGeometry(client);
            break;
        }
    }
}
}
