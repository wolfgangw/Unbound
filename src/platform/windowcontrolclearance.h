// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QWidget>
#include <memory>
namespace Unbound {
// Reserve native window controls overlapping a toolbar row. The platform owns
// measurement and event observation; destruction restores the original margins.
std::unique_ptr<QObject> reserveWindowControlClearance(QWidget *row);
}
