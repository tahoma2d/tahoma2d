

#include "pane.h"

// Tnz6 includes
#include "tapp.h"
#include "mainwindow.h"
#include "tenv.h"
#include "saveloadqsettings.h"

#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/tscenehandle.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QPainter>
#include <QStyleOptionDockWidget>
#include <QMouseEvent>
#include <QMainWindow>
#include <QSettings>
#include <QToolBar>
#include <QMap>
#include <QApplication>
#include <QFile>
#include <qdrawutil.h>
#include <assert.h>
#include <QDesktopWidget>
#include <QDialog>
#include <QLineEdit>
#include <QWidgetAction>
#include <QLabel>
#include <QCheckBox>
#include <QGroupBox>

extern TEnv::StringVar EnvSafeAreaName;
extern TEnv::IntVar CameraViewTransparency;
extern TEnv::IntVar IsometricLeftAngle;
extern TEnv::IntVar IsometricRightAngle;
extern TEnv::IntVar IsometricLeftStep;
extern TEnv::IntVar IsometricRightStep;
extern TEnv::IntVar ShowRuleOfThirds;
extern TEnv::IntVar ShowGoldenRatio;
extern TEnv::IntVar ShowIsometricGrid;
extern TEnv::IntVar ShowHorizontalGrid;
extern TEnv::IntVar ShowVerticalGrid;
extern TEnv::IntVar VerticalSpacing;
extern TEnv::IntVar HorizontalSpacing;
extern TEnv::IntVar ShowFieldGuide;
extern TEnv::IntVar GuideOpacity;
extern TEnv::IntVar HorizontalOffset;
extern TEnv::IntVar VerticalOffset;
extern TEnv::IntVar ShowHorizon;
extern TEnv::IntVar HorizonAngle;
extern TEnv::IntVar HorizonStep;
extern TEnv::IntVar HorizonOffset;
extern TEnv::IntVar ShowVanishingPointRays;
extern TEnv::IntVar VanishingPointRayAngles;
extern TEnv::IntVar VanishingPointRayOpacity;

//=============================================================================
// TPanel
//-----------------------------------------------------------------------------

TPanel::TPanel(QWidget *parent, Qt::WindowFlags flags,
               TDockWidget::Orientation orientation)
    : TDockWidget(parent, flags)
    , m_panelType("")
    , m_isMaximizable(true)
    , m_isMaximized(false)
    , m_panelTitleBar(0)
    , m_multipleInstancesAllowed(true) {
  // setFeatures(QDockWidget::DockWidgetMovable |
  // QDockWidget::DockWidgetFloatable);
  // setFloating(false);
  m_panelTitleBar = new TPanelTitleBar(this, orientation);
  setTitleBarWidget(m_panelTitleBar);
  // connect(m_panelTitleBar,SIGNAL(doubleClick()),this,SLOT(onDoubleClick()));
  connect(m_panelTitleBar, SIGNAL(doubleClick(QMouseEvent *)), this,
          SIGNAL(doubleClick(QMouseEvent *)));
  connect(m_panelTitleBar, SIGNAL(closeButtonPressed()), this,
          SLOT(onCloseButtonPressed()));
  setOrientation(orientation);
  setCursor(Qt::ArrowCursor);
}

//-----------------------------------------------------------------------------

TPanel::~TPanel() {
  // On quitting, save the floating panel's geomtry and state in order to
  // restore them when opening the floating panel next time
  if (isFloating()) {
    TFilePath savePath =
        ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
    QSettings settings(QString::fromStdWString(savePath.getWideString()),
                       QSettings::IniFormat);
    settings.beginGroup("Panels");
    settings.beginGroup(QString::fromStdString(m_panelType));
    settings.setValue("geometry", geometry());
    if (SaveLoadQSettings *persistent =
            dynamic_cast<SaveLoadQSettings *>(widget()))
      persistent->save(settings);
  }
}

//-----------------------------------------------------------------------------

void TPanel::paintEvent(QPaintEvent *e) {
  QPainter painter(this);

  if (widget()) {
    QRect dockRect = widget()->geometry();

    dockRect.adjust(0, 0, -1, -1);
    painter.fillRect(dockRect, m_bgcolor);
    painter.setPen(Qt::black);
    painter.drawRect(dockRect);
  }

  if (m_floating && !m_panelTitleBar->isVisible()) {
    m_panelTitleBar->showTitleBar(true);
  }

  painter.end();
}

//-----------------------------------------------------------------------------

void TPanel::onCloseButtonPressed() {
  emit closeButtonPressed();

  // Currently, Toonz panels that get closed indeed just remain hidden -
  // ready to reappair if they are needed again. However, the user expects
  // a new panel to be created - so we just reset the panel here.
  // reset();    //Moved to panel invocation in floatingpanelcommand.cpp

  // Also, remove widget from its dock layout control
  if (parentLayout()) parentLayout()->removeWidget(this);
}

//-----------------------------------------------------------------------------
/*! activate the panel and set focus specified widget when mouse enters
 */
void TPanel::enterEvent(QEvent *event) {
  // Only when Toonz application is active
  QWidget *w = qApp->activeWindow();
  // if (m_floating) {
  //    m_panelTitleBar->showTitleBar(true);
  //}
  if (w) {
    // grab the focus, unless a line-edit is focused currently

    QWidget *focusWidget = qApp->focusWidget();
    if (focusWidget && dynamic_cast<QLineEdit*>(focusWidget)) {
        event->accept();
        return;
    }


    widgetFocusOnEnter();


    // Some panels (e.g. Viewer, StudioPalette, Palette, ColorModel) are
    // activated when mouse enters. Viewer is activatable only when being
    // docked.
    // Active windows will NOT switch when the current active window is dialog.
    if (qobject_cast<QDialog *>(w) == 0 && isActivatableOnEnter())
      activateWindow();
    event->accept();
  } else
    event->accept();
}

//-----------------------------------------------------------------------------
/*! clear focus when mouse leaves
 */
void TPanel::leaveEvent(QEvent *event) { 
    QWidget* focusWidget = qApp->focusWidget();
    if (focusWidget && dynamic_cast<QLineEdit*>(focusWidget)) {
        return;
    }
    widgetClearFocusOnLeave();
}

//-----------------------------------------------------------------------------
/*! load and restore previous geometry and state of the floating panel.
    called from the function OpenFloatingPanel::getOrOpenFloatingPanel()
    in floatingpanelcommand.cpp
*/
void TPanel::restoreFloatingPanelState() {
  TFilePath savePath = ToonzFolder::getMyModuleDir() + TFilePath("popups.ini");
  QSettings settings(QString::fromStdWString(savePath.getWideString()),
                     QSettings::IniFormat);
  settings.beginGroup("Panels");

  if (!settings.childGroups().contains(QString::fromStdString(m_panelType)))
    return;

  settings.beginGroup(QString::fromStdString(m_panelType));

  QRect geom = settings.value("geometry", saveGeometry()).toRect();
  // check if it can be visible in the current screen
  if (!(geom & QApplication::desktop()->availableGeometry(this)).isEmpty())
    setGeometry(geom);
  // load optional settings
  if (SaveLoadQSettings *persistent =
          dynamic_cast<SaveLoadQSettings *>(widget()))
    persistent->load(settings);
}

//=============================================================================
// TPanelTitleBarButton
//-----------------------------------------------------------------------------

TPanelTitleBarButton::TPanelTitleBarButton(QWidget *parent,
                                           const QString &standardPixmapName)
    : QWidget(parent)
    , m_standardPixmap(standardPixmapName)
    , m_standardPixmapName(standardPixmapName)
    , m_rollover(false)
    , m_pressed(false)
    , m_buttonSet(0)
    , m_id(0) {
  setFixedSize(m_standardPixmap.size());
}

//-----------------------------------------------------------------------------

TPanelTitleBarButton::TPanelTitleBarButton(QWidget *parent,
                                           const QPixmap &standardPixmap)
    : QWidget(parent)
    , m_standardPixmap(standardPixmap)
    , m_rollover(false)
    , m_pressed(false)
    , m_buttonSet(0)
    , m_id(0) {
  setFixedSize(m_standardPixmap.size() / m_standardPixmap.devicePixelRatio());
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setButtonSet(TPanelTitleBarButtonSet *buttonSet,
                                        int id) {
  m_buttonSet = buttonSet;
  m_id        = id;
  m_buttonSet->add(this);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::setPressed(bool pressed) {
  if (pressed != m_pressed) {
    m_pressed = pressed;
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::paintEvent(QPaintEvent *event) {
  QPainter painter(this);

  // Create color states for the button
  QPixmap normalPixmap(m_standardPixmap.size());
  QPixmap onPixmap(m_standardPixmap.size());
  QPixmap overPixmap(m_standardPixmap.size());
  normalPixmap.fill(Qt::transparent);
  onPixmap.fill(QColor(getPressedColor()));
  overPixmap.fill(QColor(getOverColor()));

  // Set unique 'pressed' colors if filename contains...
  if (m_standardPixmapName.contains("freeze", Qt::CaseInsensitive)) {
    onPixmap.fill(QColor(getFreezeColor()));
  }
  if (m_standardPixmapName.contains("preview", Qt::CaseInsensitive)) {
    onPixmap.fill(QColor(getPreviewColor()));
  }

  // Compose the state colors
  painter.drawPixmap(
      0, 0, m_pressed ? onPixmap : m_rollover ? overPixmap : normalPixmap);

  // Icon
  QPixmap panePixmap    = recolorPixmap(m_standardPixmap);
  QPixmap paneOffPixmap = setOpacity(panePixmap, 0.8);
  painter.drawPixmap(
      0, 0, m_pressed ? panePixmap : m_rollover ? panePixmap : paneOffPixmap);

  painter.end();
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::mouseMoveEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::enterEvent(QEvent *) {
  if (!m_rollover) {
    m_rollover = true;
    if (!m_pressed) update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::leaveEvent(QEvent *) {
  if (m_rollover) {
    m_rollover = false;
    if (!m_pressed) update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButton::mousePressEvent(QMouseEvent *e) {
  if (m_buttonSet) {
    if (m_pressed) return;
    m_buttonSet->select(this);
  } else {
    m_pressed = !m_pressed;
    emit toggled(m_pressed);
    update();
  }
}

//=============================================================================
// TPanelTitleBarButtonForSafeArea
//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::getSafeAreaNameList(
    QList<QString> &nameList) {
  TFilePath fp                = TEnv::getConfigDir();
  QString currentSafeAreaName = QString::fromStdString(EnvSafeAreaName);

  std::string safeAreaFileName = "safearea.ini";

  while (!TFileStatus(fp + safeAreaFileName).doesExist() && !fp.isRoot() &&
         fp.getParentDir() != TFilePath())
    fp = fp.getParentDir();

  fp = fp + safeAreaFileName;

  if (TFileStatus(fp).doesExist()) {
    QSettings settings(toQString(fp), QSettings::IniFormat);

    // find the current safearea name from the list
    QStringList groups = settings.childGroups();
    for (int g = 0; g < groups.size(); g++) {
      settings.beginGroup(groups.at(g));
      nameList.push_back(settings.value("name", "").toString());
      settings.endGroup();
    }
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::RightButton) {
    m_pressed = !m_pressed;
    emit toggled(m_pressed);
    update();
  }
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);

  QList<QString> safeAreaNameList;
  getSafeAreaNameList(safeAreaNameList);
  for (int i = 0; i < safeAreaNameList.size(); i++) {
    QAction *action = new QAction(safeAreaNameList.at(i), this);
    action->setData(safeAreaNameList.at(i));
    connect(action, SIGNAL(triggered()), this, SLOT(onSetSafeArea()));
    if (safeAreaNameList.at(i) == QString::fromStdString(EnvSafeAreaName)) {
      action->setCheckable(true);
      action->setChecked(true);
    }
    menu.addAction(action);
  }

  menu.exec(e->globalPos());
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForSafeArea::onSetSafeArea() {
  QString safeAreaName = qobject_cast<QAction *>(sender())->data().toString();
  // change safearea if the different one is selected
  if (QString::fromStdString(EnvSafeAreaName) != safeAreaName) {
    EnvSafeAreaName = safeAreaName.toStdString();
    // emit sceneChanged without setting dirty flag
    TApp::instance()->getCurrentScene()->notifySceneChanged(false);
  }
}

//-----------------------------------------------------------------------------

TPanelTitleBarButtonForCameraView::TPanelTitleBarButtonForCameraView(
    QWidget *parent, const QString &standardPixmapName)
    : TPanelTitleBarButton(parent, standardPixmapName) {
  m_menu                          = new QMenu(this);
  QWidgetAction *sliderAction     = new QWidgetAction(this);
  QWidget *transparencyWidget     = new QWidget(this);
  QHBoxLayout *transparencyLayout = new QHBoxLayout(this);
  transparencyLayout->addWidget(new QLabel(tr("Opacity: ")));
  QSlider *transparencySlider = new QSlider(this);
  transparencySlider->setRange(20, 100);
  transparencySlider->setValue(CameraViewTransparency);
  transparencySlider->setOrientation(Qt::Horizontal);
  connect(transparencySlider, &QSlider::valueChanged, [=](int value) {
    CameraViewTransparency = value;
    emit updateViewer();
  });
  transparencyLayout->addWidget(transparencySlider);
  transparencyWidget->setLayout(transparencyLayout);
  sliderAction->setDefaultWidget(transparencyWidget);
  m_menu->addAction(sliderAction);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForCameraView::mousePressEvent(QMouseEvent *e) {
  m_menu->exec(e->globalPos() + QPoint(-20, 10));
}

//-----------------------------------------------------------------------------

TPanelTitleBarButtonForGrids::TPanelTitleBarButtonForGrids(
    QWidget *parent, const QString &standardPixmapName)
    : TPanelTitleBarButton(parent, standardPixmapName) {
  m_menu = new QMenu(this);

  QWidgetAction *gridsAction = new QWidgetAction(this);
  QWidget *gridWidget        = new QWidget(this);
  QGridLayout *gridLayout    = new QGridLayout(this);

  QCheckBox *thirdsCheckbox = new QCheckBox(tr("Rule of Thirds"), this);
  thirdsCheckbox->setChecked(ShowRuleOfThirds != 0);
  connect(thirdsCheckbox, &QCheckBox::stateChanged, [=](int value) {
    ShowRuleOfThirds = value > 0 ? 1 : 0;
    emit updateViewer();
  });
  QCheckBox *goldenRationCheckbox = new QCheckBox(tr("Golden Ratio"), this);
  goldenRationCheckbox->setChecked(ShowGoldenRatio != 0);
  connect(goldenRationCheckbox, &QCheckBox::stateChanged, [=](int value) {
    ShowGoldenRatio = value > 0 ? 1 : 0;
    emit updateViewer();
  });

  QCheckBox *fieldGuideCheckbox = new QCheckBox(tr("Field Guide"), this);
  fieldGuideCheckbox->setChecked(ShowFieldGuide != 0);
  connect(fieldGuideCheckbox, &QCheckBox::stateChanged, [=](int value) {
    ShowFieldGuide = value > 0 ? 1 : 0;
    emit updateViewer();
  });

  QGroupBox* vanishingCheckbox = new QGroupBox(tr("Vanishing Point Rays"), this);
  vanishingCheckbox->setCheckable(true);
  vanishingCheckbox->setChecked(ShowVanishingPointRays != 0);
  connect(vanishingCheckbox, &QGroupBox::toggled, [=](bool value) {
      ShowVanishingPointRays = value == true ? 1 : 0;
      emit updateViewer();
      });

  QSlider* vanishingAngleSlider = new QSlider(this);
  vanishingAngleSlider->setRange(5, 90);
  vanishingAngleSlider->setValue(VanishingPointRayAngles);
  vanishingAngleSlider->setOrientation(Qt::Horizontal);
  vanishingAngleSlider->setMinimumWidth(300);
  QLabel* vanishingAngleLabel = new QLabel(this);
  vanishingAngleLabel->setText(tr("Angle: ") +
      QString::number(VanishingPointRayAngles));
  connect(vanishingAngleSlider, &QSlider::valueChanged, [=](int value) {
      VanishingPointRayAngles = value;
      vanishingAngleLabel->setText(tr("Angle: ") +
          QString::number(VanishingPointRayAngles));
      emit updateViewer();
      });

  QSlider* vanishingOpacitySlider = new QSlider(this);
  vanishingOpacitySlider->setRange(1, 100);
  vanishingOpacitySlider->setValue(VanishingPointRayOpacity);
  vanishingOpacitySlider->setOrientation(Qt::Horizontal);
  QLabel* vanishingOpacityLabel = new QLabel(this);
  vanishingOpacityLabel->setText(tr("Opacity: ") +
      QString::number(VanishingPointRayOpacity));
  connect(vanishingOpacitySlider, &QSlider::valueChanged, [=](int value) {
      VanishingPointRayOpacity = value;
      vanishingOpacityLabel->setText(tr("Opacity: ") +
          QString::number(VanishingPointRayOpacity));
      emit updateViewer();
      });

  QGridLayout* vanishingLayout = new QGridLayout(this);
  vanishingLayout->addWidget(vanishingAngleLabel, 0, 0, Qt::AlignRight);
  vanishingAngleLabel->setFixedWidth(110);
  vanishingAngleLabel->setAlignment(Qt::AlignRight);
  vanishingLayout->addWidget(vanishingAngleSlider, 0, 1);
  vanishingLayout->addWidget(vanishingOpacityLabel, 1, 0, Qt::AlignRight);
  vanishingLayout->addWidget(vanishingOpacitySlider, 1, 1);
  vanishingCheckbox->setLayout(vanishingLayout);

  QGroupBox *horizontalCheckbox = new QGroupBox(tr("Horizontal Grid"), this);
  horizontalCheckbox->setCheckable(true);
  horizontalCheckbox->setChecked(ShowHorizontalGrid != 0);
  connect(horizontalCheckbox, &QGroupBox::toggled, [=](bool value) {
    ShowHorizontalGrid = value == true ? 1 : 0;
    emit updateViewer();
  });

  QSlider *horizontalSpacingSlider = new QSlider(this);
  horizontalSpacingSlider->setRange(10, 250);
  horizontalSpacingSlider->setValue(HorizontalSpacing);
  horizontalSpacingSlider->setOrientation(Qt::Horizontal);
  QLabel *horizontalSpacingLabel = new QLabel(this);
  horizontalSpacingLabel->setText(tr("Spacing: ") +
                                  QString::number(HorizontalSpacing));
  connect(horizontalSpacingSlider, &QSlider::valueChanged, [=](int value) {
    HorizontalSpacing = value;
    horizontalSpacingLabel->setText(tr("Spacing: ") +
                                    QString::number(HorizontalSpacing));
    emit updateViewer();
  });

  QSlider *horizontalOffsetSlider = new QSlider(this);
  horizontalOffsetSlider->setRange(-100, 100);
  horizontalOffsetSlider->setValue(HorizontalOffset);
  horizontalOffsetSlider->setOrientation(Qt::Horizontal);
  QLabel *horizontalOffsetLabel = new QLabel(this);
  horizontalOffsetLabel->setText(tr("Offset: ") +
                                 QString::number(HorizontalOffset));
  connect(horizontalOffsetSlider, &QSlider::valueChanged, [=](int value) {
    HorizontalOffset = value;
    horizontalOffsetLabel->setText(tr("Offset: ") +
                                   QString::number(HorizontalOffset));
    emit updateViewer();
  });

  QGridLayout *horizontalLayout = new QGridLayout(this);
  horizontalLayout->addWidget(horizontalSpacingLabel, 0, 0, Qt::AlignRight);
  horizontalSpacingLabel->setFixedWidth(110);
  horizontalSpacingLabel->setAlignment(Qt::AlignRight);
  horizontalLayout->addWidget(horizontalSpacingSlider, 0, 1);
  horizontalLayout->addWidget(horizontalOffsetLabel, 1, 0, Qt::AlignRight);
  horizontalLayout->addWidget(horizontalOffsetSlider, 1, 1);
  horizontalCheckbox->setLayout(horizontalLayout);

  QGroupBox *verticalCheckbox = new QGroupBox(tr("Vertical Grid"), this);
  verticalCheckbox->setCheckable(true);
  verticalCheckbox->setChecked(ShowVerticalGrid != 0);
  connect(verticalCheckbox, &QGroupBox::toggled, [=](bool value) {
    ShowVerticalGrid = value == true ? 1 : 0;
    emit updateViewer();
  });

  QSlider *verticalSpacingSlider = new QSlider(this);
  verticalSpacingSlider->setRange(10, 250);
  verticalSpacingSlider->setValue(VerticalSpacing);
  verticalSpacingSlider->setOrientation(Qt::Horizontal);
  verticalSpacingSlider->setMinimumWidth(300);
  QLabel *verticalSpacingLabel = new QLabel(this);
  verticalSpacingLabel->setText(tr("Spacing: ") +
                                QString::number(VerticalSpacing));
  connect(verticalSpacingSlider, &QSlider::valueChanged, [=](int value) {
    VerticalSpacing = value;
    verticalSpacingLabel->setText(tr("Spacing: ") +
                                  QString::number(VerticalSpacing));
    emit updateViewer();
  });

  QSlider *verticalOffsetSlider = new QSlider(this);
  verticalOffsetSlider->setRange(-100, 100);
  verticalOffsetSlider->setValue(VerticalOffset);
  verticalOffsetSlider->setOrientation(Qt::Horizontal);
  QLabel *verticalOffsetLabel = new QLabel(this);
  verticalOffsetLabel->setText(tr("Offset: ") +
                               QString::number(VerticalOffset));
  connect(verticalOffsetSlider, &QSlider::valueChanged, [=](int value) {
    VerticalOffset = value;
    verticalOffsetLabel->setText(tr("Offset: ") +
                                 QString::number(VerticalOffset));
    emit updateViewer();
  });

  QGridLayout *verticalLayout = new QGridLayout(this);
  verticalLayout->addWidget(verticalSpacingLabel, 0, 0, Qt::AlignRight);
  verticalSpacingLabel->setFixedWidth(110);
  verticalSpacingLabel->setAlignment(Qt::AlignRight);
  verticalLayout->addWidget(verticalSpacingSlider, 0, 1);
  verticalLayout->addWidget(verticalOffsetLabel, 1, 0, Qt::AlignRight);
  verticalLayout->addWidget(verticalOffsetSlider, 1, 1);
  verticalCheckbox->setLayout(verticalLayout);

  
    QGroupBox* horizonCheckbox = new QGroupBox(tr("Horizon"), this);
    horizonCheckbox->setCheckable(true);
    horizonCheckbox->setChecked(ShowHorizon != 0);
    connect(horizonCheckbox, &QGroupBox::toggled, [=](bool value) {
        ShowHorizon = value == true ? 1 : 0;
        emit updateViewer();
        });

    QSlider* horizonAngleSlider = new QSlider(this);
    horizonAngleSlider->setRange(-90, 90);
    horizonAngleSlider->setValue(HorizonAngle);
    horizonAngleSlider->setOrientation(Qt::Horizontal);
    horizonAngleSlider->setMinimumWidth(300);
    QLabel* horizonAngleLabel = new QLabel(this);
    horizonAngleLabel->setText(tr("Angle: ") +
        QString::number(HorizonAngle));
    connect(horizonAngleSlider, &QSlider::valueChanged, [=](int value) {
        HorizonAngle = value;
        horizonAngleLabel->setText(tr("Angle: ") +
            QString::number(HorizonAngle));
        emit updateViewer();
        });

    QSlider* horizonStepSlider = new QSlider(this);
    horizonStepSlider->setRange(2, 100);
    horizonStepSlider->setValue(HorizonStep);
    horizonStepSlider->setOrientation(Qt::Horizontal);
    QLabel* horizonStepLabel = new QLabel(this);
    horizonStepLabel->setText(tr("Step: ") +
        QString::number(HorizonStep));
    connect(horizonStepSlider, &QSlider::valueChanged, [=](int value) {
        HorizonStep = value;
        horizonStepLabel->setText(tr("Step: ") +
            QString::number(HorizonStep));
        emit updateViewer();
        });

    QSlider* horizonOffsetSlider = new QSlider(this);
    horizonOffsetSlider->setRange(-1000, 1000);
    horizonOffsetSlider->setValue(HorizonOffset);
    horizonOffsetSlider->setOrientation(Qt::Horizontal);
    QLabel* horizonOffsetLabel = new QLabel(this);
    horizonOffsetLabel->setText(tr("Offset: ") +
        QString::number(HorizonOffset));
    connect(horizonOffsetSlider, &QSlider::valueChanged, [=](int value) {
        HorizonOffset = value;
        horizonOffsetLabel->setText(tr("Offset: ") +
            QString::number(HorizonOffset));
        emit updateViewer();
        });

    QGridLayout* horizonLayout = new QGridLayout(this);
    horizonLayout->addWidget(horizonAngleLabel, 0, 0, Qt::AlignRight);
    horizonAngleLabel->setFixedWidth(110);
    horizonAngleLabel->setAlignment(Qt::AlignRight);
    horizonLayout->addWidget(horizonAngleSlider, 0, 1);
    horizonLayout->addWidget(horizonStepLabel, 1, 0, Qt::AlignRight);
    horizonLayout->addWidget(horizonStepSlider, 1, 1);
    horizonLayout->addWidget(horizonOffsetLabel, 2, 0, Qt::AlignRight);
    horizonLayout->addWidget(horizonOffsetSlider, 2, 1);
    horizonCheckbox->setLayout(horizonLayout);
  

  QGroupBox *isometricCheckbox = new QGroupBox(tr("Isometric Grid"), this);
  isometricCheckbox->setCheckable(true);
  isometricCheckbox->setChecked(ShowIsometricGrid != 0);
  connect(isometricCheckbox, &QGroupBox::toggled, [=](int value) {
    ShowIsometricGrid = value == true ? 1 : 0;
    emit updateViewer();
  });

  QSlider *leftAngleSlider = new QSlider(this);
  leftAngleSlider->setRange(10, 89);
  leftAngleSlider->setValue(IsometricLeftAngle);
  leftAngleSlider->setOrientation(Qt::Horizontal);
  QLabel *leftAngleLabel = new QLabel(this);
  leftAngleLabel->setText(tr("Left Angle: ") +
                          QString::number(IsometricLeftAngle));
  connect(leftAngleSlider, &QSlider::valueChanged, [=](int value) {
    IsometricLeftAngle = value;
    leftAngleLabel->setText(tr("Left Angle: ") +
                            QString::number(IsometricLeftAngle));
    emit updateViewer();
  });

  QSlider *rightAngleSlider = new QSlider(this);
  rightAngleSlider->setRange(10, 89);
  rightAngleSlider->setValue(IsometricRightAngle);
  rightAngleSlider->setOrientation(Qt::Horizontal);
  QLabel *rightAngleLabel = new QLabel(this);
  rightAngleLabel->setText(tr("Right Angle: ") +
                           QString::number(IsometricRightAngle));
  connect(rightAngleSlider, &QSlider::valueChanged, [=](int value) {
    IsometricRightAngle = value;
    rightAngleLabel->setText(tr("Right Angle: ") +
                             QString::number(IsometricRightAngle));
    emit updateViewer();
  });

  QSlider *leftStepSlider = new QSlider(this);
  leftStepSlider->setRange(1, 100);
  leftStepSlider->setValue(IsometricLeftStep / 5);
  leftStepSlider->setOrientation(Qt::Horizontal);
  QLabel *leftStepLabel = new QLabel(this);
  leftStepLabel->setText(tr("Left Spacing: ") +
                         QString::number(IsometricLeftStep));
  connect(leftStepSlider, &QSlider::valueChanged, [=](int value) {
    IsometricLeftStep = value * 5;
    leftStepLabel->setText(tr("Left Spacing: ") +
                           QString::number(IsometricLeftStep));
    emit updateViewer();
  });

  QSlider *rightStepSlider = new QSlider(this);
  rightStepSlider->setRange(1, 100);
  rightStepSlider->setValue(IsometricRightStep / 5);
  rightStepSlider->setOrientation(Qt::Horizontal);
  QLabel *rightStepLabel = new QLabel(this);
  rightStepLabel->setText(tr("Right Spacing: ") +
                          QString::number(IsometricRightStep));
  connect(rightStepSlider, &QSlider::valueChanged, [=](int value) {
    IsometricRightStep = value * 5;
    rightStepLabel->setText(tr("Right Spacing: ") +
                            QString::number(IsometricRightStep));
    emit updateViewer();
  });

  QGridLayout *isometricLayout = new QGridLayout(this);
  leftAngleLabel->setFixedWidth(110);
  leftAngleLabel->setAlignment(Qt::AlignRight);
  isometricLayout->addWidget(leftAngleLabel, 0, 0, Qt::AlignRight);
  isometricLayout->addWidget(leftAngleSlider, 0, 1);
  isometricLayout->addWidget(leftStepLabel, 1, 0, Qt::AlignRight);
  isometricLayout->addWidget(leftStepSlider, 1, 1);
  isometricLayout->addWidget(rightAngleLabel, 2, 0, Qt::AlignRight);
  isometricLayout->addWidget(rightAngleSlider, 2, 1);
  isometricLayout->addWidget(rightStepLabel, 3, 0, Qt::AlignRight);
  isometricLayout->addWidget(rightStepSlider, 3, 1);

  isometricCheckbox->setLayout(isometricLayout);

  QSlider *guideOpacitySlider = new QSlider(this);
  guideOpacitySlider->setRange(10, 100);
  guideOpacitySlider->setValue(GuideOpacity);
  guideOpacitySlider->setOrientation(Qt::Horizontal);
  QLabel *guideOpacityLabel = new QLabel(this);
  guideOpacityLabel->setText(tr("Opacity: ") + QString::number(GuideOpacity));
  connect(guideOpacitySlider, &QSlider::valueChanged, [=](int value) {
    GuideOpacity = value;
    guideOpacityLabel->setText(tr("Opacity: ") + QString::number(GuideOpacity));
    emit updateViewer();
  });

  gridLayout->addWidget(thirdsCheckbox, 0, 0, 1, 2);
  gridLayout->addWidget(goldenRationCheckbox, 1, 0, 1, 2);
  gridLayout->addWidget(fieldGuideCheckbox, 2, 0, 1, 2);
  gridLayout->addWidget(horizontalCheckbox, 3, 0, 1, 2);
  gridLayout->addWidget(verticalCheckbox, 4, 0, 1, 2);

  gridLayout->addWidget(horizonCheckbox, 5, 0, 1, 2);
  gridLayout->addWidget(isometricCheckbox, 6, 0, 1, 2);
  gridLayout->addWidget(vanishingCheckbox, 7, 0, 1, 2);
  
  gridLayout->addWidget(guideOpacityLabel, 8, 0);
  gridLayout->addWidget(guideOpacitySlider, 8, 1);
  gridWidget->setLayout(gridLayout);
  gridsAction->setDefaultWidget(gridWidget);
  m_menu->addAction(gridsAction);
}

//-----------------------------------------------------------------------------

void TPanelTitleBarButtonForGrids::mousePressEvent(QMouseEvent *e) {
  m_menu->exec(e->globalPos() + QPoint(-100, 12));
}

//=============================================================================
// TPanelTitleBarButtonSet
//-----------------------------------------------------------------------------

TPanelTitleBarButtonSet::TPanelTitleBarButtonSet() {}

TPanelTitleBarButtonSet::~TPanelTitleBarButtonSet() {}

void TPanelTitleBarButtonSet::add(TPanelTitleBarButton *button) {
  m_buttons.push_back(button);
}

void TPanelTitleBarButtonSet::select(TPanelTitleBarButton *button) {
  int i;
  for (i = 0; i < (int)m_buttons.size(); i++)
    m_buttons[i]->setPressed(button == m_buttons[i]);
  emit selected(button->getId());
}

//=============================================================================
// PaneTitleBar
//-----------------------------------------------------------------------------

TPanelTitleBar::TPanelTitleBar(QWidget *parent,
                               TDockWidget::Orientation orientation)
    : QFrame(parent), m_closeButtonHighlighted(false) {
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);
  setCursor(Qt::ArrowCursor);
}

//-----------------------------------------------------------------------------

QSize TPanelTitleBar::minimumSizeHint() const { return QSize(20, 18); }

//-----------------------------------------------------------------------------

void TPanelTitleBar::paintEvent(QPaintEvent *) {
  QPainter painter(this);
  QRect rect = this->rect();

  bool isPanelActive;

  TPanel *dw = qobject_cast<TPanel *>(parentWidget());
  Q_ASSERT(dw != 0);
  // docked panel
  if (!dw->isFloating()) {
    isPanelActive = dw->widgetInThisPanelIsFocused();
    qDrawBorderPixmap(&painter, rect, QMargins(3, 3, 3, 3),
                      (isPanelActive) ? m_activeBorderPm : m_borderPm);
  }
  // floating panel
  else {
    isPanelActive = isActiveWindow();
    qDrawBorderPixmap(
        &painter, rect, QMargins(3, 3, 3, 3),
        (isPanelActive) ? m_floatActiveBorderPm : m_floatBorderPm);
  }

  if (dw->getOrientation() == TDockWidget::vertical) {
    QString titleText = painter.fontMetrics().elidedText(
        dw->windowTitle(), Qt::ElideRight, rect.width() - 50);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(isPanelActive ? m_activeTitleColor : m_titleColor);
    painter.drawText(QPointF(8, 13), titleText);
  }

  if (dw->isFloating()) {
    QIcon paneCloseIcon = createQIcon("pane_close");
    const static QPixmap closeButtonPixmap(
        paneCloseIcon.pixmap(20, 18, QIcon::Normal, QIcon::Off));
    const static QPixmap closeButtonPixmapOver(
        paneCloseIcon.pixmap(20, 18, QIcon::Active));

    QPoint closeButtonPos(rect.right() - 20, rect.top());

    if (m_closeButtonHighlighted)
      painter.drawPixmap(closeButtonPos, closeButtonPixmapOver);
    else
      painter.drawPixmap(closeButtonPos, closeButtonPixmap);
  }

  painter.end();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::mousePressEvent(QMouseEvent *event) {
  TDockWidget *dw = static_cast<TDockWidget *>(parentWidget());

  QPoint pos = event->pos();

  if (dw->isFloating()) {
    QRect rect = this->rect();
    QRect closeButtonRect(rect.right() - 20, rect.top() + 1, 20, 18);
    if (closeButtonRect.contains(pos) && dw->isFloating()) {
      event->accept();
      dw->hide();
      m_closeButtonHighlighted = false;
      emit closeButtonPressed();
      return;
    }
  }
  event->ignore();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::mouseMoveEvent(QMouseEvent *event) {
  TDockWidget *dw = static_cast<TDockWidget *>(parentWidget());

  if (dw->isFloating()) {
    QPoint pos = event->pos();
    QRect rect = this->rect();
    QRect closeButtonRect(rect.right() - 18, rect.top() + 1, 18, 18);

    if (closeButtonRect.contains(pos) && dw->isFloating())
      m_closeButtonHighlighted = true;
    else
      m_closeButtonHighlighted = false;
  }

  update();
  event->ignore();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::mouseDoubleClickEvent(QMouseEvent *me) {
  emit doubleClick(me);
  me->ignore();
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::add(const QPoint &pos, QWidget *widget) {
  m_buttons.push_back(std::make_pair(pos, widget));
}

//-----------------------------------------------------------------------------

void TPanelTitleBar::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  int i;
  for (i = 0; i < (int)m_buttons.size(); i++) {
    QPoint p   = m_buttons[i].first;
    QWidget *w = m_buttons[i].second;
    if (p.x() < 0) p.setX(p.x() + width());
    w->move(p);
  }
}

//=============================================================================
// TPanelFactory
//-----------------------------------------------------------------------------

TPanelFactory::TPanelFactory(QString panelType) : m_panelType(panelType) {
  assert(tableInstance().count(panelType) == 0);
  tableInstance()[m_panelType] = this;
}

//-----------------------------------------------------------------------------

TPanelFactory::~TPanelFactory() { tableInstance().remove(m_panelType); }

//-----------------------------------------------------------------------------

QMap<QString, TPanelFactory *> &TPanelFactory::tableInstance() {
  static QMap<QString, TPanelFactory *> table;
  return table;
}

//-----------------------------------------------------------------------------

TPanel *TPanelFactory::createPanel(QWidget *parent, QString panelType) {
  TPanel *panel = 0;

  QMap<QString, TPanelFactory *>::iterator it = tableInstance().find(panelType);
  if (it == tableInstance().end()) {
    TPanel *panel = new TPanel(parent);
    panel->setPanelType(panelType.toStdString());
    return panel;
  } else {
    TPanelFactory *factory = it.value();
    TPanel *panel          = factory->createPanel(parent);
    panel->setPanelType(panelType.toStdString());
    return panel;
  }
}

//-----------------------------------------------------------------------------

TPanel *TPanelFactory::createPanel(QWidget *parent) {
  TPanel *panel = new TPanel(parent);
  panel->setObjectName(getPanelType());
  panel->setWindowTitle(getPanelType());
  initialize(panel);
  return panel;
}

//-----------------------------------------------------------------------------
