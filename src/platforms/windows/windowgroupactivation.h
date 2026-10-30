// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QSet>
#include <QAbstractNativeEventFilter>
#include "panefloatingwindow.h"
#include "cinemaviewervisibility.h"
#include "panewindowgroup.h"

namespace Unbound {
// Shared policy uses normal Z order only. The Cinema guard retains ownership
// of native fullscreen transitions.
class WindowGroupActivation : public PaneWindowGroup, public QAbstractNativeEventFilter
{
public:
    WindowGroupActivation() : PaneWindowGroup(
        [this](QWidget *selected,const QList<QWidget *> &pins) { return raiseGroup(selected,pins); },
        [](QWidget *window) {
            if(QGuiApplication::platformName()==QLatin1String("windows"))
                SetWindowPos(reinterpret_cast<HWND>(window->effectiveWinId()),nullptr,0,0,0,0,
                    SWP_HIDEWINDOW|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
        },[](QWidget *window) {
            if(QGuiApplication::platformName()==QLatin1String("windows"))
                SetWindowPos(reinterpret_cast<HWND>(window->effectiveWinId()),nullptr,0,0,0,0,
                    SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
        }) { qApp->installNativeEventFilter(this); }
    ~WindowGroupActivation() override { if(qApp) qApp->removeNativeEventFilter(this); }
    void setWindow(QWidget *window) { PaneWindowGroup::setWindow(window); m_cinema.setWindow(window); }
private:
    bool nativeEventFilter(const QByteArray &type,void *message,long *result) override {
        if(type!="windows_generic_MSG" || !message || !result || m_raising) return false;
        auto msg=static_cast<MSG *>(message);
        if(msg->message!=WM_WINDOWPOSCHANGING || !msg->lParam) return false;
        auto pos=reinterpret_cast<WINDOWPOS *>(msg->lParam);
        if(pos->flags&(SWP_NOZORDER|SWP_HIDEWINDOW) || pos->hwndInsertAfter==HWND_TOPMOST
            || pos->hwndInsertAfter==HWND_BOTTOM) return false;
        auto target=QWidget::find(reinterpret_cast<WId>(pos->hwnd));
        auto foreground=QWidget::find(reinterpret_cast<WId>(GetForegroundWindow()));
        if(!activationMember(target) || !activationMember(foreground) || (target->property("unboundKeepAtop").toBool() && !(pos->flags&SWP_NOACTIVATE))
            || (!layoutMenu(target) && (GetWindowLongPtr(pos->hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST)) || !stackingAllowed()) return false;
        // Showing the main-owned tool menu can raise its owner before the
        // floating invoking window is restored. Preserve ordinary peers too:
        // a menu is not a request to bring main forward. QWidget visibility
        // is already set during show(), before the native owner raise occurs.
        if(target==m_main && (pos->flags&SWP_NOACTIVATE) && foreground!=m_main) {
            for(auto window:QApplication::topLevelWidgets())
                if(layoutMenu(window) && window->isVisible()) {
                    pos->flags|=SWP_NOZORDER;
                    return false;
                }
        }
        // Suppress preliminary member/menu raises while pins are visible. The
        // shared refresh submits the final group order in one batch. Merely
        // replacing hwndInsertAfter is insufficient: owner/owned reordering
        // can still expose a peer, even when the proposed insert-after handle
        // appears below the pins. Preserve activation, sizing and ownership.
        QSet<HWND> pins;
        for(auto window:priorityWindows()) {
            auto handle=reinterpret_cast<HWND>(window->effectiveWinId());
            if(handle && IsWindowVisible(handle) && !IsIconic(handle)) pins.insert(handle);
        }
        if(!pins.isEmpty()) pos->flags|=SWP_NOZORDER;
        return false;
    }
    bool raiseGroup(QWidget *selected,const QList<QWidget *> &pins)
    {
        if(QGuiApplication::platformName()!=QLatin1String("windows")) return false;
        if (!activationMember(selected) || !selected->isVisible() || selected->isMinimized()
            || QApplication::activeModalWidget() || QCoreApplication::closingDown()) return false;
        const auto active = reinterpret_cast<HWND>(selected->effectiveWinId());
        // A queued activation must not pull Resolve back over another app or
        // its own dialog if focus moved again before this callback executed.
        if (!active || GetForegroundWindow() != active
            || (!layoutMenu(selected) && (GetWindowLongPtr(active, GWL_EXSTYLE) & WS_EX_TOPMOST))) return false;
        QSet<HWND> peers;
        for (auto window : QApplication::topLevelWidgets()) {
            if (window == selected || !member(window) || !window->isVisible() || window->isMinimized()) continue;
            const auto handle = reinterpret_cast<HWND>(window->effectiveWinId());
            if (handle && IsWindowVisible(handle) && !IsIconic(handle)
                && !(GetWindowLongPtr(handle, GWL_EXSTYLE) & WS_EX_TOPMOST)) peers.insert(handle);
        }
        // Submit the final order once; never lower pinned panes behind the
        // selected ordinary window as an intermediate group-activation step.
        QList<HWND> ordered;
        for(auto window:pins) {
            auto handle=reinterpret_cast<HWND>(window->effectiveWinId());
            if(handle==active || peers.remove(handle)) ordered.append(handle);
        }
        // The menu retains its native popup level and focus; only arrange the
        // normal-level group underneath it. Never insert peers after a topmost
        // menu handle, which would promote those peers into the topmost band.
        if(member(selected) && !ordered.contains(active)) ordered.append(active);
        for(auto handle=GetTopWindow(nullptr);handle;handle=GetWindow(handle,GW_HWNDNEXT))
            if(peers.contains(handle)) ordered.append(handle);
        if(ordered.isEmpty()) return true;
        auto batch=BeginDeferWindowPos(ordered.size());
        if(!batch) return false;
        HWND after=HWND_TOP;
        for (auto handle : ordered) {
            batch = DeferWindowPos(batch, handle, after, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            if (!batch) break;
            after = handle;
        }
        return batch && EndDeferWindowPos(batch);
    }
    CinemaViewerVisibility m_cinema;
};
}
