// SPDX-License-Identifier: MIT
#ifndef UNBOUND_MAINWINDOWSNAP_H
#define UNBOUND_MAINWINDOWSNAP_H
#include "panesnapwindows.h"
#include "windowinteraction.h"
#include <commctrl.h>
namespace Unbound {
class MainWindowSnap
{
public:
    ~MainWindowSnap() { setWindow(nullptr); }
    void setWindow(QWidget *window)
    {
        const auto handle = window ? reinterpret_cast<HWND>(window->effectiveWinId()) : nullptr;
        if (handle == m_handle) return;
        stopFrames(); m_liveTarget=false;
        if (m_handle) RemoveWindowSubclass(m_handle, dispatch, reinterpret_cast<UINT_PTR>(this));
        m_handle = nullptr;
        m_active = false;
        m_topAligned = m_commitPending = false;
        if (handle && SetWindowSubclass(handle, dispatch, reinterpret_cast<UINT_PTR>(this), reinterpret_cast<DWORD_PTR>(this)))
            m_handle = handle;
    }
private:
    static LRESULT CALLBACK dispatch(HWND handle, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data)
    {
        auto self = reinterpret_cast<MainWindowSnap *>(data);
        if (message == WM_NCDESTROY) {
            self->stopFrames();
            RemoveWindowSubclass(handle, dispatch, id);
            self->m_handle = nullptr;
            self->m_active = false;
            return DefSubclassProc(handle, message, wParam, lParam);
        }
        return self->process(handle, message, wParam, lParam);
    }
    LRESULT process(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if(message==WM_TIMER && wParam==reinterpret_cast<UINT_PTR>(this)) {
            if(!m_active || !m_liveTarget || !IsWindowVisible(handle) || IsZoomed(handle) || IsIconic(handle)) {
                stopFrames(); return 0;
            }
            RECT frame=m_rawRect;
            m_snap.applyAnimated(frame,m_hit,m_limits);
            putRect(handle,frame);
            if(!m_snap.animating()) stopFrames();
            return 0;
        }
        if (message == commitMessage()) {
            if (!m_commitPending) return 0;
            m_commitPending = false;
            RECT current{};
            if (!m_active && IsWindowVisible(handle) && !IsZoomed(handle) && !IsIconic(handle)
                && !(GetAsyncKeyState(VK_MENU) & 0x8000) && GetWindowRect(handle, &current)
                && current.left == m_finalRect.left && current.right == m_finalRect.right
                && current.bottom - current.top == m_finalRect.bottom - m_finalRect.top
                && current.top > m_finalRect.top && current.top - m_finalRect.top <= m_topInset) {
                // The native loop may clamp the outer frame on release. Commit
                // the rectangle already negotiated by WM_MOVING after that
                // loop finishes; do not invent an additional position offset.
                SetWindowPos(handle, nullptr, m_finalRect.left, m_finalRect.top, 0, 0,
                    SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }
        if (message == WM_ENTERSIZEMOVE) {
            WindowInteractionEvent::notify(QWidget::find(reinterpret_cast<WId>(handle)));
            const auto result = DefSubclassProc(handle, message, wParam, lParam);
            m_snap.begin(handle);
            m_limits = MINMAXINFO{};
            SendMessage(handle, WM_GETMINMAXINFO, 0, reinterpret_cast<LPARAM>(&m_limits));
            m_active = true;
            m_topAligned = m_commitPending = false;
            stopFrames(); m_liveTarget=false;
            m_proposal.reset();
            return result;
        }
        if (message == WM_CANCELMODE || (message == WM_KEYDOWN && wParam == VK_ESCAPE)) {
            m_topAligned = m_commitPending = false;
            stopFrames(); m_liveTarget=false;
        }
        if (message == WM_EXITSIZEMOVE) {
            const bool settle=m_liveTarget;
            stopFrames(); m_liveTarget=false;
            m_active = false;
            const auto result = DefSubclassProc(handle, message, wParam, lParam);
            if(settle && IsWindowVisible(handle) && !IsZoomed(handle) && !IsIconic(handle)) {
                RECT final=m_rawRect;
                m_snap.applyAnimated(final,m_hit,m_limits,true);
                putRect(handle,final);
            }
            if (m_topAligned && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
                m_commitPending = true;
                PostMessage(handle, commitMessage(), 0, 0);
            }
            return result;
        }
        if (m_active && !IsZoomed(handle) && lParam && (message == WM_MOVING || message == WM_SIZING)) {
            int hit = HTCAPTION;
            if (message == WM_SIZING) {
                switch (wParam) {
                case WMSZ_LEFT: hit = HTLEFT; break;
                case WMSZ_RIGHT: hit = HTRIGHT; break;
                case WMSZ_TOP: hit = HTTOP; break;
                case WMSZ_TOPLEFT: hit = HTTOPLEFT; break;
                case WMSZ_TOPRIGHT: hit = HTTOPRIGHT; break;
                case WMSZ_BOTTOM: hit = HTBOTTOM; break;
                case WMSZ_BOTTOMLEFT: hit = HTBOTTOMLEFT; break;
                case WMSZ_BOTTOMRIGHT: hit = HTBOTTOMRIGHT; break;
                default: return TRUE;
                }
            }
            auto &target = *reinterpret_cast<RECT *>(lParam);
            if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
                POINT cursor;
                GetCursorPos(&cursor);
                const bool move = hit == HTCAPTION;
                const bool left = move || hit == HTLEFT || hit == HTTOPLEFT || hit == HTBOTTOMLEFT;
                const bool right = move || hit == HTRIGHT || hit == HTTOPRIGHT || hit == HTBOTTOMRIGHT;
                const bool top = move || hit == HTTOP || hit == HTTOPLEFT || hit == HTTOPRIGHT;
                const bool bottom = move || hit == HTBOTTOM || hit == HTBOTTOMLEFT || hit == HTBOTTOMRIGHT;
                auto proposed = m_proposal.update(QRect(target.left, target.top, target.right - target.left, target.bottom - target.top),
                    QPoint(cursor.x, cursor.y), left, right, top, bottom);
                target = RECT{proposed.x(), proposed.y(), proposed.x() + proposed.width(), proposed.y() + proposed.height()};
                if (!move) {
                    const LONG width = qBound(qMax(1L, m_limits.ptMinTrackSize.x), target.right - target.left,
                        m_limits.ptMaxTrackSize.x > 0 ? m_limits.ptMaxTrackSize.x : 16777215L);
                    const LONG height = qBound(qMax(1L, m_limits.ptMinTrackSize.y), target.bottom - target.top,
                        m_limits.ptMaxTrackSize.y > 0 ? m_limits.ptMaxTrackSize.y : 16777215L);
                    if (left) target.left = target.right - width; else target.right = target.left + width;
                    if (top) target.top = target.bottom - height; else target.bottom = target.top + height;
                }
            } else m_proposal.reset(); // Preserve keyboard/system-menu movement.
            // Retain Resolve's native constraints after deriving the raw drag.
            DefSubclassProc(handle, message, wParam, lParam);
            m_rawRect=target; m_hit=hit;
            m_liveTarget=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
            RECT final=target;
            m_snap.apply(final,hit,m_limits);
            if(m_liveTarget) {
                m_snap.applyAnimated(target,hit,m_limits);
                if(m_snap.animating()) {
                    if(!m_frameTimer) m_frameTimer=SetTimer(handle,reinterpret_cast<UINT_PTR>(this),16,nullptr);
                } else stopFrames();
            } else { stopFrames(); target=final; }
            m_topAligned = false;
            if (message == WM_MOVING && !(GetAsyncKeyState(VK_MENU) & 0x8000)) {
                RECT current{};
                GetWindowRect(handle, &current);
                m_topInset = paneContactBounds(handle).top() - current.top;
                MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
                if (GetMonitorInfo(MonitorFromRect(&final, MONITOR_DEFAULTTONEAREST), &monitor)
                    && final.top + m_topInset == monitor.rcWork.top) {
                    m_finalRect = final;
                    m_topAligned = true;
                }
            }
            return TRUE;
        }
        return DefSubclassProc(handle, message, wParam, lParam);
    }
    static UINT commitMessage()
    {
        static const UINT message = RegisterWindowMessageW(L"Unbound.MainWindowSnap.Commit");
        return message;
    }
    void stopFrames() {
        if(m_frameTimer && m_handle) KillTimer(m_handle,m_frameTimer);
        m_frameTimer=0;
    }
    static void putRect(HWND handle,const RECT &target) {
        RECT current{};
        if(GetWindowRect(handle,&current) && !EqualRect(&current,&target))
            SetWindowPos(handle,nullptr,target.left,target.top,target.right-target.left,target.bottom-target.top,
                SWP_NOZORDER|SWP_NOACTIVATE);
    }
    HWND m_handle = nullptr;
    Unbound::PaneSnapWindows m_snap;
    Unbound::PaneDragProposal m_proposal;
    MINMAXINFO m_limits{};
    bool m_active = false;
    bool m_topAligned = false, m_commitPending = false;
    int m_topInset = 0;
    RECT m_finalRect{};
    RECT m_rawRect{};
    int m_hit=HTCAPTION;
    bool m_liveTarget=false;
    UINT_PTR m_frameTimer=0;
};
}
#endif
