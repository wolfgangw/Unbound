// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QSet>
#include "panefloatingwindow.h"
#include "cinemaviewervisibility.h"

namespace Unbound {
// Activation is the only trigger. There are no hooks in other processes and
// no recurring timer or work on painting/playback events.
// The associated Cinema visibility guard handles native fullscreen transitions.
class WindowGroupActivation : public QObject
{
public:
    WindowGroupActivation()
    {
        if (QGuiApplication::platformName() == QLatin1String("windows"))
            qApp->installEventFilter(this);
    }
    void setWindow(QWidget *window) { m_main = window; m_cinema.setWindow(window); }
protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() != QEvent::WindowActivate || m_raising) return false;
        auto window = qobject_cast<QWidget *>(object);
        if (!member(window)) return false;
        m_selected = window;
        if (!m_pending) {
            m_pending = true;
            QTimer::singleShot(0, this, [this] {
                m_pending = false;
                auto selected = m_selected.data();
                m_selected = nullptr;
                raiseGroup(selected);
            });
        }
        return false;
    }
private:
    bool member(QWidget *window) const
    {
        return m_main && window && window->isWindow()
            && (window == m_main || dynamic_cast<PaneFloatingWindow *>(window));
    }
    void raiseGroup(QWidget *selected)
    {
        if (!member(selected) || !selected->isVisible() || selected->isMinimized()
            || QApplication::activeModalWidget() || QCoreApplication::closingDown()) return;
        const auto active = reinterpret_cast<HWND>(selected->effectiveWinId());
        // A queued activation must not pull Resolve back over another app or
        // its own dialog if focus moved again before this callback executed.
        if (!active || GetForegroundWindow() != active
            || (GetWindowLongPtr(active, GWL_EXSTYLE) & WS_EX_TOPMOST)) return;
        QSet<HWND> peers;
        for (auto window : QApplication::topLevelWidgets()) {
            if (window == selected || !member(window) || !window->isVisible() || window->isMinimized()) continue;
            const auto handle = reinterpret_cast<HWND>(window->effectiveWinId());
            if (handle && IsWindowVisible(handle) && !IsIconic(handle)
                && !(GetWindowLongPtr(handle, GWL_EXSTYLE) & WS_EX_TOPMOST)) peers.insert(handle);
        }
        if (peers.isEmpty()) return;
        // Preserve the peers' relative order. Place them immediately behind the
        // selected window, leaving focus and the topmost band untouched.
        QList<HWND> ordered;
        for (auto handle = GetTopWindow(nullptr); handle; handle = GetWindow(handle, GW_HWNDNEXT))
            if (peers.contains(handle)) ordered.append(handle);
        auto batch = BeginDeferWindowPos(ordered.size());
        if (!batch) return;
        m_raising = true;
        auto after = active;
        for (auto handle : ordered) {
            batch = DeferWindowPos(batch, handle, after, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            if (!batch) break;
            after = handle;
        }
        if (batch) EndDeferWindowPos(batch);
        m_raising = false;
    }
    QPointer<QWidget> m_main, m_selected;
    CinemaViewerVisibility m_cinema;
    bool m_pending = false, m_raising = false;
};
}
