// SPDX-License-Identifier: MIT
#include "layouttransitionpresentation.h"
namespace Unbound {
// macOS layout smoothing is intentionally unavailable until a permission-free
// presentation mechanism is validated. Pane/layout behavior remains shared.
std::unique_ptr<LayoutTransitionPresentation> makeLayoutTransitionPresentation() {
    return std::make_unique<NoLayoutTransitionPresentation>();
}
}
