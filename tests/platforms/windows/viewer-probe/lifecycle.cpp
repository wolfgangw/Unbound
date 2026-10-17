// SPDX-License-Identifier: MIT
#include "editviewers.h"
#include <QLabel>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <cstdio>
#include <cstdlib>
static void require(bool ok,const char *message) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
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
    QApplication app(argc,argv);
    QTemporaryDir settingsDirectory;
    Unbound::PaneSettings settings(settingsDirectory.filePath("viewers.json"));
    QWidget main;
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
        page->hide(); app.processEvents();
        require(!program->isVisible() && !source->isVisible(),"Page hide did not hide hosts");
        require(players->height()>0 && timeline->y()>0,"Inactive page retained adapted geometry");
        page->show(); app.processEvents();
        require(program->isVisible() && source->isVisible(),"Page return did not restore hosts");
        require(timeline->geometry()==expandedRegion() && players->height()==0,"Page return lost reclamation");
        experiment->dockBoth(); app.processEvents();
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
    QString presetName; bool duplicate=false;
    require(reloaded.savePreset("WStackPage_Conform",presetName,duplicate).isEmpty(),"Cannot save viewer preset");
    const auto programHost=program->window(),sourceHost=source->window();
    programHost->move(250,200); sourceHost->move(450,260); app.processEvents();
    experiment->restorePreset(); app.processEvents(); app.processEvents();
    require(program->window()==programHost && source->window()==sourceHost &&
        programHost->geometry()==programRect && sourceHost->geometry()==sourceRect,
        "Viewer preset restoration rebuilt hosts or lost saved geometry");
    const auto presetId=reloaded.activePreset("WStackPage_Conform");
    auto dockedPanes=reloaded.preset("WStackPage_Conform",presetId).value("panes").toObject();
    for(auto key:{program->objectName(),source->objectName()}) {
        auto value=dockedPanes.value(key).toObject(); value.insert("floating",false); dockedPanes.insert(key,value);
    }
    experiment->preparePreset(dockedPanes); app.processEvents();
    require(program->window()==&main && source->window()==&main,"Viewer preset docking differences were not applied");
    require(reloaded.activatePreset("WStackPage_Conform",presetId).isEmpty(),"Cannot activate viewer preset");
    experiment->restorePreset(); app.processEvents(); app.processEvents(); app.processEvents();
    require(program->window()!=&main && source->window()!=&main && program->window()->geometry()==programRect &&
        source->window()->geometry()==sourceRect,"Viewer preset did not restore through startup floating intent");
    program->window()->close(); app.processEvents();
    require(!reloaded.floating(program->objectName()) && reloaded.floating(source->objectName()),"User docking did not preserve separate floating states");
    delete experiment; app.processEvents();
    qInfo("PASS: independent viewer geometry on dock/re-undock and disk reload; startup floating restoration; shutdown intent; explicit docking");
    qInfo("PASS: viewer whole-widget detachment, source alternatives, page visibility, native order/stretch, close docking and repeated cleanup");
}
