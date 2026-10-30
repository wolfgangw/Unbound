// SPDX-License-Identifier: MIT
#include "widgetquirks.h"
namespace Unbound {
std::unique_ptr<QObject> attachScopesWidgetQuirks(QWidget *,QWidget *) {
    // Windows repaints retained native Scopes headers on page return itself.
    return nullptr;
}
}
