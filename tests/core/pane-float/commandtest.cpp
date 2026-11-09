// SPDX-License-Identifier: MIT
#include "commandapi.h"
#include "scriptcommandfallback.h"
#include "layoutcommandgate.h"
#include "layoutadapter.h"
#include <QEventLoop>
#include <QThread>
#include <thread>
#include <vector>
void testLayoutCommands() {
    using namespace Unbound;
    using LayoutTest::require;
    // Native hierarchy can be rebuilt without changing labels or shortcut assignments.
    {
        QMenu workspace;workspace.setObjectName("workspace");
        auto scripts=workspace.addMenu("Scripts");
        auto managed=scripts->addMenu("Unbound");
        for(const auto &command:scriptCommands)managed->addAction(command.label);
        int delivered=0;CommandRequest parsed;
        ScriptCommandFallback fallback(&workspace,[&](const ScriptCommand &command) {
            require(parseCommand(1,command.command,command.arguments,parsed)==CommandAccepted,"Catalogue command invalid");
            ++delivered;
        });
        struct QueuedRefreshCounter : QObject {
            int calls=0;
            bool eventFilter(QObject *,QEvent *event) override {
                if(event->type()==QEvent::MetaCall)++calls;
                return false;
            }
        } counter;
        fallback.installEventFilter(&counter);
        QMenu unrelatedMenu;
        auto unrelatedSection=workspace.addMenu("Unrelated section");
        auto unrelatedNested=unrelatedSection->addMenu("Other commands");
        QCoreApplication::processEvents();QCoreApplication::processEvents();counter.calls=0;
        for(auto menu:{&unrelatedMenu,unrelatedNested}) {
            menu->addAction("Unrelated command");
            QEvent show(QEvent::Show);QCoreApplication::sendEvent(menu,&show);
        }
        QCoreApplication::processEvents();QCoreApplication::processEvents();
        require(counter.calls==0,"Unrelated menus scheduled fallback discovery");
        for(auto action:managed->actions())action->trigger();
        require(delivered==8,"Native command catalogue dispatch incomplete");
        auto unrelated=scripts->addAction("Dock all");unrelated->trigger();
        require(delivered==8,"Unrelated action dispatched");
        auto old=managed->actions().first();managed->removeAction(old);
        old->trigger();require(delivered==8,"Removed action retained dispatch");
        delete managed;
        managed=scripts->addMenu("Unbound");
        // Discovery must retain incomplete/lazily populated managed menus.
        QCoreApplication::processEvents();QCoreApplication::processEvents();
        for(const auto &command:scriptCommands)managed->addAction(command.label);
        QCoreApplication::processEvents();QCoreApplication::processEvents();
        for(auto action:managed->actions())action->trigger();
        require(delivered==16,"Rebuilt menu duplicated or lost dispatch");
        scripts->removeAction(managed->menuAction());
        QCoreApplication::processEvents();QCoreApplication::processEvents();counter.calls=0;
        managed->addAction("Detached command");
        managed->actions().first()->trigger();
        QCoreApplication::processEvents();QCoreApplication::processEvents();
        require(delivered==16 && counter.calls==0,"Detached submenu retained observation or dispatch");
    }
    CommandRequest request;
    require(parseCommand(1,"layout.activate","{\"slot\":5}",request)==CommandAccepted && request.slot==5,"Slot admission");
    for(const auto &json:{"{}","{\"slot\":0}","{\"slot\":6}","{\"slot\":1.5}","{\"slot\":\"1\"}","{\"slot\":1,\"x\":2}","[]","bad"})
        require(parseCommand(1,"layout.activate",json,request)==CommandArguments,"Invalid arguments accepted");
    require(parseCommand(2,"layout.save","{}",request)==CommandVersion,"Unknown API version accepted");
    require(parseCommand(1,"other","{}",request)==CommandUnknown,"Unknown command accepted");
    require(parseCommand(1,"layout.save","{\"slot\":1}",request)==CommandArguments,"Unexpected arguments accepted");
    require(UnboundExecuteCommand(99,"layout.save","{}")==CommandVersion,"Export version validation");
    require(UnboundExecuteCommand(1,nullptr,"{}")==CommandUnknown,"Export null command validation");
    require(UnboundExecuteCommand(1,"layout.save",nullptr)==CommandArguments,"Export null arguments validation");
    // No controller is needed to test bounded concurrent admission and stale requests.
    std::atomic<int> accepted{0};std::vector<std::thread> workers;
    for(int i=0;i<32;++i)workers.emplace_back([&] {
        if(UnboundExecuteCommand(1,"layout.save","{}")==CommandAccepted)++accepted;
    });
    for(auto &worker:workers)worker.join();
    require(accepted==1,"Concurrent requests queued a backlog");
    ++commandContext();QCoreApplication::processEvents();
    require(!commandQueued(),"Stale request retained admission reservation");
    QWidget main;
    LayoutCommandGate gate;bool pending=false;int calls=0;
    gate.pending=[&] {return pending;};
    auto action=[&]() -> QString {++calls;return {};};
    pending=true;require(!gate.run(&main,action).isEmpty() && calls==0,"Pending work allowed command");
    pending=false;
    require(gate.run(&main,[&]() -> QString {
        ++calls;
        require(!gate.run(&main,action).isEmpty(),"Reentrant operation accepted");
        QEvent event(QEvent::LayoutRequest);QCoreApplication::sendEvent(&main,&event);
        return {};
    }).isEmpty(),"First operation rejected");
    for(int i=0;i<100;++i)require(!gate.run(&main,action).isEmpty(),"Burst overlapped restoration");
    require(UnboundExecuteCommand(1,"layout.save","{}")==CommandBusy && !commandQueued(),"Busy command queued for later replay");
    require(calls==1,"Rejected commands executed");
    QEventLoop loop;QTimer::singleShot(200,&loop,&QEventLoop::quit);loop.exec();
    require(!gate.busy() && calls==1,"Gate replayed commands or failed to settle");
    require(gate.run(&main,action).isEmpty() && calls==2,"Settled gate failed to reopen");
    pending=true;
    require(gate.busy(),"Pending correction did not hold admission");
    QTimer::singleShot(3100,&loop,&QEventLoop::quit);loop.exec();
    require(!gate.busy(),"Persistent native correction permanently locked commands");
    require(gate.run(&main,action).isEmpty() && calls==3,"Bounded correction could not be superseded");
    std::puts("PASS: command ABI validation, bounded concurrent admission, stale cancellation, reentrancy, burst rejection and settlement without replay");
}
