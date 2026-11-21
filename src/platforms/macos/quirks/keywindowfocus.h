// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QWidget>
#include <functional>
namespace Unbound {
// Cocoa can retain a group window as key while Qt clears activation as a
// floating host or Cinema Viewer disappears. Follow the existing native key.
class CocoaKeyWindowFocus : public QObject {
public:
    explicit CocoaKeyWindowFocus(std::function<bool(QWidget *)> member);
    void hostHidden(bool restoreResponder=false);
private:
    std::function<bool(QWidget *)> m_member;
    bool m_queued=false;
    bool m_restoreResponder=false;
};
}
