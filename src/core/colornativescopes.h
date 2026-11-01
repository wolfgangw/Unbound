// SPDX-License-Identifier: MIT
#pragma once
#include "nativescopescontent.h"
namespace Unbound {
class ColorNativeScopes final : public NativeScopesContent {
public:
 explicit ColorNativeScopes(QObject *owner):NativeScopesContent(owner){}
 QString start(QWidget *slot,QWidget *host,QWidget *page) {
  auto main=slot->window();
  // The native Scopes shell is a QWidget on macOS and a QDialog on Windows.
  // Its object identity and native controls define the contract, not that subtype.
  auto dialog=main->findChild<QWidget *>("UiWaveformWindow");
  if(dialog && !dialog->isWindow())return QStringLiteral("Another page owns the native Scopes window.");
  const bool outsideVisible=dialog && dialog->isVisible();
  if(!dialog || !dialog->isVisible()) {
   QAbstractButton *expand=nullptr;
   for(auto button:slot->findChildren<QAbstractButton *>("btnUndock")) {
    bool hidden=false;
    for(auto w=static_cast<QWidget *>(button);w && w!=slot;w=w->parentWidget())hidden=hidden || w->isHidden();
    if(!hidden && button->isEnabled()){expand=button;break;}
   }
   if(!expand)return QStringLiteral("The native Scopes expansion control is unavailable.");
   expand->click();
   dialog=main->findChild<QWidget *>("UiWaveformWindow");
  }
  if(!dialog || !dialog->isVisible() || !dialog->findChild<QAbstractButton *>("btnHide"))
   return QStringLiteral("DaVinci Resolve did not provide its native floating Scopes window.");
  const QPointer<QWidget> retainedSlot=slot,retainedHost=host,retainedPage=page;
  reacquire=[this,retainedSlot,retainedHost,retainedPage] {
   return retainedSlot && retainedHost && retainedPage && start(retainedSlot,retainedHost,retainedPage).isEmpty();
  };
  return adopt(slot,dialog,host,page,outsideVisible);
 }
};
}
