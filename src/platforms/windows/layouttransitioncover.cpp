// SPDX-License-Identifier: MIT
#include "layouttransitionpresentation.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QWidget>
#include <QOperatingSystemVersion>
#include <qt_windows.h>
#include <dwmapi.h>
#include <vector>

namespace Unbound {
namespace {
class DesktopCover final : public QObject,public LayoutTransitionPresentation {
    struct Surface {
        DesktopCover *owner=nullptr;
        RECT rect{};
        HWND window=nullptr;
        HDC dc=nullptr;
        HBITMAP bitmap=nullptr;
        HGDIOBJ previous=nullptr;
        ~Surface() {
            if(window) DestroyWindow(window);
            if(previous) SelectObject(dc,previous);
            if(bitmap) DeleteObject(bitmap);
            if(dc) DeleteDC(dc);
        }
    };
    std::vector<std::unique_ptr<Surface>> surfaces;
    QTimer animation;
    QElapsedTimer elapsed;
    int duration=120;
    qint64 bytes=0;
    std::function<void()> interrupted;
    std::function<void(bool)> completion;
    bool interruptionQueued=false;
    HWINEVENTHOOK foregroundHook=nullptr;
    static DesktopCover *&current() {static DesktopCover *value=nullptr;return value;}
    static bool ownForeground() {
        DWORD pid=0;GetWindowThreadProcessId(GetForegroundWindow(),&pid);
        return pid==GetCurrentProcessId();
    }
    void interrupt() {
        if(interruptionQueued)return;
        interruptionQueued=true;
        // Conceal immediately; defer owner destruction until the native callback unwinds.
        for(auto &s:surfaces) if(s->window) ShowWindow(s->window,SW_HIDE);
        QTimer::singleShot(0,this,[this] {auto callback=interrupted;if(callback)callback();});
    }
    static void CALLBACK foregroundChanged(HWINEVENTHOOK,DWORD,HWND,LONG,LONG,DWORD,DWORD) {
        if(current() && !ownForeground())current()->interrupt();
    }
    static LRESULT CALLBACK procedure(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
        auto s=reinterpret_cast<Surface *>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            s=static_cast<Surface *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(s));
        }
        if(s) {
            if(message==WM_PAINT) {
                PAINTSTRUCT paint{};auto dc=BeginPaint(hwnd,&paint);
                BitBlt(dc,0,0,s->rect.right-s->rect.left,s->rect.bottom-s->rect.top,s->dc,0,0,SRCCOPY);
                EndPaint(hwnd,&paint);return 0;
            }
            if(message==WM_ERASEBKGND)return 1;
            if(message==WM_MOUSEACTIVATE) {s->owner->interrupt();return MA_NOACTIVATEANDEAT;}
            if(message==WM_LBUTTONDOWN || message==WM_RBUTTONDOWN || message==WM_MBUTTONDOWN
                || message==WM_MOUSEWHEEL || message==WM_DISPLAYCHANGE) {s->owner->interrupt();return 0;}
            if(message==WM_NCHITTEST)return HTCLIENT;
        }
        return DefWindowProcW(hwnd,message,wp,lp);
    }
public:
    DesktopCover() {
        animation.setInterval(16);
        connect(&animation,&QTimer::timeout,this,[this] {
            if(!ownForeground()) {interrupt();return;}
            const auto progress=qBound(0.0,double(elapsed.elapsed())/duration,1.0);
            for(auto &s:surfaces)SetLayeredWindowAttributes(s->window,0,BYTE(255*(1-progress)),LWA_ALPHA);
            if(progress>=1) {
                auto callback=std::move(completion);cancel();if(callback)callback(true);
            }
        });
    }
    ~DesktopCover() override {cancel();}
    bool begin(QWidget *,std::function<void()> callback,const QList<QWidget *> &interactiveWindows) override {
        cancel();
        if(QGuiApplication::platformName()!=QLatin1String("windows") || !ownForeground())return false;
        interrupted=std::move(callback);interruptionQueued=false;
        struct CaptureExclusions {
            std::vector<std::pair<HWND,DWORD>> previous;
            bool restore() {
                bool restored=true;
                for(auto item:previous)if(IsWindow(item.first) && !SetWindowDisplayAffinity(item.first,item.second))restored=false;
                if(restored)previous.clear();return restored;
            }
            ~CaptureExclusions() {restore();}
        } exclusions;
        QList<HWND> live;
        for(auto widget:interactiveWindows)if(widget && widget->isVisible()) {
            // Keep the existing live popup above the cover without hiding it or
            // baking its old content into the snapshot. Older Windows versions
            // interpret EXCLUDEFROMCAPTURE as black instead of excluding it.
            const auto version=QOperatingSystemVersion::current();
            if(version.majorVersion()<10 || (version.majorVersion()==10 && version.microVersion()<19041))return false;
            const auto handle=reinterpret_cast<HWND>(widget->effectiveWinId());DWORD affinity=0;
            if(!widget->isWindow() || !handle || !(GetWindowLongPtr(handle,GWL_EXSTYLE)&WS_EX_TOPMOST)
                || !GetWindowDisplayAffinity(handle,&affinity))return false;
            exclusions.previous.emplace_back(handle,affinity);
            if(!SetWindowDisplayAffinity(handle,WDA_EXCLUDEFROMCAPTURE))return false;
            live.append(handle);
        }
        if(!live.isEmpty() && FAILED(DwmFlush()))return false;
        std::vector<RECT> monitors;
        EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR,HDC,LPRECT rect,LPARAM data)->BOOL {
            reinterpret_cast<std::vector<RECT> *>(data)->push_back(*rect);return TRUE;
        },reinterpret_cast<LPARAM>(&monitors));
        for(auto rect:monitors)bytes+=qint64(rect.right-rect.left)*(rect.bottom-rect.top)*4;
        // Bound raw snapshots. Native compositor storage is additional and measured separately.
        if(monitors.empty() || bytes>128*1024*1024) {cancel();return false;}
        auto screen=GetDC(nullptr);
        if(!screen) {cancel();return false;}
        bool captured=true;
        for(auto rect:monitors) {
            auto s=std::make_unique<Surface>();s->owner=this;s->rect=rect;
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=rect.right-rect.left;info.bmiHeader.biHeight=-(rect.bottom-rect.top);
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            void *pixels=nullptr;
            s->dc=CreateCompatibleDC(screen);
            s->bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
            if(!s->dc || !s->bitmap) {captured=false;break;}
            s->previous=SelectObject(s->dc,s->bitmap);
            if(!BitBlt(s->dc,0,0,info.bmiHeader.biWidth,-info.bmiHeader.biHeight,screen,rect.left,rect.top,SRCCOPY|CAPTUREBLT)) {captured=false;break;}
            surfaces.push_back(std::move(s));
        }
        ReleaseDC(nullptr,screen);
        if(!captured) {cancel();return false;}
        if(!exclusions.restore()) {cancel();return false;}
        WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=GetModuleHandleW(nullptr);
        wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.lpszClassName=L"UnboundLayoutCoverExperiment";
        if(!RegisterClassW(&wc) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) {cancel();return false;}
        for(auto &s:surfaces) {
            s->window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_LAYERED,
                wc.lpszClassName,L"",WS_POPUP,s->rect.left,s->rect.top,s->rect.right-s->rect.left,
                s->rect.bottom-s->rect.top,nullptr,nullptr,wc.hInstance,s.get());
            if(!s->window) {cancel();return false;}
            SetLayeredWindowAttributes(s->window,0,255,LWA_ALPHA);
        }
        current()=this;
        foregroundHook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,
            foregroundChanged,0,0,WINEVENT_OUTOFCONTEXT);
        if(!foregroundHook) {cancel();return false;}
        // Every screenshot exists before any cover appears, preventing recursive capture.
        HWND after=HWND_TOPMOST;
        for(auto handle=GetTopWindow(nullptr);handle;handle=GetWindow(handle,GW_HWNDNEXT))
            if(live.contains(handle))after=handle;
        for(auto &s:surfaces) {
            SetWindowPos(s->window,after,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_SHOWWINDOW);
            UpdateWindow(s->window);
        }
        if(FAILED(DwmFlush()) || !ownForeground()) {cancel();return false;}
        return true;
    }
    qint64 retainedBytes() const override {return bytes;}
    void hold() override {
        animation.stop();completion={};
        for(auto &s:surfaces)SetLayeredWindowAttributes(s->window,0,255,LWA_ALPHA);
    }
    void reveal(int milliseconds,std::function<void(bool)> callback) override {
        hold();
        // This acknowledges already-submitted application updates, not future Resolve rendering.
        if(FAILED(DwmFlush()) || !ownForeground()) {callback(false);return;}
        completion=std::move(callback);duration=qMax(1,milliseconds);elapsed.start();animation.start();
    }
    void cancel() override {
        animation.stop();completion={};interrupted={};
        if(foregroundHook) {UnhookWinEvent(foregroundHook);foregroundHook=nullptr;}
        if(current()==this)current()=nullptr;
        surfaces.clear();bytes=0;
    }
};
}
std::unique_ptr<LayoutTransitionPresentation> makeLayoutTransitionPresentation() {return std::make_unique<DesktopCover>();}
}
