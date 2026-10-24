// SPDX-License-Identifier: MIT
#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QRect>
class QWidget;
namespace Unbound { namespace Platform {
// Native restore returns false when the common Qt geometry fallback is needed.
// document is the native placement envelope or shared readable Qt geometry.
bool restoreWindowPlacement(QWidget *window, const QJsonObject &document, const QJsonArray &screens);
QByteArray saveWindowPlacement(QWidget *window, const QJsonArray &screens);
bool windowPlacementEligible(QWidget *window);
// Read-only preview of saved main placement, in logical contact coordinates.
QRect windowPlacementReference(QWidget *window,const QJsonObject &saved);
} }
