// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANEFLOATINGWINDOW_H
#define UNBOUND_PANEFLOATINGWINDOW_H
#include <QWidget>
#include <QLayout>
#include <QPointer>
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QTimer>
#include "geometryjson.h"
#include "panesnapwindows.h"
#include "paneresizehitfilter.h"
#include "windowinteraction.h"
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <dwmapi.h>
#endif
namespace Unbound {
class PaneFloatingWindow : public QWidget
{
public:
    PaneFloatingWindow() : QWidget(nullptr, Qt::Window | Qt::CustomizeWindowHint | Qt::WindowSystemMenuHint)
    {
        // Auxiliary taskbar windows must not keep Resolve alive.
        setAttribute(Qt::WA_QuitOnClose, false);
        // Snap membership belongs to the host, independent of pane identifiers.
        setProperty("unboundFloatingWindow",true);
        m_snapFrame.setInterval(16);
        m_snapFrame.setTimerType(Qt::PreciseTimer);
        connect(&m_snapFrame,&QTimer::timeout,this,[this] {
            if(m_dragHit) updateInteractiveDrag(m_dragCursor);
            else m_snapFrame.stop();
        });
    }
    ~PaneFloatingWindow() override { if(m_resizeFilter) m_resizeFilter->remove(this); }
    bool restoreGeometry(const QByteArray &bytes)
    {
        const bool restored = QWidget::restoreGeometry(bytes);
        if (!restored || QGuiApplication::platformName() != QLatin1String("windows")
            || isMaximized() || isFullScreen()) return restored;
        // Qt's generic restoration reserves a title-bar-height safety margin,
        // even for this captionless host. Restore the saved client rectangle
        // exactly when it is reachable on the current monitor layout; retain
        // Qt's recovery for removed displays or changed screen dimensions.
        const auto saved = qtGeometryJson(bytes);
        QRect client;
        if (!readRectangle(saved.value("clientRect"), client)) return restored;
        for (auto screen : QGuiApplication::screens()) {
            if (screen->availableGeometry().contains(client)
                && (!saved.value("screenWidth").toInt()
                    || saved.value("screenWidth").toInt() == screen->geometry().width())) {
                setGeometry(client);
                break;
            }
        }
        return restored;
    }
    bool beginButtonMove()
    {
        if (QGuiApplication::platformName() != QLatin1String("windows") || !isVisible()) return false;
        // Keep Qt's button grab, including when Resolve gives its header a
        // native child HWND. Do not transfer capture to the floating host.
        beginInteractiveDrag(HTCAPTION, true);
        return interactiveDragActive();
    }
    void updateButtonMove()
    {
        if (!m_buttonMove) return;
        POINT cursor{};
        GetCursorPos(&cursor);
        updateInteractiveDrag(cursor);
    }
    void finishButtonMove(bool cancel = false)
    {
        if (!m_buttonMove) return;
        if (cancel) SetWindowPos(reinterpret_cast<HWND>(winId()), nullptr,
            m_dragRect.left, m_dragRect.top, m_dragRect.right - m_dragRect.left,
            m_dragRect.bottom - m_dragRect.top, SWP_NOZORDER | SWP_NOACTIVATE);
        finishDeferredResize(!cancel);
    }
    void beginDeferredResize()
    {
        if (m_deferredLayout || !layout() || !layout()->isEnabled()) return;
        m_deferredLayout = layout();
        m_deferredLayout->setEnabled(false);
    }
    void finishDeferredResize(bool settle=true)
    {
#ifdef Q_OS_WIN
        m_snapFrame.stop();
        if(m_dragHit && settle) updateInteractiveDrag(m_dragCursor,true);
        const bool releaseCapture = m_dragHit != 0 && !m_buttonMove;
        m_dragHit = 0;
        m_buttonMove = false;
        if (releaseCapture && GetCapture() == reinterpret_cast<HWND>(effectiveWinId())) ReleaseCapture();
#endif
        if (!m_deferredLayout) return;
        auto saved = m_deferredLayout.data();
        m_deferredLayout = nullptr;
        saved->setEnabled(true);
        saved->invalidate();
        saved->activate();
        update();
    }
#ifdef Q_OS_WIN
    POINT dragStartPosition() const { return m_dragStart; }
    bool interactiveDragActive() const { return m_dragHit != 0; }
    void updateInteractiveDrag(POINT cursor,bool settle=false)
    {
        if (!m_dragHit) return;
        m_dragCursor=cursor;
        RECT target = m_dragRect;
        const LONG dx = cursor.x - m_dragStart.x, dy = cursor.y - m_dragStart.y;
        if (m_dragHit == HTCAPTION) OffsetRect(&target, dx, dy);
        else {
            const bool left = m_dragHit == HTLEFT || m_dragHit == HTTOPLEFT || m_dragHit == HTBOTTOMLEFT;
            const bool right = m_dragHit == HTRIGHT || m_dragHit == HTTOPRIGHT || m_dragHit == HTBOTTOMRIGHT;
            const bool top = m_dragHit == HTTOP || m_dragHit == HTTOPLEFT || m_dragHit == HTTOPRIGHT;
            const bool bottom = m_dragHit == HTBOTTOM || m_dragHit == HTBOTTOMLEFT || m_dragHit == HTBOTTOMRIGHT;
            if (left) target.left += dx;
            if (right) target.right += dx;
            if (top) target.top += dy;
            if (bottom) target.bottom += dy;
            const LONG width = qBound(m_dragLimits.ptMinTrackSize.x, target.right - target.left, m_dragLimits.ptMaxTrackSize.x);
            const LONG height = qBound(m_dragLimits.ptMinTrackSize.y, target.bottom - target.top, m_dragLimits.ptMaxTrackSize.y);
            if (left) target.left = target.right - width; else target.right = target.left + width;
            if (top) target.top = target.bottom - height; else target.bottom = target.top + height;
        }
        m_snap.applyAnimated(target, m_dragHit, m_dragLimits,settle);
        if(m_snap.animating()) { if(!m_snapFrame.isActive()) m_snapFrame.start(); }
        else m_snapFrame.stop();
        const auto handle=reinterpret_cast<HWND>(winId());
        RECT current{};
        if(GetWindowRect(handle,&current) && !EqualRect(&current,&target))
            SetWindowPos(handle, nullptr, target.left, target.top,
                target.right - target.left, target.bottom - target.top, SWP_NOZORDER | SWP_NOACTIVATE);
    }
#endif
protected:
    bool event(QEvent *event) override
    {
        const bool handled = QWidget::event(event);
        if ((event->type() == QEvent::WinIdChange || event->type() == QEvent::Show
                || event->type() == QEvent::ScreenChangeInternal)
            && testAttribute(Qt::WA_WState_Created)
            && QGuiApplication::platformName() == QLatin1String("windows")) {
            const auto handle = reinterpret_cast<HWND>(effectiveWinId());
            const auto style = GetWindowLongPtr(handle, GWL_STYLE);
            // Qt's minimize/maximize hints would also add a caption. Keep the
            // native commands and shell shortcuts without restoring a title bar.
            const auto desired = (style & ~LONG_PTR(WS_CAPTION))
                | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
            if (style != desired) SetWindowLongPtr(handle, GWL_STYLE, desired);
            // Also refresh Qt's measured margins after native creation or a
            // screen change; system defaults still include the thick top frame.
            SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            m_resizeFilter=PaneResizeHitFilter::shared();
            m_resizeFilter->watch(handle,this,[this](int hit) { beginInteractiveDrag(hit,false); });
        }
        return handled;
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(20, 20, 20));
    }
    bool nativeEvent(const QByteArray &type, void *message, long *result) override
    {
#ifdef Q_OS_WIN
        const auto msg = static_cast<MSG *>(message);
        const auto handle = msg->hwnd;
        if (msg->message == WM_NCACTIVATE && !IsIconic(handle)) {
            // The default activation repaint uses the full native top resize
            // band, overwriting client pixels reclaimed by WM_NCCALCSIZE.
            // Preserve activation processing but suppress that legacy repaint.
            *result = long(DefWindowProc(handle, msg->message, msg->wParam, -1));
            return true;
        }
        if (msg->message == WM_NCCALCSIZE) {
            auto rect = reinterpret_cast<RECT *>(msg->lParam);
            const LONG top = rect->top;
            DefWindowProc(handle, msg->message, msg->wParam, msg->lParam);
            if (!IsZoomed(handle) && (GetWindowLongPtr(handle, GWL_STYLE) & WS_THICKFRAME)) {
                // A captionless thick frame exposes its entire resize band at
                // the top. Keep only its thin outline; native hit-testing still
                // supplies the top/corner resize zones. Maximized insets remain
                // native so content stays inside the monitor work area.
                rect->top = qMin(rect->top, top + 1);
            }
            // Qt records the before/after NCCALCSIZE rectangles, keeping its
            // geometry cache consistent with the actual client rectangle.
            *result = 0;
            return true;
        }
        if (msg->message == WM_ERASEBKGND) {
            RECT client;
            GetClientRect(handle, &client);
            HBRUSH background = CreateSolidBrush(RGB(20, 20, 20));
            FillRect(reinterpret_cast<HDC>(msg->wParam), &client, background);
            DeleteObject(background);
            *result = 1;
            return true;
        }
        if (msg->message == WM_NCLBUTTONDOWN
            && (msg->wParam == HTCAPTION || (msg->wParam >= HTLEFT && msg->wParam <= HTBOTTOMRIGHT))) {
            // Consume the press instead of allowing DefWindowProc to enter its
            // modal sizing loop. Every mouse update returns to Qt's event loop.
            beginInteractiveDrag(static_cast<int>(msg->wParam), false);
            *result = 0;
            return true;
        }
        if (m_dragHit && !m_buttonMove && msg->message == WM_MOUSEMOVE) {
            POINT cursor;
            GetCursorPos(&cursor);
            updateInteractiveDrag(cursor);
            *result = 0;
            return true;
        }
        if (m_dragHit && !m_buttonMove && (msg->message == WM_LBUTTONUP || msg->message == WM_CAPTURECHANGED
            || msg->message == WM_CANCELMODE || (msg->message == WM_KEYDOWN && msg->wParam == VK_ESCAPE))) {
            if (msg->message == WM_KEYDOWN || msg->message == WM_CANCELMODE)
                SetWindowPos(handle, nullptr, m_dragRect.left, m_dragRect.top,
                    m_dragRect.right - m_dragRect.left, m_dragRect.bottom - m_dragRect.top, SWP_NOZORDER | SWP_NOACTIVATE);
            finishDeferredResize(msg->message!=WM_KEYDOWN && msg->message!=WM_CANCELMODE);
            *result = 0;
            return true;
        }
        if (msg->message == WM_SHOWWINDOW && !msg->wParam) finishDeferredResize(false);
        // WM_SIZING distinguishes border resizing from title-bar dragging.
        if (msg->message == WM_SIZING) beginDeferredResize();
        if (msg->message == WM_EXITSIZEMOVE) finishDeferredResize();
#endif
        return QWidget::nativeEvent(type, message, result);
    }
private:
    void beginInteractiveDrag(int hit, bool fromButton)
    {
        WindowInteractionEvent::notify(this);
        const auto handle = reinterpret_cast<HWND>(winId());
        if (isMaximized()) showNormal();
        SetForegroundWindow(handle);
        GetWindowRect(handle, &m_dragRect);
        m_snap.begin(handle);
        GetCursorPos(&m_dragStart);
        m_dragCursor=m_dragStart;
        m_dragLimits = MINMAXINFO{};
        SendMessage(handle, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&m_dragLimits));
        if (m_dragLimits.ptMaxTrackSize.x <= 0) m_dragLimits.ptMaxTrackSize.x = 32767;
        if (m_dragLimits.ptMaxTrackSize.y <= 0) m_dragLimits.ptMaxTrackSize.y = 32767;
        m_dragHit = hit;
        m_buttonMove = fromButton;
        if (hit != HTCAPTION) beginDeferredResize();
        if (!fromButton) {
            SetCapture(handle);
            if (GetCapture() != handle) finishDeferredResize();
        }
    }
    QPointer<QLayout> m_deferredLayout;
#ifdef Q_OS_WIN
    int m_dragHit = 0;
    bool m_buttonMove = false;
    RECT m_dragRect{};
    POINT m_dragStart{};
    POINT m_dragCursor{};
    QTimer m_snapFrame;
    MINMAXINFO m_dragLimits{};
    PaneSnapWindows m_snap;
    QPointer<PaneResizeHitFilter> m_resizeFilter;
#endif
};
}
#endif
