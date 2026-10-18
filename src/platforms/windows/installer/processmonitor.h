// SPDX-License-Identifier: MIT
#pragma once
#include "installation.h"
#include <thread>
#include <atomic>
namespace Installation {
// Once Resolve is found, poll just its synchronization handle. No process-list
// snapshots while that instance remains alive, no module/window enumeration.
class ResolvePresence {
    HANDLE process=nullptr;
    std::wstring executable;
public:
    explicit ResolvePresence(std::wstring name=L"Resolve.exe") : executable(std::move(name)) {}
#ifdef UNBOUND_INSTALLER_TEST
    unsigned snapshots=0;
#endif
    ~ResolvePresence() { if(process) CloseHandle(process); }
    bool operator()() {
        if(process) {
            if(WaitForSingleObject(process,0)==WAIT_TIMEOUT) return true;
            CloseHandle(process); process=nullptr;
        }
#ifdef UNBOUND_INSTALLER_TEST
        ++snapshots;
#endif
        auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
        if(snapshot==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot inspect process presence");
        PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry); bool found=false;
        if(Process32FirstW(snapshot,&entry)) do {
            if(!_wcsicmp(entry.szExeFile,executable.c_str())) {
                found=true; process=OpenProcess(SYNCHRONIZE,FALSE,entry.th32ProcessID); break;
            }
        } while(Process32NextW(snapshot,&entry));
        CloseHandle(snapshot); return found;
    }
};
class ProcessMonitor {
    HANDLE stopEvent=nullptr;
    std::thread worker;
public:
    std::atomic<bool> background{false};
    ~ProcessMonitor() { stop(); }
    void start(HWND window,UINT message,bool initial,std::function<bool()> probe={}) {
        stop(); background=false;
        stopEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!stopEvent) throw std::runtime_error("Cannot start DaVinci Resolve monitor");
        worker=std::thread([this,window,message,initial,probe] {
            const bool mode=SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_BEGIN)!=FALSE;
            background=mode || SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_IDLE)!=FALSE;
            // Never run discovery at normal priority if priority setup fails.
            if(!background) return;
            ResolvePresence presence;
            bool previous=initial;
            while(WaitForSingleObject(stopEvent,500)==WAIT_TIMEOUT) {
                try {
                    const bool running=probe ? probe() : presence();
                    if(running!=previous) {
                        PostMessageW(window,message,running,0); previous=running;
                    }
                } catch(...) {
                    // Fail closed, silently retry at the next normal interval.
                    if(!previous) { PostMessageW(window,message,TRUE,0); previous=true; }
                }
            }
            if(mode) SetThreadPriority(GetCurrentThread(),THREAD_MODE_BACKGROUND_END);
        });
    }
    void stop() {
        if(stopEvent) SetEvent(stopEvent);
        if(worker.joinable()) worker.join();
        if(stopEvent) { CloseHandle(stopEvent); stopEvent=nullptr; }
    }
};
}
