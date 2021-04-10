#pragma once

#ifndef PROJECTPOPUP_H
#define PROJECTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonz/tproject.h"
#include "filebrowsermodel.h"
#include <QList>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>

// forward declaration
namespace DVGui {
class FileField;
class LineEdit;
class CheckBox;
}

class QComboBox;
class QGridLayout;
class QGroupBox;

//=============================================================================
// ProjectPopup
//-----------------------------------------------------------------------------

class ProjectPopup : public DVGui::Dialog, public TProjectManager::Listener {
  Q_OBJECT

protected:
  DVGui::LineEdit *m_nameFld;
  QList<QPair<std::string, DVGui::FileField *>> m_folderFlds;
  //  QList<QPair<std::string, DVGui::CheckBox *>> m_useScenePathCbs;
  DVGui::CheckBox *m_useSubSceneCbs;

  QLabel *m_prjNameLabel;
  QLabel *m_choosePrjLabel;
  QLabel *m_pathFieldLabel;
  QLabel *m_nameAsLabel;

  DVGui::FileField *m_projectLocationFld;

  QLabel *m_settingsLabel;
  QFrame *m_settingsBox;
  QPushButton *m_showSettingsButton;

public:
  ProjectPopup(bool isModal);
  // da TProjectManager::Listener
  void onProjectSwitched() override;
  // da TProjectManager::Listener
  void onProjectChanged() override {}

  void updateProjectFromFields(TProject *);
  void updateFieldsFromProject(TProject *);

  QFrame *createSettingsBox();

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

// The ClickableLabel class is used to allow click and dragging
// on a label to change the value of a linked field
class ClickableProjectLabel : public QLabel {
  Q_OBJECT

  QString m_path;

protected:
  void mouseReleaseEvent(QMouseEvent *) override;
  void enterEvent(QEvent *) override;
  void leaveEvent(QEvent *) override;

public:
  ClickableProjectLabel(const QString &text, QWidget *parent = nullptr,
                        Qt::WindowFlags f = Qt::WindowFlags());
  ~ClickableProjectLabel();
  void setPath(QString path) { m_path = path; }
  QString getPath() { return m_path; }

signals:
  void onMouseRelease(QMouseEvent *event);
};

#endif  // PROJECTPOPUP_H
