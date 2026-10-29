// SPDX-License-Identifier: MIT
#include "editviewers.h"
#include <QLabel>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char *message) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
class ViewerApplication : public QApplication {
public:
    using QApplication::QApplication;
    QWidget *main=nullptr;
    int mainActivations=0;
    bool notify(QObject *object,QEvent *event) override {
        if(object==main && event->type()==QEvent::WindowActivate) ++mainActivations;
        return QApplication::notify(object,event);
    }
};
class ViewportFixture : public QWidget {
public:
    using QWidget::QWidget;
    int divider=100;
    QList<QSize> deliveredSizes;
protected:
    void resizeEvent(QResizeEvent *event) override {
        deliveredSizes.append(event->size());
        divider=qMin(divider,qMax(0,event->size().height()-90));
    }
};
class NativeRegionLayoutFixture : public QObject {
public:
    std::function<void()> layoutRegions;
    int passes=0;
protected:
    bool eventFilter(QObject *,QEvent *event) override {
        if(event->type()==QEvent::Resize) { ++passes; layoutRegions(); }
        return false;
    }
};
class MainWindowFixture : public QWidget {
public:
    int presentations=0;
protected:
    bool event(QEvent *event) override {
        if(event->type()==QEvent::UpdateRequest) ++presentations;
        return QWidget::event(event);
    }
};
// Model a native timeline whose divider is clamped when its viewport shrinks.
// Re-expanding afterwards cannot recover the user's previous divider position.
class TimelineFixture : public QWidget {
public:
    explicit TimelineFixture(QWidget *parent) : QWidget(parent) {
        auto layout=new QVBoxLayout(this); layout->setContentsMargins(0,0,0,0);
        viewport=new ViewportFixture(this); layout->addWidget(viewport);
    }
    ViewportFixture *viewport=nullptr;
    QList<QSize> deliveredSizes;
    int divider=100;
    QSize lastDelivered;
    bool consistentTransitions=true;
protected:
    void resizeEvent(QResizeEvent *event) override {
        deliveredSizes.append(event->size());
        if(lastDelivered.isValid() && event->oldSize()!=lastDelivered) consistentTransitions=false;
        lastDelivered=event->size();
        divider=qMin(divider,qMax(0,event->size().height()-90));
        QWidget::resizeEvent(event);
    }
};
int main(int argc,char **argv)
{
    ViewerApplication app(argc,argv);
    QTemporaryDir settingsDirectory;
    Unbound::PaneSettings settings(settingsDirectory.filePath("viewers.json"));
    MainWindowFixture main;
    app.main=&main;
    auto mainLayout=new QVBoxLayout(&main);
    auto page=new QWidget(&main); page->setObjectName("WStackPage_Conform"); mainLayout->addWidget(page);
    auto pl=new QVBoxLayout(page);
    auto root=new QWidget(page); root->setObjectName("frameEditPageWidgetsContainer"); pl->addWidget(root);
    auto players=new QWidget(root); players->setObjectName("frameEditPageVideoPlayers");
    auto timeline=new TimelineFixture(root); timeline->setObjectName("frameEditTimelinesHolder");
    auto splitter=new QWidget(root); splitter->setObjectName("timelineBarAnchor_Splitter");
    auto inspector=new QWidget(root); inspector->setObjectName("frameEditPageInspector");
    auto nativeLayout=new QHBoxLayout(players);
    auto source=new QStackedWidget(players); source->setObjectName("stackedEditSourceOfflinePlayers");
    auto program=new QWidget(players); program->setObjectName("widgetEditTimelinePlayer");
    for(int i=0;i<3;++i) source->addWidget(new QLabel(QString::number(i)));
    source->setCurrentIndex(2);
    auto header=[](QWidget *owner) {
        auto time=new QWidget(owner); time->setObjectName("framePlayerTime");
        auto right=new QWidget(time); right->setObjectName("rightFrame");
        auto row=new QHBoxLayout(right);
        auto options=new QToolButton(right); options->setObjectName("btnOptions"); row->addWidget(options);
    };
    header(program); header(source->currentWidget());
    nativeLayout->addWidget(source,3); nativeLayout->addWidget(program,7);
    main.resize(1000,600); main.show(); app.processEvents();
    auto nativeRegions=[&] {
        const int half=root->height()/2;
        players->setGeometry(0,0,root->width()-201,half-1);
        inspector->setGeometry(root->width()-200,0,200,half-1);
        timeline->setGeometry(0,half,root->width(),root->height()-half);
        splitter->setGeometry(0,half-4,root->width(),7); splitter->show();
    };
    nativeRegions();
    NativeRegionLayoutFixture regionLayout;
    regionLayout.layoutRegions=nativeRegions;
    root->installEventFilter(&regionLayout);
    NativeRegionLayoutFixture lateMainLayout;
    bool nativePaintAllowed=true;
    // A main-window handler may assign native regions after nested child
    // layouts/resize handlers have already returned to it.
    lateMainLayout.layoutRegions=[&] {
        nativeRegions(); nativePaintAllowed=root->updatesEnabled();
    };
    main.installEventFilter(&lateMainLayout);
    auto expandedRegion=[&] { return QRect(0,0,root->width()-201,root->height()); };
    int viewerMenus=0;
    auto experiment=new EditViewers(&main,players,program,source,settings,nullptr,
        [&](QWidget *,const QPoint &) { ++viewerMenus; });
    auto context=[&](QWidget *content) {
        auto button=content->findChild<QToolButton *>("resolvePaneUndockButton");
        require(button,"Viewer header did not receive an Unbound button");
        QContextMenuEvent event(QContextMenuEvent::Mouse,QPoint(5,5),button->mapToGlobal(QPoint(5,5)));
        QApplication::sendEvent(button,&event);
    };
    context(program); context(source);
    require(viewerMenus==2 && program->window()==&main && source->window()==&main,
        "Docked viewer context menus changed docking or failed to route");
    for(int pass=0;pass<2;++pass) {
        experiment->toggleProgram(); experiment->toggleSource(); app.processEvents();
        require(program->window()!=&main && source->window()!=&main,"Whole viewers were not floated");
        QApplication::setActiveWindow(&main); app.processEvents();
        const int beforeSource=app.mainActivations;
        QApplication::setActiveWindow(source->window()); app.processEvents();
        require(app.mainActivations==beforeSource+1,
            "Floating Source activation did not restore native page command context exactly once");
        require(QApplication::activeWindow()==source->window(),
            "Source activation bridge changed the active window");
        const int beforeProgram=app.mainActivations;
        QApplication::setActiveWindow(program->window()); app.processEvents();
        require(app.mainActivations==beforeProgram,
            "Program activation unexpectedly synthesized main activation");
        QApplication::setActiveWindow(source->window()); app.processEvents();
        require(app.mainActivations==beforeProgram+1,
            "Returning to floating Source did not restore native page command context");
        if(pass==0) {
            context(program); context(source);
            require(viewerMenus==4 && program->window()!=&main && source->window()!=&main,
                "Floating viewer context menus changed docking or failed to route");
        }
        require(source->currentIndex()==2 && source->count()==3,"Source alternatives changed during detachment");
        require(nativeLayout->count()==2,"Original nativeLayout not preserved");
        app.processEvents(); app.processEvents();
        require(!players->isHidden() && players->height()==0,"Empty native viewer region was not collapsed");
        require(program->isVisible() && source->isVisible(),"Collapsing the region hid floating viewers");
        require(timeline->geometry()==expandedRegion(),"Timeline did not reclaim the complete central region");
        require(inspector->height()==root->height() && !timeline->geometry().intersects(inspector->geometry()),"Inspector was covered by expanded timeline");
        require(splitter->isHidden(),"Native divider remains active during reclamation");
        timeline->divider=timeline->height()-150;
        const int savedDivider=timeline->divider;
        timeline->viewport->divider=timeline->viewport->height()-150;
        const int savedViewportDivider=timeline->viewport->divider;
        timeline->viewport->deliveredSizes.clear();
        timeline->deliveredSizes.clear();
        timeline->consistentTransitions=true; timeline->lastDelivered=timeline->size();
        main.resize(1100+pass*100,700+pass*100);
        require(timeline->geometry()==expandedRegion() && players->height()==0,
            "Late main-window layout left viewer space visible until the next queued callback");
        require(!nativePaintAllowed && root->updatesEnabled(),
            "Native resize could paint interim regions, or updates were not restored");
        require(program->updatesEnabled() && source->updatesEnabled(),
            "Main-window repaint transaction disabled floating viewer playback updates");
        app.processEvents();
        // Native sidebar toggles/docking can assign regions directly, without
        // resizing any retained ancestor. Their synchronous paints must not see
        // the temporary bottom-half timeline allocation.
        nativeRegions();
        require(!root->updatesEnabled(),"Pane layout change could paint temporary timeline geometry");
        const int presentations=main.presentations;
        QEvent presentation(QEvent::UpdateRequest);
        QApplication::sendEvent(&main,&presentation);
        require(main.presentations==presentations,
            "Main backing store presented cached pixels at temporary timeline geometry");
        require(program->updatesEnabled() && source->updatesEnabled(),
            "Pane layout transaction disabled floating viewer updates");
        app.processEvents(); app.processEvents();
        require(root->updatesEnabled() && timeline->geometry()==expandedRegion(),
            "Pane layout transaction failed to settle or restore painting");
        require(main.presentations>presentations,"Deferred main-window presentation was lost");
        root->setUpdatesEnabled(false);
        nativeRegions(); app.processEvents(); app.processEvents();
        require(!root->updatesEnabled(),"Reclamation enabled painting that was already disabled");
        root->setUpdatesEnabled(true);
        nativeRegions(); app.processEvents();
        require(timeline->geometry()==expandedRegion() && players->height()==0,"Native resize burst defeated reclamation");
        require(timeline->divider==savedDivider,"Transient native shrink clamped the timeline divider");
        require(timeline->viewport->divider==savedViewportDivider,
            "Qt child layout processed the native shrink before the timeline object filter");
        for(auto size:timeline->viewport->deliveredSizes)
            require(size.height()>root->height()/2,"Nested native viewport received the temporary half-height resize");
        require(timeline->consistentTransitions,"Timeline resize oldSize referred to a suppressed interim size");
        const int previousPasses=regionLayout.passes;
        root->resize(root->width()+17,root->height()+25);
        require(regionLayout.passes==previousPasses+1,"Native resize handler was delivered more than once");
        require(timeline->geometry()==expandedRegion() && players->height()==0,
            "Native resize returned with visible viewer space before queued work ran");
        require(timeline->viewport->divider==savedViewportDivider,
            "Synchronous native layout boundary clamped the nested viewport divider");
        app.processEvents();
        for(int burst=0;burst<4;++burst) { nativeRegions(); app.processEvents(); }
        require(timeline->divider==savedDivider && timeline->consistentTransitions,
            "Repeated native layout bursts moved the divider or broke resize continuity");
        for(auto size:timeline->deliveredSizes)
            require(size.height()>root->height()/2,"Timeline content received the temporary native half-height resize");
        nativeRegions();
        require(!root->updatesEnabled(),"Missing pending layout transaction before page hide");
        page->hide(); app.processEvents();
        require(root->updatesEnabled(),"Page hide left a pending layout transaction frozen");
        require(!program->isVisible() && !source->isVisible(),"Page hide did not hide hosts");
        require(players->height()>0 && timeline->y()>0,"Inactive page retained adapted geometry");
        page->show(); app.processEvents();
        require(program->isVisible() && source->isVisible(),"Page return did not restore hosts");
        require(timeline->geometry()==expandedRegion() && players->height()==0,"Page return lost reclamation");
        // Cinema hides the main window without a native resize. Its inherited
        // child Hide/Show events must preserve all pre-reclamation baselines.
        main.hide(); app.processEvents(); app.processEvents();
        main.show(); app.processEvents(); app.processEvents();
        require(timeline->geometry()==expandedRegion() && players->height()==0,
            "Main-window return lost dual-viewer reclamation");
        nativeRegions();
        require(!root->updatesEnabled(),"Missing pending layout transaction before viewer docking");
        experiment->dockBoth(); app.processEvents();
        require(root->updatesEnabled(),"Viewer docking left a pending layout transaction frozen");
        require(players->height()==root->height()/2-1 && timeline->y()==root->height()/2,"Docking did not restore latest native region geometry");
        require(!splitter->isHidden(),"Native divider was not restored");
        require(inspector->height()==root->height()/2-1,"Inspector native geometry was not restored");
        require(program->parentWidget()==players && source->parentWidget()==players,"Docking did not restore native parents");
        require(nativeLayout->itemAt(0)->widget()==source && nativeLayout->itemAt(1)->widget()==program,"Native order changed");
        require(nativeLayout->stretch(0)==3 && nativeLayout->stretch(1)==7,"Native stretches changed");
        require(!players->isHidden() && program->isVisible() && source->isVisible(),"Docked viewer visibility not restored");
    }
    experiment->toggleProgram(); experiment->toggleSource(); app.processEvents();
    program->window()->close(); app.processEvents();
    require(program->parentWidget()==players,"Closing a host did not dock");
    experiment->dockBoth(); app.processEvents();
    source->hide(); experiment->toggleProgram(); app.processEvents(); app.processEvents();
    require(players->height()==0,"Single Viewer Mode did not reclaim with only Program floating");
    for(int cycle=0;cycle<3;++cycle) {
        main.hide(); app.processEvents(); app.processEvents();
        main.show(); app.processEvents(); app.processEvents();
        require(players->height()==0 && timeline->geometry()==expandedRegion(),
            "Repeated Cinema visibility transitions lost single-viewer reclamation");
    }
    experiment->toggleProgram(); app.processEvents(); app.processEvents();
    require(players->height()==root->height()/2-1 && timeline->y()==root->height()/2,
        "Docking after main hide/show lost the native viewer region baseline");
    require(program->isVisible() && program->parentWidget()==players,
        "Docking after main hide/show did not show Program");
    experiment->toggleProgram(); app.processEvents(); app.processEvents();
    const auto hoverProgramWindow=program->window();
    const auto hoverProgramRect=hoverProgramWindow->geometry();
    program->hide(); source->show(); app.processEvents(); app.processEvents();
    require(program->isVisible() && program->window()==hoverProgramWindow && program->window()->geometry()==hoverProgramRect,
        "Single Viewer Source preview blanked or moved the floating Program viewer");
    require(players->height()>0 && source->isVisible(),"Enabling native Source viewer did not restore its region");
    source->hide(); program->show(); app.processEvents(); app.processEvents();
    require(players->height()==0,"Disabling native Source viewer did not reclaim the region again");
    experiment->toggleProgram(); app.processEvents();
    require(program->isVisible() && program->parentWidget()==players,
        "Docking after hover exit kept Program hidden because its native show was already satisfied");
    experiment->toggleProgram(); app.processEvents(); app.processEvents();
    program->hide(); source->show(); app.processEvents(); app.processEvents();
    require(program->isVisible(),"Repeated native hover swap blanked the floating Program viewer");
    page->hide(); app.processEvents();
    require(!hoverProgramWindow->isVisible(),"Program visibility preservation defeated page hiding");
    page->show(); app.processEvents(); app.processEvents();
    require(program->isVisible(),"Page return failed to retain the independent Program viewer");
    experiment->toggleProgram(); app.processEvents();
    require(program->isHidden() && source->isVisible(),
        "Docking during Source preview discarded Resolve's native Program visibility intent");
    source->hide(); program->show(); app.processEvents();
    require(program->isVisible(),"Native hover exit failed to restore the docked Program viewer");
    source->show(); app.processEvents();
    delete experiment; app.processEvents();
    require(source->parentWidget()==players && source->currentIndex()==2,"Cleanup lost native source stack");
    experiment=new EditViewers(&main,players,program,source,settings);
    experiment->toggleProgram(); experiment->toggleSource(); app.processEvents();
    program->window()->setGeometry(30,60,700,430);
    source->window()->setGeometry(65,95,660,420); app.processEvents();
    const auto programRect=program->window()->geometry(), sourceRect=source->window()->geometry();
    experiment->dockBoth(); app.processEvents();
    experiment->toggleProgram(); experiment->toggleSource(); app.processEvents();
    require(program->window()->geometry()==programRect && source->window()->geometry()==sourceRect,"Dock/re-undock lost saved viewer geometry");
    delete experiment; app.processEvents();
    Unbound::PaneSettings reloaded(settingsDirectory.filePath("viewers.json"));
    require(reloaded.floating(program->objectName()) && reloaded.floating(source->objectName()),"Shutdown discarded floating intent");
    require(!reloaded.geometry(program->objectName()).isEmpty() && !reloaded.geometry(source->objectName()).isEmpty(),"Viewer geometry was not stored on disk");
    experiment=new EditViewers(&main,players,program,source,reloaded);
    app.processEvents(); app.processEvents(); app.processEvents();
    require(program->window()!=&main && source->window()!=&main,"Saved floating viewers were not restored");
    require(program->window()->geometry()==programRect && source->window()->geometry()==sourceRect,"Startup did not recall independent viewer geometry");
    QString layoutName; bool duplicate=false;
    require(reloaded.saveLayout("WStackPage_Conform",layoutName,duplicate).isEmpty(),"Cannot save viewer layout");
    const auto programHost=program->window(),sourceHost=source->window();
    programHost->move(250,200); sourceHost->move(450,260); app.processEvents();
    experiment->restoreLayout(); app.processEvents(); app.processEvents();
    require(program->window()==programHost && source->window()==sourceHost &&
        programHost->geometry()==programRect && sourceHost->geometry()==sourceRect,
        "Viewer layout restoration rebuilt hosts or lost saved geometry");
    const auto layoutId=reloaded.activeLayout("WStackPage_Conform");
    auto dockedPanes=reloaded.layout("WStackPage_Conform",layoutId).value("panes").toObject();
    for(auto key:{program->objectName(),source->objectName()}) {
        auto value=dockedPanes.value(key).toObject(); value.insert("floating",false); dockedPanes.insert(key,value);
    }
    experiment->prepareLayout(dockedPanes); app.processEvents();
    require(program->window()==&main && source->window()==&main,"Viewer layout docking differences were not applied");
    require(reloaded.activateLayout("WStackPage_Conform",layoutId).isEmpty(),"Cannot activate viewer layout");
    experiment->restoreLayout(); app.processEvents(); app.processEvents(); app.processEvents();
    require(program->window()!=&main && source->window()!=&main && program->window()->geometry()==programRect &&
        source->window()->geometry()==sourceRect,"Viewer layout did not restore through startup floating intent");
    program->window()->close(); app.processEvents();
    require(!reloaded.floating(program->objectName()) && reloaded.floating(source->objectName()),"User docking did not preserve separate floating states");
    auto waitDock=[] { QEventLoop loop; QTimer::singleShot(180,&loop,&QEventLoop::quit); loop.exec(); };
    experiment->toggleProgram(); app.processEvents();
    const auto beforeFailedDock=program->window()->geometry();
    nativeLayout->setSizeConstraint(QLayout::SetNoConstraint);
    players->setFixedWidth(1);
    experiment->toggleProgram(); waitDock();
    auto warning=dynamic_cast<Unbound::UndockButton *>(program->findChild<QToolButton *>("resolvePaneUndockButton"));
    require(program->window()!=&main && reloaded.floating(program->objectName()) && program->window()->geometry()==beforeFailedDock,
        "Collapsed viewer allocation lost floating content, geometry or saved state");
    require(warning && warning->dockingWarningPending() && !warning->dockingFailure().contains("vertical video"),
        "Viewer failure lacked a generic warning or suggested an unconfirmed preference remedy");
    players->setMaximumWidth(QWIDGETSIZE_MAX); players->setMinimumWidth(0);
    nativeRegions(); app.processEvents(); app.processEvents();
    experiment->dockBoth(); waitDock();
    require(program->window()==&main && source->window()==&main && warning->dockingFailure().isEmpty(),
        "Viewer docking recovery retained a stale warning");
    delete experiment; app.processEvents();
    // Starting while the native players belong to the second screen must
    // still bind floating visibility to the persistent Edit page.
    QWidget auxiliary; auxiliary.setObjectName("UiEditPanelAux"); auxiliary.resize(900,500);
    players->setParent(&auxiliary); players->show(); source->show(); program->show();
    auxiliary.show(); app.processEvents();
    experiment=new EditViewers(&main,players,program,source,settings);
    experiment->toggleProgram(); experiment->toggleSource(); app.processEvents();
    auto auxProgramHost=program->window(),auxSourceHost=source->window();
    require(auxProgramHost!=&auxiliary && auxSourceHost!=&auxiliary,"Auxiliary viewers did not float");
    players->setParent(root); players->show(); auxiliary.hide(); app.processEvents();
    require(auxProgramHost->isVisible() && auxSourceHost->isVisible(),"Screen-mode change hid floating viewers");
    page->hide(); app.processEvents();
    require(!auxProgramHost->isVisible() && !auxSourceHost->isVisible(),"Auxiliary viewers ignored page departure");
    page->show(); app.processEvents(); app.processEvents();
    require(auxProgramHost->isVisible() && auxSourceHost->isVisible(),"Auxiliary viewers failed page return");
    nativeRegions(); experiment->dockBoth(); waitDock();
    require(program->window()==&main && source->window()==&main,"Auxiliary viewer docking lost native ownership");
    delete experiment; app.processEvents();
    qInfo("PASS: viewers initialized on the auxiliary screen survive mode changes and follow logical page visibility");
    qInfo("PASS: independent viewer geometry on dock/re-undock and disk reload; startup floating restoration; shutdown intent; explicit docking");
    qInfo("PASS: viewer whole-widget detachment, source alternatives, page visibility, native order/stretch, close docking and repeated cleanup");
    qInfo("PASS: docking after Cinema main-window hide/show restores native viewer, timeline and side-region geometry without resizing");
}
