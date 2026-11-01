// SPDX-License-Identifier: MIT
#include "editscopes.h"
#include "colornativescopes.h"
#include "layoutadapter.h"
#include <QTemporaryDir>
#include <QPushButton>
#include <QStackedWidget>
#include <cstdio>
using LayoutTest::require;
using LayoutTest::settle;
namespace {
class DepartureCapture final : public QObject {
public:
    QWidget *page=nullptr;
    std::function<void()> capture;
    DepartureCapture() {qApp->installEventFilter(this);}
    ~DepartureCapture() override {qApp->removeEventFilter(this);}
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==page && event->type()==QEvent::Hide && capture)capture();
        return false;
    }
};
}
void testEditScopes() {
    for(bool dialogShell:{false,true}) {
        QTemporaryDir directory;
        Unbound::PaneSettings settings(directory.filePath("scopes.json"));
        QWidget main;main.setObjectName("UiMainWindow");main.resize(1100,700);
        QStackedWidget pages(&main);pages.setObjectName("widgetStack_Panel");pages.resize(main.size());
        QWidget edit,color;edit.setObjectName("WStackPage_Conform");color.setObjectName("WStackPage_Color");
        pages.addWidget(&edit);pages.addWidget(&color);
        std::unique_ptr<QWidget> native(dialogShell ? static_cast<QWidget *>(new QDialog(&main)) : new QWidget(&main,Qt::Window));
        native->setObjectName("UiWaveformWindow");native->resize(620,450);
        auto layout=new QVBoxLayout(native.get());auto title=new QWidget;title->setObjectName("titleFrame");layout->addWidget(title);
        auto row=new QHBoxLayout(title);auto hide=new QPushButton("Close");hide->setObjectName("btnHide");row->addWidget(hide);
        auto options=new QPushButton("Options");options->setObjectName("btnWaveformViewerOptions");row->addWidget(options);
        auto render=new QWidget;render->setMinimumSize(300,200);layout->addWidget(render);
        QAction toggle(&main);toggle.setObjectName("workspaceVideoScopesToggle");toggle.setCheckable(true);
        bool registered=false;
        QObject::connect(&toggle,&QAction::triggered,&main,[&](bool on){
            registered=on;native->setVisible(on);
        });
        QObject::connect(hide,&QPushButton::clicked,&main,[&]{
            if(native->isVisible()){registered=false;toggle.setChecked(false);}native->hide();
        });
        main.show();settle();
        // The transition observer exists before lazily created pane managers.
        DepartureCapture departure;departure.page=&edit;
        {
            Unbound::EditScopes scopes(&main,&edit,settings,&main);
            int errors=0;scopes.report=[&](const QString &){++errors;};
            settle();require(!scopes.active() && !scopes.host(),"Closed native Scopes opened on Edit startup");
            main.activateWindow();settle();
            const bool checkActivation=Unbound::nativeApplicationActive();
            toggle.trigger();settle();
            require(scopes.active() && scopes.host() && native->parentWidget()==scopes.host() && registered,
                "Workspace On did not directly adopt native Scopes");
            if(checkActivation)require(scopes.host()->isActiveWindow(),
                "Explicit Workspace opening did not activate the adopted Scopes host");
            auto button=scopes.host()->findChild<Unbound::UndockButton *>("resolvePaneUndockButton");
            require(button && button->isChecked(),"Edit Scopes lacks a floating Unbound button");
            scopes.host()->setGeometry(120,140,660,460);scopes.flushGeometry();
            const auto geometry=scopes.host()->geometry();
            QString saved;bool duplicate=false;
            require(settings.saveLayout(edit.objectName(),saved,duplicate).isEmpty(),"Cannot save Edit Scopes layout");
            bool captured=false;
            departure.capture=[&] {
                captured=true;
                require(scopes.host()->isVisible() && native->isVisible()
                    && native->parentWidget()==scopes.host(),
                    "Edit Scopes disappeared before outgoing transition capture");
            };
            pages.setCurrentWidget(&color);settle();
            departure.capture={};require(captured,"Outgoing Edit capture was not observed");
            require(scopes.active() && !scopes.host()->isVisible() && native->isWindow() && !native->isVisible()
                && !registered && settings.floating(Unbound::EditScopes::key()),"Edit departure lost intent or leaked native Scopes");
            // Color borrows exactly the same renderer, with independent intent.
            QWidget slot(&color),colorHost;auto slotLayout=new QVBoxLayout(&slot);
            auto expand=new QPushButton("Expand");expand->setObjectName("btnUndock");slotLayout->addWidget(expand);slot.show();
            QObject::connect(expand,&QPushButton::clicked,&main,[&]{registered=true;toggle.setChecked(true);native->show();});
            Unbound::ColorNativeScopes colorScopes(&main);
            require(colorScopes.start(&slot,&colorHost,&color).isEmpty(),"Color could not acquire Edit's released renderer");
            colorHost.show();settle();
            require(native->parentWidget()==&colorHost,"Color lost renderer ownership");
            require(colorScopes.suspend(),"Color could not release Scopes for Edit");colorHost.hide();
            pages.setCurrentWidget(&edit);settle();
            require(scopes.host()->isVisible() && native->parentWidget()==scopes.host() && scopes.host()->geometry()==geometry
                && registered,"Edit round trip lost visibility, renderer or geometry");
            require(!settings.layoutDirty(edit.objectName(),settings.activeLayout(edit.objectName())),"Edit page round trip dirtied the Scopes layout");
            button=scopes.host()->findChild<Unbound::UndockButton *>("resolvePaneUndockButton");button->click();settle();
            require(!scopes.active() && !scopes.host() && !native->isVisible() && !registered && !toggle.isChecked(),
                "Edit Scopes docking click did not close native Scopes");
            toggle.trigger();settle();toggle.trigger();settle();
            require(!scopes.active() && !scopes.host(),"Workspace Off failed to close the Unbound host");
            settings.saveFloating(Unbound::EditScopes::key(),true);scopes.restoreLayout();settle();
            require(scopes.host() && registered,"Saved Edit Scopes intent did not initialize the native renderer");
            scopes.prepareLayout({});settle();
            require(!scopes.host() && !registered,"Dock all/closed layout failed to close Edit Scopes");
            require(!errors,"Edit Scopes lifecycle reported an error");
            settings.saveFloating(Unbound::EditScopes::key(),true);
        }
        // A fresh manager restores persisted intent without opening a native
        // window first; no native visibility state is needed for startup.
        {Unbound::EditScopes restored(&main,&edit,settings,&main);settle();
            require(restored.host() && registered,"Cold Edit Scopes restoration failed");}
        require(native->parentWidget()==&main && !registered && settings.floating(Unbound::EditScopes::key()),
            "Shutdown lost Edit intent or stranded the global renderer");
    }
    std::puts("PASS: Edit native Scopes direct opening, close/menu, page ownership, geometry, layouts and startup");
}
