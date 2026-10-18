// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "fixtures.h"
#include "windowinteraction.h"
#include "pagelayoutmenu.h"
#include <QEventLoop>
#include <QElapsedTimer>
void initializePlatformTests() { qputenv("QT_STYLE_OVERRIDE","Fusion"); }
void settlePlatformWindowState(QWidget &window)
{
    // Cocoa fullscreen moves the window between Spaces asynchronously. The
    // Windows fixture's single processEvents() cannot complete that animation.
    const bool full=window.isFullScreen();
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    QElapsedTimer clock; clock.start();
    while (clock.elapsed()<5000) {
        QEventLoop loop; QTimer::singleShot(20,&loop,&QEventLoop::quit); loop.exec();
        const bool nativeFull=(native.styleMask & NSWindowStyleMaskFullScreen)!=0;
        // Qt 5.15.2's property changes only at DidEnter/DidExit, after animation.
        const bool completed=[[native valueForKey:@"qt_fullScreen"] boolValue];
        if(nativeFull==full && completed==full) return;
    }
    check(false,"Cocoa fullscreen transition did not finish");
}
std::function<unsigned long(void *)> resolveBootstrap(QLibrary &helper) {
    auto start=reinterpret_cast<unsigned long (*)(void *)>(helper.resolve("UnboundStart"));
    return start ? std::function<unsigned long(void *)>(start) : std::function<unsigned long(void *)>();
}
QRect nativeWindowBounds(QWidget &window) { return window.frameGeometry(); }
void prepareNativePlacementTest(QWidget &,Unbound::PaneSettings &) {}
void verifyNativePlacementTest(QWidget &window,const QRect &expected) {
    check(window.frameGeometry()==expected,"macOS frame placement did not roundtrip");
}
bool runPlatformDiscoveryTests(QApplication &) { return false; }
class InteractionObserver : public QObject {
public:
    int starts=0;
    bool eventFilter(QObject *,QEvent *event) override {
        if(event->type()==Unbound::WindowInteractionEvent::eventType()) ++starts;
        return false;
    }
};
static bool runMacLayoutTest(QApplication &app) {
    if(!app.arguments().contains("--mac-layout-test")) return false;
    QTemporaryDir directory;
    Unbound::PaneSettings settings(directory.filePath("Unbound.json"));
    const QString page="WStackPage_Conform";
    QWidget main; main.setGeometry(150,150,600,400); main.show(); main.activateWindow();
    auto settle=[] { QEventLoop loop; QTimer::singleShot(30,&loop,&QEventLoop::quit); loop.exec(); };
    settle();
    Unbound::PageLayoutMenu menu(settings,page,&main,[] { return false; });
    menu.save=[&](QString &name,bool &duplicate) { return settings.saveLayout(page,name,duplicate); };
    menu.update=[&](const QString &id) { return settings.updateLayout(page,id); };
    menu.remove=[&](const QString &id) { return settings.deleteLayout(page,id); };
    menu.rename=[&](const QString &id,const QString &name) { return settings.renameLayout(page,id,name); };
    Unbound::UndockButton trigger(&main); trigger.setGeometry(20,10,24,24); trigger.show();
    trigger.contextMenu=[&](QWidget *button,const QPoint &) {
        menu.popup(button->mapToGlobal(QPoint(0,button->height())),button);
    };
    // Deliver native events only to this fixture's NSView. This exercises Cocoa's
    // press-triggered context menu and Qt's QWindow -> QWidget event translation.
    auto nativeMouse=[&](NSEventType type,QPoint point) {
        auto view=reinterpret_cast<NSView *>(main.winId());
        const auto location=[view convertPoint:NSMakePoint(point.x(),point.y()) toView:nil];
        static double timestamp=NSProcessInfo.processInfo.systemUptime;
        auto event=[NSEvent mouseEventWithType:type location:location modifierFlags:0
            timestamp:(timestamp+=1) windowNumber:view.window.windowNumber context:nil
            eventNumber:0 clickCount:1 pressure:1];
        if(type==NSEventTypeRightMouseDown) [view rightMouseDown:event];
        else if(type==NSEventTypeRightMouseUp) [view rightMouseUp:event];
        else if(type==NSEventTypeRightMouseDragged) [view rightMouseDragged:event];
        else if(type==NSEventTypeLeftMouseDown) [view mouseDown:event];
        else if(type==NSEventTypeLeftMouseUp) [view mouseUp:event];
        settle();
    };
    const auto origin=trigger.geometry().center();
    nativeMouse(NSEventTypeRightMouseDown,origin);
    check(menu.isVisible(),"Cocoa right press did not open layouts");
    nativeMouse(NSEventTypeRightMouseDragged,origin+QPoint(1,0));
    check(menu.isVisible(),"Opening right-button jitter dismissed layouts");
    nativeMouse(NSEventTypeRightMouseUp,origin);
    check(menu.isVisible(),"Opening right release over invoking button dismissed layouts");
    nativeMouse(NSEventTypeLeftMouseDown,QPoint(550,350));
    check(!menu.isVisible(),"Subsequent outside click did not dismiss layouts");
    nativeMouse(NSEventTypeLeftMouseUp,QPoint(550,350));
    nativeMouse(NSEventTypeRightMouseDown,origin);
    nativeMouse(NSEventTypeRightMouseDragged,QPoint(550,350));
    check(!menu.isVisible(),"Outside drag did not dismiss layouts");
    nativeMouse(NSEventTypeRightMouseUp,QPoint(550,350));
    nativeMouse(NSEventTypeRightMouseDown,origin);
    nativeMouse(NSEventTypeRightMouseUp,origin);
    check(menu.isVisible(),"Repeated context click did not leave layouts open");
    nativeMouse(NSEventTypeRightMouseDown,QPoint(550,350));
    check(!menu.isVisible(),"Later outside right press retained the opening exemption");
    nativeMouse(NSEventTypeRightMouseUp,QPoint(550,350));
    // A release-triggered or keyboard opening has no held mouse button. Its
    // normal outside-release dismissal must retain the Windows behavior.
    check(QApplication::mouseButtons()==Qt::NoButton,"Native fixture left a mouse button held");
    menu.popup(trigger.mapToGlobal(QPoint(0,trigger.height())),&trigger);
    QMouseEvent laterRelease(QEvent::MouseButtonRelease,trigger.rect().center(),
        trigger.mapToGlobal(trigger.rect().center()),Qt::RightButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(&trigger,&laterRelease);
    check(!menu.isVisible(),"Opening without held buttons suppressed an unrelated release");
    nativeMouse(NSEventTypeRightMouseDown,origin);
    nativeMouse(NSEventTypeRightMouseUp,origin);
    auto save=menu.findChild<QPushButton *>("unboundLayoutSave");
    check(save && save->text()=="Save as new Edit layout","Shared layout save control missing");
    check(!menu.findChild<QPushButton *>("unboundLayoutDockAll")->isVisible(),"Dock all visible without floats");
    save->click(); settle();
    check(menu.isVisible() && settings.layouts(page).size()==1,"Saving dismissed layout popout");
    const auto id=settings.activeLayout(page);
    auto load=static_cast<Unbound::LayoutNameButton *>(menu.findChild<QPushButton *>("unboundLayoutRecall"));
    check(load && load->property("activeLayout").toBool(),"Active layout not highlighted");
    save->click(); settle();
    check(settings.layouts(page).size()==1,"Identical layout duplicated");
    check(settings.saveFloating("frameEditPageMediaPool",true).isEmpty(),"Cannot change fixture state");
    // Cancelling inline rename refreshes current state without replacing rows.
    load->renameRequested(); settle();
    auto cancelled=static_cast<Unbound::LayoutNameEdit *>(menu.findChild<QLineEdit *>("unboundLayoutNameEdit"));
    check(cancelled && cancelled->isVisible(),"Inline rename did not open");
    cancelled->finish(false); settle();
    auto update=menu.findChild<QToolButton *>("unboundLayoutUpdate");
    check(update && update->isVisible() && static_cast<Unbound::LayoutNameButton *>(menu.findChild<QPushButton *>("unboundLayoutRecall"))==load,"Dirty update or retained row missing");
    update->click(); settle();
    check(menu.isVisible() && !settings.layoutDirty(page,id),"Layout update failed or dismissed popout");
    // Use the same long-press callback as the shared button's one-second timer.
    load->renameRequested(); settle();
    auto editor=static_cast<Unbound::LayoutNameEdit *>(menu.findChild<QLineEdit *>("unboundLayoutNameEdit"));
    check(editor && editor->isVisible() && editor->hasFocus(),"Inline editor lacks Cocoa keyboard focus");
    editor->setText("Mac layout");
    QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);
    QApplication::sendEvent(editor,&enter); settle();
    check(settings.layoutName(page,id)=="Mac layout" && settings.activeLayout(page)==id,"Rename changed stable identity or failed");
    check(menu.isVisible(),"Rename dismissed popout");
    main.setGeometry(main.geometry().translated(10,10)); settle();
    check(menu.isVisible(),"Programmatic placement dismissed popout");
    Unbound::WindowInteractionEvent::notify(&main); settle();
    check(!menu.isVisible(),"User window gesture did not dismiss popout");
    menu.popup(QPoint(180,180)); settle();
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
    QApplication::sendEvent(&menu,&escape); settle();
    check(!menu.isVisible(),"Escape did not dismiss popout");
    puts("PASS: Cocoa context press/release persistence, later outside click/drag dismissal, shared layouts, retained rows, rename/focus and Escape");
    return true;
}
class MainSnapReleaseWindow : public QWidget {
public:
    bool replayRelease=false;
    QPoint releasePosition;
    int releases=0;
    void mouseReleaseEvent(QMouseEvent *event) override {
        if(replayRelease && event->button()==Qt::LeftButton) {
            replayRelease=false; ++releases;
            // Resolve's custom title handler reapplies its raw position when
            // Qt eventually delivers the release, after the native monitor.
            move(releasePosition);
        }
        QWidget::mouseReleaseEvent(event);
    }
};
static bool runMacPlacementTest(QApplication &app) {
    if(!app.arguments().contains("--mac-placement-test")) return false;
    auto settle=[] { QEventLoop loop; QTimer::singleShot(60,&loop,&QEventLoop::quit); loop.exec(); };
    const auto area=QGuiApplication::primaryScreen()->availableGeometry();
    const CGFloat desktopTop=NSMaxY(NSScreen.screens.firstObject.frame);
    for(bool integratedTitle:{false,true}) {
        QWidget window;
        window.setGeometry(180,180,600,400); window.show(); settle();
        auto native=reinterpret_cast<NSView *>(window.winId()).window;
        if(integratedTitle) {
            native.styleMask|=NSWindowStyleMaskTitled|NSWindowStyleMaskFullSizeContentView|NSWindowStyleMaskResizable;
            native.titlebarAppearsTransparent=YES;
            native.titleVisibility=NSWindowTitleHidden;
            settle();
        }
        auto bounds=[&] {
            const auto r=native.frame;
            return QRect(qRound(r.origin.x),qRound(desktopTop-NSMaxY(r)),qRound(r.size.width),qRound(r.size.height));
        };
        auto place=[&](QRect frame) {
            [native setFrame:NSMakeRect(frame.x(),desktopTop-frame.y()-frame.height(),frame.width(),frame.height()) display:YES];
            settle();
        };
        auto save=[&] { return Unbound::Platform::saveWindowPlacement(&window,{}); };
        auto restore=[&](const QByteArray &bytes) {
            check(Unbound::Platform::restoreWindowPlacement(&window,Unbound::qtGeometryJson(bytes),{}),"Mac native placement was not handled");
            settle();
        };
        QRect expected(QPoint(),bounds().size());
        const QList<QPoint> positions{area.topLeft(),QPoint(area.right()-expected.width()+1,area.top()),
            QPoint(area.left(),area.bottom()-expected.height()+1),area.topLeft()+QPoint(120,100)};
        QByteArray normal;
        for(const auto point:positions) {
            expected.moveTopLeft(point); place(expected);
            normal=save();
            const auto record=Unbound::qtGeometryJson(normal);
            check(record.value("frameRect")==Unbound::rectangleJson(expected),"Native capture lost actual main frame");
            for(int repeat=0;repeat<3;++repeat) {
                place(expected.translated(80,80)); restore(normal);
                check(bounds()==expected,"Native frame drifted across repeated restores");
            }
        }
        // Test normal geometry beneath maximization, not just the zoomed state.
        expected.moveTopLeft(area.topLeft()+QPoint(120,0)); place(expected); normal=save();
        [native zoom:nil]; settle();
        check(window.isMaximized(),"Native fixture failed to maximize");
        const auto maximized=save();
        restore(normal); check(!window.isMaximized() && bounds()==expected,"Normal restore failed after maximizing");
        place(expected.translated(90,120)); restore(maximized);
        check(window.isMaximized(),"Native restore lost maximized state");
        // Use the user's Cocoa zoom action. Qt showNormal() rewrites styleMask
        // even when unchanged, losing Cocoa's user frame with FullSizeContentView.
        // That Qt behavior also occurs without the placement adapter.
        [native zoom:nil]; settle();
        check(!window.isMaximized() && bounds()==expected,"Maximized restore lost the saved normal frame");
        restore(maximized); restore(maximized);
        [native zoom:nil]; settle();
        check(!window.isMaximized() && bounds()==expected,"Repeated maximized restore lost the normal frame");
        // Legacy Windows placement must use its shared logical fallback.
        const QJsonObject windows{{"format","unbound-main-placement-1"},
            {"normal",QJsonArray{9000,9000,10000,10000}},
            {"qt",QString::fromLatin1(normal.toBase64())}};
        place(expected.translated(90,120));
        check(Unbound::Platform::restoreWindowPlacement(&window,windows,{}),"Windows fallback was rejected on Mac"); settle();
        check(bounds()==expected,"Windows workspace coordinates leaked into Cocoa placement");
        auto offscreen=Unbound::qtGeometryJson(normal);
        const auto oversized=Unbound::rectangleJson(QRect(100000,100000,8000,5000));
        offscreen.insert("clientRect",oversized); offscreen.insert("normalClientRect",oversized);
        offscreen.insert("screenWidth",10000); restore(Unbound::qtGeometryBytes(offscreen));
        check(area.contains(bounds()),"Disconnected/scaled monitor recovery left native main frame unreachable");
        window.setMinimumSize(700,450); restore(normal);
        check(window.width()>=700 && window.height()>=450,"Native restore ignored content minimum size");
        window.setMinimumSize(0,0); window.setMaximumSize(500,300); restore(normal);
        check(window.width()<=500 && window.height()<=300,"Native restore ignored content maximum size");
        window.setMaximumSize(QWIDGETSIZE_MAX,QWIDGETSIZE_MAX); restore(normal);
        const auto before=bounds();
        check(!Unbound::Platform::restoreWindowPlacement(&window,{},{}),"Malformed native placement was accepted");
        check(bounds()==before,"Malformed placement changed the main window");
    }
    // Resolve's macOS main window includes its title area in its client
    // rectangle. Exercise its integrated title style through shared tracking.
    // Qt restoreGeometry otherwise invents 20 px of external title clearance.
    QTemporaryDir directory;
    Unbound::PaneSettings edgeSettings(directory.filePath("edge.json"));
    QWidget edgeWindow;
    auto edgeLayout=new QVBoxLayout(&edgeWindow);
    auto edgePages=new QStackedWidget(&edgeWindow); edgePages->setObjectName("widgetStack_Panel");
    edgeLayout->addWidget(edgePages);
    auto edgeEdit=new QWidget; edgeEdit->setObjectName("WStackPage_Conform"); edgePages->addWidget(edgeEdit);
    auto edgeColor=new QWidget; edgeColor->setObjectName("WStackPage_Color"); edgePages->addWidget(edgeColor);
    const auto workArea=QGuiApplication::primaryScreen()->availableGeometry();
    const QRect snapped(workArea.topLeft()+QPoint(100,0),QSize(400,260));
    const auto other=snapped.translated(50,100);
    edgeWindow.setGeometry(snapped); edgeWindow.show(); settle();
    auto edgeNative=reinterpret_cast<NSView *>(edgeWindow.winId()).window;
    edgeNative.styleMask|=NSWindowStyleMaskFullSizeContentView;
    edgeNative.titlebarAppearsTransparent=YES;
    edgeNative.titleVisibility=NSWindowTitleHidden;
    edgeWindow.setGeometry(snapped); settle();
    {
        Unbound::MainWindowPlacement edgePlacement(edgeSettings); edgePlacement.attach(&edgeWindow);
        edgePages->setCurrentWidget(edgeColor); app.processEvents();
        edgeWindow.setGeometry(other); app.processEvents();
        for(int repeat=0;repeat<3;++repeat) {
            edgePages->setCurrentWidget(edgeEdit); app.processEvents();
            check(edgeWindow.geometry()==snapped,"Main page return added phantom title-bar clearance");
            edgePages->setCurrentWidget(edgeColor); app.processEvents();
            check(edgeWindow.geometry()==other,"Restoring edge-aligned main placement changed another page");
        }
        edgePages->setCurrentWidget(edgeEdit); app.processEvents(); edgePlacement.flush();
        QString name; bool duplicate=false;
        const auto page=edgeEdit->objectName();
        check(edgeSettings.saveLayout(page,name,duplicate).isEmpty() && !duplicate,"Cannot save edge layout");
        const auto id=edgeSettings.activeLayout(page);
        edgeWindow.setGeometry(other); app.processEvents(); edgePlacement.flush();
        check(edgeSettings.activateLayout(page,id).isEmpty(),"Cannot recall edge layout");
        edgePlacement.reloadCurrent(); app.processEvents();
        check(edgeWindow.geometry()==snapped,"Layout recall added phantom title-bar clearance");
        edgePlacement.flush();
    }
    edgeWindow.setGeometry(other); app.processEvents();
    Unbound::PaneSettings edgeReloaded(directory.filePath("edge.json"));
    Unbound::MainWindowPlacement edgeRestored(edgeReloaded); edgeRestored.attach(&edgeWindow);
    app.processEvents();
    check(edgeWindow.geometry()==snapped,"Reloading main placement added phantom title-bar clearance");
    puts("PASS: native Mac capture/restore, titled/integrated title edges, page/startup/layout placement, zoom normal frame, Windows fallback, monitor recovery and constraints");
    return true;
}
static bool runMacMainSnapTest(QApplication &app) {
    const bool integrated=app.arguments().contains("--mac-main-drag-test");
    if(!integrated && !app.arguments().contains("--mac-main-snap-test")) return false;
    MainSnapReleaseWindow window; window.setObjectName("UiMainWindow");
    window.setGeometry(180,180,320,160); window.show(); window.activateWindow();
    auto settle=[](int ms=100) { QEventLoop loop; QTimer::singleShot(ms,&loop,&QEventLoop::quit); loop.exec(); };
    settle();
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    if(integrated) {
        native.styleMask |= NSWindowStyleMaskFullSizeContentView;
        settle();
    }
    const auto initialStyle=native.styleMask;
    const BOOL initialMovable=native.movable;
    const auto area=window.screen()->availableGeometry();
    const auto size=window.frameGeometry().size();
    const CGFloat desktopTop=NSMaxY(NSScreen.screens.firstObject.frame);
    auto setFrame=[&](const QRect &frame) {
        [native setFrame:NSMakeRect(frame.x(),desktopTop-frame.y()-frame.height(),
            frame.width(),frame.height()) display:YES];
    };
    auto mouse=[&](NSEventType type,bool resize=false) {
        return [NSEvent mouseEventWithType:type
            location:NSMakePoint(native.frame.size.width/2,resize ? -2 : native.frame.size.height-16)
            modifierFlags:0 timestamp:NSProcessInfo.processInfo.systemUptime
            windowNumber:native.windowNumber context:nil eventNumber:0 clickCount:1
            pressure:type==NSEventTypeLeftMouseUp ? 0 : 1];
    };
    Unbound::MainWindowSnap snap; snap.setWindow(&window);
    check(native.movable==(integrated ? NO : initialMovable),
        "Integrated title bar retained a competing Cocoa drag, or ordinary title drag was disabled");
    check(native.styleMask==initialStyle,"Main drag ownership changed native resize/title controls");
    snap.setWindow(&window);
    check(native.movable==(integrated ? NO : initialMovable),"Repeated discovery lost main drag ownership");
    Unbound::PaneFloatingWindow peer;
    peer.setGeometry(area.x()+100,area.y()+80,200,350);
    struct Case { QRect target; QPoint direction; };
    QRect center(QPoint(),size); center.moveCenter(area.center());
    QRect left=center; left.moveLeft(area.left());
    QRect right=center; right.moveRight(area.right());
    QRect top=center; top.moveTop(area.top());
    QRect bottom=center; bottom.moveBottom(area.bottom());
    const QList<Case> cases{{left,{1,0}},{right,{-1,0}},{top,{0,1}},{bottom,{0,-1}},
        {QRect(QPoint(peer.geometry().right()+1,peer.geometry().top()+40),size),{1,0}}};
    for(int index=0;index<cases.size();++index) {
        if(index==cases.size()-1) { peer.show(); window.activateWindow(); settle(); }
        const auto scenario=cases[index];
        for(int delay:{0,70}) for(int gap:{20,10,5}) {
            setFrame(center); settle();
            const int expectedReleases=window.releases+1;
            [NSApp postEvent:mouse(NSEventTypeLeftMouseDown) atStart:NO];
            QTimer::singleShot(30,[&,scenario,delay,gap] {
                setFrame(scenario.target.translated(scenario.direction*gap));
                QTimer::singleShot(delay,[&,scenario,gap] {
                    // Local monitors see release before Cocoa completes its
                    // event. Model its final unsnapped frame in that same turn,
                    // both before and after our magnetic animation has settled.
                    auto up=mouse(NSEventTypeLeftMouseUp);
                    const auto timestamp=static_cast<ulong>(up.timestamp*1000);
                    [NSApp sendEvent:up];
                    setFrame(scenario.target.translated(scenario.direction*qMax(1,gap/2)));
                    QTimer::singleShot(0,[&,scenario,gap,timestamp] {
                        check(window.frameGeometry()==scenario.target,"Native snap completion did not precede queued Qt release");
                        QTimer::singleShot(0,[&,scenario,gap,timestamp] {
                            window.replayRelease=true;
                            window.releasePosition=scenario.target.topLeft()+scenario.direction*qMax(1,gap/2);
                            QMouseEvent release(QEvent::MouseButtonRelease,QPointF(100,10),
                                window.mapToGlobal(QPoint(100,10)),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
                            release.setTimestamp(timestamp);
                            QApplication::sendEvent(&window,&release);
                        });
                    });
                });
            });
            QElapsedTimer deadline; deadline.start();
            while(deadline.elapsed()<2000 && (window.releases<expectedReleases
                || window.frameGeometry()!=scenario.target)) settle(10);
            check(window.releases==expectedReleases,"Queued host release did not run");
            check(window.frameGeometry()==scenario.target,"Main snap jumped away after queued host release");
            settle();
            check(window.frameGeometry()==scenario.target,"Main snap drifted after queued geometry delivery");
        }
    }
    check(window.releases==cases.size()*6,"Host Qt release handler was bypassed");
    // A new gesture supersedes the preceding release's queued completion.
    peer.hide(); setFrame(center); settle();
    [NSApp postEvent:mouse(NSEventTypeLeftMouseDown) atStart:NO];
    QTimer::singleShot(30,[&] {
        setFrame(left.translated(20,0));
        [NSApp sendEvent:mouse(NSEventTypeLeftMouseUp)];
        // Use a resize press so these adjacent synthetic presses do not
        // invoke Cocoa's title-bar double-click zoom action.
        [NSApp sendEvent:mouse(NSEventTypeLeftMouseDown,true)];
        setFrame(center);
    });
    settle();
    check(window.frameGeometry()==center,"Previous release overrode a newer gesture");
    [NSApp sendEvent:mouse(NSEventTypeLeftMouseUp)]; settle();
    check(window.frameGeometry()==center,"Unsnapped main move changed on release");
    // A native-only release can leave no Qt content release to match. Its
    // recorded target must not affect unrelated input or later layout placement.
    ulong completionTimestamp=0;
    [NSApp postEvent:mouse(NSEventTypeLeftMouseDown) atStart:NO];
    QTimer::singleShot(30,[&] {
        setFrame(left.translated(20,0));
        auto up=mouse(NSEventTypeLeftMouseUp);
        completionTimestamp=static_cast<ulong>(up.timestamp*1000);
        [NSApp sendEvent:up];
    });
    settle(); setFrame(center); settle();
    auto qtRelease=[&](QWidget &receiver,ulong timestamp) {
        QMouseEvent event(QEvent::MouseButtonRelease,QPointF(100,10),
            receiver.mapToGlobal(QPoint(100,10)),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        event.setTimestamp(timestamp); QApplication::sendEvent(&receiver,&event);
    };
    check(completionTimestamp!=0,"Native-only release did not run");
    qtRelease(window,completionTimestamp+1); settle();
    check(window.frameGeometry()==center,"Unrelated Qt release replayed a snap");
    QWidget unrelated;
    qtRelease(unrelated,completionTimestamp); settle();
    check(window.frameGeometry()==center,"Another window's release replayed a snap");
    auto contentPress=[NSEvent mouseEventWithType:NSEventTypeLeftMouseDown
        location:NSMakePoint(native.frame.size.width/2,native.frame.size.height/2)
        modifierFlags:0 timestamp:NSProcessInfo.processInfo.systemUptime
        windowNumber:native.windowNumber context:nil eventNumber:0 clickCount:1 pressure:1];
    [NSApp sendEvent:contentPress];
    const auto restored=center.translated(70,0);
    setFrame(restored);
    qtRelease(window,completionTimestamp); settle();
    check(window.frameGeometry()==restored,"Content click/placement did not invalidate old release completion");
    [NSApp sendEvent:mouse(NSEventTypeLeftMouseUp)]; settle();
    snap.setWindow(nullptr);
    check(native.movable==initialMovable,"Detaching snapping did not restore original native movement");
    if(integrated) {
        native.movable=NO;
        snap.setWindow(&window); snap.setWindow(nullptr);
        check(!native.movable,"Detaching enabled movement that was originally disabled");
        native.movable=initialMovable;
    }
    puts("PASS: Cocoa and queued host Qt release snapping, early/settled release, repeated gaps, new gestures and unrelated input/placement isolation");
    return true;
}
static bool runMacFloatingResizeTest(QApplication &app) {
    if(!app.arguments().contains("--mac-floating-resize-test")) return false;
    auto settle=[] { QEventLoop loop; QTimer::singleShot(40,&loop,&QEventLoop::quit); loop.exec(); };
    Unbound::PaneFloatingWindow window;
    auto layout=new QVBoxLayout(&window);
    auto content=new QWidget; content->setMinimumSize(100,80); layout->addWidget(content);
    const QRect original(350,250,600,400);
    window.setGeometry(original); window.show(); window.activateWindow(); settle();
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    const CGFloat desktopTop=NSMaxY(NSScreen.screens.firstObject.frame);
    auto setFrame=[&](QRect frame) {
        [native setFrame:NSMakeRect(frame.x(),desktopTop-frame.y()-frame.height(),frame.width(),frame.height()) display:YES];
    };
    auto mouse=[&](NSEventType type,NSPoint point) {
        return [NSEvent mouseEventWithType:type location:point modifierFlags:0
            timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:native.windowNumber
            context:nil eventNumber:0 clickCount:1 pressure:type==NSEventTypeLeftMouseUp ? 0 : 1];
    };
    auto start=[&](NSPoint point) {
        [NSApp postEvent:mouse(NSEventTypeLeftMouseDown,point) atStart:NO]; settle();
        [NSNotificationCenter.defaultCenter postNotificationName:NSWindowWillStartLiveResizeNotification object:native];
        check(window.interactiveDragActive() && !layout->isEnabled(),"Native pane resize did not start/defer layout");
    };
    auto end=[&] {
        [NSNotificationCenter.defaultCenter postNotificationName:NSWindowDidEndLiveResizeNotification object:native];
        [NSApp postEvent:mouse(NSEventTypeLeftMouseUp,NSMakePoint(0,0)) atStart:NO]; settle();
        check(!window.interactiveDragActive() && layout->isEnabled(),"Native pane resize did not finish/restore layout");
    };
    InteractionObserver observer; window.installEventFilter(&observer);
    const QList<Qt::Edges> cases{Qt::LeftEdge,Qt::RightEdge,Qt::TopEdge,Qt::BottomEdge,
        Qt::LeftEdge|Qt::TopEdge,Qt::RightEdge|Qt::TopEdge,
        Qt::LeftEdge|Qt::BottomEdge,Qt::RightEdge|Qt::BottomEdge};
    int gestures=0;
    for(auto edges:cases) for(const auto inset:{QPoint(-2,-2),QPoint(1,10),QPoint(10,1),QPoint(8,8)}) {
        window.setGeometry(original); settle();
        check(observer.starts==gestures,"Programmatic pane placement started resizing");
        const auto contentSize=content->size();
        const auto point=NSMakePoint(edges.testFlag(Qt::LeftEdge) ? inset.x() :
            edges.testFlag(Qt::RightEdge) ? original.width()-inset.x() : original.width()/2,
            edges.testFlag(Qt::BottomEdge) ? inset.y() :
            edges.testFlag(Qt::TopEdge) ? original.height()-inset.y() : original.height()/2);
        start(point); ++gestures;
        check(observer.starts==gestures,"Native pane resize emitted duplicate start notifications");
        QRect expected=original;
        if(edges.testFlag(Qt::LeftEdge)) expected.setLeft(original.left()+70);
        if(edges.testFlag(Qt::RightEdge)) expected.setRight(original.right()-70);
        setFrame(expected); settle();
        if(edges.testFlag(Qt::TopEdge)) expected.setTop(original.top()+60);
        if(edges.testFlag(Qt::BottomEdge)) expected.setBottom(original.bottom()-60);
        setFrame(expected); settle();
        check(window.geometry()==expected,"Corner resize lost an axis or moved the opposite edge");
        check(content->size()==contentSize,"Pane contents relaid out during native resize");
        end();
        check(window.geometry()==expected,"Native pane resize reverted after release");
        check(content->size()!=contentSize,"Pane contents did not settle after native resize");
    }
    // Resizing must still snap both moving edges while leaving the anchor fixed.
    Unbound::PaneFloatingWindow peer; peer.setGeometry(1000,800,200,100); peer.show();
    window.setGeometry(original); window.activateWindow(); settle();
    start(NSMakePoint(original.width()-1,10));
    setFrame(QRect(350,250,640,540)); settle(); end();
    check(window.geometry()==QRect(350,250,650,550),"Native diagonal resize lost shared two-axis snapping");
    peer.hide();
    window.setGeometry(original); settle(); start(NSMakePoint(1,10));
    setFrame(original.adjusted(60,0,0,-50)); settle();
    // Exercise the host's explicit cancellation path without depending on
    // synthetic key-event acceptance by the desktop session.
    window.finishDeferredResize(false); settle(); end();
    check(window.geometry()==original,"Native pane resize cancellation lost the initial geometry");
    puts("PASS: native pane edges/corners including asymmetric insets, independent axes, fixed anchors, layout deferral, exact release, two-axis snapping and cancellation");
    return true;
}
static bool runMacMainResizeTest(QApplication &app) {
    const bool integrated=app.arguments().contains("--mac-main-drag-test");
    if(!integrated && !app.arguments().contains("--mac-main-resize-test")) return false;
    QWidget window; window.setObjectName("UiMainWindow");
    const QRect original(180,120,700,500);
    window.setGeometry(original); window.show(); window.activateWindow();
    auto settle=[] { QEventLoop loop; QTimer::singleShot(150,&loop,&QEventLoop::quit); loop.exec(); };
    settle();
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    if(integrated) { native.styleMask |= NSWindowStyleMaskFullSizeContentView; settle(); }
    Unbound::MainWindowSnap snap; snap.setWindow(&window);
    InteractionObserver observer; window.installEventFilter(&observer);
    auto mouse=[&](NSEventType type,NSPoint point) {
        return [NSEvent mouseEventWithType:type location:point modifierFlags:0
            timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:native.windowNumber
            context:nil eventNumber:0 clickCount:1 pressure:type==NSEventTypeLeftMouseUp ? 0 : 1];
    };
    const QList<Qt::Edges> edges{Qt::BottomEdge,Qt::TopEdge,Qt::LeftEdge,Qt::RightEdge,
        Qt::BottomEdge|Qt::LeftEdge,Qt::BottomEdge|Qt::RightEdge,
        Qt::TopEdge|Qt::LeftEdge,Qt::TopEdge|Qt::RightEdge};
    int gestures=0;
    for(auto edge:edges) {
        window.setGeometry(original); settle();
        check(observer.starts==gestures,"Programmatic main placement started a native gesture");
        const auto before=window.geometry(); const NSRect frame=native.frame;
        const NSPoint press=NSMakePoint(edge.testFlag(Qt::LeftEdge) ? -2 :
            edge.testFlag(Qt::RightEdge) ? frame.size.width+2 : frame.size.width/2,
            edge.testFlag(Qt::BottomEdge) ? -2 :
            edge.testFlag(Qt::TopEdge) ? frame.size.height+2 : frame.size.height/2);
        QRect expected=before;
        if(edge.testFlag(Qt::LeftEdge)) expected.setLeft(before.left()+80);
        if(edge.testFlag(Qt::RightEdge)) expected.setRight(before.right()-80);
        if(edge.testFlag(Qt::TopEdge)) expected.setTop(before.top()+100);
        if(edge.testFlag(Qt::BottomEdge)) expected.setBottom(before.bottom()-100);
        // Native queue monitors see an edge press outside the content rectangle.
        // Model Cocoa's accepted resize independently of the physical cursor:
        // notification delivery can precede Qt's geometry/pointer updates.
        [NSApp postEvent:mouse(NSEventTypeLeftMouseDown,press) atStart:NO];
        QTimer::singleShot(50,[&,frame,before,expected,press] {
            NSRect proposed=frame;
            proposed.origin.x+=expected.x()-before.x();
            proposed.origin.y-=expected.bottom()-before.bottom();
            proposed.size.width+=expected.width()-before.width();
            proposed.size.height+=expected.height()-before.height();
            [native setFrame:proposed display:YES];
            [NSNotificationCenter.defaultCenter postNotificationName:NSWindowDidEndLiveResizeNotification object:native];
            [NSApp postEvent:mouse(NSEventTypeLeftMouseUp,press) atStart:NO];
        });
        settle(); ++gestures;
        check(observer.starts==gestures,"Main resize edge did not start exactly one gesture");
        check(window.geometry()==expected,"Main resize restored its old size or moved the opposite edge");
        settle();
        check(window.geometry()==expected,"Main resize reverted after release or queued Qt events");
    }
    puts("PASS: Cocoa main-window shrinking on all edges/corners, native geometry precedence, fixed opposite edges and release stability");
    return true;
}
bool runPlatformWindowTests(QApplication &app) {
    if(runMacPlacementTest(app)) return true;
    if(runMacMainSnapTest(app)) {
        if(app.arguments().contains("--mac-main-drag-test")) runMacMainResizeTest(app);
        return true;
    }
    if(runMacMainResizeTest(app)) return true;
    if(runMacFloatingResizeTest(app)) return true;
    if(runMacLayoutTest(app)) return true;
    if(!app.arguments().contains("--mac-window-test")) return false;
    Unbound::PaneFloatingWindow window;
    window.setGeometry(120,120,320,240); window.show(); app.processEvents();
    auto native=reinterpret_cast<NSView *>(window.winId()).window;
    check(native && native.canBecomeKeyWindow,"Floating pane cannot receive keyboard focus");
    check(!(native.styleMask & NSWindowStyleMaskTitled),"Floating pane gained a title bar");
    check(!native.hidesOnDeactivate,"Floating pane hides when another application activates");
    InteractionObserver observer; window.installEventFilter(&observer);
    const auto original=window.geometry();
    window.setGeometry(original.translated(3,3)); app.processEvents();
    check(observer.starts==0,"Programmatic restore emitted a user gesture");
    window.setGeometry(original);
    check(window.beginButtonMove(),"Header move did not start");
    check(observer.starts==1 && window.interactiveDragActive(),"Header move notification/state missing");
    window.finishButtonMove(true);
    check(!window.interactiveDragActive() && window.geometry()==original,"Move cancellation changed geometry");
    window.hide();
    check(!window.beginButtonMove(),"Hidden window began a gesture");
    puts("PASS: Cocoa captionless keyboard-capable host, gesture notification, cancellation and programmatic isolation");
    return true;
}
