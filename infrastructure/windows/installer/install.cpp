// SPDX-License-Identifier: MIT
// Development command; uses exactly the graphical installer's transaction code.
#include "installation.h"
#include <iostream>
int wmain(int argc,wchar_t **argv) {
    try {
        if(argc!=4) throw std::runtime_error("Expected runtime directory, startup hook and version file");
        Installation::Payload payload{Installation::read(std::filesystem::path(argv[1])/L"UnboundLuaLoader.dll"),
            Installation::read(std::filesystem::path(argv[1])/L"Unbound.dll"),Installation::read(argv[2]),Installation::read(argv[3])};
        Installation::apply(Installation::userPaths(),&payload);
        std::cout<<"Installed. Open DaVinci Resolve normally.\n"; return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<"\n"; return 1;}
}
