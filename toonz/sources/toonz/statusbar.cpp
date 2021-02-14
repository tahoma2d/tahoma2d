#include "statusbar.h"

#include "tapp.h"

#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tobjecthandle.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"
#include "toonz/tstageobjecttree.h"

#include "tools/tool.h"

#include "tools/toolhandle.h"

#include <QLayout>
#include <QLabel>

StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent) {
  setObjectName("StatusBar");
  m_currentFrameLabel = new StatusLabel(tr("Level: 1   Frame: 1"), this);
  m_currentFrameLabel->setObjectName("MainWindowPlainLabel");

  m_infoLabel = new StatusLabel(tr("Info goes here."), this);
  m_infoLabel->setObjectName("MainWindowPlainLabel");
  m_infoLabel->setMinimumWidth(1);
  addWidget(m_infoLabel, 0);
  addPermanentWidget(m_currentFrameLabel, 0);

  TApp* app                    = TApp::instance();
  TFrameHandle* frameHandle    = app->getCurrentFrame();
  TXshLevelHandle* levelHandle = app->getCurrentLevel();
  TObjectHandle* object        = app->getCurrentObject();

  bool ret = true;

  ret = ret && connect(frameHandle, SIGNAL(frameTypeChanged()), this,
                       SLOT(updateInfoText()));
  ret = ret && connect(app->getCurrentTool(), SIGNAL(toolSwitched()), this,
                       SLOT(updateInfoText()));
  ret = ret && connect(levelHandle, SIGNAL(xshLevelChanged()), this,
                       SLOT(updateInfoText()));
  ret = ret &&
        connect(object, SIGNAL(objectSwitched()), this, SLOT(updateInfoText()));

  assert(ret);

  makeMap();
  updateInfoText();
}

//-----------------------------------------------------------------------------

StatusBar::~StatusBar() {}

//-----------------------------------------------------------------------------

void StatusBar::showEvent(QShowEvent* event) {}

//-----------------------------------------------------------------------------

void StatusBar::updateFrameText(QString text) {
  m_currentFrameLabel->setText(text);
}

//-----------------------------------------------------------------------------

void StatusBar::updateInfoText() {
  TApp* app              = TApp::instance();
  ToolHandle* toolHandle = app->getCurrentTool();
  TTool* tool            = toolHandle->getTool();
  TObjectHandle* object  = app->getCurrentObject();
  TStageObjectTree* tree =
      app->getCurrentXsheet()->getXsheet()->getStageObjectTree();
  if (object->isSpline()) {
    m_infoLabel->setText(
        tr("Motion Path Selected: Click on a level or frame to leave motion "
           "path editing."));
    return;
  }
  std::string name = tool->getName();
  tool->getToolType();
  int target = tool->getTargetType();

  int type                     = -1;
  TXshLevelHandle* levelHandle = app->getCurrentLevel();
  TXshLevel* level             = levelHandle->getLevel();
  if (level) {
    type = level->getType();
  }
  bool isRaster        = false;
  bool isVector        = false;
  bool isSmartRaster   = false;
  bool isEmpty         = false;
  std::string namePlus = "";
  if (type >= 0) {
    if (type == TXshLevelType::PLI_XSHLEVEL) {
      isVector = true;
      namePlus = "Vector";
    } else if (type == TXshLevelType::TZP_XSHLEVEL) {
      isSmartRaster = true;
      namePlus      = "SmartRaster";
    } else if (type == TXshLevelType::OVL_XSHLEVEL) {
      isRaster = true;
      namePlus = "Raster";
    } else if (type == NO_XSHLEVEL)
      isEmpty = true;
  }
  QString text = "";
  if (m_infoMap.find(name + namePlus) != m_infoMap.end()) {
    text += m_infoMap[name + namePlus];
    int i = 0;
    i++;
  } else if (m_infoMap.find(name) != m_infoMap.end()) {
    text += m_infoMap[name];
    int i = 0;
    i++;
  }

  m_infoLabel->setText(text);
}

//-----------------------------------------------------------------------------

void StatusBar::makeMap() {
  QString spacer = "                    ";
  // tools
  m_infoMap.insert({"T_Hand", "<b>Hand Tool:</b> Pans the workspace"});
  m_infoMap.insert(
      {"T_Selection",
       "Selection Tool: Select parts of your image to transform it."});
  m_infoMap.insert({"T_Edit",
                    "Animate Tool: Modifies the position, rotation and size of "
                    "the current column"});
  m_infoMap.insert({"T_Brush", "Brush Tool: Draws in the work area freehand"});
  m_infoMap.insert(
      {"T_BrushVector", "Brush Tool: Draws in the work area freehand" + spacer +
                            "Shift - Straight Lines" + spacer +
#ifdef MACOSX
                            "Cmd - Straight Lines Snapped to Angles" + spacer +
                            "Cmd + Opt - Add / Remove Vanishing Point" +
                            spacer + "Opt - Draw to Vanishing Point" + spacer +
                            "Hold Cmd + Shift - Toggle Snapping"});
#else
                            "Control - Straight Lines Snapped to Angles" +
                            spacer +
                            "Ctrl + Alt - Add / Remove Vanishing Point" +
                            spacer + "Alt - Draw to Vanishing Point" + spacer +
                            "Hold Ctrl + Shift - Toggle Snapping"});
#endif
  m_infoMap.insert({"T_BrushSmartRaster",
                    "Brush Tool: Draws in the work area freehand" + spacer +
                        "Shift - Straight Lines" + spacer +
#ifdef MACOSX
                        "Cmd - Straight Lines Snapped to Angles" + spacer +
                        "Cmd + Opt - Add / Remove Vanishing Point" + spacer +
                        "Opt - Draw to Vanishing Point"});
#else
                        "Control - Straight Lines Snapped to Angles" + spacer +
                        "Ctrl + Alt - Add / Remove Vanishing Point" + spacer +
                        "Alt - Draw to Vanishing Point"});
#endif
  m_infoMap.insert(
      {"T_BrushRaster", "Brush Tool: Draws in the work area freehand" + spacer +
                            "Shift - Straight Lines" + spacer +
#ifdef MACOSX
                            "Cmd - Straight Lines Snapped to Angles" + spacer +
                            "Cmd + Opt - Add / Remove Vanishing Point" +
                            spacer + "Opt - Draw to Vanishing Point"});
#else
                            "Control - Straight Lines Snapped to Angles" +
                            spacer +
                            "Ctrl + Alt - Add / Remove Vanishing Point" +
                            spacer + "Alt - Draw to Vanishing Point"});
#endif
  m_infoMap.insert({"T_Geometric", "Geometry Tool: Draws geometric shapes"});
  m_infoMap.insert(
      {"T_GeometricVector", "Geometry Tool: Draws geometric shapes" + spacer +
#ifdef MACOSX
                                "Hold Cmd + Shift - Toggle Snapping"});
#else
                                "Hold Ctrl + Shift - Toggle Snapping"});
#endif
  m_infoMap.insert({"T_Type", "Type Tool: Adds text"});
  m_infoMap.insert(
      {"T_PaintBrush",
       "Smart Raster Painter: Paints areas in Smart Raster leves"});
  m_infoMap.insert(
      {"T_Fill", "Fill Tool: Fills drawing areas with the current style"});
  m_infoMap.insert({"T_Eraser", "Eraser: Erases lines and areas"});
  m_infoMap.insert(
      {"T_Tape", "Tape Tool: Closes gaps in raster, joins edges in vector"});
  m_infoMap.insert(
      {"T_StylePicker", "Style Picker: Selects style on current drawing"});
  m_infoMap.insert(
      {"T_RGBPicker",
       "RGB Picker: Picks color on screen and applies to current style"});
  m_infoMap.insert({"T_ControlPointEditor",
                    "Control Point Editor: Modifies vector lines by editing "
                    "its control points"});
  m_infoMap.insert({"T_Pinch", "Pinch Tool: Pulls vector drawings"});
  m_infoMap.insert({"T_Pump", "Pump Tool: Changes vector thickness"});
  m_infoMap.insert({"T_Magnet", "Magnet Tool: Deforms vector lines"});
  m_infoMap.insert(
      {"T_Bender", "Bend Tool: Bends vector shapes around the first click"});
  m_infoMap.insert({"T_Iron", "Iron Tool: Smooths vector lines"});
  m_infoMap.insert({"T_Cutter", "Cutter Tool: Splits vector lines"});
  m_infoMap.insert({"T_Hook", ""});
  m_infoMap.insert({"T_Skeleton",
                    "Skeleton Tool: Allows to build a skeleton and animate in "
                    "a cut-out workflow"});
  m_infoMap.insert(
      {"T_Tracker",
       "Tracker: Tracks specific regions in a sequence of images"});
  m_infoMap.insert({"T_Plastic",
                    "Plastic Tool: Builds a mesh that allows to deform and "
                    "animate a level"});
  m_infoMap.insert({"T_Zoom", "Zoom Tool: Zooms viewer"});
  m_infoMap.insert({"T_Rotate", "Rotate Tool: Rotate the workspace"});
  m_infoMap.insert({"T_Ruler", ""});
  m_infoMap.insert(
      {"T_Finger", "Finger Tool: Smudges small areas to cover with line"});
  m_infoMap.insert({"T_Dummy", "This tool doesn't work on this layer type."});
}
