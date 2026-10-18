// SPDX-License-Identifier: MIT
#include "installation.h"
#include <iostream>
static void require(bool c,const char *m) { if(!c) throw std::runtime_error(m); }
int main() {
    namespace fs=std::filesystem; using namespace Installation;
    const auto root=fs::temp_directory_path()/(L"unbound-installer-test-"+std::to_wstring(GetCurrentProcessId()));
    if(fs::exists(root)) return 1;
    try {
        Paths p{root/L"Local User/Unbound",root/L"Roaming User/Scripts"};
        const Payload first{"loader-one","helper-one","hook-one","0.3.0+test.1"}, second{"loader-two","helper-two","hook-two","0.3.0+test.2"};
        auto idle=[]{return false;};
        apply(p,&first,idle); require(matches(p,first),"Fresh install mismatch");
        write(p.support/L"Unbound.json","saved layouts"); write(p.scripts/L"unrelated.lua","keep");
        for(int phase=0;phase<=3;++phase) {
            bool failed=false;
            try { apply(p,&second,idle,[=](int stage){if(stage==phase) throw std::runtime_error("injected failure");}); }
            catch(...) { failed=true; }
            require(failed && matches(p,first),"Failed update did not restore old runtime and hook");
        }
        bool blocked=false; try { apply(p,&second,[]{return true;}); } catch(...) { blocked=true; }
        require(blocked && matches(p,first),"Resolve guard failed");
        int calls=0; blocked=false;
        try { apply(p,&second,[&]{return ++calls==2;}); } catch(...) {blocked=true;}
        require(blocked && matches(p,first),"Resolve launch during staging changed installation");
        apply(p,&second,idle); require(matches(p,second),"Update mismatch");
        fs::remove(p.hook()); require(!matches(p,second),"Broken hook disables repair"); apply(p,&second,idle);
        write(p.support/L"settings.json","legacy");
        const auto retained=[&]{return read(p.support/L"Unbound.json")=="saved layouts" && read(p.support/L"settings.json")=="legacy";};
        apply(p,nullptr,idle); require(!installed(p) && retained(),"Uninstall removed settings");
        apply(p,&first,idle); require(matches(p,first) && retained(),"Reinstall lost settings");
        apply(p,nullptr,idle);
        apply(p,nullptr,idle);
        require(!installed(p) && retained() && read(p.scripts/L"unrelated.lua")=="keep","Repeated uninstall altered retained files");
        fs::remove_all(root);
        std::cout<<"PASS: install/update, all rollback stages, Resolve guard, repair, uninstall/reinstall, settings and unrelated files\n";
        return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<"; retained fixture: "<<root<<"\n"; return 1;}
}
