// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QWidget>
#include <QPointer>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QKeyEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <functional>

// Diagnostic-only dynamic receiver for existing native bool signals.
class BoolSignalProbe : public QObject {
    std::function<void(bool)> report;
public:
    BoolSignalProbe(QObject *source, int signal, QObject *owner, std::function<void(bool)> callback)
        : QObject(owner), report(std::move(callback)) {
        QMetaObject::connect(source,signal,this,QObject::staticMetaObject.methodCount(),Qt::DirectConnection,nullptr);
    }
    int qt_metacall(QMetaObject::Call call,int method,void **args) override {
        method=QObject::qt_metacall(call,method,args);
        if(method<0)return method;
        if(call==QMetaObject::InvokeMetaMethod) {
            if(method==0 && QThread::currentThread()==qApp->thread())report(*static_cast<bool*>(args[1]));
            --method;
        }
        return method;
    }
};

static QString id(QObject *o) { return o ? QString("%1/%2@%3").arg(o->objectName(),o->metaObject()->className()).arg(quintptr(o),0,16) : QString(); }
class Probe : public QObject {
    QFile file;
    QList<QPointer<QWidget>> players;
    bool queued=false;
    int count=0;
public:
    Probe(const QString &path):QObject(qApp),file(path) {
        file.open(QIODevice::WriteOnly|QIODevice::Append);
        for(auto w:QApplication::allWidgets()) {
            if(w->inherits("UiPlayerWidgetImp") || w->objectName()=="m_PlayerPreview") {
                players.append(w);
                QJsonArray methods,properties;
                const auto m=w->metaObject();
                for(int i=0;i<m->methodCount();++i) methods.append(QString::fromLatin1(m->method(i).methodSignature()));
                for(int i=0;i<m->propertyCount();++i) properties.append(QString::fromLatin1(m->property(i).name()));
                log({{"kind","inventory"},{"player",id(w)},{"methods",methods},{"properties",properties}});
                for(const auto signature:{"IsActiveChanged(bool)","MuteAudioChanged(bool)"}) {
                    const int signal=m->indexOfSignal(signature);
                    if(signal>=0 && w->thread()==qApp->thread()) {
                        const auto player=id(w);const QString name=QString::fromLatin1(signature);
                        new BoolSignalProbe(w,signal,this,[this,player,name](bool value){log({{"kind","signal"},{"player",player},{"signal",name},{"value",value}});});
                    }
                }
            }
        }
        connect(qApp,&QApplication::focusChanged,this,[this](QWidget *a,QWidget *b){log({{"kind","focus"},{"old",id(a)},{"new",id(b)}});snapshot();});
        qApp->installEventFilter(this);
        snapshot();
        QTimer::singleShot(180000,this,[this]{deleteLater();});
    }
    ~Probe(){qApp->removeEventFilter(this);log({{"kind","stop"}});}
    void log(QJsonObject data) {
        data.insert("time",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        data.insert("activeWindow",id(QApplication::activeWindow()));
        data.insert("focus",id(QApplication::focusWidget()));
        file.write(QJsonDocument(data).toJson(QJsonDocument::Compact)+'\n');file.flush();
    }
    void snapshot() {
        if(queued)return;queued=true;
        QTimer::singleShot(0,this,[this]{queued=false;
            for(auto p:players) if(p) {
                QJsonObject values;
                for(int i=0;i<p->metaObject()->propertyCount();++i) {
                    auto prop=p->metaObject()->property(i);const QString name=prop.name();
                    if(name.contains("audio",Qt::CaseInsensitive)||name.contains("mute",Qt::CaseInsensitive)||name.contains("active",Qt::CaseInsensitive)||name.contains("volume",Qt::CaseInsensitive))
                        values.insert(name,QJsonValue::fromVariant(prop.read(p)));
                }
                log({{"kind","state"},{"player",id(p)},{"window",id(p->window())},{"visible",p->isVisible()},{"enabled",p->isEnabled()},{"properties",values}});
            }
        });
    }
    bool eventFilter(QObject *o,QEvent *e) override {
        const auto t=e->type();
        if(t!=QEvent::WindowActivate&&t!=QEvent::WindowDeactivate&&t!=QEvent::FocusIn&&t!=QEvent::FocusOut&&t!=QEvent::MouseButtonPress&&t!=QEvent::KeyPress)return false;
        if(t==QEvent::KeyPress) {
            auto k=static_cast<QKeyEvent*>(e)->key();
            if(k!=Qt::Key_J&&k!=Qt::Key_K&&k!=Qt::Key_L&&k!=Qt::Key_Space)return false;
        }
        auto w=qobject_cast<QWidget*>(o);if(!w)return false;
        if((t==QEvent::WindowActivate||t==QEvent::WindowDeactivate)&&!w->isWindow())return false;
        if(++count>3000){qApp->removeEventFilter(this);deleteLater();return false;}
        log({{"kind","event"},{"object",id(o)},{"event",int(t)}});snapshot();return false;
    }
};
static QPointer<Probe> probe;
extern "C" int UnboundAuditionAudioTrace(const char *path) {
    if(!qApp)return 0;const QString output=QString::fromUtf8(path);
    return QMetaObject::invokeMethod(qApp,[output]{if(probe)delete probe.data();probe=new Probe(output);},Qt::QueuedConnection)?1:0;
}
