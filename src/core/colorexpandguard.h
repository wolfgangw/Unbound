// SPDX-License-Identifier: MIT
#ifndef UNBOUND_COLOREXPANDGUARD_H
#define UNBOUND_COLOREXPANDGUARD_H
#include <QAbstractButton>
#include <QPointer>
#include <QTimer>
#include <QEvent>
#include <QSet>
namespace Unbound {
// Observe only the floating tool subtree. Native show/mode changes may enable
// Expand again or construct a new header; neither may steal our hosted content.
class ColorExpandGuard : public QObject {
public:
    ColorExpandGuard(QWidget *host,QObject *owner) : QObject(owner),m_host(host) { observe(); }
    ~ColorExpandGuard() override {
        for(auto widget:m_widgets) if(widget) widget->removeEventFilter(this);
        for(const auto &entry:m_buttons) if(entry.first) entry.first->setEnabled(entry.second);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()==QEvent::EnabledChange || event->type()==QEvent::Show) {
            for(const auto &entry:m_buttons) if(entry.first==object && entry.first->isEnabled()) {
                entry.first->setEnabled(false); break;
            }
        }
        if(event->type()==QEvent::ChildAdded && !m_queued) {
            m_queued=true;
            QTimer::singleShot(0,this,[this] { m_queued=false; observe(); });
        }
        return false;
    }
private:
    void observe() {
        if(!m_host) return;
        auto widgets=m_host->findChildren<QWidget *>(); widgets.prepend(m_host);
        QSet<QWidget *> known;
        for(auto widget:m_widgets) if(widget) known.insert(widget);
        for(auto widget:widgets) {
            if(!known.contains(widget)) { m_widgets.append(widget); widget->installEventFilter(this); }
            auto button=qobject_cast<QAbstractButton *>(widget);
            if(!button || (button->objectName()!=QLatin1String("btnUndock")
                && button->objectName()!=QLatin1String("btnExpandWidget")
                && button->objectName()!=QLatin1String("btnTGExpand"))) continue;
            bool retained=false;
            for(const auto &entry:m_buttons) if(entry.first==button) { retained=true; break; }
            if(!retained) m_buttons.append({button,!button->testAttribute(Qt::WA_ForceDisabled)});
            button->setEnabled(false);
        }
    }
    QPointer<QWidget> m_host;
    QList<QPointer<QWidget>> m_widgets;
    QList<QPair<QPointer<QAbstractButton>,bool>> m_buttons;
    bool m_queued=false;
};
}
#endif
