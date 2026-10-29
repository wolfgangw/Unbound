// SPDX-License-Identifier: MIT
#include "windowintegration.h"
#include "panesettings.h"
#include "pagelayoutmenu.h"
#include "layoutadapter.h"
#include <QDialog>
#include <QTemporaryDir>
using namespace Unbound;
using namespace LayoutTest;
void testWindowGroup() {
    QTemporaryDir directory;
    const auto path=directory.filePath("pinning.json");
    PaneSettings settings(path);
    const QString key="frameEditPageMediaPool",page="WStackPage_Conform";
    require(!settings.keepAtop(key),"Legacy missing pin flag is not false");
    settings.saveFloating(key,true);
    QString name; bool duplicate=false;
    settings.saveLayout(page,name,duplicate);
    const auto id=settings.activeLayout(page);
    require(settings.saveKeepAtop(key,true).isEmpty() && settings.layoutDirty(page,id),"Pin did not dirty layout");
    require(PaneSettings(path).keepAtop(key),"Pin did not survive settings reload");
    settings.saveKeepAtop(key,false);
    require(!settings.layoutDirty(page,id),"Unpin did not restore clean legacy layout");
    settings.saveKeepAtop(key,true); settings.updateLayout(page,id);
    settings.saveKeepAtop(key,false); settings.activateLayout(page,id);
    require(settings.keepAtop(key),"Recall lost saved pin preference");
    settings.saveFloating(key,false);
    require(settings.keepAtop(key),"Docking discarded pin preference");
    // A pre-feature format-7 file must load without any rewrite, then change
    // only the requested working pane when pinning is first used.
    const auto legacyPath=directory.filePath("legacy.json");
    const QJsonObject pane{{"floating",true},{"toolbarButtonChecked",false},
        {"window",QJsonObject{{"clientRect",rectangleJson(QRect(-400,80,640,480))},
                             {"normalClientRect",rectangleJson(QRect(-400,80,640,480))}}}};
    const QJsonObject configuration{{"panes",QJsonObject{{key,pane}}},
        {"mainWindow",QJsonObject{{"maximized",true}}}};
    const QString legacyId="{ebd583d4-2844-4de0-a6fe-854968fbd51a}";
    const QJsonArray presets{QJsonObject{{"id",legacyId},{"name","My layout"},{"configuration",configuration}}};
    const QJsonObject pageData{{"panes",configuration["panes"]},{"mainWindow",configuration["mainWindow"]},
        {"presets",presets},{"activePreset",legacyId},{"nextPresetNumber",4}};
    const QJsonObject legacy{{"version",7},{"pages",QJsonObject{{page,pageData}}}};
    const auto bytes=QJsonDocument(legacy).toJson();
    QFile legacyFile(legacyPath);
    require(legacyFile.open(QIODevice::WriteOnly) && legacyFile.write(bytes)==bytes.size(),"Cannot seed legacy settings");
    legacyFile.close();
    PaneSettings migrated(legacyPath);
    require(migrated.readError().isEmpty() && !migrated.keepAtop(key)
        && !migrated.layoutDirty(page,legacyId) && !migrated.geometry(key).isEmpty(),"Legacy settings changed on load");
    require(legacyFile.open(QIODevice::ReadOnly) && legacyFile.readAll()==bytes,"Loading rewrote legacy settings");
    legacyFile.close();
    require(migrated.saveKeepAtop(key,true).isEmpty(),"Cannot pin legacy pane");
    require(legacyFile.open(QIODevice::ReadOnly),"Cannot read updated legacy settings");
    auto changed=QJsonDocument::fromJson(legacyFile.readAll()).object(); legacyFile.close();
    auto pages=changed["pages"].toObject(); auto savedPage=pages[page].toObject();
    auto panes=savedPage["panes"].toObject(); auto savedPane=panes[key].toObject();
    require(savedPane.take("keepAtop").toBool(),"Pin flag missing from legacy working pane");
    panes[key]=savedPane; savedPage["panes"]=panes; pages[page]=savedPage; changed["pages"]=pages;
    require(changed==legacy,"Pinning changed legacy geometry, toggles, layouts or identity");
    require(migrated.activateLayout(page,legacyId).isEmpty() && !migrated.keepAtop(key)
        && migrated.pageConfiguration(page)==configuration,"Legacy layout recall did not clear pin and preserve placement");
    const auto unsavedPath=directory.filePath("no-layout.json");
    PaneSettings unsaved(unsavedPath); unsaved.saveKeepAtop(key,true);
    require(PaneSettings(unsavedPath).keepAtop(key) && unsaved.activeLayout(page).isEmpty(),
        "Pin persistence without a layout failed");
    std::puts("PASS: per-pane pin default, persistence, dirtiness, canonical unpin and layout recall");
    if(QGuiApplication::platformName()=="offscreen") return;
    QWidget main,outsider;
    main.setObjectName("groupMain"); outsider.setObjectName("groupOutsider");
    PaneFloatingWindow first,second,hidden,minimized;
    // Viewers and audition panes retain a Qt/native owner; ordinary panes do not.
    second.setParent(&main,second.windowFlags());
    WindowGroupActivation group; group.setWindow(&main);
    first.setObjectName("pinA"); second.setObjectName("pinB");
    for(auto window:{&main,&outsider,static_cast<QWidget *>(&first),static_cast<QWidget *>(&second),static_cast<QWidget *>(&minimized)}) {
        window->setGeometry(100,100,400,300); window->show();
    }
    minimized.showMinimized();
    first.setProperty("unboundKeepAtop",true); second.setProperty("unboundKeepAtop",true);
    activate(main); settle();
    require(stackedAbove(first,main) && stackedAbove(second,main),"Pinned panes do not cover ordinary main");
    require(ownsKeyboardFocus(main),"Pinning stole keyboard focus");
    activate(first); settle();
    require(stackedAbove(first,second),"Activated pinned pane did not become first");
    verifyActivationBelowPins(main,{&first,&second});
    require(stackedAbove(first,second) && stackedAbove(second,main),"Ordinary activation changed pin order");
    activate(second); settle(); activate(main); settle();
    require(stackedAbove(second,first) && ownsKeyboardFocus(main),"Pinned order or selected focus was lost");
    activate(first); activate(main); settle();
    {
        PageLayoutMenu menu(settings,page,&main);
        verifyStackingDuring(second,{&first},[&] {
            verifyStackingDuring(main,{&first,&second},[&] { menu.popup(main.mapToGlobal(QPoint(30,30))); });
        });
        require(stackedAbove(first,main) && stackedAbove(second,main),"Opening layout menu covered pinned panes with main");
        require(stackedAbove(menu,first) && stackedAbove(menu,second),"Pinned panes covered layout menu");
        verifyStackingDuring(main,{&first,&second},[&] { activate(menu); });
        require(stackedAbove(first,main) && stackedAbove(second,main) && ownsKeyboardFocus(menu),
            "Active layout menu lost focus or pinned priority");
        settings.activateLayout(page,id); menu.finishRecall(); settle();
        require(stackedAbove(first,main) && stackedAbove(second,main) && stackedAbove(menu,first),
            "Layout recall under open menu lost pinned priority");
        menu.dismiss(); activate(main); settle();
        require(stackedAbove(first,second) && stackedAbove(second,main),"Menu changed pinned activation order");
    }
    activate(outsider); settle();
    require(stackedAbove(outsider,first) && stackedAbove(outsider,second),"Pinned panes covered another application window");
    QDialog dialog(&main); dialog.setModal(true); dialog.show(); activate(dialog); settle();
    require(stackedAbove(dialog,first) && ownsKeyboardFocus(dialog),"Pinning covered modal dialog");
    dialog.hide(); first.showMaximized(); activate(main); settle();
    require(first.isMaximized(),"Fixture did not establish a maximized pane");
    main.showMinimized(); settle();
    require(!nativeVisible(first) && !nativeVisible(second),"Main minimization left floating panes exposed");
    main.showNormal(); activate(main); settle();
    require(nativeVisible(first) && nativeVisible(second) && !first.isMinimized() && !second.isMinimized() && minimized.isMinimized() && hidden.isHidden(),
        "Group restoration changed independent hidden/minimized state");
    require(ownsKeyboardFocus(main),"Group restoration stole focus");
    require(first.isMaximized(),"Grouped minimization lost a maximized pane state");
    first.showNormal();
    second.setProperty("unboundKeepAtop",false); activate(second); settle();
    require(stackedAbove(first,second),"Remaining pinned pane lost priority after unpinning its peer");
    activate(first); settle();
    verifyActivationBelowPins(second,{&first});
    group.setWindow(nullptr);
    std::puts("PASS: native pin/menu order, clean recall, ordinary focus, outsider/modal priority and grouped minimize/restore");
}
