// SPDX-License-Identifier: MIT
#pragma once
#include <QWidget>
#include <memory>
namespace Unbound {
class MacWindowInteraction;
class PaneFloatingWindow : public QWidget {
public:
    PaneFloatingWindow();
    ~PaneFloatingWindow() override;
    bool restoreGeometry(const QByteArray &bytes);
    bool beginButtonMove();
    void updateButtonMove();
    void finishButtonMove(bool cancel = false);
    bool interactiveDragActive() const;
    void beginDeferredResize();
    void finishDeferredResize(bool settle = true);
protected:
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *) override;
private:
    std::unique_ptr<MacWindowInteraction> m_interaction;
};
}
