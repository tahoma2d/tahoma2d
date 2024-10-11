#pragma once

#ifndef STYLE_DATA_INCLUDED
#define STYLE_DATA_INCLUDED

#include "toonzqt/dvmimedata.h"
#include "tpalette.h"
#include <set>
#include <vector>

class TColorStyle;

//=============================================================================
// StyleData
//-----------------------------------------------------------------------------

class StyleData final : public DvMimeData {
  std::vector<std::pair<int, TColorStyle *>> m_styles;
  StyleAnimationTable
      m_styleAnimationTable;  //!< Table of style animations (per style).

public:
  StyleData();
  ~StyleData();

  StyleData *clone() const override;

  void addStyle(int styleIndex, TColorStyle *style,
                StyleAnimation styleAnimation);  // gets ownership

  int getStyleCount() const { return (int)m_styles.size(); }
  TColorStyle *getStyle(int index) const;
  StyleAnimation getStyleAnimation(int styleId) const;
  int getStyleIndex(int index) const;
};

#endif
