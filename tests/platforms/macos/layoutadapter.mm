// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "layoutadapter.h"
#include "windowplacement.h"
#include "geometryjson.h"
#include <QApplication>
#include <QMenu>
namespace LayoutTest {
void initialize() { qputenv("QT_STYLE_OVERRIDE","Fusion"); }
std::unique_ptr<QWidget> makeMainWindow() { return std::make_unique<QWidget>(); }
QRect nativeBounds(QWidget &window) { return window.frameGeometry(); }
QRect prepareMainPlacement(QWidget &window) { return nativeBounds(window); }
void nativePlacementEdges() {
    QWidget window; window.setGeometry(100,100,700,500); window.show(); settle();
    const QJsonArray screens;
    for(const auto point:{QPoint(0,30),QPoint(0,80),QPoint(100,30)}) {
        window.move(point); settle();
        const auto expected=nativeBounds(window);
        const auto saved=Unbound::Platform::saveWindowPlacement(&window,screens);
        window.move(200,200); settle();
        require(Unbound::Platform::restoreWindowPlacement(&window,Unbound::qtGeometryJson(saved),screens),"Cocoa native placement restore failed"); settle();
        require(nativeBounds(window)==expected,"Cocoa placement changed the native frame");
    }
}
void verifyGestureDismissal(QWidget &floating,QWidget &main,QWidget &menu,const std::function<QWidget *()> &reopen) {
    auto popup=&menu;
    // Post into this fixture's native queue so Cocoa's local event monitors see
    // the border press. Escape cancels before any geometry is committed.
    auto gesture=[&](QWidget &window) {
        auto native=reinterpret_cast<NSView *>(window.winId()).window;
        const auto before=window.geometry();
        auto press=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
            location:NSMakePoint(native.frame.size.width-1,20) modifierFlags:0 timestamp:0
            windowNumber:native.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];
        auto cancel=[NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint modifierFlags:0
            timestamp:0 windowNumber:native.windowNumber context:nil characters:@"\033"
            charactersIgnoringModifiers:@"\033" isARepeat:NO keyCode:53];
        auto release=[NSEvent mouseEventWithType:NSEventTypeLeftMouseUp
            location:press.locationInWindow modifierFlags:0 timestamp:0 windowNumber:native.windowNumber
            context:nil eventNumber:1 clickCount:1 pressure:0];
        [NSApp postEvent:press atStart:NO]; [NSApp postEvent:cancel atStart:NO];
        [NSApp postEvent:release atStart:NO]; settle();
        require(!popup->isVisible(),"Cocoa native border gesture did not dismiss layout popout");
        require(window.geometry()==before,"Cancelled native gesture changed geometry");
        popup=reopen();
    };
    gesture(floating);
    // NSView children share their NSWindow's native resize monitor.
    { QWidget child(&floating); child.setAttribute(Qt::WA_NativeWindow); child.show(); settle(); gesture(floating); }
    gesture(main);
}
void transientPopup(QWidget &menu) {
    QMenu popup(&menu); popup.addAction("Native action"); popup.popup(menu.pos()+QPoint(10,10)); popup.hide();
}
bool ownsKeyboardFocus(QWidget &window) {
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    return native.isKeyWindow && native.firstResponder!=nil;
}
}
