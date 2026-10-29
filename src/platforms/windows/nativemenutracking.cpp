// SPDX-License-Identifier: MIT
#include "nativemenutracking.h"
#include <QApplication>
#include <QHash>
#include <qt_windows.h>
namespace Unbound {
bool nativeApplicationActive() {
    if(QGuiApplication::platformName()!=QLatin1String("windows"))
        return QGuiApplication::applicationState()==Qt::ApplicationActive;
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    return process==GetCurrentProcessId();
}
class WindowsMenuTracking final : public NativeMenuTracking {
    HWINEVENTHOOK hook=nullptr;std::function<void()> departed;
    static QHash<HWINEVENTHOOK,WindowsMenuTracking *> &observers() {
        static QHash<HWINEVENTHOOK,WindowsMenuTracking *> value;return value;
    }
    static void CALLBACK changed(HWINEVENTHOOK hook,DWORD,HWND,LONG,LONG,DWORD,DWORD) {
        auto observer=observers().value(hook);
        if(!observer || nativeApplicationActive())return;
        auto callback=observer->departed;callback(); // May destroy this observer.
    }
public:
    explicit WindowsMenuTracking(std::function<void()> callback):departed(std::move(callback)) {
        hook=SetWinEventHook(EVENT_SYSTEM_FOREGROUND,EVENT_SYSTEM_FOREGROUND,nullptr,changed,0,0,WINEVENT_OUTOFCONTEXT);
        if(hook)observers().insert(hook,this);
    }
    ~WindowsMenuTracking() override {
        if(hook) {observers().remove(hook);UnhookWinEvent(hook);}
    }
};
std::unique_ptr<NativeMenuTracking> observeNativeMenuTracking(std::function<void()> callback) {
    if(QGuiApplication::platformName()!=QLatin1String("windows"))return {};
    return std::make_unique<WindowsMenuTracking>(std::move(callback));
}
}
