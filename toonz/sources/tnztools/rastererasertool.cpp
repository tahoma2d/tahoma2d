

// TnzTools includes
#include "tools/toolutils.h"
#include "tools/cursors.h"
#include "tools/tool.h"
#include "tools/toolhandle.h"

#include "bluredbrush.h"

// TnzQt includes
#include "toonzqt/icongenerator.h"

// TnzLib includes
#include "toonz/ttilesaver.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/observer.h"
#include "toonz/toonzimageutils.h"
#include "toonz/levelproperties.h"
#include "toonz/stage2.h"
#include "toonz/ttileset.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/preferences.h"
#include "toonz/fill.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tstroke.h"
#include "tmathutil.h"
#include "drawutil.h"
#include "tcolorstyles.h"
#include "tundo.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tproperty.h"
#include "tgl.h"
#include "trop.h"
#include "tinbetween.h"
#include "ttile.h"
#include "tropcm.h"
#include "timage_io.h"
#include "symmetrytool.h"
#include "cmrasterbrush.h"
#include "vectorbrush.h"
#include "symmetrystroke.h"

// Qt includes
#include <QCoreApplication>  // For Qt translation support
#include <QPainter>

using namespace ToolUtils;

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

#define NORMALERASE L"Normal"
#define RECTERASE L"Rectangular"
#define FREEHANDERASE L"Freehand"
#define POLYLINEERASE L"Polyline"
#define SEGMENTERASE L"Segment"

TEnv::DoubleVar EraseSize("InknpaintEraseSize", 10);
TEnv::StringVar EraseType("InknpaintEraseType", "Normal");
TEnv::IntVar EraseSelective("InknpaintEraseSelective", 0);
TEnv::IntVar EraseInvert("InknpaintEraseInvert", 0);
TEnv::IntVar EraseRange("InknpaintEraseRange", 0);
TEnv::StringVar EraseColorType("InknpaintEraseColorType", "Lines");
TEnv::DoubleVar EraseHardness("EraseHardness", 100);
TEnv::IntVar ErasePencil("InknpaintErasePencil", 0);
TEnv::IntVar EraseOnlySavebox("InknpaintEraseOnlySavebox", 0);

namespace {

//==============================================================================
//   Undo dei Tools
//==============================================================================

/*!
        Viene realizzata la funzione di "UNDO" per il cancellino nell'opzione
   RECTERASE
*/

class RectRasterUndo final : public TRasterUndo {
  TRectD m_modifyArea;
  TStroke *m_stroke;
  int m_styleId;
  std::wstring m_colorType;
  std::wstring m_eraseType;
  bool m_selective;
  bool m_invert;
  bool m_pencilMode;

public:
  RectRasterUndo(TTileSetCM32 *tileSet, const TRectD &modifyArea,
                 TStroke stroke, int styleId, std::wstring eraseType,
                 std::wstring colorType, TXshSimpleLevel *level, bool selective,
                 bool invert, bool pencil, const TFrameId &frameId)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_modifyArea(modifyArea)
      , m_styleId(styleId)
      , m_eraseType(eraseType)
      , m_colorType(colorType)
      , m_selective(selective)
      , m_invert(invert)
      , m_pencilMode(pencil) {
    m_stroke = new TStroke(stroke);
  }

  void redo() const override {
    TToonzImageP ti = getImage();
    if (!ti) return;
    bool eraseInk   = m_colorType == LINES || m_colorType == ALL;
    bool erasePaint = m_colorType == AREAS || m_colorType == ALL;
    if (m_eraseType == RECTERASE) {
      TRect rect = ToonzImageUtils::eraseRect(ti, m_modifyArea, m_styleId,
                                              eraseInk, erasePaint);
      if (!rect.isEmpty()) ToolUtils::updateSaveBox(m_level, m_frameId);

    } else if (m_eraseType == FREEHANDERASE || m_eraseType == POLYLINEERASE) {
      if (m_level) {
        TPoint pos;
        TRaster32P ras = convertStrokeToImage(
            m_stroke, ti->getRaster()->getBounds(), pos, m_pencilMode);
        if (!ras) return;
        ToonzImageUtils::eraseImage(ti, ras, pos, m_invert, eraseInk,
                                    erasePaint, m_selective, m_styleId);
        ToolUtils::updateSaveBox(m_level, m_frameId);
      }
    }
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return TRasterUndo::getSize() + sizeof(this) +
           m_stroke->getControlPointCount() * sizeof(TThickPoint) + 100;
  }

  ~RectRasterUndo() {
    if (m_stroke) delete m_stroke;
  }

  QString getToolName() override { return QString("Eraser Tool (Rect)"); }
  int getHistoryType() override { return HistoryType::EraserTool; }
};

//=====================================================================
/*!
        Viene realizzata la funzione di "UNDO" per il cancellino nell'opzione
   NORMALERAS
*/
class RasterEraserUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_selective;
  bool m_isPencil;
  ColorType m_colorType;
  int m_colorSelected;
  TPointD m_dpiScale;
  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;

public:
  RasterEraserUndo(TTileSetCM32 *tileSet,
                   const std::vector<TThickPoint> &points, ColorType colorType,
                   int styleId, bool selective, int colorSelected,
                   TXshSimpleLevel *level, const TFrameId &frameId,
                   bool isPencil, TPointD dpiScale, double symmetryLines,
                   double rotation, TPointD centerPoint, bool useLineSymmetry)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_colorType(colorType)
      , m_colorSelected(colorSelected)
      , m_isPencil(isPencil)
      , m_dpiScale(dpiScale)
      , m_brushCount(symmetryLines)
      , m_rotation(rotation)
      , m_centerPoint(centerPoint)
      , m_useLineSymmetry(useLineSymmetry) {}

  void redo() const override {
    TToonzImageP image = m_level->getFrame(m_frameId, true);
    TRasterCM32P ras   = image->getRaster();
    CMRasterBrush eraser(ras, ERASE, m_colorType, 0, m_points[0], m_selective,
                         m_colorSelected, false, !m_isPencil);

    if (m_brushCount > 1)
      eraser.addSymmetryBrushes(m_brushCount, m_rotation, m_centerPoint,
                                m_useLineSymmetry, m_dpiScale);

    eraser.setPointsSequence(m_points);
    eraser.generateStroke(m_isPencil);
    image->setSavebox(image->getSavebox() +
                      eraser.getBBox(eraser.getPointsSequence()));
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Eraser Tool"); }
  int getHistoryType() override { return HistoryType::EraserTool; }
};

//=====================================================================

class RasterBluredEraserUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_selective;
  int m_size;
  double m_hardness;
  std::wstring m_mode;
  TPointD m_dpiScale;
  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;

public:
  RasterBluredEraserUndo(TTileSetCM32 *tileSet,
                         const std::vector<TThickPoint> &points, int styleId,
                         bool selective, TXshSimpleLevel *level,
                         const TFrameId &frameId, int size, double hardness,
                         const std::wstring &mode, TPointD dpiScale,
                         double symmetryLines, double rotation,
                         TPointD centerPoint, bool useLineSymmetry)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_size(size)
      , m_hardness(hardness)
      , m_mode(mode)
      , m_dpiScale(dpiScale)
      , m_brushCount(symmetryLines)
      , m_rotation(rotation)
      , m_centerPoint(centerPoint)
      , m_useLineSymmetry(useLineSymmetry) {}

  void redo() const override {
    if (m_points.size() == 0) return;
    TToonzImageP image     = getImage();
    TRasterCM32P ras       = image->getRaster();
    TRasterCM32P backupRas = ras->clone();
    TRaster32P workRaster(ras->getSize());
    QRadialGradient brushPad = ToolUtils::getBrushPad(m_size, m_hardness);
    workRaster->clear();
    RasterBlurredBrush brush(workRaster, m_size, brushPad, false);

    if (m_brushCount > 1)
      brush.addSymmetryBrushes(m_brushCount, m_rotation, m_centerPoint,
                               m_useLineSymmetry, m_dpiScale);

    std::vector<TThickPoint> points;
    points.push_back(m_points[0]);
    TRect bbox = brush.getBoundFromPoints(points);
    brush.addPoint(m_points[0], 1);
    brush.eraseDrawing(ras, ras, bbox, m_selective, m_styleId, m_mode);
    if (m_points.size() > 1) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], (m_points[0] + m_points[1]) * 0.5, m_points[1],
                   1, 1);
      brush.eraseDrawing(ras, backupRas, bbox, m_selective, m_styleId, m_mode);
      int i;
      for (i = 1; i + 2 < (int)m_points.size(); i = i + 2) {
        points.clear();
        points.push_back(m_points[i]);
        points.push_back(m_points[i + 1]);
        points.push_back(m_points[i + 2]);
        bbox = brush.getBoundFromPoints(points);
        brush.addArc(m_points[i], m_points[i + 1], m_points[i + 2], 1, 1);
        brush.eraseDrawing(ras, backupRas, bbox, m_selective, m_styleId,
                           m_mode);
      }
    }
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Eraser Tool"); }
  int getHistoryType() override { return HistoryType::EraserTool; }
};

//=============================================================================
// RasterSegmentEraseUndo
//-----------------------------------------------------------------------------

class RasterSegmentEraseUndo final : public TRasterUndo {
  // FillParameters m_params;
  bool m_saveboxOnly;
  double m_autoCloseDistance;
  int m_closeStyleIndex;
  TPoint m_point;
  TRectD m_savebox;
  TPointD m_dpiScale;
  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;

public:
  RasterSegmentEraseUndo(TTileSetCM32 *tileSet, TPoint point,
                         TXshSimpleLevel *sl, const TFrameId &fid,
                         bool saveboxOnly, TRectD savebox, TPointD dpiScale,
                         double symmetryLines, double rotation,
                         TPointD centerPoint, bool useLineSymmetry)
      : TRasterUndo(tileSet, sl, fid, false, false, 0)
      , m_point(point)
      , m_saveboxOnly(saveboxOnly)
      , m_savebox(savebox)
      , m_dpiScale(dpiScale)
      , m_brushCount(symmetryLines)
      , m_rotation(rotation)
      , m_centerPoint(centerPoint)
      , m_useLineSymmetry(useLineSymmetry) {}

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;
    bool recomputeSavebox = false;
    TRasterCM32P r;
    if (m_saveboxOnly) {
      TRect ttemp = convert(m_savebox);
      r           = image->getRaster()->extract(ttemp);
    } else
      r = image->getRaster();

    inkSegment(r, m_point, 0, 2.51, true, 0, true);

    if (m_brushCount > 1) {
      SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
          TTool::getTool("T_Symmetry", TTool::RasterImage));
      if (symmetryTool) {
        SymmetryObject symmObj       = symmetryTool->getSymmetryObject();
        std::vector<TPointD> symmPts = symmetryTool->getSymmetryPoints(
            convert(m_point), r->getCenterD(), m_dpiScale, m_brushCount,
            m_rotation, m_centerPoint, m_useLineSymmetry);

        for (int i = 0; i < symmPts.size(); i++) {
          TPoint pt(symmPts[i].x, symmPts[i].y);
          if (pt == m_point) continue;
          inkSegment(r, pt, 0, 2.51, true, 0, true);
        }
      }
    }

    if (recomputeSavebox) ToolUtils::updateSaveBox();

    TTool::Application *app = TTool::getApplication();
    if (app) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
    }
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override {
    return QString("Erase Tool : %1").arg("Segment");
  }
  int getHistoryType() override { return HistoryType::EraserTool; }
};

//-----------------------------------------------------------------------------

void eraseStroke(const TToonzImageP &ti, TStroke *stroke,
                 std::wstring eraseType, std::wstring colorType, bool invert,
                 bool selective, bool pencil, int styleId,
                 const TXshSimpleLevelP &level, const TFrameId &frameId) {
  assert(stroke);
  TPoint pos;
  TRasterCM32P ras = ti->getRaster();
  TRaster32P image =
      convertStrokeToImage(stroke, ras->getBounds(), pos, pencil);
  if (!image) return;

  TRect rasterErasedArea = image->getBounds() + pos;
  TRect area;
  if (!invert)
    area = rasterErasedArea.enlarge(2);
  else
    area                = ras->getBounds();
  TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
  tileSet->add(ras, area);
  TUndoManager::manager()->add(new RectRasterUndo(
      tileSet, convert(area), *stroke, selective ? styleId : -1, eraseType,
      colorType, level.getPointer(), selective, invert, pencil, frameId));
  bool eraseInk   = colorType == LINES || colorType == ALL;
  bool erasePaint = colorType == AREAS || colorType == ALL;
  ToonzImageUtils::eraseImage(ti, image, pos, invert, eraseInk, erasePaint,
                              selective, styleId);
}

//-----------------------------------------------------------------------------

bool eraseSegment(const TImageP &img, const TPointD &pos, TXshSimpleLevel *sl,
                  const TFrameId &fid, bool selective, int styleId,
                  bool eraseOnlySavebox, TPointD dpiScale) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return false;

  if (TToonzImageP ti = TToonzImageP(img)) {
    TPoint offs(0, 0);
    TRasterCM32P ras = ti->getRaster();

    TRectD bbox = ti->getBBox();
    if (eraseOnlySavebox) {
      TRect ibbox = convert(bbox);
      offs        = ibbox.getP00();
      ras         = ti->getRaster()->extract(ibbox);
    }

    bool recomputeSavebox = false;
    TPalette *plt         = ti->getPalette();

    if (!ras.getPointer() || ras->isEmpty()) return false;

    ras->lock();

    TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
    TTileSaverCM32 tileSaver(ras, tileSet);
    TDimension imageSize = ti->getSize();
    TPointD p(imageSize.lx % 2 ? 0.0 : 0.5, imageSize.ly % 2 ? 0.0 : 0.5);

    TPointD tmp_p = pos - p;
    TPoint newPoint =
        TPoint((int)floor(tmp_p.x + 0.5), (int)floor(tmp_p.y + 0.5));

    newPoint += ti->getRaster()->getCenter();
    newPoint -= offs;

    TRect rasRect(ras->getSize());
    if (!rasRect.contains(newPoint)) {
      ras->unlock();
      return false;
    }

    inkSegment(ras, newPoint, 0, 2.51, true, &tileSaver, true);

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    if (symmetryTool && symmetryTool->isGuideEnabled()) {
      TPointD rasCenter            = ras->getCenterD();
      std::vector<TPointD> symmPts = symmetryTool->getSymmetryPoints(
          convert(newPoint), rasCenter, dpiScale);

      for (int i = 0; i < symmPts.size(); i++) {
        if (symmPts[i] == convert(newPoint)) continue;
        inkSegment(ras, convert(symmPts[i]), 0, 2.51, true, &tileSaver, true);
      }
    }

    if (tileSaver.getTileSet()->getTileCount() != 0) {
      static int count = 0;
      // TSystem::outputDebug("FILL" + std::to_string(count++) + "\n");
      if (offs != TPoint())
        for (int i = 0; i < tileSet->getTileCount(); i++) {
          TTileSet::Tile *t = tileSet->editTile(i);
          t->m_rasterBounds = t->m_rasterBounds + offs;
        }
      double symmetryLines = 0;
      double rotation      = 0;
      bool useLineSymmetry = false;
      TPointD centerPoint(0, 0);
      if (symmetryTool && symmetryTool->isGuideEnabled()) {
        SymmetryObject symmObj = symmetryTool->getSymmetryObject();
        symmetryLines          = symmObj.getLines();
        rotation               = symmObj.getRotation();
        centerPoint            = symmObj.getCenterPoint();
        useLineSymmetry        = symmObj.isUsingLineSymmetry();
      }

      TUndoManager::manager()->add(new RasterSegmentEraseUndo(
          tileSet, newPoint, sl, fid, eraseOnlySavebox, bbox, dpiScale,
          symmetryLines, rotation, centerPoint, useLineSymmetry));
    }

    // instead of updateFrame :

    TXshLevel *xl = app->getCurrentLevel()->getLevel();
    if (!xl) return false;

    TXshSimpleLevel *sl = xl->getSimpleLevel();
    sl->getProperties()->setDirtyFlag(true);
    if (recomputeSavebox &&
        Preferences::instance()->isMinimizeSaveboxAfterEditing())
      ToolUtils::updateSaveBox(sl, fid);

    ras->unlock();
  }

  TTool *t = app->getCurrentTool()->getTool();
  if (t) t->notifyImageChanged();

  return true;
}

//-----------------------------------------------------------------------------

void drawLine(const TPointD &point, const TPointD &centre, bool horizontal,
              bool isDecimal) {
  if (!isDecimal) {
    if (horizontal) {
      tglDrawSegment(TPointD(point.x - 1.5, point.y + 0.5) + centre,
                     TPointD(point.x - 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y - 0.5, -point.x + 1.5) + centre,
                     TPointD(point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);

      tglDrawSegment(TPointD(point.y - 0.5, point.x + 0.5) + centre,
                     TPointD(point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, -point.y + 0.5) + centre,
                     TPointD(point.x - 1.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 1.5) + centre);
      tglDrawSegment(TPointD(-point.x - 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x + 0.5, point.y + 0.5) + centre);
    } else {
      tglDrawSegment(TPointD(point.x - 1.5, point.y + 1.5) + centre,
                     TPointD(point.x - 1.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 1.5, point.y + 0.5) + centre,
                     TPointD(point.x - 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, -point.x + 1.5) + centre,
                     TPointD(point.y - 0.5, -point.x + 1.5) + centre);
      tglDrawSegment(TPointD(point.y - 0.5, -point.x + 1.5) + centre,
                     TPointD(point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 0.5) + centre,
                     TPointD(-point.x + 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);

      tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre,
                     TPointD(point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(point.y - 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 1.5, -point.y - 0.5) + centre,
                     TPointD(point.x - 1.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 1.5, -point.y + 0.5) + centre,
                     TPointD(point.x - 0.5, -point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, -point.x + 1.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 1.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 1.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 1.5) + centre,
                     TPointD(-point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, point.y + 0.5) + centre);
    }
  } else {
    if (horizontal) {
      tglDrawSegment(TPointD(point.x - 0.5, point.y + 0.5) + centre,
                     TPointD(point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre,
                     TPointD(point.y + 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, -point.x + 0.5) + centre,
                     TPointD(point.y + 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.x + 0.5, -point.y - 0.5) + centre,
                     TPointD(point.x - 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.x - 0.5, -point.y - 0.5) + centre,
                     TPointD(-point.x + 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, point.y + 0.5) + centre);
    } else {
      tglDrawSegment(TPointD(point.x - 0.5, point.y + 1.5) + centre,
                     TPointD(point.x - 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, point.y + 0.5) + centre,
                     TPointD(point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 1.5, point.x - 0.5) + centre,
                     TPointD(point.y + 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, point.x - 0.5) + centre,
                     TPointD(point.y + 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 1.5, -point.x + 0.5) + centre,
                     TPointD(point.y + 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(point.y + 0.5, -point.x + 0.5) + centre,
                     TPointD(point.y + 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, -point.y - 1.5) + centre,
                     TPointD(point.x - 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(point.x - 0.5, -point.y - 0.5) + centre,
                     TPointD(point.x + 0.5, -point.y - 0.5) + centre);

      tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 1.5) + centre,
                     TPointD(-point.x + 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, -point.y - 0.5) + centre,
                     TPointD(-point.x - 0.5, -point.y - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, -point.x + 0.5) + centre,
                     TPointD(-point.y - 0.5, -point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 1.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x - 0.5) + centre);
      tglDrawSegment(TPointD(-point.y - 0.5, point.x - 0.5) + centre,
                     TPointD(-point.y - 0.5, point.x + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 1.5) + centre,
                     TPointD(-point.x + 0.5, point.y + 0.5) + centre);
      tglDrawSegment(TPointD(-point.x + 0.5, point.y + 0.5) + centre,
                     TPointD(-point.x - 0.5, point.y + 0.5) + centre);
    }
  }
}

//-------------------------------------------------------------------------------------------------------

void drawEmptyCircle(int thick, const TPointD &mousePos, bool isPencil,
                     bool isLxEven, bool isLyEven) {
  TPointD pos = mousePos;
  if (isLxEven) pos.x += 0.5;
  if (isLyEven) pos.y += 0.5;
  if (!isPencil)
    tglDrawCircle(pos, (thick)*0.5);
  else {
    int x = 0, y = tround((thick * 0.5) - 0.5);
    int d           = 3 - 2 * (int)(thick * 0.5);
    bool horizontal = true, isDecimal = thick % 2 != 0;
    drawLine(TPointD(x, y), pos, horizontal, isDecimal);
    while (y > x) {
      if (d < 0) {
        d          = d + 4 * x + 6;
        horizontal = true;
      } else {
        d          = d + 4 * (x - y) + 10;
        horizontal = false;
        y--;
      }
      x++;
      drawLine(TPointD(x, y), pos, horizontal, isDecimal);
    }
  }
}

//==================================================================================================
//
//  InkPaintTool class declaration
//
//------------------------------------------------------------------------

class EraserTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(EraserTool)

public:
  EraserTool(std::string name);
  ~EraserTool() { m_firstStrokes.clear(); }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  void draw() override;

  void update(const TToonzImageP &ti, const TPointD &pos);
  void saveUndo();
  void update(const TToonzImageP &ti, TRectD selArea,
              const TXshSimpleLevelP &level, bool multi = false,
              const TFrameId &frameId = -1);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;

  void multiAreaEraser(const TXshSimpleLevelP &sl, TFrameId &firstFid,
                       TFrameId &lastFid, std::vector<TStroke *> firstStrokes,
                       std::vector<TStroke *> lastStrokes);
  void doMultiEraser(const TImageP &img, double t, const TXshSimpleLevelP &sl,
                     const TFrameId &fid, const TVectorImageP &firstImage,
                     const TVectorImageP &lastImage);
  void doMultiSegmentEraser(const TImageP &img, double t,
                            const TXshSimpleLevelP &sl, const TFrameId &fid,
                            const TVectorImageP &firstImage,
                            const TVectorImageP &lastImage);

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
  void onLeave() override;
  void onActivate() override;
  bool onPropertyChanged(std::string propertyName) override;
  void onImageChanged() override;

  void multiUpdate(const TXshSimpleLevelP &level, TFrameId firstFrameId,
                   TFrameId lastFrameId, TRectD firstRect, TRectD lastRect);

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  int getCursorId() const override;
  void resetMulti();

  /*-- ドラッグ中にツールが切り替わった場合、処理を終了させる--*/
  void onDeactivate() override;
  /*-- Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す --*/
  bool isPencilModeActive() override;
  TPointD getCenteredCursorPos(const TPointD &originalCursorPos);
  TPointD fixMousePos(TPointD pos, bool precise = false);

private:
  /*-- 終了処理 --*/
  void storeUndoAndRefresh();

  enum Type { NONE = 0, BRUSH, INKCHANGE, ERASER };

private:
  TPropertyGroup m_prop;

  TEnumProperty m_eraseType;
  TIntProperty m_toolSize;
  TDoubleProperty m_hardness;
  TBoolProperty m_invertOption;
  TBoolProperty m_currentStyle;
  TBoolProperty m_multi;
  TBoolProperty m_pencil;
  TEnumProperty m_colorType;
  TBoolProperty m_eraseOnlySavebox;

  Type m_type;

  TXshSimpleLevelP m_level;
  std::pair<int, int> m_currCell;

  TFrameId m_firstFrameId, m_veryFirstFrameId;

  VectorBrush m_track;

  std::vector<TStroke *>m_firstStrokes;

  TTileSaverCM32 *m_tileSaver;
  TTileSetCM32 *m_tileSet;

  CMRasterBrush *m_normalEraser;

  TRectD m_selectingRect, m_firstRect;

  ColorType m_colorTypeEraser;

  TPointD m_mousePos, m_brushPos, m_firstPos;

  SymmetryStroke m_polyline;

  // gestione cancellino blurato
  TRasterCM32P m_backupRas;
  TRaster32P m_workRas;
  QRadialGradient m_brushPad;
  std::vector<TThickPoint> m_points;
  RasterBlurredBrush *m_blurredEraser;

  double m_pointSize, m_distance2, m_cleanerSize, m_thick;

  bool m_isXsheetCell, m_active, m_enabled, m_selecting, m_firstFrameSelected,
      m_firstTime;

  /*---  消しゴム開始時のFrameIdを保存し、マウスリリース時（Undoの登録時）に
          別のフレームに移動していた場合に備える
  ---*/
  TFrameId m_workingFrameId;
  /*--- マウスボタンDown→プロパティ変更→マウスボタンUpのように、
          ボタンDownからUpまでストレートに来なかった場合は処理を行わないようにする。
  ---*/
  bool m_isLeftButtonPressed;
};

EraserTool inkPaintEraserTool("T_Eraser");

//==================================================================================================
//
//  InkPaintTool implementation
//
//------------------------------------------------------------------------

EraserTool::EraserTool(std::string name)
    : TTool(name)
    , m_toolSize("Size:", 1, 1000, 10, false)  // W_ToolOptions_EraserToolSize
    , m_hardness("Hardness:", 0, 100, 100)
    , m_eraseType("Type:")                // W_ToolOptions_Erasetype
    , m_colorType("Mode:")                // W_ToolOptions_InkOrPaint
    , m_currentStyle("Selective", false)  // W_ToolOptions_Selective
    , m_invertOption("Invert", false)     // W_ToolOptions_Invert
    , m_multi("Frame Range", false)       // W_ToolOptions_FrameRange
    , m_pencil("Pencil Mode", false)
    , m_currCell(-1, -1)
    , m_tileSaver(0)
    , m_blurredEraser(0)
    , m_pointSize(-1)
    , m_thick(0.5)
    , m_firstFrameSelected(false)
    , m_selecting(false)
    , m_active(false)
    , m_enabled(false)
    , m_isXsheetCell(false)
    , m_firstTime(true)
    , m_workingFrameId(TFrameId())
    , m_isLeftButtonPressed(false)
    , m_eraseOnlySavebox("Savebox", false) {
  bind(TTool::ToonzImage);

  m_toolSize.setNonLinearSlider();

  m_prop.bind(m_toolSize);
  m_prop.bind(m_hardness);
  m_prop.bind(m_eraseType);
  m_eraseType.addValue(NORMALERASE);
  m_eraseType.addValue(RECTERASE);
  m_eraseType.addValue(FREEHANDERASE);
  m_eraseType.addValue(POLYLINEERASE);
  m_eraseType.addValue(SEGMENTERASE);

  m_colorType.addValue(LINES);
  m_colorType.addValue(AREAS);
  m_colorType.addValue(ALL);
  m_prop.bind(m_colorType);

  m_prop.bind(m_currentStyle);
  m_prop.bind(m_invertOption);
  m_prop.bind(m_multi);
  m_prop.bind(m_pencil);
  m_prop.bind(m_eraseOnlySavebox);

  m_currentStyle.setId("Selective");
  m_invertOption.setId("Invert");
  m_multi.setId("FrameRange");
  m_pencil.setId("PencilMode");
  m_colorType.setId("Mode");
  m_eraseType.setId("Type");
  m_eraseOnlySavebox.setId("EraseOnlySavebox");
}

//------------------------------------------------------------------------

void EraserTool::updateTranslation() {
  m_toolSize.setQStringName(tr("Size:"));
  m_hardness.setQStringName(tr("Hardness:"));

  m_eraseType.setQStringName(tr("Type:"));
  m_eraseType.setItemUIName(NORMALERASE, tr("Normal"));
  m_eraseType.setItemUIName(RECTERASE, tr("Rectangular"));
  m_eraseType.setItemUIName(FREEHANDERASE, tr("Freehand"));
  m_eraseType.setItemUIName(POLYLINEERASE, tr("Polyline"));
  m_eraseType.setItemUIName(SEGMENTERASE, tr("Segment"));

  m_colorType.setQStringName(tr("Mode:"));
  m_colorType.setItemUIName(LINES, tr("Lines"));
  m_colorType.setItemUIName(AREAS, tr("Areas"));
  m_colorType.setItemUIName(ALL, tr("Lines & Areas"));

  m_currentStyle.setQStringName(tr("Selective"));
  m_invertOption.setQStringName(tr("Invert"));
  m_multi.setQStringName(tr("Frame Range"));
  m_pencil.setQStringName(tr("Pencil Mode"));
  m_eraseOnlySavebox.setQStringName(tr("Savebox"));
}

//-------------------------------------------------------------------------------------------------------

TPointD EraserTool::getCenteredCursorPos(const TPointD &originalCursorPos) {
  TXshLevelHandle *levelHandle = m_application->getCurrentLevel();
  TXshSimpleLevel *level = levelHandle ? levelHandle->getSimpleLevel() : 0;
  TDimension resolution =
      level ? level->getProperties()->getImageRes() : TDimension(0, 0);

  bool xEven = (resolution.lx % 2 == 0);
  bool yEven = (resolution.ly % 2 == 0);

  TPointD centeredCursorPos = originalCursorPos;

  if (xEven) centeredCursorPos.x -= 0.5;
  if (yEven) centeredCursorPos.y -= 0.5;

  return centeredCursorPos;
}

//-----------------------------------------------------------------------------

TPointD EraserTool::fixMousePos(TPointD pos, bool precise) {
  TPointD fixedPos = getCenteredCursorPos(pos);
  if (precise) {
    TPointD pp(tround(fixedPos.x), tround(fixedPos.y));
    fixedPos = pp;
  }
  return fixedPos;
}

//------------------------------------------------------------------------

void EraserTool::draw() {
  if (m_eraseType.getValue() == SEGMENTERASE && m_eraseOnlySavebox.getValue()) {
    TToonzImageP ti = (TToonzImageP)getImage(false);
    if (ti) {
      TRectD bbox =
          ToonzImageUtils::convertRasterToWorld(convert(ti->getBBox()), ti);
      drawRect(bbox.enlarge(0.5) * ti->getSubsampling(),
               TPixel32(210, 210, 210), 0xF0F0, true);
    }
  }

  /*-- MouseLeave時に赤点が描かれるのを防ぐ --*/
  if (!m_multi.getValue() && m_pointSize == -1 && m_cleanerSize == 0) return;

  double pixelSize2 = getPixelSize() * getPixelSize();
  m_thick           = sqrt(pixelSize2) / 2.0;

  TImageP img = getImage(false);
  if (!img) return;

//  TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
//                     ? TPixel32::White
//                     : TPixel32::Black;
  TPixel color = TPixel32::Red;
  if (m_eraseType.getValue() == RECTERASE) {
    if (m_multi.getValue() && m_firstFrameSelected) {
      if (m_firstStrokes.size()) {
        tglColor(color);
        for (int i = 0; i < m_firstStrokes.size(); i++)
          drawStrokeCenterline(*m_firstStrokes[i], 1);
      } else
        drawRect(m_firstRect, color, 0x3F33, true);
    }

    if (m_selecting || (m_multi.getValue() && !m_firstFrameSelected)) {
      if (m_polyline.size() > 1) {
        glPushMatrix();
        m_polyline.drawRectangle(color);
        glPopMatrix();
      } else
        drawRect(m_selectingRect, color, 0xFFFF, true);
    }
  }
  if (m_eraseType.getValue() == NORMALERASE) {
    // If toggled off, don't draw brush outline
    if (!Preferences::instance()->isCursorOutlineEnabled()) return;

    TToonzImageP image(img);
    TRasterP ras = image->getRaster();
    int lx       = ras->getLx();
    int ly       = ras->getLy();

    /*-- InkCheck, PaintCheck, Ink#1CheckがONのときは、BrushTipの描画色を変える
     * --*/
    if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) ||
        (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint) ||
        (ToonzCheck::instance()->getChecks() & ToonzCheck::eInk1))
      glColor3d(0.5, 0.8, 0.8);
    else
      glColor3d(1.0, 0.0, 0.0);
    drawEmptyCircle(tround(m_cleanerSize), m_brushPos,
                    (m_pencil.getValue() || m_colorType.getValue() == AREAS),
                    lx % 2 == 0, ly % 2 == 0);
  }
  if ((m_eraseType.getValue() == FREEHANDERASE ||
       m_eraseType.getValue() == POLYLINEERASE ||
       m_eraseType.getValue() == SEGMENTERASE) &&
      m_multi.getValue()) {
    tglColor(color);
    for (int i = 0; i < m_firstStrokes.size(); i++)
      drawStrokeCenterline(*m_firstStrokes[i], 1);
  }
  if (m_eraseType.getValue() == POLYLINEERASE && !m_polyline.empty()) {
    m_polyline.drawPolyline(m_mousePos, color);
  } else if ((m_eraseType.getValue() == FREEHANDERASE ||
              m_eraseType.getValue() == SEGMENTERASE) &&
             !m_track.isEmpty()) {
    tglColor(color);
    glPushMatrix();
    m_track.drawAllFragments();
    glPopMatrix();
  }
}

//----------------------------------------------------------------------

int EraserTool::getCursorId() const {
  int ret;
  if (m_eraseType.getValue() == NORMALERASE)
    ret = ToolCursor::NormalEraserCursor;
  else {
    ret = ToolCursor::RectEraserCursor;

    if (m_eraseType.getValue() == FREEHANDERASE)
      ret = ret | ToolCursor::Ex_FreeHand;
    else if (m_eraseType.getValue() == POLYLINEERASE)
      ret = ret | ToolCursor::Ex_PolyLine;
    else if (m_eraseType.getValue() == RECTERASE)
      ret = ret | ToolCursor::Ex_Rectangle;
  }

  if (m_colorType.getValue() == LINES)
    ret = ret | ToolCursor::Ex_Line;
  else if (m_colorType.getValue() == AREAS)
    ret = ret | ToolCursor::Ex_Area;

  if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
    ret = ret | ToolCursor::Ex_Negate;
  return ret;
}

//----------------------------------------------------------------------

void EraserTool::resetMulti() {
  m_isXsheetCell       = false;
  m_firstFrameSelected = false;
  m_firstRect.empty();
  m_selectingRect.empty();
  TTool::Application *app = TTool::getApplication();
  m_level                 = app->getCurrentLevel()->getLevel()
                ? app->getCurrentLevel()->getSimpleLevel()
                : 0;
  m_firstFrameId = m_veryFirstFrameId = getFrameId();
  m_firstStrokes.clear();
}

//----------------------------------------------------------------------

void EraserTool::multiUpdate(const TXshSimpleLevelP &level, TFrameId firstFid,
                             TFrameId lastFid, TRectD firstRect,
                             TRectD lastRect) {
  bool backward = false;
  if (firstFid > lastFid) {
    std::swap(firstFid, lastFid);
    backward = true;
  }
  assert(firstFid <= lastFid);
  std::vector<TFrameId> allFids;
  level->getFids(allFids);

  /*-- フレーム範囲に対応するFIdを取得 --*/
  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFid) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFid) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  std::wstring levelName = level->getName();

  /*-- FrameRangeの各フレームについて --*/
  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TToonzImageP ti = level->getFrame(fid, true);
    if (!ti) continue;
    /*--補間の係数を取得 --*/
    double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    /*--invertがONのとき、外側領域を4つのRectに分けてupdate--*/
    if (m_invertOption.getValue()) {
      TRect rect =
          convert(interpolateRect(firstRect, lastRect, backward ? 1 - t : t));
      TRectD rect01 = TRectD(TPointD(-100000., -100000.),
                             TPointD((double)rect.x0, 100000.));
      update(ti, rect01, level, true, fid);
      TRectD rect02 =
          TRectD(convert(rect.getP01()), TPointD((double)rect.x1, 100000.));
      update(ti, rect02, level, true, fid);
      TRectD rect03 =
          TRectD(TPointD((double)rect.x0, -100000.), convert(rect.getP10()));
      update(ti, rect03, level, true, fid);
      TRectD rect04 =
          TRectD(TPointD((double)rect.x1, -100000.), TPointD(100000., 100000.));
      update(ti, rect04, level, true, fid);
    } else
      update(ti, interpolateRect(firstRect, lastRect, backward ? 1 - t : t),
             level, true, fid);

    TRect savebox;
    TRop::computeBBox(ti->getRaster(), savebox);
    ti->setSavebox(savebox);

    level->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//----------------------------------------------------------------------

void EraserTool::update(const TToonzImageP &ti, TRectD selArea,
                        const TXshSimpleLevelP &level, bool multi,
                        const TFrameId &frameId) {
  if (m_selectingRect.x0 > m_selectingRect.x1) {
    selArea.x1 = m_selectingRect.x0;
    selArea.x0 = m_selectingRect.x1;
  }
  if (m_selectingRect.y0 > m_selectingRect.y1) {
    selArea.y1 = m_selectingRect.y0;
    selArea.y0 = m_selectingRect.y1;
  }
  if (selArea.getLx() < 1 || selArea.getLy() < 1) return;
  bool selective = m_currentStyle.getValue();
  int styleId    = TTool::getApplication()->getCurrentLevelStyleIndex();

  TRasterCM32P raster   = ti->getRaster();
  TTileSetCM32 *tileSet = new TTileSetCM32(raster->getSize());
  tileSet->add(raster, ToonzImageUtils::convertWorldToRaster(selArea, ti));
  TUndo *undo;

  std::wstring eraseType =
      (m_eraseType.getValue() == RECTERASE && m_polyline.hasSymmetryBrushes())
          ? POLYLINEERASE
          : m_eraseType.getValue();

  std::wstring inkPaint = m_colorType.getValue();
  undo                  = new RectRasterUndo(
      tileSet, selArea, TStroke(), selective ? styleId : -1, eraseType,
      inkPaint, level.getPointer(), selective, m_invertOption.getValue(),
      m_pencil.getValue(), frameId);

  ToonzImageUtils::eraseRect(ti, selArea, selective ? styleId : -1,
                             inkPaint == LINES || inkPaint == ALL,
                             inkPaint == AREAS || inkPaint == ALL);

  TUndoManager::manager()->add(undo);
}

//----------------------------------------------------------------------

void EraserTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_selecting = true;
  TImageP image(getImage(true));

  TRectD invalidateRect;
  if (TToonzImageP ti = image) {
    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    TPointD dpiScale       = getViewer()->getDpiScale();
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();

    if (m_eraseType.getValue() == RECTERASE) {
      if (m_multi.getValue() && m_firstRect.isEmpty()) {
        invalidateRect = m_selectingRect;
        m_selectingRect.empty();
        invalidate(invalidateRect.enlarge(2));
      }
      m_selectingRect.x0 = pos.x;
      m_selectingRect.y0 = pos.y;
      m_selectingRect.x1 = pos.x + 1;
      m_selectingRect.y1 = pos.y + 1;
      invalidateRect     = m_selectingRect.enlarge(2);

      if (!m_invertOption.getValue() && symmetryTool &&
          symmetryTool->isGuideEnabled()) {
        // We'll use polyline
        m_polyline.reset();
        m_polyline.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                      symmObj.getCenterPoint(),
                                      symmObj.isUsingLineSymmetry(), dpiScale);
        m_polyline.setRectangle(
            TPointD(m_selectingRect.x0, m_selectingRect.y0),
            TPointD(m_selectingRect.x1, m_selectingRect.y1));
      }
    }
    if (m_eraseType.getValue() == NORMALERASE) {
      TRasterCM32P raster = ti->getRaster();
      TPointD fixedPos    = fixMousePos(pos);
      TThickPoint intPos;
      /*--Areasタイプの時は常にPencilと同じ消し方にする--*/
      if (m_pencil.getValue() || m_colorType.getValue() == AREAS)
        intPos = TThickPoint(fixedPos + convert(raster->getCenter()),
                             m_toolSize.getValue());
      else
        intPos = TThickPoint(fixedPos + convert(raster->getCenter()),
                             m_toolSize.getValue() - 1);
      int currentStyle = 0;
      if (m_currentStyle.getValue())
        currentStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
      m_tileSet      = new TTileSetCM32(raster->getSize());
      m_tileSaver    = new TTileSaverCM32(raster, m_tileSet);
      TPointD halfThick(m_toolSize.getValue() * 0.5,
                        m_toolSize.getValue() * 0.5);
      invalidateRect = TRectD(fixedPos - halfThick, fixedPos + halfThick);
      if (m_hardness.getValue() == 100 || m_pencil.getValue() ||
          m_colorType.getValue() == AREAS) {
        if (m_colorType.getValue() == LINES) {
          m_colorTypeEraser = INK;
        }
        if (m_colorType.getValue() == AREAS) m_colorTypeEraser = PAINT;
        if (m_colorType.getValue() == ALL) m_colorTypeEraser   = INKNPAINT;
        m_normalEraser = new CMRasterBrush(
            raster, ERASE, m_colorTypeEraser, 0, intPos,
            m_currentStyle.getValue(), currentStyle, false,
            !(m_pencil.getValue() || m_colorType.getValue() == AREAS));

        if (symmetryTool && symmetryTool->isGuideEnabled()) {
          TPointD dpiScale       = getViewer()->getDpiScale();
          SymmetryObject symmObj = symmetryTool->getSymmetryObject();
          m_normalEraser->addSymmetryBrushes(
              symmObj.getLines(), symmObj.getRotation(),
              symmObj.getCenterPoint(), symmObj.isUsingLineSymmetry(),
              dpiScale);
        }

        m_tileSaver->save(m_normalEraser->getLastRect());
        m_normalEraser->generateLastPieceOfStroke(
            m_pencil.getValue() || m_colorType.getValue() == AREAS);
      } else {
        m_points.clear();
        m_backupRas = raster->clone();
        m_workRas   = TRaster32P(raster->getSize());
        m_workRas->clear();
        TPointD center = raster->getCenterD();
        TThickPoint point(fixedPos + center, m_toolSize.getValue());
        m_points.push_back(point);
        m_blurredEraser = new RasterBlurredBrush(
            m_workRas, m_toolSize.getValue(), m_brushPad, false);

        if (symmetryTool && symmetryTool->isGuideEnabled()) {
          TPointD dpiScale       = getViewer()->getDpiScale();
          SymmetryObject symmObj = symmetryTool->getSymmetryObject();
          m_blurredEraser->addSymmetryBrushes(
              symmObj.getLines(), symmObj.getRotation(),
              symmObj.getCenterPoint(), symmObj.isUsingLineSymmetry(),
              dpiScale);
        }

        TRect bbox = m_blurredEraser->getBoundFromPoints(m_points);
        m_tileSaver->save(bbox);
        m_blurredEraser->addPoint(point, 1);
        m_blurredEraser->eraseDrawing(raster, m_backupRas, bbox,
                                    m_currentStyle.getValue(), currentStyle,
                                    m_colorType.getValue());
      }
      /*--- 現在のFidを記憶する ---*/
      m_workingFrameId = getFrameId();
    }
    if (m_eraseType.getValue() == FREEHANDERASE ||
        m_eraseType.getValue() == POLYLINEERASE ||
        m_eraseType.getValue() == SEGMENTERASE) {
      int col   = getColumnIndex();
      m_enabled = col >= 0;

      if (!m_enabled) return;

      if (m_multi.getValue() && m_firstStrokes.size() && !m_firstFrameSelected) {
        m_firstStrokes.clear();
        invalidate();
      }

      m_active = true;
      m_track.reset();
      if (!m_invertOption.getValue() && symmetryTool &&
          symmetryTool->isGuideEnabled()) {
        m_track.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                   symmObj.getCenterPoint(),
                                   symmObj.isUsingLineSymmetry(), dpiScale);
      }
      m_firstPos        = pos;
      double pixelSize2 = getPixelSize() * getPixelSize();
      m_track.add(TThickPoint(pos, m_thick), pixelSize2);

      if (m_eraseType.getValue() == POLYLINEERASE) {
        if (!m_invertOption.getValue() && symmetryTool &&
            symmetryTool->isGuideEnabled() &&
            !m_polyline.hasSymmetryBrushes()) {
          m_polyline.addSymmetryBrushes(
              symmObj.getLines(), symmObj.getRotation(),
              symmObj.getCenterPoint(), symmObj.isUsingLineSymmetry(),
              dpiScale);
        }

        if (m_polyline.empty() || m_polyline.back() != pos)
          m_polyline.push_back(pos);
      }

      int maxThick = 2 * m_thick;
      TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
      invalidateRect = TRectD(pos - halfThick, pos + halfThick);
    }
  }
  invalidate(invalidateRect.enlarge(2));
  m_isLeftButtonPressed = true;
}

//----------------------------------------------------------------------

void EraserTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  /*-- 先にLeftButtonDownに入っていなければreturn --*/
  if (!m_isLeftButtonPressed) return;

  double pixelSize2 = getPixelSize() * getPixelSize();

  m_mousePos = pos;
  m_brushPos = fixMousePos(pos);
  if (!m_selecting) return;

  TImageP image(getImage(true));

  if (TToonzImageP ti = image) {
    TRectD invalidateRect;
    TPointD rasCenter = ti->getRaster()->getCenterD();
    if (m_eraseType.getValue() == RECTERASE) {
      TRectD oldRect = m_selectingRect;
      if (oldRect.x0 > oldRect.x1) std::swap(oldRect.x1, oldRect.x0);
      if (oldRect.y0 > oldRect.y1) std::swap(oldRect.y1, oldRect.y0);
      m_selectingRect.x1 = pos.x;
      m_selectingRect.y1 = pos.y;
      invalidateRect     = m_selectingRect;
      if (invalidateRect.x0 > invalidateRect.x1)
        std::swap(invalidateRect.x1, invalidateRect.x0);
      if (invalidateRect.y0 > invalidateRect.y1)
        std::swap(invalidateRect.y1, invalidateRect.y0);
      invalidateRect += oldRect;

      if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
        m_polyline.clear();
        m_polyline.setRectangle(
            TPointD(m_selectingRect.x0, m_selectingRect.y0),
            TPointD(m_selectingRect.x1, m_selectingRect.y1));
        invalidate();
      } else
        invalidate(invalidateRect.enlarge(2));
    }
    if (m_eraseType.getValue() == NORMALERASE) {
      TPointD fixedPos = fixMousePos(pos);
      if (m_normalEraser &&
          (m_hardness.getValue() == 100 || m_pencil.getValue() ||
           m_colorType.getValue() == AREAS)) {
        TPointD pp(fixedPos.x, fixedPos.y);
        TThickPoint intPos;
        if (m_pencil.getValue() || m_colorType.getValue() == AREAS)
          intPos = TThickPoint(pp + convert(ti->getRaster()->getCenter()),
                               m_toolSize.getValue());
        else
          intPos = TThickPoint(pp + convert(ti->getRaster()->getCenter()),
                               m_toolSize.getValue() - 1);

        m_normalEraser->add(intPos);
        if (ti) {
          m_tileSaver->save(m_normalEraser->getLastRect());
          m_normalEraser->generateLastPieceOfStroke(
              m_pencil.getValue() || m_colorType.getValue() == AREAS);
          std::vector<TThickPoint> brushPoints =
              m_normalEraser->getPointsSequence();
          int m = (int)brushPoints.size();
          std::vector<TThickPoint> points;
          if (m_normalEraser->hasSymmetryBrushes())
            points = brushPoints;
          else {
            if (m == 3) {
              points.push_back(brushPoints[0]);
              points.push_back(brushPoints[1]);
            } else {
              points.push_back(brushPoints[m - 4]);
              points.push_back(brushPoints[m - 3]);
              points.push_back(brushPoints[m - 2]);
            }
          }
          invalidateRect = ToolUtils::getBounds(points, m_toolSize.getValue());
        }
      } else {
        assert(m_workRas.getPointer() && m_backupRas.getPointer());

        TThickPoint old = m_points.back();
        if (norm2(fixedPos - old) < 4) return;

        int thickness = m_toolSize.getValue();
        TThickPoint point(fixedPos + rasCenter, thickness);
        TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
        m_points.push_back(mid);
        m_points.push_back(point);

        int currentStyle = 0;
        if (m_currentStyle.getValue())
          currentStyle = TTool::getApplication()->getCurrentLevelStyleIndex();

        TRect bbox;
        int m = (int)m_points.size();
        std::vector<TThickPoint> points;
        if (m == 3) {
          // ho appena cominciato. devo disegnare un segmento
          TThickPoint pa = m_points.front();
          points.push_back(pa);
          points.push_back(mid);
          bbox           = m_blurredEraser->getBoundFromPoints(points);
          m_blurredEraser->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
        } else {
          points.push_back(m_points[m - 4]);
          points.push_back(old);
          points.push_back(mid);
          bbox           = m_blurredEraser->getBoundFromPoints(points);
          m_blurredEraser->addArc(m_points[m - 4], old, mid, 1, 1);
        }
        m_tileSaver->save(bbox);
        m_blurredEraser->eraseDrawing(ti->getRaster(), m_backupRas, bbox,
                                    m_currentStyle.getValue(), currentStyle,
                                    m_colorType.getValue());

        invalidateRect = ToolUtils::getBounds(points, thickness);
        std::vector<TThickPoint> symmPts =
            m_blurredEraser->getSymmetryPoints(points);
        points.insert(points.end(), symmPts.begin(), symmPts.end());
      }
      invalidate(invalidateRect.enlarge(2) - rasCenter);
    }
    if (m_eraseType.getValue() == FREEHANDERASE ||
        m_eraseType.getValue() == SEGMENTERASE) {
      if (!m_enabled || !m_active) return;
      m_track.add(TThickPoint(pos, m_thick), pixelSize2);
      invalidate(m_track.getModifiedRegion());
    }
  }
}

//----------------------------------------------------------------------

void EraserTool::onImageChanged() {
  if (!m_multi.getValue()) return;
  TTool::Application *app = TTool::getApplication();
  TXshSimpleLevel *xshl   = 0;
  if (app->getCurrentLevel()->getLevel())
    xshl = app->getCurrentLevel()->getSimpleLevel();

  if (!xshl || m_level.getPointer() != xshl ||
      (m_selectingRect.isEmpty() && !m_firstStrokes.size()))
    resetMulti();
  else if (m_firstFrameId == getFrameId())
    m_firstFrameSelected = false;  // nel caso sono passato allo stato 1 e torno
                                   // all'immagine iniziale, torno allo stato
                                   // iniziale
  else {                           // cambio stato.
    m_firstFrameSelected = true;
    if (m_eraseType.getValue() != FREEHANDERASE &&
        m_eraseType.getValue() != POLYLINEERASE &&
        m_eraseType.getValue() != SEGMENTERASE) {
      assert(!m_selectingRect.isEmpty());
      m_firstRect = m_selectingRect;
    }
  }
}

//----------------------------------------------------------------------

void EraserTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  /*--先にLeftButtonDownに入っていなければ処理をしない--*/
  if (!m_isLeftButtonPressed) return;
  m_isLeftButtonPressed = false;

  if (!m_selecting) return;
  TImageP image(getImage(true));
  if (TToonzImageP ti = image) {
    bool updateSavebox = true;
    if (m_eraseType.getValue() == RECTERASE) {
      if (m_selectingRect.x0 > m_selectingRect.x1)
        std::swap(m_selectingRect.x1, m_selectingRect.x0);
      if (m_selectingRect.y0 > m_selectingRect.y1)
        std::swap(m_selectingRect.y1, m_selectingRect.y0);

      if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
        // We'll use polyline
        m_polyline.clear();
        m_polyline.setRectangle(
            TPointD(m_selectingRect.x0, m_selectingRect.y0),
            TPointD(m_selectingRect.x1, m_selectingRect.y1));
      }

      if (m_multi.getValue()) {
        TTool::Application *app = TTool::getApplication();
        if (m_firstFrameSelected) {
          m_selecting = false;

          if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
            // We'll use polyline
            TFrameId tmp = getCurrentFid();
            std::vector<TStroke *> lastStrokes;
            for (int i = 0; i < m_polyline.getBrushCount(); i++)
              lastStrokes.push_back(m_polyline.makeRectangleStroke(i));
            multiAreaEraser(m_level, m_firstFrameId, tmp, m_firstStrokes,
                            lastStrokes);
          } else {
            multiUpdate(m_level, m_firstFrameId, getFrameId(), m_firstRect,
                        m_selectingRect);
          }

          if (e.isShiftPressed()) {
            m_firstRect    = m_selectingRect;
            m_firstFrameId = getFrameId();
            m_firstStrokes.clear();
            if (m_polyline.size() > 1) {
              for (int i = 0; i < m_polyline.getBrushCount(); i++)
                m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
            }
            invalidate();
          } else {
            if (m_isXsheetCell) {
              app->getCurrentColumn()->setColumnIndex(m_currCell.first);
              app->getCurrentFrame()->setFrame(m_currCell.second);
            } else
              app->getCurrentFrame()->setFid(m_veryFirstFrameId);
            resetMulti();
            m_polyline.reset();
          }
        } else {
          m_firstStrokes.clear();
          if (m_polyline.size() > 1) {
            for (int i = 0; i < m_polyline.getBrushCount(); i++)
              m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
          }
          m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
          m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
          invalidate();
        }
      } else {
        TTool::Application *app   = TTool::getApplication();
        TXshLevel *level          = app->getCurrentLevel()->getLevel();
        TXshSimpleLevelP simLevel = level->getSimpleLevel();
        TFrameId frameId          = getFrameId();
        if (m_invertOption.getValue()) {
          TUndoManager::manager()->beginBlock();
          TRectD rect      = m_selectingRect;
          TRectD worldBBox = ToonzImageUtils::convertRasterToWorld(
              ti->getRaster()->getBounds(), ti);
          rect *= ToonzImageUtils::convertRasterToWorld(ti->getSavebox(), ti);

          TRectD rect01 =
              TRectD(worldBBox.getP00(), TPointD(rect.x0, worldBBox.y1));
          if (rect01.getLx() > 0 && rect01.getLy() > 0)
            update(ti, rect01, simLevel, false, frameId);

          TRectD rect02 = TRectD(rect.getP01(), TPointD(rect.x1, worldBBox.y1));
          if (rect02.getLx() > 0 && rect02.getLy() > 0)
            update(ti, rect02, simLevel, false, frameId);

          TRectD rect03 = TRectD(TPointD(rect.x0, worldBBox.y0), rect.getP10());
          if (rect03.getLx() > 0 && rect03.getLy() > 0)
            update(ti, rect03, simLevel, false, frameId);

          TRectD rect04 = TRectD(TPointD(rect.x1, worldBBox.y0),
                                 TPointD(worldBBox.x1, worldBBox.y1));
          if (rect04.getLx() > 0 && rect04.getLy() > 0)
            update(ti, rect04, simLevel, false, frameId);
          TUndoManager::manager()->endBlock();
          invalidate();
        } else if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
          TUndoManager::manager()->beginBlock();

          int styleId     = app->getCurrentLevelStyleIndex();
          TStroke *stroke = m_polyline.makeRectangleStroke();
          eraseStroke(image, stroke, POLYLINEERASE, m_colorType.getValue(),
                      m_invertOption.getValue(), m_currentStyle.getValue(),
                      m_pencil.getValue(), styleId, simLevel, frameId);

          for (int i = 1; i < m_polyline.getBrushCount(); i++) {
            TStroke *symmStroke = m_polyline.makeRectangleStroke(i);
            symmStroke->setStyle(stroke->getStyle());
            eraseStroke(image, symmStroke, POLYLINEERASE,
                        m_colorType.getValue(), m_invertOption.getValue(),
                        m_currentStyle.getValue(), m_pencil.getValue(), styleId,
                        simLevel, frameId);
          }

          TUndoManager::manager()->endBlock();
          invalidate();
        } else {
          update(ti, m_selectingRect, simLevel, false, frameId);
          invalidate(m_selectingRect.enlarge(2));
        }

        m_selectingRect.empty();
        m_polyline.reset();

        TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
        notifyImageChanged();
      }
    }
    if (m_eraseType.getValue() == NORMALERASE) {
      TPointD fixedPos          = fixMousePos(pos);
      TTool::Application *app   = TTool::getApplication();
      int currentStyle          = app->getCurrentLevelStyleIndex();
      TXshLevel *level          = app->getCurrentLevel()->getLevel();
      TXshSimpleLevelP simLevel = level->getSimpleLevel();

      /*--Erase中にフレームが動いても、クリック時のFidに対してUndoを記録する--*/
      TFrameId frameId =
          m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId;

      SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
          TTool::getTool("T_Symmetry", TTool::RasterImage));

      if (m_normalEraser &&
          (m_hardness.getValue() == 100 || m_pencil.getValue() ||
           m_colorType.getValue() == AREAS)) {
        double symmetryLines = 0;
        double rotation      = 0;
        bool useLineSymmetry = false;
        TPointD centerPoint(0, 0);
        if (symmetryTool && symmetryTool->isGuideEnabled()) {
          SymmetryObject symmObj = symmetryTool->getSymmetryObject();
          symmetryLines          = symmObj.getLines();
          rotation               = symmObj.getRotation();
          centerPoint            = symmObj.getCenterPoint();
          useLineSymmetry        = symmObj.isUsingLineSymmetry();
        }

        TPointD dpiScale = getViewer()->getDpiScale();

        TUndoManager::manager()->add(new RasterEraserUndo(
            m_tileSet, m_normalEraser->getPointsSequence(true),
            m_colorTypeEraser, 0, m_normalEraser->isSelective(), currentStyle,
            simLevel.getPointer(), frameId,
            (m_pencil.getValue() || m_colorType.getValue() == AREAS), dpiScale,
            symmetryLines, rotation, centerPoint, useLineSymmetry));
        app->getCurrentTool()->getTool()->notifyImageChanged(frameId);
        app->getCurrentXsheet()->notifyXsheetChanged();
        delete m_normalEraser;
        m_normalEraser = 0;
      } else {
        if (m_points.size() != 1) {
          TPointD rasCenter = ti->getRaster()->getCenterD();
          TThickPoint point(fixedPos + rasCenter, m_toolSize.getValue());
          m_points.push_back(point);
          int m = m_points.size();
          std::vector<TThickPoint> points;
          points.push_back(m_points[m - 3]);
          points.push_back(m_points[m - 2]);
          points.push_back(m_points[m - 1]);
          TRect bbox = m_blurredEraser->getBoundFromPoints(points);
          m_tileSaver->save(bbox);
          m_blurredEraser->addArc(points[0], points[1], points[2], 1, 1);
          m_blurredEraser->eraseDrawing(ti->getRaster(), m_backupRas, bbox,
                                        m_currentStyle.getValue(), currentStyle,
                                        m_colorType.getValue());

          std::vector<TThickPoint> allPts = points;
          std::vector<TThickPoint> symmPts =
              m_blurredEraser->getSymmetryPoints(points);
          allPts.insert(allPts.end(), symmPts.begin(), symmPts.end());

          TRectD invalidateRect =
              ToolUtils::getBounds(allPts, m_toolSize.getValue());

          invalidate(invalidateRect.enlarge(2) - rasCenter);
        }

        m_backupRas = TRasterCM32P();
        m_workRas   = TRaster32P();
        delete m_blurredEraser;
        m_blurredEraser = 0;

        double symmetryLines = 0;
        double rotation      = 0;
        bool useLineSymmetry = false;
        TPointD centerPoint(0, 0);
        if (symmetryTool && symmetryTool->isGuideEnabled()) {
          SymmetryObject symmObj = symmetryTool->getSymmetryObject();
          symmetryLines          = symmObj.getLines();
          rotation               = symmObj.getRotation();
          centerPoint            = symmObj.getCenterPoint();
          useLineSymmetry        = symmObj.isUsingLineSymmetry();
        }

        TPointD dpiScale = getViewer()->getDpiScale();

        TUndoManager::manager()->add(new RasterBluredEraserUndo(
            m_tileSet, m_points, currentStyle, m_currentStyle.getValue(),
            simLevel.getPointer(), frameId, m_toolSize.getValue(),
            m_hardness.getValue() * 0.01, m_colorType.getValue(), dpiScale,
            symmetryLines, rotation, centerPoint, useLineSymmetry));
      }
      delete m_tileSaver;
      m_tileSaver = 0;
      /*-- 作業したフレームをシグナルで知らせる --*/
      notifyImageChanged(frameId);
      /*-- 作業フレームをリセット --*/
      m_workingFrameId = TFrameId();
    }
    if (m_eraseType.getValue() == FREEHANDERASE) {
      bool isValid = m_enabled && m_active;
      m_enabled = m_active = false;
      if (!isValid) return;
      if (m_track.isEmpty()) return;
      double pixelSize2 = getPixelSize() * getPixelSize();
      m_track.add(TThickPoint(m_firstPos, m_thick), pixelSize2);
      m_track.filterPoints();
      double error    = (30.0 / 11) * sqrt(pixelSize2);
      TStroke *stroke = m_track.makeStroke(error);

      stroke->setStyle(1);

      TTool::Application *app = TTool::getApplication();
      int styleId             = app->getCurrentLevelStyleIndex();
      if (m_multi.getValue())  // stroke multi
      {
        if (m_firstFrameSelected) {
          TFrameId tmp = getFrameId();
          if (m_firstStrokes.size() && stroke) {
            std::vector<TStroke *> lastStrokes;
            for (int i = 0; i < m_track.getBrushCount(); i++)
              lastStrokes.push_back(m_track.makeStroke(error, i));
            multiAreaEraser(m_level, m_firstFrameId, tmp, m_firstStrokes, lastStrokes);
          }
          notifyImageChanged();
          if (e.isShiftPressed()) {
            m_firstStrokes.clear();
            for (int i = 0; i < m_track.getBrushCount(); i++)
              m_firstStrokes.push_back(m_track.makeStroke(error, i));
            invalidate();
            m_firstFrameId = getFrameId();
          } else {
            if (m_isXsheetCell) {
              app->getCurrentColumn()->setColumnIndex(m_currCell.first);
              app->getCurrentFrame()->setFrame(m_currCell.second);
            } else
              app->getCurrentFrame()->setFid(m_veryFirstFrameId);
            resetMulti();
            delete stroke;
          }
        } else  // primo frame
        {
          m_firstStrokes.clear();
          for (int i = 0; i < m_track.getBrushCount(); i++)
            m_firstStrokes.push_back(m_track.makeStroke(error, i));
          m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
          m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
          invalidate();
        }
      } else  // stroke non multi
      {
        if (!getImage(true)) return;
        TXshLevel *level          = app->getCurrentLevel()->getLevel();
        TXshSimpleLevelP simLevel = level->getSimpleLevel();
        TFrameId frameId          = getFrameId();

        if (m_track.hasSymmetryBrushes()) TUndoManager::manager()->beginBlock();

        eraseStroke(image, stroke, m_eraseType.getValue(),
                    m_colorType.getValue(), m_invertOption.getValue(),
                    m_currentStyle.getValue(), m_pencil.getValue(), styleId,
                    simLevel, frameId);

        if (m_track.hasSymmetryBrushes()) {
          std::vector<TStroke *> symmStrokes =
              m_track.makeSymmetryStrokes(error);
          for (int i = 0; i < symmStrokes.size(); i++) {
            symmStrokes[i]->setStyle(stroke->getStyle());
            eraseStroke(image, symmStrokes[i], m_eraseType.getValue(),
                        m_colorType.getValue(), m_invertOption.getValue(),
                        m_currentStyle.getValue(), m_pencil.getValue(), styleId,
                        simLevel, frameId);
          }

          TUndoManager::manager()->endBlock();
        }

        notifyImageChanged();
        if (m_invertOption.getValue() || m_track.hasSymmetryBrushes())
          invalidate();
        else
          invalidate(stroke->getBBox().enlarge(2));
      }
      m_track.reset();
    }
    if (m_eraseType.getValue() == SEGMENTERASE) {
      bool isValid = m_enabled && m_active;
      m_enabled = m_active = false;
      if (!isValid) return;
      if (m_track.isEmpty()) return;
      double pixelSize2 = getPixelSize() * getPixelSize();

      m_track.filterPoints();
      double error    = (30.0 / 11) * sqrt(pixelSize2);
      TStroke *stroke = m_track.makeStroke(error);

      stroke->setStyle(1);

      bool selective          = m_currentStyle.getValue();
      TTool::Application *app = TTool::getApplication();
      int styleId             = app->getCurrentLevelStyleIndex();
      if (m_multi.getValue())  // stroke multi
      {
        if (m_firstFrameSelected) {
          TFrameId tmp = getFrameId();
          if (m_firstStrokes.size() && stroke) {
            std::vector<TStroke *> lastStrokes;
            for (int i = 0; i < m_track.getBrushCount(); i++)
              lastStrokes.push_back(m_track.makeStroke(error, i));
            multiAreaEraser(m_level, m_firstFrameId, tmp, m_firstStrokes,
                            lastStrokes);
          }
          notifyImageChanged();
          if (e.isShiftPressed()) {
            m_firstStrokes.clear();
            for (int i = 0; i < m_track.getBrushCount(); i++)
              m_firstStrokes.push_back(m_track.makeStroke(error, i));
            invalidate();
            m_firstFrameId = getFrameId();
          } else {
            if (m_isXsheetCell) {
              app->getCurrentColumn()->setColumnIndex(m_currCell.first);
              app->getCurrentFrame()->setFrame(m_currCell.second);
            } else
              app->getCurrentFrame()->setFid(m_veryFirstFrameId);
            resetMulti();
            delete stroke;
          }
        } else  // primo frame
        {
          m_firstStrokes.clear();
          for (int i = 0; i < m_track.getBrushCount(); i++)
            m_firstStrokes.push_back(m_track.makeStroke(error, i));
          m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
          m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
          invalidate();
        }
      } else  // stroke non multi
      {
        TUndoManager::manager()->beginBlock();
        int length              = (int)stroke->getLength();
        TTool::Application *app = TTool::getApplication();
        if (!app) return;

        TXshLevel *xl       = app->getCurrentLevel()->getLevel();
        TXshSimpleLevel *sl = xl ? xl->getSimpleLevel() : 0;

        TToonzImageP ti = TImageP(getImage(true));
        if (!ti) return;
        TRasterCM32P ras = ti->getRaster();
        if (!ras) return;

        updateSavebox = false;
        for (int i = 0; i < length; i++) {
          TPointD lengthAt = stroke->getPointAtLength(i);
          TPointD tmpPos(lengthAt);
          TPoint ipos((int)(tmpPos.x + ras->getLx() / 2),
                      (int)(tmpPos.y + ras->getLy() / 2));
          if (!ras->getBounds().contains(ipos)) continue;
          TPixelCM32 pix = ras->pixels(ipos.y)[ipos.x];
          if ((selective && pix.getInk() != styleId) || pix.isPurePaint())
            continue;
          TPointD dpiScale = getViewer()->getDpiScale();
          updateSavebox |= eraseSegment(
              getImage(true), tmpPos, sl, getCurrentFid(), selective, styleId,
              m_eraseOnlySavebox.getValue(), dpiScale);
        }

        TPointD mousePos = pos;

        TUndoManager::manager()->endBlock();
        invalidate();
      }
      m_track.clear();
    }
    if (updateSavebox) ToolUtils::updateSaveBox();
  }

  m_selecting = false;
}

//----------------------------------------------------------------------

void EraserTool::leftButtonDoubleClick(const TPointD &pos,
                                       const TMouseEvent &e) {
  TStroke *stroke;
  TTool::Application *app = TTool::getApplication();
  if (m_polyline.size() <= 1) {
    resetMulti();
    return;
  }
  if (m_polyline.back() != pos) m_polyline.push_back(pos);
  if (m_polyline.back() != m_polyline.front())
    m_polyline.push_back(m_polyline.front());
  stroke = m_polyline.makePolylineStroke();
  assert(stroke->getPoint(0) == stroke->getPoint(1));

  int styleId = app->getCurrentLevelStyleIndex();
  if (m_multi.getValue())  // stroke multi
  {
    if (m_firstFrameSelected) {
      TFrameId tmp = getFrameId();
      if (m_firstStrokes.size() && stroke) {
        std::vector<TStroke *> lastStrokes;
        for (int i = 0; i < m_polyline.getBrushCount(); i++)
          lastStrokes.push_back(m_polyline.makePolylineStroke(i));
        multiAreaEraser(m_level, m_firstFrameId, tmp, m_firstStrokes,
                        lastStrokes);
      }
      if (e.isShiftPressed()) {
        m_firstStrokes.clear();
        for (int i = 0; i < m_polyline.getBrushCount(); i++)
          m_firstStrokes.push_back(m_polyline.makePolylineStroke(i));
        invalidate();
        m_firstFrameId = getFrameId();
      } else {
        if (m_isXsheetCell) {
          app->getCurrentColumn()->setColumnIndex(m_currCell.first);
          app->getCurrentFrame()->setFrame(m_currCell.second);
        } else
          app->getCurrentFrame()->setFid(m_veryFirstFrameId);
        resetMulti();
        delete stroke;
      }
    } else  // primo frame
    {
      m_firstStrokes.clear();
      for (int i = 0; i < m_track.getBrushCount(); i++)
        m_firstStrokes.push_back(m_polyline.makePolylineStroke(i));
      m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
      m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
      invalidate();
    }
  } else {
    if (!getImage(true)) return;
    TXshLevel *level          = app->getCurrentLevel()->getLevel();
    TXshSimpleLevelP simLevel = level->getSimpleLevel();
    TFrameId frameId          = getFrameId();
    TToonzImageP ti           = (TToonzImageP)getImage(true);

    if (m_polyline.hasSymmetryBrushes()) TUndoManager::manager()->beginBlock();

    eraseStroke(ti, stroke, m_eraseType.getValue(), m_colorType.getValue(),
                m_invertOption.getValue(), m_currentStyle.getValue(),
                m_pencil.getValue(), styleId, simLevel, frameId);

    if (m_polyline.hasSymmetryBrushes()) {
      for (int i = 1; i < m_polyline.getBrushCount(); i++) {
        TStroke *symmStroke = m_polyline.makePolylineStroke(i);
        symmStroke->setStyle(stroke->getStyle());
        eraseStroke(ti, symmStroke, m_eraseType.getValue(),
                    m_colorType.getValue(), m_invertOption.getValue(),
                    m_currentStyle.getValue(), m_pencil.getValue(), styleId,
                    simLevel, frameId);
      }

      TUndoManager::manager()->endBlock();
    }

    notifyImageChanged();
    if (m_invertOption.getValue() || m_polyline.hasSymmetryBrushes())
      invalidate();
    else
      invalidate(stroke->getBBox().enlarge(2));
  }
  m_polyline.reset();
}

//----------------------------------------------------------------------

bool EraserTool::onPropertyChanged(std::string propertyName) {
  /*--- 変更されたPropertyに合わせて処理を分ける ---*/
  if (propertyName == m_eraseType.getName()) {
    /*--- polylineにした時、前回のpolyline選択をクリアする ---*/
    if (m_eraseType.getValue() == POLYLINEERASE) m_polyline.reset();
    EraseType = ::to_string(m_eraseType.getValue());
  }

  else if (propertyName == m_toolSize.getName()) {
    EraseSize     = m_toolSize.getValue();
    m_cleanerSize = m_toolSize.getValue();

    m_brushPad = ToolUtils::getBrushPad(m_toolSize.getValue(),
                                        m_hardness.getValue() * 0.01);
  }

  else if (propertyName == m_invertOption.getName())
    EraseInvert = m_invertOption.getValue();

  else if (propertyName == m_currentStyle.getName())
    EraseSelective = m_currentStyle.getValue();

  else if (propertyName == m_multi.getName()) {
    if (m_multi.getValue()) resetMulti();
    EraseRange = m_multi.getValue();
  }

  else if (propertyName == m_pencil.getName()) {
    ErasePencil = m_pencil.getValue();
  }

  else if (propertyName == m_colorType.getName()) {
    EraseColorType = ::to_string(m_colorType.getValue());
    /*-- ColorModelのCursor更新のためにSIGNALを出す --*/
    TTool::getApplication()->getCurrentTool()->notifyToolChanged();
  }

  else if (propertyName == m_hardness.getName()) {
    EraseHardness = m_hardness.getValue();
    m_brushPad    = ToolUtils::getBrushPad(m_toolSize.getValue(),
                                        m_hardness.getValue() * 0.01);
  } else if (propertyName == m_eraseOnlySavebox.getName()) {
    EraseOnlySavebox = (int)(m_eraseOnlySavebox.getValue());
  }

  if (propertyName == m_hardness.getName() ||
      propertyName == m_toolSize.getName()) {
    m_brushPad = ToolUtils::getBrushPad(m_toolSize.getValue(),
                                        m_hardness.getValue() * 0.01);
    TRectD rect(m_brushPos - TPointD(EraseSize + 2, EraseSize + 2),
                m_brushPos + TPointD(EraseSize + 2, EraseSize + 2));
    invalidate(rect);
  }

  /*--- Erase動作中にプロパティが変わったら、終了処理を行う ---*/
  if (m_isLeftButtonPressed) {
    storeUndoAndRefresh();
  }

  return true;
}

//----------------------------------------------------------------------

void EraserTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
#if (!defined(LINUX) && !defined(FREEBSD)) || QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
#endif

  struct Locals {
    EraserTool *m_this;

    void setValue(TIntProperty &prop, int value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addValue(TIntProperty &prop, double add) {
      const TIntProperty::Range &range = prop.getRange();
      setValue(prop,
               tcrop<double>(prop.getValue() + add, range.first, range.second));
    }

  } locals = {this};

  switch (e.getModifiersMask()) {
  case TMouseEvent::ALT_KEY: {
    // User wants to alter the maximum brush size
    const TPointD &diff = pos - m_mousePos;
    double add          = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;

    locals.addValue(m_toolSize, add);
    break;
  }

  default:
    m_brushPos = fixMousePos(pos);
    break;
  }

  m_mousePos = pos;
  invalidate();
}

//----------------------------------------------------------------------

void EraserTool::onEnter() {
  TToonzImageP ti(getImage(false));
  if (!ti) return;
  if (m_firstTime) {
    m_toolSize.setValue(EraseSize);
    m_eraseType.setValue(::to_wstring(EraseType.getValue()));
    m_currentStyle.setValue(EraseSelective ? 1 : 0);
    m_invertOption.setValue(EraseInvert ? 1 : 0);
    m_colorType.setValue(::to_wstring(EraseColorType.getValue()));
    m_multi.setValue(EraseRange ? 1 : 0);
    m_hardness.setValue(EraseHardness);
    m_pencil.setValue(ErasePencil);
    m_eraseOnlySavebox.setValue(EraseOnlySavebox ? 1 : 0);
    m_firstTime = false;
  }
  double x = m_toolSize.getValue();

  double minRange = 1;
  double maxRange = 100;

  double minSize = 0.1;
  double maxSize = 100;

  m_pointSize =
      (x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize;

  //  getApplication()->editImage();
  m_cleanerSize           = m_toolSize.getValue();
  TTool::Application *app = TTool::getApplication();
  m_level                 = app->getCurrentLevel()->getLevel()
                ? app->getCurrentLevel()->getSimpleLevel()
                : 0;
}

//----------------------------------------------------------------------

void EraserTool::onLeave() {
  m_pointSize   = -1;
  m_cleanerSize = 0;
}

//----------------------------------------------------------------------

void EraserTool::onActivate() {
  if (m_multi.getValue()) resetMulti();

  /*-- 他のツールからpolylineに入った時、前回のpolyline選択をクリアする --*/
  if (m_eraseType.getValue() == POLYLINEERASE) m_polyline.reset();

  onEnter();
  m_brushPad = ToolUtils::getBrushPad(m_toolSize.getValue(),
                                      m_hardness.getValue() * 0.01);
}

//------------------------------------------------------------------------------------------------------------

void EraserTool::multiAreaEraser(const TXshSimpleLevelP &sl, TFrameId &firstFid,
                                 TFrameId &lastFid, std::vector<TStroke *>firstStrokes,
                                 std::vector<TStroke *>lastStrokes) {
//  TStroke *first           = new TStroke();
//  TStroke *last            = new TStroke();
//  *first                   = *firstStroke;
//  *last                    = *lastStroke;
  TVectorImageP firstImage = new TVectorImage();
  TVectorImageP lastImage  = new TVectorImage();
  for (int i = 0; i < firstStrokes.size(); i++)
    firstImage->addStroke(firstStrokes[i]);
  for (int i = 0; i < lastStrokes.size(); i++)
    lastImage->addStroke(lastStrokes[i]);

  bool backward = false;
  if (firstFid > lastFid) {
    std::swap(firstFid, lastFid);
    backward = true;
  }
  assert(firstFid <= lastFid);
  std::vector<TFrameId> allFids;
  sl->getFids(allFids);

  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFid) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFid) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);
  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TImageP img = sl->getFrame(fid, true);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    if (m_eraseType.getValue() == SEGMENTERASE)
      doMultiSegmentEraser(img, backward ? 1 - t : t, sl, fid, firstImage,
                           lastImage);
    else
      doMultiEraser(img, backward ? 1 - t : t, sl, fid, firstImage, lastImage);
    sl->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//-------------------------------------------------------------------------------------------------

void EraserTool::doMultiSegmentEraser(const TImageP &img, double t,
                                      const TXshSimpleLevelP &sl,
                                      const TFrameId &fid,
                                      const TVectorImageP &firstImage,
                                      const TVectorImageP &lastImage) {
  //  ColorController::instance()->switchToLevelPalette();
  int styleId    = TTool::getApplication()->getCurrentLevelStyleIndex();
  bool selective = m_currentStyle.getValue();
  bool updateSavebox = false;
  TPointD dpiScale   = getViewer()->getDpiScale();
  if (t == 0) {
    TStroke *stroke = firstImage->getStroke(0);
    int length      = (int)stroke->getLength();
    int styleId     = 0;
    TToonzImageP ti = img;
    if (!ti) return;
    TRasterCM32P ras = ti->getRaster();
    if (!ras) return;

    for (int i = 0; i < length; i++) {
      TPointD lengthAt = stroke->getPointAtLength(i);
      TPointD tmpPos(lengthAt);
      TPoint ipos((int)(tmpPos.x + ras->getLx() / 2),
                  (int)(tmpPos.y + ras->getLy() / 2));
      if (!ras->getBounds().contains(ipos)) continue;
      TPixelCM32 pix = ras->pixels(ipos.y)[ipos.x];
      if ((selective && pix.getInk() != styleId) || pix.isPurePaint()) continue;
      updateSavebox |=
          eraseSegment(img, tmpPos, sl.getPointer(), fid, selective, styleId,
                       m_eraseOnlySavebox.getValue(), dpiScale);
    }
  }

  else if (t == 1) {
    TStroke *stroke = lastImage->getStroke(0);
    int length      = (int)stroke->getLength();
    int styleId     = 0;
    TToonzImageP ti = img;
    if (!ti) return;
    TRasterCM32P ras = ti->getRaster();
    if (!ras) return;

    for (int i = 0; i < length; i++) {
      TPointD lengthAt = stroke->getPointAtLength(i);
      TPointD tmpPos(lengthAt);
      TPoint ipos((int)(tmpPos.x + ras->getLx() / 2),
                  (int)(tmpPos.y + ras->getLy() / 2));
      if (!ras->getBounds().contains(ipos)) continue;
      TPixelCM32 pix = ras->pixels(ipos.y)[ipos.x];
      if ((selective && pix.getInk() != styleId) || pix.isPurePaint()) continue;
      updateSavebox |=
          eraseSegment(img, tmpPos, sl.getPointer(), fid, selective, styleId,
                       m_eraseOnlySavebox.getValue(), dpiScale);
    }
  } else {
    assert(firstImage->getStrokeCount() == 1);
    assert(lastImage->getStrokeCount() == 1);
    TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
    assert(vi->getStrokeCount() == 1);
    TStroke *stroke = vi->getStroke(0);
    int length      = (int)stroke->getLength();
    int styleId     = 0;
    TToonzImageP ti = img;
    if (!ti) return;
    TRasterCM32P ras = ti->getRaster();
    if (!ras) return;

    for (int i = 0; i < length; i++) {
      TPointD lengthAt = stroke->getPointAtLength(i);
      TPointD tmpPos(lengthAt);
      TPoint ipos((int)(tmpPos.x + ras->getLx() / 2),
                  (int)(tmpPos.y + ras->getLy() / 2));
      if (!ras->getBounds().contains(ipos)) continue;
      TPixelCM32 pix = ras->pixels(ipos.y)[ipos.x];
      if ((selective && pix.getInk() != styleId) || pix.isPurePaint()) continue;
      updateSavebox |=
          eraseSegment(img, tmpPos, sl.getPointer(), fid, selective, styleId,
                       m_eraseOnlySavebox.getValue(), dpiScale);
    }
  }
  if (updateSavebox) ToolUtils::updateSaveBox();

  TTool::Application *app = TTool::getApplication();
  if (!app) return;
}

//-------------------------------------------------------------------------------------------------

void EraserTool::doMultiEraser(const TImageP &img, double t,
                               const TXshSimpleLevelP &sl, const TFrameId &fid,
                               const TVectorImageP &firstImage,
                               const TVectorImageP &lastImage) {
  //  ColorController::instance()->switchToLevelPalette();
  int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
  std::wstring eraseType = m_eraseType.getValue() == RECTERASE
                               ? POLYLINEERASE
                               : m_eraseType.getValue();
  if (t == 0)
    for (int i = 0; i < firstImage->getStrokeCount(); i++)
      eraseStroke(img, firstImage->getStroke(i), eraseType,
                  m_colorType.getValue(), m_invertOption.getValue(),
                  m_currentStyle.getValue(), m_pencil.getValue(), styleId, sl,
                  fid);
  else if (t == 1)
    for (int i = 0; i < lastImage->getStrokeCount(); i++)
      eraseStroke(img, lastImage->getStroke(i), eraseType,
                  m_colorType.getValue(), m_invertOption.getValue(),
                  m_currentStyle.getValue(), m_pencil.getValue(), styleId, sl,
                  fid);
  else {
//    assert(firstImage->getStrokeCount() == 1);
//    assert(lastImage->getStrokeCount() == 1);
    TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
//    assert(vi->getStrokeCount() == 1);
    for (int i = 0; i < vi->getStrokeCount(); i++)
      eraseStroke(img, vi->getStroke(i), eraseType, m_colorType.getValue(),
                  m_invertOption.getValue(), m_currentStyle.getValue(),
                  m_pencil.getValue(), styleId, sl, fid);
  }
}

//--------------------------------------------------------------------------------------------------
/*!ドラッグ中にツールが切り替わった時、終了処理を行う
 */

void EraserTool::onDeactivate() {
  if (!m_isLeftButtonPressed || !m_selecting) return;

  storeUndoAndRefresh();
}

//--------------------------------------------------------------------------------------------------
/*-- Erase終了処理 --*/
void EraserTool::storeUndoAndRefresh() {
  m_isLeftButtonPressed = false;

  /*-- 各データのリフレッシュ --*/
  m_firstStrokes.clear();
  double symmetryLines = 0;
  double rotation      = 0;
  bool useLineSymmetry = false;
  TPointD centerPoint(0, 0);
  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (symmetryTool && symmetryTool->isGuideEnabled()) {
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();
    symmetryLines          = symmObj.getLines();
    rotation               = symmObj.getRotation();
    centerPoint            = symmObj.getCenterPoint();
    useLineSymmetry        = symmObj.isUsingLineSymmetry();
  }

  TPointD dpiScale = getViewer()->getDpiScale();

  if (m_normalEraser) {
    TUndoManager::manager()->add(new RasterEraserUndo(
        m_tileSet, m_normalEraser->getPointsSequence(true), m_colorTypeEraser,
        0, m_normalEraser->isSelective(),
        TTool::getApplication()->getCurrentLevelStyleIndex(),
        TTool::getApplication()
            ->getCurrentLevel()
            ->getLevel()
            ->getSimpleLevel(),
        m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId,
        (m_pencil.getValue() || m_colorType.getValue() == AREAS), dpiScale,
        symmetryLines, rotation, centerPoint, useLineSymmetry));
    delete m_normalEraser;
    m_normalEraser = 0;
  }
  if (m_blurredEraser) {
    m_backupRas = TRasterCM32P();
    m_workRas   = TRaster32P();
    delete m_blurredEraser;
    m_blurredEraser = 0;
    TUndoManager::manager()->add(new RasterBluredEraserUndo(
        m_tileSet, m_points,
        TTool::getApplication()->getCurrentLevelStyleIndex(),
        m_currentStyle.getValue(), TTool::getApplication()
                                       ->getCurrentLevel()
                                       ->getLevel()
                                       ->getSimpleLevel(),
        m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId,
        m_toolSize.getValue(), m_hardness.getValue() * 0.01,
        m_colorType.getValue(), dpiScale, symmetryLines, rotation, centerPoint,
        useLineSymmetry));
  }
  if (m_tileSaver) {
    delete m_tileSaver;
    m_tileSaver = 0;
  }

  TRectD invalidateRect;
  if (m_firstRect != TRectD()) {
    invalidateRect += m_firstRect;
    m_firstRect.empty();
  }
  if (m_selectingRect != TRectD()) {
    invalidateRect += m_selectingRect;
    m_selectingRect.empty();
  }
  if (!m_track.isEmpty()) m_track.clear();

  if (!invalidateRect.isEmpty()) invalidate(invalidateRect.enlarge(2));
}

//--------------------------------------------------------------------------------------------------
/*! Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す
 */
bool EraserTool::isPencilModeActive() {
  return m_eraseType.getValue() == NORMALERASE && m_pencil.getValue();
}

}  // namespace
