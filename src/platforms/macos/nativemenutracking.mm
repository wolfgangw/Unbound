// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "nativemenutracking.h"
namespace Unbound {
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
    }
    ~CocoaMenuTracking() override {
        [NSNotificationCenter.defaultCenter removeObserver:m_observer];
    }
private:
    id m_observer=nil;
};
std::unique_ptr<NativeMenuTracking> observeNativeMenuTracking(std::function<void()> started) {
    return std::make_unique<CocoaMenuTracking>(std::move(started));
}
}
