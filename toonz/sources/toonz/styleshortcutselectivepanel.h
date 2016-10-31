#pragma once

#ifndef STYLE_SHORTCUT_SELECTIVE_PANEL_H
#define STYLE_SHORTCUT_SELECTIVE_PANEL_H

#include "pane.h"

//=============================================================================
// StyleShortcutSelectivePanel
//-----------------------------------------------------------------------------
// StyleShortcutSelectivePanel class is inherited by panels which can select
// the current style by 0-9 number keys if the Preferences option
// "Use Numpad and Tab keys for Switching Styles" is active.
// Currently inherited by ComboViewer, Viewer and Palette
//-----------------------------------------------------------------------------

class StyleShortcutSelectivePanel : public TPanel {
  Q_OBJECT

  QHash<QWidget *, Qt::FocusPolicy> m_childrenFocusPolicies;

public:
  StyleShortcutSelectivePanel(
      QWidget *parent = 0, Qt::WindowFlags flags = 0,
      TDockWidget::Orientation orientation = TDockWidget::vertical)
      : TPanel(parent, flags, orientation) {}

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  virtual void onPreferenceChanged(const QString &prefName);
  void updateTabFocus();
};

#endif