#pragma once

#ifndef FUNCTION_KEYFRAME_NAVIGATOR_INCLUDED
#define FUNCTION_KEYFRAME_NAVIGATOR_INCLUDED

#include "toonzqt/keyframenavigator.h"
#include "tdoubleparam.h"

#include <QToolBar>
class FunctionPanel;
class TFrameHandle;
class TColumnHandle;
class FrameNavigator;

class DVAPI FunctionKeyframeNavigator final : public KeyframeNavigator {
  Q_OBJECT
  TDoubleParamP m_curve;
  TXsheetHandle *m_xsheetHandle;
  TColumnHandle *m_columnHandle;
  TObjectHandle *m_objectHandle;

public:
  FunctionKeyframeNavigator(QWidget *parent);

  void setCurve(TDoubleParam *curve);

  void setXsheetHandle(TXsheetHandle *xsheetHandle) {
    m_xsheetHandle = xsheetHandle;
  }

  void setColumnHandle(TColumnHandle *columnHandle) {
    m_columnHandle = columnHandle;
  }

  void setObjectHandle(TObjectHandle *objectHandle) {
    m_objectHandle = objectHandle;
  }

protected:
  bool hasNext() const override;
  bool hasPrev() const override;
  bool hasKeyframes() const override;
  bool isKeyframe() const override;
  bool isFullKeyframe() const override { return isKeyframe(); }
  void toggle() override;
  void goNext() override;
  void goPrev() override;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

public slots:
  void onFrameSwitched() { update(); }
};

#endif
