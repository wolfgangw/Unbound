// SPDX-License-Identifier: MIT
// Scoped passive event tracing, never QObject::dumpObjectInfo/private Qt data.
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <qt_windows.h>

static QString probeDirectory;
class LayoutTrace final : public QObject {
    QList<QPointer<QWidget>> targets;
    QJsonArray events;
    bool finished=false;
public:
    explicit LayoutTrace(QObject *parent):QObject(parent) {
        const QSet<QString> names={"frameEditPageWidgetsContainer","frameEditAuxWidgetsContainer",
            "frameEditPageVideoPlayers","frameEditTimelinesHolder","stackedWidget"};
        for (auto w:QApplication::allWidgets()) if (names.contains(w->objectName())) {
            if (w->objectName()=="stackedWidget" && !w->findChild<QWidget *>("pageEditFullTimelineTools")) continue;
            targets.append(w); w->installEventFilter(this);
        }
        QTimer::singleShot(120000,this,[this]{finish();});
    }
    ~LayoutTrace() override { detach(); }
    void detach() { for (auto w:targets) if (w) w->removeEventFilter(this); }
    void finish() {
        if (finished) return;
        finished=true; detach();
        QFile file(QDir(probeDirectory).filePath(QStringLiteral("layout-trace-%1-%2.json").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch())));
        if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{
            {"events",events},{"limitReached",events.size()>=500},
            {"resolveBase",QString::number(quintptr(GetModuleHandleW(nullptr)),16)}}).toJson());
        deleteLater();
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        if (finished) return false;
        const auto type=event->type();
        if (type!=QEvent::ParentChange && type!=QEvent::Show && type!=QEvent::Hide && type!=QEvent::Resize && type!=QEvent::Move) return false;
        if (events.size()>=500) return false;
        auto w=static_cast<QWidget *>(object); auto r=w->geometry();
        void *frames[48]; const auto count=CaptureStackBackTrace(0,48,frames,nullptr);
        QJsonArray stack;
        for (USHORT i=0;i<count;++i) stack.append(QString::number(quintptr(frames[i]),16));
        events.append(QJsonObject{{"name",w->objectName()},{"event",int(type)},
            {"parent",w->parentWidget()?w->parentWidget()->objectName():QString()},
            {"geometry",QJsonArray{r.x(),r.y(),r.width(),r.height()}},
            {"stack",stack}});
        return false;
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *)
{
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        static QPointer<LayoutTrace> installed;
        if (installed) { installed->finish(); installed=nullptr; return; }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto length=GetModuleFileNameW(module,path,32768);
        probeDirectory=QString::fromWCharArray(path,int(length)).section('\\',0,-2);
        installed=new LayoutTrace(qApp);
    },Qt::QueuedConnection) ? 1 : 0;
}
