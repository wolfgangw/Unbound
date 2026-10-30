// SPDX-License-Identifier: MIT
#include "layoutreadiness.h"
#include "layoutadapter.h"
#ifdef UNBOUND_LAYOUT_TRANSITIONS
#include <QEventLoop>
void testLayoutReadiness() {
    using namespace Unbound;
    using LayoutTest::require;
    auto wait=[] {QEventLoop loop;QTimer::singleShot(120,&loop,&QEventLoop::quit);loop.exec();};
    QWidget root;root.show();LayoutTest::settle();
    int reveals=0,invalidations=0;
    LayoutPendingWork delayed;delayed.set(&root);
    LayoutReadiness::begin(&root,"fixture");
    auto release=LayoutReadiness::observe([&] {++reveals;},[&] {++invalidations;});
    LayoutReadiness::pending(0,0,0,0);wait();
    require(!reveals,"Readiness ignored earlier pending work");
    delayed.clear();wait();wait();
    require(reveals==1,"Final raster update did not yield one ready candidate");
    root.resize(root.width()+1,root.height());wait();wait();
    require(invalidations>=1 && reveals==2,"Geometry change failed to invalidate readiness");
    release();require(!LayoutReadiness::active(),"Completed presentation retained readiness observation");
    LayoutReadiness::begin(&root,"replacement");
    release();require(LayoutReadiness::active(),"Old presentation cleanup cancelled its successor");
    LayoutReadiness::active()->finish();
    require(!LayoutReadiness::active(),"Cancelled readiness remained active");
    std::puts("PASS: readiness waits for queued work and raster delivery, invalidates geometry and releases observers");
}
#else
void testLayoutReadiness() {}
#endif
