// SPDX-License-Identifier: MIT
#include <windows.h>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryDir>
#include <QFile>
#include <thread>
#include <cstdio>
#include <cstdlib>
static void require(bool c,const char *m) { if(!c) { fprintf(stderr,"FAIL: %s\n",m); exit(1); } }
int main(int argc,char **argv) {
    require(argc>=2,"Expected loader path");
    auto module=LoadLibraryW(reinterpret_cast<LPCWSTR>(QString::fromLocal8Bit(argv[1]).utf16()));
    require(module,"Cannot load loader");
    auto start=reinterpret_cast<int (*)()>(GetProcAddress(module,"UnboundLuaStart"));
    auto status=reinterpret_cast<int (*)()>(GetProcAddress(module,"UnboundLuaStatus"));
    require(start && status,"Missing exports");
    require(start()==-1 && status()==0,"Accepted without Qt app or prevented retry");
    QApplication app(argc,argv);
    QObject sentinel(&app);
    const bool existing=app.arguments().contains("--existing");
    if(existing) sentinel.setObjectName("UnboundPaneController");
    int result=0; std::thread worker([&] {result=start();}); worker.join();
    require(result==1 && status()==1,"Worker entry not queued");
    require(start()==1,"Duplicate queued start changed state");
    FreeLibrary(module);
    QEventLoop loop; QTimer timer; timer.setInterval(10);
    QObject::connect(&timer,&QTimer::timeout,&loop,[&]{if(status()!=1) loop.quit();});
    timer.start(); QTimer::singleShot(3000,&loop,&QEventLoop::quit); loop.exec();
    require(status()==(existing ? 3 : 2),"GUI-thread attachment failed");
    require(start()==status(),"Duplicate completion changed state");
    require(app.findChildren<QObject *>("UnboundPaneController").size()==1,"Missing/duplicate controller");
    puts("PASS: Windows Lua loader validates host, queues worker entry, retains libraries and attaches once");
}
