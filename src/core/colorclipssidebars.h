// SPDX-License-Identifier: MIT
#pragma once
#include <QAbstractButton>
#include <QPointer>
#include <QWidget>
#include <QVector>
#include <functional>

namespace Unbound {
// Lightbox hides these native containers without providing ordinary side-panel
// allocation. Reuse their contents and remembered native widths in the reclaimed
// upper region. No native minimum/maximum constraints are changed.
class ColorClipsSidebars {
    struct Column {
        QPointer<QWidget> widget,divider;
        QVector<QPointer<QAbstractButton>> toggles;
        QRect original,dividerOriginal;
        bool hidden=false,dividerHidden=false,left=false;
        int width=0,dividerThickness=1;
        bool requested() const {
            for(auto toggle:toggles) if(toggle && toggle->isChecked()) return true;
            return false;
        }
    };
    QVector<Column> columns;
    QVector<QMetaObject::Connection> connections;
public:
    void capture(QWidget *page,QWidget *root,QObject *owner,const std::function<void()> &changed) {
        auto add=[&](const char *name,const char *divider,bool left,std::initializer_list<const char *> toggles) {
            auto widget=page->findChild<QWidget *>(name);
            if(!widget || widget->parentWidget()!=root) return;
            Column c; c.widget=widget;c.left=left;c.original=widget->geometry();c.hidden=widget->isHidden();
            c.width=qMax(widget->width(),qMax(widget->minimumWidth(),widget->minimumSizeHint().width()));
            c.divider=root->findChild<QWidget *>(divider,Qt::FindDirectChildrenOnly);
            if(c.divider) {c.dividerOriginal=c.divider->geometry();c.dividerHidden=c.divider->isHidden();
                // Unused native handles retain QWidget's default 100px geometry.
                // Borrow the initialized left handle thickness in that case.
                auto reference=root->findChild<QWidget *>("leftSideBarRightAnchor_Splitter",Qt::FindDirectChildrenOnly);
                c.dividerThickness=c.dividerOriginal.width();
                if(c.dividerThickness>12 || c.dividerThickness<1)
                    c.dividerThickness=reference ? qBound(1,reference->width(),12) : 1;
            }
            for(auto name:toggles) {
                const auto found=page->window()->findChildren<QAbstractButton *>(name);
                if(found.size()!=1) continue;
                c.toggles.append(found.first());
                connections.append(QObject::connect(found.first(),&QAbstractButton::toggled,owner,[changed]{changed();}));
            }
            columns.append(c);
        };
        add("m_pFrameGalleryContainer","leftSideBarRightAnchor_Splitter",true,
            {"btnColorToggleGallery","btnColorToggleLutsBrowser","btnColorToggleMediaPool"});
        // Allocate from the outside inward: Effects sits to the right of Nodes.
        add("m_pFrameOFXPlugin","ofxLeftAnchor_Splitter",false,{"btnColorToggleOpenFX"});
        add("m_pFrameNodeGraph","nodesLeftAnchor_Splitter",false,{"btnColorToggleNodeGraph"});
    }
    bool isDivider(QObject *object) const {
        for(const auto &c:columns) if(c.divider==object && c.widget && c.requested()) return true;
        return false;
    }
    void captureDraggedWidths() {
        for(auto &c:columns) if(c.widget && c.requested()) c.width=c.widget->width();
    }
    bool watches(QObject *object) const {
        for(const auto &c:columns) if(c.widget==object || c.divider==object) return true;
        return false;
    }
    QRect apply(QRect viewer) {
        // Reserve native minima for all requested columns before distributing
        // remaining width. The viewer retains a usable region in narrow windows.
        int minimum=0;
        for(const auto &c:columns) if(c.widget && c.requested())
            minimum+=qMax(c.widget->minimumWidth(),qMax(0,c.widget->minimumSizeHint().width()))+1;
        for(auto &c:columns) {
            if(!c.widget) continue;
            const bool show=c.requested();
            if(show) {
                const int min=qMax(c.widget->minimumWidth(),qMax(0,c.widget->minimumSizeHint().width()));
                minimum-=min+1;
                const int available=qMax(min,viewer.width()-minimum-160-1);
                const int width=qBound(min,c.width,qMax(min,qMin(available,c.widget->maximumWidth())));
                const int x=c.left ? viewer.left() : viewer.right()-width+1;
                c.widget->setGeometry(x,viewer.y(),width,viewer.height());
                if(c.left) viewer.setLeft(x+width+1);else viewer.setRight(x-2);
                if(c.divider) {
                    const int thickness=c.dividerThickness;
                    const int edge=c.left ? x+width : x;
                    c.divider->setGeometry(edge-thickness/2,viewer.y(),thickness,viewer.height());
                }
            }
            c.widget->setVisible(show);if(c.divider)c.divider->setVisible(show);
        }
        return viewer;
    }
    void restore() {
        for(const auto &c:columns) {
            if(c.widget) {c.widget->setGeometry(c.original);c.widget->setVisible(!c.hidden);}
            if(c.divider) {c.divider->setGeometry(c.dividerOriginal);c.divider->setVisible(!c.dividerHidden);}
        }
        columns.clear();
        for(const auto &connection:connections) QObject::disconnect(connection);
        connections.clear();
    }
};
}
