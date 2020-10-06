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
#if QT_VERSION >= 0x050500
  QuickToolbar(XsheetViewer *parent = 0, Qt::WindowFlags flags = 0,
                bool isCollapsible = false);
#else
  QuickToolbar(XsheetViewer *parent = 0, Qt::WFlags flags = 0);
#endif
  static void toggleQuickToolbar();
  void showToolbar(bool show);

protected:
  void showEvent(QShowEvent *e) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void doCustomizeCommandBar();
};

}  // namespace XsheetGUI;

#endif  // QUICKTOOLBAR_H
