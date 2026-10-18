#pragma once
#include <functional>
#include <QRect>
class QApplication;
class QLibrary;
class QWidget;
namespace Unbound { class PaneSettings; }
// Implemented by the platform's test adapter, never by production core code.
void initializePlatformTests();
void settlePlatformWindowState(QWidget &window);
std::function<unsigned long(void *)> resolveBootstrap(QLibrary &helper);
bool runPlatformDiscoveryTests(QApplication &app);
bool runPlatformWindowTests(QApplication &app);
QRect nativeWindowBounds(QWidget &window);
void prepareNativePlacementTest(QWidget &main, Unbound::PaneSettings &settings);
void verifyNativePlacementTest(QWidget &main, const QRect &expectedNative);
