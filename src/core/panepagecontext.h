// SPDX-License-Identifier: MIT
#pragma once
#include <QWidget>
#include "panecatalogue.h"

namespace Unbound {
inline const PaneCatalogue::PageDescriptor *auxiliaryPanePage(const QWidget *widget) {
    for(auto ancestor=widget;ancestor;ancestor=ancestor->parentWidget())
        for(const auto &page:PaneCatalogue::pages())
            if(!page.auxiliaryWindow.isEmpty() && ancestor->objectName()==page.auxiliaryWindow)
                return &page;
    return nullptr;
}

// Native screen modes move dock slots between windows. Floating visibility must
// follow the logical page, not an auxiliary container retired by that move.
inline QWidget *panePageContext(QWidget *pane,QWidget *main) {
    if(!pane)return nullptr;
    const auto entry=PaneCatalogue::find(pane->objectName());
    if(!entry)return nullptr;
    if(const auto control=PaneCatalogue::colorControl(pane->objectName())) {
        bool found=false;
        for(auto p=pane->parentWidget();p;p=p->parentWidget())
            if(p->objectName()==control->scope){found=true;break;}
        if(!found)return nullptr;
    }
    const auto &page=PaneCatalogue::page(entry->page);
    for(auto p=pane->parentWidget();p;p=p->parentWidget())
        if(p->objectName()==page.paneRoot)return p;
    const auto auxiliary=auxiliaryPanePage(pane);
    if(!main || !auxiliary || auxiliary->id!=page.id)return nullptr;
    auto logical=main->findChild<QWidget *>(page.objectName);
    if(!logical)return nullptr;
    return page.paneRoot==page.objectName?logical:logical->findChild<QWidget *>(page.paneRoot);
}
}
