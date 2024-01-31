#pragma once
#ifndef OCAIO_H
#define OCAIO_H

#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include "toonz/txshlevelhandle.h"

#include <QString>
#include <QList>
#include <QMap>

#include <QThread>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>

#define USE_ocaImportPopup

class ToonzScene;
class TXshCellColumn;
class TXsheet;
class TXshSimpleLevel;
class TFrameId;
class TFilePath;
class TOutputProperties;
class TXshLevelHandle;

namespace OCAIo {

struct OCAAsset {
  int width;
  int height;
  QString fileName;
  QString fileExt;
};

class OCAData {
protected:
  QString m_path;
  QString m_name;
  double m_framerate;
  int m_width, m_height;
  int m_startTime, m_endTime;
  double m_bgRed, m_bgGreen, m_bgBlue, m_bgAlpha;
  QJsonArray m_layers;
  int m_subId;
  QMap<QString, OCAAsset> m_assets;
  bool m_raEXR, m_veSVG;
  int m_stOff;

  DVGui::ProgressDialog *m_progressDialog;

public:
  void write(QJsonObject &json) const;
  bool isBlank(TXshCellColumn *column, int row);
  bool getLayerName(TXshCellColumn *column, QString &out);
  bool getCellName(TXshCellColumn *column, int row, QString &out);
  bool saveCell(TXshCellColumn *column, int row, const QString &cellname,
                OCAAsset &out);
  int frameLen(TXshCellColumn *column, const QList<int> &rows, int index);
  bool isGroup(TXshCellColumn *column);
  bool buildGroup(QJsonObject &json, const QList<int> &rows,
                  TXshCellColumn *column);
  bool buildLayer(QJsonObject &json, const QList<int> &rows,
                  TXshCellColumn *column);

  void setProgressDialog(DVGui::ProgressDialog *dialog);
  void build(ToonzScene *scene, TXsheet *xsheet, QString name, QString path,
             int startOffset, bool useEXR, bool vectorAsSVG);
  bool isEmpty() { return m_layers.isEmpty(); }
};

class OCAInputData : public OCAData {
  // json objects
  QString m_colorDepth;
  QString m_originApp;
  QString m_originAppVersion;
  QString m_ocaVersion;
  QJsonObject m_json;
  // toonz objects
  ToonzScene * m_scene;
  TXsheet *m_xsheet;
  TOutputProperties *m_oprop;
  TFilePath m_parentDir;
  // hardcoded...
  int m_dpi                 = 120; // this should match default m_dpi set in startuppopup.cpp@588 !?
  float m_antialiasSoftness = 0.0;
  bool m_whiteTransp        = true;
  bool m_doPremultiply      = true;

public:
  OCAInputData();
  //OCAInputData(float antialiasSoftness, bool whiteTransp, bool doPremultiply);
  bool load(QString path, QJsonObject &json);
  void getSceneData();
  //void readLayerList(const QJsonObject &json);
  void read(const QJsonObject &json, QMap<QString, int> importLayerMap);
  void setSceneData();
  void importOcaLayer(const QJsonObject &jsonLayer);
  void importOcaFrame(const QJsonObject &jsonFrame, TXshSimpleLevel *sl);
  void reset();
  QJsonObject &getJson() { return m_json; }
  TFilePath &getParentDir() { return m_parentDir; }
  ToonzScene *getScene() { return m_scene; }
};
}  // namespace OCAIo

#include "filebrowserpopup.h"

class ocaImportPopup : public CustomLoadFilePopup {
  Q_OBJECT

public:
  ocaImportPopup(OCAIo::OCAInputData *data);
  void rebuildCustomLayout(const TFilePath &fp);
  void removeCustomLayout();
  void onMergeStatusIndexChanged(QString layerName, int status);
  QMap<QString, int> &getImportLayerMap() { return m_importLayerMap; }

public slots:
  void onFileClicked(const TFilePath &);

private:
  //QWidget *m_customWidget; inherited from parent CustomLoadFilePopup
  //QGridLayout *m_customLayout; it would crash if not created on the fly
  QList<QComboBox *> levelMergeStatusComboList;
  QList<QLabel *> layerNameLabelList;
  QList<QLabel *> layerPathLabelList;
  OCAIo::OCAInputData *m_data;
  QMap<QString, int> m_importLayerMap;
};
#endif