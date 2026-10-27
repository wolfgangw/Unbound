// SPDX-License-Identifier: MIT
#pragma once
#include <QWidget>
#include <QScreen>
#include <QGuiApplication>
#include <QRegion>
#include "geometryjson.h"
namespace Unbound {
inline QRect panePlacementWorkArea(QWidget *nativePane)
{
    auto screen=nativePane ? QGuiApplication::screenAt(nativePane->mapToGlobal(nativePane->rect().center())) : nullptr;
    if(!screen && nativePane) screen=nativePane->screen();
    if(!screen) screen=QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect();
}
inline void centerNewPane(QWidget *window,const QRect &area)
{
    if(area.isEmpty()) return;
    window->resize(window->size().boundedTo(area.size()));
    // Native minimums may exceed the display; keep top-left controls reachable.
    window->move(area.left()+qMax(0,(area.width()-window->width())/2),
        area.top()+qMax(0,(area.height()-window->height())/2));
}
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
inline QRect reachablePaneGeometry(const QRect &geometry,const QList<QRect> &areas)
{
    QRegion available;
    for(const auto &area:areas) available+=area;
    // A deliberate span across adjacent monitors is still fully reachable.
    if(QRegion(geometry).subtracted(available).isEmpty()) return geometry;
    return reachableWindowGeometry(geometry,areas);
}
inline void ensureReachablePaneGeometry(QWidget *window)
{
    if(!window || window->isMaximized() || window->isFullScreen()) return;
    QList<QRect> areas;
    for(auto screen:QGuiApplication::screens()) areas.append(screen->availableGeometry());
    const auto geometry=reachablePaneGeometry(window->geometry(),areas);
    if(geometry!=window->geometry()) window->setGeometry(geometry);
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
    // QWidget may expand a recalled rectangle to a native content minimum.
    // Recover the actual allocation, not just the smaller saved rectangle.
    ensureReachablePaneGeometry(window);
}
}
