// SPDX-License-Identifier: MIT
#include "mainwindowplacement.h"
#include <QDir>
#include <qt_windows.h>
struct Experiment final : QObject {
 Unbound::PaneSettings settings{QDir::tempPath()+"/Unbound-main-placement-experiment-"+QString::number(GetCurrentProcessId())+".json"};
 Unbound::MainWindowPlacement placement{settings};
 explicit Experiment(QWidget *w):QObject(qApp){placement.attach(w);}
};
static QPointer<Experiment> active;
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
 return QMetaObject::invokeMethod(qApp,[]{
  if(active){delete active;return;}
  for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="UiMainWindow"){active=new Experiment(w);break;}
 },Qt::QueuedConnection)?1:0;
}
