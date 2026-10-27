// SPDX-License-Identifier: MIT
// Bounded public-Qt diagnostic; never consumes input or changes player state.
#include <QApplication>
#include <QAbstractNativeEventFilter>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <qt_windows.h>

static QString directory;
class NavigationTrace final : public QObject, public QAbstractNativeEventFilter {
    QFile file;
    QElapsedTimer clock;
    bool pending=false;
    int count=0,paints=0,updates=0;
    int traceEvents=0;
    qint64 started=0;
    QPointer<QWidget> source;
    void write(QJsonObject value) {
        value.insert("us",double(clock.nsecsElapsed()/1000));
        file.write(QJsonDocument(value).toJson(QJsonDocument::Compact)+"\n");
        file.flush();
    }
public:
    NavigationTrace():QObject(qApp),file(QDir(directory).filePath(
        QStringLiteral("navigation-%1.jsonl").arg(QDateTime::currentMSecsSinceEpoch()))) {
        for(auto w:QApplication::allWidgets())
            if(w->objectName()=="stackedEditSourceOfflinePlayers") source=w;
        file.open(QIODevice::WriteOnly); clock.start();
        qApp->installEventFilter(this);
        qApp->installNativeEventFilter(this);
        QTimer::singleShot(180000,this,[this]{ deleteLater(); });
    }
    ~NavigationTrace() override { qApp->removeEventFilter(this); qApp->removeNativeEventFilter(this); }
    bool nativeEventFilter(const QByteArray &type,void *message,long *) override {
        if(type!="windows_generic_MSG" || !message || pending || count>=100) return false;
        const auto msg=static_cast<MSG *>(message);
        if(msg->message!=WM_KEYDOWN || (msg->wParam!=VK_UP && msg->wParam!=VK_DOWN)) return false;
        begin(int(msg->wParam),QStringLiteral("native-key"));
        return false;
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(count && traceEvents<30 && clock.nsecsElapsed()-started<1000000000LL
            && (event->type()==QEvent::Paint || event->type()==QEvent::UpdateRequest)) {
            ++traceEvents;
            write({{"event",event->type()==QEvent::Paint?"paint":"update"},
                {"target",object->objectName()},{"class",object->metaObject()->className()},
                {"elapsedUs",double((clock.nsecsElapsed()-started)/1000)}});
        }
        if(pending) {
            if(event->type()==QEvent::Paint) ++paints;
            if(event->type()==QEvent::UpdateRequest) ++updates;
        }
        if(event->type()!=QEvent::KeyPress || pending || count>=100) return false;
        auto key=static_cast<QKeyEvent *>(event);
        if(key->key()!=Qt::Key_Up && key->key()!=Qt::Key_Down) return false;
        begin(key->key(),object->objectName());
        return false;
    }
    void begin(int key,const QString &target) {
        pending=true; paints=updates=traceEvents=0; started=clock.nsecsElapsed(); ++count;
        write({{"event","key"},{"key",key},{"target",target},
            {"sourceVisible",source && source->isVisible()}});
        QTimer::singleShot(0,this,[this] {
            write({{"event","queued"},{"elapsedUs",double((clock.nsecsElapsed()-started)/1000)},
                {"paints",paints},{"updates",updates}});
            pending=false;
        });
        const auto sample=count;
        const auto start=started;
        for(int delay:{16,50,100,250,500,1000}) QTimer::singleShot(delay,this,[this,sample,start,delay] {
            write({{"event","checkpoint"},{"sample",sample},{"delayMs",delay},
                {"elapsedUs",double((clock.nsecsElapsed()-start)/1000)}});
        });
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        static QPointer<NavigationTrace> trace;
        if(trace) { delete trace.data(); return; }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto size=GetModuleFileNameW(module,path,32768);
        directory=QString::fromWCharArray(path,int(size)).section('\\',0,-2);
        trace=new NavigationTrace;
    },Qt::QueuedConnection)?1:0;
}
