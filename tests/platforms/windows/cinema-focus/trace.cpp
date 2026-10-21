// SPDX-License-Identifier: MIT
// Passive public Qt/Win32 metadata only; no custom getters or native slots.
#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeyEvent>
#include <QShortcutEvent>
#include <QShortcut>
#include <QStackedWidget>
#include <QMetaMethod>
#include <QAction>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <qt_windows.h>
static QString output;
static QString id(const void *p) {return QString::number(quintptr(p),16);}
static QJsonObject describe(QObject *o) {
    if(!o) return {};
    QJsonObject r{{"id",id(o)},{"class",o->metaObject()->className()},{"name",o->objectName()},{"parent",id(o->parent())}};
    if(auto w=qobject_cast<QWidget *>(o)) {
        r.insert("window",id(w->window())); r.insert("visible",w->isVisible()); r.insert("hidden",w->isHidden());
        r.insert("focusPolicy",int(w->focusPolicy())); r.insert("focusProxy",id(w->focusProxy()));
        r.insert("state",int(w->windowState())); r.insert("flags",QString::number(qulonglong(w->windowFlags()),16));
        if(w->testAttribute(Qt::WA_WState_Created)) r.insert("native",id(reinterpret_cast<void *>(w->effectiveWinId())));
    }
    return r;
}
class Trace : public QObject,public QAbstractNativeEventFilter {
    QJsonArray records; bool done=false;
    QJsonObject state() {
        QString page;
        for(auto window:QApplication::topLevelWidgets())
            if(window->objectName()==QLatin1String("UiMainWindow"))
                if(auto stack=window->findChild<QStackedWidget *>("widgetStack_Panel"))
                    if(stack->currentWidget()) page=stack->currentWidget()->objectName();
        return {{"time",QDateTime::currentMSecsSinceEpoch()},{"page",page},
            {"keyboardLayout",id(GetKeyboardLayout(0))},{"focus",describe(QApplication::focusWidget())},
            {"active",describe(QApplication::activeWindow())},{"focusObject",describe(QGuiApplication::focusObject())},
            {"keyGrabber",describe(QWidget::keyboardGrabber())},{"foreground",id(GetForegroundWindow())},
            {"nativeFocus",id(GetFocus())},{"nativeActive",id(GetActiveWindow())}};
    }
    void add(QJsonObject r) {if(!done && records.size()<1500) records.append(r);}
public:
    Trace():QObject(qApp) {qApp->installEventFilter(this); qApp->installNativeEventFilter(this); snapshot(); QTimer::singleShot(120000,this,[this]{finish();});}
    void snapshot() {
        auto r=state(); r.insert("kind","snapshot"); QJsonArray widgets;
        QJsonArray shortcuts;
        for(auto w:QApplication::allWidgets()) {
            if(w->isWindow() || w->objectName().contains("Player",Qt::CaseInsensitive) || w->hasFocus()) {
                auto info=describe(w);
                if(QByteArray(w->metaObject()->className()).contains("Cinema")) {
                    QJsonArray methods;
                    for(int i=w->metaObject()->methodOffset();i<w->metaObject()->methodCount();++i)
                        methods.append(QString::fromLatin1(w->metaObject()->method(i).methodSignature()));
                    info.insert("methods",methods);
                }
                widgets.append(info);
            }
            for(auto shortcut:w->findChildren<QShortcut *>(QString(),Qt::FindDirectChildrenOnly)) {
                auto info=describe(shortcut);
                info.insert("key",shortcut->key().toString()); info.insert("context",int(shortcut->context()));
                info.insert("enabled",shortcut->isEnabled()); info.insert("owner",describe(w));
                shortcuts.append(info);
            }
        }
        r.insert("shortcuts",shortcuts);
        r.insert("widgets",widgets); add(r);
        QFile file(output); if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{{"records",records},{"truncated",records.size()>=1500}}).toJson());
    }
    void finish() {if(done)return; snapshot(); done=true; qApp->removeEventFilter(this); qApp->removeNativeEventFilter(this); deleteLater();}
    bool eventFilter(QObject *o,QEvent *e) override {
        auto type=e->type(); auto w=qobject_cast<QWidget *>(o);
        const bool key=type==QEvent::KeyPress || type==QEvent::KeyRelease || type==QEvent::ShortcutOverride;
        if(!key && type!=QEvent::Shortcut && type!=QEvent::FocusIn && type!=QEvent::FocusOut
            && !((type==QEvent::WindowActivate || type==QEvent::WindowDeactivate) && w && w->isWindow())
            && !((type==QEvent::Show || type==QEvent::Hide || type==QEvent::ParentChange) && w && w->isWindow())) return false;
        auto r=state(); r.insert("kind","qt"); r.insert("event",int(type)); r.insert("target",describe(o));
        if(type==QEvent::Shortcut) {
            auto shortcut=static_cast<QShortcutEvent *>(e);
            r.insert("shortcut",shortcut->key().toString());
            r.insert("ambiguous",shortcut->isAmbiguous());
            if(auto action=qobject_cast<QAction *>(o)) {
                r.insert("action",action->text());
                QJsonArray owners; for(auto owner:action->associatedWidgets()) owners.append(describe(owner));
                r.insert("owners",owners);
            }
        }
        if(key) {auto k=static_cast<QKeyEvent *>(e); const auto code=k->key();
            const bool pageKey=(k->modifiers() & Qt::ShiftModifier) && code>=Qt::Key_2 && code<=Qt::Key_8;
            r.insert("modifiers",int(k->modifiers()));
            r.insert("key",(pageKey || code==Qt::Key_Space || code==Qt::Key_Slash || code==Qt::Key_J || code==Qt::Key_K || code==Qt::Key_L || code==Qt::Key_Left || code==Qt::Key_Right || code==Qt::Key_Escape) ? code : 0);}
        add(r); return false;
    }
    bool nativeEventFilter(const QByteArray &,void *event,long *) override {
        auto m=static_cast<MSG *>(event);
        if(m->message!=WM_KEYDOWN && m->message!=WM_KEYUP && m->message!=WM_SETFOCUS && m->message!=WM_KILLFOCUS && m->message!=WM_ACTIVATE) return false;
        auto r=state(); r.insert("kind","native"); r.insert("message",int(m->message)); r.insert("target",id(m->hwnd));
        if(m->message==WM_KEYDOWN || m->message==WM_KEYUP) {
            const bool shift=GetKeyState(VK_SHIFT)<0;
            r.insert("shift",shift);
            r.insert("key",((shift && m->wParam>='2' && m->wParam<='8') || m->wParam==VK_SPACE || m->wParam=='J' || m->wParam=='K' || m->wParam=='L' || m->wParam==VK_LEFT || m->wParam==VK_RIGHT || m->wParam==VK_ESCAPE) ? int(m->wParam) : 0);
        }
        add(r); return false;
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(qApp,[]{
        static QPointer<Trace> trace;
        if(trace) {trace->snapshot();return;}
        HMODULE module; GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; auto n=GetModuleFileNameW(module,path,32768);
        output=QString::fromWCharArray(path,int(n)).section('\\',0,-2)+QString("/cinema-%1-%2.json").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch());
        trace=new Trace;
    },Qt::QueuedConnection) ? 1 : 0;
}
