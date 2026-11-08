// SPDX-License-Identifier: MIT
#pragma once
#include "commandapi.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QVariant>
#include <functional>
namespace Unbound {
// Event-driven settlement, shared by menu and script operations. No replay queue.
// Persistent native layout churn must not permanently lock out new commands.
class LayoutCommandGate final : public QObject {
    QTimer quiet,deadline;
    mutable QElapsedTimer pendingDeadline;
    bool running=false,settling=false,changed=false;
    QPointer<QWidget> main;
    void finish() {
        if(running)return;
        settling=false;quiet.stop();deadline.stop();
        commandOperationActive()=false;
        if(main)main->setProperty("unboundLayoutCommandsBusy",false);
        if(qApp)qApp->removeEventFilter(this);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(!settling)return false;
        const auto type=event->type();
        if(type!=QEvent::LayoutRequest && type!=QEvent::Resize && type!=QEvent::Move
            && type!=QEvent::Show && type!=QEvent::Hide && type!=QEvent::ParentChange)return false;
        if(auto widget=qobject_cast<QWidget *>(object)) {
            auto window=widget->window();
            if(window==main || window->property("unboundFloatingWindow").toBool()) {changed=true;quiet.start();}
        }
        return false;
    }
public:
    std::function<bool()> pending;
    LayoutCommandGate() {
        quiet.setSingleShot(true);quiet.setInterval(150);
        deadline.setSingleShot(true);deadline.setInterval(3000);
        connect(&quiet,&QTimer::timeout,this,[this] {finish();});
        connect(&deadline,&QTimer::timeout,this,[this] {finish();});
    }
    ~LayoutCommandGate() override {if(settling)commandOperationActive()=false;if(qApp)qApp->removeEventFilter(this);}
    bool busy() const {
        if(running || settling)return true;
        if(!(pending && pending())) {pendingDeadline.invalidate();return false;}
        if(!pendingDeadline.isValid())pendingDeadline.start();
        // Deferred corrections already support supersession. After a bounded
        // wait, let a new request replace persistent native layout churn.
        return pendingDeadline.elapsed()<3000;
    }
    QString run(QWidget *window,const std::function<QString()> &operation) {
        if(busy())return QStringLiteral("A layout change is still in progress. Please try again.");
        main=window;running=true;settling=true;changed=false;pendingDeadline.start();
        ++commandContext(); // A menu operation supersedes an older queued script.
        commandOperationActive()=true;
        if(main)main->setProperty("unboundLayoutCommandsBusy",true);
        qApp->installEventFilter(this);deadline.start();
        const QPointer<LayoutCommandGate> alive=this;
        const auto result=operation();
        if(!alive)return result;
        running=false;
        if(!changed && !(pending && pending()))finish();else quiet.start();
        return result;
    }
};
}
