// SPDX-License-Identifier: MIT
#include "palettesizediagnostics.h"
#include <QApplication>
#include <qt_windows.h>
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        for(auto widget:QApplication::allWidgets())
            if(widget->objectName()==QStringLiteral("timelineFrame") && widget->parentWidget())
                Unbound::recordPaletteSizes(widget->parentWidget(),QStringLiteral("colorTimelineRegion"));
        for(auto host:QApplication::topLevelWidgets())
            if(host->objectName().startsWith("unboundFloating_"))
                Unbound::recordPaletteSizes(host,host->objectName().mid(16));
    },Qt::QueuedConnection)?1:0;
}
