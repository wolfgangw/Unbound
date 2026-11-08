// SPDX-License-Identifier: MIT
#pragma once
#include <QPointer>
#include <QWidget>
#include <QList>
namespace Unbound {
// Retain queued work across readiness generations, without idle polling.
class LayoutPendingWork {
public:
    QPointer<QWidget> scope;
    bool pending=false;
    static QList<LayoutPendingWork *> &registry() {static QList<LayoutPendingWork *> value;return value;}
    static void (*&notify())() {static void (*value)()=nullptr;return value;}
    LayoutPendingWork() {registry().append(this);}
    LayoutPendingWork(const LayoutPendingWork &)=delete;
    LayoutPendingWork &operator=(const LayoutPendingWork &)=delete;
    ~LayoutPendingWork() {registry().removeOne(this);if(pending && notify()) notify()();}
    void set(QWidget *widget,bool value=true) {
        if(scope==widget && pending==value)return;
        scope=widget;pending=value;if(notify())notify()();
    }
    void clear() {if(pending) {pending=false;if(notify())notify()();}}
};
}
