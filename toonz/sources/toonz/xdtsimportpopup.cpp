#include "xdtsimportpopup.h"

#include "tapp.h"
#include "tsystem.h"
#include "toonzqt/filefield.h"
#include "toonz/toonzscene.h"

#include <QMainWindow>
#include <QTableView>
#include <QPushButton>
#include <QScrollArea>
#include <QGridLayout>
#include <QLabel>

using namespace DVGui;

XDTSImportPopup::XDTSImportPopup(QStringList levelNames, ToonzScene* scene,
                                 TFilePath scenePath)
    : m_scene(scene)
    , DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "XDTSImport") {
  setWindowTitle(tr("Importing XDTS file %1")
                     .arg(QString::fromStdString(scenePath.getLevelName())));
  QPushButton* loadButton   = new QPushButton(tr("Load"), this);
  QPushButton* cancelButton = new QPushButton(tr("Cancel"), this);

  QString description =
      tr("Please specify the level locations. Suggested paths "
         "are input in the fields with blue border.");

  m_topLayout->addWidget(new QLabel(description, this), 0);
  m_topLayout->addSpacing(15);

  QScrollArea* fieldsArea = new QScrollArea(this);
  fieldsArea->setWidgetResizable(true);

  QWidget* fieldsWidget = new QWidget(this);

  QGridLayout* fieldsLay = new QGridLayout();
  fieldsLay->setMargin(0);
  fieldsLay->setHorizontalSpacing(10);
  fieldsLay->setVerticalSpacing(10);
  fieldsLay->addWidget(new QLabel(tr("Level Name"), this), 0, 0,
                       Qt::AlignLeft | Qt::AlignVCenter);
  fieldsLay->addWidget(new QLabel(tr("Level Path"), this), 0, 1,
                       Qt::AlignLeft | Qt::AlignVCenter);
  for (QString& levelName : levelNames) {
    int row = fieldsLay->rowCount();
    fieldsLay->addWidget(new QLabel(levelName, this), row, 0,
                         Qt::AlignRight | Qt::AlignVCenter);
    FileField* fileField = new FileField(this);
    fieldsLay->addWidget(fileField, row, 1);
    m_fields.insert(levelName, fileField);
    fileField->setFileMode(QFileDialog::AnyFile);
    fileField->setObjectName("SuggestiveFileField");
    connect(fileField, SIGNAL(pathChanged()), this, SLOT(onPathChanged()));
  }
  fieldsLay->setColumnStretch(1, 1);
  fieldsLay->setRowStretch(fieldsLay->rowCount(), 1);

  fieldsWidget->setLayout(fieldsLay);
  fieldsArea->setWidget(fieldsWidget);
  m_topLayout->addWidget(fieldsArea, 1);

  connect(loadButton, SIGNAL(clicked()), this, SLOT(accept()));
  connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(loadButton, cancelButton);

  updateSuggestions(scenePath.getQString());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::onPathChanged() {
  FileField* fileField = dynamic_cast<FileField*>(sender());
  if (!fileField) return;
  QString levelName = m_fields.key(fileField);
  // make the field non-suggestive
  m_pathSuggestedLevels.removeAll(levelName);

  // if the path is specified under the sub-folder with the same name as the
  // level, then try to make suggestions from the parent folder of it
  TFilePath fp =
      m_scene->decodeFilePath(TFilePath(fileField->getPath())).getParentDir();
  if (QDir(fp.getQString()).dirName() == levelName)
    updateSuggestions(fp.getQString());

  updateSuggestions(fileField->getPath());
}

//-----------------------------------------------------------------------------

void XDTSImportPopup::updateSuggestions(const QString samplePath) {
  TFilePath fp(samplePath);
  fp = m_scene->decodeFilePath(fp).getParentDir();
  QDir suggestFolder(fp.getQString());
  QStringList filters;
  filters << "*.bmp"
          << "*.jpg"
          << "*.nol"
          << "*.pic"
          << "*.pict"
          << "*.pct"
          << "*.png"
          << "*.rgb"
          << "*.sgi"
          << "*.tga"
          << "*.tif"
          << "*.tiff"
          << "*.tlv"
          << "*.pli"
          << "*.psd";
  suggestFolder.setNameFilters(filters);
  suggestFolder.setFilter(QDir::Files);
  TFilePathSet pathSet;
  try {
    TSystem::readDirectory(pathSet, suggestFolder, true);
  } catch (...) {
    return;
  }

  QMap<QString, FileField*>::iterator fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    QString levelName    = fieldsItr.key();
    FileField* fileField = fieldsItr.value();
    // check if the field can be filled with suggestion
    if (fileField->getPath().isEmpty() ||
        m_pathSuggestedLevels.contains(levelName)) {
      // input suggestion if there is a file with the same level name
      bool found = false;
      for (TFilePath path : pathSet) {
        if (path.getName() == levelName.toStdString()) {
          TFilePath codedPath = m_scene->codeFilePath(path);
          fileField->setPath(codedPath.getQString());
          if (!m_pathSuggestedLevels.contains(levelName))
            m_pathSuggestedLevels.append(levelName);
          found = true;
          break;
        }
      }
      // Not found in the current folder.
      // Then check if there is a sub-folder with the same name as the level
      // (like foo/A/A.tlv), as CSP exports levels like that.
      if (!found && suggestFolder.cd(levelName)) {
        TFilePathSet subPathSet;
        try {
          TSystem::readDirectory(subPathSet, suggestFolder, true);
        } catch (...) {
          return;
        }
        for (TFilePath path : subPathSet) {
          if (path.getName() == levelName.toStdString()) {
            TFilePath codedPath = m_scene->codeFilePath(path);
            fileField->setPath(codedPath.getQString());
            if (!m_pathSuggestedLevels.contains(levelName))
              m_pathSuggestedLevels.append(levelName);
            break;
          }
        }
        // back to parent folder
        suggestFolder.cdUp();
      }
    }
    ++fieldsItr;
  }

  // repaint fields
  fieldsItr = m_fields.begin();
  while (fieldsItr != m_fields.end()) {
    if (m_pathSuggestedLevels.contains(fieldsItr.key()))
      fieldsItr.value()->setStyleSheet(
          QString("#SuggestiveFileField "
                  "QLineEdit{border-color:#2255aa;border-width:2px;}"));
    else
      fieldsItr.value()->setStyleSheet(QString(""));
    ++fieldsItr;
  }
}

QString XDTSImportPopup::getLevelPath(QString levelName) {
  FileField* field = m_fields.value(levelName);
  if (!field) return QString();
  return field->getPath();
}