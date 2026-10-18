// SPDX-License-Identifier: MIT
#pragma once
#include <QString>
#include <QStandardPaths>
namespace Unbound { namespace Platform {
inline QString settingsDataDirectory()
{
    auto base=qEnvironmentVariable("LOCALAPPDATA");
    if(base.isEmpty()) base=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base;
}
} }
