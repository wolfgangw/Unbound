// SPDX-License-Identifier: MIT
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
// Read-only: no Qt dependencies, callbacks or widget changes.
__attribute__((visibility("default"))) int UnboundLuaProbePid(void) { return getpid(); }
__attribute__((visibility("default"))) int UnboundLuaProbeMainThread(void) { return pthread_main_np(); }
__attribute__((visibility("default"))) int UnboundLuaProbeQt(void) {
    void **application=(void **)dlsym(RTLD_DEFAULT,"_ZN16QCoreApplication4selfE");
    return application && *application;
}
