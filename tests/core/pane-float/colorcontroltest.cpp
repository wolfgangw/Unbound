// SPDX-License-Identifier: MIT
#include "panedecoration.h"
#include "sidebarfloatsession.h"
#include "layoutadapter.h"
#include <QStackedWidget>
#include <QPushButton>
#include <cstdio>

void testColorControls() {
    using namespace Unbound;
    using LayoutTest::require;
    QWidget page; page.setObjectName("WStackPage_Color"); page.resize(800,500);
    auto outer=new QVBoxLayout(&page);
    auto stack=new QStackedWidget; outer->addWidget(stack);
    auto other=new QWidget; stack->addWidget(other);
    page.show(); LayoutTest::settle();
    for(const auto &control:PaneCatalogue::colorControls()) {
        auto slot=new QWidget; slot->setObjectName(control.objectName); stack->addWidget(slot);
        auto native=new QVBoxLayout(slot);
        auto content=new QWidget; auto body=new QVBoxLayout(content);
        native->addWidget(content);
        // Camera Raw/HDR own multiple layout entries, transferred as a unit.
        if(control.wholeLayout) native->addWidget(new QLabel("Footer"));
        const bool key=control.objectName=="keyTab" || control.objectName=="sizingTab";
        auto owner=key ? content : new QWidget;
        owner->setObjectName(control.headerOwner);
        if(!key) body->addWidget(owner);
        auto ownerLayout=key ? body : new QVBoxLayout(owner);
        auto header=new QFrame; header->setObjectName(control.header); ownerLayout->addWidget(header);
        auto row=new QHBoxLayout(header); row->addWidget(new QLabel(control.label),1);
        QWidget *sizingTitle=nullptr;
        QPushButton *sizingAction=nullptr;
        if(control.objectName=="sizingTab") {
            // A control ancestor models UiPaletteSelector's exclusion from
            // generic header dragging. Only its plain child frame is a root.
            auto selector=new QPushButton; selector->setObjectName("cbWidgets"); row->addWidget(selector,1);
            auto selectorRow=new QHBoxLayout(selector);
            auto titleFrame=new QFrame; selectorRow->addWidget(titleFrame,1);
            auto titleRow=new QHBoxLayout(titleFrame);
            sizingTitle=new QLabel("Sizing - Input Sizing"); titleRow->addWidget(sizingTitle);
            sizingAction=new QPushButton("Native mode"); selectorRow->addWidget(sizingAction);
        }
        auto expand=new QPushButton("Expand"); expand->setObjectName("btnUndock"); row->addWidget(expand);
        QToolButton *options=nullptr;
        if(!control.anchor.isEmpty()) { options=new QToolButton; options->setObjectName(control.anchor); row->addWidget(options); }
        QWidget *wheels=nullptr;
        QSizePolicy originalPolicy;
        if(control.objectName=="colorWheelsTab") {
            wheels=new QWidget; wheels->setObjectName("colorWheelsStackedWidget"); body->addWidget(wheels);
            originalPolicy=wheels->sizePolicy();
            auto wheelLayout=new QVBoxLayout(wheels);
            auto top=new QWidget; top->setObjectName("primaryTopHoldFrame"); wheelLayout->addWidget(top);
            auto groups=new QHBoxLayout(top);
            for(int i=0;i<4;++i) { auto group=new QPushButton("Wheel"); group->setMinimumWidth(250); groups->addWidget(group); }
        }
        auto value=new QPushButton("Native tool action"); body->addWidget(value);
        int clicks=0; QObject::connect(value,&QPushButton::clicked,[&]{++clicks;});
        stack->setCurrentWidget(slot); LayoutTest::settle();
        PaneDecoration decoration(slot,&page,[]{});
        require(decoration.usesNativeHeader() && decoration.button()->parentWidget()==header,
            "Color control missing native header button");
        if(options) require(row->indexOf(decoration.button())==row->indexOf(options)-1,
            "Color control button must precede options");
        const auto descriptor=PaneCatalogue::find(control.objectName);
        require(descriptor && descriptor->toggleNames().isEmpty(),"Color tool selector became a visibility toggle");
        SidebarFloatSession session;
        require(session.start(slot,control.label,&page,false,false,descriptor->preserveNativeLayout(),false).isEmpty(),
            "Color control detach failed");
        decoration.setFloating(true); LayoutTest::settle();
        if(key) {
            require(slot->findChild<QWidget *>(control.headerOwner,Qt::FindDirectChildrenOnly)==content
                && native->itemAt(0)->widget()==content && content->layout()!=body,
                "Native tool container disappeared from its original stack slot");
            require(session.floatingWindow()->layout()==body,"Retained tool content layout was not transferred intact");
        }
        if(sizingTitle && QGuiApplication::platformName()!=QLatin1String("offscreen")) {
            auto host=static_cast<PaneFloatingWindow *>(session.floatingWindow());
            auto gesture=[&](QWidget *target,bool expected) {
                const QPoint start(3,target->height()/2);
                auto send=[&](QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons held) {
                    QMouseEvent event(type,point,target->mapToGlobal(point),button,held,Qt::NoModifier);
                    QApplication::sendEvent(target,&event);
                };
                send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
                send(QEvent::MouseMove,start+QPoint(QApplication::startDragDistance()+1,0),Qt::NoButton,Qt::LeftButton);
                require(host->interactiveDragActive()==expected,"Sizing title/action drag routing incorrect");
                send(QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);
                require(!host->interactiveDragActive(),"Sizing header retained drag after release");
            };
            gesture(sizingTitle,true);
            gesture(sizingAction,false);
        }
        if(wheels) {
            require(wheels->sizePolicy().horizontalPolicy()==QSizePolicy::Ignored,"Wheel allocation still feeds host minimum");
            auto host=session.floatingWindow(); host->resize(1600,600); LayoutTest::settle();
            host->resize(850,600); LayoutTest::settle();
            require(host->width()<1600,"Color Wheels cannot shrink after expansion");
        }
        QPointer<QPushButton> lateExpand;
        if(control.objectName=="scopesWidget") {
            auto lateOwner=new QWidget(content); lateOwner->setObjectName(control.headerOwner); body->addWidget(lateOwner);
            auto lateLayout=new QVBoxLayout(lateOwner);
            auto lateHeader=new QWidget; lateHeader->setObjectName(control.header); lateLayout->addWidget(lateHeader);
            auto lateRow=new QHBoxLayout(lateHeader);
            auto menu=new QToolButton; menu->setObjectName(control.anchor); lateRow->addWidget(menu);
            lateExpand=new QPushButton("Expand"); lateExpand->setObjectName("btnUndock"); lateRow->addWidget(lateExpand);
            LayoutTest::settle();
            require(!lateExpand->isEnabled(),"Late Scopes Expand escaped guard");
            lateExpand->setEnabled(true);
            require(!lateExpand->isEnabled(),"Native Scopes update re-enabled Expand");
            decoration.refreshNativeHeader(); decoration.refreshNativeHeader();
            auto buttons=lateHeader->findChildren<QToolButton *>("resolvePaneUndockButton");
            require(buttons.size()==1 && lateRow->indexOf(buttons.first())==lateRow->indexOf(menu)-1,
                "Late Scopes header missing or duplicated button");
        }
        expand->setEnabled(true);
        require(!expand->isEnabled(),"Native update re-enabled floating Expand");
        require(stack->count()==2 && stack->widget(1)==slot,"Color stack indices changed on detach");
        require(value->window()==session.floatingWindow() && !expand->isEnabled(),"Tool content or native Expand ownership incorrect");
        stack->setCurrentWidget(other); LayoutTest::settle();
        require(session.floatingWindow()->isVisible() && value->isVisible(),"Native selector hid floating tool");
        value->click(); require(clicks==1,"Native tool connection lost");
        page.hide(); LayoutTest::settle(); require(!session.floatingWindow()->isVisible(),"Tool stayed visible on another page");
        page.show(); LayoutTest::settle(); require(session.floatingWindow()->isVisible(),"Tool failed page return");
        require(session.dock().isEmpty(),"Color control dock failed"); LayoutTest::settle();
        require(!session.active() && slot->layout()==native && content->parentWidget()==slot,
            "Native Color layout was not restored");
        if(lateExpand) require(lateExpand->isEnabled(),"Late Scopes Expand was not restored on docking");
        if(wheels) require(wheels->sizePolicy()==originalPolicy && wheels->minimumWidth()==0,"Wheel native sizing policy not restored");
        if(key) require(content->layout()==body,"Retained tool original content layout was not restored");
        require(expand->isEnabled() && stack->currentWidget()==other,"Dock changed native stack selection or failed to restore Expand");
        stack->setCurrentWidget(slot); value->click(); require(clicks==2,"Native action lost after docking");
        // Destruction order: decoration/session outlive their content through
        // this scope, so remove the test page after both objects are destroyed.
        stack->removeWidget(slot); slot->hide(); slot->deleteLater();
    }
    LayoutTest::settle();
    std::puts("PASS: twenty Color tool headers, stack-slot preservation, native actions, Expand guard, page retention and docking");
}
