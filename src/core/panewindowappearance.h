// SPDX-License-Identifier: MIT
#pragma once
#include <QColor>
namespace Unbound {
// Shared host surface; native backends only convert it to their drawing APIs.
inline constexpr int PaneWindowBackgroundLevel=20;
inline QColor paneWindowBackground()
{
    return QColor(PaneWindowBackgroundLevel,PaneWindowBackgroundLevel,PaneWindowBackgroundLevel);
}
}
