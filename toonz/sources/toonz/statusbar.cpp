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

#include "tproperty.h"

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

  m_infoMap = makeMap(tr("              "), tr(" - "), tr(" - "));
  m_hintMap = makeMap(tr("\n    "), tr("\t- "), tr("\t\t- "));
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
  bool isRaster         = false;
  bool isVector         = false;
  bool isSmartRaster    = false;
  std::string nameLevel = "";
  std::string nameMode  = "";
  if (type >= 0) {
    if (type == TXshLevelType::PLI_XSHLEVEL) {
      isVector  = true;
      nameLevel = "Vector";
    } else if (type == TXshLevelType::TZP_XSHLEVEL) {
      isSmartRaster = true;
      nameLevel     = "SmartRaster";
    } else if (type == TXshLevelType::OVL_XSHLEVEL) {
      isRaster  = true;
      nameLevel = "Raster";
    }
  }

  if (name == "T_Geometric") {
    TPropertyGroup* props = tool->getProperties(0);
    nameMode              = props->getProperty("Shape:")->getValueAsString();
  }

  QString text = "";
  if (m_infoMap.find(name + nameLevel + nameMode) != m_infoMap.end())
    text += m_infoMap[name + nameLevel + nameMode];
  else if (m_infoMap.find(name + nameLevel) != m_infoMap.end())
    text += m_infoMap[name + nameLevel];
  else if (m_infoMap.find(name + nameMode) != m_infoMap.end())
    text += m_infoMap[name + nameMode];
  else if (m_infoMap.find(name) != m_infoMap.end())
    text += m_infoMap[name];

  QString hintText = "";
  if (m_hintMap.find(name + nameLevel + nameMode) != m_hintMap.end())
    hintText += m_hintMap[name + nameLevel + nameMode];
  else if (m_hintMap.find(name + nameLevel) != m_hintMap.end())
    hintText += m_hintMap[name + nameLevel];
  else if (m_hintMap.find(name + nameMode) != m_hintMap.end())
    hintText += m_hintMap[name + nameMode];
  else if (m_hintMap.find(name) != m_hintMap.end())
    hintText += m_hintMap[name];

  m_infoLabel->setToolTip(hintText);
  m_infoLabel->setText(text);
}

//-----------------------------------------------------------------------------

std::unordered_map<std::string, QString> StatusBar::makeMap(
    QString spacer, QString cmdTextSeparator, QString cmd2TextSeparator) {
  std::unordered_map<std::string, QString> lMap;

#ifdef MACOSX
  // on macOS, we display symbols. No need to space differently
  cmd2TextSeparator = cmdTextSeparator;
#endif

  // tools
  lMap.insert({"T_Hand", tr("Hand Tool: Pans the workspace")});
  lMap.insert(
      {"T_Selection",
       tr("Selection Tool: Select parts of your image to transform it") +
           spacer +
           tr("%1%2Scale / Directional scale")
               .arg(trModKey("Shift"))
               .arg(cmd2TextSeparator) +
           spacer +
           tr("%1%2Distort / Shear")
               .arg(trModKey("Ctrl"))
               .arg(cmd2TextSeparator) +
           spacer +
           tr("%1%2Scale Symmetrically from Center")
               .arg(trModKey("Alt"))
               .arg(cmd2TextSeparator) +
           spacer +
           tr("%1%2Scale Symmetrically from Center w/ Proportion Lock")
               .arg(trModKey("Shift+Alt"))
               .arg(cmdTextSeparator)});
  lMap.insert({"T_Edit", tr("Animate Tool: Modifies the position, "
                            "rotation and size of the current column")});
  lMap.insert({"T_Brush", tr("Brush Tool: Draws in the work area freehand")});
  lMap.insert({"T_BrushVector",
               tr("Brush Tool : Draws in the work area freehand") + spacer +
                   tr("%1%2Straight Lines")
                       .arg(trModKey("Shift"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Straight Lines Snapped to Angles")
                       .arg(trModKey("Ctrl"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Add / Remove Vanishing Point")
                       .arg(trModKey("Ctrl+Alt"))
                       .arg(cmdTextSeparator) +
                   spacer +
                   tr("%1%2Draw to Vanishing Point")
                       .arg(trModKey("Alt"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Allow or Disallow Snapping")
                       .arg(trModKey("Ctrl+Shift"))
                       .arg(cmdTextSeparator)});
  lMap.insert({"T_BrushSmartRaster",
               tr("Brush Tool : Draws in the work area freehand") + spacer +
                   tr("%1%2Straight Lines")
                       .arg(trModKey("Shift"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Straight Lines Snapped to Angles")
                       .arg(trModKey("Ctrl"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Add / Remove Vanishing Point")
                       .arg(trModKey("Ctrl+Alt"))
                       .arg(cmdTextSeparator) +
                   spacer +
                   tr("%1%2Draw to Vanishing Point")
                       .arg(trModKey("Alt"))
                       .arg(cmd2TextSeparator)});
  lMap.insert({"T_BrushRaster",
               tr("Brush Tool : Draws in the work area freehand") + spacer +
                   tr("%1%2Straight Lines")
                       .arg(trModKey("Shift"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Straight Lines Snapped to Angles")
                       .arg(trModKey("Ctrl"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Add / Remove Vanishing Point")
                       .arg(trModKey("Ctrl+Alt"))
                       .arg(cmdTextSeparator) +
                   spacer +
                   tr("%1%2Draw to Vanishing Point")
                       .arg(trModKey("Alt"))
                       .arg(cmd2TextSeparator)});
  lMap.insert({"T_Geometric", tr("Geometry Tool: Draws geometric shapes")});
  lMap.insert({"T_GeometricRectangle",
               tr("Geometry Tool: Draws geometric shapes") + spacer +
                   tr("%1%2Proportion Lock")
                       .arg(trModKey("Shift"))
                       .arg(cmdTextSeparator) +
                   spacer +
                   tr("%1%2Create From Center")
                       .arg(trModKey("Alt"))
                       .arg(cmdTextSeparator)});
  lMap.insert({"T_GeometricEllipse",
               tr("Geometry Tool: Draws geometric shapes") + spacer +
                   tr("%1%2Proportion Lock")
                       .arg(trModKey("Shift"))
                       .arg(cmdTextSeparator) +
                   spacer +
                   tr("%1%2Create From Center")
                       .arg(trModKey("Alt"))
                       .arg(cmdTextSeparator)});
  lMap.insert(
      {"T_GeometricPolyline",
       tr("Geometry Tool: Draws geometric shapes") + spacer +
           tr("%1%2Create Curve").arg(tr("Click+Drag")).arg(cmdTextSeparator) +
           spacer +
           tr("%1%2Return to Straight Line")
               .arg(trModKey("Ctrl"))
               .arg(cmd2TextSeparator) +
           spacer +
           tr("%1%2Snap to Angle")
               .arg(trModKey("Shift"))
               .arg(cmd2TextSeparator)});
  lMap.insert({"T_GeometricVector",
               tr("Geometry Tool: Draws geometric shapes") + spacer +
                   tr("%1%2Allow or Disallow Snapping")
                       .arg(trModKey("Ctrl+Shift"))
                       .arg(cmdTextSeparator)});
  lMap.insert({"T_GeometricVectorRectangle",
               tr("Geometry Tool: Draws geometric shapes") + spacer +
                   tr("%1%2Proportion Lock")
                       .arg(trModKey("Shift"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Create From Center")
                       .arg(trModKey("Alt"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Allow or Disallow Snapping")
                       .arg(trModKey("Ctrl+Shift"))
                       .arg(cmdTextSeparator)});
  lMap.insert({"T_GeometricVectorEllipse",
               tr("Geometry Tool: Draws geometric shapes") + spacer +
                   tr("%1%2Proportion Lock")
                       .arg(trModKey("Shift"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Create From Center")
                       .arg(trModKey("Alt"))
                       .arg(cmd2TextSeparator) +
                   spacer +
                   tr("%1%2Allow or Disallow Snapping")
                       .arg(trModKey("Ctrl+Shift"))
                       .arg(cmdTextSeparator)});
  lMap.insert(
      {"T_GeometricVectorPolyline",
       tr("Geometry Tool: Draws geometric shapes") + spacer +
           tr("%1%2Create Curve").arg(tr("Click+Drag")).arg(cmdTextSeparator) +
           spacer +
           tr("%1%2Return to Straight Line")
               .arg(trModKey("Ctrl"))
               .arg(cmd2TextSeparator) +
           spacer +
           tr("%1%2Snap to Angle")
               .arg(trModKey("Shift"))
               .arg(cmd2TextSeparator) +
           spacer +
           tr("%1%2Allow or Disallow Snapping")
               .arg(trModKey("Ctrl+Shift"))
               .arg(cmdTextSeparator)});
  lMap.insert({"T_Type", tr("Type Tool: Adds text")});
  lMap.insert({"T_PaintBrush",
               tr("Smart Raster Painter: Paints areas in Smart Raster levels") +
                   spacer +
                   tr("%1%2Fix small fill gaps with click+dragged style")
                       .arg(trModKey("Ctrl"))
                       .arg(cmdTextSeparator) +
                   spacer +
                   tr("%1%2Selects style on current drawing")
                       .arg(trModKey("Shift"))
                       .arg(cmdTextSeparator)});
  lMap.insert(
      {"T_Fill", tr("Fill Tool: Fills drawing areas with the current style")});
  lMap.insert({"T_Eraser", tr("Eraser: Erases lines and areas")});
  lMap.insert({"T_Tape",
               tr("Tape Tool: Closes gaps in raster, joins edges in vector")});
  lMap.insert(
      {"T_StylePicker", tr("Style Picker: Selects style on current drawing")});
  lMap.insert(
      {"T_RGBPicker",
       tr("RGB Picker: Picks color on screen and applies to current style")});
  lMap.insert(
      {"T_ControlPointEditor", tr("Control Point Editor: Modifies vector lines "
                                  "by editing its control points")});
  lMap.insert({"T_Pinch", tr("Pinch Tool: Pulls vector drawings")});
  lMap.insert({"T_Pump", tr("Pump Tool: Changes vector thickness")});
  lMap.insert({"T_Magnet", tr("Magnet Tool: Deforms vector lines")});
  lMap.insert({"T_Bender",
               tr("Bend Tool: Bends vector shapes around the first click")});
  lMap.insert({"T_Iron", tr("Iron Tool: Smooths vector lines")});
  lMap.insert({"T_Cutter", tr("Cutter Tool: Splits vector lines")});
  lMap.insert({"T_Hook", ""});
  lMap.insert(
      {"T_Skeleton", tr("Skeleton Tool: Allows to build a skeleton and animate "
                        "in a cut-out workflow")});
  lMap.insert({"T_Tracker",
               tr("Tracker: Tracks specific regions in a sequence of images")});
  lMap.insert({"T_Plastic", tr("Plastic Tool: Builds a mesh that allows "
                               "to deform and animate a level")});
  lMap.insert({"T_Zoom", tr("Zoom Tool: Zooms viewer")});
  lMap.insert({"T_Rotate", tr("Rotate Tool: Rotate the workspace")});
  lMap.insert({"T_Ruler", tr("Ruler Tool: Measures distances on the canvas")});
  lMap.insert(
      {"T_Finger", tr("Finger Tool: Smudges small areas to cover with line")});
  lMap.insert({"T_Dummy", tr("This tool doesn't work on this layer type.")});

  return lMap;
}
