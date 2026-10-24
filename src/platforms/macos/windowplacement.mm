// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "windowplacement.h"
#include "geometryjson.h"
#include "panegeometryrestore.h"
#include "panegridreferences.h"
#include <QWidget>
namespace Unbound { namespace Platform {
namespace {
NSWindow *nativeWindow(QWidget *window)
{
    return window && window->testAttribute(Qt::WA_WState_Created)
        ? reinterpret_cast<NSView *>(window->effectiveWinId()).window : nil;
}
QRect logicalRect(NSRect rect)
{
    return QRect(qRound(rect.origin.x),qRound(NSMaxY(NSScreen.screens.firstObject.frame)-NSMaxY(rect)),
        qRound(rect.size.width),qRound(rect.size.height));
}
NSRect cocoaRect(const QRect &rect)
{
    return NSMakeRect(rect.x(),NSMaxY(NSScreen.screens.firstObject.frame)-rect.y()-rect.height(),
        rect.width(),rect.height());
}
}
bool restoreWindowPlacement(QWidget *window,const QJsonObject &document,const QJsonArray &)
{
    auto native=nativeWindow(window);
    if (!native || !windowPlacementEligible(window)) return false;
    // Existing Mac records use the shared geometry fields. A Windows record
    // supplies those same fields in its Qt fallback; never use workspace units.
    const auto saved=document.value("format").toString()==QLatin1String("unbound-main-placement-1")
        ? qtGeometryJson(QByteArray::fromBase64(document.value("qt").toString().toLatin1())) : document;
    const bool maximized=saved.value("maximized").toBool();
    QRect client;
    if (saved.value("fullScreen").toBool() || !readRectangle(saved.value("clientRect"),client)) return false;
    if (maximized) {
        QRect normal;
        if (readRectangle(saved.value("normalClientRect"),normal)) client=normal;
    }
    client.setSize(client.size().expandedTo(window->minimumSize()).boundedTo(window->maximumSize()));
    // Keep Qt and Cocoa's zoom state synchronized, including the normal frame
    // remembered when zooming. Frame placement itself is always native.
    window->setWindowState(window->windowState() & ~(Qt::WindowMaximized|Qt::WindowMinimized));
    auto frame=logicalRect([native frameRectForContentRect:cocoaRect(client)]);
    QList<QRect> areas;
    for (auto screen:QGuiApplication::screens()) areas.append(screen->availableGeometry());
    frame=reachableWindowGeometry(frame,areas);
    auto nativeFrame=cocoaRect(frame);
    // Native setFrame does not enforce QWidget's content size constraints.
    auto content=logicalRect([native contentRectForFrameRect:nativeFrame]);
    content.setSize(content.size().expandedTo(window->minimumSize()).boundedTo(window->maximumSize()));
    nativeFrame=[native frameRectForContentRect:cocoaRect(content)];
    [native setFrame:nativeFrame display:YES animate:NO];
    if (maximized) window->setWindowState(window->windowState()|Qt::WindowMaximized);
    return true;
}
QByteArray saveWindowPlacement(QWidget *window,const QJsonArray &)
{
    auto saved=qtGeometryJson(window->saveGeometry());
    if (auto native=nativeWindow(window)) {
        const auto frame=native.frame;
        const auto client=logicalRect([native contentRectForFrameRect:frame]);
        saved.insert("frameRect",rectangleJson(logicalRect(frame)));
        saved.insert("clientRect",rectangleJson(client));
        if (!window->isMaximized() && !window->isFullScreen())
            saved.insert("normalClientRect",rectangleJson(client));
    }
    return qtGeometryBytes(saved);
}
bool windowPlacementEligible(QWidget *window)
{
    if (window->isFullScreen()) return false;
    if (!window->testAttribute(Qt::WA_WState_Created)) return true;
    auto native=reinterpret_cast<NSView *>(window->effectiveWinId()).window;
    // Qt can report normal state while Cocoa is still changing Spaces and
    // temporarily removing the title frame. Never persist that intermediate frame.
    return !native || !(native.styleMask & NSWindowStyleMaskFullScreen);
}
QRect windowPlacementReference(QWidget *window,const QJsonObject &saved)
{
    QList<QRect> areas;for(auto screen:QGuiApplication::screens())areas.append(screen->availableGeometry());
    const auto rect=gridSavedRect(saved,areas);
    if(rect.isEmpty() || saved.value("maximized").toBool() || saved.value("fullScreen").toBool())return rect;
    auto native=nativeWindow(window);
    return native ? reachableWindowGeometry(logicalRect([native frameRectForContentRect:cocoaRect(rect)]),areas) : rect;
}
} }
