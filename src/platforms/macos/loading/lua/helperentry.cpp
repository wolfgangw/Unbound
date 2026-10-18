// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QThread>
extern "C" unsigned long UnboundStart(void *argument);
extern "C" __attribute__((visibility("default"))) int UnboundLuaAttach()
{
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || QThread::currentThread()!=app->thread()) return 0;
    // An IO-loaded controller may already own the UI in Studio. Do not create
    // another controller from this separately installed experimental helper.
    if (app->findChild<QObject *>(QStringLiteral("UnboundPaneController"))) return 2;
    return UnboundStart(nullptr)==1 ? 1 : 0;
}
