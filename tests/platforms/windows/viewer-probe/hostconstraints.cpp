// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QWidget>
#include <QWindow>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <qt_windows.h>
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
 return QMetaObject::invokeMethod(qApp,[]{
 QJsonArray rows;
 for(auto w:QApplication::topLevelWidgets())if(w->objectName().startsWith("unboundFloating_")) {
 MINMAXINFO mm{}; SendMessage(reinterpret_cast<HWND>(w->winId()),WM_GETMINMAXINFO,0,reinterpret_cast<LPARAM>(&mm));
 auto wh=w->windowHandle();
 rows.append(QJsonObject{{"name",w->objectName()},{"size",QJsonArray{w->width(),w->height()}},{"widgetMin",QJsonArray{w->minimumWidth(),w->minimumHeight()}},{"windowMin",QJsonArray{wh->minimumWidth(),wh->minimumHeight()}},{"nativeMin",QJsonArray{int(mm.ptMinTrackSize.x),int(mm.ptMinTrackSize.y)}}});
 }
 QFile f(QDir::tempPath()+"/Unbound-host-constraints.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(rows).toJson());
 },Qt::QueuedConnection)?1:0;
}
