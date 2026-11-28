// SPDX-License-Identifier: MIT
#include "installation.h"
#include <iostream>
static void require(bool c,const char *m) { if(!c) throw std::runtime_error(m); }
int wmain(int argc,wchar_t **argv) {
    namespace fs=std::filesystem; using namespace Installation;
    const auto root=fs::temp_directory_path()/(L"unbound-installer-test-"+std::to_wstring(GetCurrentProcessId()));
    if(argc>2 || fs::exists(root)) return 1;
    // An optional scratch directory on another drive exercises redirected AppData.
    const auto roaming=argc==2 ? fs::path(argv[1])/root.filename() : root/L"Roaming User";
    try {
        if(argc==2 && !fs::create_directory(roaming)) return 1;
        Paths p{root/L"Local User/Unbound",roaming/L"Scripts"};
        const Payload first{"loader-one","helper-one","hook-one","0.3.0+test.1",{{L"Layout 1.lua","command-one"}}}, second{"loader-two","helper-two","hook-two","0.3.0+test.2",{{L"Layout 1.lua","command-two"}}};
        auto idle=[]{return false;};
        apply(p,&first,idle); require(matches(p,first),"Fresh install mismatch");
        write(p.runtime()/L"UnboundHelper.dll","obsolete runtime");
        write(p.support/L"Unbound.json","saved layouts"); write(p.scripts/L"unrelated.lua","keep");
        write(p.commands()/L"Layout 2.lua","second original command");
        auto locked=CreateFileW(p.commands().c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
            nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
        require(locked!=INVALID_HANDLE_VALUE,"Could not lock the commands directory");
        bool renameBlocked=false;
        try { apply(p,&second,idle); } catch(const fs::filesystem_error &) { renameBlocked=true; }
        CloseHandle(locked);
        require(renameBlocked && read(p.commands()/L"Layout 2.lua")=="second original command",
            "A blocked directory rename lost an original command");
        fs::remove(p.commands()/L"Layout 2.lua");
        require(matches(p,first),"Blocked command backup did not restore the installation");
        for(int phase=0;phase<=4;++phase) {
            bool failed=false;
            try { apply(p,&second,idle,[=](int stage){if(stage==phase) throw std::runtime_error("injected failure");}); }
            catch(...) { failed=true; }
            require(failed && matches(p,first),"Failed update did not restore old runtime and hook");
            require(read(p.runtime()/L"UnboundHelper.dll")=="obsolete runtime","Rollback lost previous runtime files");
        }
        // A locked staging folder during rollback cleanup must not mask the root failure.
        HANDLE lockedStage=INVALID_HANDLE_VALUE;
        fs::path retainedStage;
        std::string rollbackMessage;
        try {
            apply(p,&second,idle,[&](int stage) {
                if(stage!=0) return;
                for(const auto &entry:fs::directory_iterator(p.support)) {
                    if(entry.is_directory() && entry.path().filename().wstring().rfind(L"Unbound-transaction-",0)==0) {
                        retainedStage=entry.path();
                        lockedStage=CreateFileW(retainedStage.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
                            nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
                        break;
                    }
                }
                throw std::runtime_error("root cause failure");
            });
        } catch(const std::runtime_error &e) { rollbackMessage=e.what(); }
        require(lockedStage!=INVALID_HANDLE_VALUE,"Fixture did not lock the staging directory");
        const bool stageSurvived=fs::is_directory(retainedStage);
        CloseHandle(lockedStage);
        require(stageSurvived,"Rollback cleanup did not leave the locked staging directory in place");
        require(!fs::exists(p.scripts.parent_path()/retainedStage.filename()),"Rollback skipped cleanup of the unlocked scripts stage");
        fs::remove_all(retainedStage);
        require(rollbackMessage=="root cause failure" && matches(p,first),
            "Rollback cleanup error masked the original installation failure");
        bool blocked=false; try { apply(p,&second,[]{return true;}); } catch(...) { blocked=true; }
        require(blocked && matches(p,first),"Resolve guard failed");
        int calls=0; blocked=false;
        try { apply(p,&second,[&]{return ++calls==2;}); } catch(...) {blocked=true;}
        require(blocked && matches(p,first),"Resolve launch during staging changed installation");
        apply(p,&second,idle); require(matches(p,second),"Update mismatch");
        require(!fs::exists(p.runtime()/L"UnboundHelper.dll"),"Update retained obsolete runtime files");
        fs::remove(p.commands()/L"Layout 1.lua"); require(!matches(p,second),"Missing command disables repair"); apply(p,&second,idle);
        write(p.commands()/L"Stale.lua","stale command");
        require(!matches(p,second),"Extra stale command file did not trigger repair");
        apply(p,&second,idle);
        require(matches(p,second) && !fs::exists(p.commands()/L"Stale.lua"),"Repair did not remove stale command file");
        // Invalid command filenames must be rejected before modifying existing files.
        Payload badCommand=second; badCommand.commands[L"..\\evil.lua"]="bad";
        bool rejected=false; try { apply(p,&badCommand,idle); } catch(...) { rejected=true; }
        require(rejected && matches(p,second),"Path-traversal command filename was not rejected");
        fs::remove(p.hook()); require(!matches(p,second),"Broken hook disables repair"); apply(p,&second,idle);
        write(p.support/L"settings.json","legacy");
        const auto retained=[&]{return read(p.support/L"Unbound.json")=="saved layouts" && read(p.support/L"settings.json")=="legacy";};
        apply(p,nullptr,idle); require(!installed(p) && !fs::exists(p.commands()) && retained(),"Uninstall removed settings or retained commands");
        apply(p,&first,idle); require(matches(p,first) && retained(),"Reinstall lost settings");
        apply(p,nullptr,idle);
        apply(p,nullptr,idle);
        require(!installed(p) && retained() && read(p.scripts/L"unrelated.lua")=="keep","Repeated uninstall altered retained files");
        fs::remove_all(root);
        if(argc==2) fs::remove_all(roaming);
        std::cout<<"PASS: install/update, all rollback stages, Resolve guard, repair, uninstall/reinstall, settings and unrelated files\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<"; retained fixture: "<<root<<"\n"; return 1;}
}
