// SPDX-License-Identifier: MIT
#pragma once
#include <QString>
#include <QStandardPaths>
namespace Unbound { namespace Platform {
inline QString settingsDataDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
}
} }
