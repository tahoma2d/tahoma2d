#include "perspectivetool.h"

#include "tgl.h"
#include "tenv.h"
#include "tsystem.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"

#include "toonz/stage.h"
#include "toonz/toonzfolders.h"
#include "toonz/tproject.h"

#include "toonzqt/selectioncommandids.h"

#include "../toonz/sceneviewer.h"
#include "../toonz/tscenehandle.h"
#include "../toonz/toonzscene.h"
#include "../toonz/tcamera.h"

#include <QApplication>
#include <QDir>
#include <QKeyEvent>

#include <QDebug>

TEnv::IntVar PerspectiveToolAdvancedControls("PerspectiveToolAdvancedControls",
                                             0);

PerspectiveTool perspectiveTool;

//----------------------------------------------------------------------------------------------------------

#define CUSTOM_WSTR L"<custom>"

//----------------------------------------------------------------------------------------------------------

void PerspectivePreset::saveData(TOStream &os) {
  os.openChild("version");
  os << 1 << 0;
  os.closeChild();

  os.openChild("Name");
  os << m_presetName;
  os.closeChild();

  std::vector<PerspectiveObject *>::iterator it;
  for (it = m_perspectiveSet.begin(); it != m_perspectiveSet.end(); it++) {
    PerspectiveObject *perspective = *it;
    os.openChild("PerspectiveObject");
    os.openChild("Type");
    os << perspective->getType();
    os.closeChild();
    os.openChild("CenterPoint");
    os << perspective->getCenterPoint().x << perspective->getCenterPoint().y;
    os.closeChild();
    os.openChild("RotationPos");
    os << perspective->getRotationPos().x << perspective->getRotationPos().y;
    os.closeChild();
    os.openChild("SpacingPos");
    os << perspective->getSpacingPos().x << perspective->getSpacingPos().y;
    os.closeChild();
    os.openChild("LeftPivotPos");
    os << perspective->getLeftPivotPos().x << perspective->getLeftPivotPos().y;
    os.closeChild();
    os.openChild("LeftHandlePos");
    os << perspective->getLeftHandlePos().x
       << perspective->getLeftHandlePos().y;
    os.closeChild();
    os.openChild("RightPivotPos");
    os << perspective->getRightPivotPos().x
       << perspective->getRightPivotPos().y;
    os.closeChild();
    os.openChild("RightHandlePos");
    os << perspective->getRightHandlePos().x
       << perspective->getRightHandlePos().y;
    os.closeChild();
    os.openChild("Rotation");
    os << perspective->getRotation();
    os.closeChild();
    os.openChild("Spacing");
    os << perspective->getSpacing();
    os.closeChild();
    os.openChild("Horizon");
    os << (int)perspective->isHorizon();
    os.closeChild();
    os.openChild("Opacity");
    os << perspective->getOpacity();
    os.closeChild();
    os.openChild("Color");
    os << perspective->getColor();
    os.closeChild();
    os.openChild("Parallel");
    os << (int)perspective->isParallel();
    os.closeChild();
    os.closeChild();
  }
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePreset::loadData(TIStream &is) {
  std::string tagName;

  VersionNumber version;

  while (is.matchTag(tagName)) {
    if (tagName == "version") {
      is >> version.first >> version.second;
      is.setVersion(version);
      is.matchEndTag();
    } else if (tagName == "Name")
      is >> m_presetName, is.matchEndTag();
    else if (tagName == "PerspectiveObject") {
      PerspectiveObject *newObject;
      while (is.matchTag(tagName)) {
        if (tagName == "Type") {
          int objType;
          is >> objType, is.matchEndTag();
          switch (objType) {
          case 1:
            newObject = new VanishingPointPerspective();
            break;
          case 2:
            newObject = new LinePerspective();
            break;
          default:
            break;
          }
        } else if (tagName == "CenterPoint") {
          double x, y;
          is >> x >> y, is.matchEndTag();
          ;
          newObject->setCenterPoint(TPointD(x, y));
        } else if (tagName == "RotationPos") {
          double x, y;
          is >> x >> y, is.matchEndTag();
          ;
          newObject->setRotationPos(TPointD(x, y));
        } else if (tagName == "SpacingPos") {
          double x, y;
          is >> x >> y, is.matchEndTag();
          ;
          newObject->setSpacingPos(TPointD(x, y));
        } else if (tagName == "LeftPivotPos") {
          double x, y;
          is >> x >> y, is.matchEndTag();
          ;
          newObject->setLeftPivotPos(TPointD(x, y));
        } else if (tagName == "LeftHandlePos") {
          double x, y;
          is >> x >> y, is.matchEndTag();
          ;
          newObject->setLeftHandlePos(TPointD(x, y));
        } else if (tagName == "RightPivotPos") {
          double x, y;
          is >> x >> y, is.matchEndTag();
          ;
          newObject->setRightPivotPos(TPointD(x, y));
        } else if (tagName == "RightHandlePos") {
          double x, y;
          is >> x >> y, is.matchEndTag();
          ;
          newObject->setRightHandlePos(TPointD(x, y));
        } else if (tagName == "Rotation") {
          double rotation;
          is >> rotation, is.matchEndTag();
          ;
          newObject->setRotation(rotation);
        } else if (tagName == "Spacing") {
          double spacing;
          is >> spacing, is.matchEndTag();
          ;
          newObject->setSpacing(spacing);
        } else if (tagName == "Horizon") {
          int isHorizon;
          is >> isHorizon, is.matchEndTag();
          ;
          newObject->setHorizon(isHorizon);
        } else if (tagName == "Opacity") {
          double opacity;
          is >> opacity, is.matchEndTag();
          ;
          newObject->setOpacity(opacity);
        } else if (tagName == "Color") {
          TPixel32 color;
          is >> color, is.matchEndTag();
          ;
          newObject->setColor(color);
        } else if (tagName == "Parallel") {
          int isParallel;
          is >> isParallel, is.matchEndTag();
          ;
          newObject->setParallel(isParallel);
        } else
          is.skipCurrentTag();
      }
      m_perspectiveSet.push_back(newObject);
      is.matchEndTag();
    } else
      is.skipCurrentTag();
  }
}

PERSIST_IDENTIFIER(PerspectivePreset, "PerspectivePreset");

//----------------------------------------------------------------------------------------------------------

PerspectiveObjectUndo::PerspectiveObjectUndo(
    std::vector<PerspectiveObject *> objs, PerspectiveTool *tool)
    : m_tool(tool) {
  if (!m_tool) return;
  m_undoData = m_tool->copyPerspectiveSet(objs, false);
}

//----------------------------------------------------------------------------------------------------------

void PerspectiveObjectUndo::setRedoData(std::vector<PerspectiveObject *> objs) {
  if (!m_tool) return;
  m_redoData = m_tool->copyPerspectiveSet(objs, false);
}

//----------------------------------------------------------------------------------------------------------

void PerspectiveObjectUndo::undo() const {
  if (!m_tool) return;
  m_tool->setPerspectiveObjects(m_undoData);
  m_tool->invalidate();
}

//----------------------------------------------------------------------------------------------------------

void PerspectiveObjectUndo::redo() const {
  if (!m_tool) return;
  m_tool->setPerspectiveObjects(m_redoData);
  m_tool->invalidate();
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePresetManager::addPreset(PerspectivePreset perspectiveSet) {
  removePreset(perspectiveSet.m_presetName);
  m_presets.insert(perspectiveSet);
  savePreset(perspectiveSet.m_presetName);
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePresetManager::removePreset(const std::wstring &name) {
  std::set<PerspectivePreset>::iterator it;
  for (it = m_presets.begin(); it != m_presets.end(); it++) {
    PerspectivePreset preset = *it;
    if (preset.m_presetName != name) continue;
    deletePreset(preset.m_path);
    m_presets.erase(preset);
    break;
  }
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePresetManager::loadPresets(const TFilePath &presetFolder) {
  TFileStatus fs(presetFolder);

  if (!fs.doesExist() || !fs.isDirectory()) return;

  TFilePathSet fileSet;
  fileSet.clear();

  QDir presetfp(presetFolder.getQString());
  presetfp.setNameFilters(QStringList("*.grid"));
  TSystem::readDirectory(fileSet, presetfp, false);

  if (fileSet.size() == 0) return;

  TFilePathSet::iterator it;
  for (it = fileSet.begin(); it != fileSet.end(); it++) {
    std::string tagName;
    PerspectivePreset data;
    TIStream is(*it);
    try {
      is >> data;
      data.m_path       = TFilePath(it->getQString());
      data.m_presetName = data.m_path.getWideName();
      m_presets.insert(data);
    } catch (...) {
    }
  }
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePresetManager::savePreset(std::wstring presetName) {
  std::set<PerspectivePreset>::iterator it, end = m_presets.end();
  for (it = m_presets.begin(); it != end; ++it) {
    if (it->m_presetName != presetName) continue;
    TFilePath fp = it->m_path;
    if (fp == TFilePath()) return;

    TSystem::touchParentDir(fp);

    TOStream os(fp);
    os << (TPersist &)*it;
    break;
  }
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePresetManager::deletePreset(TFilePath presetPath) {
  if (presetPath == TFilePath()) return;

  TFileStatus fs(presetPath);
  if (!fs.doesExist()) return;

  TSystem::deleteFile(presetPath);
}

//----------------------------------------------------------------------------------------------------------

void PerspectiveControls::drawControls() {
  if (!m_perspective) return;

  TPointD centerPoint = m_perspective->getCenterPoint();

  m_unit = sqrt(tglGetPixelSize2()) * getDevPixRatio();

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
  tglDrawSegment(centerPoint, m_rotationPos);
  tglDrawCircle(m_rotationPos, circleRadius);
  if (m_active) glColor3d(1.0, 1.0, 0.0);
  tglDrawDisk(m_rotationPos, diskRadius);
  glColor3d(0.50, 0.50, 0.50);
  tglDrawCircle(m_rotationPos, (m_handleRadius - 6) * m_unit);

  // Draw Spacing control
  if (m_perspective->getType() != PerspectiveType::Line ||
      m_perspective->isParallel()) {
    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(centerPoint, m_spacingPos);
    tglDrawCircle(m_spacingPos, circleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_spacingPos, diskRadius);
    glColor3d(0.50, 0.50, 0.50);
    tglDrawSegment(m_spacingPos + m_unit * TPointD(-4, 4),
                   m_spacingPos + m_unit * TPointD(-4, -4));
    tglDrawSegment(m_spacingPos + m_unit * TPointD(0, 4),
                   m_spacingPos + m_unit * TPointD(0, -4));
    tglDrawSegment(m_spacingPos + m_unit * TPointD(4, 4),
                   m_spacingPos + m_unit * TPointD(4, -4));
  }

  if (m_perspective->getType() == PerspectiveType::VanishingPoint &&
      m_showAdvanced) {
    // Draw Left Handle
    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(m_leftPivotPos, m_leftHandlePos);
    tglDrawCircle(m_leftHandlePos, circleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_leftHandlePos, diskRadius);
    TPointD p1, p2, p3;
    double dx    = m_leftPivotPos.x - m_leftHandlePos.x;
    double dy    = m_leftPivotPos.y - m_leftHandlePos.y;
    double slope = dy / dx;
    double angle = std::isnan(slope) ? 0 : std::atan(slope);
    if (m_leftHandlePos.x > centerPoint.x) angle += (180 * (3.14159 / 180));
    p1.y = std::sin(angle) * 5;
    p1.x = std::cos(angle) * 5;
    angle += (120 * (3.14159 / 180));
    p2.y = std::sin(angle) * 5;
    p2.x = std::cos(angle) * 5;
    angle += (120 * (3.14159 / 180));
    p3.y = std::sin(angle) * 5;
    p3.x = std::cos(angle) * 5;
    glColor3d(0.50, 0.50, 0.50);
    tglDrawSegment(m_leftHandlePos + p1 * m_unit,
                   m_leftHandlePos + p2 * m_unit);
    tglDrawSegment(m_leftHandlePos + p2 * m_unit,
                   m_leftHandlePos + p3 * m_unit);
    tglDrawSegment(m_leftHandlePos + p3 * m_unit,
                   m_leftHandlePos + p1 * m_unit);

    // Draw Right Handle
    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(m_rightPivotPos, m_rightHandlePos);
    tglDrawCircle(m_rightHandlePos, circleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_rightHandlePos, diskRadius);
    dx    = m_rightPivotPos.x - m_rightHandlePos.x;
    dy    = m_rightPivotPos.y - m_rightHandlePos.y;
    slope = dy / dx;
    angle = std::isnan(slope) ? 0 : std::atan(slope);
    if (m_rightHandlePos.x > centerPoint.x) angle += (180 * (3.14159 / 180));
    p1.y = std::sin(angle) * 5;
    p1.x = std::cos(angle) * 5;
    angle += (120 * (3.14159 / 180));
    p2.y = std::sin(angle) * 5;
    p2.x = std::cos(angle) * 5;
    angle += (120 * (3.14159 / 180));
    p3.y = std::sin(angle) * 5;
    p3.x = std::cos(angle) * 5;
    glColor3d(0.50, 0.50, 0.50);
    tglDrawSegment(m_rightHandlePos + p1 * m_unit,
                   m_rightHandlePos + p2 * m_unit);
    tglDrawSegment(m_rightHandlePos + p2 * m_unit,
                   m_rightHandlePos + p3 * m_unit);
    tglDrawSegment(m_rightHandlePos + p3 * m_unit,
                   m_rightHandlePos + p1 * m_unit);

    // Draw Left Pivot
    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(centerPoint, m_leftPivotPos);
    tglDrawCircle(m_leftPivotPos, circleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_leftPivotPos, diskRadius);
    glColor3d(0.50, 0.50, 0.50);
    tglDrawDisk(m_leftPivotPos, 2.0 * m_unit);

    // Draw Right Pivot
    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(centerPoint, m_rightPivotPos);
    tglDrawCircle(m_rightPivotPos, circleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_rightPivotPos, diskRadius);
    glColor3d(0.50, 0.50, 0.50);
    tglDrawDisk(m_rightPivotPos, 2.0 * m_unit);
  }

  // Draw Center Point
  glColor3d(0.70, 0.70, 0.70);
  tglDrawCircle(centerPoint, circleRadius);
  if (m_active) glColor3d(0.0, 1.0, 0.0);
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

void PerspectiveControls::shiftPerspectiveObject(TPointD delta) {
  TPointD centerPoint = m_perspective->getCenterPoint();
  m_perspective->setCenterPoint(centerPoint + delta);
  m_rotationPos += delta;
  m_spacingPos += delta;
  m_leftPivotPos += delta;
  m_leftHandlePos += delta;
  m_rightPivotPos += delta;
  m_rightHandlePos += delta;
}

//----------------------------------------------------------------------------------------------

void PerspectiveSelection::enableCommands() {
  enableCommand(m_tool, MI_Clear, &PerspectiveTool::deleteSelectedObjects);
  enableCommand(m_tool, MI_Cut, &PerspectiveTool::deleteSelectedObjects);
  enableCommand(m_tool, MI_SelectAll, &PerspectiveTool::selectAllObjects);
}

//----------------------------------------------------------------------------------------------

PerspectiveTool::PerspectiveTool()
    : TTool("T_PerspectiveGrid")
    , m_type("Type:")
    , m_opacity("Opacity:", 1.0, 100.0, 30.0)
    , m_color("Color:")
    , m_horizon("Horizon", false)
    , m_parallel("Parallel", false)
    , m_advancedControls("Advanced Controls", false)
    , m_preset("Preset:")
    , m_modified(false)
    , m_isShifting(false)
    , m_isCenterMoving(false)
    , m_isRotating(false)
    , m_isSpacing(false)
    , m_isLeftPivoting(false)
    , m_isLeftMoving(false)
    , m_isRightPivoting(false)
    , m_isRightMoving(false)
    , m_selecting(false)
    , m_selectingRect(TRectD())
    , m_undo(0) {
  bind(TTool::AllTargets);

  m_prop.bind(m_type);
  m_prop.bind(m_opacity);
  m_prop.bind(m_color);
  m_prop.bind(m_horizon);
  m_prop.bind(m_parallel);
  m_prop.bind(m_advancedControls);
  m_prop.bind(m_preset);

  m_type.addValue(L"Vanishing Point");
  m_type.addValue(L"Line");
  m_type.setId("PerspectiveType");

  m_color.addValue(L"Magenta", TPixel::Magenta);
  m_color.addValue(L"Red", TPixel::Red);
  m_color.addValue(L"Green", TPixel::Green);
  m_color.addValue(L"Blue", TPixel::Blue);
  m_color.addValue(L"Yellow", TPixel::Yellow);
  m_color.addValue(L"Cyan", TPixel::Cyan);
  m_color.addValue(L"Black", TPixel::Black);
  m_color.setId("Color");

  m_advancedControls.setValue(PerspectiveToolAdvancedControls);

  m_preset.setId("PerspectivePreset");
  m_preset.addValue(CUSTOM_WSTR);

  m_selection.setTool(this);
  m_selection.selectNone();

  TProjectManager::instance()->addListener(this);
}

//----------------------------------------------------------------------------------------------

PerspectiveTool::~PerspectiveTool() {
  TProjectManager::instance()->removeListener(this);
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::onProjectSwitched() {
  m_presetsManager.clearPresets();
  initPresets();
  for (int i = 0; i < (int)m_toolOptionsBox.size(); i++)
    m_toolOptionsBox[i]->reloadPresetCombo();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::onProjectChanged() {
  m_presetsManager.clearPresets();
  initPresets();
  for (int i = 0; i < (int)m_toolOptionsBox.size(); i++)
    m_toolOptionsBox[i]->reloadPresetCombo();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::updateTranslation() {
  m_type.setQStringName(tr("Type:"));
  m_type.setItemUIName(L"Vanishing Point", tr("Vanishing Point"));
  m_type.setItemUIName(L"Line", tr("Line"));

  m_color.setQStringName(tr("Color:"));
  m_color.setItemUIName(L"Magenta", tr("Magenta"));
  m_color.setItemUIName(L"Red", tr("Red"));
  m_color.setItemUIName(L"Green", tr("Green"));
  m_color.setItemUIName(L"Blue", tr("Blue"));
  m_color.setItemUIName(L"Yellow", tr("Yellow"));
  m_color.setItemUIName(L"Cyan", tr("Cyan"));
  m_color.setItemUIName(L"Black", tr("Black"));

  m_opacity.setQStringName(tr("Opacity:"));
  m_horizon.setQStringName(tr("Horizon"));
  m_parallel.setQStringName(tr("Parallel"));
  m_advancedControls.setQStringName(tr("Advanced Controls"));

  m_preset.setQStringName(tr("Preset:"));
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));
}

//----------------------------------------------------------------------------------------------

TPropertyGroup *PerspectiveTool::getProperties(int idx) {
  initPresets();

  return &m_prop;
}

void PerspectiveTool::setToolOptionsBox(
    PerspectiveGridToolOptionBox *toolOptionsBox) {
  m_toolOptionsBox.push_back(toolOptionsBox);
}

//----------------------------------------------------------------------------------------------

bool PerspectiveTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;

  if (propertyName == m_preset.getName()) {
    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setActive(false);
    m_selection.selectNone();

    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last preset
      loadLastPreset();

    updateToolOptionValues();
    return true;
  }

  if (propertyName == m_advancedControls.getName()) {
    PerspectiveToolAdvancedControls = m_advancedControls.getValue();

    for (int i = 0; i < m_perspectiveObjs.size(); i++)
      m_perspectiveObjs[i]->setShowAdvancedControls(
          PerspectiveToolAdvancedControls);
    return true;
  }

  if (propertyName != m_type.getName() && selectedObjects.size() &&
      m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);

    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setActive(false);

    loadLastPreset();

    updateToolOptionValues();
  }

  if (selectedObjects.size() && !m_undo)
    m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  if (propertyName == m_opacity.getName()) {
    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setOpacity(m_opacity.getValue());
    invalidate();
  } else if (propertyName == m_horizon.getName()) {
    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setHorizon(m_horizon.getValue());
  } else if (propertyName == m_color.getName()) {
    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setColor(m_color.getColorValue());
  } else if (propertyName == m_parallel.getName()) {
    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setParallel(m_parallel.getValue());
  }

  if (m_undo) {
    m_undo->setRedoData(m_perspectiveObjs);
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }
  return true;
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::draw(SceneViewer *viewer) {
  if (!m_perspectiveObjs.size()) return;

  TRectD cameraRect = getApplication()
                          ->getCurrentScene()
                          ->getScene()
                          ->getCurrentCamera()
                          ->getStageRect();

  for (int i = 0; i < m_perspectiveObjs.size(); i++) {
    if (!m_perspectiveObjs[i]) continue;
    m_perspectiveObjs[i]->draw(viewer, cameraRect);
  }

  bool drawControls = getApplication()->getCurrentTool()->getTool() == this;
  if (drawControls) {
    for (int i = 0; i < m_perspectiveObjs.size(); i++)
      m_perspectiveObjs[i]->drawControls();
  }

  if (m_selecting) {
    TPixel color = TPixel32::Red;
    ToolUtils::drawRect(m_selectingRect, color, 0xFFFF, true);
  }
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_modified        = false;
  m_isShifting      = false;
  m_isCenterMoving  = false;
  m_isRotating      = false;
  m_isSpacing       = false;
  m_isLeftPivoting  = false;
  m_isLeftMoving    = false;
  m_isRightPivoting = false;
  m_isRightMoving   = false;
  m_selecting       = false;

  m_totalChange = 0;

  m_firstPos = pos;

  int controlIdx = -1;

  for (int i = 0; i < m_perspectiveObjs.size(); i++) {
    m_perspectiveObjs[i]->setCursorPos(pos);
    if (m_perspectiveObjs[i]->isOverControl()) {
      controlIdx = i;
      break;
    }
  }

  if (controlIdx >= 0) {
    m_isCenterMoving  = m_perspectiveObjs[controlIdx]->isOverCenterPoint();
    m_isRotating      = m_perspectiveObjs[controlIdx]->isOverRotation();
    m_isSpacing       = m_perspectiveObjs[controlIdx]->isOverSpacing();
    m_isLeftPivoting  = m_perspectiveObjs[controlIdx]->isOverLeftPivot();
    m_isLeftMoving    = m_perspectiveObjs[controlIdx]->isOverLeftHandle();
    m_isRightPivoting = m_perspectiveObjs[controlIdx]->isOverRightPivot();
    m_isRightMoving   = m_perspectiveObjs[controlIdx]->isOverRightHandle();

    // Hit a control while pressing CTRL: add/remove from selection
    if (e.isCtrlPressed()) {
      if (!m_selection.isSelected(controlIdx)) {
        m_selection.select(controlIdx);
        m_perspectiveObjs[controlIdx]->setActive(true);
        m_mainControlIndex = controlIdx;
      } else {
        m_selection.unselect(controlIdx);
        m_perspectiveObjs[controlIdx]->setActive(false);
        m_isCenterMoving = m_isRotating = m_isSpacing = m_isLeftPivoting =
            m_isLeftMoving = m_isRightPivoting = m_isRightMoving = false;
        m_mainControlIndex                                       = -1;
      }
      m_selection.makeCurrent();
      invalidateControl(controlIdx);
      m_modified = true;
      updateToolOptionValues();
      return;
    }

    m_mainControlIndex = controlIdx;

    if (e.isShiftPressed()) m_isShifting = true;

    // Hit a control while not pressing Ctrl and is already active: Do nothing
    if (m_perspectiveObjs[controlIdx]->isActive()) {
      updateToolOptionValues();
      return;
    }
    // Hit a control while not pressing Ctrl: Clear selection and make current
    // active
    for (int i = 0; i < m_perspectiveObjs.size(); i++)
      if (m_perspectiveObjs[i]->isActive()) {
        m_perspectiveObjs[i]->setActive(false);
        invalidateControl(i);
      }
    m_selection.selectNone();

    m_selection.select(controlIdx);
    m_perspectiveObjs[controlIdx]->setActive(true);
    m_selection.makeCurrent();
    invalidateControl(controlIdx);

    // Update toolbar options
    updateToolOptionValues();

    return;
  } else {
    // Hit empty space
    if (e.isCtrlPressed()) {
      m_selecting        = true;  // Assume we're area selecting
      m_selectingRect.x0 = pos.x;
      m_selectingRect.y0 = pos.y;
      m_selectingRect.x1 = pos.x + 1;
      m_selectingRect.y1 = pos.y + 1;
      return;
    } else if (e.isShiftPressed())  // Do nothing
      return;

    // Hit empty space while not pressing CTRL...
    // ... If we have only 1 selected: Clear current selection, add new
    // ... If more than 1: Clear selections and do nothing
    int wasSelected = m_selection.count();
    for (int i = 0; i < m_perspectiveObjs.size(); i++)
      if (m_perspectiveObjs[i]->isActive()) {
        m_perspectiveObjs[i]->setActive(false);
        invalidateControl(i);
      }
    m_selection.selectNone();
    if (wasSelected > 1) {
      m_mainControlIndex = -1;
      updateToolOptionValues();
      return;
    }
  }

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);
    loadLastPreset();

    updateToolOptionValues();
  }

  m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  PerspectiveObject *newObject;

  switch (m_type.getIndex()) {
  case 0:
    newObject = new VanishingPointPerspective();
    break;
  case 1:
    newObject = new LinePerspective();
    break;
  default:
    return;
  }
  newObject->setCenterPoint(pos);
  newObject->setRotationPos(pos + newObject->getRotationPos());
  newObject->setSpacingPos(pos + newObject->getSpacingPos());
  newObject->setLeftPivotPos(pos + newObject->getLeftPivotPos());
  newObject->setLeftHandlePos(pos + newObject->getLeftHandlePos());
  newObject->setRightPivotPos(pos + newObject->getRightPivotPos());
  newObject->setRightHandlePos(pos + newObject->getRightHandlePos());
  newObject->setOpacity(m_opacity.getValue());
  newObject->setColor(m_color.getColorValue());
  newObject->setHorizon(m_horizon.getValue());
  newObject->setParallel(m_parallel.getValue());
  newObject->setShowAdvancedControls(PerspectiveToolAdvancedControls);
  newObject->setActive(true);

  m_perspectiveObjs.push_back(newObject);
  m_lastPreset.push_back(newObject);

  m_mainControlIndex = m_perspectiveObjs.size() - 1;

  m_selection.select(m_mainControlIndex);
  m_selection.makeCurrent();

  m_isShifting = true;  // Allow click and shift
  m_modified   = true;

  updateToolOptionValues();

  invalidate();
}

//----------------------------------------------------------------------------------------------

TPointD calculateHandlePos(TPointD handlePos, TPointD pivotPos,
                           TPointD pivotMove, TPointD centerPoint) {
  TPointD newHandlePos;

  TPointD newPivotPos = pivotPos + pivotMove;

  double dOrigPivotHandle = std::sqrt(std::pow((pivotPos.x - handlePos.x), 2) +
                                      std::pow((pivotPos.y - handlePos.y), 2));
  double dCenterNewPivot =
      std::sqrt(std::pow((centerPoint.x - newPivotPos.x), 2) +
                std::pow((centerPoint.y - newPivotPos.y), 2));
  double dNewCenterHandle = dOrigPivotHandle + dCenterNewPivot;
  double dratio           = dCenterNewPivot / dNewCenterHandle;
  newHandlePos.x = (newPivotPos.x - ((1 - dratio) * centerPoint.x)) / dratio;
  newHandlePos.y = (newPivotPos.y - ((1 - dratio) * centerPoint.y)) / dratio;

  return newHandlePos;
}

TPointD calculateCenterPoint(TPointD leftHandlePos, TPointD leftPivotPos,
                             TPointD rightHandlePos, TPointD rightPivotPos) {
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

TPointD calculatePointOnLine(TPointD firstPoint, double rotation,
                             double distance) {
  TPointD refPoint;

  double theta = rotation * (3.14159 / 180);

  double yLength = std::sin(theta) * distance;
  double xLength = std::cos(theta) * distance;

  refPoint.x = firstPoint.x + xLength;
  refPoint.y = firstPoint.y + yLength;

  return refPoint;
}

double calculateSpacing(PerspectiveType perspectiveType, TPointD spacingPos,
                        TPointD rotationPos, double rotation,
                        TPointD centerPoint) {
  double newSpace;

  double dx       = spacingPos.x - centerPoint.x;
  double dy       = spacingPos.y - centerPoint.y;
  double distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
  double angle    = std::atan2(dy, dx) / (3.14159 / 180);

  if (perspectiveType == PerspectiveType::VanishingPoint) {
    if (angle < 0) angle += 360;

    double drx    = rotationPos.x - centerPoint.x;
    double dry    = rotationPos.y - centerPoint.y;
    double rangle = std::atan2(dry, drx) / (3.14159 / 180);
    if (rangle < 0) rangle += 360;

    newSpace                     = std::abs(rangle - angle);
    if (newSpace > 180) newSpace = 360 - newSpace;
  } else {  // Assumed Line
    angle -= rotation;
    TPointD normPos = calculatePointOnLine(centerPoint, angle, distance);
    newSpace        = std::abs(normPos.y - centerPoint.y);
  }

  return newSpace;
}

TPointD calculateSpacingPos(PerspectiveType perspectiveType, double newSpace,
                            double rotation, TPointD oldSpacingPos,
                            TPointD rotationPos, TPointD centerPoint) {
  TPointD newSpacingPos;

  double dx       = oldSpacingPos.x - centerPoint.x;
  double dy       = oldSpacingPos.y - centerPoint.y;
  double distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));

  double angle;

  if (perspectiveType == PerspectiveType::VanishingPoint) {
    double drx    = rotationPos.x - centerPoint.x;
    double dry    = rotationPos.y - centerPoint.y;
    double rangle = std::atan2(dry, drx) / (3.14159 / 180);
    if (rangle < 0) rangle += 360;

    angle = rangle + newSpace;
  } else {  // Assumed Line
    angle = std::atan2(dy, dx) / (3.14159 / 180);
    angle -= rotation;
    TPointD normPos = calculatePointOnLine(centerPoint, angle, distance);
    dy              = newSpace;
    if ((normPos.y - centerPoint.y) < 0) dy *= -1;
    dx       = normPos.x - centerPoint.x;
    distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
    angle    = std::atan2(dy, dx) / (3.14159 / 180);
    angle += rotation;
  }

  newSpacingPos = calculatePointOnLine(centerPoint, angle, distance);

  return newSpacingPos;
}

void PerspectiveTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_perspectiveObjs.size()) return;

  TPointD usePos = pos;

  if (m_selecting) {
    m_selectingRect.x1 = usePos.x;
    m_selectingRect.y1 = usePos.y;

    TRectD rect(m_selectingRect);
    if (rect.x0 > rect.x1) std::swap(rect.x0, rect.x1);
    if (rect.y0 > rect.y1) std::swap(rect.y0, rect.y1);

    std::vector<PerspectiveObject *>::iterator it;
    for (int i = 0; i < m_perspectiveObjs.size(); i++) {
      PerspectiveObject *obj = m_perspectiveObjs[i];
      if (obj->isActive()) continue;
      if (rect.contains(obj->getCenterPoint())) {
        m_selection.select(i);
        obj->setActive(true);
      }
    }

    invalidate();
    return;
  }

  if (m_selection.isEmpty() || m_mainControlIndex < 0) return;

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);

    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setActive(false);

    loadLastPreset();

    updateToolOptionValues();
  }

  if (!m_undo) m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  bool applyToSelection = true;

  PerspectiveObject *mainObj = m_perspectiveObjs[m_mainControlIndex];

  TPointD centerPoint = mainObj->getCenterPoint();
  TPointD rotationPos = mainObj->getRotationPos();
  TPointD spacingPos  = mainObj->getSpacingPos();
  TPointD dPos        = usePos - m_firstPos;
  double dAngle       = 0.0;
  double dSpace       = 0.0;

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
  } else if (m_isSpacing) {
    TPointD newSpacingPos = spacingPos + dPos;
    double newSpace;

    if (mainObj->getType() == PerspectiveType::VanishingPoint) {
      double dox    = spacingPos.x - centerPoint.x;
      double doy    = spacingPos.y - centerPoint.y;
      double oangle = std::atan2(doy, dox) / (3.14159 / 180);

      double dnx    = newSpacingPos.x - centerPoint.x;
      double dny    = newSpacingPos.y - centerPoint.y;
      double nangle = std::atan2(dny, dnx) / (3.14159 / 180);
      dAngle        = nangle - oangle;
      if (nangle < 0) nangle += 360;

      double drx    = rotationPos.x - centerPoint.x;
      double dry    = rotationPos.y - centerPoint.y;
      double rangle = std::atan2(dry, drx) / (3.14159 / 180);
      if (rangle < 0) rangle += 360;

      newSpace                     = std::abs(rangle - nangle);
      if (newSpace > 180) newSpace = 360 - newSpace;
      dSpace                       = newSpace - mainObj->getSpacing();
    } else {  // Assumed Line
      double dx       = newSpacingPos.x - centerPoint.x;
      double dy       = newSpacingPos.y - centerPoint.y;
      double distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
      double angle    = std::atan2(dy, dx) / (3.14159 / 180);
      angle -= mainObj->getRotation();
      TPointD normPos = calculatePointOnLine(centerPoint, angle, distance);
      newSpace        = std::abs(normPos.y - centerPoint.y);
      dSpace          = newSpace - mainObj->getSpacing();
    }
  } else if (!m_isShifting &&
             mainObj->getType() == PerspectiveType::VanishingPoint) {
    TPointD leftPivotPos   = mainObj->getLeftPivotPos();
    TPointD leftHandlePos  = mainObj->getLeftHandlePos();
    TPointD rightPivotPos  = mainObj->getRightPivotPos();
    TPointD rightHandlePos = mainObj->getRightHandlePos();

    if (m_isLeftMoving) {
      TPointD oldCenterPoint = centerPoint;
      TPointD newPos         = leftHandlePos + dPos;
      mainObj->setLeftHandlePos(newPos);

      // Also move Center Point
      TPointD newCenterPoint;
      TPointD dPosCP;
      if (mainObj->isHorizon() && e.isAltPressed()) {
        TPointD otherHorizonPoint =
            calculatePointOnLine(centerPoint, mainObj->getRotation(), 100);
        newCenterPoint = calculateCenterPoint(newPos, leftPivotPos, centerPoint,
                                              otherHorizonPoint);
      } else
        newCenterPoint = calculateCenterPoint(newPos, leftPivotPos,
                                              rightHandlePos, rightPivotPos);
      if (!std::isnan(newCenterPoint.x) && !std::isnan(newCenterPoint.y)) {
        dPosCP      = newCenterPoint - centerPoint;
        centerPoint = newCenterPoint;
      }
      mainObj->setCenterPoint(centerPoint);

      if (mainObj->isHorizon() && e.isAltPressed()) {
        // Move Rotation/Space controls to maintain angle/spacing
        mainObj->setRotationPos(rotationPos + dPosCP);
        mainObj->setSpacingPos(spacingPos + dPosCP);

        updateMeasuredValueToolOptions();
      } else {
        // Recalculate Angle
        double dox    = rotationPos.x - oldCenterPoint.x;
        double doy    = rotationPos.y - oldCenterPoint.y;
        double oangle = std::atan2(doy, dox) / (3.14159 / 180);
        if (oangle < 0) oangle += 360;

        double dnx    = rotationPos.x - newCenterPoint.x;
        double dny    = rotationPos.y - newCenterPoint.y;
        double nangle = std::atan2(dny, dnx) / (3.14159 / 180);
        if (nangle < 0) nangle += 360;

        double dAngle;
        if ((oangle >= 270 && oangle <= 360 && nangle >= 0 && nangle <= 90))
          dAngle = (360 - oangle) + nangle;
        else if ((oangle >= 0 && oangle <= 90 && nangle >= 270 &&
                  nangle <= 360))
          dAngle = (oangle + (360 - nangle)) * -1;
        else
          dAngle = nangle - oangle;

        mainObj->setRotation(mainObj->getRotation() + dAngle);

        // Recalculate Spacing
        double newSpacing =
            calculateSpacing(mainObj->getType(), spacingPos, rotationPos,
                             mainObj->getRotation(), centerPoint);
        mainObj->setSpacing(newSpacing);
        updateMeasuredValueToolOptions();
      }

      // Check Right Handle
      rightHandlePos = calculateHandlePos(rightHandlePos, rightPivotPos,
                                          TPointD(0.0, 0.0), centerPoint);
      mainObj->setRightHandlePos(rightHandlePos);

      applyToSelection = false;
    } else if (m_isRightMoving) {
      TPointD oldCenterPoint = centerPoint;
      TPointD newPos         = rightHandlePos + dPos;
      mainObj->setRightHandlePos(newPos);

      // Also move Center Point
      TPointD newCenterPoint;
      TPointD dPosCP;
      if (mainObj->isHorizon() && e.isAltPressed()) {
        TPointD otherHorizonPoint =
            calculatePointOnLine(centerPoint, mainObj->getRotation(), 100);
        newCenterPoint = calculateCenterPoint(centerPoint, otherHorizonPoint,
                                              newPos, rightPivotPos);
      } else
        newCenterPoint = calculateCenterPoint(leftHandlePos, leftPivotPos,
                                              newPos, rightPivotPos);
      if (!std::isnan(newCenterPoint.x) && !std::isnan(newCenterPoint.y)) {
        dPosCP      = newCenterPoint - centerPoint;
        centerPoint = newCenterPoint;
      }
      mainObj->setCenterPoint(centerPoint);

      if (mainObj->isHorizon() && e.isAltPressed()) {
        // Move Rotation/Space controls to maintain angle/spacing;
        mainObj->setRotationPos(rotationPos + dPosCP);
        mainObj->setSpacingPos(spacingPos + dPosCP);
        updateMeasuredValueToolOptions();
      } else {
        // Recalculate Angle
        double dox    = rotationPos.x - oldCenterPoint.x;
        double doy    = rotationPos.y - oldCenterPoint.y;
        double oangle = std::atan2(doy, dox) / (3.14159 / 180);
        if (oangle < 0) oangle += 360;

        double dnx    = rotationPos.x - newCenterPoint.x;
        double dny    = rotationPos.y - newCenterPoint.y;
        double nangle = std::atan2(dny, dnx) / (3.14159 / 180);
        if (nangle < 0) nangle += 360;

        double dAngle;
        if ((oangle >= 270 && oangle <= 360 && nangle >= 0 && nangle <= 90))
          dAngle = (360 - oangle) + nangle;
        else if ((oangle >= 0 && oangle <= 90 && nangle >= 270 &&
                  nangle <= 360))
          dAngle = (oangle + (360 - nangle)) * -1;
        else
          dAngle = nangle - oangle;

        mainObj->setRotation(mainObj->getRotation() + dAngle);

        // Recalculate Spacing
        double newSpacing =
            calculateSpacing(mainObj->getType(), spacingPos, rotationPos,
                             mainObj->getRotation(), centerPoint);
        mainObj->setSpacing(newSpacing);
        updateMeasuredValueToolOptions();
      }

      // Check Left Handle
      leftHandlePos = calculateHandlePos(leftHandlePos, leftPivotPos,
                                         TPointD(0.0, 0.0), centerPoint);
      mainObj->setLeftHandlePos(leftHandlePos);

      applyToSelection = false;
    } else if (m_isLeftPivoting) {
      TPointD newPos = leftPivotPos + dPos;
      mainObj->setLeftPivotPos(newPos);

      // Also move Left Handle
      leftHandlePos =
          calculateHandlePos(leftHandlePos, leftPivotPos, dPos, centerPoint);
      mainObj->setLeftHandlePos(leftHandlePos);

      applyToSelection = false;
    } else if (m_isRightPivoting) {
      TPointD newPos = rightPivotPos + dPos;
      mainObj->setRightPivotPos(newPos);

      // Also move Right Handle
      rightHandlePos =
          calculateHandlePos(rightHandlePos, rightPivotPos, dPos, centerPoint);
      mainObj->setRightHandlePos(rightHandlePos);

      applyToSelection = false;
    }
  }

  if (applyToSelection) {
    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++) {
      PerspectiveObject *obj = m_perspectiveObjs[*it];
      TPointD objCenterPoint = obj->getCenterPoint();
      TPointD rotationPos    = obj->getRotationPos();
      TPointD spacingPos     = obj->getSpacingPos();

      if (m_isShifting) {
        if (obj->isHorizon() && e.isAltPressed()) {
          double distance =
              std::sqrt(std::pow(dPos.x, 2) + std::pow(dPos.y, 2));
          double rotation = obj->getRotation();
          if (rotation != 90 && rotation != 270) {
            if (dPos.x < 0) distance *= -1;
            if (rotation > 90 && rotation < 270) distance *= -1;
          } else if ((rotation == 90 && dPos.y < 0) ||
                     (rotation == 270 && dPos.y > 0))
            distance *= -1;
          TPointD newCenter =
              calculatePointOnLine(objCenterPoint, rotation, distance);
          dPos = newCenter - objCenterPoint;
        }

        obj->shiftPerspectiveObject(dPos);
      } else if (m_isCenterMoving) {  // Moving
        TPointD newCenterPoint = objCenterPoint + dPos;
        obj->setCenterPoint(newCenterPoint);

        // Recalculate Angle
        double dox    = rotationPos.x - objCenterPoint.x;
        double doy    = rotationPos.y - objCenterPoint.y;
        double oangle = std::atan2(doy, dox) / (3.14159 / 180);
        if (oangle < 0) oangle += 360;

        double dnx    = rotationPos.x - newCenterPoint.x;
        double dny    = rotationPos.y - newCenterPoint.y;
        double nangle = std::atan2(dny, dnx) / (3.14159 / 180);
        if (nangle < 0) nangle += 360;

        double dAngle;
        if ((oangle >= 270 && oangle <= 360 && nangle >= 0 && nangle <= 90))
          dAngle = (360 - oangle) + nangle;
        else if ((oangle >= 0 && oangle <= 90 && nangle >= 270 &&
                  nangle <= 360))
          dAngle = (oangle + (360 - nangle)) * -1;
        else
          dAngle = nangle - oangle;

        obj->setRotation(obj->getRotation() + dAngle);

        // Recalculate Spacing
        double newSpacing =
            calculateSpacing(obj->getType(), spacingPos, rotationPos,
                             obj->getRotation(), objCenterPoint);
        obj->setSpacing(newSpacing);
        if (obj == mainObj) updateMeasuredValueToolOptions();

        // Also Move Left and Right Handles
        if (obj->getType() == PerspectiveType::VanishingPoint) {
          TPointD leftPivotPos   = obj->getLeftPivotPos();
          TPointD leftHandlePos  = obj->getLeftHandlePos();
          TPointD rightPivotPos  = obj->getRightPivotPos();
          TPointD rightHandlePos = obj->getRightHandlePos();

          leftHandlePos = calculateHandlePos(leftHandlePos, leftPivotPos,
                                             TPointD(0.0, 0.0), objCenterPoint);
          obj->setLeftHandlePos(leftHandlePos);

          rightHandlePos = calculateHandlePos(
              rightHandlePos, rightPivotPos, TPointD(0.0, 0.0), objCenterPoint);
          obj->setRightHandlePos(rightHandlePos);
        }
      } else if (m_isRotating) {
        double rotation        = obj->getRotation();
        TPointD newRotationPos = rotationPos + dPos;
        double rot =
            rotation +
            ((obj->getType() == PerspectiveType::VanishingPoint) ? 270 : 0);
        TPointD rotPos  = obj == mainObj ? usePos : rotationPos;
        double distance = std::sqrt(std::pow((objCenterPoint.x - rotPos.x), 2) +
                                    std::pow((objCenterPoint.y - rotPos.y), 2));
        if (e.isAltPressed()) {
          double ang = tfloor((int)((rot + dAngle) + 7.5), 15);
          dAngle     = ang - rot;
          newRotationPos =
              calculatePointOnLine(objCenterPoint, (rot + dAngle), distance);
          if (obj == mainObj)
            usePos = dAngle == 0 ? m_firstPos : newRotationPos;
        } else if (obj != mainObj) {
          newRotationPos =
              calculatePointOnLine(objCenterPoint, (rot + dAngle), distance);
        }
        obj->setRotation(rotation + dAngle);
        obj->setRotationPos(newRotationPos);

        // Move Spacing control to keep current spacing
        double dx     = spacingPos.x - objCenterPoint.x;
        double dy     = spacingPos.y - objCenterPoint.y;
        distance      = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
        double angle  = std::atan2(dy, dx) / (3.14159 / 180);
        double nangle = angle + dAngle;

        TPointD newSpacingPos =
            calculatePointOnLine(objCenterPoint, nangle, distance);

        obj->setSpacingPos(newSpacingPos);

        if (obj == mainObj) updateMeasuredValueToolOptions();
      } else if (m_isSpacing) {
        double spacing        = obj->getSpacing();
        TPointD newSpacingPos = spacingPos + dPos;
        double delta          = dSpace;

        TPointD pos     = obj == mainObj ? usePos : spacingPos;
        double dx       = pos.x - objCenterPoint.x;
        double dy       = pos.y - objCenterPoint.y;
        double distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
        double angle    = std::atan2(dy, dx) / (3.14159 / 180);

        if (e.isAltPressed()) {
          double space = tfloor((int)(spacing + delta + 5), 10);
          delta        = space - spacing;

          if (obj->getType() == PerspectiveType::VanishingPoint) {
            double dx2    = newSpacingPos.x - objCenterPoint.x;
            double dy2    = newSpacingPos.y - objCenterPoint.y;
            double angle2 = std::atan2(dy2, dx2) / (3.14159 / 180);
            if (angle2 < 0) angle2 += 360;

            double drx    = rotationPos.x - objCenterPoint.x;
            double dry    = rotationPos.y - objCenterPoint.y;
            double rangle = std::atan2(dry, drx) / (3.14159 / 180);
            if (rangle < 0) rangle += 360;

            double diffAngle = angle2 - rangle;
            if (diffAngle < 0) diffAngle += 360;

            double newAngle = spacing + delta + 270;
            if (newAngle < 0) newAngle += 360;

            if (diffAngle > 180) {
              newAngle *= -1;
              distance *= -1;
            }
            newAngle += obj->getRotation();

            newSpacingPos =
                calculatePointOnLine(objCenterPoint, newAngle, distance);
          } else {  // Assumed Line
            angle -= obj->getRotation();
            TPointD normPos =
                calculatePointOnLine(objCenterPoint, angle, distance);
            dy = spacing + delta;
            if ((normPos.y - objCenterPoint.y) < 0) dy *= -1;
            dx       = normPos.x - objCenterPoint.x;
            distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
            angle    = std::atan2(dy, dx) / (3.14159 / 180);
            angle += obj->getRotation();
            newSpacingPos =
                calculatePointOnLine(objCenterPoint, angle, distance);
          }

          if (obj == mainObj) usePos = delta == 0 ? m_firstPos : newSpacingPos;

        } else if (obj != mainObj) {
          if (obj->getType() == PerspectiveType::VanishingPoint) {
            double nangle = angle + dAngle;

            newSpacingPos =
                calculatePointOnLine(objCenterPoint, nangle, distance);
            if (nangle < 0) nangle += 360;

            double drx    = rotationPos.x - objCenterPoint.x;
            double dry    = rotationPos.y - objCenterPoint.y;
            double rangle = std::atan2(dry, drx) / (3.14159 / 180);
            if (rangle < 0) rangle += 360;

            double newSpace              = std::abs(rangle - nangle);
            if (newSpace > 180) newSpace = 360 - newSpace;
            delta                        = newSpace - spacing;
          } else {  // Assumed line
            angle -= obj->getRotation();
            TPointD normPos =
                calculatePointOnLine(objCenterPoint, angle, distance);
            dy = spacing + delta;
            if ((normPos.y - objCenterPoint.y) < 0) dy *= -1;
            dx       = normPos.x - objCenterPoint.x;
            distance = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
            angle    = std::atan2(dy, dx) / (3.14159 / 180);
            angle += obj->getRotation();
            newSpacingPos =
                calculatePointOnLine(objCenterPoint, angle, distance);
          }
        }

        obj->setSpacing(spacing + delta);
        obj->setSpacingPos(newSpacingPos);

        if (obj == mainObj) updateMeasuredValueToolOptions();
      }
    }
  }

  m_modified = true;
  m_firstPos = usePos;

  invalidate();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (m_undo) {
    if (!m_modified)
      delete m_undo;
    else {
      m_undo->setRedoData(m_perspectiveObjs);
      TUndoManager::manager()->add(m_undo);
    }
    m_undo = 0;
  }
  invalidate();

  m_modified        = false;
  m_isShifting      = false;
  m_isCenterMoving  = false;
  m_isRotating      = false;
  m_isSpacing       = false;
  m_isLeftPivoting  = false;
  m_isLeftMoving    = false;
  m_isRightPivoting = false;
  m_isRightMoving   = false;
  m_selecting       = false;

  m_totalChange = 0;
}

//----------------------------------------------------------------------------------------------

bool PerspectiveTool::keyDown(QKeyEvent *event) {
  TPointD delta;

  switch (event->key()) {
  case Qt::Key_Up:
    delta.y = 1;
    break;
  case Qt::Key_Down:
    delta.y = -1;
    break;
  case Qt::Key_Left:
    delta.x = -1;
    break;
  case Qt::Key_Right:
    delta.x = 1;
    break;
  default:
    return false;
    break;
  }

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;
  for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
    m_perspectiveObjs[*it]->shiftPerspectiveObject(delta);

  return true;
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::invalidateControl(int controlIdx) {
  if (!m_perspectiveObjs.size() || controlIdx >= m_perspectiveObjs.size())
    return;
  invalidate(m_perspectiveObjs[controlIdx]->getCenterPointRect());
  invalidate(m_perspectiveObjs[controlIdx]->getRotationRect());
  invalidate(m_perspectiveObjs[controlIdx]->getSpacingRect());
  invalidate(m_perspectiveObjs[controlIdx]->getLeftPivotRect());
  invalidate(m_perspectiveObjs[controlIdx]->getLeftHandleRect());
  invalidate(m_perspectiveObjs[controlIdx]->getRightPivotRect());
  invalidate(m_perspectiveObjs[controlIdx]->getRightHandleRect());
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::onEnter() {
  if (!m_selection.isEmpty()) return;

  for (int i = 0; i < m_perspectiveObjs.size(); i++)
    if (m_perspectiveObjs[i]->isActive()) m_selection.select(i);

  if (!m_selection.isEmpty()) m_selection.makeCurrent();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::onDeactivate() {
  if (m_selection.isEmpty()) return;

  for (int i = 0; i < m_perspectiveObjs.size(); i++)
    if (m_perspectiveObjs[i]->isActive()) {
      m_perspectiveObjs[i]->setActive(false);
      invalidateControl(i);
    }
  m_selection.selectNone();
  m_mainControlIndex = -1;
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::deleteSelectedObjects() {
  if (m_selection.isEmpty()) return;

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);

    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
      m_perspectiveObjs[*it]->setActive(false);

    loadLastPreset();
  }

  m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  std::set<int>::reverse_iterator rit = selectedObjects.rbegin();
  for (; rit != selectedObjects.rend(); rit++) {
    m_lastPreset.erase(m_lastPreset.begin() + *rit);
    m_perspectiveObjs.erase(m_perspectiveObjs.begin() + *rit);
  }

  m_selection.selectNone();
  m_mainControlIndex = -1;

  m_undo->setRedoData(m_perspectiveObjs);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;

  invalidate();

  updateToolOptionValues();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::selectAllObjects() {
  if (!m_perspectiveObjs.size()) return;

  for (int i = 0; i < m_perspectiveObjs.size(); i++) {
    m_selection.select(i);
    m_perspectiveObjs[i]->setActive(true);
  }
  m_selection.makeCurrent();
  invalidate();
  updateToolOptionValues();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::setPerspectiveObjects(
    std::vector<PerspectiveObject *> objs) {
  m_perspectiveObjs = objs;
  m_selection.selectNone();
  m_lastPreset       = copyPerspectiveSet(m_perspectiveObjs);
  m_mainControlIndex = -1;

  if (m_preset.getValue() != CUSTOM_WSTR) m_preset.setValue(CUSTOM_WSTR);

  updateToolOptionValues();
}

//----------------------------------------------------------------------------------------------

std::vector<PerspectiveObject *> PerspectiveTool::copyPerspectiveSet(
    std::vector<PerspectiveObject *> perspectiveSet, bool keepStatus) {
  std::vector<PerspectiveObject *> copy;

  std::vector<PerspectiveObject *>::iterator it;
  for (it = perspectiveSet.begin(); it != perspectiveSet.end(); it++) {
    PerspectiveObject *currentObject = *it;
    PerspectiveObject *newObject;

    VanishingPointPerspective *v =
        dynamic_cast<VanishingPointPerspective *>(*it);
    LinePerspective *l = dynamic_cast<LinePerspective *>(*it);

    if (v)
      newObject = new VanishingPointPerspective();
    else if (l)
      newObject = new LinePerspective();
    else
      continue;

    newObject->setCenterPoint(currentObject->getCenterPoint());
    newObject->setRotationPos(currentObject->getRotationPos());
    newObject->setSpacingPos(currentObject->getSpacingPos());
    newObject->setLeftPivotPos(currentObject->getLeftPivotPos());
    newObject->setLeftHandlePos(currentObject->getLeftHandlePos());
    newObject->setRightPivotPos(currentObject->getRightPivotPos());
    newObject->setRightHandlePos(currentObject->getRightHandlePos());
    newObject->setRotation(currentObject->getRotation());
    newObject->setSpacing(currentObject->getSpacing());
    newObject->setOpacity(currentObject->getOpacity());
    newObject->setColor(currentObject->getColor());
    newObject->setHorizon(currentObject->isHorizon());
    newObject->setParallel(currentObject->isParallel());
    newObject->setShowAdvancedControls(PerspectiveToolAdvancedControls);
    if (keepStatus) newObject->setActive(currentObject->isActive());

    copy.push_back(newObject);
  }

  return copy;
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::initPresets() {
  if (!m_presetsManager.isLoaded()) {
    TFilePath projectFolder =
        TProjectManager::instance()->getCurrentProject()->getProjectFolder();
    m_presetsManager.loadPresets(projectFolder + "grids");
    m_presetsManager.loadPresets(ToonzFolder::getLibraryFolder() +
                                 "perspective grids");
    m_presetsManager.setIsLoaded(true);
  }

  // Rebuild the presets property entries
  const std::set<PerspectivePreset> &presets = m_presetsManager.presets();

  m_preset.deleteAllValues();
  m_preset.addValue(CUSTOM_WSTR);
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));

  std::set<PerspectivePreset>::const_iterator it, end = presets.end();
  for (it = presets.begin(); it != end; ++it)
    m_preset.addValue(it->m_presetName);
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::loadPreset() {
  const std::set<PerspectivePreset> &presets = m_presetsManager.presets();
  std::set<PerspectivePreset>::const_iterator it;

  PerspectivePreset preset;
  for (it = presets.begin(); it != presets.end(); it++) {
    preset = *it;
    if (preset.m_presetName == m_preset.getValue()) break;
  }
  if (it == presets.end()) return;

  m_perspectiveObjs = preset.m_perspectiveSet;
  onPropertyChanged(m_advancedControls.getName());

  invalidate();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::addPreset(QString name, bool isLibrary) {
  TFilePath projectFolder =
      TProjectManager::instance()->getCurrentProject()->getProjectFolder();

  TFilePath path =
      (isLibrary ? (ToonzFolder::getLibraryFolder() + "perspective grids")
                 : (projectFolder + "grids")) +
      TFilePath(name);
  path = path.withType("grid");

  PerspectivePreset preset(name.toStdWString(), path);

  preset.m_perspectiveSet = m_perspectiveObjs;

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_presetName);
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);

  initPresets();

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;

  for (it = selectedObjects.begin(); it != selectedObjects.end(); it++)
    m_perspectiveObjs[*it]->setActive(false);
  m_selection.selectNone();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  loadLastPreset();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::loadLastPreset() {
  m_perspectiveObjs = m_lastPreset;
  onPropertyChanged(m_advancedControls.getName());

  invalidate();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::updateSpacing(double space) {
  if (m_selection.isEmpty()) return;

  m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;

  for (it = selectedObjects.begin(); it != selectedObjects.end(); it++) {
    PerspectiveObject *obj = m_perspectiveObjs[*it];

    obj->setSpacing(space);

    // Move Spacing Control
    TPointD newSpacingPos = calculateSpacingPos(
        obj->getType(), space, obj->getRotation(), obj->getSpacingPos(),
        obj->getRotationPos(), obj->getCenterPoint());
    obj->setSpacingPos(newSpacingPos);
  }

  m_undo->setRedoData(m_perspectiveObjs);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
  invalidate();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::updateRotation(double rotation) {
  if (m_selection.isEmpty()) return;

  m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;

  for (it = selectedObjects.begin(); it != selectedObjects.end(); it++) {
    PerspectiveObject *obj = m_perspectiveObjs[*it];

    TPointD centerPoint    = obj->getCenterPoint();
    TPointD oldRotationPos = obj->getRotationPos();
    double oldRotation     = obj->getRotation();

    obj->setRotation(rotation);

    // Move Rotation Control
    double dx           = oldRotationPos.x - centerPoint.x;
    double dy           = oldRotationPos.y - centerPoint.y;
    double distance     = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
    double controlAngle = rotation;
    double oldAngle     = oldRotation;
    if (obj->getType() == PerspectiveType::VanishingPoint) {
      controlAngle += 270;
      oldAngle += 270;
    }
    double deltaAngle = controlAngle - oldAngle;

    TPointD newRotationPos =
        calculatePointOnLine(centerPoint, controlAngle, distance);
    obj->setRotationPos(newRotationPos);

    // Also move Spacing Control to maintain current spacing
    TPointD oldSpacingPos = obj->getSpacingPos();
    dx                    = oldSpacingPos.x - centerPoint.x;
    dy                    = oldSpacingPos.y - centerPoint.y;
    distance              = std::sqrt(std::pow(dx, 2) + std::pow(dy, 2));
    double space          = obj->getSpacing();
    controlAngle          = std::atan2(dy, dx) / (3.14159 / 180);
    controlAngle += deltaAngle;
    TPointD newSpacingPos =
        calculatePointOnLine(centerPoint, controlAngle, distance);
    obj->setSpacingPos(newSpacingPos);
  }

  m_undo->setRedoData(m_perspectiveObjs);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
  invalidate();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::updateMeasuredValueToolOptions() {
  double spacing = 0, rotation = 0;
  if (m_mainControlIndex != -1 &&
      m_mainControlIndex < m_perspectiveObjs.size()) {
    PerspectiveObject *obj = m_perspectiveObjs[m_mainControlIndex];
    if (obj) {
      spacing  = obj->getSpacing();
      rotation = obj->getRotation();
    }
  }

  for (int i = 0; i < (int)m_toolOptionsBox.size(); i++)
    m_toolOptionsBox[i]->updateMeasuredValues(spacing, rotation);
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::updateToolOptionValues() {
  if (m_mainControlIndex != -1 &&
      m_mainControlIndex < m_perspectiveObjs.size()) {
    PerspectiveObject *obj = m_perspectiveObjs[m_mainControlIndex];
    if (obj) {
      m_opacity.setValue(obj->getOpacity());
      m_color.setColor(obj->getColor());
      m_horizon.setValue(obj->isHorizon());
      m_parallel.setValue(obj->isParallel());
    }
  }

  updateMeasuredValueToolOptions();

  m_propertyUpdating = true;
  getApplication()->getCurrentTool()->notifyToolChanged();
  m_propertyUpdating = false;
}

//----------------------------------------------------------------------------------------------

void VanishingPointPerspective::draw(SceneViewer *viewer, TRectD cameraRect) {
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

  double step            = getSpacing();
  step                   = std::min(step, getMaxSpacing());
  step                   = std::max(step, getMinSpacing());
  int rays               = isHorizon() ? totalDistance : (360 / step);
  if (rays == 0) rays    = 1;
  if (!isHorizon()) step = 360.0 / (double)rays;

  double startAngle = 270.0 + getRotation();
  double endAngle   = (isHorizon() ? 180.0 : 90.0) + getRotation();

  double yLength, xLength;

  int raySpacing = 0;
  if (isHorizon()) {
    // Draw horizon line
    yLength = std::sin(getRotation() * (3.14159 / 180)) * totalDistance;
    xLength = std::cos(getRotation() * (3.14159 / 180)) * totalDistance;
    end.x   = p.x + xLength;
    end.y   = p.y + yLength;
    tglDrawSegment(p, end);
    yLength =
        std::sin((180.0 + getRotation()) * (3.14159 / 180)) * totalDistance;
    xLength =
        std::cos((180.0 + getRotation()) * (3.14159 / 180)) * totalDistance;
    end.x = p.x + xLength;
    end.y = p.y + yLength;
    tglDrawSegment(p, end);

    double sinAngle             = std::sin((270 + step) * (3.14159 / 180));
    double cosAngle             = std::cos((270 + step) * (3.14159 / 180));
    if (cosAngle == 0) cosAngle = 0.001;
    double slope                = sinAngle / cosAngle;
    if (slope == 0) slope       = 0.001;
    raySpacing = std::abs((int)((totalDistance - p.y) / slope));
    if (!raySpacing) raySpacing = 1;
  }

  double lAngle = startAngle;
  double rAngle = startAngle;
  for (i = 1; i <= rays; i++) {
    bool drawRight = true;

    TPointD lStartPoint = p, rStartPoint = p, lowerCenterPoint = p,
            lowerRefPoint = p;

    if (isHorizon()) {
      double angle =
          std::atan(-totalDistance / (double)(raySpacing * (i - 1))) /
          (3.14159 / 180);
      rAngle = getRotation() + 360 + angle;
      lAngle = getRotation() + 180 - angle;

      if (step < 60) {
        double dlower = (60 - step);

        lowerCenterPoint.y = p.y + ::sin(startAngle * (3.14159 / 180)) * dlower;
        lowerCenterPoint.x = p.x + ::cos(startAngle * (3.14159 / 180)) * dlower;
        yLength            = std::sin(getRotation() * (3.14159 / 180)) * dlower;
        xLength            = std::cos(getRotation() * (3.14159 / 180)) * dlower;
        lowerRefPoint.x    = lowerCenterPoint.x + xLength;
        lowerRefPoint.y    = lowerCenterPoint.y + yLength;
      }
    } else {
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
    }

    // Draw left of startAngle
    yLength = std::sin(lAngle * (3.14159 / 180)) * totalDistance;
    xLength = std::cos(lAngle * (3.14159 / 180)) * totalDistance;
    end.x   = p.x + xLength;
    end.y   = p.y + yLength;
    if (lowerCenterPoint != p)
      lStartPoint =
          calculateCenterPoint(end, p, lowerCenterPoint, lowerRefPoint);
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
        rStartPoint =
            calculateCenterPoint(end, p, lowerCenterPoint, lowerRefPoint);
      tglDrawSegment(rStartPoint, end);
    }

    rAngle += step;
    lAngle -= step;
  }

  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
}

//----------------------------------------------------------------------------------------------

void LinePerspective::draw(SceneViewer *viewer, TRectD cameraRect) {
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

  glEnable(GL_BLEND);  // Enable blending.
  glEnable(GL_LINE_SMOOTH);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glLineWidth(1.0f);

  TPointD p = getCenterPoint();

  TPixel32 color = getColor();
  color.m        = ((getOpacity() / 100.0) * 255);
  tglColor(color);

  TPointD start, mid, end;
  double distanceToLeft   = std::abs(p.x - bounds.x0);
  double distanceToRight  = std::abs(p.x - bounds.x1);
  double distanceToTop    = std::abs(p.y - bounds.y1);
  double distanceToBottom = std::abs(p.y - bounds.y0);
  double xDistance        = std::max(distanceToLeft, distanceToRight);
  double yDistance        = std::max(distanceToTop, distanceToBottom);
  double distance         = std::max(xDistance, yDistance);
  double totalDistance =
      std::sqrt(std::pow(distance, 2) + std::pow(distance, 2));

  double theta = getRotation() * (3.14159 / 180);

  double yLength = std::sin(theta) * totalDistance;
  double xLength = std::cos(theta) * totalDistance;

  // Draw line at control Point
  mid     = p;
  start.x = mid.x - xLength;
  start.y = mid.y - yLength;
  end.x   = mid.x + xLength;
  end.y   = mid.y + yLength;
  tglDrawSegment(start, end);

  // Draw lines above/below control point
  if (isParallel()) {
    TPointD above         = p;
    TPointD below         = p;
    double step           = getSpacing();
    step                  = std::min(step, getMaxSpacing());
    step                  = std::max(step, getMinSpacing());
    double distanceFromCP = step;

    double stepspeed = isHorizon() ? 1.5 : 1.0;

    while (distanceFromCP <= totalDistance) {
      if (!isHorizon()) {
        above.y = p.y + (std::cos(-theta) * distanceFromCP);
        above.x = p.x + (std::sin(-theta) * distanceFromCP);
        start.x = above.x - xLength;
        start.y = above.y - yLength;
        end.x   = above.x + xLength;
        end.y   = above.y + yLength;
        tglDrawSegment(start, end);
      }

      below.y = p.y - (std::cos(-theta) * distanceFromCP);
      below.x = p.x - (std::sin(-theta) * distanceFromCP);
      start.x = below.x - xLength;
      start.y = below.y - yLength;
      end.x   = below.x + xLength;
      end.y   = below.y + yLength;
      tglDrawSegment(start, end);

      distanceFromCP += (step * stepspeed);
      step = step * stepspeed;
    }
  }

  glLineWidth(1.0f);
  glDisable(GL_LINE_SMOOTH);
  glDisable(GL_BLEND);
}

TPointD LinePerspective::getReferencePoint(TPointD firstPoint) {
  TPointD refPoint;

  double theta = getRotation() * (3.14159 / 180);

  double yLength = std::sin(theta) * 100;
  double xLength = std::cos(theta) * 100;

  refPoint.x = firstPoint.x + xLength;
  refPoint.y = firstPoint.y + yLength;

  return refPoint;
}
