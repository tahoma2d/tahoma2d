#include "motionpathpanel.h"
#include "tapp.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectcmd.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "tproperty.h"
#include "graphwidget.h"
#include "menubarcommandids.h"
#include "tstroke.h"
#include "tgeometry.h"
#include "tstopwatch.h"
#include "sceneviewer.h"
#include "toutputproperties.h"
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
#include <QLineEdit>
#include <QSizePolicy>

namespace {
double distanceSquared(QPoint p1, QPoint p2) {
  int newX = p1.x() - p2.x();
  int newY = p1.y() - p2.y();
  return (newX * newX) + (newY * newY);
}
};  // namespace

//*****************************************************************************
//    MotionPathPanel  implementation
//*****************************************************************************

MotionPathPanel::MotionPathPanel(QWidget* parent)
    : QWidget(parent), m_currentSpline(0), m_playbackExecutor() {
  m_outsideLayout = new QVBoxLayout();
  m_outsideLayout->setMargin(0);
  m_outsideLayout->setSpacing(0);

  m_insideLayout = new QVBoxLayout();
  m_insideLayout->setMargin(0);
  m_insideLayout->setSpacing(0);

  m_pathsLayout = new QGridLayout();
  m_pathsLayout->setContentsMargins(0, 3, 2, 3);
  m_pathsLayout->setSpacing(2);

  QScrollArea* pathScrollArea = new QScrollArea();
  pathScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  pathScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  pathScrollArea->setWidgetResizable(true);
  QFrame* pathFrame = new QFrame(this);
  pathScrollArea->setWidget(pathFrame);
  pathScrollArea->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
  pathFrame->setLayout(m_pathsLayout);
  QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Maximum);
  pathFrame->setSizePolicy(policy);

  m_mainControlsPage = new QFrame();
  m_mainControlsPage->setLayout(m_insideLayout);

  m_graphArea = new GraphWidget();
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
  m_toolbar->addAction(CommandManager::instance()->getAction("T_Selection"));
  m_toolbar->addAction(CommandManager::instance()->getAction("T_Brush"));
  m_toolbar->addAction(
      CommandManager::instance()->getAction("T_ControlPointEditor"));
  m_toolbar->addAction(CommandManager::instance()->getAction("T_Pinch"));
  m_toolbar->addAction(CommandManager::instance()->getAction("T_Geometric"));
  m_toolbar->setFixedHeight(18);
  m_toolbar->setIconSize(QSize(16, 16));
  m_toolLayout = new QHBoxLayout();
  m_toolLayout->setSpacing(2);
  m_toolLayout->setMargin(2);
  m_toolLayout->addWidget(m_toolbar);

  QStringList geomOptions;
  geomOptions << tr("Rectangle") << tr("Circle") << tr("Ellipse") << tr("Line")
              << tr("Polyline") << tr("Arc") << tr("MultiArc") << tr("Polygon");
  m_geometryOptionsBox = new QComboBox(this);
  m_geometryOptionsBox->addItems(geomOptions);

  m_geometrySidesField = new DVGui::IntField(this);
  m_geometrySidesField->setRange(3, 15);
  m_geometrySidesField->setValue(3);

  ToolHandle* th = TApp::instance()->getCurrentTool();
  connect(th, &ToolHandle::toolSwitched, this, &MotionPathPanel::updateTools);
  connect(th, &ToolHandle::toolChanged, this, &MotionPathPanel::updateTools);
  connect(
      m_geometryOptionsBox,
      static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
      this, &MotionPathPanel::onGeometricComboChanged);
  connect(m_geometrySidesField, &DVGui::IntField::valueChanged, this,
          &MotionPathPanel::onGeometricSidesChanged);
  m_toolLayout->addWidget(m_geometryOptionsBox);
  m_toolLayout->addWidget(m_geometrySidesField);
  m_toolLayout->addStretch();
  m_geometryOptionsBox->hide();
  m_geometrySidesField->hide();

  ToolBarContainer* container = new ToolBarContainer(this);
  container->setObjectName("MotionPathToolbar");
  container->setLayout(m_toolLayout);

  m_controlsLayout = new QVBoxLayout();
  m_controlsLayout->setMargin(10);
  m_controlsLayout->setSpacing(3);

  QHBoxLayout* graphLayout = new QHBoxLayout();
  graphLayout->setMargin(0);
  graphLayout->setSpacing(0);
  graphLayout->addWidget(m_graphArea);

  m_graphFrame = new QFrame(this);
  m_graphFrame->setLayout(graphLayout);
  m_graphFrame->setObjectName("GraphAreaFrame");
  m_graphFrame->setFixedHeight(200);
  m_controlsLayout->addWidget(m_graphFrame);

  m_playToolbar = new QToolBar(this);
  m_playToolbar->setFixedHeight(18);
  m_playToolbar->setIconSize(QSize(16, 16));

  QAction* playAction = new QAction(this);
  playAction->setIcon(createQIcon("play"));
  m_playToolbar->addAction(playAction);
  connect(playAction, &QAction::triggered, [=]() {
    if (!m_currentSpline) return;
    m_looping = false;
    int fps   = 24;
    fps       = TApp::instance()
              ->getCurrentScene()
              ->getScene()
              ->getProperties()
              ->getOutputProperties()
              ->getFrameRate();
    m_playbackExecutor.resetFps(fps);
    m_currentSpline->setCurrentStep(0);
    m_currentSpline->setIsPlaying(true);
    if (!m_playbackExecutor.isRunning()) m_playbackExecutor.start();
    if (TApp::instance()->getActiveViewer())
      TApp::instance()->getActiveViewer()->update();
  });

  QAction* loopAction = new QAction(this);
  loopAction->setIcon(createQIcon("loop"));
  m_playToolbar->addAction(loopAction);
  connect(loopAction, &QAction::triggered, [=]() {
    if (!m_currentSpline) return;
    m_looping = true;
    int fps   = 24;
    fps       = TApp::instance()
              ->getCurrentScene()
              ->getScene()
              ->getProperties()
              ->getOutputProperties()
              ->getFrameRate();
    m_playbackExecutor.resetFps(fps);
    m_currentSpline->setCurrentStep(0);
    m_currentSpline->setIsPlaying(true);
    if (!m_playbackExecutor.isRunning()) m_playbackExecutor.start();
    if (TApp::instance()->getActiveViewer())
      TApp::instance()->getActiveViewer()->update();
  });

  QAction* stopActionAction = new QAction(this);
  stopActionAction->setIcon(createQIcon("stop"));
  m_playToolbar->addAction(stopActionAction);
  connect(stopActionAction, &QAction::triggered, [=]() {
    m_playbackExecutor.abort();
    m_looping = false;
    if (!m_currentSpline) return;
    m_currentSpline->setIsPlaying(false);
    m_currentSpline->setCurrentStep(0);
    if (TApp::instance()->getActiveViewer())
      TApp::instance()->getActiveViewer()->update();
  });

  m_controlsLayout->addWidget(m_playToolbar, Qt::AlignCenter);

  m_insideLayout->addWidget(container, 0, Qt::AlignTop);
  m_insideLayout->addWidget(pathScrollArea);
  m_insideLayout->addLayout(m_controlsLayout);
  setLayout(m_outsideLayout);

  TSceneHandle* scene = TApp::instance()->getCurrentScene();
  connect(scene, &TSceneHandle::castChanged, [=]() { refreshPaths(true); });
  TXsheetHandle* xsh = TApp::instance()->getCurrentXsheet();
  connect(xsh, &TXsheetHandle::xsheetChanged, [=]() { refreshPaths(); });

  TObjectHandle* object = TApp::instance()->getCurrentObject();

  connect(object, &TObjectHandle::objectSwitched, [=]() {
    if (object->isSpline()) {
      if (object->getObjectId() ==
          xsh->getXsheet()->getStageObjectTree()->getMotionPathViewerId()) {
        if (TApp::instance()->getCurrentTool()->getTool()->getName() ==
            T_Edit) {
          TApp::instance()->getCurrentTool()->setTool(T_ControlPointEditor);
        }
      }
      m_currentSpline = object->getCurrentSpline();
      m_graphArea->setPoints(m_currentSpline->getInterpolationStroke());
    } else {
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

  bool ret = connect(&m_playbackExecutor, SIGNAL(nextFrame(int)), this,
                     SLOT(onNextFrame(int)), Qt::BlockingQueuedConnection);

  ret = ret && connect(&m_playbackExecutor, SIGNAL(playbackAborted()), this,
                       SLOT(stopPlayback()));
  assert(ret);

  highlightActiveSpline();
}

//-----------------------------------------------------------------------------

MotionPathPanel::~MotionPathPanel() {}

//-----------------------------------------------------------------------------

void MotionPathPanel::showEvent(QShowEvent*) { refreshPaths(true); }

//-----------------------------------------------------------------------------

void MotionPathPanel::createControl(TStageObjectSpline* spline, int number) {
  TObjectHandle* objHandle = TApp::instance()->getCurrentObject();
  bool active              = spline->getActive();
  ClickablePathLabel* nameLabel =
      new ClickablePathLabel(QString::fromStdString(spline->getName()), this);
  m_pathLabels.push_back(nameLabel);
  DVGui::LineEdit* nameEdit       = new DVGui::LineEdit(this);
  ClickablePathLabel* deleteLabel = new ClickablePathLabel("", this);
  deleteLabel->setPixmap(createQIcon("menu_toggle_on").pixmap(QSize(23, 18)));
  TPanelTitleBarButton* activeButton =
      new TPanelTitleBarButton(this, getIconPath("pane_preview"));

  DVGui::IntLineEdit* stepsEdit = new DVGui::IntLineEdit(this);
  stepsEdit->setValue(spline->getSteps());
  stepsEdit->setRange(0, 300);
  stepsEdit->setFixedWidth(40);

  QSlider* widthSlider = new QSlider(this);
  widthSlider->setOrientation(Qt::Horizontal);
  widthSlider->setRange(1, 3);
  widthSlider->setValue(spline->getWidth());
  widthSlider->setMaximumWidth(30);

  QComboBox* colorCombo = new QComboBox(this);
  fillCombo(colorCombo, spline);

  QHBoxLayout* nameLayout = new QHBoxLayout();
  nameLayout->addWidget(nameLabel);
  nameLayout->addWidget(nameEdit);
  nameEdit->hide();
  nameLayout->addStretch();
  m_nameLayouts.push_back(nameLayout);

  m_pathsLayout->addWidget(activeButton, number, 0, Qt::AlignLeft);
  m_pathsLayout->addLayout(nameLayout, number, 1, Qt::AlignLeft);
  m_pathsLayout->addWidget(widthSlider, number, 2, Qt::AlignRight);
  m_pathsLayout->addWidget(colorCombo, number, 3, Qt::AlignRight);
  m_pathsLayout->addWidget(stepsEdit, number, 4, Qt::AlignRight);
  m_pathsLayout->addWidget(deleteLabel, number, 5, Qt::AlignRight);

  connect(nameLabel, &ClickablePathLabel::doubleClicked, [=]() {
    nameLabel->hide();
    nameEdit->setText(nameLabel->text());
    nameEdit->show();
    nameEdit->setFocus();
    nameEdit->selectAll();
  });

  connect(nameEdit, &QLineEdit::editingFinished, [=]() {
    QString text = nameEdit->text();
    nameEdit->hide();
    nameLabel->show();
    stepsEdit->clearFocus();
    if (text.length() > 0 && text != nameLabel->text()) {
      nameLabel->setText(text);
      if (spline) spline->setName(text.toStdString());
      TApp::instance()->getCurrentScene()->notifySceneChanged();
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    }
  });

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
  connect(colorCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          [=](int index) {
            spline->setColor(index);
            TApp::instance()->getCurrentScene()->notifySceneChanged();
          });
  connect(deleteLabel, &ClickablePathLabel::onMouseRelease, [=]() {
    const std::vector<TStageObjectId> objIds;
    const std::list<QPair<TStageObjectId, TStageObjectId>> links;
    std::list<int> splineIds;
    splineIds.push_back(spline->getId());
    TXsheetHandle* xshHandle = TApp::instance()->getCurrentXsheet();
    TFxHandle* fxHandle      = TApp::instance()->getCurrentFx();
    TStageObjectCmd::deleteSelection(objIds, links, splineIds, xshHandle,
                                     objHandle, fxHandle, true);
    refreshPaths();
  });
}

//-----------------------------------------------------------------------------

void MotionPathPanel::refreshPaths(bool force) {
  TXsheetHandle* xsh     = TApp::instance()->getCurrentXsheet();
  TStageObjectTree* tree = xsh->getXsheet()->getStageObjectTree();
  if (tree->getSplineCount() == m_pathLabels.size() && !force) return;
  m_pathLabels.clear();
  // m_activeSplineId = -1;
  QLayoutItem* child;

  for (auto layout : m_nameLayouts) {
    while (layout->count() != 0) {
      child = layout->takeAt(0);
      if (child->widget() != 0) {
        delete child->widget();
      }
      delete child;
    }
  }
  m_nameLayouts.clear();

  while (m_pathsLayout->count() != 0) {
    child = m_pathsLayout->takeAt(0);
    if (child->widget() != 0) {
      delete child->widget();
    }
    delete child;
  }

  if (tree->getSplineCount() > 0) {
    m_pathsLayout->addWidget(new QLabel(""), 0, 0, Qt::AlignLeft);
    m_pathsLayout->addWidget(new QLabel(tr("Path Name")), 0, 1, Qt::AlignLeft);
    m_pathsLayout->addWidget(new QLabel(tr("Width")), 0, 2, Qt::AlignCenter);
    m_pathsLayout->addWidget(new QLabel(tr("Color")), 0, 3, Qt::AlignCenter);
    m_pathsLayout->addWidget(new QLabel(tr("Steps")), 0, 4, Qt::AlignCenter);
    int i = 0;
    for (; i < tree->getSplineCount(); i++) {
      createControl(tree->getSpline(i), i + 1);
    }

    m_pathsLayout->setColumnStretch(0, 0);
    m_pathsLayout->setColumnStretch(1, 500);
    m_pathsLayout->addWidget(new QLabel(" ", this), ++i, 0);
    m_pathsLayout->setRowStretch(i, 500);
  }
  highlightActiveSpline();
}

//-----------------------------------------------------------------------------

void MotionPathPanel::highlightActiveSpline() {
  m_graphFrame->hide();
  m_playToolbar->hide();
  if (m_pathLabels.size() > 0) {
    for (auto label : m_pathLabels) {
      if (m_currentSpline &&
          m_currentSpline->getName() == label->text().toStdString()) {
        QString qss = "background: rgb(" +
                      QString::number(m_selectedColor.red()) + ", " +
                      QString::number(m_selectedColor.green()) + ", " +
                      QString::number(m_selectedColor.blue()) +
                      ");"
                      "border-radius: 2;"
                      "padding-left: 2px;"
                      "padding-right: 2px;";
        std::string css = qss.toStdString();
        label->setStyleSheet(qss);
        m_graphFrame->show();
        m_playToolbar->show();
      } else {
        QString qss = "background: rgba(" +
                      QString::number(m_selectedColor.red()) + ", " +
                      QString::number(m_selectedColor.green()) + ", " +
                      QString::number(m_selectedColor.blue()) +
                      ", 0);"
                      " border-radius: 2;"
                      " padding-left: 2px;"
                      " padding-right: 2px;"
                      " :hover{"
                      " background: rgb(" +
                      QString::number(m_hoverColor.red()) + ", " +
                      QString::number(m_hoverColor.green()) + ", " +
                      QString::number(m_hoverColor.blue()) +
                      ");"
                      " text-decoration: underline;"
                      " }";
        std::string css = qss.toStdString();
        label->setStyleSheet(qss);
      }
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

//-----------------------------------------------------------------------------

void MotionPathPanel::updateTools() {
  TTool* tool                = TApp::instance()->getCurrentTool()->getTool();
  TPropertyGroup* properties = 0;
  if (tool && tool->getName() == "T_Geometric") {
    m_geometryOptionsBox->show();
    properties        = tool->getProperties(0);
    int propertyCount = properties->getPropertyCount();
    for (int i = 0; i < propertyCount; i++) {
      TProperty* prop = properties->getProperty(i);
      std::string id  = prop->getId();
      if (id == "GeometricEdge") {
        TIntProperty* sidesProp = static_cast<TIntProperty*>(prop);
        m_geometrySidesField->setValue(sidesProp->getValue());
      }
      if (id == "GeometricShape") {
        TEnumProperty* geomProp           = static_cast<TEnumProperty*>(prop);
        const TEnumProperty::Range& range = geomProp->getRange();
        int index                         = geomProp->getIndex();
        bool polygonEnabled               = range[index] == L"Polygon";
        std::string value                 = geomProp->getValueAsString();
        if (polygonEnabled) {
          m_geometrySidesField->show();
        } else
          m_geometrySidesField->hide();
        m_geometryOptionsBox->setCurrentIndex(index);
      }
    }
  } else {
    m_geometryOptionsBox->hide();
    m_geometrySidesField->hide();
  }
}

//-----------------------------------------------------------------------------

void MotionPathPanel::onGeometricComboChanged(int index) {
  ToolHandle* th = TApp::instance()->getCurrentTool();
  if (!th) return;
  TTool* tool                = TApp::instance()->getCurrentTool()->getTool();
  TPropertyGroup* properties = 0;
  if (tool && tool->getName() == "T_Geometric") {
    properties        = tool->getProperties(0);
    int propertyCount = properties->getPropertyCount();
    for (int i = 0; i < propertyCount; i++) {
      TProperty* prop = properties->getProperty(i);
      std::string id  = prop->getId();
      if (id == "GeometricShape") {
        TEnumProperty* geomProp           = static_cast<TEnumProperty*>(prop);
        const TEnumProperty::Range& range = geomProp->getRange();
        if (index < 0 || index >= (int)range.size()) return;
        std::wstring item = range[index];
        geomProp->setValue(item);
        tool->onPropertyChanged(geomProp->getName());
      }
    }
    if (index == 7)
      m_geometrySidesField->show();
    else
      m_geometrySidesField->hide();
    th->notifyToolChanged();
  }
}

//-----------------------------------------------------------------------------

void MotionPathPanel::onGeometricSidesChanged(int value) {
  ToolHandle* th = TApp::instance()->getCurrentTool();
  if (!th) return;
  TTool* tool                = TApp::instance()->getCurrentTool()->getTool();
  TPropertyGroup* properties = 0;
  if (tool->getName() == "T_Geometric") {
    properties        = tool->getProperties(0);
    int propertyCount = properties->getPropertyCount();
    for (int i = 0; i < propertyCount; i++) {
      TProperty* prop = properties->getProperty(i);
      std::string id  = prop->getId();
      if (id == "GeometricEdge") {
        TIntProperty* sidesProp = static_cast<TIntProperty*>(prop);
        sidesProp->setValue(m_geometrySidesField->getValue());
        tool->onPropertyChanged(sidesProp->getName());
      }
    }
    th->notifyToolChanged();
  }
}

//-----------------------------------------------------------------------------

void MotionPathPanel::onNextFrame(int) {
  if (!m_currentSpline) {
    m_playbackExecutor.abort();
    return;
  }
  int steps       = m_currentSpline->getSteps();
  int currentStep = m_currentSpline->getCurrentStep();
  if (m_looping)
    m_currentSpline->setCurrentStep(currentStep >= steps - 1 ? 0
                                                             : currentStep + 1);
  else {
    if (currentStep >= steps - 1) {
      m_currentSpline->setCurrentStep(0);
      m_currentSpline->setIsPlaying(false);
      m_looping = false;
      m_playbackExecutor.abort();
    } else
      m_currentSpline->setCurrentStep(currentStep + 1);
  }
  if (TApp::instance()->getActiveViewer())
    TApp::instance()->getActiveViewer()->update();
}
//-----------------------------------------------------------------------------

void MotionPathPanel::stopPlayback() {
  if (TApp::instance()->getActiveViewer())
    TApp::instance()->getActiveViewer()->update();
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

void ClickablePathLabel::mouseDoubleClickEvent(QMouseEvent* event) {
  emit doubleClicked();
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::enterEvent(QEvent*) {}

//-----------------------------------------------------------------------------

void ClickablePathLabel::leaveEvent(QEvent*) {}

//-----------------------------------------------------------------------------

void ClickablePathLabel::setSelected() {}

//-----------------------------------------------------------------------------

void ClickablePathLabel::clearSelected() {}

//=============================================================================

MotionPathPlaybackExecutor::MotionPathPlaybackExecutor()
    : m_fps(25), m_abort(false) {}

//-----------------------------------------------------------------------------

void MotionPathPlaybackExecutor::resetFps(int fps) { m_fps = fps; }

//-----------------------------------------------------------------------------

void MotionPathPlaybackExecutor::run() {
  TStopWatch timer;
  timer.start();

  TUINT32 timeResolution =
      250;  // Use a sufficient sampling resolution (currently 1/4 sec).
            // Fps calculation is made once per sample.

  int fps = m_fps, currSample = 0;
  TUINT32 playedFramesCount = 0;
  TUINT32 loadedInstant, nextSampleInstant = timeResolution;
  TUINT32 sampleTotalLoadingTime = 0;

  TUINT32 lastFrameCounts[4]    = {0, 0, 0,
                                   0};  // Keep the last 4 'played frames' counts.
  TUINT32 lastSampleInstants[4] = {0, 0, 0,
                                   0};  // Same for the last sampling instants
  TUINT32 lastLoadingTimes[4]   = {0, 0, 0,
                                   0};  // Same for total sample loading times

  double targetFrameTime =
      1000.0 / abs(m_fps);      // User-required time between frames

  TUINT32 emissionInstant = 0;  // Effective instant in which loading is invoked
  double emissionInstantD = 0.0;  // Double precision version of the above

  double lastLoadingTime = 0.0;   // Mean frame loading time in the last sample

  while (!m_abort) {
    emissionInstant = timer.getTotalTime();

    // Draw the next frame
    if (playedFramesCount)
      emit nextFrame(fps);  // Show the next frame, telling
                            // currently measured fps

    //-------- Each nextFrame() blocks until the frame has been shown ---------

    ++playedFramesCount;
    loadedInstant = timer.getTotalTime();
    sampleTotalLoadingTime += (loadedInstant - emissionInstant);

    // Recalculate data only after the specified time resolution has passed.
    if (loadedInstant > nextSampleInstant) {
      // Sampling instant. Perform calculations.

      // Store values
      TUINT32 framesCount = playedFramesCount - lastFrameCounts[currSample];
      TUINT32 elapsedTime = loadedInstant - lastSampleInstants[currSample];
      double loadingTime =
          (sampleTotalLoadingTime - lastLoadingTimes[currSample]) /
          (double)framesCount;

      lastFrameCounts[currSample]    = playedFramesCount;
      lastSampleInstants[currSample] = loadedInstant;
      lastLoadingTimes[currSample]   = sampleTotalLoadingTime;

      currSample        = (currSample + 1) % 4;
      nextSampleInstant = loadedInstant + timeResolution;

      // Rebuild current fps
      fps             = troundp((1000 * framesCount) / (double)elapsedTime);
      targetFrameTime = 1000.0 / abs(m_fps);  // m_fps could have changed...

      // In case the playback is too slow to keep the required pace, reset the
      // emission timeline.
      // Otherwise, it should be kept as the difference needs to be compensated
      // to get the required fps.
      if ((int)emissionInstant - (int)emissionInstantD >
          20)  // Reset beyond, say, 20 msecs tolerance.
        emissionInstantD = (double)loadedInstant - loadingTime;
      else
        emissionInstantD +=
            lastLoadingTime -
            loadingTime;  // Otherwise, just adapt to the new loading time

      lastLoadingTime = loadingTime;
    }

    // Calculate the new emission instant
    emissionInstant = std::max((int)(emissionInstantD += targetFrameTime), 0);

    // Sleep until the next emission instant has been reached
    while (timer.getTotalTime() < emissionInstant) msleep(1);
  }

  m_abort = false;
  emit(playbackAborted());
}

//-----------------------------------------------------------------------------

class CreateNewSpline : public MenuItemHandler {
public:
  CreateNewSpline() : MenuItemHandler(MI_NewSpline) {}
  void execute() {
    TApp* app          = TApp::instance();
    TXsheetHandle* xsh = app->getCurrentXsheet();
    TObjectHandle* obj = app->getCurrentObject();
    obj->setObjectId(
        xsh->getXsheet()->getStageObjectTree()->getMotionPathViewerId());
    TStageObjectSpline* spline =
        TStageObjectCmd::addNewSpline(xsh, obj, 0, QPointF(0.0, 0.0), true);
    obj->setSplineObject(spline);
    obj->setIsSpline(true, true);
  }
} CreateNewSpline;