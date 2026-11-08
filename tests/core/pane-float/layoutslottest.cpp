// SPDX-License-Identifier: MIT
#include "panesettings.h"
#include "pagelayoutmenu.h"
#include "layoutadapter.h"
#include <QTemporaryDir>
void testLayoutSlots() {
    using namespace Unbound;
    using LayoutTest::require;
    QTemporaryDir directory;
    const auto path=directory.filePath("slots.json");
    const QString page="WStackPage_Conform",pane="frameEditPageMediaPool";
    QJsonArray saved;
    for(int i=1;i<=5;++i)saved.append(QJsonObject{{"id",QString("id%1").arg(i)},
        {"name",QString("Name %1").arg(i)},{"configuration",QJsonObject{{"panes",QJsonObject{
            {pane,QJsonObject{{"floating",true},{"marker",i}}}}}}}});
    const auto active=saved[4].toObject();
    auto state=active.value("configuration").toObject();
    state.insert("presets",saved);state.insert("activePreset","id5");state.insert("nextPresetNumber",12);
    const QJsonObject legacy{{"version",7},{"pages",QJsonObject{{page,state}}}};
    QFile file(path);require(file.open(QIODevice::WriteOnly),"Cannot seed slot migration");
    const auto bytes=QJsonDocument(legacy).toJson();file.write(bytes);file.close();
    PaneSettings settings(path);
    require(settings.readError().isEmpty() && settings.activeLayout(page)=="id5" && !settings.layoutDirty(page,"id5"),
        "Slot migration changed active layout or working state");
    for(int slot=1;slot<=5;++slot) {
        auto expected=saved[slot-1].toObject();expected.insert("slot",slot);
        require(settings.layoutAtSlot(page,slot)==expected,"Migration changed slot order or snapshot");
    }
    QFile backup(path+".pre-v8");require(backup.open(QIODevice::ReadOnly) && backup.readAll()==bytes,"Pre-slot backup not preserved");
    require(settings.deleteLayout(page,"id2").isEmpty() && settings.deleteLayout(page,"id4").isEmpty(),"Cannot empty slots");
    require(settings.layoutAtSlot(page,2).isEmpty() && settings.layoutAtSlot(page,4).isEmpty()
        && settings.layoutAtSlot(page,5).value("id").toString()=="id5"
        && settings.layoutAtSlot(page,3).value("id").toString()=="id3","Deletion shifted occupied slots");
    settings.saveFloating(pane,false);
    QString name;
    require(settings.saveLayout(page,name).isEmpty() && name=="Edit-12"
        && settings.layoutAtSlot(page,2).value("id").toString()==settings.activeLayout(page),"Save did not fill lowest gap");
    settings.saveKeepAtop(pane,true);
    require(settings.saveLayout(page,name).isEmpty()
        && settings.layoutAtSlot(page,4).value("id").toString()==settings.activeLayout(page),"Save did not fill next gap");
    require(settings.renameLayout(page,"id5","Renamed").isEmpty() && settings.updateLayout(page,"id5").isEmpty()
        && settings.layoutAtSlot(page,5).value("id").toString()=="id5","Rename/update moved a slot");
    PaneSettings reloaded(path);
    require(reloaded.layouts(page)==settings.layouts(page) && reloaded.activeLayout(page)=="id5","Slot state did not persist");
    settings.deleteLayout(page,"id2"); // Already absent: unrelated slots remain untouched.
    const auto full=settings.layouts(page);
    require(full.size()==5 && settings.firstEmptyLayoutSlot(page)==0,"Full slot detection failed");
    // Reject malformed version-8 slot assignments without touching the file.
    for(const auto invalid:QList<QJsonValue>{0,6,1.5,"2",QJsonValue()}) {
        auto bad=full;auto item=bad[1].toObject();item.insert("slot",invalid);bad[1]=item;
        auto badState=state;badState.insert("presets",bad);
        const auto badBytes=QJsonDocument(QJsonObject{{"version",8},{"pages",QJsonObject{{page,badState}}}}).toJson();
        require(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot seed invalid slots");file.write(badBytes);file.close();
        PaneSettings rejected(path);
        require(!rejected.readError().isEmpty(),"Invalid slot accepted");
        require(file.open(QIODevice::ReadOnly) && file.readAll()==badBytes,"Invalid slot file was rewritten");file.close();
    }
    auto duplicateSlots=full;auto item=duplicateSlots[1].toObject();item.insert("slot",1);duplicateSlots[1]=item;
    auto badState=state;badState.insert("presets",duplicateSlots);
    require(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"Cannot seed duplicate slot");
    file.write(QJsonDocument(QJsonObject{{"version",8},{"pages",QJsonObject{{page,badState}}}}).toJson());file.close();
    require(!PaneSettings(path).readError().isEmpty(),"Duplicate slot accepted");
    QWidget main;
    PaneSettings empty(directory.filePath("empty.json"));
    PageLayoutMenu menu(empty,page,&main);
    const auto numbers=menu.findChildren<QLabel *>("unboundLayoutSlotNumber");
    const auto names=menu.findChildren<QPushButton *>("unboundLayoutRecall");
    require(numbers.size()==5 && names.size()==5,"Menu does not allocate five slots");
    for(auto button:names)require(button->text().isEmpty() && !button->isEnabled(),"Empty slot name is not blank and disabled");
    require(!menu.findChild<QLabel *>("unboundLayoutStatus"),"Empty menu still shows a count/status line");
    std::puts("PASS: v7 slot migration/backup, stable gaps, lowest-slot reuse, rename/update, persistence, invalid slots and five blank numbered rows");
}
