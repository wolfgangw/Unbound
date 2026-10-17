// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QHash>
#include <QPointer>
#include <QWidget>
#include <functional>
#include <qt_windows.h>
#include <commctrl.h>

namespace Unbound {
// Native Resolve children can cover the host's client edges. Route their
// non-client hit tests to the same resize gesture without overlay windows,
// altered content geometry, or recurring work on paint/playback messages.
class PaneResizeHitFilter final : public QObject, public QAbstractNativeEventFilter
{
    struct Host {
        QPointer<QWidget> widget;
        std::function<void(int)> begin;
    };
    QHash<HWND,Host> m_hosts;
    QHash<HWND,HWND> m_children;
    static LRESULT CALLBACK childProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam,
        UINT_PTR id,DWORD_PTR data) {
        auto self=reinterpret_cast<PaneResizeHitFilter *>(data);
        if(message==WM_NCDESTROY) {
            self->m_children.remove(window); RemoveWindowSubclass(window,childProc,id);
        } else if(message==WM_NCLBUTTONDOWN && wParam>=HTLEFT && wParam<=HTBOTTOMRIGHT) {
            const auto it=self->m_hosts.constFind(GetAncestor(window,GA_ROOT));
            if(it!=self->m_hosts.cend() && it->widget && !it->widget->isMaximized() && !it->widget->isFullScreen()) {
                const auto begin=it->begin; begin(int(wParam)); return 0;
            }
        }
        return DefSubclassProc(window,message,wParam,lParam);
    }
    static int hit(HWND handle, QWidget *widget, POINT point) {
        if(!widget || !widget->isVisible() || widget->isMaximized() || widget->isFullScreen()) return HTNOWHERE;
        RECT frame{},client{}; POINT origin{};
        if(!GetWindowRect(handle,&frame) || !PtInRect(&frame,point) ||
            !GetClientRect(handle,&client) || !ClientToScreen(handle,&origin)) return HTNOWHERE;
        const int edge=qMax(1,MulDiv(6,GetDpiForWindow(handle),96));
        const int corner=qMax(edge,MulDiv(12,GetDpiForWindow(handle),96));
        const int x=point.x-origin.x,y=point.y-origin.y;
        const bool horizontal=widget->minimumWidth()<widget->maximumWidth();
        const bool vertical=widget->minimumHeight()<widget->maximumHeight();
        const bool cornerLeft=horizontal && x<corner, cornerRight=horizontal && x>=client.right-corner;
        const bool cornerTop=vertical && y<corner, cornerBottom=vertical && y>=client.bottom-corner;
        if(cornerTop && cornerLeft) return HTTOPLEFT;
        if(cornerTop && cornerRight) return HTTOPRIGHT;
        if(cornerBottom && cornerLeft) return HTBOTTOMLEFT;
        if(cornerBottom && cornerRight) return HTBOTTOMRIGHT;
        if(horizontal && x<edge) return HTLEFT;
        if(horizontal && x>=client.right-edge) return HTRIGHT;
        if(vertical && y<edge) return HTTOP;
        if(vertical && y>=client.bottom-edge) return HTBOTTOM;
        return HTNOWHERE;
    }
public:
    PaneResizeHitFilter() : QObject(qApp) { qApp->installNativeEventFilter(this); }
    ~PaneResizeHitFilter() override {
        if(qApp) qApp->removeNativeEventFilter(this);
        for(auto window:m_children.keys()) RemoveWindowSubclass(window,childProc,reinterpret_cast<UINT_PTR>(this));
    }
    static PaneResizeHitFilter *shared() {
        static QPointer<PaneResizeHitFilter> filter;
        if(!filter) filter=new PaneResizeHitFilter;
        return filter;
    }
    void remove(QWidget *widget) {
        for(auto it=m_hosts.begin();it!=m_hosts.end();) {
            if(!it->widget || it->widget==widget) {
                // Content may already be docked/reparented. Remember the host
                // at installation so its child hooks still get removed now.
                for(auto child=m_children.begin();child!=m_children.end();) {
                    if(child.value()==it.key()) {
                        RemoveWindowSubclass(child.key(),childProc,reinterpret_cast<UINT_PTR>(this));
                        child=m_children.erase(child);
                    } else ++child;
                }
                it=m_hosts.erase(it);
            } else ++it;
        }
    }
    void watch(HWND handle,QWidget *widget,std::function<void(int)> begin) {
        remove(widget); m_hosts.insert(handle,{widget,std::move(begin)});
    }
    bool nativeEventFilter(const QByteArray &eventType,void *message,long *result) override {
        // Qt also filters messages at dispatcher level, where result may be
        // null. Do not start a gesture or consume that pass: the actual window
        // procedure/subclass handles it with a valid LRESULT destination.
        if(eventType!="windows_generic_MSG" || !message || !result) return false;
        const auto msg=static_cast<MSG *>(message);
        if(msg->message!=WM_NCHITTEST && msg->message!=WM_NCLBUTTONDOWN && msg->message!=WM_SETCURSOR) return false;
        const auto root=GetAncestor(msg->hwnd,GA_ROOT);
        const auto it=m_hosts.constFind(root);
        if(it==m_hosts.cend() || !it->widget) return false;
        if(msg->message==WM_NCHITTEST) {
            const POINT point{short(LOWORD(msg->lParam)),short(HIWORD(msg->lParam))};
            const int edge=hit(root,it->widget,point);
            if(edge==HTNOWHERE) return false;
            // Qt handles child non-client presses before application native
            // filters. Subclass only a child encountered at a resize edge so
            // that press reaches our host instead of a native modal size loop.
            if(msg->hwnd!=root && !m_children.contains(msg->hwnd) &&
                SetWindowSubclass(msg->hwnd,childProc,reinterpret_cast<UINT_PTR>(this),reinterpret_cast<DWORD_PTR>(this)))
                m_children.insert(msg->hwnd,root);
            *result=edge; return true;
        }
        const int edge=msg->message==WM_SETCURSOR ? LOWORD(msg->lParam) : int(msg->wParam);
        if(edge<HTLEFT || edge>HTBOTTOMRIGHT || it->widget->isMaximized() || it->widget->isFullScreen()) return false;
        if(msg->message==WM_NCLBUTTONDOWN) {
            // Copy before invoking: activation can cause host registry updates.
            const auto begin=it->begin;
            begin(edge); *result=0; return true;
        }
        LPCTSTR cursor=(edge==HTLEFT || edge==HTRIGHT) ? IDC_SIZEWE :
            (edge==HTTOP || edge==HTBOTTOM) ? IDC_SIZENS :
            (edge==HTTOPLEFT || edge==HTBOTTOMRIGHT) ? IDC_SIZENWSE : IDC_SIZENESW;
        SetCursor(LoadCursor(nullptr,cursor)); *result=TRUE; return true;
    }
};
}
