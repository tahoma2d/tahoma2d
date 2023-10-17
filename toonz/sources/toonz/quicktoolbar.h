#pragma once

#ifndef QUICKTOOLBAR_H
#define QUICKTOOLBAR_H

#include <memory>

#include "toonz/txsheet.h"
#include "commandbar.h"
#include "toonzqt/keyframenavigator.h"

#include <QToolBar>

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QAction;

//-----------------------------------------------------------------------------

namespace XsheetGUI {

//=============================================================================
// Quick Toolbar
//-----------------------------------------------------------------------------

class QuickToolbar final : public CommandBar {
  Q_OBJECT

  XsheetViewer *m_viewer;

public:
  QuickToolbar(XsheetViewer *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags(),
                bool isCollapsible = false);
  static void toggleQuickToolbar();
  void showToolbar(bool show);

protected:
  void showEvent(QShowEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void doCustomizeCommandBar();
};

}  // namespace XsheetGUI

#endif  // QUICKTOOLBAR_H
