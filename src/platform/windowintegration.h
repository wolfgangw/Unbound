// SPDX-License-Identifier: MIT
#pragma once
// CMake supplies exactly one backend include directory. Backends provide:
// PaneFloatingWindow: QWidget with finishDeferredResize(), interactiveDragActive()
// and beginButtonMove()/updateButtonMove()/finishButtonMove(bool cancel).
// Button gestures retain Qt mouse delivery; the backend supplies native geometry.
// MainWindowSnap: setWindow(QWidget *), safely detached on destruction.
// WindowGroupActivation: setWindow(QWidget *), event-driven group raising.
// applyPaneDarkCaption(QWidget *, QWidget *) and inheritPaneTaskbarIdentity(...).
// All window operations are performed on the Qt GUI thread.
// User move/resize starts emit the shared WindowInteractionEvent, including
// native border paths that consume a press before Qt mouse-event translation.
#include <windowbackend.h>
