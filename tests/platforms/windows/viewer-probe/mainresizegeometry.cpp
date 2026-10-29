// SPDX-License-Identifier: MIT
#include "panemainresizegeometry.h"
#include <qt_windows.h>
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    return QMetaObject::invokeMethod(qApp,[] {
        if(auto previous=qApp->findChild<QObject *>("unboundMainResizeExperiment")) { delete previous; return; }
        QWidget *main=nullptr;
        for(auto w:QApplication::topLevelWidgets()) if(w->objectName()=="UiMainWindow") main=w;
        if(!main) return;
        auto owner=new QObject(qApp);owner->setObjectName("unboundMainResizeExperiment");
        for(auto host:QApplication::topLevelWidgets()) if(host->objectName().startsWith("unboundFloating_")) {
            auto guard=new Unbound::PaneMainResizeGeometry(host,main,[host](const QByteArray &bytes){host->restoreGeometry(bytes);});
            QObject::connect(owner,&QObject::destroyed,guard,[guard]{delete guard;});
        }
    },Qt::QueuedConnection)?1:0;
}
