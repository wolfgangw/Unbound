// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <stdexcept>
namespace Installation {
namespace fs=std::filesystem;
struct Paths { fs::path support, scripts; fs::path runtime() const { return support/L"Runtime"; }
    fs::path hook() const { return scripts/L"Unbound.scriptlib"; } };
inline bool resolveRunning() {
    auto snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot check whether DaVinci Resolve is running.");
    PROCESSENTRY32W entry{}; entry.dwSize=sizeof(entry); bool found=false;
    if(Process32FirstW(snapshot,&entry)) do { if(!_wcsicmp(entry.szExeFile,L"Resolve.exe")) { found=true; break; } }
        while(Process32NextW(snapshot,&entry));
    CloseHandle(snapshot); return found;
}
inline fs::path environment(const wchar_t *name) {
    wchar_t value[32768]; auto size=GetEnvironmentVariableW(name,value,32768);
    if(!size || size>=32768) throw std::runtime_error("Cannot locate the current user's application data.");
    return value;
}
inline Paths userPaths() { return {environment(L"LOCALAPPDATA")/L"Unbound",
    environment(L"APPDATA")/L"Blackmagic Design/DaVinci Resolve/Support/Fusion/Scripts"}; }
inline bool installed(const Paths &p) { return fs::exists(p.runtime()) || fs::exists(p.hook()); }
inline std::string read(const fs::path &path) { std::ifstream stream(path,std::ios::binary); return {std::istreambuf_iterator<char>(stream),{}}; }
inline void write(const fs::path &path,const std::string &bytes) {
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);
    stream.write(bytes.data(),bytes.size()); stream.close();
    if(!stream) throw std::runtime_error("Cannot write installation files.");
}
inline void rejectLinks(fs::path path) {
    for(;!path.empty() && path!=path.root_path();path=path.parent_path()) {
        const auto attr=GetFileAttributesW(path.c_str());
        if(attr!=INVALID_FILE_ATTRIBUTES && (attr&FILE_ATTRIBUTE_REPARSE_POINT))
            throw std::runtime_error("Installation path contains a link or junction; no files changed.");
    }
}
// The executable embeds these exact files; no downloads, shell or Qt dependency.
struct Payload { std::string loader,helper,hook,version; };
inline bool matches(const Paths &p,const Payload &payload) {
    return read(p.runtime()/L"VERSION")==payload.version
        && read(p.runtime()/L"Unbound.dll")==payload.helper
        && read(p.runtime()/L"UnboundLuaLoader.dll")==payload.loader
        && read(p.hook())==payload.hook;
}
// Used by both the real UI and isolated fixtures. Checks and failpoints are
// injectable only through C++ callers, never through production command lines.
inline void apply(const Paths &p,const Payload *payload,
    const std::function<bool()> &running=resolveRunning,
    const std::function<void(int)> &checkpoint=[](int){})
{
    if(running()) throw std::runtime_error("Close DaVinci Resolve normally before changing Unbound.");
    rejectLinks(p.support); rejectLinks(p.scripts); rejectLinks(p.runtime()); rejectLinks(p.hook());
    fs::create_directories(p.support);
    if(payload) fs::create_directories(p.scripts);
    const auto transaction=p.support/(L"transaction-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
    if(!fs::create_directory(transaction)) throw std::runtime_error("Another installation transaction exists.");
    std::vector<std::pair<fs::path,fs::path>> previous;
    std::vector<fs::path> published;
    try {
        if(payload) {
            if(payload->loader.empty() || payload->helper.empty() || payload->hook.empty() || payload->version.empty())
                throw std::runtime_error("Incomplete installer payload.");
            fs::create_directory(transaction/L"new-runtime");
            write(transaction/L"new-runtime/UnboundLuaLoader.dll",payload->loader);
            write(transaction/L"new-runtime/Unbound.dll",payload->helper);
            write(transaction/L"new-runtime/VERSION",payload->version);
            write(transaction/L"new-hook",payload->hook);
        }
        checkpoint(0);
        if(running()) throw std::runtime_error("DaVinci Resolve started during preparation; existing files were left in place.");
        auto save=[&](const fs::path &original) {
            if(!fs::exists(original)) return;
            rejectLinks(original);
            const auto backup=transaction/std::to_wstring(previous.size());
            fs::rename(original,backup); previous.emplace_back(original,backup);
        };
        save(p.hook()); save(p.runtime()); checkpoint(1);
        if(payload) {
            fs::rename(transaction/L"new-runtime",p.runtime()); published.push_back(p.runtime()); checkpoint(2);
            fs::rename(transaction/L"new-hook",p.hook()); published.push_back(p.hook()); checkpoint(3);
        }
    } catch(...) {
        const auto failure=std::current_exception(); bool recovered=true;
        for(auto it=published.rbegin();it!=published.rend();++it) { std::error_code e; fs::remove_all(*it,e); if(e) recovered=false; }
        for(auto it=previous.rbegin();it!=previous.rend();++it) { std::error_code e; fs::rename(it->second,it->first,e); if(e) recovered=false; }
        if(!recovered) throw std::runtime_error("Rollback could not finish. Retained recovery files in Unbound's transaction folder.");
        std::error_code e; fs::remove_all(transaction,e); std::rethrow_exception(failure);
    }
    std::error_code cleanup; fs::remove_all(transaction,cleanup);
    if(cleanup) throw std::runtime_error("Change completed, but old installation files remain in Unbound's transaction folder.");
}
}
