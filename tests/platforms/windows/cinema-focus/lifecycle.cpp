// SPDX-License-Identifier: MIT
#include "cinemaviewervisibility.h"
#include "cinemapanevisibility.h"
#include "nativescopescontent.h"
#include "sidebarfloatsession.h"
#include <QPushButton>
#include <QDialog>
#include <QShortcut>
#include <QKeyEvent>
#include <cstdio>
#include <cstdlib>

// Match only the native class identity used by the visibility adapter.
class UiCinemaViewerDialog : public QDialog { Q_OBJECT };
static void require(bool value,const char *message) {
    if(!value) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static void settle() { for(int i=0;i<4;++i) QApplication::processEvents(); }
static void checkFloatingColorViewer() {
    QWidget main;
    auto page=new QWidget(&main); page->setObjectName("WStackPage_Color");
    auto slot=new QWidget(page); slot->setObjectName("m_pFramePlayer");
    auto native=new QVBoxLayout(slot);
    auto form=new QWidget; form->setObjectName("UiMainViewerForm"); native->addWidget(form);
    main.show(); settle();
    Unbound::SidebarFloatSession session;
    require(session.start(slot,"Viewer",page,false,false,true,false).isEmpty(),"float Color viewer");
    QPointer<QWidget> host=session.floatingWindow();
    Unbound::CinemaViewerVisibility visibility;
    visibility.setWindow(&main);
    UiCinemaViewerDialog cinema;
    auto cinemaLayout=new QVBoxLayout(&cinema);
    QAction toggle(&main);
    toggle.setObjectName("workspaceViewerModeCinemaViewer"); toggle.setCheckable(true);
    bool queuedReturn=false;
    auto returnForm=[&] {
        host->layout()->addWidget(form); form->show(); host->show();
    };
    QObject::connect(&toggle,&QAction::triggered,[&](bool on) {
        if(on) {
            cinemaLayout->addWidget(form); form->show(); cinema.show(); host->hide();
        } else {
            cinema.hide();
            if(queuedReturn) QTimer::singleShot(0,&cinema,returnForm);
            else returnForm();
        }
    });
    toggle.trigger(); settle();
    require(form->window()==&cinema && form->isVisible() && !main.isVisible(),
        "Cinema keeps the borrowed Color form after deferred graph recovery");
    toggle.trigger(); settle();
    require(form->window()==host && host->isVisible() && main.isVisible(),
        "Cinema returns the complete form to the original Color host");
    int docked=0;
    session.userDocked=[&] {++docked;};
    for(bool delayed : {false,true}) {
        queuedReturn=delayed;
        toggle.trigger(); settle();
        require(session.dock().isEmpty(),"dock a Color viewer currently in Cinema");
        if(delayed) require(session.active() && form->window()==&cinema && docked==1,
            "queued Cinema return keeps the originating host alive and defers the dock notification");
        settle();
        require(main.isVisible() && !cinema.isVisible() && !session.active() && native->indexOf(form)==0,
            "explicit docking lets Cinema return the viewer before retiring its host");
        if(!delayed) {
            require(session.start(slot,"Viewer",page,false,false,true,false).isEmpty(),"refloat after Cinema docking");
            host=session.floatingWindow();
        }
    }
    require(docked==2,"each completed dock notifies exactly once");
    // Resolve controls a docked viewer's original window and return focus.
    cinemaLayout->addWidget(form); form->show(); cinema.show(); settle();
    require(main.isVisible() && !form->property(Unbound::floatingColorViewerProperty).isValid(),
        "docked Color viewer does not activate the floating visibility guard");
    cinema.hide(); native->addWidget(form);
}
class ShowObserver : public QObject {
public:
    std::function<void()> callback;
    bool eventFilter(QObject *,QEvent *event) override {
        if(event->type()==QEvent::Show && callback) callback();
        return false;
    }
};
// Run with the native Windows platform and two connected displays.
static void checkSecondaryDisplay() {
    const auto screens=QApplication::screens();
    require(screens.size()>=2,"two displays required for secondary-display fixture");
    QWidget workspace;
    QWidget panePage(&workspace);
    QWidget host;
    UiCinemaViewerDialog cinema;
    const auto first=screens[0]->availableGeometry().center();
    const auto second=screens[1]->availableGeometry().center();
    workspace.move(first);
    cinema.move(first);
    host.resize(120,90);
    class PageHide : public QObject {
    public:
        QWidget *page,*host;
        bool eventFilter(QObject *,QEvent *event) override {
            if(event->type()==QEvent::Hide)
                Unbound::onPanePageHide(this,page,host,[this] {host->hide();});
            return false;
        }
    } observer;
    observer.page=&panePage;
    observer.host=&host;
    panePage.installEventFilter(&observer);
    QDialog scopes(&workspace);
    QPushButton closeScopes(&scopes);
    closeScopes.setObjectName("btnHide");
    QObject::connect(&closeScopes,&QPushButton::clicked,&scopes,&QWidget::hide);
    Unbound::NativeScopesContent content(nullptr);
    require(content.adopt(&panePage,&scopes,&host,&panePage,false).isEmpty(),"embed native Scopes");
    int dockRequests=0;
    content.dockRequested=[&] {++dockRequests;};
    for(bool elsewhere : {true,false}) {
        host.move(elsewhere ? second : first);
        workspace.show(); host.show(); settle();
        scopes.show();
        cinema.show(); scopes.hide(); settle();
        require(dockRequests==0 && scopes.isVisible()==elsewhere,
            "Cinema hiding Scopes before the workspace must never request docking");
        cinema.hide(); scopes.show(); settle();
        workspace.hide(); scopes.hide(); cinema.show(); settle();
        require(host.isVisible()==elsewhere,"keep only floats clear of Cinema display");
        require(scopes.isVisible()==elsewhere && scopes.parentWidget()==&host,
            "preserve the embedded Scopes content only in the retained host");
        cinema.hide(); workspace.show(); settle();
    }
    host.move(second); host.show(); scopes.show(); cinema.show(); settle();
    scopes.close(); settle();
    require(dockRequests==1,"explicit Scopes close in Cinema still requests docking");
    cinema.hide(); panePage.hide();
    require(!host.isVisible(),"real page departure still hides synchronously");
    panePage.show(); host.show(); workspace.hide(); settle();
    require(!host.isVisible(),"ordinary workspace hide still conceals secondary host");
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);
    if(app.arguments().contains("--secondary-display")) {
        checkSecondaryDisplay();
        std::puts("Secondary-display Cinema visibility passed");
        return 0;
    }
    checkFloatingColorViewer();
    QWidget main;
    main.setObjectName("UiMainWindow"); main.setGeometry(30,40,800,600);
    QWidget host(&main,Qt::Window);
    host.setProperty("unboundFloatingWindow",true);
    host.setGeometry(850,40,400,300);
    QWidget program(&host); program.setObjectName("widgetEditTimelinePlayer");
    QWidget source(&main); source.setObjectName("widgetEditSourcePlayer");
    QWidget image(&program); image.setObjectName("EditTimelinePlayer");
    program.setFocusPolicy(Qt::StrongFocus); source.setFocusPolicy(Qt::StrongFocus);
    QShortcut shortcut(QKeySequence("J"),&main);
    shortcut.setContext(Qt::ApplicationShortcut);
    // The offscreen backend has no native window activation implementation.
    int returnActivations=0;
    Unbound::CinemaViewerVisibility visibility([&](QWidget *w) { ++returnActivations; QApplication::setActiveWindow(w); });
    visibility.setWindow(&main);
    main.show(); host.show(); program.show(); source.show(); image.show(); settle();
    const auto mainGeometry=main.geometry(),hostGeometry=host.geometry();
    UiCinemaViewerDialog cinema;
    ShowObserver mainShow;
    main.installEventFilter(&mainShow);
    for(int cycle=0;cycle<3;++cycle) {
        image.setParent(&cinema); image.show(); host.hide(); cinema.show(); settle();
        require(!main.isVisible() && cinema.isVisible(),"floating Cinema hides only the remaining main workspace");
        require(shortcut.isEnabled() && shortcut.context()==Qt::ApplicationShortcut,"native shortcut configuration is untouched");
        cinema.hide(); image.setParent(&program); image.show(); host.show(); settle();
        require(main.isVisible(),"main visibility restored after exit");
        require(main.geometry()==mainGeometry && host.geometry()==hostGeometry,"window geometry retained");
        require(!main.testAttribute(Qt::WA_ShowWithoutActivating),"temporary activation attribute restored");
    }
    // Match Resolve's exit ordering: host activation while main is hidden,
    // followed by main activation after the adapter restores visibility.
    for(bool newerInput : {false,true}) {
        const int beforeReturn=returnActivations;
        image.setParent(&cinema); image.show(); host.hide(); cinema.show(); settle();
        if(newerInput) mainShow.callback=[&] {
            QKeyEvent input(QEvent::KeyPress,Qt::Key_K,Qt::NoModifier);
            QApplication::sendEvent(&program,&input);
        };
        cinema.hide(); image.setParent(&program); image.show(); host.show();
        QApplication::setActiveWindow(&host); program.setFocus(); settle();
        mainShow.callback={};
        require(main.isVisible(),"return fixture restores main before late activation");
        if(newerInput) require(returnActivations==beforeReturn,"input cancels the pending return before main activates");
        // Offscreen may already have delivered the late main activation from
        // show(). Otherwise explicitly supply the observed Resolve event.
        if(newerInput || returnActivations==beforeReturn) {
            QApplication::setActiveWindow(&main); source.setFocus(); settle();
        }
        require(QApplication::activeWindow()==(newerInput ? &main : &host),
            "Cinema return preserves host unless newer input supersedes it");
        require(QApplication::focusWidget()==(newerInput ? &source : &program),
            "Cinema return preserves originating player focus");
    }
    // Source in its docked location: native Resolve owns main visibility.
    image.setObjectName("EditSourcePlayer"); image.setParent(&cinema); image.show();
    cinema.show(); settle();
    require(main.isVisible(),"docked viewer is not handled");
    cinema.hide(); settle();
    source.setParent(&host); source.show();
    cinema.show(); settle(); require(!main.isVisible(),"floating Source handled too");
    cinema.hide(); settle(); require(main.isVisible(),"floating Source exit restores main");
    main.hide(); cinema.show(); settle(); cinema.hide(); settle();
    require(!main.isVisible(),"preexisting hidden main remains hidden");
    main.show();
    cinema.show(); cinema.hide(); settle();
    require(main.isVisible(),"cancelled entry cannot hide main later");
    cinema.show(); settle(); cinema.hide(); cinema.show(); settle();
    require(!main.isVisible(),"rapid reentry stays hidden");
    cinema.hide(); settle(); require(main.isVisible(),"rapid reentry still restores");
    QDialog unrelated;
    image.setParent(&unrelated); image.show(); unrelated.show(); settle();
    require(main.isVisible(),"other dialogs are unaffected");
    unrelated.hide(); image.setParent(&cinema); image.show();
    cinema.show(); settle(); visibility.setWindow(nullptr); settle();
    require(main.isVisible(),"detaching adapter restores visibility");
    cinema.hide(); visibility.setWindow(&main);
    auto destroyedCinema=new UiCinemaViewerDialog;
    image.setParent(destroyedCinema); image.show(); destroyedCinema->show(); settle();
    require(!main.isVisible(),"destruction fixture entered");
    image.setParent(&program); delete destroyedCinema; settle();
    require(main.isVisible(),"dialog destruction restores visibility");
    image.setParent(&program);
    std::puts("Cinema visibility lifecycle tests passed");
}
#include "lifecycle.moc"
