// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANESETTINGS_H
#define UNBOUND_PANESETTINGS_H
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QStandardPaths>
#include <QMap>
#include <QSet>
#include <QUuid>
#include "geometryjson.h"
namespace Unbound {
class PaneSettings
{
public:
    explicit PaneSettings(QString path = QString()) : m_path(path)
    {
        if (m_path.isEmpty()) m_path=qEnvironmentVariable("UNBOUND_SETTINGS_FILE");
        QString previousPath;
        if (m_path.isEmpty()) {
            auto base=qEnvironmentVariable("LOCALAPPDATA");
            if(base.isEmpty()) base=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
            m_path=QDir(base).filePath(QStringLiteral("Unbound/Unbound.json"));
            if(!QFile::exists(m_path)) previousPath=QDir(base).filePath(QStringLiteral("Unbound/settings.json"));
        }
        QFile file(previousPath.isEmpty() ? m_path : previousPath);
        if(!file.exists()) return;
        if(!file.open(QIODevice::ReadOnly) || file.size()>1024*1024) {
            m_readError=QStringLiteral("Cannot read Unbound settings: %1").arg(m_path); return;
        }
        QJsonParseError error;
        const auto document=QJsonDocument::fromJson(file.readAll(),&error);
        file.close();
        const auto root=document.object();
        const int version=root.value("version").toInt();
        if(error.error!=QJsonParseError::NoError || !document.isObject() || version<1 || version>7
            || !(version>=4 ? root.value("pages").isObject() : root.value("panes").isObject())
            || (version>=6 && !validPresets(root.value("pages").toObject(),version>=7))) {
            m_readError=QStringLiteral("Unbound settings are invalid or unsupported: %1. The file was left unchanged.").arg(m_path); return;
        }
        if(version>=4) {
            m_pages=root.value("pages").toObject();
            if(version==4) {
                QJsonObject converted;
                const auto pageNames=legacyPages();
                const auto paneNames=legacyPanes();
                for(auto page=pageNames.begin();page!=pageNames.end();++page) {
                    if(!m_pages.contains(page.value())) continue;
                    auto value=m_pages.value(page.value()).toObject();
                    const auto oldPanes=value.value("panes").toObject();
                    QJsonObject panes;
                    for(auto pane=paneNames.begin();pane!=paneNames.end();++pane) {
                        const auto keys=location(pane.key());
                        if(keys.first==page.key() && oldPanes.contains(pane.value()))
                            panes.insert(keys.second,oldPanes.value(pane.value()));
                    }
                    if(value.contains("panes")) value.insert("panes",panes);
                    converted.insert(page.key(),value);
                }
                m_pages=converted;
            }
            if(version<7) m_pages=withPresetIds(m_pages);
            if(version<7 || !previousPath.isEmpty()) {
                m_readError=write(m_pages);
                if(m_readError.isEmpty() && !previousPath.isEmpty()) QFile::remove(previousPath);
            }
            return;
        }
        // One-time conversion preserves the user's arrangement. Unsupported
        // experimental panes (including Clips/LightBox) are deliberately dropped.
        const auto panes=root.value("panes").toObject();
        const auto floating=root.value("floating").toObject();
        const auto toggles=root.value("toggles").toObject();
        for(auto it=panes.begin();it!=panes.end();++it) {
            const auto geometry=placementJson(QByteArray::fromBase64(it.value().toString().toLatin1()));
            if(!geometry.isEmpty()) setEntry(m_pages,it.key(),"window",geometry);
        }
        for(auto it=floating.begin();it!=floating.end();++it)
            if(it.value().isBool()) setEntry(m_pages,it.key(),"floating",it.value());
        for(auto it=toggles.begin();it!=toggles.end();++it)
            if(it.value().isBool()) setEntry(m_pages,it.key(),"toolbarButtonChecked",it.value());
        m_readError=write(m_pages);
        if(m_readError.isEmpty() && !previousPath.isEmpty()) QFile::remove(previousPath);
    }
    QByteArray geometry(const QString &name) const { return placementBytes(entry(name).value("window").toObject()); }
    QString save(const QString &name,const QByteArray &bytes)
    {
        if(!m_readError.isEmpty()) return m_readError;
        if(bytes.isEmpty()) return {};
        const auto object=placementJson(bytes);
        if(object.isEmpty()) return QStringLiteral("Invalid window geometry for %1").arg(name);
        return saveField(name,"window",object);
    }
    bool floating(const QString &name) const { return entry(name).value("floating").toBool(); }
    QString saveFloating(const QString &name,bool value) { return saveField(name,"floating",value); }
    bool hasToggle(const QString &name) const { return entry(name).value("toolbarButtonChecked").isBool(); }
    bool toggle(const QString &name) const { return entry(name).value("toolbarButtonChecked").toBool(); }
    QString saveToggle(const QString &name,bool value) { return saveField(name,"toolbarButtonChecked",value); }
    static QString pageForPane(const QString &name) { return location(name).first; }
    QString readError() const { return m_readError; }
    static QString pageLabel(const QString &page) {
        if(page=="WStackPage_Conform") return "Edit";
        if(page=="WStackPage_Color") return "Color";
        if(page=="WStackPage_Audio") return "Fairlight";
        return {};
    }
    QJsonObject pageConfiguration(const QString &page) const {
        const auto state=m_pages.value(page).toObject();
        QJsonObject result;
        if(state.contains("mainWindow")) result.insert("mainWindow",state.value("mainWindow"));
        result.insert("panes",state.value("panes").toObject());
        return result;
    }
    QJsonArray presets(const QString &page) const { return m_pages.value(page).toObject().value("presets").toArray(); }
    QJsonObject preset(const QString &page,const QString &id) const {
        for(const auto &value:presets(page)) if(value.toObject().value("id").toString()==id)
            return value.toObject().value("configuration").toObject();
        return {};
    }
    QString presetName(const QString &page,const QString &id) const {
        for(const auto &value:presets(page)) if(value.toObject().value("id").toString()==id)
            return value.toObject().value("name").toString();
        return {};
    }
    QString activePreset(const QString &page) const {
        return m_pages.value(page).toObject().value("activePreset").toString();
    }
    bool presetDirty(const QString &page,const QString &id) const {
        return !id.isEmpty() && preset(page,id)!=pageConfiguration(page);
    }
    QString clearActivePreset(const QString &page) {
        if(!m_readError.isEmpty()) return m_readError;
        auto updated=m_pages; auto state=updated.value(page).toObject();
        if(!state.contains("activePreset")) return {};
        state.remove("activePreset"); updated.insert(page,state); return write(updated);
    }
    QString renamePreset(const QString &page,const QString &id,QString name) {
        if(!m_readError.isEmpty()) return m_readError;
        name=name.trimmed();
        if(name.isEmpty()) return QStringLiteral("Enter a preset name.");
        if(name.size()>128) return QStringLiteral("Preset names can have up to 128 characters.");
        auto saved=presets(page);
        for(int i=0;i<saved.size();++i) if(saved[i].toObject().value("id").toString()==id) {
            auto item=saved[i].toObject();
            if(item.value("name").toString()==name) return {};
            item.insert("name",name); saved[i]=item;
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert("presets",saved); updated.insert(page,state); return write(updated);
        }
        return QStringLiteral("The panel preset no longer exists.");
    }
    QString updatePreset(const QString &page,const QString &id) {
        if(!m_readError.isEmpty()) return m_readError;
        auto saved=presets(page);
        for(int i=0;i<saved.size();++i) if(saved[i].toObject().value("id").toString()==id) {
            auto item=saved[i].toObject(); item.insert("configuration",pageConfiguration(page)); saved[i]=item;
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert("presets",saved); state.insert("activePreset",id);
            updated.insert(page,state); return write(updated);
        }
        return QStringLiteral("The panel preset no longer exists.");
    }
    QString savePreset(const QString &page,QString &name,bool &duplicate) {
        if(!m_readError.isEmpty()) return m_readError;
        if(pageLabel(page).isEmpty()) return QStringLiteral("This page does not support panel presets.");
        const auto configuration=pageConfiguration(page);
        auto saved=presets(page);
        duplicate=false;
        for(const auto &value:saved) if(value.toObject().value("configuration").toObject()==configuration) {
            const auto item=value.toObject(); name=item.value("name").toString(); duplicate=true;
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert("activePreset",item.value("id")); updated.insert(page,state);
            return updated==m_pages ? QString() : write(updated);
        }
        if(saved.size()>=5) return QStringLiteral("Five presets are saved. Delete one to make room.");
        auto updated=m_pages;
        auto state=updated.value(page).toObject();
        int number=qMax(1,state.value("nextPresetNumber").toInt(1));
        QSet<QString> names;
        for(const auto &value:saved) names.insert(value.toObject().value("name").toString());
        do { name=pageLabel(page)+"-"+QString::number(number++); } while(names.contains(name));
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        saved.append(QJsonObject{{"id",id},{"name",name},{"configuration",configuration}});
        state.insert("presets",saved); state.insert("nextPresetNumber",number); state.insert("activePreset",id);
        updated.insert(page,state); return write(updated);
    }
    QString deletePreset(const QString &page,const QString &id) {
        if(!m_readError.isEmpty()) return m_readError;
        auto saved=presets(page);
        for(int i=0;i<saved.size();++i) if(saved[i].toObject().value("id").toString()==id) {
            saved.removeAt(i);
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert("presets",saved);
            if(state.value("activePreset").toString()==id) state.remove("activePreset");
            updated.insert(page,state); return write(updated);
        }
        return {};
    }
    QString activatePreset(const QString &page,const QString &id) {
        if(!m_readError.isEmpty()) return m_readError;
        const auto configuration=preset(page,id);
        if(configuration.isEmpty()) return QStringLiteral("The panel preset no longer exists.");
        auto updated=m_pages; auto state=updated.value(page).toObject();
        if(configuration.contains("mainWindow")) state.insert("mainWindow",configuration.value("mainWindow"));
        else state.remove("mainWindow");
        state.insert("panes",configuration.value("panes").toObject());
        state.insert("activePreset",id);
        updated.insert(page,state); return write(updated);
    }
private:
    static QJsonObject withPresetIds(QJsonObject pages) {
        for(auto page=pages.begin();page!=pages.end();++page) {
            auto state=page.value().toObject();
            if(!state.contains("presets")) { state.remove("activePreset"); page.value()=state; continue; }
            auto saved=state.value("presets").toArray();
            const auto active=state.value("activePreset").toString(); state.remove("activePreset");
            for(int i=0;i<saved.size();++i) {
                auto item=saved[i].toObject(); const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
                item.insert("id",id); saved[i]=item;
                if(item.value("name").toString()==active) state.insert("activePreset",id);
            }
            state.insert("presets",saved); page.value()=state;
        }
        return pages;
    }
    static bool validPresets(const QJsonObject &pages,bool stableIds) {
        for(auto page:pages) {
            const auto state=page.toObject();
            if(!state.contains("presets")) continue;
            if(!state.value("presets").isArray() || state.value("presets").toArray().size()>5) return false;
            QSet<QString> identities;
            for(const auto &value:state.value("presets").toArray()) {
                if(!value.isObject()) return false;
                const auto item=value.toObject(); const auto name=item.value("name").toString();
                const auto identity=item.value(stableIds ? "id" : "name").toString();
                if(name.trimmed().isEmpty() || identity.isEmpty() || identities.contains(identity) ||
                    !item.value("configuration").isObject()) return false;
                identities.insert(identity);
                const auto configuration=item.value("configuration").toObject();
                if(!configuration.value("panes").isObject() ||
                    (configuration.contains("mainWindow") && !configuration.value("mainWindow").isObject())) return false;
            }
            if(stableIds && state.contains("activePreset") &&
                !identities.contains(state.value("activePreset").toString())) return false;
        }
        return true;
    }
    static QMap<QString,QString> legacyPanes()
    {
        return {
            {"frameEditPageMediaPool","mediaPool"},{"frameEditAudioMeter","mixer"},
            {"frameEditPageInspector","inspector"},{"frameEditPageMetadata","metadata"},
            {"frameEditPageEffectsLibrary","effects"},{"frameEditPageSoundLibrary","soundLibrary"},
            {"frameEditPageKeyframeEditor","keyframeEditor"},{"frameEditPageEditIndex","editIndex"},
            {"Color/m_pFrameNodeGraph","nodes"},{"Color/m_pWidgetGallery","gallery"},
            {"Color/m_pWidgetLutBrowser","luts"},{"Color/m_pWidgetMediaPool","mediaPool"}};
    }
    static QMap<QString,QString> legacyPages()
    {
        return {{"WStackPage_Conform","edit"},{"WStackPage_Color","color"},
            {"WStackPage_Audio","fairlight"},{"WStackPage_Browse","media"},{"WStackPage_Cut","cut"},
            {"WStackPage_Fusion","fusion"},{"WStackPage_Deliver","deliver"}};
    }
    static QPair<QString,QString> location(const QString &name)
    {
        if(name.startsWith("MainWindow/")) return {legacyPages().contains(name.mid(11)) ? name.mid(11) : QString(),"mainWindow"};
        static const QStringList audioPanes={"frameAudioPanelMediaPoolContainer","frameAudioPanelEffectsLibraryContainer",
            "frameAudioPanelIndexContainer","frameAudioPanelGroupsContainer","frameAudioPanelSoundLibraryContainer",
            "frameAudioPanelAdrContainer","frameTimelineMixerContainer","frameMetadataContainer","frameInspectorContainer"};
        const bool viewer=name==QLatin1String("widgetEditTimelinePlayer") || name==QLatin1String("stackedEditSourceOfflinePlayers");
        if(!viewer && !legacyPanes().contains(name) && !(name.startsWith("Audio/") && audioPanes.contains(name.mid(6)))) return {};
        return {name.startsWith("Audio/") ? "WStackPage_Audio" : name.startsWith("Color/") ? "WStackPage_Color" : "WStackPage_Conform",name.section('/',-1)};
    }
    QJsonObject entry(const QString &name) const
    {
        const auto keys=location(name);
        const auto page=m_pages.value(keys.first).toObject();
        if(keys.second=="mainWindow") return QJsonObject{{"window",page.value("mainWindow")}};
        return page.value("panes").toObject().value(keys.second).toObject();
    }
    static void setEntry(QJsonObject &pages,const QString &name,const QString &field,const QJsonValue &value)
    {
        const auto keys=location(name);
        if(keys.first.isEmpty() || keys.second.isEmpty()) return;
        auto page=pages.value(keys.first).toObject();
        if(keys.second=="mainWindow") { if(field!="window") return; page.insert("mainWindow",value); }
        else {
            auto panes=page.value("panes").toObject(); auto pane=panes.value(keys.second).toObject();
            pane.insert(field,value); panes.insert(keys.second,pane); page.insert("panes",panes);
        }
        pages.insert(keys.first,page);
    }
    QString saveField(const QString &name,const QString &field,const QJsonValue &value)
    {
        if(!m_readError.isEmpty()) return m_readError;
        if(entry(name).value(field)==value) return {};
        auto updated=m_pages; setEntry(updated,name,field,value);
        if(updated==m_pages) return {};
        return write(updated);
    }
    QString write(const QJsonObject &pages)
    {
        if(!QDir().mkpath(QFileInfo(m_path).absolutePath())) return QStringLiteral("Cannot create Unbound settings directory: %1").arg(m_path);
        QSaveFile file(m_path);
        const auto bytes=QJsonDocument(QJsonObject{{"version",7},{"pages",pages}}).toJson();
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())
            return QStringLiteral("Cannot save Unbound settings: %1 (%2)").arg(m_path,file.errorString());
        m_pages=pages; return {};
    }
    QString m_path,m_readError;
    QJsonObject m_pages;
};
}
#endif
