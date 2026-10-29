// SPDX-License-Identifier: MIT
#pragma once
#include <QEvent>
#include <QLayout>
#include <QPointer>
#include <QStackedWidget>
#include <QTimer>

namespace Unbound {
// QStackedLayout normally budgets for every page, including hidden modes.
// Suppress only inactive page hints while retaining their content and constraints.
class ActiveStackSize final : public QObject {
public:
    ActiveStackSize(QStackedWidget *stack,QObject *owner,bool horizontal=true) : QObject(owner),m_stack(stack),m_horizontal(horizontal) {
        stack->installEventFilter(this);
        connect(stack,&QStackedWidget::currentChanged,this,[this] { refresh(); });
        connect(stack,&QStackedWidget::widgetRemoved,this,[this] { refresh(); });
        refresh();
    }
    ~ActiveStackSize() override {
        if(m_stack) m_stack->removeEventFilter(this);
        for(const auto &entry:m_pages) restore(entry);
        invalidate();
    }
protected:
    bool eventFilter(QObject *,QEvent *event) override {
        if((event->type()==QEvent::ChildAdded || event->type()==QEvent::LayoutRequest) && !m_queued) {
            m_queued=true;
            QTimer::singleShot(0,this,[this] { m_queued=false; refresh(); });
        }
        return false;
    }
private:
    struct Page { QPointer<QWidget> widget; QSizePolicy original,applied; };
    static void restore(const Page &page) {
        if(page.widget && page.widget->sizePolicy()==page.applied)
            page.widget->setSizePolicy(page.original);
    }
    void refresh() {
        if(!m_stack) return;
        bool changed=false;
        for(int i=m_pages.size()-1;i>=0;--i) {
            if(!m_pages[i].widget || m_stack->indexOf(m_pages[i].widget)<0) {
                restore(m_pages[i]); m_pages.removeAt(i); changed=true;
            }
        }
        for(int i=0;i<m_stack->count();++i) {
            auto widget=m_stack->widget(i);
            bool known=false;
            for(const auto &entry:m_pages) if(entry.widget==widget) { known=true; break; }
            if(!known) m_pages.append({widget,widget->sizePolicy(),widget->sizePolicy()});
        }
        for(auto &entry:m_pages) {
            // Preserve newer native policy changes rather than restoring stale state.
            if(entry.widget->sizePolicy()!=entry.applied) entry.original=entry.widget->sizePolicy();
            auto policy=entry.original;
            if(entry.widget!=m_stack->currentWidget()) {
                if(m_horizontal) policy.setHorizontalPolicy(QSizePolicy::Ignored);
                policy.setVerticalPolicy(QSizePolicy::Ignored);
            }
            entry.applied=policy;
            if(entry.widget->sizePolicy()!=policy) { entry.widget->setSizePolicy(policy); changed=true; }
        }
        if(changed) invalidate();
    }
    void invalidate() {
        // Hidden pages do not propagate updateGeometry through the ancestor chain.
        // Drop cached minima up to the host; Qt still owns the resulting geometry.
        for(auto widget=static_cast<QWidget *>(m_stack.data());widget;widget=widget->parentWidget()) {
            if(widget->layout()) { widget->layout()->invalidate(); widget->layout()->activate(); }
            widget->updateGeometry();
            if(widget->isWindow()) break;
        }
    }
    QPointer<QStackedWidget> m_stack;
    QList<Page> m_pages;
    bool m_queued=false;
    bool m_horizontal=true;
};
}
