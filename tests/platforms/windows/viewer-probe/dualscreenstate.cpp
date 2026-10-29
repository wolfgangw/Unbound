// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <qt_windows.h>
static QString identity(const QObject *o) {return o?QString::number(quintptr(o),16):QString();}
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[] {
        QJsonArray rows; bool truncated=false;
        for(auto w:QApplication::allWidgets()) {
            if(rows.size()>=30000) {truncated=true;break;}
            const auto g=w->geometry();
            QJsonObject row{{"id",identity(w)},{"parent",identity(w->parentWidget())},
                {"window",identity(w->window())},{"name",w->objectName()},
                {"class",w->metaObject()->className()},{"visible",w->isVisible()},{"hidden",w->isHidden()},
                {"geometry",QJsonArray{g.x(),g.y(),g.width(),g.height()}}};
            if(auto b=qobject_cast<QAbstractButton *>(w)) {
                row.insert("checked",b->isChecked()); row.insert("enabled",b->isEnabled());
                row.insert("tooltip",b->toolTip());
            }
            if(auto v=qobject_cast<QAbstractItemView *>(w))row.insert("model",identity(v->model()));
            rows.append(row);
        }
        const auto path=QDir::tempPath()+"/Unbound-dual-screen";QDir().mkpath(path);
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".json");
        if(file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(QJsonObject{{"widgets",rows},{"truncated",truncated}}).toJson());file.commit();
        }
    },Qt::QueuedConnection)?1:0;
}
