// SPDX-License-Identifier: MIT
// Included only by the installer test build, inside its controller implementation.
- (BOOL)writeSnapshot:(NSString *)path {
    NSView *view = self.window.contentView;
    NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
    [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [png writeToFile:path atomically:YES];
}
- (void)smokeTest {
    NSArray *args = NSProcessInfo.processInfo.arguments;
    NSUInteger index = [args indexOfObject:@"--snapshot"];
    NSString *snapshot = index != NSNotFound && index + 1 < args.count ? args[index + 1] : nil;
    BOOL valid = self.window.visible
        && exists([self.resources stringByAppendingPathComponent:@"scripts/install.sh"])
        && exists([self.resources stringByAppendingPathComponent:@"scripts/uninstall.sh"])
        && exists([self.resources stringByAppendingPathComponent:@"payload/runtime/UnboundHelper.dylib"]);
    NSString *originalSupport = self.support;
    NSString *originalScripts = self.scripts;
    NSString *fixture = [NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
    self.support = [fixture stringByAppendingPathComponent:@"Support"];
    self.scripts = [fixture stringByAppendingPathComponent:@"Scripts"];
    fixtureResolveRunning = 0;
    [self refresh:nil];
    valid = valid && [self.installButton.title hasPrefix:@"Install\n"]
        && self.installButton.enabled == !resolveRunning()
        && !self.uninstallButton.enabled;
    NSString *runtime = [self.support stringByAppendingPathComponent:@"Runtime"];
    NSFileManager *files = NSFileManager.defaultManager;
    valid = [files createDirectoryAtPath:runtime withIntermediateDirectories:YES attributes:nil error:nil] && valid;
    [self refresh:nil];
    valid = valid && [self.installButton.title hasPrefix:@"Update\n"]
        && self.installButton.enabled == !resolveRunning(); // Unknown legacy version remains updatable.
    NSString *version = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    NSString *build = [NSBundle.mainBundle objectForInfoDictionaryKey:@"UnboundRuntimeBuild"];
    valid = valid && NSEqualRects(self.installButton.frame,frame(InstallerUI::rects::install))
        && NSEqualRects(self.uninstallButton.frame,frame(InstallerUI::rects::uninstall))
        && NSEqualRects(self.status.frame,frame(InstallerUI::rects::status))
        && [self.installButton.title isEqualToString:string(std::string(InstallerUI::text::update)+"\n"+
            InstallerUI::version(utf8(version),utf8(build)))];
    valid = valid && [self.window.title isEqualToString:
        [NSString stringWithFormat:@"Unbound %@ (%@)", version, build]];
    NSString *versionPath = [runtime stringByAppendingPathComponent:@"VERSION"];
    NSString *buildPath = [runtime stringByAppendingPathComponent:@"BUILD"];
    valid = [[version stringByAppendingString:@"\n"] writeToFile:versionPath atomically:YES encoding:NSUTF8StringEncoding error:nil] && valid;
    valid = [[build stringByAppendingString:@"\n"] writeToFile:buildPath atomically:YES encoding:NSUTF8StringEncoding error:nil] && valid;
    [self refresh:nil];
    valid = valid && self.installButton.enabled; // Matching metadata alone cannot certify a complete installation.
    NSString *payload = [self.resources stringByAppendingPathComponent:@"payload"];
    NSString *hook = [self.scripts stringByAppendingPathComponent:@"Unbound.scriptlib"];
    valid = [files createDirectoryAtPath:self.scripts withIntermediateDirectories:YES attributes:nil error:nil] && valid;
    NSDictionary<NSString *, NSString *> *payloadFiles = @{
        versionPath: @"VERSION", buildPath: @"BUILD",
        [runtime stringByAppendingPathComponent:@"load.lua"]: @"runtime/load.lua",
        [runtime stringByAppendingPathComponent:@"UnboundLuaLoader.dylib"]: @"runtime/UnboundLuaLoader.dylib",
        [runtime stringByAppendingPathComponent:@"UnboundHelper.dylib"]: @"runtime/UnboundHelper.dylib",
        hook: @"startup/Unbound.scriptlib"
    };
    for (NSString *path in payloadFiles) {
        NSData *contents = [NSData dataWithContentsOfFile:[payload stringByAppendingPathComponent:payloadFiles[path]]];
        valid = [contents writeToFile:path atomically:YES] && valid;
    }
    [self refresh:nil];
    valid = valid && !self.installButton.enabled && self.uninstallButton.enabled == !resolveRunning();
    // Each required file independently controls repair availability, even when
    // version/build metadata still matches. Restoring it disables Update again.
    for (NSString *path in payloadFiles) {
        NSData *contents = [NSData dataWithContentsOfFile:path];
        if (!contents.length) { valid = NO; continue; }
        valid = [files removeItemAtPath:path error:nil] && valid;
        [self refresh:nil];
        valid = valid && self.installButton.enabled && self.uninstallButton.enabled;
        valid = [contents writeToFile:path atomically:YES] && valid;
        [self refresh:nil];
        valid = valid && !self.installButton.enabled;
        NSMutableData *changed = [contents mutableCopy];
        ((unsigned char *)changed.mutableBytes)[0] ^= 1; // Same length, different contents.
        valid = [changed writeToFile:path atomically:YES] && valid;
        [self refresh:nil];
        valid = valid && self.installButton.enabled;
        fixtureResolveRunning = 1;
        [self refresh:nil];
        valid = valid && !self.installButton.enabled && !self.uninstallButton.enabled;
        fixtureResolveRunning = 0;
        valid = [contents writeToFile:path atomically:YES] && valid;
        [self refresh:nil];
        valid = valid && !self.installButton.enabled && self.uninstallButton.enabled;
    }
    valid = [@"different-build\n" writeToFile:buildPath atomically:YES encoding:NSUTF8StringEncoding error:nil] && valid;
    [self refresh:nil];
    valid = valid && self.installButton.enabled == !resolveRunning(); // Same release, different internal build.
    valid = [@"different-version\n" writeToFile:versionPath atomically:YES encoding:NSUTF8StringEncoding error:nil] && valid;
    valid = [build writeToFile:buildPath atomically:YES encoding:NSUTF8StringEncoding error:nil] && valid;
    [self refresh:nil];
    valid = valid && self.installButton.enabled == !resolveRunning();
    fixtureResolveRunning = 1;
    [self refresh:nil];
    valid = valid && !self.installButton.enabled && !self.uninstallButton.enabled;
    fixtureResolveRunning = 0;
    [self refresh:nil];
    valid = valid && self.installButton.enabled && self.uninstallButton.enabled;
    // Retained settings never enable an uninstall or deletion action.
    valid = [files removeItemAtPath:runtime error:nil] && valid;
    [self refresh:nil];
    valid = valid && self.installButton.enabled && self.uninstallButton.enabled; // Orphaned startup hook.
    valid = [files removeItemAtPath:hook error:nil] && valid;
    NSString *settings = [self.support stringByAppendingPathComponent:@"Unbound.json"];
    valid = [@"{}" writeToFile:settings atomically:YES encoding:NSUTF8StringEncoding error:nil] && valid;
    [self refresh:nil];
    valid = valid && self.installButton.enabled && !self.uninstallButton.enabled
        && [self.uninstallButton.title isEqualToString:@"Uninstall"]
        && [[NSString stringWithContentsOfFile:settings encoding:NSUTF8StringEncoding error:nil] isEqualToString:@"{}"];
    if (snapshot) valid = [self writeSnapshot:[[snapshot stringByDeletingPathExtension] stringByAppendingString:@"-settings.png"]] && valid;
    fixtureResolveRunning = 1;
    [self refresh:nil];
    valid = valid && !self.installButton.enabled && !self.uninstallButton.enabled;
    fixtureResolveRunning = 0;
    NSString *legacySettings = [self.support stringByAppendingPathComponent:@"settings.json"];
    valid = [files moveItemAtPath:settings toPath:legacySettings error:nil] && valid;
    [self refresh:nil];
    valid = valid && !self.uninstallButton.enabled && self.installButton.enabled;
    valid = [files removeItemAtPath:legacySettings error:nil] && valid;
    [self refresh:nil];
    valid = valid && !self.uninstallButton.enabled && self.installButton.enabled
        && [self.uninstallButton.title isEqualToString:@"Uninstall"];
    valid = [files removeItemAtPath:fixture error:nil] && valid;
    fixtureResolveRunning = -1;
    self.support = originalSupport;
    self.scripts = originalScripts;
    [self refresh:nil];
    if (snapshot) valid = [self writeSnapshot:snapshot] && valid;
    fprintf(stdout, "%s: installer UI, retained settings, exact payload comparison, missing/altered files, Resolve guard and bundled payload\n", valid ? "PASS" : "FAIL");
    fflush(stdout);
    exit(valid ? 0 : 1);
}
