

#include "projectpopup.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "filebrowser.h"
#include "iocommand.h"
#include "filebrowsermodel.h"
#include "dvdirtreeview.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/filefield.h"
#include "toonzqt/lineedit.h"
#include "toonzqt/checkbox.h"
#include "toonzqt/gutil.h"

// TnzCore includes
#include "tsystem.h"
#include "tenv.h"
#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"

// Qt includes
#include <QPushButton>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QMainWindow>
#include <QComboBox>
#include <QStandardPaths>

using namespace DVGui;

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

  m_choosePrjLabel = new QLabel(tr("Project:"), this);
  m_prjNameLabel   = new QLabel(tr("Project Name:"), this);
  m_pathFieldLabel = new QLabel(tr("Create Project In:"), this);
  m_nameFld        = new LineEdit();

  m_projectLocationFld =
      new DVGui::FileField(this, getDocumentsPath().getQString());

  m_nameFld->setMaximumHeight(WidgetHeight);

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

      upperLayout->addWidget(m_pathFieldLabel, 1, 0,
                             Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(m_projectLocationFld, 1, 1);
    }
    upperLayout->setColumnStretch(0, 0);
    upperLayout->setColumnStretch(1, 1);

    std::vector<std::string> folderNames;
    pm->getFolderNames(folderNames);
    int i;
    for (i = 0; i < (int)folderNames.size(); i++) {
      std::string name = folderNames[i];
      QString qName    = QString::fromStdString(name);
      FileField *ff    = new FileField(0, qName);
      m_folderFlds.append(qMakePair(name, ff));
      // upperLayout->addWidget(new QLabel("+" + qName, this), i + 2, 0,
      //                       Qt::AlignRight | Qt::AlignVCenter);
      upperLayout->addWidget(ff, i + 3, 1);
      ff->hide();
    }
    std::vector<std::tuple<QString, std::string>> cbs = {
        std::make_tuple(tr("Append $scenepath to +drawings"),
                        TProject::Drawings),
        std::make_tuple(tr("Append $scenepath to +inputs"), TProject::Inputs),
        std::make_tuple(tr("Append $scenepath to +extras"), TProject::Extras)};
    int currentRow = upperLayout->rowCount();

    for (int i = 0; i < cbs.size(); ++i) {
      auto const &name       = std::get<0>(cbs[i]);
      auto const &folderName = std::get<1>(cbs[i]);
      CheckBox *cb           = new CheckBox(name);
      cb->setMaximumHeight(WidgetHeight);
      upperLayout->addWidget(cb, currentRow + i, 1);
      m_useScenePathCbs.append(qMakePair(folderName, cb));
      cb->hide();
    }
    m_topLayout->addLayout(upperLayout);
  }

  pm->addListener(this);
}

//-----------------------------------------------------------------------------

void ProjectPopup::updateFieldsFromProject(TProject *project) {
  m_nameFld->setText(toQString(project->getName()));
  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    std::string folderName = m_folderFlds[i].first;
    TFilePath path         = project->getFolder(folderName);
    m_folderFlds[i].second->setPath(toQString(path));
  }
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    std::string folderName = m_useScenePathCbs[i].first;
    Qt::CheckState cbState =
        project->getUseScenePath(folderName) ? Qt::Checked : Qt::Unchecked;
    CheckBox *cb                = m_useScenePathCbs[i].second;
    bool signalesAlreadyBlocked = cb->blockSignals(true);
    cb->setCheckState(cbState);
    cb->blockSignals(signalesAlreadyBlocked);
  }
}

//-----------------------------------------------------------------------------

void ProjectPopup::updateProjectFromFields(TProject *project) {
  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    std::string folderName = m_folderFlds[i].first;
    TFilePath folderPath(m_folderFlds[i].second->getPath().toStdWString());
    project->setFolder(folderName, folderPath);
  }
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    std::string folderName = m_useScenePathCbs[i].first;
    int cbState            = m_useScenePathCbs[i].second->checkState();
    bool useScenePath      = cbState == Qt::Checked;
    project->setUseScenePath(folderName, cbState);
  }
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

//=============================================================================
/*! \class ProjectSettingsPopup
                \brief The ProjectSettingsPopup class provides a dialog to
   change current project settings.

                Inherits \b ProjectPopup.
*/
//-----------------------------------------------------------------------------

ProjectSettingsPopup::ProjectSettingsPopup() : ProjectPopup(false) {
  setWindowTitle(tr("Switch Project"));

  m_prjNameLabel->hide();
  m_nameFld->hide();
  m_choosePrjLabel->hide();
  m_pathFieldLabel->setText(tr("Project:"));

  int i;
  for (i = 0; i < m_folderFlds.size(); i++) {
    FileField *ff = m_folderFlds[i].second;
    connect(ff, SIGNAL(pathChanged()), this, SLOT(onFolderChanged()));
  }
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    CheckBox *cb = m_useScenePathCbs[i].second;
    connect(cb, SIGNAL(stateChanged(int)), this,
            SLOT(onUseSceneChekboxChanged(int)));
  }
  connect(m_projectLocationFld, &DVGui::FileField::pathChanged, this,
          &ProjectSettingsPopup::projectChanged);
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

void ProjectSettingsPopup::onFolderChanged() {
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

void ProjectSettingsPopup::onUseSceneChekboxChanged(int) {
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

  m_nameFld->setText("");

  m_projectLocationFld->blockSignals(true);
  m_projectLocationFld->setPath(TProjectManager::instance()
                                    ->getCurrentProjectPath()
                                    .getParentDir()
                                    .getQString());
  m_oldPath = TFilePath(m_projectLocationFld->getPath());
  m_projectLocationFld->blockSignals(false);

  resize(600, 75);
  QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  setSizePolicy(sizePolicy);
  setMinimumSize(QSize(600, 75));
  setMaximumSize(QSize(600, 75));
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
  m_choosePrjLabel->hide();
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
  for (i = 0; i < m_useScenePathCbs.size(); i++) {
    CheckBox *cb                = m_useScenePathCbs[i].second;
    bool signalesAlreadyBlocked = cb->blockSignals(true);
    cb->setChecked(false);
    cb->blockSignals(signalesAlreadyBlocked);
  }

  m_nameFld->setText("");

  resize(600, 150);
  QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  sizePolicy.setHorizontalStretch(0);
  sizePolicy.setVerticalStretch(0);
  setSizePolicy(sizePolicy);
  setMinimumSize(QSize(600, 150));
  setMaximumSize(QSize(600, 150));
  setFixedSize(width(), height());
  setSizeGripEnabled(false);
}

void ProjectCreatePopup::setPath(QString path) {
  m_projectLocationFld->setPath(path);
}

//-----------------------------------------------------------------------------

OpenPopupCommandHandler<ProjectCreatePopup> openProjectCreatePopup(
    MI_NewProject);
