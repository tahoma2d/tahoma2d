

#include "preferencespopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "versioncontrol.h"
#include "levelsettingspopup.h"
#include "tapp.h"
#include "cleanupsettingsmodel.h"
#include "formatsettingspopups.h"
#include "tenv.h"
#include "mainwindow.h"
#include "columncommand.h"

// TnzQt includes
#include "toonzqt/tabbar.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/filefield.h"
#include "toonzqt/lutcalibrator.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshleveltypes.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/levelproperties.h"
#include "toonz/sceneproperties.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/stage.h"
#include "toonz/toonzfolders.h"
#include "viewereventlogpopup.h"

// TnzCore includes
#include "tsystem.h"
#include "tfont.h"

#include "kis_tablet_support_win8.h"

// Qt includes
#include <QHBoxLayout>
#include <QComboBox>
#include <QFontComboBox>
#include <QLabel>
#include <QStackedWidget>
#include <QLineEdit>
#include <QFileDialog>
#include <QFile>
#include <QPushButton>
#include <QApplication>
#include <QMainWindow>
#include <QStringList>
#include <QListWidget>
#include <QGroupBox>
#include <QCheckBox>

using namespace DVGui;

//*******************************************************************************************
//    Local namespace
//*******************************************************************************************

namespace {
enum DpiPolicy { DP_ImageDpi, DP_CustomDpi };

inline void setupLayout(QGridLayout* lay, int margin = 15) {
  lay->setContentsMargins(margin, margin, margin, margin);
  lay->setHorizontalSpacing(5);
  lay->setVerticalSpacing(10);
  lay->setColumnStretch(2, 1);
}

QGridLayout* insertGroupBox(const QString label, QGridLayout* layout) {
  QGroupBox* box   = new QGroupBox(label);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay, 5);
  box->setLayout(lay);
  layout->addWidget(box, layout->rowCount(), 0, 1, 3);
  return lay;
}

inline TPixel colorToTPixel(const QColor& color) {
  return TPixel(color.red(), color.green(), color.blue(), color.alpha());
}
}  // namespace

//-----------------------------------------------------------------------------

SizeField::SizeField(QSize min, QSize max, QSize value, QWidget* parent)
    : QWidget(parent) {
  m_fieldX =
      new DVGui::IntLineEdit(this, value.width(), min.width(), max.width());
  m_fieldY =
      new DVGui::IntLineEdit(this, value.height(), min.height(), max.height());
  QHBoxLayout* lay = new QHBoxLayout();
  lay->setSpacing(5);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->addWidget(m_fieldX, 1);
  lay->addWidget(new QLabel("x", this), 0);
  lay->addWidget(m_fieldY, 1);
  lay->addStretch(1);
  setLayout(lay);

  bool ret = true;
  ret      = ret && connect(m_fieldX, SIGNAL(editingFinished()), this,
                            SIGNAL(editingFinished()));
  ret      = ret && connect(m_fieldY, SIGNAL(editingFinished()), this,
                            SIGNAL(editingFinished()));
  assert(ret);
}

QSize SizeField::getValue() const {
  return QSize(m_fieldX->getValue(), m_fieldY->getValue());
}

void SizeField::setValue(const QSize& size) {
  m_fieldX->setValue(size.width());
  m_fieldY->setValue(size.height());
}

//**********************************************************************************
//    PreferencesPopup::FormatProperties  implementation
//**********************************************************************************

PreferencesPopup::FormatProperties::FormatProperties(PreferencesPopup* parent)
    : DVGui::Dialog(parent, false, true) {
  setWindowTitle(tr("Level Settings by File Format"));
  setModal(true);  // The underlying selected format should not
  // be changed by the user

  // Main layout
  beginVLayout();

  QGridLayout* gridLayout = new QGridLayout;
  int row                 = 0;

  // Key values
  QLabel* nameLabel = new QLabel(tr("Name:"));
  nameLabel->setFixedHeight(20);  // Due to DVGui::Dialog's quirky behavior
  gridLayout->addWidget(nameLabel, row, 0, Qt::AlignRight);

  m_name = new DVGui::LineEdit;
  gridLayout->addWidget(m_name, row++, 1);

  QLabel* regExpLabel = new QLabel(tr("Regular Expression:"));
  gridLayout->addWidget(regExpLabel, row, 0, Qt::AlignRight);

  m_regExp = new DVGui::LineEdit;
  gridLayout->addWidget(m_regExp, row++, 1);

  QLabel* priorityLabel = new QLabel(tr("Priority"));
  gridLayout->addWidget(priorityLabel, row, 0, Qt::AlignRight);

  m_priority = new DVGui::IntLineEdit;
  gridLayout->addWidget(m_priority, row++, 1);

  gridLayout->setRowMinimumHeight(row++, 20);

  // LevelProperties
  m_dpiPolicy = new QComboBox;
  gridLayout->addWidget(m_dpiPolicy, row++, 1);

  m_dpiPolicy->addItem(QObject::tr("Image DPI"));
  m_dpiPolicy->addItem(QObject::tr("Custom DPI"));

  QLabel* dpiLabel = new QLabel(LevelSettingsPopup::tr("DPI:"));
  gridLayout->addWidget(dpiLabel, row, 0, Qt::AlignRight);

  m_dpi = new DVGui::DoubleLineEdit;
  m_dpi->setRange(1, (std::numeric_limits<double>::max)());  // Tried
                                                             // limits::min(),
                                                             // but input 0 was
  gridLayout->addWidget(m_dpi, row++,
                        1);  // then replaced with something * e^-128

  m_premultiply = new DVGui::CheckBox(LevelSettingsPopup::tr("Premultiply"));
  gridLayout->addWidget(m_premultiply, row++, 1);

  m_whiteTransp =
      new DVGui::CheckBox(LevelSettingsPopup::tr("White As Transparent"));
  gridLayout->addWidget(m_whiteTransp, row++, 1);

  m_doAntialias =
      new DVGui::CheckBox(LevelSettingsPopup::tr("Add Antialiasing"));
  gridLayout->addWidget(m_doAntialias, row++, 1);

  QLabel* antialiasLabel =
      new QLabel(LevelSettingsPopup::tr("Antialias Softness:"));
  gridLayout->addWidget(antialiasLabel, row, 0, Qt::AlignRight);

  m_antialias = new DVGui::IntLineEdit(
      this, 10, 0, 100);  // Tried 1, but then m_doAntialias was forcedly
  gridLayout->addWidget(m_antialias, row++, 1);  // initialized to true

  QLabel* subsamplingLabel = new QLabel(LevelSettingsPopup::tr("Subsampling:"));
  gridLayout->addWidget(subsamplingLabel, row, 0, Qt::AlignRight);

  m_subsampling = new DVGui::IntLineEdit(this, 1, 1);
  gridLayout->addWidget(m_subsampling, row++, 1);

  QLabel* gammaLabel = new QLabel(LevelSettingsPopup::tr("Color Space Gamma:"));
  gridLayout->addWidget(gammaLabel, row, 0, Qt::AlignRight);

  m_colorSpaceGamma = new DVGui::DoubleLineEdit(this);
  m_colorSpaceGamma->setRange(0.1, 10.);
  gridLayout->addWidget(m_colorSpaceGamma, row++, 1);

  addLayout(gridLayout);

  endVLayout();

  // Establish connections
  bool ret = true;

  // enable gamma field only when the regexp field contains ".exr"
  ret = connect(m_regExp, SIGNAL(editingFinished()),
                SLOT(updateEnabledStatus())) &&
        ret;
  ret = connect(m_dpiPolicy, SIGNAL(currentIndexChanged(int)),
                SLOT(updateEnabledStatus())) &&
        ret;
  ret =
      connect(m_doAntialias, SIGNAL(clicked()), SLOT(updateEnabledStatus())) &&
      ret;

  assert(ret);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::FormatProperties::updateEnabledStatus() {
  m_dpi->setEnabled(m_dpiPolicy->currentIndex() == DP_CustomDpi);
  m_antialias->setEnabled(m_doAntialias->isChecked());

  // enable gamma field only when the regexp field contains ".exr"
  m_colorSpaceGamma->setEnabled(m_regExp->text().contains(".exr"));
}

//-----------------------------------------------------------------------------

void PreferencesPopup::FormatProperties::setLevelFormat(
    const Preferences::LevelFormat& lf) {
  const LevelOptions& lo = lf.m_options;

  m_name->setText(lf.m_name);
  m_regExp->setText(lf.m_pathFormat.pattern());
  m_priority->setValue(lf.m_priority);

  m_dpiPolicy->setCurrentIndex(
      lo.m_dpiPolicy == LevelOptions::DP_ImageDpi ? DP_ImageDpi : DP_CustomDpi);
  m_dpi->setValue(lo.m_dpi);
  m_premultiply->setChecked(lo.m_premultiply);
  m_whiteTransp->setChecked(lo.m_whiteTransp);
  m_doAntialias->setChecked(lo.m_antialias > 0);
  m_antialias->setValue(lo.m_antialias);
  m_subsampling->setValue(lo.m_subsampling);
  m_colorSpaceGamma->setValue(lo.m_colorSpaceGamma);

  updateEnabledStatus();
}

//-----------------------------------------------------------------------------

Preferences::LevelFormat PreferencesPopup::FormatProperties::levelFormat()
    const {
  Preferences::LevelFormat lf(m_name->text());

  // Assign key values
  lf.m_pathFormat.setPattern(m_regExp->text());
  lf.m_priority = m_priority->getValue();

  // Assign level format values
  lf.m_options.m_dpiPolicy   = (m_dpiPolicy->currentIndex() == DP_ImageDpi)
                                   ? LevelOptions::DP_ImageDpi
                                   : LevelOptions::DP_CustomDpi;
  lf.m_options.m_dpi         = m_dpi->getValue();
  lf.m_options.m_subsampling = m_subsampling->getValue();
  lf.m_options.m_antialias =
      m_doAntialias->isChecked() ? m_antialias->getValue() : 0;
  lf.m_options.m_premultiply = m_premultiply->isChecked();
  lf.m_options.m_whiteTransp = m_whiteTransp->isChecked();

  if (m_colorSpaceGamma->isEnabled())
    lf.m_options.m_colorSpaceGamma = m_colorSpaceGamma->getValue();

  return lf;
}

//**********************************************************************************
//   PreferencesPopup::Display30bitCheckerView  implementation
//**********************************************************************************

PreferencesPopup::Display30bitChecker::GLView::GLView(QWidget* parent,
                                                      bool is30bit)
    : QOpenGLWidget(parent), m_is30bit(is30bit) {
  setFixedSize(500, 100);
  if (m_is30bit) setTextureFormat(TGL_TexFmt10);
}

void PreferencesPopup::Display30bitChecker::GLView::initializeGL() {
  initializeOpenGLFunctions();
  glClear(GL_COLOR_BUFFER_BIT);
}
void PreferencesPopup::Display30bitChecker::GLView::resizeGL(int width,
                                                             int height) {
  glViewport(0, 0, width, height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, 1, 0, 1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}
void PreferencesPopup::Display30bitChecker::GLView::paintGL() {
  initializeOpenGLFunctions();
  glPushMatrix();
  glBegin(GL_QUADS);
  glColor3d(0.071, 0.153, 0.0);
  glVertex3d(0, 0, 0);
  glVertex3d(0, 1, 0);
  glColor3d(0.141, 0.239, 0.0);
  glVertex3d(1, 1, 0);
  glVertex3d(1, 0, 0);
  glEnd();
  glPopMatrix();
}

//-------------------------

PreferencesPopup::Display30bitChecker::Display30bitChecker(
    PreferencesPopup* parent)
    : Dialog(parent) {
  setModal(true);
  m_currentDefaultFormat = QSurfaceFormat::defaultFormat();

  setWindowTitle(tr("Check 30bit display availability"));

  QSurfaceFormat sFmt = m_currentDefaultFormat;
  sFmt.setRedBufferSize(10);
  sFmt.setGreenBufferSize(10);
  sFmt.setBlueBufferSize(10);
  sFmt.setAlphaBufferSize(2);
  QSurfaceFormat::setDefaultFormat(sFmt);

  GLView* view8bit      = new GLView(this, false);
  GLView* view10bit     = new GLView(this, true);
  QPushButton* closeBtn = new QPushButton(tr("Close"), this);
  QString infoLabel     = tr(
      "If the lower gradient looks smooth and has no banding compared to the upper gradient,\n\
30bit display is available in the current configuration.");

  QVBoxLayout* lay = new QVBoxLayout();
  lay->setContentsMargins(10, 10, 10, 10);;
  lay->setSpacing(10);
  {
    lay->addWidget(view8bit);
    lay->addWidget(view10bit);
    lay->addWidget(new QLabel(infoLabel, this));
    lay->addWidget(closeBtn, 0, Qt::AlignCenter);
  }
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->addLayout(lay);
  lay->setSizeConstraint(QLayout::SetFixedSize);

  connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
}

PreferencesPopup::Display30bitChecker::~Display30bitChecker() {
  QSurfaceFormat::setDefaultFormat(m_currentDefaultFormat);
}

//**********************************************************************************
//    PreferencesPopup::AdditionalStyleEdit  implementation
//**********************************************************************************

PreferencesPopup::AdditionalStyleEdit::AdditionalStyleEdit(
    PreferencesPopup* parent)
    : DVGui::Dialog(parent, true, false, "AdditionalStyleEdit") {
  setWindowTitle(tr("Additional Style Sheet"));
  setModal(true);

  m_edit                   = new QTextEdit(this);
  QPushButton* okButton    = new QPushButton(tr("OK"), this);
  QPushButton* applyButton = new QPushButton(tr("Apply"), this);
  QPushButton* closeButton = new QPushButton(tr("Close"), this);

  QString placeHolderTxt(
      "/* Type additional style sheet here to customize GUI. \n"
      "   Example: To enlarge the Style Editor buttons */\n\n"
      "#StyleEditor #bottomWidget QPushButton{ \n  padding : 13 21; \n }");
  m_edit->setPlaceholderText(placeHolderTxt);
  m_edit->setAcceptRichText(false);

  m_topLayout->addWidget(m_edit);

  addButtonBarWidget(okButton, applyButton, closeButton);

  bool ret = true;
  ret      = ret && connect(okButton, SIGNAL(pressed()), this, SLOT(onOK()));
  ret = ret && connect(applyButton, SIGNAL(pressed()), this, SLOT(onApply()));
  ret = ret && connect(closeButton, SIGNAL(pressed()), this, SLOT(close()));
}

void PreferencesPopup::AdditionalStyleEdit::showEvent(QShowEvent*) {
  m_edit->setPlainText(Preferences::instance()->getAdditionalStyleSheet());
}

void PreferencesPopup::AdditionalStyleEdit::onOK() {
  onApply();
  close();
}

void PreferencesPopup::AdditionalStyleEdit::onApply() {
  Preferences::instance()->setValue(additionalStyleSheet,
                                    m_edit->toPlainText());
  emit additionalSheetEdited();
}

//**********************************************************************************
//    PreferencesPopup  implementation
//**********************************************************************************

void PreferencesPopup::rebuildFormatsList() {
  const Preferences& prefs = *Preferences::instance();

  m_levelFormatNames->clear();

  int lf, lfCount = prefs.levelFormatsCount();
  for (lf = 0; lf != lfCount; ++lf)
    m_levelFormatNames->addItem(prefs.levelFormat(lf).m_name);

  m_editLevelFormat->setEnabled(m_levelFormatNames->count() > 0);
}

//-----------------------------------------------------------------------------

QList<ComboBoxItem> PreferencesPopup::buildFontStyleList() const {
  TFontManager* instance = TFontManager::instance();
  std::vector<std::wstring> typefaces;
  std::vector<std::wstring>::iterator it;
  QString font  = m_pref->getStringValue(interfaceFont);
  QString style = m_pref->getStringValue(interfaceFontStyle);
  try {
    instance->loadFontNames();
    instance->setFamily(font.toStdWString());
    instance->getAllTypefaces(typefaces);
  } catch (TFontCreationError&) {
    it = typefaces.begin();
    typefaces.insert(it, style.toStdWString());
  } catch (...) {
    it = typefaces.begin();
    typefaces.insert(it, style.toStdWString());
  }
  QList<ComboBoxItem> styleList;
  for (it = typefaces.begin(); it != typefaces.end(); ++it)
    styleList.append(ComboBoxItem(QString::fromStdWString(*it),
                                  QString::fromStdWString(*it)));
  return styleList;
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefaultProjectPathChanged() {
  // emit signal to update behavior of the File browser
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "DefaultProjectPath");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSaveChanged() {
  bool on = getUI<QGroupBox*>(autosaveEnabled)->isChecked();
  if (!on) return;
  CheckBox* autoSaveSceneCB      = getUI<CheckBox*>(autosaveSceneEnabled);
  CheckBox* autoSaveOtherFilesCB = getUI<CheckBox*>(autosaveOtherFilesEnabled);
  if (!autoSaveSceneCB->isChecked() && !autoSaveOtherFilesCB->isChecked()) {
    autoSaveSceneCB->setChecked(true);
    autoSaveOtherFilesCB->setChecked(true);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSaveOptionsChanged() {
  bool autoSaveScene = getUI<CheckBox*>(autosaveSceneEnabled)->isChecked();
  bool autoSaveOtherFiles =
      getUI<CheckBox*>(autosaveOtherFilesEnabled)->isChecked();
  if (!autoSaveScene && !autoSaveOtherFiles) {
    getUI<QGroupBox*>(autosaveEnabled)->setChecked(false);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onWatchFileSystemClicked() {
  // emit signal to update behavior of the File browser
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "WatchFileSystem");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowAdvancedOptionsChanged() {
  CheckBox* showAdvancedOptionsCB = getUI<CheckBox*>(showAdvancedOptions);
  if (!showAdvancedOptionsCB->isChecked()) {
    m_pref->setValue(pixelsOnly, true);
    m_pref->setValue(linearUnits, "pixel");
    m_pref->setValue(cameraUnits, "pixel");
    m_pref->setValue(DefLevelDpi, Stage::standardDpi);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPathAliasPriorityChanged() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "PathAliasPriority");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onStyleSheetTypeChanged() {
  QApplication::setOverrideCursor(Qt::WaitCursor);

  QString currentStyle = m_pref->getCurrentStyleSheet();
  qApp->setStyleSheet(currentStyle);
  QApplication::restoreOverrideCursor();

  bool isDarkTheme = Preferences::instance()->getIconTheme();
  bool themeCondition =
      currentStyle.contains("file:///") && (currentStyle.contains("Light") ||
                                            currentStyle.contains("Neutral")) ||
      currentStyle.contains("imgs/black");

  if (themeCondition) {
    if (!isDarkTheme) {
      m_pref->setValue(iconTheme, true);
      DVGui::MsgBoxInPopup(DVGui::MsgType(INFORMATION),
                           tr("Please restart to reload the icons."));
    }
  } else {
    if (isDarkTheme) {
      m_pref->setValue(iconTheme, false);
      DVGui::MsgBoxInPopup(DVGui::MsgType(INFORMATION),
                           tr("Please restart to reload the icons."));
    }
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPixelsOnlyChanged() {
  QComboBox* unitOm           = getUI<QComboBox*>(linearUnits);
  QComboBox* cameraUnitOm     = getUI<QComboBox*>(cameraUnits);
  DoubleLineEdit* defLevelDpi = getUI<DoubleLineEdit*>(DefLevelDpi);
  MeasuredDoubleLineEdit* defLevelWidth =
      getUI<MeasuredDoubleLineEdit*>(DefLevelWidth);
  MeasuredDoubleLineEdit* defLevelHeight =
      getUI<MeasuredDoubleLineEdit*>(DefLevelHeight);

  bool isPixel = m_pref->getBoolValue(pixelsOnly);
  if (isPixel) {
    m_pref->setValue(DefLevelDpi, Stage::standardDpi);
    TCamera* camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimension camRes = camera->getRes();
    TDimensionD camSize;
    camSize.lx = camRes.lx / Stage::standardDpi;
    camSize.ly = camRes.ly / Stage::standardDpi;
    camera->setSize(camSize);
    TDimension cleanupRes = CleanupSettingsModel::instance()
                                ->getCurrentParameters()
                                ->m_camera.getRes();
    TDimensionD cleanupSize;
    cleanupSize.lx = cleanupRes.lx / Stage::standardDpi;
    cleanupSize.ly = cleanupRes.ly / Stage::standardDpi;
    CleanupSettingsModel::instance()->getCurrentParameters()->m_camera.setSize(
        cleanupSize);

    m_pref->storeOldUnits();

    if (unitOm->currentText() != tr("pixel"))
      unitOm->setCurrentText(tr("pixel"));
    if (cameraUnitOm->currentText() != tr("pixel"))
      cameraUnitOm->setCurrentText(tr("pixel"));
    unitOm->setDisabled(true);
    cameraUnitOm->setDisabled(true);
    defLevelDpi->setDisabled(true);
    defLevelDpi->setValue(Stage::standardDpi);
    defLevelWidth->setMeasure("camera.lx");
    defLevelHeight->setMeasure("camera.ly");
    defLevelWidth->setValue(m_pref->getDoubleValue(DefLevelWidth));
    defLevelHeight->setValue(m_pref->getDoubleValue(DefLevelHeight));
    defLevelHeight->setDecimals(0);
    defLevelWidth->setDecimals(0);
  } else {
    QString tempUnit = m_pref->getStringValue(oldUnits);
    unitOm->setCurrentIndex(unitOm->findData(tempUnit));
    tempUnit = m_pref->getStringValue(oldCameraUnits);
    cameraUnitOm->setCurrentIndex(cameraUnitOm->findData(tempUnit));
    unitOm->setDisabled(false);
    cameraUnitOm->setDisabled(false);
    bool isRaster = m_pref->getIntValue(DefLevelType) != PLI_XSHLEVEL;
    if (isRaster && !m_pref->getBoolValue(newLevelSizeToCameraSizeEnabled)) {
      defLevelDpi->setDisabled(false);
    }
    defLevelHeight->setMeasure("level.ly");
    defLevelWidth->setMeasure("level.lx");
    defLevelWidth->setValue(m_pref->getDoubleValue(DefLevelWidth));
    defLevelHeight->setValue(m_pref->getDoubleValue(DefLevelHeight));
    defLevelHeight->setDecimals(4);
    defLevelWidth->setDecimals(4);
  }
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("pixelsOnly");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::beforeUnitChanged() { m_pref->storeOldUnits(); }

//-----------------------------------------------------------------------------

void PreferencesPopup::onUnitChanged() {
  CheckBox* pixelsOnlyCB = getUI<CheckBox*>(pixelsOnly);
  if (!pixelsOnlyCB->isChecked() &&
      (m_pref->getStringValue(linearUnits) == "pixel" ||
       m_pref->getStringValue(cameraUnits) == "pixel")) {
    pixelsOnlyCB->setCheckState(Qt::Checked);
  } else if (pixelsOnlyCB->isChecked() &&
             (m_pref->getStringValue(linearUnits) != "pixel" &&
              m_pref->getStringValue(cameraUnits) != "pixel")) {
    m_pref->setPixelsOnly();
    pixelsOnlyCB->setCheckState(Qt::Unchecked);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::beforeRoomChoiceChanged() {
  TApp::instance()->writeSettings();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onColorCalibrationChanged() {
  LutManager::instance()->update();
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "ColorCalibration");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRecordAsUserChanged() {
  QString username = m_pref->getStringValue(recordAsUsername);
  if (username.isEmpty()) {
    username = TSystem::getUserName();
    m_pref->setValue(recordAsUsername, QVariant::fromValue(username));
    getUI<LineEdit*>(recordAsUsername)->setText(username);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onDefLevelTypeChanged() {
  bool isRaster = m_pref->getIntValue(DefLevelType) != PLI_XSHLEVEL &&
                  !m_pref->getBoolValue(newLevelSizeToCameraSizeEnabled);
  m_controlIdMap.key(DefLevelWidth)->setEnabled(isRaster);
  m_controlIdMap.key(DefLevelHeight)->setEnabled(isRaster);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled() &&
      !m_pref->getBoolValue(pixelsOnly))
    m_controlIdMap.key(DefLevelDpi)->setEnabled(isRaster);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onUseNumpadForSwitchingStylesClicked() {
  bool checked = m_pref->getBoolValue(useNumpadForSwitchingStyles);
  if (checked) {
    // check if there are any commands with numpadkey shortcuts
    CommandManager* cm = CommandManager::instance();
    QList<QAction*> actionsList;
    for (int key = Qt::Key_0; key <= Qt::Key_9; key++) {
      std::string str = QKeySequence(key).toString().toStdString();
      QAction* action = cm->getActionFromShortcut(str);
      if (action) actionsList.append(action);
    }
    QAction* tabAction = cm->getActionFromShortcut("Tab");
    if (tabAction) actionsList.append(tabAction);
    tabAction = cm->getActionFromShortcut("Shift+Tab");
    if (tabAction) actionsList.append(tabAction);
    // if there are actions using numpad shortcuts, notify to release them.
    if (!actionsList.isEmpty()) {
      QString msgStr =
          tr("Numpad keys are assigned to the following commands.\nIs it OK to "
             "release these shortcuts?");
      for (int a = 0; a < actionsList.size(); a++) {
        msgStr += "\n" + actionsList.at(a)->iconText() + "  (" +
                  actionsList.at(a)->shortcut().toString() + ")";
      }
      int ret = DVGui::MsgBox(msgStr, tr("OK"), tr("Cancel"), 1);
      if (ret == 2 || ret == 0) {  // canceled
        getUI<CheckBox*>(useNumpadForSwitchingStyles)->setChecked(false);
        return;
      } else {  // accepted, then release shortcuts
        for (int a = 0; a < actionsList.size(); a++)
          cm->setShortcut(actionsList[a], "");
      }
    }
  }
  // emit signal to update Palette and Viewer
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "NumpadForSwitchingStyles");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelBasedToolsDisplayChanged() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "ToolbarDisplay");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowKeyframesOnCellAreaChanged() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("XsheetCamera");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowQuickToolbarClicked() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("QuickToolbar");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowXsheetBreadcrumbsClicked() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "XsheetBreadcrumbs");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onShowDragBarsChanged() {
  bool enabled = m_pref->getBoolValue(showDragBars);
  m_controlIdMap.key(timelineLayoutPreference)->setEnabled(!enabled);

  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "XsheetDragBars");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onUnifyColumnVisibilityTogglesChanged() {
  // Check if any column has visibility toggles with different states and the
  // "unify visibility toggles" option is enabled
  if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
    ColumnCmd::unifyColumnVisibilityToggles();

  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "unifyColumnVisibilityToggles");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onModifyExpressionOnMovingReferencesChanged() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged(
      "modifyExpressionOnMovingReferences");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onBlankCountChanged() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("BlankCount");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onBlankColorChanged() {
  TApp::instance()->getCurrentScene()->notifyPreferenceChanged("BlankColor");
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOnionSkinVisibilityChanged() {
  bool onionActive = m_pref->getBoolValue(onionSkinEnabled);
  m_controlIdMap.key(onionPaperThickness)->setEnabled(onionActive);
  m_controlIdMap.key(backOnionColor)->setEnabled(onionActive);
  m_controlIdMap.key(frontOnionColor)->setEnabled(onionActive);
  m_controlIdMap.key(onionInksOnly)->setEnabled(onionActive);

  OnionSkinMask osm =
      TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
  osm.enable(onionActive);
  TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(osm);
  TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
}

//---------------------------------------------------------------------------------------

void PreferencesPopup::onOnionColorChanged() {
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  TApp::instance()->getCurrentLevel()->notifyLevelViewChange();
  TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
}

//-----------------------------------------------------------------------------

void invalidateIcons();

void PreferencesPopup::onTranspCheckDataChanged() { invalidateIcons(); }

//-----------------------------------------------------------------------------

void PreferencesPopup::onUseThemeViewerColorsChanged() {
  bool enable = m_pref->getBoolValue(useThemeViewerColors);
  m_controlIdMap.key(viewerBGColor)->setEnabled(!enable);
  m_controlIdMap.key(previewBGColor)->setEnabled(!enable);
  notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onChessboardChanged() {
  CommonChessboard::instance()->update();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSVNEnabledChanged() {
  if (m_pref->getBoolValue(SVNEnabled)) {
    if (!VersionControl::instance()->testSetup())
      getUI<CheckBox*>(SVNEnabled)->setChecked(false);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::notifySceneChanged() {
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSaveExternallyChanged() {
  QGroupBox* autoSaveGroup = getUI<QGroupBox*>(autosaveEnabled);
  autoSaveGroup->setChecked(m_pref->getBoolValue(autosaveEnabled));
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAutoSavePeriodExternallyChanged() {
  IntLineEdit* minuteFld = getUI<IntLineEdit*>(autosavePeriod);
  minuteFld->setValue(m_pref->getIntValue(autosavePeriod));
}

//-----------------------------------------------------------------------------

// void PreferencesPopup::onProjectRootChanged() {
//  int index = 0;
//  // if (m_projectRootStuff->isChecked())
//  index |= 0x08;
//  if (m_projectRootDocuments->isChecked()) index |= 0x04;
//  if (m_projectRootDesktop->isChecked()) index |= 0x02;
//  if (m_projectRootCustom->isChecked()) index |= 0x01;
//  m_pref->setValue(projectRoot, index);
//}
//-----------------------------------------------------------------------------

void PreferencesPopup::onEditAdditionalStyleSheet() {
  if (!m_additionalStyleEdit) {
    m_additionalStyleEdit = new AdditionalStyleEdit(this);

    bool ret = connect(m_additionalStyleEdit, SIGNAL(additionalSheetEdited()),
                       this, SLOT(onAdditionalStyleSheetEdited()));
    assert(ret);
  }
  m_additionalStyleEdit->show();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAdditionalStyleSheetEdited() {
  onStyleSheetTypeChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onPixelUnitExternallySelected(bool on) {
  CheckBox* pixelsOnlyCB = getUI<CheckBox*>(pixelsOnly);
  // call slot function onPixelsOnlyChanged() accordingly
  pixelsOnlyCB->setCheckState((on) ? Qt::Checked : Qt::Unchecked);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onInterfaceFontChanged(const QString& text) {
  m_pref->setValue(interfaceFont, text);

  // rebuild font styles
  QComboBox* fontStyleCombo         = getUI<QComboBox*>(interfaceFontStyle);
  QString oldTypeface               = fontStyleCombo->currentText();
  QList<ComboBoxItem> newStyleItems = buildFontStyleList();
  fontStyleCombo->clear();
  for (ComboBoxItem& item : newStyleItems)
    fontStyleCombo->addItem(item.first, item.second);
  if (!oldTypeface.isEmpty()) {
    int newIndex = fontStyleCombo->findText(oldTypeface);
    if (newIndex < 0) newIndex = 0;
    fontStyleCombo->setCurrentIndex(newIndex);
  }

  if (text.contains("Comic Sans"))
    DVGui::warning(tr("Life is too short for Comic Sans"));
  if (text.contains("Wingdings"))
    DVGui::warning(tr("Good luck.  You're on your own from here."));
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLutPathChanged() {
  FileField* lutPathFileField = getUI<FileField*>(colorCalibrationLutPaths);
  m_pref->setColorCalibrationLutPath(LutManager::instance()->getMonitorName(),
                                     lutPathFileField->getPath());
  onColorCalibrationChanged();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onCheck30bitDisplay() {
  Display30bitChecker checker(this);
  checker.exec();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onFrameFormatButton() {
  ToonzScene* scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  std::string ext = Preferences::instance()->getDefRasterFormat().toStdString();
  openFormatSettingsPopup(this, ext, nullptr,
                          &scene->getProperties()->formatTemplateFIdForInput());
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onAddLevelFormat() {
  bool ok            = true;
  QString formatName = DVGui::getText(tr("New Level Format"),
                                      tr("Assign the new level format name:"),
                                      tr("New Format"), &ok);

  if (ok) {
    int formatIdx = Preferences::instance()->addLevelFormat(formatName);
    rebuildFormatsList();

    m_levelFormatNames->setCurrentIndex(formatIdx);
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onRemoveLevelFormat() {
  Preferences::instance()->removeLevelFormat(
      m_levelFormatNames->currentIndex());
  rebuildFormatsList();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onEditLevelFormat() {
  if (!m_formatProperties) {
    m_formatProperties = new FormatProperties(this);

    bool ret = connect(m_formatProperties, SIGNAL(dialogClosed()),
                       SLOT(onLevelFormatEdited()));
    assert(ret);
  }

  const Preferences::LevelFormat& lf =
      Preferences::instance()->levelFormat(m_levelFormatNames->currentIndex());

  m_formatProperties->setLevelFormat(lf);
  m_formatProperties->show();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onLevelFormatEdited() {
  assert(m_formatProperties);

  Preferences& prefs = *Preferences::instance();
  int formatIdx      = m_levelFormatNames->currentIndex();

  prefs.removeLevelFormat(formatIdx);
  formatIdx = prefs.addLevelFormat(m_formatProperties->levelFormat());

  rebuildFormatsList();

  m_levelFormatNames->setCurrentIndex(formatIdx);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onImportPolicyExternallyChanged(int policy) {
  QComboBox* importPolicyCombo = getUI<QComboBox*>(importPolicy);
  // update preferences data accordingly
  importPolicyCombo->setCurrentIndex(policy);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onOpenViewerEventLog() {
  if (!m_viewerEventLogPopup) {
    m_viewerEventLogPopup = new ViewerEventLogPopup();
    ViewerEventLogManager::instance()->setViewerEventLogPopup(
        m_viewerEventLogPopup);
  }

  m_viewerEventLogPopup->show();
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createUI(PreferencesItemId id,
                                    const QList<ComboBoxItem>& comboItems,
                                    bool isLineEdit, bool useMinMaxSlider) {
  PreferencesItem item = m_pref->getItem(id);
  // create widget depends on the parameter types
  QWidget* widget = nullptr;
  bool ret        = false;
  switch (item.type) {
  case QMetaType::Bool:  // create CheckBox
  {
    CheckBox* cb = new CheckBox(getUIString(id), this);
    cb->setChecked(item.value.toBool());
    ret    = connect(cb, SIGNAL(stateChanged(int)), this, SLOT(onChange()));
    widget = cb;
  } break;

  case QMetaType::Int:            // create either QComboBox or IntLineEdit
    if (!comboItems.isEmpty()) {  // create QComboBox
      QComboBox* combo = new QComboBox(this);
      for (const ComboBoxItem& item : comboItems)
        combo->addItem(item.first, item.second);
      combo->setCurrentIndex(combo->findData(item.value));
      ret = connect(combo, SIGNAL(currentIndexChanged(int)), this,
                    SLOT(onChange()));
      widget = combo;
    } else if (useMinMaxSlider) {
      DVGui::IntField* field = new DVGui::IntField(this);
      field->setRange(item.min.toInt(), item.max.toInt());
      field->setValue(item.value.toInt());
      ret = connect(field, SIGNAL(valueEditedByHand()), this, SLOT(onChange()));
      ret = ret && connect(field, SIGNAL(valueChanged(bool)), this,
                           SLOT(onSliderChanged(bool)));
      widget = field;
    } else {  // create IntLineEdit
      assert(item.max.toInt() != -1);
      DVGui::IntLineEdit* field = new DVGui::IntLineEdit(
          this, item.value.toInt(), item.min.toInt(), item.max.toInt());
      ret = connect(field, SIGNAL(editingFinished()), this, SLOT(onChange()));
      widget = field;
    }
    break;

  case QMetaType::Double:     // create either MeasuredDoubleLineEdit or
                              // DoubleLineEdit
    if (id == DefLevelDpi) {  // currently DoubleLineEdit is only used in the
                              // dpi field
      DoubleLineEdit* field = new DoubleLineEdit(this, item.value.toDouble());
      field->setRange(item.min.toDouble(), item.max.toDouble());
      ret    = connect(field, SIGNAL(valueChanged()), this, SLOT(onChange()));
      widget = field;
    } else {
      MeasuredDoubleLineEdit* field = new MeasuredDoubleLineEdit(this);
      field->setRange(item.min.toDouble(), item.max.toDouble());
      if (m_pref->getStringValue(linearUnits) == "pixel")
        field->setMeasure((id == DefLevelWidth) ? "camera.lx" : "camera.ly");
      else
        field->setMeasure((id == DefLevelWidth) ? "level.lx" : "level.ly");
      field->setValue(item.value.toDouble());
      ret    = connect(field, SIGNAL(valueChanged()), this, SLOT(onChange()));
      widget = field;
    }
    break;

  case QMetaType::QString:      // create QFontComboBox, QComboBox or FileField
    if (id == interfaceFont) {  // create QFontComboBox
      QFontComboBox* combo = new QFontComboBox(this);
      combo->setCurrentText(item.value.toString());
      ret    = connect(combo, SIGNAL(currentTextChanged(const QString&)), this,
                       SLOT(onInterfaceFontChanged(const QString&)));
      widget = combo;
    } else if (!comboItems.isEmpty()) {  // create QComboBox
      QComboBox* combo = new QComboBox(this);
      for (const ComboBoxItem& item : comboItems)
        combo->addItem(item.first, item.second);
      combo->setCurrentIndex(combo->findData(item.value));
      ret    = connect(combo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onChange()));
      widget = combo;
    } else if (isLineEdit) {  // create LineEdit
      DVGui::LineEdit* lineEdit =
          new DVGui::LineEdit(item.value.toString(), this);
      ret =
          connect(lineEdit, SIGNAL(editingFinished()), this, SLOT(onChange()));
      widget = lineEdit;
    } else {  // create FileField
      DVGui::FileField* field =
          new DVGui::FileField(this, item.value.toString());
      ret    = connect(field, SIGNAL(pathChanged()), this, SLOT(onChange()));
      widget = field;
    }
    break;

  case QMetaType::QSize:  // create SizeField
  {
    SizeField* field = new SizeField(item.min.toSize(), item.max.toSize(),
                                     item.value.toSize(), this);
    ret    = connect(field, SIGNAL(editingFinished()), this, SLOT(onChange()));
    widget = field;
  } break;

  case QMetaType::QColor:  // create ColorField
  {
    ColorField* field =
        new ColorField(this, false, colorToTPixel(item.value.value<QColor>()));
    ret    = connect(field, SIGNAL(colorChanged(const TPixel32&, bool)), this,
                     SLOT(onColorFieldChanged(const TPixel32&, bool)));
    widget = field;
  } break;

  case QMetaType::QVariantMap:  // used in colorCalibrationLutPaths
  {
    assert(id == colorCalibrationLutPaths);
    DVGui::FileField* field = new DVGui::FileField(
        this, QString("- Please specify 3DLUT file (.3dl) -"), false, true);
    QString lutPath = m_pref->getColorCalibrationLutPath(
        LutManager::instance()->getMonitorName());
    if (!lutPath.isEmpty()) field->setPath(lutPath);
    field->setFileMode(QFileDialog::ExistingFile);
    QStringList lutFileTypes = {"3dl"};
    field->setFilters(lutFileTypes);
    ret = connect(field, SIGNAL(pathChanged()), this, SLOT(onLutPathChanged()));
    widget = field;
  } break;

  default:
    std::cout << "unsupported value type" << std::endl;
    break;
  }
  assert(ret);
  m_controlIdMap[widget] = id;
  return widget;
}

//-----------------------------------------------------------------------------

QGridLayout* PreferencesPopup::insertGroupBoxUI(PreferencesItemId id,
                                                QGridLayout* layout) {
  PreferencesItem item = m_pref->getItem(id);
  assert(item.type == QMetaType::Bool);
  QGroupBox* box = new QGroupBox(getUIString(id), this);
  box->setCheckable(true);
  box->setChecked(item.value.toBool());

  QGridLayout* lay = new QGridLayout();
  setupLayout(lay, 5);
  box->setLayout(lay);

  layout->addWidget(box, layout->rowCount(), 0, 1, 3);

  bool ret = connect(box, SIGNAL(clicked(bool)), this, SLOT(onChange()));
  // bool ret = connect(box, SIGNAL(clicked(bool)), this,
  // SLOT(onGroupBoxChanged(bool)));
  assert(ret);
  m_controlIdMap[box] = id;
  return lay;
}

//-----------------------------------------------------------------------------

void PreferencesPopup::insertUI(PreferencesItemId id, QGridLayout* layout,
                                const QList<ComboBoxItem>& comboItems,
                                bool isLineEdit, bool useMinMaxSlider) {
  PreferencesItem item = m_pref->getItem(id);

  QWidget* widget = createUI(id, comboItems, isLineEdit, useMinMaxSlider);
  if (!widget) return;

  bool isEditBox = false;
  if (item.type == QMetaType::QVariantMap ||
      (item.type == QMetaType::QString &&
       (dynamic_cast<FileField*>(widget) || dynamic_cast<LineEdit*>(widget))))
    isEditBox = true;

  // CheckBox contains label in itself
  if (item.type == QMetaType::Bool)
    layout->addWidget(widget, layout->rowCount(), 0, 1, 3, Qt::AlignLeft);
  else {  // insert labels for other types
    int row = layout->rowCount();
    layout->addWidget(new QLabel(getUIString(id), this), row, 0,
                      Qt::AlignRight | Qt::AlignVCenter);
    if (isEditBox)
      layout->addWidget(widget, row, 1, 1, (isLineEdit ? 1 : 2));
    else {
      bool isWideComboBox = false;
      for (auto cbItem : comboItems) {
        if (widget->fontMetrics().horizontalAdvance(cbItem.first) > 100) {
          isWideComboBox = true;
          break;
        }
      }
      if (id == interfaceFont) isWideComboBox = true;

      layout->addWidget(widget, row, 1, 1, (isWideComboBox) ? 2 : 1,
                        Qt::AlignLeft | Qt::AlignVCenter);
    }
  }
}

//-----------------------------------------------------------------------------

void PreferencesPopup::insertDualUIs(PreferencesItemId leftId,
                                     PreferencesItemId rightId,
                                     QGridLayout* layout,
                                     const QList<ComboBoxItem>& leftComboItems,
                                     const QList<ComboBoxItem>& rightComboItems,
                                     bool leftMinMaxSlider,
                                     bool rightMinMaxSlider) {
  int row = layout->rowCount();
  int col = 0;
  if (m_pref->getItem(leftId).type != QMetaType::Bool) {
    col = 1;
    layout->addWidget(new QLabel(getUIString(leftId), this), row, 0,
                      Qt::AlignRight | Qt::AlignVCenter);
  }
  QHBoxLayout* innerLay = new QHBoxLayout();
  innerLay->setContentsMargins(0, 0, 0, 0);
  innerLay->setSpacing(5);
  {
    innerLay->addWidget(
        createUI(leftId, leftComboItems, false, leftMinMaxSlider), 0);
    if ((m_pref->getItem(leftId).type == QMetaType::Bool &&
         m_pref->getItem(rightId).type != QMetaType::Bool) ||
        rightMinMaxSlider)
      innerLay->addSpacing(40);
    if (m_pref->getItem(rightId).type != QMetaType::Bool)
      innerLay->addWidget(new QLabel(getUIString(rightId), this), 0,
                          Qt::AlignRight | Qt::AlignVCenter);
    innerLay->addWidget(
        createUI(rightId, rightComboItems, false, rightMinMaxSlider), 0);
    innerLay->addStretch(1);
  }
  layout->addLayout(innerLay, row, col, 1, 2);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::insertFootNote(QGridLayout* layout) {
  QLabel* note = new QLabel(
      tr("* Changes will take effect the next time you run Tahoma2D"));
  note->setStyleSheet("font-size: 10px; font: italic;");
  layout->addWidget(note, layout->rowCount(), 0, 1, 3,
                    Qt::AlignLeft | Qt::AlignVCenter);
}

//-----------------------------------------------------------------------------

QString PreferencesPopup::getUIString(PreferencesItemId id) {
  const static QMap<PreferencesItemId, QString> uiStringTable = {
      // General
      {defaultViewerEnabled, tr("Use Default Viewer for Movie Formats")},
      {rasterOptimizedMemory, tr("Minimize Raster Memory Fragmentation*")},
      {autosaveEnabled, tr("Save Automatically")},
      {autosavePeriod, tr("Interval (Minutes):")},
      {autosaveSceneEnabled, tr("Automatically Save the Scene File")},
      {autosaveOtherFilesEnabled, tr("Automatically Save Non-Scene Files")},
      {startupPopupEnabled, tr("Show Startup Window when Tahoma2D Starts")},
      {tipsPopupEnabled, tr("Show Tips Window when Tahoma2D Starts")},
      {undoMemorySize, tr("Undo Memory Size (MB):")},
      {taskchunksize, tr("Render Task Chunk Size:")},
      {replaceAfterSaveLevelAs,
       tr("Replace Vector and Smart Level after SaveLevelAs command")},
      {backupEnabled, tr("Backup Scene and Animation Levels when Saving")},
      {backupKeepCount, tr("# of backups to keep:")},
      {sceneNumberingEnabled, tr("Show Info in Rendered Frames")},
      {watchFileSystemEnabled,
       tr("Watch File System and Update File Browser Automatically")},
      //{ projectRoot,               tr("") },
      {customProjectRoot, tr("Custom Project Path(s):")},
      {pathAliasPriority, tr("Path Alias Priority:")},
      {showAdvancedOptions, tr("Show Advanced Preferences and Options*")},

      // Interface
      {CurrentStyleSheetName, tr("Theme:")},
      {iconTheme, tr("Switch to dark icons")},
      {pixelsOnly, tr("All imported images will use the same DPI")},
      //{ oldUnits,                               tr("") },
      //{ oldCameraUnits,                         tr("") },
      {linearUnits, tr("Unit:")},
      {cameraUnits, tr("Camera Unit:")},
      {CurrentRoomChoice, tr("Rooms*:")},
      {functionEditorToggle, tr("Function Editor*:")},
      {moveCurrentFrameByClickCellArea,
       tr("Move Current Frame by Clicking on Layer Header / Numerical Columns "
          "Cell "
          "Area")},
      {actualPixelViewOnSceneEditingMode,
       tr("Enable Actual Pixel View on Scene Editing Mode")},
      {showRasterImagesDarkenBlendedInViewer,
       tr("Show Raster Images Darken Blended")},
      {iconSize, tr("Level Strip Thumbnail Size*:")},
      {viewShrink, tr("Viewer Shrink:")},
      {viewStep, tr("Step:")},
      {viewerZoomCenter, tr("Viewer Zoom Center:")},
      {CurrentLanguageName, tr("Language*:")},
      {interfaceFont, tr("Font*:")},
      {interfaceFontStyle, tr("Style*:")},
      {colorCalibrationEnabled, tr("Color Calibration using 3D Look-up Table")},
      {colorCalibrationLutPaths,
       tr("3DLUT File for [%1]:")
           .arg(LutManager::instance()->getMonitorName())},
      {displayIn30bit, tr("30bit Display*")},
      {showIconsInMenu, tr("Show Icons In Menu*")},
      {viewerIndicatorEnabled, tr("Show Viewer Indicators")},
      {highDpiScalingEnabled, tr("Enable High DPI Scaling*")},

      // Visualization
      {show0ThickLines, tr("Show Lines with Thickness 0")},
      {regionAntialias, tr("Antialiased Region Boundaries")},
      {rasterizeAntialias, tr("Rasterize Vector with Anti Aliasing")},

      // Loading
      {importPolicy, tr("Default File Import Behavior:")},
      {autoExposeEnabled, tr("Expose Loaded Levels in the Scene")},
      {autoRemoveUnusedLevels,
       tr("Automatically Remove Unused Levels From Scene Cast")},
      {subsceneFolderEnabled, tr("Create Sub-folder when Importing Sub-Scene")},
      {removeSceneNumberFromLoadedLevelName,
       tr("Automatically Remove Scene Number from Loaded Level Name")},
      {IgnoreImageDpi, tr("Use Camera DPI for All Imported Images")},
      {rasterLevelCachingBehavior, tr("Raster Level Caching Behavior:")},
      {columnIconLoadingPolicy, tr("Column Icon:")},
      //{ levelFormats,                           tr("") },

      // Saving
      {rasterBackgroundColor, tr("Matte color:")},
      {resetUndoOnSavingLevel, tr("Clear Undo History when Saving Levels")},
      {doNotShowPopupSaveScene, tr("Do not show Save Scene popup warning")},
      {defaultProjectPath, tr("Default Project Path:")},
      {recordFileHistory, tr("Record File History* (tnz, pli, hst)")},
      {recordAsUsername, tr("History Username*:")},

      // Import / Export
      {ffmpegPath, tr("Executable Directory:")},
      {ffmpegTimeout, tr("Import/Export Timeout (seconds):")},
      {fastRenderPath, tr("Fast Render Output Directory:")},
      {ffmpegMultiThread,
       tr("Allow Multi-Thread in FFMPEG Rendering (UNSTABLE)")},
      {rhubarbPath, tr("Executable Directory:")},
      {rhubarbTimeout, tr("Analyze Audio Timeout (seconds):")},

      // Drawing
      {DefRasterFormat, tr("Default Raster Level Format:")},
      //{scanLevelType, tr("Scan File Format:")},
      {DefLevelType, tr("Default Level Type:")},
      {newLevelSizeToCameraSizeEnabled,
       tr("New Levels Default to the Current Camera Size")},
      {DefLevelWidth, tr("Width:")},
      {DefLevelHeight, tr("Height:")},
      {DefLevelDpi, tr("DPI:")},
      {EnableAutocreation, tr("Enable Autocreation")},
      {NumberingSystem, tr("Numbering System:")},
      {EnableAutoStretch, tr("Enable Auto-stretch Frame")},
      {EnableCreationInHoldCells, tr("Enable Creation in Hold Cells")},
      {EnableAutoRenumber, tr("Enable Autorenumber")},
      {EnableImplicitHold, tr("Enable Implicit Hold")},
      {vectorSnappingTarget, tr("Vector Snapping:")},
      {saveUnpaintedInCleanup,
       tr("Keep Original Cleaned Up Drawings As Backup")},
      {minimizeSaveboxAfterEditing, tr("Minimize Savebox after Editing")},
      {useNumpadForSwitchingStyles,
       tr("Use Numpad and Tab keys for Switching Styles")},
      {downArrowInLevelStripCreatesNewFrame,
       tr("Down Arrow at End of Level Strip Creates a New Frame")},
      {keepFillOnVectorSimplify,
       tr("Keep fill when using \"Replace Vectors\" command")},
      {useHigherDpiOnVectorSimplify,
       tr("Use higher DPI for calculations - Slower but more accurate")},

      // Tools
      // {dropdownShortcutsCycleOptions, tr("Dropdown Shortcuts:")}, // removed
      {FillOnlysavebox,
       tr("Use the TLV Savebox to Limit Filling Operations")},  // Moved to
                                                                // tools that
                                                                // need it
      {multiLayerStylePickerEnabled,
       tr("Multi Layer Style Picker: Switch Levels by Picking")},
      {cursorBrushType, tr("Basic Cursor Type:")},
      {cursorBrushStyle, tr("Cursor Style:")},
      {cursorOutlineEnabled, tr("Show Cursor Size Outlines")},
      {levelBasedToolsDisplay, tr("Toolbar Display Behaviour:")},
      {useCtrlAltToResizeBrush,
       tr("Use %1 to Resize Brush").arg(trModKey("Ctrl+Alt"))},
      {temptoolswitchtimer,
       tr("Temporary Tool Switch Shortcut Hold Time (ms):")},
      {magnetNonLinearSliderEnabled,
       tr("Magnet Tool Size Slider - Non-Linear mode*")},
      {toolScale, tr("Increase Selection and Control Point Editor Tool Widget Size (For 4k Displays)")},

      // Xsheet
      {xsheetLayoutPreference, tr("Xsheet Header Layout*:")},
      {xsheetStep, tr("Next/Previous Step Frames:")},
      {xsheetAutopanEnabled, tr("Autopan during Playback")},
      {showDragBars, tr("Show Column and Cell Drag Bars")},
      {timelineLayoutPreference, tr("Timeline Layer Layout:")},
      {DragCellsBehaviour, tr("Cell-dragging Behaviour:")},
      {pasteCellsBehavior, tr("Paste Cells Behaviour:")},
      {ignoreAlphaonColumn1Enabled,
       tr("Ignore Alpha Channel on Levels in Column 1")},
      {showKeyframesOnXsheetCellArea, tr("Show Keyframes on Cell Area")},
      {showXsheetCameraColumn, tr("Show Camera Column")},
      {useArrowKeyToShiftCellSelection,
       tr("Use Arrow Key to Shift Cell Selection")},
      {inputCellsWithoutDoubleClickingEnabled,
       tr("Enable to Input Cells without Double Clicking")},
      {shortcutCommandsWhileRenamingCellEnabled,
       tr("Enable Tahoma2D Commands' Shortcut Keys While Renaming Cell")},
      {showQuickToolbar, tr("Show Quick Toolbar")},
      {showXsheetBreadcrumbs, tr("Show Sub-Scene Navigation Bar")},
      {expandFunctionHeader, tr("Expand Function Editor Header to Match Xsheet Header Height*")},
      {showColumnNumbers, tr("Show Column Numbers")},
      {unifyColumnVisibilityToggles,
       tr("Unify Preview and Camstand Visibility Toggles")},
      {parentColorsInXsheetColumn,
       tr("Show Column Parent's Color in the Xsheet")},
      {highlightLineEverySecond, tr("Highlight Line Every Second")},
      {syncLevelRenumberWithXsheet,
       tr("Sync Level Strip Drawing Number Changes with the Scene")},
      {currentTimelineEnabled,
       tr("Show Current Time Indicator (Timeline Mode only)")},
      {currentColumnColor, tr("Current Column Color:")},
      {currentCellColor, tr("Current Cell Color:")},
      //{ levelNameOnEachMarkerEnabled, tr("Display Level Name on Each Marker")
      //},
      {levelNameDisplayType, tr("Level Name Display:")},
      {showFrameNumberWithLetters,
       tr("Show \"ABC\" Appendix to the Frame Number in Scene Cell")},

      // Animation
      {keyframeType, tr("Default Interpolation:")},
      {animationStep, tr("Animation Step:")},
      {modifyExpressionOnMovingReferences,
       tr("[Experimental Feature] ") +
           tr("Automatically Modify Expression On Moving Referenced Objects")},

      // Preview
      {blanksCount, tr("Blank Frames:")},
      {blankColor, tr("Blank Frames Color:")},
      {rewindAfterPlayback, tr("Rewind after Playback")},
      {shortPlayFrameCount, tr("Number of Frames to Play \nfor Short Play:")},
      {previewAlwaysOpenNewFlip, tr("Display in a New Flipbook Window")},
      {fitToFlipbook, tr("Fit to Flipbook")},
      {generatedMovieViewEnabled, tr("Open Flipbook after Rendering")},
      {inbetweenFlipDrawingCount, tr("Drawings:")},
      {inbetweenFlipSpeed, tr("Flip Speed (msec):")},

      // Onion Skin
      {onionSkinEnabled, tr("Onion Skin ON")},
      {onionPaperThickness, tr("Paper Thickness:")},
      {backOnionColor, tr("Previous Frames Correction:")},
      {frontOnionColor, tr("Following Frames Correction:")},
      {onionInksOnly, tr("Display Lines Only")},
      {onionSkinDuringPlayback, tr("Show Onion Skin During Playback")},
      {useOnionColorsForShiftAndTraceGhosts,
       tr("Use Onion Skin Colors for Reference Drawings of Shift and Trace")},
      {animatedGuidedDrawing, tr("Vector Guided Style:")},

      // Colors
      {viewerBGColor, tr("Viewer BG Color:")},
      {previewBGColor, tr("Preview BG Color:")},
      {useThemeViewerColors,
       tr("Use the Curent Theme's Viewer Background Colors")},
      {levelEditorBoxColor, tr("Level Editor Canvas Color:")},
      {chessboardColor1, tr("Chessboard Color 1:")},
      {chessboardColor2, tr("Chessboard Color 2:")},
      {transpCheckInkOnWhite, tr("Ink Color on White BG:")},
      {transpCheckInkOnBlack, tr("Ink Color on Black BG:")},
      {transpCheckPaint, tr("Paint Color:")},

      // Version Control
      {SVNEnabled, tr("Enable Version Control*")},
      {automaticSVNFolderRefreshEnabled,
       tr("Automatically Refresh Folder Contents")},
      {latestVersionCheckEnabled,
       tr("Check for the Latest Version of Tahoma2D on Launch")},

      // Touch / Tablet Settings
      // TounchGestureControl // Touch Gesture is a checkable command and not in
      // preferences.ini
      {gestureUndoMethod, tr("Undo Gesture:")},
      {gestureRedoMethod, tr("Redo Gesture:")},
      {winInkEnabled, tr("Enable Windows Ink Support* (EXPERIMENTAL)")},
      {useQtNativeWinInk,
       tr("Use Qt's Native Windows Ink Support*\n(CAUTION: This options is for "
          "maintenance purpose. \n Do not activate this option or the tablet "
          "won't work properly.)")}};

  return uiStringTable.value(id, QString());
}

//-----------------------------------------------------------------------------
// returns list for combo items
// ComboBoxItem consists of an item label string and data to be stored in
// preferences

QList<ComboBoxItem> PreferencesPopup::getComboItemList(
    PreferencesItemId id) const {
  const static QMap<PreferencesItemId, QList<ComboBoxItem>> comboItemsTable = {
      {pathAliasPriority,
       {{tr("Project Folder Aliases (+drawings, +scenes, etc.)"),
         Preferences::ProjectFolderAliases},
        {tr("Scene Folder Alias ($scenefolder)"),
         Preferences::SceneFolderAlias},
        {tr("Use Project Folder Aliases Only"),
         Preferences::ProjectFolderOnly}}},
      {linearUnits,  // cameraUnits shares items with linearUnits
       {{tr("cm"), "cm"},
        {tr("mm"), "mm"},
        {tr("inch"), "inch"},
        {tr("field"), "field"},
        {tr("pixel"), "pixel"}}},
      {functionEditorToggle,
       {{tr("Graph Editor Opens in Popup"),
         Preferences::ShowGraphEditorInPopup},
        {tr("Spreadsheet Opens in Popup"),
         Preferences::ShowFunctionSpreadsheetInPopup},
        {tr("Toggle Between Graph Editor and Spreadsheet"),
         Preferences::ToggleBetweenGraphAndSpreadsheet}}},
      {viewerZoomCenter, {{tr("Mouse Cursor"), 0}, {tr("Viewer Center"), 1}}},
      {importPolicy,
       {{tr("Always ask before loading or importing"), 0},
        {tr("Always import the file to the current project"), 1},
        {tr("Always load the file from the current location"), 2}}},
      {rasterLevelCachingBehavior,
       {{tr("On Demand"), 0},
        {tr("All Icons"), 1},
        {tr("All Icons & Images"), 2}}},
      {columnIconLoadingPolicy,
       {{tr("At Once"), Preferences::LoadAtOnce},
        {tr("On Demand"), Preferences::LoadOnDemand}}},
      {DefRasterFormat, {{"tif", "tif"}, {"png", "png"}}},
      //{scanLevelType, {{"tif", "tif"}, {"png", "png"}}},
      {DefLevelType,
       {{tr("Vector Level"), PLI_XSHLEVEL},
        {tr("Smart Raster Level"), TZP_XSHLEVEL},
        {tr("Raster Level"), OVL_XSHLEVEL}}},
      {NumberingSystem, {{tr("Incremental"), 0}, {tr("Animation Sheet"), 1}}},
      {vectorSnappingTarget,
       {{tr("Strokes"), 0}, {tr("Guides"), 1}, {tr("All"), 2}}},
      //{dropdownShortcutsCycleOptions,
      // {{tr("Open the dropdown to display all options"), 0},
      //  {tr("Cycle through the available options"), 1}}},
      {cursorBrushType,
       {{tr("Small"), "Small"},
        {tr("Large"), "Large"},
        {tr("Crosshair"), "Crosshair"},
        {tr("Triangle Top Left"), "Triangle Top Left"},
        {tr("Triangle Top Right"), "Triangle Top Right"},
        {tr("Triangle Bottom Left"), "Triangle Bottom Left"},
        {tr("Triangle Bottom Right"), "Triangle Bottom Right"},
        {tr("Triangle Up"), "Triangle Up"},
        {tr("Triangle Down"), "Triangle Down"},
        {tr("Triangle Left"), "Triangle Left"},
        {tr("Triangle Right"), "Triangle Right"}}},
      {cursorBrushStyle,
       {{tr("Default"), "Default"},
        {tr("Left-Handed"), "Left-Handed"},
        {tr("Simple"), "Simple"}}},
      {levelBasedToolsDisplay,
       {{tr("Default"), 0},
        {tr("Enable Tools For Level Only"), 1},
        {tr("Show Tools For Level Only"), 2}}},
      {xsheetLayoutPreference,
       {{tr("Compact"), "Compact"},
        {tr("Roomy"), "Roomy"},
        {tr("Minimum"), "Minimum"}}},
      {timelineLayoutPreference,
       {{tr("Compact"), "NoDragCompact"},
        {tr("Roomy"), "Roomy"},
        {tr("Minimum"), "NoDragMinimum"}}},
      {levelNameDisplayType,
       {{tr("Default"), Preferences::ShowLevelName_Default},
        {tr("Display on Each Marker"), Preferences::ShowLevelNameOnEachMarker},
        {tr("Display on Column Header"),
         Preferences::ShowLevelNameOnColumnHeader}}},
      {DragCellsBehaviour,
       {{tr("Cells Only"), 0},
        {tr("Cells and Column Data"), 1},
// Disabled until OT officially releases it
//        {tr("Disable Dragging Cells"), 2}
       }},
      {pasteCellsBehavior,
       {{tr("Insert Paste Whole Data"), 0},
        {tr("Overwrite Paste Cell Numbers"), 1}}},
      {keyframeType,  // note that the value starts from 1, not 0
       {{tr("Constant"), 1},
        {tr("Linear"), 2},
        {tr("Speed In / Speed Out"), 3},
        {tr("Ease In / Ease Out"), 4},
        {tr("Ease In / Ease Out %"), 5},
        {tr("Exponential"), 6},
        {tr("Expression "), 7},
        {tr("File"), 8}}},
      {animatedGuidedDrawing,
       {{tr("Arrow Markers"), 0}, {tr("Animated Guide"), 1}}},
      {gestureUndoMethod,
       {{tr("2-Finger Tap"), Preferences::TwoFingerTap},
        {tr("3-Finger Drag Left"), Preferences::ThreeFingerDragLeft}}},
      {gestureRedoMethod,
       {{tr("3-Finger Tap"), Preferences::ThreeFingerTap},
        {tr("3-Finger Drag Right"), Preferences::ThreeFingerDragRight}}}};
  assert(comboItemsTable.contains(id));
  return comboItemsTable.value(id, QList<ComboBoxItem>());
}

template <typename T>
inline T PreferencesPopup::getUI(PreferencesItemId id) {
  assert(m_controlIdMap.keys(id).count() == 1);
  T ret = dynamic_cast<T>(m_controlIdMap.key(id));
  assert(ret);
  return ret;
}

//**********************************************************************************
//    PreferencesPopup's  constructor
//**********************************************************************************

PreferencesPopup::PreferencesPopup()
    : Dialog(TApp::instance()->getMainWindow())
    , m_formatProperties()
    , m_additionalStyleEdit(nullptr)
    , m_viewerEventLogPopup(0) {
  setWindowTitle(tr("Preferences"));
  setObjectName("PreferencesPopup");

  m_pref = Preferences::instance();

  // Category List
  m_categoryList = new QListWidget(this);
  QStringList categories;
  categories << tr("General") << tr("Interface") << tr("Visualization")
             << tr("Loading") << tr("Saving") << tr("Drawing") << tr("Tools")
             << tr("Scene") << tr("Animation") << tr("Preview")
             << tr("Onion Skin") << tr("Colors") << tr("3rd Party Apps")
             << tr("Version Control") << tr("Touch/Tablet Settings");
  m_categoryList->addItems(categories);
  m_categoryList->setFixedWidth(160);
  m_categoryList->setCurrentRow(0);
  m_categoryList->setAlternatingRowColors(true);

  m_stackedWidget = new QStackedWidget(this);
  m_stackedWidget->addWidget(createGeneralPage());
  m_stackedWidget->addWidget(createInterfacePage());
  m_stackedWidget->addWidget(createVisualizationPage());
  m_stackedWidget->addWidget(createLoadingPage());
  m_stackedWidget->addWidget(createSavingPage());
  m_stackedWidget->addWidget(createDrawingPage());
  m_stackedWidget->addWidget(createToolsPage());
  m_stackedWidget->addWidget(createXsheetPage());
  m_stackedWidget->addWidget(createAnimationPage());
  m_stackedWidget->addWidget(createPreviewPage());
  m_stackedWidget->addWidget(createOnionSkinPage());
  m_stackedWidget->addWidget(createColorsPage());
  m_stackedWidget->addWidget(createImportExportPage());
  m_stackedWidget->addWidget(createVersionControlPage());
  m_stackedWidget->addWidget(createTouchTabletPage());
  // createImportPrefsPage() must always be last
  m_stackedWidget->addWidget(createImportPrefsPage());

  QPushButton* importPrefButton = new QPushButton("Import Preferences");

  QHBoxLayout* mainLayout = new QHBoxLayout();
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  {
    // Category
    QVBoxLayout* categoryLayout = new QVBoxLayout();
    categoryLayout->setContentsMargins(5, 5, 5, 5);
    categoryLayout->setSpacing(10);
    categoryLayout->addWidget(m_categoryList, 1);
    categoryLayout->addWidget(importPrefButton, 0);
    mainLayout->addLayout(categoryLayout, 0);
    mainLayout->addWidget(m_stackedWidget, 1);
  }
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->addLayout(mainLayout);

  bool ret = connect(m_categoryList, SIGNAL(currentRowChanged(int)),
                     m_stackedWidget, SLOT(setCurrentIndex(int)));

  ret = ret && connect(importPrefButton, SIGNAL(clicked()),
                       SLOT(onImportPreferences()));

  assert(ret);
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createGeneralPage() {
  // m_projectRootDocuments = new CheckBox(tr("My Documents/Tahoma*"), this);
  // m_projectRootDesktop   = new CheckBox(tr("Desktop/Tahoma*"), this);
  // m_projectRootCustom    = new CheckBox(tr("Custom*"), this);
  // QWidget* customField   = new QWidget(this);
  // QGridLayout* customLay = new QGridLayout();
  // setupLayout(customLay, 5);
  //{
  //  insertUI(customProjectRoot, customLay);
  //  customLay->addWidget(
  //      new QLabel(
  //          tr("Advanced: Multiple paths can be separated by ** (No Spaces)"),
  //          this),
  //      customLay->rowCount(), 0, 1, 2, Qt::AlignLeft | Qt::AlignVCenter);
  //}
  // customField->setLayout(customLay);

  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);
  insertUI(defaultProjectPath, lay);
  insertUI(defaultViewerEnabled, lay);
  insertUI(rasterOptimizedMemory, lay);
  insertUI(startupPopupEnabled, lay);
  insertUI(tipsPopupEnabled, lay);
  insertUI(undoMemorySize, lay);
  insertUI(taskchunksize, lay);
  insertUI(sceneNumberingEnabled, lay);
  insertUI(watchFileSystemEnabled, lay);

  // QGridLayout* projectRootLay =
  //    insertGroupBox(tr("Additional Project Locations"), lay);
  //{
  //  projectRootLay->addWidget(m_projectRootDocuments, 0, 0, 1, 2);
  //  projectRootLay->addWidget(m_projectRootDesktop, 1, 0, 1, 2);
  //  projectRootLay->addWidget(m_projectRootCustom, 2, 0, 1, 2);
  //  projectRootLay->addWidget(customField, 3, 0, 1, 2);
  //}

  insertUI(pathAliasPriority, lay, getComboItemList(pathAliasPriority));

  insertUI(showAdvancedOptions, lay);

  lay->setRowStretch(lay->rowCount(), 1);
  insertFootNote(lay);
  widget->setLayout(lay);

  // int projectPaths = m_pref->getIntValue(projectRoot);
  // m_projectRootDocuments->setChecked(projectPaths & 0x04);
  // m_projectRootDesktop->setChecked(projectPaths & 0x02);
  // m_projectRootCustom->setChecked(projectPaths & 0x01);
  // if (!(projectPaths & 0x01)) customField->hide();

  QComboBox* pathAliasPriorityCB = getUI<QComboBox*>(pathAliasPriority);
  pathAliasPriorityCB->setToolTip(
      tr("This option defines which alias to be used\nif both are possible on "
         "coding file path."));
  pathAliasPriorityCB->setItemData(0, QString(" "), Qt::ToolTipRole);
  QString scenefolderTooltip =
      tr("Choosing this option will set initial location of all file browsers "
         "to $scenefolder.\n"
         "Also the initial output destination for new scenes will be set to "
         "$scenefolder as well.");
  pathAliasPriorityCB->setItemData(1, scenefolderTooltip, Qt::ToolTipRole);
  pathAliasPriorityCB->setItemData(2, QString(" "), Qt::ToolTipRole);

  m_onEditedFuncMap.insert(defaultProjectPath,
                           &PreferencesPopup::onDefaultProjectPathChanged);
  m_onEditedFuncMap.insert(autosaveEnabled,
                           &PreferencesPopup::onAutoSaveChanged);
  m_onEditedFuncMap.insert(autosaveSceneEnabled,
                           &PreferencesPopup::onAutoSaveOptionsChanged);
  m_onEditedFuncMap.insert(autosaveOtherFilesEnabled,
                           &PreferencesPopup::onAutoSaveOptionsChanged);
  m_onEditedFuncMap.insert(watchFileSystemEnabled,
                           &PreferencesPopup::onWatchFileSystemClicked);
  m_onEditedFuncMap.insert(showAdvancedOptions,
                           &PreferencesPopup::onShowAdvancedOptionsChanged);

  bool ret = true;
  ret      = ret && connect(m_pref, SIGNAL(stopAutoSave()), this,
                            SLOT(onAutoSaveExternallyChanged()));
  ret      = ret && connect(m_pref, SIGNAL(startAutoSave()), this,
                            SLOT(onAutoSaveExternallyChanged()));
  ret      = ret && connect(m_pref, SIGNAL(autoSavePeriodChanged()), this,
                            SLOT(onAutoSavePeriodExternallyChanged()));

  // ret = ret && connect(m_projectRootDocuments, SIGNAL(stateChanged(int)),
  //                     SLOT(onProjectRootChanged()));
  // ret = ret && connect(m_projectRootDesktop, SIGNAL(stateChanged(int)),
  //                     SLOT(onProjectRootChanged()));
  // ret = ret && connect(m_projectRootCustom, SIGNAL(stateChanged(int)),
  //                     SLOT(onProjectRootChanged()));
  // ret = ret && connect(m_projectRootCustom, SIGNAL(clicked(bool)),
  // customField,
  //                     SLOT(setVisible(bool)));
  assert(ret);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createInterfacePage() {
  QList<ComboBoxItem> styleSheetItemList;
  for (const QString& str : m_pref->getStyleSheetList()) {
    TFilePath path(str.toStdWString());
    QString name = QString::fromStdWString(path.getWideName());
    styleSheetItemList.push_back(ComboBoxItem(name, name));
  }

  QList<ComboBoxItem> roomItemList;
  foreach (QString roomName, m_pref->getRoomMap())
    roomItemList.push_back(ComboBoxItem(roomName, roomName));

  QList<ComboBoxItem> languageItemList;
  for (const QString& name : m_pref->getLanguageList())
    languageItemList.push_back(ComboBoxItem(name, name));

  QPushButton* check30bitBtn = new QPushButton(tr("Check Availability"));

  QPushButton* additionalStyleSheetBtn =
      new QPushButton(tr("Edit Additional Style Sheet.."));

  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(CurrentStyleSheetName, lay, styleSheetItemList);
  int row = lay->rowCount();
  lay->addWidget(additionalStyleSheetBtn, row - 1, 3);

  // lay->addWidget(new QLabel(tr("Icon Theme*:"), this), 2, 0,
  //               Qt::AlignRight | Qt::AlignVCenter);
  // lay->addWidget(createUI(iconTheme), 2, 1);

  if (Preferences::instance()->isShowAdvancedOptionsEnabled()) {
    insertUI(linearUnits, lay, getComboItemList(linearUnits));
    insertUI(cameraUnits, lay,
             getComboItemList(linearUnits));  // share items with linearUnits

    lay->addWidget(new QLabel(tr("Pixels Only:"), this), 5, 0,
                   Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(createUI(pixelsOnly), 5, 1, 1, 2, Qt::AlignLeft);

    insertUI(CurrentRoomChoice, lay, roomItemList);
  }

  insertUI(functionEditorToggle, lay, getComboItemList(functionEditorToggle));
  insertUI(moveCurrentFrameByClickCellArea, lay);
  insertUI(actualPixelViewOnSceneEditingMode, lay);
  insertUI(viewerIndicatorEnabled, lay);
  insertUI(showRasterImagesDarkenBlendedInViewer, lay);
  insertUI(iconSize, lay);
  insertDualUIs(viewShrink, viewStep, lay);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(viewerZoomCenter, lay, getComboItemList(viewerZoomCenter));
  insertUI(CurrentLanguageName, lay, languageItemList);

  if (Preferences::instance()->isShowAdvancedOptionsEnabled()) {
    insertUI(interfaceFont, lay);  // creates QFontComboBox
    insertUI(interfaceFontStyle, lay, buildFontStyleList());
    qobject_cast<QComboBox*>(m_controlIdMap.key(interfaceFontStyle))
        ->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  }

  QGridLayout* colorCalibLay = insertGroupBoxUI(colorCalibrationEnabled, lay);
  { insertUI(colorCalibrationLutPaths, colorCalibLay); }
  insertUI(displayIn30bit, lay);
  row = lay->rowCount();
  lay->addWidget(check30bitBtn, row - 1, 2, Qt::AlignRight);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(showIconsInMenu, lay);

  insertUI(highDpiScalingEnabled, lay);

  lay->setRowStretch(lay->rowCount(), 1);
  insertFootNote(lay);
  widget->setLayout(lay);

  if (Preferences::instance()->isShowAdvancedOptionsEnabled() &&
      m_pref->getBoolValue(pixelsOnly)) {
    m_controlIdMap.key(linearUnits)->setDisabled(true);
    m_controlIdMap.key(cameraUnits)->setDisabled(true);
  }
  // pixels unit may deactivated externally on loading scene (see
  // IoCmd::loadScene())
  bool ret = true;
  ret      = ret && connect(TApp::instance()->getCurrentScene(),
                            SIGNAL(pixelUnitSelected(bool)), this,
                            SLOT(onPixelUnitExternallySelected(bool)));
  ret      = ret && connect(additionalStyleSheetBtn, SIGNAL(clicked()), this,
                            SLOT(onEditAdditionalStyleSheet()));
  ret      = ret && connect(check30bitBtn, SIGNAL(clicked()), this,
                            SLOT(onCheck30bitDisplay()));
  assert(ret);

  m_onEditedFuncMap.insert(CurrentStyleSheetName,
                           &PreferencesPopup::onStyleSheetTypeChanged);
  m_onEditedFuncMap.insert(pixelsOnly, &PreferencesPopup::onPixelsOnlyChanged);
  m_preEditedFuncMap.insert(linearUnits, &PreferencesPopup::beforeUnitChanged);
  m_onEditedFuncMap.insert(linearUnits, &PreferencesPopup::onUnitChanged);
  m_preEditedFuncMap.insert(cameraUnits, &PreferencesPopup::beforeUnitChanged);
  m_onEditedFuncMap.insert(cameraUnits, &PreferencesPopup::onUnitChanged);
  m_preEditedFuncMap.insert(CurrentRoomChoice,
                            &PreferencesPopup::beforeRoomChoiceChanged);
  m_onEditedFuncMap.insert(colorCalibrationEnabled,
                           &PreferencesPopup::onColorCalibrationChanged);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createVisualizationPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(show0ThickLines, lay);
  insertUI(regionAntialias, lay);
  insertUI(rasterizeAntialias, lay);

  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);
  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createLoadingPage() {
  m_levelFormatNames = new QComboBox;
  m_levelFormatNames->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  m_editLevelFormat = new QPushButton(tr("Edit"));

  QPushButton* addLevelFormat    = new QPushButton("+");
  QPushButton* removeLevelFormat = new QPushButton("-");
  addLevelFormat->setFixedSize(20, 20);
  removeLevelFormat->setFixedSize(20, 20);
  rebuildFormatsList();

  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(importPolicy, lay, getComboItemList(importPolicy));
  QGridLayout* autoExposeLay = insertGroupBoxUI(autoExposeEnabled, lay);
  { insertUI(autoRemoveUnusedLevels, autoExposeLay); }
  insertUI(subsceneFolderEnabled, lay);
  insertUI(removeSceneNumberFromLoadedLevelName, lay);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(IgnoreImageDpi, lay);
  insertUI(rasterLevelCachingBehavior, lay,
           getComboItemList(rasterLevelCachingBehavior));
  insertUI(columnIconLoadingPolicy, lay,
           getComboItemList(columnIconLoadingPolicy));

  // levelFormats,// need to be handle separately
  int row = lay->rowCount();
  lay->addWidget(new QLabel(tr("Level Settings by File Format:")), row, 0,
                 Qt::AlignRight | Qt::AlignVCenter);
  QHBoxLayout* levelFormatLay = new QHBoxLayout();
  levelFormatLay->setContentsMargins(0, 0, 0, 0);
  levelFormatLay->setSpacing(5);
  {
    levelFormatLay->addWidget(m_levelFormatNames);
    levelFormatLay->addWidget(addLevelFormat);
    levelFormatLay->addWidget(removeLevelFormat);
    levelFormatLay->addWidget(m_editLevelFormat);
    levelFormatLay->addStretch(1);
  }
  lay->addLayout(levelFormatLay, row, 1, 1, 2);

  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);

  bool ret = true;
  ret      = ret &&
        connect(addLevelFormat, SIGNAL(clicked()), SLOT(onAddLevelFormat()));
  ret = ret && connect(removeLevelFormat, SIGNAL(clicked()),
                       SLOT(onRemoveLevelFormat()));
  ret = ret && connect(m_editLevelFormat, SIGNAL(clicked()),
                       SLOT(onEditLevelFormat()));
  ret = ret && connect(TApp::instance()->getCurrentScene(),
                       SIGNAL(importPolicyChanged(int)), this,
                       SLOT(onImportPolicyExternallyChanged(int)));
  assert(ret);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createSavingPage() {
  auto putLabel = [&](const QString& labelStr, QGridLayout* lay) {
    lay->addWidget(new QLabel(labelStr, this), lay->rowCount(), 0, 1, 3,
                   Qt::AlignLeft | Qt::AlignVCenter);
  };
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);
  QGridLayout* autoSaveLay = insertGroupBoxUI(autosaveEnabled, lay);
  {
    insertUI(autosavePeriod, autoSaveLay);
    insertUI(autosaveSceneEnabled, autoSaveLay);
    insertUI(autosaveOtherFilesEnabled, autoSaveLay);
  }
  insertUI(replaceAfterSaveLevelAs, lay);
  QGridLayout* backupLay = insertGroupBoxUI(backupEnabled, lay);
  { insertUI(backupKeepCount, backupLay); }
  putLabel(tr("Matte color is used for background when overwriting "
              "raster levels with transparent pixels\nin non "
              "alpha-enabled image format."),
           lay);
  insertUI(rasterBackgroundColor, lay);
  insertUI(resetUndoOnSavingLevel, lay);
  insertUI(doNotShowPopupSaveScene, lay);

  insertUI(fastRenderPath, lay);

  QGridLayout* recordHistoryLay = insertGroupBoxUI(recordFileHistory, lay);
  { insertUI(recordAsUsername, recordHistoryLay, QList<ComboBoxItem>(), true); }

  lay->setRowStretch(lay->rowCount(), 1);
  insertFootNote(lay);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(recordAsUsername,
                           &PreferencesPopup::onRecordAsUserChanged);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createImportExportPage() {
  auto putLabel = [&](const QString& labelStr, QGridLayout* lay) {
    lay->addWidget(new QLabel(labelStr, this), lay->rowCount(), 0, 1, 3,
                   Qt::AlignLeft | Qt::AlignVCenter);
  };

  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);
  putLabel(tr("External applications used by Tahoma2D.\nThese come bundled "
              "with Tahoma2D, but you can set path to a different version."),
           lay);

  QGridLayout* ffmpegOptionsLay = insertGroupBox(tr("FFmpeg"), lay);
  {
    insertUI(ffmpegPath, ffmpegOptionsLay);
    insertUI(ffmpegTimeout, ffmpegOptionsLay);

    putLabel(
        tr("Enabling multi-thread rendering will render significantly faster \n"
           "but a random crash might occur, use at your own risk:"),
        ffmpegOptionsLay);
    insertUI(ffmpegMultiThread, ffmpegOptionsLay);
  }

  QGridLayout* rhubarbOptionsLay = insertGroupBox(tr("Rhubarb Lip Sync"), lay);
  {
    insertUI(rhubarbPath, rhubarbOptionsLay);
    insertUI(rhubarbTimeout, rhubarbOptionsLay);
  }

  lay->setRowStretch(lay->rowCount(), 1);
  insertFootNote(lay);
  widget->setLayout(lay);
  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createDrawingPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();

  QPushButton* frameFormatBtn =
      new QPushButton(tr("Default Frame Filename Format"));

  setupLayout(lay);

  insertUI(DefLevelType, lay, getComboItemList(DefLevelType));
  insertUI(DefRasterFormat, lay, getComboItemList(DefRasterFormat));
  int row = lay->rowCount();
  lay->addWidget(frameFormatBtn, row - 1, 2, Qt::AlignLeft);
  insertUI(newLevelSizeToCameraSizeEnabled, lay);
  insertDualUIs(DefLevelWidth, DefLevelHeight, lay);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(DefLevelDpi, lay);
  QGridLayout* creationLay = insertGroupBox(tr("Frame Creation Options"), lay);
  {
    insertUI(NumberingSystem, creationLay, getComboItemList(NumberingSystem));
    insertUI(EnableAutoStretch, creationLay);
    insertUI(EnableImplicitHold, creationLay);
    insertUI(EnableAutoRenumber, creationLay);
  }
  QGridLayout* autoCreationLay = insertGroupBoxUI(EnableAutocreation, lay);
  { insertUI(EnableCreationInHoldCells, autoCreationLay); }
  insertUI(vectorSnappingTarget, lay, getComboItemList(vectorSnappingTarget));
  insertUI(saveUnpaintedInCleanup, lay);
  insertUI(minimizeSaveboxAfterEditing, lay);
  insertUI(useNumpadForSwitchingStyles, lay);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(downArrowInLevelStripCreatesNewFrame, lay);
  QGridLayout* replaceVectorsLay = insertGroupBox(
      tr("Replace Vectors with Simplified Vectors Command"), lay);
  {
    insertUI(keepFillOnVectorSimplify, replaceVectorsLay);
    insertUI(useHigherDpiOnVectorSimplify, replaceVectorsLay);
  }
  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(DefLevelType,
                           &PreferencesPopup::onDefLevelTypeChanged);
  m_onEditedFuncMap.insert(newLevelSizeToCameraSizeEnabled,
                           &PreferencesPopup::onDefLevelTypeChanged);

  onDefLevelTypeChanged();

  if (Preferences::instance()->isShowAdvancedOptionsEnabled() &&
      m_pref->getBoolValue(pixelsOnly)) {
    m_controlIdMap.key(DefLevelDpi)->setDisabled(true);
    getUI<MeasuredDoubleLineEdit*>(DefLevelWidth)->setDecimals(0);
    getUI<MeasuredDoubleLineEdit*>(DefLevelHeight)->setDecimals(0);
  }

  //bool ret = ret && connect(frameFormatBtn, SIGNAL(clicked()), this,
  //                          SLOT(onFrameFormatButton()));
  connect(frameFormatBtn, SIGNAL(clicked()), this, SLOT(onFrameFormatButton()));

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createToolsPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  // insertUI(dropdownShortcutsCycleOptions, lay,
  //         getComboItemList(dropdownShortcutsCycleOptions));
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(FillOnlysavebox, lay);
  insertUI(multiLayerStylePickerEnabled, lay);
  QGridLayout* cursorOptionsLay = insertGroupBox(tr("Cursor Options"), lay);
  {
    insertUI(cursorBrushType, cursorOptionsLay,
             getComboItemList(cursorBrushType));
    insertUI(cursorBrushStyle, cursorOptionsLay,
             getComboItemList(cursorBrushStyle));
    insertUI(cursorOutlineEnabled, cursorOptionsLay);
  }
  insertUI(levelBasedToolsDisplay, lay,
           getComboItemList(levelBasedToolsDisplay));
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(useCtrlAltToResizeBrush, lay);
  insertUI(temptoolswitchtimer, lay);
  insertUI(magnetNonLinearSliderEnabled, lay);
  insertUI(toolScale, lay);

  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);

  //  m_onEditedFuncMap.insert(FillOnlysavebox,
  //                           &PreferencesPopup::notifySceneChanged);
  m_onEditedFuncMap.insert(levelBasedToolsDisplay,
                           &PreferencesPopup::onLevelBasedToolsDisplayChanged);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createXsheetPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(xsheetLayoutPreference, lay,
           getComboItemList(xsheetLayoutPreference));
  insertUI(levelNameDisplayType, lay, getComboItemList(levelNameDisplayType));
  insertUI(xsheetStep, lay);
  insertUI(xsheetAutopanEnabled, lay);
  insertDualUIs(showDragBars, timelineLayoutPreference, lay,
                QList<ComboBoxItem>(),
                getComboItemList(timelineLayoutPreference));
  insertUI(DragCellsBehaviour, lay, getComboItemList(DragCellsBehaviour));
  insertUI(pasteCellsBehavior, lay, getComboItemList(pasteCellsBehavior));
  insertUI(ignoreAlphaonColumn1Enabled, lay);
  QGridLayout* showKeyLay =
      insertGroupBoxUI(showKeyframesOnXsheetCellArea, lay);
  { insertUI(showXsheetCameraColumn, showKeyLay); }
  insertUI(useArrowKeyToShiftCellSelection, lay);
  insertUI(inputCellsWithoutDoubleClickingEnabled, lay);
  insertUI(shortcutCommandsWhileRenamingCellEnabled, lay);
  QGridLayout* xshToolbarLay = insertGroupBox(tr("Scene Tools"), lay);
  {
    insertUI(showQuickToolbar, xshToolbarLay);
    insertUI(showXsheetBreadcrumbs, xshToolbarLay);
    insertUI(expandFunctionHeader, xshToolbarLay);
  }
  insertUI(showColumnNumbers, lay);
// Disabled until OT officially releases it
//  insertUI(unifyColumnVisibilityToggles, lay);
  insertUI(parentColorsInXsheetColumn, lay);
  insertUI(highlightLineEverySecond, lay);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled()) {
    insertUI(syncLevelRenumberWithXsheet, lay);
    insertUI(currentTimelineEnabled, lay);
  }
  insertUI(currentColumnColor, lay);
  insertUI(currentCellColor, lay);
  if (Preferences::instance()->isShowAdvancedOptionsEnabled())
    insertUI(showFrameNumberWithLetters, lay);

  lay->setRowStretch(lay->rowCount(), 1);
  insertFootNote(lay);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(showKeyframesOnXsheetCellArea,
                           &PreferencesPopup::onShowKeyframesOnCellAreaChanged);
  m_onEditedFuncMap.insert(showXsheetCameraColumn,
                           &PreferencesPopup::onShowKeyframesOnCellAreaChanged);
  m_onEditedFuncMap.insert(
      unifyColumnVisibilityToggles,
      &PreferencesPopup::onUnifyColumnVisibilityTogglesChanged);
  m_onEditedFuncMap.insert(showQuickToolbar,
                           &PreferencesPopup::onShowQuickToolbarClicked);
  m_onEditedFuncMap.insert(showXsheetBreadcrumbs,
                           &PreferencesPopup::onShowXsheetBreadcrumbsClicked);
  m_onEditedFuncMap.insert(showDragBars,
                           &PreferencesPopup::onShowDragBarsChanged);
  m_onEditedFuncMap.insert(timelineLayoutPreference,
                           &PreferencesPopup::onShowDragBarsChanged);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createAnimationPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(keyframeType, lay, getComboItemList(keyframeType));
  insertUI(animationStep, lay);
  insertUI(modifyExpressionOnMovingReferences, lay);

  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(
      modifyExpressionOnMovingReferences,
      &PreferencesPopup::onModifyExpressionOnMovingReferencesChanged);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createPreviewPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(blanksCount, lay);
  insertUI(blankColor, lay);
  insertUI(rewindAfterPlayback, lay);
  insertUI(shortPlayFrameCount, lay);
  insertUI(previewAlwaysOpenNewFlip, lay);
  insertUI(fitToFlipbook, lay);
  insertUI(generatedMovieViewEnabled, lay);
  QGridLayout* inbetweenfliplay = insertGroupBox(tr("Inbetween Flip"), lay);
  {
    QList<ComboBoxItem> emptyList;
    insertDualUIs(inbetweenFlipDrawingCount, inbetweenFlipSpeed,
                  inbetweenfliplay, emptyList, emptyList, true, true);
  }


  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(blanksCount, &PreferencesPopup::onBlankCountChanged);
  m_onEditedFuncMap.insert(blankColor, &PreferencesPopup::onBlankColorChanged);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createOnionSkinPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(onionSkinEnabled, lay);
  insertUI(onionPaperThickness, lay);
  insertUI(backOnionColor, lay);
  insertUI(frontOnionColor, lay);
  insertUI(onionInksOnly, lay);
  insertUI(onionSkinDuringPlayback, lay);
  insertUI(useOnionColorsForShiftAndTraceGhosts, lay);
  insertUI(animatedGuidedDrawing, lay, getComboItemList(animatedGuidedDrawing));

  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(onionSkinEnabled,
                           &PreferencesPopup::onOnionSkinVisibilityChanged);
  m_onEditedFuncMap.insert(onionPaperThickness,
                           &PreferencesPopup::notifySceneChanged);
  m_onEditedFuncMap.insert(backOnionColor,
                           &PreferencesPopup::onOnionColorChanged);
  m_onEditedFuncMap.insert(frontOnionColor,
                           &PreferencesPopup::onOnionColorChanged);
  m_onEditedFuncMap.insert(onionInksOnly,
                           &PreferencesPopup::notifySceneChanged);

  bool onionActive = m_pref->getBoolValue(onionSkinEnabled);
  if (!onionActive) {
    m_controlIdMap.key(onionPaperThickness)->setDisabled(true);
    m_controlIdMap.key(backOnionColor)->setDisabled(true);
    m_controlIdMap.key(frontOnionColor)->setDisabled(true);
    m_controlIdMap.key(onionInksOnly)->setDisabled(true);
  }

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createColorsPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);
  insertUI(useThemeViewerColors, lay);
  insertUI(viewerBGColor, lay);
  insertUI(previewBGColor, lay);
  insertUI(levelEditorBoxColor, lay);
  insertUI(chessboardColor1, lay);
  insertUI(chessboardColor2, lay);
  QGridLayout* tcLay = insertGroupBox(tr("Transparency Check"), lay);
  {
    insertUI(transpCheckInkOnWhite, tcLay);
    insertUI(transpCheckInkOnBlack, tcLay);
    insertUI(transpCheckPaint, tcLay);
  }
  lay->setRowStretch(lay->rowCount(), 1);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(viewerBGColor,
                           &PreferencesPopup::notifySceneChanged);
  m_onEditedFuncMap.insert(previewBGColor,
                           &PreferencesPopup::notifySceneChanged);
  m_onEditedFuncMap.insert(levelEditorBoxColor,
                           &PreferencesPopup::notifySceneChanged);
  m_onEditedFuncMap.insert(chessboardColor1,
                           &PreferencesPopup::notifySceneChanged);
  m_onEditedFuncMap.insert(chessboardColor2,
                           &PreferencesPopup::notifySceneChanged);
  m_onEditedFuncMap.insert(chessboardColor1,
                           &PreferencesPopup::onChessboardChanged);
  m_onEditedFuncMap.insert(chessboardColor2,
                           &PreferencesPopup::onChessboardChanged);
  m_onEditedFuncMap.insert(useThemeViewerColors,
                           &PreferencesPopup::onUseThemeViewerColorsChanged);

  bool enable = m_pref->getBoolValue(useThemeViewerColors);
  if (enable) {
    m_controlIdMap.key(viewerBGColor)->setDisabled(true);
    m_controlIdMap.key(previewBGColor)->setDisabled(true);
  }

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createVersionControlPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  insertUI(SVNEnabled, lay);
  insertUI(automaticSVNFolderRefreshEnabled, lay);
  insertUI(latestVersionCheckEnabled, lay);

  lay->setRowStretch(lay->rowCount(), 1);
  insertFootNote(lay);
  widget->setLayout(lay);

  m_onEditedFuncMap.insert(SVNEnabled, &PreferencesPopup::onSVNEnabledChanged);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createTouchTabletPage() {
  bool winInkAvailable = false;
#if defined(_WIN32)
  winInkAvailable = KisTabletSupportWin8::isAvailable();
#endif

  QAction* touchAction =
      CommandManager::instance()->getAction(MI_TouchGestureControl);

  QPushButton* viewerEventLogBtn =
      new QPushButton(tr("Open Viewer Event Log"));

  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

  QGroupBox* touchGesturesBox =
      new QGroupBox(tr("Enable Touch Gesture Controls"), this);
  touchGesturesBox->setCheckable(true);
  touchGesturesBox->setChecked(touchAction->isChecked());
  QGridLayout* touchGestureslay = new QGridLayout();
  setupLayout(touchGestureslay, 5);
  touchGesturesBox->setLayout(touchGestureslay);
  {
    insertUI(gestureUndoMethod, touchGestureslay,
             getComboItemList(gestureUndoMethod));
    insertUI(gestureRedoMethod, touchGestureslay,
             getComboItemList(gestureRedoMethod));
  }

  lay->addWidget(touchGesturesBox, 0, 0, 1, 2);
  lay->addWidget(viewerEventLogBtn, 0, 3, 2, 1, Qt::AlignTop);
  if (winInkAvailable) insertUI(winInkEnabled, lay);
#ifdef WITH_WINTAB
  insertUI(useQtNativeWinInk, lay);
#endif

  lay->setRowStretch(lay->rowCount(), 1);
  if (winInkAvailable) insertFootNote(lay);
  widget->setLayout(lay);

#ifdef MACOSX
  // Can only support 3-finger swipe undo/redo gestures for now
  m_controlIdMap.key(gestureUndoMethod)->setEnabled(false);
  m_controlIdMap.key(gestureRedoMethod)->setEnabled(false);
#endif

  bool ret = true;
  ret = ret && connect(touchGesturesBox, SIGNAL(clicked(bool)), touchAction,
                       SLOT(setChecked(bool)));
  ret = ret && connect(touchAction, SIGNAL(triggered(bool)), touchGesturesBox,
                       SLOT(setChecked(bool)));
  ret = ret && connect(viewerEventLogBtn, SIGNAL(clicked()), this,
                       SLOT(onOpenViewerEventLog()));

  assert(ret);

  return widget;
}

//-----------------------------------------------------------------------------

QWidget* PreferencesPopup::createImportPrefsPage() {
  QWidget* widget  = new QWidget(this);
  QGridLayout* lay = new QGridLayout();
  setupLayout(lay);

#ifdef MACOSX
  QLabel* pathLabel =
      new QLabel(tr("Select the Tahoma2D application that you want to import "
                    "preferences from"));
#else
  QLabel* pathLabel =
      new QLabel(tr("Select the folder of the Tahoma2D application that you "
                    "want to import preferences from"));
#endif

  lay->addWidget(pathLabel, 1, 1);

  m_importPrefpath = new DVGui::FileField(this);
  lay->addWidget(m_importPrefpath, 2, 1);

  QGridLayout* importOptions = insertGroupBox(tr("Import Options"), lay);
  {
    m_importPrefsCB = new QCheckBox(tr("Preferences and Settings"), this);
    m_importPrefsCB->setChecked(true);
    importOptions->addWidget(m_importPrefsCB, 3, 1);

    m_importFavoritesCB = new QCheckBox(tr("Favorites"), this);
    m_importFavoritesCB->setChecked(true);
    importOptions->addWidget(m_importFavoritesCB, 4, 1);

    m_importRoomsCB = new QCheckBox(tr("Room Layouts"), this);
    m_importRoomsCB->setChecked(true);
    importOptions->addWidget(m_importRoomsCB, 5, 1);

    m_importProjectsCB = new QCheckBox(tr("Sandbox and Projects"), this);
    m_importProjectsCB->setChecked(true);
    importOptions->addWidget(m_importProjectsCB, 6, 1);

    m_importFxPluginsCB = new QCheckBox(tr("Fx and Plugins"), this);
    m_importFxPluginsCB->setChecked(true);
    importOptions->addWidget(m_importFxPluginsCB, 7, 1);

    m_importStudioPalettesCB = new QCheckBox(tr("Studio Palettes"), this);
    m_importStudioPalettesCB->setChecked(true);
    importOptions->addWidget(m_importStudioPalettesCB, 8, 1);

    m_importLibraryCB = new QCheckBox(tr("Library"), this);
    m_importLibraryCB->setChecked(true);
    importOptions->addWidget(m_importLibraryCB, 9, 1);

    m_importToonzfarmCB = new QCheckBox(tr("Toonzfarm"), this);
    m_importToonzfarmCB->setChecked(true);
    importOptions->addWidget(m_importToonzfarmCB, 10, 1);
  }

  QPushButton* importBtn = new QPushButton(tr("Import"));
  lay->addWidget(importBtn, 10, 1, Qt::AlignHCenter | Qt::AlignBottom);

  lay->setRowStretch(lay->rowCount(), 1);

  widget->setLayout(lay);

  connect(importBtn, SIGNAL(clicked()), SLOT(onImport()));

  return widget;
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onChange() {
  QWidget* senderWidget = qobject_cast<QWidget*>(sender());
  if (!senderWidget) return;
  PreferencesItemId id = m_controlIdMap.value(senderWidget);

  if (m_preEditedFuncMap.contains(id)) (this->*m_preEditedFuncMap[id])();

  if (CheckBox* cb = dynamic_cast<CheckBox*>(senderWidget))
    m_pref->setValue(id, cb->isChecked());
  else if (IntLineEdit* edit = dynamic_cast<IntLineEdit*>(senderWidget))
    m_pref->setValue(id, edit->getValue());
  else if (QComboBox* comboBox = dynamic_cast<QComboBox*>(senderWidget))
    m_pref->setValue(id, comboBox->currentData());
  else if (DoubleValueLineEdit* field =
               dynamic_cast<DoubleValueLineEdit*>(senderWidget))
    m_pref->setValue(id, field->getValue());
  else if (FileField* field = dynamic_cast<FileField*>(senderWidget))
    m_pref->setValue(id, field->getPath());
  else if (SizeField* field = dynamic_cast<SizeField*>(senderWidget))
    m_pref->setValue(id, field->getValue());
  else if (QGroupBox* groupBox = dynamic_cast<QGroupBox*>(senderWidget))
    m_pref->setValue(id, groupBox->isChecked());
  else if (LineEdit* lineEdit = dynamic_cast<LineEdit*>(senderWidget))
    m_pref->setValue(id, lineEdit->text());
  else if (IntField* field = dynamic_cast<IntField*>(senderWidget))
    m_pref->setValue(id, field->getValue());
  else
    return;

  if (m_onEditedFuncMap.contains(id)) (this->*m_onEditedFuncMap[id])();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onSliderChanged(bool ignore) {
  QWidget* senderWidget = qobject_cast<QWidget*>(sender());
  if (!senderWidget) return;
  PreferencesItemId id = m_controlIdMap.value(senderWidget);

  if (m_preEditedFuncMap.contains(id)) (this->*m_preEditedFuncMap[id])();

  if (IntField* field = dynamic_cast<IntField*>(senderWidget))
    m_pref->setValue(id, field->getValue());
  else
    return;

  if (m_onEditedFuncMap.contains(id)) (this->*m_onEditedFuncMap[id])();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onColorFieldChanged(const TPixel32& color,
                                           bool isDragging) {
  QWidget* senderWidget = qobject_cast<QWidget*>(sender());
  if (!senderWidget) return;
  PreferencesItemId id = m_controlIdMap.value(senderWidget);

  if (m_preEditedFuncMap.contains(id)) (this->*m_preEditedFuncMap[id])();

  // do not save to file while dragging
  m_pref->setValue(id, QColor(color.r, color.g, color.b, color.m), !isDragging);

  // very dirty implementation but I want to avoid calling invalidateIcons
  // while dragging transparency colors sliders..!
  if (id == transpCheckInkOnWhite || id == transpCheckInkOnBlack ||
      id == transpCheckPaint)
    return;

  if (m_onEditedFuncMap.contains(id)) (this->*m_onEditedFuncMap[id])();
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onImportPreferences() {
  m_categoryList->setCurrentRow(-1);
  int stackedSize = m_stackedWidget->count();
  m_stackedWidget->setCurrentIndex(stackedSize - 1);
}

//-----------------------------------------------------------------------------

void PreferencesPopup::onImport() {
  if (m_importPrefpath->getPath().isEmpty()) {
#ifdef MACOSX
    DVGui::error("Please select a Tahoma2D application and try again.");
#else
    DVGui::error(
        "Please select a folder containing a Tahoma2D application and try "
        "again.");
#endif
    return;
  }

  TFilePath oldStuffPath =
      TFilePath(m_importPrefpath->getPath() + "/tahomastuff");
  if (!TFileStatus(oldStuffPath).doesExist()) {
    DVGui::error("Unable to find the 'tahomastuff' folder in " +
                 m_importPrefpath->getPath() +
                 ".\nPlease check path and try again.");
    return;
  }

  if (oldStuffPath == TEnv::getStuffDir()) {
    DVGui::error(
        "Please choose a different Tahoma2D application and try again.");
    return;
  }

  bool useLegacy = false;
  if (!TFileStatus(oldStuffPath + "profiles/users").doesExist())
    useLegacy = true;

  TFilePath srcDir, destDir;

  //-------------------
  // --- Preferences and Settings
  //-------------------
  if (m_importPrefsCB->isChecked()) {
    // -- Settings
    destDir = ToonzFolder::getMyModuleDir();
    if (useLegacy)
      srcDir = oldStuffPath + TFilePath(L"profiles/layouts/settings." +
                                        TSystem::getUserName().toStdWString());
    else
      srcDir = oldStuffPath + TFilePath(L"profiles/users/" +
                                        TSystem::getUserName().toStdWString());
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Settings.\nCould not find " +
                     srcDir.getQString());
    else {
      QString origFfmpegPath  = Preferences::instance()->getFfmpegPath();
      QString origRhubarbPath = Preferences::instance()->getRhubarbPath();

      QFileInfoList fil = QDir(toQString(srcDir)).entryInfoList();
      int i;
      for (i = 0; i < fil.size(); i++) {
        QFileInfo fi = fil.at(i);
        if (fi.fileName() == QString(".") || fi.fileName() == QString(".."))
          continue;
        TFilePath src  = srcDir + fi.fileName().toStdWString(),
                  dest = destDir + fi.fileName().toStdWString();
        if (fi.isDir()) {
          if (fi.fileName() == QString("layouts") ||
              fi.fileName() == QString("favorites"))
            continue;
          TSystem::copyDir(dest, src, true);
        } else
          TSystem::copyFile(dest, src, true);
      }

      // Force reload now as we need to clear out the ffmpeg directory to force
      // it to find it again otherwise it will point to old location
      Preferences::instance()->load();
      Preferences::instance()->setValue(ffmpegPath, origFfmpegPath);
      Preferences::instance()->setValue(rhubarbPath, origRhubarbPath);
    }

    // -- Environment variables
    if (useLegacy) {
      srcDir = oldStuffPath + TFilePath("profiles/env");
      if (!TFileStatus(srcDir).doesExist())
        DVGui::warning("Failed to process Env.\nCould not find " +
                       srcDir.getQString());
      else {
        if (TFileStatus(srcDir +
                        (TSystem::getUserName().toStdWString() + L".env"))
                .doesExist()) {
          TSystem::copyFile(
              destDir + L"env.ini",
              srcDir + (TSystem::getUserName().toStdWString() + L".env"), true);
        }
      }
    }
    // Force reload now because it will automatically save on quit
    if (TFileStatus(destDir + L"env.ini").doesExist())
      TEnv::loadAllEnvVariables();

    // -- Config
    if (useLegacy) {
      srcDir = oldStuffPath + L"config";
      if (!TFileStatus(srcDir).doesExist())
        DVGui::warning("Failed to process Config.\nCould not find " +
                       srcDir.getQString());
      else {
        // Only get specific files and directories, if found, from the config
        // directory
        std::wstring fileList[7] = {
            (TSystem::getUserName().toStdWString() + L"_history.txt"),
            L"brush_raster.txt",
            L"brush_vector.txt",
            L"brush_smartraster.txt",
            L"brush_toonzraster.txt",
            L"reslist.txt",
            L"cleanupreslist.txt"};
        int fileListCount = 7;
        for (int i = 0; i < fileListCount; i++) {
          TFilePath srcFile  = srcDir + fileList[i],
                    destFile = destDir + fileList[i];

          // Filename changes
          if (destFile.getLevelNameW() ==
              (TSystem::getUserName().toStdWString() + L"_history.txt"))
            destFile = destDir + L"file_history.txt";
          else if (destFile.getLevelNameW() == L"brush_toonzraster.txt")
            destFile = destDir + L"brush_smartraster.txt";

          if (TFileStatus(srcFile).doesExist())
            TSystem::copyFile(destFile, srcFile, true);
        }

        if (TFileStatus(srcDir + L"outputpresets").doesExist())
          TSystem::copyDir(destDir + L"outputpresets",
                           srcDir + L"outputpresets", true);
      }
    }
  }
  //-------------------
  // --- Favorites
  //-------------------
  if (m_importFavoritesCB->isChecked()) {
    destDir = ToonzFolder::getMyFavoritesFolder();
    srcDir  = oldStuffPath +
             TFilePath(L"profiles/users/" +
                       TSystem::getUserName().toStdWString() + L"/favorites");
    if (TFileStatus(srcDir).doesExist())
      TSystem::copyDir(destDir, srcDir, true);
  }
  //-------------------
  // --- Room Layouts
  //-------------------
  if (m_importRoomsCB->isChecked()) {
    destDir = ToonzFolder::getMyModuleDir() + TFilePath("layouts/Default");
    if (useLegacy)
      srcDir = oldStuffPath + TFilePath(L"profiles/layouts/personal/Default." +
                                        TSystem::getUserName().toStdWString());
    else
      srcDir = oldStuffPath + TFilePath(L"profiles/users/" +
                                        TSystem::getUserName().toStdWString() +
                                        L"/layouts/Default");
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Room Layouts.\nCould not find " +
                     srcDir.getQString());
    else {
      MainWindow* mainWin =
          qobject_cast<MainWindow*>(TApp::instance()->getMainWindow());

      mainWin->setSaveSettingsOnQuit(false);
      TSystem::rmDirTree(destDir);
      TSystem::mkDir(destDir);
      TSystem::copyDir(destDir, srcDir, true);
    }
  }
  //-------------------
  // --- Sandbox and Projects
  //-------------------
  if (m_importProjectsCB->isChecked()) {
    // -- Sandbox
    destDir = TEnv::getStuffDir() + L"sandbox";
    srcDir  = oldStuffPath + L"sandbox";
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Sandbox.\nCould not find " +
                     srcDir.getQString());
    else {
      TSystem::copyDir(destDir, srcDir, true);

      // Recent history needs to be updated
      if (m_importPrefsCB->isChecked() &&
          TFileStatus(ToonzFolder::getMyModuleDir() + L"RecentFiles.ini")
              .doesExist()) {
        RecentFiles::instance()->clearAllRecentFilesList(false);
        RecentFiles::instance()->loadRecentFiles();
        RecentFiles::instance()->updateStuffPath(srcDir.getQString(),
                                                 destDir.getQString());
        RecentFiles::instance()->saveRecentFiles();
      }
    }

    // -- Projects
    destDir = TEnv::getStuffDir() + L"projects";
    srcDir  = oldStuffPath + L"projects";
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Projects.\nCould not find " +
                     srcDir.getQString());
    else {
      TSystem::copyDir(destDir, srcDir, true);

      // Recent history needs to be updated
      if (m_importPrefsCB->isChecked() &&
          TFileStatus(ToonzFolder::getMyModuleDir() + L"RecentFiles.ini")
              .doesExist()) {
        RecentFiles::instance()->clearAllRecentFilesList(false);
        RecentFiles::instance()->loadRecentFiles();
        RecentFiles::instance()->updateStuffPath(srcDir.getQString(),
                                                 destDir.getQString());
        RecentFiles::instance()->saveRecentFiles();
      }
    }
  }
  //-------------------
  // --- Fxs and Plugins
  //-------------------
  if (m_importFxPluginsCB->isChecked()) {
    // -- Fxs
    destDir = TEnv::getStuffDir() + L"fxs";
    srcDir  = oldStuffPath + L"fxs";
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Fxs.\nCould not find " +
                     srcDir.getQString());
    else
      TSystem::copyDir(destDir, srcDir, true);

    // -- Plugins
    destDir = TEnv::getStuffDir() + L"plugins";
    srcDir  = oldStuffPath + L"plugins";
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Plugins.\nCould not find " +
                     srcDir.getQString());
    else
      TSystem::copyDir(destDir, srcDir, true);
  }
  //-------------------
  // --- Studiopalette
  //-------------------
  if (m_importStudioPalettesCB->isChecked()) {
    destDir = TEnv::getStuffDir() + L"studiopalette";
    srcDir  = oldStuffPath + L"studiopalette";
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Studio Palette.\nCould not find " +
                     srcDir.getQString());
    else
      TSystem::copyDir(destDir, srcDir, true);
  }
  //-------------------
  // --- Library
  //-------------------
  if (m_importLibraryCB->isChecked()) {
    destDir = TEnv::getStuffDir() + L"library";
    srcDir  = oldStuffPath + L"library";
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Library.\nCould not find " +
                     srcDir.getQString());
    else
      TSystem::copyDir(destDir, srcDir, true);
  }
  //-------------------
  // --- Toonzfarm
  //-------------------
  if (m_importToonzfarmCB->isChecked()) {
    destDir = TEnv::getStuffDir() + L"toonzfarm";
    srcDir  = oldStuffPath + L"toonzfarm";
    if (!TFileStatus(srcDir).doesExist())
      DVGui::warning("Failed to process Toonzfarm.\nCould not find " +
                     srcDir.getQString());
    else
      TSystem::copyDir(destDir, srcDir, true);
  }

  DVGui::MsgBoxInPopup(
      DVGui::MsgType(INFORMATION),
      tr("Import complete. Please restart to complete applying the changes."));
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<PreferencesPopup> openPreferencesPopup(MI_Preferences);
