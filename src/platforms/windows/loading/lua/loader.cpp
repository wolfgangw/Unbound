// SPDX-License-Identifier: MIT
// Qt-free entry point invoked inside Resolve by its Lua startup library.
#include <windows.h>
#include <atomic>
#include <cstring>
#include <string>
#include "startuplog.h"
static std::atomic<int> state{0};
static std::atomic<int> resolveEdition{-1};
extern "C" __declspec(dllexport) int UnboundLuaStatus() { return state.load(); }
static int startForEdition(int edition)
{
    if(state.load()!=0) return state.load();
    const auto qt=GetModuleHandleW(L"Qt5Core.dll");
    auto version=qt ? reinterpret_cast<const char *(*)()>(GetProcAddress(qt,"qVersion")) : nullptr;
    if(!version) { startupLog("No existing Qt application"); return -1; }
    if(strcmp(version(),"5.15.2")) { startupLog("Unsupported Qt version"); return -2; }
    auto app=reinterpret_cast<void **>(GetProcAddress(qt,"?self@QCoreApplication@@0PEAV1@EA"));
    if(!app || !*app) { startupLog("No existing Qt application"); return -1; }
    int expected=0;
    if(!state.compare_exchange_strong(expected,1)) return expected;
    resolveEdition=edition;
    HMODULE own=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&startForEdition),&own)) { state=-3; return -3; }
    // Keep both images resident: queued Qt work outlives the Lua invocation.
    wchar_t path[32768];
    const DWORD length=GetModuleFileNameW(own,path,32768);
    if(!length || length>=32768) { FreeLibrary(own); state=-3; return -3; }
    const std::wstring ownPath(path,length);
    const auto slash=ownPath.find_last_of(L"\\/");
    if(slash==std::wstring::npos) { FreeLibrary(own); state=-3; return -3; }
    auto helperPath=ownPath.substr(0,slash)+L"\\Unbound.dll";
    auto helper=LoadLibraryExW(helperPath.c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!helper) { FreeLibrary(own); state=-4; startupLog("Cannot load adjacent Unbound.dll",GetLastError()); return -4; }
    auto configure=reinterpret_cast<void (*)(int)>(GetProcAddress(helper,"UnboundSetResolveEdition"));
    if(configure) configure(resolveEdition.load());
    startupLog(resolveEdition==0 ? "Shortcut route: Free native actions" : resolveEdition==1 ? "Shortcut route: Studio scripts" : "Shortcut route: unknown edition; scripts retained");
    auto attach=reinterpret_cast<int (*)(void (*)(int))>(GetProcAddress(helper,"UnboundLuaAttach"));
    if(!attach || !attach([](int result) { state=result; })) {
        FreeLibrary(helper); FreeLibrary(own);
        state=-5; startupLog("Helper rejected queued attachment"); return -5;
    }
    startupLog("Lua request accepted; attachment queued");
    return state.load();
}
extern "C" __declspec(dllexport) int UnboundLuaStart() { return startForEdition(-1); }

extern "C" __declspec(dllexport) int UnboundLuaStartForEdition(int edition)
{
    return startForEdition(edition);
}
