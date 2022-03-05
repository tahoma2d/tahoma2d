#include "stopmotioncontroller.h"
#include "webcam.h"
#include "cameracapturelevelcontrol.h"

// TnzLib includes
#include "toonz/levelset.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"
#include "toonz/toonzscene.h"
#include "toonz/tcamera.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/tstageobjecttree.h"
#include "motionpathpanel.h"
#include "toonz/tproject.h"
#include "toonz/filepathproperties.h"

// TnzCore includes
#include "filebrowsermodel.h"
#include "formatsettingspopups.h"
#include "tapp.h"
#include "tenv.h"
#include "tlevel_io.h"
#include "toutputproperties.h"
#include "tsystem.h"
#include "tfilepath.h"
#include "flipbook.h"
#include "iocommand.h"
#include "tlevel_io.h"
#include "filebrowser.h"

// TnzQt includes
#include "toonzqt/filefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/menubarcommand.h"

// Qt includes
#include <QAction>
#include <QApplication>
#include <QCameraInfo>
#include <QCheckBox>
#include <QComboBox>
#include <QCommonStyle>
#include <QDesktopWidget>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QSerialPort>
#include <QDomDocument>
#include <QHostInfo>
#include <QDesktopServices>

#ifdef _WIN32
#include <dshow.h>
#endif

// Whether to open save-in popup on launch
TEnv::IntVar CamCapOpenSaveInPopupOnLaunch("CamCapOpenSaveInPopupOnLaunch", 0);

// SaveInFolderPopup settings
TEnv::StringVar CamCapSaveInParentFolder("CamCapSaveInParentFolder", "");
TEnv::IntVar CamCapSaveInPopupSubFolder("CamCapSaveInPopupSubFolder", 0);
TEnv::StringVar CamCapSaveInPopupProject("CamCapSaveInPopupProject", "");
TEnv::StringVar CamCapSaveInPopupEpisode("CamCapSaveInPopupEpisode", "1");
TEnv::StringVar CamCapSaveInPopupSequence("CamCapSaveInPopupSequence", "1");
TEnv::StringVar CamCapSaveInPopupScene("CamCapSaveInPopupScene", "1");
TEnv::IntVar CamCapSaveInPopupAutoSubName("CamCapSaveInPopupAutoSubName", 1);
TEnv::IntVar CamCapSaveInPopupCreateSceneInFolder(
    "CamCapSaveInPopupCreateSceneInFolder", 0);
TEnv::IntVar CamCapDoCalibration("CamCapDoCalibration", 0);

namespace {

//-----------------------------------------------------------------------------

#ifdef _WIN32
void openCaptureFilterSettings(const QWidget *parent,
                               const QString &cameraName) {
  HRESULT hr;

  ICreateDevEnum *createDevEnum = NULL;
  IEnumMoniker *enumMoniker     = NULL;
  IMoniker *moniker             = NULL;

  IBaseFilter *deviceFilter;

  ISpecifyPropertyPages *specifyPropertyPages;
  CAUUID cauuid;
  // set parent's window handle in order to make the dialog modal
  HWND ghwndApp = (HWND)(parent->winId());

  // initialize COM
  CoInitialize(NULL);

  // get device list
  CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                   IID_ICreateDevEnum, (PVOID *)&createDevEnum);

  // create EnumMoniker
  createDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,
                                       &enumMoniker, 0);
  if (enumMoniker == NULL) {
    // if no connected devices found
    return;
  }

  // reset EnumMoniker
  enumMoniker->Reset();

  // find target camera
  ULONG fetched      = 0;
  bool isCameraFound = false;
  while (hr = enumMoniker->Next(1, &moniker, &fetched), hr == S_OK) {
    // get friendly name (= device name) of the camera
    IPropertyBag *pPropertyBag;
    moniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropertyBag);
    VARIANT var;
    var.vt = VT_BSTR;
    VariantInit(&var);

    pPropertyBag->Read(L"FriendlyName", &var, 0);

    QString deviceName = QString::fromWCharArray(var.bstrVal);

    VariantClear(&var);

    if (deviceName == cameraName) {
      // bind monkier to the filter
      moniker->BindToObject(0, 0, IID_IBaseFilter, (void **)&deviceFilter);

      // release moniker etc.
      moniker->Release();
      enumMoniker->Release();
      createDevEnum->Release();

      isCameraFound = true;
      break;
    }
  }

  // if no matching camera found
  if (!isCameraFound) return;

  // open capture filter popup
  hr = deviceFilter->QueryInterface(IID_ISpecifyPropertyPages,
                                    (void **)&specifyPropertyPages);
  if (hr == S_OK) {
    hr = specifyPropertyPages->GetPages(&cauuid);

    hr = OleCreatePropertyFrame(ghwndApp, 30, 30, NULL, 1,
                                (IUnknown **)&deviceFilter, cauuid.cElems,
                                (GUID *)cauuid.pElems, 0, 0, NULL);

    CoTaskMemFree(cauuid.pElems);
    specifyPropertyPages->Release();
  }
}
#endif

//-----------------------------------------------------------------------------

QScrollArea *makeChooserPage(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

//-----------------------------------------------------------------------------

QScrollArea *makeChooserPageWithoutScrollBar(QWidget *chooser) {
  QScrollArea *scrollArea = new QScrollArea();
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scrollArea->setWidgetResizable(true);
  scrollArea->setWidget(chooser);
  return scrollArea;
}

//-----------------------------------------------------------------------------

QChar numToLetter(int letterNum) {
  switch (letterNum) {
  case 0:
    return QChar();
    break;
  case 1:
    return 'A';
    break;
  case 2:
    return 'B';
    break;
  case 3:
    return 'C';
    break;
  case 4:
    return 'D';
    break;
  case 5:
    return 'E';
    break;
  case 6:
    return 'F';
    break;
  case 7:
    return 'G';
    break;
  case 8:
    return 'H';
    break;
  case 9:
    return 'I';
    break;
  default:
    return QChar();
    break;
  }
}

int letterToNum(QChar appendix) {
  if (appendix == QChar('A') || appendix == QChar('a'))
    return 1;
  else if (appendix == QChar('B') || appendix == QChar('b'))
    return 2;
  else if (appendix == QChar('C') || appendix == QChar('c'))
    return 3;
  else if (appendix == QChar('D') || appendix == QChar('d'))
    return 4;
  else if (appendix == QChar('E') || appendix == QChar('e'))
    return 5;
  else if (appendix == QChar('F') || appendix == QChar('f'))
    return 6;
  else if (appendix == QChar('G') || appendix == QChar('g'))
    return 7;
  else if (appendix == QChar('H') || appendix == QChar('h'))
    return 8;
  else if (appendix == QChar('I') || appendix == QChar('i'))
    return 9;
  else
    return 0;
}

QString convertToFrameWithLetter(int value, int length = -1) {
  QString str;
  str.setNum((int)(value / 10));
  while (str.length() < length) str.push_front("0");
  QChar letter = numToLetter(value % 10);
  if (!letter.isNull()) str.append(letter);
  return str;
}

}  // namespace

//=============================================================================

StopMotionSaveInFolderPopup::StopMotionSaveInFolderPopup(QWidget *parent)
    : Dialog(parent, true, false, "PencilTestSaveInFolder") {
  setWindowTitle(tr("Create the Destination Subfolder to Save"));

  m_parentFolderField = new DVGui::FileField(this);

  QPushButton *setAsDefaultBtn = new QPushButton(tr("Set As Default"), this);
  setAsDefaultBtn->setToolTip(
      tr("Set the current \"Save In\" path as the default."));

  m_subFolderCB = new QCheckBox(tr("Create Subfolder"), this);

  QFrame *subFolderFrame = new QFrame(this);

  QGroupBox *infoGroupBox    = new QGroupBox(tr("Infomation"), this);
  QGroupBox *subNameGroupBox = new QGroupBox(tr("Subfolder Name"), this);

  m_projectField  = new QLineEdit(this);
  m_episodeField  = new QLineEdit(this);
  m_sequenceField = new QLineEdit(this);
  m_sceneField    = new QLineEdit(this);

  m_autoSubNameCB      = new QCheckBox(tr("Auto Format:"), this);
  m_subNameFormatCombo = new QComboBox(this);
  m_subFolderNameField = new QLineEdit(this);

  QCheckBox *showPopupOnLaunchCB =
      new QCheckBox(tr("Show This on Launch of the Camera Capture"), this);
  m_createSceneInFolderCB = new QCheckBox(tr("Save Scene in Subfolder"), this);

  QPushButton *okBtn     = new QPushButton(tr("OK"), this);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);

  //---- properties

  m_subFolderCB->setChecked(CamCapSaveInPopupSubFolder != 0);
  subFolderFrame->setEnabled(CamCapSaveInPopupSubFolder != 0);

  // project name
  QString prjName = QString::fromStdString(CamCapSaveInPopupProject.getValue());
  if (prjName.isEmpty()) {
    prjName = TProjectManager::instance()
                  ->getCurrentProject()
                  ->getName()
                  .getQString();
  }
  m_projectField->setText(prjName);

  m_episodeField->setText(
      QString::fromStdString(CamCapSaveInPopupEpisode.getValue()));
  m_sequenceField->setText(
      QString::fromStdString(CamCapSaveInPopupSequence.getValue()));
  m_sceneField->setText(
      QString::fromStdString(CamCapSaveInPopupScene.getValue()));

  m_autoSubNameCB->setChecked(CamCapSaveInPopupAutoSubName != 0);
  m_subNameFormatCombo->setEnabled(CamCapSaveInPopupAutoSubName != 0);
  QStringList items;
  items << tr("C- + Sequence + Scene") << tr("Sequence + Scene")
        << tr("Episode + Sequence + Scene")
        << tr("Project + Episode + Sequence + Scene");
  m_subNameFormatCombo->addItems(items);
  m_subNameFormatCombo->setCurrentIndex(CamCapSaveInPopupAutoSubName - 1);

  showPopupOnLaunchCB->setChecked(CamCapOpenSaveInPopupOnLaunch != 0);
  m_createSceneInFolderCB->setChecked(CamCapSaveInPopupCreateSceneInFolder !=
                                      0);
  m_createSceneInFolderCB->setToolTip(
      tr("Save the current scene in the subfolder.\nSet the output folder path "
         "to the subfolder as well."));

  addButtonBarWidget(okBtn, cancelBtn);

  //---- layout
  m_topLayout->setMargin(10);
  m_topLayout->setSpacing(10);
  {
    QGridLayout *saveInLay = new QGridLayout();
    saveInLay->setMargin(0);
    saveInLay->setHorizontalSpacing(3);
    saveInLay->setVerticalSpacing(0);
    {
      saveInLay->addWidget(new QLabel(tr("Save In:"), this), 0, 0,
                           Qt::AlignRight | Qt::AlignVCenter);
      saveInLay->addWidget(m_parentFolderField, 0, 1);
      saveInLay->addWidget(setAsDefaultBtn, 1, 1);
    }
    saveInLay->setColumnStretch(0, 0);
    saveInLay->setColumnStretch(1, 1);
    m_topLayout->addLayout(saveInLay);

    m_topLayout->addWidget(m_subFolderCB, 0, Qt::AlignLeft);

    QVBoxLayout *subFolderLay = new QVBoxLayout();
    subFolderLay->setMargin(0);
    subFolderLay->setSpacing(10);
    {
      QGridLayout *infoLay = new QGridLayout();
      infoLay->setMargin(10);
      infoLay->setHorizontalSpacing(3);
      infoLay->setVerticalSpacing(10);
      {
        infoLay->addWidget(new QLabel(tr("Project:"), this), 0, 0);
        infoLay->addWidget(m_projectField, 0, 1);

        infoLay->addWidget(new QLabel(tr("Episode:"), this), 1, 0);
        infoLay->addWidget(m_episodeField, 1, 1);

        infoLay->addWidget(new QLabel(tr("Sequence:"), this), 2, 0);
        infoLay->addWidget(m_sequenceField, 2, 1);

        infoLay->addWidget(new QLabel(tr("Scene:"), this), 3, 0);
        infoLay->addWidget(m_sceneField, 3, 1);
      }
      infoLay->setColumnStretch(0, 0);
      infoLay->setColumnStretch(1, 1);
      infoGroupBox->setLayout(infoLay);
      subFolderLay->addWidget(infoGroupBox, 0);

      QGridLayout *subNameLay = new QGridLayout();
      subNameLay->setMargin(10);
      subNameLay->setHorizontalSpacing(3);
      subNameLay->setVerticalSpacing(10);
      {
        subNameLay->addWidget(m_autoSubNameCB, 0, 0);
        subNameLay->addWidget(m_subNameFormatCombo, 0, 1);

        subNameLay->addWidget(new QLabel(tr("Subfolder Name:"), this), 1, 0);
        subNameLay->addWidget(m_subFolderNameField, 1, 1);
      }
      subNameLay->setColumnStretch(0, 0);
      subNameLay->setColumnStretch(1, 1);
      subNameGroupBox->setLayout(subNameLay);
      subFolderLay->addWidget(subNameGroupBox, 0);

      subFolderLay->addWidget(m_createSceneInFolderCB, 0, Qt::AlignLeft);
    }
    subFolderFrame->setLayout(subFolderLay);
    m_topLayout->addWidget(subFolderFrame);

    m_topLayout->addWidget(showPopupOnLaunchCB, 0, Qt::AlignLeft);

    m_topLayout->addStretch(1);
  }

  resize(300, 440);

  //---- signal-slot connection
  bool ret = true;

  ret = ret && connect(m_subFolderCB, SIGNAL(clicked(bool)), subFolderFrame,
                       SLOT(setEnabled(bool)));
  ret = ret && connect(m_projectField, SIGNAL(textEdited(const QString &)),
                       this, SLOT(updateSubFolderName()));
  ret = ret && connect(m_episodeField, SIGNAL(textEdited(const QString &)),
                       this, SLOT(updateSubFolderName()));
  ret = ret && connect(m_sequenceField, SIGNAL(textEdited(const QString &)),
                       this, SLOT(updateSubFolderName()));
  ret = ret && connect(m_sceneField, SIGNAL(textEdited(const QString &)), this,
                       SLOT(updateSubFolderName()));
  ret = ret && connect(m_autoSubNameCB, SIGNAL(clicked(bool)), this,
                       SLOT(onAutoSubNameCBClicked(bool)));
  ret = ret && connect(m_subNameFormatCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(updateSubFolderName()));

  ret = ret && connect(showPopupOnLaunchCB, SIGNAL(clicked(bool)), this,
                       SLOT(onShowPopupOnLaunchCBClicked(bool)));
  ret = ret && connect(m_createSceneInFolderCB, SIGNAL(clicked(bool)), this,
                       SLOT(onCreateSceneInFolderCBClicked(bool)));
  ret = ret && connect(setAsDefaultBtn, SIGNAL(pressed()), this,
                       SLOT(onSetAsDefaultBtnPressed()));

  ret = ret && connect(okBtn, SIGNAL(clicked(bool)), this, SLOT(onOkPressed()));
  ret = ret && connect(cancelBtn, SIGNAL(clicked(bool)), this, SLOT(reject()));
  assert(ret);

  updateSubFolderName();
}

//-----------------------------------------------------------------------------

QString StopMotionSaveInFolderPopup::getPath() {
  if (!m_subFolderCB->isChecked()) return m_parentFolderField->getPath();

  // re-code filepath
  TFilePath path(m_parentFolderField->getPath() + "\\" +
                 m_subFolderNameField->text());
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene) {
    path = scene->decodeFilePath(path);
    path = scene->codeFilePath(path);
  }
  return path.getQString();
}

//-----------------------------------------------------------------------------

QString StopMotionSaveInFolderPopup::getParentPath() {
  return m_parentFolderField->getPath();
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::showEvent(QShowEvent *event) {
  // Show "Save the scene" check box only when the scene is untitled
  bool isUntitled =
      TApp::instance()->getCurrentScene()->getScene()->isUntitled();
  m_createSceneInFolderCB->setVisible(isUntitled);
}

//-----------------------------------------------------------------------------
namespace {
QString formatString(QString inStr, int charNum) {
  if (inStr.isEmpty()) return QString("0").rightJustified(charNum, '0');

  QString numStr, postStr;
  // find the first non-digit character
  int index = inStr.indexOf(QRegExp("[^0-9]"), 0);

  if (index == -1)  // only digits
    numStr = inStr;
  else if (index == 0)  // only post strings
    return inStr;
  else {  // contains both
    numStr  = inStr.left(index);
    postStr = inStr.right(inStr.length() - index);
  }
  return numStr.rightJustified(charNum, '0') + postStr;
}
};  // namespace

void StopMotionSaveInFolderPopup::updateSubFolderName() {
  if (!m_autoSubNameCB->isChecked()) return;

  QString episodeStr  = formatString(m_episodeField->text(), 3);
  QString sequenceStr = formatString(m_sequenceField->text(), 3);
  QString sceneStr    = formatString(m_sceneField->text(), 4);

  QString str;

  switch (m_subNameFormatCombo->currentIndex()) {
  case 0:  // C- + Sequence + Scene
    str = QString("C-%1-%2").arg(sequenceStr).arg(sceneStr);
    break;
  case 1:  // Sequence + Scene
    str = QString("%1-%2").arg(sequenceStr).arg(sceneStr);
    break;
  case 2:  // Episode + Sequence + Scene
    str = QString("%1-%2-%3").arg(episodeStr).arg(sequenceStr).arg(sceneStr);
    break;
  case 3:  // Project + Episode + Sequence + Scene
    str = QString("%1-%2-%3-%4")
              .arg(m_projectField->text())
              .arg(episodeStr)
              .arg(sequenceStr)
              .arg(sceneStr);
    break;
  default:
    return;
  }
  m_subFolderNameField->setText(str);
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::onAutoSubNameCBClicked(bool on) {
  m_subNameFormatCombo->setEnabled(on);
  updateSubFolderName();
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::onShowPopupOnLaunchCBClicked(bool on) {
  CamCapOpenSaveInPopupOnLaunch = (on) ? 1 : 0;
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::onCreateSceneInFolderCBClicked(bool on) {
  CamCapSaveInPopupCreateSceneInFolder = (on) ? 1 : 0;
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::onSetAsDefaultBtnPressed() {
  CamCapSaveInParentFolder = m_parentFolderField->getPath().toStdString();
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::onOkPressed() {
  if (!m_subFolderCB->isChecked()) {
    accept();
    return;
  }

  // check the subFolder value
  QString subFolderName = m_subFolderNameField->text();
  if (subFolderName.isEmpty()) {
    DVGui::MsgBox(DVGui::WARNING, tr("Subfolder name should not be empty."));
    return;
  }

  int index = subFolderName.indexOf(QRegExp("[\\]:;|=,\\[\\*\\.\"/\\\\]"), 0);
  if (index >= 0) {
    DVGui::MsgBox(DVGui::WARNING,
                  tr("Subfolder name should not contain following "
                     "characters:  * . \" / \\ [ ] : ; | = , "));
    return;
  }

  TFilePath fp(m_parentFolderField->getPath());
  fp += TFilePath(subFolderName);
  TFilePath actualFp =
      TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(fp);

  if (QFileInfo::exists(actualFp.getQString())) {
    DVGui::MsgBox(DVGui::WARNING,
                  tr("Folder %1 already exists.").arg(actualFp.getQString()));
    return;
  }

  // save the current properties to env data
  CamCapSaveInPopupSubFolder   = (m_subFolderCB->isChecked()) ? 1 : 0;
  CamCapSaveInPopupProject     = m_projectField->text().toStdString();
  CamCapSaveInPopupEpisode     = m_episodeField->text().toStdString();
  CamCapSaveInPopupSequence    = m_sequenceField->text().toStdString();
  CamCapSaveInPopupScene       = m_sceneField->text().toStdString();
  CamCapSaveInPopupAutoSubName = (!m_autoSubNameCB->isChecked())
                                     ? 0
                                     : m_subNameFormatCombo->currentIndex() + 1;

  // create folder
  try {
    TSystem::mkDir(actualFp);
  } catch (...) {
    DVGui::MsgBox(DVGui::CRITICAL,
                  tr("It is not possible to create the %1 folder.")
                      .arg(toQString(actualFp)));
    return;
  }

  createSceneInFolder();
  accept();
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::createSceneInFolder() {
  // make sure that the check box is displayed (= the scene is untitled) and is
  // checked.
  if (m_createSceneInFolderCB->isHidden() ||
      !m_createSceneInFolderCB->isChecked())
    return;
  // just in case
  if (!m_subFolderCB->isChecked()) return;

  // set the output folder
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;

  TFilePath fp(getPath().toStdWString());

  // for the scene folder mode, output destination must be already set to
  // $scenefolder or its subfolder. See TSceneProperties::onInitialize()
  if (Preferences::instance()->getPathAliasPriority() !=
      Preferences::SceneFolderAlias) {
    TOutputProperties *prop = scene->getProperties()->getOutputProperties();
    prop->setPath(prop->getPath().withParentDir(fp));
  }

  // save the scene
  TFilePath sceneFp =
      scene->decodeFilePath(fp) +
      TFilePath(m_subFolderNameField->text().toStdWString()).withType("tnz");
  IoCmd::saveScene(sceneFp, 0);
}

//-----------------------------------------------------------------------------

void StopMotionSaveInFolderPopup::updateParentFolder() {
  // If the parent folder is saved in the scene, use it
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  QString parentFolder =
      scene->getProperties()->cameraCaptureSaveInPath().getQString();
  if (parentFolder.isEmpty()) {
    // else then, if the user-env stores the parent folder value, use it
    parentFolder = QString::fromStdString(CamCapSaveInParentFolder);
    // else, use "+extras" project folder
    if (parentFolder.isEmpty())
      parentFolder =
          QString("+%1").arg(QString::fromStdString(TProject::Extras));
  }

  m_parentFolderField->setPath(parentFolder);
}

//=============================================================================

FrameNumberLineEdit::FrameNumberLineEdit(QWidget* parent, TFrameId fId,
                                         bool acceptLetter)
    : LineEdit(parent) {
  if (acceptLetter) {
    QString regExpStr   = QString("^%1$").arg(TFilePath::fidRegExpStr());
    m_regexpValidator   = new QRegExpValidator(QRegExp(regExpStr), this);
    TProjectManager* pm = TProjectManager::instance();
    pm->addListener(this);
  } else
    m_regexpValidator = new QRegExpValidator(QRegExp("^\\d{1,4}$"), this);

  m_regexpValidator_alt =
      new QRegExpValidator(QRegExp("^\\d{1,3}[A-Ia-i]?$"), this);

  updateValidator();
  updateSize();

  setValue(fId);
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::updateValidator() {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled())
    setValidator(m_regexpValidator_alt);
  else
    setValidator(m_regexpValidator);
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::updateSize() {
  FilePathProperties* fpProp =
      TProjectManager::instance()->getCurrentProject()->getFilePathProperties();
  bool useStandard = fpProp->useStandard();
  int letterCount  = fpProp->letterCountForSuffix();
  if (useStandard)
    setFixedWidth(60);
  else {
    // 4 digits + letters reserve 12 px each
    int lc = (letterCount == 0) ? 9 : letterCount + 4;
    setFixedWidth(12 * lc);
  }
  updateGeometry();
}
//-----------------------------------------------------------------------------

void FrameNumberLineEdit::setValue(TFrameId fId) {
  QString str;
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    if (!fId.getLetter().isEmpty()) {
      // need some warning?
    }
    str = convertToFrameWithLetter(fId.getNumber(), 3);
  } else {
    str = QString::fromStdString(fId.expand());
  }
  setText(str);
  setCursorPosition(0);
}

//-----------------------------------------------------------------------------

TFrameId FrameNumberLineEdit::getValue() {
  if (Preferences::instance()->isShowFrameNumberWithLettersEnabled()) {
    QString str = text();
    int f;
    // if no letters added
    if (str.at(str.size() - 1).isDigit())
      f = str.toInt() * 10;
    else {
      f = str.left(str.size() - 1).toInt() * 10 +
          letterToNum(str.at(str.size() - 1));
    }
    return TFrameId(f);
  } else {
    QString regExpStr = QString("^%1$").arg(TFilePath::fidRegExpStr());
    QRegExp rx(regExpStr);
    int pos = rx.indexIn(text());
    if (pos < 0) return TFrameId();
    if (rx.cap(2).isEmpty())
      return TFrameId(rx.cap(1).toInt());
    else
      return TFrameId(rx.cap(1).toInt(), rx.cap(2));
  }
}

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::onProjectSwitched() {
  QRegExpValidator* oldValidator = m_regexpValidator;
  QString regExpStr = QString("^%1$").arg(TFilePath::fidRegExpStr());
  m_regexpValidator = new QRegExpValidator(QRegExp(regExpStr), this);
  updateValidator();
  if (oldValidator) delete oldValidator;
  updateSize();
}

void FrameNumberLineEdit::onProjectChanged() { onProjectSwitched(); }

//-----------------------------------------------------------------------------

void FrameNumberLineEdit::focusInEvent(QFocusEvent *e) {
  m_textOnFocusIn = text();
}

void FrameNumberLineEdit::focusOutEvent(QFocusEvent *e) {
  // if the field is empty, then revert the last input
  if (text().isEmpty()) setText(m_textOnFocusIn);

  LineEdit::focusOutEvent(e);
}

//=============================================================================

LevelNameLineEdit::LevelNameLineEdit(QWidget *parent)
    : DVGui::LineEdit(parent), m_textOnFocusIn("") {
  // Exclude all character which cannot fit in a filepath (Win).
  // Dots are also prohibited since they are internally managed by Toonz.
  QRegExp rx("[^\\\\/:?*.\"<>|]+");
  setValidator(new QRegExpValidator(rx, this));
  setObjectName("LargeSizedText");

  connect(this, SIGNAL(editingFinished()), this, SLOT(onEditingFinished()));
}

void LevelNameLineEdit::focusInEvent(QFocusEvent *e) {
  m_textOnFocusIn = text();
}

void LevelNameLineEdit::onEditingFinished() {
  // if the content is not changed, do nothing.
  if (text() == m_textOnFocusIn) return;

  emit levelNameEdited();
}

//*****************************************************************************
//    StopMotionController  implementation
//*****************************************************************************

StopMotionController::StopMotionController(QWidget *parent) : QWidget(parent) {
  m_stopMotion = StopMotion::instance();
  m_tabBar     = new DVGui::TabBar(this);
  m_tabBar->setDrawBase(false);
  m_tabBar->setObjectName("StopMotionTabBar");
  m_tabBar->addSimpleTab(tr("Controls"));
  m_tabBar->addSimpleTab(tr("Settings"));
  m_tabBar->addSimpleTab(tr("Options"));
  m_tabBar->addSimpleTab(tr("Light"));
  m_tabBar->addSimpleTab(tr("Tests"));
  m_tabBar->addSimpleTab(tr("Paths"));
  // m_tabBar->addSimpleTab(tr("Motion"));
  m_tabBarContainer    = new TabBarContainter(this);
  m_mainControlsPage   = new QFrame(this);
  m_cameraSettingsPage = new QFrame(this);
  m_optionsPage        = new QFrame(this);
  m_motionPage         = new QFrame(this);
  m_lightPage          = new QFrame(this);
  m_testsPage          = new QFrame(this);
  m_pathsPage          = new QFrame(this);

  // **********************
  // Make Control Page
  // **********************

  m_saveInFolderPopup = new StopMotionSaveInFolderPopup(this);
  m_cameraListCombo   = new QComboBox(this);
  m_resolutionCombo   = new QComboBox(this);
  m_resolutionCombo->setFixedWidth(fontMetrics().width("0000 x 0000") + 40);
  m_resolutionLabel                 = new QLabel(tr("Resolution: "), this);
  m_cameraStatusLabel               = new QLabel(tr("Camera Status"), this);
  QPushButton *refreshCamListButton = new QPushButton(tr("Refresh"), this);
  refreshCamListButton->setFixedHeight(28);
  refreshCamListButton->setStyleSheet("padding: 0 2;");
  QGroupBox *fileFrame = new QGroupBox(tr("File"), this);
  m_levelNameEdit      = new LevelNameLineEdit(this);

  // set the start frame 10 if the option in preferences
  // "Show ABC Appendix to the Frame Number in Xsheet Cell" is active.
  // (frame 10 is displayed as "1" with this option)
  int startFrame =
      Preferences::instance()->isShowFrameNumberWithLettersEnabled() ? 10 : 1;
  m_frameNumberEdit = new FrameNumberLineEdit(this, startFrame);
  m_frameInfoLabel  = new QLabel("", this);

  m_xSheetFrameNumberEdit = new DVGui::IntLineEdit(this, 1, 1);
  m_saveInFileFld =
      new DVGui::FileField(this, m_saveInFolderPopup->getParentPath());
  QToolButton *nextLevelButton       = new QToolButton(this);
  m_previousLevelButton              = new QToolButton(this);
  QPushButton *nextOpenLevelButton   = new QPushButton(this);
  QToolButton *nextFrameButton       = new QToolButton(this);
  m_previousFrameButton              = new QToolButton(this);
  QPushButton *lastFrameButton       = new QPushButton(this);
  QToolButton *nextXSheetFrameButton = new QToolButton(this);
  m_previousXSheetFrameButton        = new QToolButton(this);
  m_onionOpacityFld                  = new DVGui::IntField(this);

  m_captureFramesCombo = new QComboBox(this);
  m_captureFramesCombo->addItems(
      {"1s", "2s", "3s", "4s", "5s", "6s", "7s", "8s"});
  m_captureFramesCombo->setCurrentIndex(
      m_stopMotion->getCaptureNumberOfFrames() - 1);

  // should choosing the file type is disabled for simplicity
  // too many options can be a bad thing
  m_fileTypeCombo          = new QComboBox(this);
  m_fileFormatOptionButton = new QPushButton(tr("Options"), this);
  m_fileFormatOptionButton->setFixedHeight(28);
  m_fileFormatOptionButton->setStyleSheet("padding: 0 2;");
  // QPushButton *subfolderButton = new QPushButton(tr("Subfolder"), this);
  m_fileTypeCombo->addItems({"jpg", "png", "tga", "tif"});
  m_fileTypeCombo->setCurrentIndex(0);

  fileFrame->setObjectName("CleanupSettingsFrame");
  m_frameNumberEdit->setObjectName("LargeSizedText");
  m_frameInfoLabel->setAlignment(Qt::AlignRight);
  nextLevelButton->setFixedSize(24, 24);
  nextLevelButton->setArrowType(Qt::RightArrow);
  nextLevelButton->setToolTip(tr("Next Level"));
  nextOpenLevelButton->setText(tr("Next New"));
  nextOpenLevelButton->setFixedHeight(28);
  nextOpenLevelButton->setStyleSheet("padding: 0 2;");
  nextOpenLevelButton->setSizePolicy(QSizePolicy::Maximum,
                                     QSizePolicy::Maximum);
  m_previousLevelButton->setFixedSize(24, 24);
  m_previousLevelButton->setArrowType(Qt::LeftArrow);
  m_previousLevelButton->setToolTip(tr("Previous Level"));
  nextFrameButton->setFixedSize(24, 24);
  nextFrameButton->setArrowType(Qt::RightArrow);
  nextFrameButton->setToolTip(tr("Next Frame"));
  lastFrameButton->setText(tr("Last Frame"));
  lastFrameButton->setFixedHeight(28);
  lastFrameButton->setStyleSheet("padding: 0 2;");
  lastFrameButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_previousFrameButton->setFixedSize(24, 24);
  m_previousFrameButton->setArrowType(Qt::LeftArrow);
  m_previousFrameButton->setToolTip(tr("Previous Frame"));

  m_xSheetFrameNumberEdit->setObjectName("LargeSizedText");
  nextXSheetFrameButton->setFixedSize(24, 24);
  nextXSheetFrameButton->setArrowType(Qt::RightArrow);
  nextXSheetFrameButton->setToolTip(tr("Next XSheet Frame"));
  m_previousXSheetFrameButton->setFixedSize(24, 24);
  m_previousXSheetFrameButton->setArrowType(Qt::LeftArrow);
  m_previousXSheetFrameButton->setToolTip(tr("Previous XSheet Frame"));

  m_setToCurrentXSheetFrameButton = new QPushButton(this);
  m_setToCurrentXSheetFrameButton->setText(tr("Current Frame"));
  m_setToCurrentXSheetFrameButton->setFixedHeight(28);
  m_setToCurrentXSheetFrameButton->setSizePolicy(QSizePolicy::Maximum,
                                                 QSizePolicy::Maximum);
  m_setToCurrentXSheetFrameButton->setStyleSheet("padding: 2px;");
  m_setToCurrentXSheetFrameButton->setToolTip(
      tr("Set to the Current Playhead Location"));

  m_onionOpacityFld->setRange(0, 100);
  m_onionOpacityFld->setValue(100);
  m_onionOpacityFld->setDisabled(false);
  m_toggleLiveViewButton = new QPushButton(tr("Start Live View"));
  m_toggleLiveViewButton->setObjectName("LargeSizedText");
  m_toggleLiveViewButton->setFixedHeight(35);
  m_captureButton = new QPushButton(tr("Capture"), this);
  m_captureButton->setObjectName("LargeSizedText");
  m_captureButton->setFixedHeight(35);
  QCommonStyle style;
  m_captureButton->setIcon(style.standardIcon(QStyle::SP_DialogOkButton));
  m_captureButton->setIconSize(QSize(20, 20));
  m_alwaysUseLiveViewImagesButton = new QPushButton();
  // m_alwaysUseLiveViewImagesButton->setObjectName("LargeSizedText");
  m_alwaysUseLiveViewImagesButton->setObjectName("LiveViewButton");
  m_alwaysUseLiveViewImagesButton->setFixedHeight(35);
  m_alwaysUseLiveViewImagesButton->setFixedWidth(35);
  m_alwaysUseLiveViewImagesButton->setCheckable(true);
  m_alwaysUseLiveViewImagesButton->setIconSize(QSize(25, 25));
  m_alwaysUseLiveViewImagesButton->setToolTip(
      tr("Show original live view images in timeline"));

  // subfolderButton->setObjectName("SubfolderButton");
  // subfolderButton->setIconSize(QSize(15, 15));
  m_saveInFileFld->setMaximumWidth(380);
  m_levelNameEdit->setMaximumWidth(380);

  m_saveInFolderPopup->hide();
  m_zoomButton = new QPushButton(tr("Check"), this);
  m_zoomButton->setFixedHeight(28);
  m_zoomButton->setStyleSheet("padding: 5 2;");
  m_zoomButton->setMaximumWidth(100);
  m_zoomButton->setToolTip(tr("Zoom in to check focus"));
  m_zoomButton->setCheckable(true);
  m_pickZoomButton = new QPushButton(tr("Pick"), this);
  m_pickZoomButton->setStyleSheet("padding: 5 2;");
  m_pickZoomButton->setMaximumWidth(100);
  m_pickZoomButton->setFixedHeight(28);
  m_pickZoomButton->setToolTip(tr("Set focus check location"));
  m_pickZoomButton->setCheckable(true);
  m_focusNearButton = new QPushButton(tr("<"), this);
  m_focusNearButton->setFixedSize(32, 28);
  m_focusFarButton = new QPushButton(tr(">"), this);
  m_focusFarButton->setFixedSize(32, 28);
  m_focusNear2Button = new QPushButton(tr("<<"), this);
  m_focusNear2Button->setFixedSize(32, 28);
  m_focusFar2Button = new QPushButton(tr(">>"), this);
  m_focusFar2Button->setFixedSize(32, 28);
  m_focusNear3Button = new QPushButton(tr("<<<"), this);
  m_focusNear3Button->setFixedSize(32, 28);
  m_focusFar3Button = new QPushButton(tr(">>>"), this);
  m_focusFar3Button->setFixedSize(32, 28);
  //*****//****

  QVBoxLayout *controlLayout = new QVBoxLayout();
  controlLayout->setSpacing(0);
  controlLayout->setMargin(5);

  {
    {
      QGridLayout *camLay = new QGridLayout();
      camLay->setMargin(0);
      camLay->setSpacing(3);
      {
        camLay->addWidget(new QLabel(tr("Camera:"), this), 0, 0,
                          Qt::AlignRight);
        camLay->addWidget(m_cameraListCombo, 0, 1, Qt::AlignLeft);
        camLay->addWidget(refreshCamListButton, 0, 2, Qt::AlignLeft);
        // if (m_captureFilterSettingsBtn) {
        //  camLay->addWidget(m_captureFilterSettingsBtn, 0, 3, Qt::AlignLeft);
        //  camLay->addWidget(m_resolutionLabel, 1, 0, Qt::AlignRight);
        //  camLay->addWidget(m_resolutionCombo, 1, 1, 1, 3, Qt::AlignLeft);
        //  camLay->setColumnStretch(3, 30);
        //} else {
        //}
        camLay->addWidget(m_resolutionLabel, 1, 0, Qt::AlignRight);
        camLay->addWidget(m_resolutionCombo, 1, 1, 1, 2, Qt::AlignLeft);
        camLay->setColumnStretch(2, 30);
        camLay->addWidget(m_cameraStatusLabel, 2, 1, 1, 2, Qt::AlignLeft);
      }
      controlLayout->addLayout(camLay, 0);

      QVBoxLayout *fileLay = new QVBoxLayout();
      fileLay->setMargin(8);
      fileLay->setSpacing(5);
      {
        QGridLayout *levelLay = new QGridLayout();
        levelLay->setMargin(0);
        levelLay->setHorizontalSpacing(3);
        levelLay->setVerticalSpacing(5);
        {
          levelLay->addWidget(new QLabel(tr("Name:"), this), 0, 0,
                              Qt::AlignRight);
          QHBoxLayout *nameLay = new QHBoxLayout();
          nameLay->setMargin(0);
          nameLay->setSpacing(2);
          {
            nameLay->addWidget(m_previousLevelButton, 0);
            nameLay->addWidget(m_levelNameEdit, 1);
            nameLay->addWidget(nextLevelButton, 0);
            nameLay->addWidget(nextOpenLevelButton, 0);
          }
          levelLay->addLayout(nameLay, 0, 1);

          levelLay->addWidget(new QLabel(tr("Frame:"), this), 1, 0,
                              Qt::AlignRight);

          QHBoxLayout *frameLay = new QHBoxLayout();
          frameLay->setMargin(0);
          frameLay->setSpacing(2);
          {
            frameLay->addWidget(m_previousFrameButton, 0);
            frameLay->addWidget(m_frameNumberEdit, 1);
            frameLay->addWidget(nextFrameButton, 0);
            frameLay->addWidget(lastFrameButton, 0);
            frameLay->addWidget(m_frameInfoLabel, 1, Qt::AlignVCenter);
          }
          levelLay->addLayout(frameLay, 1, 1);
        }
        levelLay->setColumnStretch(0, 0);
        levelLay->setColumnStretch(1, 1);
        fileLay->addLayout(levelLay, 0);

        QHBoxLayout *fileTypeLay = new QHBoxLayout();
        fileTypeLay->setMargin(0);
        fileTypeLay->setSpacing(3);
        {
          // fileTypeLay->addWidget(new QLabel(tr("File Type:"), this), 0);
          fileTypeLay->addWidget(m_fileTypeCombo, 1);
          // fileTypeLay->addSpacing(10);
          fileTypeLay->addWidget(m_fileFormatOptionButton);
          m_fileTypeCombo->hide();
          m_fileFormatOptionButton->hide();
        }
        fileLay->addLayout(fileTypeLay, 0);

        QHBoxLayout *saveInLay = new QHBoxLayout();
        saveInLay->setMargin(0);
        saveInLay->setSpacing(3);
        {
          // saveInLay->addWidget(new QLabel(tr("Save In:"), this), 0);
          saveInLay->addWidget(m_saveInFileFld, 1);
          m_saveInFileFld->hide();
        }
        fileLay->addLayout(saveInLay, 0);
        // fileLay->addWidget(subfolderButton, 0);
      }
      fileFrame->setLayout(fileLay);
      controlLayout->addWidget(fileFrame, 0);

      QGridLayout *displayLay = new QGridLayout();
      displayLay->setMargin(8);
      displayLay->setHorizontalSpacing(3);
      displayLay->setVerticalSpacing(5);
      {
        displayLay->addWidget(new QLabel(tr("Expose as: ")), 0, 0,
                              Qt::AlignRight);
        displayLay->addWidget(m_captureFramesCombo, 0, 1, Qt::AlignLeft);
        displayLay->addWidget(new QLabel(tr("Scene Frame:"), this), 1, 0,
                              Qt::AlignRight);
        QHBoxLayout *xsheetLay = new QHBoxLayout();
        xsheetLay->setMargin(0);
        xsheetLay->setSpacing(2);
        {
          xsheetLay->addWidget(m_previousXSheetFrameButton, Qt::AlignLeft);
          xsheetLay->addWidget(m_xSheetFrameNumberEdit, Qt::AlignLeft);
          xsheetLay->addWidget(nextXSheetFrameButton, Qt::AlignLeft);
          xsheetLay->addWidget(m_setToCurrentXSheetFrameButton,
                               Qt::AlignCenter);
          xsheetLay->addStretch(50);
        }
        displayLay->addLayout(xsheetLay, 1, 1);
      }
      displayLay->setColumnStretch(0, 0);
      displayLay->setColumnStretch(1, 1);
      controlLayout->addLayout(displayLay, 0);
      controlLayout->addStretch(1);
      controlLayout->addSpacing(5);
      controlLayout->addStretch(1);
    }

    m_mainControlsPage->setLayout(controlLayout);

    // Make Settings Page
    QVBoxLayout *innerSettingsLayout = new QVBoxLayout;
    m_noCameraFrame                  = new QFrame();
    QHBoxLayout *noCameraLayout      = new QHBoxLayout();
    noCameraLayout->addStretch();
    noCameraLayout->addWidget(
        new QLabel(tr("Select a camera to change settings.")));
    noCameraLayout->addStretch();
    m_noCameraFrame->setLayout(noCameraLayout);
    innerSettingsLayout->addWidget(m_noCameraFrame);

    m_apertureLabel  = new QLabel(tr(""), this);
    m_apertureSlider = new QSlider(Qt::Horizontal, this);
    m_apertureSlider->setRange(0, 10);
    m_apertureSlider->setTickInterval(1);
    m_apertureSlider->setFixedWidth(260);
    m_isoLabel  = new QLabel(tr(""), this);
    m_isoSlider = new QSlider(Qt::Horizontal, this);
    m_isoSlider->setRange(0, 10);
    m_isoSlider->setTickInterval(1);
    m_isoSlider->setFixedWidth(260);
    m_shutterSpeedLabel  = new QLabel(tr(""), this);
    m_shutterSpeedSlider = new QSlider(Qt::Horizontal, this);
    m_shutterSpeedSlider->setRange(0, 10);
    m_shutterSpeedSlider->setTickInterval(1);
    m_shutterSpeedSlider->setFixedWidth(260);
    m_kelvinValueLabel = new QLabel(tr("Temperature: "), this);
    m_kelvinSlider     = new QSlider(Qt::Horizontal, this);
    m_kelvinSlider->setRange(0, 10);
    m_kelvinSlider->setTickInterval(1);
    m_kelvinSlider->setFixedWidth(260);
    m_exposureCombo       = new QComboBox(this);
    m_whiteBalanceCombo   = new QComboBox(this);
    m_imageQualityCombo   = new QComboBox(this);
    m_pictureStyleCombo   = new QComboBox(this);
    m_cameraSettingsLabel = new QLabel(tr("Camera Model"), this);
    m_cameraModeLabel     = new QLabel(tr("Camera Mode"), this);
    m_exposureCombo->setFixedWidth(fontMetrics().width("000000") + 25);
    m_liveViewCompensationLabel  = new QLabel(tr("Live View Offset: 0"), this);
    m_liveViewCompensationSlider = new QSlider(Qt::Horizontal, this);
    m_liveViewCompensationSlider->setRange(-15, 12);
    m_liveViewCompensationSlider->setTickInterval(1);
    m_liveViewCompensationSlider->setFixedWidth(260);
    QVBoxLayout *settingsLayout = new QVBoxLayout;
    settingsLayout->setSpacing(0);
    settingsLayout->setMargin(5);

    QGridLayout *settingsGridLayout = new QGridLayout;
    {
      settingsGridLayout->setMargin(0);
      settingsGridLayout->setSpacing(3);
      settingsGridLayout->addWidget(m_cameraSettingsLabel, 0, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_cameraModeLabel, 1, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 2, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_shutterSpeedLabel, 3, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_shutterSpeedSlider, 4, 0, 1, 2,
                                    Qt::AlignCenter);

      settingsGridLayout->addWidget(m_apertureLabel, 5, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_apertureSlider, 6, 0, 1, 2,
                                    Qt::AlignCenter);

      settingsGridLayout->addWidget(m_isoLabel, 7, 0, 1, 2, Qt::AlignCenter);
      settingsGridLayout->addWidget(m_isoSlider, 8, 0, 1, 2, Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 9, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(tr("White Balance: ")), 10, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_whiteBalanceCombo, 10, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(m_kelvinValueLabel, 11, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_kelvinSlider, 12, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 13, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(tr("Picture Style: ")), 14, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_pictureStyleCombo, 14, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Image Quality: ")), 15, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_imageQualityCombo, 15, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(tr("Exposure: ")), 16, 0,
                                    Qt::AlignRight);
      settingsGridLayout->addWidget(m_exposureCombo, 16, 1, Qt::AlignLeft);
      settingsGridLayout->addWidget(new QLabel(" ", this), 17, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_liveViewCompensationLabel, 18, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(m_liveViewCompensationSlider, 19, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 20, 0, 1, 2,
                                    Qt::AlignCenter);
      settingsGridLayout->addWidget(new QLabel(" ", this), 21, 0, 1, 2,
                                    Qt::AlignCenter);

      settingsGridLayout->setColumnStretch(1, 30);
    }
    settingsLayout->addLayout(settingsGridLayout, 0);
    m_focusAndZoomLayout = new QHBoxLayout;
    m_focusAndZoomLayout->addStretch();
    m_focusAndZoomLayout->addWidget(m_focusNear3Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusNear2Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusNearButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_zoomButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_pickZoomButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusFarButton, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusFar2Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addWidget(m_focusFar3Button, Qt::AlignCenter);
    m_focusAndZoomLayout->addStretch();
    // settingsLayout->addStretch();
    settingsLayout->addLayout(m_focusAndZoomLayout);
    m_settingsTakeTestButton = new QPushButton(tr("Test Shot"));
    m_settingsTakeTestButton->setFixedHeight(25);
    settingsLayout->addWidget(m_settingsTakeTestButton);
    settingsLayout->addStretch();
    m_dslrFrame = new QFrame();
    m_dslrFrame->setLayout(settingsLayout);
    innerSettingsLayout->addWidget(m_dslrFrame);
    m_dslrFrame->hide();

    QVBoxLayout *webcamSettingsLayout = new QVBoxLayout;
    webcamSettingsLayout->setSpacing(0);
    webcamSettingsLayout->setMargin(5);
    QHBoxLayout *webcamLabelLayout = new QHBoxLayout();
    QGroupBox *imageFrame          = new QGroupBox(tr("Image adjust"), this);
    m_webcamLabel        = new QLabel("insert webcam name here", this);
    m_colorTypeCombo     = new QComboBox(this);
    m_camCapLevelControl = new CameraCaptureLevelControl(this);
    // m_upsideDownCB = new QCheckBox(tr("Upside down"), this);
    // m_upsideDownCB->setChecked(false);
    imageFrame->setObjectName("CleanupSettingsFrame");
    m_colorTypeCombo->addItems(
        {tr("Color"), tr("Grayscale"), tr("Black & White")});
    m_colorTypeCombo->setCurrentIndex(0);

    webcamLabelLayout->addStretch();
    webcamLabelLayout->addWidget(m_webcamLabel);
    webcamLabelLayout->addStretch();
    webcamSettingsLayout->addLayout(webcamLabelLayout);
    webcamSettingsLayout->addSpacing(10);

    // webcam focus
    m_webcamAutoFocusGB = new QGroupBox(tr("Manual Focus"), this);
    m_webcamAutoFocusGB->setCheckable(true);

    m_webcamFocusSlider = new QSlider(Qt::Horizontal, this);
    m_webcamFocusSlider->setRange(0, 255);
    m_webcamFocusSlider->setTickInterval(5);

    QHBoxLayout *webcamFocusLay = new QHBoxLayout();
    webcamFocusLay->addWidget(new QLabel(tr("Focus: "), this), 0);
    webcamFocusLay->addWidget(m_webcamFocusSlider, 1);
    m_webcamAutoFocusGB->setLayout(webcamFocusLay);
    webcamSettingsLayout->addWidget(m_webcamAutoFocusGB);
    webcamSettingsLayout->addSpacing(5);

    QGridLayout *webcamGridLay = new QGridLayout();
    webcamGridLay->setMargin(0);
    webcamGridLay->setSpacing(3);
    webcamGridLay->setColumnStretch(0, 0);
    webcamGridLay->setColumnStretch(1, 1);

    // webcam exposure
    m_webcamExposureSlider = new QSlider(Qt::Horizontal, this);
    m_webcamExposureSlider->setRange(-13, -1);
    m_webcamExposureSlider->setTickInterval(1);

    QHBoxLayout *webcamExposureLay = new QHBoxLayout();
    webcamExposureLay->addWidget(m_webcamExposureSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Exposure: "), this), 0, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamExposureLay, 0, 1, 1, 1);

    // webcam brightness
    m_webcamBrightnessSlider = new QSlider(Qt::Horizontal, this);
    m_webcamBrightnessSlider->setRange(0, 255);

    QHBoxLayout *webcamBrightnessLay = new QHBoxLayout();
    webcamBrightnessLay->addWidget(m_webcamBrightnessSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Brightness: "), this), 1, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamBrightnessLay, 1, 1, 1, 1);

    // webcam contrast
    m_webcamContrastSlider = new QSlider(Qt::Horizontal, this);
    m_webcamContrastSlider->setRange(0, 255);

    QHBoxLayout *webcamContrastLay = new QHBoxLayout();
    webcamContrastLay->addWidget(m_webcamContrastSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Contrast: "), this), 2, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamContrastLay, 2, 1, 1, 1);

    // webcam gain
    m_webcamGainSlider = new QSlider(Qt::Horizontal, this);
    m_webcamGainSlider->setRange(0, 255);

    QHBoxLayout *webcamGainLay = new QHBoxLayout();
    webcamGainLay->addWidget(m_webcamGainSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Gain: "), this), 3, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamGainLay, 3, 1, 1, 1);

    // webcam saturation
    m_webcamSaturationSlider = new QSlider(Qt::Horizontal, this);
    m_webcamSaturationSlider->setRange(0, 255);

    QHBoxLayout *webcamSaturationLay = new QHBoxLayout();
    webcamSaturationLay->addWidget(m_webcamSaturationSlider, 1);
    webcamGridLay->addWidget(new QLabel(tr("Saturation: "), this), 4, 0, 1, 1,
                             Qt::AlignRight);
    webcamGridLay->addLayout(webcamSaturationLay, 4, 1, 1, 1);

#ifdef _WIN32
    m_captureFilterSettingsBtn = new QPushButton(this);
#else
    m_captureFilterSettingsBtn = 0;
#endif
    if (m_captureFilterSettingsBtn) {
      m_captureFilterSettingsBtn->setObjectName("GearButton");
      m_captureFilterSettingsBtn->setFixedSize(128, 28);
      m_captureFilterSettingsBtn->setText(tr("More"));
      m_captureFilterSettingsBtn->setIconSize(QSize(15, 15));
      m_captureFilterSettingsBtn->setToolTip(tr("Webcam Settings..."));
      webcamGridLay->addWidget(m_captureFilterSettingsBtn, 5, 0, 1, 2,
                               Qt::AlignCenter);
    }

    QGridLayout *imageLay = new QGridLayout();
    imageLay->setMargin(8);
    imageLay->setHorizontalSpacing(3);
    imageLay->setVerticalSpacing(5);
    {
      imageLay->addWidget(new QLabel(tr("Color type:"), this), 0, 0,
                          Qt::AlignRight);
      imageLay->addWidget(m_colorTypeCombo, 0, 1);

      imageLay->addWidget(m_camCapLevelControl, 1, 0, 1, 3);

      // imageLay->addWidget(m_upsideDownCB, 2, 0, 1, 3, Qt::AlignLeft);
    }
    imageLay->setColumnStretch(0, 0);
    imageLay->setColumnStretch(1, 0);
    imageLay->setColumnStretch(2, 1);
    imageFrame->setLayout(imageLay);
    webcamGridLay->addWidget(imageFrame, 6, 0, 1, 2);

    webcamSettingsLayout->addLayout(webcamGridLay);

    webcamSettingsLayout->addStretch();
    m_webcamFrame = new QFrame();
    m_webcamFrame->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Expanding);
    m_webcamFrame->setLayout(webcamSettingsLayout);
    innerSettingsLayout->addWidget(m_webcamFrame);
    m_webcamFrame->hide();

    // Calibration
    m_calibrationUI.groupBox  = new QGroupBox(tr("Calibration"), this);
    m_calibrationUI.capBtn    = new QPushButton(tr("Capture"), this);
    m_calibrationUI.cancelBtn = new QPushButton(tr("Cancel"), this);
    m_calibrationUI.newBtn    = new QPushButton(tr("Start calibration"), this);
    m_calibrationUI.loadBtn   = new QPushButton(tr("Load"), this);
    m_calibrationUI.exportBtn = new QPushButton(tr("Export"), this);
    m_calibrationUI.label     = new QLabel(this);
    m_calibrationUI.groupBox->setCheckable(true);
    m_calibrationUI.groupBox->setChecked(CamCapDoCalibration);
    QAction *calibrationHelp =
        new QAction(tr("Open Readme.txt for Camera calibration..."));
    m_calibrationUI.groupBox->addAction(calibrationHelp);
    m_calibrationUI.groupBox->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_calibrationUI.groupBox->setToolTip(
        tr("Use Camera Calibration.\nRight-click for more information."));
    m_calibrationUI.capBtn->hide();
    m_calibrationUI.cancelBtn->hide();
    m_calibrationUI.label->hide();
    m_calibrationUI.exportBtn->setEnabled(false);
    connect(calibrationHelp, SIGNAL(triggered()), this, SLOT(onCalibReadme()));

    // Calibration
    QGridLayout *calibLay = new QGridLayout();
    calibLay->setMargin(8);
    calibLay->setHorizontalSpacing(3);
    calibLay->setVerticalSpacing(5);
    {
      calibLay->addWidget(m_calibrationUI.newBtn, 0, 0);
      calibLay->addWidget(m_calibrationUI.loadBtn, 0, 1);
      calibLay->addWidget(m_calibrationUI.exportBtn, 0, 2);
      QHBoxLayout *lay = new QHBoxLayout();
      lay->setMargin(0);
      lay->setSpacing(5);
      lay->addWidget(m_calibrationUI.capBtn, 1);
      lay->addWidget(m_calibrationUI.label, 0);
      lay->addWidget(m_calibrationUI.cancelBtn, 1);
      calibLay->addLayout(lay, 1, 0, 1, 3);
    }
    calibLay->setColumnStretch(0, 1);
    m_calibrationUI.groupBox->setLayout(calibLay);

    QVBoxLayout *commonSettingsLayout = new QVBoxLayout;
    commonSettingsLayout->setSpacing(0);
    commonSettingsLayout->setMargin(5);
    commonSettingsLayout->addWidget(m_calibrationUI.groupBox);
    commonSettingsLayout->addStretch();
    m_commonFrame = new QFrame();
    m_commonFrame->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Expanding);
    m_commonFrame->setLayout(commonSettingsLayout);
    innerSettingsLayout->addWidget(m_commonFrame);
    innerSettingsLayout->addStretch();

    m_cameraSettingsPage->setLayout(innerSettingsLayout);

    // Make Options Page
    QGroupBox *webcamBox  = new QGroupBox(tr("Webcam Options"), this);
    QGroupBox *dslrBox    = new QGroupBox(tr("DSLR Options"), this);
    m_timerCB             = new QGroupBox(tr("Use Time Lapse"), this);
    m_timerIntervalFld    = new DVGui::DoubleField(this, true, 1);
    m_timerCB->setCheckable(true);
    m_timerCB->setObjectName("CleanupSettingsFrame");
    m_timerCB->setChecked(false);
    m_timerIntervalFld->setRange(0.0, 60.0);
    m_timerIntervalFld->setValue(0.0);

    m_postCaptureReviewFld = new DVGui::DoubleField(this, true, 1);
    m_postCaptureReviewFld->setRange(0.0, 10.0);
    m_postCaptureReviewFld->setValue(0.0);

    m_subsamplingFld = new DVGui::IntField(this);
    m_subsamplingFld->setRange(1, 30);
    m_subsamplingFld->setDisabled(true);

    m_placeOnXSheetCB = new QCheckBox(tr("Place on XSheet"), this);
    m_placeOnXSheetCB->setToolTip(tr("Place the frame in the Scene"));

    m_directShowCB = new QCheckBox(tr("Use Direct Show Webcam Drivers"), this);
    m_useMjpgCB    = new QCheckBox(tr("Use MJPG with Webcam"), this);
    m_useNumpadCB = new QCheckBox(tr("Use Numpad Shortcuts When Active"), this);
    m_useNumpadCB->setToolTip(
        tr("Requires restarting camera when toggled\n"
           "NP 1 = Previous Frame\n"
           "NP 2 = Next Frame\n"
           "NP 3 = Jump To Camera\n"
           "NP 5 = Toggle Live View\n"
           "NP 6 = Short Play\n"
           "NP 8 = Loop\n"
           "NP 0 = Play\n"
           "Period = Use Live View Images\n"
           "Plus = Raise Opacity\n"
           "Minus = Lower Opacity\n"
           "Enter = Capture\n"
           "BackSpace = Remove Frame\n"
           "Multiply = Toggle Zoom\n"
           "Divide = Focus Check"));
    m_drawBeneathCB = new QCheckBox(tr("Show Camera Below Other Levels"), this);
    m_liveViewOnAllFramesCB =
        new QCheckBox(tr("Show Live View on All Frames"), this);
    m_playSound = new QCheckBox(tr("Play Sound on Capture"), this);
    m_playSound->setToolTip(tr("Make a click sound on each capture"));

    QVBoxLayout *optionsOutsideLayout = new QVBoxLayout;
    QGridLayout *optionsLayout        = new QGridLayout;
    optionsLayout->setSpacing(3);
    optionsLayout->setMargin(5);
    QGridLayout *webcamLayout   = new QGridLayout;
    QGridLayout *dslrLayout     = new QGridLayout;
    QGridLayout *checkboxLayout = new QGridLayout;

    dslrLayout->setColumnStretch(1, 30);
    dslrBox->setLayout(dslrLayout);
    dslrBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    optionsOutsideLayout->addWidget(dslrBox, Qt::AlignCenter);
    dslrBox->hide();

    webcamLayout->addWidget(m_directShowCB, 0, 0, 1, 2);
    webcamLayout->addWidget(m_useMjpgCB, 1, 0, 1, 2);
    webcamLayout->setColumnStretch(1, 30);
    webcamBox->setLayout(webcamLayout);
    webcamBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    optionsOutsideLayout->addWidget(webcamBox, Qt::AlignCenter);
    webcamBox->hide();

    QGridLayout *timerLay = new QGridLayout();
    timerLay->setMargin(8);
    timerLay->setHorizontalSpacing(3);
    timerLay->setVerticalSpacing(5);
    {
      timerLay->addWidget(new QLabel(tr("Interval(sec):"), this), 1, 0,
                          Qt::AlignRight);
      timerLay->addWidget(m_timerIntervalFld, 1, 1);
    }
    timerLay->setColumnStretch(0, 0);
    timerLay->setColumnStretch(1, 1);
    m_timerCB->setLayout(timerLay);
    optionsOutsideLayout->addWidget(m_timerCB);

    checkboxLayout->addWidget(m_placeOnXSheetCB, 0, 0, 1, 2);
    m_placeOnXSheetCB->hide();
    checkboxLayout->addWidget(m_drawBeneathCB, 1, 0, 1, 2);
    m_drawBeneathCB->hide();

    checkboxLayout->addWidget(m_useNumpadCB, 2, 0, 1, 2);
    checkboxLayout->addWidget(m_liveViewOnAllFramesCB, 3, 0, 1, 2);
    m_liveViewOnAllFramesCB->hide();
    checkboxLayout->addWidget(m_playSound, 4, 0, 1, 2);

    checkboxLayout->setColumnStretch(1, 30);
    optionsOutsideLayout->addLayout(checkboxLayout, Qt::AlignLeft);

    optionsLayout->addWidget(new QLabel(tr("Capture Review Time: ")), 0, 0,
                             Qt::AlignRight);
    optionsLayout->addWidget(m_postCaptureReviewFld, 0, 1);
    // optionsLayout->addWidget(new QLabel(tr("Level Subsampling: ")), 1, 0,
    //                         Qt::AlignRight);
    optionsLayout->addWidget(m_subsamplingFld, 1, 1);
    m_subsamplingFld->hide();
    optionsLayout->setColumnStretch(1, 30);
    optionsLayout->setRowStretch(2, 30);
    optionsOutsideLayout->addLayout(optionsLayout, Qt::AlignLeft);
    optionsOutsideLayout->addStretch();

    m_optionsPage->setLayout(optionsOutsideLayout);

    m_blackScreenForCapture = new QCheckBox(tr("Blackout all Screens"), this);
    QVBoxLayout *lightOutsideLayout = new QVBoxLayout;
    m_testLightsButton              = new QPushButton(tr("Test"), this);
    m_testLightsButton->setMaximumWidth(150);
    m_testLightsButton->setFixedHeight(28);
    m_testLightsButton->setSizePolicy(QSizePolicy::Maximum,
                                      QSizePolicy::Maximum);
    m_testLightsButton->setStyleSheet("padding: 2px;");
    m_lightTestTimer = new QTimer(this);
    m_lightTestTimer->setSingleShot(true);
    m_screen1ColorFld = new DVGui::ColorField(
        this, false, TPixel32(0, 0, 0, 255), 40, true, 60);

    m_screen2ColorFld = new DVGui::ColorField(
        this, false, TPixel32(0, 0, 0, 255), 40, true, 60);

    m_screen3ColorFld = new DVGui::ColorField(
        this, false, TPixel32(0, 0, 0, 255), 40, true, 60);

    m_showScene1 = new QCheckBox(tr("Use current frame as overlay."), this);
    m_showScene2 = new QCheckBox(tr("Use current frame as overlay."), this);
    m_showScene3 = new QCheckBox(tr("Use current frame as overlay."), this);
    m_showScene1->setToolTip(tr("Use the current scene frame as an overlay."));
    m_showScene2->setToolTip(tr("Use the current scene frame as an overlay."));
    m_showScene3->setToolTip(tr("Use the current scene frame as an overlay."));

    QGridLayout *lightTopLayout = new QGridLayout;
    lightTopLayout->addWidget(m_blackScreenForCapture, 0, 0, Qt::AlignRight);
    lightTopLayout->setColumnStretch(1, 30);
    lightOutsideLayout->addLayout(lightTopLayout);

    m_screen1Box = new QGroupBox(tr("Screen 1"), this);
    m_screen1Box->setCheckable(true);
    m_screen1Box->setChecked(false);
    QGridLayout *screen1Layout = new QGridLayout;
    screen1Layout->addWidget(m_showScene1, 0, 0, 1, 2, Qt::AlignLeft);
    screen1Layout->addWidget(m_screen1ColorFld, 1, 0, 1, 2, Qt::AlignLeft);
    screen1Layout->setColumnStretch(1, 30);
    m_screen1Box->setLayout(screen1Layout);
    m_screen1Box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    lightOutsideLayout->addWidget(m_screen1Box, Qt::AlignCenter);

    m_screen2Box = new QGroupBox(tr("Screen 2"), this);
    m_screen2Box->setCheckable(true);
    m_screen2Box->setChecked(false);
    QGridLayout *screen2Layout = new QGridLayout;
    screen2Layout->addWidget(m_showScene2, 0, 0, 1, 2, Qt::AlignLeft);
    screen2Layout->addWidget(m_screen2ColorFld, 1, 0, 1, 2, Qt::AlignLeft);
    screen2Layout->setColumnStretch(1, 30);
    m_screen2Box->setLayout(screen2Layout);
    m_screen2Box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    lightOutsideLayout->addWidget(m_screen2Box, Qt::AlignCenter);

    m_screen3Box = new QGroupBox(tr("Screen 3"), this);
    m_screen3Box->setCheckable(true);
    m_screen3Box->setChecked(false);
    QGridLayout *screen3Layout = new QGridLayout;
    screen3Layout->addWidget(m_showScene3, 0, 0, 1, 2, Qt::AlignLeft);
    screen3Layout->addWidget(m_screen3ColorFld, 1, 0, 1, 2, Qt::AlignLeft);
    screen3Layout->setColumnStretch(1, 30);
    m_screen3Box->setLayout(screen3Layout);
    m_screen3Box->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    lightOutsideLayout->addWidget(m_screen3Box, Qt::AlignCenter);

    QHBoxLayout *testLayout = new QHBoxLayout;
    testLayout->addWidget(m_testLightsButton, Qt::AlignHCenter);
    lightOutsideLayout->addLayout(testLayout);
    lightOutsideLayout->addStretch();
    m_lightPage->setLayout(lightOutsideLayout);

    if (m_stopMotion->m_light->m_screenCount < 3) m_screen3Box->hide();
    if (m_stopMotion->m_light->m_screenCount < 2) m_screen2Box->hide();

    QVBoxLayout *motionOutsideLayout = new QVBoxLayout;
    // QGridLayout* motionInsideLayout = new QGridLayout;
    m_controlDeviceCombo = new QComboBox(this);
    m_controlDeviceCombo->addItems(
        m_stopMotion->m_serial->getAvailableSerialPorts());

    QGroupBox *motionBox      = new QGroupBox(tr("Motion Control"), this);
    QGridLayout *motionLayout = new QGridLayout;
    motionLayout->addWidget(new QLabel(tr("Port: ")), 0, 0, Qt::AlignRight);
    motionLayout->addWidget(m_controlDeviceCombo, 0, 1, Qt::AlignLeft);
    motionLayout->setColumnStretch(1, 30);
    motionBox->setLayout(motionLayout);
    motionBox->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);
    motionOutsideLayout->addWidget(motionBox, Qt::AlignCenter);
    motionOutsideLayout->addStretch();

    // motionOutsideLayout->addLayout(motionInsideLayout);
    m_motionPage->setLayout(motionOutsideLayout);

    m_testsOutsideLayout = new QVBoxLayout;
    m_testsOutsideLayout->setMargin(0);
    m_testsOutsideLayout->setSpacing(0);
    m_testsInsideLayout = new QVBoxLayout;
    m_testsInsideLayout->setMargin(0);
    m_testsInsideLayout->setSpacing(5);
    QVBoxLayout *testsButtonLayout = new QVBoxLayout;
    testsButtonLayout->setContentsMargins(0, 5, 0, 5);
    testsButtonLayout->setSpacing(0);
    QFrame *testShotsFrame = new QFrame();
    testShotsFrame->setContentsMargins(0, 0, 0, 0);
    testShotsFrame->setStyleSheet("padding:0; margin:0;");

    QScrollArea *testShotsArea = new QScrollArea();
    testShotsArea->setContentsMargins(0, 0, 0, 0);
    testShotsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    testShotsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    testShotsArea->setWidgetResizable(true);

    m_takeTestButton = new QPushButton(tr("Test Shot"));
    m_takeTestButton->setFixedHeight(25);
    testsButtonLayout->addWidget(m_takeTestButton);
    m_testsOutsideLayout->addLayout(testsButtonLayout);

    testShotsFrame->setLayout(m_testsInsideLayout);
    testShotsArea->setWidget(testShotsFrame);
    m_testsOutsideLayout->addWidget(testShotsArea, 500);
    m_testsOutsideLayout->addStretch();
    m_testsInsideLayout->addStretch();
    m_testsPage->setLayout(m_testsOutsideLayout);
    m_testsPage->setContentsMargins(0, 0, 0, 0);
    m_testsPage->setStyleSheet("padding:0; margin:0;");

    QVBoxLayout *pathsLayout = new QVBoxLayout(this);
    pathsLayout->setMargin(0);
    pathsLayout->setSpacing(0);
    pathsLayout->addWidget(new MotionPathPanel(this));
    m_pathsPage->setLayout(pathsLayout);
    m_pathsPage->setContentsMargins(0, 0, 0, 0);
    m_pathsPage->setStyleSheet("padding:0; margin:0;");

    QScrollArea *mainArea = makeChooserPageWithoutScrollBar(m_mainControlsPage);
    QScrollArea *settingsArea =
        makeChooserPageWithoutScrollBar(m_cameraSettingsPage);
    QScrollArea *optionsArea = makeChooserPageWithoutScrollBar(m_optionsPage);
    QScrollArea *lightArea   = makeChooserPageWithoutScrollBar(m_lightPage);
    QScrollArea *motionArea  = makeChooserPageWithoutScrollBar(m_motionPage);
    QScrollArea *testsArea   = makeChooserPageWithoutScrollBar(m_testsPage);
    QScrollArea *pathsArea   = makeChooserPageWithoutScrollBar(m_pathsPage);

    mainArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    settingsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    optionsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    lightArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    motionArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    testsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pathsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_stackedChooser = new QStackedWidget(this);
    m_stackedChooser->addWidget(mainArea);
    m_stackedChooser->addWidget(settingsArea);
    m_stackedChooser->addWidget(optionsArea);
    m_stackedChooser->addWidget(lightArea);
    m_stackedChooser->addWidget(testsArea);
    m_stackedChooser->addWidget(pathsArea);
    m_stackedChooser->addWidget(motionArea);
    m_stackedChooser->setFocusPolicy(Qt::NoFocus);

    QFrame *opacityFrame    = new QFrame();
    QHBoxLayout *opacityLay = new QHBoxLayout();
    opacityLay->addWidget(new QLabel(tr("Opacity:"), this), 0);
    opacityLay->addWidget(m_onionOpacityFld, 1);
    opacityFrame->setLayout(opacityLay);
    opacityFrame->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum);

    QFrame *controlButtonFrame    = new QFrame();
    QHBoxLayout *controlButtonLay = new QHBoxLayout();
    controlButtonLay->addWidget(m_captureButton, 0);
    controlButtonLay->addWidget(m_toggleLiveViewButton, 0);
    controlButtonLay->addWidget(m_alwaysUseLiveViewImagesButton, 0);
    controlButtonFrame->setLayout(controlButtonLay);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);
    {
      QHBoxLayout *hLayout = new QHBoxLayout;
      hLayout->setMargin(0);
      {
        hLayout->addSpacing(4);
        hLayout->addWidget(m_tabBar);
        hLayout->addStretch();
      }
      m_tabBarContainer->setLayout(hLayout);

      mainLayout->addWidget(m_tabBarContainer, 0, 0);
      mainLayout->addWidget(m_stackedChooser, 1, 0);
      mainLayout->addWidget(opacityFrame, 0);
      mainLayout->addWidget(controlButtonFrame, 0);
      setLayout(mainLayout);
      m_tabBarContainer->layout()->update();
    }
  }

  TSceneHandle *sceneHandle   = TApp::instance()->getCurrentScene();
  TXsheetHandle *xsheetHandle = TApp::instance()->getCurrentXsheet();

  bool ret = true;

  // Outside Connections
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));
  ret = ret &&
        connect(xsheetHandle, SIGNAL(xsheetSwitched()), this, SLOT(update()));

  // UI SIGNALS
  ret = ret && connect(m_tabBar, SIGNAL(currentChanged(int)), this,
                       SLOT(setPage(int)));

  // Control Page
  ret = ret && connect(refreshCamListButton, SIGNAL(clicked()), this,
                       SLOT(refreshCameraListCalled()));
  ret = ret && connect(m_cameraListCombo, SIGNAL(activated(int)), this,
                       SLOT(onCameraListComboActivated(int)));
  ret = ret && connect(m_resolutionCombo, SIGNAL(activated(const QString &)),
                       this, SLOT(onResolutionComboActivated(const QString &)));
  if (m_captureFilterSettingsBtn)
    ret = ret && connect(m_captureFilterSettingsBtn, SIGNAL(clicked()), this,
                         SLOT(onCaptureFilterSettingsBtnPressed()));
  ret = ret && connect(m_fileFormatOptionButton, SIGNAL(clicked()), this,
                       SLOT(onFileFormatOptionButtonPressed()));
  ret = ret && connect(m_levelNameEdit, SIGNAL(levelNameEdited()), this,
                       SLOT(onLevelNameEdited()));
  ret = ret &&
        connect(nextLevelButton, SIGNAL(clicked()), this, SLOT(onNextName()));
  ret = ret && connect(m_previousLevelButton, SIGNAL(clicked()), this,
                       SLOT(onPreviousName()));
  ret = ret && connect(nextOpenLevelButton, SIGNAL(clicked()), this,
                       SLOT(onNextNewLevel()));
  ret = ret &&
        connect(nextFrameButton, SIGNAL(clicked()), this, SLOT(onNextFrame()));
  ret = ret &&
        connect(lastFrameButton, SIGNAL(clicked()), this, SLOT(onLastFrame()));
  ret = ret && connect(m_previousFrameButton, SIGNAL(clicked()), this,
                       SLOT(onPreviousFrame()));
  ret = ret && connect(nextXSheetFrameButton, SIGNAL(clicked()), this,
                       SLOT(onNextXSheetFrame()));
  ret = ret && connect(m_previousXSheetFrameButton, SIGNAL(clicked()), this,
                       SLOT(onPreviousXSheetFrame()));
  ret = ret && connect(m_setToCurrentXSheetFrameButton, SIGNAL(clicked()), this,
                       SLOT(setToCurrentXSheetFrame()));
  ret = ret && connect(m_captureFramesCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onCaptureFramesChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(captureNumberOfFramesChanged(int)),
                       this, SLOT(onCaptureNumberOfFramesChanged(int)));
  ret = ret && connect(m_onionOpacityFld, SIGNAL(valueEditedByHand()), this,
                       SLOT(onOnionOpacityFldEdited()));
  ret = ret && connect(m_onionOpacityFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onOnionOpacitySliderChanged(bool)));
  ret = ret && connect(m_captureButton, SIGNAL(clicked(bool)), this,
                       SLOT(onCaptureButtonClicked(bool)));
  // ret = ret && connect(subfolderButton, SIGNAL(clicked(bool)), this,
  //                     SLOT(openSaveInFolderPopup()));
  ret = ret && connect(m_saveInFileFld, SIGNAL(pathChanged()), this,
                       SLOT(onSaveInPathEdited()));
  ret = ret && connect(m_fileTypeCombo, SIGNAL(activated(int)), this,
                       SLOT(onFileTypeActivated()));
  ret = ret && connect(m_frameNumberEdit, SIGNAL(editingFinished()), this,
                       SLOT(onFrameNumberChanged()));
  ret = ret && connect(m_xSheetFrameNumberEdit, SIGNAL(editingFinished()), this,
                       SLOT(onXSheetFrameNumberChanged()));
  ret = ret && connect(m_toggleLiveViewButton, SIGNAL(clicked()), this,
                       SLOT(onLiveViewToggleClicked()));
  ret = ret && connect(m_alwaysUseLiveViewImagesButton, SIGNAL(clicked()), this,
                       SLOT(onAlwaysUseLiveViewImagesButtonClicked()));
  ret =
      ret && connect(m_stopMotion, SIGNAL(alwaysUseLiveViewImagesToggled(bool)),
                     this, SLOT(onAlwaysUseLiveViewImagesToggled(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(filePathChanged(QString)), this,
                       SLOT(onFilePathChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(levelNameChanged(QString)), this,
                       SLOT(onLevelNameChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(fileTypeChanged(QString)), this,
                       SLOT(onFileTypeChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(frameInfoTextChanged(QString)),
                       this, SLOT(onFrameInfoTextChanged(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(xSheetFrameNumberChanged(int)),
                       this, SLOT(onXSheetFrameNumberChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(frameNumberChanged(int)), this,
                       SLOT(onFrameNumberChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(opacityChanged(int)), this,
                       SLOT(onOpacityChanged(int)));

  // Options Page
  ret = ret && connect(m_liveViewOnAllFramesCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onLiveViewOnAllFramesChanged(int)));
  ret = ret && connect(m_playSound, SIGNAL(toggled(bool)), this,
                       SLOT(onPlaySoundToggled(bool)));
  ret = ret && connect(m_placeOnXSheetCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onPlaceOnXSheetChanged(int)));
  ret = ret && connect(m_directShowCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseDirectShowChanged(int)));
  ret = ret && connect(m_useMjpgCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseMjpgChanged(int)));
  ret = ret && connect(m_useNumpadCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onUseNumpadChanged(int)));
  ret = ret && connect(m_drawBeneathCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onDrawBeneathChanged(int)));
  ret = ret && connect(m_postCaptureReviewFld, SIGNAL(valueEditedByHand()),
                       this, SLOT(onCaptureReviewFldEdited()));
  ret = ret && connect(m_postCaptureReviewFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onCaptureReviewSliderChanged(bool)));
  ret = ret && connect(m_subsamplingFld, SIGNAL(valueEditedByHand()), this,
                       SLOT(onSubsamplingFldEdited()));
  ret = ret && connect(m_subsamplingFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onSubsamplingSliderChanged(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(subsamplingChanged(int)), this,
                       SLOT(onSubsamplingChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(liveViewOnAllFramesSignal(bool)),
                       this, SLOT(onLiveViewOnAllFramesSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(placeOnXSheetSignal(bool)), this,
                       SLOT(onPlaceOnXSheetSignal(bool)));
  ret =
      ret && connect(m_stopMotion->m_webcam, SIGNAL(useDirectShowSignal(bool)),
                     this, SLOT(onUseDirectShowSignal(bool)));
  ret = ret && connect(m_stopMotion->m_webcam, SIGNAL(useMjpgSignal(bool)),
                       this, SLOT(onUseMjpgSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(useNumpadSignal(bool)), this,
                       SLOT(onUseNumpadSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(drawBeneathLevelsSignal(bool)),
                       this, SLOT(onDrawBeneathSignal(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(reviewTimeChangedSignal(int)), this,
                       SLOT(onReviewTimeChangedSignal(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(playCaptureSignal(bool)), this,
                       SLOT(onPlayCaptureSignal(bool)));

  // From Stop Motion Main
  ret = ret && connect(m_stopMotion, SIGNAL(newDimensions()), this,
                       SLOT(updateDimensions()));
  ret = ret && connect(m_stopMotion, SIGNAL(updateCameraList(QString)), this,
                       SLOT(refreshCameraList(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(liveViewChanged(bool)), this,
                       SLOT(onLiveViewChanged(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(newCameraSelected(int, bool)), this,
                       SLOT(onNewCameraSelected(int, bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(cameraChanged(QString)), this,
                       SLOT(refreshCameraList(QString)));
  ret = ret && connect(m_stopMotion, SIGNAL(optionsChanged()), this,
                       SLOT(refreshOptionsLists()));
  ret = ret && connect(m_stopMotion, SIGNAL(changeCameraIndex(int)), this,
                       SLOT(onCameraIndexChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(updateStopMotionControls(bool)),
                       this, SLOT(onUpdateStopMotionControls(bool)));

  // EOS Connections
  ret = ret &&
        connect(m_zoomButton, SIGNAL(clicked()), this, SLOT(onZoomPressed()));
  ret = ret && connect(m_pickZoomButton, SIGNAL(clicked()), this,
                       SLOT(onPickZoomPressed()));
  ret = ret && connect(m_focusNearButton, SIGNAL(clicked()), this,
                       SLOT(onFocusNear()));
  ret = ret &&
        connect(m_focusFarButton, SIGNAL(clicked()), this, SLOT(onFocusFar()));
  ret = ret && connect(m_focusNear2Button, SIGNAL(clicked()), this,
                       SLOT(onFocusNear2()));
  ret = ret && connect(m_focusFar2Button, SIGNAL(clicked()), this,
                       SLOT(onFocusFar2()));
  ret = ret && connect(m_focusNear3Button, SIGNAL(clicked()), this,
                       SLOT(onFocusNear3()));
  ret = ret && connect(m_focusFar3Button, SIGNAL(clicked()), this,
                       SLOT(onFocusFar3()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(apertureChangedSignal(QString)),
                this, SLOT(onApertureChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(isoChangedSignal(QString)),
                       this, SLOT(onIsoChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(shutterSpeedChangedSignal(QString)), this,
                       SLOT(onShutterSpeedChangedSignal(QString)));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(exposureChangedSignal(QString)),
                this, SLOT(onExposureChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(whiteBalanceChangedSignal(QString)), this,
                       SLOT(onWhiteBalanceChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(imageQualityChangedSignal(QString)), this,
                       SLOT(onImageQualityChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(pictureStyleChangedSignal(QString)), this,
                       SLOT(onPictureStyleChangedSignal(QString)));
  ret = ret && connect(m_stopMotion->m_canon,
                       SIGNAL(colorTemperatureChangedSignal(QString)), this,
                       SLOT(onColorTemperatureChangedSignal(QString)));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(liveViewOffsetChangedSignal(int)),
                this, SLOT(onLiveViewCompensationChangedSignal(int)));
  ret = ret && connect(m_apertureSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onApertureChanged(int)));
  ret = ret && connect(m_shutterSpeedSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onShutterSpeedChanged(int)));
  ret = ret && connect(m_isoSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onIsoChanged(int)));
  ret = ret && connect(m_liveViewCompensationSlider, SIGNAL(valueChanged(int)),
                       this, SLOT(onLiveViewCompensationChanged(int)));
  ret = ret && connect(m_exposureCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onExposureChanged(int)));
  ret = ret && connect(m_whiteBalanceCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onWhiteBalanceChanged(int)));
  ret = ret && connect(m_kelvinSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onColorTemperatureChanged(int)));
  ret = ret && connect(m_imageQualityCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onImageQualityChanged(int)));
  ret = ret && connect(m_pictureStyleCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(onPictureStyleChanged(int)));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(apertureOptionsChanged()),
                       this, SLOT(refreshApertureList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(shutterSpeedOptionsChanged()),
                this, SLOT(refreshShutterSpeedList()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(isoOptionsChanged()), this,
                       SLOT(refreshIsoList()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(exposureOptionsChanged()),
                       this, SLOT(refreshExposureList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(whiteBalanceOptionsChanged()),
                this, SLOT(refreshWhiteBalanceList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(imageQualityOptionsChanged()),
                this, SLOT(refreshImageQualityList()));
  ret = ret &&
        connect(m_stopMotion->m_canon, SIGNAL(pictureStyleOptionsChanged()),
                this, SLOT(refreshPictureStyleList()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(modeChanged()), this,
                       SLOT(refreshMode()));
  ret = ret && connect(m_stopMotion->m_canon, SIGNAL(focusCheckToggled(bool)),
                       this, SLOT(onFocusCheckToggled(bool)));
  ret =
      ret && connect(m_stopMotion->m_canon, SIGNAL(pickFocusCheckToggled(bool)),
                     this, SLOT(onPickFocusCheckToggled(bool)));
  ret = ret && connect(m_settingsTakeTestButton, SIGNAL(clicked()), this,
                       SLOT(onTakeTestButtonClicked()));

  // Webcam Specific Connections
  ret = ret && connect(m_stopMotion, SIGNAL(webcamResolutionsChanged()), this,
                       SLOT(onWebcamResolutionsChanged()));
  ret = ret && connect(m_stopMotion, SIGNAL(newWebcamResolutionSelected(int)),
                       this, SLOT(onNewWebcamResolutionSelected(int)));
  ret = ret && connect(m_webcamFocusSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamFocusSliderChanged(int)));
  ret = ret && connect(m_webcamAutoFocusGB, SIGNAL(toggled(bool)), this,
                       SLOT(onWebcamAutofocusToggled(bool)));
  ret = ret && connect(m_webcamExposureSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamExposureSliderChanged(int)));
  ret = ret && connect(m_webcamBrightnessSlider, SIGNAL(valueChanged(int)),
                       this, SLOT(onWebcamBrightnessSliderChanged(int)));
  ret = ret && connect(m_webcamContrastSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamContrastSliderChanged(int)));
  ret = ret && connect(m_webcamGainSlider, SIGNAL(valueChanged(int)), this,
                       SLOT(onWebcamGainSliderChanged(int)));
  ret = ret && connect(m_webcamSaturationSlider, SIGNAL(valueChanged(int)),
                       this, SLOT(onWebcamSaturationSliderChanged(int)));
  ret = ret && connect(m_colorTypeCombo, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onColorTypeComboChanged(int)));
  ret = ret && connect(m_stopMotion->m_webcam, SIGNAL(updateHistogram(cv::Mat)),
                       this, SLOT(onUpdateHistogramCalled(cv::Mat)));

  // Calibration
  ret = ret && connect(m_calibrationUI.groupBox, &QGroupBox::toggled,
                       [&](bool checked) {
                         CamCapDoCalibration                   = checked;
                         m_stopMotion->m_calibration.isEnabled = checked;
                         resetCalibSettingsFromFile();
                       });
  ret = ret && connect(m_calibrationUI.capBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibCapBtnClicked()));
  ret = ret && connect(m_calibrationUI.newBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibNewBtnClicked()));
  ret = ret && connect(m_calibrationUI.cancelBtn, SIGNAL(clicked()), this,
                       SLOT(resetCalibSettingsFromFile()));
  ret = ret && connect(m_calibrationUI.loadBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibLoadBtnClicked()));
  ret = ret && connect(m_calibrationUI.exportBtn, SIGNAL(clicked()), this,
                       SLOT(onCalibExportBtnClicked()));

  // Lighting Connections
  ret = ret &&
        connect(m_screen1ColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(setScreen1Color(const TPixel32 &, bool)));
  ret = ret &&
        connect(m_screen2ColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(setScreen2Color(const TPixel32 &, bool)));
  ret = ret &&
        connect(m_screen3ColorFld, SIGNAL(colorChanged(const TPixel32 &, bool)),
                this, SLOT(setScreen3Color(const TPixel32 &, bool)));
  ret = ret && connect(m_screen1Box, SIGNAL(toggled(bool)), this,
                       SLOT(onScreen1OverlayToggled(bool)));
  ret = ret && connect(m_screen2Box, SIGNAL(toggled(bool)), this,
                       SLOT(onScreen2OverlayToggled(bool)));
  ret = ret && connect(m_screen3Box, SIGNAL(toggled(bool)), this,
                       SLOT(onScreen3OverlayToggled(bool)));
  ret = ret && connect(m_showScene1, SIGNAL(toggled(bool)), this,
                       SLOT(onShowScene1Toggled(bool)));
  ret = ret && connect(m_showScene2, SIGNAL(toggled(bool)), this,
                       SLOT(onShowScene2Toggled(bool)));
  ret = ret && connect(m_showScene3, SIGNAL(toggled(bool)), this,
                       SLOT(onShowScene3Toggled(bool)));
  ret = ret && connect(m_stopMotion->m_light, SIGNAL(showSceneOn1Changed(bool)),
                       this, SLOT(onShowSceneOn1Changed(bool)));
  ret = ret && connect(m_stopMotion->m_light, SIGNAL(showSceneOn2Changed(bool)),
                       this, SLOT(onShowSceneOn2Changed(bool)));
  ret = ret && connect(m_stopMotion->m_light, SIGNAL(showSceneOn3Changed(bool)),
                       this, SLOT(onShowSceneOn3Changed(bool)));
  ret = ret && connect(m_lightTestTimer, SIGNAL(timeout()), this,
                       SLOT(onTestLightsTimeout()));
  ret = ret && connect(m_testLightsButton, SIGNAL(clicked()), this,
                       SLOT(onTestLightsPressed()));
  ret = ret &&
        connect(m_stopMotion->m_light, SIGNAL(screen1ColorChanged(TPixel32)),
                this, SLOT(onScreen1ColorChanged(TPixel32)));
  ret = ret &&
        connect(m_stopMotion->m_light, SIGNAL(screen2ColorChanged(TPixel32)),
                this, SLOT(onScreen2ColorChanged(TPixel32)));
  ret = ret &&
        connect(m_stopMotion->m_light, SIGNAL(screen3ColorChanged(TPixel32)),
                this, SLOT(onScreen3ColorChanged(TPixel32)));
  ret =
      ret && connect(m_stopMotion->m_light, SIGNAL(screen1OverlayChanged(bool)),
                     this, SLOT(onScreen1OverlayChanged(bool)));
  ret =
      ret && connect(m_stopMotion->m_light, SIGNAL(screen2OverlayChanged(bool)),
                     this, SLOT(onScreen2OverlayChanged(bool)));
  ret =
      ret && connect(m_stopMotion->m_light, SIGNAL(screen3OverlayChanged(bool)),
                     this, SLOT(onScreen3OverlayChanged(bool)));
  ret = ret && connect(m_blackScreenForCapture, SIGNAL(stateChanged(int)), this,
                       SLOT(onBlackScreenForCaptureChanged(int)));
  ret = ret && connect(m_stopMotion->m_light, SIGNAL(blackCaptureSignal(bool)),
                       this, SLOT(onBlackCaptureSignal(bool)));

  // Serial Port Connections
  ret = ret && connect(m_controlDeviceCombo, SIGNAL(currentIndexChanged(int)),
                       this, SLOT(serialPortChanged(int)));

  // Time Lapse
  ret = ret && connect(m_timerCB, SIGNAL(toggled(bool)), this,
                       SLOT(onIntervalTimerCBToggled(bool)));
  ret = ret && connect(m_timerIntervalFld, SIGNAL(valueChanged(bool)), this,
                       SLOT(onIntervalSliderValueChanged(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalAmountChanged(int)), this,
                       SLOT(onIntervalAmountChanged(int)));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalToggled(bool)), this,
                       SLOT(onIntervalToggled(bool)));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalStarted()), this,
                       SLOT(onIntervalStarted()));
  ret = ret && connect(m_stopMotion, SIGNAL(intervalStopped()), this,
                       SLOT(onIntervalStopped()));
  ret = ret && connect(m_stopMotion->m_intervalTimer, SIGNAL(timeout()), this,
                       SLOT(onIntervalCaptureTimerTimeout()));
  ret = ret && connect(m_stopMotion->m_countdownTimer, SIGNAL(timeout()), this,
                       SLOT(onIntervalCountDownTimeout()));

  // Tests
  ret = ret && connect(m_takeTestButton, SIGNAL(clicked()), this,
                       SLOT(onTakeTestButtonClicked()));
  ret = ret && connect(m_stopMotion, SIGNAL(updateTestShots()), this,
                       SLOT(onRefreshTests()));

  // Calibration
  ret = ret && connect(m_stopMotion, SIGNAL(calibrationImageCaptured()), this,
                       SLOT(onCalibImageCaptured()));
  assert(ret);

  m_placeOnXSheetCB->setChecked(
      m_stopMotion->getPlaceOnXSheet() == true ? true : false);

  m_onionOpacityFld->setValue(double(100 * m_stopMotion->getOpacity()) / 255.0);
  m_directShowCB->setChecked(m_stopMotion->m_webcam->getUseDirectShow());
  m_useMjpgCB->setChecked(m_stopMotion->m_webcam->getUseMjpg());
  m_useNumpadCB->setChecked(m_stopMotion->getUseNumpadShortcuts());
  m_drawBeneathCB->setChecked(m_stopMotion->m_drawBeneathLevels);
  m_liveViewOnAllFramesCB->setChecked(m_stopMotion->getAlwaysLiveView());
  m_playSound->setChecked(m_stopMotion->getPlayCaptureSound());
  m_blackScreenForCapture->setChecked(
      m_stopMotion->m_light->getBlackCapture() ? true : false);
  if (m_stopMotion->m_light->getBlackCapture()) {
    m_screen1Box->setDisabled(true);
    m_screen2Box->setDisabled(true);
    m_screen3Box->setDisabled(true);
  }
  m_postCaptureReviewFld->setValue(m_stopMotion->getReviewTimeDSec() / 10.0);
  m_timerIntervalFld->setValue(m_stopMotion->getIntervalDSec() / 10.0);

  refreshCameraList(QString(""));
  onSceneSwitched();
  m_stopMotion->setToNextNewLevel();
  m_saveInFileFld->setPath(m_stopMotion->getFilePath());

  m_stopMotion->m_calibration.isEnabled = m_calibrationUI.groupBox->isChecked();

#ifndef _WIN32
  m_directShowCB->hide();
#endif
}

//-----------------------------------------------------------------------------

StopMotionController::~StopMotionController() {}

//-----------------------------------------------------------------------------

void StopMotionController::setPage(int index) {
  if (index > 0 && m_tabBar->tabText(1) != tr("Settings")) {
    index += 1;
  }
  m_stackedChooser->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewOnAllFramesChanged(int checked) {
  m_stopMotion->setAlwaysLiveView(checked > 0 ? true : false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewOnAllFramesSignal(bool on) {
  m_liveViewOnAllFramesCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onBlackScreenForCaptureChanged(int checked) {
  m_stopMotion->m_light->setBlackCapture(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onBlackCaptureSignal(bool on) {
  m_blackScreenForCapture->blockSignals(true);
  updateLightsEnabled();
  m_blackScreenForCapture->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::setScreen1Color(const TPixel32 &bgColor,
                                           bool isDragging) {
  m_stopMotion->m_light->setScreen1Color(bgColor);
}

//-----------------------------------------------------------------------------

void StopMotionController::setScreen2Color(const TPixel32 &bgColor,
                                           bool isDragging) {
  m_stopMotion->m_light->setScreen2Color(bgColor);
}

//-----------------------------------------------------------------------------

void StopMotionController::setScreen3Color(const TPixel32 &bgColor,
                                           bool isDragging) {
  m_stopMotion->m_light->setScreen3Color(bgColor);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen1OverlayToggled(bool on) {
  m_stopMotion->m_light->setScreen1UseOverlay(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen2OverlayToggled(bool on) {
  m_stopMotion->m_light->setScreen2UseOverlay(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen3OverlayToggled(bool on) {
  m_stopMotion->m_light->setScreen3UseOverlay(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onShowScene1Toggled(bool on) {
  m_stopMotion->m_light->setShowSceneOn1(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onShowScene2Toggled(bool on) {
  m_stopMotion->m_light->setShowSceneOn2(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onShowScene3Toggled(bool on) {
  m_stopMotion->m_light->setShowSceneOn3(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onShowSceneOn1Changed(bool on) {
  m_showScene1->blockSignals(true);
  m_showScene1->setChecked(on);
  m_showScene1->blockSignals(false);
  if (on)
    m_screen1ColorFld->setDisabled(true);
  else
    m_screen1ColorFld->setEnabled(true);
}

//-----------------------------------------------------------------------------

void StopMotionController::onShowSceneOn2Changed(bool on) {
  m_showScene2->blockSignals(true);
  m_showScene2->setChecked(on);
  m_showScene2->blockSignals(false);
  if (on)
    m_screen2ColorFld->setDisabled(true);
  else
    m_screen2ColorFld->setEnabled(true);
}

//-----------------------------------------------------------------------------

void StopMotionController::onShowSceneOn3Changed(bool on) {
  m_showScene3->blockSignals(true);
  m_showScene3->setChecked(on);
  m_showScene3->blockSignals(false);
  if (on)
    m_screen3ColorFld->setDisabled(true);
  else
    m_screen3ColorFld->setEnabled(true);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen1ColorChanged(TPixel32 color) {
  m_screen1ColorFld->blockSignals(true);
  m_screen1ColorFld->setColor(color);
  m_screen1ColorFld->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen2ColorChanged(TPixel32 color) {
  m_screen2ColorFld->blockSignals(true);
  m_screen2ColorFld->setColor(color);
  m_screen2ColorFld->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen3ColorChanged(TPixel32 color) {
  m_screen3ColorFld->blockSignals(true);
  m_screen3ColorFld->setColor(color);
  m_screen3ColorFld->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen1OverlayChanged(bool on) {
  m_screen1Box->blockSignals(true);
  m_screen1Box->setChecked(on);
  updateLightsEnabled();
  m_screen1Box->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen2OverlayChanged(bool on) {
  m_screen2Box->blockSignals(true);
  m_screen2Box->setChecked(on);
  updateLightsEnabled();
  m_screen2Box->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onScreen3OverlayChanged(bool on) {
  m_screen3Box->blockSignals(true);
  m_screen3Box->setChecked(on);
  updateLightsEnabled();
  m_screen3Box->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onTestLightsPressed() {
  m_stopMotion->m_light->showOverlays();
  m_lightTestTimer->start(2000);
}

//-----------------------------------------------------------------------------

void StopMotionController::onTestLightsTimeout() {
  m_stopMotion->m_light->hideOverlays();
}

//-----------------------------------------------------------------------------

void StopMotionController::updateLightsEnabled() {
  bool enabled = true;
  if (m_blackScreenForCapture->isChecked()) {
    enabled = false;
  }
  m_screen1Box->setEnabled(enabled);
  m_screen2Box->setEnabled(enabled);
  m_screen3Box->setEnabled(enabled);
  if (m_blackScreenForCapture->isChecked() || m_screen1Box->isChecked() ||
      m_screen2Box->isChecked() || m_screen3Box->isChecked()) {
    m_testLightsButton->setEnabled(true);
  } else {
    m_testLightsButton->setEnabled(false);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlaceOnXSheetChanged(int checked) {
  m_stopMotion->setPlaceOnXSheet(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlaceOnXSheetSignal(bool on) {
  m_placeOnXSheetCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseDirectShowChanged(int checked) {
  m_stopMotion->m_webcam->setUseDirectShow(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseDirectShowSignal(bool on) {
  m_directShowCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseMjpgChanged(int checked) {
  m_stopMotion->m_webcam->setUseMjpg(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseMjpgSignal(bool on) {
  m_useMjpgCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseNumpadChanged(int checked) {
  m_stopMotion->setUseNumpadShortcuts(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUseNumpadSignal(bool on) {
  m_useNumpadCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onDrawBeneathChanged(int checked) {
  m_stopMotion->setDrawBeneathLevels(checked);
}

//-----------------------------------------------------------------------------

void StopMotionController::onDrawBeneathSignal(bool on) {
  m_drawBeneathCB->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureReviewFldEdited() {
  m_stopMotion->setReviewTimeDSec(m_postCaptureReviewFld->getValue() * 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureReviewSliderChanged(bool ignore) {
  m_stopMotion->setReviewTimeDSec(m_postCaptureReviewFld->getValue() * 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onReviewTimeChangedSignal(int time) {
  m_postCaptureReviewFld->setValue(time / 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlayCaptureSignal(bool on) {
  m_playSound->setChecked(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onSubsamplingChanged(int subsampling) {
  if (subsampling < 1) {
    m_subsamplingFld->setValue(1);
    m_subsamplingFld->setDisabled(true);
  } else {
    m_subsamplingFld->setValue(subsampling);
    m_subsamplingFld->setEnabled(true);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameNumberChanged(int frameNumber) {
  m_frameNumberEdit->setValue(frameNumber);
  m_previousFrameButton->setDisabled(frameNumber == 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::onXSheetFrameNumberChanged(int frameNumber) {
  m_xSheetFrameNumberEdit->setValue(frameNumber);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureNumberOfFramesChanged(int frames) {
  m_captureFramesCombo->setCurrentIndex(frames - 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::onFilePathChanged(QString filePath) {
  m_saveInFileFld->setPath(filePath);
}

//-----------------------------------------------------------------------------

void StopMotionController::onLevelNameChanged(QString levelName) {
  m_levelNameEdit->setText(levelName);
  onRefreshTests();
}

//-----------------------------------------------------------------------------

void StopMotionController::onFileTypeChanged(QString fileType) {
  m_fileTypeCombo->setCurrentText(fileType);
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameInfoTextChanged(QString infoText) {
  m_frameInfoLabel->setText(infoText);
  m_frameInfoLabel->setStyleSheet(QString("QLabel{color: %1;}\
                                          QLabel QWidget{ color: black;}")
                                      .arg(m_stopMotion->getInfoColorName()));
  m_frameInfoLabel->setToolTip(m_stopMotion->getFrameInfoToolTip());
}

//-----------------------------------------------------------------------------

void StopMotionController::onSubsamplingFldEdited() {
  m_stopMotion->setSubsamplingValue(m_subsamplingFld->getValue());
  m_stopMotion->setSubsampling();
}

//-----------------------------------------------------------------------------

void StopMotionController::onSubsamplingSliderChanged(bool ignore) {
  m_stopMotion->setSubsamplingValue(m_subsamplingFld->getValue());
  m_stopMotion->setSubsampling();
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshCameraListCalled() {
  m_stopMotion->refreshCameraList();
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshCameraList(QString activeCamera) {
  m_cameraListCombo->blockSignals(true);
  m_cameraListCombo->clear();
  m_cameraStatusLabel->hide();
  QList<QCameraInfo> webcams = m_stopMotion->m_webcam->getWebcams();
  int count                  = webcams.count();
#ifdef WITH_CANON
  count += m_stopMotion->m_canon->getCameraCount();
#endif
  if (count < 1) {
    m_cameraListCombo->addItem(tr("No camera detected."));
    m_cameraSettingsLabel->setText(tr("No camera detected"));
    m_cameraModeLabel->setText("");
    m_cameraListCombo->setDisabled(true);
    m_captureButton->setDisabled(true);
    m_toggleLiveViewButton->setDisabled(true);
    m_toggleLiveViewButton->setText(tr("Start Live View"));
  } else {
    int maxTextLength = 0;
    m_cameraListCombo->addItem(tr("- Select camera -"));
    if (webcams.count() > 0) {
      for (int c = 0; c < webcams.size(); c++) {
        std::string name = webcams.at(c).deviceName().toStdString();
        QString camDesc  = webcams.at(c).description();
        m_cameraListCombo->addItem(camDesc);
        maxTextLength = std::max(maxTextLength, fontMetrics().width(camDesc));
      }
    }
#ifdef WITH_CANON
    if (m_stopMotion->m_canon->getCameraCount() > 0) {
      QString name;
      m_stopMotion->m_canon->getCamera(0);
      bool open = m_stopMotion->m_canon->m_sessionOpen;
      if (!open) m_stopMotion->m_canon->openCameraSession();
      name = QString::fromStdString(m_stopMotion->m_canon->getCameraName());
      if (!open) m_stopMotion->m_canon->closeCameraSession();
      m_cameraSettingsLabel->setText(name);
      m_cameraListCombo->addItem(name);
      maxTextLength = std::max(maxTextLength, fontMetrics().width(name));
    }
#endif
    m_cameraListCombo->setMaximumWidth(maxTextLength + 40);
    m_cameraListCombo->setEnabled(true);
    m_cameraListCombo->setCurrentIndex(0);
    m_captureButton->setEnabled(true);
    m_toggleLiveViewButton->setEnabled(true);
  }
  if (activeCamera != "") m_cameraListCombo->setCurrentText(activeCamera);
  m_cameraListCombo->blockSignals(false);
  m_stopMotion->updateLevelNameAndFrame(m_levelNameEdit->text().toStdWString());
  refreshOptionsLists();
#ifdef WITH_CANON
  refreshMode();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshOptionsLists() {
  m_apertureSlider->blockSignals(true);
  m_isoSlider->blockSignals(true);
  m_shutterSpeedSlider->blockSignals(true);
  m_exposureCombo->blockSignals(true);
  m_whiteBalanceCombo->blockSignals(true);
  m_kelvinSlider->blockSignals(true);
  m_imageQualityCombo->blockSignals(true);
  m_pictureStyleCombo->blockSignals(true);

  // m_isoCombo->clear();
  // m_shutterSpeedCombo->clear();
  // m_apertureSlider->clear();
  m_exposureCombo->clear();
#ifdef WITH_CANON
  if (m_stopMotion->m_canon->getCameraCount() == 0) {
    m_shutterSpeedSlider->setDisabled(true);
    m_isoSlider->setDisabled(true);
    m_apertureSlider->setDisabled(true);
    m_exposureCombo->setDisabled(true);
    m_whiteBalanceCombo->setDisabled(true);
    m_kelvinSlider->setDisabled(true);
    m_imageQualityCombo->setDisabled(true);
    m_pictureStyleCombo->setDisabled(true);
    return;
  }

  refreshApertureList();
  refreshShutterSpeedList();
  refreshIsoList();
  refreshExposureList();
  refreshWhiteBalanceList();
  refreshImageQualityList();
  refreshPictureStyleList();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshMode() {
#ifdef WITH_CANON
  if (m_stopMotion->m_canon->getCameraCount() == 0) {
    m_cameraModeLabel->setText("");
    m_cameraStatusLabel->hide();
    return;
  }
  QString mode    = m_stopMotion->m_canon->getMode();
  QString battery = m_stopMotion->m_canon->getCurrentBatteryLevel();
  m_cameraModeLabel->setText(tr("Mode: ") + mode);
  m_cameraStatusLabel->setText("Mode: " + mode + " - Battery: " + battery);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshApertureList() {
#ifdef WITH_CANON
  m_apertureSlider->blockSignals(true);
  int count = 0;
  m_stopMotion->m_canon->getAvailableApertures();
  count = m_stopMotion->m_canon->getApertureOptions().size();

  if (count == 0) {
    m_apertureLabel->setText(tr("Aperture: Auto"));
    m_apertureSlider->setDisabled(true);
    m_apertureSlider->setRange(0, 0);
  } else {
    m_apertureSlider->setEnabled(true);
    m_apertureLabel->setText(tr("Aperture: ") +
                             m_stopMotion->m_canon->getCurrentAperture());
    m_apertureSlider->setRange(0, count - 1);
    m_apertureSlider->setValue(
        m_stopMotion->m_canon->getApertureOptions().lastIndexOf(
            m_stopMotion->m_canon->getCurrentAperture()));
  }
  m_apertureSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshShutterSpeedList() {
#ifdef WITH_CANON
  m_shutterSpeedSlider->blockSignals(true);
  int count = 0;
  m_stopMotion->m_canon->getAvailableShutterSpeeds();
  count = m_stopMotion->m_canon->getShutterSpeedOptions().size();

  if (count == 0) {
    m_shutterSpeedLabel->setText(tr("Shutter Speed: Auto"));
    m_shutterSpeedSlider->setDisabled(true);
    m_shutterSpeedSlider->setRange(0, 0);
  } else {
    m_shutterSpeedSlider->setEnabled(true);
    m_shutterSpeedLabel->setText(
        tr("Shutter Speed: ") +
        m_stopMotion->m_canon->getCurrentShutterSpeed());
    m_shutterSpeedSlider->setRange(0, count - 1);
    m_shutterSpeedSlider->setValue(
        m_stopMotion->m_canon->getShutterSpeedOptions().lastIndexOf(
            m_stopMotion->m_canon->getCurrentShutterSpeed()));
  }
  m_shutterSpeedSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshIsoList() {
#ifdef WITH_CANON
  m_isoSlider->blockSignals(true);
  int count = 0;
  m_stopMotion->m_canon->getAvailableIso();
  count = m_stopMotion->m_canon->getIsoOptions().size();

  if (count == 0) {
    m_isoLabel->setText(tr("Iso: ") + tr("Auto"));
    m_isoSlider->setDisabled(true);
    m_isoSlider->setRange(0, 0);
  } else {
    m_isoSlider->setEnabled(true);
    m_isoLabel->setText(tr("Iso: ") + m_stopMotion->m_canon->getCurrentIso());
    m_isoSlider->setRange(0, count - 1);
    m_isoSlider->setValue(m_stopMotion->m_canon->getIsoOptions().lastIndexOf(
        m_stopMotion->m_canon->getCurrentIso()));
  }
  m_isoSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshExposureList() {
#ifdef WITH_CANON
  m_exposureCombo->blockSignals(true);
  m_exposureCombo->clear();
  m_stopMotion->m_canon->getAvailableExposureCompensations();
  QStringList options = m_stopMotion->m_canon->getExposureOptions();
  m_exposureCombo->addItems(options);
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_exposureCombo->count() == 0) {
    m_exposureCombo->addItem(tr("Disabled"));
    m_exposureCombo->setDisabled(true);
    m_exposureCombo->setMaximumWidth(fontMetrics().width("Disabled") + 25);
  } else {
    m_exposureCombo->setEnabled(true);
    m_exposureCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentExposureCompensation());
    m_exposureCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_exposureCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshWhiteBalanceList() {
#ifdef WITH_CANON
  m_whiteBalanceCombo->blockSignals(true);
  m_whiteBalanceCombo->clear();
  m_stopMotion->m_canon->getAvailableWhiteBalances();
  QStringList options = m_stopMotion->m_canon->getWhiteBalanceOptions();
  m_whiteBalanceCombo->addItems(options);
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_whiteBalanceCombo->count() == 0) {
    m_whiteBalanceCombo->addItem(tr("Disabled"));
    m_whiteBalanceCombo->setDisabled(true);
    m_whiteBalanceCombo->setMaximumWidth(fontMetrics().width("Disabled") + 25);
  } else {
    m_whiteBalanceCombo->setEnabled(true);
    m_whiteBalanceCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentWhiteBalance());
    m_whiteBalanceCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_whiteBalanceCombo->blockSignals(false);
  refreshColorTemperatureList();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshColorTemperatureList() {
#ifdef WITH_CANON
  m_kelvinSlider->blockSignals(true);
  int count = 0;
  count     = m_stopMotion->m_canon->getColorTemperatureOptions().size();

  if (count == 0 ||
      m_stopMotion->m_canon->getCurrentWhiteBalance() != "Color Temperature") {
    // m_kelvinCombo->addItem(tr("Disabled"));
    m_kelvinSlider->setDisabled(true);
    m_kelvinSlider->hide();
    m_kelvinValueLabel->hide();
  } else {
    m_kelvinSlider->show();
    m_kelvinValueLabel->show();
    m_kelvinSlider->setEnabled(true);
    m_kelvinSlider->setRange(0, count - 1);
    m_kelvinValueLabel->setText(
        tr("Temperature: ") +
        m_stopMotion->m_canon->getCurrentColorTemperature());
  }
  m_kelvinSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshImageQualityList() {
#ifdef WITH_CANON
  m_imageQualityCombo->blockSignals(true);
  m_imageQualityCombo->clear();
  m_stopMotion->m_canon->getAvailableImageQualities();
  QStringList options = m_stopMotion->m_canon->getImageQualityOptions();
  m_imageQualityCombo->addItems(options);
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_imageQualityCombo->count() == 0) {
    m_imageQualityCombo->addItem(tr("Disabled"));
    m_imageQualityCombo->setDisabled(true);
    m_imageQualityCombo->setMaximumWidth(fontMetrics().width("Disabled") + 25);
  } else {
    m_imageQualityCombo->setEnabled(true);
    m_imageQualityCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentImageQuality());
    m_imageQualityCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_imageQualityCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::refreshPictureStyleList() {
#ifdef WITH_CANON
  m_pictureStyleCombo->blockSignals(true);
  m_pictureStyleCombo->clear();
  m_stopMotion->m_canon->getAvailablePictureStyles();
  QStringList options = m_stopMotion->m_canon->getPictureStyleOptions();
  m_pictureStyleCombo->addItems(
      m_stopMotion->m_canon->getPictureStyleOptions());
  int maxTextLength = 0;
  for (int i = 0; i < options.size(); i++) {
    maxTextLength = std::max(maxTextLength, fontMetrics().width(options.at(i)));
  }
  if (m_pictureStyleCombo->count() == 0) {
    m_pictureStyleCombo->addItem(tr("Disabled"));
    m_pictureStyleCombo->setDisabled(true);
    m_pictureStyleCombo->setMaximumWidth(fontMetrics().width("Disabled") + 25);
  } else {
    m_pictureStyleCombo->setEnabled(true);
    m_pictureStyleCombo->setCurrentText(
        m_stopMotion->m_canon->getCurrentPictureStyle());
    m_pictureStyleCombo->setMaximumWidth(maxTextLength + 25);
  }
  m_pictureStyleCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onCameraListComboActivated(int comboIndex) {
  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  int cameraCount            = cameras.size();
#ifdef WITH_CANON
  cameraCount += m_stopMotion->m_canon->getCameraCount();
#endif
  if (cameraCount != m_cameraListCombo->count() - 1) return;

  m_stopMotion->changeCameras(comboIndex);
  m_stopMotion->updateStopMotionControls(m_stopMotion->m_usingWebcam);

  if (m_calibrationUI.groupBox->isChecked() && comboIndex > 0) {
    m_stopMotion->m_calibration.isValid = false;
    m_calibrationUI.exportBtn->setEnabled(false);
    if (m_stopMotion->m_usingWebcam) resetCalibSettingsFromFile();
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onNewCameraSelected(int index, bool useWebcam) {
  onCameraIndexChanged(index);
  onUpdateStopMotionControls(useWebcam);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCameraIndexChanged(int index) {
  if (index < m_cameraListCombo->count())
    m_cameraListCombo->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUpdateStopMotionControls(bool useWebcam) {
  int index = m_cameraListCombo->currentIndex();

  if (index == 0) {
    m_cameraListCombo->setCurrentIndex(index);
    m_resolutionCombo->hide();
    m_resolutionLabel->hide();
    m_cameraStatusLabel->hide();
    m_pickZoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
    m_zoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
    m_pickZoomButton->setChecked(false);
    m_zoomButton->setChecked(false);
    m_dslrFrame->hide();
    m_webcamFrame->hide();
    m_commonFrame->hide();
    m_noCameraFrame->show();
    m_alwaysUseLiveViewImagesButton->hide();
    // if (m_tabBar->tabText(1) == tr("Settings")) {
    //    m_tabBar->removeTab(1);
    //}
  } else if (useWebcam) {
    m_resolutionCombo->show();
    m_resolutionCombo->setEnabled(true);
    m_resolutionLabel->show();
    if (m_captureFilterSettingsBtn) m_captureFilterSettingsBtn->show();
    m_cameraStatusLabel->hide();
    m_webcamFrame->show();
    m_dslrFrame->hide();
    m_commonFrame->show();
    m_noCameraFrame->hide();
    m_alwaysUseLiveViewImagesButton->hide();
    getWebcamStatus();
    // if (m_tabBar->tabText(1) == tr("Options")) {
    //    m_tabBar->insertTab(1, tr("Settings"));
    //}
    m_webcamLabel->setText(m_cameraListCombo->currentText());
  } else {
    m_resolutionCombo->hide();
    m_resolutionLabel->hide();
    if (m_captureFilterSettingsBtn) m_captureFilterSettingsBtn->hide();
    m_cameraStatusLabel->show();
    m_dslrFrame->show();
    m_webcamFrame->hide();
    m_commonFrame->show();
    m_noCameraFrame->hide();
    m_alwaysUseLiveViewImagesButton->show();
    // if (m_tabBar->tabText(1) == tr("Options")) {
    //  m_tabBar->insertTab(1, tr("Settings"));
    //}
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamResolutionsChanged() {
  m_resolutionCombo->clear();
  QList<QSize> resolutions = m_stopMotion->m_webcam->getWebcamResolutions();
  for (int s = 0; s < resolutions.size(); s++) {
    m_resolutionCombo->addItem(QString("%1 x %2")
                                   .arg(resolutions.at(s).width())
                                   .arg(resolutions.at(s).height()));
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onNewWebcamResolutionSelected(int index) {
  m_resolutionCombo->setCurrentIndex(index);
}

//-----------------------------------------------------------------------------

void StopMotionController::onResolutionComboActivated(const QString &itemText) {
  m_stopMotion->setWebcamResolution(itemText);

  m_stopMotion->m_calibration.isValid = false;
  m_calibrationUI.exportBtn->setEnabled(false);
  if (m_stopMotion->m_usingWebcam) resetCalibSettingsFromFile();
}

//-----------------------------------------------------------------------------

void StopMotionController::onColorTypeComboChanged(int index) {
  m_camCapLevelControl->setMode(index != 2);
  m_stopMotion->m_webcam->setColorMode(index);
}

//-----------------------------------------------------------------------------

void StopMotionController::onUpdateHistogramCalled(cv::Mat image) {
  m_camCapLevelControl->updateHistogram(image);
}

////-----------------------------------------------------------------------------
//
// void StopMotionController::onWebcamFocusFldEdited() {
//    int value = m_webcamFocusSlider->getValue();
//    value = value + abs((value % 5) - 5);
//    m_stopMotion->m_webcam->setWebcamFocusValue(value);
//}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamAutofocusToggled(bool on) {
  m_stopMotion->m_webcam->setWebcamAutofocusStatus(!on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamFocusSliderChanged(int value) {
  // int value = m_webcamFocusSlider->getValue();
  value = value + abs((value % 5) - 5);
  m_stopMotion->m_webcam->setWebcamFocusValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamExposureSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamExposureValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamBrightnessSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamBrightnessValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamContrastSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamContrastValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamGainSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamGainValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onWebcamSaturationSliderChanged(int value) {
  m_stopMotion->m_webcam->setWebcamSaturationValue(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::getWebcamStatus() {
  m_webcamAutoFocusGB->setChecked(
      !m_stopMotion->m_webcam->getWebcamAutofocusStatus());
  m_webcamFocusSlider->setValue(m_stopMotion->m_webcam->getWebcamFocusValue());
  m_webcamExposureSlider->setValue(
      m_stopMotion->m_webcam->getWebcamExposureValue());
  m_webcamBrightnessSlider->setValue(
      m_stopMotion->m_webcam->getWebcamBrightnessValue());
  m_webcamContrastSlider->setValue(
      m_stopMotion->m_webcam->getWebcamContrastValue());
  m_webcamGainSlider->setValue(m_stopMotion->m_webcam->getWebcamGainValue());
  m_webcamSaturationSlider->setValue(
      m_stopMotion->m_webcam->getWebcamSaturationValue());
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureFilterSettingsBtnPressed() {
  if (!m_stopMotion->m_webcam->getWebcam() ||
      m_stopMotion->m_webcam->getWebcamDeviceName().isNull())
    return;

  QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
  for (int c = 0; c < cameras.size(); c++) {
    if (cameras.at(c).deviceName() ==
        m_stopMotion->m_webcam->getWebcamDeviceName()) {
#ifdef _WIN32
      m_stopMotion->m_webcam->openSettingsWindow();
// openCaptureFilterSettings(this, cameras.at(c).description());

#endif
      return;
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onFileFormatOptionButtonPressed() {
  if (m_fileTypeCombo->currentIndex() == 0) return;
  // Tentatively use the preview output settings
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  TOutputProperties *prop = scene->getProperties()->getPreviewProperties();
  std::string ext         = m_fileTypeCombo->currentText().toStdString();
  openFormatSettingsPopup(this, ext, prop->getFileFormatProperties(ext));
}

//-----------------------------------------------------------------------------

void StopMotionController::onLevelNameEdited() {
  m_stopMotion->updateLevelNameAndFrame(m_levelNameEdit->text().toStdWString());
}

//-----------------------------------------------------------------------------
void StopMotionController::onNextName() { m_stopMotion->nextName(); }

//-----------------------------------------------------------------------------

void StopMotionController::onNextNewLevel() {
  m_stopMotion->setToNextNewLevel();
}

//-----------------------------------------------------------------------------

void StopMotionController::onPreviousName() { m_stopMotion->previousName(); }

//-----------------------------------------------------------------------------

void StopMotionController::onNextFrame() { m_stopMotion->nextFrame(); }

//-----------------------------------------------------------------------------

void StopMotionController::onLastFrame() {}

//-----------------------------------------------------------------------------

void StopMotionController::onPreviousFrame() { m_stopMotion->previousFrame(); }

//-----------------------------------------------------------------------------

void StopMotionController::onNextXSheetFrame() {
  m_stopMotion->setXSheetFrameNumber(m_stopMotion->getXSheetFrameNumber() + 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPreviousXSheetFrame() {
  m_stopMotion->setXSheetFrameNumber(m_stopMotion->getXSheetFrameNumber() - 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::setToCurrentXSheetFrame() {
  int frameNumber = TApp::instance()->getCurrentFrame()->getFrame() + 1;
  m_stopMotion->setXSheetFrameNumber(frameNumber);
}

//-----------------------------------------------------------------------------

void StopMotionController::updateDimensions() {
  m_stopMotion->refreshFrameInfo();
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameCaptured(QImage &image) {}

//-----------------------------------------------------------------------------

void StopMotionController::onApertureChanged(int index) {
#ifdef WITH_CANON
  m_apertureSlider->blockSignals(true);
  QStringList apertureOptions = m_stopMotion->m_canon->getApertureOptions();
  m_stopMotion->m_canon->setAperture(
      apertureOptions.at(m_apertureSlider->value()));
  m_apertureSlider->setRange(0, apertureOptions.size() - 1);
  m_apertureSlider->setValue(
      apertureOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentAperture()));
  m_apertureLabel->setText(tr("Aperture: ") +
                           m_stopMotion->m_canon->getCurrentAperture());
  m_apertureSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onApertureChangedSignal(QString text) {
#ifdef WITH_CANON
  m_apertureSlider->blockSignals(true);
  QStringList apertureOptions = m_stopMotion->m_canon->getApertureOptions();
  m_apertureLabel->setText(tr("Aperture: ") +
                           m_stopMotion->m_canon->getCurrentAperture());
  m_apertureSlider->setRange(0, apertureOptions.size() - 1);
  m_apertureSlider->setValue(
      apertureOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentAperture()));
  m_apertureSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onShutterSpeedChanged(int index) {
#ifdef WITH_CANON
  m_shutterSpeedSlider->blockSignals(true);
  QStringList shutterSpeedOptions =
      m_stopMotion->m_canon->getShutterSpeedOptions();
  m_stopMotion->m_canon->setShutterSpeed(
      shutterSpeedOptions.at(m_shutterSpeedSlider->value()));
  m_shutterSpeedSlider->setRange(0, shutterSpeedOptions.size() - 1);
  m_shutterSpeedSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onShutterSpeedChangedSignal(QString text) {
#ifdef WITH_CANON
  m_shutterSpeedSlider->blockSignals(true);
  QStringList shutterSpeedOptions =
      m_stopMotion->m_canon->getShutterSpeedOptions();
  m_shutterSpeedLabel->setText(tr("Shutter Speed: ") + text);
  m_shutterSpeedSlider->setRange(0, shutterSpeedOptions.size() - 1);
  m_shutterSpeedSlider->setValue(shutterSpeedOptions.lastIndexOf(text));
  m_shutterSpeedSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onIsoChanged(int index) {
#ifdef WITH_CANON
  m_isoSlider->blockSignals(true);
  QStringList isoOptions = m_stopMotion->m_canon->getIsoOptions();
  m_stopMotion->m_canon->setIso(isoOptions.at(m_isoSlider->value()));
  m_isoSlider->setRange(0, isoOptions.size() - 1);
  m_isoSlider->setValue(
      isoOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentIso()));
  m_isoLabel->setText(tr("Iso: ") + m_stopMotion->m_canon->getCurrentIso());
  m_isoSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onIsoChangedSignal(QString text) {
#ifdef WITH_CANON
  m_isoSlider->blockSignals(true);
  QStringList isoOptions = m_stopMotion->m_canon->getIsoOptions();
  m_isoSlider->setRange(0, isoOptions.size() - 1);
  m_isoSlider->setValue(
      isoOptions.lastIndexOf(m_stopMotion->m_canon->getCurrentIso()));
  m_isoLabel->setText(tr("Iso: ") + m_stopMotion->m_canon->getCurrentIso());
  m_isoSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onExposureChanged(int index) {
#ifdef WITH_CANON
  m_exposureCombo->blockSignals(true);
  m_stopMotion->m_canon->setExposureCompensation(
      m_exposureCombo->currentText());
  m_exposureCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onExposureChangedSignal(QString text) {
#ifdef WITH_CANON
  m_exposureCombo->blockSignals(true);
  m_exposureCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentExposureCompensation());
  m_exposureCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onWhiteBalanceChanged(int index) {
#ifdef WITH_CANON
  m_whiteBalanceCombo->blockSignals(true);
  m_stopMotion->m_canon->setWhiteBalance(m_whiteBalanceCombo->currentText());
  m_whiteBalanceCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onWhiteBalanceChangedSignal(QString text) {
#ifdef WITH_CANON
  m_whiteBalanceCombo->blockSignals(true);
  m_whiteBalanceCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentWhiteBalance());
  refreshColorTemperatureList();
  m_whiteBalanceCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onColorTemperatureChanged(int index) {
#ifdef WITH_CANON
  m_kelvinSlider->blockSignals(true);
  QStringList kelvinOptions =
      m_stopMotion->m_canon->getColorTemperatureOptions();
  m_stopMotion->m_canon->setColorTemperature(
      kelvinOptions.at(m_kelvinSlider->value()));
  m_kelvinSlider->setRange(0, kelvinOptions.size() - 1);
  m_kelvinSlider->setValue(kelvinOptions.lastIndexOf(
      m_stopMotion->m_canon->getCurrentColorTemperature()));
  m_kelvinValueLabel->setText(
      tr("Temperature: ") +
      m_stopMotion->m_canon->getCurrentColorTemperature());
  m_kelvinSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onColorTemperatureChangedSignal(QString text) {
#ifdef WITH_CANON
  m_kelvinSlider->blockSignals(true);
  QStringList kelvinOptions =
      m_stopMotion->m_canon->getColorTemperatureOptions();
  m_kelvinSlider->setRange(0, kelvinOptions.size() - 1);
  m_kelvinSlider->setValue(kelvinOptions.lastIndexOf(
      m_stopMotion->m_canon->getCurrentColorTemperature()));
  m_kelvinValueLabel->setText(
      tr("Temperature: ") +
      m_stopMotion->m_canon->getCurrentColorTemperature());
  m_kelvinSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onImageQualityChanged(int index) {
#ifdef WITH_CANON
  m_imageQualityCombo->blockSignals(true);
  m_stopMotion->m_canon->setImageQuality(m_imageQualityCombo->currentText());
  m_imageQualityCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onImageQualityChangedSignal(QString text) {
#ifdef WITH_CANON
  m_imageQualityCombo->blockSignals(true);
  m_imageQualityCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentImageQuality());
  m_imageQualityCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPictureStyleChanged(int index) {
#ifdef WITH_CANON
  m_pictureStyleCombo->blockSignals(true);
  m_stopMotion->m_canon->setPictureStyle(m_pictureStyleCombo->currentText());
  m_pictureStyleCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPictureStyleChangedSignal(QString text) {
#ifdef WITH_CANON
  m_pictureStyleCombo->blockSignals(true);
  m_pictureStyleCombo->setCurrentText(
      m_stopMotion->m_canon->getCurrentPictureStyle());
  m_pictureStyleCombo->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewCompensationChanged(int index) {
#ifdef WITH_CANON
  m_liveViewCompensationSlider->blockSignals(true);
  m_stopMotion->m_canon->setLiveViewOffset(
      m_liveViewCompensationSlider->value());
  onShutterSpeedChanged(0);
  m_liveViewCompensationSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewCompensationChangedSignal(int value) {
#ifdef WITH_CANON
  m_liveViewCompensationSlider->blockSignals(true);
  m_liveViewCompensationSlider->setValue(value);
  QString labelText = tr("Live View Offset: ");
  labelText         = labelText + QString::number(value);
  m_liveViewCompensationLabel->setText(labelText);
  m_liveViewCompensationSlider->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusCheckToggled(bool on) {
#ifdef WITH_CANON
  if (on) {
    m_zoomButton->setStyleSheet("border:1px solid rgba(0, 255, 0, 255);");
  } else {
    m_zoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
  }
  m_zoomButton->blockSignals(true);
  m_zoomButton->setChecked(on);
  m_zoomButton->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPickFocusCheckToggled(bool on) {
#ifdef WITH_CANON
  if (on) {
    m_pickZoomButton->setStyleSheet("border:1px solid rgba(0, 255, 0, 255);");

  } else {
    m_pickZoomButton->setStyleSheet("border:1px solid rgba(0, 0, 0, 0);");
  }
  m_pickZoomButton->blockSignals(true);
  m_pickZoomButton->setChecked(on);
  m_pickZoomButton->blockSignals(false);
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onZoomPressed() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->zoomLiveView();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onPickZoomPressed() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->toggleZoomPicking();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusNear();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusFar();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear2() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusNear2();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar2() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusFar2();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusNear3() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusNear3();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::onFocusFar3() {
#ifdef WITH_CANON
  m_stopMotion->m_canon->focusFar3();
#endif
}

//-----------------------------------------------------------------------------

void StopMotionController::showEvent(QShowEvent *event) {
  bool hasCanon  = false;
  bool hasWebcam = false;
#ifdef WITH_CANON
  m_stopMotion->m_canon->initializeCanonSDK();
  if (!m_stopMotion->m_canon->m_sessionOpen) {
    m_dslrFrame->hide();
    m_alwaysUseLiveViewImagesButton->hide();
  } else {
    m_dslrFrame->show();
    m_alwaysUseLiveViewImagesButton->show();
    hasCanon = true;
  }
#else
  m_dslrFrame->hide();
  m_alwaysUseLiveViewImagesButton->hide();
#endif
  if (!m_stopMotion->m_usingWebcam) {
    m_resolutionCombo->hide();
    m_resolutionLabel->hide();
    m_webcamFrame->hide();
  } else {
    hasWebcam = true;
    m_webcamFrame->show();
  }

  if (!hasWebcam && !hasCanon) {
    m_commonFrame->hide();
    m_noCameraFrame->show();
  } else {
    m_commonFrame->show();
    m_noCameraFrame->hide();
  }
  onRefreshTests();
}

//-----------------------------------------------------------------------------

void StopMotionController::hideEvent(QHideEvent *event) {
  // stop interval timer if it is active
  if (m_timerCB->isChecked() && m_captureButton->isChecked()) {
    m_captureButton->setChecked(false);
    onCaptureButtonClicked(false);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::resizeEvent(QResizeEvent *event) {
  int width      = event->size().width();
  int imageWidth = 1;
  if (m_testImages.size() > 0) {
    imageWidth = m_testImages.at(0).width() + 30;

    int perRow = width / imageWidth;
    if (perRow != m_testsImagesPerRow) {
      m_testsImagesPerRow = perRow;
      reflowTestShots();
    }
  }
  QWidget::resizeEvent(event);
}

//-----------------------------------------------------------------------------

void StopMotionController::keyPressEvent(QKeyEvent *event) {
  int key          = event->key();
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  int origFrame    = fh->getFrame();
#ifdef WITH_CANON
  if ((m_stopMotion->m_canon->m_pickLiveViewZoom ||
       m_stopMotion->m_canon->m_zooming) &&
      (key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up ||
       key == Qt::Key_Down || key == Qt::Key_2 || key == Qt::Key_4 ||
       key == Qt::Key_6 || key == Qt::Key_8)) {
    if (m_stopMotion->m_canon->m_liveViewZoomReadyToPick == true) {
      if (key == Qt::Key_Left || key == Qt::Key_4) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.x -= 10;
      }
      if (key == Qt::Key_Right || key == Qt::Key_6) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.x += 10;
      }
      if (key == Qt::Key_Up || key == Qt::Key_8) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.y += 10;
      }
      if (key == Qt::Key_Down || key == Qt::Key_2) {
        m_stopMotion->m_canon->m_liveViewZoomPickPoint.y -= 10;
      }
      if (m_stopMotion->m_canon->m_zooming) {
        m_stopMotion->m_canon->setZoomPoint();
      }
    }
    m_stopMotion->m_canon->calculateZoomPoint();
    event->accept();
  } else if (m_stopMotion->m_canon->m_pickLiveViewZoom &&
             (key == Qt::Key_Escape || key == Qt::Key_Enter ||
              key == Qt::Key_Return)) {
    m_stopMotion->m_canon->toggleZoomPicking();
  } else
#endif
      if (key == Qt::Key_Up || key == Qt::Key_Left) {
    fh->prevFrame();
    event->accept();
  } else if (key == Qt::Key_Down || key == Qt::Key_Right) {
    fh->nextFrame();
    event->accept();
  } else if (key == Qt::Key_Home) {
    fh->firstFrame();
    event->accept();
  } else if (key == Qt::Key_End) {
    fh->lastFrame();
    event->accept();
  } else if (key == Qt::Key_Return || key == Qt::Key_Enter) {
    m_captureButton->animateClick();
    event->accept();
  }

  else
    event->ignore();
}
//
////-----------------------------------------------------------------------------
//
// void StopMotionController::mousePressEvent(QMouseEvent *event) {}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewToggleClicked() {
  m_stopMotion->toggleLiveView();
}

//-----------------------------------------------------------------------------

void StopMotionController::onAlwaysUseLiveViewImagesButtonClicked() {
  m_stopMotion->toggleAlwaysUseLiveViewImages();
}

//-----------------------------------------------------------------------------

void StopMotionController::onAlwaysUseLiveViewImagesToggled(bool on) {
  m_alwaysUseLiveViewImagesButton->blockSignals(true);
  m_alwaysUseLiveViewImagesButton->setChecked(on);
  m_alwaysUseLiveViewImagesButton->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onLiveViewChanged(bool on) {
  if (on) {
    m_toggleLiveViewButton->setText(tr("Stop Live View"));

  } else
    m_toggleLiveViewButton->setText(tr("Start Live View"));
}

//-----------------------------------------------------------------------------

void StopMotionController::onOnionOpacityFldEdited() {
  int value = (int)(255.0f * (float)m_onionOpacityFld->getValue() / 100.0f);
  m_stopMotion->setOpacity(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onOnionOpacitySliderChanged(bool ignore) {
  int value = (int)(255.0f * (float)m_onionOpacityFld->getValue() / 100.0f);
  m_stopMotion->setOpacity(value);
}

//-----------------------------------------------------------------------------

void StopMotionController::onOpacityChanged(int opacity) {
  m_onionOpacityFld->setValue(double(100 * opacity) / 255.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureButtonClicked(bool on) {
  if (m_timerCB->isChecked()) {
    // start interval capturing
    if (on) {
      m_stopMotion->startInterval();
    }
    // stop interval capturing
    else {
      m_stopMotion->stopInterval();
    }
  }
  // capture immediately
  else {
    m_stopMotion->captureImage();
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalTimerCBToggled(bool on) {
  m_stopMotion->toggleInterval(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalSliderValueChanged(bool on) {
  m_stopMotion->setIntervalDSec(m_timerIntervalFld->getValue() * 10.0);
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalCaptureTimerTimeout() {
  if (m_stopMotion->m_liveViewStatus < 1) {
    onCaptureButtonClicked(false);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onIntervalCountDownTimeout() {
  m_captureButton->setText(QString::number(
      m_stopMotion->m_intervalTimer->isActive()
          ? (m_stopMotion->m_intervalTimer->remainingTime() / 1000 + 1)
          : 0));
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalAmountChanged(int value) {
  m_timerIntervalFld->blockSignals(true);
  m_timerIntervalFld->setValue(value / 10.0);
  m_timerIntervalFld->blockSignals(false);
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalToggled(bool on) {
  m_timerCB->blockSignals(true);
  m_captureButton->setCheckable(on);
  if (on)
    m_captureButton->setText(tr("Start Capturing"));
  else
    m_captureButton->setText(tr("Capture"));
  m_timerCB->blockSignals(false);
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalStarted() {
  m_captureButton->setText(tr("Stop Capturing"));
  m_timerCB->setDisabled(true);
  m_timerIntervalFld->setDisabled(true);
  m_captureButton->blockSignals(true);
  m_captureButton->setChecked(true);
  m_captureButton->blockSignals(false);
}

//-----------------------------------------------------------------------------
void StopMotionController::onIntervalStopped() {
  m_captureButton->setText(tr("Start Capturing"));
  m_timerCB->setDisabled(false);
  m_timerIntervalFld->setDisabled(false);
  m_captureButton->blockSignals(true);
  m_captureButton->setChecked(false);
  m_captureButton->blockSignals(false);
}

//-----------------------------------------------------------------------------

void StopMotionController::onPlaySoundToggled(bool on) {
  m_stopMotion->setPlayCaptureSound(on);
}

//-----------------------------------------------------------------------------

void StopMotionController::openSaveInFolderPopup() {
  if (m_saveInFolderPopup->exec()) {
    QString oldPath = m_saveInFileFld->getPath();
    m_saveInFileFld->setPath(m_saveInFolderPopup->getPath());
    if (oldPath == m_saveInFileFld->getPath())
      m_stopMotion->setToNextNewLevel();
    else {
      onSaveInPathEdited();
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onFileTypeActivated() {
  m_stopMotion->setFileType(m_fileTypeCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::onFrameNumberChanged() {
  m_stopMotion->setFrameNumber(m_frameNumberEdit->getValue().getNumber());
}

//-----------------------------------------------------------------------------

void StopMotionController::onXSheetFrameNumberChanged() {
  m_stopMotion->setXSheetFrameNumber(m_xSheetFrameNumberEdit->getValue());
}

//-----------------------------------------------------------------------------

void StopMotionController::onCaptureFramesChanged(int index) {
  m_stopMotion->setCaptureNumberOfFrames(index + 1);
}

//-----------------------------------------------------------------------------

void StopMotionController::onSaveInPathEdited() {
  m_stopMotion->setFilePath(m_saveInFileFld->getPath());
}

//-----------------------------------------------------------------------------

void StopMotionController::onSceneSwitched() {
  // m_saveInFolderPopup->updateParentFolder();
  // m_saveInFileFld->setPath(m_saveInFolderPopup->getParentPath());
  // m_stopMotion->refreshFrameInfo();
  onRefreshTests();
}

//-----------------------------------------------------------------------------

void StopMotionController::serialPortChanged(int index) {
  m_stopMotion->m_serial->setSerialPort(m_controlDeviceCombo->currentText());
}

//-----------------------------------------------------------------------------

void StopMotionController::updateStopMotion() {}

//-----------------------------------------------------------------------------

void StopMotionController::onTakeTestButtonClicked() {
  if (m_stopMotion->m_liveViewStatus == StopMotion::LiveViewOpen) {
    m_stopMotion->takeTestShot();
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onRefreshTests() {
  clearTests();
  m_testFullResListVector.clear();
  m_testImages.clear();
  m_testTooltips.clear();

  TApp *app              = TApp::instance();
  ToonzScene *scene      = app->getCurrentScene()->getScene();
  TXsheet *xsh           = scene->getXsheet();
  std::wstring levelName = m_levelNameEdit->text().toStdWString();
  if (levelName == L"") return;

  TFilePath filePath  = TFilePath(m_saveInFileFld->getPath());
  TFilePath parentDir = scene->decodeFilePath(filePath);

  TFilePath testsFolder = scene->decodeFilePath(
      TFilePath(filePath) + TFilePath(levelName + L"_Tests"));

  TFilePath testsThumbsFolder = scene->decodeFilePath(
      TFilePath(filePath) + TFilePath(levelName + L"_Tests") +
      TFilePath("Thumbs"));

  TFilePath testsXml =
      scene->decodeFilePath(testsFolder + TFilePath(levelName + L"_tests.xml"));
  // load xml file for tooltip
  QDomDocument document;
  bool loadedDocument = false;
  {
    QString xmlFileName = testsXml.getQString();
    QFile file(xmlFileName);
    if (file.exists()) {
      if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file";
      }
      if (!document.setContent(&file)) {
        qDebug() << "failed to parse file";

      } else {
        loadedDocument = true;
      }
      file.close();
    }
  }
  QDomElement domBody;
  if (loadedDocument) domBody = document.documentElement();

  // if (!TSystem::doesExistFileOrLevel(testsFolder)) return;
  QString huh = QDir(testsFolder.getQString()).absolutePath();
  if (!QDir(huh).exists()) return;

  TFilePathSet set = TSystem::readDirectory(testsFolder, false, true, false);

  for (auto const &tempPath : set) {
    if (tempPath.getUndottedType() == "jpg") {
      TFrameId tempFrame    = tempPath.getFrame();
      TFilePath justFolders = tempPath.getParentDir();
      QString name          = tempPath.getQString();
      name                  = name.remove(
          0, name.lastIndexOf(QString::fromStdString(tempPath.getName())));
      name                 = name.remove(name.size() - 4, name.size() - 1);
      std::string justName = name.toStdString() + ".jpg";
      TFilePath testsThumbsFile =
          scene->decodeFilePath(testsThumbsFolder + justName);
      // TFilePath
      // testsThumbsFile(testsThumbsFp.withFrame(tempFrame.getNumber()));
      if (!TSystem::doesExistFileOrLevel(testsThumbsFile)) continue;
      QPixmap thumb = QPixmap(testsThumbsFile.getQString());
      m_testFullResListVector.push_back(tempPath);
      m_testImages.push_back(thumb);
      QString imageNumber   = tempPath.getQString();
      std::string imgNumStr = imageNumber.toStdString();
      imageNumber = imageNumber.remove(0, imageNumber.lastIndexOf("+") + 1);
      imgNumStr   = imageNumber.toStdString();
      imageNumber =
          imageNumber.remove(imageNumber.size() - 4, imageNumber.size() - 1);
      imgNumStr            = imageNumber.toStdString();
      QDomElement testInfo = domBody.firstChildElement("Test_" + imageNumber);
      if (!testInfo.isNull()) {
        bool isWebcam =
            testInfo.firstChildElement("Webcam").firstChild().nodeValue() ==
            "yes";
        if (isWebcam) {
          QString tooltipString;
          tooltipString =
              testInfo.firstChildElement("CameraName").firstChild().nodeValue();
          tooltipString += "\nWidth: " +
                           testInfo.firstChildElement("CameraResolutionX")
                               .firstChild()
                               .nodeValue();
          tooltipString += "\nHeight: " +
                           testInfo.firstChildElement("CameraResolutionY")
                               .firstChild()
                               .nodeValue();
          m_testTooltips.push_back(tooltipString);
        } else {
          QString tooltipString;
          tooltipString =
              testInfo.firstChildElement("CameraName").firstChild().nodeValue();
          tooltipString +=
              "\nAperture: " +
              testInfo.firstChildElement("Aperture").firstChild().nodeValue();
          tooltipString += "\nShutter Speed: " +
                           testInfo.firstChildElement("ShutterSpeed")
                               .firstChild()
                               .nodeValue();
          tooltipString +=
              "\nISO: " +
              testInfo.firstChildElement("ISO").firstChild().nodeValue();
          tooltipString += "\nPicture Style: " +
                           testInfo.firstChildElement("PictureStyle")
                               .firstChild()
                               .nodeValue();
          tooltipString += "\nImage Quality: " +
                           testInfo.firstChildElement("ImageQuality")
                               .firstChild()
                               .nodeValue();
          QString wb = testInfo.firstChildElement("WhiteBalance")
                           .firstChild()
                           .nodeValue();
          tooltipString += "\nWhite Balance: " + wb;
          if (wb == "Color Temperature")
            tooltipString += "\nColor Temperature: " +
                             testInfo.firstChildElement("ColorTemperature")
                                 .firstChild()
                                 .nodeValue();
          // tooltipString += "\nExposure Compensation: " +
          // testInfo.firstChildElement("ExposureCompensation").firstChild().nodeValue();
          m_testTooltips.push_back(tooltipString);
        }
      } else {
        m_testTooltips.push_back("");
      }
    }
  }
  if (m_testFullResListVector.size() > 0 && m_testImages[0].width() != 0) {
    int width           = this->width();
    int padding         = 30;
    int imageWidth      = m_testImages[0].width();
    int imageHeight     = m_testImages[0].height();
    m_testsImagesPerRow = width / imageWidth;
  }
  reflowTestShots();
}

//-----------------------------------------------------------------------------

void StopMotionController::reflowTestShots() {
  clearTests();

  if (m_testFullResListVector.size() > 0) {
    int padding         = 20;
    int imageWidth      = m_testImages[0].width();
    int imageHeight     = m_testImages[0].height();
    QHBoxLayout *layout = new QHBoxLayout();
    int j               = 0;
    for (int i = 0; i < m_testImages.size(); i++) {
      QPushButton *button = new QPushButton(this);
      button->setIcon(QIcon(m_testImages.at(i)));
      button->setFixedHeight(imageHeight + padding);
      button->setFixedWidth(imageWidth + padding);
      button->setIconSize(QSize(imageWidth, imageHeight));
      button->setToolTip(m_testTooltips.at(i));
      connect(button, &QPushButton::clicked, [=] {
        FlipBook *fb = ::viewFile(m_testFullResListVector.at(i));
      });
      layout->addWidget(button);
      j++;
      if (j >= m_testsImagesPerRow) {
        m_testHBoxes.push_back(layout);
        layout = new QHBoxLayout();
        j      = 0;
      }
    }
    if (j < m_testsImagesPerRow && j > 0) {
      m_testHBoxes.push_back(layout);
    }
  }
  for (QHBoxLayout *layout : m_testHBoxes) {
    // m_testsOutsideLayout->addLayout(layout);
    m_testsInsideLayout->insertLayout(m_testsInsideLayout->count() - 1, layout);
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::clearTests() {
  if (m_testHBoxes.size() > 0) {
    for (QHBoxLayout *layout : m_testHBoxes) {
      QLayoutItem *item;
      while ((item = layout->takeAt(0)) != NULL) {
        delete item->widget();
        delete item;
      }
      m_testsInsideLayout->removeItem(layout);
      delete layout;
    }
    m_testHBoxes.clear();
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onCalibCapBtnClicked() {
  if (!m_stopMotion->m_hasLiveViewImage ||
      m_stopMotion->m_liveViewStatus !=
          m_stopMotion->LiveViewStatus::LiveViewOpen) {
    DVGui::warning(tr("Cannot capture image unless live view is active."));
    return;
  }
  m_stopMotion->m_calibration.captureCue = true;
}

//-----------------------------------------------------------------------------

void StopMotionController::onCalibNewBtnClicked() {
  if (m_stopMotion->m_calibration.isValid) {
    QString question = tr("Do you want to restart camera calibration?");
    int ret =
        DVGui::MsgBox(question, QObject::tr("Restart"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
  }
  // initialize calibration parameter
  m_stopMotion->m_calibration.filePath    = getCurrentCalibFilePath();
  m_stopMotion->m_calibration.captureCue  = false;
  m_stopMotion->m_calibration.refCaptured = 0;
  m_stopMotion->m_calibration.obj_points.clear();
  m_stopMotion->m_calibration.image_points.clear();
  m_stopMotion->m_calibration.isValid = false;

  // initialize label
  m_calibrationUI.label->setText(
      QString("%1/%2").arg(m_stopMotion->m_calibration.refCaptured).arg(10));
  // swap UIs
  m_calibrationUI.newBtn->hide();
  m_calibrationUI.loadBtn->hide();
  m_calibrationUI.exportBtn->hide();
  m_calibrationUI.label->show();
  m_calibrationUI.capBtn->show();
  m_calibrationUI.cancelBtn->show();
}

//-----------------------------------------------------------------------------

void StopMotionController::resetCalibSettingsFromFile() {
  if (m_calibrationUI.capBtn->isVisible()) {
    // swap UIs
    m_calibrationUI.label->hide();
    m_calibrationUI.capBtn->hide();
    m_calibrationUI.cancelBtn->hide();
    m_calibrationUI.newBtn->show();
    m_calibrationUI.loadBtn->show();
    m_calibrationUI.exportBtn->show();
  }
  if (m_calibrationUI.groupBox->isChecked() &&
      !m_stopMotion->m_calibration.isValid) {
    QString calibFp = getCurrentCalibFilePath();
    std::cout << calibFp.toStdString() << std::endl;
    if (!calibFp.isEmpty() && QFileInfo(calibFp).exists()) {
      cv::Mat intrinsic, distCoeffs, new_intrinsic;
      cv::FileStorage fs(calibFp.toStdString(), cv::FileStorage::READ);
      if (!fs.isOpened()) return;
      std::string identifierStr;
      fs["identifier"] >> identifierStr;
      if (identifierStr != "OpenToonzCameraCalibrationSettings") return;
      cv::Size resolution;
      int camWidth = m_stopMotion->m_usingWebcam
                         ? m_stopMotion->m_webcam->getWebcamWidth()
                         : m_stopMotion->m_canon->m_fullImageDimensions.lx;
      int camHeight = m_stopMotion->m_usingWebcam
                          ? m_stopMotion->m_webcam->getWebcamHeight()
                          : m_stopMotion->m_canon->m_fullImageDimensions.ly;
      QSize currentResolution(camWidth, camHeight);
      fs["resolution"] >> resolution;
      if (currentResolution != QSize(resolution.width, resolution.height))
        return;
      fs["instrinsic"] >> intrinsic;
      fs["distCoeffs"] >> distCoeffs;
      fs["new_intrinsic"] >> new_intrinsic;
      fs.release();

      cv::Mat mapX, mapY;
      cv::Mat mapR = cv::Mat::eye(3, 3, CV_64F);
      cv::initUndistortRectifyMap(
          intrinsic, distCoeffs, mapR, new_intrinsic,
          cv::Size(currentResolution.width(), currentResolution.height()),
          CV_32FC1, mapX, mapY);

      if (m_stopMotion->m_usingWebcam)
        m_stopMotion->m_webcam->setCalibration(mapX, mapY);
      else
        m_stopMotion->m_canon->setCalibration(mapX, mapY);

      m_stopMotion->m_calibration.isValid  = true;
      m_stopMotion->m_calibration.filePath = calibFp;
      m_calibrationUI.exportBtn->setEnabled(true);
    }
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::captureCalibrationRefImage(cv::Mat &image) {
  cv::cvtColor(image, image, cv::COLOR_RGB2GRAY);
  std::vector<cv::Point2f> corners;
  bool found = cv::findChessboardCorners(
      image, m_stopMotion->m_calibration.boardSize, corners,
      cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_FILTER_QUADS);
  if (found) {
    // compute corners in detail
    cv::cornerSubPix(
        image, corners, cv::Size(11, 11), cv::Size(-1, -1),
        cv::TermCriteria(cv::TermCriteria::EPS | cv::TermCriteria::MAX_ITER, 30,
                         0.1));
    // count up
    m_stopMotion->m_calibration.refCaptured++;
    // register corners
    m_stopMotion->m_calibration.image_points.push_back(corners);
    // register 3d points in real world space
    std::vector<cv::Point3f> obj;
    for (int i = 0; i < m_stopMotion->m_calibration.boardSize.width *
                            m_stopMotion->m_calibration.boardSize.height;
         i++)
      obj.push_back(cv::Point3f(i / m_stopMotion->m_calibration.boardSize.width,
                                i % m_stopMotion->m_calibration.boardSize.width,
                                0.0f));
    m_stopMotion->m_calibration.obj_points.push_back(obj);

    // needs 10 references
    if (m_stopMotion->m_calibration.refCaptured < 10) {
      // update label
      m_calibrationUI.label->setText(
          QString("%1/%2")
              .arg(m_stopMotion->m_calibration.refCaptured)
              .arg(10));
    } else {
      // swap UIs
      m_calibrationUI.label->hide();
      m_calibrationUI.capBtn->hide();
      m_calibrationUI.cancelBtn->hide();
      m_calibrationUI.newBtn->show();
      m_calibrationUI.loadBtn->show();
      m_calibrationUI.exportBtn->show();

      cv::Mat intrinsic          = cv::Mat(3, 3, CV_32FC1);
      intrinsic.ptr<float>(0)[0] = 1.f;
      intrinsic.ptr<float>(1)[1] = 1.f;
      cv::Mat distCoeffs;
      std::vector<cv::Mat> rvecs;
      std::vector<cv::Mat> tvecs;
      cv::calibrateCamera(m_stopMotion->m_calibration.obj_points,
                          m_stopMotion->m_calibration.image_points,
                          image.size(), intrinsic, distCoeffs, rvecs, tvecs);

      cv::Mat mapX, mapY;
      cv::Mat mapR          = cv::Mat::eye(3, 3, CV_64F);
      cv::Mat new_intrinsic = cv::getOptimalNewCameraMatrix(
          intrinsic, distCoeffs, image.size(),
          0.0);  // setting the last argument to 1.0 will include all source
                 // pixels in the frame
      cv::initUndistortRectifyMap(intrinsic, distCoeffs, mapR, new_intrinsic,
                                  image.size(), CV_32FC1, mapX, mapY);

      int camWidth, camHeight;
      if (m_stopMotion->m_usingWebcam) {
        m_stopMotion->m_webcam->setCalibration(mapX, mapY);
        camWidth  = m_stopMotion->m_webcam->getWebcamWidth();
        camHeight = m_stopMotion->m_webcam->getWebcamHeight();
      } else {
        m_stopMotion->m_canon->setCalibration(mapX, mapY);
        camWidth  = m_stopMotion->m_canon->m_fullImageDimensions.lx;
        camHeight = m_stopMotion->m_canon->m_fullImageDimensions.ly;
      }

      // save calibration settings
      QString calibFp = getCurrentCalibFilePath();
      cv::FileStorage fs(calibFp.toStdString(), cv::FileStorage::WRITE);
      if (!fs.isOpened()) {
        DVGui::warning(
            tr("Failed to save calibration settings to %1.").arg(calibFp));
        return;
      }
      fs << "identifier"
         << "OpenToonzCameraCalibrationSettings";
      fs << "resolution" << cv::Size(camWidth, camHeight);
      fs << "instrinsic" << intrinsic;
      fs << "distCoeffs" << distCoeffs;
      fs << "new_intrinsic" << new_intrinsic;
      fs.release();

      m_stopMotion->m_calibration.isValid = true;
      m_calibrationUI.exportBtn->setEnabled(true);
    }
  }
}

//-----------------------------------------------------------------------------

QString StopMotionController::getCurrentCalibFilePath() {
  QString cameraName = m_cameraListCombo->currentText();
  if (cameraName.isEmpty()) return QString();
  QString resolution   = m_resolutionCombo->currentText();
  QString hostName     = QHostInfo::localHostName();
  QString fileName = hostName + "_" + cameraName + "_" + resolution + ".xml";
  TFilePath folderPath = ToonzFolder::getLibraryFolder() +
                         "camera calibration" + TFilePath(fileName);
  return folderPath.getQString();
}

//-----------------------------------------------------------------------------

void StopMotionController::onCalibLoadBtnClicked() {
  LoadCalibrationFilePopup popup(this);

  QString fp = popup.getPath().getQString();
  if (fp.isEmpty()) return;
  try {
    cv::FileStorage fs(fp.toStdString(), cv::FileStorage::READ);
    if (!fs.isOpened())
      throw TException(fp.toStdWString() + L": Can't open file");

    std::string identifierStr;
    fs["identifier"] >> identifierStr;
    if (identifierStr != "OpenToonzCameraCalibrationSettings")
      throw TException(fp.toStdWString() + L": Identifier does not match");
    cv::Size resolution;
    int camWidth = m_stopMotion->m_usingWebcam
                       ? m_stopMotion->m_webcam->getWebcamWidth()
                       : m_stopMotion->m_canon->m_fullImageDimensions.lx;
    int camHeight = m_stopMotion->m_usingWebcam
                        ? m_stopMotion->m_webcam->getWebcamHeight()
                        : m_stopMotion->m_canon->m_fullImageDimensions.ly;
    QSize currentResolution(camWidth, camHeight);
    fs["resolution"] >> resolution;
    if (currentResolution != QSize(resolution.width, resolution.height))
      throw TException(fp.toStdWString() + L": Resolution does not match");
  } catch (const TException &se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  } catch (...) {
    DVGui::error(tr("Couldn't load %1").arg(fp));
    return;
  }

  if (m_stopMotion->m_calibration.isValid) {
    QString question = tr("Overwriting the current calibration. Are you sure?");
    int ret = DVGui::MsgBox(question, QObject::tr("OK"), QObject::tr("Cancel"));
    if (ret == 0 || ret == 2) return;
    m_stopMotion->m_calibration.isValid = false;
  }

  QString calibFp = getCurrentCalibFilePath();
  TSystem::copyFile(TFilePath(calibFp), TFilePath(fp), true);
  resetCalibSettingsFromFile();
}

//-----------------------------------------------------------------------------

void StopMotionController::onCalibExportBtnClicked() {
  // just in case
  if (!m_stopMotion->m_calibration.isValid) return;
  if (!QFileInfo(getCurrentCalibFilePath()).exists()) return;

  ExportCalibrationFilePopup popup(this);

  QString fp = popup.getPath().getQString();
  if (fp.isEmpty()) return;

  try {
    {
      QFileInfo fs(fp);
      if (fs.exists() && !fs.isWritable()) {
        throw TSystemException(
            TFilePath(fp),
            L"The file cannot be saved: it is a read only file.");
      }
    }
    TSystem::copyFile(TFilePath(fp), TFilePath(getCurrentCalibFilePath()),
                      true);
  } catch (const TSystemException &se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
  } catch (...) {
    DVGui::error(tr("Couldn't save %1").arg(fp));
  }
}

//-----------------------------------------------------------------------------

void StopMotionController::onCalibReadme() {
  TFilePath readmeFp =
      ToonzFolder::getLibraryFolder() + "camera calibration" + "readme.txt";
  if (!TFileStatus(readmeFp).doesExist()) return;
  if (TSystem::isUNC(readmeFp))
    QDesktopServices::openUrl(QUrl(readmeFp.getQString()));
  else
    QDesktopServices::openUrl(QUrl::fromLocalFile(readmeFp.getQString()));
}
//-----------------------------------------------------------------------------

void StopMotionController::onCalibImageCaptured() {
  cv::Mat camImage = m_stopMotion->m_usingWebcam
                         ? m_stopMotion->m_webcam->getWebcamImage()
                         : m_stopMotion->m_canon->getcanonImage();
  captureCalibrationRefImage(camImage);
}

//=============================================================================

ExportCalibrationFilePopup::ExportCalibrationFilePopup(QWidget *parent)
    : GenericSaveFilePopup(tr("Export Camera Calibration Settings")) {
  Qt::WindowFlags flags = windowFlags();
  setParent(parent);
  setWindowFlags(flags);
  m_browser->enableGlobalSelection(false);
  setFilterTypes(QStringList("xml"));
}

void ExportCalibrationFilePopup::showEvent(QShowEvent *e) {
  FileBrowserPopup::showEvent(e);
  setFolder(ToonzFolder::getLibraryFolder() + "camera calibration");
}

//=============================================================================

LoadCalibrationFilePopup::LoadCalibrationFilePopup(QWidget *parent)
    : GenericLoadFilePopup(tr("Load Camera Calibration Settings")) {
  Qt::WindowFlags flags = windowFlags();
  setParent(parent);
  setWindowFlags(flags);
  m_browser->enableGlobalSelection(false);
  setFilterTypes(QStringList("xml"));
}

void LoadCalibrationFilePopup::showEvent(QShowEvent *e) {
  FileBrowserPopup::showEvent(e);
  setFolder(ToonzFolder::getLibraryFolder() + "camera calibration");
}
