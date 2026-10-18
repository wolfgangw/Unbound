// SPDX-License-Identifier: MIT
// Qt-free entry point invoked inside Resolve by its Lua startup library.
#include <windows.h>
#include <atomic>
#include <cstring>
#include <string>
#include "startuplog.h"
static std::atomic<int> state{0};
extern "C" __declspec(dllexport) int UnboundLuaStatus() { return state.load(); }
extern "C" __declspec(dllexport) int UnboundLuaStart()
{
    if(state.load()!=0) return state.load();
    const auto qt=GetModuleHandleW(L"Qt5Core.dll");
    auto version=qt ? reinterpret_cast<const char *(*)()>(GetProcAddress(qt,"qVersion")) : nullptr;
    auto app=qt ? reinterpret_cast<void **>(GetProcAddress(qt,"?self@QCoreApplication@@0PEAV1@EA")) : nullptr;
    if(!version || !app || !*app) { startupLog("No existing Qt application"); return -1; }
    if(strcmp(version(),"5.15.2")) { startupLog("Unsupported Qt version"); return -2; }
    int expected=0;
    if(!state.compare_exchange_strong(expected,1)) return expected;
    HMODULE own=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<LPCWSTR>(&UnboundLuaStart),&own)) { state=-3; return -3; }
    // Keep both images resident: queued Qt work outlives the Lua invocation.
    wchar_t path[32768];
    const DWORD length=GetModuleFileNameW(own,path,32768);
    if(!length || length>=32768) { state=-3; return -3; }
    auto helperPath=std::wstring(path).substr(0,std::wstring(path).find_last_of(L"\\/"))+L"\\Unbound.dll";
    auto helper=LoadLibraryExW(helperPath.c_str(),nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!helper) { state=-4; startupLog("Cannot load adjacent Unbound.dll",GetLastError()); return -4; }
    auto attach=reinterpret_cast<int (*)(void (*)(int))>(GetProcAddress(helper,"UnboundLuaAttach"));
    if(!attach || !attach([](int result) { state=result; })) {
        state=-5; startupLog("Helper rejected queued attachment"); return -5;
    }
    startupLog("Lua request accepted; attachment queued");
    return state.load();
}
