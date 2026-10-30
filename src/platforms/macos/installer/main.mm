// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "../../../installer/presentation.h"

static NSString *string(const std::string &value) { return [NSString stringWithUTF8String:value.c_str()]; }
static std::string utf8(NSString *value) { return value.UTF8String ?: ""; }
static NSRect frame(InstallerUI::Rect r) {
    // Shared coordinates start at the top left; AppKit starts at the bottom.
    return NSMakeRect(r.x,InstallerUI::height-r.y-r.height,r.width,r.height);
}
static void buttonText(NSButton *button,const std::string &value) {
    button.title=string(value);
    button.font=[NSFont systemFontOfSize:InstallerUI::fonts::body];
    button.bezelStyle=NSBezelStyleRegularSquare;
    NSButtonCell *cell=(NSButtonCell *)button.cell;
    cell.usesSingleLineMode=NO;
    cell.wraps=YES;
    cell.lineBreakMode=NSLineBreakByWordWrapping;
}

static BOOL exists(NSString *path) { return [NSFileManager.defaultManager fileExistsAtPath:path]; }
static BOOL sameContents(NSString *installed, NSString *bundled) {
    NSData *actual = [NSData dataWithContentsOfFile:installed];
    NSData *expected = [NSData dataWithContentsOfFile:bundled];
    // Missing or unreadable files must leave Update available for repair.
    return actual && expected && [actual isEqualToData:expected];
}
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
@property(strong) NSProgressIndicator *progress;
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
    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, InstallerUI::width, InstallerUI::height)
        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title = applicationName();
    self.window.delegate = self;
    self.window.releasedWhenClosed = NO;
    [self.window center];
    self.window.contentView = [[InstallerContent alloc] initWithFrame:NSMakeRect(0, 0, InstallerUI::width, InstallerUI::height)];
    NSView *view = self.window.contentView;
    NSImage *icon = [[NSImage alloc] initWithContentsOfFile:[self.resources stringByAppendingPathComponent:@"unbound-128.png"]];
    NSImageView *image = [[NSImageView alloc] initWithFrame:frame(InstallerUI::rects::icon)];
    image.image = icon;
    [view addSubview:image];
    [view addSubview:label(string(InstallerUI::text::app), frame(InstallerUI::rects::title), InstallerUI::fonts::title, YES)];
    NSTextField *subtitle = label(string(InstallerUI::text::subtitle), frame(InstallerUI::rects::subtitle), InstallerUI::fonts::body);
    subtitle.textColor = NSColor.secondaryLabelColor;
    [view addSubview:subtitle];
    NSBox *separator=[[NSBox alloc] initWithFrame:frame(InstallerUI::rects::separator)];
    separator.boxType=NSBoxSeparator;
    [view addSubview:separator];
    self.status = label(@"", frame(InstallerUI::rects::status), InstallerUI::fonts::status);
    self.status.textColor = NSColor.secondaryLabelColor;
    [view addSubview:self.status];
    self.installButton = [NSButton buttonWithTitle:@"" target:self action:@selector(install:)];
    self.installButton.frame = frame(InstallerUI::rects::install);
    self.installButton.keyEquivalent = InstallerUI::primaryAction==InstallerUI::Action::Install ? @"\r" : @"";
    [view addSubview:self.installButton];
    self.uninstallButton = [NSButton buttonWithTitle:@"" target:self action:@selector(uninstall:)];
    self.uninstallButton.frame = frame(InstallerUI::rects::uninstall);
    self.uninstallButton.keyEquivalent = InstallerUI::primaryAction==InstallerUI::Action::Uninstall ? @"\r" : @"";
    [view addSubview:self.uninstallButton];
    self.progress = [[NSProgressIndicator alloc] initWithFrame:frame(InstallerUI::rects::progress)];
    self.progress.style = NSProgressIndicatorStyleSpinning;
    self.progress.displayedWhenStopped = NO;
    [view addSubview:self.progress];
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
- (BOOL)matchesPayload {
    NSString *runtime = [self.support stringByAppendingPathComponent:@"Runtime"];
    NSString *payload = [self.resources stringByAppendingPathComponent:@"payload"];
    for (NSString *name in @[@"VERSION", @"BUILD", @"load.lua", @"UnboundLuaLoader.dylib", @"UnboundHelper.dylib"]) {
        NSString *source = [@[@"VERSION", @"BUILD"] containsObject:name]
            ? name : [@"runtime" stringByAppendingPathComponent:name];
        if (!sameContents([runtime stringByAppendingPathComponent:name],
                          [payload stringByAppendingPathComponent:source])) return NO;
    }
    return sameContents([self.scripts stringByAppendingPathComponent:@"Unbound.scriptlib"],
                        [payload stringByAppendingPathComponent:@"startup/Unbound.scriptlib"]);
}
- (void)refresh:(NSNotification *)notification {
    if (self.busy) return;
    BOOL running = resolveRunning();
    BOOL installed = self.installed;
    NSString *runtime = [self.support stringByAppendingPathComponent:@"Runtime"];
    NSCharacterSet *whitespace = NSCharacterSet.whitespaceAndNewlineCharacterSet;
    NSString *version = [[NSString stringWithContentsOfFile:[runtime stringByAppendingPathComponent:@"VERSION"]
        encoding:NSUTF8StringEncoding error:nil] stringByTrimmingCharactersInSet:whitespace];
    NSString *build = [[NSString stringWithContentsOfFile:[runtime stringByAppendingPathComponent:@"BUILD"]
        encoding:NSUTF8StringEncoding error:nil] stringByTrimmingCharactersInSet:whitespace];
    BOOL sameBuild = installed && [self matchesPayload];
    const auto ui=InstallerUI::present({bool(installed),bool(running),bool(sameBuild)},
        InstallerUI::version(utf8(version),utf8(build)),
        InstallerUI::version(utf8([NSBundle.mainBundle objectForInfoDictionaryKey:@"CFBundleShortVersionString"]),
            utf8([NSBundle.mainBundle objectForInfoDictionaryKey:@"UnboundRuntimeBuild"])));
    buttonText(self.installButton,ui.install); buttonText(self.uninstallButton,ui.uninstall);
    self.installButton.enabled=ui.installEnabled; self.uninstallButton.enabled=ui.uninstallEnabled;
    self.status.stringValue=string(ui.status);
}
- (void)applicationDidBecomeActive:(NSNotification *)notification { [self refresh:nil]; }
- (void)install:(id)sender { [self runOperation:NO]; }
- (void)uninstall:(id)sender {
    if (self.busy || resolveRunning()) { [self refresh:nil]; return; }
    [self refresh:nil];
    if (!self.uninstallButton.enabled) return;
    const InstallerUI::Operation op{true,bool(self.installed)};
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText=string(op.confirmationTitle()); alert.informativeText=string(op.confirmationText());
    [alert addButtonWithTitle:string(InstallerUI::text::cancel)];
    [alert addButtonWithTitle:string(InstallerUI::text::uninstall)];
    [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSAlertSecondButtonReturn) [self runOperation:YES];
    }];
}
- (void)runOperation:(BOOL)uninstall {
    if (self.busy || resolveRunning()) { [self refresh:nil]; return; }
    [self refresh:nil];
    if (uninstall ? !self.uninstallButton.enabled : !self.installButton.enabled) return;
    // Use the same installed-state check as the controls, before files change.
    BOOL installed = self.installed;
    const InstallerUI::Operation op{bool(uninstall),bool(installed)};
    self.busy = YES;
    self.installButton.enabled = self.uninstallButton.enabled = NO;
    self.status.stringValue=string(op.progress());
    [self.progress startAnimation:nil];
    NSString *script = [self.resources stringByAppendingPathComponent:uninstall ? @"scripts/uninstall.sh" : @"scripts/install.sh"];
    NSArray<NSString *> *arguments = uninstall ? @[script]
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
                self.status.stringValue=string(op.success());
            } else {
                self.status.stringValue=string(op.failure());
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = self.status.stringValue;
                alert.informativeText = output.length ? output : string(InstallerUI::text::retry);
                [alert addButtonWithTitle:string(InstallerUI::text::ok)];
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
