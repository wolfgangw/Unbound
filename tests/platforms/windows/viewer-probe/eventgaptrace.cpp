// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractEventDispatcher>
#include <QElapsedTimer>
#include <QPointer>
#include <QWidget>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDir>
#include <QTimer>
#include <qt_windows.h>
class EventGapTrace final : public QObject {
    QElapsedTimer clock;
    qint64 last=0;
    QPointer<QObject> previous;
    int previousType=-1;
    QJsonArray rows;
    static QJsonObject describe(QObject *o,int type) {
        QJsonObject result{{"type",type}};
        if(!o)return result;
        result.insert("name",o->objectName());result.insert("class",o->metaObject()->className());
        QStringList parents;for(auto p=o->parent();p;p=p->parent())parents.append(p->objectName());
        result.insert("parents",parents.join('/'));
        return result;
    }
    void sample(QObject *o,int type) {
        const auto now=clock.nsecsElapsed()/1000;
        if(last && now-last>20000 && rows.size()<1000)
            rows.append(QJsonObject{{"us",double(now)},{"gap_us",double(now-last)},
                {"previous",describe(previous,previousType)},{"next",describe(o,type)}});
        last=now;previous=o;previousType=type;
    }
    bool eventFilter(QObject *o,QEvent *e) override {sample(o,int(e->type()));return false;}
public:
    EventGapTrace():QObject(qApp) {
        setObjectName("UnboundEventGapTrace");clock.start();qApp->installEventFilter(this);
        auto dispatcher=QAbstractEventDispatcher::instance();
        connect(dispatcher,&QAbstractEventDispatcher::aboutToBlock,this,[this] {sample(nullptr,-2);last=0;});
        connect(dispatcher,&QAbstractEventDispatcher::awake,this,[this] {last=clock.nsecsElapsed()/1000;previous=nullptr;previousType=-3;});
        QTimer::singleShot(60000,this,[this] {finish();});
    }
    void finish() {
        qApp->removeEventFilter(this);
        QSaveFile file(QDir::tempPath()+"/Unbound-event-gaps.json");
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(rows).toJson());file.commit();}
        deleteLater();
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[]{
        if(auto old=qApp->findChild<QObject *>("UnboundEventGapTrace"))static_cast<EventGapTrace *>(old)->finish();
        else new EventGapTrace;
    },Qt::QueuedConnection)?1:0;
}
