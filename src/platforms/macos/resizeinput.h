// SPDX-License-Identifier: MIT
#pragma once
#import <AppKit/AppKit.h>

namespace Unbound {
// This only scopes modifier delivery; Cocoa still decides whether the press
// starts resizing and which edges move (including its larger corner handles).
inline bool macResizePress(NSEvent *event)
{
    NSWindow *window=event.window;
    if (event.type!=NSEventTypeLeftMouseDown || !window
        || !(window.styleMask & NSWindowStyleMaskResizable)
        || (window.styleMask & NSWindowStyleMaskFullScreen)) return false;
    const NSPoint p=event.locationInWindow;
    const NSSize size=window.frame.size;
    const CGFloat x=fmin(p.x,size.width-p.x),y=fmin(p.y,size.height-p.y);
    return x<=5 || y<=5 || (x<=12 && y<=12);
}
inline NSEvent *macResizeInput(NSEvent *event,bool resizing)
{
    if (!event || !resizing || !(event.modifierFlags & NSEventModifierFlagShift)) return event;
    switch (event.type) {
    case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseUp: case NSEventTypeFlagsChanged: break;
    default: return event;
    }
    // Reserve Shift for the shared grid instead of Cocoa's proportional resize.
    // Copy, never mutate the source event or physical modifier state: the grid
    // continues to read the real Shift state, including mid-gesture changes.
    CGEventRef source=event.CGEvent;
    if (!source) return event;
    CGEventRef copy=CGEventCreateCopy(source);
    if (!copy) return event;
    CGEventSetFlags(copy,CGEventGetFlags(copy)&~kCGEventFlagMaskShift);
    NSEvent *result=[NSEvent eventWithCGEvent:copy];
    CFRelease(copy);
    return result ?: event;
}
}
