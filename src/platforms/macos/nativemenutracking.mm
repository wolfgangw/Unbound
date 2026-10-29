// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "nativemenutracking.h"
#include <QGuiApplication>
namespace Unbound {
bool nativeApplicationActive() {
    return QGuiApplication::platformName()==QLatin1String("cocoa") ? NSApp.active
        : QGuiApplication::applicationState()==Qt::ApplicationActive;
}
class CocoaMenuTracking final : public NativeMenuTracking {
public:
    explicit CocoaMenuTracking(std::function<void()> started) {
        m_observer=[NSNotificationCenter.defaultCenter
            addObserverForName:NSMenuDidBeginTrackingNotification object:nil queue:nil
            usingBlock:^(NSNotification *note) {
                if (note.object!=NSApp.mainMenu) return;
                // Hiding the popout removes this observer during notification.
                // Retain the callable independently until it returns.
                auto callback=started;
                callback();
            }];
        m_departure=[NSNotificationCenter.defaultCenter
            addObserverForName:NSApplicationDidResignActiveNotification object:NSApp queue:nil
            usingBlock:^(NSNotification *) {auto callback=started;callback();}];
    }
    ~CocoaMenuTracking() override {
        [NSNotificationCenter.defaultCenter removeObserver:m_observer];
        [NSNotificationCenter.defaultCenter removeObserver:m_departure];
    }
private:
    id m_observer=nil;
    id m_departure=nil;
};
std::unique_ptr<NativeMenuTracking> observeNativeMenuTracking(std::function<void()> started) {
    return std::make_unique<CocoaMenuTracking>(std::move(started));
}
}
