// SPDX-License-Identifier: MIT
#pragma once
#include "panesettings.h"
#include "panegeometryrestore.h"
#include <functional>
namespace Unbound {
inline QRect gridSavedRect(QJsonObject saved,const QList<QRect> &areas) {
    if(saved.contains("qtFallback"))saved=saved.value("qtFallback").toObject();
    QRect rect;if(!readRectangle(saved.value("clientRect"),rect))return {};
    if(saved.value("maximized").toBool() || saved.value("fullScreen").toBool()) {
        const auto normal=reachableWindowGeometry(rect,areas);
        for(const auto &area:areas)if(area.contains(normal.center()))return area;
    }
    return reachableWindowGeometry(rect,areas);
}
inline QVector<QRect> gridPageReferences(const PaneSettings &settings,const QString &current,
    const QList<QRect> &areas,const std::function<QRect(const QJsonObject &)> &mainRect) {
    QVector<QRect> result;
    auto append=[&](QRect rect) {if(!rect.isEmpty() && !result.contains(rect))result.append(rect);};
    if(current.isEmpty())return result;
    for(const auto &page:PaneCatalogue::pages()) {
        if(page.objectName==current)continue;
        const auto config=settings.pageConfiguration(page.objectName);
        const auto main=config.value("mainWindow").toObject();
        if(!main.isEmpty())append(mainRect(main));
        const auto panes=config.value("panes").toObject();
        for(const auto &descriptor:PaneCatalogue::panes()) {
            if(descriptor.page!=page.id)continue;
            const auto pane=panes.value(descriptor.objectName).toObject();
            if(pane.value("floating").toBool())append(gridSavedRect(pane.value("window").toObject(),areas));
        }
    }
    return result;
}
}
