// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <qt_windows.h>
#include "../../../../src/core/panecatalogue.h"
static QJsonObject describe(QWidget *w) {
    const auto g=w->geometry();
    return {{"id",QString::number(quintptr(w),16)},{"name",w->objectName()},
        {"class",w->metaObject()->className()},{"visible",w->isVisible()},
        {"hidden",w->isHidden()},{"window",w->isWindow()},
        {"geometry",QJsonArray{g.x(),g.y(),g.width(),g.height()}}};
}
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp)return 0;
    return QMetaObject::invokeMethod(qApp,[] {
        QJsonArray rows,actions;
        for(auto w:QApplication::allWidgets()) {
            const auto name=w->objectName();
            if(!Unbound::PaneCatalogue::find(name) && !w->isWindow()
                && !name.contains("Aux") && !name.startsWith("WStackPage_")
                && name!="frameEditPageVideoPlayers" && !name.startsWith("unboundFloating_"))continue;
            auto row=describe(w);QJsonArray ancestors;
            for(auto p=w->parentWidget();p;p=p->parentWidget())ancestors.append(describe(p));
            row.insert("ancestors",ancestors);rows.append(row);
            for(auto action:w->findChildren<QAction *>())
                if(action->objectName().contains("dual",Qt::CaseInsensitive)
                    || action->objectName().contains("fullScreenTimeline",Qt::CaseInsensitive))
                    actions.append(QJsonObject{{"name",action->objectName()},{"text",action->text()},
                        {"checked",action->isChecked()},{"enabled",action->isEnabled()}});
        }
        const auto path=QDir::tempPath()+"/Unbound-page-context";QDir().mkpath(path);
        QSaveFile file(path+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+".json");
        if(file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(QJsonObject{{"widgets",rows},{"actions",actions}}).toJson());file.commit();
        }
    },Qt::QueuedConnection)?1:0;
}
