// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <string>
inline void startupLog(const char *message,DWORD error=0)
{
    wchar_t path[32768];
    auto size=GetEnvironmentVariableW(L"UNBOUND_LOG_FILE",path,32768);
    if(!size) {
        size=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
        if(!size || size>=32768) return;
        const auto directory=std::wstring(path)+L"\\Unbound";
        CreateDirectoryW(directory.c_str(),nullptr);
        const auto file=directory+L"\\lua-bootstrap.log";
        wcscpy_s(path,file.c_str());
    } else if(size>=32768) return;
    auto file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return;
    const auto line=std::to_string(GetCurrentProcessId())+": "+message+" ("+std::to_string(error)+")\r\n";
    DWORD written; WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr); CloseHandle(file);
}
