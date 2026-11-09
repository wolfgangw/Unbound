// SPDX-License-Identifier: MIT
#pragma once
#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QMap>
#include <QSet>
#include <functional>
#include "commandapi.h"
#include "scriptcommands.generated.h"

namespace Unbound {
// Free only. Resolve retains ownership of actions, shortcuts and script execution.
// Discover under the named Workspace root, never by positional action identifiers.
class ScriptCommandFallback : public QObject {
public:
    using Dispatch=std::function<void(const ScriptCommand &)>;
    explicit ScriptCommandFallback(QObject *parent, Dispatch dispatch=[](const ScriptCommand &command) {
        UnboundExecuteCommand(1,command.command,command.arguments);
    }) : QObject(parent), m_dispatch(std::move(dispatch)) {
        setObjectName(QStringLiteral("UnboundScriptCommandFallback"));
        qApp->installEventFilter(this);
        for(auto widget:QApplication::allWidgets()) observe(widget);
        refresh();
    }
    ~ScriptCommandFallback() override {
        if(qApp)qApp->removeEventFilter(this);
        for(const auto &connection:m_connections) QObject::disconnect(connection);
    }
private:
    void observe(QObject *object) {
        auto menu=qobject_cast<QMenu *>(object);
        if(!menu || menu->objectName()!=QLatin1String("workspace") || m_roots.contains(menu))return;
        m_roots.insert(menu);
        connect(menu,&QObject::destroyed,this,[this,menu] {m_roots.remove(menu);schedule();});
    }
    QMap<QAction *,const ScriptCommand *> inventory() const {
        QMap<QAction *,const ScriptCommand *> result;
        for(auto root:m_roots)for(auto section:root->actions()) {
            auto scripts=section->menu();
            if(!scripts)continue;
            for(auto group:scripts->actions()) {
                auto menu=group->menu();
                if(!menu || menu->title()!=QLatin1String("Unbound"))continue;
                // Require the complete managed command set, not an unrelated
                // action with the same label elsewhere in Resolve's menus.
                QMap<QAction *,const ScriptCommand *> candidate;
                bool complete=true;
                for(const auto &command:scriptCommands) {
                    QAction *match=nullptr;
                    for(auto action:menu->actions())if(action->text()==QLatin1String(command.label)) {
                        if(match) {complete=false;break;}
                        match=action;
                    }
                    if(!match)complete=false;
                    else candidate.insert(match,&command);
                }
                if(complete)for(auto it=candidate.cbegin();it!=candidate.cend();++it)result.insert(it.key(),it.value());
            }
        }
        return result;
    }
    void refresh() {
        const auto actions=inventory();
        for(auto it=m_connections.begin();it!=m_connections.end();) {
            if(!actions.contains(it.key())) {QObject::disconnect(it.value());it=m_connections.erase(it);}
            else ++it;
        }
        for(auto it=actions.cbegin();it!=actions.cend();++it) {
            auto action=it.key();
            if(m_connections.contains(action))continue;
            m_connections.insert(action,connect(action,&QAction::triggered,this,[this,action] {
                // Revalidate membership if a menu was rebuilt before deferred discovery.
                const auto current=inventory();
                if(auto command=current.value(action,nullptr))m_dispatch(*command);
            }));
            if(!m_observed.contains(action)) {
                m_observed.insert(action);
                connect(action,&QObject::destroyed,this,[this,action] {
                    m_connections.remove(action);m_observed.remove(action);
                });
            }
        }
    }
    void schedule() {
        if(m_queued)return;
        m_queued=true;
        QTimer::singleShot(0,this,[this] {m_queued=false;refresh();});
    }
    bool eventFilter(QObject *object,QEvent *event) override {
        switch(event->type()) {
        case QEvent::ActionAdded: case QEvent::ActionRemoved: case QEvent::ActionChanged:
        case QEvent::Show: case QEvent::ChildPolished:
            if(qobject_cast<QMenu *>(object)) {
                observe(object);
                if(event->type()==QEvent::Show)refresh();
                else schedule();
            }
            break;
        default:break;
        }
        return false;
    }
    Dispatch m_dispatch;
    QSet<QMenu *> m_roots;
    QSet<QAction *> m_observed;
    QMap<QAction *,QMetaObject::Connection> m_connections;
    bool m_queued=false;
};
}
