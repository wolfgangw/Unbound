// SPDX-License-Identifier: MIT
#include "widgetquirks.h"
#include <QApplication>
#include <QEventLoop>
#include <QMoveEvent>
#include <QTimer>
#include <QWidget>
#include <cstdio>
#include <cstdlib>
struct Painted : QWidget { using QWidget::QWidget; int paints=0; void paintEvent(QPaintEvent *) override {++paints;} };
void settle(){QEventLoop loop;QTimer::singleShot(100,&loop,&QEventLoop::quit);loop.exec();}
void check(bool ok,const char *message){if(!ok){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}}
int main(int argc,char **argv){
 QApplication app(argc,argv);
 QWidget host;host.resize(500,400);
 QWidget scopes(&host);scopes.setGeometry(0,0,500,400);
 Painted title(&scopes),header(&scopes),plot(&scopes),hidden(&scopes);
 title.setObjectName("titleFrame");header.setObjectName("headerFrame");hidden.setObjectName("headerFrame");
 title.setGeometry(0,0,500,30);header.setGeometry(0,30,500,30);plot.setGeometry(0,60,500,300);hidden.hide();
 host.show();settle();
 auto quirk=Unbound::attachScopesWidgetQuirks(&scopes,&host);settle();
 auto reset=[&]{title.paints=header.paints=plot.paints=hidden.paints=0;};
 auto show=[&]{QEvent event(QEvent::Show);QApplication::sendEvent(&scopes,&event);};
 reset();show();settle();
 check(title.paints>0 && header.paints>0,"show must invalidate both header types");
 check(plot.paints==0 && hidden.paints==0,"show must not invalidate plots or hidden headers");
 // Resolve's late layout Move can follow the initial show repaint. Deliver
 // that notification without QWidget's own geometry repaint masking the quirk.
 reset();QMoveEvent moved(QPoint(),QPoint(1542,187));QApplication::sendEvent(&scopes,&moved);settle();
 check(title.paints>0 && header.paints>0,"late content movement must invalidate headers again");
 check(plot.paints==0,"late content movement must not invalidate plots");
 reset();show();QEvent hide(QEvent::Hide);QApplication::sendEvent(&scopes,&hide);settle();
 check(title.paints==0 && header.paints==0,"hide must cancel deferred invalidation");
 reset();show();quirk.reset();settle();
 check(title.paints==0 && header.paints==0,"teardown must cancel deferred invalidation");
 std::puts("PASS: macOS Scopes show/late-move header repaint, plot isolation, hide and teardown cancellation");
}
