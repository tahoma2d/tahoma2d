#pragma once
#ifndef XDTSIMPORTPOPUP_H
#define XDTSIMPORTPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"

#include <QMap>
namespace DVGui {
class FileField;
}
class ToonzScene;

class XDTSImportPopup : public DVGui::Dialog {
  Q_OBJECT
  QMap<QString, DVGui::FileField *> m_fields;
  QStringList m_pathSuggestedLevels;
  ToonzScene *m_scene;

  void updateSuggestions(const QString samplePath);

public:
  XDTSImportPopup(QStringList levelNames, ToonzScene *scene,
                  TFilePath scenePath);
  QString getLevelPath(QString levelName);
protected slots:
  void onPathChanged();
};

#endif