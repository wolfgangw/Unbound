// SPDX-License-Identifier: MIT
#import <AppKit/AppKit.h>
#include "windowbackend.h"
#include "panesnap.h"
#include "geometryjson.h"
#include "panegeometryrestore.h"
#include "panewindowappearance.h"
#include "windowinteraction.h"
#include <QApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QEvent>
#include <QIcon>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QWindow>

namespace Unbound {
static NSWindow *nativeWindow(QWidget *widget)
{
    if (!widget || !widget->testAttribute(Qt::WA_WState_Created)) return nil;
    return reinterpret_cast<NSView *>(widget->effectiveWinId()).window;
}
// QWidget geometry and QCursor are logical, top-left screen coordinates.
// Cocoa frames are bottom-left points. Only native event hit-testing uses Cocoa;
// all snap proposals and constraints stay in Qt coordinates.
class MacWindowInteraction : public QObject {
public:
    MacWindowInteraction(QWidget *window, bool floating) : m_window(window), m_floating(floating)
    {
        if (!m_floating) {
            auto native=nativeWindow(m_window);
            if (native && (native.styleMask & NSWindowStyleMaskFullSizeContentView)) {
                // Resolve moves its integrated title bar through QWidget::move.
                // Cocoa's concurrent window-server drag can deliver competing
                // positions even outside snap zones. Leave movement to Resolve;
                // NSWindow resizing and our shared snap policy remain active.
                m_hostDragWindow=native;
                m_originalMovable=native.movable;
                native.movable=NO;
            }
        }
        m_clock.start();
        m_frame.setInterval(PaneSnapFrameIntervalMs); m_frame.setTimerType(Qt::PreciseTimer);
        connect(&m_frame, &QTimer::timeout, this, [this] { update(m_cursor); });
        qApp->installEventFilter(this);
        QPointer<MacWindowInteraction> alive(this);
        m_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:
            NSEventMaskLeftMouseDown | NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp | NSEventMaskKeyDown
            handler:^NSEvent *(NSEvent *event) {
                return alive ? alive->nativeEvent(event) : event;
            }];
        auto center = NSNotificationCenter.defaultCenter;
        m_beginResize = [center addObserverForName:NSWindowWillStartLiveResizeNotification object:nil queue:nil usingBlock:^(NSNotification *note) {
            if (!alive || !alive->m_floating || note.object != nativeWindow(alive->m_window) || alive->m_button) return;
            // Cocoa owns the cursor and its complete edge/corner hit regions.
            // Some native resize starts lie outside our thin frame press band.
            if (!alive->m_active) alive->begin({},false,true);
            else alive->m_nativeResize=true;
            if (alive->m_active && alive->m_floating) alive->deferLayout();
        }];
        m_move = [center addObserverForName:NSWindowDidMoveNotification object:nil queue:nil usingBlock:^(NSNotification *note) {
            if (alive && note.object == nativeWindow(alive->m_window) && alive->followsNativeFrame() && alive->m_active && !alive->m_applying)
                alive->updateNativeGeometry();
        }];
        m_resize = [center addObserverForName:NSWindowDidResizeNotification object:nil queue:nil usingBlock:^(NSNotification *note) {
            if (alive && note.object == nativeWindow(alive->m_window) && alive->followsNativeFrame() && alive->m_active && !alive->m_applying)
                alive->updateNativeGeometry();
        }];
        m_end = [center addObserverForName:NSWindowDidEndLiveResizeNotification object:nil queue:nil usingBlock:^(NSNotification *note) {
            if (alive && note.object == nativeWindow(alive->m_window) && alive->followsNativeFrame()) alive->finish(false);
        }];
    }
    ~MacWindowInteraction() override
    {
        finish(false);
        if (m_hostDragWindow && nativeWindow(m_window)==m_hostDragWindow)
            m_hostDragWindow.movable=m_originalMovable;
        if (qApp) qApp->removeEventFilter(this);
        if (m_monitor) [NSEvent removeMonitor:m_monitor];
        for (id observer : {m_beginResize, m_move, m_resize, m_end}) if (observer) [NSNotificationCenter.defaultCenter removeObserver:observer];
    }
    bool active() const { return m_active; }
    bool begin(Qt::Edges edges, bool button = false, bool nativeResize = false)
    {
        if (!m_window || !m_window->isVisible() || m_active || m_window->isFullScreen()) return false;
        ++m_gesture;
        m_releasePending=false;
        WindowInteractionEvent::notify(m_window);
        if (m_window->isMaximized()) m_window->showNormal();
        m_edges = edges; m_button = button; m_nativeResize = nativeResize; m_active = true;
        m_window->activateWindow();
        m_start = m_window->geometry(); m_startCursor = m_cursor = QCursor::pos();
        m_nativeProposal = m_target = m_start;
        m_margins = QMargins(m_start.left()-m_window->frameGeometry().left(), m_start.top()-m_window->frameGeometry().top(),
            m_window->frameGeometry().right()-m_start.right(), m_window->frameGeometry().bottom()-m_start.bottom());
        m_minimum = m_window->minimumSize().expandedTo(m_window->minimumSizeHint()).expandedTo(QSize(1,1));
        m_maximum = m_window->maximumSize().expandedTo(m_minimum);
        m_targets.clear(); m_areas.clear(); m_transition.reset();
        for (auto peer : QApplication::topLevelWidgets())
            if (peer != m_window && peer->isVisible() && !peer->isMinimized()
                && (peer->property("unboundFloatingWindow").toBool() || peer->objectName() == "UiMainWindow"))
                m_targets.append(peer->frameGeometry());
        for (auto screen : QGuiApplication::screens()) m_areas.append(screen->availableGeometry());
        if (m_floating && (edges || nativeResize)) deferLayout();
        return true;
    }
    void deferLayout()
    {
        if (m_window && !m_layout && m_window->layout() && m_window->layout()->isEnabled()) {
            m_layout = m_window->layout(); m_layout->setEnabled(false);
        }
    }
    void update(QPoint cursor, bool settle = false)
    {
        if (!m_active || !m_window || m_applying) return;
        m_cursor = cursor;
        const QPoint delta = cursor - m_startCursor;
        QRect desired = followsNativeFrame() ? m_nativeProposal : m_start;
        const bool moving = !m_edges && !m_nativeResize;
        const bool left = moving || m_edges.testFlag(Qt::LeftEdge), right = moving || m_edges.testFlag(Qt::RightEdge);
        const bool top = moving || m_edges.testFlag(Qt::TopEdge), bottom = moving || m_edges.testFlag(Qt::BottomEdge);
        if (!followsNativeFrame() && moving) desired.translate(delta);
        QRect frame = desired.marginsAdded(m_margins);
        QPoint correction, engaged;
        const bool bypass = ([NSEvent modifierFlags] & NSEventModifierFlagOption) != 0;
        if (!bypass) correction = paneSnapOffset(frame, m_targets, PaneSnapDistance, left, right, top, bottom,
            m_minimum+QSize(m_margins.left()+m_margins.right(),m_margins.top()+m_margins.bottom()),
            m_maximum+QSize(m_margins.left()+m_margins.right(),m_margins.top()+m_margins.bottom()),m_areas,&engaged);
        if (bypass) { m_transition.reset(); m_frame.stop(); }
        else if (!settle) {
            correction = m_transition.update(correction,engaged,m_clock.elapsed());
            if (m_transition.animating()) { if (!m_frame.isActive()) m_frame.start(); }
            else m_frame.stop();
        }
        if (moving) frame.translate(correction);
        else {
            if (left) frame.setLeft(frame.left()+correction.x());
            if (right) frame.setRight(frame.right()+correction.x());
            if (top) frame.setTop(frame.top()+correction.y());
            if (bottom) frame.setBottom(frame.bottom()+correction.y());
        }
        m_target = frame.marginsRemoved(m_margins);
        applyGeometry(m_target);
    }
    void finish(bool cancel)
    {
        if (m_active && m_window) {
            if (cancel) applyGeometry(m_start);
            else {
                update(m_cursor,true);
                if (followsNativeFrame()) {
                    // The local mouse-up monitor runs before Cocoa finishes its
                    // native gesture. Cocoa may subsequently publish its final
                    // unsnapped frame. Commit the exact target once more after
                    // that event has returned, without recomputing it from the
                    // native frame or an intermediate animation position.
                    queueFinalCommit(m_target);
                }
            }
        }
        m_active = m_button = m_nativeResize = false; m_frame.stop();
        if (m_layout) { auto layout=m_layout.data(); m_layout=nullptr; layout->setEnabled(true); layout->invalidate(); layout->activate(); }
    }
protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type()==QEvent::ApplicationDeactivate
            || (object==m_window && (event->type()==QEvent::Hide || event->type()==QEvent::Destroy))) {
            ++m_gesture; m_releasePending=false;
            if (m_active) finish(true);
        }
        if (m_releasePending && m_window && event->type()==QEvent::MouseButtonRelease) {
            const auto mouse=static_cast<QMouseEvent *>(event);
            const auto widget=qobject_cast<QWidget *>(object);
            if (mouse->button()==Qt::LeftButton && mouse->timestamp()==m_releaseTimestamp
                && (object==m_window->windowHandle() || (widget && widget->window()==m_window))) {
                // Resolve's custom title bar receives Qt's queued release after
                // Cocoa's monitor and can move back to its unsnapped position.
                // Let that handler run normally, then commit this gesture's
                // saved target. Cocoa and Qt 5.15 use the same timestamp in ms;
                // matching it excludes unrelated later releases and restores.
                m_releasePending=false;
                queueFinalCommit(m_releaseTarget);
            }
        }
        return false;
    }
private:
    bool followsNativeFrame() const { return !m_floating || m_nativeResize; }
    void queueFinalCommit(const QRect &target)
    {
        const auto gesture=m_gesture;
        QTimer::singleShot(0,this,[this,target,gesture] {
            if (m_window && !m_active && gesture==m_gesture && m_window->isVisible()
                && !m_window->isMaximized() && !m_window->isFullScreen()) applyGeometry(target);
        });
    }
    void applyGeometry(const QRect &target)
    {
        m_applying=true;
        if (m_floating && m_button) {
            if (target!=m_window->geometry()) m_window->setGeometry(target);
        } else if (auto native=nativeWindow(m_window)) {
            // Qt's cached geometry can still describe our previous correction
            // while Cocoa is delivering a new native frame. Compare and apply
            // in native coordinates so that cache cannot suppress a correction.
            const auto contact=target.marginsAdded(m_margins);
            const CGFloat desktopTop=NSMaxY(NSScreen.screens.firstObject.frame);
            const NSRect frame=NSMakeRect(contact.x(),desktopTop-contact.y()-contact.height(),
                contact.width(),contact.height());
            if (!NSEqualRects(native.frame,frame)) [native setFrame:frame display:YES];
        }
        m_applying=false;
    }
    void updateNativeGeometry()
    {
        auto native = nativeWindow(m_window);
        if (!native || !m_active || m_applying) return;
        // Cocoa owns frame resizing and main-window movement and constraints.
        // Read its proposed frame directly: Qt geometry/cursor delivery can lag
        // the native notification. Keep this proposal separate from our snap
        // corrections so animation/release cannot feed an adjusted frame back in.
        const NSRect frame = native.frame;
        const CGFloat desktopTop = NSMaxY(NSScreen.screens.firstObject.frame);
        const QRect contact(qRound(frame.origin.x), qRound(desktopTop-NSMaxY(frame)),
            qRound(frame.size.width), qRound(frame.size.height));
        m_nativeProposal = contact.marginsRemoved(m_margins);
        if (m_nativeResize) {
            // Infer the actual moving edges from Cocoa's proposal, not a second
            // hit test that can disagree with its diagonal resize cursor. Keep
            // edges already observed if one axis pauses or returns to its start.
            if (m_nativeProposal.left()!=m_start.left()) m_edges|=Qt::LeftEdge;
            if (m_nativeProposal.right()!=m_start.right()) m_edges|=Qt::RightEdge;
            if (m_nativeProposal.top()!=m_start.top()) m_edges|=Qt::TopEdge;
            if (m_nativeProposal.bottom()!=m_start.bottom()) m_edges|=Qt::BottomEdge;
        }
        update(QCursor::pos());
    }
    NSEvent *nativeEvent(NSEvent *event)
    {
        if (!m_window) return event;
        if (event.type==NSEventTypeLeftMouseDown) {
            // Even a content click must invalidate the preceding gesture's
            // completion; it may trigger a layout or another placement change.
            ++m_gesture; m_releasePending=false;
        }
        if (m_active && event.type == NSEventTypeKeyDown && event.keyCode == 53) { finish(true); return nil; }
        if (m_active && !m_button && event.type == NSEventTypeLeftMouseDragged) {
            if (!followsNativeFrame()) { update(QCursor::pos()); return nil; }
        }
        if (m_active && !m_button && event.type == NSEventTypeLeftMouseUp) {
            const bool consumed=!followsNativeFrame(); m_cursor=QCursor::pos(); finish(false);
            if (!m_floating) {
                m_releasePending=true;
                m_releaseTimestamp=static_cast<ulong>(event.timestamp*1000);
                m_releaseTarget=m_target;
            }
            return consumed ? nil : event;
        }
        if (event.type != NSEventTypeLeftMouseDown || event.window != nativeWindow(m_window) || m_active) return event;
        const NSRect frame=event.window.frame;
        const NSPoint p=event.locationInWindow;
        const CGFloat width=frame.size.width, height=frame.size.height;
        Qt::Edges edges;
        constexpr CGFloat border=5;
        if (p.x < border) edges |= Qt::LeftEdge;
        if (p.x >= width-border) edges |= Qt::RightEdge;
        if (p.y < border) edges |= Qt::BottomEdge;
        if (p.y >= height-border) edges |= Qt::TopEdge;
        if (m_floating) {
            // Keep the press in Cocoa's native resize path. Unbound observes
            // frame proposals for snapping and defers content layout until end.
            if (edges) begin({},false,true);
        } else {
            // Cocoa resize presses can lie just outside the content rectangle,
            // including below its bottom edge. Classify those edges before the
            // title bar, or our move handler restores the original window size.
            const bool title = !edges && !NSPointInRect(p,event.window.contentLayoutRect);
            if (title || edges) begin(edges);
        }
        return event;
    }
    QPointer<QWidget> m_window;
    QPointer<QLayout> m_layout;
    bool m_floating=false, m_active=false, m_button=false, m_applying=false, m_nativeResize=false;
    Qt::Edges m_edges;
    QPoint m_startCursor, m_cursor;
    QRect m_start, m_nativeProposal, m_target;
    quint64 m_gesture=0;
    bool m_releasePending=false;
    ulong m_releaseTimestamp=0;
    QRect m_releaseTarget;
    QMargins m_margins;
    QSize m_minimum, m_maximum;
    QVector<QRect> m_targets, m_areas;
    PaneSnapTransition m_transition;
    QElapsedTimer m_clock;
    QTimer m_frame;
    id m_monitor=nil, m_beginResize=nil, m_move=nil, m_resize=nil, m_end=nil;
    NSWindow *m_hostDragWindow=nil; // Identity only; Resolve owns the window.
    BOOL m_originalMovable=YES;
};
PaneFloatingWindow::PaneFloatingWindow() : QWidget(nullptr,Qt::Window | Qt::FramelessWindowHint),
    m_interaction(new MacWindowInteraction(this,true))
{
    setAttribute(Qt::WA_QuitOnClose,false);
    setAttribute(Qt::WA_ShowWithoutActivating,true);
    setProperty("unboundFloatingWindow",true);
}
PaneFloatingWindow::~PaneFloatingWindow() = default;
bool PaneFloatingWindow::restoreGeometry(const QByteArray &bytes)
{
    const bool restored=QWidget::restoreGeometry(bytes);
    if (restored) restoreReachablePaneClientGeometry(this,bytes);
    return restored;
}
bool PaneFloatingWindow::beginButtonMove() { return m_interaction->begin({},true); }
void PaneFloatingWindow::updateButtonMove() { m_interaction->update(QCursor::pos()); }
void PaneFloatingWindow::finishButtonMove(bool cancel) { m_interaction->finish(cancel); }
bool PaneFloatingWindow::interactiveDragActive() const { return m_interaction->active(); }
void PaneFloatingWindow::beginDeferredResize() { m_interaction->deferLayout(); }
void PaneFloatingWindow::finishDeferredResize(bool settle) { m_interaction->finish(!settle); }
bool PaneFloatingWindow::event(QEvent *event)
{
    const bool result=QWidget::event(event);
    if (event->type()==QEvent::Show || event->type()==QEvent::WinIdChange) applyPaneDarkCaption(this,nullptr);
    return result;
}
void PaneFloatingWindow::paintEvent(QPaintEvent *) { QPainter p(this); p.fillRect(rect(),paneWindowBackground()); }
bool applyPaneDarkCaption(QWidget *host,QWidget *)
{
    if (QGuiApplication::platformName()!=QStringLiteral("cocoa")) return false;
    host->winId(); NSWindow *window=nativeWindow(host); if (!window) return false;
    window.appearance=[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua];
    window.backgroundColor=[NSColor colorWithWhite:PaneWindowBackgroundLevel/255.0 alpha:1];
    window.hasShadow=YES;
    window.styleMask |= NSWindowStyleMaskResizable;
    window.hidesOnDeactivate=NO;
    window.level=NSNormalWindowLevel;
    window.movableByWindowBackground=NO;
    return true;
}
void inheritPaneTaskbarIdentity(QWidget *host,QWidget *mainWindow) { host->setWindowIcon(mainWindow->windowIcon()); }
MainWindowSnap::MainWindowSnap() = default;
MainWindowSnap::~MainWindowSnap() = default;
void MainWindowSnap::setWindow(QWidget *window)
{
    // The controller calls this repeatedly during discovery. Keep the observer.
    if (m_interaction && m_interaction->property("window").value<QObject *>()==window) return;
    m_interaction.reset();
    if (window) { m_interaction.reset(new MacWindowInteraction(window,false)); m_interaction->setProperty("window",QVariant::fromValue<QObject *>(window)); }
}
WindowGroupActivation::WindowGroupActivation() { qApp->installEventFilter(this); }
WindowGroupActivation::~WindowGroupActivation() { if (qApp) qApp->removeEventFilter(this); }
bool WindowGroupActivation::eventFilter(QObject *object,QEvent *event)
{
    if (event->type()!=QEvent::WindowActivate || m_raising) return false;
    auto window=qobject_cast<QWidget *>(object);
    if (!m_main || !window || (window!=m_main && !window->property("unboundFloatingWindow").toBool())) return false;
    m_selected=window;
    if (!m_pending) { m_pending=true; QTimer::singleShot(0,this,[this] { m_pending=false; raiseGroup(); }); }
    return false;
}
void WindowGroupActivation::raiseGroup()
{
    auto selected=m_selected.data(); NSWindow *key=nativeWindow(selected);
    if (!m_main || !selected || !key || NSApp.keyWindow!=key || !NSApp.active
        || QApplication::activeModalWidget() || QApplication::activePopupWidget()) return;
    QList<NSWindow *> peers;
    for (NSWindow *window in NSApp.orderedWindows)
        for (auto widget : QApplication::topLevelWidgets())
            if (widget!=selected && (widget==m_main || widget->property("unboundFloatingWindow").toBool())
                && widget->isVisible() && !widget->isMinimized() && nativeWindow(widget)==window) peers.append(window);
    m_raising=true; NSInteger above=key.windowNumber;
    for (NSWindow *window : peers) { [window orderWindow:NSWindowBelow relativeTo:above]; above=window.windowNumber; }
    m_raising=false;
}
}
