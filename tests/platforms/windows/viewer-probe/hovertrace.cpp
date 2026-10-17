// SPDX-License-Identifier: MIT
// Passive bounded viewer visibility trace; no private getters/native handlers.
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
static QString outputDirectory;
static QString id(const QObject *object) { return QString::number(quintptr(object),16); }
static QJsonObject describe(QWidget *w) {
    const auto r=w->geometry();
    return {{"id",id(w)},{"name",w->objectName()},{"class",w->metaObject()->className()},
        {"parent",id(w->parentWidget())},{"hidden",w->isHidden()},{"visible",w->isVisible()},
        {"updates",w->updatesEnabled()},{"geometry",QJsonArray{r.x(),r.y(),r.width(),r.height()}}};
}
class HoverTrace final : public QObject {
    QList<QPointer<QWidget>> targets;
    QJsonArray initial,events;
    bool done=false;
public:
    explicit HoverTrace(QObject *owner):QObject(owner) {
        QSet<QWidget *> included;
        for(auto w:QApplication::allWidgets()) {
            if(w->objectName()!="widgetEditTimelinePlayer" && w->objectName()!="stackedEditSourceOfflinePlayers") continue;
            included.insert(w);
            for(auto child:w->findChildren<QWidget *>()) included.insert(child);
            for(auto p=w->parentWidget();p;p=p->parentWidget()) included.insert(p);
        }
        for(auto w:included) { targets.append(w); initial.append(describe(w)); w->installEventFilter(this); }
        QTimer::singleShot(120000,this,[this] { finish(); });
    }
    void finish() {
        if(done) return;
        done=true;
        QJsonArray final;
        for(auto w:targets) if(w) { w->removeEventFilter(this); final.append(describe(w)); }
        QFile file(QDir(outputDirectory).filePath(QString("hover-%1-%2.json").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch())));
        if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{
            {"initial",initial},{"events",events},{"final",final},
            {"resolveBase",QString::number(quintptr(GetModuleHandleW(nullptr)),16)},
            {"unboundBase",QString::number(quintptr(GetModuleHandleW(L"Unbound.dll")),16)},
            {"truncated",events.size()>=2000}}).toJson());
        deleteLater();
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        const auto type=event->type();
        if(done || events.size()>=2000 || (type!=QEvent::Show && type!=QEvent::Hide &&
            type!=QEvent::ShowToParent && type!=QEvent::HideToParent &&
            type!=QEvent::ParentChange && type!=QEvent::Resize && type!=QEvent::Move)) return false;
        auto record=describe(static_cast<QWidget *>(object));
        record.insert("event",int(type)); record.insert("time",QDateTime::currentMSecsSinceEpoch());
        if(type==QEvent::Hide || type==QEvent::Show) {
            void *frames[32]; const auto n=CaptureStackBackTrace(0,32,frames,nullptr);
            QJsonArray stack; for(USHORT i=0;i<n;++i) stack.append(QString::number(quintptr(frames[i]),16));
            record.insert("stack",stack);
        }
        events.append(record); return false;
    }
};
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        static QPointer<HoverTrace> installed;
        if(installed) { installed->finish(); installed=nullptr; return; }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto n=GetModuleFileNameW(module,path,32768);
        outputDirectory=QString::fromWCharArray(path,int(n)).section('\\',0,-2);
        installed=new HoverTrace(qApp);
    },Qt::QueuedConnection) ? 1 : 0;
}
