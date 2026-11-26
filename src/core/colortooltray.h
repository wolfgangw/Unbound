// SPDX-License-Identifier: MIT
#pragma once
#include "panecatalogue.h"
#include "primariespickeractivation.h"
#include <QBoxLayout>
#include <QFrame>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QStackedWidget>

namespace Unbound {

// Resolve owns viewer routing through its palette selectors. When that
// selection displaces a docked palette, display its real controls in the same
// tray rectangle. The shell and stack indices remain available to Resolve.
class ColorToolTray final : public QObject {
    using Tool = PaneCatalogue::ColorControlDescriptor;
    struct Choice {
        QPointer<QWidget> slot;
        QRect area;
    };
    struct Presentation {
        QPointer<QWidget> anchor, slot, holder, surface;
        QPointer<QLayout> layout;
        bool showedShell = false;
    };
public:
    using Lookup = std::function<QWidget *(const QString &)>;
    ColorToolTray(QWidget *page, Lookup slot, Lookup host, PrimariesPickerActivation::Viewer viewer, QObject *parent)
        : QObject(parent), m_page(page), m_slot(std::move(slot)), m_host(std::move(host)), m_viewer(std::move(viewer)) {
        page->installEventFilter(this);
    }
    ~ColorToolTray() override { reset(); }

    void reset() {
        ++m_generation;
        m_departingChoices.clear();
        m_owner.clear();
        m_pendingToolbar=false;
        QObject::disconnect(m_click);
        if (m_observing && qApp) qApp->removeEventFilter(this);
        m_observing = false;
        while (!m_presentations.isEmpty()) release(m_presentations.size()-1);
    }

    void prepareTransfer(QWidget *slot) {
        if (!slot) return;
        ++m_generation;
        QObject::disconnect(m_click);
        if (owns(slot)) m_owner.clear();
        for (int i=m_presentations.size()-1;i>=0;--i) {
            const auto &p=m_presentations.at(i);
            if (p.slot==slot || p.anchor==slot) release(i);
        }
        stopIfIdle();
    }
    bool owns(QWidget *slot) const { return slot && slot->objectName()==m_owner; }

    bool selectsFromFloatingPress(const Tool &tool,QWidget *receiver,QWidget *host) const {
        // A click into a floating palette chooses it when the viewer is still
        // with a docked tool. No second click on its already-armed picker.
        if (m_owner.isEmpty() || m_owner==tool.objectName || requestsViewer(tool,receiver,host)) return true;
        // With another floating viewer tool selected, leave local adjustments
        // alone. A click on the host or a plain palette background chooses it.
        if (qobject_cast<QLabel *>(receiver) && ancestor(receiver,tool.header)) return true;
        return receiver==host || receiver->metaObject()==&QFrame::staticMetaObject
            || receiver->metaObject()==&QWidget::staticMetaObject;
    }

    QString displayedSelector(const QString &native) const {
        for (const auto &p:m_presentations) {
            if (!p.anchor || !p.slot) continue;
            const auto anchor=PaneCatalogue::colorControl(p.anchor->objectName());
            const auto palette=PaneCatalogue::colorControl(p.slot->objectName());
            if (anchor && palette && anchor->selector==native) return palette->selector;
        }
        return m_departingChoices.value(native,native);
    }

    static bool requestsViewer(const Tool &tool, QWidget *receiver, QWidget *boundary) {
        // Check actual viewer actions, including their custom child widgets.
        // Most buttons, even checkable ones, only adjust local settings.
        static const QHash<QString,QStringList> actions {
            {"qualTab", {"framePickerColor","framePickerSoftness","btnPicker","btnPickerPlusColor",
                "btnPickerMinusColor","btnPickerPlusSoftness","btnPickerMinusSoftness"}},
            {"windowTab", {"btnWindowEnable","btnAddWindowCircle","btnAddWindowCurve","btnAddWindowGradient",
                "btnAddWindowLinear","btnAddWindowPolygon"}},
            {"trackerTab", {"chkboxColorSecTrackerStableInteractiveMode","chkboxColorSecTrackerWindowInteractiveMode",
                "frameInteractiveModeButtons","frameWinTrackModeInteractiveButtons","btnColorSecTrackerStableSetPoint",
                "btnColorSecTrackerWindowSetPoint"}},
            {"personMaskTab", {"btnForegroundStroke","btnBackgroundStroke","btnForegroundBrushStroke","btnBackgroundBrushStroke"}},
            {"colorHDRTab", {"hdrColorZonesScroller","treeWidgetZones","btnZone1LowHigh","btnZone2LowHigh",
                "btnZone3LowHigh","btnZone4LowHigh","btnZone5LowHigh",
                "lblZone1","lblZone2","lblZone3","lblZone4","lblZone5"}}
        };
        const auto names = actions.constFind(tool.objectName);
        if (names==actions.cend()) return false;
        for (auto w=receiver; w && w!=boundary; w=w->parentWidget()) {
            if (names->contains(w->objectName())) return true;
            // Clicking the palette title is also an explicit choice of tool.
            if (w->objectName()==tool.header && receiver==w) return true;
        }
        return false;
    }

    void activate(QWidget *slot, QAbstractButton *selector) {
        if (!selector) return;
        if (!slot || !m_page || !m_page->isVisible()) { selector->click(); return; }
        ++m_generation;
        if (!m_region || !m_region->isAncestorOf(slot))
            m_region=ancestor(slot,"frameColorBottomToolsContainer");
        if (!m_region) { selector->click(); return; }
        const auto choices=displayed();
        m_owner=slot->objectName();
        if (!m_observing) { qApp->installEventFilter(this); m_observing=true; }
        selectAndPresent(slot,selector,choices);
    }

protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        const auto type=event->type();
        if (object==m_page && type==QEvent::Hide && m_observing) {
            // Page selection capture may run before or after this filter. Give
            // it the displayed choices in either order, then return all layouts.
            QHash<QString,QString> choices;
            for (const auto &p:m_presentations) {
                if (!p.slot || !p.anchor) continue;
                const auto anchor=PaneCatalogue::colorControl(p.anchor->objectName());
                const auto palette=PaneCatalogue::colorControl(p.slot->objectName());
                if (anchor && palette) choices.insert(anchor->selector,palette->selector);
            }
            reset(); m_departingChoices=choices;
            return false;
        }
        if (object==m_page && type==QEvent::Show) m_departingChoices.clear();
        if (!m_observing || m_busy || !m_page || !m_region) return false;
        if ((type==QEvent::Move || type==QEvent::Resize || type==QEvent::Show || type==QEvent::Hide) && m_watched.contains(object)) {
            for (const auto &p:m_presentations) {
                if (!p.anchor || !p.surface || !object->isWidgetType()) continue;
                auto w=static_cast<QWidget *>(object);
                if (w!=p.anchor && !w->isAncestorOf(p.anchor)) continue;
                if (!m_region->isAncestorOf(p.anchor)) { scheduleCleanup(); continue; }
                if (p.anchor->isVisibleTo(m_region) && m_region->isVisible()) {
                    p.surface->setGeometry(bounds(p.anchor)); p.surface->show(); p.surface->raise();
                } else { p.surface->hide(); scheduleCleanup(); }
            }
            return false;
        }
        if (type==QEvent::ApplicationDeactivate) { ++m_generation; return false; }
        if (type!=QEvent::MouseButtonPress && type!=QEvent::KeyPress && type!=QEvent::Shortcut) return false;
        if (m_pendingToolbar) { ++m_generation; m_pendingToolbar=false; }
        if (!object->isWidgetType() || QApplication::activeModalWidget() || QApplication::activePopupWidget()) return false;
        if (type==QEvent::MouseButtonPress && static_cast<QMouseEvent *>(event)->button()!=Qt::LeftButton) return false;
        if (type==QEvent::KeyPress) {
            const auto key=static_cast<QKeyEvent *>(event)->key();
            if (key!=Qt::Key_Space && key!=Qt::Key_Return && key!=Qt::Key_Enter) return false;
        }
        QPointer<QWidget> receiver=static_cast<QWidget *>(object);
        if (ancestor(receiver,"resolvePaneUndockButton")) return false;
        if (auto button=qobject_cast<QAbstractButton *>(receiver.data())) {
            if (ancestor(button,"frameColorBottomToolbar") && m_page->isAncestorOf(button)) {
                for (const auto &tool:PaneCatalogue::colorControls())
                    if (button->objectName()==tool.selector) { watchClick(button,tool); return false; }
            }
        }
        for (int i=0;i<m_presentations.size();++i) {
            const auto p=m_presentations.at(i);
            if (!p.slot || !p.surface || !p.surface->isAncestorOf(receiver)) continue;
            const auto tool=PaneCatalogue::colorControl(p.slot->objectName());
            if (!tool || !requestsViewer(*tool,receiver,p.surface)) return false;
            if (auto button=availableButton(*tool)) {
                ++m_generation; m_owner.clear(); release(i);
                stopIfIdle();
                button->click(); // Before Resolve receives this original input.
            }
            return !receiver;
        }
        // An explicitly chosen viewer action in a different, naturally visible
        // region also supersedes the previous owner. Resolve handles that input.
        for (auto w=receiver.data(); w && w!=m_region; w=w->parentWidget()) {
            const auto tool=PaneCatalogue::colorControl(w->objectName());
            if (tool && w==m_slot(tool->objectName) && requestsViewer(*tool,receiver,w)) {
                ++m_generation; m_owner.clear(); break;
            }
        }
        // Expanded tools receive their viewer assignment from Resolve itself.
        for (const auto name:{"curvesTab","colorMapperTab"}) {
            auto host=m_host(name);
            if (host && receiver->window()==host) { ++m_generation; m_owner.clear(); break; }
        }
        stopIfIdle();
        return false;
    }

private:
    void stopIfIdle() {
        if (m_observing && m_owner.isEmpty() && m_presentations.isEmpty()) reset();
    }
    static QWidget *ancestor(QWidget *w,const QString &name) {
        for (; w; w=w->parentWidget()) if (w->objectName()==name) return w;
        return nullptr;
    }
    QRect bounds(QWidget *w) const { return QRect(w->mapTo(m_region,QPoint()),w->size()); }
    static bool sameArea(const QRect &a,const QRect &b) {
        const auto overlap=a.intersected(b);
        // Combined mode can place different stacks in one tray. Compare actual
        // allocations, allowing native border differences but never adjacent trays.
        return overlap.isValid() && 10*overlap.width()>=9*qMax(a.width(),b.width())
            && 10*overlap.height()>=9*qMax(a.height(),b.height());
    }
    QList<Choice> displayed() const {
        QList<Choice> result;
        for (const auto &p:m_presentations)
            if (p.surface && p.surface->isVisible()) result.append({p.slot,p.surface->geometry()});
        for (const auto &tool:PaneCatalogue::colorControls()) {
            auto w=m_slot(tool.objectName);
            if (!w || m_host(tool.objectName) || !m_region->isAncestorOf(w) || !w->isVisible()) continue;
            auto stack=qobject_cast<QStackedWidget *>(w->parentWidget());
            if (stack && stack->currentWidget()==w) result.append({w,bounds(w)});
        }
        return result;
    }
    QAbstractButton *availableButton(const Tool &tool) const {
        auto bottom=ancestor(m_region,"frameColorBottom");
        auto toolbar=bottom ? bottom->findChild<QWidget *>("frameColorBottomToolbar") : nullptr;
        if (toolbar) for (auto b:toolbar->findChildren<QAbstractButton *>(tool.selector))
            if (b->isVisible() && b->isEnabled()) return b;
        return nullptr;
    }
    void watchClick(QAbstractButton *button,const Tool &tool) {
        const auto press=++m_generation;
        QObject::disconnect(m_click);
        m_click=connect(button,&QAbstractButton::clicked,this,[this,press,name=tool.objectName] {
            QObject::disconnect(m_click);
            if (m_busy || press!=m_generation || m_owner.isEmpty()) return;
            const auto generation=++m_generation;
            if (m_host(name)) { reset(); return; }
            // Wait for Resolve's clicked handler and native action dispatch to
            // unwind. Re-selecting inside that handler loses viewer ownership.
            m_pendingToolbar=true;
            QTimer::singleShot(0,this,[this,name,generation] {
                if (generation!=m_generation) return;
                m_pendingToolbar=false;
                if (!m_page || !m_page->isVisible() || !m_region
                    || QApplication::activePopupWidget() || QApplication::activeModalWidget()) return;
                auto owner=m_slot(m_owner),host=m_host(m_owner),chosen=m_slot(name);
                const auto tool=PaneCatalogue::colorControl(m_owner);
                if (!owner || !host || !host->isVisible() || !chosen || !chosen->isVisible()
                    || !m_region->isAncestorOf(chosen) || !tool) return;
                auto selector=availableButton(*tool);
                if (!selector) return;
                const auto area=bounds(chosen);
                for (int i=m_presentations.size()-1;i>=0;--i) {
                    const auto surface=m_presentations.at(i).surface;
                    if (!surface || sameArea(surface->geometry(),area)) release(i);
                }
                selectAndPresent(owner,selector,{{chosen,area}});
            });
        });
    }
    void selectAndPresent(QPointer<QWidget> anchor,QAbstractButton *button,const QList<Choice> &choices) {
        const auto generation=m_generation;
        m_busy=true;
        const QPointer<ColorToolTray> alive(this);
        button->click();
        if (!alive) return;
        m_busy=false;
        if (generation!=m_generation || !anchor || !m_region || !anchor->isVisible()
            || !m_region->isAncestorOf(anchor)) return;
        const auto area=bounds(anchor);
        for (const auto &choice:choices) {
            if (!choice.slot || choice.slot==anchor || !sameArea(choice.area,area)) continue;
            for (int i=m_presentations.size()-1;i>=0;--i) {
                const auto &p=m_presentations.at(i);
                if (p.anchor==anchor && p.slot==choice.slot && p.surface) {
                    p.surface->setGeometry(area); p.surface->show(); p.surface->raise(); return;
                }
                if (p.slot==choice.slot || (p.surface && sameArea(p.surface->geometry(),area))) release(i);
            }
            if (!choice.slot->isVisible()) present(anchor,choice.slot);
            return;
        }
    }
    void present(QWidget *anchor,QWidget *slot) {
        const auto tool=PaneCatalogue::colorControl(slot->objectName());
        if (!tool || m_host(tool->objectName)) return;
        auto holder=tool->retainedContainer.isEmpty()? slot :
            slot->findChild<QWidget *>(tool->retainedContainer,Qt::FindDirectChildrenOnly);
        auto layout=holder? holder->layout() : nullptr;
        if (!layout || !layout->count() || (!qobject_cast<QBoxLayout *>(layout) && !qobject_cast<QGridLayout *>(layout))) return;
        auto surface=new QFrame(m_region);
        surface->setAutoFillBackground(true);
        surface->setLayout(layout);
        if (surface->layout()!=layout) { delete surface; return; }
        auto stack=qobject_cast<QStackedWidget *>(slot->parentWidget());
        const bool showShell=stack && anchor->parentWidget()==stack && stack->currentWidget()==anchor && slot->isHidden();
        if (showShell) { slot->show(); anchor->raise(); }
        surface->setGeometry(bounds(anchor));
        surface->show(); surface->raise();
        if (tool->objectName=="colorWheelsTab" && m_viewer) new PrimariesPickerActivation(surface,m_viewer);
        const QPointer<ColorToolTray> self(this);
        auto lostSlot=[self] { if (self) self->scheduleCleanup(); };
        connect(slot,&QObject::destroyed,surface,lostSlot);
        connect(anchor,&QObject::destroyed,surface,lostSlot);
        m_presentations.append({anchor,slot,holder,surface,layout,showShell});
        rebuildWatched();
    }
    void rebuildWatched() {
        m_watched.clear();
        for (const auto &p:m_presentations)
            for (auto w=p.anchor.data(); w && w!=m_page; w=w->parentWidget()) m_watched.insert(w);
    }
    void scheduleCleanup() {
        if (m_cleanupQueued) return;
        m_cleanupQueued=true;
        QTimer::singleShot(0,this,[this] {
            m_cleanupQueued=false;
            for (int i=m_presentations.size()-1;i>=0;--i) {
                const auto &p=m_presentations.at(i);
                if (!p.slot || !p.holder || !p.anchor || !m_region || !m_region->isAncestorOf(p.anchor) || !p.anchor->isVisible())
                    release(i);
            }
            stopIfIdle();
        });
    }
    void release(int index) {
        const auto p=m_presentations.takeAt(index);
        rebuildWatched();
        if (p.showedShell && p.slot) {
            auto stack=qobject_cast<QStackedWidget *>(p.slot->parentWidget());
            if (stack && stack->currentWidget()!=p.slot) p.slot->hide();
        }
        if (!p.surface) return;
        p.surface->hide();
        if (p.layout && p.surface->layout()==p.layout && p.holder) {
            // Never destroy Resolve's controls if it replaced the holder layout.
            if (p.holder->layout()) { p.surface->setParent(p.holder); return; }
            p.holder->setLayout(p.layout);
        }
        delete p.surface.data();
    }
    QPointer<QWidget> m_page,m_region;
    Lookup m_slot,m_host;
    PrimariesPickerActivation::Viewer m_viewer;
    QList<Presentation> m_presentations;
    QSet<QObject *> m_watched;
    QHash<QString,QString> m_departingChoices;
    QString m_owner;
    QMetaObject::Connection m_click;
    unsigned m_generation=0;
    bool m_busy=false,m_observing=false,m_cleanupQueued=false,m_pendingToolbar=false;
};
}
