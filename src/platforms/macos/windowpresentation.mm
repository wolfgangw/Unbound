// SPDX-License-Identifier: MIT
#include "windowpresentation.h"
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <objc/runtime.h>
#include <QApplication>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <vector>

// The image only covers presentation. Qt and Resolve still own input, native
// layout and renderer lifetime underneath it.
namespace Unbound {
namespace {
id presentationHitTest(id,SEL,NSPoint) { return nil; }
BOOL presentationAcceptsFocus(id,SEL) { return NO; }
Class presentationViewClass() {
    // Fixtures may link the backend and then load the helper. Register once per
    // process rather than emitting two Objective-C class definitions.
    Class type=objc_getClass("UnboundPresentationView");
    if(!type) {
        type=objc_allocateClassPair(NSView.class,"UnboundPresentationView",0);
        class_addMethod(type,@selector(hitTest:),reinterpret_cast<IMP>(presentationHitTest),
            method_getTypeEncoding(class_getInstanceMethod(NSView.class,@selector(hitTest:))));
        class_addMethod(type,@selector(acceptsFirstResponder),reinterpret_cast<IMP>(presentationAcceptsFocus),
            method_getTypeEncoding(class_getInstanceMethod(NSView.class,@selector(acceptsFirstResponder))));
        objc_registerClassPair(type);
    }
    return type;
}
struct Surface {
    QPointer<QWidget> widget;
    NSInteger number=0;
    NSRect windowFrame{}, contentFrame{}, bounds{};
    CGFloat scale=1;
    CGImageRef image=nullptr;
    NSView *view=nil;
    Surface() = default;
    Surface(const Surface &)=delete;
    ~Surface() { if(image)CGImageRelease(image); }
};
struct Presentation {
    bool cancelled=false;
    int pending=0;
    std::function<void(bool)> ready;
    std::vector<std::unique_ptr<Surface>> surfaces;
    void clear() {
        cancelled=true;ready={};
        for(auto &surface:surfaces)if(surface->view) {
            [surface->view removeFromSuperview];[surface->view release];surface->view=nil;
        }
    }
    void complete(bool success) {
        if(cancelled || !ready)return;
        if(!success) {
            auto callback=std::move(ready);clear();callback(false);return;
        }
        if(--pending)return;
        // Install all covers together; never enter native layout with only one
        // affected window protected. Reject a moved/reparented capture target.
        for(auto &surface:surfaces) {
            auto w=surface->widget.data();
            NSView *content=w && w->internalWinId()
                ? reinterpret_cast<NSView *>(w->internalWinId()).window.contentView:nil;
            if(!content || content.window.windowNumber!=surface->number ||
                !NSEqualRects(content.bounds,surface->bounds)) {complete(false);return;}
        }
        [CATransaction begin];[CATransaction setDisableActions:YES];
        for(auto &surface:surfaces) {
            NSView *content=reinterpret_cast<NSView *>(surface->widget->internalWinId()).window.contentView;
            surface->view=[[presentationViewClass() alloc]initWithFrame:content.bounds];
            surface->view.wantsLayer=YES;
            surface->view.layer.contents=(id)surface->image;
            surface->view.layer.contentsGravity=kCAGravityResize;
            [content addSubview:surface->view positioned:NSWindowAbove relativeTo:nil];
        }
        [CATransaction commit];
        auto callback=std::move(ready);callback(true);
    }
};
class CocoaPresentation final : public WindowPresentationHold {
    std::shared_ptr<Presentation> state;
public:
    ~CocoaPresentation() override {if(state)state->clear();}
    void begin(const QList<QWidget *> &windows,std::function<void(bool)> ready) override {
        if(state)state->clear();
        state=std::make_shared<Presentation>();
        auto current=state;current->ready=std::move(ready);
        for(auto w:windows) {
            if(!w || !w->isVisible())continue;
            NSView *content=w->internalWinId()
                ? reinterpret_cast<NSView *>(w->internalWinId()).window.contentView:nil;
            if(!content) {current->complete(false);return;}
            auto surface=std::make_unique<Surface>();surface->widget=w;
            surface->number=content.window.windowNumber;surface->windowFrame=content.window.frame;
            surface->contentFrame=[content.window convertRectToScreen:[content convertRect:content.bounds toView:nil]];
            surface->bounds=content.bounds;surface->scale=content.window.backingScaleFactor;
            current->surfaces.push_back(std::move(surface));
        }
        current->pending=int(current->surfaces.size());
        if(!current->pending) {current->complete(false);return;}
        if(@available(macOS 14.4,*)) {
            // This API restricts discovery to content the current process can
            // capture without TCC consent. Never request general screen access.
            [SCShareableContent getCurrentProcessShareableContentWithCompletionHandler:
                ^(SCShareableContent *content,NSError *error) {
                dispatch_async(dispatch_get_main_queue(),^{
                    if(current->cancelled)return;
                    if(error || !content) {current->complete(false);return;}
                    for(size_t index=0;index<current->surfaces.size();++index) {
                        auto &surface=*current->surfaces[index];SCWindow *match=nil;
                        for(SCWindow *window in content.windows)
                            if(window.windowID==surface.number) {match=window;break;}
                        if(!match) {current->complete(false);return;}
                        auto filter=[[SCContentFilter alloc]initWithDesktopIndependentWindow:match];
                        auto config=[[SCStreamConfiguration alloc]init];
                        config.width=NSUInteger(surface.windowFrame.size.width*surface.scale);
                        config.height=NSUInteger(surface.windowFrame.size.height*surface.scale);
                        config.showsCursor=NO;config.ignoreShadowsSingleWindow=YES;
                        [SCScreenshotManager captureImageWithFilter:filter configuration:config completionHandler:
                            ^(CGImageRef image,NSError *captureError) {
                            if(image)CGImageRetain(image);
                            dispatch_async(dispatch_get_main_queue(),^{
                                if(!current->cancelled && image && !captureError) {
                                    auto &target=*current->surfaces[index];
                                    const CGFloat sx=CGImageGetWidth(image)/target.windowFrame.size.width;
                                    const CGFloat sy=CGImageGetHeight(image)/target.windowFrame.size.height;
                                    const CGRect crop=CGRectMake(
                                        (NSMinX(target.contentFrame)-NSMinX(target.windowFrame))*sx,
                                        (NSMaxY(target.windowFrame)-NSMaxY(target.contentFrame))*sy,
                                        target.contentFrame.size.width*sx,target.contentFrame.size.height*sy);
                                    target.image=CGImageCreateWithImageInRect(image,crop);
                                    current->complete(target.image!=nullptr);
                                } else if(!current->cancelled)current->complete(false);
                                if(image)CGImageRelease(image);
                            });
                        }];
                        [filter release];[config release];
                    }
                });
            }];
        } else current->complete(false);
    }
    void release() override {
        auto current=std::move(state);if(!current)return;
        // Let the restored Qt layout and its queued backing-store update reach
        // Cocoa before revealing native surfaces. No persistent timer or stream.
        QTimer::singleShot(0,qApp,[current] {
            QTimer::singleShot(0,qApp,[current] {current->clear();});
        });
    }
};
}
std::unique_ptr<WindowPresentationHold> makeWindowPresentationHold()
{ return std::make_unique<CocoaPresentation>(); }
}
