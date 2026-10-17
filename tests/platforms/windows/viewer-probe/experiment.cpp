// SPDX-License-Identifier: MIT
#include "editviewers.h"
#include <qt_windows.h>
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *)
{
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || QCoreApplication::closingDown()) return 0;
    static QPointer<EditViewers> installed;
    static Unbound::PaneSettings settings;
    return QMetaObject::invokeMethod(app,[] {
        if(installed) return;
        for(auto w:QApplication::topLevelWidgets()) if(w->objectName()=="UiMainWindow") {
            auto page=w->findChild<QWidget *>("WStackPage_Conform");
            auto players=page ? page->findChild<QWidget *>("frameEditPageVideoPlayers") : nullptr;
            auto program=players ? players->findChild<QWidget *>("widgetEditTimelinePlayer") : nullptr;
            auto source=players ? players->findChild<QWidget *>("stackedEditSourceOfflinePlayers") : nullptr;
            if(!players || !program || !source) return;
            auto experiment=new EditViewers(w,players,program,source,settings);
            installed=experiment;
            QObject::connect(qApp,&QCoreApplication::aboutToQuit,experiment,[experiment] { delete experiment; });
            return;
        }
    },Qt::QueuedConnection) ? 1 : 0;
}
