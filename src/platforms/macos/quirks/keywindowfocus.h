// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QWidget>
#include <functional>
namespace Unbound {
// Cocoa can retain main as key while Qt clears activation as a floating host
// disappears. Synchronize only with the existing native key window.
class CocoaKeyWindowFocus : public QObject {
public:
    explicit CocoaKeyWindowFocus(std::function<bool(QWidget *)> member);
    void hostHidden();
private:
    std::function<bool(QWidget *)> m_member;
    bool m_queued=false;
};
}
