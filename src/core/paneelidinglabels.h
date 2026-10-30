// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QTimer>
#include "panecatalogue.h"
namespace Unbound {
// Resolve keeps painting the original text with its native middle elision.
// Only the full-text horizontal hint is relaxed; content minimums stay native.
class PaneElidingLabels final : public QObject {
public:
    PaneElidingLabels(QWidget *host,const QString &pane) : m_host(host) {
        for(const auto &entry:PaneCatalogue::elidingLabels()) if(entry.pane==pane) m_specs.append(entry);
        if(m_specs.isEmpty()) return;
        qApp->installEventFilter(this);
        refresh();
    }
    ~PaneElidingLabels() override {
        if(qApp) qApp->removeEventFilter(this);
        for(const auto &entry:m_labels) restore(entry);
    }
private:
    struct Entry { QPointer<QLabel> label; QSizePolicy original,applied; };
    static void restore(const Entry &entry) {
        // Preserve a newer native policy if somebody else took ownership.
        if(entry.label && entry.label->sizePolicy()==entry.applied) entry.label->setSizePolicy(entry.original);
    }
    bool contains(QWidget *widget) const {return m_host && widget && (widget==m_host || m_host->isAncestorOf(widget));}
    bool eventFilter(QObject *object,QEvent *event) override {
        if(event->type()!=QEvent::ChildAdded && event->type()!=QEvent::ChildRemoved && event->type()!=QEvent::ParentChange) return false;
        auto widget=qobject_cast<QWidget *>(object);
        if(!widget) return false;
        bool tracked=false;
        for(const auto &entry:m_labels) if(entry.label==widget) {tracked=true;break;}
        if(!m_pending && (contains(widget) || tracked)) {
            m_pending=true;
            QTimer::singleShot(0,this,[this]{m_pending=false;refresh();});
        }
        return false;
    }
    void refresh() {
        if(!m_host) return;
        QList<QLabel *> matches;
        for(const auto &spec:m_specs)
            for(auto scope:m_host->findChildren<QWidget *>(spec.scope))
                for(auto label:scope->findChildren<QLabel *>(spec.label))
                    if(label->property("elideMode").isValid()
                        && label->property("elideMode").toInt()==int(Qt::ElideMiddle)
                        && !matches.contains(label)) matches.append(label);
        for(int i=m_labels.size()-1;i>=0;--i) if(!m_labels[i].label || !matches.contains(m_labels[i].label)) {
            restore(m_labels[i]);m_labels.removeAt(i);
        }
        bool changed=false;
        for(auto label:matches) {
            bool known=false;
            for(const auto &entry:m_labels) if(entry.label==label) {known=true;break;}
            if(known) continue;
            const auto original=label->sizePolicy(); auto applied=original;
            applied.setHorizontalPolicy(QSizePolicy::Ignored);
            applied.setHorizontalStretch(qMax(1,original.horizontalStretch()));
            m_labels.append({label,original,applied});
            label->setSizePolicy(applied);changed=true;
            for(auto parent=label->parentWidget();parent;parent=parent->parentWidget()) {
                if(parent->layout()) parent->layout()->invalidate();
                parent->updateGeometry();
                if(parent==m_host) break;
            }
        }
        if(changed && m_host->layout()) m_host->layout()->activate();
    }
    QPointer<QWidget> m_host;
    QVector<PaneCatalogue::ElidingLabelDescriptor> m_specs;
    QList<Entry> m_labels;
    bool m_pending=false;
};
}
