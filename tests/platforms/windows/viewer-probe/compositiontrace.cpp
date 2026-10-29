// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QOpenGLWidget>
#include <QPointer>
#include <QSaveFile>
#include <QTimer>
#include <qt_windows.h>

// Standalone research observer; no native handlers, rendering settings or input
// are changed. Pair only documented GL composition and framebuffer-size signals.
class CompositionTrace final : public QObject {
    QElapsedTimer clock;
    QJsonArray rows,metadata;
    QList<QMetaObject::Connection> connections;
    int dropped=0;
    void record(QOpenGLWidget *gl,int id,const char *phase,qint64 start=-1) {
        if(rows.size()>=12000) {++dropped;return;}
        const auto now=clock.nsecsElapsed()/1000;
        QJsonObject row{{"us",double(now)},{"id",id},{"window",gl->window()->objectName()},
            {"phase",phase},{"visible",gl->isVisible()}};
        if(start>=0) row.insert("duration_us",double(now-start));
        if(gl->context()) row.insert("swap_interval",gl->context()->format().swapInterval());
        rows.append(row);
    }
public:
    explicit CompositionTrace() : QObject(qApp) {
        setObjectName("UnboundCompositionTrace"); clock.start();
        int id=0;
        for(auto window:QApplication::topLevelWidgets()) {
            if(window->objectName()!=QLatin1String("UiMainWindow") && !window->property("unboundFloatingWindow").toBool()) continue;
            for(auto gl:window->findChildren<QOpenGLWidget *>()) {
                if(++id>512) break;
                metadata.append(QJsonObject{{"id",id},{"name",gl->objectName()},
                    {"class",gl->metaObject()->className()},{"initial_window",window->objectName()}});
                auto compose=std::make_shared<qint64>(-1),resize=std::make_shared<qint64>(-1);
                connections.append(connect(gl,&QOpenGLWidget::aboutToCompose,this,[this,gl,id,compose] {
                    *compose=clock.nsecsElapsed()/1000; record(gl,id,"compose_begin");
                }));
                connections.append(connect(gl,&QOpenGLWidget::frameSwapped,this,[this,gl,id,compose] {
                    record(gl,id,"compose_end",*compose); *compose=-1;
                }));
                connections.append(connect(gl,&QOpenGLWidget::aboutToResize,this,[this,gl,id,resize] {
                    *resize=clock.nsecsElapsed()/1000; record(gl,id,"resize_begin");
                }));
                connections.append(connect(gl,&QOpenGLWidget::resized,this,[this,gl,id,resize] {
                    record(gl,id,"resize_end",*resize); *resize=-1;
                }));
            }
        }
        QTimer::singleShot(60000,this,[this] {finish();});
    }
    void finish() {
        for(const auto &connection:connections) disconnect(connection);
        const auto path=QDir::tempPath()+"/Unbound-composition-traces"; QDir().mkpath(path);
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".json");
        const auto bytes=QJsonDocument(QJsonObject{{"events",rows},{"surfaces",metadata},{"dropped",dropped},
            {"share_gl_contexts",QCoreApplication::testAttribute(Qt::AA_ShareOpenGLContexts)}}).toJson();
        if(file.open(QIODevice::WriteOnly)) {if(file.write(bytes)==bytes.size()) file.commit();else file.cancelWriting();}
        setObjectName({}); deleteLater();
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp) return 0;
    return QMetaObject::invokeMethod(qApp,[] {
        if(auto existing=qApp->findChild<QObject *>("UnboundCompositionTrace")) static_cast<CompositionTrace *>(existing)->finish();
        else new CompositionTrace;
    },Qt::QueuedConnection)?1:0;
}
