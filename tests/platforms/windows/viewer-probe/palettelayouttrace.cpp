// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QDir>
#include <qt_windows.h>
class PaletteLayoutTrace : public QObject {
    QFile file{QDir::tempPath()+"/Unbound-palette-layout.jsonl"};
    QElapsedTimer elapsed;
    int count=0;
    void record(QWidget *widget,int event,bool queued) {
        if(++count>600) return;
        QJsonObject row{{"ms",int(elapsed.elapsed())},{"event",event},{"queued",queued},
            {"height",widget->height()},{"minimum",widget->minimumHeight()},
            {"hint",widget->minimumSizeHint().height()}};
        file.write(QJsonDocument(row).toJson(QJsonDocument::Compact)+"\n"); file.flush();
    }
public:
    PaletteLayoutTrace() : QObject(qApp) {
        file.open(QIODevice::WriteOnly|QIODevice::Truncate); elapsed.start();
        qApp->installEventFilter(this);
        QTimer::singleShot(60000,this,[this] { qApp->removeEventFilter(this); deleteLater(); });
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object->objectName()!=QStringLiteral("unboundFloating_colorMapperTab")) return false;
        const auto type=event->type();
        if(type!=QEvent::Resize && type!=QEvent::LayoutRequest && type!=QEvent::Show) return false;
        auto widget=qobject_cast<QWidget *>(object); if(!widget) return false;
        record(widget,type,false);
        QPointer<QWidget> guard(widget);
        QTimer::singleShot(0,this,[this,guard,type] {if(guard)record(guard,type,true);});
        return false;
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    return QMetaObject::invokeMethod(qApp,[] {new PaletteLayoutTrace;},Qt::QueuedConnection)?1:0;
}
