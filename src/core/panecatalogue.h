// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANECATALOGUE_H
#define UNBOUND_PANECATALOGUE_H
#include <QMap>
#include <QStringList>
#include <QVector>
namespace Unbound {
// Resolve metadata is shared by every backend. Adding a page/pane here feeds
// discovery, decoration and persistence; specialized layout adapters stay in core.
namespace PaneCatalogue {
enum class Page { Edit, Color, Fairlight, Media, Cut, Fusion, Deliver };
enum class Kind { Sidebar, Viewer };
enum class Header { None, MediaPool, SoundLibrary, Keyframes, Effects, Index,
    Inspector, Metadata, Mixer, Nodes, Gallery, Luts, Groups, Adr, TimelineCorner };
struct PageDescriptor {
    Page id;
    QString objectName, label, legacyName, paneRoot, settingsPrefix;
    QString togglePrefix;
    QStringList toggleSuffixes;
    bool supported;
};
inline const QVector<PageDescriptor> &pages() {
    static const QVector<PageDescriptor> entries={
        {Page::Edit,"WStackPage_Conform","Edit","edit","frameEditPageWidgetsContainer","","btnEditToggle",{"InSSM","InDSM","InEFTM"},true},
        {Page::Color,"WStackPage_Color","Color","color","WStackPage_Color","Color/","",{"","InDSM"},true},
        {Page::Fairlight,"WStackPage_Audio","Fairlight","fairlight","WStackPage_Audio","Audio/","btnAudioPanel",{"InSSM","InDSM"},true},
        {Page::Media,"WStackPage_Browse","Media","media","WStackPage_Browse","Media/","",{},false},
        {Page::Cut,"WStackPage_Cut","Cut","cut","WStackPage_Cut","Cut/","",{},false},
        {Page::Fusion,"WStackPage_Fusion","Fusion","fusion","WStackPage_Fusion","Fusion/","",{},false},
        {Page::Deliver,"WStackPage_Deliver","Deliver","deliver","WStackPage_Deliver","Deliver/","",{},false}
    };
    return entries;
}
inline const PageDescriptor &page(Page id) {
    for(const auto &entry:pages()) if(entry.id==id) return entry;
    Q_UNREACHABLE();
}
struct PaneDescriptor {
    QString objectName, label;
    Page page;
    QString toggleName, legacyName;
    Header header;
    bool initializeBeforeRestore;
    Kind kind=Kind::Sidebar;
    QString additionalDragHeader;
    QString settingsKey() const { return PaneCatalogue::page(page).settingsPrefix+objectName; }
    QStringList toggleNames() const {
        QStringList names;
        if(toggleName.isEmpty()) return names;
        const auto &owner=PaneCatalogue::page(page);
        for(const auto &suffix:owner.toggleSuffixes) names.append(owner.togglePrefix+toggleName+suffix);
        return names;
    }
    bool preserveNativeLayout() const { return page==Page::Color; }
    bool requireNativeHeader() const { return page==Page::Fairlight; }
    bool serializeInitialization() const { return page==Page::Color || page==Page::Fairlight; }
};
inline const QVector<PaneDescriptor> &panes() {
    static const QVector<PaneDescriptor> entries={
        {"frameEditPageMediaPool","Media Pool",Page::Edit,"MediaPool","mediaPool",Header::MediaPool,false},
        {"frameEditPageSoundLibrary","Sound Library",Page::Edit,"SoundLibrary","soundLibrary",Header::SoundLibrary,false},
        {"frameEditPageKeyframeEditor","Keyframe Editor",Page::Edit,"KeyframeEditor","keyframeEditor",Header::Keyframes,false},
        {"frameEditPageEffectsLibrary","Effects",Page::Edit,"EffectsLibrary","effects",Header::Effects,false},
        {"frameEditPageEditIndex","Edit Index",Page::Edit,"EditIndex","editIndex",Header::Index,false,Kind::Sidebar,"frameEditPageIndexTabContainer"},
        {"frameEditPageInspector","Inspector",Page::Edit,"Inspector","inspector",Header::Inspector,true},
        {"frameEditPageMetadata","Metadata",Page::Edit,"Metadata","metadata",Header::Metadata,true},
        {"frameEditAudioMeter","Mixer",Page::Edit,"Mixer","mixer",Header::Mixer,true},
        {"m_pFrameNodeGraph","Nodes",Page::Color,"btnColorToggleNodeGraph","nodes",Header::Nodes,true},
        {"m_pWidgetGallery","Gallery",Page::Color,"btnColorToggleGallery","gallery",Header::Gallery,true},
        {"m_pWidgetLutBrowser","LUTs",Page::Color,"btnColorToggleLutsBrowser","luts",Header::Luts,true},
        {"m_pWidgetMediaPool","Media Pool",Page::Color,"btnColorToggleMediaPool","mediaPool",Header::MediaPool,true},
        {"m_pFrameOFXPlugin","Effects",Page::Color,"btnColorToggleOpenFX","",Header::Effects,true},
        {"timelineFrame","Timeline",Page::Color,"btnColorToggleTimeline","",Header::TimelineCorner,true},
        {"frameAudioPanelMediaPoolContainer","Media Pool",Page::Fairlight,"MediaPool","",Header::MediaPool,true},
        {"frameAudioPanelEffectsLibraryContainer","Effects",Page::Fairlight,"EffectsLibrary","",Header::Effects,true},
        {"frameAudioPanelIndexContainer","Index",Page::Fairlight,"Index","",Header::Index,true,Kind::Sidebar,"frameAudioPageIndexTabContainer"},
        {"frameAudioPanelGroupsContainer","Groups",Page::Fairlight,"Groups","",Header::Groups,true},
        {"frameAudioPanelSoundLibraryContainer","Sound Library",Page::Fairlight,"SoundLibrary","",Header::SoundLibrary,true},
        {"frameAudioPanelAdrContainer","ADR",Page::Fairlight,"Adr","",Header::Adr,true},
        {"frameTimelineMixerContainer","Mixer",Page::Fairlight,"Mixer","",Header::Mixer,true},
        {"frameMetadataContainer","Metadata",Page::Fairlight,"Metadata","",Header::Metadata,true},
        {"frameInspectorContainer","Inspector",Page::Fairlight,"Inspector","",Header::Inspector,true},
        {"widgetEditTimelinePlayer","Program Viewer",Page::Edit,"","",Header::None,false,Kind::Viewer},
        {"stackedEditSourceOfflinePlayers","Source Viewer",Page::Edit,"","",Header::None,false,Kind::Viewer}
    };
    return entries;
}
inline const PaneDescriptor *find(const QString &objectName) {
    for(const auto &entry:panes()) if(entry.objectName==objectName) return &entry;
    return nullptr;
}
inline const PaneDescriptor *findSettingsKey(const QString &key) {
    for(const auto &entry:panes()) if(entry.settingsKey()==key) return &entry;
    return nullptr;
}
inline QMap<QString,QString> sidebarNames() {
    QMap<QString,QString> names;
    for(const auto &entry:panes()) if(entry.kind==Kind::Sidebar) names.insert(entry.objectName,entry.label);
    return names;
}
inline QString pageForRoot(const QString &name) {
    for(const auto &entry:pages())
        if(entry.supported && (entry.objectName==name || entry.paneRoot==name)) return entry.objectName;
    return {};
}
inline bool matchesHeader(Header header,const QString &name,const QString &parent,const QString &grandparent) {
    switch(header) {
    case Header::MediaPool: return name=="btnMediaPoolOptions" && parent=="frameMediaPoolTitleBarTop";
    case Header::Inspector: return name=="btnTitleOptions" && parent=="frameEffectsWidgetsTitle";
    case Header::Mixer: return name=="btnAudioMixerOptions" && (parent=="frameOptions" || parent=="frameHeaderContainer");
    case Header::Effects: return (name=="btnOptions" && parent=="frameEffectTopBar")
        || (name=="btnOFXPoolOptions" && parent=="frameTopRight" && grandparent=="frameOFXTitle");
    case Header::SoundLibrary: return name=="m_BtnDisplayOptions" && parent=="frameMenu";
    case Header::Keyframes: return name=="btnCurveSettingsOptions" && parent=="keyframeToolbarButtons";
    case Header::Index: return (name=="btnEdlOptions" && parent=="frameEDLTopBar")
        || (name=="m_BtnMenu" && (parent=="frameTitlebar" || parent=="frameHeaderTitle"));
    case Header::Metadata: return name=="btnPresetsSelector" && parent=="frameMetadataTop";
    case Header::Luts: return name=="btnOptions" && parent=="frameLutBrowserTitleBarTop";
    case Header::Gallery: return name=="btnOptions" && parent=="frameTopColorGallery";
    case Header::Nodes: return name=="btnOptions" && parent=="rightFrame" && grandparent=="m_NGHeaderFrame";
    case Header::Groups: return name=="m_BtnMenu" && parent=="frameHeaderTitle";
    case Header::Adr: return name=="optionsBtn" && parent=="headerFrame";
    case Header::None: return false;
    case Header::TimelineCorner: return false;
    }
    return false;
}
} // namespace PaneCatalogue
} // namespace Unbound
#endif
