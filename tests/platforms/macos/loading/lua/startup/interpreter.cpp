// SPDX-License-Identifier: MIT
// Use Resolve's actual LuaJIT runtime in a separate, non-GUI fixture process.
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
struct lua_State;
template<typename T> T symbol(void *library,const char *name) {
    auto result=reinterpret_cast<T>(dlsym(library,name));
    if(!result) { std::fprintf(stderr,"Missing Lua export: %s\n",name); std::exit(1); }
    return result;
}
int main(int argc,char **argv) {
    if(argc!=4) { std::fprintf(stderr,"Expected LuaJIT library, test script, repository root\n"); return 1; }
    auto library=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!library) { std::fprintf(stderr,"%s\n",dlerror()); return 1; }
    auto create=symbol<lua_State *(*)()>(library,"luaL_newstate");
    auto open=symbol<void (*)(lua_State *)>(library,"luaL_openlibs");
    auto load=symbol<int (*)(lua_State *,const char *)>(library,"luaL_loadfile");
    auto call=symbol<int (*)(lua_State *,int,int,int)>(library,"lua_pcall");
    auto string=symbol<const char *(*)(lua_State *,int,size_t *)>(library,"lua_tolstring");
    auto push=symbol<void (*)(lua_State *,const char *)>(library,"lua_pushstring");
    auto set=symbol<void (*)(lua_State *,int,const char *)>(library,"lua_setfield");
    auto close=symbol<void (*)(lua_State *)>(library,"lua_close");
    auto state=create(); if(!state) return 1;
    open(state);
    push(state,argv[3]); set(state,-10002,"unboundTestSourceRoot"); // Lua 5.1 globals index
    int status=load(state,argv[2]);
    if(!status) status=call(state,0,0,0);
    if(status) std::fprintf(stderr,"%s\n",string(state,-1,nullptr));
    close(state); dlclose(library);
    return status ? 1 : 0;
}
