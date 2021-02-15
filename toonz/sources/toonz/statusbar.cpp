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
#include "toonzqt/gutil.h"

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
  addWidget(m_infoLabel, 1);
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

  m_infoLabel->setToolTip(text);
  m_infoLabel->setText(text);
}

//-----------------------------------------------------------------------------
QString trModKey(QString key) {
#ifdef MACOSX
  // Convert Windows key modifier to macOS modifier
  if (key == "Ctrl")  // Command
    return QString::fromStdWString(L"\u2318");
  else if (key == "Shift")
    return QString::fromStdWString(L"\u21e7");
  else if (key == "Alt")
    return QString::fromStdWString(L"\u2325");
//  else if (key == "???") // Control
//    return QString::fromStdWString(L"\u2303");
#endif

  return key;
}

void StatusBar::makeMap() {
  QString spacer = tr("                    ");
  // tools
  m_infoMap.insert({"T_Hand", tr("Hand Tool: Pans the workspace")});
  m_infoMap.insert(
      {"T_Selection",
       tr("Selection Tool: Select parts of your image to transform it") +
           spacer +
           tr("%1 - Scale / Directional scale").arg(trModKey("Shift")) +
           spacer + tr("%1 - Distort / Shear").arg(trModKey("Ctrl")) + spacer +
           tr("%1 - Scale symmetrically from center point")
               .arg(trModKey("Alt"))});
  m_infoMap.insert({"T_Edit", tr("Animate Tool: Modifies the position, "
                                 "rotation and size of the current column")});
  m_infoMap.insert(
      {"T_Brush", tr("Brush Tool: Draws in the work area freehand")});
  m_infoMap.insert(
      {"T_BrushVector",
       tr("Brush Tool : Draws in the work area freehand") + spacer +
           tr("%1 - Straight Lines").arg(trModKey("Shift")) + spacer +
           tr("%1 - Straight Lines Snapped to Angles").arg(trModKey("Ctrl")) +
           spacer +
           tr("%1 - Add / Remove Vanishing Point")
               .arg(trModKey("Ctrl") + "+" + trModKey("Alt")) +
           spacer + tr("%1 - Draw to Vanishing Point").arg(trModKey("Alt")) +
           spacer +
           tr("Hold %1 - Toggle Snapping")
               .arg(trModKey("Ctrl") + "+" + trModKey("Shift"))});
  m_infoMap.insert(
      {"T_BrushSmartRaster",
       tr("Brush Tool : Draws in the work area freehand") + spacer +
           tr("%1 - Straight Lines").arg(trModKey("Shift")) + spacer +
           tr("%1 - Straight Lines Snapped to Angles").arg(trModKey("Ctrl")) +
           spacer +
           tr("%1 - Add / Remove Vanishing Point")
               .arg(trModKey("Ctrl") + "+" + trModKey("Alt")) +
           spacer + tr("%1 - Draw to Vanishing Point").arg(trModKey("Alt"))});
  m_infoMap.insert(
      {"T_BrushRaster",
       tr("Brush Tool : Draws in the work area freehand") + spacer +
           tr("%1 - Straight Lines").arg(trModKey("Shift")) + spacer +
           tr("%1 - Straight Lines Snapped to Angles").arg(trModKey("Ctrl")) +
           spacer +
           tr("%1 - Add / Remove Vanishing Point")
               .arg(trModKey("Ctrl") + "+" + trModKey("Alt")) +
           spacer + tr("%1 - Draw to Vanishing Point").arg(trModKey("Alt"))});
  m_infoMap.insert(
      {"T_Geometric", tr("Geometry Tool: Draws geometric shapes")});
  m_infoMap.insert({"T_GeometricVector",
                    tr("Geometry Tool: Draws geometric shapes") + spacer +
                        tr("Hold %1 - Toggle Snapping")
                            .arg(trModKey("Ctrl") + "+" + trModKey("Shift"))});
  m_infoMap.insert({"T_Type", tr("Type Tool: Adds text")});
  m_infoMap.insert(
      {"T_PaintBrush",
       tr("Smart Raster Painter: Paints areas in Smart Raster leves")});
  m_infoMap.insert(
      {"T_Fill", tr("Fill Tool: Fills drawing areas with the current style")});
  m_infoMap.insert({"T_Eraser", tr("Eraser: Erases lines and areas")});
  m_infoMap.insert(
      {"T_Tape",
       tr("Tape Tool: Closes gaps in raster, joins edges in vector")});
  m_infoMap.insert(
      {"T_StylePicker", tr("Style Picker: Selects style on current drawing")});
  m_infoMap.insert(
      {"T_RGBPicker",
       tr("RGB Picker: Picks color on screen and applies to current style")});
  m_infoMap.insert(
      {"T_ControlPointEditor", tr("Control Point Editor: Modifies vector lines "
                                  "by editing its control points")});
  m_infoMap.insert({"T_Pinch", tr("Pinch Tool: Pulls vector drawings")});
  m_infoMap.insert({"T_Pump", tr("Pump Tool: Changes vector thickness")});
  m_infoMap.insert({"T_Magnet", tr("Magnet Tool: Deforms vector lines")});
  m_infoMap.insert(
      {"T_Bender",
       tr("Bend Tool: Bends vector shapes around the first click")});
  m_infoMap.insert({"T_Iron", tr("Iron Tool: Smooths vector lines")});
  m_infoMap.insert({"T_Cutter", tr("Cutter Tool: Splits vector lines")});
  m_infoMap.insert({"T_Hook", ""});
  m_infoMap.insert(
      {"T_Skeleton", tr("Skeleton Tool: Allows to build a skeleton and animate "
                        "in a cut-out workflow")});
  m_infoMap.insert(
      {"T_Tracker",
       tr("Tracker: Tracks specific regions in a sequence of images")});
  m_infoMap.insert({"T_Plastic", tr("Plastic Tool: Builds a mesh that allows "
                                    "to deform and animate a level")});
  m_infoMap.insert({"T_Zoom", tr("Zoom Tool: Zooms viewer")});
  m_infoMap.insert({"T_Rotate", tr("Rotate Tool: Rotate the workspace")});
  m_infoMap.insert(
      {"T_Ruler", tr("Ruler Tool: Measures distances on the canvas")});
  m_infoMap.insert(
      {"T_Finger", tr("Finger Tool: Smudges small areas to cover with line")});
  m_infoMap.insert(
      {"T_Dummy", tr("This tool doesn't work on this layer type.")});
}
