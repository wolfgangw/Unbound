// SPDX-License-Identifier: MIT
#include "installation.h"
#include "processmonitor.h"
#include "../../../installer/presentation.h"
#include <commctrl.h>
namespace {
Installation::Paths paths;
Installation::Payload payload;
HWND statusLabel,installButton,uninstallButton;
HFONT font,titleFont,statusFont;
constexpr UINT resolveStatusChanged=WM_APP+1;
Installation::ProcessMonitor processMonitor;
bool lastResolveRunning=false;
#ifdef UNBOUND_INSTALLER_TEST
std::atomic<bool> fixtureResolveRunning{false};
#endif
bool resolveRunning() {
#ifdef UNBOUND_INSTALLER_TEST
    return fixtureResolveRunning;
#else
    return Installation::resolveRunning();
#endif
}
std::wstring wide(const std::string &s) {
    const auto size=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),nullptr,0);
    std::wstring result(size,L'\0');
    MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),result.data(),size);
    return result;
}
void text(HWND control,const std::string &value) { SetWindowTextW(control,wide(value).c_str()); }
std::string resource(int id) {
    auto module=GetModuleHandleW(nullptr); auto info=FindResourceW(module,MAKEINTRESOURCEW(id),RT_RCDATA);
    if(!info) throw std::runtime_error("Installer payload is missing.");
    auto data=LoadResource(module,info); auto bytes=LockResource(data);
    if(!bytes) throw std::runtime_error("Cannot read installer payload.");
    return {static_cast<const char *>(bytes),SizeofResource(module,info)};
}
void refresh(bool running) {
    const bool installed=Installation::installed(paths);
    const auto ui=InstallerUI::present({installed,running,Installation::matches(paths,payload)},
        InstallerUI::version(Installation::read(paths.runtime()/L"VERSION")),InstallerUI::version(payload.version));
    text(statusLabel,ui.status); text(installButton,ui.install); text(uninstallButton,ui.uninstall);
    EnableWindow(installButton,ui.installEnabled); EnableWindow(uninstallButton,ui.uninstallEnabled);
    lastResolveRunning=running;
}
void refresh() { refresh(resolveRunning()); }
void operation(HWND window,bool uninstall) {
    refresh();
    if(!IsWindowEnabled(uninstall ? uninstallButton : installButton)) return;
    const InstallerUI::Operation op{uninstall,Installation::installed(paths)};
    if(uninstall && MessageBoxW(window,wide(op.confirmationText()).c_str(),
        wide(op.confirmationTitle()).c_str(),MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return;
    Installation::apply(paths,uninstall ? nullptr : &payload);
    refresh();
    text(statusLabel,op.success());
}
HWND control(HWND parent,const wchar_t *kind,const wchar_t *text,DWORD style,int x,int y,int w,int h,int id=0) {
    auto child=CreateWindowExW(0,kind,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE); return child;
}
HWND control(HWND parent,const wchar_t *kind,const char *text,DWORD style,InstallerUI::Rect r,int id=0) {
    return control(parent,kind,wide(text).c_str(),style,r.x,r.y,r.width,r.height,id);
}
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    try {
        switch(message) {
        case WM_CREATE: {
            font=CreateFontW(-InstallerUI::fonts::body,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
            titleFont=CreateFontW(-InstallerUI::fonts::title,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
            statusFont=CreateFontW(-InstallerUI::fonts::status,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
            auto icon=control(window,L"STATIC","",SS_ICON,InstallerUI::rects::icon);
            SendMessageW(icon,STM_SETICON,reinterpret_cast<WPARAM>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,InstallerUI::rects::icon.width,InstallerUI::rects::icon.height,LR_SHARED)),0);
            auto title=control(window,L"STATIC",InstallerUI::text::app,0,InstallerUI::rects::title); SendMessageW(title,WM_SETFONT,reinterpret_cast<WPARAM>(titleFont),TRUE);
            control(window,L"STATIC",InstallerUI::text::subtitle,0,InstallerUI::rects::subtitle);
            uninstallButton=control(window,L"BUTTON","",BS_PUSHBUTTON|BS_MULTILINE|WS_TABSTOP,InstallerUI::rects::uninstall,102);
            installButton=control(window,L"BUTTON","",BS_DEFPUSHBUTTON|BS_MULTILINE|WS_TABSTOP,InstallerUI::rects::install,101);
            control(window,L"STATIC","",SS_ETCHEDHORZ,InstallerUI::rects::separator);
            statusLabel=control(window,L"STATIC","",0,InstallerUI::rects::status);
            SendMessageW(statusLabel,WM_SETFONT,reinterpret_cast<WPARAM>(statusFont),TRUE);
            refresh();
            processMonitor.start(window,resolveStatusChanged,lastResolveRunning
#ifdef UNBOUND_INSTALLER_TEST
                ,resolveRunning
#endif
            );
            return 0;
        }
        case resolveStatusChanged:
            // Re-read installed files/update controls only on a state change.
            // Leave operation feedback intact while the process state is stable.
            if((w!=0)!=lastResolveRunning) refresh(w!=0);
            return 0;
        case WM_ACTIVATE: if(LOWORD(w)!=WA_INACTIVE) refresh(); break;
        case WM_COMMAND: if(LOWORD(w)==101 || LOWORD(w)==IDOK) { operation(window,false); return 0; }
            if(LOWORD(w)==102) { operation(window,true); return 0; } break;
        case WM_DESTROY: processMonitor.stop(); DeleteObject(font); DeleteObject(titleFont); DeleteObject(statusFont); PostQuitMessage(0); return 0;
        }
    } catch(const std::exception &e) { MessageBoxW(window,wide(e.what()).c_str(),L"Unbound",MB_OK|MB_ICONERROR); }
    return DefWindowProcW(window,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    // A single installer per desktop prevents overlapping file transactions.
    auto mutex=CreateMutexW(nullptr,FALSE,L"Local\\UnboundInstaller");
    if(!mutex || GetLastError()==ERROR_ALREADY_EXISTS) { MessageBoxW(nullptr,L"The Unbound installer is already open.",L"Unbound",MB_OK); return 1; }
    try {
        paths=Installation::userPaths(); payload={resource(201),resource(202),resource(203),resource(204)};
        INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&common);
        WNDCLASSW type{}; type.hInstance=instance; type.lpfnWndProc=windowProc; type.lpszClassName=L"UnboundInstaller";
        type.hCursor=LoadCursorW(nullptr,IDC_ARROW); type.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); type.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);
        RegisterClassW(&type);
        RECT rect{0,0,InstallerUI::width,InstallerUI::height}; AdjustWindowRectEx(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE,0);
        const auto title=wide(resource(206));
        auto window=CreateWindowExW(WS_EX_CONTROLPARENT,type.lpszClassName,title.c_str(),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
            CW_USEDEFAULT,CW_USEDEFAULT,rect.right-rect.left,rect.bottom-rect.top,nullptr,nullptr,instance,nullptr);
        if(!window) throw std::runtime_error("Cannot create installer window.");
        ShowWindow(window,show); MSG event;
        while(GetMessageW(&event,nullptr,0,0)>0) if(!IsDialogMessageW(window,&event)) { TranslateMessage(&event); DispatchMessageW(&event); }
    } catch(const std::exception &e) { MessageBoxW(nullptr,wide(e.what()).c_str(),L"Unbound",MB_OK|MB_ICONERROR); CloseHandle(mutex); return 1; }
    CloseHandle(mutex); return 0;
}
