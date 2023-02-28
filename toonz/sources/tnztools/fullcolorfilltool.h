#pragma once

#ifndef FULLCOLORFILLTOOL_H
#define FULLCOLORFILLTOOL_H

// TnzCore includes
#include "tproperty.h"

// TnzTools includes
#include "tools/tool.h"
#include "toonz/fill.h"
#include "toonz/txshsimplelevel.h"

#include <QCoreApplication>
#include <QObject>

#define IGNOREGAPS L"Ignore Gaps"
#define FILLGAPS L"Fill Gaps"
#define CLOSEANDFILLGAPS L"Close and Fill"

class FullColorFillTool final : public QObject, public TTool {
  Q_DECLARE_TR_FUNCTIONS(FullColorFillTool)

  TXshSimpleLevelP m_level;
  TDoublePairProperty m_fillDepth;
  TPropertyGroup m_prop;
  TPointD m_clickPoint;
  TBoolProperty m_referenced;
  TDoubleProperty m_rasterGapDistance;
  TEnumProperty m_closeRasterGaps;
  TStyleIndexProperty m_closeStyleIndex;

public:
  FullColorFillTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  FillParameters getFillParameters() const;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;

  bool onPropertyChanged(std::string propertyName) override;

  void onActivate() override;
  int getCursorId() const override;

private:
  void applyFill(const TImageP &img, const TPointD &pos, FillParameters &params,
                 bool isShiftFill, TXshSimpleLevel *sl, const TFrameId &fid,
                 TXsheet *xsheet, int frameIndex, bool fillGap, bool closeGap,
                 int closeStyleIndex);
};

#endif  // FULLCOLORFILLTOOL_H