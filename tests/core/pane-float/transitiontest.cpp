// SPDX-License-Identifier: MIT
#include "colorplaybackpause.h"
#include "layouttransition.h"
#include "layoutadapter.h"
#include "pagelayoutmenu.h"
#include "panecatalogue.h"
#include "pagetransition.h"
#include <QTemporaryDir>
#include <QEventLoop>
#include <QPushButton>
#include <QKeyEvent>
using LayoutTest::require;
using LayoutTest::settle;
namespace {
struct PresentationState {int begins=0,holds=0,reveals=0,cancels=0;};
class Presentation final : public Unbound::LayoutTransitionPresentation {
    std::shared_ptr<PresentationState> state;
public:
    explicit Presentation(std::shared_ptr<PresentationState> value):state(value) {}
    bool begin(QWidget *,std::function<void()>,const QList<QWidget *> &) override {++state->begins;return true;}
    qint64 retainedBytes() const override {return 0;}
    void hold() override {++state->holds;}
    void reveal(int,std::function<void(bool)> finished) override {++state->reveals;finished(true);}
    void cancel() override {++state->cancels;}
};
}
void testLayoutTransition() {
#ifdef UNBOUND_LAYOUT_COVER_EXPERIMENT
    {
        QWidget root;root.resize(400,300);
        QStackedWidget stack(&root);stack.setObjectName("widgetStack_Panel");stack.resize(400,300);
        QWidget edit,color,audio;
        edit.setObjectName("WStackPage_Conform");color.setObjectName("WStackPage_Color");audio.setObjectName("WStackPage_Audio");
        for(auto page:{&edit,&color,&audio})stack.addWidget(page);
        root.show();LayoutTest::activate(root);
        Unbound::PageTransition pages;auto state=std::make_shared<PresentationState>();
        pages.presentation=[state] {return std::make_unique<Presentation>(state);};
        bool floating=false,audioFloating=false;
        pages.hasFloating=[&](const QString &page) {
            return (floating && page==color.objectName()) || (audioFloating && page==audio.objectName());
        };
        int arrivals=0;
        pages.arrived=[&] {++arrivals;Unbound::LayoutTrace::pending(0,0,0,0);};pages.attach(&root);
        stack.setCurrentWidget(&audio);settle();stack.setCurrentWidget(&edit);settle();
        require(!state->begins && !arrivals,"Fully docked page switches started capture or restoration work");
        Unbound::LayoutTraceWork work;work.set("deferred_page_host_show",&color);
        floating=true;
        struct Boundary final : QObject {
            std::shared_ptr<PresentationState> state;int showBegins=0,hideBegins=0;
            bool eventFilter(QObject *,QEvent *e) override {
                if(e->type()==QEvent::Show)showBegins=state->begins;
                if(e->type()==QEvent::Hide)hideBegins=state->begins;
                return false;
            }
        } boundary;
        boundary.state=state;color.installEventFilter(&boundary);
        stack.setCurrentWidget(&color);
        require(state->begins==1 && boundary.showBegins==1,"Incoming capture missed the page's Show handler");
        auto wait=[] {QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();};
        wait();require(!state->reveals,"Page transition revealed before deferred host work");
        work.clear();wait();wait();
        require(state->reveals==1 && state->cancels==1,"Incoming page failed to reveal");
        QEvent duplicateHide(QEvent::Hide);
        QApplication::sendEvent(&color,&duplicateHide);
        QApplication::sendEvent(&color,&duplicateHide);
        require(state->begins==2 && state->cancels==1 && boundary.hideBegins==2,
            "Departure did not capture before pane Hide handlers, or duplicate Hide replaced it");
        const int beforeDockedArrivals=arrivals;
        stack.setCurrentWidget(&edit);
        require(state->begins==2 && boundary.hideBegins==2 && arrivals==beforeDockedArrivals+1,
            "Docked destination replaced the outgoing cover or missed readiness");
        wait();wait();require(state->reveals==2 && state->cancels==2,
            "Docked destination did not promptly reveal and release the outgoing cover");
        stack.setCurrentWidget(&color);stack.setCurrentWidget(&audio);
        wait();wait();require(state->begins==4 && state->reveals==3 && state->cancels==4,
            "Rapid switch to docked page failed to replace/release its cover");
        audioFloating=true;
        stack.setCurrentWidget(&color);stack.setCurrentWidget(&audio);
        wait();wait();require(state->begins==6 && state->reveals==4 && state->cancels==6,
            "Floating-to-floating switch retained or revealed a superseded cover");
        stack.setCurrentWidget(&color);
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&color,&escape);
        wait();require(state->reveals==4 && state->cancels==7,"User input failed to cancel page cover");
        root.hide();settle();require(state->begins==7,"Hiding main was mistaken for a page switch");
        stack.setCurrentWidget(&audio);stack.setCurrentWidget(&color);settle();
        require(state->begins==7,"Hidden page selection started a cover");
        root.show();settle();require(state->begins==7,"Restoring main started a page-switch cover");
        stack.hide();stack.setCurrentWidget(&audio);stack.setCurrentWidget(&color);settle();
        require(state->begins==7,"Hidden page stack started a cover");
        color.removeEventFilter(&boundary);
        delete Unbound::LayoutTrace::active();
        std::puts("PASS: departure/arrival page capture, docked-to-docked bypass, deferred return, rapid switch and cancellation");
    }
    // Exercise the actual recall adapter, not only its presentation lifecycle.
    // Catalogue coverage prevents a page-specific experiment gate returning.
    for(const auto &descriptor:Unbound::PaneCatalogue::pages()) if(descriptor.supported) {
        QWidget root,page(&root);root.resize(400,300);page.resize(400,300);
        page.setObjectName(descriptor.objectName);root.show();page.show();LayoutTest::activate(root);
        Unbound::LayoutTraceWork work;work.set("deferred_page_restore",&page);
        Unbound::LayoutTrace::begin(&root,descriptor.objectName);
        auto state=std::make_shared<PresentationState>();
        Unbound::LayoutTransitionRecall recall(&root,descriptor.objectName,{},nullptr,std::make_unique<Presentation>(state));
        require(state->begins==1,"Recall presentation excluded a supported Resolve page");
        recall.complete();Unbound::LayoutTrace::pending(0,0,0,0);
        auto wait=[] {QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();};
        wait();require(!state->reveals,"Recall revealed before its page adapter finished");
        work.clear();wait();wait();
        require(state->reveals==1 && state->cancels==1,"Supported page failed to reveal and release presentation");
        delete Unbound::LayoutTrace::active();
    }
#endif
    QWidget main,page(&main);page.setObjectName("WStackPage_Color");main.show();page.show();
    LayoutTest::activate(main);
    auto none=Unbound::LayoutTransition::begin(&main,page.objectName(),
        std::make_unique<Unbound::NoLayoutTransitionPresentation>());
    require(!none,"Unsupported presentation must not create a readiness wait");settle();
    auto first=std::make_shared<PresentationState>();
    auto transition=Unbound::LayoutTransition::begin(&main,page.objectName(),std::make_unique<Presentation>(first));
    require(transition && first->begins==1,"Shared presentation did not start injected backend");
    transition->invalidate();require(first->holds==1,"Readiness invalidation did not hold backend");
    auto second=std::make_shared<PresentationState>();
    auto replacement=Unbound::LayoutTransition::begin(&main,page.objectName(),std::make_unique<Presentation>(second));
    require(first->cancels==1,"Superseding transition left old backend alive");
    replacement->ready();require(second->reveals==1 && second->cancels==1,"Ready did not complete/release presentation");
    replacement->ready();require(second->reveals==1,"Completed transition revealed twice");settle();
    auto hidden=std::make_shared<PresentationState>();
    transition=Unbound::LayoutTransition::begin(&main,page.objectName(),std::make_unique<Presentation>(hidden));
    page.hide();require(hidden->cancels==1,"Page departure did not cancel presentation");settle();
    page.show();QWidget menu(&main,Qt::Tool);QPushButton action(&menu);menu.show();
    auto live=std::make_shared<PresentationState>();
    transition=Unbound::LayoutTransition::begin(&main,page.objectName(),std::make_unique<Presentation>(live),{&menu});
    QKeyEvent input(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);QApplication::sendEvent(&action,&input);
    require(live->cancels==0,"Interactive menu input cancelled its presentation");
    menu.hide();require(live->cancels==1,"Dismissed live menu left its presentation active");settle();
    QTemporaryDir settingsDirectory;Unbound::PaneSettings settings(settingsDirectory.filePath("menu.json"));
    bool floaters=true;
    Unbound::PageLayoutMenu popout(settings,page.objectName(),&main,[&] {return floaters;});
    const auto area=QGuiApplication::primaryScreen()->availableGeometry();
    popout.popup(area.bottomRight()-QPoint(5,5));LayoutTest::activate(popout);
    const auto stableMenu=popout.geometry();
    const auto dockAll=popout.findChild<QPushButton *>("unboundLayoutDockAll");
    require(dockAll && dockAll->isVisible() && dockAll->isEnabled(),"Missing active Dock all row");
    for(bool present:{false,true,false,true}) {
        floaters=present;popout.refreshDockingWarning();settle();
        require(popout.geometry()==stableMenu && dockAll->isVisible() && dockAll->isEnabled()==present,
            "Zero-host restoration interval moved/resized the menu or removed Dock all");
    }
    popout.dockingWarning=[] {return QStringLiteral("A native destination is unavailable. This longer status must not push the menu below the display edge during restoration.");};
    popout.refreshDockingWarning();settle();
    require(popout.isVisible() && area.contains(popout.geometry()),"Growing live menu escaped its work area");
    QEvent inactive(QEvent::ApplicationDeactivate);QApplication::sendEvent(qApp,&inactive);
    require(popout.isVisible(),"Qt activation churn hid the live menu");
    QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&popout,&escape);
    require(!popout.isVisible(),"Escape no longer dismisses the menu");
    std::puts("PASS: shared transition readiness, invalidation, supersession, cancellation and no-presentation backend");
}
void testColorPlaybackPause() {
    using Pause=Unbound::ColorPlaybackPause;
    QWidget main,page(&main),player(&page);
    main.resize(600,300);page.resize(600,300);player.resize(500,200);
    page.setObjectName("WStackPage_Color");player.setObjectName("m_pFramePlayer");
    QPushButton forward(&player),reverse(&player),stop(&player);
    forward.setObjectName("btnTransportPlay");reverse.setObjectName("btnTransportReversePlay");stop.setObjectName("btnTransportStop");
    for(auto b:{&forward,&reverse,&stop})b->setCheckable(true);
    QLabel speedLabel(&player);speedLabel.setObjectName("labelPlaySpeed");
    QAction faster(&main),rewind(&main);faster.setObjectName("controlPlayForward");rewind.setObjectName("controlPlayReverse");
    int stopCount=0,forwardCount=0,reverseCount=0,acceleration=0;
    double speed=1;bool acceptStop=true;
    auto playing=[&](bool backwards,double rate=1) {
        stop.setChecked(false);forward.setChecked(!backwards);reverse.setChecked(backwards);speed=rate;
        speedLabel.setText(QString::number(rate)+"x");speedLabel.setVisible(rate!=1);
    };
    QObject::connect(&stop,&QPushButton::clicked,&main,[&] {
        ++stopCount;if(!acceptStop)return;
        forward.setChecked(false);reverse.setChecked(false);stop.setChecked(true);speedLabel.hide();
    });
    QObject::connect(&forward,&QPushButton::clicked,&main,[&] {++forwardCount;playing(false);});
    QObject::connect(&reverse,&QPushButton::clicked,&main,[&] {++reverseCount;playing(true);});
    auto accelerate=[&] {++acceleration;speed*=2;speedLabel.setText(QString::number(speed)+"x");speedLabel.show();};
    QObject::connect(&faster,&QAction::triggered,&main,accelerate);
    QObject::connect(&rewind,&QAction::triggered,&main,accelerate);
    main.show();page.show();player.show();LayoutTest::activate(main);
    // The shared policy must never borrow transport on another page, even if
    // native Color controls remain discoverable below the same main window.
    QJsonObject clips{{"m_pLightBox",QJsonObject{{"floating",true}}}};
    for(const auto &descriptor:Unbound::PaneCatalogue::pages()) if(descriptor.supported
        && descriptor.id!=Unbound::PaneCatalogue::Page::Color) {
        auto unrelated=Unbound::LayoutRestorePolicy::pauseForRecall(&main,descriptor.objectName,clips,true);
        require(!unrelated && !stopCount,"Color playback exception escaped its page scope");
    }
    auto idle=Pause::acquire(&page);
    require(!idle && !stop.isChecked(),"Fresh idle transport unexpectedly acquired a pause lease");
    idle.reset();settle();
    require(stopCount==0 && forwardCount==0 && reverseCount==0,"Fresh idle transport was stopped or restarted");
    stop.setEnabled(false);idle=Pause::acquire(&page);
    require(!idle,"Idle transport required an enabled Stop button");stop.setEnabled(true);
    forward.setChecked(true);reverse.setChecked(true);
    auto ambiguous=Pause::acquire(&page);
    require(!ambiguous && stopCount==0,"Contradictory directions were treated as idle");
    forward.setChecked(false);reverse.setChecked(false);stop.setChecked(true);
    auto stopped=Pause::acquire(&page);
    require(!stopped && stopCount==0,"Stopped playback was altered");
    playing(false,4);
    auto first=Pause::acquire(&page);auto nested=Pause::acquire(&page);
    require(first && nested==first && stopCount==1 && stop.isChecked(),"Nested operations did not share one acknowledged stop");
    first.reset();settle();require(forwardCount==0,"Playback resumed before the final operation completed");
    nested.reset();settle();
    require(forwardCount==1 && forward.isChecked() && speed==4 && acceleration==2,"Forward shuttle speed was not restored");
    playing(true,2);auto backwards=Pause::acquire(&page);backwards.reset();settle();
    require(reverseCount==1 && reverse.isChecked() && speed==2,"Reverse direction/speed lost");
    playing(false);speedLabel.setText("16x");speedLabel.hide();
    auto hidden=Pause::acquire(&page);hidden.reset();settle();
    require(speed==1,"Hidden stale speed text changed normal playback");
    playing(true,0.5);auto slow=Pause::acquire(&page);slow.reset();settle();
    require(reverse.isChecked() && speed==1,"Unsupported speed did not use same-direction 1x fallback");
    auto expectCancelled=[&](const std::function<void()> &interrupt) {
        playing(false);auto value=Pause::acquire(&page);const int before=forwardCount;
        interrupt();value.reset();settle();require(forwardCount==before,"Cancelled transaction restarted playback");
    };
    expectCancelled([&] {QKeyEvent key(QEvent::ShortcutOverride,Qt::Key_K,Qt::NoModifier);QApplication::sendEvent(&player,&key);});
    expectCancelled([&] {stop.click();});
    expectCancelled([&] {main.setWindowTitle("Changed project");});
    expectCancelled([&] {page.hide();});page.show();LayoutTest::activate(main);
    playing(false);auto old=Pause::acquire(&page);old.reset();
    auto newer=Pause::acquire(&page);const int before=forwardCount;settle();
    require(forwardCount==before,"Queued stale completion ignored a newer lease");
    newer.reset();settle();require(forwardCount==before+1,"Newer lease failed to complete once");
    playing(false);acceptStop=false;auto rejected=Pause::acquire(&page);
    require(!rejected && forward.isChecked(),"Stop button's own check was mistaken for native acknowledgement");
    acceptStop=true;stop.click();settle();
    std::puts("PASS: Color stop acknowledgement, nested work, shuttle directions, fallback and stale-resume cancellation");
}
