// SPDX-License-Identifier: MIT
#pragma once
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <atomic>
namespace Unbound {
// Stable C ABI: 1 queued, 2 busy, negative values are admission failures.
enum CommandResult { CommandAccepted=1, CommandBusy=2, CommandUnavailable=-1,
    CommandVersion=-2, CommandUnknown=-3, CommandArguments=-4 };
struct CommandRequest { QByteArray name; int slot=0; };
inline int parseCommand(unsigned version,const QByteArray &name,const QByteArray &arguments,CommandRequest &request) {
    if(version!=1)return CommandVersion;
    if(name!="layout.activate" && name!="layout.dockAll" && name!="layout.save" && name!="layout.saveNew")return CommandUnknown;
    QJsonParseError error;
    const auto document=QJsonDocument::fromJson(arguments,&error);
    if(error.error!=QJsonParseError::NoError || !document.isObject())return CommandArguments;
    const auto object=document.object();
    int slot=0;
    if(name=="layout.activate") {
        const auto value=object.value("slot");
        if(object.size()!=1 || !value.isDouble() || value.toDouble()<1 || value.toDouble()>5
            || value.toDouble()!=value.toInt())return CommandArguments;
        slot=value.toInt();
    } else if(!object.isEmpty())return CommandArguments;
    request={name,slot};return CommandAccepted;
}
// Admission is bounded even if Lua calls arrive from another thread.
inline std::atomic<bool> &commandQueued() {static std::atomic<bool> value{false};return value;}
inline std::atomic<bool> &commandOperationActive() {static std::atomic<bool> value{false};return value;}
inline std::atomic<unsigned long long> &commandContext() {static std::atomic<unsigned long long> value{0};return value;}
}
extern "C" Q_DECL_EXPORT int UnboundExecuteCommand(unsigned int apiVersion,const char *command,const char *argumentsJson);
