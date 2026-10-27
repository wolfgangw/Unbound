// SPDX-License-Identifier: MIT
#include "layoutadapter.h"
#include "windowplacement.h"
#include <QApplication>
#include <QMenu>
#include <QAbstractNativeEventFilter>
#include <QJsonDocument>
#include <qt_windows.h>
namespace LayoutTest {
void requestActivation(QWidget &window) {
    window.raise(); window.activateWindow();
    if(QGuiApplication::platformName()!=QLatin1String("windows")) return;
    const auto handle=reinterpret_cast<HWND>(window.winId());
    const auto thread=GetCurrentThreadId();
    const auto foreground=GetWindowThreadProcessId(GetForegroundWindow(),nullptr);
    const bool attached=foreground && foreground!=thread && AttachThreadInput(thread,foreground,TRUE);
    SetForegroundWindow(handle); SetFocus(handle);
    if(attached) AttachThreadInput(thread,foreground,FALSE);
}
void initialize() {}
bool nativeVisible(QWidget &window) { return IsWindowVisible(reinterpret_cast<HWND>(window.winId())); }
bool stackedAbove(QWidget &upper,QWidget &lower) {
    const auto a=reinterpret_cast<HWND>(upper.winId()),b=reinterpret_cast<HWND>(lower.winId());
    for(auto window=GetTopWindow(nullptr);window;window=GetWindow(window,GW_HWNDNEXT)) {
        if(window==a) return true;
        if(window==b) return false;
    }
    return false;
}
void verifyStackingDuring(QWidget &window,const QList<QWidget *> &pins,const std::function<void()> &action) {
    class Observer : public QAbstractNativeEventFilter {
    public:
        QWidget *target=nullptr; QList<QWidget *> pins; bool exposed=false;
        bool nativeEventFilter(const QByteArray &type,void *message,long *result) override {
            if(type!="windows_generic_MSG" || !message || !result) return false;
            const auto msg=static_cast<MSG *>(message);
            if(msg->message!=WM_WINDOWPOSCHANGED || msg->hwnd!=reinterpret_cast<HWND>(target->winId())) return false;
            for(auto pin:pins) if(!stackedAbove(*pin,*target)) exposed=true;
            return false;
        }
    } observer;
    observer.target=&window; observer.pins=pins;
    qApp->installNativeEventFilter(&observer);
    action(); settle();
    qApp->removeNativeEventFilter(&observer);
    require(!observer.exposed,"Ordinary window transiently crossed pinned panes");
    for(auto pin:pins) require(stackedAbove(*pin,window),"Ordinary window covered pinned panes after action");
}
void verifyActivationBelowPins(QWidget &window,const QList<QWidget *> &pins) {
    verifyStackingDuring(window,pins,[&] { activate(window); });
    require(ownsKeyboardFocus(window),"Stack constraint interfered with activation");
}
class PlacementFrame final : public QWidget {
protected:
    bool nativeEvent(const QByteArray &type,void *message,long *result) override {
        const auto msg=static_cast<MSG *>(message);
        if(msg->message==WM_NCCALCSIZE && !IsZoomed(msg->hwnd)) {
            auto rect=reinterpret_cast<RECT *>(msg->lParam);
            rect->left+=4; rect->right-=4; rect->bottom-=4;
            *result=0; return true;
        }
        return QWidget::nativeEvent(type,message,result);
    }
};
void nativePlacementEdges() {
    if(QGuiApplication::platformName()!=QLatin1String("windows")) return;
    PlacementFrame frame; frame.setGeometry(100,100,700,500); frame.show(); settle();
    auto handle=reinterpret_cast<HWND>(frame.winId());
    for(const auto point:{QPoint(-4,0),QPoint(0,-1),QPoint(-4,-1)}) {
        SetWindowPos(handle,nullptr,point.x(),point.y(),700,500,SWP_NOZORDER|SWP_NOACTIVATE); settle();
        RECT expected{}; GetWindowRect(handle,&expected);
        const QJsonArray screens;
        const auto saved=QJsonDocument::fromJson(Unbound::Platform::saveWindowPlacement(&frame,screens)).object();
        SetWindowPos(handle,nullptr,100,100,700,500,SWP_NOZORDER|SWP_NOACTIVATE); settle();
        require(Unbound::Platform::restoreWindowPlacement(&frame,saved,screens),"Native edge restoration failed"); settle();
        RECT actual{}; GetWindowRect(handle,&actual);
        require(EqualRect(&expected,&actual),"Windows placement changed an edge-aligned rectangle");
    }
    frame.hide();
    std::puts("PASS: exact native left/top edge placement with Resolve-like custom client borders");
}

std::unique_ptr<QWidget> makeMainWindow() { return std::make_unique<PlacementFrame>(); }
QRect nativeBounds(QWidget &window) {
    if(QGuiApplication::platformName()!=QLatin1String("windows")) return window.frameGeometry();
    RECT r{}; GetWindowRect(reinterpret_cast<HWND>(window.winId()),&r);
    return QRect(r.left,r.top,r.right-r.left,r.bottom-r.top);
}
QRect prepareMainPlacement(QWidget &window) {
    if(QGuiApplication::platformName()==QLatin1String("windows")) {
        SetWindowPos(reinterpret_cast<HWND>(window.winId()),nullptr,-4,0,650,450,SWP_NOZORDER|SWP_NOACTIVATE); settle();
    }
    return nativeBounds(window);
}
void verifyGestureDismissal(QWidget &floatingWindow,QWidget &main,QWidget &popup,const std::function<QWidget *()> &reopen) {
    if(QGuiApplication::platformName()!=QLatin1String("windows")) return;
    auto floating=&floatingWindow;
    auto menu=&popup;
    const auto savedGeometry=floating->geometry();
    const auto handle=reinterpret_cast<HWND>(floating->winId());
    SendMessage(handle,WM_NCLBUTTONDOWN,HTRIGHT,0);
    require(!menu->isVisible(),"Native host border resize did not dismiss the popout");
    SendMessage(handle,WM_CANCELMODE,0,0); settle();
    require(floating->geometry()==savedGeometry,"Cancelled dismissal test changed floating geometry");
    menu=reopen();
    // Exercise the native-child subclass path too: its border press never
    // becomes a Qt NonClientAreaMouseButtonPress on the floating host.
    {
        QWidget child(floating); child.setGeometry(0,0,50,30);
        child.setAttribute(Qt::WA_NativeWindow); child.show(); settle();
        const auto childHandle=reinterpret_cast<HWND>(child.winId());
        const auto point=floating->mapToGlobal(QPoint(25,1));
        require(SendMessage(childHandle,WM_NCHITTEST,0,MAKELPARAM(point.x(),point.y()))==HTTOP,
            "Native-child dismissal fixture did not reach a resize edge");
        SendMessage(childHandle,WM_NCLBUTTONDOWN,HTTOP,MAKELPARAM(point.x(),point.y()));
        require(!menu->isVisible(),"Native-child border resize did not dismiss the popout");
        SendMessage(handle,WM_CANCELMODE,0,0); settle();
    }
    menu=reopen();
    const auto mainHandle=reinterpret_cast<HWND>(main.winId());
    SendMessage(mainHandle,WM_ENTERSIZEMOVE,0,0);
    require(!menu->isVisible(),"Main-window native move/resize did not dismiss the popout");
    SendMessage(mainHandle,WM_EXITSIZEMOVE,0,0); settle();
    menu=reopen();
}
void transientPopup(QWidget &menu) {
    if(QGuiApplication::platformName()!=QLatin1String("windows")) return;
    QMenu popup(&menu); popup.addAction("Native action"); popup.popup(menu.pos()+QPoint(10,10)); popup.hide();
}
bool ownsKeyboardFocus(QWidget &window) {
    return QGuiApplication::platformName()!=QLatin1String("windows")
        || (GetAncestor(GetFocus(),GA_ROOT)==reinterpret_cast<HWND>(window.winId())
            && GetForegroundWindow()==reinterpret_cast<HWND>(window.winId()));
}
}
