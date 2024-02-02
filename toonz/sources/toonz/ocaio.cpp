#include "ocaio.h"

#include "tsystem.h"
#include "tenv.h"

#include "toonz/tcamera.h"

#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/levelset.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/preferences.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobject.h"
#include "toutputproperties.h"

#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

#include "tapp.h"
#include "menubarcommandids.h"
#include "filebrowserpopup.h"

#include "tiio.h"
#include "timage_io.h"
#include "tlevel_io.h"
#include "tvectorimage.h"
#include "exportlevelcommand.h"
#include "iocommand.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDesktopServices>
#include <QCoreApplication>

#include <QWidget>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QMessageBox>

using namespace OCAIo;

void OCAData::write(QJsonObject &json) const {
  // Header
  json["name"]       = m_name;
  json["frameRate"]  = m_framerate;
  json["width"]      = m_width;
  json["height"]     = m_height;
  json["startTime"]  = m_startTime + m_stOff;
  json["endTime"]    = m_endTime + m_stOff;
  json["colorDepth"] = m_raEXR ? "U16" : "U8";

  // Background color
  QJsonArray bgColorArray;
  bgColorArray.append(m_bgRed);
  bgColorArray.append(m_bgGreen);
  bgColorArray.append(m_bgBlue);
  bgColorArray.append(m_bgAlpha);
  json["backgroundColor"] = bgColorArray;

  // Layers
  json["layers"] = m_layers;

  // Versions
  json["originApp"] = "Tahoma2D";
  json["originAppVersion"] =
      QString::fromStdString(TEnv::getApplicationVersion());
  json["ocaVersion"] = "1.1.0";
}

int OCAData::frameLen(TXshCellColumn *column, const QList<int> &rows, int index) {
  // Next cells must match same level and frame-id
  int length          = 0;
  const TXshCell &stc = column->getCell(rows[index]);
  bool isStcStopFrame    = stc.getFrameId().isStopFrame();
  for (int i = index; i < rows.count(); i++) {
    int currentRow       = rows[i];
    const TXshCell &cell = column->getCell(currentRow);
    if ((isStcStopFrame && !cell.isEmpty() &&
         !cell.getFrameId().isStopFrame()) ||
        (!isStcStopFrame &&
         (stc.m_frameId != cell.m_frameId || stc.m_level != cell.m_level)))
      break;
    length++;
  }
  return length;
}

bool OCAData::isBlank(TXshCellColumn *column, int row) {
  TXshCell cell = column->getCell(row);
  return cell.isEmpty() || cell.getFrameId().isStopFrame();
}

bool OCAData::getLayerName(TXshCellColumn *column, QString &out) {
  // Layer name will be the first cel that occur on the column
  TXshCell cell = column->getCell(column->getFirstRow());
  if (cell.isEmpty() || cell.getFrameId().isStopFrame()) return false;

  out = QString::fromStdWString(cell.m_level->getName());
  return true;
}

bool OCAData::getCellName(TXshCellColumn *column, int row, QString &out) {
  TXshCell cell = column->getCell(row);
  if (cell.isEmpty() || cell.getFrameId().isStopFrame()) return false;

  TFilePath fp(cell.m_level->getName());
  fp = fp.withFrame(cell.getFrameId(), TFrameId::FrameFormat::FOUR_ZEROS);
  out = fp.getQString();
  return true;
 }

bool OCAData::saveCell(TXshCellColumn *column, int row,
                        const QString &cellname, OCAAsset &out) {
  TXshCell cell = column->getCell(row);
  TXshSimpleLevel *sl = cell.getSimpleLevel();

  std::string format = m_raEXR ? "exr" : "png";

  // Rasterize cell
  TImageP image = IoCmd::exportedImage(format, *sl, cell.getFrameId());
  if (!image) return false;

  if (TRasterImageP ri = (TRasterImageP)(image)) {
    TRasterP raster;
    raster = ri->getRaster();
    out.width  = raster->getLx();
    out.height = raster->getLy();
    if (!raster) return false;
  } else {
    return false;
  }

  out.fileExt = QString::fromStdString(format);

  if (m_veSVG) {
    // Get vector image directly and discard rasterized image
    TVectorImageP vectorImg = cell.getImage(false);
    if (vectorImg) {
      image       = vectorImg;
      out.fileExt = "svg";
    }
  }

  QString filename = QString("/%1.%2").arg(cellname, out.fileExt);
  TFilePath fpr(m_path);
  TFilePath fpa(m_path + filename);
  out.fileName = QString::fromStdWString(fpr.getWideName()) + filename;

  TLevelWriterP lw(fpa);
  lw->setFrameRate(m_framerate);
  TImageWriterP iw = lw->getFrameWriter(cell.getFrameId());
  iw->setFilePath(fpa); // Added to aid my own sanity!
  iw->save(image);

  return true;
}

bool OCAData::isGroup(TXshCellColumn *column) {
  TXshCell firstcell = column->getCell(column->getFirstRow());
  TXshChildLevel *cl = firstcell.getChildLevel();
  return cl;
}

bool OCAData::buildGroup(QJsonObject &json, const QList<int> &rows,
                         TXshCellColumn *column) {
  QString layername;
  if (!getLayerName(column, layername)) return false;

  // Get sub-xsheet
  int firstrow       = column->getFirstRow();
  TXshCell firstcell = column->getCell(firstrow);
  TXshChildLevel *cl = firstcell.getChildLevel();
  TXsheet *xsheet = cl->getXsheet();
  if (!xsheet) return false;

  // Build a list of child rows
  QList<int> crows;
  for (int i = m_startTime; i <= m_endTime; i++) {
    TXshCell cell = column->getCell(i);
    if (cell.isEmpty() || cell.getFrameId().isStopFrame())
      crows.append(-1);
    else
      crows.append(cell.getFrameId().getNumber() - 1);
  }

  // Build all columns from sub-xsheet
  int subID = ++m_subId;
  QJsonArray layers;
  for (int col = 0; col < xsheet->getFirstFreeColumnIndex(); col++) {
    if (xsheet->isColumnEmpty(col)) continue;
    TXshCellColumn *column = xsheet->getColumn(col)->getCellColumn();
    if (!column) continue;                      // skip non-cell column
    if (!column->isPreviewVisible()) continue;  // skip inactive column

    if (column->getColumnType() == column->eLevelType) {
      QJsonObject json;
      if (isGroup(column)) {
        if (buildGroup(json, crows, column)) layers.append(json);
      } else {
        if (buildLayer(json, crows, column)) layers.append(json);
      }
    }
  }

  QJsonArray jsonBlankPos;
  jsonBlankPos.append(m_width / 2);
  jsonBlankPos.append(m_height / 2);
  json["name"]         = layername;
  json["position"]     = jsonBlankPos;
  json["width"]        = m_width;
  json["height"]       = m_height;
  json["frames"]       = QJsonArray();
  json["childLayers"]  = layers;
  json["type"]         = "grouplayer";
  json["fileType"]     = "png";
  json["blendingMode"] = "normal";
  json["animated"]     = false;
  json["label"]        = 0;
  json["opacity"]      = column->getOpacity() / 255.0;
  json["visible"]      = column->isCamstandVisible();
  json["passThrough"]  = false;
  json["reference"]    = false;
  json["inheritAlpha"] = false;

  return true;
}

bool OCAData::buildLayer(QJsonObject &json, const QList<int> &rows,
                         TXshCellColumn *column) {
  QJsonArray jsonFrames;
  bool firstcel = true;

  // Layer name will be the first cel that occur on the column
  int firstrow = column->getFirstRow();
  QString layername;
  QString cellname;
  if (!getLayerName(column, layername)) return false;
  if (!getCellName(column, firstrow, cellname)) return false;

  QJsonArray jsonBlankPos;
  jsonBlankPos.append(m_width / 2);
  jsonBlankPos.append(m_height / 2);
  json["name"]     = layername;
  json["position"] = jsonBlankPos;
  json["width"]    = 0;
  json["height"]   = 0;

  for (int i = 0; i < rows.count(); i++) {
    int row = rows[i];

    QJsonObject frame;
    QJsonArray jsonPosition;
    jsonPosition.append(m_width / 2);
    jsonPosition.append(m_height / 2);
    int len = frameLen(column, rows, i);

    frame["frameNumber"] = i + m_stOff;
    frame["position"]    = jsonPosition;
    frame["opacity"]     = 1.0;  // OT uses Transparency Fx node
    frame["duration"]    = len;

    if (isBlank(column, row)) {
      frame["name"]        = "_blank";
      frame["fileName"]    = "";
      frame["width"]       = 0;
      frame["height"]      = 0;
    } else {
      // Cell name will be used for the frame name
      if (!getCellName(column, row, cellname)) continue;

      // Save cell image as an asset
      OCAAsset asset;
      if (!m_assets.contains(cellname)) {
        if (!saveCell(column, row, cellname, asset)) return false;
        m_assets.insert(cellname, asset);
      } else {
        asset = m_assets.value(cellname);
      }

      frame["name"]     = cellname;
      frame["fileName"] = asset.fileName;
      frame["width"]    = asset.width;
      frame["height"]   = asset.height;

      if (firstcel) {
        // Layer size and type will depend of the first cell too
        json["width"]    = asset.width;
        json["height"]   = asset.height;
        json["fileType"] = asset.fileExt;
        firstcel         = false;
      }
    }
    jsonFrames.append(frame);

    i += len - 1;
  }

  json["frames"]       = jsonFrames;
  json["childLayers"]  = QJsonArray();
  json["type"]         = "paintlayer";
  json["blendingMode"] = "normal";  // OT uses nodes Fx to make blending
                                    // possible, how to approach this?
  json["animated"]     = jsonFrames.count() >= 2;
  json["label"]        = 0;  // OT doesn't support labels
  json["opacity"]      = column->getOpacity() / 255.0;
  json["visible"]      = column->isCamstandVisible();
  json["passThrough"]  = false;
  json["reference"]    = false;
  json["inheritAlpha"] = false;

  return true;
}

void OCAData::setProgressDialog(DVGui::ProgressDialog *dialog) {
  m_progressDialog = dialog;
}

void OCAData::build(ToonzScene *scene, TXsheet *xsheet, QString name,
                    QString path, int startOffset, bool useEXR,
                    bool vectorAsSVG) {
  m_name  = name;
  m_path  = path;
  m_subId = 0;
  m_raEXR = useEXR;
  m_veSVG = vectorAsSVG;
  m_stOff = startOffset;

  // if the current xsheet is top xsheet in the scene and the output
  // frame range is specified, set the "to" frame value as duration
  TOutputProperties *oprop = scene->getProperties()->getOutputProperties();
  int from, to, step;
  if (scene->getTopXsheet() == xsheet && oprop->getRange(from, to, step)) {
    m_startTime = from;
    m_endTime   = to;
    //m_stepTime  = step;
  } else {
    m_startTime = 0;
    m_endTime   = xsheet->getFrameCount() - 1;
    //m_stepTime  = 1;
  }
  if (m_endTime < 0) m_endTime = 0;

  // Build a list of rows
  QList<int> rows;
  for (int i = m_startTime; i <= m_endTime; i++) {
    rows.append(i);
  }

  m_framerate = oprop->getFrameRate();
  m_width     = scene->getCurrentCamera()->getRes().lx;
  m_height    = scene->getCurrentCamera()->getRes().ly;

  TPixel bgc = scene->getProperties()->getBgColor();
  m_bgRed    = bgc.r / double(TPixel::maxChannelValue);
  m_bgGreen  = bgc.g / double(TPixel::maxChannelValue);
  m_bgBlue   = bgc.b / double(TPixel::maxChannelValue);
  m_bgAlpha  = bgc.m / double(TPixel::maxChannelValue);

  // Build all columns from current xsheet
  m_layers.empty();
  for (int col = 0; col < xsheet->getColumnCount(); col++) {
    if (xsheet->isColumnEmpty(col)) continue;
    TXshCellColumn *column = xsheet->getColumn(col)->getCellColumn();
    if (!column) continue;  // skip non-cell column
    if (!column->isPreviewVisible()) continue;  // skip inactive column

    if (column->getColumnType() == column->eLevelType) {
      if (m_progressDialog) {
        QString layername;
        getLayerName(column, layername);
        m_progressDialog->setLabelText(QObject::tr("Layer: ") + layername);
        m_progressDialog->setMaximum(xsheet->getColumnCount());
        m_progressDialog->setValue(col);
        QCoreApplication::processEvents();
      }

      QJsonObject json;
      if (isGroup(column)) {
        if (buildGroup(json, rows, column)) m_layers.append(json);
      } else {
        if (buildLayer(json, rows, column)) m_layers.append(json);
      }
    }
  }
}

class ExportOCACommand final : public MenuItemHandler {
public:
  ExportOCACommand() : MenuItemHandler(MI_ExportOCA) {}
  void execute() override;
} exportOCACommand;

void ExportOCACommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *xsheet   = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp      = scene->getScenePath().withType("oca");

  static GenericSaveFilePopup *savePopup = 0;
  static QCheckBox *exrImageFormat       = nullptr;
  static QCheckBox *rasVectors           = nullptr;
  static QCheckBox *doKeyframing         = nullptr;
  static QHBoxLayout *startingOffsetLay  = nullptr;
  static QLabel *startingOffsetLab       = nullptr;
  static QSpinBox *startingOffsetSpin    = nullptr;

  static DVGui::ProgressDialog *progressDialog = nullptr;

  if (!savePopup) {
    QWidget *customWidget = new QWidget();

    exrImageFormat = new QCheckBox(tr("Save Images in EXR Format"));
    rasVectors     = new QCheckBox(tr("Rasterize Vectors"));

    startingOffsetLay  = new QHBoxLayout();
    startingOffsetLab  = new QLabel(tr("Frame Offset: "));
    startingOffsetSpin = new QSpinBox();

    exrImageFormat->setToolTip(
        tr("Checked: Images are saved as EXR\nUnchecked: Images are "
           "saved as PNG"));
    rasVectors->setToolTip(
        tr("Checked: Rasterize into EXR/PNG\nUnchecked: Vectors are "
           "saved as SVG"));
    startingOffsetLab->setToolTip(tr("Starting Frame Offset"));
    startingOffsetSpin->setToolTip(tr("Starting Frame Offset"));
    rasVectors->setChecked(true);

    startingOffsetSpin->setValue(1);
    startingOffsetSpin->setMinimum(0);
    startingOffsetSpin->setMaximum(1000000);
    startingOffsetLab->adjustSize();
    startingOffsetLay->setMargin(0);
    startingOffsetLay->setSpacing(0);
    startingOffsetLay->addWidget(startingOffsetLab, 0, Qt::AlignRight);
    startingOffsetLay->addWidget(startingOffsetSpin);

    QGridLayout *customLay = new QGridLayout();
    customLay->setMargin(5);
    customLay->setSpacing(5);
    customLay->addWidget(exrImageFormat, 0, 0);
    customLay->addWidget(rasVectors, 1, 0);
    customLay->addLayout(startingOffsetLay, 0, 1);
    customWidget->setLayout(customLay);

    progressDialog = new DVGui::ProgressDialog("", tr("Hide"), 0, 0);
    progressDialog->setWindowTitle(tr("Exporting..."));
    progressDialog->setWindowFlags(
        Qt::Dialog | Qt::WindowTitleHint);  // Don't show ? and X buttons
    progressDialog->setWindowModality(Qt::WindowModal);

    savePopup = new GenericSaveFilePopup(
        QObject::tr("Export Open Cel Animation (OCA)"), customWidget);
    savePopup->addFilterType("oca");
  }
  if (!scene->isUntitled())
    savePopup->setFolder(fp.getParentDir());
  else
    savePopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());

  savePopup->setFilename(fp.withoutParentDir());
  fp = savePopup->getPath();
  if (fp.isEmpty()) return;

  int frameOffset  = startingOffsetSpin->value();
  bool exrImageFmt = exrImageFormat->isChecked();
  bool rasterVecs  = rasVectors->isChecked();

  // Export

  progressDialog->setLabelText(tr("Starting..."));
  progressDialog->setValue(0);
  progressDialog->show();
  QCoreApplication::processEvents();

  QString ocafile = fp.getQString();
  QString ocafolder(ocafile);
  ocafolder.replace(".", "_");

  QFile saveFile(ocafile);
  QDir saveDir(ocafolder);

  if (!saveFile.open(QIODevice::WriteOnly)) {
    progressDialog->close();
    DVGui::error(QObject::tr("Unable to open OCA file for saving."));
    return;
  }
  if (!saveDir.exists()) {
    if (!saveDir.mkpath(".")) {
      progressDialog->close();
      DVGui::error(QObject::tr("Unable to create folder for saving layers."));
      return;
    }
  }

  OCAData ocaData;
  ocaData.setProgressDialog(progressDialog);
  ocaData.build(scene, xsheet, QString::fromStdString(fp.getName()), ocafolder,
                frameOffset, exrImageFmt, !rasterVecs);
  if (ocaData.isEmpty()) {
    progressDialog->close();
    DVGui::error(QObject::tr("No columns can be exported."));
    return;
  }

  QJsonObject ocaObject;
  ocaData.write(ocaObject);
  QJsonDocument saveDoc(ocaObject);
  QByteArray jsonByteArrayData = saveDoc.toJson();
  saveFile.write(jsonByteArrayData);

  progressDialog->close();

  // Done

  QString str = QObject::tr("%1 has been exported successfully.")
                    .arg(QString::fromStdString(fp.getLevelName()));

  std::vector<QString> buttons = {QObject::tr("OK"),
                                  QObject::tr("Open containing folder")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, str, buttons);

  if (ret == 2) {
    TFilePath folderPath = fp.getParentDir();
    if (TSystem::isUNC(folderPath))
      QDesktopServices::openUrl(QUrl(folderPath.getQString()));
    else
      QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath.getQString()));
  }
}

/// <summary>
///  OCA importer
/// 
/// DONE:
/// - add the option to update the current scene from OCA, importing only the
/// missing layers (example : import a krita paint layer, but keep the tlv with
/// their palette as smart rasters...)
/// - progress bar
/// - clearLayout when oca importer is re-opened.
/// /// - We should be skipping "_blank" frames completely not creating empty image frames.
/// - copy cells based on duration if !Preferences::instance()->isImplicitHoldEnabled()
/// - expose per layer options in ui : whiteTransp / doPremultiply
/// colorSpaceGamma (antialiasSoftness / dpi (currently hardcoded or using
/// preference if any) not exposed)
/// - FIXED level settings telling that images are in extra folder (this happens
/// when the level path is not valid) - This was causing a crash when saving scene later (copyFile failed...) : 
/// find first real frame (with non empty file path)
/// - avoid duplicate names with load mergingStatus : increment imported level's
/// name when necessary (and column's name too...)
/// - import child layers as normal layers, but popup message at the end of the import to notify the user that the layer's groups have been ignored.
/// - ocaImportPopup (selective import ui) : fix the case of no filename or folder filename name in your test/screenshot (Attempting to load my OCA exported test scene, this UI appears bit buggy and cumbersome:).
/// - Insert stop frames after the last frame + its duration (stop frames are actually already there as "_blank" frames)

/// TODO:
/// - ocaImportPopup: fold import options by default (see ex: filebrowserpopup.cpp showSubsequenceButton)
/// - add "replace","merge" to mergingStatus
/// merge would add only missing frames ?
/// replace would avoid duplicate (but load would still be usefull to compare
/// and make changes manually)
/// - avoid replacing tzp levels (using filename comparison)
/// while TZP_XSHLEVEL is for toonz levels (smart rasters with palette)
/// For roundtrip / scene sharing between softwares,
/// Smart raster TZP_XSHLEVEL cannot be recreated from raster
/// OVL_XSHLEVEL, without palette loss.
/// The comparison to set levelMergeStatusCombo only uses layer names for
/// now...
/// Look at how the column command Collapse works. This command creates
/// childLevels and adds levels into it.
/// - keyframe animation transfert for camera, pegs, etc...
/// that would be another tool and another open file format (ascii (json or
/// xml?) or binary (faster for huge scenes?) ?).
/// - blendingMode (implement blendingMode in viewport with fbo pingpong ? + ui
/// ComboBoxes on column headers ?)
/// - OCA doesn't support frame tags (NavigationTags)
/// this would be very usefull to mark keys / breakdowns / inbetweens / etc
/// drawing ... for the onion skining and fliping
/// Use OCA metadata to transfert frame tags ?
/// - increment column names on duplicate columns
/// - frame position: import from OCA until glTF format support is added
/// 
/// </summary>


class ImportOCACommand final : public MenuItemHandler {
public:
  ImportOCACommand() : MenuItemHandler(MI_ImportOCA) {}
  void execute() override;
} ImportOCACommand;

void ImportOCACommand::execute() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  TXsheet *xsheet   = TApp::instance()->getCurrentXsheet()->getXsheet();
  TFilePath fp      = scene->getScenePath().withType("oca");
  static OCAInputData *data;
  static ocaImportPopup *loadPopup;
  static DVGui::ProgressDialog *progressDialog;
  // ocaImportPopup let us decide which layer to import
  if (data == nullptr || loadPopup == nullptr || progressDialog == nullptr) {
    data        = new OCAInputData();
    loadPopup = new ocaImportPopup(data);
    progressDialog = new DVGui::ProgressDialog("", tr("Hide"), 0, 0);
    //DVGui::info(QObject::tr("new ocaImportPopup"));
  }
  loadPopup->addFilterType("oca");
  if (!scene->isUntitled())
    loadPopup->setFolder(fp.getParentDir());
  else
    loadPopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());
  loadPopup->setFilename(fp.withoutParentDir());
  loadPopup->show();

  // import the oca file
  fp = loadPopup->getPath();
  if (fp.isEmpty()) {
    DVGui::info(QObject::tr("Open OCA file cancelled : empty filepath."));
    return;
  } else {
    DVGui::info(QObject::tr("Open OCA file : ") + fp.getQString());
  }
  if (data == 0) {
    DVGui::error(QObject::tr("ImportOCACommand::execute: no data"));
    return;
  }
  QJsonObject ocaObject;
  data->setProgressDialog(progressDialog);
  if (!data->load(fp.getQString(), ocaObject)) {
    DVGui::warning(QObject::tr("Failed to load OCA file: ") + fp.getQString());
    return;
  }
  progressDialog->setWindowTitle(tr("Importing..."));
  progressDialog->setWindowFlags(
      Qt::Dialog | Qt::WindowTitleHint);  // Don't show ? and X buttons
  progressDialog->setWindowModality(Qt::WindowModal);
  progressDialog->setLabelText(tr("Starting..."));
  progressDialog->setValue(0);
  progressDialog->show();
  QCoreApplication::processEvents();

  data->getSceneData();  // gather default values
  data->read(ocaObject, loadPopup->getImportLayerMap(),
             loadPopup->getImportOptionMap());  // parser
  data->setSceneData();   // set scene/xsheet values
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
  progressDialog->close();
  if (!data->getMessage().isEmpty())
    QMessageBox::warning(0, "OCA import", data->getMessage());
}

OCAIo::OCAInputData::OCAInputData() {
}

bool OCAIo::OCAInputData::load(QString path, QJsonObject &json) { 
  QFileInfo fi(path);
  if (fi.isDir()) return false;
  if (TFilePath(path).getType() != "oca") return false;
  m_scene     = TApp::instance()->getCurrentScene()->getScene();
  m_xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_oprop  = m_scene->getProperties()->getOutputProperties();
  m_path      = path;
  m_parentDir = TFilePath(m_path).getParentDir();
  // 1. Open the QFile, read it in a byteArray and close the file
  QFile file;
  file.setFileName(path);
  if (!file.open(QIODevice::ReadOnly)) {
    DVGui::error(QObject::tr("Unable to open OCA file for loading."));
    return false;
  } else {
    DVGui::info(QObject::tr("Loading : ") + path);
  }
  QByteArray byteArray;
  byteArray = file.readAll();
  file.close();

  // 2. Format the content of the byteArray as QJsonDocument and check on parse Errors
  QJsonParseError parseError;
  QJsonDocument jsonDoc;
  jsonDoc = QJsonDocument::fromJson(byteArray, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    DVGui::warning(QObject::tr("Parse error at %1 while loading OCA file.")
                       .arg(parseError.offset));
    return false;
  }
  json = jsonDoc.object();
  m_json = json;
  return true;
}

/// <summary>
/// Gather default values from current scene, 
/// just in case they are missing in the oca file...
/// </summary>
void OCAIo::OCAInputData::getSceneData() {
  m_framerate = m_oprop->getFrameRate();
  int from, to, step;
  if (m_scene->getTopXsheet() == m_xsheet &&
      m_oprop->getRange(from, to, step)) {
    m_startTime = from;
    m_endTime   = to;
  } else {
    m_startTime = 0;
    m_endTime   = m_xsheet->getFrameCount() - 1;
  }
  if (m_endTime < 0) m_endTime = 0;
  m_width  = m_scene->getCurrentCamera()->getRes().lx;
  m_height = m_scene->getCurrentCamera()->getRes().ly;
}

/// <summary>
/// json OCA parser, getting global properties and import the layers
/// </summary>
/// <param name="json"></param>
void OCAIo::OCAInputData::read(const QJsonObject &json,
                               QMap<QString, int> importLayerMap,
                               QMap<QString, LevelOptions> importOptionMap) {
  m_originApp        = json.value("originApp").toString();
  m_originAppVersion = json.value("originAppVersion").toString();
  m_ocaVersion       = json.value("ocaVersion").toString();
  m_name             = json.value("name").toString();
  m_framerate  = json.value("frameRate").toInt(m_framerate);
  m_width      = json.value("width").toInt(m_width);
  m_height     = json.value("height").toInt(m_height);
  m_startTime  = json.value("startTime").toInt(m_startTime);
  m_endTime    = json.value("endTime").toInt(m_endTime);
  m_colorDepth = json.value("colorDepth").toString();
  QJsonArray bgColorArray = json["backgroundColor"].toArray();
  m_bgRed                 = bgColorArray[0].toDouble();
  m_bgGreen               = bgColorArray[1].toDouble();
  m_bgBlue                = bgColorArray[2].toDouble();
  m_bgAlpha               = bgColorArray[3].toDouble();
  m_layers                = json.value("layers").toArray();
  auto flattenLayers      = getFlattenLayers(m_layers);

  int col = 0;
  for (auto layer : flattenLayers) {
    auto jsonLayer = layer.toObject();
    QString layername = jsonLayer["name"].toString();
    if (m_progressDialog) {
      m_progressDialog->setLabelText(QObject::tr("Layer: ") + layername);
      m_progressDialog->setMaximum(importLayerMap.size());
    }
    if (importLayerMap.contains(layername)) {
      switch (importLayerMap[layername]) { 
      case 0:
        DVGui::info(QObject::tr("OCA importing layer : ") + layername);
        importOcaLayer(jsonLayer, importOptionMap);
        if (parentMap.contains(jsonLayer["name"].toString())) {
          m_message += "child layer " + jsonLayer["name"].toString() +
                       " was imported but not grouped in " +
                       parentMap[jsonLayer["name"].toString()] + "\n";
        }
        m_progressDialog->setValue(col);

        break;
      case 1:
        DVGui::info(QObject::tr("OCA skipping layer : ") + layername);
        break;
      }
    } else {
      // no importLayerMap...
      //importOcaLayer(jsonLayer, importOptionMap);
    }
    col++;
  }
}

/// <summary>
/// Updates global scene properties.
/// </summary>
void OCAIo::OCAInputData::setSceneData() {
  m_scene->setSceneName(m_name.toStdWString());
  m_oprop->setFrameRate(m_framerate);
  auto resolution = TDimension(m_width, m_height);
  m_scene->getCurrentCamera()->setRes(resolution);
  if (m_dpi > 0) {
    TDimensionD size(0, 0);
    size.lx = resolution.lx / m_dpi;
    size.ly = resolution.ly / m_dpi;
    m_scene->getCurrentCamera()->setSize(size, false, false);
  }
  m_xsheet->updateFrameCount();
  m_oprop->setRange(m_startTime, m_endTime, 1);
  TPixel32 color = TPixel32(m_bgRed * 255.0, m_bgGreen * 255.0,
                            m_bgBlue * 255.0, m_bgAlpha * 255.0);
  m_scene->getProperties()->setBgColor(color);
  // https://github.com/RxLaboratory/OCA/blob/master/src-docs/docs/specs/color-depth.md
  // only raises nonlinearBpp, never make it lower than it is... (the levels could be 8 bit but comp output higher)
  int nonlinearBpp = m_oprop->getNonlinearBpp();
  if (m_colorDepth == "U8" && nonlinearBpp < 32)
    m_oprop->setNonlinearBpp(32);
  else if (m_colorDepth == "U16" && nonlinearBpp < 64)
    m_oprop->setNonlinearBpp(64);
  else if (m_colorDepth == "F16" && nonlinearBpp < 64)
    m_oprop->setNonlinearBpp(64);
  else if (m_colorDepth == "U32" && nonlinearBpp < 128)
    m_oprop->setNonlinearBpp(128);
}

/// <summary>
/// creates level from OCA layer, and exposes it in the xsheet
/// see OCA layer specs https://github.com/RxLaboratory/OCA/blob/master/src-docs/docs/specs/layer.md?plain=1
/// - only raster levels are supported
/// - blendingMode / animated are not used
/// - OCA doesn't support frame tags (NavigationTags) 
/// this would be very usefull to mark keys / breakdowns / inbetweens / etc drawing ... 
/// for the onion skining and fliping
/// - recursive layers (layer groups) have not neen tested 
/// (not sure if and how it could translate in toonz ? see CHILD_XSHLEVEL ?)
/// 
/// </summary>
/// <param name="jsonLayer"></param>
void OCAIo::OCAInputData::importOcaLayer(
    const QJsonObject &jsonLayer, QMap<QString, LevelOptions> importOptionMap) {
  if (jsonLayer["type"] == "paintlayer" || jsonLayer["type"] == "grouplayer") {
    if (jsonLayer["blendingMode"].toString() != "normal") {
      DVGui::warning(QObject::tr("importOcaLayer : blending modes not implemented ") +
                  jsonLayer["name"].toString());
    }
    int firstRealFrameIndex = findFirstRealFrameIndex(jsonLayer);
    if (firstRealFrameIndex == -1) {
      DVGui::warning(
          QObject::tr("importOcaLayer : layer ") +
                     jsonLayer["name"].toString() + " has not image frame to import : skipping...");
      return;
    }
    //DVGui::info(QObject::tr("importOcaLayer : ") +
    //            jsonLayer["name"].toString());
    auto levelType   = OVL_XSHLEVEL; // OVL_XSHLEVEL is for raster images,
    // auto increment level and column name...
    auto layerName = jsonLayer["name"].toString();
    auto levelSet  = getScene()->getLevelSet();
    while (levelSet->hasLevel(layerName.toStdWString())) {
      if (layerName.indexOf("_") == -1) {
        layerName += "_1";
      } else {
        auto tokens = layerName.split("_");
        auto numStr = tokens.back();
        bool ok;
        int version = numStr.toInt(&ok);
        if (ok)
          layerName = layerName + "_" + QString::number(++version);
        else
          layerName += "_1";
      }
    }

    //  I am using my own preference m_dpi / m_antialiasSoftness / m_whiteTransp / m_doPremultiply
    auto resolution  = TDimension(m_width, m_height);
    // problem : ToonzScene::createNewLevel use LevelName for building level's
    // filepath : change the name later with level->setName ! not here...
    TXshLevel *level = m_scene->createNewLevel(
        levelType, jsonLayer["name"].toString().toStdWString(), resolution,
        m_dpi);

    TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(level);
    // TXshLevel *ToonzScene::createNewLevel(int type, std::wstring levelName, const TDimension &dim, double dpi, TFilePath fp) 
    // ignores the resolution argument !
    sl->getProperties()->setImageRes(resolution);


    auto frames = jsonLayer["frames"].toArray();
    //bool formatSpecified = false;
    LevelProperties *lp  = sl->getProperties();
    if (frames.size() > 0 && firstRealFrameIndex != -1) {
      auto jsonFrame = frames[firstRealFrameIndex].toObject();
      auto levelPath = TFilePath(m_parentDir.getQString() + "/" +
                            jsonFrame["fileName"].toString());
      sl->setPath(levelPath, true); // attempts to fix /extras folder bug
    } 
    // else sl->setPath(TFilePath ("")); will never happen
    if (importOptionMap.contains(jsonLayer["name"].toString())) {
    //if (importOptionMap.size()>0) {
      auto option = importOptionMap[jsonLayer["name"].toString()];
      sl->getProperties()->setWhiteTransp(option.m_whiteTransp);
      sl->getProperties()->setDoPremultiply(option.m_premultiply);
      sl->getProperties()->setDoAntialias(option.m_antialias);
      sl->getProperties()->setDpi(option.m_dpi);
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
      // sl->getProperties()->setImageDpi(lp->getImageDpi()); //?
      sl->getProperties()->setSubsampling(option.m_subsampling);
      sl->getProperties()->setColorSpaceGamma(option.m_colorSpaceGamma);
    } else {
      sl->getProperties()->setWhiteTransp(m_whiteTransp);
      sl->getProperties()->setDoPremultiply(m_doPremultiply);
      sl->getProperties()->setDoAntialias(m_antialiasSoftness);
      // we filmmakers don't use dpi ??
      sl->getProperties()->setDpi(m_dpi);
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    }

    // float colorSpaceGamma   = 2.2;
    // display gamma 2.2 is only valid for srgb color space, and for rec709 is
    // grading on a computer monitor...but... gamma for rec709 should be 2.4 if
    // you have a rec709 grading monitor set for HDTV, gamma for rec2020 should
    // be 2.6 if you have a rec2020 grading monitor, gamma for DCI-P3 should
    // be 2.4 if you have a DCI-P3 grading theatre !? linear images colorDepth
    // could be could be F16 or F32 (but the channel
    // width preference is locked to 32 bit if checking linear color space...) 
    //if (sl->getPath().getType() == "exr")
    //    sl->getProperties()->setColorSpaceGamma(colorSpaceGamma); 
    // Why is colorSpaceGamma disabled in the level settings ui ??
    // see render>output settings> color settings

    sl->setName(layerName.toStdWString());
    std::vector<TFrameId> fids;
    std::vector<int> frameNumbers;
    std::vector<int> durations;

    for (auto frame : jsonLayer["frames"].toArray()) {
      auto jsonFrame = frame.toObject();
      importOcaFrame(jsonFrame, sl);

      TFrameId fid;
      //fid = TFrameId(jsonFrame["frameNumber"].toInt());
      // the exporter names stop frames as _blank too... 
      if (Preferences::instance()->isImplicitHoldEnabled()
                   && (jsonFrame["name"] == "_blank" ||
          jsonFrame["fileName"].toString() == ""))
        fid = TFrameId(TFrameId::STOP_FRAME); //EMPTY_FRAME
      else
        fid = TFrameId(jsonFrame["frameNumber"].toInt());

      fids.push_back(fid);
      frameNumbers.push_back(jsonFrame["frameNumber"].toInt());
      durations.push_back(jsonFrame["duration"].toInt());
    }
    
    // Shouldn't api provide a simple method to expose the level using fids in xsheet timing ?
    int emptyColumnIndex = m_xsheet->getFirstFreeColumnIndex();
    int frameOffset = 0;
    if (m_startTime < 1) { // prevent crash if startTime is less than 1 : offset the timing...
      frameOffset = -m_startTime + 1;
    }
    //for (auto fid : fids) {
    for (int f = 0; f < fids.size() && f < frameNumbers.size() && f < durations.size(); f++) {
      auto fid = fids[f];
      auto frameNumber = frameNumbers[f];
      auto duration = durations[f];
      // -1 converts framenumber to index, frameOffset moves the frame range above 0...
      int frame = frameNumber - 1 + frameOffset;
      m_xsheet->setCell(frame, emptyColumnIndex, TXshCell(sl, fid)); // should work in all cases (real frame or empty frame)
      if (!Preferences::instance()->isImplicitHoldEnabled()) {
        // add cells to hold using image duration
        for (int f2 = frame + 1; f2 <= frame + duration;
            f2++) {
          m_xsheet->setCell(f2, emptyColumnIndex, TXshCell(sl, fid));
        }
      }
    }

    // Note about drawing numbering : I personally use the frame number as drawing number,
    // following richard william's animator's survival kit ("The best numbering system", page 76)
    // In toonz, this means : preference>Drawing>Numbering system = Animation sheet (isAnimationSheetEnabled ==  true) 
    // + Autorenumber
    // That way, I can define the timing on paper while numbering drawings
    // (i write barsheets with music and avoid the computer as much as i can)
    // column properties
    auto column = m_xsheet->getColumn(emptyColumnIndex);
    TStageObject *stageColumn =
        m_xsheet->getStageObject(TStageObjectId::ColumnId(emptyColumnIndex));
    //stageColumn->setName(jsonLayer["name"].toString().toStdString());

    stageColumn->setName(layerName.toStdString());
    //std::string str = m_columnName.toStdString();
    if (column && jsonLayer["type"].toString() !=
                      "grouplayer") {  // not sure why it's necessary
      column->setOpacity(byteFromFloat(
          jsonLayer["opacity"].toDouble() *
          255.0));  // not sure if byteFromFloat is really necessary...
      // confusing but preview is render visibility
      column->setPreviewVisible(!jsonLayer["reference"].toBool());
      // and Camstand is viewport visibility
      column->setCamstandVisible(jsonLayer["visible"].toBool());

      // jsonLayer["label"]
      // https://github.com/RxLaboratory/OCA/blob/master/src-docs/docs/specs/layer-labels.md
      // Most drawing and animation applications are able to
      // label the layers. These labels are often just different colors to
      // display the layers and used to differentiate them. OCA stores a simple
      // number to identify these labels. If this number is less than 0, it
      // means the layer is not labelled. We use column filters...
      column->setColorFilterId(jsonLayer["label"].toInt());
      // column->setColorTag(jsonLayer["label"].toInt()); what is the difference
      // between ColorTag and  ColorFilterId ??
    }

    ////// unused OCA properties:
    // - jsonLayer["animated"] Whether this layer is a single frame or not.
    // - jsonLayer["fileType"] The file extension, without the initial dot.
    // - Blending Modes
    // jsonLayer["blendingMode"] This could be supported with the fx schematic
    // (that wouldn't help while animating)
    // side note about blending mode :
    // I think blending modes should be implemented as realtime shaders in the viewport (fbo pingpong + glsl)
    // exposed in the ui as an easy to use column property in the column header (like opacity etc...)
    // the taoist RGB white japanese method of dealing with transparency based on white level,
    // or "RGBM" as it's called,
    // make it very paintfull to not have blending mode while animating... 
    // (ex : shoot paper drawings and they are opaque rasters, so onion skin doesn' work.
    // Ok you can set white level in the capture setting image ajust,
    // and check White As Transparent in the raster level settings,
    // The level premult option doesn't make onion skin work, if white level is too low...
    // but that's destructive.
    // And the level premult option doesn't make onion skin work, if white level is too low...
    // I would even add a realtime shader for chromakey and lumakey in the column header 
    // (even though i agree that it would be painfull to have all necessary keying parameters exposed column header in the ui)
    // - Frame tags
    // NavigationTags (frame tags) should be supported by OCA
    // NavigationTags *navTags = xsh->getNavigationTags();
    // this could be a great base to implement an onion skin filter... (like TVPaint has)
    // (ex : onion skin displays only 1 prev + 1 next key drawing, etc...)
    // Unfortunately, OCA doesn't support that, but it could be added with the metadata tag

    TApp::instance()->getCurrentLevel()->setLevel(
        level->getSimpleLevel());  // selects the last created level
    TApp::instance()->getCurrentLevel()->notifyLevelChange();
    TApp::instance()->getCurrentScene()->notifyCastChange();

  } else {
    DVGui::warning(QObject::tr("importOcaLayer skipping unimplemented layer type : ") +
        jsonLayer["type"].toString() + " : " + jsonLayer["name"].toString()
    );
  }
}

/// <summary>
/// creates frame from OCA frame
/// see OCA frame specs
/// https://github.com/RxLaboratory/OCA/blob/master/src-docs/docs/specs/frame.md
/// UNUSED properties :
/// - meta: metadata...
/// - opacity: per frame opacity isn't used.
/// - position: And what about scale/rotation ? transforms would be handled in a 
/// separate file... (keyframe animation transfert for camera, pegs, etc...) 
/// </summary>
/// <param name="jsonLayer"></param>

void OCAIo::OCAInputData::importOcaFrame(const QJsonObject &jsonFrame,
                                         TXshSimpleLevel *sl) {
  // just skip empty frames
  TFrameId fid(jsonFrame["frameNumber"].toInt());
  if (jsonFrame["name"].toString() == "_blank" ||
      jsonFrame["fileName"].toString() == "") {
    //TImageP img = sl->createEmptyFrame();
    //sl->setFrame(fid, img);
    return;
  }
  auto path       = TFilePath(m_parentDir.getQString() + "/" +
                        jsonFrame["fileName"].toString());
  bool fileExists = TFileStatus(path).doesExist();
  TImageP img;
  if (fileExists) {
    auto imageReader = TImageReaderP(path);
    try {
      img = imageReader->load();
      sl->setFrame(fid, img);
      // if (img.getPointer() == 0) img = sl->createEmptyFrame();
    } catch (const std::string &msg)
    {
      DVGui::error(QObject::tr("importOcaFrame load failed : ") +
                   path.getQString());
      //img = sl->createEmptyFrame();
      throw TException(msg);
    }
    if (img->getType() != TImage::RASTER) {
      DVGui::warning(
          QObject::tr("importOcaFrame skipping unimplemented image type : ") +
          jsonFrame["name"].toString());
    }
    auto info = imageReader->getImageInfo();
    DVGui::info(QObject::tr("importOcaFrame : ") + path.getQString() +
                " width: " + QString::number(info->m_lx) +
                " height: " + QString::number(info->m_ly) 
                + " bits: " + QString::number(info->m_bitsPerSample) );

  } else {
    DVGui::error(QObject::tr("importOcaFrame file not found! : ") + path.getQString());
    //img = sl->createEmptyFrame();
  }
}

int OCAIo::OCAInputData::findFirstRealFrameIndex(const QJsonObject &jsonLayer) {
  auto frames             = jsonLayer["frames"].toArray();
  for (int i = 0; i < frames.size(); i++) {
    auto jsonFrame = frames[i].toObject();
    if (jsonFrame["fileName"] != "") {
      return i;
    }
  }
  return -1;
}

QJsonArray OCAIo::OCAInputData::getFlattenLayers(QJsonArray layers) { 
  QJsonArray flattenLayers;
  for (auto layer : layers) {
    flattenLayers.append(layer);
    for (auto childLayer : layer.toObject()["childLayers"].toArray()) {
      parentMap[childLayer.toObject()["name"].toString()] =
          layer.toObject()["name"].toString();
      flattenLayers.append(childLayer);
    }
  }
  return flattenLayers;
}

void OCAIo::OCAInputData::reset() { 
    m_json = QJsonObject(); 
    parentMap.clear();
}

/// <summary>
/// ocaImportPopup
/// A dynamic ui to let us choose the import behavior for each layer
/// </summary>
/// <param name="data"></param>
ocaImportPopup::ocaImportPopup(OCAIo::OCAInputData *data) 
    : CustomLoadFilePopup(tr("Load OCA"), new QWidget(0)) {
  if (data == nullptr) data = new OCAInputData();
  m_data = data;
  connect(this, SIGNAL(filePathClicked(const TFilePath &)), this,
          SLOT(onFileClicked(const TFilePath &)));
}

void ocaImportPopup::rebuildCustomLayout(const TFilePath &fp) {
  //DVGui::info(QObject::tr("ocaImportPopup::rebuildLayout()"));
  if (m_customWidget == 0) {
    DVGui::error(
        QObject::tr("ocaImportPopup::rebuildLayout(): m_customWidget == 0"));
    return;
  }
  if (m_data == 0) {
    DVGui::error(
        QObject::tr("ocaImportPopup::rebuildLayout(): m_data == 0"));
    return;
  }
  // clear ui if already build
  removeCustomLayout();
  // rebuildCustomLayout("") with an empty string creates an empty layout
  if (fp.getQString() == "") {
    QWidget *optionWidget = (QWidget *)m_customWidget;
    setModal(false);
    QGridLayout *mainLayout;
    mainLayout = new QGridLayout();
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);
    optionWidget->setLayout(mainLayout);
    return;
  }
  QJsonObject ocaObject;
  if (!m_data->load(fp.getQString(), ocaObject)) {
    DVGui::warning(QObject::tr("ocaImportPopup: Failed to load OCA file: ") + fp.getQString());
    return;
  }
  QWidget *optionWidget = (QWidget *)m_customWidget;
  setModal(false);

  QGridLayout *mainLayout;
  mainLayout = new QGridLayout();
  mainLayout->setMargin(5);
  mainLayout->setSpacing(5);

  // rebuild ui
  QStringList mergingStatus = {"load", "skip"};
  // Read layers information to build the ui
  auto json   = m_data->getJson();
  auto layers = json.value("layers").toArray();
  // flat list of all layers...
  auto flattenLayers = m_data->getFlattenLayers(layers);
  int row = 0, column = 0;
  auto levelSet = m_data->getScene()->getLevelSet();
  const Preferences &prefs = *Preferences::instance();
  //for (auto layer : layers) {
  for (auto layer : flattenLayers) { // include children...
    auto jsonLayer = layer.toObject();
    if (jsonLayer["frames"].toArray().size() == 0) {
      // skip empty levels (probably grouplayers)
      //if (jsonLayer["type"].toString() == "grouplayer" || jsonLayer["childLayers"].toArray().size() > 0) ...
      row = 0;
      continue;
    }
    int firstRealFrameIndex = m_data->findFirstRealFrameIndex(jsonLayer);
    if (firstRealFrameIndex == -1) {
      // skip levels with no image
      row = 0;
      continue;
    }
    levelMergeStatusComboList << new QComboBox();
    auto levelMergeStatusCombo = levelMergeStatusComboList.back();
    levelMergeStatusCombo->addItems(mergingStatus);
    layerNameLabelList << new QLabel(jsonLayer["name"].toString());
    mainLayout->addWidget(levelMergeStatusCombo, row, column++);
    mainLayout->addWidget(layerNameLabelList.back(), row, column++);
    // connect(levelMergeStatusComboList.back(),
    // SIGNAL(currentIndexChanged(int)),
    //        this, SLOT(onMergeStatusIndexChanged(int)));
    //// requires C++14
    //connect(levelMergeStatusCombo,
    //          qOverload<int>(&QComboBox::currentIndexChanged), [=](int value) {
    //          onMergeStatusIndexChanged(jsonLayer["name"].toString(), value);
    //        });
    //// requires C++11 https://stackoverflow.com/questions/16794695/connecting-overloaded-signals-and-slots-in-qt-5
    connect(levelMergeStatusCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int value) {
              onMergeStatusIndexChanged(jsonLayer["name"].toString(), value);
            });
    
    if (levelSet->hasLevel(jsonLayer["name"].toString().toStdWString())) {
      levelMergeStatusCombo->setCurrentIndex(1); // this doesn't trigger signals ?
      m_importLayerMap[jsonLayer["name"].toString()] = 1;
    } else {
      levelMergeStatusCombo->setCurrentIndex(0);
      m_importLayerMap[jsonLayer["name"].toString()] = 0;
    }

    QCheckBox *premultiplyCB = new QCheckBox("Premult");
    premultiplyCB->setCheckState(Qt::CheckState::Checked);
    QCheckBox *whiteTranspCB = new QCheckBox("Transparent");
    whiteTranspCB->setCheckState(Qt::CheckState::Checked);
    QLabel *colorSpaceGammaLabel   = new QLabel("Gamma");
    QSlider *colorSpaceGammaSlider = new QSlider();
    colorSpaceGammaSlider->setValue(
        m_importOptionMap[jsonLayer["name"].toString()].DefaultColorSpaceGamma);
    QLabel *antialiasLabel   = new QLabel("Antialias");
    QSlider *antialiasSlider = new QSlider();
    antialiasSlider->setValue(0); 

    connect(premultiplyCB, &QCheckBox::stateChanged,
            [this, premultiplyCB, jsonLayer]() { 
            m_importOptionMap[jsonLayer["name"].toString()].m_premultiply =
                  premultiplyCB->isChecked();
    });
    connect(whiteTranspCB, &QCheckBox::stateChanged,
            [this, whiteTranspCB, jsonLayer]() { 
            m_importOptionMap[jsonLayer["name"].toString()].m_whiteTransp =
                  whiteTranspCB->isChecked();
    });
    connect(colorSpaceGammaSlider, &QSlider::valueChanged,
        [this, colorSpaceGammaSlider, jsonLayer]() {
          m_importOptionMap[jsonLayer["name"].toString()].m_colorSpaceGamma =
              colorSpaceGammaSlider->value();
    });
    connect(
        antialiasSlider, &QSlider::valueChanged,
        [this, antialiasSlider, jsonLayer]() {
          m_importOptionMap[jsonLayer["name"].toString()].m_antialias =
              antialiasSlider->value();
    });


    bool formatSpecified = false;
    LevelProperties *lp  = new LevelProperties();

    auto frames = jsonLayer["frames"].toArray();
    if (frames.size() > 0) {
      auto jsonFrame = frames[firstRealFrameIndex].toObject();
      auto levelPath = TFilePath(m_data->getParentDir().getQString() + "/" +
                                 jsonFrame["fileName"].toString());
      layerPathLabelList << new QLabel(levelPath.getQString());
      mainLayout->addWidget(layerPathLabelList.back(), row, column++);

      // display format options
      int formatIdx            = prefs.matchLevelFormat(levelPath);
      
      if (formatIdx >= 0) {
        lp->options()   = prefs.levelFormat(formatIdx).m_options;
        formatSpecified = true;
      }
      if (formatSpecified) {
        whiteTranspCB->setCheckState(lp->whiteTransp()
                                         ? Qt::CheckState::Checked
                                         : Qt::CheckState::Unchecked);
        premultiplyCB->setCheckState(lp->doPremultiply()
                                         ? Qt::CheckState::Checked
                                         : Qt::CheckState::Unchecked);
        antialiasSlider->setValue(lp ->antialiasSoftness());
        colorSpaceGammaSlider->setValue(lp->colorSpaceGamma());
      } else {
        lp->setWhiteTransp(whiteTranspCB->checkState());
        lp->setDoPremultiply(premultiplyCB->checkState());
        lp->setDoAntialias(antialiasSlider->value());
        lp->setColorSpaceGamma(colorSpaceGammaSlider->value());
      }
      auto options                                    = lp->options();
      m_importOptionMap[jsonLayer["name"].toString()] = options;

      //// The comparison to set levelMergeStatusCombo only uses layer names for
      //// now... NOT filenames...
      // 
      // if (levelSet->hasLevel(*m_data->getScene(), levelPath)) {
      //  levelMergeStatusCombo->setCurrentIndex(1);
      //  m_importLayerMap[jsonLayer["name"].toString()] = 1;
      //} else {
      //  levelMergeStatusCombo->setCurrentIndex(0);
      //  m_importLayerMap[jsonLayer["name"].toString()] = 0;
      //}
    }

    mainLayout->addWidget(premultiplyCB, row, column++);
    mainLayout->addWidget(whiteTranspCB, row, column++);
    // no ugly sliders (turn to int fields if desired...)
    //if (json["colorDepth"].toString() != "U8")
    //  mainLayout->addWidget(colorSpaceGammaSlider, row, column++);
    //mainLayout->addWidget(antialiasLabel, row, column++);
    //mainLayout->addWidget(antialiasSlider, row, column++);
    row++;
    column = 0;
  }
  optionWidget->setLayout(mainLayout);
}

void ocaImportPopup::removeCustomLayout() {
    // https://copyprogramming.com/howto/clearing-a-layout-in-qt
  //QLayout *po_l_layout = QWidget::layout();
    QLayout *po_l_layout = m_customWidget->layout();
  if (po_l_layout != 0) {
    QLayoutItem *item;
    while ((item = po_l_layout->takeAt(0)) != 0) po_l_layout->removeItem(item);
    delete po_l_layout;
  }
}

void ocaImportPopup::onFileClicked(const TFilePath &fp) { 
  QFileInfo fi(fp.getQString());
  // ignore directories and non oca files....
  if (fi.isDir()) return;
  if (fp.getType() != "oca") return;
  if (m_data == nullptr) return;
  QJsonObject ocaObject;
  if (!m_data->load(fp.getQString(), ocaObject)) {
    DVGui::warning(QObject::tr("Failed to load OCA file: ") + fp.getQString());
    return;
  }
  rebuildCustomLayout(fp);
}

void ocaImportPopup::onMergeStatusIndexChanged(QString layerName, int status) {
  m_importLayerMap[layerName] = status;
}

void ocaImportPopup::show() {
  reset();
  FileBrowserPopup::show();
}

void ocaImportPopup::reset() {
  rebuildCustomLayout(TFilePath(""));
}
