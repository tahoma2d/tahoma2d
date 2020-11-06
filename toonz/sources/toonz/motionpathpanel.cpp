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
#include <QPushButton>

namespace {
double distanceSquared(QPoint p1, QPoint p2) {
  int newX = p1.x() - p2.x();
  int newY = p1.y() - p2.y();
  return (newX * newX) + (newY * newY);
}
};

//*****************************************************************************
//    MotionPathPanel  implementation
//*****************************************************************************

MotionPathPanel::MotionPathPanel(QWidget* parent) 
  : QWidget(parent)
  , m_currentSpline(0) {
  m_outsideLayout = new QVBoxLayout(this);
  m_outsideLayout->setMargin(0);
  m_outsideLayout->setSpacing(0);
  m_insideLayout = new QVBoxLayout(this);
  m_insideLayout->setMargin(0);
  m_insideLayout->setSpacing(0);
  m_pathsLayout = new QGridLayout(this);
  m_pathsLayout->setContentsMargins(0, 3, 2, 3);
  m_pathsLayout->setSpacing(2);
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
  m_controlsLayout->setMargin(10);
  QHBoxLayout* graphLayout = new QHBoxLayout(this);
  graphLayout->setMargin(0);
  graphLayout->setSpacing(0);
  graphLayout->addWidget(m_graphArea);
  QFrame* graphFrame = new QFrame(this);
  graphFrame->setLayout(graphLayout);
  graphFrame->setObjectName("GraphAreaFrame");
  m_controlsLayout->addWidget(graphFrame);

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
          m_graphArea->setPoints(m_currentSpline->getInterpolationStroke());
      }
      else {
          m_graphArea->clearPoints();
          m_currentSpline = 0;
      }
      highlightActiveSpline();
    m_graphArea->update();
  });
  connect(m_graphArea, &GraphWidget::controlPointChanged, [=](bool dragging) {
    if (m_currentSpline)
      m_currentSpline->setInterpolationStroke(m_graphArea->getPoints());
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  });
  highlightActiveSpline();
}

//-----------------------------------------------------------------------------

MotionPathPanel::~MotionPathPanel() {}

//-----------------------------------------------------------------------------

void MotionPathPanel::createControl(TStageObjectSpline* spline, int number) {
  getIconThemePath("actions/20/pane_preview.svg");
  TObjectHandle* objHandle = TApp::instance()->getCurrentObject();
  bool active = spline->getActive();
  ClickablePathLabel* nameLabel =
      new ClickablePathLabel(QString::fromStdString(spline->getName()), this);
  m_pathLabels.push_back(nameLabel);
  ClickablePathLabel* deleteLabel =
      new ClickablePathLabel("", this);
  deleteLabel->setPixmap(createQIcon("menu_toggle_on").pixmap(QSize(23, 18)));
  TPanelTitleBarButton* activeButton = new TPanelTitleBarButton(
      this, getIconThemePath("actions/20/pane_preview.svg"));

  DVGui::IntLineEdit* stepsEdit = new DVGui::IntLineEdit(this);
  stepsEdit->setValue(spline->getSteps());
  stepsEdit->setFixedWidth(40);

  QSlider* widthSlider = new QSlider(this);
  widthSlider->setOrientation(Qt::Horizontal);
  widthSlider->setRange(1, 3);
  widthSlider->setValue(spline->getWidth());
  widthSlider->setMaximumWidth(30);

  QComboBox* colorCombo = new QComboBox(this);
  fillCombo(colorCombo, spline);

  //QPushButton* removeButton = new QPushButton("-", this);

  m_pathsLayout->addWidget(activeButton, number, 0, Qt::AlignLeft);
  m_pathsLayout->addWidget(nameLabel, number, 1, Qt::AlignLeft);
  m_pathsLayout->addWidget(widthSlider, number, 2, Qt::AlignRight);
  m_pathsLayout->addWidget(colorCombo, number, 3, Qt::AlignRight);
  m_pathsLayout->addWidget(stepsEdit, number, 4, Qt::AlignRight);
  m_pathsLayout->addWidget(deleteLabel, number, 5, Qt::AlignRight);

  connect(nameLabel, &ClickablePathLabel::onMouseRelease, [=]() {
    TApp* app = TApp::instance();
    TStageObjectTree* pegTree =
        app->getCurrentXsheet()->getXsheet()->getStageObjectTree();
    TStageObject* viewer = pegTree->getMotionPathViewer();
    viewer->setSpline(spline);
    objHandle->setObjectId(pegTree->getMotionPathViewerId());
    objHandle->setIsSpline(true, true);
  });
  activeButton->setPressed(spline->getActive());
  connect(activeButton, &TPanelTitleBarButton::toggled, [=](bool pressed) {
    spline->setActive(pressed);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  });
  connect(stepsEdit, &DVGui::IntLineEdit::textChanged,
          [=](const QString& text) {
            int steps = text.toInt();
            spline->setSteps(steps);
            TApp::instance()->getCurrentScene()->notifySceneChanged();
          });
  connect(widthSlider, &QSlider::valueChanged, [=]() {
    int width = widthSlider->value();
    spline->setWidth(width);
    TApp::instance()->getCurrentScene()->notifySceneChanged();
  });
  connect(colorCombo, qOverload<int>(&QComboBox::currentIndexChanged),
          [=](int index) {

            spline->setColor(index);
            TApp::instance()->getCurrentScene()->notifySceneChanged();
          });
  connect(deleteLabel, &ClickablePathLabel::onMouseRelease,
      [=]() {
          const std::vector<TStageObjectId> objIds;
          const std::list<QPair<TStageObjectId, TStageObjectId>> links;
          std::list<int> splineIds;
          splineIds.push_back(spline->getId());
          TXsheetHandle* xshHandle = TApp::instance()->getCurrentXsheet();
          TFxHandle* fxHandle = TApp::instance()->getCurrentFx();
          TStageObjectCmd::deleteSelection(objIds, links, splineIds, xshHandle, objHandle, fxHandle, true);
          refreshPaths();
      });
  //if (objHandle->isSpline() && spline == objHandle->getCurrentSpline()) {
  //    m_activeSplineId = spline->getId();
  //}
}

//-----------------------------------------------------------------------------

void MotionPathPanel::refreshPaths() {
  TXsheetHandle* xsh     = TApp::instance()->getCurrentXsheet();
  TStageObjectTree* tree = xsh->getXsheet()->getStageObjectTree();
  m_pathLabels.clear();
  //m_activeSplineId = -1;
  QLayoutItem* child;
  while (m_pathsLayout->count() != 0) {
    child = m_pathsLayout->takeAt(0);
    if (child->widget() != 0) {
      delete child->widget();
    }
    delete child;
  }

  if (tree->getSplineCount() > 0) {
    m_pathsLayout->addWidget(new QLabel(""), 0, 0, Qt::AlignLeft);
    m_pathsLayout->addWidget(new QLabel("Path Name"), 0, 1, Qt::AlignLeft);
    m_pathsLayout->addWidget(new QLabel("Width"), 0, 2, Qt::AlignCenter);
    m_pathsLayout->addWidget(new QLabel("Color"), 0, 3, Qt::AlignCenter);
    m_pathsLayout->addWidget(new QLabel("Steps"), 0, 4, Qt::AlignCenter);
    int i = 0;
    for (; i < tree->getSplineCount(); i++) {
      // MotionPathControl* control = new MotionPathControl(this);
      createControl(tree->getSpline(i), i + 1);
    }

    m_pathsLayout->setColumnStretch(0, 0);
    m_pathsLayout->setColumnStretch(1, 500);
    m_pathsLayout->addWidget(new QLabel(" ", this), ++i, 0);
    m_pathsLayout->setRowStretch(i, 500);
  }
}

//-----------------------------------------------------------------------------

void MotionPathPanel::highlightActiveSpline() {
    if (m_pathLabels.size() > 0) {

        for (auto label : m_pathLabels) {
            if (m_currentSpline && m_currentSpline->getName() == label->text().toStdString()) {
                label->setSelected();
            }
            else label->clearSelected();
        }
    }
}

//-----------------------------------------------------------------------------

void MotionPathPanel::fillCombo(QComboBox* combo, TStageObjectSpline* spline) {
  QPixmap magenta(15, 15);
  magenta.fill(Qt::magenta);
  combo->addItem(QIcon(magenta), "");

  QPixmap yellow(15, 15);
  yellow.fill(Qt::yellow);
  combo->addItem(QIcon(yellow), "");

  QPixmap cyan(15, 15);
  cyan.fill(Qt::cyan);
  combo->addItem(QIcon(cyan), "");

  QPixmap red(15, 15);
  red.fill(Qt::red);
  combo->addItem(QIcon(red), "");

  QPixmap blue(15, 15);
  blue.fill(Qt::blue);
  combo->addItem(QIcon(blue), "");

  QPixmap green(15, 15);
  green.fill(Qt::green);
  combo->addItem(QIcon(green), "");

  QPixmap black(15, 15);
  black.fill(Qt::black);
  combo->addItem(QIcon(black), "");

  QPixmap white(15, 15);
  white.fill(Qt::white);
  combo->addItem(QIcon(white), "");

  QPixmap lightGray(15, 15);
  lightGray.fill(Qt::lightGray);
  combo->addItem(QIcon(lightGray), "");

  QPixmap gray(15, 15);
  gray.fill(Qt::gray);
  combo->addItem(QIcon(gray), "");

  QPixmap darkGray(15, 15);
  darkGray.fill(Qt::darkGray);
  combo->addItem(QIcon(darkGray), "");

  combo->setCurrentIndex(spline->getColor());
}

//=============================================================================

ClickablePathLabel::ClickablePathLabel(const QString& text, QWidget* parent,
                                       Qt::WindowFlags f)
    : QLabel(text, parent, f) {
    setMaximumHeight(18);
    setObjectName("MotionPathLabel");
}

//-----------------------------------------------------------------------------

ClickablePathLabel::~ClickablePathLabel() {}

//-----------------------------------------------------------------------------

void ClickablePathLabel::mouseReleaseEvent(QMouseEvent* event) {
  emit onMouseRelease(event);
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::enterEvent(QEvent*) {
  
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::leaveEvent(QEvent*) {
  
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::setSelected() {
    QString qss = "background: rgb(" + QString::number(m_selectedColor.red()) + ", " + QString::number(m_selectedColor.green()) + ", " + QString::number(m_selectedColor.blue()) + ");";
    setStyleSheet(qss);
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::clearSelected() {
    setStyleSheet("#MotionPathLabel{"
  "qproperty - SelectedColor: #5385a6;"
  "qproperty - HoverColor: #717171;"
  "border - radius: 2;"
  "padding - left: 2px;"
  "padding - right: 2px;"
        "}"
        "#MotionPathLabel:hover{"
        "background: rgb(" + QString::number(m_hoverColor.red()) + ", " + QString::number(m_hoverColor.green()) + ", " + QString::number(m_hoverColor.blue()) + ");"
          "text - decoration: underline;"
        "}");
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
    TStageObjectCmd::addNewSpline(xsh, obj, 0, QPointF(0.0, 0.0), true);
  }
} CreateNewSpline;