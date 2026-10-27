// SPDX-License-Identifier: MIT
// Passive, bounded public-Qt tracing. No native addresses or implementation data.
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <qt_windows.h>

static QString probeDirectory;
class PaintTrace final : public QObject {
    QPointer<QWidget> root,timeline,main;
    QJsonArray events;
    QElapsedTimer clock;
    bool finished=false;
    static QJsonArray rect(QWidget *w) {
        if(!w) return {};
        const auto r=w->geometry(); return {r.x(),r.y(),r.width(),r.height()};
    }
public:
    explicit PaintTrace(QObject *parent):QObject(parent) {
        for(auto w:QApplication::allWidgets()) {
            if(w->objectName()=="frameEditPageWidgetsContainer") root=w;
            if(w->objectName()=="frameEditTimelinesHolder") timeline=w;
        }
        if(root) main=root->window();
        clock.start(); qApp->installEventFilter(this);
        QTimer::singleShot(60000,this,[this]{finish();});
    }
    void finish() {
        if(finished) return;
        finished=true; qApp->removeEventFilter(this);
        QFile file(QDir(probeDirectory).filePath(QStringLiteral("paint-trace-%1.json").arg(QDateTime::currentMSecsSinceEpoch())));
        if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{{"events",events},{"limitReached",events.size()>=20000}}).toJson());
        deleteLater();
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(finished || events.size()>=20000) return false;
        QString type;
        switch(event->type()) {
        case QEvent::Move: type="Move"; break;
        case QEvent::Resize: type="Resize"; break;
        case QEvent::Paint: type="Paint"; break;
        case QEvent::UpdateRequest: type="UpdateRequest"; break;
        case QEvent::LayoutRequest: type="LayoutRequest"; break;
        case QEvent::MouseButtonPress: type="Press"; break;
        case QEvent::MouseButtonRelease: type="Release"; break;
        case QEvent::Show: type="Show"; break;
        case QEvent::Hide: type="Hide"; break;
        default: return false;
        }
        auto w=qobject_cast<QWidget *>(object);
        if(!w || !main || w->window()!=main) return false;
        events.append(QJsonObject{{"us",double(clock.nsecsElapsed()/1000)},
            {"event",type},{"name",w->objectName()},{"class",w->metaObject()->className()},
            {"parent",w->parentWidget()?w->parentWidget()->objectName():QString()},
            {"geometry",rect(w)},{"timeline",rect(timeline)},{"root",rect(root)},
            {"updates",w->updatesEnabled()},{"rootUpdates",root && root->updatesEnabled()},
            {"mainUpdates",main->updatesEnabled()}});
        return false;
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        static QPointer<PaintTrace> installed;
        if(installed) { installed->finish(); installed=nullptr; return; }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto length=GetModuleFileNameW(module,path,32768);
        probeDirectory=QString::fromWCharArray(path,int(length)).section('\\',0,-2);
        installed=new PaintTrace(qApp);
    },Qt::QueuedConnection)?1:0;
}
