// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANESNAPWINDOWS_H
#define UNBOUND_PANESNAPWINDOWS_H
#include "panesnap.h"
#include "panegridsnap.h"
#include "panegridguide.h"
#include <QCursor>
#include <QScreen>
#include <QApplication>
#include <QWidget>
#include <QPointer>
#include <QMargins>
#include <QOperatingSystemVersion>
#include <QElapsedTimer>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <dwmapi.h>
namespace Unbound {
inline QRect paneContactBounds(HWND handle)
{
    RECT bounds{};
    const bool hasFrame = SUCCEEDED(DwmGetWindowAttribute(handle,
        DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds)));
    if (!hasFrame && !GetWindowRect(handle, &bounds)) return {};
    // Qt obtains the real Windows build through RtlGetVersion. A failed DWM
    // attribute query alone does not identify Windows 10.
    static const auto os = QOperatingSystemVersion::current();
    int inset = 0;
    if (hasFrame && os.majorVersion() == 10 && os.minorVersion() == 0) {
        if (os.microVersion() >= 10240 && os.microVersion() < 22000) {
            // Windows 10's translucent outline exposes the desktop between
            // touching frames. Compensate by one physical pixel on each edge.
            inset = 1;
        } else if (os.microVersion() >= 22000) {
            UINT border = 0;
            if (SUCCEEDED(DwmGetWindowAttribute(handle, DWMWA_VISIBLE_FRAME_BORDER_THICKNESS,
                    &border, sizeof(border)))) {
                // Use the measured physical outline thickness. Keeping one
                // pixel on each neighbour leaves a two-pixel seam.
                inset = int(qMin(border, UINT(4)));
                RECT client{};
                POINT origin{};
                if (GetClientRect(handle, &client) && ClientToScreen(handle, &origin)) {
                    // Resolve's custom client rectangle includes DWM's top
                    // outline. ClientToScreen alone therefore cannot measure
                    // that edge; use DWM's reported thickness there.
                    const int left = qMin(inset, qMax(0, int(origin.x - bounds.left)));
                    const int top = inset;
                    const int right = qMin(inset, qMax(0, int(bounds.right - origin.x - client.right)));
                    const int bottom = qMin(inset, qMax(0, int(bounds.bottom - origin.y - client.bottom)));
                    return QRect(bounds.left + left, bounds.top + top,
                        bounds.right - bounds.left - left - right,
                        bounds.bottom - bounds.top - top - bottom);
                }
                // Without client bounds, do not risk overlapping content.
                inset = 0;
            }
        }
    }
    // Resolve's custom main frame can have client content at its very top.
    // Insetting that edge invents a target one pixel below the desktop edge.
    // Never remove more top border than actually exists before the client.
    POINT clientOrigin{};
    const int topInset = ClientToScreen(handle, &clientOrigin)
        ? qMin(inset, qMax(0, int(clientOrigin.y - bounds.top))) : inset;
    return QRect(bounds.left + inset, bounds.top + topInset,
        bounds.right - bounds.left - 2 * inset, bounds.bottom - bounds.top - topInset - inset);
}
class PaneSnapWindows
{
public:
    ~PaneSnapWindows() { end(); }
    static UINT modifierMessage() {static const UINT value=RegisterWindowMessageW(L"Unbound.SnapModifierChanged");return value;}
    void end() {m_gridGuide.hide();if(m_keyboard)UnhookWindowsHookEx(m_keyboard);m_keyboard=nullptr;if(s_modifierTarget==m_handle)s_modifierTarget=nullptr;}
    void begin(HWND handle)
    {
        end();m_handle = handle;
        s_modifierTarget=handle;
        m_keyboard=SetWindowsHookExW(WH_KEYBOARD,keyboard,nullptr,GetCurrentThreadId());
        m_grid.reset();m_gridMode=false;
        m_transition.reset(); m_clock.start();
        m_windows.clear();
        m_workAreas.clear();
        // Use native coordinates, as for the window rectangles below. Qt screen
        // geometry can be logical coordinates on mixed-DPI desktops.
        EnumDisplayMonitors(nullptr, nullptr, collectWorkArea, reinterpret_cast<LPARAM>(&m_workAreas));
        for (auto widget : QApplication::topLevelWidgets()) {
            if (reinterpret_cast<HWND>(widget->effectiveWinId()) != handle
                && (widget->objectName() == QLatin1String("UiMainWindow")
                    || widget->property("unboundFloatingWindow").toBool()))
                m_windows.append(widget);
        }
        RECT bounds{};
        GetWindowRect(handle, &bounds);
        const auto contact = paneContactBounds(handle);
        m_insets = QMargins(contact.x() - bounds.left, contact.y() - bounds.top,
            bounds.right - contact.x() - contact.width(), bounds.bottom - contact.y() - contact.height());
    }
    void apply(RECT &target, int hit, const MINMAXINFO &limits, bool bypass = false, QPoint *engaged = nullptr) const
    {
        if(engaged) *engaged=QPoint();
        if (!m_handle || bypass || (GetAsyncKeyState(VK_MENU) & 0x8000)) {m_gridGuide.hide();return;}
        QVector<QRect> targets;
        for (const auto &widget : m_windows) {
            if (!widget || !widget->isVisible() || widget->isMinimized()) continue;
            const auto handle = reinterpret_cast<HWND>(widget->effectiveWinId());
            if (handle && IsWindowVisible(handle) && !IsIconic(handle)) targets.append(paneContactBounds(handle));
        }
        // Window and monitor edges compete by distance in the same calculation;
        // windows win ties; work-area edges remain local to their monitor.
        const bool move = hit == HTCAPTION;
        const bool left = move || hit == HTLEFT || hit == HTTOPLEFT || hit == HTBOTTOMLEFT;
        const bool right = move || hit == HTRIGHT || hit == HTTOPRIGHT || hit == HTBOTTOMRIGHT;
        const bool top = move || hit == HTTOP || hit == HTTOPLEFT || hit == HTTOPRIGHT;
        const bool bottom = move || hit == HTBOTTOM || hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT;
        const int horizontal = m_insets.left() + m_insets.right();
        const int vertical = m_insets.top() + m_insets.bottom();
        const QRect visible(target.left + m_insets.left(), target.top + m_insets.top(),
            target.right - target.left - horizontal, target.bottom - target.top - vertical);
        const QSize minimum(qMax(1, int(limits.ptMinTrackSize.x) - horizontal), qMax(1, int(limits.ptMinTrackSize.y) - vertical));
        const QSize maximum(limits.ptMaxTrackSize.x > 0 ? limits.ptMaxTrackSize.x - horizontal : 16777215,
            limits.ptMaxTrackSize.y > 0 ? limits.ptMaxTrackSize.y - vertical : 16777215);
        const int distance = qMax(1, MulDiv(PaneSnapDistance, GetDpiForWindow(m_handle), 96));
        const bool grid=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
        if(grid!=m_gridMode) {m_grid.reset();m_transition.reset();m_gridMode=grid;}
        if(grid) {
            auto screen=QGuiApplication::screenAt(QCursor::pos());
            if(screen)m_gridGuide.show(screen->availableGeometry());
        } else m_gridGuide.hide();
        POINT cursor{};GetCursorPos(&cursor);
        const auto offset = grid ? m_grid.offset(visible,paneGridArea({cursor.x,cursor.y},m_workAreas),
            qMax(1,MulDiv(PaneGridSpacing,GetDpiForWindow(m_handle),96)),left,right,top,bottom,minimum,maximum,engaged)
            : paneSnapOffset(visible, targets, distance, left, right, top, bottom, minimum, maximum, m_workAreas, engaged);
        if (left) target.left += offset.x();
        if (right) target.right += offset.x();
        if (top) target.top += offset.y();
        if (bottom) target.bottom += offset.y();
    }
    void applyAnimated(RECT &target,int hit,const MINMAXINFO &limits,bool settle=false)
    {
        if(!m_handle || (GetAsyncKeyState(VK_MENU)&0x8000)) { m_transition.reset(); m_grid.reset();m_gridGuide.hide(); return; }
        const RECT raw=target;
        QPoint engaged;
        apply(target,hit,limits,false,&engaged);
        if(settle) { m_transition.reset(); m_grid.reset();m_gridGuide.hide(); return; }
        const bool move=hit==HTCAPTION;
        const bool left=move || hit==HTLEFT || hit==HTTOPLEFT || hit==HTBOTTOMLEFT;
        const bool right=move || hit==HTRIGHT || hit==HTTOPRIGHT || hit==HTBOTTOMRIGHT;
        const bool top=move || hit==HTTOP || hit==HTTOPLEFT || hit==HTTOPRIGHT;
        const bool bottom=move || hit==HTBOTTOM || hit==HTBOTTOMLEFT || hit==HTBOTTOMRIGHT;
        const QPoint desired(left ? target.left-raw.left : target.right-raw.right,
            top ? target.top-raw.top : target.bottom-raw.bottom);
        const auto offset=m_transition.update(desired,engaged,m_clock.elapsed(),m_grid.takeChanged());
        target=raw;
        if(left) target.left+=offset.x();
        if(right) target.right+=offset.x();
        if(top) target.top+=offset.y();
        if(bottom) target.bottom+=offset.y();
        // A fading release correction must not exceed current resize limits.
        if(!move) {
            const LONG width=qBound(qMax(1L,limits.ptMinTrackSize.x),target.right-target.left,
                limits.ptMaxTrackSize.x>0 ? limits.ptMaxTrackSize.x : 16777215L);
            const LONG height=qBound(qMax(1L,limits.ptMinTrackSize.y),target.bottom-target.top,
                limits.ptMaxTrackSize.y>0 ? limits.ptMaxTrackSize.y : 16777215L);
            if(left) target.left=target.right-width; else target.right=target.left+width;
            if(top) target.top=target.bottom-height; else target.bottom=target.top+height;
        }
    }
    bool animating() const { return m_transition.animating(); }
private:
    // Thread-local, gesture-scoped observation; never consume or rewrite keys.
    static LRESULT CALLBACK keyboard(int code,WPARAM key,LPARAM flags) {
        if(code==HC_ACTION && s_modifierTarget && (key==VK_SHIFT || key==VK_MENU))
            PostMessage(s_modifierTarget,modifierMessage(),0,0);
        return CallNextHookEx(nullptr,code,key,flags);
    }
    inline static thread_local HWND s_modifierTarget=nullptr;
    HHOOK m_keyboard=nullptr;
    mutable PaneGridSnap m_grid;
    mutable PaneGridPresentation m_gridGuide;
    mutable bool m_gridMode=false;
    static BOOL CALLBACK collectWorkArea(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info)) {
            const auto &r = info.rcWork;
            reinterpret_cast<QVector<QRect> *>(data)->append(QRect(r.left, r.top, r.right-r.left, r.bottom-r.top));
        }
        return TRUE;
    }
    HWND m_handle = nullptr;
    QList<QPointer<QWidget>> m_windows;
    QVector<QRect> m_workAreas;
    QMargins m_insets;
    mutable PaneSnapTransition m_transition;
    QElapsedTimer m_clock;
};

}
#endif
#endif
