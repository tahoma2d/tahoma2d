#pragma once

#ifndef COMMANDBAR_H
#define COMMANDBAR_H

#include <memory>

#include "saveloadqsettings.h"

#include "toonz/txsheet.h"
#include "toonzqt/keyframenavigator.h"

#include <QToolBar>

//-----------------------------------------------------------------------------

// forward declaration
class QAction;

//=============================================================================
// CommandBar
//-----------------------------------------------------------------------------

class CommandBar : public QToolBar, public SaveLoadQSettings {
  Q_OBJECT
protected:
  bool m_isCollapsible;
  bool m_isQuickToolbar;
  QString m_barId;

public:
#if QT_VERSION >= 0x050500
  CommandBar(QWidget *parent = 0, Qt::WindowFlags flags = 0,
             bool isCollapsible = false, bool isQuickToolbar = false);
#else
  CommandBar(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif

  QString getBarId() { return m_barId; }

  // SaveLoadQSettings
  virtual void save(QSettings &settings,
                    bool forPopupIni = false) const override;
  virtual void load(QSettings &settings) override;

signals:
  void updateVisibility();

protected:
  static void fillToolbar(CommandBar *toolbar, bool isQuickToolbar = false,
                          QString barId = "");
  static void buildDefaultToolbar(CommandBar *toolbar);
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void doCustomizeCommandBar();
  void onCloseButtonPressed();
};

#endif  // COMMANDBAR_H
