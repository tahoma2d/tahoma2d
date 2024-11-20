#pragma once

#ifndef TIPSPOPUP_H
#define TIPSPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/tabbar.h"
#include "toonzqt/gutil.h"

class QTextBrowser;
class QCheckBox;

//=============================================================================
// TipsPopup
//-----------------------------------------------------------------------------

class TipsPopup final : public DVGui::Dialog {
  Q_OBJECT

  QTextBrowser *m_tips;
  QCheckBox *m_showAtStartCB;

public:
  TipsPopup();

  void loadTips();

public slots:
  void onShowAtStartChanged(int index);
};

#endif  // TIPSPOPUP_H