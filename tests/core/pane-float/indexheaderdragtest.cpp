// SPDX-License-Identifier: MIT
#include "panedecoration.h"
#include "layoutadapter.h"
#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>
#include <QStackedWidget>
#include <cstdio>

static void testIndexHeaderDrag(bool fairlight, bool effects=false) {
    if(QGuiApplication::platformName()==QLatin1String("offscreen")) return;
    using LayoutTest::require;
    Unbound::PaneFloatingWindow host;
    host.setGeometry(300,200,600,300);
    auto outer=new QVBoxLayout(&host);
    auto pane=new QFrame; pane->setObjectName(effects ? "m_pFrameOFXPlugin" : fairlight ? "frameAudioPanelIndexContainer" : "frameEditPageEditIndex"); outer->addWidget(pane);
    auto column=new QVBoxLayout(pane);
    auto tabs=new QFrame; tabs->setObjectName(effects ? "frameOFXTitle" : fairlight ? "frameAudioPageIndexTabContainer" : "frameEditPageIndexTabContainer"); column->addWidget(tabs);
    auto row=new QHBoxLayout(tabs); row->addStretch();
    QLabel *title=nullptr;
    if(effects) { title=new QLabel("Effects"); row->addWidget(title); }
    int clicks=0;
    QList<QPushButton *> buttons;
    QStringList tabNames={"Edit Index","Tracks","Markers"};
    if(fairlight) tabNames.prepend("Clips");
    for(const auto &name:tabNames) {
        auto button=new QPushButton(name); row->addWidget(button); buttons.append(button);
        QObject::connect(button,&QPushButton::clicked,[&] { ++clicks; });
    }
    row->addStretch();
    auto header=new QFrame; header->setObjectName(effects ? "frameTopRight" : "frameEDLTopBar");
    if(effects) row->addWidget(header); else column->addWidget(header);
    auto actions=new QHBoxLayout(header); actions->addStretch();
    auto options=new QToolButton; options->setObjectName(effects ? "btnOFXPoolOptions" : "btnEdlOptions"); actions->addWidget(options);
    column->addStretch();
    Unbound::PaneDecoration decoration(pane,&host,[] {});
    decoration.setFloating(true);
    host.show(); LayoutTest::settle();
    require(decoration.button()->parentWidget()==header,"Index button moved out of native action header");
    auto mouse=[](QWidget *target,QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons held) {
        QMouseEvent event(type,point,target->mapToGlobal(point),button,held,Qt::NoModifier);
        QApplication::sendEvent(target,&event);
    };
    auto gesture=[&](QWidget *surface,bool expected) {
        const QPoint start(3,surface->height()/2);
        mouse(surface,QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        mouse(surface,QEvent::MouseMove,start+QPoint(QApplication::startDragDistance()+1,0),Qt::NoButton,Qt::LeftButton);
        require(host.interactiveDragActive()==expected,"Index upper-row drag state incorrect");
        mouse(surface,QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);
        require(!host.interactiveDragActive(),"Index upper-row release retained drag");
    };
    gesture(tabs,true);
    gesture(header,true);
    if(title) gesture(title,true);
    gesture(options,false);
    for(auto button:buttons) gesture(button,false);
    require(clicks==tabNames.size(),"Index tabs lost native click behavior");
    // The same retained row must be inert as a drag surface after docking.
    QWidget docked;
    pane->setParent(&docked); pane->show(); docked.show(); LayoutTest::settle();
    gesture(tabs,false);
    pane->setParent(&host); outer->addWidget(pane); pane->show(); LayoutTest::settle();
    gesture(tabs,true);
    std::puts(effects ? "PASS: Color Effects full title/header dragging, excluded native controls and dock/refloat lifecycle"
        : fairlight ? "PASS: Fairlight Index tab/header dragging, native clicks and dock/refloat lifecycle"
        : "PASS: Edit Index tab/header dragging, native clicks and dock/refloat lifecycle");
}

static void testMetadataHeaders(bool fairlight) {
    using LayoutTest::require;
    QWidget docked;
    Unbound::PaneFloatingWindow host;
    host.setGeometry(300,200,500,300);
    auto outer=new QVBoxLayout(&host);
    auto pane=new QFrame;
    pane->setObjectName(fairlight ? "frameMetadataContainer" : "frameEditPageMetadata");
    outer->addWidget(pane);
    auto column=new QVBoxLayout(pane);
    auto stack=new QStackedWidget; column->addWidget(stack);
    QList<QWidget *> headers;
    QList<QLabel *> titles;
    for(const auto &name:{"emptyClipView","singleViewFrame","multiViewFrame"}) {
        auto view=new QFrame; view->setObjectName(name); stack->addWidget(view);
        auto content=new QVBoxLayout(view);
        auto header=new QFrame; header->setObjectName("frameMetadataTop"); content->addWidget(header);
        auto row=new QHBoxLayout(header);
        auto title=new QLabel("Metadata"); row->addWidget(title,1); titles.append(title);
        if(QString(name)!="emptyClipView") {
            auto options=new QToolButton; options->setObjectName("btnPresetsSelector"); row->addWidget(options);
        }
        content->addStretch(); headers.append(header);
    }
    int clicks=0;
    {
        Unbound::PaneDecoration decoration(pane,&host,[&] { ++clicks; });
        decoration.setFloating(true); host.show(); LayoutTest::settle();
        require(decoration.usesNativeHeader() && pane->contentsMargins()==QMargins(),"Metadata added a fallback strip");
        for(int i:{0,1,2,0}) {
            stack->setCurrentIndex(i); LayoutTest::settle();
            auto button=headers[i]->findChild<QToolButton *>("resolvePaneUndockButton");
            require(button && button->isVisible() && button->isChecked(),"Metadata state lost floating button");
            button->click();
            decoration.setFloating(true);
            auto row=qobject_cast<QBoxLayout *>(headers[i]->layout());
            auto options=headers[i]->findChild<QToolButton *>("btnPresetsSelector");
            require(row->indexOf(button)==(options ? row->indexOf(options)-1 : row->count()-1),"Metadata header placement changed");
        }
        require(clicks==4,"Metadata buttons lost docking action");
        if(QGuiApplication::platformName()!=QLatin1String("offscreen")) {
            auto title=titles[0]; const QPoint start(3,title->height()/2);
            auto send=[&](QEvent::Type type,QPoint pos,Qt::MouseButton button,Qt::MouseButtons held) {
                QMouseEvent event(type,pos,title->mapToGlobal(pos),button,held,Qt::NoModifier);
                QApplication::sendEvent(title,&event);
            };
            send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
            send(QEvent::MouseMove,start+QPoint(QApplication::startDragDistance()+1,0),Qt::NoButton,Qt::LeftButton);
            require(host.interactiveDragActive(),"Empty Metadata title did not drag");
            send(QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);
            require(!host.interactiveDragActive(),"Empty Metadata title retained drag");
        }
        pane->setParent(&docked); decoration.setFloating(false);
        pane->show(); docked.show(); LayoutTest::settle();
        auto button=headers[0]->findChild<QToolButton *>("resolvePaneUndockButton");
        require(button && button->isVisible() && !button->isChecked(),"Empty Metadata lost undocking button after docking");
        button->click(); require(clicks==5,"Empty Metadata lost undocking action");
    }
    require(pane->findChildren<QToolButton *>("resolvePaneUndockButton").isEmpty(),"Metadata decoration leaked buttons");
    std::puts(fairlight ? "PASS: Fairlight Metadata empty/single/multi headers, title dragging and docking"
        : "PASS: Edit Metadata empty/single/multi headers, title dragging and docking");
}

static void testUndockDrag() {
    using namespace Unbound;
    using LayoutTest::require;
    if(QGuiApplication::platformName()!=QLatin1String("offscreen")) {
        QWidget docked; docked.resize(400,200); docked.show();
        WindowGroupActivation group; group.setWindow(&docked);
        PaneFloatingWindow host; host.resize(420,260);
        host.setAttribute(Qt::WA_ShowWithoutActivating); host.show();
        LayoutTest::settle(); LayoutTest::activate(docked);
        auto active=QApplication::activeWindow();
        host.setProperty("unboundUndockDragging",true);
        host.raise();
        require(host.beginButtonMove(false),"Nonactivating tearoff movement did not begin");
        LayoutTest::settle();
        require(QApplication::activeWindow()==active,"Tearoff movement stole activation before release");
        require(LayoutTest::stackedAbove(host,docked),"Queued group refresh covered the tearoff with main");
        PaneFloatingWindow pinned; pinned.setProperty("unboundKeepAtop",true); pinned.show();
        LayoutTest::settle();
        require(LayoutTest::stackedAbove(pinned,host) && LayoutTest::stackedAbove(host,docked),
            "Tearoff lost priority below pins and above focused main");
        host.finishButtonMove();
        host.setProperty("unboundUndockDragging",QVariant());
        LayoutTest::settle();
        require(LayoutTest::stackedAbove(docked,host),"Finished tearoff retained temporary stacking priority");
        require(!host.interactiveDragActive(),"Nonactivating tearoff retained movement");
    }
    // Model both a transferred header button and Scopes' replacement header.
    for(int mode=0;mode<3;++mode) {
        QWidget docked; docked.resize(400,200); docked.show();
        QObject owner;
        PaneFloatingWindow host; host.resize(420,260);
        host.setObjectName("unboundFloating_fixture");
        auto layout=new QHBoxLayout(&host);
        layout->addWidget(new QWidget,1);
        QPointer<UndockButton> source=new UndockButton(&docked);
        source->setCheckable(true); source->setGeometry(20,20,20,20); source->show();
        source->setProperty("unboundPaneName","fixture");
        QPointer<UndockButton> target;
        QSize initialSize;
        int undocks=0,clicks=0;
        QObject::connect(source,&QToolButton::clicked,&owner,[&] { ++clicks; });
        source->undockDrag=[&](QWidget *button,const QPoint &offset) {
            PaneUndockDrag::start(&owner,button,offset,[&] {
                ++undocks;
                auto transfer=[&] {
                    if(mode==0) target=source;
                    else { delete source.data(); target=new UndockButton; target->setCheckable(true); }
                    target->setFixedSize(20,20); layout->addWidget(target); target->setFloatingState(true);
                    host.move(900,500); PaneUndockDrag::prepareHost(&host);
                    require(PaneUndockDrag::ownsHost(&host),"Tearoff host was not recognized for delayed activation");
                    require(host.windowOpacity()==0,"Undock host exposed remembered placement before anchoring");
                    host.show(); target->show(); layout->activate();
                    initialSize=host.size();
                };
                if(mode==2) QTimer::singleShot(0,&owner,transfer);
                else transfer();
            },[&]() -> QWidget * { return target; });
        };
        LayoutTest::settle();
        LayoutTest::activate(docked);
        auto send=[](QWidget *widget,QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons held) {
            QMouseEvent event(type,point,widget->mapToGlobal(point),button,held,Qt::NoModifier);
            QApplication::sendEvent(widget,&event);
        };
        // A tiny wiggle remains exactly one normal click.
        send(source,QEvent::MouseButtonPress,{8,8},Qt::LeftButton,Qt::LeftButton);
        send(source,QEvent::MouseMove,{9,8},Qt::NoButton,Qt::LeftButton);
        send(source,QEvent::MouseButtonRelease,{9,8},Qt::LeftButton,Qt::NoButton);
        require(clicks==1 && undocks==0,"Tiny docked-button wiggle started an undock drag");
        source->setChecked(false);
        send(source,QEvent::MouseButtonPress,{8,8},Qt::LeftButton,Qt::LeftButton);
        send(source,QEvent::MouseMove,{8+QApplication::startDragDistance()+1,8},Qt::NoButton,Qt::LeftButton);
        // Release before delayed native content exists must not become a click.
        const auto dropPosition=mode==2 ? docked.mapToGlobal(QPoint(8,8)) : QCursor::pos();
        if(mode==2 && QGuiApplication::platformName()!=QLatin1String("offscreen")) {
            QEvent transient(QEvent::ApplicationDeactivate);
            QApplication::sendEvent(qApp,&transient);
        }
        if(mode==2) send(&docked,QEvent::MouseButtonRelease,{8,8},Qt::LeftButton,Qt::NoButton);
        LayoutTest::settle();
        require(undocks==1 && clicks==1 && target && target->isVisible(),"Undock transfer lost/repeated action or target");
        require((target->mapToGlobal({8,8})-dropPosition).manhattanLength()<=2,"Floating button was not anchored under pointer");
        require(host.size()==initialSize,"Undock drag changed initial floating size");
        require(host.windowOpacity()==1,"Anchored undock host remained invisible");
        require(!PaneUndockDrag::ownsHost(&host),"Completed tearoff retained activation ownership");
        require(!host.property("unboundUndockDragging").isValid(),"Completed tearoff retained stacking priority");
        send(target,QEvent::MouseButtonRelease,{8,8},Qt::LeftButton,Qt::NoButton);
        require(clicks==1 && !host.interactiveDragActive(),"Drag release clicked or retained movement");
    }
    std::puts("PASS: docked-button click threshold, transfer/replacement, delayed release, anchored size and release suppression");
}

void testIndexHeaderDrag() {
    testUndockDrag();
    testMetadataHeaders(false);
    testMetadataHeaders(true);
    testIndexHeaderDrag(false);
    testIndexHeaderDrag(true);
    testIndexHeaderDrag(false,true);
}
