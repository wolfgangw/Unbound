// SPDX-License-Identifier: MIT
#include "panecatalogue.h"
#include "panesettings.h"
#include "layoutadapter.h"
#include <QTemporaryDir>
#include <QSet>
using LayoutTest::require;
void testPaneCatalogue() {
    using namespace Unbound;
    QTemporaryDir directory;
    PaneSettings settings(directory.filePath("catalogue.json"));
    QSet<QString> names,keys,toggles;
    for(const auto &pane:PaneCatalogue::panes()) {
        const auto &page=PaneCatalogue::page(pane.page);
        require(!names.contains(pane.objectName) && !keys.contains(pane.settingsKey()),"Duplicate pane identity in catalogue");
        names.insert(pane.objectName); keys.insert(pane.settingsKey());
        require(page.supported && !pane.label.isEmpty(),"Pane registered without supported page or label");
        require(PaneCatalogue::pageForRoot(page.paneRoot)==page.objectName,"Pane discovery root belongs to another page");
        require(PaneSettings::pageForPane(pane.settingsKey())==page.objectName,"Pane is not persisted on its discovery page");
        for(const auto &toggle:pane.toggleNames()) {
            require(!toggles.contains(toggle),"Native toggle is ambiguously assigned to multiple panes"); toggles.insert(toggle);
        }
        require(settings.saveFloating(pane.settingsKey(),true).isEmpty(),"Cannot save registered pane");
        require(settings.saveToggle(pane.settingsKey(),false).isEmpty(),"Cannot save registered pane toggle");
    }
    PaneSettings reloaded(directory.filePath("catalogue.json"));
    for(const auto &pane:PaneCatalogue::panes())
        require(reloaded.floating(pane.settingsKey()) && reloaded.hasToggle(pane.settingsKey()) && !reloaded.toggle(pane.settingsKey()),
            "Registered pane did not roundtrip through shared persistence");
    for(const auto &invalid:{"Audio/frameAudioPanelMetersContainer","Color/m_pWidgetClips","Color/m_pWidgetLightBox",
            "frameAudioPanelGroupsContainer","Audio/frameEditPageMediaPool","Fusion/unknown"})
        require(PaneSettings::pageForPane(invalid).isEmpty(),"Unsupported or incorrectly namespaced pane was accepted");

    // Historical v4 fixture is independent of the catalogue: moving/renaming
    // catalogue rows must never drop or move users' existing saved panes.
    const QByteArray v4=R"({"version":4,"pages":{
        "edit":{"panes":{"mediaPool":{"floating":true},"mixer":{"floating":true},
            "inspector":{"floating":true},"metadata":{"floating":true},"effects":{"floating":true},
            "soundLibrary":{"floating":true},"keyframeEditor":{"floating":true},"editIndex":{"floating":true}}},
        "color":{"panes":{"nodes":{"floating":true},"gallery":{"floating":true},
            "luts":{"floating":true},"mediaPool":{"floating":true}}}}})";
    const auto path=directory.filePath("legacy.json");
    QFile file(path); require(file.open(QIODevice::WriteOnly),"Cannot seed legacy catalogue fixture"); file.write(v4); file.close();
    PaneSettings migrated(path);
    require(migrated.readError().isEmpty(),"Catalogue migration failed");
    const QStringList edit={"frameEditPageMediaPool","frameEditAudioMeter","frameEditPageInspector","frameEditPageMetadata",
        "frameEditPageEffectsLibrary","frameEditPageSoundLibrary","frameEditPageKeyframeEditor","frameEditPageEditIndex"};
    const QStringList color={"m_pFrameNodeGraph","m_pWidgetGallery","m_pWidgetLutBrowser","m_pWidgetMediaPool"};
    require(migrated.pageConfiguration("WStackPage_Conform").value("panes").toObject().size()==edit.size()
        && migrated.pageConfiguration("WStackPage_Color").value("panes").toObject().size()==color.size(),
        "Catalogue migration lost or invented legacy panes");
    for(const auto &name:edit) require(migrated.floating(name),"Legacy Edit key changed");
    for(const auto &name:color) require(migrated.floating("Color/"+name),"Legacy Color key changed");
    require(PaneSettings::pageForPane("Audio/frameAudioPanelGroupsContainer")=="WStackPage_Audio"
        && PaneSettings::pageForPane("widgetEditTimelinePlayer")=="WStackPage_Conform"
        && PaneSettings::pageForPane("stackedEditSourceOfflinePlayers")=="WStackPage_Conform",
        "Fairlight or viewer compatibility keys changed");
    require(PaneCatalogue::find("frameEditPageMediaPool")->toggleNames()==QStringList{
        "btnEditToggleMediaPoolInSSM","btnEditToggleMediaPoolInDSM","btnEditToggleMediaPoolInEFTM"}
        && PaneCatalogue::find("m_pWidgetGallery")->toggleNames()==QStringList{"btnColorToggleGallery","btnColorToggleGalleryInDSM"}
        && PaneCatalogue::find("frameAudioPanelGroupsContainer")->toggleNames()==QStringList{"btnAudioPanelGroupsInSSM","btnAudioPanelGroupsInDSM"},
        "Native selector variants changed");
    std::puts("PASS: shared pane identities, selector uniqueness, persistence roundtrip and legacy key compatibility");
}
