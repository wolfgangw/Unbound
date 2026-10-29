// SPDX-License-Identifier: MIT
#include "colorscopeslayoutrefresh.h"
#include <qt_windows.h>
static QPointer<Unbound::ColorScopesLayoutRefresh> active;
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
 return QMetaObject::invokeMethod(qApp,[]{
  if(active){delete active;return;}
  for(auto w:QApplication::allWidgets())if(w->objectName()=="UiWaveformWindow" && w->isVisible()
      && w->parentWidget() && w->parentWidget()->property("unboundFloatingWindow").toBool()) {
   active=new Unbound::ColorScopesLayoutRefresh(w,w->parentWidget(),qApp);
   QTimer::singleShot(300000,active,&QObject::deleteLater);break;
  }
 },Qt::QueuedConnection)?1:0;
}
