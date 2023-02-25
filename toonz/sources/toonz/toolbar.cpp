

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
#include "toonz/tscenehandle.h"

// TnzBase includes
#include "tenv.h"

#include <QPainter>
#include <QAction>
#include <QToolButton>
#include <QVBoxLayout>
#include <QObject>
#include <QContextMenuEvent>
#include <QStyle>

TEnv::IntVar ShowAllToolsToggle("ShowAllToolsToggle", 1);

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

Toolbar::Toolbar(QWidget *parent, bool isViewerToolbar)
    : QToolBar(parent)
    , m_isExpanded(ShowAllToolsToggle != 0)
    , m_isViewerToolbar(isViewerToolbar)
    , m_isVertical(true) {
  // Fondamentale per lo style sheet
  setObjectName("toolBar");

  if (m_isViewerToolbar) m_isVertical = false;

  m_panel      = dynamic_cast<TPanel *>(parent);

  setMovable(false);
  setIconSize(QSize(20, 20));
  setToolButtonStyle(Qt::ToolButtonIconOnly);

  m_moreButton   = findChild<QToolButton *>("qt_toolbar_ext_button");

  m_expandButton = new QToolButton(this);
  m_expandButton->setObjectName("expandButton");
  m_expandButton->setCheckable(true);
  m_expandButton->setChecked(m_isExpanded);
  m_expandButton->hide();
  m_expandAction = addWidget(m_expandButton);

  connect(m_expandButton, SIGNAL(toggled(bool)), this,
          SLOT(setIsExpanded(bool)));

  updateOrientation(m_isVertical);
}

//-----------------------------------------------------------------------------
/*! Layout the tool buttons according to the state of the expandButton
 */
void Toolbar::updateToolbar() {
  TApp *app                 = TApp::instance();
  TFrameHandle *frameHandle = app->getCurrentFrame();

  if (frameHandle->isPlaying()) return;

  TXshLevelHandle *currlevel = app->getCurrentLevel();
  TXshLevel *level           = currlevel ? currlevel->getLevel() : 0;
  int levelType              = level ? level->getType() : NO_XSHLEVEL;

  TColumnHandle *colHandle = app->getCurrentColumn();
  int colIndex             = colHandle->getColumnIndex();

  int rowIndex = frameHandle->getFrameIndex();

  if (Preferences::instance()->isAutoCreateEnabled() &&
      Preferences::instance()->isAnimationSheetEnabled()) {
    // If in an empty cell, find most recent level
    if (levelType == NO_XSHLEVEL) {
      TXsheetHandle *xshHandle = app->getCurrentXsheet();
      TXsheet *xsh             = xshHandle->getXsheet();

      if (colIndex >= 0 && !xsh->isColumnEmpty(colIndex)) {
        int r0, r1;
        xsh->getCellRange(colIndex, r0, r1);
        if (0 <= r0 && r0 <= r1) {
          // level type depends on previous occupied cell
          for (int r = std::min(r1, rowIndex); r >= r0; r--) {
            TXshCell cell = xsh->getCell(r, colIndex);
            if (cell.isEmpty()) continue;
            levelType = cell.m_level->getType();
            rowIndex  = r;
            break;
          }

          if (levelType == NO_XSHLEVEL) {
            TXshCell cell = xsh->getCell(r0, colIndex);
            levelType     = cell.m_level->getType();
            rowIndex      = r0;
          }
        }
      }
    }
  }

  m_toolbarLevel = levelType;

  TTool::ToolTargetType targetType = TTool::NoTarget;

  switch (m_toolbarLevel) {
  case OVL_XSHLEVEL:
    targetType = TTool::RasterImage;
    break;
  case TZP_XSHLEVEL:
    targetType = TTool::ToonzImage;
    break;
  case PLI_XSHLEVEL:
  default:
    targetType = TTool::VectorImage;
    break;
  case MESH_XSHLEVEL:
    targetType = TTool::MeshImage;
    break;
  }

  // Hide action for now
  for (int idx = 0; m_buttonLayout[idx].toolName; idx++) {
    if (m_buttonLayout[idx].action) removeAction(m_buttonLayout[idx].action);
  }

  removeAction(m_expandAction);

  int levelBasedDisplay = Preferences::instance()->getLevelBasedToolsDisplay();

  bool actionEnabled     = false;
  ToolHandle *toolHandle = TApp::instance()->getCurrentTool();

  for (int idx = 0; m_buttonLayout[idx].toolName; idx++) {
    TTool *tool = TTool::getTool(m_buttonLayout[idx].toolName, targetType);
    if (tool) tool->updateEnabled(rowIndex, colIndex);
    bool enable =
        !levelBasedDisplay ? true : (!tool ? actionEnabled : tool->isEnabled());

    // Plastic tool should always be available so you can create a mesh
    if (!enable && !strncmp(m_buttonLayout[idx].toolName, T_Plastic, 9) &&
        (m_toolbarLevel & LEVELCOLUMN_XSHLEVEL))
      enable = true;

    if (!m_isExpanded && m_buttonLayout[idx].collapsible) continue;

    if (!m_buttonLayout[idx].action) {
      if (m_buttonLayout[idx].isSeparator)
        m_buttonLayout[idx].action = addSeparator();
      else
        m_buttonLayout[idx].action =
            CommandManager::instance()->getAction(m_buttonLayout[idx].toolName);
    }

    if (levelBasedDisplay != 2)
      m_buttonLayout[idx].action->setEnabled(enable);
    else if (!enable)
      continue;

    actionEnabled = addAction(m_buttonLayout[idx].action) || actionEnabled;

    if (m_buttonLayout[idx].isSeparator) actionEnabled = false;
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
  connect(frameHandle, SIGNAL(frameSwitched()), this, SLOT(updateToolbar()));
  connect(frameHandle, SIGNAL(frameTypeChanged()), this, SLOT(updateToolbar()));

  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();
  connect(xsheetHandle, SIGNAL(xsheetChanged()), this, SLOT(updateToolbar()));

  connect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()),
          SLOT(onToolChanged()));

  TXshLevelHandle *levelHandle = TApp::instance()->getCurrentLevel();
  connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
          SLOT(updateToolbar()));

  connect(TApp::instance()->getCurrentScene(),
          SIGNAL(preferenceChanged(const QString &)), this,
          SLOT(onPreferenceChanged(const QString &)));
}

//-----------------------------------------------------------------------------

void Toolbar::hideEvent(QHideEvent *e) {
  disconnect(TApp::instance()->getCurrentLevel(), 0, this, 0);
  disconnect(TApp::instance()->getCurrentTool(), SIGNAL(toolSwitched()), this,
             SLOT(onToolChanged()));

  disconnect(TApp::instance()->getCurrentColumn(),
             SIGNAL(columnIndexSwitched()), this, SLOT(updateToolbar()));

  disconnect(TApp::instance()->getCurrentFrame(), SIGNAL(frameSwitched()), this,
             SLOT(updateToolbar()));
  disconnect(TApp::instance()->getCurrentFrame(), SIGNAL(frameTypeChanged()),
             this, SLOT(updateToolbar()));

  disconnect(TApp::instance()->getCurrentXsheet(), SIGNAL(xsheetChanged()),
             this, SLOT(updateToolbar()));

  disconnect(TApp::instance()->getCurrentScene(),
             SIGNAL(preferenceChanged(const QString &)), this,
             SLOT(onPreferenceChanged(const QString &)));
}

//-----------------------------------------------------------------------------

void Toolbar::contextMenuEvent(QContextMenuEvent *event) {
  if (m_isViewerToolbar) return;

  QMenu *menu             = new QMenu();

  QAction *toggleOrientation = menu->addAction(tr("Toggle Orientation"));
  connect(toggleOrientation, SIGNAL(triggered(bool)), this,
          SLOT(orientationToggled(bool)));

  toggleOrientation->setEnabled(m_panel->isFloating());

  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void Toolbar::orientationToggled(bool ignore) {
  if (m_isViewerToolbar) return;

  updateOrientation(!m_isVertical);
}

void Toolbar::updateOrientation(bool isVertical) {
  bool wasVertical = m_isVertical;

  m_isVertical = isVertical;

  if (!m_isViewerToolbar) {
    // Current dimensions
    int x = wasVertical ? m_panel->height() : m_panel->width();

    m_panel->setOrientation(m_isVertical);
    m_panel->setFixedWidth(m_isVertical ? 44 : QWIDGETSIZE_MAX);
    m_panel->setFixedHeight(m_isVertical ? QWIDGETSIZE_MAX : 44);
    if (m_isVertical)
      m_panel->resize(44, x);
    else
      m_panel->resize(x, 44);
  }

  setFixedWidth(m_isVertical ? 34 : QWIDGETSIZE_MAX);
  setFixedHeight(m_isVertical ? QWIDGETSIZE_MAX : 34);
  setOrientation(m_isVertical ? Qt::Vertical : Qt::Horizontal);
  setSizePolicy(QSizePolicy::Policy::Expanding,
                  QSizePolicy::Policy::Expanding);

//  m_moreButton->setIcon(m_isVertical ? m_moreDownIcon : m_moreIcon);
  m_moreButton->setObjectName(m_isVertical
                                  ? "qt_toolbar_ext_button_vertical"
                                  : "qt_toolbar_ext_button_horizontal");
  QStyle *s = m_moreButton->style();
  s->unpolish(m_moreButton);
  s->polish(m_moreButton);

  m_expandButton->setArrowType(m_isVertical ? Qt::DownArrow : Qt::RightArrow);
  updateToolbar();
}

// SaveLoadQSettings
void Toolbar::save(QSettings &settings, bool forPopupIni) const {
  UINT orientation = 0;
  orientation      = m_isVertical ? 1 : 0;
  settings.setValue("vertical", orientation);
}

void Toolbar::load(QSettings &settings) {
  UINT orientation = settings.value("vertical", 1).toUInt();
  m_isVertical     = orientation == 1;
  updateOrientation(m_isVertical);
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

//-----------------------------------------------------------------------------

void Toolbar::onPreferenceChanged(const QString &prefName) {
  if (prefName == "ToolbarDisplay" || prefName.isEmpty()) updateToolbar();
}

//=============================================================================

OpenFloatingPanel openToolbarPane(MI_OpenToolbar, "ToolBar", "");
