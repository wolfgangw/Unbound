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
            for(const auto pane:{"colorWheelsTab","colorMapperTab","curvesTab"})
                if(host->objectName()==QStringLiteral("unboundFloating_")+QLatin1String(pane))
                    Unbound::recordPaletteSizes(host,QLatin1String(pane));
    },Qt::QueuedConnection)?1:0;
}
