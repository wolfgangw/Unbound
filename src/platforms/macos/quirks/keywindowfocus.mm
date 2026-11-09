// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "keywindowfocus.h"
#include <QApplication>
#include <QTimer>
namespace Unbound {
CocoaKeyWindowFocus::CocoaKeyWindowFocus(std::function<bool(QWidget *)> member)
    : m_member(std::move(member)) {}
void CocoaKeyWindowFocus::hostHidden() {
    if(m_queued)return;
    m_queued=true;
    QTimer::singleShot(0,this,[this] {
        m_queued=false;
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
            // Public Qt activation restores its remembered focus/shortcut context.
            // Do not activate/raise a native window or select a different control.
            QApplication::setActiveWindow(window);
            break;
        }
    });
}
}
