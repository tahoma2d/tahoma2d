#pragma once

#ifndef PERSPECTIVETOOL_H
#define PERSPECTIVETOOL_H

#include "tproperty.h"
#include "tpersist.h"
#include "tundo.h"
#include "historytypes.h"

#include "toonzqt/selection.h"

#include "tools/tool.h"
#include "tools/tooloptions.h"

#include <QCoreApplication>

class SceneViewer;
class PerspectiveTool;

//--------------------------------------------------------------
enum PerspectiveType { Undefined, VanishingPoint, Line };

//************************************************************************
//  Perspective Data declaration
//************************************************************************

class PerspectiveData {
public:
  PerspectiveData(PerspectiveType perspectiveType, TPointD centerPoint,
                  double rotation, int spacing, bool horizon, double opacity,
                  TPixel32 color, bool parallel)
      : m_type(perspectiveType)
      , m_centerPoint(centerPoint)
      , m_rotation(rotation)
      , m_spacing(spacing)
      , m_horizon(horizon)
      , m_opacity(opacity)
      , m_color(color)
      , m_parallel(parallel){};

  ~PerspectiveData(){};

  virtual void draw(SceneViewer *viewer, TRectD cameraRect){};

  void setType(PerspectiveType pType) { m_type = pType; }
  PerspectiveType getType() { return m_type; }

  void setCenterPoint(TPointD point) { m_centerPoint = point; }
  TPointD getCenterPoint() { return m_centerPoint; }

  void setRotation(double angle) {
    // Normalize the angle between 0 and 360
    int a                          = (int)angle % 360;
    m_rotation                     = (double)a + (angle - (int)angle);
    if (m_rotation < 0) m_rotation = 360.0 + m_rotation;
  }
  virtual double getRotation() { return m_rotation; }

  void setParallel(bool isParallel) { m_parallel = isParallel; }
  bool isParallel() { return m_parallel; }

  void setSpacing(double space) {
    m_spacing = space;
    m_spacing = std::min(m_spacing, m_maxSpacing);
    m_spacing = std::max(m_spacing, m_minSpacing);
  }
  virtual double getSpacing() { return m_spacing; }
  void setMinSpacing(int minSpace) { m_minSpacing = minSpace; }
  void setMaxSpacing(int maxSpace) { m_maxSpacing = maxSpace; }

  void setHorizon(bool isHorizon) { m_horizon = isHorizon; }
  bool isHorizon() { return m_horizon; }

  void setOpacity(double opacity) { m_opacity = opacity; }
  double getOpacity() { return m_opacity; }

  void setColor(TPixel32 color) { m_color = color; }
  TPixel32 getColor() { return m_color; }

protected:
  PerspectiveType m_type;
  TPointD m_centerPoint;
  double m_rotation;
  bool m_parallel;
  double m_spacing, m_minSpacing = 1.0, m_maxSpacing = 250.0;
  bool m_horizon;
  double m_opacity;
  TPixel32 m_color;
};

//************************************************************************
//  Perspective Controls declaration
//************************************************************************

class PerspectiveControls {
  PerspectiveData *m_perspective;
  TPointD m_cursorPos;
  bool m_active;

  double m_handleRadius = 10;
  // Common Controls
  TPointD m_rotationPos = TPointD(40, 0);
  TPointD m_spacingPos  = TPointD(0, -40);

  // Vanishing Point Controls
  TPointD m_leftPivotPos   = TPointD(-40, 0);
  TPointD m_leftHandlePos  = TPointD(-80, 0);
  TPointD m_rightPivotPos  = TPointD(40, 0);
  TPointD m_rightHandlePos = TPointD(80, 0);

public:
  PerspectiveControls(PerspectiveData *perspective)
      : m_perspective(perspective), m_active(false) {}
  ~PerspectiveControls() {}

  void setCursorPos(TPointD pos) { m_cursorPos = pos; }
  TPointD getCursorPos() { return m_cursorPos; }

  void setActive(bool active) { m_active = active; }
  bool isActive() { return m_active; }

  void setRotationPos(TPointD pos) { m_rotationPos = pos; }
  TPointD getRotationPos() { return m_rotationPos; }

  void setSpacingPos(TPointD pos) { m_spacingPos = pos; }
  TPointD getSpacingPos() { return m_spacingPos; }

  void setLeftPivotPos(TPointD pos) { m_leftPivotPos = pos; }
  TPointD getLeftPivotPos() { return m_leftPivotPos; }

  void setLeftHandlePos(TPointD pos) { m_leftHandlePos = pos; }
  TPointD getLeftHandlePos() { return m_leftHandlePos; }

  void setRightPivotPos(TPointD pos) { m_rightPivotPos = pos; }
  TPointD getRightPivotPos() { return m_rightPivotPos; }

  void setRightHandlePos(TPointD pos) { m_rightHandlePos = pos; }
  TPointD getRightHandlePos() { return m_rightHandlePos; }

  void shiftPerspectiveObject(TPointD delta);

  void drawControls();

  bool isOverCenterPoint() {
    TPointD centerPoint = m_perspective->getCenterPoint();
    return areAlmostEqual(centerPoint.x, m_cursorPos.x, m_handleRadius) &&
           areAlmostEqual(centerPoint.y, m_cursorPos.y, m_handleRadius);
  }
  bool isOverRotation() {
    return areAlmostEqual(m_rotationPos.x, m_cursorPos.x, m_handleRadius) &&
           areAlmostEqual(m_rotationPos.y, m_cursorPos.y, m_handleRadius);
  }
  bool isOverSpacing() {
    if (!m_perspective || (m_perspective->getType() == PerspectiveType::Line &&
                           !m_perspective->isParallel()))
      return false;

    return areAlmostEqual(m_spacingPos.x, m_cursorPos.x, m_handleRadius) &&
           areAlmostEqual(m_spacingPos.y, m_cursorPos.y, m_handleRadius);
  }

  bool isOverLeftPivot() {
    if (!m_perspective ||
        m_perspective->getType() != PerspectiveType::VanishingPoint)
      return false;

    return areAlmostEqual(m_leftPivotPos.x, m_cursorPos.x, m_handleRadius) &&
           areAlmostEqual(m_leftPivotPos.y, m_cursorPos.y, m_handleRadius);
  }

  bool isOverLeftHandle() {
    if (!m_perspective ||
        m_perspective->getType() != PerspectiveType::VanishingPoint)
      return false;

    return areAlmostEqual(m_leftHandlePos.x, m_cursorPos.x, m_handleRadius) &&
           areAlmostEqual(m_leftHandlePos.y, m_cursorPos.y, m_handleRadius);
  }

  bool isOverRightPivot() {
    if (!m_perspective ||
        m_perspective->getType() != PerspectiveType::VanishingPoint)
      return false;

    return areAlmostEqual(m_rightPivotPos.x, m_cursorPos.x, m_handleRadius) &&
           areAlmostEqual(m_rightPivotPos.y, m_cursorPos.y, m_handleRadius);
  }

  bool isOverRightHandle() {
    if (!m_perspective ||
        m_perspective->getType() != PerspectiveType::VanishingPoint)
      return false;

    return areAlmostEqual(m_rightHandlePos.x, m_cursorPos.x, m_handleRadius) &&
           areAlmostEqual(m_rightHandlePos.y, m_cursorPos.y, m_handleRadius);
  }

  bool isOverControl() {
    return isOverCenterPoint() | isOverRotation() | isOverSpacing() |
           isOverLeftPivot() | isOverLeftHandle() | isOverRightPivot() |
           isOverRightHandle();
  }

  TRectD getCenterPointRect() {
    TPointD centerPoint = m_perspective->getCenterPoint();
    return TRectD(
        centerPoint.x - m_handleRadius, centerPoint.y - m_handleRadius,
        centerPoint.x + m_handleRadius, centerPoint.x + m_handleRadius);
  }
  TRectD getRotationRect() {
    return TRectD(
        m_rotationPos.x - m_handleRadius, m_rotationPos.y - m_handleRadius,
        m_rotationPos.x + m_handleRadius, m_rotationPos.x + m_handleRadius);
  }
  TRectD getSpacingRect() {
    return TRectD(
        m_spacingPos.x - m_handleRadius, m_spacingPos.y - m_handleRadius,
        m_spacingPos.x + m_handleRadius, m_spacingPos.x + m_handleRadius);
  }
  TRectD getLeftPivotRect() {
    return TRectD(
        m_leftPivotPos.x - m_handleRadius, m_leftPivotPos.y - m_handleRadius,
        m_leftPivotPos.x + m_handleRadius, m_leftPivotPos.x + m_handleRadius);
  }
  TRectD getLeftHandleRect() {
    return TRectD(
        m_leftHandlePos.x - m_handleRadius, m_leftHandlePos.y - m_handleRadius,
        m_leftHandlePos.x + m_handleRadius, m_leftHandlePos.x + m_handleRadius);
  }
  TRectD getRightPivotRect() {
    return TRectD(
        m_rightPivotPos.x - m_handleRadius, m_rightPivotPos.y - m_handleRadius,
        m_rightPivotPos.x + m_handleRadius, m_rightPivotPos.x + m_handleRadius);
  }
  TRectD getRightHandleRect() {
    return TRectD(m_rightHandlePos.x - m_handleRadius,
                  m_rightHandlePos.y - m_handleRadius,
                  m_rightHandlePos.x + m_handleRadius,
                  m_rightHandlePos.x + m_handleRadius);
  }
};

//=============================================================================
// PerspectiveObject
//-----------------------------------------------------------------------------

class PerspectiveObject : public PerspectiveData, public PerspectiveControls {
public:
  PerspectiveObject(PerspectiveType perspectiveType,
                    TPointD centerPoint = TPointD(0, 0), double rotation = 0.0,
                    double spacing = 100.0, bool horizon = false,
                    double opacity = 30.0, TPixel32 color = TPixel::Magenta,
                    bool parallel = true)
      : PerspectiveData(perspectiveType, centerPoint, rotation, spacing,
                        horizon, opacity, color, parallel)
      , PerspectiveControls(this){};
  ~PerspectiveObject(){};

  virtual TPointD getReferencePoint(TPointD firstPoint) { return firstPoint; }
};

//=============================================================================
// PerspectiveObjectUndo
//-----------------------------------------------------------------------------

class PerspectiveObjectUndo final : public TUndo {
  std::vector<PerspectiveObject *> m_undoData;
  std::vector<PerspectiveObject *> m_redoData;
  PerspectiveTool *m_tool;

public:
  PerspectiveObjectUndo(std::vector<PerspectiveObject *> objs,
                        PerspectiveTool *tool);
  ~PerspectiveObjectUndo() {}

  void undo() const override;
  void redo() const override;

  void setRedoData(std::vector<PerspectiveObject *> objs);

  int getSize() const override { return sizeof(this); }

  QString getToolName() { return QString("Perspective Grid Tool"); }
  int getHistoryType() override { return HistoryType::PerspectiveGridTool; }
  QString getHistoryString() override { return getToolName(); }
};

//=============================================================================
// PerspectivePreset
//-----------------------------------------------------------------------------

struct PerspectivePreset final : public TPersist {
  PERSIST_DECLARATION(PerspectivePreset)

  std::wstring m_presetName;
  std::vector<PerspectiveObject *> m_perspectiveSet;

  PerspectivePreset() {}
  PerspectivePreset(std::wstring presetName) : m_presetName(presetName) {}
  PerspectivePreset(std::wstring presetName,
                    std::vector<PerspectiveObject *> perspectiveSet)
      : m_presetName(presetName), m_perspectiveSet(perspectiveSet) {}

  bool operator<(const PerspectivePreset &other) const {
    return m_presetName < other.m_presetName;
  }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;
};

//************************************************************************
//   Perspective Preset Manager declaration
//************************************************************************

class PerspectivePresetManager {
  TFilePath m_fp;                         //!< Presets file path
  std::set<PerspectivePreset> m_presets;  //!< Current presets container

public:
  PerspectivePresetManager() {}

  void load(const TFilePath &fp);
  void save();

  const TFilePath &path() { return m_fp; };
  const std::set<PerspectivePreset> &presets() const { return m_presets; }

  void addPreset(PerspectivePreset perspectiveSet);
  void removePreset(const std::wstring &name);
};

//=============================================================================
// PerspectiveSelection
//-----------------------------------------------------------------------------

class PerspectiveSelection final : public TSelection {
private:
  std::set<int> m_selectedObjects;

  PerspectiveTool *m_tool;

public:
  PerspectiveSelection() : m_tool(0) {}

  void enableCommands() override;

  void setTool(PerspectiveTool *tool) { m_tool = tool; }

  bool isEmpty() const override { return m_selectedObjects.empty(); }

  void selectNone() override { m_selectedObjects.clear(); }

  bool isSelected(int index) {
    return m_selectedObjects.find(index) != m_selectedObjects.end();
  }
  void select(int index) { m_selectedObjects.insert(index); }
  void unselect(int index) { m_selectedObjects.erase(index); }

  int count() { return m_selectedObjects.size(); }
  std::set<int> getSelectedObjects() { return m_selectedObjects; }
};

//=============================================================================
// VanishingPointPerspective
//-----------------------------------------------------------------------------

class VanishingPointPerspective final : public PerspectiveObject {
public:
  VanishingPointPerspective()
      : PerspectiveObject(PerspectiveType::VanishingPoint) {
    setSpacing(10);
    setParallel(false);
    setMaxSpacing(180);
    setRotationPos(TPointD(40, -20));
  };
  ~VanishingPointPerspective(){};

  void draw(SceneViewer *viewer, TRectD cameraRect) override;

  double getSpacing() override {
    if (isHorizon()) return std::min(m_spacing, 90.0);

    return m_spacing;
  }

  TPointD getReferencePoint(TPointD firstPoint) override {
    return getCenterPoint();
  }
};

//=============================================================================
// LinePerspective
//-----------------------------------------------------------------------------

class LinePerspective final : public PerspectiveObject {
public:
  LinePerspective() : PerspectiveObject(PerspectiveType::Line) {
    setSpacing(30);
  };
  ~LinePerspective(){};

  void draw(SceneViewer *viewer, TRectD cameraRect) override;

  TPointD getReferencePoint(TPointD firstPoint) override;
};

//=============================================================================
// PerspectiveTool
//-----------------------------------------------------------------------------

class PerspectiveTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(PerspectiveTool)

public:
  PerspectiveTool();

  ToolType getToolType() const override { return TTool::GenericTool; }

  void updateTranslation();

  void draw(SceneViewer *viewer) override;

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;

  bool onPropertyChanged(std::string propertyName) override;

  int getCursorId() const override { return ToolCursor::StrokeSelectCursor; };

  TPropertyGroup *getProperties(int targetType) override;

  void deleteSelectedObjects();

  void setPerspectiveObjects(std::vector<PerspectiveObject *> objs) {
    m_perspectiveObjs = objs;
    m_selection.selectNone();
    m_mainControlIndex = -1;
  }
  std::vector<PerspectiveObject *> getPerspectiveObjects() {
    return m_perspectiveObjs;
  }

  std::vector<PerspectiveObject *> copyPerspectiveSet(
      std::vector<PerspectiveObject *> perspectiveSet);

  void invalidateControl(int controlIndex);

  void onEnter() override;
  void onDeactivate() override;

  void initPresets();
  void loadPreset();
  void addPreset(QString name);
  void removePreset();
  void loadLastPreset();

protected:
  TPropertyGroup m_prop;

  TEnumProperty m_type;
  TDoubleProperty m_opacity;
  TColorChipProperty m_color;
  TBoolProperty m_horizon;
  TBoolProperty m_parallel;
  TEnumProperty m_preset;

  std::vector<PerspectiveObject *> m_perspectiveObjs;
  std::vector<PerspectiveObject *> m_lastPreset;

  PerspectiveSelection m_selection;
  bool m_selecting;
  TRectD m_selectingRect;

  bool m_modified;
  bool m_isShifting;
  bool m_isCenterMoving;
  bool m_isRotating;
  bool m_isSpacing;
  bool m_isLeftPivoting;
  bool m_isLeftMoving;
  bool m_isRightPivoting;
  bool m_isRightMoving;

  TPointD m_mousePos, m_firstPos;

  double m_totalSpacing;

  int m_mainControlIndex;

  bool m_propertyUpdating = false;

  PerspectivePresetManager m_presetsManager;
  bool m_presetsLoaded;

  PerspectiveObjectUndo *m_undo;
};

#endif