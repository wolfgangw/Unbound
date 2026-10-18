// SPDX-License-Identifier: MIT
#include "controller.h"
#include "startuplog.h"
#include <QApplication>
#include <QThread>
#include <QTimer>
extern "C" __declspec(dllexport) int UnboundLuaAttach(void (*completed)(int))
{
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if(!app || !completed || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[app,completed] {
        if(app->findChild<QObject *>(QStringLiteral("UnboundPaneController"))) {
            startupLog("Existing controller reused"); completed(3); return;
        }
        if(!Unbound::startController()) { completed(-5); return; }
        // startController itself queues construction; confirm after that event.
        QTimer::singleShot(0,app,[app,completed] {
            const bool attached=app->findChild<QObject *>(QStringLiteral("UnboundPaneController"));
            startupLog(attached ? "Shared controller attached on Qt GUI thread" : "Controller attachment failed");
            completed(attached ? 2 : -5);
        });
    },Qt::QueuedConnection) ? 1 : 0;
}
