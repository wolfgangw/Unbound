// SPDX-License-Identifier: MIT
#include "cinemaviewervisibility.h"
#include <QDialog>
#include <QShortcut>
#include <cstdio>
#include <cstdlib>

// Match only the native class identity used by the visibility adapter.
class UiCinemaViewerDialog : public QDialog { Q_OBJECT };
static void require(bool value,const char *message) {
    if(!value) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static void settle() { for(int i=0;i<4;++i) QApplication::processEvents(); }
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    app.setQuitOnLastWindowClosed(false);
    QWidget main;
    main.setObjectName("UiMainWindow"); main.setGeometry(30,40,800,600);
    QWidget host(&main,Qt::Window);
    host.setProperty("unboundFloatingWindow",true);
    host.setGeometry(850,40,400,300);
    QWidget program(&host); program.setObjectName("widgetEditTimelinePlayer");
    QWidget source(&main); source.setObjectName("widgetEditSourcePlayer");
    QWidget image(&program); image.setObjectName("EditTimelinePlayer");
    QShortcut shortcut(QKeySequence("J"),&main);
    shortcut.setContext(Qt::ApplicationShortcut);
    Unbound::CinemaViewerVisibility visibility;
    visibility.setWindow(&main);
    main.show(); host.show(); program.show(); source.show(); image.show(); settle();
    const auto mainGeometry=main.geometry(),hostGeometry=host.geometry();
    UiCinemaViewerDialog cinema;
    for(int cycle=0;cycle<3;++cycle) {
        image.setParent(&cinema); image.show(); host.hide(); cinema.show(); settle();
        require(!main.isVisible() && cinema.isVisible(),"floating Cinema hides only the remaining main workspace");
        require(shortcut.isEnabled() && shortcut.context()==Qt::ApplicationShortcut,"native shortcut configuration is untouched");
        cinema.hide(); image.setParent(&program); image.show(); host.show(); settle();
        require(main.isVisible(),"main visibility restored after exit");
        require(main.geometry()==mainGeometry && host.geometry()==hostGeometry,"window geometry retained");
        require(!main.testAttribute(Qt::WA_ShowWithoutActivating),"temporary activation attribute restored");
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
