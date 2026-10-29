// SPDX-License-Identifier: MIT
// External, bounded diagnostic sampler. Never part of the shipped helper.
#include <windows.h>
#include <dbghelp.h>
#include <fstream>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
int wmain(int argc,wchar_t **argv) {
    if(argc!=3)return 2;
    DWORD pid=wcstoul(argv[1],nullptr,10);
    struct Target {DWORD pid,tid=0;} target{pid};
    EnumWindows([](HWND w,LPARAM data)->BOOL {
        auto &t=*reinterpret_cast<Target *>(data);DWORD pid=0;DWORD tid=GetWindowThreadProcessId(w,&pid);
        wchar_t title[512]{};GetWindowTextW(w,title,512);
        if(pid==t.pid && wcsstr(title,L"DaVinci Resolve")){t.tid=tid;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&target));
    if(!target.tid)return 3;
    HANDLE process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
    HANDLE thread=OpenThread(THREAD_GET_CONTEXT|THREAD_SUSPEND_RESUME|THREAD_QUERY_INFORMATION,FALSE,target.tid);
    if(!process || !thread)return 4;
    SymSetOptions(SYMOPT_DEFERRED_LOADS|SYMOPT_UNDNAME|SYMOPT_NO_PROMPTS|SYMOPT_FAIL_CRITICAL_ERRORS);
    if(!SymInitialize(process,nullptr,TRUE))return 5;
    struct Sample {ULONGLONG ms;std::vector<DWORD64> stack;};std::vector<Sample> samples;
    const auto start=GetTickCount64();
    while(GetTickCount64()-start<30000) {
        Sample sample{GetTickCount64()-start,{}};sample.stack.reserve(32);
        if(SuspendThread(thread)!=DWORD(-1)) {
            CONTEXT context{};context.ContextFlags=CONTEXT_FULL;
            if(GetThreadContext(thread,&context)) {
                STACKFRAME64 frame{};frame.AddrPC.Offset=context.Rip;frame.AddrStack.Offset=context.Rsp;frame.AddrFrame.Offset=context.Rbp;
                frame.AddrPC.Mode=frame.AddrStack.Mode=frame.AddrFrame.Mode=AddrModeFlat;
                for(int i=0;i<32 && frame.AddrPC.Offset;++i) {
                    sample.stack.push_back(frame.AddrPC.Offset);
                    if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,thread,&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
                }
            }
            ResumeThread(thread);
        }
        samples.push_back(std::move(sample));Sleep(25);
    }
    // Resolve names only after sampling, never while Resolve is suspended.
    std::map<DWORD64,std::string> symbols;
    auto name=[&](DWORD64 pc) {
        if(symbols.count(pc))return symbols[pc];
        IMAGEHLP_MODULE64 module{};module.SizeOfStruct=sizeof(module);
        std::string value=SymGetModuleInfo64(process,pc,&module)?module.ModuleName:"unknown";
        char storage[sizeof(SYMBOL_INFO)+1024]{};auto symbol=reinterpret_cast<SYMBOL_INFO *>(storage);
        symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;DWORD64 displacement=0;
        if(SymFromAddr(process,pc,&displacement,symbol))
            value+="!"+std::string(symbol->Name)+"+"+std::to_string(displacement);
        else value+="+"+std::to_string(pc-module.BaseOfImage);
        return symbols[pc]=value;
    };
    std::ofstream output(argv[2]);
    for(const auto &sample:samples) {
        output<<sample.ms;
        for(auto pc:sample.stack)output<<" | "<<name(pc);
        output<<'\n';
    }
    SymCleanup(process);CloseHandle(thread);CloseHandle(process);return 0;
}
