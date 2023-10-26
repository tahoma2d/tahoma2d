#include "symmetrytool.h"

#include "tgl.h"
#include "tenv.h"
#include "tsystem.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"

#include "toonz/stage.h"
#include "toonz/toonzfolders.h"

#include "../toonz/sceneviewer.h"
#include "../toonz/tscenehandle.h"
#include "../toonz/toonzscene.h"
#include "../toonz/tcamera.h"

#include <QApplication>
#include <QDir>
#include <QKeyEvent>

#include <QDebug>

TEnv::IntVar SymmetryLines("SymmetryLines", 2);
TEnv::DoubleVar SymmetryOpacity("SymmetryOpacity", 30);
TEnv::IntVar SymmetryColorIndex("SymmetryColorIndex", 0);
TEnv::IntVar SymmetryUseLineSymmetry("SymmetryUseLineSymmetry", 1);
TEnv::DoubleVar SymmetryRotation("SymmetryRotation", 0);
TEnv::IntVar SymmetryCenterPointX("SymmetryCenterPointX", 0);
TEnv::IntVar SymmetryCenterPointY("SymmetryCenterPointY", 0);
TEnv::IntVar SymmetryRotationPosX("SymmetryRotationPosX", 0);
TEnv::IntVar SymmetryRotationPosY("SymmetryRotationPosY", -40);
TEnv::StringVar SymmetryPreset("SymmetryPreset", "<custom>");

//----------------------------------------------------------------------------------------------------------

#define CUSTOM_WSTR L"<custom>"

//*******************************************************************************
//    Symmetry Data implementation
//*******************************************************************************

void SymmetryData::saveData(TOStream &os) {
  os.openChild("Name");
  os << m_name;
  os.closeChild();
  os.openChild("Lines");
  os << m_lines;
  os.closeChild();
  os.openChild("Opacity");
  os << m_opacity;
  os.closeChild();
  os.openChild("Color");
  os << m_color;
  os.closeChild();
  os.openChild("UseLineSymmetry");
  os << (m_useLineSymmetry ? 1 : 0);
  os.closeChild();
  os.openChild("CenterPoint");
  os << m_centerPoint.x << m_centerPoint.y;
  os.closeChild();
  os.openChild("Rotation");
  os << m_rotation;
  os.closeChild();
  os.openChild("RotationPos");
  os << m_rotationPos.x << m_rotationPos.y;
  os.closeChild();
}

//----------------------------------------------------------------------------------------------------------

void SymmetryData::loadData(TIStream &is) {
  std::string tagName;
  int val;
  double x, y;
  while (is.matchTag(tagName)) {
    if (tagName == "Name")
      is >> m_name, is.matchEndTag();
    else if (tagName == "Lines")
      is >> m_lines, is.matchEndTag();
    else if (tagName == "Opacity")
      is >> m_opacity, is.matchEndTag();
    else if (tagName == "Color")
      is >> m_color, is.matchEndTag();
    else if (tagName == "UseLineSymmetry")
      is >> val, m_useLineSymmetry = val, is.matchEndTag();
    else if (tagName == "CenterPoint")
      is >> x >> y, m_centerPoint = TPointD(x, y), is.matchEndTag();
    else if (tagName == "Rotation")
      is >> m_rotation, is.matchEndTag();
    else if (tagName == "RotationPos")
      is >> x >> y, m_rotationPos = TPointD(x, y), is.matchEndTag();
    else
      is.skipCurrentTag();
  }
}

//----------------------------------------------------------------------------------------------------------

PERSIST_IDENTIFIER(SymmetryData, "SymmetryData");

//*******************************************************************************
//    Symmetry Preset Manager implementation
//*******************************************************************************

void SymmetryPresetManager::load(const TFilePath &fp) {
  m_fp = fp;

  std::string tagName;
  SymmetryData data;

  TIStream is(m_fp);
  try {
    while (is.matchTag(tagName)) {
      if (tagName == "version") {
        VersionNumber version;
        is >> version.first >> version.second;

        is.setVersion(version);
        is.matchEndTag();
      } else if (tagName == "symmetryguides") {
        while (is.matchTag(tagName)) {
          if (tagName == "symmetrydata") {
            is >> data, m_presets.insert(data);
            is.matchEndTag();
          } else
            is.skipCurrentTag();
        }

        is.matchEndTag();
      } else
        is.skipCurrentTag();
    }
  } catch (...) {
  }
}

//----------------------------------------------------------------------------------------------------------

void SymmetryPresetManager::save() {
  TSystem::touchParentDir(m_fp);

  TOStream os(m_fp);

  os.openChild("version");
  os << 1 << 19;
  os.closeChild();

  os.openChild("symmetryguides");

  std::set<SymmetryData>::iterator it, end = m_presets.end();
  for (it = m_presets.begin(); it != end; ++it) {
    os.openChild("symmetrydata");
    os << (TPersist &)*it;
    os.closeChild();
  }

  os.closeChild();
}

//----------------------------------------------------------------------------------------------------------

void SymmetryPresetManager::addPreset(const SymmetryData &data) {
  m_presets.erase(data);  // Overwriting insertion
  m_presets.insert(data);
  save();
}

//----------------------------------------------------------------------------------------------------------

void SymmetryPresetManager::removePreset(const std::wstring &name) {
  m_presets.erase(SymmetryData(name));
  save();
}

//*******************************************************************************
//    Symmetry Object Undo implementation
//*******************************************************************************

SymmetryObjectUndo::SymmetryObjectUndo(SymmetryObject obj, SymmetryTool *tool)
    : m_undoData(obj), m_tool(tool) {}

//----------------------------------------------------------------------------------------------------------

void SymmetryObjectUndo::setRedoData(SymmetryObject obj) { m_redoData = obj; }

//----------------------------------------------------------------------------------------------------------

void SymmetryObjectUndo::undo() const {
  if (!m_tool) return;
  m_tool->setSymmetryObject(m_undoData);
  m_tool->invalidate();
}

//----------------------------------------------------------------------------------------------------------

void SymmetryObjectUndo::redo() const {
  if (!m_tool) return;
  m_tool->setSymmetryObject(m_redoData);
  m_tool->invalidate();
}

//*******************************************************************************
//    Symmetry Tool implementation
//*******************************************************************************

SymmetryTool::SymmetryTool()
    : TTool("T_Symmetry")
    , m_guideEnabled(false)
    , m_propertyUpdating(false)
    , m_lines("Lines:", MIN_SYMMETRY, MAX_SYMMETRY, 2)
    , m_opacity("Opacity:", 1.0, 100.0, 30.0)
    , m_color("Color:")
    , m_useLineSymmetry("Line Symmetry", true)
    , m_isShifting(false)
    , m_isCenterMoving(false)
    , m_isRotating(false)
    , m_undo(0)
    , m_presetsLoaded(false)
    , m_preset("Preset:") {
  bind(TTool::AllTargets);

  m_prop.bind(m_lines);
  m_prop.bind(m_opacity);
  m_prop.bind(m_color);
  m_prop.bind(m_useLineSymmetry);
  m_prop.bind(m_preset);

  m_color.addValue(L"Magenta", TPixel::Magenta);
  m_color.addValue(L"Red", TPixel::Red);
  m_color.addValue(L"Green", TPixel::Green);
  m_color.addValue(L"Blue", TPixel::Blue);
  m_color.addValue(L"Yellow", TPixel::Yellow);
  m_color.addValue(L"Cyan", TPixel::Cyan);
  m_color.addValue(L"Black", TPixel::Black);

  m_lines.setId("Lines");
  m_opacity.setId("Opacity");
  m_color.setId("Color");
  m_useLineSymmetry.setId("UseLineSymmetry");
  m_preset.setId("SymmetryPreset");
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::updateTranslation() {
  m_lines.setQStringName(tr("Lines:"));

  m_opacity.setQStringName(tr("Opacity"));

  m_color.setQStringName(tr("Color:"));
  m_color.setItemUIName(L"Magenta", tr("Magenta"));
  m_color.setItemUIName(L"Red", tr("Red"));
  m_color.setItemUIName(L"Green", tr("Green"));
  m_color.setItemUIName(L"Blue", tr("Blue"));
  m_color.setItemUIName(L"Yellow", tr("Yellow"));
  m_color.setItemUIName(L"Cyan", tr("Cyan"));
  m_color.setItemUIName(L"Black", tr("Black"));

  m_useLineSymmetry.setQStringName(tr("Line Symmetry"));

  m_preset.setQStringName(tr("Preset:"));
}

//----------------------------------------------------------------------------------------------

TPropertyGroup *SymmetryTool::getProperties(int idx) {
  if (!m_presetsLoaded) initPresets();
  return &m_prop;
}

void SymmetryTool::setToolOptionsBox(SymmetryToolOptionBox *toolOptionsBox) {
  m_toolOptionsBox.push_back(toolOptionsBox);

  updateMeasuredValueToolOptions();
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::setSymmetryObject(SymmetryObject obj) {
  m_symmetryObj = obj;

  m_lines.setValue(m_symmetryObj.getLines());
  m_opacity.setValue(m_symmetryObj.getOpacity());
  m_color.setColor(m_symmetryObj.getColor());
  m_useLineSymmetry.setValue(m_symmetryObj.isUsingLineSymmetry());

  m_propertyUpdating = true;
  getApplication()->getCurrentTool()->notifyToolChanged();
  m_propertyUpdating = false;

  updateMeasuredValueToolOptions();
}

//----------------------------------------------------------------------------------------------

bool SymmetryTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  if (propertyName == "Preset:") {
    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last saved brush settings
      loadLastSymmetry();

    SymmetryPreset     = m_preset.getValueAsString();
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
    return true;
  }

  if (propertyName == m_lines.getName()) {
    m_symmetryObj.setLines(m_lines.getValue());
    invalidate();
  } else if (propertyName == m_opacity.getName()) {
    m_symmetryObj.setOpacity(m_opacity.getValue());
    invalidate();
  } else if (propertyName == m_useLineSymmetry.getName()) {
    m_symmetryObj.setUseLineSymmetry(m_useLineSymmetry.getValue());
  } else if (propertyName == m_color.getName()) {
    m_symmetryObj.setColor(m_color.getColorValue());
  }

  SymmetryLines           = m_symmetryObj.getLines();
  SymmetryOpacity         = m_symmetryObj.getOpacity();
  SymmetryColorIndex      = m_color.getIndex();
  SymmetryUseLineSymmetry = m_symmetryObj.isUsingLineSymmetry();
  SymmetryRotation        = m_symmetryObj.getRotation();
  SymmetryCenterPointX    = m_symmetryObj.getCenterPoint().x;
  SymmetryCenterPointY    = m_symmetryObj.getCenterPoint().y;
  SymmetryRotationPosX    = m_symmetryObj.getRotationPos().x;
  SymmetryRotationPosY    = m_symmetryObj.getRotationPos().y;

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    SymmetryPreset     = m_preset.getValueAsString();
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

  return true;
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    m_presetsManager.load(ToonzFolder::getMyModuleDir() +
                          "symmetry_guides.txt");
  }

  // Rebuild the presets property entries
  const std::set<SymmetryData> &presets = m_presetsManager.presets();

  m_preset.deleteAllValues();
  m_preset.addValue(CUSTOM_WSTR);
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));

  std::set<SymmetryData>::const_iterator it, end = presets.end();
  for (it = presets.begin(); it != end; ++it) m_preset.addValue(it->m_name);
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::loadPreset() {
  const std::set<SymmetryData> &presets = m_presetsManager.presets();
  std::set<SymmetryData>::const_iterator it;

  it = presets.find(SymmetryData(m_preset.getValue()));
  if (it == presets.end()) return;

  const SymmetryData &preset = *it;

  try  // Don't bother with RangeErrors
  {
    m_lines.setValue(preset.m_lines);
    m_opacity.setValue(preset.m_opacity);
    m_color.setColor(preset.m_color);
    m_useLineSymmetry.setValue(preset.m_useLineSymmetry);

    m_symmetryObj.setLines(m_lines.getValue());
    m_symmetryObj.setOpacity(m_opacity.getValue());
    m_symmetryObj.setColor(m_color.getColorValue());
    m_symmetryObj.setUseLineSymmetry(m_useLineSymmetry.getValue());
    m_symmetryObj.setRotation(preset.m_rotation);
    m_symmetryObj.setCenterPoint(preset.m_centerPoint);
    m_symmetryObj.setRotationPos(preset.m_rotationPos);

    updateMeasuredValueToolOptions();
  } catch (...) {
  }
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::addPreset(QString name) {
  // Build the preset
  SymmetryData preset(name.toStdWString());

  preset.m_lines           = m_lines.getValue();
  preset.m_opacity         = m_opacity.getValue();
  preset.m_color           = m_color.getColorValue();
  preset.m_useLineSymmetry = m_useLineSymmetry.getValue();
  preset.m_centerPoint     = m_symmetryObj.getCenterPoint();
  preset.m_rotation        = m_symmetryObj.getRotation();
  preset.m_rotationPos     = m_symmetryObj.getRotationPos();

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_name);
  SymmetryPreset = m_preset.getValueAsString();
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);
  initPresets();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  onPropertyChanged(m_preset.getName());
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::loadLastSymmetry() {
  m_lines.setValue(SymmetryLines);
  m_opacity.setValue(SymmetryOpacity);
  m_color.setIndex(SymmetryColorIndex);
  m_useLineSymmetry.setValue(SymmetryUseLineSymmetry);

  m_symmetryObj.setLines(m_lines.getValue());
  m_symmetryObj.setOpacity(m_opacity.getValue());
  m_symmetryObj.setColor(m_color.getColorValue());
  m_symmetryObj.setUseLineSymmetry(m_useLineSymmetry.getValue());
  m_symmetryObj.setRotation(SymmetryRotation);
  m_symmetryObj.setCenterPoint(
      TPointD(SymmetryCenterPointX, SymmetryCenterPointY));
  m_symmetryObj.setRotationPos(
      TPointD(SymmetryRotationPosX, SymmetryRotationPosY));

  updateMeasuredValueToolOptions();
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::loadTool() {
  std::wstring wpreset =
      QString::fromStdString(SymmetryPreset.getValue()).toStdWString();
  if (wpreset != CUSTOM_WSTR) {
    initPresets();
    if (!m_preset.isValue(wpreset)) wpreset = CUSTOM_WSTR;
    m_preset.setValue(wpreset);
    SymmetryPreset = m_preset.getValueAsString();
    loadPreset();
  } else
    loadLastSymmetry();
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::draw(SceneViewer *viewer) {
  TRectD cameraRect = getApplication()
                          ->getCurrentScene()
                          ->getScene()
                          ->getCurrentCamera()
                          ->getStageRect();

  m_symmetryObj.draw(viewer, cameraRect);

  bool drawControls = getApplication()->getCurrentTool()->getTool() == this;
  if (drawControls) m_symmetryObj.drawControls(viewer);
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_isShifting     = false;
  m_isCenterMoving = false;
  m_isRotating     = false;

  m_firstPos = pos;

  m_symmetryObj.setCursorPos(pos);

  TPointD cp = m_symmetryObj.getCenterPoint();
  TPointD rp = m_symmetryObj.getRotationPos();

  m_isCenterMoving = m_symmetryObj.isOverCenterPoint();
  m_isRotating     = m_symmetryObj.isOverRotation();

  if (!m_isCenterMoving && !m_isRotating) return;

  if (e.isShiftPressed()) m_isShifting = true;

  invalidateControl();

  // Update toolbar options
  updateToolOptionValues();
}

//----------------------------------------------------------------------------------------------

TPointD calculateSymmetryCenterPoint(TPointD leftHandlePos,
                                     TPointD leftPivotPos,
                                     TPointD rightHandlePos,
                                     TPointD rightPivotPos) {
  TPointD newCenterPoint;

  double xNum = (((leftHandlePos.x * leftPivotPos.y) -
                  (leftHandlePos.y * leftPivotPos.x)) *
                 (rightHandlePos.x - rightPivotPos.x)) -
                ((leftHandlePos.x - leftPivotPos.x) *
                 ((rightHandlePos.x * rightPivotPos.y) -
                  (rightHandlePos.y * rightPivotPos.x)));
  double yNum = (((leftHandlePos.x * leftPivotPos.y) -
                  (leftHandlePos.y * leftPivotPos.x)) *
                 (rightHandlePos.y - rightPivotPos.y)) -
                ((leftHandlePos.y - leftPivotPos.y) *
                 ((rightHandlePos.x * rightPivotPos.y) -
                  (rightHandlePos.y * rightPivotPos.x)));
  double denom = ((leftHandlePos.x - leftPivotPos.x) *
                  (rightHandlePos.y - rightPivotPos.y)) -
                 ((leftHandlePos.y - leftPivotPos.y) *
                  (rightHandlePos.x - rightPivotPos.x));
  newCenterPoint.x = xNum / denom;
  newCenterPoint.y = yNum / denom;

  return newCenterPoint;
}

TPointD calculatePointOnSymmetryLine(TPointD firstPoint, double rotation,
                                     double distance) {
  TPointD refPoint;

  double theta = rotation * (3.14159 / 180);

  double yLength = std::sin(theta) * distance;
  double xLength = std::cos(theta) * distance;

  refPoint.x = firstPoint.x + xLength;
  refPoint.y = firstPoint.y + yLength;

  return refPoint;
}

void SymmetryTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  TPointD usePos = pos;

  if (!m_isCenterMoving && !m_isRotating) return;

  if (!m_undo) m_undo = new SymmetryObjectUndo(m_symmetryObj, this);

  TPointD centerPoint = m_symmetryObj.getCenterPoint();
  TPointD rotationPos = m_symmetryObj.getRotationPos();
  TPointD dPos        = usePos - m_firstPos;
  double dAngle       = 0.0;

  if (m_isRotating) {
    TPointD newRotationPos = rotationPos + dPos;

    double dox    = rotationPos.x - centerPoint.x;
    double doy    = rotationPos.y - centerPoint.y;
    double oangle = std::atan2(doy, dox) / (3.14159 / 180);
    if (oangle < 0) oangle += 360;

    double dnx    = newRotationPos.x - centerPoint.x;
    double dny    = newRotationPos.y - centerPoint.y;
    double nangle = std::atan2(dny, dnx) / (3.14159 / 180);
    if (nangle < 0) nangle += 360;

    if ((oangle >= 270 && oangle <= 360 && nangle >= 0 && nangle <= 90))
      dAngle = (360 - oangle) + nangle;
    else if ((oangle >= 0 && oangle <= 90 && nangle >= 270 && nangle <= 360))
      dAngle = (oangle + (360 - nangle)) * -1;
    else
      dAngle = nangle - oangle;
  }

  if (m_isShifting) {
    m_symmetryObj.shiftSymmetryObject(dPos);
    onPropertyChanged("CenterPoint");
  } else if (m_isCenterMoving) {  // Moving
    TPointD newCenterPoint = centerPoint + dPos;
    m_symmetryObj.setCenterPoint(newCenterPoint);

    // Recalculate Angle
    double dox    = rotationPos.x - centerPoint.x;
    double doy    = rotationPos.y - centerPoint.y;
    double oangle = std::atan2(doy, dox) / (3.14159 / 180);
    if (oangle < 0) oangle += 360;

    double dnx    = rotationPos.x - newCenterPoint.x;
    double dny    = rotationPos.y - newCenterPoint.y;
    double nangle = std::atan2(dny, dnx) / (3.14159 / 180);
    if (nangle < 0) nangle += 360;

    double dAngle;
    if ((oangle >= 270 && oangle <= 360 && nangle >= 0 && nangle <= 90))
      dAngle = (360 - oangle) + nangle;
    else if ((oangle >= 0 && oangle <= 90 && nangle >= 270 && nangle <= 360))
      dAngle = (oangle + (360 - nangle)) * -1;
    else
      dAngle = nangle - oangle;

    updateRotation(m_symmetryObj.getRotation() + dAngle);

    onPropertyChanged("CenterPoint and Rotation");

    updateMeasuredValueToolOptions();
  } else if (m_isRotating) {
    double rotation        = m_symmetryObj.getRotation();
    TPointD newRotationPos = rotationPos + dPos;
    double rot             = rotation + 270;
    TPointD rotPos         = usePos;
    double distance        = std::sqrt(std::pow((centerPoint.x - rotPos.x), 2) +
                                std::pow((centerPoint.y - rotPos.y), 2));

    if (e.isAltPressed()) {
      double ang = tfloor((int)((rot + dAngle) + 7.5), 15);
      dAngle     = ang - rot;
      newRotationPos =
          calculatePointOnSymmetryLine(centerPoint, (rot + dAngle), distance);
      usePos = dAngle == 0 ? m_firstPos : newRotationPos;
    }

    double newRotation = rotation + dAngle;
    m_symmetryObj.setRotation(newRotation);
    m_symmetryObj.setRotationPos(newRotationPos);

    onPropertyChanged("Rotation");

    updateMeasuredValueToolOptions();
  }

  m_firstPos = usePos;

  invalidate();
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (m_undo) {
    m_undo->setRedoData(m_symmetryObj);
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }
  invalidate();

  m_isShifting     = false;
  m_isCenterMoving = false;
  m_isRotating     = false;
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::invalidateControl() {
  invalidate(m_symmetryObj.getCenterPointRect());
  invalidate(m_symmetryObj.getRotationRect());
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::updateRotation(double rotation) {
  bool localUndo = false;
  if (!m_undo) {
    m_undo    = new SymmetryObjectUndo(m_symmetryObj, this);
    localUndo = true;
  }

  TPointD centerPoint    = m_symmetryObj.getCenterPoint();
  TPointD oldRotationPos = m_symmetryObj.getRotationPos();
  double oldRotation     = m_symmetryObj.getRotation();

  m_symmetryObj.setRotation(rotation);

  // Move Rotation Control
  double dx           = oldRotationPos.x - centerPoint.x;
  double dy           = oldRotationPos.y - centerPoint.y;
  double distance     = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
  double controlAngle = rotation + 270;
  double oldAngle     = oldRotation + 270;
  double deltaAngle   = controlAngle - oldAngle;

  TPointD newRotationPos =
      calculatePointOnSymmetryLine(centerPoint, controlAngle, distance);
  m_symmetryObj.setRotationPos(newRotationPos);

  if (localUndo) {
    m_undo->setRedoData(m_symmetryObj);
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }
  invalidate();
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::resetPosition() {
  TPointD centerPoint = m_symmetryObj.getCenterPoint();

  m_undo = new SymmetryObjectUndo(m_symmetryObj, this);

  m_symmetryObj.shiftSymmetryObject(TPointD(-centerPoint.x, -centerPoint.y));

  onPropertyChanged("CenterPoint");

  m_undo->setRedoData(m_symmetryObj);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;

  invalidate();
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::updateMeasuredValueToolOptions() {
  double rotation = 0;
  rotation        = m_symmetryObj.getRotation();

  for (int i = 0; i < (int)m_toolOptionsBox.size(); i++)
    m_toolOptionsBox[i]->updateMeasuredValues(rotation);
}

//----------------------------------------------------------------------------------------------

void SymmetryTool::updateToolOptionValues() {
  m_lines.setValue(m_symmetryObj.getLines());
  m_opacity.setValue(m_symmetryObj.getOpacity());
  m_color.setColor(m_symmetryObj.getColor());
  m_useLineSymmetry.setValue(m_symmetryObj.isUsingLineSymmetry());

  updateMeasuredValueToolOptions();

  m_propertyUpdating = true;
  getApplication()->getCurrentTool()->notifyToolChanged();
  m_propertyUpdating = false;
}

//----------------------------------------------------------------------------------------------

std::vector<TPointD> SymmetryTool::getSymmetryPoints(TPointD firstPoint,
                                                     TPointD rasCenter,
                                                     TPointD dpiScale) {
  SymmetryObject symmObj = getSymmetryObject();

  return getSymmetryPoints(firstPoint, rasCenter, dpiScale, symmObj.getLines(),
                           symmObj.getRotation(), symmObj.getCenterPoint(),
                           symmObj.isUsingLineSymmetry());
}

//----------------------------------------------------------------------------------------------

std::vector<TPointD> SymmetryTool::getSymmetryPoints(
    TPointD firstPoint, TPointD rasCenter, TPointD dpiScale, double lines,
    double rotation, TPointD centerPoint, bool useLineSymmetry) {
  std::vector<TPointD> points;

  double step = 360.0 / lines;
  TPointD fp  = firstPoint - rasCenter;
  fp.x *= dpiScale.x;
  fp.y *= dpiScale.y;

  double dx       = fp.x - centerPoint.x;
  double dy       = fp.y - centerPoint.y;
  double distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
  double oangle   = std::atan2(dy, dx) / (3.14159 / 180);
  if (oangle < 0.0) oangle += 360.0;
  oangle -= rotation;

  points.push_back(firstPoint);

  for (int i = 1; i < lines; i++) {
    double angle = ((double)i * step) + oangle;
    if (angle < 90.0) angle += 360.0;

    if (useLineSymmetry && (i % 2) != 0) angle += step + 180 - (oangle * 2);

    angle += rotation;

    TPointD newPoint =
        calculatePointOnSymmetryLine(centerPoint, angle, distance);

    newPoint.x /= dpiScale.x;
    newPoint.y /= dpiScale.y;
    newPoint += rasCenter;

    points.push_back(newPoint);
  }

  return points;
}

//*******************************************************************************
//    Symmetry Object implementation
//*******************************************************************************

void SymmetryObject::draw(SceneViewer *viewer, TRectD cameraRect) {
  TRectD rect = cameraRect;

  int x1, x2, y1, y2;
  viewer->rect().getCoords(&x1, &y1, &x2, &y2);
  TRect clipRect = TRect(x1, y1, x2 + 1, y2 + 1);

  GLfloat modelView[16];
  glGetFloatv(GL_MODELVIEW_MATRIX, modelView);
  TAffine modelViewAff(modelView[0], modelView[4], modelView[12], modelView[1],
                       modelView[5], modelView[13]);

  TPointD clipCorner[] = {
      modelViewAff.inv() * TPointD(clipRect.x0, clipRect.y0),
      modelViewAff.inv() * TPointD(clipRect.x1, clipRect.y0),
      modelViewAff.inv() * TPointD(clipRect.x1, clipRect.y1),
      modelViewAff.inv() * TPointD(clipRect.x0, clipRect.y1)};

  TRectD bounds;
  bounds.x0 = bounds.x1 = clipCorner[0].x;
  bounds.y0 = bounds.y1 = clipCorner[0].y;
  int i;
  for (i = 1; i < 4; i++) {
    const TPointD &p = clipCorner[i];
    if (p.x < bounds.x0)
      bounds.x0 = p.x;
    else if (p.x > bounds.x1)
      bounds.x1 = p.x;
    if (p.y < bounds.y0)
      bounds.y0 = p.y;
    else if (p.y > bounds.y1)
      bounds.y1 = p.y;
  }

  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glLineWidth(1.0f);

  TPointD p = getCenterPoint();

  TPixel32 color = getColor();
  color.m        = ((getOpacity() / 100.0) * 255);
  tglColor(color);

  TPointD end;
  double distanceToLeft   = std::abs(p.x - bounds.x0);
  double distanceToRight  = std::abs(p.x - bounds.x1);
  double distanceToTop    = std::abs(p.y - bounds.y1);
  double distanceToBottom = std::abs(p.y - bounds.y0);
  double xDistance        = std::max(distanceToLeft, distanceToRight);
  double yDistance        = std::max(distanceToTop, distanceToBottom);
  double distance         = std::max(xDistance, yDistance);
  double totalDistance =
      std::sqrt(std::pow(distance, 2) + std::pow(distance, 2));

  double step        = 360.0 / getLines();
  int rays           = 360 / step;
  if (rays < 2) rays = 2;
  step               = 360.0 / (double)rays;

  double startAngle = 270.0 + getRotation();
  double endAngle   = 90.0 + getRotation();

  double yLength, xLength;

  int raySpacing = 0;

  double lAngle = startAngle;
  double rAngle = startAngle;
  for (i = 1; i <= rays; i++) {
    bool drawRight = true;

    TPointD lStartPoint = p, rStartPoint = p, lowerCenterPoint = p,
            lowerRefPoint = p;

    // No need to draw right side if left side already drawn
    int e = endAngle * 1000;
    int l = lAngle * 1000.0;
    int r = (rAngle - 360.0) * 1000.0;
    if (i != 1) {
      if (l == e || r == e || l == r)
        drawRight = false;
      else if (l < e)
        break;
    }

    // Draw left of startAngle
    yLength = std::sin(lAngle * (3.14159 / 180)) * totalDistance;
    xLength = std::cos(lAngle * (3.14159 / 180)) * totalDistance;
    end.x   = p.x + xLength;
    end.y   = p.y + yLength;
    if (lowerCenterPoint != p)
      lStartPoint =
          calculateSymmetryCenterPoint(end, p, lowerCenterPoint, lowerRefPoint);
    tglDrawSegment(lStartPoint, end);

    if (i > 1) {
      // When overlaps, no need to draw right side if left side already drawn
      if (!drawRight) break;

      // Draw right of startAngle
      yLength = std::sin(rAngle * (3.14159 / 180)) * totalDistance;
      xLength = std::cos(rAngle * (3.14159 / 180)) * totalDistance;
      end.x   = rStartPoint.x + xLength;
      end.y   = rStartPoint.y + yLength;
      if (lowerCenterPoint != p)
        rStartPoint = calculateSymmetryCenterPoint(end, p, lowerCenterPoint,
                                                   lowerRefPoint);
      tglDrawSegment(rStartPoint, end);
    }

    rAngle += step;
    lAngle -= step;
  }

  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
}

//----------------------------------------------------------------------------------------------------------

void SymmetryObject::drawControls(SceneViewer *viewer) {
  TPointD centerPoint = m_symmetry.m_centerPoint;
  TPointD rotationPos = m_symmetry.m_rotationPos;

  m_unit = sqrt(tglGetPixelSize2()) * viewer->getDevPixRatio();

  double circleRadius = m_handleRadius * m_unit;
  double diskRadius   = (m_handleRadius - 2) * m_unit;

  glLineWidth(1.5);

  glLineStipple(1, 0xFFFF);
  glEnable(GL_LINE_STIPPLE);

  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // Draw Rotation control
  glColor3d(0.70, 0.70, 0.70);
  tglDrawSegment(centerPoint, rotationPos);
  tglDrawCircle(rotationPos, circleRadius);
  glColor3d(1.0, 1.0, 0.0);
  tglDrawDisk(rotationPos, diskRadius);
  glColor3d(0.50, 0.50, 0.50);
  tglDrawCircle(rotationPos, (m_handleRadius - 6) * m_unit);

  // Draw Center Point
  glColor3d(0.70, 0.70, 0.70);
  tglDrawCircle(centerPoint, circleRadius);
  glColor3d(0.0, 1.0, 0.0);
  tglDrawDisk(centerPoint, diskRadius);
  glColor3d(0.50, 0.50, 0.50);
  tglDrawSegment(centerPoint + m_unit * TPointD(-12, 0),
                 centerPoint + m_unit * TPointD(12, 0));
  tglDrawSegment(centerPoint + m_unit * TPointD(0, 12),
                 centerPoint + m_unit * TPointD(0, -12));

  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
  glDisable(GL_LINE_STIPPLE);
}

//----------------------------------------------------------------------------------------------------------

void SymmetryObject::shiftSymmetryObject(TPointD delta) {
  TPointD centerPoint      = m_symmetry.m_centerPoint;
  m_symmetry.m_centerPoint = TPointD(centerPoint + delta);
  m_symmetry.m_rotationPos += delta;
  SymmetryCenterPointX = m_symmetry.m_centerPoint.x;
  SymmetryCenterPointY = m_symmetry.m_centerPoint.y;
}

//----------------------------------------------------------------------------------------------------------

SymmetryTool symmetryTool;
