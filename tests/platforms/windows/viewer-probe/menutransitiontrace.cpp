// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <QWindow>
#include <QWidget>
#include <QPointer>
#include <qt_windows.h>
class MenuTransitionTrace final : public QObject,public QAbstractNativeEventFilter {
    QJsonArray rows;QElapsedTimer clock;QPointer<QWidget> menu;bool done=false;
    void record(const QString &kind,QObject *object=nullptr,QJsonObject extra={}) {
        if(done || rows.size()>=5000)return;
        DWORD pid=0;auto foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,&pid);
        QJsonObject row{{"ms",double(clock.elapsed())},{"event",kind},{"qt_state",int(qApp->applicationState())},
            {"foreground_own",pid==GetCurrentProcessId()},{"object",object?object->objectName():QString()}};
        if(menu) {
            row.insert("visible",menu->isVisible());row.insert("hidden",menu->isHidden());
            const auto r=menu->geometry();row.insert("rect",QJsonArray{r.x(),r.y(),r.width(),r.height()});
            const auto p=r.center();auto top=WindowFromPoint(POINT{p.x(),p.y()});
            wchar_t cls[256]{};GetClassNameW(top,cls,256);row.insert("hit_class",QString::fromWCharArray(cls));
            row.insert("hit_menu",top==reinterpret_cast<HWND>(menu->effectiveWinId()));
            const auto handle=reinterpret_cast<HWND>(menu->effectiveWinId());DWORD affinity=0;GetWindowDisplayAffinity(handle,&affinity);
            row.insert("affinity",int(affinity));row.insert("native_visible",bool(IsWindowVisible(handle)));
            row.insert("topmost",bool(GetWindowLongPtr(handle,GWL_EXSTYLE)&WS_EX_TOPMOST));
            row.insert("updates",menu->updatesEnabled());
        }
        for(auto it=extra.begin();it!=extra.end();++it)row.insert(it.key(),it.value());
        rows.append(row);
    }
    bool eventFilter(QObject *o,QEvent *e) override {
        if(auto w=qobject_cast<QWidget *>(o))if(w->objectName()=="unboundLayoutMenu")menu=w;
        if(e->type()==QEvent::ApplicationDeactivate || e->type()==QEvent::ApplicationActivate)record(QString("app_%1").arg(e->type()),o);
        if(menu && (o==menu || o==menu->windowHandle()))switch(e->type()) {
        case QEvent::Show:case QEvent::Hide:case QEvent::Move:case QEvent::Resize:
        case QEvent::WindowActivate:case QEvent::WindowDeactivate:case QEvent::ZOrderChange:
        case QEvent::MouseButtonPress:case QEvent::MouseButtonRelease:
        case QEvent::Paint:case QEvent::UpdateRequest:
            record(QString("qt_%1").arg(e->type()),o);break;
        default:break;
        }
        return false;
    }
    bool nativeEventFilter(const QByteArray &,void *message,long *) override {
        if(!menu || !message)return false;
        auto msg=static_cast<MSG *>(message);
        if(msg->hwnd!=reinterpret_cast<HWND>(menu->effectiveWinId()))return false;
        if(msg->message==WM_SHOWWINDOW || msg->message==WM_WINDOWPOSCHANGED || msg->message==WM_WINDOWPOSCHANGING || msg->message==WM_ACTIVATE) {
            QJsonObject extra{{"wparam",double(msg->wParam)},{"lparam",double(msg->lParam)}};
            if((msg->message==WM_WINDOWPOSCHANGED || msg->message==WM_WINDOWPOSCHANGING) && msg->lParam) {
                const auto pos=reinterpret_cast<WINDOWPOS *>(msg->lParam);wchar_t cls[256]{};
                GetClassNameW(pos->hwndInsertAfter,cls,256);extra.insert("insert_after_class",QString::fromWCharArray(cls));
                extra.insert("position_flags",int(pos->flags));
            }
            record(QString("win_%1").arg(msg->message),menu,extra);
        }
        return false;
    }
public:
    MenuTransitionTrace():QObject(qApp) {
        setObjectName("UnboundMenuTransitionTrace");clock.start();qApp->installEventFilter(this);qApp->installNativeEventFilter(this);
        for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="unboundLayoutMenu")menu=w;
        record("start");QTimer::singleShot(120000,this,[this]{finish();});
    }
    void finish() {
        if(done)return;record("finish");done=true;qApp->removeEventFilter(this);qApp->removeNativeEventFilter(this);
        QSaveFile file(QDir::tempPath()+"/Unbound-menu-transition.json");
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(rows).toJson());file.commit();}deleteLater();
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[]{
        if(auto old=qApp->findChild<QObject *>("UnboundMenuTransitionTrace"))static_cast<MenuTransitionTrace *>(old)->finish();
        else new MenuTransitionTrace;
    },Qt::QueuedConnection)?1:0;
}
