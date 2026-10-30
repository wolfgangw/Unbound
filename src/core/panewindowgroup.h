// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QWidget>
#include <QDynamicPropertyChangeEvent>
#include <QPointer>
#include <QTimer>
#include <algorithm>
#include <functional>
#include "panecatalogue.h"
namespace Unbound {
// Shared application-local pinning and ordering policy. Native adapters preserve
// focus and confine pin priority to the active application group.
class PaneWindowGroup : public QObject {
public:
    PaneWindowGroup(std::function<bool(QWidget *,const QList<QWidget *> &)> group,
        std::function<void(QWidget *)> conceal,std::function<void(QWidget *)> reveal)
        : m_group(std::move(group)),m_conceal(std::move(conceal)),m_reveal(std::move(reveal)) { qApp->installEventFilter(this); }
    ~PaneWindowGroup() override { if(qApp) qApp->removeEventFilter(this); }
    void setWindow(QWidget *window) {
        if(m_main!=window) m_auxiliaryPages.clear();
        m_main=window;
    }
    void observeNativePage(QWidget *widget) {
        if(!widget || !m_main) return;
        for(const auto &page:PaneCatalogue::pages())
            if(!page.auxiliaryWindow.isEmpty() && widget->objectName()==page.auxiliaryWindow) {
                if(!m_auxiliaryPages.contains(widget)) m_auxiliaryPages.append(widget);
                return;
            }
    }
protected:
    QPointer<QWidget> m_main;
    bool m_raising=false;
    bool member(QWidget *window) const {
        if(!window || !window->isWindow() || !m_main) return false;
        if(window==m_main || window->property("unboundFloatingWindow").toBool()) return true;
        // Resolve's named auxiliary pages share an unnamed top-level wrapper.
        // Retain page identities, so reparenting never leaves a stale HWND peer.
        for(const auto &page:m_auxiliaryPages) if(page && page->window()==window) return true;
        return false;
    }
    bool layoutMenu(QWidget *window) const {
        return window && window->isWindow() && m_main
            && window->objectName()=="unboundLayoutMenu" && window->parentWidget()
            && window->parentWidget()->window()==m_main;
    }
    bool activationMember(QWidget *window) const { return member(window) || layoutMenu(window); }
    bool eventFilter(QObject *object,QEvent *event) override {
        if(m_raising) return false;
        const auto type=event->type();
        if(type!=QEvent::WindowActivate && type!=QEvent::Show && type!=QEvent::Hide
            && type!=QEvent::WindowStateChange && type!=QEvent::DynamicPropertyChange
            && type!=QEvent::ApplicationActivate) return false;
        auto window=qobject_cast<QWidget *>(object);
        if(type==QEvent::WindowActivate && member(window)
            && window->property("unboundKeepAtop").toBool()) {
            m_order.removeAll(window); m_order.prepend(window);
        }
        if((activationMember(window) && (type==QEvent::WindowActivate || type==QEvent::Show
            || type==QEvent::Hide || type==QEvent::WindowStateChange
            || (type==QEvent::DynamicPropertyChange &&
                (static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName()=="unboundKeepAtop"
                 || static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName()=="unboundUndockDragging"))))
            || type==QEvent::ApplicationActivate) schedule();
        return false;
    }
protected:
    // Native adapters can request the same coalesced policy pass when an OS
    // gesture changes stacking without changing Qt activation.
    void schedule() {
        if(m_pending) return;
        m_pending=true;
        QTimer::singleShot(0,this,[this] { m_pending=false; refresh(); });
    }
private:
    void refresh() {
        if(!m_main || QCoreApplication::closingDown()) return;
        m_raising=true;
        if(m_main->isMinimized()) {
            for(auto window:QApplication::topLevelWidgets())
                if(member(window) && window->property("unboundFloatingWindow").toBool()
                    && window->isVisible() && !window->isMinimized()) {
                    if(!m_groupHidden.contains(window)) {
                        m_groupHidden.append(window); m_conceal(window);
                    }
                }
        } else {
            const auto hidden=m_groupHidden; m_groupHidden.clear();
            for(const auto &entry:hidden) if(entry && !entry->isHidden()) {
                auto window=entry.data();
                const auto pageName=window->property("unboundPage").toString();
                auto page=pageName.isEmpty() ? nullptr : m_main->findChild<QWidget *>(pageName);
                if(!page || page->isVisible()) {
                    m_reveal(window);
                }
            }
        }
        m_raising=false;
        if(!stackingAllowed()) return;
        auto selected=QApplication::activeWindow();
        if(!activationMember(selected)) return;
        const auto pins=priorityWindows();
        m_raising=true;
        m_group(selected,pins);
        m_raising=false;
    }
protected:
    // A tearoff keeps keyboard focus in its original window until release, but
    // its host must precede that active window in every queued group refresh.
    // Persistent pins retain priority; this gesture state never enters settings.
    QList<QWidget *> priorityWindows() {
        auto windows=pinnedWindows();
        for(auto window:QApplication::topLevelWidgets())
            if(member(window) && window->isVisible() && !window->isMinimized()
                && window->property("unboundUndockDragging").toBool() && !windows.contains(window))
                windows.append(window);
        return windows;
    }
    bool stackingAllowed() const {
        if(!m_main || m_main->isMinimized() || QCoreApplication::closingDown()
            || QApplication::applicationState()!=Qt::ApplicationActive
            || QApplication::activeModalWidget() || QApplication::activePopupWidget()) return false;
        return true;
    }
    QList<QWidget *> pinnedWindows() {
        QList<QWidget *> newcomers;
        for(auto window:QApplication::topLevelWidgets())
            if(member(window) && window!=m_main && window->property("unboundKeepAtop").toBool()
                && !m_order.contains(window)) newcomers.append(window);
        std::sort(newcomers.begin(),newcomers.end(),[](QWidget *a,QWidget *b) {return a->objectName()<b->objectName();});
        for(auto window:newcomers) m_order.append(window);
        for(int i=m_order.size()-1;i>=0;--i) if(!m_order[i]) m_order.removeAt(i);
        QList<QWidget *> pins;
        for(const auto &entry:m_order) {
            auto window=entry.data();
            if(window && window->isVisible() && !window->isMinimized()
                && window->property("unboundKeepAtop").toBool()) pins.append(window);
        }
        return pins;
    }
private:
    QList<QPointer<QWidget>> m_auxiliaryPages;
    QList<QPointer<QWidget>> m_groupHidden;
    QList<QPointer<QWidget>> m_order;
    std::function<bool(QWidget *,const QList<QWidget *> &)> m_group;
    std::function<void(QWidget *)> m_conceal,m_reveal;
    bool m_pending=false;
};
}
