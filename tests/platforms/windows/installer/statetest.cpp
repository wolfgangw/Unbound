// SPDX-License-Identifier: MIT
// Exercise the actual installer controls/monitor without touching Resolve or the
// user's installation. Only process detection is replaced in this fixture.
#define UNBOUND_INSTALLER_TEST
#define wWinMain unusedInstallerEntry
#include "../../../../src/platforms/windows/installer/main.cpp"
#undef wWinMain
#include <iostream>
static void require(bool value,const char *message) { if(!value) throw std::runtime_error(message); }
static std::wstring caption(HWND window) {
    wchar_t buffer[512]; GetWindowTextW(window,buffer,512); return buffer;
}
static void presentationTests() {
    require(InstallerUI::version("0.3.0+internal.20261018.5")=="2026.10.18.005","Windows internal version formatting");
    require(InstallerUI::version("0.3.0\n","2026.10.18.005\n")=="2026.10.18.005","macOS internal version formatting");
    require(InstallerUI::version("0.3.0")=="0.3.0","Release version formatting");
    require(InstallerUI::version("2026.10.18.0005")=="2026.10.18.005","Legacy counter normalization");
    require(InstallerUI::version("")==InstallerUI::text::unknownVersion,"Unknown installed version");
    for(bool installed:{false,true}) for(bool running:{false,true}) for(bool same:{false,true}) {
        auto ui=InstallerUI::present({installed,running,same},"old","new");
        require(ui.install==(installed ? "Update\nnew" : "Install\nnew"),"Shared install label/version");
        require(ui.uninstall==(installed ? "Uninstall\nold" : "Uninstall"),"Shared removal label/version");
        require(ui.installEnabled==(!running && !same) && ui.uninstallEnabled==(!running && installed),"Shared action availability");
    }
    require(std::string(InstallerUI::Operation{false,false}.success())==InstallerUI::text::installedSuccess,"Installation feedback");
    require(std::string(InstallerUI::Operation{true,true}.success())==InstallerUI::text::removedSuccess,"Uninstall feedback");
}
static void waitForMonitor(bool change=true) {
    const auto deadline=GetTickCount64()+2000;
    while(GetTickCount64()<deadline) {
        MSG event;
        if(PeekMessageW(&event,nullptr,0,0,PM_REMOVE)) {
            TranslateMessage(&event); DispatchMessageW(&event);
            if(change && event.message==resolveStatusChanged) return;
        } else MsgWaitForMultipleObjects(0,nullptr,FALSE,100,QS_ALLINPUT);
    }
    if(change) throw std::runtime_error("Installer process monitor did not notify");
}
int main() {
    const auto root=Installation::fs::temp_directory_path()/(L"unbound-status-test-"+std::to_wstring(GetCurrentProcessId()));
    HWND window=nullptr;
    try {
        presentationTests();
        wchar_t ownPath[32768];
        require(GetModuleFileNameW(nullptr,ownPath,32768)!=0,"Cannot locate fixture executable");
        Installation::ResolvePresence ownProcess(Installation::fs::path(ownPath).filename().wstring());
        require(ownProcess(),"Cannot observe fixture process");
        for(int i=0;i<1000;++i) require(ownProcess(),"Cached running process was lost");
        require(ownProcess.snapshots==1,"Running-process checks enumerated the process list again");
        require(!Installation::fs::exists(root),"Fixture directory already exists");
        paths={root/L"Unbound",root/L"Scripts"}; payload={"loader","helper","hook","test"};
        Installation::apply(paths,&payload,[]{return false;});
        // Different bundled runtime enables Update after Resolve exits.
        payload.version="next";
        fixtureResolveRunning=true;
        WNDCLASSW type{}; type.hInstance=GetModuleHandleW(nullptr); type.lpfnWndProc=windowProc; type.lpszClassName=L"UnboundInstallerStateTest";
        RegisterClassW(&type);
        window=CreateWindowW(type.lpszClassName,L"Fixture",0,0,0,InstallerUI::width,InstallerUI::height,nullptr,nullptr,type.hInstance,nullptr);
        require(caption(installButton)==L"Update\nnext" && caption(uninstallButton)==L"Uninstall\ntest","Buttons do not contain their respective versions");
        RECT left,right,status;
        GetWindowRect(uninstallButton,&left); GetWindowRect(installButton,&right);
        GetWindowRect(statusLabel,&status);
        require(left.left<right.left && left.top==right.top && left.bottom<status.top,
            "Shared row order or left/right action placement changed");
        require((GetWindowLongW(installButton,GWL_STYLE)&BS_MULTILINE)!=0,"Version button cannot display two rows");
        require(window && !IsWindowEnabled(installButton) && !IsWindowEnabled(uninstallButton),"Running Resolve did not disable actions");
        fixtureResolveRunning=false; waitForMonitor();
        require(IsWindowEnabled(installButton) && IsWindowEnabled(uninstallButton),"Quit without focus change did not enable actions");
        require(processMonitor.background,"Process monitor was not placed at background/idle priority");
        SetWindowTextW(statusLabel,L"Operation completed"); waitForMonitor(false);
        wchar_t text[128]; GetWindowTextW(statusLabel,text,128);
        require(std::wstring(text)==L"Operation completed","Unchanged status overwrote operation feedback");
        fixtureResolveRunning=true; waitForMonitor();
        require(!IsWindowEnabled(installButton) && !IsWindowEnabled(uninstallButton),"Launch without focus change did not disable actions");
        fixtureResolveRunning=false; payload.version="test"; waitForMonitor();
        require(!IsWindowEnabled(installButton) && IsWindowEnabled(uninstallButton),"Matching runtime incorrectly enabled Update");
        Installation::apply(paths,nullptr,[]{return false;});
        Installation::write(paths.support/L"Unbound.json","{}");
        refresh(false);
        require(caption(uninstallButton)==L"Uninstall" && !IsWindowEnabled(uninstallButton) && IsWindowEnabled(installButton),"Retained settings must not enable uninstall");
        require(Installation::read(paths.support/L"Unbound.json")=="{}","Refresh changed retained settings");
        DestroyWindow(window); window=nullptr; Installation::fs::remove_all(root);
        std::cout<<"PASS: cached process handle avoids scans; background monitor detects quit/start without activation and preserves feedback/build state\n"; return 0;
    } catch(const std::exception &e) { if(window) DestroyWindow(window); std::cerr<<e.what()<<"\n"; return 1; }
}
