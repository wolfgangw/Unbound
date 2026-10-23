// SPDX-License-Identifier: MIT
#include "sidebarfloatsession.h"
#include "panedecoration.h"
#include "layoutadapter.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QSlider>
using LayoutTest::require;
using LayoutTest::settle;
static void testColorClipsChrome() {
    QWidget page; auto column=new QVBoxLayout(&page);
    auto bar=new QWidget; bar->setObjectName("frameColorTopBar");bar->setFixedHeight(36);column->addWidget(bar);
    auto nativeRow=new QHBoxLayout(bar);
    auto left=new QWidget;left->setObjectName("frameColorTopBarLeftInSSM");nativeRow->addWidget(left);
    auto right=new QWidget;right->setObjectName("frameColorTopBarRightInSSM");nativeRow->addWidget(right);
    auto l=new QHBoxLayout(left);auto r=new QHBoxLayout(right);
    auto gallery=new QPushButton("Gallery");gallery->setObjectName("btnColorToggleGallery");gallery->setCheckable(true);l->addWidget(gallery);
    auto controls=new QPushButton("Color Controls");controls->setObjectName("btnColorToggleColorControls");l->addWidget(controls);
    auto nodes=new QPushButton("Nodes");nodes->setObjectName("btnColorToggleNodeGraph");nodes->setCheckable(true);r->addWidget(nodes);
    auto effects=new QPushButton("Effects");effects->setObjectName("btnColorToggleOpenFX");effects->setCheckable(true);r->addWidget(effects);
    auto mode=new QPushButton("Lightbox");mode->setObjectName("btnColorToggleLightbox");r->addWidget(mode);
    auto root=new QWidget;column->addWidget(root);
    QWidget sidebar(root),graph(root),ofx(root),divider(root),ofxDivider(root);
    sidebar.setObjectName("m_pFrameGalleryContainer");sidebar.setGeometry(0,0,260,400);sidebar.setMinimumWidth(100);
    graph.setObjectName("m_pFrameNodeGraph");graph.setGeometry(700,0,300,400);graph.setMinimumWidth(100);
    ofx.setObjectName("m_pFrameOFXPlugin");ofx.setGeometry(1000,0,240,400);ofx.setMinimumWidth(100);
    divider.setObjectName("leftSideBarRightAnchor_Splitter");divider.resize(7,400);
    ofxDivider.setObjectName("ofxLeftAnchor_Splitter");ofxDivider.resize(100,30);
    page.resize(1400,700);page.show();settle();
    int clicks=0;QObject::connect(gallery,&QPushButton::clicked,[&]{++clicks;});
    Unbound::ColorClipsToolbar toolbar(&page);
    require(toolbar.start(&page),"Color toolbar row could not start");settle();
    require(column->count()==3 && controls->parentWidget()==left && mode->parentWidget()==right,
        "Added Color row displaced native Lightbox controls");
    require(gallery->parentWidget()!=left && nodes->parentWidget()!=right,"Native Color controls did not transfer");
    gallery->hide();settle();require(gallery->isVisible(),"Native hide removed transferred toolbar control");
    gallery->click();require(clicks==1,"Transferred toolbar lost native button connection");
    Unbound::ColorClipsSidebars panels;int changes=0;
    panels.capture(&page,root,&page,[&]{++changes;});
    nodes->setChecked(true);effects->setChecked(true);
    const QRect allocation(0,0,1400,400);const auto viewer=panels.apply(allocation);
    require(sidebar.isVisible() && graph.isVisible() && ofx.isVisible(),"Requested Color sidebars not visible");
    require(sidebar.geometry().right()<viewer.left() && viewer.right()<graph.x() && graph.geometry().right()<ofx.x(),
        "Color sidebars overlap viewer or each other");
    require(ofxDivider.width()==7,"Uninitialized native divider obscures Effects");
    gallery->setChecked(false);nodes->setChecked(false);effects->setChecked(false);
    require(panels.apply(allocation)==allocation && sidebar.isHidden() && graph.isHidden() && ofx.isHidden(),
        "Hidden Color sidebars did not release viewer space");
    panels.restore();const int before=changes;gallery->setChecked(true);
    require(changes==before,"Stopped Color sidebars retained toggle callbacks");
    toolbar.restore();settle();
    require(column->count()==2 && l->indexOf(gallery)==0 && r->indexOf(nodes)==0 && r->indexOf(effects)==1,
        "Color toolbar did not restore original native ownership/order");
}
class NativeColorDivider : public QWidget {
public:
    QWidget *panel=nullptr; bool *ready=nullptr; bool left=false,pressed=false;
    int presses=0,releases=0,startWidth=0,startX=0,startHandleX=0;
    bool event(QEvent *event) override {
        if(event->type()==QEvent::MouseButtonPress) {
            require(*ready,"Native divider press arrived before queued normal layout");
            auto mouse=static_cast<QMouseEvent *>(event);
            pressed=true;++presses;startX=mouse->globalX();startWidth=panel->width();startHandleX=x();return true;
        }
        if(event->type()==QEvent::MouseMove && pressed) {
            auto mouse=static_cast<QMouseEvent *>(event);
            panel->resize(startWidth+(left?1:-1)*(mouse->globalX()-startX),panel->height());move(startHandleX+mouse->globalX()-startX,y());return true;
        }
        if(event->type()==QEvent::MouseButtonRelease) {pressed=false;++releases;return true;}
        return QWidget::event(event);
    }
};
static void testColorDividerGestures() {
    QWidget main,host;main.resize(1400,700);host.resize(400,300);
    QWidget page(&main),root(&page);page.resize(1400,700);root.resize(1400,650);
    QWidget player(&root),region(&root),clips(&region);
    player.setObjectName("m_pFramePlayerContainer");region.setObjectName("m_pTimeLineFrame");clips.setObjectName("m_pLightBox");
    player.setGeometry(0,0,1400,300);region.setGeometry(0,300,1400,100);
    QPushButton mode(&page),controls(&page);mode.setObjectName("btnColorToggleLightbox");controls.setObjectName("btnColorToggleColorControls");
    mode.setCheckable(true);controls.setCheckable(true);
    bool ready=true;QList<NativeColorDivider *> handles;
    const char *panels[]={"m_pFrameGalleryContainer","m_pFrameNodeGraph","m_pFrameOFXPlugin"};
    const char *dividers[]={"leftSideBarRightAnchor_Splitter","nodesLeftAnchor_Splitter","ofxLeftAnchor_Splitter"};
    const char *toggles[]={"btnColorToggleGallery","btnColorToggleNodeGraph","btnColorToggleOpenFX"};
    for(int i=0;i<3;++i) {
        auto panel=new QWidget(&root);panel->setObjectName(panels[i]);panel->setGeometry(i*250,0,220,300);panel->setMinimumWidth(100);
        auto toggle=new QPushButton(&page);toggle->setObjectName(toggles[i]);toggle->setCheckable(true);toggle->setChecked(true);
        auto handle=new NativeColorDivider;handle->setParent(&root);handle->setObjectName(dividers[i]);handle->resize(7,300);
        handle->panel=panel;handle->ready=&ready;handle->left=i==0;handles.append(handle);
    }
    QObject::connect(&mode,&QPushButton::toggled,&page,[&](bool on){
        ready=false;region.setGeometry(on?QRect(0,0,1400,500):QRect(0,300,1400,100));
        QTimer::singleShot(0,&page,[&,on]{ready=!on;});
    });
    main.show();host.show();page.show();root.show();settle();
    Unbound::ColorClipsAdapter adapter(&page,&main);require(adapter.start(&host),"Divider adapter failed to start");settle();
    auto send=[](NativeColorDivider *handle,QEvent::Type type,QPoint global){
        QMouseEvent mouse(type,handle->mapFromGlobal(global),global,
            type==QEvent::MouseMove?Qt::NoButton:Qt::LeftButton,
            type==QEvent::MouseButtonRelease?Qt::NoButton:Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(handle,&mouse);
    };
    for(auto handle:handles) {
        const int before=handle->panel->width();const auto start=handle->mapToGlobal(QPoint(3,30));
        send(handle,QEvent::MouseButtonPress,start);
        adapter.suspendNativeMode(false);
        require(!mode.isChecked(),"Routine restore resumed Lightbox during native divider gesture");
        require(!main.updatesEnabled() && !page.updatesEnabled() && !host.updatesEnabled(),"Divider did not freeze top-level backing store and host");
        auto guide=main.findChild<QWidget *>("unboundColorDividerGuide");
        require(guide && guide->isWindow() && guide->updatesEnabled() && guide->testAttribute(Qt::WA_ShowWithoutActivating)
            && guide->testAttribute(Qt::WA_TransparentForMouseEvents),"Divider guide cannot paint independently or intercepts input");
        const int guideStart=guide->x();
        const auto end=start+QPoint(handle->left?40:-40,0);
        send(handle,QEvent::MouseMove,end);settle();
        require(guide->x()==guideStart+(handle->left?40:-40),"Divider guide did not follow native movement");
        send(handle,QEvent::MouseButtonRelease,end);settle();
        require(!main.findChild<QWidget *>("unboundColorDividerGuide"),"Divider guide survived release");
        require(handle->panel->width()==before+40 && mode.isChecked(),"Native divider width not retained on Lightbox return");
        require(main.updatesEnabled() && page.updatesEnabled() && host.updatesEnabled() && !handle->pressed && handle->releases==1,
            "Divider release left frozen repaint or native gesture");
    }
    auto handle=handles.first();auto start=handle->mapToGlobal(QPoint(3,30));
    const int presses=handle->presses;
    send(handle,QEvent::MouseButtonPress,start);
    QEvent deactivate(QEvent::ApplicationDeactivate);QApplication::sendEvent(qApp,&deactivate);settle();
    require(handle->presses==presses && main.updatesEnabled() && page.updatesEnabled() && host.updatesEnabled(),"Cancelled queued press was delivered or froze repaint");
    start=handle->mapToGlobal(QPoint(3,30));send(handle,QEvent::MouseButtonPress,start);settle();
    require(handle->pressed,"Native press missing before interruption test");
    page.hide();settle();
    require(!handle->pressed && main.updatesEnabled() && page.updatesEnabled() && host.updatesEnabled(),"Page leave retained native drag or frozen host");
    page.show();settle();require(mode.isChecked(),"Page return failed to resume floating Clips mode");
    start=handle->mapToGlobal(QPoint(3,30));send(handle,QEvent::MouseButtonPress,start);settle();
    QEvent ungrab(QEvent::UngrabMouse);QApplication::sendEvent(handle,&ungrab);settle();
    require(!handle->pressed && main.updatesEnabled() && page.updatesEnabled() && host.updatesEnabled() && mode.isChecked(),
        "Mouse-grab loss left native gesture or repaint suspended");
    start=handle->mapToGlobal(QPoint(3,30));send(handle,QEvent::MouseButtonPress,start);adapter.stop();settle();
    require(main.updatesEnabled() && page.updatesEnabled() && host.updatesEnabled() && !handle->pressed,"Docking with queued gesture left repaint frozen");
    std::puts("PASS: Color native divider deferred input, all three widths, repaint cleanup, app/page interruption and docking");
}
void testColorClips() {
    testColorClipsChrome();
    testColorDividerGestures();
    QWidget main; main.resize(900,700);
    QWidget page(&main); page.setObjectName("WStackPage_Color"); page.setGeometry(0,40,900,660);
    QWidget root(&page); root.setGeometry(0,0,900,660);
    QWidget player(&root); player.setObjectName("m_pFramePlayerContainer"); player.setGeometry(0,0,900,300);
    QWidget image(&player); image.setGeometry(0,0,100,100);
    QWidget region(&root); region.setObjectName("m_pTimeLineFrame"); region.setGeometry(0,300,900,140);
    QWidget slot(&region); slot.setObjectName("m_pLightBox"); slot.setGeometry(0,0,900,140);
    auto native=new QVBoxLayout(&slot); native->setContentsMargins(0,0,0,0);
    auto content=new QWidget; content->setObjectName("frameLightBoxBorder");
    auto contentLayout=new QVBoxLayout(content);
    auto header=new QWidget(content); header->setObjectName("topBorderFrame"); header->setFixedHeight(24);
    auto row=new QHBoxLayout(header); row->setContentsMargins(0,0,0,0); row->addStretch();
    auto zoom=new QSlider(Qt::Horizontal); row->addWidget(zoom);
    contentLayout->addWidget(header); contentLayout->addWidget(new QPushButton("Clip")); native->addWidget(content);
    QWidget timeline(&region); timeline.setObjectName("timelineFrame"); timeline.setFixedHeight(72); timeline.hide();
    auto timelineLayout=new QHBoxLayout(&timeline); timelineLayout->setContentsMargins(0,0,0,0);
    auto tracks=new QWidget; timelineLayout->addWidget(tracks);
    QPushButton timelineToggle(&main); timelineToggle.setObjectName("btnColorToggleTimeline"); timelineToggle.setCheckable(true);
    QPushButton mode(&main),controls(&main);
    mode.setObjectName("btnColorToggleLightbox"); controls.setObjectName("btnColorToggleColorControls");
    mode.setCheckable(true); controls.setCheckable(true);
    QObject::connect(&mode,&QPushButton::toggled,[&](bool on) {
        player.setVisible(!on); region.show();
        timeline.setMinimumHeight(on ? 0 : 72); timeline.setMaximumHeight(on ? QWIDGETSIZE_MAX : 72); timeline.hide();
        region.setGeometry(on ? QRect(0,0,900,440) : QRect(0,300,900,140));
    });
    Unbound::SidebarFloatSession session;
    Unbound::PaneDecoration decoration(&slot,&main,[] {});
    main.show(); page.show(); root.show(); player.show(); region.show(); slot.show(); settle();
    auto button=decoration.button();
    require(button && button->parentWidget()==content && content->contentsMargins().left()==32,
        "Clips lacks its reserved leading gutter");
    require(button->x()==6 && button->y()==10,"Docked Clips button is not aligned in its gutter");
    const auto original=player.geometry();
    require(session.start(&slot,"Clips",&page,false,false,true,false).isEmpty(),"Clips failed to float");
    decoration.setFloating(true); settle();
    auto host=static_cast<Unbound::PaneFloatingWindow *>(session.floatingWindow());
    require(button->y()==16,"Floating Clips button lost its decoration-row offset");
    require(host && content->window()==host && button->window()==host,"Clips gutter did not transfer with native content");
    require(mode.isChecked() && controls.isChecked() && player.isVisible() && region.isHidden(),"Clips native mode/adapter not enabled");
    require(image.isVisible(),"Viewer descendants remained hidden after Lightbox entry");
    require(player.geometry()==region.geometry(),"Viewer did not reclaim Lightbox allocation");
    if(QGuiApplication::platformName()!=QLatin1String("offscreen")) {
        auto gesture=[&](QWidget *surface,bool expected) {
            const QPoint start(2,surface->height()/2);
            auto send=[&](QEvent::Type type,QPoint point,Qt::MouseButton b,Qt::MouseButtons held) {
                QMouseEvent event(type,point,surface->mapToGlobal(point),b,held,Qt::NoModifier);
                QApplication::sendEvent(surface,&event);
            };
            send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
            send(QEvent::MouseMove,start+QPoint(QApplication::startDragDistance()+1,0),Qt::NoButton,Qt::LeftButton);
            require(host->interactiveDragActive()==expected,"Clips header drag/control exclusion failed");
            send(QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);
            require(!host->interactiveDragActive(),"Clips header retained mouse drag");
        };
        gesture(header,true); gesture(zoom,false);
        auto gutter=content->findChild<QWidget *>("unboundClipsGutter",Qt::FindDirectChildrenOnly);
        require(gutter && gutter->width()==32 && gutter->height()==content->height(),"Clips gutter drag surface has wrong bounds");
        gesture(gutter,true);
    }
    session.suspendClipsMode(true); settle();
    require(session.active() && !mode.isChecked(),"Layout preparation docked Clips");
    session.suspendClipsMode(false); settle();
    require(mode.isChecked() && session.floatingWindow()==host,"Layout recall failed to resume retained Clips");
    // Simulate Resolve assigning its native Lightbox allocation again.
    region.setGeometry(0,0,800,380); player.hide(); region.show(); settle();
    require(player.isVisible() && region.isHidden() && player.geometry()==region.geometry(),"Native relayout displaced adapted viewer");
    require(image.isVisible(),"Native hide propagation left black viewer descendants");
    timelineToggle.setChecked(true);
    region.setGeometry(0,0,800,380); region.show(); settle();
    require(timeline.isVisible() && region.height()==72 && player.height()==308,
        "Docked Timeline did not retain its allocation beside floating Clips");
    require(tracks->height()==timeline.contentsRect().height(),"Timeline tracks clipped after allocation correction");
    timelineToggle.setChecked(false); region.setGeometry(0,0,800,380); settle();
    require(region.isHidden() && player.height()==380,"Hidden Timeline did not release viewer space");
    page.hide(); settle();
    require(session.active() && !host->isVisible(),"Page departure docked or leaked Clips");
    page.show(); settle();
    require(session.floatingWindow()==host && host->isVisible() && player.isVisible(),"Page return did not retain Clips host/adapter");
    page.hide(); settle();
    require(session.dock().isEmpty(),"Hidden-page Clips docking failed"); decoration.setFloating(false); settle();
    require(mode.isChecked(),"Hidden-page docking changed native mode prematurely");
    page.show(); settle();
    require(!mode.isChecked() && !controls.isChecked() && content->parentWidget()==&slot,"Docking failed to restore native mode/ownership");
    require(player.geometry()==original,"Docking failed to restore original viewer geometry");
    require(session.start(&slot,"Clips",&page,false,false,true,false).isEmpty(),"Repeat Clips float failed");
    decoration.setFloating(true); settle(); mode.click(); settle();
    require(!session.active() && !mode.isChecked() && content->parentWidget()==&slot,"Native Lightbox exit left floating strip");
    std::puts("PASS: Color Clips gutter, native mode, allocation recovery, retained page host, hidden docking and native exit");
}
#include "controller.h"
#include "panesettings.h"
#include "pagelayoutmenu.h"
#include <QContextMenuEvent>
#include <QTemporaryDir>

void testColorClipsStartup() {
    QTemporaryDir directory;
    qputenv("UNBOUND_SETTINGS_FILE",directory.filePath("clips-startup.json").toUtf8());
    Unbound::PaneSettings seed;
    QString savedName; bool duplicate=false;
    seed.saveFloating("Color/m_pFrameNodeGraph",true);
    seed.saveFloating("Color/m_pWidgetLutBrowser",true);
    seed.saveToggle("Color/m_pFrameNodeGraph",false);
    seed.saveToggle("Color/m_pWidgetLutBrowser",false);
    require(seed.saveLayout("WStackPage_Color",savedName,duplicate).isEmpty(),"Cannot seed Nodes/LUTs layout");
    const auto nodesLayout=seed.activeLayout("WStackPage_Color");
    seed.saveFloating("Color/m_pLightBox",true);
    seed.saveToggle("Color/m_pLightBox",false);
    require(seed.saveLayout("WStackPage_Color",savedName,duplicate).isEmpty(),"Cannot seed Nodes/Clips layout");
    const auto nodesClipsLayout=seed.activeLayout("WStackPage_Color");
    seed.saveFloating("Color/m_pFrameNodeGraph",false);
    seed.saveFloating("Color/m_pWidgetLutBrowser",false);
    seed.saveFloating("Color/m_pLightBox",false);
    seed.saveFloating("Color/timelineFrame",true);
    seed.saveToggle("Color/timelineFrame",false);
    require(seed.saveLayout("WStackPage_Color",savedName,duplicate).isEmpty(),"Cannot seed Timeline layout");
    const auto timelineLayout=seed.activeLayout("WStackPage_Color");
    for(const auto &name:{"m_pWidgetGallery","m_pWidgetLutBrowser","timelineFrame","m_pLightBox"}) {
        require(seed.saveFloating(QStringLiteral("Color/")+name,true).isEmpty(),"Cannot seed Color startup state");
        require(seed.saveToggle(QStringLiteral("Color/")+name,false).isEmpty(),"Cannot seed Color toggle state");
    }
    require(seed.saveLayout("WStackPage_Color",savedName,duplicate).isEmpty(),"Cannot seed Clips layout");
    const auto clipsLayout=seed.activeLayout("WStackPage_Color");
    QWidget main; main.setObjectName("UiMainWindow"); main.resize(900,700);
    QWidget page(&main); page.setObjectName("WStackPage_Color"); page.setGeometry(0,0,900,700);
    QWidget root(&page); root.setGeometry(0,40,900,660);
    QWidget player(&root); player.setObjectName("m_pFramePlayerContainer"); player.setGeometry(0,0,900,300);
    QWidget region(&root); region.setObjectName("m_pTimeLineFrame"); region.setGeometry(0,300,900,140);
    QWidget slot(&region); slot.setObjectName("m_pLightBox");
    auto native=new QVBoxLayout(&slot);
    auto content=new QWidget; content->setObjectName("frameLightBoxBorder"); native->addWidget(content);
    QPushButton mode(&page),controls(&page);
    mode.setObjectName("btnColorToggleLightbox"); controls.setObjectName("btnColorToggleColorControls");
    mode.setCheckable(true); controls.setCheckable(true);
    QList<QPushButton *> toggles;
    auto addPane=[&](const QString &name,const QString &toggleName,const QString &headerName) {
        auto pane=new QWidget(&page); pane->setObjectName(name);
        auto layout=new QVBoxLayout(pane);
        auto body=new QWidget; layout->addWidget(body);
        body->setObjectName(name=="timelineFrame" ? "m_pTimeLinePnl" : "nativeContent");
        if(!headerName.isEmpty()) {
            auto column=new QVBoxLayout(body);
            auto header=new QWidget; header->setObjectName(headerName); column->addWidget(header);
            auto row=new QHBoxLayout(header);
            auto options=new QPushButton("..."); options->setObjectName("btnOptions"); row->addWidget(options);
        }
        auto toggle=new QPushButton(&page); toggle->setObjectName(toggleName); toggle->setCheckable(true);
        toggles.append(toggle);
        QObject::connect(toggle,&QPushButton::toggled,pane,[pane](bool on) {pane->setVisible(on);});
        pane->hide(); toggle->setVisible(!mode.isChecked());
    };
    addPane("m_pWidgetGallery","btnColorToggleGallery","frameTopColorGallery");
    addPane("m_pWidgetLutBrowser","btnColorToggleLutsBrowser","frameLutBrowserTitleBarTop");
    addPane("m_pFrameNodeGraph","btnColorToggleNodeGraph",{});
    QObject::connect(&mode,&QPushButton::toggled,[&](bool on) {
        for(auto toggle:toggles) toggle->setVisible(!on);
        player.setVisible(!on); region.show();
        region.setGeometry(on ? QRect(0,0,900,440) : QRect(0,300,900,140));
    });
    main.show(); page.show(); root.show(); region.show(); slot.show(); mode.show(); controls.show();
    mode.setChecked(true); settle();
    require(Unbound::startController(),"Cannot start Color restoration controller");
    for(int i=0;i<8;++i) settle();
    auto floating=[](const QString &name) -> QWidget * {
        for(auto window:QApplication::topLevelWidgets())
            if(window->objectName()==QStringLiteral("unboundFloating_")+name) return window;
        return nullptr;
    };
    require(!mode.isChecked(),"Startup Lightbox prevented ordinary pane initialization");
    require(floating("m_pWidgetGallery") && floating("m_pWidgetLutBrowser"),"Color sidebars did not restore before Clips");
    require(!floating("m_pLightBox"),"Clips entered Lightbox before a late saved pane appeared");
    addPane("timelineFrame","btnColorToggleTimeline",{});
    for(int i=0;i<8;++i) settle();
    for(const auto &name:{"m_pWidgetGallery","m_pWidgetLutBrowser","timelineFrame","m_pLightBox"})
        require(floating(name) && floating(name)->isVisible(),"Saved Color host missing after startup barrier");
    require(mode.isChecked(),"Clips failed to resume native Lightbox after restoration");
    auto button=floating("m_pLightBox")->findChild<QToolButton *>("resolvePaneUndockButton");
    require(button,"Missing Clips context control");
    QContextMenuEvent context(QContextMenuEvent::Mouse,QPoint(2,2),button->mapToGlobal(QPoint(2,2)));
    QApplication::sendEvent(button,&context); settle();
    auto menu=main.findChild<Unbound::PageLayoutMenu *>("unboundLayoutMenu");
    require(menu,"Missing Color layout menu");
    bool applied=false;
    require(menu->recall(nodesLayout,applied).isEmpty() && applied,"Cannot recall Nodes/LUTs layout");
    for(int i=0;i<8;++i) settle();
    require(floating("m_pFrameNodeGraph") && floating("m_pWidgetLutBrowser"),"Leaving floating Clips restored LUTs but lost Nodes");
    require(!floating("m_pLightBox"),"Incoming layout left Clips floating");
    require(!mode.isChecked() && player.isVisible(),"Ordinary layout restored Lightbox instead of the Color viewer");
    require(menu->recall(clipsLayout,applied).isEmpty() && applied,"Cannot recall Clips layout");
    for(int i=0;i<8;++i) settle();
    for(const auto &name:{"m_pWidgetGallery","m_pWidgetLutBrowser","timelineFrame","m_pLightBox"})
        require(floating(name) && floating(name)->isVisible(),"Return to Clips layout lost a host");
    auto retained=floating("m_pLightBox");
    require(menu->recall(nodesClipsLayout,applied).isEmpty() && applied,"Cannot recall Nodes with retained Clips");
    for(int i=0;i<8;++i) settle();
    require(floating("m_pLightBox")==retained && floating("m_pFrameNodeGraph")
        && floating("m_pWidgetLutBrowser") && mode.isChecked(),"Retained Clips blocked incoming Nodes");
    require(menu->recall(nodesLayout,applied).isEmpty() && applied,"Cannot repeat ordinary layout recall");
    for(int i=0;i<8;++i) settle();
    require(!floating("m_pLightBox") && floating("m_pFrameNodeGraph") && floating("m_pWidgetLutBrowser"),
        "Repeated Clips-to-ordinary recall lost a host");
    require(!mode.isChecked() && player.isVisible(),"Repeated ordinary recall left native Lightbox active");
    require(menu->recall(timelineLayout,applied).isEmpty() && applied,"Cannot recall Timeline-only layout");
    for(int i=0;i<8;++i) settle();
    require(floating("timelineFrame") && !floating("m_pLightBox") && !mode.isChecked() && player.isVisible(),
        "Timeline-only layout did not restore normal Color presentation");
    mode.click(); settle();
    require(menu->recall(timelineLayout,applied).isEmpty(),"Cannot reselect current ordinary layout");
    for(int i=0;i<8;++i) settle();
    require(!mode.isChecked() && player.isVisible(),"Reselecting ordinary layout retained native Lightbox");
    delete qApp->findChild<QObject *>("UnboundPaneController");
    std::puts("PASS: Color Lightbox startup, late Timeline, first Nodes/LUTs recall, return and retained Clips transitions");
}
