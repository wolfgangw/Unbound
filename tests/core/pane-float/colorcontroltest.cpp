// SPDX-License-Identifier: MIT
#include "panedecoration.h"
#include "sidebarfloatsession.h"
#include "layoutadapter.h"
#include <QStackedWidget>
#include <QPushButton>
#include <cstdio>

namespace {
class NativePaletteDialog : public QDialog {
public:
    NativePaletteDialog(QWidget *scope,QWidget *slot,QWidget *palette)
        : QDialog(scope),slot(slot),palette(palette) {
        setObjectName("UiFloatingPaletteWidgetDialog"); new QVBoxLayout(this);
        setMinimumSize(960,420);
    }
    void expand() { registered=true; layout()->addWidget(palette); palette->show(); show(); }
    bool registered=false,rejectClose=false;
    void reject() override {
        if(rejectClose) return;
        registered=false; slot->layout()->addWidget(palette); palette->show();
        QDialog::reject();
    }
private:
    QWidget *slot,*palette;
};
}

void testColorControls() {
    using namespace Unbound;
    using LayoutTest::require;
    {
        QWidget host;
        host.setMinimumSize(600,400); host.resize(600,400); host.show();
        bool restored=false;
        new PaletteGeometryRestore(&host,QSize(600,300),[] { return true; },[&] { restored=true; });
        WindowInteractionEvent::notify(&host);
        host.setMinimumHeight(200); LayoutTest::settle();
        require(!restored,"Deferred palette recall overrode a user geometry gesture");
    }
    {
        QWidget host;
        auto layout=new QVBoxLayout(&host); layout->setContentsMargins(0,0,0,0);
        auto controls=new QWidget; controls->setMinimumSize(300,180);layout->addWidget(controls);
        host.resize(600,300);host.show();LayoutTest::settle();
        new PaletteGeometryRestore(&host,QSize(600,300),[] {return true;},[&] {host.resize(600,300);});
        // Other panes are still initializing. The final native allocation can
        // raise and then lower this palette's minimum after its first second.
        QTimer::singleShot(600,&host,[&] {QCoreApplication::postEvent(&host,new QEvent(QEvent::LayoutRequest));});
        QTimer::singleShot(1100,controls,[=] {controls->setMinimumHeight(400);});
        QTimer::singleShot(1150,controls,[=] {controls->setMinimumHeight(180);});
        QEventLoop loop;QTimer::singleShot(1350,&loop,&QEventLoop::quit);loop.exec();
        require(host.size()==QSize(600,300),"Ongoing native layout outlived palette geometry restoration");
    }
    QWidget page; page.setObjectName("WStackPage_Color"); page.resize(800,500);
    auto outer=new QVBoxLayout(&page);
    auto stack=new QStackedWidget; stack->setObjectName("UiSecWidgetContainer"); outer->addWidget(stack);
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
        const bool expanded=ColorNativePalette::supports(slot);
        auto owner=key || expanded ? content : new QWidget;
        owner->setObjectName(control.headerOwner);
        if(!key && !expanded) body->addWidget(owner);
        auto ownerLayout=key || expanded ? body : new QVBoxLayout(owner);
        auto header=new QFrame; header->setObjectName(control.header); ownerLayout->addWidget(header);
        auto row=new QHBoxLayout(header); row->addWidget(new QLabel(control.label),1);
        QWidget *paletteTitle=nullptr;
        QPushButton *paletteAction=nullptr;
        if(control.objectName=="sizingTab" || control.objectName=="trackerTab" || control.objectName=="blurTab" || control.objectName=="colorWheelsTab" || control.objectName=="tab3D") {
            // A control ancestor models UiPaletteSelector's exclusion from
            // generic header dragging. Only its plain child frame is a root.
            auto selector=new QPushButton; selector->setObjectName(control.objectName=="sizingTab" ? "cbWidgets" : control.objectName=="trackerTab" ? "cbBoxTrackStableSelection" : control.objectName=="colorWheelsTab" ? "cbColorWidgets" : "cmbSetTo"); row->addWidget(selector,1);
            auto selectorRow=new QHBoxLayout(selector);
            auto titleFrame=new QFrame; selectorRow->addWidget(titleFrame,1);
            auto titleRow=new QHBoxLayout(titleFrame);
            paletteTitle=new QLabel("Native tool title"); titleRow->addWidget(paletteTitle);
            paletteAction=new QPushButton("Native mode"); selectorRow->addWidget(paletteAction);
        }
        auto expand=new QPushButton("Expand"); expand->setObjectName("btnUndock"); row->addWidget(expand);
        NativePaletteDialog *dialog=nullptr;
        if(expanded) {
            if(control.objectName!="colorMapperTab") content->setMinimumSize(360,180);
            dialog=new NativePaletteDialog(stack,slot,content);
            // Keep QDialog's real closeEvent: hidden close does not call reject.
            // Resolve returns registered palettes in its virtual reject path.
            dialog->expand(); dialog->hide();
            require(dialog->close() && dialog->registered,"Hidden QDialog close unexpectedly returned palette");
            dialog->reject();
            require(!dialog->registered && content->parentWidget()==slot,"Native reject fixture failed");
            QObject::connect(expand,&QPushButton::clicked,dialog,[dialog,stack,other] {
                dialog->expand(); stack->setCurrentWidget(other);
            });
        }
        QToolButton *options=nullptr;
        if(!control.anchor.isEmpty()) { options=new QToolButton; options->setObjectName(control.anchor); row->addWidget(options); }
        QWidget *wheels=nullptr;
        QStackedWidget *warperModes=nullptr;
        QList<QSizePolicy> warperPolicies;
        if(control.objectName=="colorMapperTab") {
            warperModes=new QStackedWidget(content); warperModes->setObjectName("m_WidgetPanel");
            body->addWidget(warperModes);
            for(const auto minimum:{QSize(747,337),QSize(586,300),QSize(360,180)}) {
                auto mode=new QWidget; auto modeLayout=new QVBoxLayout(mode);
                modeLayout->setContentsMargins(0,0,0,0);
                auto controls=new QWidget; controls->setMinimumSize(minimum); modeLayout->addWidget(controls);
                warperPolicies.append(mode->sizePolicy()); warperModes->addWidget(mode);
            }
            warperModes->setCurrentIndex(2);
        }
        QSizePolicy originalPolicy;
        if(control.objectName=="colorWheelsTab") {
            wheels=new QWidget; wheels->setObjectName("colorWheelsStackedWidget"); body->addWidget(wheels);
            originalPolicy=wheels->sizePolicy();
            auto wheelLayout=new QVBoxLayout(wheels);
            auto top=new QWidget; top->setObjectName("primaryTopHoldFrame"); wheelLayout->addWidget(top);
            auto groups=new QHBoxLayout(top);
            for(int i=0;i<4;++i) { auto group=new QPushButton("Wheel"); group->setMinimumWidth(250); groups->addWidget(group); }
            const int compactMinimum=colorWheelsMinimumWidth(wheels);
            auto bars=new QWidget; bars->setObjectName("primaryTopHoldFrame"); wheelLayout->addWidget(bars);
            auto barsLayout=new QHBoxLayout(bars);
            barsLayout->setContentsMargins(30,10,30,10); barsLayout->setSpacing(40);
            for(int i=0;i<4;++i) { auto group=new QPushButton("Wheel"); group->setMinimumWidth(250); barsLayout->addWidget(group); }
            require(colorWheelsMinimumWidth(wheels)==compactMinimum,
                "Expanded Bars padding became a permanent Wheels minimum");
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
        if(warperModes) {
            QWidget savedHost;
            savedHost.setGeometry(100,100,600,300);
            session.setSavedGeometry(savedHost.saveGeometry());
            auto controls=warperModes->currentWidget()->layout()->itemAt(0)->widget();
            controls->setMinimumHeight(400);
            QTimer::singleShot(0,controls,[controls] { controls->setMinimumHeight(180); });
        }
        require(session.start(slot,control.label,&page,false,false,descriptor->preserveNativeLayout(),false).isEmpty(),
            "Color control detach failed");
        decoration.setFloating(true); LayoutTest::settle();
        if(warperModes) require(session.floatingWindow()->height()==300,
            "Temporary native palette minimum replaced saved startup height");
        if(warperModes) {
            const auto saved=session.floatingWindow()->saveGeometry();
            auto controls=warperModes->currentWidget()->layout()->itemAt(0)->widget();
            session.applySavedGeometry(saved);
            // Native recall can first succeed, then temporarily raise a minimum
            // while the remaining native mode/layout events are delivered.
            controls->setMinimumHeight(400);
            QTimer::singleShot(0,controls,[controls] { controls->setMinimumHeight(180); });
            LayoutTest::settle();
            require(session.floatingWindow()->height()==300,
                "Retained palette lost saved height to a later native layout pass");
            WindowInteractionEvent::notify(session.floatingWindow());
        }
        if(dialog) require(dialog->registered && dialog->parentWidget()==session.floatingWindow()
            && !dialog->isWindow(),"Expanded palette registration/ownership was lost");
        auto checkPaletteSizing=[&] {
            auto host=session.floatingWindow();
            host->resize(600,300); LayoutTest::settle();
            require(host->width()<960 && host->height()<420,
                "Expanded dialog minimum still constrains the Unbound host");
            host->resize(1,1); LayoutTest::settle();
            require(content->minimumSize()==(warperModes ? QSize(0,0) : QSize(360,180))
                && host->width()>=360 && host->height()>=180,
                "Palette control minimum was lost");
            require(dialog->registered,"Resizing lost expanded palette registration");
        };
        if(dialog) checkPaletteSizing();
        if(warperModes) {
            auto host=session.floatingWindow();
            for(int index:{0,1,2}) {
                warperModes->setCurrentIndex(index); LayoutTest::settle();
                host->resize(1,1); LayoutTest::settle();
                const int expected=index==0 ? 747 : index==1 ? 586 : 360;
                require(host->width()>=expected,"Active Warper mode lost its native minimum");
                if(index!=0) require(host->width()<747,"Inactive Warper mode still imposes its minimum");
                require(warperModes->currentWidget()->sizePolicy()==warperPolicies[index],
                    "Active Warper mode sizing policy changed");
                require(dialog->registered,"Warper mode switch lost native registration");
            }
            auto late=new QWidget; auto lateLayout=new QVBoxLayout(late);
            auto lateControls=new QWidget; lateControls->setMinimumSize(900,500); lateLayout->addWidget(lateControls);
            const auto latePolicy=late->sizePolicy(); warperModes->addWidget(late); LayoutTest::settle();
            host->resize(600,300); LayoutTest::settle();
            require(host->width()<747,"Late hidden Warper mode inflated host minimum");
            warperModes->setCurrentWidget(late); LayoutTest::settle();
            host->resize(1,1); LayoutTest::settle();
            require(host->width()>=900 && host->height()>=500,"Late active mode lost control minimums");
            warperModes->setCurrentIndex(2); warperModes->removeWidget(late); LayoutTest::settle();
            require(late->sizePolicy()==latePolicy,"Removed mode policy was not restored");
            delete late;
            checkPaletteSizing();
        }
        if(key) {
            require(slot->findChild<QWidget *>(control.headerOwner,Qt::FindDirectChildrenOnly)==content
                && native->itemAt(0)->widget()==content && content->layout()!=body,
                "Native tool container disappeared from its original stack slot");
            require(session.floatingWindow()->layout()==body,"Retained tool content layout was not transferred intact");
        }
        if(paletteTitle && QGuiApplication::platformName()!=QLatin1String("offscreen")) {
            auto host=static_cast<PaneFloatingWindow *>(session.floatingWindow());
            auto gesture=[&](QWidget *target,bool expected) {
                const QPoint start(3,target->height()/2);
                auto send=[&](QEvent::Type type,QPoint point,Qt::MouseButton button,Qt::MouseButtons held) {
                    QMouseEvent event(type,point,target->mapToGlobal(point),button,held,Qt::NoModifier);
                    QApplication::sendEvent(target,&event);
                };
                send(QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
                send(QEvent::MouseMove,start+QPoint(QApplication::startDragDistance()+1,0),Qt::NoButton,Qt::LeftButton);
                require(host->interactiveDragActive()==expected,"Palette title/action drag routing incorrect");
                send(QEvent::MouseButtonRelease,start,Qt::LeftButton,Qt::NoButton);
                require(!host->interactiveDragActive(),"Palette header retained drag after release");
            };
            gesture(paletteTitle,true);
            gesture(paletteAction,false);
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
        if(dialog) {
            auto host=session.floatingWindow();
            auto controls=warperModes ? warperModes->currentWidget()->layout()->itemAt(0)->widget() : content;
            const int nativeMinimum=controls->minimumHeight();
            // Page return is not layout recall: preserve the most recent working
            // size, including manual changes never saved to a layout snapshot.
            for(const int height:{320,344}) {
                WindowInteractionEvent::notify(host);
                host->resize(650,height); LayoutTest::settle();
                const auto working=host->geometry();
                page.hide(); LayoutTest::settle();
                page.show(); LayoutTest::settle();
                controls->setMinimumHeight(450); LayoutTest::settle();
                require(host->height()>working.height(),"Page-return fixture did not grow the native palette");
                controls->setMinimumHeight(nativeMinimum); LayoutTest::settle();
                require(host->geometry()==working,"Page return retained a temporary native palette size");
            }
            WindowInteractionEvent::notify(host);
        }
        if(dialog) {
            dialog->rejectClose=true;
            require(!session.dock().isEmpty() && session.active() && dialog->registered
                && content->window()==session.floatingWindow(),"Rejected native close lost floating palette");
            checkPaletteSizing();
            dialog->rejectClose=false;
        }
        require(session.dock().isEmpty(),"Color control dock failed"); LayoutTest::settle();
        if(dialog) require(!dialog->registered && dialog->parentWidget()==stack && dialog->isWindow(),
            "Native palette close did not restore registration/ownership");
        if(dialog) require(dialog->minimumSize()==QSize(960,420),"Native dialog minimum was not restored");
        if(warperModes) for(int i=0;i<warperModes->count();++i)
            require(warperModes->widget(i)->sizePolicy()==warperPolicies[i],"Docking failed to restore Warper mode policies");
        require(!session.active() && slot->layout()==native && content->parentWidget()==slot,
            "Native Color layout was not restored");
        if(lateExpand) require(lateExpand->isEnabled(),"Late Scopes Expand was not restored on docking");
        if(wheels) require(wheels->sizePolicy()==originalPolicy && wheels->minimumWidth()==0,"Wheel native sizing policy not restored");
        if(key) require(content->layout()==body,"Retained tool original content layout was not restored");
        require(expand->isEnabled() && stack->currentWidget()==other,"Dock changed native stack selection or failed to restore Expand");
        stack->setCurrentWidget(slot); value->click(); require(clicks==2,"Native action lost after docking");
        if(dialog) {
            decoration.setFloating(false);
            // Take over an already-native-expanded palette whose slot is empty.
            expand->click(); LayoutTest::settle();
            require(native->count()==0 && dialog->registered,"Native expansion fixture failed");
            decoration.refreshNativeHeader();
            require(decoration.button()->parentWidget()==header,"Already expanded palette lost header button");
            require(session.start(slot,control.label,&page,false,false,true,false).isEmpty(),
                "Already expanded palette could not be hosted");
            decoration.setFloating(true); LayoutTest::settle();
            checkPaletteSizing();
            dialog->close(); LayoutTest::settle();
            require(!session.active() && !dialog->registered && content->parentWidget()==slot,
                "Native close did not route through normal session docking");
            // A queued close must not affect a subsequent floating session.
            require(session.start(slot,control.label,&page,false,false,true,false).isEmpty(),"Repeated expansion failed");
            QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);
            QApplication::sendEvent(dialog,&escape); LayoutTest::settle();
            require(!session.active() && !dialog->registered,"Expanded palette Escape did not dock");
            delete dialog;
        }
        // Destruction order: decoration/session outlive their content through
        // this scope, so remove the test page after both objects are destroyed.
        stack->removeWidget(slot); slot->hide(); slot->deleteLater();
    }
    LayoutTest::settle();
    std::puts("PASS: twenty Color tool headers, stack-slot preservation, native actions, Expand guard, page retention and docking");
}
