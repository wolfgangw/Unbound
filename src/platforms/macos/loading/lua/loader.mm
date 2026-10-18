// SPDX-License-Identifier: MIT
// Explicit in-process entry point. No constructors, IO callbacks or Qt link.
#import <Foundation/Foundation.h>
#include <atomic>
#include <cstring>
#include <dlfcn.h>
#include "startuplog.h"
static std::atomic<int> state{0};
static void *retainedLoader=nullptr, *retainedHelper=nullptr;
// 0 idle, 1 queued, 2 attached, 3 already active; negative values are failures.
extern "C" __attribute__((visibility("default"))) int UnboundLuaStatus() { return state.load(); }
extern "C" __attribute__((visibility("default"))) int UnboundLuaStart()
{
    @autoreleasepool {
        if (state.load()!=0) return state.load();
        auto singleton=reinterpret_cast<void **>(dlsym(RTLD_DEFAULT,"_ZN16QCoreApplication4selfE"));
        auto version=reinterpret_cast<const char *(*)()>(dlsym(RTLD_DEFAULT,"qVersion"));
        if (!singleton || !*singleton || !version) {
            unboundStartupLog("Lua request rejected: no existing Qt application"); return -1;
        }
        if (strcmp(version(),"5.15.2")) {
            unboundStartupLog("Lua request rejected: unsupported Qt version"); return -2;
        }
        int expected=0;
        if (!state.compare_exchange_strong(expected,1)) return expected;
        Dl_info own{};
        if (!dladdr(reinterpret_cast<void *>(&UnboundLuaStart),&own) || !own.dli_fname
            || !(retainedLoader=dlopen(own.dli_fname,RTLD_NOW|RTLD_LOCAL))) {
            state=-3; unboundStartupLog("Lua loader could not retain its image"); return -3;
        }
        NSString *helper=[[[NSString stringWithUTF8String:own.dli_fname] stringByDeletingLastPathComponent]
            stringByAppendingPathComponent:@"UnboundHelper.dylib"];
        unboundStartupLog("Lua request accepted; queued GUI-thread attachment");
        // Dispatch copies this block and retains its NSString capture. No Lua
        // callbacks survive the script. This image is retained independently.
        dispatch_async(dispatch_get_main_queue(),^{
            retainedHelper=dlopen(helper.fileSystemRepresentation,RTLD_NOW|RTLD_LOCAL);
            if (!retainedHelper) { state=-4; unboundStartupLog(dlerror()); return; }
            auto attach=reinterpret_cast<int (*)()>(dlsym(retainedHelper,"UnboundLuaAttach"));
            const int result=attach ? attach() : 0;
            if (result==1) { state=2; unboundStartupLog("Lua loader attached shared Unbound controller"); }
            else if (result==2) { state=3; unboundStartupLog("Unbound controller already active; reused without duplicate UI"); }
            else { state=-5; unboundStartupLog("Lua helper rejected GUI-thread attachment"); }
        });
        return 1;
    }
}
