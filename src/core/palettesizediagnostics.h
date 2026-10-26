// SPDX-License-Identifier: MIT
#pragma once
#ifdef UNBOUND_PALETTE_SIZE_DIAGNOSTICS
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QPointer>
#include <QSaveFile>
#include <QStackedWidget>
#include <QTimer>
#include <QWidget>

namespace Unbound {
// Internal-build observation only. No text values, addresses or native calls.
inline void recordPaletteSizes(QWidget *host,const QString &pane) {
    QTimer::singleShot(250,host,[guard=QPointer<QWidget>(host),pane] {
        if(!guard) return;
        auto size=[](QSize s) { return QJsonArray{s.width(),s.height()}; };
        QJsonArray widgets;
        auto all=guard->findChildren<QWidget *>(); all.prepend(guard);
        // Bound output even if a future native palette contains an enormous tree.
        const int count=qMin(all.size(),2048);
        for(int i=0;i<count;++i) {
            auto w=all.at(i);
            const auto policy=w->sizePolicy();
            QJsonObject item{{"index",i},{"parent",all.indexOf(w->parentWidget())},
                {"name",w->objectName()},{"class",QString::fromLatin1(w->metaObject()->className())},
                {"size",size(w->size())},{"minimum",size(w->minimumSize())},
                {"maximum",size(w->maximumSize())},{"hint",size(w->sizeHint())},
                {"minimumHint",size(w->minimumSizeHint())},{"hidden",w->isHidden()},
                {"horizontalPolicy",int(policy.horizontalPolicy())},
                {"verticalPolicy",int(policy.verticalPolicy())}};
            if(auto stack=qobject_cast<QStackedWidget *>(w))
                item.insert("currentPage",all.indexOf(stack->currentWidget()));
            if(auto layout=w->layout()) {
                QJsonArray entries;
                for(int j=0;j<qMin(layout->count(),2048);++j) {
                    auto entry=layout->itemAt(j);
                    entries.append(QJsonObject{{"widget",all.indexOf(entry->widget())},
                        {"minimum",size(entry->minimumSize())},{"hint",size(entry->sizeHint())},
                        {"maximum",size(entry->maximumSize())},{"empty",entry->isEmpty()}});
                }
                item.insert("layout",QJsonObject{{"minimum",size(layout->minimumSize())},
                    {"hint",size(layout->sizeHint())},{"constraint",int(layout->sizeConstraint())},
                    {"entries",entries}});
            }
            widgets.append(item);
        }
        const auto directory=QDir::tempPath()+QStringLiteral("/Unbound-palette-sizes");
        if(!QDir().mkpath(directory)) return;
        QSaveFile file(directory+QLatin1Char('/')+pane+QStringLiteral(".json"));
        if(!file.open(QIODevice::WriteOnly)) return;
        file.write(QJsonDocument(QJsonObject{{"pane",pane},{"truncated",all.size()>count},
            {"widgets",widgets}}).toJson());
        file.commit();
    });
}
}
#endif
