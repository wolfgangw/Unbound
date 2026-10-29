// SPDX-License-Identifier: MIT
#include "sidebarfloatsession.h"
#include "layoutadapter.h"
#include <QLabel>

void testHostLifecycle() {
    using namespace Unbound;
    using LayoutTest::require;
    for(bool retained:{false,true}) {
        QWidget page; page.setObjectName("WStackPage_Color");
        auto pageLayout=new QVBoxLayout(&page);
        auto slot=new QWidget; pageLayout->addWidget(slot);
        auto native=new QVBoxLayout(slot);
        QPointer<QLabel> content=new QLabel("Native content"); native->addWidget(content);
        page.show(); LayoutTest::settle();
        SidebarFloatSession session;
        for(int round=0;round<3;++round) {
            require(session.start(slot,"Fixture",&page,false,false,retained,false).isEmpty(),"Host lifecycle transfer failed");
            QPointer<QWidget> host=session.floatingWindow();
            require(host && content && content->window()==host,"Native content did not enter host");
            require(session.dock().isEmpty(),"Host lifecycle docking failed");
            LayoutTest::settle();
            require(!host && !session.active() && !session.floatingWindow(),"Docked pane retained its empty host");
            require(content && content->parentWidget()==slot && slot->layout()==native,
                "Host destruction lost native content or its original layout");
        }
    }
    std::puts("PASS: repeated docking destroys empty hosts while preserving native content and layout ownership");
}
