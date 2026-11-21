// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "keywindowfocus.h"
#include <QApplication>
#include <QTimer>
#include <QWindow>
#include <QPointer>
namespace Unbound {
CocoaKeyWindowFocus::CocoaKeyWindowFocus(std::function<bool(QWidget *)> member)
    : m_member(std::move(member)) {}
void CocoaKeyWindowFocus::hostHidden(bool restoreResponder) {
    m_restoreResponder=m_restoreResponder || restoreResponder;
    if(m_queued)return;
    m_queued=true;
    QTimer::singleShot(0,this,[this] {
        m_queued=false;
        const bool restoreResponder=m_restoreResponder;
        m_restoreResponder=false;
        if(!qApp || QCoreApplication::closingDown() || !NSApp.active
            || QApplication::activeWindow() || QApplication::focusWidget()
            || QApplication::activeModalWidget() || QApplication::activePopupWidget())return;
        NSWindow *key=NSApp.keyWindow;
        if(!key)return;
        for(auto window:QApplication::topLevelWidgets()) {
            if(!m_member(window) || !window->isVisible() || window->isMinimized()
                || !window->internalWinId())continue;
            NSView *view=reinterpret_cast<NSView *>(window->internalWinId());
            if(view.window!=key)continue;
            // Cinema can leave its hidden view as native input responder even
            // though Cocoa has returned key ownership to this host. Qt's public
            // request restores the responder of that already-key window, without
            // raising it or choosing a different window/control.
            QPointer<QWidget> target(window);
            if(restoreResponder && window->windowHandle())window->windowHandle()->requestActivate();
            if(target && NSApp.keyWindow==key)QApplication::setActiveWindow(target);
            break;
        }
    });
}
}
