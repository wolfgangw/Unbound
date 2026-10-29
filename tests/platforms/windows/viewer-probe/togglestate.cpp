// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAbstractButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDir>
#include <qt_windows.h>
#include "../../../../src/core/panecatalogue.h"
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    if(!qApp) return 0;
    return QMetaObject::invokeMethod(qApp,[] {
        QJsonArray rows;
        for(auto w:QApplication::allWidgets()) {
            bool relevant=false;
            for(const auto &pane:Unbound::PaneCatalogue::panes())
                if(w->objectName()==pane.objectName || pane.toggleNames().contains(w->objectName())) relevant=true;
            if(!relevant) continue;
            QJsonArray ancestry;
            for(auto p=w;p;p=p->parentWidget()) ancestry.append(p->objectName());
            QJsonObject row{{"name",w->objectName()},{"visible",w->isVisible()},
                {"window",QString::number(quintptr(w->window()),16)},{"ancestry",ancestry}};
            if(auto b=qobject_cast<QAbstractButton *>(w)) {
                row.insert("checked",b->isChecked());row.insert("checkable",b->isCheckable());row.insert("enabled",b->isEnabled());
            }
            rows.append(row);
        }
        QSaveFile out(QDir::tempPath()+"/Unbound-toggle-state.json");
        if(out.open(QIODevice::WriteOnly)){out.write(QJsonDocument(rows).toJson());out.commit();}
    },Qt::QueuedConnection)?1:0;
}
