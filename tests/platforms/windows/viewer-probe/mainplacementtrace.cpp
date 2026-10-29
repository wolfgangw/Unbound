// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QWidget>
#include <QWindow>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <qt_windows.h>
#include <commctrl.h>
static QJsonArray rect(RECT r) { return {r.left,r.top,r.right-r.left,r.bottom-r.top}; }
static QJsonArray rect(QRect r) { return {r.x(),r.y(),r.width(),r.height()}; }
class Trace final : public QObject {
public:
 QPointer<QWidget> window; HWND handle=nullptr; QJsonArray rows; QElapsedTimer clock;
 Trace(QWidget *w):QObject(qApp),window(w),handle(reinterpret_cast<HWND>(w->winId())) {
  clock.start(); w->installEventFilter(this);
  SetWindowSubclass(handle,dispatch,reinterpret_cast<UINT_PTR>(this),reinterpret_cast<DWORD_PTR>(this));
  record("start",0,0); QTimer::singleShot(30000,this,[this]{delete this;});
 }
 ~Trace() override {
  if(window)window->removeEventFilter(this);
  if(IsWindow(handle))RemoveWindowSubclass(handle,dispatch,reinterpret_cast<UINT_PTR>(this));
  QFile f(QDir::tempPath()+"/Unbound-main-placement-trace.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(rows).toJson());
 }
 void record(const char *phase,UINT message,LPARAM parameter) {
  if(!window || rows.size()>5000)return;
  RECT outer{},client{};GetWindowRect(handle,&outer);GetClientRect(handle,&client);
  WINDOWPLACEMENT p{};p.length=sizeof(p);GetWindowPlacement(handle,&p);
  auto wh=window->windowHandle();auto m=wh->frameMargins();
  QJsonObject row{{"us",double(clock.nsecsElapsed()/1000)},{"phase",phase},{"message",int(message)},
   {"outer",rect(outer)},{"client",rect(client)},{"normal",rect(p.rcNormalPosition)},{"show",int(p.showCmd)},
   {"qt",rect(window->geometry())},{"qtFrame",rect(window->frameGeometry())},{"qtNormal",rect(window->normalGeometry())},
   {"qtState",int(window->windowState())},{"margins",QJsonArray{m.left(),m.top(),m.right(),m.bottom()}},
   {"minimum",QJsonArray{window->minimumWidth(),window->minimumHeight()}}};
  if(parameter && (message==WM_WINDOWPOSCHANGING || message==WM_WINDOWPOSCHANGED)) {
   auto v=reinterpret_cast<WINDOWPOS *>(parameter);row.insert("proposed",QJsonArray{v->x,v->y,v->cx,v->cy});row.insert("flags",int(v->flags));
  }
  rows.append(row);
 }
 bool eventFilter(QObject *,QEvent *e) override {
  if(e->type()==QEvent::Resize || e->type()==QEvent::Move || e->type()==QEvent::WindowStateChange)record("qt",int(e->type()),0);
  return false;
 }
 static LRESULT CALLBACK dispatch(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
  auto self=reinterpret_cast<Trace *>(data);
  const bool selected=msg==WM_WINDOWPOSCHANGING || msg==WM_WINDOWPOSCHANGED || msg==WM_SIZE || msg==WM_NCCALCSIZE || msg==WM_SYSCOMMAND;
  if(selected)self->record("before",msg,lp);
  auto result=DefSubclassProc(h,msg,wp,lp);
  if(selected)self->record("after",msg,lp);
  if(msg==WM_NCDESTROY){RemoveWindowSubclass(h,dispatch,id);self->handle=nullptr;}
  return result;
 }
};
static QPointer<Trace> trace;
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *) {
 return QMetaObject::invokeMethod(qApp,[]{
  if(trace){delete trace;return;}
  for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="UiMainWindow"){trace=new Trace(w);break;}
 },Qt::QueuedConnection)?1:0;
}
