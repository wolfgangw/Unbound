// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <dlfcn.h>
#include <thread>
#include <cstdio>
#include <cstdlib>
static void require(bool condition,const char *message) { if(!condition) { fprintf(stderr,"FAIL: %s\n",message); exit(1); } }
int main(int argc,char **argv)
{
    require(argc>=2,"Expected loader path");
    void *library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL); require(library,dlerror());
    auto start=reinterpret_cast<int (*)()>(dlsym(library,"UnboundLuaStart"));
    auto status=reinterpret_cast<int (*)()>(dlsym(library,"UnboundLuaStatus"));
    require(start && status,"Missing Lua loader exports");
    require(start()==-1 && status()==0,"Accepted startup without an application or prevented retry");
    QApplication app(argc,argv);
    const bool existing=app.arguments().contains("--existing-controller");
    QObject sentinel(&app);
    if(existing) sentinel.setObjectName("UnboundPaneController");
    int result=0;
    std::thread worker([&] { result=start(); }); worker.join();
    require(result==1 && status()==1,"Worker request was not queued");
    require(start()==1,"Duplicate queued request changed state");
    dlclose(library); // Pending callback must survive the caller releasing its handle.
    QEventLoop loop;
    QTimer polling; polling.setInterval(10);
    QObject::connect(&polling,&QTimer::timeout,&loop,[&] { if(status()!=1) loop.quit(); });
    QTimer::singleShot(3000,&loop,&QEventLoop::quit); polling.start(); loop.exec();
    for(int i=0;i<4;++i) app.processEvents();
    require(status()==(existing ? 3 : 2),"Main-thread attachment failed");
    require(start()==status(),"Duplicate completed request changed state");
    require(app.findChildren<QObject *>("UnboundPaneController").size()==1,"Missing or duplicate controller");
    puts("PASS: Lua native loader validates Qt, queues worker entry to GUI thread, retains callbacks, starts once and respects existing controllers");
}
