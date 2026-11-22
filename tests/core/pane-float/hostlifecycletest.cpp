// SPDX-License-Identifier: MIT
#include "sidebarfloatsession.h"
#include "layoutadapter.h"
#include "panepagecontext.h"
#include "panesettings.h"
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
    for(const auto &name:{"frameEditPageMediaPool","frameEditPageEditIndex","m_pWidgetMediaPool","frameAudioPanelIndexContainer","frameMediaPageMediaPool"}) {
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
    std::puts("PASS: catalogued hosts survive screen-mode moves and dock reversibly");
    // Startup directly in Dual Screen must discover panes outside main's tree.
    QTemporaryDir directory;
    const auto previous=qgetenv("UNBOUND_SETTINGS_FILE");
    qputenv("UNBOUND_SETTINGS_FILE",directory.filePath("settings.json").toUtf8());
    {
        QWidget mediaPage;
        QWidget regions(&mediaPage), viewer(&regions), slot(&regions);
        QToolButton demand(&mediaPage);
        demand.setCheckable(true);
        int selectionChanges=0;
        QObject::connect(&demand,&QToolButton::toggled,&regions,[&] { ++selectionChanges; });
        auto arrange=[&] {
            const int right=demand.isChecked()?170:0;
            viewer.setGeometry(0,0,regions.width()-right,200);
            slot.setGeometry(viewer.width(),0,right,200);
            slot.setVisible(demand.isChecked());
        };
        QObject::connect(&demand,&QToolButton::toggled,&regions,arrange);
        regions.resize(640,200); mediaPage.show(); demand.setChecked(true); arrange();
        {
            MediaMetadataSpace allocation(&slot,&mediaPage,&demand);
            allocation.setExpanded(false);
            require(demand.isChecked() && slot.isHidden() && viewer.width()==640,
                "Live Media Metadata retained its native sidebar allocation");
            const int changes=selectionChanges;
            arrange(); LayoutTest::settle();
            require(slot.isHidden() && viewer.width()==640 && selectionChanges==changes,
                "A repeated native allocation unnecessarily toggled Metadata");
            regions.resize(800,200); arrange(); LayoutTest::settle();
            require(demand.isChecked() && slot.isHidden() && viewer.width()==800,
                "Native resize reintroduced the empty Metadata column");
            const int beforeDrag=selectionChanges;
            MainWindowResizeEvent::notify(&mediaPage,true);
            for(int width:{840,900,800}) {
                regions.resize(width,200);
                arrange();
                require(slot.isHidden() && viewer.width()==width, "Native resize exposed the Metadata placeholder");
                LayoutTest::settle();
                require(selectionChanges==beforeDrag+1 && !demand.isChecked(), "Metadata was remeasured during a resize gesture");
            }
            MainWindowResizeEvent::notify(&mediaPage,false);
            LayoutTest::settle();
            require(selectionChanges==beforeDrag+2 && demand.isChecked() && slot.isHidden() && viewer.width()==800,
                "Ending the resize failed to restore Metadata delivery once at the final allocation");
            mediaPage.hide(); LayoutTest::settle();
            require(!demand.isChecked(),"Media kept requesting Metadata on another page");
            mediaPage.show(); LayoutTest::settle();
            require(demand.isChecked() && slot.isHidden() && viewer.width()==800,
                "Media page return lost Metadata delivery or its reclaimed space");
            demand.click(); LayoutTest::settle();
            require(slot.isVisible() && viewer.width()==630 && NativeToggleState::checked(&demand),
                "Explicit reveal did not restore native Metadata allocation");
        }
    }
    const QString metadataKey="Media/frameMediaPageMetadataWidget";
    PaneSettings seed;
    require(seed.saveFloating(metadataKey,true).isEmpty() && seed.saveToggle(metadataKey,false).isEmpty(),
        "Could not seed a floating Metadata panel with its docked sidebar collapsed");
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
    // Media's Dual Screen toolbar can be in main while its pane lives on the peer.
    page->hide(); secondary.hide();
    auto media=new QWidget(&main); media->setObjectName("WStackPage_Browse");
    auto selector=new QToolButton(media);
    selector->setObjectName("btnMediaToggleInspectorPanelVisibilityDSM");
    selector->setCheckable(true); selector->setChecked(true);
    QWidget mediaPeer; mediaPeer.setObjectName("UiMediaPanelAux");
    auto inspector=new QWidget(&mediaPeer); inspector->setObjectName("frameMediaPageInspector");
    (new QVBoxLayout(&mediaPeer))->addWidget(inspector);
    mediaPeer.resize(500,400);
    auto title=new QWidget; title->setObjectName("frameEffectsWidgetsTitle");
    (new QVBoxLayout(inspector))->addWidget(title);
    auto options=new QToolButton; options->setObjectName("btnTitleOptions");
    (new QHBoxLayout(title))->addWidget(options);
    QObject::connect(selector,&QToolButton::clicked,inspector,&QWidget::setVisible);
    media->show(); mediaPeer.show(); LayoutTest::settle();
    auto button=title->findChild<QToolButton *>("resolvePaneUndockButton");
    require(button,"Media Inspector did not acquire its native header button");
    button->click(); LayoutTest::settle();
    require(!selector->isChecked() && inspector->isHidden() && title->isVisible()
        && title->window()->objectName()=="unboundFloating_frameMediaPageInspector",
        "Floating auxiliary Media Inspector lost its content or missed its primary toolbar");
    button->click(); LayoutTest::settle();
    require(selector->isChecked() && inspector->isVisible() && title->parentWidget()==inspector,
        "Docking auxiliary Media Inspector did not restore native content and visibility");
    auto metadata=new QWidget;
    metadata->setObjectName("frameMediaPageMetadataWidget"); mediaPeer.layout()->addWidget(metadata);
    auto metadataSelector=new QToolButton(media);
    metadataSelector->setObjectName("btnMediaToggleMetadataPanelVisibilityDSM"); metadataSelector->setCheckable(true);
    QObject::connect(metadataSelector,&QToolButton::toggled,metadata,&QWidget::setVisible);
    auto header=new QWidget;
    header->setObjectName("frameMetadataTop"); (new QVBoxLayout(metadata))->addWidget(header);
    auto presets=new QToolButton;
    presets->setObjectName("btnPresetsSelector"); (new QHBoxLayout(header))->addWidget(presets);
    metadata->hide(); metadataSelector->show(); LayoutTest::settle();
    auto metadataButton=header->findChild<QToolButton *>("resolvePaneUndockButton");
    require(metadataButton && metadataSelector->isChecked() && !NativeToggleState::checked(metadataSelector) && metadata->isHidden() && header->isVisible()
        && header->window()->objectName()=="unboundFloating_frameMediaPageMetadataWidget",
        "Restored Media Metadata did not release its native slot");
    // Resolve only publishes new Media selections while a native Metadata selector is on.
    auto clipSelection=new QToolButton(media);
    QObject::connect(clipSelection,&QToolButton::clicked,header,[=] {
        if(metadataSelector->isChecked()) header->setWindowTitle("Clip B");
    });
    clipSelection->click();
    require(header->windowTitle()=="Clip B" && metadata->isHidden(), "Reclaiming the sidebar stopped native clip delivery");
    metadataButton->click(); LayoutTest::settle();
    require(header->parentWidget()==metadata && metadataSelector->isChecked(),"Media Metadata failed to redock");
    metadataButton->click(); LayoutTest::settle();
    require(metadataSelector->isChecked() && !NativeToggleState::checked(metadataSelector) && metadata->isHidden() && header->isVisible()
        && header->window()->objectName()=="unboundFloating_frameMediaPageMetadataWidget",
        "Manual Media Metadata detachment left the native sidebar open");
    std::puts("PASS: Media Metadata reclaims its sidebar on startup and manual detachment, and restores it on docking");
    delete qApp->findChild<QObject *>("UnboundPaneController");
    if(previous.isNull())qunsetenv("UNBOUND_SETTINGS_FILE");else qputenv("UNBOUND_SETTINGS_FILE",previous);
    std::puts("PASS: controller startup discovers catalogued auxiliary panes outside the main window");
}
