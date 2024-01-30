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
#include <QVBoxLayout>

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
/// Roadmap:
/// - test the case of child layer, add progress bar
/// - add the option to update the current scene from OCA, importing only the missing layers 
/// (example : import a krita paint layer, but keep the tlv with their palette as smart rasters...)
/// - keyframe animation transfert for camera, pegs, etc...
/// that would be another tool and another open file format (ascii (json or xml?) or binary (faster for huge scenes?) ?).
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
  static GenericLoadFilePopup *loadPopup = 0;
  if (!loadPopup) {
    loadPopup             = new GenericLoadFilePopup( QObject::tr("Import Open Cel Animation (OCA)") );
    loadPopup->addFilterType("oca");
  }
  if (!scene->isUntitled())
    loadPopup->setFolder(fp.getParentDir());
  else
    loadPopup->setFolder(
        TProjectManager::instance()->getCurrentProject()->getScenesPath());

  loadPopup->setFilename(fp.withoutParentDir());
  fp = loadPopup->getPath();
  if (fp.isEmpty()) {
    DVGui::info(QObject::tr("Open OCA file cancelled : empty filepath."));
    return;
  } else {
    DVGui::info(QObject::tr("Open OCA file : ") + fp.getQString());
  }

  QString ocafile = fp.getQString();
  OCAInputData ocaInputData = OCAInputData(0, true, false);
  QJsonObject ocaObject;
  if (!ocaInputData.load(ocafile, ocaObject)) {
    DVGui::warning(QObject::tr("Failed to load OCA file: ") +
                   fp.getQString());
    return;
  }
  ocaInputData.getSceneData();   // gather default values
  ocaInputData.read(ocaObject); // parser
  ocaInputData.setSceneData();  // set scene/xsheet values
  // TApp::instance()->getCurrentLevel()->notifyLevelChange(); > importOcaLayer
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

OCAIo::OCAInputData::OCAInputData(float antialiasSoftness, bool whiteTransp,
                                  bool doPremultiply) {
  m_antialiasSoftness = antialiasSoftness;
  m_whiteTransp       = whiteTransp;
  m_doPremultiply     = doPremultiply;
  m_scene             = TApp::instance()->getCurrentScene()->getScene();
  m_xsheet            = TApp::instance()->getCurrentXsheet()->getXsheet();
  m_oprop             = m_scene->getProperties()->getOutputProperties();
}

bool OCAIo::OCAInputData::load(QString path, QJsonObject &json) { 
  //see XdtsIo::loadXdtsScene
  m_path = path;
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
void OCAIo::OCAInputData::read(const QJsonObject &json) {
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
  for (auto jsonLayer : m_layers) {
    importOcaLayer(jsonLayer.toObject());
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
  //else {
  //  TDimensionD size(16, 9);
  //  m_scene->getCurrentCamera()->setSize(size, false, false);
  //}
  m_xsheet->updateFrameCount();
  m_oprop->setRange(m_startTime, m_endTime, 1);
  TPixel32 color = TPixel32(m_bgRed * 255.0, m_bgGreen * 255.0,
                            m_bgBlue * 255.0, m_bgAlpha * 255.0);
  m_scene->getProperties()->setBgColor(color);
  // anyway touching rendersettings here doesn't make sense
  // m_colorDepth (json["colorDepth"]) doesn't tell if the image is linear or
  // not... (a 32 bit tif could very well be non-linear, or a linear exr could
  // be 16 bit only, usually probalby half float 16 bit ...)
  // TRenderSettings renderSettings = m_scene->getProperties()->getOutputProperties()->getRenderSettings(); // incomplete type ?
  // renderSettings.m_bpp...
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
/// - whiteTransp / doPremultiply / colorSpaceGamma / antialiasSoftness / dpi are hardcoded
/// - blendingMode / animated are not used
/// - OCA doesn't support frame tags (NavigationTags) 
/// this would be very usefull to mark keys / breakdowns / inbetweens / etc drawing ... 
/// for the onion skining and fliping
/// - recursive layers (layer groups) have not neen tested 
/// (not sure if and how it could translate in toonz ? see CHILD_XSHLEVEL ?)
/// 
/// TODO :
/// - while TZP_XSHLEVEL is for toonz levels (smart rasters with palette)
/// For roundtrip / scene sharing between softwares,
/// Smart raster TZP_XSHLEVEL cannot be recreated from raster OVL_XSHLEVEL,
/// without palette loss. I would thus implement a dialog to let the user load
/// only missing layers in the scene (in order to keep smart raster levels
/// intact...)
/// </summary>
/// <param name="jsonLayer"></param>
void OCAIo::OCAInputData::importOcaLayer(const QJsonObject &jsonLayer) {
  if (jsonLayer["type"] == "paintlayer") {
    if (jsonLayer["blendingMode"].toString() != "normal") {
      DVGui::warning(QObject::tr("importOcaLayer : blending modes not implemented ") +
                  jsonLayer["name"].toString());
    }
    DVGui::info(QObject::tr("importOcaLayer : ") +
                jsonLayer["name"].toString());
    auto levelType   = OVL_XSHLEVEL; // OVL_XSHLEVEL is for raster images,

    //  I am using my own preference m_dpi / m_antialiasSoftness / m_whiteTransp / m_doPremultiply
    auto resolution  = TDimension(m_width, m_height);
    TXshLevel *level = m_scene->createNewLevel(
        levelType, jsonLayer["name"].toString().toStdWString(), resolution,
        m_dpi);

    TXshSimpleLevel *sl = dynamic_cast<TXshSimpleLevel *>(level);
    // TXshLevel *ToonzScene::createNewLevel(int type, std::wstring levelName, const TDimension &dim, double dpi, TFilePath fp) 
    // ignores the resolution argument !
    sl->getProperties()->setImageRes(resolution);

    // lookup the first frame to eventually load image format loading preferences which is based on frame extention
    // seeTXshLevel *ToonzScene::loadLevel
    // the problem is tahomastuff/profiles/users/[username]/preferences.ini doesn't add new image fomats to the list...
    auto frames = jsonLayer["frames"].toArray();
    bool formatSpecified = false;
    LevelProperties *lp  = sl->getProperties();
    if (frames.size() > 0) {
      auto jsonFrame      = frames[0].toObject();
      auto levelPath = TFilePath(m_parentDir.getQString() + "/" +
                            jsonFrame["fileName"].toString());

      const Preferences &prefs = *Preferences::instance();
      int formatIdx            = prefs.matchLevelFormat(levelPath);
      if (formatIdx >= 0) {
        lp->options()   = prefs.levelFormat(formatIdx).m_options;
        formatSpecified = true;
      }
    }
    if (formatSpecified) {
      sl->getProperties()->setWhiteTransp(lp->whiteTransp());
      sl->getProperties()->setDoPremultiply(lp->doPremultiply());
      sl->getProperties()->setDoAntialias(lp->antialiasSoftness());
      sl->getProperties()->setDpi(lp->getDpi());
      sl->getProperties()->setDpiPolicy(lp->getDpiPolicy());
      // sl->getProperties()->setImageDpi(lp->getImageDpi()); //?
      sl->getProperties()->setSubsampling(lp->getSubsampling());
      sl->getProperties()->setColorSpaceGamma(lp->colorSpaceGamma());
    } else {
      sl->getProperties()->setWhiteTransp(m_whiteTransp);
      sl->getProperties()->setDoPremultiply(m_doPremultiply);
      sl->getProperties()->setDoAntialias(m_antialiasSoftness);
      // we filmmakers don't use dpi ??
      sl->getProperties()->setDpi(m_dpi);
      sl->getProperties()->setDpiPolicy(LevelProperties::DP_CustomDpi);
    }

    // float colorSpaceGamma   = 2.2;
    // we don't need to touch this...
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

    sl->setName(jsonLayer["name"].toString().toStdWString());
    std::vector<TFrameId> fids;
    for (auto frame : jsonLayer["frames"].toArray()) {
      importOcaFrame(frame.toObject(), sl);
      TFrameId fid(frame.toObject()["frameNumber"].toInt());
      fids.push_back(fid);
    }
    
    // Shouldn't api provide a simple method to expose the level using fids in xsheet timing ?
    int emptyColumnIndex = m_xsheet->getFirstFreeColumnIndex();
    int frameOffset = 0;
    if (m_startTime < 1) { // prevent crash if startTime is less than 1 : offset the timing...
      frameOffset = -m_startTime + 1;
    }
    for (auto fid : fids) {
      if (fid.getNumber() <= 0) {
        DVGui::warning(
            QObject::tr("importOcaLayer : skipping frame -1! ") +
            jsonLayer["name"].toString());
        continue;
      }
      // -1 converts framenumber to index, frameOffset moves the frame range above 0...
      m_xsheet->setCell(fid.getNumber() - 1 + frameOffset, emptyColumnIndex,
                        TXshCell(sl, fid));
    }

    // Note about drawing numbering : I personally frame number as drawing number,
    // following richard william's animator's survival kit ("The best numbering system", page 76)
    // In toonz, this means : preference>Drawing>Numbering system = Animation sheet (isAnimationSheetEnabled ==  true) 
    // + Autorenumber
    // That way, I can define the timing on paper while numbering drawings
    // (i write barsheets with music and avoid the computer as much as i can)
    // column properties
    auto column = m_xsheet->getColumn(emptyColumnIndex);
    TStageObject *stageColumn =
        m_xsheet->getStageObject(TStageObjectId::ColumnId(emptyColumnIndex));
    stageColumn->setName(jsonLayer["name"].toString().toStdString());
    //std::string str = m_columnName.toStdString();
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
    // label the layers. These labels are often just different colors to display
    // the layers and used to differentiate them. OCA stores a simple number to
    // identify these labels. If this number is less than 0, it means the layer
    // is not labelled.
    // We use column filters...
    column->setColorFilterId(jsonLayer["label"].toInt());
    // column->setColorTag(jsonLayer["label"].toInt()); what is the difference
    // between ColorTag and  ColorFilterId ??


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
    // Unfortunately, OCA doesn't support that.

    // recursion for sublayers : nothing special to do ?
    // see CHILD_XSHLEVEL ?
    for (auto layer : jsonLayer["childLayers"].toArray()) {
      // I guess we dont need to deal with jsonLayer["passThrough"] ?
      // when passThrough is false, the group content *must* be merged in rendering, 
      // else the group is only used as a way to group the layers in the UI of the application
      importOcaLayer(layer.toObject());
    }

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
/// - position: transforms handled in a separate file ? (keyframe animation transfert for camera, pegs, etc...) 
/// And what about scale/rotation ?
/// - name: could be used to create empty frames (named "_blank")
/// </summary>
/// <param name="jsonLayer"></param>

void OCAIo::OCAInputData::importOcaFrame(const QJsonObject &jsonFrame,
                                         TXshSimpleLevel *sl) {
  // doesExists (QFileInfo.exists()) should skip empty frames ! but...
  TFrameId fid(jsonFrame["frameNumber"].toInt());
  if (jsonFrame["name"].toString() == "_blank" ||
      jsonFrame["fileName"].toString() == "") {
    TImageP img = sl->createEmptyFrame();
    sl->setFrame(fid, img);
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
      if (img.getPointer() == 0) img = sl->createEmptyFrame();
    } catch (const std::string &msg)
    {
      DVGui::error(QObject::tr("importOcaFrame load failed : ") +
                   path.getQString());
      img = sl->createEmptyFrame();
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
    img = sl->createEmptyFrame();
  }
  sl->setFrame(fid, img);
  if (!Preferences::instance()->isImplicitHoldEnabled()) {
      //do something with the image duration ?
  }
}

