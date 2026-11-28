// SPDX-License-Identifier: MIT
// Explicit in-process entry point. No constructors, IO callbacks or Qt link.
#import <Foundation/Foundation.h>
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include "startuplog.h"
static std::atomic<int> state{0};
static std::atomic<int> resolveEdition{-1};
static void *retainedLoader=nullptr, *retainedHelper=nullptr;
// 0 idle, 1 queued, 2 attached, 3 already active; negative values are failures.
extern "C" __attribute__((visibility("default"))) int UnboundLuaStatus() { return state.load(); }
static int startForEdition(int edition)
{
    @autoreleasepool {
        if (state.load()!=0) return state.load();
        auto version=reinterpret_cast<const char *(*)()>(dlsym(RTLD_DEFAULT,"qVersion"));
        if (!version) {
            unboundStartupLog("Lua request rejected: no existing Qt application"); return -1;
        }
        if (strcmp(version(),"5.15.2")) {
            unboundStartupLog("Lua request rejected: unsupported Qt version"); return -2;
        }
        auto singleton=reinterpret_cast<void **>(dlsym(RTLD_DEFAULT,"_ZN16QCoreApplication4selfE"));
        if (!singleton || !*singleton) {
            unboundStartupLog("Lua request rejected: no existing Qt application"); return -1;
        }
        int expected=0;
        if (!state.compare_exchange_strong(expected,1)) return expected;
        resolveEdition=edition;
        Dl_info own{};
        if (!dladdr(reinterpret_cast<void *>(&startForEdition),&own) || !own.dli_fname
            || !(retainedLoader=dlopen(own.dli_fname,RTLD_NOW|RTLD_LOCAL))) {
            state=-3; unboundStartupLog("Lua loader could not retain its image"); return -3;
        }
        NSString *helper=[[[NSString stringWithUTF8String:own.dli_fname] stringByDeletingLastPathComponent]
            stringByAppendingPathComponent:@"Unbound.dylib"];
        unboundStartupLog("Lua request accepted; queued GUI-thread attachment");
        // Dispatch copies this block and retains its NSString capture. No Lua
        // callbacks survive the script. This image is retained independently.
        dispatch_async(dispatch_get_main_queue(),^{
            retainedHelper=dlopen(helper.fileSystemRepresentation,RTLD_NOW|RTLD_LOCAL);
            if (!retainedHelper) {
                if (retainedLoader) { dlclose(retainedLoader); retainedLoader=nullptr; }
                state=-4; unboundStartupLog(dlerror()); return;
            }
            auto configure=reinterpret_cast<void (*)(int)>(dlsym(retainedHelper,"UnboundSetResolveEdition"));
            if(configure) configure(resolveEdition.load());
            unboundStartupLog(resolveEdition==0 ? "Shortcut route: Free native actions" : resolveEdition==1 ? "Shortcut route: Studio scripts" : "Shortcut route: unknown edition; scripts retained");
            auto attach=reinterpret_cast<int (*)()>(dlsym(retainedHelper,"UnboundLuaAttach"));
            const int result=attach ? attach() : 0;
            if (result==1) { state=2; unboundStartupLog("Lua loader attached shared Unbound controller"); }
            else if (result==2) { state=3; unboundStartupLog("Unbound controller already active; reused without duplicate UI"); }
            else {
                dlclose(retainedHelper); retainedHelper=nullptr;
                if (retainedLoader) { dlclose(retainedLoader); retainedLoader=nullptr; }
                state=-5; unboundStartupLog("Lua helper rejected GUI-thread attachment");
            }
        });
        return 1;
    }
}
extern "C" __attribute__((visibility("default"))) int UnboundLuaStart() { return startForEdition(-1); }

extern "C" __attribute__((visibility("default"))) int UnboundLuaStartForEdition(int edition)
{
    return startForEdition(edition);
}
