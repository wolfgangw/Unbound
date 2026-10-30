// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <memory>
class QWidget;
namespace Unbound {
// Platform compatibility work scoped to the embedded native Scopes lifetime.
// Destruction cancels deferred work; platforms needing none return nullptr.
std::unique_ptr<QObject> attachScopesWidgetQuirks(QWidget *scopes,QWidget *host);
}
