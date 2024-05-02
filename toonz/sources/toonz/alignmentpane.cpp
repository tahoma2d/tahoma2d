#include "alignmentpane.h"
#include "menubarcommandids.h"
#include "tapp.h"

#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"

#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

#include <QComboBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QGridLayout>
#include <QBoxLayout>
#include <QGroupBox>
#include <QLabel>

AlignmentPane::AlignmentPane(QWidget* parent, Qt::WindowFlags flags)
    : QFrame(parent), m_lastLevelType(UNKNOWN_XSHLEVEL) {
  setObjectName("AlignmentPanel");

  m_alignMethodCB = new QComboBox();
  m_alignMethodCB->addItem(tr("Selection Area"));
  m_alignMethodCB->setCurrentIndex(0);

  QAction* action;
  m_alignLeftBtn = new QPushButton(createQIcon("select_align_left"), 0, this);
  action         = CommandManager::instance()->getAction(MI_AlignLeft);
  m_alignLeftBtn->addAction(action);
  m_alignLeftBtn->setToolTip(tr("Align Left"));
  m_alignLeftBtn->setFixedSize(30, 30);
  connect(m_alignLeftBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignRightBtn = new QPushButton(createQIcon("select_align_right"), 0, this);
  action          = CommandManager::instance()->getAction(MI_AlignRight);
  m_alignRightBtn->addAction(action);
  m_alignRightBtn->setToolTip(tr("Align Right"));
  m_alignRightBtn->setFixedSize(30, 30);
  connect(m_alignRightBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignTopBtn = new QPushButton(createQIcon("select_align_top"), 0, this);
  action        = CommandManager::instance()->getAction(MI_AlignTop);
  m_alignTopBtn->addAction(action);
  m_alignTopBtn->setToolTip(tr("Align Top"));
  m_alignTopBtn->setFixedSize(30, 30);
  connect(m_alignTopBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignBottomBtn =
      new QPushButton(createQIcon("select_align_bottom"), 0, this);
  action = CommandManager::instance()->getAction(MI_AlignBottom);
  m_alignBottomBtn->addAction(action);
  m_alignBottomBtn->setToolTip(tr("Align Bottom"));
  m_alignBottomBtn->setFixedSize(30, 30);
  connect(m_alignBottomBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignCenterHBtn =
      new QPushButton(createQIcon("select_align_center_h"), 0, this);
  action = CommandManager::instance()->getAction(MI_AlignCenterHorizontal);
  m_alignCenterHBtn->addAction(action);
  m_alignCenterHBtn->setToolTip(tr("Align Center Horizontally"));
  m_alignCenterHBtn->setFixedSize(30, 30);
  connect(m_alignCenterHBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_alignCenterVBtn =
      new QPushButton(createQIcon("select_align_center_v"), 0, this);
  action = CommandManager::instance()->getAction(MI_AlignCenterVertical);
  m_alignCenterVBtn->addAction(action);
  m_alignCenterVBtn->setToolTip(tr("Align Center Vertically"));
  m_alignCenterVBtn->setFixedSize(30, 30);
  connect(m_alignCenterVBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_distributeHBtn =
      new QPushButton(createQIcon("select_distribute_h"), 0, this);
  action = CommandManager::instance()->getAction(MI_DistributeHorizontal);
  m_distributeHBtn->addAction(action);
  m_distributeHBtn->setToolTip(tr("Distribute Horizontally"));
  m_distributeHBtn->setFixedSize(30, 30);
  connect(m_distributeHBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  m_distributeVBtn =
      new QPushButton(createQIcon("select_distribute_v"), 0, this);
  action = CommandManager::instance()->getAction(MI_DistributeVertical);
  m_distributeVBtn->addAction(action);
  m_distributeVBtn->setToolTip(tr("Distribute Vertically"));
  m_distributeVBtn->setFixedSize(30, 30);
  connect(m_distributeVBtn, SIGNAL(clicked()), action, SLOT(trigger()));

  QGridLayout* mainlayout = new QGridLayout();
  mainlayout->setContentsMargins(5, 5, 5, 5);
  mainlayout->setSpacing(2);
  {
    mainlayout->addWidget(new QLabel(tr("Relative to: ")), 0, 0,
                          Qt::AlignRight);
    mainlayout->addWidget(m_alignMethodCB, 0, 1);

    QGroupBox* alignBox      = new QGroupBox(tr("Align"), this);
    QGridLayout* alignLayout = new QGridLayout();
    alignLayout->setContentsMargins(1, 1, 1, 1);
    alignLayout->setSpacing(1);
    {
      alignLayout->addWidget(m_alignLeftBtn, 0, 0, Qt::AlignCenter);
      alignLayout->addWidget(m_alignCenterVBtn, 0, 1, Qt::AlignCenter);
      alignLayout->addWidget(m_alignRightBtn, 0, 2, Qt::AlignCenter);

      alignLayout->addWidget(m_alignTopBtn, 1, 0, Qt::AlignCenter);
      alignLayout->addWidget(m_alignCenterHBtn, 1, 1, Qt::AlignCenter);
      alignLayout->addWidget(m_alignBottomBtn, 1, 2, Qt::AlignCenter);
    }
    alignBox->setLayout(alignLayout);
    mainlayout->addWidget(alignBox, 1, 0, 1, 2);

    QGroupBox* distributeBox      = new QGroupBox(tr("Distribute"), this);
    QGridLayout* distributeLayout = new QGridLayout();
    distributeLayout->setContentsMargins(1, 1, 1, 1);
    distributeLayout->setSpacing(1);
    {
      distributeLayout->addWidget(m_distributeHBtn, 0, 0, Qt::AlignCenter);
      distributeLayout->addWidget(m_distributeVBtn, 0, 1, Qt::AlignCenter);
    }
    distributeBox->setLayout(distributeLayout);
    mainlayout->addWidget(distributeBox, 2, 0, 1, 2);
  }

  setLayout(mainlayout);

  bool ret = true;

  ret = ret && connect(m_alignMethodCB, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onAlignMethodChanged(int)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void AlignmentPane::updateButtons() {
  TTool* tool = TApp::instance()->getCurrentTool()->getTool();

  QStringList inputs;
  int method = m_alignMethodCB->currentIndex();

  m_alignMethodCB->blockSignals(true);
  if (tool->getName() == T_Selection) {
    m_alignMethodCB->setEnabled(true);

    inputs << tr("Selection Area") << tr("First Selected")
           << tr("Last Selected") << tr("Smallest Object")
           << tr("Largest Object") << tr("Camera Area");
    m_alignMethodCB->clear();
    m_alignMethodCB->addItems(inputs);
    if (method >= inputs.size()) method = 0;
    m_alignMethodCB->setCurrentIndex(method);
  } else if (tool->getName() == T_ControlPointEditor) {
    m_alignMethodCB->setEnabled(true);

    QStringList inputs;
    inputs << tr("Selection Area") << tr("First Selected")
           << tr("Last Selected");
    m_alignMethodCB->clear();
    m_alignMethodCB->addItems(inputs);
    if (method >= inputs.size()) method = 0;
    m_alignMethodCB->setCurrentIndex(method);

    // Disable edge alignments if first/last selected or if other buttons
    // are disabled
    if (method == ALIGN_METHOD::FIRST_SELECTED ||
        method == ALIGN_METHOD::LAST_SELECTED ||
        !m_alignCenterHBtn->actions()[0]->isEnabled()) {
      m_alignLeftBtn->actions()[0]->setEnabled(false);
      m_alignRightBtn->actions()[0]->setEnabled(false);
      m_alignTopBtn->actions()[0]->setEnabled(false);
      m_alignBottomBtn->actions()[0]->setEnabled(false);

    } else {
      m_alignLeftBtn->actions()[0]->setEnabled(true);
      m_alignRightBtn->actions()[0]->setEnabled(true);
      m_alignTopBtn->actions()[0]->setEnabled(true);
      m_alignBottomBtn->actions()[0]->setEnabled(true);
    }
  } else
    m_alignMethodCB->setEnabled(false);
  m_alignMethodCB->blockSignals(false);

  if (tool->getName() == T_Selection ||
      tool->getName() == T_ControlPointEditor) {
    // Disable distribute if not Selection Area or Camera or if other buttons
    // are disabled
    if ((method > ALIGN_METHOD::SELECT_AREA &&
         method < ALIGN_METHOD::CAMERA_AREA) ||
        !m_alignLeftBtn->actions()[0]->isEnabled()) {
      m_distributeHBtn->actions()[0]->setEnabled(false);
      m_distributeVBtn->actions()[0]->setEnabled(false);
    } else {
      m_distributeHBtn->actions()[0]->setEnabled(true);
      m_distributeVBtn->actions()[0]->setEnabled(true);
    }
  }

  m_alignLeftBtn->setEnabled(m_alignLeftBtn->actions()[0]->isEnabled());
  m_alignRightBtn->setEnabled(m_alignRightBtn->actions()[0]->isEnabled());
  m_alignTopBtn->setEnabled(m_alignTopBtn->actions()[0]->isEnabled());
  m_alignBottomBtn->setEnabled(m_alignBottomBtn->actions()[0]->isEnabled());
  m_alignCenterHBtn->setEnabled(m_alignCenterHBtn->actions()[0]->isEnabled());
  m_alignCenterVBtn->setEnabled(m_alignCenterVBtn->actions()[0]->isEnabled());
  m_distributeHBtn->setEnabled(m_distributeHBtn->actions()[0]->isEnabled());
  m_distributeVBtn->setEnabled(m_distributeVBtn->actions()[0]->isEnabled());
}

//-----------------------------------------------------------------------------

void AlignmentPane::showEvent(QShowEvent*) {
  TApp* app = TApp::instance();

  bool ret = true;

  ret = ret &&
        connect(app->getCurrentLevel(), SIGNAL(xshLevelSwitched(TXshLevel*)),
                this, SLOT(onLevelSwitched(TXshLevel*)));
  ret = ret && connect(app->getCurrentTool(), SIGNAL(toolSwitched()), this,
                       SLOT(onToolSwitched()));
  ret =
      ret && connect(app->getCurrentSelection(),
                     SIGNAL(selectionSwitched(TSelection*, TSelection*)), this,
                     SLOT(onSelectionSwitched(TSelection*, TSelection*)));

  assert(ret);
  updateButtons();
}

//-----------------------------------------------------------------------------

void AlignmentPane::hideEvent(QHideEvent*) {
  TApp* app = TApp::instance();

  disconnect(app->getCurrentLevel(), 0, this, 0);
  disconnect(app->getCurrentTool(), 0, this, 0);
  disconnect(app->getCurrentSelection(), 0, this, 0);
}

//-----------------------------------------------------------------------------

void AlignmentPane::onLevelSwitched(TXshLevel* oldLvl) { updateButtons(); }

//-----------------------------------------------------------------------------

void AlignmentPane::onToolSwitched() { updateButtons(); }

//-----------------------------------------------------------------------------

void AlignmentPane::onAlignMethodChanged(int index) {
  TTool* tool = TApp::instance()->getCurrentTool()->getTool();
  tool->setAlignMethod(index);
  updateButtons();
}

//-----------------------------------------------------------------------------

void AlignmentPane::onSelectionSwitched(TSelection* oldSelection,
                                        TSelection* newSelection) {
  updateButtons();
}