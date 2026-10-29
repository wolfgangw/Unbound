// SPDX-License-Identifier: MIT
#include "windowcontrolclearance.h"
namespace Unbound {
std::unique_ptr<QObject> reserveWindowControlClearance(QWidget *) {
    // Native caption controls do not overlap the Windows client toolbar.
    return {};
}
}
