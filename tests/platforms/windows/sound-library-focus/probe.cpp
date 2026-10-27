// SPDX-License-Identifier: MIT
#include <windows.h>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLayout>
#include <QMetaMethod>
#include <QShortcutEvent>
#include <QThread>
#include <QTimer>
#include "bridge.h"

static QString identity(QObject *object) {
    return object ? QString("%1/%2@%3").arg(object->objectName(),object->metaObject()->className())
        .arg(quintptr(object),0,16) : QString();
}
class Probe final : public QObject {
    Q_OBJECT
public:
    Probe(const QString &path,bool bridge) : QObject(qApp),m_file(path) {
        m_file.open(QIODevice::WriteOnly);
        QWidget *main=nullptr,*page=nullptr,*library=nullptr;
        // One explicit inventory; no discovery on input, paint or timers.
        const auto widgets=QApplication::allWidgets();
        for(auto w:widgets) {
            if(w->objectName()=="UiMainWindow") main=w;
#ifdef UNBOUND_EDIT_SOURCE_VIEWER
            if(w->objectName()=="WStackPage_Conform") page=w;
            if(w->objectName()=="widgetEditSourcePlayer") library=w;
#elif defined(UNBOUND_FAIRLIGHT_SOUND_LIBRARY)
            if(w->objectName()=="WStackPage_Audio") page=w;
            if(w->objectName()=="soundLibraryWidget") library=w;
#else
            if(w->objectName()=="WStackPage_Conform") page=w;
            if(w->objectName()=="widgetEditSoundLibrary") library=w;
#endif
        }
#ifdef UNBOUND_EDIT_SOURCE_VIEWER
        auto preview=library;
#else
        auto preview=library ? library->findChild<QWidget *>("m_PlayerPreview") : nullptr;
#endif
        log("start",{{"bridgeRequested",bridge},{"main",identity(main)},
            {"page",identity(page)},{"library",identity(library)},{"preview",identity(preview)}});
        // Bounded, explicit-start snapshot for docked/floating presentation comparison.
        if(library) {
            auto related=library->findChildren<QWidget *>();
            related.prepend(library);
            for(auto parent=library->parentWidget();parent;parent=parent->parentWidget()) related.prepend(parent);
            for(auto w:related) {
                const auto r=w->geometry();
                log("widget",{{"object",identity(w)},{"parent",identity(w->parentWidget())},
                    {"geometry",QJsonArray{r.x(),r.y(),r.width(),r.height()}},
                    {"minimum",QJsonArray{w->minimumWidth(),w->minimumHeight()}},
                    {"maximum",QJsonArray{w->maximumWidth(),w->maximumHeight()}},
                    {"visible",w->isVisible()},{"hidden",w->isHidden()},
                    {"layout",identity(w->layout())}});
            }
        }
        for(auto w:widgets) if(w->inherits("UiPlayerWidgetImp")) {
            for(const auto signature:{"IsActiveChanged(bool)","TransportControlTriggered(TransportControlMode,bool)"}) {
                const int signal=w->metaObject()->indexOfSignal(signature);
                const int slot=metaObject()->indexOfSlot(signature[0]=='I' ? "active(bool)" : "transport()");
                if(signal>=0 && w->thread()==qApp->thread())
                    QObject::connect(w,w->metaObject()->method(signal),this,metaObject()->method(slot),Qt::DirectConnection);
            }
        }
        connect(qApp,&QApplication::focusChanged,this,[this](QWidget *old,QWidget *now) {
            log("focus",{{"old",identity(old)},{"new",identity(now)}});
        });
        qApp->installEventFilter(this);
        if(bridge && main && page && preview) m_bridge=new AuditionActivationBridge(main,page,preview,
            [this](const QString &kind,QJsonObject data) { log(kind,data); },this);
        log("ready",{{"bridgeEnabled",m_bridge && m_bridge->enabled()}});
        QTimer::singleShot(300000,this,[this] { stop(); });
    }
    ~Probe() override { stop(); }
    void stop() {
        if(m_stopped) return;
        log("stop"); m_stopped=true;
        qApp->removeEventFilter(this);
        if(m_bridge) m_bridge->stop();
        m_file.flush();
    }
private slots:
    void active(bool value) { log("player-active",{{"player",identity(sender())},{"value",value}}); }
    void transport() { log("player-transport",{{"player",identity(sender())}}); }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        const auto type=event->type();
        if(type==QEvent::KeyPress || type==QEvent::Shortcut || type==QEvent::FocusIn || type==QEvent::MouseButtonPress ||
            ((type==QEvent::WindowActivate || type==QEvent::WindowDeactivate) &&
                qobject_cast<QWidget *>(object) && static_cast<QWidget *>(object)->isWindow())) {
            QJsonObject data{{"object",identity(object)},{"event",int(type)}};
            if(type==QEvent::KeyPress) {
                const auto key=static_cast<QKeyEvent *>(event)->key();
                // Record only the commands under investigation, never typed search text.
                if(key!=Qt::Key_J && key!=Qt::Key_K && key!=Qt::Key_L && key!=Qt::Key_Space && key!=Qt::Key_I && key!=Qt::Key_O) return false;
                data.insert("key",key);
            }
            if(type==QEvent::Shortcut) {
                auto shortcut=static_cast<QShortcutEvent *>(event);
                data.insert("shortcut",shortcut->key().toString()); data.insert("ambiguous",shortcut->isAmbiguous());
            }
            log("event",data);
        }
        return false;
    }
private:
    void log(const QString &kind,QJsonObject data={}) {
        if(m_stopped || QThread::currentThread()!=qApp->thread()) return;
        if(m_count>=2500 && kind!="stop") { stop(); return; }
        data.insert("kind",kind); data.insert("time",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        data.insert("focus",identity(QApplication::focusWidget()));
        data.insert("window",identity(QApplication::activeWindow()));
        m_file.write(QJsonDocument(data).toJson(QJsonDocument::Compact)+'\n'); m_file.flush(); ++m_count;
    }
    QFile m_file;
    AuditionActivationBridge *m_bridge=nullptr;
    bool m_stopped=false;
    int m_count=0;
};
static QPointer<Probe> probe;
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *argument) {
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    const auto operation=quintptr(argument);
    return QMetaObject::invokeMethod(app,[operation] {
        if(probe) { probe->stop(); probe->deleteLater(); probe=nullptr; }
        if(operation==3) return;
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto length=GetModuleFileNameW(module,path,32768);
        const auto directory=QString::fromWCharArray(path,int(length)).section('\\',0,-2)+"/logs";
        QDir().mkpath(directory);
        probe=new Probe(directory+QString("/%1-%2-%3.jsonl").arg(operation==2 ? "bridge" : "passive")
            .arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch()),operation==2);
    },Qt::QueuedConnection) ? 1 : 0;
}
#include "probe.moc"
