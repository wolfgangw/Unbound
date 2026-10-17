// SPDX-License-Identifier: MIT
#ifndef UNBOUND_PANESETTINGS_H
#define UNBOUND_PANESETTINGS_H
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QMap>
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
        if(error.error!=QJsonParseError::NoError || !document.isObject() || version<1 || version>5
            || !(version>=4 ? root.value("pages").isObject() : root.value("panes").isObject())) {
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
            if(version<5 || !previousPath.isEmpty()) {
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
private:
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
        const auto bytes=QJsonDocument(QJsonObject{{"version",5},{"pages",pages}}).toJson();
        if(!file.open(QIODevice::WriteOnly) || file.write(bytes)!=bytes.size() || !file.commit())
            return QStringLiteral("Cannot save Unbound settings: %1 (%2)").arg(m_path,file.errorString());
        m_pages=pages; return {};
    }
    QString m_path,m_readError;
    QJsonObject m_pages;
};
}
#endif
