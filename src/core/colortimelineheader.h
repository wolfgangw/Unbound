// SPDX-License-Identifier: MIT
#pragma once
#include <QBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPointer>
#include <QToolButton>
#include "paneheaderdrag.h"

namespace Unbound {
// The native timeline's ruler and track gutter seek the playhead. Give only
// the floating host a separate drag surface; keep its existing button/actions.
class ColorTimelineHeader : public QObject {
public:
    ColorTimelineHeader(QWidget *host,QWidget *content,const QString &title,QObject *owner)
        : QObject(owner)
    {
        auto layout=qobject_cast<QVBoxLayout *>(host->layout());
        if(!layout || !content) return;
        m_button=content->findChild<QToolButton *>("resolvePaneUndockButton");
        if(!m_button) return;
        m_parent=m_button->parentWidget();
        m_header=new QFrame(host);
        m_header->setObjectName("unboundColorTimelineHeader");
        m_header->setFixedHeight(28);
        auto row=new QHBoxLayout(m_header);
        row->setContentsMargins(6,4,6,4); row->setSpacing(6);
        row->addWidget(m_button);
        auto label=new QLabel(title,m_header);
        label->setTextFormat(Qt::PlainText);
        label->setTextInteractionFlags(Qt::NoTextInteraction);
        row->addWidget(label); row->addStretch();
        layout->setSpacing(0);
        layout->insertWidget(0,m_header);
        new PaneHeaderDrag(m_header,m_header);
        m_button->show(); m_header->show();
    }
    ~ColorTimelineHeader() override {
        // Return the same button before destroying the host/header. Its native
        // corner placement is restored by PaneDecoration after docking.
        if(m_button && m_parent) { m_button->setParent(m_parent); m_button->show(); }
        delete m_header.data();
    }
private:
    QPointer<QWidget> m_header,m_parent;
    QPointer<QToolButton> m_button;
};
}
