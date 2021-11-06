#include "perspectivetool.h"

#include "tgl.h"
#include "tenv.h"
#include "tsystem.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"

#include "toonz/stage.h"
#include "toonz/toonzfolders.h"

#include "toonzqt/selectioncommandids.h"

#include "../toonz/sceneviewer.h"
#include "../toonz/tscenehandle.h"
#include "../toonz/toonzscene.h"
#include "../toonz/tcamera.h"

#include <QApplication>

#include <QDebug>

PerspectiveTool perspectiveTool;

//----------------------------------------------------------------------------------------------------------

#define CUSTOM_WSTR L"<custom>"

//----------------------------------------------------------------------------------------------------------

void PerspectivePreset::saveData(TOStream &os) {
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

  while (is.matchTag(tagName)) {
    if (tagName == "Name")
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
  m_undoData = m_tool->copyPerspectiveSet(objs);
}

//----------------------------------------------------------------------------------------------------------

void PerspectiveObjectUndo::setRedoData(std::vector<PerspectiveObject *> objs) {
  if (!m_tool) return;
  m_redoData = m_tool->copyPerspectiveSet(objs);
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
  save();
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePresetManager::removePreset(const std::wstring &name) {
  std::set<PerspectivePreset>::iterator it;
  for (it = m_presets.begin(); it != m_presets.end(); it++) {
    PerspectivePreset preset = *it;
    if (preset.m_presetName == name) m_presets.erase(preset);
  }
  save();
}

//----------------------------------------------------------------------------------------------------------

void PerspectivePresetManager::load(const TFilePath &fp) {
  m_fp = fp;

  std::string tagName;
  TIStream is(m_fp);
  try {
    while (is.matchTag(tagName)) {
      if (tagName == "version") {
        VersionNumber version;
        is >> version.first >> version.second;

        is.setVersion(version);
        is.matchEndTag();
      } else if (tagName == "presets") {
        while (is.matchTag(tagName)) {
          if (tagName == "preset") {
            PerspectivePreset data;

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

void PerspectivePresetManager::save() {
  TSystem::touchParentDir(m_fp);

  TOStream os(m_fp);

  os.openChild("version");
  os << 1 << 0;
  os.closeChild();

  os.openChild("presets");

  std::set<PerspectivePreset>::iterator it, end = m_presets.end();
  for (it = m_presets.begin(); it != end; ++it) {
    os.openChild("preset");
    os << (TPersist &)*it;
    os.closeChild();
  }

  os.closeChild();
}

//----------------------------------------------------------------------------------------------------------

void PerspectiveControls::drawControls() {
  if (!m_perspective) return;

  TPointD centerPoint = m_perspective->getCenterPoint();

  glLineWidth(1.5);

  glLineStipple(1, 0xFFFF);
  glEnable(GL_LINE_STIPPLE);

  // Draw rotation control
  glColor3d(0.70, 0.70, 0.70);
  tglDrawSegment(centerPoint, m_rotationPos);
  tglDrawCircle(m_rotationPos, m_handleRadius);
  if (m_active) glColor3d(1.0, 1.0, 0.0);
  tglDrawDisk(m_rotationPos, m_handleRadius - 2);

  // Draw spacing control
  if (m_perspective->getType() != PerspectiveType::Line ||
      m_perspective->isParallel()) {
    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(centerPoint, m_spacingPos);
    tglDrawCircle(m_spacingPos, m_handleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_spacingPos, m_handleRadius - 2);
  }

  if (m_perspective->getType() == PerspectiveType::VanishingPoint) {
    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(m_leftPivotPos, m_leftHandlePos);
    tglDrawCircle(m_leftHandlePos, m_handleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_leftHandlePos, m_handleRadius - 2);

    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(centerPoint, m_leftPivotPos);
    tglDrawCircle(m_leftPivotPos, m_handleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_leftPivotPos, m_handleRadius - 2);

    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(m_rightPivotPos, m_rightHandlePos);
    tglDrawCircle(m_rightHandlePos, m_handleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_rightHandlePos, m_handleRadius - 2);

    glColor3d(0.70, 0.70, 0.70);
    tglDrawSegment(centerPoint, m_rightPivotPos);
    tglDrawCircle(m_rightPivotPos, m_handleRadius);
    if (m_active) glColor3d(1.0, 1.0, 0.0);
    tglDrawDisk(m_rightPivotPos, m_handleRadius - 2);
  }

  // Draw center/move control
  glColor3d(0.70, 0.70, 0.70);
  tglDrawCircle(centerPoint, m_handleRadius);
  if (m_active) glColor3d(0.0, 1.0, 0.0);
  tglDrawDisk(centerPoint, m_handleRadius - 2);

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
}

//----------------------------------------------------------------------------------------------

PerspectiveTool::PerspectiveTool()
    : TTool("T_PerspectiveGrid")
    , m_type("Type:")
    , m_opacity("Opacity:", 1.0, 100.0, 30.0)
    , m_color("Color:")
    , m_horizon("Horizon", false)
    , m_parallel("Parallel", false)
    , m_preset("Preset:")
    , m_presetsLoaded(false)
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

  m_preset.setId("PerspectivePreset");
  m_preset.addValue(CUSTOM_WSTR);

  m_selection.setTool(this);
  m_selection.selectNone();
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

  m_preset.setQStringName(tr("Preset:"));
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));
}

//----------------------------------------------------------------------------------------------

TPropertyGroup *PerspectiveTool::getProperties(int idx) {
  if (!m_presetsLoaded) initPresets();

  return &m_prop;
}

//----------------------------------------------------------------------------------------------

bool PerspectiveTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  if (propertyName == m_preset.getName()) {
    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last preset
      loadLastPreset();

    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
    return true;
  }

  std::set<int> selectedObjects = m_selection.getSelectedObjects();
  std::set<int>::iterator it;

  if (propertyName != m_type.getName() && selectedObjects.size() &&
      m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);
    loadLastPreset();

    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
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

  m_totalSpacing = 0;

  m_firstPos = pos;

  int controlIdx = -1;

  for (int i = 0; i < m_perspectiveObjs.size(); i++)
    if (m_perspectiveObjs[i]->isOverControl()) {
      controlIdx = i;
      break;
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
      return;
    }

    m_mainControlIndex = controlIdx;

    if (e.isShiftPressed()) m_isShifting = true;

    // Hit a control while not pressing Ctrl and is already active: Do nothing
    if (m_perspectiveObjs[controlIdx]->isActive()) return;

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
    m_opacity.setValue(m_perspectiveObjs[controlIdx]->getOpacity());
    m_horizon.setValue(m_perspectiveObjs[controlIdx]->isHorizon());
    m_color.setColor(m_perspectiveObjs[controlIdx]->getColor());
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;

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
      return;
    }
  }

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);
    loadLastPreset();

    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
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
  newObject->setActive(true);

  m_perspectiveObjs.push_back(newObject);
  m_lastPreset.push_back(newObject);

  m_mainControlIndex = m_perspectiveObjs.size() - 1;

  m_selection.select(m_mainControlIndex);
  m_selection.makeCurrent();

  m_isShifting = true;  // Allow click and shift
  m_modified   = true;

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

TPointD calculateHorizonPoint(TPointD firstPoint, double rotation,
                              double distance) {
  TPointD refPoint;

  double theta = rotation * (3.14159 / 180);

  double yLength = std::sin(theta) * distance;
  double xLength = std::cos(theta) * distance;

  refPoint.x = firstPoint.x + xLength;
  refPoint.y = firstPoint.y + yLength;

  return refPoint;
}

void PerspectiveTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_perspectiveObjs.size()) return;

  if (m_selecting) {
    m_selectingRect.x1 = pos.x;
    m_selectingRect.y1 = pos.y;

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

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);
    loadLastPreset();

    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

  if (!m_undo) m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  bool applyToSelection = true;

  PerspectiveObject *mainObj = m_perspectiveObjs[m_mainControlIndex];

  TPointD centerPoint = mainObj->getCenterPoint();
  TPointD dPos        = pos - m_firstPos;
  double dAngle       = 0.0;
  double dSpace       = 0.0;

  if (m_isRotating) {
    TPointD a = m_firstPos - centerPoint;
    TPointD b = pos - centerPoint;

    double a2 = norm2(a), b2 = norm2(b);
    const double eps = 1e-8;
    if (a2 < eps || b2 < eps) return;

    dAngle = asin(cross(a, b) / sqrt(a2 * b2)) * M_180_PI;

    if (e.isAltPressed()) {
      m_totalSpacing += dAngle;
      if (std::abs(m_totalSpacing) >= 10) {
        dAngle         = m_totalSpacing < 0 ? -45 : 45;
        m_totalSpacing = 0;
      } else
        dAngle = 0;
    }
  } else if (m_isSpacing) {
    double da = std::sqrt(std::pow(centerPoint.x - m_firstPos.x, 2) +
                          std::pow(centerPoint.y - m_firstPos.y, 2));
    double db = std::sqrt(std::pow(centerPoint.x - pos.x, 2) +
                          std::pow(centerPoint.y - pos.y, 2));
    dSpace = da - db;

    if (e.isAltPressed()) {
      m_totalSpacing += dSpace;
      if (std::abs(m_totalSpacing) >= 10) {
        dSpace         = m_totalSpacing < 0 ? -10 : 10;
        m_totalSpacing = 0;
      } else
        dSpace = 0;
    }
  } else if (!m_isShifting &&
             mainObj->getType() == PerspectiveType::VanishingPoint) {
    TPointD leftPivotPos   = mainObj->getLeftPivotPos();
    TPointD leftHandlePos  = mainObj->getLeftHandlePos();
    TPointD rightPivotPos  = mainObj->getRightPivotPos();
    TPointD rightHandlePos = mainObj->getRightHandlePos();

    if (m_isLeftMoving) {
      TPointD newPos = leftHandlePos + dPos;
      mainObj->setLeftHandlePos(newPos);

      // Also move Center Point
      TPointD newCenterPoint;
      if (mainObj->isHorizon() && e.isAltPressed()) {
        TPointD otherHorizonPoint =
            calculateHorizonPoint(centerPoint, mainObj->getRotation(), 100);
        newCenterPoint = calculateCenterPoint(newPos, leftPivotPos, centerPoint,
                                              otherHorizonPoint);
      } else
        newCenterPoint = calculateCenterPoint(newPos, leftPivotPos,
                                              rightHandlePos, rightPivotPos);
      if (!std::isnan(newCenterPoint.x) && !std::isnan(newCenterPoint.y))
        centerPoint = newCenterPoint;
      mainObj->setCenterPoint(centerPoint);

      // Check Right Handle
      rightHandlePos = calculateHandlePos(rightHandlePos, rightPivotPos,
                                          TPointD(0.0, 0.0), centerPoint);
      mainObj->setRightHandlePos(rightHandlePos);

      applyToSelection = false;
    } else if (m_isRightMoving) {
      TPointD newPos = rightHandlePos + dPos;
      mainObj->setRightHandlePos(newPos);
      // Also move Center Point
      TPointD newCenterPoint;
      if (mainObj->isHorizon() && e.isAltPressed()) {
        TPointD otherHorizonPoint =
            calculateHorizonPoint(centerPoint, mainObj->getRotation(), 100);
        newCenterPoint = calculateCenterPoint(centerPoint, otherHorizonPoint,
                                              newPos, rightPivotPos);
      } else
        newCenterPoint = calculateCenterPoint(leftHandlePos, leftPivotPos,
                                              newPos, rightPivotPos);
      if (!std::isnan(newCenterPoint.x) && !std::isnan(newCenterPoint.y))
        centerPoint = newCenterPoint;
      mainObj->setCenterPoint(centerPoint);

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
    std::set<int> selectedObjects = m_selection.getSelectedObjects();
    std::set<int>::iterator it;
    for (it = selectedObjects.begin(); it != selectedObjects.end(); it++) {
      PerspectiveObject *obj = m_perspectiveObjs[*it];
      if (m_isShifting)
        obj->shiftPerspectiveObject(dPos);
      else if (m_isCenterMoving) {  // Moving
        TPointD objCenterPoint = obj->getCenterPoint();

        if (obj->isHorizon() && e.isAltPressed()) {
          double distance =
              std::sqrt(std::pow(dPos.x, 2) + std::pow(dPos.y, 2));
          double rotation = mainObj->getRotation();
          if (rotation != 90 && rotation != 270) {
            if (dPos.x < 0) distance *= -1;
            if (rotation > 90 && rotation < 270) distance *= -1;
          } else if ((rotation == 90 && dPos.y < 0) ||
                     (rotation == 270 && dPos.y > 0))
            distance *= -1;
          TPointD newCenter =
              calculateHorizonPoint(objCenterPoint, rotation, distance);
          dPos = newCenter - objCenterPoint;
        }

        obj->setCenterPoint(objCenterPoint + dPos);

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
        double rotation = obj->getRotation();
        obj->setRotation(rotation + dAngle);

        TPointD rotationPos = obj->getRotationPos();
        obj->setRotationPos(rotationPos + dPos);
      } else if (m_isSpacing) {
        double spacing = obj->getSpacing();
        obj->setSpacing(spacing + dSpace);

        TPointD spacingPos = obj->getSpacingPos();
        obj->setSpacingPos(spacingPos + dPos);
      }
    }
  }

  m_modified = true;
  m_firstPos = pos;

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
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_mousePos = pos;

  if (!m_perspectiveObjs.size()) return;

  for (int i = 0; i < m_perspectiveObjs.size(); i++)
    m_perspectiveObjs[i]->setCursorPos(pos);
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

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    m_lastPreset = copyPerspectiveSet(m_perspectiveObjs);
    loadLastPreset();

    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

  m_undo = new PerspectiveObjectUndo(m_perspectiveObjs, this);

  std::set<int> selectedObjects = m_selection.getSelectedObjects();

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
}

//----------------------------------------------------------------------------------------------

std::vector<PerspectiveObject *> PerspectiveTool::copyPerspectiveSet(
    std::vector<PerspectiveObject *> perspectiveSet) {
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

    copy.push_back(newObject);
  }

  return copy;
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    m_presetsManager.load(ToonzFolder::getMyModuleDir() +
                          "perspective_grids.txt");
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

  invalidate();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::addPreset(QString name) {
  PerspectivePreset preset(name.toStdWString());  // , m_perspectiveObjs);

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

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  loadLastPreset();
}

//----------------------------------------------------------------------------------------------

void PerspectiveTool::loadLastPreset() {
  m_perspectiveObjs = m_lastPreset;

  invalidate();
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
  double totalDistance =
      std::sqrt(std::pow(xDistance, 2) + std::pow(yDistance, 2));

  double step            = getSpacing();
  int rays               = isHorizon() ? 360 : (360 / step);
  if (rays == 0) rays    = 1;
  if (!isHorizon()) step = 360.0 / (double)rays;

  double startAngle = 270.0 + getRotation();
  double endAngle   = (isHorizon() ? 180.0 : 90.0) + getRotation();

  // Draw starting angle
  double yLength = std::sin(startAngle * (3.14159 / 180)) * totalDistance;
  double xLength = std::cos(startAngle * (3.14159 / 180)) * totalDistance;
  end.x          = p.x + xLength;
  end.y          = p.y + yLength;
  tglDrawSegment(p, end);

  int raySpacing = 0;
  if (isHorizon()) {
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
  for (i = 2; i <= rays; i++) {
    bool drawRight = true;

    if (isHorizon()) {
      double angle =
          std::atan(-totalDistance / (double)(raySpacing * (i - 1))) /
          (3.14159 / 180);
      rAngle = getRotation() + 360 + angle;
      lAngle = getRotation() + 180 - angle;
    } else {
      rAngle += step;
      lAngle -= step;

      // No need to draw right side if left side already drawn
      int e = endAngle * 1000;
      int l = lAngle * 1000.0;
      int r = (rAngle - 360.0) * 1000.0;
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
    tglDrawSegment(p, end);

    // When overlaps, no need to draw right side if left side already drawn
    if (!drawRight) break;

    // Draw right of startAngle
    yLength = std::sin(rAngle * (3.14159 / 180)) * totalDistance;
    xLength = std::cos(rAngle * (3.14159 / 180)) * totalDistance;
    end.x   = p.x + xLength;
    end.y   = p.y + yLength;
    tglDrawSegment(p, end);
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
  double totalDistance =
      std::sqrt(std::pow(xDistance, 2) + std::pow(yDistance, 2));

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
    TPointD above    = p;
    TPointD below    = p;
    double distance  = 0.0;
    double step      = getSpacing();
    double stepspeed = isHorizon() ? 1.5 : 1.0;

    double distanceFromCP = 0.0;

    while (std::abs(distanceFromCP) <= totalDistance) {
      distance += (step * stepspeed);
      step = step * stepspeed;

      distanceFromCP = distance / Stage::standardDpi * Stage::inch;

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
