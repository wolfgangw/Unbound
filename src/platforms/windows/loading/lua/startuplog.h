// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <string>
inline void startupLog(const char *message,DWORD error=0)
{
    wchar_t buffer[32768];
    std::wstring target;
    auto size=GetEnvironmentVariableW(L"UNBOUND_LOG_FILE",buffer,32768);
    if(!size) {
        size=GetEnvironmentVariableW(L"LOCALAPPDATA",buffer,32768);
        if(!size || size>=32768) return;
        const auto directory=std::wstring(buffer,size)+L"\\Unbound";
        CreateDirectoryW(directory.c_str(),nullptr);
        target=directory+L"\\lua-bootstrap.log";
    } else if(size>=32768) return;
    else target.assign(buffer,size);
    auto file=CreateFileW(target.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return;
    const auto line=std::to_string(GetCurrentProcessId())+": "+message+" ("+std::to_string(error)+")\r\n";
    DWORD written; WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr); CloseHandle(file);
}
