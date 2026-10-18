// SPDX-License-Identifier: MIT
#include "controller.h"
#include "startuplog.h"
#include <QApplication>
#include <QThread>
#include <QToolButton>
#include <QTimer>
#include <atomic>
namespace {
// One-shot, read-only confirmation when Resolve first displays our shared UI.
class StartupObserver : public QObject {
public:
    StartupObserver() { qApp->installEventFilter(this); }
    bool eventFilter(QObject *object,QEvent *event) override {
        if (event->type()==QEvent::Show && object->objectName()==QStringLiteral("resolvePaneUndockButton")) {
            unboundStartupLog("Shared pane docking control shown");
            qApp->removeEventFilter(this); deleteLater();
        }
        return false;
    }
};
}
extern "C" __attribute__((visibility("default"))) unsigned long UnboundStart(void *argument)
{
    if(argument || !qobject_cast<QApplication *>(QCoreApplication::instance())
        || QThread::currentThread()!=qApp->thread() || strcmp(qVersion(),"5.15.2")) return 0;
    static std::atomic<bool> observed{false};
    if(!observed.exchange(true)) {
        auto observer=new StartupObserver; observer->setParent(qApp);
        unboundStartupLog("Qt 5.15.2 GUI thread verified; starting existing controller");
    }
    return Unbound::startController() ? 1 : 0;
}
