// SPDX-License-Identifier: MIT
#include <QApplication>
#include <QAction>
#include <QAbstractButton>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLayout>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QScreen>
#include <QSet>
#include <QStackedWidget>
#include <qt_windows.h>

static QString identity(const QObject *o)
{
    return o ? QString::number(quintptr(o),16) : QString();
}
static QJsonArray rect(QRect r) { return {r.x(),r.y(),r.width(),r.height()}; }
static QJsonObject describe(QWidget *w)
{
    QJsonObject result{{"id",identity(w)},{"parent",identity(w->parentWidget())},
        {"window",identity(w->window())},{"name",w->objectName()},
        {"class",w->metaObject()->className()},{"visible",w->isVisible()},
        {"hidden",w->isHidden()},{"geometry",rect(w->geometry())},
        {"minimum",QJsonArray{w->minimumWidth(),w->minimumHeight()}},
        {"flags",QString::number(qulonglong(w->windowFlags()),16)}};
    if (auto b=qobject_cast<QAbstractButton *>(w)) {
        result.insert("checked",b->isChecked()); result.insert("enabled",b->isEnabled());
        result.insert("tooltip",b->toolTip());
    }
    if (auto stack=qobject_cast<QStackedWidget *>(w)) result.insert("currentIndex",stack->currentIndex());
    if (auto layout=w->layout()) {
        QJsonArray items;
        for (int i=0;i<layout->count();++i) {
            auto item=layout->itemAt(i);
            items.append(QJsonObject{{"widget",identity(item->widget())},{"geometry",rect(item->geometry())}});
        }
        result.insert("layout",QJsonObject{{"class",layout->metaObject()->className()},{"items",items}});
    }
    QJsonArray methods;
    for (int i=QWidget::staticMetaObject.methodCount();i<w->metaObject()->methodCount();++i) {
        auto m=w->metaObject()->method(i);
        methods.append(QJsonObject{{"signature",QString::fromLatin1(m.methodSignature())},
            {"type",int(m.methodType())},{"access",int(m.access())}});
    }
    result.insert("methods",methods);
    QJsonObject properties;
    for (int i=QWidget::staticMetaObject.propertyCount();i<w->metaObject()->propertyCount();++i) {
        auto p=w->metaObject()->property(i);
        if (p.isReadable()) properties.insert(p.name(),QJsonValue::fromVariant(p.read(w)));
    }
    result.insert("properties",properties);
    return result;
}
extern "C" __declspec(dllexport) DWORD WINAPI UnboundStart(void *)
{
    auto app=qobject_cast<QApplication *>(QCoreApplication::instance());
    if (!app || QCoreApplication::closingDown()) return 0;
    return QMetaObject::invokeMethod(app,[] {
        QJsonArray widgets,actions,screens;
        QSet<QWidget *> included;
        QSet<QAction *> includedActions;
        auto add=[&](QWidget *w) { if (!included.contains(w)) { included.insert(w); widgets.append(describe(w)); } };
        for (auto screen:QApplication::screens()) screens.append(QJsonObject{
            {"name",screen->name()},{"geometry",rect(screen->geometry())},
            {"available",rect(screen->availableGeometry())},{"dpr",screen->devicePixelRatio()}});
        const QSet<QString> names={"UiMainWindow","m_pConformPanel","frameEditPageWidgetsContainer",
            "frameEditPageVideoPlayers","frameEditPlayersContainer","stackedEditSourceOfflinePlayers",
            "widgetEditTimelinePlayer","widgetEditSourcePlayer","widgetEditOfflinePlayer","widgetEditSourceTimelinePlayer",
            "frameEditTimelinesHolder","timelineBarAnchor_Splitter","stackedWidget","pageEditFullTimelineTools"};
        for (auto w:QApplication::allWidgets()) {
            if (names.contains(w->objectName())) {
                add(w);
                for (auto child:w->findChildren<QWidget *>(QString(),Qt::FindDirectChildrenOnly)) add(child);
                for (auto parent=w->parentWidget();parent;parent=parent->parentWidget()) add(parent);
            }
            // Header controls are scoped by a player ancestor, since these
            // names occur in Sound Library/Inspector players as well.
            if (w->objectName()=="framePlayerTime" || w->objectName()=="btnViewerMode" || w->objectName()=="btnOptions") {
                for (auto p=w->parentWidget();p;p=p->parentWidget()) {
                    if (p->objectName().startsWith("widgetEdit") && p->metaObject()->className()==QByteArray("UiPlayerWidgetImp")) {
                        add(w); break;
                    }
                }
            }
            if (auto menu=qobject_cast<QMenu *>(w)) for (auto action:menu->actions()) {
                if (includedActions.contains(action)) continue;
                includedActions.insert(action);
                const auto text=action->text();
                if (!text.contains("Timeline",Qt::CaseInsensitive) && !text.contains("Screen",Qt::CaseInsensitive)
                    && !text.contains("Viewer",Qt::CaseInsensitive)) continue;
                actions.append(QJsonObject{{"id",identity(action)},{"name",action->objectName()},
                    {"text",text},{"enabled",action->isEnabled()},{"checked",action->isChecked()},
                    {"checkable",action->isCheckable()},{"menuTitle",menu->title()},
                    {"parent",identity(action->parent())}});
            }
        }
        HMODULE module=nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&UnboundStart),&module);
        wchar_t path[32768]; const auto length=GetModuleFileNameW(module,path,32768);
        const auto directory=QString::fromWCharArray(path,int(length)).section('\\',0,-2);
        QFile output(QDir(directory).filePath(QStringLiteral("viewers-%1-%2.json").arg(GetCurrentProcessId()).arg(QDateTime::currentMSecsSinceEpoch())));
        if (output.open(QIODevice::WriteOnly)) output.write(QJsonDocument(QJsonObject{
            {"widgets",widgets},{"actions",actions},{"screens",screens},{"truncated",false}}).toJson());
    },Qt::QueuedConnection) ? 1 : 0;
}
