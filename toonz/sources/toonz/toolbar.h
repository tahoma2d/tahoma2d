#pragma once

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "tools/toolcommandids.h"

#include "pane.h"
#include "saveloadqsettings.h"

#include <QToolBar>

class QToolButton;

class Toolbar final : public QToolBar, public SaveLoadQSettings {
  Q_OBJECT

  std::map<std::string, QAction *> m_toolbarList;
  QToolButton *m_expandButton;
  QAction *m_expandAction;
  bool m_isExpanded;
  int m_toolbarLevel;

  bool m_isViewerToolbar;
  TPanel *m_panel;
  bool m_isVertical = true;
  QToolButton *m_moreButton;

  struct {
    const char *toolName;
    bool isSeparator;
    bool collapsible;
    QAction *action;
  } m_buttonLayout[35] = {
      {T_Edit, false, false, 0},       {T_Selection, false, false, 0},
      {"Separator_1", true, false, 0}, {T_Brush, false, false, 0},
      {T_Geometric, false, false, 0},  {T_Type, false, true, 0},
      {T_Fill, false, false, 0},       {T_PaintBrush, false, false, 0},
      {"Separator_2", true, false, 0}, {T_Eraser, false, false, 0},
      {T_Tape, false, false, 0},  //{T_Finger, false, false, 0},
      {"Separator_3", true, false, 0}, {T_StylePicker, false, false, 0},
      {T_RGBPicker, false, false, 0},  {T_PerspectiveGrid, false, false, 0},
      {T_Symmetry, false, false, 0},   {T_Ruler, false, false, 0},
      {"Separator_4", true, false, 0}, {T_ControlPointEditor, false, false, 0},
      {T_Pinch, false, true, 0},       {T_Pump, false, true, 0},
      {T_Magnet, false, true, 0},      {T_Bender, false, true, 0},
      {T_Iron, false, true, 0},        {T_Cutter, false, true, 0},
      {"Separator_5", true, true, 0},  {T_Skeleton, false, true, 0},
      {T_Tracker, false, true, 0},     {T_Hook, false, true, 0},
      {T_Plastic, false, true, 0},     {"Separator_6", true, false, 0},
      {T_Zoom, false, false, 0},       {T_Rotate, false, true, 0},
      {T_Hand, false, false, 0},       {0, false, false, 0}};

public:
  Toolbar(QWidget *parent, bool isViewerToolbar = false);
  ~Toolbar();

  // SaveLoadQSettings
  virtual void save(QSettings &settings) const override;
  virtual void load(QSettings &settings) override;

  void updateOrientation(bool isVertical);

protected:
  bool addAction(QAction *act);

  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void onToolChanged();
  void onPreferenceChanged(const QString &prefName);
  void setIsExpanded(bool expand);
  void updateToolbar();
  void orientationToggled(bool);
};

#endif  // TOOLBAR_H
