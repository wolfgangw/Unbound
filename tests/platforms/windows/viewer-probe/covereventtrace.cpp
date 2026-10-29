// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>
#include <qt_windows.h>
class CoverEventTrace final : public QObject {
    QJsonArray rows;QElapsedTimer clock;
    void record(const QString &kind,QWidget *w=nullptr) {
        if(rows.size()>=1000)return;
        DWORD pid=0;auto foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,&pid);
        wchar_t nativeClass[256]{};GetClassNameW(foreground,nativeClass,256);
        QJsonObject row{{"ms",double(clock.elapsed())},{"event",kind},{"qt_state",int(qApp->applicationState())},
            {"foreground_own",pid==GetCurrentProcessId()},{"foreground_class",QString::fromWCharArray(nativeClass)}};
        bool coverVisible=false;
        for(auto h=GetTopWindow(nullptr);h;h=GetWindow(h,GW_HWNDNEXT)) {
            wchar_t name[256]{};GetClassNameW(h,name,256);
            if(QString::fromWCharArray(name)=="UnboundLayoutCoverExperiment" && IsWindowVisible(h))coverVisible=true;
        }
        row.insert("cover_visible",coverVisible);
        if(w) {
            row.insert("name",w->objectName());row.insert("class",w->metaObject()->className());
            row.insert("window",w->isWindow());row.insert("modal",w->isModal());
            QJsonArray parents;for(auto p=w->parentWidget();p;p=p->parentWidget())parents.append(p->objectName());row.insert("parents",parents);
        }
        rows.append(row);
    }
    bool eventFilter(QObject *o,QEvent *e) override {
        if(e->type()==QEvent::Show) if(auto d=qobject_cast<QDialog *>(o)) record("dialog_show",d);
        if(e->type()==QEvent::Show || e->type()==QEvent::Hide)
            if(auto w=qobject_cast<QWidget *>(o))
                if(w->objectName().startsWith("WStackPage_") || w->property("unboundFloatingWindow").toBool())
                    record(e->type()==QEvent::Show?"show":"hide",w);
        return false;
    }
public:
    CoverEventTrace():QObject(qApp) {
        setObjectName("UnboundCoverEventTrace");clock.start();qApp->installEventFilter(this);record("start");
        connect(qApp,&QGuiApplication::applicationStateChanged,this,[this](Qt::ApplicationState){record("application_state");});
        QTimer::singleShot(60000,this,[this]{finish();});
    }
    void finish() {
        qApp->removeEventFilter(this);QSaveFile file(QDir::tempPath()+"/Unbound-cover-events.json");
        if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(rows).toJson());file.commit();}deleteLater();
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[]{
        if(auto old=qApp->findChild<QObject *>("UnboundCoverEventTrace"))static_cast<CoverEventTrace *>(old)->finish();
        else new CoverEventTrace;
    },Qt::QueuedConnection)?1:0;
}
