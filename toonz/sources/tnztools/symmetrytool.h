#pragma once

#ifndef SYMMETRYTOOL_H
#define SYMMETRYTOOL_H

#include "tproperty.h"
#include "tpersist.h"
#include "tundo.h"
#include "historytypes.h"

#include "toonzqt/selection.h"

#include "tools/tool.h"
#include "tools/tooloptions.h"

#include "../toonz/menubarcommandids.h"

#include <QCoreApplication>

#define MIN_SYMMETRY 2
#define MAX_SYMMETRY 16

class SceneViewer;
class SymmetryTool;

//************************************************************************
//  Symmetry Data declaration
//************************************************************************

struct SymmetryData final : public TPersist {
  PERSIST_DECLARATION(SymmetryData)

  SymmetryData(){};

  SymmetryData(const std::wstring &name) : m_name(name){};

  SymmetryData(TPointD centerPoint, double rotation, TPointD rotationPos,
               int lines, bool useLineSymmetry, double opacity, TPixel32 color)
      : m_centerPoint(centerPoint)
      , m_rotation(rotation)
      , m_rotationPos(rotationPos)
      , m_lines(lines)
      , m_useLineSymmetry(useLineSymmetry)
      , m_opacity(opacity)
      , m_color(color) {}

  ~SymmetryData() {}

  bool operator<(const SymmetryData &other) const {
    return m_name < other.m_name;
  }

  void saveData(TOStream &os) override;
  void loadData(TIStream &is) override;

  std::wstring m_name;

  TPointD m_centerPoint  = TPointD(0, 0);
  double m_rotation      = 0.0;
  TPointD m_rotationPos  = TPointD(0, -40);
  int m_lines            = 2;
  bool m_useLineSymmetry = true;
  double m_opacity       = 30;
  TPixel32 m_color       = TPixel32::Magenta;
};

//************************************************************************
//   Symmetry Preset Manager declaration
//************************************************************************

class SymmetryPresetManager {
  TFilePath m_fp;                    //!< Presets file path
  std::set<SymmetryData> m_presets;  //!< Current presets container

public:
  SymmetryPresetManager() {}

  void load(const TFilePath &fp);
  void save();

  const TFilePath &path() { return m_fp; };
  const std::set<SymmetryData> &presets() const { return m_presets; }

  void addPreset(const SymmetryData &data);
  void removePreset(const std::wstring &name);
};

//=============================================================================
// SymmetryObject
//-----------------------------------------------------------------------------

class SymmetryObject final {
  SymmetryData m_symmetry;

  TPointD m_cursorPos;
  double m_unit = 1;

  double m_handleRadius = 10;

public:
  SymmetryObject(TPointD centerPoint = TPointD(0, 0), double rotation = 0.0,
                 TPointD rotationPos = TPointD(0, -40), int lines = 2,
                 bool symmetry = true, double opacity = 30.0,
                 TPixel32 color = TPixel::Magenta) {
    m_symmetry.m_centerPoint     = centerPoint;
    m_symmetry.m_rotation        = rotation;
    m_symmetry.m_rotationPos     = rotationPos;
    m_symmetry.m_lines           = lines;
    m_symmetry.m_useLineSymmetry = symmetry;
    m_symmetry.m_opacity         = opacity;
    m_symmetry.m_color           = color;
  };
  ~SymmetryObject(){};

  void draw(SceneViewer *viewer, TRectD cameraRect);

  TPointD getReferencePoint(TPointD firstPoint) { return firstPoint; }

  void setCenterPoint(TPointD point) { m_symmetry.m_centerPoint = point; }
  TPointD getCenterPoint() { return m_symmetry.m_centerPoint; }

  void setRotation(double angle) { m_symmetry.m_rotation = angle; }
  virtual double getRotation() { return m_symmetry.m_rotation; }

  void setRotationPos(TPointD rotationPos) {
    m_symmetry.m_rotationPos = rotationPos;
  }
  virtual TPointD getRotationPos() { return m_symmetry.m_rotationPos; }

  void setLines(int lines) { m_symmetry.m_lines = lines; }
  double getLines() { return m_symmetry.m_lines; }

  void setUseLineSymmetry(bool useLineSymmetry) {
    m_symmetry.m_useLineSymmetry = useLineSymmetry;
  }
  bool isUsingLineSymmetry() {
    if ((m_symmetry.m_lines % 2) != 0) return false;
    return m_symmetry.m_useLineSymmetry;
  }

  void setOpacity(double opacity) { m_symmetry.m_opacity = opacity; }
  double getOpacity() { return m_symmetry.m_opacity; }

  void setColor(TPixel32 color) { m_symmetry.m_color = color; }
  TPixel32 getColor() { return m_symmetry.m_color; }

  void setCursorPos(TPointD pos) { m_cursorPos = pos; }
  TPointD getCursorPos() { return m_cursorPos; }

  void shiftSymmetryObject(TPointD delta);

  void drawControls(SceneViewer *viewer);

  bool isOverCenterPoint() {
    TPointD centerPoint = m_symmetry.m_centerPoint;
    return checkOver(centerPoint, m_cursorPos);
  }
  bool isOverRotation() {
    return checkOver(m_symmetry.m_rotationPos, m_cursorPos);
  }

  bool isOverControl() { return isOverCenterPoint() | isOverRotation(); }

  TRectD getCenterPointRect() {
    TPointD centerPoint = m_symmetry.m_centerPoint;
    return getControlRect(centerPoint);
  }

  TRectD getRotationRect() { return getControlRect(m_symmetry.m_rotationPos); }

private:
  TRectD getControlRect(TPointD controlPos) {
    return TRectD(controlPos.x - m_handleRadius * m_unit,
                  controlPos.y + m_handleRadius * m_unit,
                  controlPos.x + m_handleRadius * m_unit,
                  controlPos.y - m_handleRadius * m_unit);
  }

  bool checkOver(TPointD controlPos, TPointD mousePos) {
    return areAlmostEqual(controlPos.x, mousePos.x, m_handleRadius * m_unit) &&
           areAlmostEqual(controlPos.y, mousePos.y, m_handleRadius * m_unit);
  }
};

//=============================================================================
// SymmetryObjectUndo
//-----------------------------------------------------------------------------

class SymmetryObjectUndo final : public TUndo {
  SymmetryObject m_undoData;
  SymmetryObject m_redoData;
  SymmetryTool *m_tool;

public:
  SymmetryObjectUndo(SymmetryObject obj, SymmetryTool *tool);
  ~SymmetryObjectUndo() {}

  void undo() const override;
  void redo() const override;

  void setRedoData(SymmetryObject obj);

  int getSize() const override { return sizeof(this); }

  QString getToolName() { return QString("Symmetry Tool"); }
  int getHistoryType() override { return HistoryType::SymmetryTool; }
  QString getHistoryString() override { return getToolName(); }
};

//=============================================================================
// SymmetryTool
//-----------------------------------------------------------------------------

class SymmetryTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(SymmetryTool)

public:
  SymmetryTool();
  ~SymmetryTool(){};

  ToolType getToolType() const override { return TTool::GenericTool; }

  void updateTranslation();

  void draw(SceneViewer *viewer) override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;

  bool onPropertyChanged(std::string propertyName) override;

  int getCursorId() const override { return ToolCursor::StrokeSelectCursor; };

  TPropertyGroup *getProperties(int targetType) override;

  void setToolOptionsBox(SymmetryToolOptionBox *toolOptionsBox);

  void setSymmetryObject(SymmetryObject obj);
  SymmetryObject getSymmetryObject() { return m_symmetryObj; }

  void invalidateControl();

  void updateRotation(double rotation);
  void resetPosition();

  void updateMeasuredValueToolOptions();
  void updateToolOptionValues();

  std::vector<TPointD> getSymmetryPoints(TPointD firstPoint, TPointD rasCenter,
                                         TPointD dpiScale);
  std::vector<TPointD> getSymmetryPoints(TPointD firstPoint, TPointD rasCenter,
                                         TPointD dpiScale, double lines,
                                         double rotation, TPointD centerPoint,
                                         bool useLineSymmetry);

  void setGuideEnabled(bool enabled) { m_guideEnabled = enabled; }
  void toggleGuideEnabled() { m_guideEnabled = !m_guideEnabled; }
  bool isGuideEnabled() {
    if (!CommandManager::instance()->getAction(MI_FieldGuide)->isChecked())
      return false;

    return m_guideEnabled;
  }

  void initPresets();
  void loadPreset();
  void addPreset(QString name);
  void removePreset();
  void loadLastSymmetry();

  void loadTool() override;

protected:
  TPropertyGroup m_prop;
  std::vector<SymmetryToolOptionBox *> m_toolOptionsBox;

  TIntProperty m_lines;
  TDoubleProperty m_opacity;
  TColorChipProperty m_color;
  TBoolProperty m_useLineSymmetry;
  TEnumProperty m_preset;

  SymmetryObject m_symmetryObj;

  bool m_isShifting;
  bool m_isCenterMoving;
  bool m_isRotating;

  TPointD m_mousePos, m_firstPos;

  bool m_propertyUpdating;

  SymmetryObjectUndo *m_undo;

  bool m_guideEnabled;

  bool m_presetsLoaded;

  SymmetryPresetManager
      m_presetsManager;  //!< Manager for presets of this tool instance
};

#endif