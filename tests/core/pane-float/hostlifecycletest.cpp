// SPDX-License-Identifier: MIT
#include "sidebarfloatsession.h"
#include "layoutadapter.h"
#include "panepagecontext.h"
#include "controller.h"
#include <QLabel>
#include <QTemporaryDir>
#include <QToolButton>

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

void testAuxiliaryPageContext() {
    using namespace Unbound;
    using LayoutTest::require;
    // Every supported catalogue entry must resolve identically from either
    // native window, including scoped Color tools and both Edit viewers.
    for(const auto &entry:PaneCatalogue::panes()) {
        const auto &descriptor=PaneCatalogue::page(entry.page);
        QWidget main;
        auto page=new QWidget(&main); page->setObjectName(descriptor.objectName);
        auto root=page;
        if(descriptor.paneRoot!=descriptor.objectName) {
            root=new QWidget(page); root->setObjectName(descriptor.paneRoot);
        }
        QWidget auxiliary; auxiliary.setObjectName(descriptor.auxiliaryWindow);
        auto branch=new QWidget(&auxiliary);
        if(const auto control=PaneCatalogue::colorControl(entry.objectName)) branch->setObjectName(control->scope);
        auto slot=new QWidget(branch); slot->setObjectName(entry.objectName);
        require(panePageContext(slot,&main)==root,"Catalogued auxiliary pane lost its logical page");
        branch->setParent(root);
        require(panePageContext(slot,&main)==root,"Native screen-mode move changed logical page");
        branch->setParent(&auxiliary);
        auxiliary.setObjectName("UnrelatedWindow");
        require(!panePageContext(slot,&main),"Unrelated window was accepted as a native page");
    }
    for(const auto &name:{"frameEditPageMediaPool","frameEditPageEditIndex","m_pWidgetMediaPool","frameAudioPanelIndexContainer"}) {
        const auto &descriptor=PaneCatalogue::page(PaneCatalogue::find(name)->page);
        QWidget main; main.setObjectName("UiMainWindow"); main.resize(700,500);
        auto page=new QWidget(&main); page->setObjectName(descriptor.objectName); page->resize(700,500);
        auto root=page;
        if(descriptor.paneRoot!=descriptor.objectName) {
            root=new QWidget(page); root->setObjectName(descriptor.paneRoot); root->resize(700,500);
        }
        QWidget secondary; secondary.setObjectName(descriptor.auxiliaryWindow); secondary.resize(500,400);
        auto auxiliary=new QWidget(&secondary); auxiliary->setObjectName("frameEditAuxWidgetsContainer"); auxiliary->resize(500,400);
        auto slot=new QWidget(auxiliary); slot->setObjectName(name); slot->resize(400,300);
        auto native=new QVBoxLayout(slot);
        QPointer<QLabel> content=new QLabel("Native pane"); native->addWidget(content);
        main.show(); secondary.show(); LayoutTest::settle();
        require(panePageContext(slot,&main)==root,"Auxiliary pane did not resolve its stable logical page");
        SidebarFloatSession session;
        require(session.start(slot,"Fixture",panePageContext(slot,&main),false,false,false,false).isEmpty(),
            "Auxiliary pane could not float");
        auto host=session.floatingWindow(); host->setGeometry(100,100,420,320); LayoutTest::settle();
        const auto geometry=host->geometry();
        for(int round=0;round<2;++round) {
            slot->setParent(root); slot->show(); secondary.hide(); LayoutTest::settle();
            require(session.active() && host->isVisible() && content->window()==host && host->geometry()==geometry,
                "Leaving Dual Screen lost floating content, visibility or geometry");
            page->hide(); LayoutTest::settle();
            require(!host->isVisible(),"Floating auxiliary pane ignored actual page departure");
            page->show(); LayoutTest::settle();
            require(host->isVisible() && host->geometry()==geometry,"Logical page return lost its floating pane");
            secondary.show(); slot->setParent(auxiliary); slot->show(); LayoutTest::settle();
            require(host->isVisible() && panePageContext(slot,&main)==root,"Returning to Dual Screen changed page context");
        }
        slot->setParent(root); slot->show(); secondary.hide(); LayoutTest::settle();
        require(session.dock().isEmpty(),"Pane could not dock after leaving Dual Screen");
        require(content && content->parentWidget()==slot && slot->layout()==native,
            "Screen-mode changes broke reversible layout ownership");
    }
    std::puts("PASS: all catalogue entries resolve stable page context; Edit/Color/Fairlight hosts survive screen-mode moves and dock reversibly");
    // Startup directly in Dual Screen must discover panes outside main's tree.
    QTemporaryDir directory;
    const auto previous=qgetenv("UNBOUND_SETTINGS_FILE");
    qputenv("UNBOUND_SETTINGS_FILE",directory.filePath("settings.json").toUtf8());
    QWidget main; main.setObjectName("UiMainWindow"); main.resize(700,500);
    auto page=new QWidget(&main); page->setObjectName("WStackPage_Conform"); page->resize(700,500);
    auto root=new QWidget(page); root->setObjectName("frameEditPageWidgetsContainer"); root->resize(700,500);
    QWidget secondary; secondary.setObjectName("UiEditPanelAux"); secondary.resize(500,400);
    auto column=new QVBoxLayout(&secondary);
    auto slot=new QWidget; slot->setObjectName("frameEditPageMediaPool"); column->addWidget(slot);
    auto content=new QVBoxLayout(slot); content->addWidget(new QLabel("Media Pool"));
    main.show(); secondary.show(); LayoutTest::settle();
    require(startController(),"Auxiliary startup controller failed"); LayoutTest::settle();
    require(slot->findChild<QToolButton *>("resolvePaneUndockButton"),"Startup discovery missed auxiliary Media Pool");
    delete qApp->findChild<QObject *>("UnboundPaneController");
    if(previous.isNull())qunsetenv("UNBOUND_SETTINGS_FILE");else qputenv("UNBOUND_SETTINGS_FILE",previous);
    std::puts("PASS: controller startup discovers catalogued auxiliary panes outside the main window");
}
