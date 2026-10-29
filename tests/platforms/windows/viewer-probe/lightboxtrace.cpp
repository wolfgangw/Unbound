// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractButton>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <qt_windows.h>
class LightboxTrace final : public QObject {
    QElapsedTimer clock;QJsonArray rows;
    QList<QPointer<QAbstractButton>> buttons;
    bool queued=false;
    void sample(const QString &reason) {
        if(rows.size()>=2000)return;
        QJsonArray states;
        for(auto b:buttons)if(b) {
            QJsonArray parents;
            for(auto p=b->parentWidget();p;p=p->parentWidget())
                parents.append(QJsonObject{{"name",p->objectName()},{"visible",p->isVisible()},{"enabled",p->isEnabled()}});
            states.append(QJsonObject{{"name",b->objectName()},{"enabled",b->isEnabled()},
                {"visible",b->isVisible()},{"checked",b->isChecked()},{"parents",parents}});
        }
        rows.append(QJsonObject{{"ms",double(clock.elapsed())},{"reason",reason},{"buttons",states}});
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()!=QEvent::EnabledChange && event->type()!=QEvent::Show && event->type()!=QEvent::Hide)return false;
        auto b=qobject_cast<QAbstractButton *>(object);
        if(!b || !buttons.contains(b))return false;
        sample(QString::number(int(event->type()))+":"+b->objectName());
        if(!queued) {queued=true;QTimer::singleShot(0,this,[this]{queued=false;sample("queued_state");});}
        return false;
    }
public:
    LightboxTrace():QObject(qApp) {
        setObjectName("UnboundLightboxTrace");clock.start();
        for(auto w:QApplication::allWidgets())if(auto b=qobject_cast<QAbstractButton *>(w))
            if(b->objectName().startsWith("btnColorToggle")) {
                buttons.append(b);
                connect(b,&QAbstractButton::toggled,this,[this,b](bool on){sample("toggled:"+b->objectName()+":"+QString::number(on));});
                connect(b,&QAbstractButton::clicked,this,[this,b]{
                    sample("clicked:"+b->objectName());
                    QTimer::singleShot(0,this,[this]{sample("after_click_queued");});
                    QTimer::singleShot(100,this,[this]{sample("after_click_100ms");});
                });
            }
        qApp->installEventFilter(this);sample("start");
        QTimer::singleShot(120000,this,[this]{finish();});
    }
    void finish() {
        sample("finish");qApp->removeEventFilter(this);
        const auto path=QDir::tempPath()+"/Unbound-lightbox-traces";QDir().mkpath(path);
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".json");
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(rows).toJson());file.commit();}deleteLater();
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[]{
        if(auto old=qApp->findChild<QObject *>("UnboundLightboxTrace"))static_cast<LightboxTrace *>(old)->finish();
        else new LightboxTrace;
    },Qt::QueuedConnection)?1:0;
}
