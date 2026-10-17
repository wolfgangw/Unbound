// SPDX-License-Identifier: MIT
#pragma once
#include <QEvent>
#include <QPointer>
#include <QWidget>
#include <QResizeEvent>
#include <QApplication>
#include <functional>

// Application filters run before QLayout::widgetEvent; widget filters run after
// it. Inspect only the resize events of this adapter's retained widgets.
class ViewerResizeGate final : public QObject {
public:
    using QObject::QObject;
    std::function<bool(QObject *,QEvent *)> resize;
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        return event->type()==QEvent::Resize && resize && resize(object,event);
    }
};

// Adapt Resolve's explicitly positioned Edit regions. Native layout computes
// baselines normally; one queued adjustment follows each native layout burst.
// No native anchor fields, split ratios, minimum sizes or timers are changed.
class ViewerGeometryAdapter final : public QObject
{
    struct Placement {
        QPointer<QWidget> widget;
        QRect geometry;
        bool hidden=false;
    };
    QPointer<QWidget> m_root,m_page;
    QList<QPointer<QWidget>> m_resizeAncestors;
    Placement m_viewers,m_timeline;
    QList<Placement> m_splitters;
    QList<Placement> m_sideRegions;
    bool m_enabled=false,m_paused=false,m_applying=false,m_pending=false;
    bool m_deliveringTimeline=false;
    QSize m_deliveredTimelineSize;
    ViewerResizeGate *m_resizeGate=nullptr;
    bool m_forwardingNativeResize=false;
    static void capture(Placement &p) {
        if (p.widget) { p.geometry=p.widget->geometry(); p.hidden=p.widget->isHidden(); }
    }
    bool valid() const {
        return m_root && !m_root->layout() && m_viewers.widget && m_timeline.widget &&
            m_viewers.widget->parentWidget()==m_root && m_timeline.widget->parentWidget()==m_root;
    }
    void apply() {
            if (!m_enabled || m_paused || !valid()) return;
            const QRect native=m_timeline.geometry;
            const int top=m_viewers.geometry.top();
            if (native.isEmpty() || native.top()<top) return;
            m_applying=true;
            // Keep the viewer container explicitly shown: the floating hosts
            // retain native ancestry through it. Hiding it can hide owned hosts.
            auto viewerRect=m_viewers.geometry; viewerRect.setHeight(0);
            if (m_viewers.widget->geometry()!=viewerRect) m_viewers.widget->setGeometry(viewerRect);
            // Timeline normally extends underneath upper side columns. While
            // expanded, use the horizontal intersection with the viewer region
            // so Inspector/Media Pool cannot be painted over by the timeline.
            const int left=qMax(native.left(),m_viewers.geometry.left());
            const int right=qMin(native.right(),m_viewers.geometry.right());
            const QRect expanded(left,top,qMax(0,right-left+1),native.bottom()-top+1);
            if (m_timeline.widget->geometry()!=expanded) m_timeline.widget->setGeometry(expanded);
            for (auto &p:m_sideRegions) if (p.widget && !p.hidden && !p.geometry.isEmpty()) {
                auto side=p.geometry;
                const bool beside=side.right()<m_viewers.geometry.left() || side.left()>m_viewers.geometry.right();
                // Extend a column that occupied the whole original upper region.
                // Partial/stacked side regions retain their native geometry.
                if (beside && side.top()==top && qAbs(side.bottom()-m_viewers.geometry.bottom())<=1) {
                    side.setBottom(qMax(side.bottom(),expanded.bottom()));
                    if (p.widget->geometry()!=side) p.widget->setGeometry(side);
                }
            }
            for (auto &p:m_splitters) if (p.widget && !p.widget->isHidden()) p.widget->hide();
            m_applying=false;
    }
    void schedule() {
        if (!m_enabled || m_paused || m_pending) return;
        m_pending=true;
        QMetaObject::invokeMethod(this,[this] {
            m_pending=false; apply();
        },Qt::QueuedConnection);
    }
    void restore() {
        m_applying=true;
        auto put=[this](const Placement &p) {
            if (!p.widget || p.widget->parentWidget()!=m_root) return;
            p.widget->setGeometry(p.geometry); p.widget->setVisible(!p.hidden);
        };
        put(m_viewers); put(m_timeline);
        if(m_timeline.widget && m_timeline.widget->size()!=m_deliveredTimelineSize) {
            QResizeEvent pending(m_timeline.widget->size(),m_deliveredTimelineSize);
            QCoreApplication::sendEvent(m_timeline.widget,&pending);
        }
        for (const auto &p:m_splitters) put(p);
        for (const auto &p:m_sideRegions) put(p);
        m_applying=false;
    }
    bool resizeBeforeLayout(QObject *object,QEvent *event) {
        if(object!=m_timeline.widget) {
            bool ancestor=false;
            for(auto w:m_resizeAncestors) if(object==w) { ancestor=true; break; }
            if(!m_enabled || m_paused || m_applying || m_forwardingNativeResize ||
                !ancestor) return false;
            // Finish the native resize/layout pass once, then finalize the
            // reclaimed geometry before returning to painting or the size loop.
            // Nested region events remain inside this single native pass.
            // Higher ancestors can assign regions again after a nested root
            // resize returns. Keep the entire outer pass visually atomic,
            // including synchronous repaint requests from native handlers.
            const bool updates=m_root && m_root->updatesEnabled();
            if(updates) m_root->setUpdatesEnabled(false);
            m_forwardingNativeResize=true;
            QCoreApplication::sendEvent(object,event);
            m_forwardingNativeResize=false;
            apply();
            if(updates && m_root) m_root->setUpdatesEnabled(true);
            return true;
        }
        if(object==m_timeline.widget && event->type()==QEvent::Resize && !m_deliveringTimeline) {
            auto resize=static_cast<QResizeEvent *>(event);
            if(m_enabled && !m_paused && !m_applying) {
                // Resolve still assigns the native half-height region. Retain
                // that baseline, but do not let its temporary shrink clamp the
                // timeline's video/audio split before our queued expansion.
                capture(m_timeline); schedule(); return true;
            }
            const auto previous=m_deliveredTimelineSize;
            m_deliveredTimelineSize=resize->size();
            if(m_applying && previous==resize->size()) return true;
            if(m_applying && previous.isValid() && resize->oldSize()!=previous) {
                // Report the actual transition seen by native content, not the
                // suppressed interim size. Keep one resize delivery per update.
                QResizeEvent coalesced(resize->size(),previous);
                m_deliveringTimeline=true;
                QCoreApplication::sendEvent(object,&coalesced);
                m_deliveringTimeline=false;
                return true;
            }
        }
        return false;
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if (!m_enabled || m_applying) return false;
        const auto type=event->type();
        if (type==QEvent::ParentAboutToChange &&
            (object==m_viewers.widget || object==m_timeline.widget || object==m_root)) {
            setEnabled(false); return false;
        }
        if (object==m_page && type==QEvent::Hide) {
            m_paused=true; restore(); return false;
        }
        if (object==m_page && type==QEvent::Show) m_paused=false;
        if (type==QEvent::Move || type==QEvent::Resize || type==QEvent::Show || type==QEvent::Hide) {
            auto update=[&](Placement &p) { if (object==p.widget) capture(p); };
            update(m_viewers); update(m_timeline);
            for (auto &p:m_splitters) update(p);
            for (auto &p:m_sideRegions) update(p);
            schedule();
        } else if (type==QEvent::LayoutRequest) schedule();
        return false;
    }
public:
    ViewerGeometryAdapter(QWidget *viewers,QWidget *page,QObject *owner)
        : QObject(owner),m_root(viewers?viewers->parentWidget():nullptr),m_page(page) {
        m_viewers.widget=viewers;
        // Retain the complete native resize boundary once, rather than looking
        // up parents/discovering widgets on every event.
        for(auto w=m_root.data();w;w=w->parentWidget()) {
            m_resizeAncestors.append(w);
            if(w->isWindow()) break;
        }
        m_resizeGate=new ViewerResizeGate(this);
        m_resizeGate->resize=[this](QObject *object,QEvent *event) { return resizeBeforeLayout(object,event); };
        if (m_root) {
            m_timeline.widget=m_root->findChild<QWidget *>("frameEditTimelinesHolder",Qt::FindDirectChildrenOnly);
            if(m_timeline.widget) m_deliveredTimelineSize=m_timeline.widget->size();
            for (auto w:m_root->findChildren<QWidget *>("timelineBarAnchor_Splitter",Qt::FindDirectChildrenOnly)) {
                Placement p; p.widget=w; m_splitters.append(p); w->installEventFilter(this);
            }
            for (auto w:m_root->findChildren<QWidget *>(QString(),Qt::FindDirectChildrenOnly)) {
                if (w==m_viewers.widget || w==m_timeline.widget || w->objectName()=="timelineBarAnchor_Splitter") continue;
                if (w->objectName().startsWith("frameEditPage") || w->objectName().endsWith("_Splitter")) {
                    Placement p; p.widget=w; m_sideRegions.append(p); w->installEventFilter(this);
                }
            }
            m_root->installEventFilter(this);
        }
        for (auto w:{m_viewers.widget.data(),m_timeline.widget.data(),m_page.data()}) if (w) w->installEventFilter(this);
    }
    ~ViewerGeometryAdapter() override { setEnabled(false); }
    bool setEnabled(bool enabled) {
        if (enabled==m_enabled) return m_enabled;
        if (!enabled) { m_enabled=false; restore(); qApp->removeEventFilter(m_resizeGate); return false; }
        if (!valid()) return false;
        capture(m_viewers); capture(m_timeline);
        m_deliveredTimelineSize=m_timeline.widget->size();
        for (auto &p:m_splitters) capture(p);
        for (auto &p:m_sideRegions) capture(p);
        m_paused=m_page && !m_page->isVisible(); m_enabled=true;
        qApp->installEventFilter(m_resizeGate); schedule(); return true;
    }
    bool isEnabled() const { return m_enabled; }
};
