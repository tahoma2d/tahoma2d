#pragma once

#ifndef PROJECTPOPUP_H
#define PROJECTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/tproject.h"
#include "filebrowsermodel.h"
#include <QList>

// forward declaration
namespace DVGui {
class FileField;
class LineEdit;
class CheckBox;
}

class QComboBox;

//=============================================================================
// ProjectPopup
//-----------------------------------------------------------------------------

class ProjectPopup : public DVGui::Dialog, public TProjectManager::Listener {
  Q_OBJECT

protected:
  DVGui::LineEdit *m_nameFld;
  QList<QPair<std::string, DVGui::FileField *>> m_folderFlds;
  QList<QPair<std::string, DVGui::CheckBox *>> m_useScenePathCbs;

  QLabel *m_prjNameLabel;
  QLabel *m_choosePrjLabel;
  QLabel *m_pathFieldLabel;

  DVGui::FileField *m_projectLocationFld;

public:
  ProjectPopup(bool isModal);
  // da TProjectManager::Listener
  void onProjectSwitched() override;
  // da TProjectManager::Listener
  void onProjectChanged() override {}

  void updateProjectFromFields(TProject *);
  void updateFieldsFromProject(TProject *);

protected:
  void showEvent(QShowEvent *) override;
};

//=============================================================================
// ProjectSettingsPopup
//-----------------------------------------------------------------------------

class ProjectSettingsPopup final : public ProjectPopup {
  Q_OBJECT

  TFilePath m_oldPath;

public:
  ProjectSettingsPopup();

public slots:
  void onFolderChanged();
  void onUseSceneChekboxChanged(int);
  void projectChanged();
  void onProjectChanged() override;

protected:
  void showEvent(QShowEvent *) override;
};

//=============================================================================
// ProjectCreatePopup
//-----------------------------------------------------------------------------

class ProjectCreatePopup final : public ProjectPopup {
  Q_OBJECT

public:
  ProjectCreatePopup();
  void setPath(QString path);

public slots:
  void createProject();

protected:
  void showEvent(QShowEvent *) override;
};

#endif  // PROJECTPOPUP_H
