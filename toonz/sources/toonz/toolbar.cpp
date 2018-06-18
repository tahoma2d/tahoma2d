

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

// TnzBase includes
#include "tenv.h"

#include <QPainter>
#include <QAction>
#include <QToolButton>
#include <QVBoxLayout>
#include <QObject>

TEnv::IntVar ShowAllToolsToggle("ShowAllToolsToggle", 0);

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

  connect(m_expandButton, SIGNAL(toggled(bool)), this,
          SLOT(setIsExpanded(bool)));

  QAction *expandAction = addWidget(m_expandButton);

  m_toolbarList[T_Edit] = CommandManager::instance()->getAction(T_Edit);
  m_toolbarList[T_Selection] =
      CommandManager::instance()->getAction(T_Selection);
  m_toolbarList["Separator_1"] = addSeparator();
  m_toolbarList[T_Brush]       = CommandManager::instance()->getAction(T_Brush);
  m_toolbarList[T_Geometric] =
      CommandManager::instance()->getAction(T_Geometric);
  m_toolbarList[T_Type] = CommandManager::instance()->getAction(T_Type);
  m_toolbarList[T_Fill] = CommandManager::instance()->getAction(T_Fill);
  m_toolbarList[T_PaintBrush] =
      CommandManager::instance()->getAction(T_PaintBrush);
  m_toolbarList["Separator_2"] = addSeparator();
  m_toolbarList[T_Eraser] = CommandManager::instance()->getAction(T_Eraser);
  m_toolbarList[T_Tape]   = CommandManager::instance()->getAction(T_Tape);
  m_toolbarList[T_Finger] = CommandManager::instance()->getAction(T_Finger);
  m_toolbarList["Separator_3"] = addSeparator();
  m_toolbarList[T_StylePicker] =
      CommandManager::instance()->getAction(T_StylePicker);
  m_toolbarList[T_RGBPicker] =
      CommandManager::instance()->getAction(T_RGBPicker);
  m_toolbarList[T_Ruler]       = CommandManager::instance()->getAction(T_Ruler);
  m_toolbarList["Separator_4"] = addSeparator();
  m_toolbarList[T_ControlPointEditor] =
      CommandManager::instance()->getAction(T_ControlPointEditor);
  m_toolbarList[T_Pinch]  = CommandManager::instance()->getAction(T_Pinch);
  m_toolbarList[T_Pump]   = CommandManager::instance()->getAction(T_Pump);
  m_toolbarList[T_Magnet] = CommandManager::instance()->getAction(T_Magnet);
  m_toolbarList[T_Bender] = CommandManager::instance()->getAction(T_Bender);
  m_toolbarList[T_Iron]   = CommandManager::instance()->getAction(T_Iron);
  m_toolbarList[T_Cutter] = CommandManager::instance()->getAction(T_Cutter);
  m_toolbarList["Separator_5"] = addSeparator();
  m_toolbarList[T_Skeleton] = CommandManager::instance()->getAction(T_Skeleton);
  m_toolbarList[T_Hook]     = CommandManager::instance()->getAction(T_Hook);
  m_toolbarList[T_Tracker]  = CommandManager::instance()->getAction(T_Tracker);
  m_toolbarList[T_Plastic]  = CommandManager::instance()->getAction(T_Plastic);
  m_toolbarList["Separator_6"] = addSeparator();
  m_toolbarList[T_Zoom]        = CommandManager::instance()->getAction(T_Zoom);
  m_toolbarList[T_Rotate]   = CommandManager::instance()->getAction(T_Rotate);
  m_toolbarList[T_Hand]     = CommandManager::instance()->getAction(T_Hand);
  m_toolbarList["Expander"] = expandAction;

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

  std::map<std::string, QAction *>::iterator it;
  for (it = m_toolbarList.begin(); it != m_toolbarList.end(); it++) {
    QAction *action = it->second;
    if (!action) continue;
    removeAction(action);
  }

  bool actionEnabled = false;

  if (levelType != SND_XSHLEVEL && levelType != SND_TXT_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_Edit]) || actionEnabled;

  if (levelType == OVL_XSHLEVEL || levelType == TZI_XSHLEVEL ||
      levelType == PLI_XSHLEVEL || levelType == TZP_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_Selection]) || actionEnabled;

  if (actionEnabled) addAction(m_toolbarList["Separator_1"]);
  actionEnabled = false;

  if (levelType == NO_XSHLEVEL || levelType == OVL_XSHLEVEL ||
      levelType == TZI_XSHLEVEL || levelType == PLI_XSHLEVEL ||
      levelType == TZP_XSHLEVEL) {
    actionEnabled = addAction(m_toolbarList[T_Brush]) || actionEnabled;
    actionEnabled = addAction(m_toolbarList[T_Geometric]) || actionEnabled;
    if (m_isExpanded)
      actionEnabled = addAction(m_toolbarList[T_Type]) || actionEnabled;
  }

  if (levelType == PLI_XSHLEVEL || levelType == TZP_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_Fill]) || actionEnabled;

  if (levelType == TZP_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_PaintBrush]) || actionEnabled;

  if (actionEnabled) addAction(m_toolbarList["Separator_2"]);
  actionEnabled = false;

  if (levelType == OVL_XSHLEVEL || levelType == TZI_XSHLEVEL ||
      levelType == PLI_XSHLEVEL || levelType == TZP_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_Eraser]) || actionEnabled;

  if (levelType == PLI_XSHLEVEL || levelType == TZP_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_Tape]) || actionEnabled;

  if (levelType == TZP_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_Finger]) || actionEnabled;

  if (actionEnabled) addAction(m_toolbarList["Separator_3"]);
  actionEnabled = false;

  if (levelType == OVL_XSHLEVEL || levelType == TZI_XSHLEVEL ||
      levelType == PLI_XSHLEVEL || levelType == TZP_XSHLEVEL) {
    actionEnabled = addAction(m_toolbarList[T_StylePicker]) || actionEnabled;
    actionEnabled = addAction(m_toolbarList[T_RGBPicker]) || actionEnabled;
  }

  actionEnabled = addAction(m_toolbarList[T_Ruler]) || actionEnabled;

  if (actionEnabled) addAction(m_toolbarList["Separator_4"]);
  actionEnabled = false;

  if (levelType == PLI_XSHLEVEL) {
    actionEnabled =
        addAction(m_toolbarList[T_ControlPointEditor]) || actionEnabled;
    if (m_isExpanded) {
      actionEnabled = addAction(m_toolbarList[T_Pinch]) || actionEnabled;
      actionEnabled = addAction(m_toolbarList[T_Pump]) || actionEnabled;
      actionEnabled = addAction(m_toolbarList[T_Magnet]) || actionEnabled;
      actionEnabled = addAction(m_toolbarList[T_Bender]) || actionEnabled;
      actionEnabled = addAction(m_toolbarList[T_Iron]) || actionEnabled;
      actionEnabled = addAction(m_toolbarList[T_Cutter]) || actionEnabled;
    }
  }

  if (actionEnabled) addAction(m_toolbarList["Separator_5"]);

  actionEnabled = false;

  if (m_isExpanded && levelType != NO_XSHLEVEL && levelType != MESH_XSHLEVEL &&
      levelType != SND_XSHLEVEL && levelType != SND_TXT_XSHLEVEL)
    actionEnabled = addAction(m_toolbarList[T_Skeleton]) || actionEnabled;

  if (m_isExpanded &&
      (levelType == OVL_XSHLEVEL || levelType == TZI_XSHLEVEL ||
       levelType == PLI_XSHLEVEL || levelType == TZP_XSHLEVEL)) {
    actionEnabled = addAction(m_toolbarList[T_Hook]) || actionEnabled;
    actionEnabled = addAction(m_toolbarList[T_Tracker]) || actionEnabled;
  }

  if (m_isExpanded &&
      (levelType == OVL_XSHLEVEL || levelType == TZI_XSHLEVEL ||
       levelType == CHILD_XSHLEVEL || levelType == PLI_XSHLEVEL ||
       levelType == TZP_XSHLEVEL || levelType == MESH_XSHLEVEL))
    actionEnabled = addAction(m_toolbarList[T_Plastic]) || actionEnabled;

  if (actionEnabled) addAction(m_toolbarList["Separator_6"]);

  actionEnabled = false;

  actionEnabled = addAction(m_toolbarList[T_Zoom]) || actionEnabled;
  if (m_isExpanded)
    actionEnabled = addAction(m_toolbarList[T_Rotate]) || actionEnabled;

  actionEnabled = addAction(m_toolbarList[T_Hand]) || actionEnabled;

  actionEnabled = addAction(m_toolbarList["Expander"]) || actionEnabled;

  if (m_isExpanded) {
    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::UpArrow : Qt::LeftArrow);
    m_expandButton->setToolTip(tr("Hide certain tools"));
  } else {
    m_expandButton->setArrowType(
        (orientation() == Qt::Vertical) ? Qt::DownArrow : Qt::RightArrow);
    m_expandButton->setToolTip(tr("Show all available tools"));
  }

  update();
}

//----------------------------------------------------------------------------

void Toolbar::setIsExpanded(bool expand) {
  m_isExpanded       = expand;
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
  TApp *app = TApp::instance();

  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();
  connect(levelHandle, SIGNAL(xshLevelChanged()), this, SLOT(updateToolbar()));
  connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
          SLOT(updateToolbar()));
  connect(levelHandle, SIGNAL(xshLevelViewChanged()), this,
          SLOT(updateToolbar()));

  connect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()),
          SLOT(onToolChanged()));

  updateToolbar();
}

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
