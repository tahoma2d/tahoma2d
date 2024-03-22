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

#define LINEAR_INTERPOLATION L"Linear"
#define EASE_IN_INTERPOLATION L"Ease In"
#define EASE_OUT_INTERPOLATION L"Ease Out"
#define EASE_IN_OUT_INTERPOLATION L"Ease In/Out"

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

  TEnumProperty m_frameRange;
  bool m_firstClick;
  TPointD m_firstPoint;
  std::pair<int, int> m_currCell;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_firstFrameIdx, m_lastFrameIdx;

public:
  FullColorFillTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  FillParameters getFillParameters() const;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;

  void resetMulti();
  bool onPropertyChanged(std::string propertyName) override;

  void onActivate() override;
  int getCursorId() const override;

  void draw() override;

private:
  void applyFill(const TImageP &img, const TPointD &pos, FillParameters &params,
                 bool isShiftFill, TXshSimpleLevel *sl, const TFrameId &fid,
                 TXsheet *xsheet, int frameIndex, bool fillGap, bool closeGap,
                 int closeStyleIndex, bool undoBlockStarted);

  void processSequence(const TPointD &pos, FillParameters &params,
                       int closeStyleIndex, TXshSimpleLevel *sl,
                       TFrameId firstFid, TFrameId lastFid, int multi);
  void processSequence(const TPointD &pos, FillParameters &params,
                       int closeStyleIndex, TXsheet *xsheet, int firstFidx,
                       int lastFidx, int multi);
};

#endif  // FULLCOLORFILLTOOL_H