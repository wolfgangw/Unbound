// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "layoutadapter.h"
#include "windowplacement.h"
#include "windowintegration.h"
#include "geometryjson.h"
#include <QApplication>
#include <QMenu>
namespace LayoutTest {
void initialize() { qputenv("QT_STYLE_OVERRIDE","Fusion"); }
bool nativeVisible(QWidget &window) { return reinterpret_cast<NSView *>(window.winId()).window.visible; }
bool stackedAbove(QWidget &upper,QWidget &lower) {
    const auto a=reinterpret_cast<NSView *>(upper.winId()).window.windowNumber;
    const auto b=reinterpret_cast<NSView *>(lower.winId()).window.windowNumber;
    // NSApplication.orderedWindows excludes NSPanel, including our tool popout.
    // Compare actual WindowServer order, reading only the two fixture IDs.
    auto windows=CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly,kCGNullWindowID);
    if(!windows)return false;
    int upperIndex=-1,lowerIndex=-1,index=0;
    for(NSDictionary *entry in (NSArray *)windows) {
        const auto number=[entry[(id)kCGWindowNumber] integerValue];
        if(number==a)upperIndex=index;
        if(number==b)lowerIndex=index;
        ++index;
    }
    CFRelease(windows);
    return upperIndex>=0 && lowerIndex>=0 && upperIndex<lowerIndex;
}
void requestActivation(QWidget &window) {
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    // Synthetic recall can reparent native children while the tool window
    // remains Cocoa's key window. Qt then retains the former host as active;
    // makeKeyWindow alone is a no-op. Establish a fresh native handoff before
    // fixture input, without changing production focus or synthesizing Qt state.
    if(native.isKeyWindow) {
        auto active=QApplication::activeWindow();
        if(active && active!=&window) {
            auto previous=reinterpret_cast<NSView *>(active->winId()).window;
            [previous makeKeyWindow];
        }
    }
    window.raise(); window.activateWindow();
}
void verifyStackingDuring(QWidget &window,const QList<QWidget *> &pins,const std::function<void()> &action) {
    action(); settle();
    for(auto pin:pins) require(stackedAbove(*pin,window),"Ordinary window covered pinned panes after action");
}
void verifyActivationBelowPins(QWidget &window,const QList<QWidget *> &pins) {
    activate(window);
    // Native activation and the WindowServer's published stacking order settle
    // separately. Observe the result without replaying activation or ordering.
    QElapsedTimer deadline;deadline.start();
    while(deadline.elapsed()<1000) {
        bool ordered=true;
        for(auto pin:pins)ordered=ordered && stackedAbove(*pin,window);
        if(ordered)break;
        settle();
    }
    for(auto pin:pins)if(!stackedAbove(*pin,window))
        std::fprintf(stderr,"Stacking target=%s pin=%s nativeFocus=%d appActive=%d\n",
            qPrintable(window.objectName()),qPrintable(pin->objectName()),ownsKeyboardFocus(window),int(NSApp.active));
    for(auto pin:pins) require(stackedAbove(*pin,window),"Activated window crossed pinned panes");
    require(ownsKeyboardFocus(window),"Stack constraint interfered with activation");
    // An already-key Cocoa window can rise on a click without a second Qt
    // WindowActivate. Model that native raise, then deliver real fixture input.
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    for(auto pin:pins) Unbound::applyPaneDarkCaption(pin,&window);
    [native orderFront:nil];
    for(auto pin:pins) {
        require(reinterpret_cast<NSView *>(pin->winId()).window.level>native.level,
            "Pinned host lacks native priority over the ordinary window");
        require(stackedAbove(*pin,window),"Native raise transiently covered a pinned pane before deferred correction");
    }
    for(auto type:{NSEventTypeLeftMouseDown,NSEventTypeLeftMouseUp}) {
        auto event=[NSEvent mouseEventWithType:type location:NSMakePoint(100,100)
            modifierFlags:0 timestamp:NSProcessInfo.processInfo.systemUptime
            windowNumber:native.windowNumber context:nil eventNumber:0 clickCount:1 pressure:0];
        [NSApp postEvent:event atStart:NO];
    }
    deadline.restart();
    do {
        settle();
        bool ordered=true;
        for(auto pin:pins) ordered=ordered && stackedAbove(*pin,window);
        if(ordered)break;
    } while(deadline.elapsed()<1000);
    for(auto pin:pins) require(stackedAbove(*pin,window),"Clicking an already-key window covered pinned panes");
    require(ownsKeyboardFocus(window),"Repeated-click pin correction stole keyboard focus");
    [NSNotificationCenter.defaultCenter postNotificationName:NSApplicationWillResignActiveNotification object:NSApp];
    for(auto pin:pins) require(reinterpret_cast<NSView *>(pin->winId()).window.level==NSNormalWindowLevel,
        "Application departure retained elevated pin levels");
    [NSNotificationCenter.defaultCenter postNotificationName:NSApplicationDidBecomeActiveNotification object:NSApp];
    settle();
    for(auto pin:pins) require(reinterpret_cast<NSView *>(pin->winId()).window.level==NSFloatingWindowLevel,
        "Group reactivation failed to restore pin levels");


}
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
