// SPDX-License-Identifier: MIT
// Passive, bounded public Qt/native focus trace. No text content is recorded.
#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <qt_windows.h>
#include <functional>
static QString outputDirectory;
static QString id(const void *p) { return QString::number(quintptr(p),16); }
static QJsonObject describe(QObject *o) {
    if(!o) return {};
    QJsonObject r{{"id",id(o)},{"class",o->metaObject()->className()},{"name",o->objectName()}};
    if(auto w=qobject_cast<QWidget *>(o)) {
        r.insert("window",id(w->window())); r.insert("visible",w->isVisible()); r.insert("focus",w->hasFocus());
        r.insert("native",QString::number(quintptr(w->effectiveWinId()),16));
    }
    return r;
}
class WidgetKeys final : public QObject {
public:
    std::function<void(QObject *,QEvent *)> observe;
    explicit WidgetKeys(QObject *owner) : QObject(owner) {}
    bool eventFilter(QObject *object,QEvent *event) override {
        if(observe && (event->type()==QEvent::KeyPress || event->type()==QEvent::ShortcutOverride)) observe(object,event);
        return false;
    }
};
class FocusTrace final : public QObject, public QAbstractNativeEventFilter {
    QPointer<QWidget> menu;
    QJsonArray records;
    bool done=false;
    bool capturing=false;
    WidgetKeys widgetKeys{this};
    QJsonObject state() const {
        QJsonObject r{{"time",QDateTime::currentMSecsSinceEpoch()},
            {"qtFocus",describe(QApplication::focusWidget())},{"qtActive",describe(QApplication::activeWindow())},
            {"focusObject",describe(QGuiApplication::focusObject())},
            {"keyGrabber",describe(QWidget::keyboardGrabber())},
            {"mouseGrabber",describe(QWidget::mouseGrabber())},
            {"appClass",qApp->metaObject()->className()},
            {"nativeFocus",id(GetFocus())},{"nativeActive",id(GetActiveWindow())},{"foreground",id(GetForegroundWindow())}};
        if(menu) for(auto editor:menu->findChildren<QLineEdit *>("unboundPresetNameEdit")) if(editor->isVisible()) {
            r.insert("editor",describe(editor)); r.insert("selected",editor->selectedText().size());
            r.insert("length",editor->text().size()); r.insert("cursor",editor->cursorPosition());
        }
        return r;
    }
    void record(QJsonObject r) {
        if(done || records.size()>=2500) return;
        records.append(r);
    }
public:
    explicit FocusTrace(QObject *owner) : QObject(owner) {
        widgetKeys.observe=[this](QObject *object,QEvent *event) {
            auto r=state(); r.insert("kind","widget-filter"); r.insert("target",describe(object));
            r.insert("event",int(event->type())); record(r);
        };
        for(auto w:QApplication::allWidgets()) if(w->objectName()=="unboundPresetMenu") menu=w;
        qApp->installEventFilter(this); qApp->installNativeEventFilter(this);
        auto initial=state(); initial.insert("kind","start"); record(initial);
        // Allow time to return to Resolve; start the focused capture on rename.
        // Both paths are bounded and use single-shot timers, with no polling.
        QTimer::singleShot(5*60*1000,this,[this] { finish(); });
    }
    void finish() {
        if(done) return;
        auto final=state(); final.insert("kind","finish"); record(final); done=true;
        qApp->removeEventFilter(this); qApp->removeNativeEventFilter(this);
        QFile file(QDir(outputDirectory).filePath(QString("focus-%1.json").arg(GetCurrentProcessId())));
        if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{{"records",records},{"truncated",records.size()>=2500}}).toJson());
        deleteLater();
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        const auto type=event->type();
        if(type==QEvent::Show && object->objectName()=="unboundPresetMenu") menu=qobject_cast<QWidget *>(object);
        if(done || !menu || !menu->isVisible()) return false;
        if(type==QEvent::Show && object->objectName()=="unboundPresetNameEdit") {
            object->installEventFilter(&widgetKeys);
            if(!capturing) { capturing=true; QTimer::singleShot(90000,this,[this] { finish(); }); }
        }
        if(type!=QEvent::FocusIn && type!=QEvent::FocusOut && type!=QEvent::WindowActivate && type!=QEvent::WindowDeactivate &&
           type!=QEvent::MouseButtonPress && type!=QEvent::MouseButtonRelease && type!=QEvent::KeyPress &&
           type!=QEvent::KeyRelease && type!=QEvent::ShortcutOverride && type!=QEvent::Show && type!=QEvent::Hide) return false;
        auto r=state(); r.insert("kind","qt"); r.insert("event",int(type)); r.insert("target",describe(object));
        if(type==QEvent::KeyPress || type==QEvent::KeyRelease || type==QEvent::ShortcutOverride) {
            auto key=static_cast<QKeyEvent *>(event); r.insert("key",key->key()>=Qt::Key_Escape ? key->key() : 0); r.insert("modifiers",int(key->modifiers()));
        }
        record(r); return false;
    }
    bool nativeEventFilter(const QByteArray &,void *message,long *) override {
        if(done || !menu || !menu->isVisible()) return false;
        auto msg=static_cast<MSG *>(message);
        if(msg->message!=WM_SETFOCUS && msg->message!=WM_KILLFOCUS && msg->message!=WM_ACTIVATE &&
           msg->message!=WM_MOUSEACTIVATE && msg->message!=WM_LBUTTONDOWN && msg->message!=WM_LBUTTONUP &&
           msg->message!=WM_KEYDOWN && msg->message!=WM_KEYUP && msg->message!=WM_CHAR) return false;
        auto r=state(); r.insert("kind","native"); r.insert("message",int(msg->message)); r.insert("target",id(msg->hwnd));
        if(msg->message==WM_KEYDOWN || msg->message==WM_KEYUP) {
            const auto key=msg->wParam;
            r.insert("key",(key==VK_LEFT || key==VK_RIGHT || key==VK_HOME || key==VK_END ||
                key==VK_RETURN || key==VK_ESCAPE || key==VK_BACK || key==VK_DELETE || key==VK_TAB) ? int(key) : 0);
        }
        record(r); return false;
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        static QPointer<FocusTrace> installed;
        if(installed) { installed->finish(); installed=nullptr; return; }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto n=GetModuleFileNameW(module,path,32768);
        outputDirectory=QString::fromWCharArray(path,int(n)).section('\\',0,-2);
        installed=new FocusTrace(qApp);
    },Qt::QueuedConnection) ? 1 : 0;
}
