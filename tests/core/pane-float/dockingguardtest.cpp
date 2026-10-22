// SPDX-License-Identifier: MIT
#include "panedockingguard.h"
#include "layoutadapter.h"
#include <QEventLoop>
using namespace Unbound;
using LayoutTest::require;
static void settleDock() { QEventLoop loop; QTimer::singleShot(160,&loop,&QEventLoop::quit); loop.exec(); }
void testDockingGuard() {
    QWidget page; page.resize(600,400);
    QWidget parent(&page); parent.setGeometry(0,0,400,300);
    QWidget content(&parent); content.setGeometry(0,0,300,200);
    page.show(); LayoutTest::settle();
    PaneDockingGuard guard(&page);
    int completed=0; bool failed=false;
    auto result=[&](bool value) { ++completed; failed=value; };
    parent.resize(1,300); // Child retains its size, but its ancestor clips it.
    guard.watch(&content,&page,result); settleDock();
    require(completed==1 && failed,"Collapsed ancestor was not detected");
    guard.watch(&content,&page,result);
    QTimer::singleShot(30,&page,[&] { parent.resize(400,300); }); settleDock();
    require(completed==2 && !failed,"Transient allocation was mistaken for failed docking");
    content.hide(); parent.resize(1,300);
    guard.watch(&content,&page,result); settleDock();
    require(completed==3 && !failed,"Intentionally hidden native content was rejected");
    content.show(); page.hide();
    guard.watch(&content,&page,result); settleDock();
    require(completed==3,"Inactive page was checked");
    page.show(); settleDock();
    require(completed==4 && failed,"Deferred check did not run on page return");
    guard.watch(&content,&page,result); guard.cancel(); settleDock();
    require(completed==4,"Superseded docking attempt still completed");
    auto removed=new QWidget(&page); removed->show();
    guard.watch(removed,&page,result); delete removed; settleDock();
    require(completed==4,"Destroyed content produced a stale completion");
    std::puts("PASS: docking allocation, ancestor clipping, transient sizes, hidden content, inactive pages and cancellation");
}
