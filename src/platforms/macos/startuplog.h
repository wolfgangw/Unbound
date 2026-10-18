// SPDX-License-Identifier: MIT
#pragma once
#import <Foundation/Foundation.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#ifndef UNBOUND_STARTUP_LOG_NAME
#define UNBOUND_STARTUP_LOG_NAME "bootstrap.log"
#endif
inline void unboundStartupLog(const char *message)
{
    @autoreleasepool {
        const char *overridePath=getenv("UNBOUND_LOG_FILE");
        NSString *path=overridePath && *overridePath ? [NSString stringWithUTF8String:overridePath] :
            [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Logs/Unbound/" UNBOUND_STARTUP_LOG_NAME];
        NSString *directory = path.stringByDeletingLastPathComponent;
        [NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
        FILE *file = fopen(path.fileSystemRepresentation, "a");
        if (file) { fprintf(file,"%s pid=%d %s\n", NSDate.date.description.UTF8String, getpid(), message); fclose(file); }
    }
}
