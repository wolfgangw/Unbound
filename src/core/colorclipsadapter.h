// SPDX-License-Identifier: MIT
#pragma once
#include "layoutreadiness.h"
#include <QApplication>
#include <QAbstractButton>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QLayout>
#include <functional>
#include <QMouseEvent>
#include <QPainter>
#include "colorclipstoolbar.h"
#include "windowpresentation.h"
#include "colorclipssidebars.h"
#include "colorplaybackpause.h"
#include "layoutrestorepolicy.h"

namespace Unbound {
// Separate nonactivating native surface: a child of the frozen page cannot
// repaint. Input passes through; native divider handling retains the mouse.
class ColorDividerGuide final : public QWidget {
public:
    explicit ColorDividerGuide(QWidget *owner) : QWidget(owner,Qt::Tool|Qt::FramelessWindowHint|
        Qt::WindowTransparentForInput|Qt::WindowDoesNotAcceptFocus) {
        setObjectName("unboundColorDividerGuide");
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
    }
protected:
    void paintEvent(QPaintEvent *) override {QPainter painter(this);painter.fillRect(rect(),QColor(180,180,180));}
};
// Lightbox supplies the native wrapping renderer. Retain its page allocation as
// an anchor, but display the viewer there while its grid lives in our host.
class ColorClipsAdapter final : public QObject {
    QPointer<QWidget> m_page,m_root,m_player,m_region,m_clips,m_timeline,m_divider;
    QPointer<QAbstractButton> m_timelineToggle;
    bool m_initializing=false;
    bool m_initializeTimelineToggle=false;
    LayoutRestorePolicy::ToggleState m_timelineToggleState;
    unsigned m_startGeneration=0;
    ColorClipsToolbar *m_toolbar=nullptr;
    ColorClipsSidebars m_sidebars;
    QPointer<QWidget> m_host,m_dragTarget,m_repaintWindow;
    QPointer<ColorDividerGuide> m_dragGuide;
    QMetaObject::Connection m_dragDestroyed;
    QPoint m_dragGlobal;
    quint64 m_dragGeneration=0;
    bool m_drag=false,m_nativePressed=false,m_mainUpdates=false,m_hostUpdates=false,m_dragPrepared=false;
    std::unique_ptr<WindowPresentationHold> m_presentation;
    QList<std::function<void()>> m_waitingInput;

    void finishDivider(bool resume=true) {
        if(!m_drag) return;
        ++m_dragGeneration;
        m_waitingInput.clear();
        disconnect(m_dragDestroyed);
        m_busy=true;
        // End only the native gesture we started; queued input from an aborted
        // gesture is discarded by generation below.
        if(m_nativePressed && m_dragTarget) {
            QMouseEvent release(QEvent::MouseButtonRelease,m_dragTarget->mapFromGlobal(m_dragGlobal),
                m_dragGlobal,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QCoreApplication::sendEvent(m_dragTarget,&release);
        }
        m_nativePressed=false;
        delete m_dragGuide.data();m_dragGuide=nullptr;
        if(m_dragPrepared)m_sidebars.captureDraggedWidths();
        m_drag=false;m_dragTarget=nullptr;
        m_suspended=false;
        if(resume && m_dragPrepared && m_enabled && m_page && m_page->isVisible() && m_mode) {
            if(!m_mode->isChecked()) m_mode->click();
            if(m_controls && !m_controls->isChecked()) m_controls->click();
            if(m_region) {m_nativeAllocation=m_region->geometry();m_appliedRegion=m_nativeAllocation;}
        } else if(resume && m_dragPrepared) m_returning=true;
        m_busy=false;
        if(resume) {apply();schedule();}
        if(m_repaintWindow && m_mainUpdates) m_repaintWindow->setUpdatesEnabled(true);
        m_repaintWindow=nullptr;
        if(m_host && m_hostUpdates) m_host->setUpdatesEnabled(true);
        m_mainUpdates=false;m_hostUpdates=false;
        if(m_presentation) {
            if(m_dragPrepared)m_presentation->release();
            m_presentation.reset();
        }
        m_dragPrepared=false;
    }
    bool dividerEvent(QObject *object,QEvent *event) {
        const auto type=event->type();
        if(m_drag && (type==QEvent::ApplicationDeactivate ||
            (object==m_page && type==QEvent::Hide))) finishDivider();
        if(m_drag && object==m_dragTarget && type==QEvent::UngrabMouse) {
            const auto generation=m_dragGeneration;
            QTimer::singleShot(0,this,[this,generation] {
                if(m_drag && generation==m_dragGeneration) finishDivider();
            });
        }
        if(!m_enabled || !m_page || !m_page->isVisible()) return false;
        if(!m_drag && !m_suspended && m_mode && m_mode->isChecked() && m_sidebars.isDivider(object)
            && type==QEvent::MouseButtonPress && static_cast<QMouseEvent *>(event)->button()==Qt::LeftButton) {
            m_dragTarget=qobject_cast<QWidget *>(object);
            if(!m_dragTarget) return false;
            m_drag=true;m_suspended=true;++m_dragGeneration;
            m_dragDestroyed=connect(m_dragTarget,&QObject::destroyed,this,[this] {finishDivider();});
            // Freeze the backing-store owner, not just its page: native mode
            // changes may otherwise repaint the ancestor over the frozen content.
            m_repaintWindow=m_page->window();
            m_dragGuide=new ColorDividerGuide(m_repaintWindow);
            const auto top=m_dragTarget->mapToGlobal(QPoint(m_dragTarget->width()/2,0));
            m_dragGuide->setGeometry(top.x()-1,top.y(),2,m_dragTarget->height());
            m_dragGuide->show();
            m_mainUpdates=m_repaintWindow->updatesEnabled();m_hostUpdates=m_host && m_host->updatesEnabled();
            if(m_mainUpdates)m_repaintWindow->setUpdatesEnabled(false);
            if(m_hostUpdates)m_host->setUpdatesEnabled(false);
            m_presentation=makeWindowPresentationHold();
            const auto generation=m_dragGeneration;
            QPointer<ColorClipsAdapter> alive(this);
            m_presentation->begin({m_repaintWindow,m_host},[alive,generation](bool ready) {
                if(!alive || !alive->m_drag || generation!=alive->m_dragGeneration)return;
                if(!ready) {
                    // Keep native Lightbox and existing allocations intact if
                    // the platform cannot protect their interim presentation.
                    QTimer::singleShot(0,alive,[alive,generation] {
                        if(alive && alive->m_drag && generation==alive->m_dragGeneration)alive->finishDivider(false);
                    });
                    return;
                }
                alive->m_dragPrepared=true;
                alive->m_busy=true;alive->m_mode->click();alive->m_busy=false;
                const auto waiting=std::move(alive->m_waitingInput);
                alive->m_waitingInput.clear();
                for(const auto &input:waiting)QTimer::singleShot(0,alive,input);
            });
        }
        if(!m_drag || object!=m_dragTarget || (type!=QEvent::MouseButtonPress &&
            type!=QEvent::MouseMove && type!=QEvent::MouseButtonRelease)) return false;
        auto mouse=static_cast<QMouseEvent *>(event);
        const auto global=mouse->globalPos();const auto button=mouse->button();
        const auto buttons=mouse->buttons();const auto modifiers=mouse->modifiers();
        const auto generation=m_dragGeneration;
        // Resolve must complete its queued normal-mode layout before the press.
        // Preserve ordering and map coordinates against the current native handle.
        auto deliver=[this,generation,type,global,button,buttons,modifiers] {
            if(!m_drag || generation!=m_dragGeneration || !m_dragTarget) return;
            m_dragGlobal=global;
            QMouseEvent copy(type,m_dragTarget->mapFromGlobal(global),global,button,buttons,modifiers);
            m_busy=true;QCoreApplication::sendEvent(m_dragTarget,&copy);m_busy=false;
            if(m_dragGuide && m_dragTarget) {
                const int x=m_dragTarget->mapToGlobal(QPoint(m_dragTarget->width()/2,0)).x();
                m_dragGuide->move(x-1,m_dragGuide->y());
            }
            if(type==QEvent::MouseButtonPress && button==Qt::LeftButton) m_nativePressed=true;
            if(type==QEvent::MouseButtonRelease && button==Qt::LeftButton) {
                m_nativePressed=false;finishDivider();
            }
        };
        if(m_dragPrepared)QTimer::singleShot(0,this,deliver);
        else m_waitingInput.append(std::move(deliver));
        return true;
    }
    QRect m_nativeAllocation,m_originalRegion,m_appliedRegion;
    QSize m_timelineMinimum,m_timelineMaximum;
    int m_timelineHeight=0;
    bool m_clipsHidden=false,m_timelineHidden=false,m_dividerHidden=false;
    QPointer<QAbstractButton> m_mode,m_controls;
    QList<QPointer<QWidget>> m_ancestors;
    QRect m_originalPlayer;
    bool m_playerHidden=false,m_regionHidden=false;
    bool m_originalMode=false,m_originalControls=false;
    bool m_enabled=false,m_busy=false,m_pending=false,m_restorePending=false,m_returning=false,m_suspended=false;
    LayoutPendingWork m_layoutWork,m_initializeWork,m_restoreWork;
    ColorPlaybackPause::Hold m_playbackHold;
    void finishPlaybackChange() {
        if(!m_initializing && !m_suspended && !m_restorePending && !m_drag)m_playbackHold.reset();
    }
    static QAbstractButton *uniqueButton(QWidget *root,const QString &name) {
        const auto buttons=root->findChildren<QAbstractButton *>(name);
        return buttons.size()==1 ? buttons.first() : nullptr;
    }
    void schedule() {
        if(m_pending) return;
        m_pending=true;
        m_layoutWork.set(m_page);
        QTimer::singleShot(0,this,[this] {
            m_pending=false;
            m_layoutWork.clear();
            if(m_restorePending) restoreMode();
            else {
                if(m_enabled && !m_suspended && m_returning && m_page && m_page->isVisible() && m_mode) {
                    m_returning=false; m_busy=true;
                    if(!m_mode->isChecked()) m_mode->click();
                    m_busy=false;
                }
                apply();
            }
        });
    }
    bool restoreControls() {
        if(!m_controls || !m_mode) return false;
        if(m_controls->isChecked()==m_originalControls) return true;
        // This action belongs to Lightbox. Outside that mode Resolve attempts
        // an implicit re-entry, which can be rejected during playback and leave
        // its toolbar inconsistent with the unchecked mode button.
        if(!m_mode->isChecked()) m_mode->click();
        if(!m_mode->isChecked()) return false;
        m_controls->click();
        return m_controls->isChecked()==m_originalControls;
    }
    void restoreMode() {
        if(!m_restorePending || !m_page || !m_page->isVisible() || !m_mode || !m_controls) return;
        if(m_mode->isChecked()!=m_originalMode || m_controls->isChecked()!=m_originalControls)
            prepareModeChange();
        m_busy=true;
        // Controls must be restored while their native Lightbox is still open.
        if(!restoreControls()) {m_busy=false;return;}
        if(m_mode->isChecked()!=m_originalMode) m_mode->click();
        m_restorePending=m_mode->isChecked()!=m_originalMode;
        if(!m_restorePending) m_restoreWork.clear();
        m_busy=false;
        finishPlaybackChange();
    }
public:
    void prepareModeChange() {
        m_playbackHold=ColorPlaybackPause::acquire(m_page);
    }
    std::function<void()> nativeDockRequested;
    ColorClipsAdapter(QWidget *page,QObject *owner) : QObject(owner),m_page(page) {
        if(!page) return;
        m_player=page->findChild<QWidget *>("m_pFramePlayerContainer");
        m_region=page->findChild<QWidget *>("m_pTimeLineFrame");
        m_root=m_player ? m_player->parentWidget() : nullptr;
        m_clips=page->findChild<QWidget *>("m_pLightBox");
        m_timeline=page->findChild<QWidget *>("timelineFrame");
        if(m_timeline && m_timeline->parentWidget()) m_divider=m_timeline->parentWidget()->findChild<QWidget *>("divider",Qt::FindDirectChildrenOnly);
        m_timelineToggle=uniqueButton(page->window(),"btnColorToggleTimeline");
        m_mode=uniqueButton(page->window(),"btnColorToggleLightbox");
        m_controls=uniqueButton(page->window(),"btnColorToggleColorControls");
        for(auto w=m_root.data();w;w=w->parentWidget()) {
            m_ancestors.append(w); if(w->isWindow()) break;
        }
        if(m_mode) connect(m_mode,&QAbstractButton::toggled,this,[this](bool on) {
            if(!m_enabled || m_busy || on || !m_page || !m_page->isVisible()) return;
            // A native Lightbox exit must not leave a floating one-row renderer.
            m_originalMode=false;
            QTimer::singleShot(0,this,[this] {
                if(m_enabled && m_mode && !m_mode->isChecked() && nativeDockRequested) nativeDockRequested();
            });
        });
        if(m_page) m_page->installEventFilter(this);
    }
    ~ColorClipsAdapter() override { stop(); if(qApp) qApp->removeEventFilter(this); }
    bool valid() const {
        return m_page && m_root && !m_root->layout() && m_player && m_region &&
            m_region->parentWidget()==m_root && m_mode && m_controls &&
            m_mode->isCheckable() && m_controls->isCheckable();
    }
    bool start(QWidget *host=nullptr) {
        m_host=host;
        if(m_enabled) return true;
        if(!valid() || !m_page->isVisible() || !m_mode->isEnabled() || !m_controls->isEnabled()) return false;
        if(m_restorePending) restoreMode();
        if(m_restorePending) return false; // Do not overwrite an unfinished native return.
        prepareModeChange();
        m_originalMode=m_mode->isChecked(); m_originalControls=m_controls->isChecked();
        m_originalPlayer=m_player->geometry(); m_originalRegion=m_region->geometry();
        m_playerHidden=m_player->isHidden(); m_regionHidden=m_region->isHidden();
        m_enabled=true; m_suspended=false; m_busy=true;
        // Capture ordinary page presentation even when undocking from Lightbox.
        // Use native handlers; never infer visibility from text or binary state.
        if(m_mode->isChecked()) m_mode->click();
        m_initializeTimelineToggle=m_timelineToggle && m_timelineToggle->isEnabled()
            && !m_timelineToggle->isChecked();
        if(m_initializeTimelineToggle) {
            m_timelineToggleState.begin(m_timelineToggle);
            m_timelineToggle->click();
        }
        // Exiting native Lightbox queues normal Timeline constraints. Capturing
        // its hidden geometry synchronously can retain a collapsed strip. Let
        // that native pass finish before entering Lightbox again.
        m_initializing=true;
        m_initializeWork.set(m_page);
        const auto generation=++m_startGeneration;
        QTimer::singleShot(0,this,[this,generation] {
            if(!m_enabled || generation!=m_startGeneration) return;
            completeStart();
        });
        m_busy=false;
        return true;
    }
    void completeStart() {
        if(!m_initializing || !m_enabled) return;
        if(!valid()) { stop(); return; }
        if(!m_page->isVisible()) return;
        m_busy=true;
        if(m_clips) m_clipsHidden=m_clips->isHidden();
        if(m_divider) m_dividerHidden=m_divider->isHidden();
        if(m_timeline) {
            m_timelineHidden=m_initializeTimelineToggle || m_timeline->isHidden();
            m_timelineMinimum=m_timeline->minimumSize(); m_timelineMaximum=m_timeline->maximumSize();
            m_timelineHeight=qBound(m_timelineMinimum.height(),m_timeline->height(),m_timelineMaximum.height());
        }
        if(m_initializeTimelineToggle && m_timelineToggle && m_timelineToggle->isChecked()) m_timelineToggle->click();
        m_timelineToggleState.end();
        m_initializeTimelineToggle=false;
        m_sidebars.capture(m_page,m_root,this,[this] {schedule();});
        m_toolbar=new ColorClipsToolbar(this);
        if(!m_toolbar->start(m_page)) {delete m_toolbar;m_toolbar=nullptr;}
        if(!m_suspended && !m_mode->isChecked()) m_mode->click();
        if(!m_suspended && !m_mode->isChecked())ColorPlaybackPause::cancelActive("clips_entry_rejected");
        if(!m_suspended && m_mode->isChecked() && !m_controls->isChecked()) m_controls->click();
        m_nativeAllocation=m_region->geometry();
        m_appliedRegion=m_nativeAllocation;
        m_initializing=false; m_busy=false;
        m_initializeWork.clear();
        qApp->installEventFilter(this);
        apply(); schedule();
        finishPlaybackChange();
    }
    void stop() {
        m_initializeWork.clear();m_layoutWork.clear();
        finishDivider(false);
        if(!m_enabled) return;
        ++m_startGeneration;
        if(m_initializing) {
            if(m_initializeTimelineToggle && m_timelineToggle && m_timelineToggle->isChecked()) m_timelineToggle->click();
            m_timelineToggleState.end();
            m_initializeTimelineToggle=false;
            m_initializing=false; m_enabled=false; m_suspended=false;
            m_restorePending=true; m_restoreWork.set(m_page); restoreMode();
            return;
        }
        m_enabled=false;m_suspended=false;
        qApp->removeEventFilter(this);
        m_busy=true;
        delete m_toolbar; m_toolbar=nullptr;
        m_sidebars.restore();
        if(m_player) { m_player->setGeometry(m_originalPlayer); m_player->setVisible(!m_playerHidden); }
        if(m_region) {m_region->setGeometry(m_originalRegion); m_region->setVisible(!m_regionHidden);}
        if(m_clips) m_clips->setVisible(!m_clipsHidden);
        if(m_timeline) {
            m_timeline->setMinimumSize(m_timelineMinimum); m_timeline->setMaximumSize(m_timelineMaximum);
            m_timeline->setVisible(!m_timelineHidden);
        }
        if(m_divider) m_divider->setVisible(!m_dividerHidden);
        m_busy=false;
        m_restorePending=true; m_restoreWork.set(m_page); restoreMode();
    }
    bool suspendNativeMode(bool suspend) {
        if(m_initializing) { m_suspended=suspend; return true; }
        // Routine restoration may ask to resume while native gesture layout is
        // still pending. The gesture owns that suspension until it finishes.
        if(m_drag && !suspend) return true;
        finishDivider(false);
        if(!m_enabled || !m_mode || !m_page || !m_page->isVisible() || m_suspended==suspend) return true;
        prepareModeChange();
        m_suspended=suspend; m_busy=true;
        // Suspension precedes docking during recall. Restore the outgoing
        // controls choice now, while its native Lightbox context still exists.
        if(suspend && !restoreControls()) {
            m_busy=false;m_suspended=false;
            ColorPlaybackPause::cancelActive("clips_controls_rejected");m_playbackHold.reset();return false;
        }
        if(m_mode->isChecked()==suspend) m_mode->click();
        if(m_mode->isChecked()==suspend) {
            m_busy=false;m_suspended=!suspend;
            ColorPlaybackPause::cancelActive("clips_mode_rejected");m_playbackHold.reset();return false;
        }
        // A rejected Lightbox entry must not be followed by a controls click.
        if(!suspend && m_mode->isChecked() && !m_controls->isChecked()) m_controls->click();
        m_busy=false;
        if(!suspend) { apply(); schedule(); }
        finishPlaybackChange();
        return true;
    }
    void returnToNormalMode() {
        // An explicit docked layout supersedes a deferred pre-float Lightbox
        // return. Keep any controls restoration, but never resurrect that mode
        // on a later page visit.
        if(m_enabled || !m_restorePending) return;
        m_originalMode=false;
        restoreMode();
    }
    void apply() {
        if(!m_enabled || m_initializing || m_suspended || m_busy || !valid() || !m_page->isVisible() || !m_mode->isChecked()) return;
        m_busy=true;
        const auto allocation=m_nativeAllocation;
        if(!allocation.isEmpty()) {
            // Keep the shared region for a docked Timeline; only its Clips slot
            // is empty. Native Lightbox removes Timeline's strip constraints.
            if(m_clips) m_clips->hide();
            if(m_divider) m_divider->hide();
            const bool timeline=m_timeline && m_timelineToggle && m_timelineToggle->isChecked();
            const int height=timeline ? qMin(m_timelineHeight,qMax(0,allocation.height()-100)) : 0;
            if(m_timeline) {
                m_timeline->setMinimumSize(m_timelineMinimum);
                m_timeline->setMaximumSize(m_timelineMaximum);
                m_timeline->setVisible(timeline);
            }
            auto viewer=allocation;
            if(height>0) {
                viewer.setHeight(qMax(0,allocation.height()-height));
                m_region->setGeometry(allocation.x(),viewer.bottom()+1,allocation.width(),height);
                m_region->show();
            } else m_region->hide();
            m_appliedRegion=m_region->geometry();
            if(timeline && m_timeline->layout() && m_timeline->layout()->count()==1) {
                auto child=m_timeline->layout()->itemAt(0)->widget();
                if(child && child->size()!=m_timeline->contentsRect().size()) {
                    m_timeline->layout()->invalidate(); m_timeline->layout()->activate();
                }
            }
            viewer=m_sidebars.apply(viewer);
            if(m_player->geometry()!=viewer) m_player->setGeometry(viewer);
            m_player->show();
        }
        m_busy=false;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_busy) return false;
        if(dividerEvent(object,event)) return true;
        const auto type=event->type();
        if(object==m_page && type==QEvent::Show) {
            m_returning=true;
            if(m_initializing) QTimer::singleShot(0,this,[this] { completeStart(); });
            schedule();
        }
        if(!m_enabled || m_suspended || !m_page || !m_page->isVisible()) return false;
        bool watched=m_sidebars.watches(object) || object==m_player || object==m_region || object==m_clips || object==m_timeline;
        for(auto w:m_ancestors) if(object==w) watched=true;
        if(!watched || (type!=QEvent::Resize && type!=QEvent::Move && type!=QEvent::Show &&
            type!=QEvent::Hide && type!=QEvent::LayoutRequest)) return false;
        if(object==m_region && (type==QEvent::Resize || type==QEvent::Move))
            m_nativeAllocation=m_region->geometry();
        // QWidget hide propagation continues after Hide delivery. Showing an
        // ancestor inside that event leaves its children implicitly invisible.
        if(type==QEvent::Hide || type==QEvent::Show || type==QEvent::Move) {schedule(); return false;}
        // Deliver the entire native pass once, then fix its allocation before
        // painting. Nested events are allowed through while this pass is busy.
        const bool updates=m_root && m_root->updatesEnabled();
        if(updates) m_root->setUpdatesEnabled(false);
        m_busy=true; QCoreApplication::sendEvent(object,event); m_busy=false;
        // A native ancestor pass can resize the region through nested events
        // while our reentrancy guard is set. Capture that allocation too.
        if(m_region && m_region->geometry()!=m_appliedRegion)
            m_nativeAllocation=m_region->geometry();
        apply();
        if(updates && m_root) m_root->setUpdatesEnabled(true);
        schedule(); return true;
    }
};
}
