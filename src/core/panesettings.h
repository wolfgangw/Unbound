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
#include "settingslocation.h"
#include "layouttracediagnostics.h"
#include <QMap>
#include <QSet>
#include <QUuid>
#include "geometryjson.h"
#include "panecatalogue.h"
namespace Unbound {
class PaneSettings
{
    // Format 7 wire names stay stable across the terminology change, so existing
    // installations can read and write the same layouts without a migration.
    static constexpr const char *LayoutsKey="presets";
    static constexpr const char *ActiveLayoutKey="activePreset";
    static constexpr const char *NextLayoutNumberKey="nextPresetNumber";
public:
    // The optional data root lets shared migration fixtures use an isolated
    // directory without impersonating a particular OS's environment.
    explicit PaneSettings(QString path = QString(),
        const QString &dataDirectory = Platform::settingsDataDirectory()) : m_path(path)
    {
        if (m_path.isEmpty()) m_path=qEnvironmentVariable("UNBOUND_SETTINGS_FILE");
        QString previousPath;
        if (m_path.isEmpty()) {
            m_path=QDir(dataDirectory).filePath(QStringLiteral("Unbound/Unbound.json"));
            if(!QFile::exists(m_path)) previousPath=QDir(dataDirectory).filePath(QStringLiteral("Unbound/settings.json"));
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
            || (version>=6 && !validLayouts(root.value("pages").toObject(),version>=7))) {
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
            if(version<7) m_pages=withLayoutIds(m_pages);
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
            if(it.key()==QLatin1String("Color/m_pLightBox")) continue; // Retired pre-format-4 experiment.
            const auto geometry=placementJson(QByteArray::fromBase64(it.value().toString().toLatin1()));
            if(!geometry.isEmpty()) setEntry(m_pages,it.key(),"window",geometry);
        }
        for(auto it=floating.begin();it!=floating.end();++it)
            if(it.key()!=QLatin1String("Color/m_pLightBox") && it.value().isBool()) setEntry(m_pages,it.key(),"floating",it.value());
        for(auto it=toggles.begin();it!=toggles.end();++it)
            if(it.key()!=QLatin1String("Color/m_pLightBox") && it.value().isBool()) setEntry(m_pages,it.key(),"toolbarButtonChecked",it.value());
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
    // Additive format-7 field: old files/snapshots need no rewrite or migration.
    // Only an explicit preference change writes it; existing layouts stay intact.
    bool keepAtop(const QString &name) const { return entry(name).value("keepAtop").toBool(); }
    QString saveKeepAtop(const QString &name,bool value) {
        // Missing means false. Returning to the default also returns an old
        // layout to clean, rather than introducing a redundant false field.
        return saveField(name,"keepAtop",value ? QJsonValue(true) : QJsonValue(QJsonValue::Undefined));
    }
    bool hasToggle(const QString &name) const { return entry(name).value("toolbarButtonChecked").isBool(); }
    bool toggle(const QString &name) const { return entry(name).value("toolbarButtonChecked").toBool(); }
    QString saveToggle(const QString &name,bool value) { return saveField(name,"toolbarButtonChecked",value); }
    static QString pageForPane(const QString &name) { return location(name).first; }
    QString readError() const { return m_readError; }
    static QString pageLabel(const QString &page) {
        for(const auto &entry:PaneCatalogue::pages())
            if(entry.supported && entry.objectName==page) return entry.label;
        return {};
    }
    QJsonObject pageConfiguration(const QString &page) const {
        const auto state=m_pages.value(page).toObject();
        QJsonObject result;
        if(state.contains("mainWindow")) result.insert("mainWindow",state.value("mainWindow"));
        result.insert("panes",state.value("panes").toObject());
        return result;
    }
    QJsonArray layouts(const QString &page) const { return m_pages.value(page).toObject().value(LayoutsKey).toArray(); }
    QJsonObject layout(const QString &page,const QString &id) const {
        for(const auto &value:layouts(page)) if(value.toObject().value("id").toString()==id)
            return value.toObject().value("configuration").toObject();
        return {};
    }
    QString layoutName(const QString &page,const QString &id) const {
        for(const auto &value:layouts(page)) if(value.toObject().value("id").toString()==id)
            return value.toObject().value("name").toString();
        return {};
    }
    QString activeLayout(const QString &page) const {
        return m_pages.value(page).toObject().value(ActiveLayoutKey).toString();
    }
    bool layoutDirty(const QString &page,const QString &id) const {
        return !id.isEmpty() && layout(page,id)!=pageConfiguration(page);
    }
    QString clearActiveLayout(const QString &page) {
        if(!m_readError.isEmpty()) return m_readError;
        auto updated=m_pages; auto state=updated.value(page).toObject();
        if(!state.contains(ActiveLayoutKey)) return {};
        state.remove(ActiveLayoutKey); updated.insert(page,state); return write(updated);
    }
    QString renameLayout(const QString &page,const QString &id,QString name) {
        if(!m_readError.isEmpty()) return m_readError;
        name=name.trimmed();
        if(name.isEmpty()) return QStringLiteral("Enter a layout name.");
        if(name.size()>128) return QStringLiteral("Layout names can have up to 128 characters.");
        auto saved=layouts(page);
        for(int i=0;i<saved.size();++i) if(saved[i].toObject().value("id").toString()==id) {
            auto item=saved[i].toObject();
            if(item.value("name").toString()==name) return {};
            item.insert("name",name); saved[i]=item;
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert(LayoutsKey,saved); updated.insert(page,state); return write(updated);
        }
        return QStringLiteral("The panel layout no longer exists.");
    }
    QString updateLayout(const QString &page,const QString &id) {
        if(!m_readError.isEmpty()) return m_readError;
        auto saved=layouts(page);
        for(int i=0;i<saved.size();++i) if(saved[i].toObject().value("id").toString()==id) {
            auto item=saved[i].toObject(); item.insert("configuration",pageConfiguration(page)); saved[i]=item;
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert(LayoutsKey,saved); state.insert(ActiveLayoutKey,id);
            updated.insert(page,state); return write(updated);
        }
        return QStringLiteral("The panel layout no longer exists.");
    }
    QString saveLayout(const QString &page,QString &name,bool &duplicate) {
        if(!m_readError.isEmpty()) return m_readError;
        if(pageLabel(page).isEmpty()) return QStringLiteral("This page does not support panel layouts.");
        const auto configuration=pageConfiguration(page);
        auto saved=layouts(page);
        duplicate=false;
        for(const auto &value:saved) if(value.toObject().value("configuration").toObject()==configuration) {
            const auto item=value.toObject(); name=item.value("name").toString(); duplicate=true;
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert(ActiveLayoutKey,item.value("id")); updated.insert(page,state);
            return updated==m_pages ? QString() : write(updated);
        }
        if(saved.size()>=5) return QStringLiteral("Five layouts are saved. Delete one to make room.");
        auto updated=m_pages;
        auto state=updated.value(page).toObject();
        int number=qMax(1,state.value(NextLayoutNumberKey).toInt(1));
        QSet<QString> names;
        for(const auto &value:saved) names.insert(value.toObject().value("name").toString());
        do { name=pageLabel(page)+"-"+QString::number(number++); } while(names.contains(name));
        const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        saved.append(QJsonObject{{"id",id},{"name",name},{"configuration",configuration}});
        state.insert(LayoutsKey,saved); state.insert(NextLayoutNumberKey,number); state.insert(ActiveLayoutKey,id);
        updated.insert(page,state); return write(updated);
    }
    QString deleteLayout(const QString &page,const QString &id) {
        if(!m_readError.isEmpty()) return m_readError;
        auto saved=layouts(page);
        for(int i=0;i<saved.size();++i) if(saved[i].toObject().value("id").toString()==id) {
            saved.removeAt(i);
            auto updated=m_pages; auto state=updated.value(page).toObject();
            state.insert(LayoutsKey,saved);
            if(state.value(ActiveLayoutKey).toString()==id) state.remove(ActiveLayoutKey);
            updated.insert(page,state); return write(updated);
        }
        return {};
    }
    QString activateLayout(const QString &page,const QString &id) {
        if(!m_readError.isEmpty()) return m_readError;
        const auto configuration=layout(page,id);
        if(configuration.isEmpty()) return QStringLiteral("The panel layout no longer exists.");
        auto updated=m_pages; auto state=updated.value(page).toObject();
        if(configuration.contains("mainWindow")) state.insert("mainWindow",configuration.value("mainWindow"));
        else state.remove("mainWindow");
        state.insert("panes",configuration.value("panes").toObject());
        state.insert(ActiveLayoutKey,id);
        updated.insert(page,state); return write(updated);
    }
private:
    static QJsonObject withLayoutIds(QJsonObject pages) {
        for(auto page=pages.begin();page!=pages.end();++page) {
            auto state=page.value().toObject();
            if(!state.contains(LayoutsKey)) { state.remove(ActiveLayoutKey); page.value()=state; continue; }
            auto saved=state.value(LayoutsKey).toArray();
            const auto active=state.value(ActiveLayoutKey).toString(); state.remove(ActiveLayoutKey);
            for(int i=0;i<saved.size();++i) {
                auto item=saved[i].toObject(); const auto id=QUuid::createUuid().toString(QUuid::WithoutBraces);
                item.insert("id",id); saved[i]=item;
                if(item.value("name").toString()==active) state.insert(ActiveLayoutKey,id);
            }
            state.insert(LayoutsKey,saved); page.value()=state;
        }
        return pages;
    }
    static bool validLayouts(const QJsonObject &pages,bool stableIds) {
        for(auto page:pages) {
            const auto state=page.toObject();
            if(!state.contains(LayoutsKey)) continue;
            if(!state.value(LayoutsKey).isArray() || state.value(LayoutsKey).toArray().size()>5) return false;
            QSet<QString> identities;
            for(const auto &value:state.value(LayoutsKey).toArray()) {
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
            if(stableIds && state.contains(ActiveLayoutKey) &&
                !identities.contains(state.value(ActiveLayoutKey).toString())) return false;
        }
        return true;
    }
    static QMap<QString,QString> legacyPanes()
    {
        QMap<QString,QString> names;
        for(const auto &pane:PaneCatalogue::panes())
            if(!pane.legacyName.isEmpty()) names.insert(pane.settingsKey(),pane.legacyName);
        return names;
    }
    static QMap<QString,QString> legacyPages()
    {
        QMap<QString,QString> names;
        for(const auto &page:PaneCatalogue::pages()) names.insert(page.objectName,page.legacyName);
        return names;
    }
    static QPair<QString,QString> location(const QString &name)
    {
        if(name.startsWith("MainWindow/")) return {legacyPages().contains(name.mid(11)) ? name.mid(11) : QString(),"mainWindow"};
        const auto pane=PaneCatalogue::findSettingsKey(name);
        return pane ? qMakePair(PaneCatalogue::page(pane->page).objectName,pane->objectName) : QPair<QString,QString>();
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
        LayoutTraceSpan trace("settings_atomic_write");
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
