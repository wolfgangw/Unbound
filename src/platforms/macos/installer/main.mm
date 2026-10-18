// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>

static BOOL exists(NSString *path) { return [NSFileManager.defaultManager fileExistsAtPath:path]; }
static NSString *applicationName() {
    return [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];
}
#ifdef UNBOUND_INSTALLER_TEST
static int fixtureResolveRunning = -1;
#endif
static BOOL resolveRunning() {
#ifdef UNBOUND_INSTALLER_TEST
    if (fixtureResolveRunning >= 0) return fixtureResolveRunning != 0;
#endif
    for (NSRunningApplication *app in NSWorkspace.sharedWorkspace.runningApplications)
        if ([app.bundleIdentifier isEqualToString:@"com.blackmagic-design.DaVinciResolve"]
            || [app.executableURL.lastPathComponent isEqualToString:@"Resolve"]) return YES;
    return NO;
}
static NSTextField *label(NSString *text, NSRect frame, CGFloat size, BOOL bold = NO) {
    NSTextField *field = [NSTextField wrappingLabelWithString:text];
    field.frame = frame;
    field.font = bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
    return field;
}

@interface InstallerContent : NSView
@end
@implementation InstallerContent
- (BOOL)isOpaque { return YES; }
- (void)drawRect:(NSRect)rect {
    [NSColor.windowBackgroundColor setFill];
    NSRectFill(rect);
}
@end

@interface Installer : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property(strong) NSWindow *window;
@property(strong) NSTextField *status;
@property(strong) NSButton *installButton;
@property(strong) NSButton *uninstallButton;
@property(strong) NSButton *removeSettings;
@property(strong) NSProgressIndicator *progress;
@property(strong) NSTextField *installedVersion;
@property BOOL busy;
@property(copy) NSString *support;
@property(copy) NSString *scripts;
@property(copy) NSString *resources;
@end

@implementation Installer
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    self.support = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/Unbound"];
    self.scripts = [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/Blackmagic Design/DaVinci Resolve/Fusion/Scripts"];
    self.resources = NSBundle.mainBundle.resourcePath;
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 480, 250)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title = applicationName();
    self.window.delegate = self;
    self.window.releasedWhenClosed = NO;
    [self.window center];
    self.window.contentView = [[InstallerContent alloc] initWithFrame:NSMakeRect(0, 0, 480, 250)];
    NSView *view = self.window.contentView;
    NSImage *icon = [[NSImage alloc] initWithContentsOfFile:[self.resources stringByAppendingPathComponent:@"unbound-128.png"]];
    NSImageView *image = [[NSImageView alloc] initWithFrame:NSMakeRect(28, 182, 48, 48)];
    image.image = icon;
    [view addSubview:image];
    [view addSubview:label(@"Unbound", NSMakeRect(90, 201, 330, 29), 24, YES)];
    NSTextField *subtitle = label(@"Floating panels for DaVinci Resolve", NSMakeRect(90, 179, 350, 22), 13);
    subtitle.textColor = NSColor.secondaryLabelColor;
    [view addSubview:subtitle];
    self.status = label(@"", NSMakeRect(28, 130, 424, 30), 12);
    self.status.textColor = NSColor.secondaryLabelColor;
    [view addSubview:self.status];
    self.removeSettings = [NSButton checkboxWithTitle:@"Also remove settings and layouts when uninstalling"
        target:self action:@selector(refresh:)];
    self.removeSettings.frame = NSMakeRect(28, 89, 424, 26);
    self.removeSettings.font = [NSFont systemFontOfSize:12];
    self.removeSettings.state = NSControlStateValueOff;
    [view addSubview:self.removeSettings];
    self.installButton = [NSButton buttonWithTitle:@"Install Unbound" target:self action:@selector(install:)];
    self.installButton.frame = NSMakeRect(292, 25, 160, 34);
    self.installButton.keyEquivalent = @"\r";
    [view addSubview:self.installButton];
    self.uninstallButton = [NSButton buttonWithTitle:@"Uninstall" target:self action:@selector(uninstall:)];
    self.uninstallButton.frame = NSMakeRect(28, 25, 110, 34);
    [view addSubview:self.uninstallButton];
    self.progress = [[NSProgressIndicator alloc] initWithFrame:NSMakeRect(259, 33, 16, 16)];
    self.progress.style = NSProgressIndicatorStyleSpinning;
    self.progress.displayedWhenStopped = NO;
    [view addSubview:self.progress];
    NSString *version = [NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    NSString *build = [NSBundle.mainBundle objectForInfoDictionaryKey:@"UnboundRuntimeBuild"];
    NSTextField *versionLabel = label([NSString stringWithFormat:@"Version %@ · %@", version, build],
        NSMakeRect(244, 6, 208, 18), 11);
    versionLabel.textColor = NSColor.secondaryLabelColor;
    versionLabel.alignment = NSTextAlignmentRight;
    [view addSubview:versionLabel];
    self.installedVersion = label(@"", NSMakeRect(28, 6, 208, 18), 11);
    self.installedVersion.textColor = NSColor.secondaryLabelColor;
    [view addSubview:self.installedVersion];
    [NSWorkspace.sharedWorkspace.notificationCenter addObserver:self selector:@selector(refresh:)
        name:NSWorkspaceDidTerminateApplicationNotification object:nil];
    [NSWorkspace.sharedWorkspace.notificationCenter addObserver:self selector:@selector(refresh:)
        name:NSWorkspaceDidLaunchApplicationNotification object:nil];
    [self refresh:nil];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
#ifdef UNBOUND_INSTALLER_TEST
    if ([NSProcessInfo.processInfo.arguments containsObject:@"--smoke-test"]) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 3), dispatch_get_main_queue(), ^{
            [self smokeTest];
        });
    }
#endif
}
- (BOOL)installed {
    return exists([self.support stringByAppendingPathComponent:@"Runtime"])
        || exists([self.scripts stringByAppendingPathComponent:@"Unbound.scriptlib"])
        || exists([self.scripts stringByAppendingPathComponent:@"Utility/Unbound.lua"])
        || exists([self.scripts stringByAppendingPathComponent:@"Utility/UnboundLua"]);
}
- (BOOL)hasSettings {
    return exists([self.support stringByAppendingPathComponent:@"Unbound.json"])
        || exists([self.support stringByAppendingPathComponent:@"settings.json"]);
}
- (void)refresh:(NSNotification *)notification {
    if (self.busy) return;
    BOOL running = resolveRunning();
    BOOL installed = self.installed;
    BOOL settingsOnly = !installed && self.hasSettings;
    NSString *runtime = [self.support stringByAppendingPathComponent:@"Runtime"];
    NSCharacterSet *whitespace = NSCharacterSet.whitespaceAndNewlineCharacterSet;
    NSString *version = [[NSString stringWithContentsOfFile:[runtime stringByAppendingPathComponent:@"VERSION"]
        encoding:NSUTF8StringEncoding error:nil] stringByTrimmingCharactersInSet:whitespace];
    NSString *build = [[NSString stringWithContentsOfFile:[runtime stringByAppendingPathComponent:@"BUILD"]
        encoding:NSUTF8StringEncoding error:nil] stringByTrimmingCharactersInSet:whitespace];
    self.installedVersion.hidden = !installed;
    self.installedVersion.stringValue = version.length
        ? (build.length ? [NSString stringWithFormat:@"Version %@ · %@", version, build]
                        : [NSString stringWithFormat:@"Version %@", version])
        : @"Version unknown";
    self.installButton.title = installed ? @"Update Unbound" : @"Install Unbound";
    BOOL sameBuild = installed
        && [version isEqualToString:[NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]]
        && [build isEqualToString:[NSBundle.mainBundle objectForInfoDictionaryKey:@"UnboundRuntimeBuild"]];
    self.installButton.enabled = !running && !sameBuild;
    self.uninstallButton.title = settingsOnly ? @"Remove settings and layouts…" : @"Uninstall";
    self.uninstallButton.frame = NSMakeRect(28, 25, settingsOnly ? 224 : 110, 34);
    self.uninstallButton.enabled = !running && (installed || settingsOnly);
    self.removeSettings.hidden = !installed;
    if (!installed) self.removeSettings.state = NSControlStateValueOff;
    self.removeSettings.enabled = !running;
    self.status.stringValue = running ? @"Quit DaVinci Resolve to install or uninstall Unbound."
        : installed ? @"Unbound is already installed."
        : self.hasSettings ? @"Unbound is not installed. Your saved settings are available."
        : @"Ready to install.";
}
- (void)applicationDidBecomeActive:(NSNotification *)notification { [self refresh:nil]; }
- (void)install:(id)sender { [self runOperation:NO removeSettings:NO]; }
- (void)uninstall:(id)sender {
    if (self.busy || resolveRunning()) { [self refresh:nil]; return; }
    [self refresh:nil];
    if (!self.uninstallButton.enabled) return;
    BOOL settingsOnly = !self.installed && self.hasSettings;
    BOOL remove = settingsOnly || self.removeSettings.state == NSControlStateValueOn;
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = settingsOnly ? @"Remove settings and layouts?" : @"Uninstall Unbound?";
    alert.informativeText = settingsOnly ? @"Your saved Unbound settings and layouts will be permanently removed."
        : remove ? @"Unbound and your saved settings and layouts will be removed from this account."
                 : @"Unbound will be removed from this account. Your settings and layouts will be kept for a future installation.";
    [alert addButtonWithTitle:@"Cancel"];
    [alert addButtonWithTitle:settingsOnly ? @"Remove" : @"Uninstall"];
    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        // A settings-only confirmation must not remove a newly installed runtime.
        if (settingsOnly && self.installed) { [self refresh:nil]; return; }
        if (response == NSAlertSecondButtonReturn) [self runOperation:YES removeSettings:remove];
    }];
}
- (void)runOperation:(BOOL)uninstall removeSettings:(BOOL)remove {
    if (self.busy || resolveRunning()) { [self refresh:nil]; return; }
    [self refresh:nil];
    if (uninstall ? !self.uninstallButton.enabled : !self.installButton.enabled) return;
    // Use the same installed-state check as the controls, before files change.
    BOOL installed = self.installed;
    BOOL updating = !uninstall && installed;
    BOOL settingsOnly = uninstall && !installed && remove;
    self.busy = YES;
    self.installButton.enabled = self.uninstallButton.enabled = self.removeSettings.enabled = NO;
    self.status.stringValue = settingsOnly ? @"Removing settings and layouts…"
        : uninstall ? @"Uninstalling…" : updating ? @"Updating…" : @"Installing…";
    [self.progress startAnimation:nil];
    NSString *script = [self.resources stringByAppendingPathComponent:uninstall ? @"scripts/uninstall.sh" : @"scripts/install.sh"];
    NSArray<NSString *> *arguments = uninstall ? (remove ? @[script, @"--remove-settings"] : @[script])
        : @[script, [self.resources stringByAppendingPathComponent:@"payload"]];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSTask *task = [[NSTask alloc] init];
        task.executableURL = [NSURL fileURLWithPath:@"/bin/bash"];
        task.arguments = arguments;
        NSMutableDictionary *environment = [NSProcessInfo.processInfo.environment mutableCopy];
        // A Finder-launched installer always operates on the logged-in account;
        // developer fixture redirects must never leak into the graphical app.
        environment[@"UNBOUND_USER_HOME"] = NSHomeDirectory();
        environment[@"PATH"] = @"/usr/bin:/bin:/usr/sbin:/sbin";
        task.environment = environment;
        NSPipe *pipe = [NSPipe pipe];
        task.standardOutput = pipe; task.standardError = pipe;
        NSError *error = nil;
        BOOL launched = [task launchAndReturnError:&error];
        NSString *output = error.localizedDescription ?: @"";
        int status = 1;
        if (launched) {
            NSData *data = [pipe.fileHandleForReading readDataToEndOfFile];
            [task waitUntilExit];
            output = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] ?: @"";
            status = task.terminationStatus;
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            self.busy = NO;
            [self.progress stopAnimation:nil];
            [self refresh:nil];
            if (status == 0) {
                self.removeSettings.state = NSControlStateValueOff;
                [self refresh:nil];
                self.status.stringValue = settingsOnly ? @"Settings and layouts removed." : uninstall
                    ? (remove ? @"Unbound, settings and layouts removed." : @"Unbound removed. Settings and layouts kept.")
                    : updating ? @"Updated. Open DaVinci Resolve to use Unbound."
                               : @"Installed. Open DaVinci Resolve to use Unbound.";
            } else {
                self.status.stringValue = settingsOnly ? @"Settings and layouts could not be removed."
                    : uninstall ? @"Uninstallation could not finish."
                    : updating ? @"Update could not finish." : @"Installation could not finish.";
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = self.status.stringValue;
                alert.informativeText = output.length ? output : @"Please try again after closing Resolve.";
                [alert addButtonWithTitle:@"OK"];
                [alert beginSheetModalForWindow:self.window completionHandler:nil];
            }
        });
    });
}
- (BOOL)windowShouldClose:(NSWindow *)sender { return !self.busy; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    return self.busy ? NSTerminateCancel : NSTerminateNow;
}
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }
#ifdef UNBOUND_INSTALLER_TEST
#include "smoketest.h"
#endif

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = NSApplication.sharedApplication;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        NSMenu *menu = [[NSMenu alloc] init];
        NSMenuItem *item = [[NSMenuItem alloc] init];
        NSMenu *application = [[NSMenu alloc] initWithTitle:applicationName()];
        [application addItemWithTitle:[@"Quit " stringByAppendingString:applicationName()]
            action:@selector(terminate:) keyEquivalent:@"q"];
        item.submenu = application; [menu addItem:item]; app.mainMenu = menu;
        __attribute__((objc_precise_lifetime)) Installer *delegate = [[Installer alloc] init];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
