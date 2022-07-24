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
  json["originApp"] = "OpenToonz";
  json["originAppVersion"] =
      QString::fromStdString(TEnv::getApplicationVersion());
  json["ocaVersion"] = "1.1.0";
}

int OCAData::frameLen(TXshCellColumn *column, const QList<int> &rows, int index) {
  // Next cells must match same level and frame-id
  int length = 0;
  const TXshCell &stc = column->getCell(rows[index]);
  for (int i = index; i < rows.count(); i++) {
    int currentRow       = rows[i];
    const TXshCell &cell = column->getCell(currentRow);
    if (stc.m_frameId != cell.m_frameId || stc.m_level != cell.m_level) break;
    length++;
  }
  return length;
}

bool OCAData::isBlank(TXshCellColumn *column, int row) {
  TXshCell cell = column->getCell(row);
  return cell.isEmpty();
}

bool OCAData::getLayerName(TXshCellColumn *column, QString &out) {
  // Layer name will be the first cel that occur on the column
  TXshCell cell = column->getCell(column->getFirstRow());
  if (cell.isEmpty()) return false;

  out = QString::fromStdWString(cell.m_level->getName());
  return true;
}

bool OCAData::getCellName(TXshCellColumn *column, int row, QString &out) {
  TXshCell cell = column->getCell(row);
  if (cell.isEmpty()) return false;

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
    if (cell.isEmpty())
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
    m_startTime = from - 1;
    m_endTime   = to - 1;
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
    qWarning("Couldn't open save file.");
    return;
  }
  if (!saveDir.exists()) {
    if (!saveDir.mkdir(".")) {
      qWarning("Couldn't create folder.");
      return;
    }
  }

  OCAData ocaData;
  ocaData.setProgressDialog(progressDialog);
  ocaData.build(scene, xsheet, QString::fromStdString(fp.getName()), ocafolder,
                frameOffset, exrImageFmt, !rasterVecs);
  if (ocaData.isEmpty()) {
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