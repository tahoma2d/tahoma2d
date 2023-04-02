

#include "projectpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "filebrowser.h"
#include "iocommand.h"
#include "filebrowsermodel.h"
#include "dvdirtreeview.h"
#include "mainwindow.h"
#include "filebrowserpopup.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"

// TnzLib
#include "toonz/filepathproperties.h"

// TnzCore includes
#include "tsystem.h"
#include "tenv.h"
#include "tapp.h"
#include "tfilepath.h"

#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/preferences.h"

// Qt includes
#include <QPushButton>
#include <QFileInfo>
#include <QFormLayout>
#include <QMainWindow>
#include <QComboBox>
#include <QStandardPaths>
#include <QGroupBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QRadioButton>

using namespace DVGui;

namespace {

enum { Rule_Standard = 0, Rule_Custom };

}

//===================================================================

TFilePath getDocumentsPath() {
  QString documentsPath =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0];
  return TFilePath(documentsPath);
}

//=============================================================================
// ProjectPopup
//-----------------------------------------------------------------------------

ProjectPopup::ProjectPopup(bool isModal)
    : Dialog(TApp::instance()->getMainWindow(), isModal, false, "Project") {
  TProjectManager *pm = TProjectManager::instance();
  // m_mainFrame->setFixedHeight(100);
  m_mainFrame->setMinimumWidth(400);
  this->layout()->setSizeConstraint(QLayout::SetFixedSize);

  m_choosePrjLabel      = new QLabel(tr("Project:"), this);
  m_prjNameLabel        = new QLabel(tr("Project Name:"), this);
  m_pathFieldLabel      = new QLabel(tr("Create Project In:"), this);
  m_nameFld             = new LineEdit();
  m_nameAsLabel         = new QLabel();
  m_projectLocationFld =
      new DVGui::FileField(this, getDocumentsPath().getQString());
  QString defaultProjectLocation =
      Preferences::instance()->getDefaultProjectPath();
  if (TSystem::doesExistFileOrLevel(TFilePath(defaultProjectLocation))) {
    m_projectLocationFld->setPath(defaultProjectLocation);
  }

  m_nameFld->setMaximumHeight(WidgetHeight);
  m_nameAsLabel->setMaximumHeight(WidgetHeight);

  m_showSettingsButton = new QPushButton("", this);
  m_showSettingsButton->setObjectName("menuToggleButton");
  m_showSettingsButton->setFixedSize(15, 15);
  m_showSettingsButton->setIcon(createQIcon("menu_toggle"));
  m_showSettingsButton->setCheckable(true);
  m_showSettingsButton->setChecked(false);
  m_showSettingsButton->setFocusPolicy(Qt::NoFocus);

  m_useSubSceneCbs =
      new CheckBox(tr("*Separate assets into scene sub-folders"));
  m_useSubSceneCbs->setMaximumHeight(WidgetHeight);

  m_rulePreferenceBG       = new QButtonGroup(this);
  QRadioButton *standardRB = new QRadioButton(tr("Standard"), this);
  QRadioButton *customRB =
      new QRadioButton(QString("[Experimental]  ") + tr("Custom"), this);
  m_acceptNonAlphabetSuffixCB =
      new CheckBox(tr("Accept Non-alphabet Suffix"), this);
  m_letterCountCombo = new QComboBox(this);

  //-----

  m_rulePreferenceBG->addButton(standardRB, Rule_Standard);
  m_rulePreferenceBG->addButton(customRB, Rule_Custom);
  m_rulePreferenceBG->setExclusive(true);
  standardRB->setToolTip(tr(
      "In the standard mode files with the following file name are handled as sequential images:\n\
[LEVEL_NAME][\".\"or\"_\"][FRAME_NUMBER][SUFFIX].[EXTENSION]\n\
For [SUFFIX] zero or one occurrences of alphabet (a-z, A-Z) can be used in the standard mode."));
  customRB->setToolTip(
      tr("In the custom mode you can customize the file path rules.\n\
Note that this mode uses regular expression for file name validation and may slow the operation."));

  m_letterCountCombo->addItem(tr("1"), 1);
  m_letterCountCombo->addItem(tr("2"), 2);
  m_letterCountCombo->addItem(tr("3"), 3);
  m_letterCountCombo->addItem(tr("5"), 5);
  m_letterCountCombo->addItem(tr("Unlimited"), 0);

  m_settingsLabel = new QLabel(tr("Settings"), this);
  m_settingsBox   = createSettingsBox();

  //----layout
  m_topLayout->setMargin(5);
  m_topLayout->setSpacing(10);
  {
    QGridLayout *upperLayout = new QGridLayout();
    upperLayout->setMargin(5);
    upperLayout->setHorizontalSpacing(5);
    upperLayout->setVerticalSpacing(10);
    {
      upperLayout->addWidget(m_prjNameLabel, 0, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(m_nameFld, 0, 1);
      upperLayout->addWidget(m_nameAsLabel, 0, 1);

      upperLayout->addWidget(m_pathFieldLabel, 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(m_projectLocationFld, 1, 1);
    }
    upperLayout->setColumnStretch(0, 0);
    upperLayout->setColumnStretch(1, 1);

    m_topLayout->addLayout(upperLayout);

    QHBoxLayout *settingsLabelLay = new QHBoxLayout();
    settingsLabelLay->setMargin(0);
    settingsLabelLay->setSpacing(3);
    {
      settingsLabelLay->addWidget(m_showSettingsButton, 0);
      settingsLabelLay->addWidget(m_settingsLabel, 0);
      settingsLabelLay->addStretch(1);
    }
    m_topLayout->addLayout(settingsLabelLay, 0);
    m_topLayout->addWidget(m_settingsBox, 0);
  }

  bool ret = connect(m_showSettingsButton, SIGNAL(toggled(bool)), m_settingsBox,
                     SLOT(setVisible(bool)));

  pm->addListener(this);

  //---------
  connect(m_rulePreferenceBG, SIGNAL(buttonClicked(int)), this,
          SLOT(onRulePreferenceToggled(int)));
}

//-----------------------------------------------------------------------------

QFrame *ProjectPopup::createSettingsBox() {
  QFrame *projectSettingsBox = new QFrame(this);
  projectSettingsBox->setObjectName("ProjectSettingsBox");

  TProjectManager *pm = TProjectManager::instance();

  QTabWidget *tabWidget = new QTabWidget(this);

  QVBoxLayout *settingsLayout       = new QVBoxLayout();
  settingsLayout->setMargin(5);
  settingsLayout->setSpacing(10);
  {
    settingsLayout->addWidget(tabWidget, 1);

    // project folder settings
    QWidget *projectFolderPanel = new QWidget(this);
    QGridLayout *folderLayout = new QGridLayout();
    folderLayout->setMargin(5);
    folderLayout->setHorizontalSpacing(5);
    folderLayout->setVerticalSpacing(10);
    {
      std::vector<std::string> folderNames;
      pm->getFolderNames(folderNames);
      int i;
      for (i = 0; i < (int)folderNames.size(); i++) {
        std::string name = folderNames[i];
        QString qName = QString::fromStdString(name);
        FileField *ff = new FileField(0, qName);
        m_folderFlds.append(qMakePair(name, ff));
        bool assetFolder = false;
        if (qName == "drawings" || qName == "extras" || qName == "inputs")
          assetFolder = true;
        QLabel *label = new QLabel("+" + qName + (assetFolder ? "*" : ""), this);
        folderLayout->addWidget(label, i + 4, 0, Qt::AlignRight | Qt::AlignVCenter);
        folderLayout->addWidget(ff, i + 4, 1);
      }
      int currentRow = folderLayout->rowCount();
      folderLayout->addWidget(m_useSubSceneCbs, currentRow, 1);
    }
    projectFolderPanel->setLayout(folderLayout);
    tabWidget->addTab(projectFolderPanel, tr("Project Folder"));


    // file path settings
    QWidget *filePathPanel = new QWidget(this);
    QVBoxLayout *fpLayout  = new QVBoxLayout();
    fpLayout->setMargin(5);
    fpLayout->setSpacing(10);
    {
      fpLayout->addWidget(m_rulePreferenceBG->buttons()[0], 0); // standardRB
      fpLayout->addWidget(m_rulePreferenceBG->buttons()[1], 0); // customRB

      // add some indent
      QGridLayout *customLay = new QGridLayout();
      customLay->setMargin(10);
      customLay->setHorizontalSpacing(10);
      customLay->setVerticalSpacing(10);
      {
        customLay->addWidget(m_acceptNonAlphabetSuffixCB, 0, 0, 1, 2);
        customLay->addWidget(
            new QLabel(tr("Maximum Letter Count For Suffix"), this), 1, 0);
        customLay->addWidget(m_letterCountCombo, 1, 1);
      }
      customLay->setColumnStretch(2, 1);
      fpLayout->addLayout(customLay, 0);
      fpLayout->addStretch(1);
    }
    filePathPanel->setLayout(fpLayout);
    tabWidget->addTab(filePathPanel, tr("File Path Rules"));
  }

  projectSettingsBox->setLayout(settingsLayout);

  return projectSettingsBox;
}

//-----------------------------------------------------------------------------

void ProjectPopup::updateFieldsFromProject(TProject *project) {
  m_nameFld->setText(toQString(project->getName()));
  m_nameAsLabel->setText(toQString(project->getName()));
  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    std::string folderName = m_folderFlds[i].first;
    TFilePath path         = project->getFolder(folderName);
    m_folderFlds[i].second->setPath(toQString(path));
  }
/*
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    std::string folderName = m_useScenePathCbs[i].first;
    Qt::CheckState cbState =
        project->getUseScenePath(folderName) ? Qt::Checked : Qt::Unchecked;
    CheckBox *cb                = m_useScenePathCbs[i].second;
    bool signalesAlreadyBlocked = cb->blockSignals(true);
    cb->setCheckState(cbState);
    cb->blockSignals(signalesAlreadyBlocked);
  }
*/
  m_useSubSceneCbs->blockSignals(true);
  m_useSubSceneCbs->setChecked(project->getUseSubScenePath());
  m_useSubSceneCbs->blockSignals(false);

  // file path
  FilePathProperties *fpProp = project->getFilePathProperties();
  bool useStandard           = fpProp->useStandard();
  bool acceptNonAlphabet     = fpProp->acceptNonAlphabetSuffix();
  int letterCount            = fpProp->letterCountForSuffix();
  m_rulePreferenceBG->button((useStandard) ? Rule_Standard : Rule_Custom)
      ->setChecked(true);
  onRulePreferenceToggled((useStandard) ? Rule_Standard : Rule_Custom);
  m_acceptNonAlphabetSuffixCB->setChecked(acceptNonAlphabet);
  m_letterCountCombo->setCurrentIndex(
      m_letterCountCombo->findData(letterCount));
}

//-----------------------------------------------------------------------------

void ProjectPopup::updateProjectFromFields(TProject *project) {
  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    std::string folderName = m_folderFlds[i].first;
    TFilePath folderPath(m_folderFlds[i].second->getPath().toStdWString());
    project->setFolder(folderName, folderPath);
  }
/*
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    std::string folderName = m_useScenePathCbs[i].first;
    int cbState            = m_useScenePathCbs[i].second->checkState();
    bool useScenePath      = cbState == Qt::Checked;
    project->setUseScenePath(folderName, cbState);
  }
*/
  bool useScenePath = m_useSubSceneCbs->isChecked();
  project->setUseSubScenePath(useScenePath);

  // file path
  FilePathProperties *fpProp = project->getFilePathProperties();
  bool useStandard           = m_rulePreferenceBG->checkedId() == Rule_Standard;
  bool acceptNonAlphabet     = m_acceptNonAlphabetSuffixCB->isChecked();
  int letterCount            = m_letterCountCombo->currentData().toInt();
  fpProp->setUseStandard(useStandard);
  fpProp->setAcceptNonAlphabetSuffix(acceptNonAlphabet);
  fpProp->setLetterCountForSuffix(letterCount);

  if (TFilePath::setFilePathProperties(useStandard, acceptNonAlphabet,
                                       letterCount))
    DvDirModel::instance()->refreshFolderChild(QModelIndex());  // refresh all

  TProjectManager::instance()->notifyProjectChanged();
}

//-----------------------------------------------------------------------------

void ProjectPopup::onProjectSwitched() {
  TProjectP currentProject = TProjectManager::instance()->getCurrentProject();
  updateFieldsFromProject(currentProject.getPointer());
}

//-----------------------------------------------------------------------------

void ProjectPopup::showEvent(QShowEvent *) {
  TProjectP currentProject = TProjectManager::instance()->getCurrentProject();
  updateFieldsFromProject(currentProject.getPointer());
}

//-----------------------------------------------------------------------------

void ProjectPopup::onRulePreferenceToggled(int id) {
  m_acceptNonAlphabetSuffixCB->setEnabled((id == Rule_Custom));
  m_letterCountCombo->setEnabled((id == Rule_Custom));
}

//=============================================================================
/*! \class ProjectSettingsPopup
                \brief The ProjectSettingsPopup class provides a dialog to
   change current project settings.

                Inherits \b ProjectPopup.
*/
//-----------------------------------------------------------------------------

ProjectSettingsPopup::ProjectSettingsPopup() : ProjectPopup(false) {
  setWindowTitle(tr("Project Settings"));

  //  m_prjNameLabel->hide();
  m_nameFld->hide();
  m_choosePrjLabel->hide();
  m_pathFieldLabel->hide();
  m_projectLocationFld->hide();
  m_showSettingsButton->hide();
  m_settingsLabel->hide();

  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    FileField *ff = m_folderFlds[i].second;
    connect(ff, SIGNAL(pathChanged()), this, SLOT(onSomethingChanged()));
  }
/*
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    CheckBox *cb = m_useScenePathCbs[i].second;
    connect(cb, SIGNAL(stateChanged(int)), this,
            SLOT(onUseSceneChekboxChanged(int)));
  }
  */

  connect(m_useSubSceneCbs, SIGNAL(stateChanged(int)), this, SLOT(onSomethingChanged()));

  // file path settings
  connect(m_rulePreferenceBG, SIGNAL(buttonClicked(int)), this,
          SLOT(onSomethingChanged()));
  connect(m_acceptNonAlphabetSuffixCB, SIGNAL(clicked(bool)), this,
          SLOT(onSomethingChanged()));
  connect(m_letterCountCombo, SIGNAL(activated(int)), this,
          SLOT(onSomethingChanged()));
}

//-----------------------------------------------------------------------------

void ProjectSettingsPopup::projectChanged() {
  TProjectManager *pm     = TProjectManager::instance();
  TFilePath projectFolder = TFilePath(m_projectLocationFld->getPath());
  TFilePath path          = pm->projectFolderToProjectPath(projectFolder);

  if (path == pm->getCurrentProjectPath()) {
    return;
  }
  if (!TSystem::doesExistFileOrLevel(projectFolder)) {
    projectFolder =
        TApp::instance()->getCurrentScene()->getScene()->decodeFilePath(
            projectFolder);
    m_projectLocationFld->setPath(projectFolder.getQString());
    if (!TSystem::doesExistFileOrLevel(projectFolder)) {
      DVGui::warning(tr(
          "This is not a valid folder.  Please choose an existing location."));
      m_projectLocationFld->setPath(m_oldPath.getQString());
      return;
    }
  }
  if (!pm->isProject(projectFolder)) {
    QStringList buttonList;
    buttonList.append(tr("Yes"));
    buttonList.append(tr("No"));
    int answer = DVGui::MsgBox(tr("No project found at this location \n"
                                  "What would you like to do?"),
                               tr("Make a new project"), tr("Cancel"), 1, this);
    if (answer != 1) {
      m_projectLocationFld->blockSignals(true);
      m_projectLocationFld->setPath(TApp::instance()
                                        ->getCurrentScene()
                                        ->getScene()
                                        ->getProject()
                                        ->getProjectFolder()
                                        .getQString());
      m_projectLocationFld->blockSignals(false);
    } else {
      ProjectCreatePopup *popup = new ProjectCreatePopup();
      popup->setPath(projectFolder.getQString());
      popup->exec();
      accept();
      return;
    }
  }

  if (!IoCmd::saveSceneIfNeeded(QObject::tr("Change Project"))) {
    m_projectLocationFld->blockSignals(true);
    m_projectLocationFld->setPath(TApp::instance()
                                      ->getCurrentScene()
                                      ->getScene()
                                      ->getProject()
                                      ->getProjectFolder()
                                      .getQString());
    m_projectLocationFld->blockSignals(false);
    return;
  }

  pm->setCurrentProjectPath(path);

  TProject *projectP =
      TProjectManager::instance()->getCurrentProject().getPointer();

  // In case the project file was upgraded to current version, save it now
  if (projectP->getProjectPath() != path) {
    projectP->save();
  }
  RecentFiles::instance()->addFilePath(path.getParentDir().getQString(),
                                       RecentFiles::Project);
  updateFieldsFromProject(projectP);
  IoCmd::newScene();
  accept();
}

//-----------------------------------------------------------------------------

void ProjectSettingsPopup::onProjectChanged() {
  m_projectLocationFld->blockSignals(true);
  m_projectLocationFld->setPath(TApp::instance()
                                    ->getCurrentScene()
                                    ->getScene()
                                    ->getProject()
                                    ->getProjectFolder()
                                    .getQString());
  m_projectLocationFld->blockSignals(false);
  TProjectP currentProject = TProjectManager::instance()->getCurrentProject();
  updateFieldsFromProject(currentProject.getPointer());
}

//-----------------------------------------------------------------------------

void ProjectSettingsPopup::onSomethingChanged() {
  TProjectP project = TProjectManager::instance()->getCurrentProject();
  updateProjectFromFields(project.getPointer());
  try {
    project->save();
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }
  DvDirModel::instance()->refreshFolder(project->getProjectFolder());
}

//-----------------------------------------------------------------------------

void ProjectSettingsPopup::showEvent(QShowEvent *) {
  TProjectP currentProject = TProjectManager::instance()->getCurrentProject();
  updateFieldsFromProject(currentProject.getPointer());

  QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  setSizePolicy(sizePolicy);
  setFixedSize(width(), height());
  setSizeGripEnabled(false);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ProjectSettingsPopup> openProjectSettingsPopup(
    MI_ProjectSettings);

//=============================================================================
/* class ProjectCreatePopup
   The ProjectCreatePopup class provides a modal dialog to
   create a new project.

   Inherits ProjectPopup.
*/
//-----------------------------------------------------------------------------

ProjectCreatePopup::ProjectCreatePopup() : ProjectPopup(true) {
  setWindowTitle(tr("New Project"));

  m_pathFieldLabel->setText(tr("Create Project In:"));

  QPushButton *okBtn = new QPushButton(tr("OK"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(createProject()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  m_buttonLayout->setMargin(0);
  m_buttonLayout->setSpacing(20);
  {
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(okBtn);
    m_buttonLayout->addWidget(cancelBtn);
  }

  m_prjNameLabel->show();
  m_nameFld->show();
  m_nameAsLabel->hide();
  m_choosePrjLabel->hide();
  m_settingsBox->setVisible(false);
}

//-----------------------------------------------------------------------------

void ProjectCreatePopup::createProject() {
  if (!IoCmd::saveSceneIfNeeded(QObject::tr("Create project"))) return;

  QFileInfo fi(m_nameFld->text());

  if (!isValidFileName(fi.baseName())) {
    error(
        tr("Project Name cannot be empty or contain any of the following "
           "characters:\n \\ / : * ? \" < > |"));
    return;
  }

  if (isReservedFileName_message(fi.baseName())) {
    return;
  }

  TProjectManager *pm   = TProjectManager::instance();
  TFilePath projectName = TFilePath(m_nameFld->text().toStdWString());
  if (projectName == TFilePath()) {
    return;
  }

  if (projectName.isAbsolute()) {
    error(tr("Bad project name: '%1' looks like an absolute file path")
              .arg(m_nameFld->text()));
    return;
  }

  // if (pm->getProjectPathByName(projectName) != TFilePath()) {
  //  error(tr("Project '%1' already exists").arg(m_nameFld->text()));
  //  // project already exists
  //  return;
  //}

  TFilePath newLocation      = TFilePath(m_projectLocationFld->getPath());
  std::string newLocStr      = newLocation.getQString().toStdString();
  TFilePath projectFolder    = newLocation + projectName;
  TFilePath projectPath      = pm->projectFolderToProjectPath(projectFolder);
  std::string projectPathStr = projectPath.getQString().toStdString();

  if (TSystem::doesExistFileOrLevel(projectPath)) {
    error(tr("Project '%1' already exists").arg(m_nameFld->text()));
    // project already exists
    return;
  }

  TProject *project = new TProject();
  updateProjectFromFields(project);
  TProjectP currentProject = pm->getCurrentProject();
  project->setSceneProperties(currentProject->getSceneProperties());
  try {
    bool isSaved = project->save(projectPath);
    if (!isSaved)
      DVGui::error(tr("It is not possible to create the %1 project.")
                       .arg(toQString(projectPath)));
  } catch (TSystemException se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return;
  }
  pm->setCurrentProjectPath(projectPath);
  IoCmd::newScene();
  DvDirModel::instance()->refreshFolder(projectFolder.getParentDir());
  accept();
}

//-----------------------------------------------------------------------------

void ProjectCreatePopup::showEvent(QShowEvent *) {
  int i;
  QString fldName;
  for (i = 0; i < m_folderFlds.size(); i++) {
    fldName = QString::fromStdString(m_folderFlds[i].first);
    m_folderFlds[i].second->setPath(fldName);
  }
/*
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    CheckBox *cb                = m_useScenePathCbs[i].second;
    bool signalesAlreadyBlocked = cb->blockSignals(true);
    cb->setChecked(false);
    cb->blockSignals(signalesAlreadyBlocked);
  }
*/
  m_useSubSceneCbs->blockSignals(true);
  m_useSubSceneCbs->setChecked(false);
  m_useSubSceneCbs->blockSignals(false);

  QString defaultProjectLocation =
      Preferences::instance()->getDefaultProjectPath();
  if (TSystem::doesExistFileOrLevel(TFilePath(defaultProjectLocation))) {
    m_projectLocationFld->setPath(defaultProjectLocation);
  }
  m_nameFld->setText("");
  m_nameAsLabel->setText("");

  QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  setSizePolicy(sizePolicy);
  setFixedSize(width(), height());
  setSizeGripEnabled(false);

  // default file path settings
  m_rulePreferenceBG->button(Rule_Standard)->setChecked(true);
  onRulePreferenceToggled(Rule_Standard);
  m_acceptNonAlphabetSuffixCB->setChecked(false);
  m_letterCountCombo->setCurrentIndex(m_letterCountCombo->findData(1));
}

void ProjectCreatePopup::setPath(QString path) {
  m_projectLocationFld->setPath(path);
}

//=============================================================================

ClickableProjectLabel::ClickableProjectLabel(const QString &text,
                                             QWidget *parent, Qt::WindowFlags f)
    : QLabel(text, parent, f) {
  m_path = text;
}

//-----------------------------------------------------------------------------

ClickableProjectLabel::~ClickableProjectLabel() {}

//-----------------------------------------------------------------------------

void ClickableProjectLabel::mouseReleaseEvent(QMouseEvent *event) {
  emit onMouseRelease(event);
}

//-----------------------------------------------------------------------------

void ClickableProjectLabel::enterEvent(QEvent *) {
  setStyleSheet("text-decoration: underline;");
}

//-----------------------------------------------------------------------------

void ClickableProjectLabel::leaveEvent(QEvent *) {
  setStyleSheet("text-decoration: none;");
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ProjectCreatePopup> openProjectCreatePopup(
    MI_NewProject);

//-----------------------------------------------------------------------------

class OpenRecentProjectCommandHandler final : public MenuItemHandler {
public:
  OpenRecentProjectCommandHandler() : MenuItemHandler(MI_OpenRecentProject) {}
  void execute() override {
    QAction *act = CommandManager::instance()->getAction(MI_OpenRecentProject);
    DVMenuAction *menu = dynamic_cast<DVMenuAction *>(act->menu());
    int index          = menu->getTriggeredActionIndex();

    TProjectManager *pm = TProjectManager::instance();
    TFilePath fp        = TFilePath(
        RecentFiles::instance()->getFilePath(index, RecentFiles::Project));

    if (!pm->isProject(fp)) {
      DVGui::warning(tr("Project could not be found."));
      return;
    }

    if (!IoCmd::saveSceneIfNeeded(QObject::tr("Open Recent Project"))) return;

    TFilePath path = pm->projectFolderToProjectPath(fp);

    if (path == pm->getCurrentProjectPath()) return;

    pm->setCurrentProjectPath(path);

    TProject *projectP =
        TProjectManager::instance()->getCurrentProject().getPointer();

    // In case the project file was upgraded to current version, save it now
    if (projectP->getProjectPath() != path) {
      projectP->save();
    }
    RecentFiles::instance()->addFilePath(fp.getQString(), RecentFiles::Project);
    IoCmd::newScene();
  }
} OpenRecentProjectCommandHandler;

//-----------------------------------------------------------------------------

class LoadProjectCommandHandler final : public MenuItemHandler {
public:
  LoadProjectCommandHandler() : MenuItemHandler(MI_LoadProject) {}
  void execute() override {
    TProjectManager *pm = TProjectManager::instance();
    TFilePath fp;

    static GenericLoadFilePopup *popup = 0;
    if (popup == 0) {
      popup = new GenericLoadFilePopup(QObject::tr("Open Project"));
      popup->setFileMode(true);
    }

    fp = popup->getPath();
    if (fp == TFilePath()) return;

    if (!pm->isProject(fp)) {
      DVGui::warning(
          tr("No project found at this location. Please select another "
             "location."));
      return;
    }

    if (!IoCmd::saveSceneIfNeeded(QObject::tr("Load Project"))) return;

    TFilePath path = pm->projectFolderToProjectPath(fp);

    if (path == pm->getCurrentProjectPath()) return;

    pm->setCurrentProjectPath(path);

    TProject *projectP =
        TProjectManager::instance()->getCurrentProject().getPointer();

    // In case the project file was upgraded to current version, save it now
    if (projectP->getProjectPath() != path) {
      projectP->save();
    }
    RecentFiles::instance()->addFilePath(fp.getQString(), RecentFiles::Project);
    IoCmd::newScene();
  }
} LoadProjectCommandHandler;
