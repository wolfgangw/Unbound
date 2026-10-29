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
    bool done=false;
    void record(QWidget *widget,int event,bool queued) {
        if(done || ++count>1200) return;
        QJsonObject row{{"ms",int(elapsed.elapsed())},{"event",event},{"queued",queued},
            {"name",widget->objectName()},{"visible",widget->isVisible()},
            {"x",widget->x()},{"y",widget->y()},{"width",widget->width()},
            {"minimumWidth",widget->minimumWidth()},{"widthHint",widget->minimumSizeHint().width()},
            {"height",widget->height()},{"minimum",widget->minimumHeight()},
            {"hint",widget->minimumSizeHint().height()}};
        file.write(QJsonDocument(row).toJson(QJsonDocument::Compact)+"\n"); file.flush();
    }
public:
    PaletteLayoutTrace() : QObject(qApp) {
        setObjectName("UnboundPaletteLayoutTrace");
        file.open(QIODevice::WriteOnly|QIODevice::Truncate); elapsed.start();
        qApp->installEventFilter(this);
        for(auto w:QApplication::topLevelWidgets())if(w->objectName().startsWith("unboundFloating_"))record(w,-1,false);
        QTimer::singleShot(60000,this,[this] { finish(); });
    }
    void finish() {done=true;qApp->removeEventFilter(this);file.close();deleteLater();}
    bool eventFilter(QObject *object,QEvent *event) override {
        const auto name=object->objectName();
        if(name!=QStringLiteral("unboundFloating_colorMapperTab") && name!=QStringLiteral("unboundFloating_curvesTab")
            && name!=QStringLiteral("unboundFloating_colorWheelsTab") && name!=QStringLiteral("unboundFloating_colorMatchTab")) return false;
        const auto type=event->type();
        if(type!=QEvent::Resize && type!=QEvent::LayoutRequest && type!=QEvent::Show && type!=QEvent::Hide) return false;
        auto widget=qobject_cast<QWidget *>(object); if(!widget) return false;
        record(widget,type,false);
        QPointer<QWidget> guard(widget);
        QTimer::singleShot(0,this,[this,guard,type] {if(guard)record(guard,type,true);});
        return false;
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    return QMetaObject::invokeMethod(qApp,[] {
        if(auto old=qApp->findChild<QObject *>("UnboundPaletteLayoutTrace"))static_cast<PaletteLayoutTrace *>(old)->finish();
        else new PaletteLayoutTrace;
    },Qt::QueuedConnection)?1:0;
}
