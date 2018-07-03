

#include "toolbar.h"
#include "tapp.h"
#include "pane.h"
#include "floatingpanelcommand.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"
#include "tools/toolcommandids.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"

#include "toonz/txshleveltypes.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

#include <QPainter>
#include <QAction>
#include <QToolButton>
#include <QVBoxLayout>
#include <QObject>

TEnv::IntVar ShowAllToolsToggle("ShowAllToolsToggle", 0);

namespace {
enum ActivateLevel {
  Empty       = 0x1,
  Raster      = 0x2,    //! OVL_XSHLEVEL
  Scan        = 0x4,    //! TZI_XSHLEVEL
  ToonzRaster = 0x8,    //! TZP_XSHLEVEL
  ToonzVector = 0x10,   //! PLI_XSHLEVEL
  Child       = 0x20,   //! CHILD_XSHLEVEL
  Mesh        = 0x40,   //! MESH_XSHLEVEL
  ZeraryFX    = 0x80,   //! ZERARYFX_XSHLEVEL
  Sound       = 0x100,  //! SND_XSHLEVEL
  SoundText   = 0x200,  //! SND_TXT_XSHLEVEL
  Palette     = 0x400,  //! PLT_XSHLEVEL

  Separator = 0x800,

  All = 0x1000,
};

struct {
  const char *toolName;
  bool collapsable;
  int displayLevels;
  QAction *action;
} buttonLayout[] = {
    {T_Edit, false,
     (ActivateLevel::Empty | ActivateLevel::Raster | ActivateLevel::Scan |
      ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector |
      ActivateLevel::Child | ActivateLevel::Mesh | ActivateLevel::ZeraryFX |
      ActivateLevel::Palette),
     0},
    {T_Selection, false,
     (ActivateLevel::Raster | ActivateLevel::Scan | ActivateLevel::ToonzRaster |
      ActivateLevel::ToonzVector),
     0},
    {"Separator_1", false, ActivateLevel::Separator, 0},
    {T_Brush, false,
     (ActivateLevel::Empty | ActivateLevel::Raster | ActivateLevel::Scan |
      ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector),
     0},
    {T_Geometric, false,
     (ActivateLevel::Empty | ActivateLevel::Raster | ActivateLevel::Scan |
      ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector),
     0},
    {T_Type, true, (ActivateLevel::Empty | ActivateLevel::ToonzRaster |
                    ActivateLevel::ToonzVector),
     0},
    {T_Fill, false, (ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector),
     0},
    {T_PaintBrush, false, ActivateLevel::ToonzRaster, 0},
    {"Separator_2", false, ActivateLevel::Separator, 0},
    {T_Eraser, false, (ActivateLevel::Raster | ActivateLevel::Scan |
                       ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector),
     0},
    {T_Tape, false, (ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector),
     0},
    {T_Finger, false, ActivateLevel::ToonzRaster, 0},
    {"Separator_3", false, ActivateLevel::Separator, 0},
    {T_StylePicker, false,
     (ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector), 0},
    {T_RGBPicker, false,
     (ActivateLevel::Raster | ActivateLevel::Scan | ActivateLevel::ToonzRaster |
      ActivateLevel::ToonzVector),
     0},
    {T_Ruler, false, ActivateLevel::All, 0},
    {"Separator_4", false, ActivateLevel::Separator, 0},
    {T_ControlPointEditor, false, ActivateLevel::ToonzVector, 0},
    {T_Pinch, true, ActivateLevel::ToonzVector, 0},
    {T_Pump, true, ActivateLevel::ToonzVector, 0},
    {T_Magnet, true, ActivateLevel::ToonzVector, 0},
    {T_Bender, true, ActivateLevel::ToonzVector, 0},
    {T_Iron, true, ActivateLevel::ToonzVector, 0},
    {T_Cutter, true, ActivateLevel::ToonzVector, 0},
    {"Separator_5", false, ActivateLevel::Separator, 0},
    {T_Skeleton, true,
     (ActivateLevel::Raster | ActivateLevel::Scan | ActivateLevel::ToonzRaster |
      ActivateLevel::ToonzVector | ActivateLevel::Child |
      ActivateLevel::ZeraryFX | ActivateLevel::Palette),
     0},
    {T_Tracker, true, (ActivateLevel::Raster | ActivateLevel::Scan |
                       ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector),
     0},
    {T_Hook, true, (ActivateLevel::Raster | ActivateLevel::Scan |
                    ActivateLevel::ToonzRaster | ActivateLevel::ToonzVector),
     0},
    {T_Plastic, true,
     (ActivateLevel::Raster | ActivateLevel::Scan | ActivateLevel::ToonzRaster |
      ActivateLevel::ToonzVector | ActivateLevel::Child | ActivateLevel::Mesh),
     0},
    {"Separator_6", false, ActivateLevel::Separator, 0},
    {T_Zoom, false, ActivateLevel::All, 0},
    {T_Rotate, true, ActivateLevel::All, 0},
    {T_Hand, false, ActivateLevel::All, 0},
    {0, false, 0}};
}
//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

Toolbar::Toolbar(QWidget *parent, bool isVertical)
    : QToolBar(parent), m_isExpanded(ShowAllToolsToggle != 0) {
  // Fondamentale per lo style sheet
  setObjectName("toolBar");

  setMovable(false);
  if (isVertical)
    setOrientation(Qt::Vertical);
  else
    setOrientation(Qt::Horizontal);

  setIconSize(QSize(23, 23));
  setToolButtonStyle(Qt::ToolButtonIconOnly);

  m_expandButton = new QToolButton(this);
  m_expandButton->setObjectName("expandButton");
  m_expandButton->setCheckable(true);
  m_expandButton->setChecked(m_isExpanded);
  m_expandButton->setArrowType((isVertical) ? Qt::DownArrow : Qt::RightArrow);

  m_expandAction = addWidget(m_expandButton);

  connect(m_expandButton, SIGNAL(toggled(bool)), this,
          SLOT(setIsExpanded(bool)));

  updateToolbar();
}

//-----------------------------------------------------------------------------
/*! Layout the tool buttons according to the state of the expandButton
*/
void Toolbar::updateToolbar() {
  TApp *app                  = TApp::instance();
  TXshLevelHandle *currlevel = app->getCurrentLevel();
  TXshLevel *level           = currlevel ? currlevel->getLevel() : 0;
  int levelType              = level ? level->getType() : NO_XSHLEVEL;

  // If in an empty cell, find most recent level
  if (levelType == NO_XSHLEVEL) {
    TXsheetHandle *xshHandle = app->getCurrentXsheet();
    TXsheet *xsh             = xshHandle->getXsheet();
    TColumnHandle *colHandle = app->getCurrentColumn();
    int index                = colHandle->getColumnIndex();

    if (index >= 0 && !xsh->isColumnEmpty(index)) {
      int r0, r1;
      xsh->getCellRange(index, r0, r1);
      if (0 <= r0 && r0 <= r1) {
        TFrameHandle *frameHandle = app->getCurrentFrame();
        int currentFrame          = frameHandle->getFrameIndex();
        // level type depends on previous occupied cell
        for (int r = min(r1, currentFrame); r >= r0; r--) {
          TXshCell cell = xsh->getCell(r, index);
          if (cell.isEmpty()) continue;
          levelType = cell.m_level->getType();
          break;
        }
      }
    }
  }

  if (levelType == m_toolbarLevel) return;

  m_toolbarLevel = levelType;

  // Hide action for now
  for (int idx = 0; buttonLayout[idx].toolName; idx++) {
    if (buttonLayout[idx].action) removeAction(buttonLayout[idx].action);
  }

  removeAction(m_expandAction);

  bool showLevelBased = Preferences::instance()->isShowLevelBasedToolsEnabled();

  bool actionEnabled = false;
  for (int idx = 0; buttonLayout[idx].toolName; idx++) {
    if (!buttonLayout[idx].action) {
      if (buttonLayout[idx].displayLevels & ActivateLevel::Separator)
        buttonLayout[idx].action = addSeparator();
      else
        buttonLayout[idx].action =
            CommandManager::instance()->getAction(buttonLayout[idx].toolName);
    }

    // Unhide if it meets the criteria for showing
    bool enable = false;

    if (!showLevelBased || buttonLayout[idx].displayLevels & ActivateLevel::All)
      enable = true;
    else if (buttonLayout[idx].displayLevels & ActivateLevel::Separator)
      enable = actionEnabled;
    else {
      switch (levelType) {
      case OVL_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::Raster);
        break;
      case TZI_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::Scan);
        break;
      case CHILD_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::Child);
        break;
      case PLI_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::ToonzVector);
        break;
      case TZP_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::ToonzRaster);
        break;
      case MESH_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::Mesh);
        break;
      case ZERARYFX_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::ZeraryFX);
        break;
      case SND_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::Sound);
        break;
      case SND_TXT_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::SoundText);
        break;
      case PLT_XSHLEVEL:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::Palette);
        break;
      case NO_XSHLEVEL:
      default:
        enable = (buttonLayout[idx].displayLevels & ActivateLevel::Empty);
        break;
      }
    }

    if (!enable || (!m_isExpanded && buttonLayout[idx].collapsable)) continue;

    actionEnabled = addAction(buttonLayout[idx].action) || actionEnabled;

    if (buttonLayout[idx].displayLevels & ActivateLevel::Separator)
      actionEnabled = false;
  }

  addAction(m_expandAction);

  if (m_isExpanded) {
    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::UpArrow : Qt::LeftArrow);
    m_expandButton->setToolTip(tr("Collapse toolbar"));
  } else {
    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::DownArrow : Qt::RightArrow);
    m_expandButton->setToolTip(tr("Expand toolbar"));
  }

  update();
}

//----------------------------------------------------------------------------

void Toolbar::setIsExpanded(bool expand) {
  m_isExpanded       = expand;
  m_toolbarLevel     = -1;  // Force toolbar to refresh
  ShowAllToolsToggle = (expand) ? 1 : 0;
  updateToolbar();
}

//-----------------------------------------------------------------------------

Toolbar::~Toolbar() {}

//-----------------------------------------------------------------------------

bool Toolbar::addAction(QAction *act) {
  if (!act) return false;
  QToolBar::addAction(act);
  return true;
}

//-----------------------------------------------------------------------------

void Toolbar::showEvent(QShowEvent *e) {
  TColumnHandle *columnHandle = TApp::instance()->getCurrentColumn();
  connect(columnHandle, SIGNAL(columnIndexSwitched()), this,
          SLOT(updateToolbar()));

  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
  connect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(onFrameSwitched()));

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  connect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(onXsheetChanged()));

  connect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()),
          SLOT(onToolChanged()));
}

void Toolbar::onFrameSwitched() {
  TFrameHandle *frameHandle = TApp::instance()->getCurrentFrame();
  if (frameHandle->isPlaying()) return;
  updateToolbar();
}

void Toolbar::onXsheetChanged() { updateToolbar(); }

//-----------------------------------------------------------------------------

void Toolbar::hideEvent(QHideEvent *e) {
  disconnect(TApp::instance()->getCurrentLevel(), 0, this, 0);
  disconnect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
             SLOT(onToolChanged()));
}

//-----------------------------------------------------------------------------

void Toolbar::onToolChanged() {
  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();
  TTool *tool            = toolHandle->getTool();
  std::string toolName   = tool->getName();
  QAction *act = CommandManager::instance()->getAction(toolName.c_str());
  if (!act || act->isChecked()) return;
  act->setChecked(true);
}

//=============================================================================

OpenFloatingPanel openToolbarPane(MI_OpenToolbar, "ToolBar", "");
