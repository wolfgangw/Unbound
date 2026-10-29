// SPDX-License-Identifier: MIT
#pragma once
#include <QJsonObject>
#include <QWidget>
#include "colorplaybackpause.h"
#include "nativetogglestate.h"

namespace Unbound {
// Shared home for native restoration exceptions. Presentation/backends know
// neither page controls nor pane quirks; adapters keep native ownership and
// handlers. Add an exception here only after diagnosing its native behavior.
struct LayoutRestorePolicy {
    using ToggleState=NativeToggleState;
    using PlaybackHold=ColorPlaybackPause::Hold;
    using PlaybackRecall=ColorPlaybackPause::Recall;

    static bool normalColorMode(const QString &page,const QJsonObject &panes) {
        return page==QLatin1String("WStackPage_Color")
            && !panes.value("m_pLightBox").toObject().value("floating").toBool();
    }
    static PlaybackHold pauseForRecall(QWidget *main,const QString &page,
        const QJsonObject &panes,bool outgoingClips,bool clean=false) {
        if(!main || page!=QLatin1String("WStackPage_Color"))return {};
        const auto mode=main->findChild<QAbstractButton *>("btnColorToggleLightbox");
        const bool lightbox=mode && mode->isChecked();
        const bool incomingClips=!normalColorMode(page,panes);
        const bool changesMode=clean ? (!incomingClips && lightbox)
            : (outgoingClips || incomingClips || lightbox);
        return changesMode ? ColorPlaybackPause::acquire(main->findChild<QWidget *>(page)) : PlaybackHold{};
    }
    static void presentationEnded(const QString &page,const QString &reason) {
        if(page==QLatin1String("WStackPage_Color") && reason!="revealed")
            ColorPlaybackPause::cancelActive(reason);
    }
    static bool expectedDialog(const QString &page,QWidget *dialog) {
        // Expanded palettes briefly remain top-level before their native
        // registration is retained inside the regular floating host.
        return page==QLatin1String("WStackPage_Color") && dialog
            && (dialog->objectName()==QLatin1String("UiFloatingPaletteWidgetDialog")
                || dialog->objectName()==QLatin1String("UiWaveformWindow"));
    }
};
}
