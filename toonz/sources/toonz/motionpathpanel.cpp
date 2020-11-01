#include "motionpathpanel.h"
#include "tapp.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectcmd.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/tscenehandle.h"
#include "graphwidget.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "tstroke.h"
#include "tgeometry.h"

#include "pane.h"

#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QToolBar>
#include <QPixmap>
#include <QSlider>
#include <QComboBox>
#include <QPainter>
#include <QPainterPath>


namespace {
    double distanceSquared(QPoint p1, QPoint p2) {
        int newX = p1.x() - p2.x();
        int newY = p1.y() - p2.y();
        return (newX * newX) + (newY * newY);
    }
};

void MotionPathControl::createControl(TStageObjectSpline* spline) {
  getIconThemePath("actions/20/pane_preview.svg");
  m_spline = spline;
  m_active = spline->getActive();
  m_nameLabel =
      new ClickablePathLabel(QString::fromStdString(spline->getName()), this);
  m_activeButton = new TPanelTitleBarButton(
      this, getIconThemePath("actions/20/pane_preview.svg"));

  m_stepsEdit = new DVGui::IntLineEdit(this);
  m_stepsEdit->setValue(spline->getSteps());
  m_stepsEdit->setFixedWidth(40);

  m_widthSlider = new QSlider(this);
  m_widthSlider->setOrientation(Qt::Horizontal);
  m_widthSlider->setRange(1, 3);
  m_widthSlider->setValue(m_spline->getWidth());
  m_widthSlider->setMaximumWidth(30);

  m_colorCombo = new QComboBox(this);
  fillCombo();

  m_controlLayout = new QGridLayout(this);
  m_controlLayout->setMargin(1);
  m_controlLayout->setSpacing(3);
  m_controlLayout->addWidget(m_activeButton, 0, 0, Qt::AlignLeft);
  m_controlLayout->addWidget(m_nameLabel, 0, 1, Qt::AlignLeft);
  m_controlLayout->addWidget(m_widthSlider, 0, 2, Qt::AlignRight);
  m_controlLayout->addWidget(m_colorCombo, 0, 3, Qt::AlignRight);
  m_controlLayout->addWidget(m_stepsEdit, 0, 4, Qt::AlignRight);
  m_controlLayout->setColumnStretch(0, 0);
  m_controlLayout->setColumnStretch(1, 500);
  setLayout(m_controlLayout);
  connect(m_nameLabel, &ClickablePathLabel::onMouseRelease, [=]() {
    TApp* app = TApp::instance();
    TStageObjectTree* pegTree =
        app->getCurrentXsheet()->getXsheet()->getStageObjectTree();
    TStageObject* viewer = pegTree->getMotionPathViewer();
    viewer->setSpline(spline);
    app->getCurrentObject()->setObjectId(pegTree->getMotionPathViewerId());
    app->getCurrentObject()->setIsSpline(true, true);
  });
  m_activeButton->setPressed(m_spline->getActive());
  connect(m_activeButton, &TPanelTitleBarButton::toggled, [=](bool pressed) {
    m_spline->setActive(pressed);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  });
  connect(m_stepsEdit, &DVGui::IntLineEdit::textChanged,
          [=](const QString& text) {
            int steps = text.toInt();
            m_spline->setSteps(steps);
            TApp::instance()->getCurrentScene()->notifySceneChanged();
          });
  connect(m_widthSlider, &QSlider::valueChanged, [=]() {
    int width = m_widthSlider->value();
    m_spline->setWidth(width);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  });
  connect(m_colorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
          [=](int index) {

            m_spline->setColor(index);
            TApp::instance()->getCurrentScene()->notifySceneChanged();
          });
}

void MotionPathControl::fillCombo() {
  QPixmap magenta(15, 15);
  magenta.fill(Qt::magenta);
  m_colorCombo->addItem(QIcon(magenta), "");

  QPixmap yellow(15, 15);
  yellow.fill(Qt::yellow);
  m_colorCombo->addItem(QIcon(yellow), "");

  QPixmap cyan(15, 15);
  cyan.fill(Qt::cyan);
  m_colorCombo->addItem(QIcon(cyan), "");

  QPixmap red(15, 15);
  red.fill(Qt::red);
  m_colorCombo->addItem(QIcon(red), "");

  QPixmap blue(15, 15);
  blue.fill(Qt::blue);
  m_colorCombo->addItem(QIcon(blue), "");

  QPixmap green(15, 15);
  green.fill(Qt::green);
  m_colorCombo->addItem(QIcon(green), "");

  QPixmap black(15, 15);
  black.fill(Qt::black);
  m_colorCombo->addItem(QIcon(black), "");

  QPixmap white(15, 15);
  white.fill(Qt::white);
  m_colorCombo->addItem(QIcon(white), "");

  QPixmap lightGray(15, 15);
  lightGray.fill(Qt::lightGray);
  m_colorCombo->addItem(QIcon(lightGray), "");

  QPixmap gray(15, 15);
  gray.fill(Qt::gray);
  m_colorCombo->addItem(QIcon(gray), "");

  QPixmap darkGray(15, 15);
  darkGray.fill(Qt::darkGray);
  m_colorCombo->addItem(QIcon(darkGray), "");

  m_colorCombo->setCurrentIndex(m_spline->getColor());
}

//*****************************************************************************
//    MotionPathPanel  implementation
//*****************************************************************************

MotionPathPanel::MotionPathPanel(QWidget* parent) : QWidget(parent) {
  m_outsideLayout = new QVBoxLayout(this);
  m_outsideLayout->setMargin(0);
  m_outsideLayout->setSpacing(0);
  m_insideLayout = new QVBoxLayout(this);
  m_insideLayout->setMargin(0);
  m_insideLayout->setSpacing(0);
  m_pathsLayout = new QGridLayout(this);
  m_pathsLayout->setMargin(0);
  m_pathsLayout->setSpacing(0);
  m_mainControlsPage = new QFrame(this);
  m_mainControlsPage->setLayout(m_insideLayout);
  m_graphArea = new GraphWidget(this);
  m_graphArea->setMaxXValue(1000);
  m_graphArea->setMaxYValue(1000);

  QScrollArea* scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(m_mainControlsPage);
  m_outsideLayout->addWidget(scrollArea);

  // make the toolbar
  m_toolbar = new QToolBar(this);
  m_toolbar->addAction(CommandManager::instance()->getAction("MI_NewSpline"));
  m_toolbar->addAction(CommandManager::instance()->getAction("T_Brush"));
  m_toolbar->addAction(CommandManager::instance()->getAction("T_Geometric"));
  m_toolbar->addAction(
      CommandManager::instance()->getAction("T_ControlPointEditor"));
  m_toolbar->setFixedHeight(18);
  m_toolbar->setIconSize(QSize(16, 16));
  m_toolLayout = new QHBoxLayout(this);
  m_toolLayout->setSpacing(2);
  m_toolLayout->setMargin(2);
  m_toolLayout->addWidget(m_toolbar);
  ToolBarContainer* container = new ToolBarContainer(this);
  container->setObjectName("MotionPathToolbar");
  container->setLayout(m_toolLayout);

  m_controlsLayout = new QHBoxLayout(this);
  m_controlsLayout->addWidget(m_graphArea);

  m_insideLayout->addWidget(container);
  m_insideLayout->addLayout(m_pathsLayout);
  m_insideLayout->addStretch();
  m_insideLayout->addLayout(m_controlsLayout);
  setLayout(m_outsideLayout);
  TXsheetHandle* xsh = TApp::instance()->getCurrentXsheet();
  connect(xsh, &TXsheetHandle::xsheetChanged, [=]() { refreshPaths(); });
  TObjectHandle* object = TApp::instance()->getCurrentObject();
  connect(object, &TObjectHandle::objectSwitched, [=]() {
      if (object->isSpline()) {
          m_currentSpline = object->getCurrentSpline();
          m_graphArea->setStroke(m_currentSpline->getInterpolationStroke());
      }
      else {
          m_graphArea->clearStroke();
      }
      m_graphArea->update();
      });
}

//-----------------------------------------------------------------------------

MotionPathPanel::~MotionPathPanel() {}

//-----------------------------------------------------------------------------

void MotionPathPanel::refreshPaths() {
  TXsheetHandle* xsh     = TApp::instance()->getCurrentXsheet();
  TStageObjectTree* tree = xsh->getXsheet()->getStageObjectTree();
  if (tree->getSplineCount() == m_motionPathControls.size()) return;

  m_motionPathControls.clear();
  QLayoutItem* child;
  while (m_pathsLayout->count() != 0) {
    child = m_pathsLayout->takeAt(0);
    if (child->widget() != 0) {
      delete child->widget();
    }
    delete child;
  }

  int i = 0;
  for (; i < tree->getSplineCount(); i++) {
    MotionPathControl* control = new MotionPathControl(this);
    control->createControl(tree->getSpline(i));
    m_pathsLayout->addWidget(control, i, 0);
    m_pathsLayout->setRowStretch(i, 0);
    m_motionPathControls.push_back(control);
  }
  m_pathsLayout->addWidget(new QLabel(" ", this), ++i, 0);
  m_pathsLayout->setRowStretch(i, 500);
}

//=============================================================================

ClickablePathLabel::ClickablePathLabel(const QString& text, QWidget* parent,
                                       Qt::WindowFlags f)
    : QLabel(text, parent, f) {}

//-----------------------------------------------------------------------------

ClickablePathLabel::~ClickablePathLabel() {}

//-----------------------------------------------------------------------------

void ClickablePathLabel::mouseReleaseEvent(QMouseEvent* event) {
  emit onMouseRelease(event);
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::enterEvent(QEvent*) {
  setStyleSheet("text-decoration: underline;");
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::leaveEvent(QEvent*) {
  setStyleSheet("text-decoration: none;");
}

//=============================================================================

class CreateNewSpline : public MenuItemHandler {
public:
  CreateNewSpline() : MenuItemHandler(MI_NewSpline) {}
  void execute() {
    TApp* app          = TApp::instance();
    TXsheetHandle* xsh = app->getCurrentXsheet();
    TObjectHandle* obj = app->getCurrentObject();
    obj->setObjectId(
        xsh->getXsheet()->getStageObjectTree()->getMotionPathViewerId());
    TStageObjectCmd::addNewSpline(xsh, obj, 0);
  }
} CreateNewSpline;