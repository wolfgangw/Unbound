// SPDX-License-Identifier: MIT
#pragma once
#include <QFrame>
#include <QBoxLayout>
#include <QPointer>
#include <QTimer>
#include <QEvent>
#include <QVector>
#include "windowcontrolclearance.h"

// The existing native Color controls occupy an additional row while Clips floats.
// Preserve ownership and layout positions so docking is reversible.
namespace Unbound {
class ColorClipsToolbar : public QObject {
    struct Item { QPointer<QWidget> widget,parent; QPointer<QBoxLayout> layout; int index,stretch; Qt::Alignment alignment; bool hidden; };
    QVector<Item> items;
    QPointer<QFrame> row;
    std::unique_ptr<QObject> clearance;
    bool pending=false,stopping=false;
public:
    explicit ColorClipsToolbar(QObject *owner):QObject(owner) {}
    bool start(QWidget *page) {
        auto bar=page->findChild<QWidget *>("frameColorTopBar");
        auto parent=bar?bar->parentWidget():nullptr;
        auto vertical=parent?qobject_cast<QVBoxLayout *>(parent->layout()):nullptr;
        if(!vertical || vertical->indexOf(bar)<0) return false;
        row=new QFrame(parent); row->setObjectName("unboundColorToolbarRow");
        row->setPalette(bar->palette()); row->setAutoFillBackground(true);
        row->setFixedHeight(bar->height());
        auto box=new QHBoxLayout(row); box->setContentsMargins(12,3,12,3); box->setSpacing(7);
        for(const char *name:{"frameColorTopBarLeftInSSM","frameColorTopBarRightInSSM"}) {
            if(!items.isEmpty()) box->addStretch();
            auto group=bar->findChild<QWidget *>(name);
            auto layout=group?qobject_cast<QBoxLayout *>(group->layout()):nullptr;
            if(!layout) {restore();return false;}
            QVector<Item> groupItems;
            for(int i=0;i<layout->count();++i) {
                auto entry=layout->itemAt(i); auto w=entry->widget();
                if(!w || w->objectName()=="btnColorToggleLightbox" || w->objectName()=="btnColorToggleColorControls") continue;
                groupItems.append({w,group,layout,i,layout->stretch(i),entry->alignment(),w->isHidden()});
            }
            for(const auto &item:groupItems) {
                items.append(item); layout->removeWidget(item.widget);
                box->addWidget(item.widget,0,item.alignment); item.widget->show(); item.widget->installEventFilter(this);
            }
        }
        vertical->insertWidget(vertical->indexOf(bar),row); row->show();
        vertical->invalidate();vertical->activate();
        clearance=reserveWindowControlClearance(row);
        return true;
    }
    void restore() {
        if(stopping) return; stopping=true;
        clearance.reset();
        for(const auto &item:items) if(item.widget && item.parent && item.layout) {
            item.widget->removeEventFilter(this);
            if(row && row->layout()) row->layout()->removeWidget(item.widget);
            item.widget->setParent(item.parent);
            item.layout->insertWidget(item.index,item.widget,item.stretch,item.alignment);
            item.widget->setVisible(!item.hidden);
        }
        items.clear();delete row.data();row=nullptr;
    }
    ~ColorClipsToolbar() override {restore();}
    bool eventFilter(QObject *,QEvent *e) override {
        if(!stopping && !pending && e->type()==QEvent::Hide) {
            pending=true;QTimer::singleShot(0,this,[this]{pending=false;if(stopping)return;for(const auto &item:items)if(item.widget)item.widget->show();});
        }
        return false;
    }
};

}
