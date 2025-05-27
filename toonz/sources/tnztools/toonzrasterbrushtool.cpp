

#include "toonzrasterbrushtool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tooloptions.h"
#include "bluredbrush.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/ttileset.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/toonzimageutils.h"
#include "toonz/palettecontroller.h"
#include "toonz/stage2.h"
#include "toonz/preferences.h"
#include "toonz/tpalettehandle.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/toonzfolders.h"
#include "toonz/tstageobjectcmd.h"

// TnzCore includes
#include "tstream.h"
#include "tcolorstyles.h"
#include "tvectorimage.h"
#include "tenv.h"
#include "tregion.h"
#include "tinbetween.h"
#include "tsystem.h"

#include "tgl.h"
#include "trop.h"

#include "perspectivetool.h"

// Qt includes
#include <QPainter>

using namespace ToolUtils;

TEnv::DoubleVar RasterBrushMinSize("InknpaintRasterBrushMinSize", 1);
TEnv::DoubleVar RasterBrushMaxSize("InknpaintRasterBrushMaxSize", 5);
TEnv::DoubleVar BrushSmooth("InknpaintBrushSmooth", 0);
TEnv::IntVar BrushDrawOrder("InknpaintBrushDrawOrder", 0);
TEnv::IntVar RasterBrushPencilMode("InknpaintRasterBrushPencilMode", 0);
TEnv::IntVar BrushPressureSensitivity("InknpaintBrushPressureSensitivity", 1);
TEnv::DoubleVar RasterBrushHardness("RasterBrushHardness", 100);
TEnv::DoubleVar RasterBrushModifierSize("RasterBrushModifierSize", 0);
TEnv::StringVar RasterBrushPreset("RasterBrushPreset", "<custom>");
TEnv::IntVar BrushLockAlpha("InknpaintBrushLockAlpha", 0);
TEnv::IntVar BrushSnapGrid("InknpaintBrushSnapGrid", 0);

//-------------------------------------------------------------------
#define CUSTOM_WSTR L"<custom>"
//-------------------------------------------------------------------
//
// (Da mettere in libreria) : funzioni che spezzano una stroke
// nei suoi punti angolosi. Lo facciamo specialmente per limitare
// i problemi di fill.
//
//-------------------------------------------------------------------

//
// Split a stroke in n+1 parts, according to n parameter values
// Input:
//      stroke            = stroke to split
//      parameterValues[] = vector of parameters where I want to split the
//      stroke
//                          assert: 0<a[0]<a[1]<...<a[n-1]<1
// Output:
//      strokes[]         = the split strokes
//
// note: stroke is unchanged
//

static void split(TStroke *stroke, const std::vector<double> &parameterValues,
                  std::vector<TStroke *> &strokes) {
  TThickPoint p2;
  std::vector<TThickPoint> points;
  TThickPoint lastPoint = stroke->getControlPoint(0);
  int n                 = parameterValues.size();
  int chunk;
  double t;
  int last_chunk = -1, startPoint = 0;
  double lastLocT = 0;

  for (int i = 0; i < n; i++) {
    points.push_back(lastPoint);  // Add first point of the stroke
    double w =
        parameterValues[i];  // Global parameter. along the stroke 0<=w<=1
    stroke->getChunkAndT(w, chunk,
                         t);  // t: local parameter in the chunk-th quadratic

    if (i == 0)
      startPoint = 1;
    else {
      int indexAfterLastT =
          stroke->getControlPointIndexAfterParameter(parameterValues[i - 1]);
      startPoint = indexAfterLastT;
      if ((indexAfterLastT & 1) && lastLocT != 1) startPoint++;
    }
    int endPoint = 2 * chunk + 1;
    if (lastLocT != 1 && i > 0) {
      if (last_chunk != chunk || t == 1)
        points.push_back(p2);  // If the last local t is not an extreme
                               // add the point p2
    }

    for (int j = startPoint; j < endPoint; j++)
      points.push_back(stroke->getControlPoint(j));

    TThickPoint p, A, B, C;
    p       = stroke->getPoint(w);
    C       = stroke->getControlPoint(2 * chunk + 2);
    B       = stroke->getControlPoint(2 * chunk + 1);
    A       = stroke->getControlPoint(2 * chunk);
    p.thick = A.thick;

    if (last_chunk != chunk) {
      TThickPoint p1 = (1 - t) * A + t * B;
      points.push_back(p1);
      p.thick = p1.thick;
    } else {
      if (t != 1) {
        // If the i-th cut point belong to the same chunk of the (i-1)-th cut
        // point.
        double tInters  = lastLocT / t;
        TThickPoint p11 = (1 - t) * A + t * B;
        TThickPoint p1  = (1 - tInters) * p11 + tInters * p;
        points.push_back(p1);
        p.thick = p1.thick;
      }
    }

    points.push_back(p);

    if (t != 1) p2 = (1 - t) * B + t * C;

    assert(points.size() & 1);

    // Add new stroke
    TStroke *strokeAdd = new TStroke(points);
    strokeAdd->setStyle(stroke->getStyle());
    strokeAdd->outlineOptions() = stroke->outlineOptions();
    strokes.push_back(strokeAdd);

    lastPoint  = p;
    last_chunk = chunk;
    lastLocT   = t;
    points.clear();
  }
  // Add end stroke
  points.push_back(lastPoint);

  if (lastLocT != 1) points.push_back(p2);

  startPoint =
      stroke->getControlPointIndexAfterParameter(parameterValues[n - 1]);
  if ((stroke->getControlPointIndexAfterParameter(parameterValues[n - 1]) &
       1) &&
      lastLocT != 1)
    startPoint++;
  for (int j = startPoint; j < stroke->getControlPointCount(); j++)
    points.push_back(stroke->getControlPoint(j));

  assert(points.size() & 1);
  TStroke *strokeAdd = new TStroke(points);
  strokeAdd->setStyle(stroke->getStyle());
  strokeAdd->outlineOptions() = stroke->outlineOptions();
  strokes.push_back(strokeAdd);
  points.clear();
}

// Compute Parametric Curve Curvature
// By Formula:
// k(t)=(|p'(t) x p''(t)|)/Norm2(p')^3
// p(t) is parametric curve
// Input:
//      dp  = First Derivate.
//      ddp = Second Derivate
// Output:
//      return curvature value.
//      Note: if the curve is a single point (that's dp=0) or it is a straight
//      line (that's ddp=0) return 0

static double curvature(TPointD dp, TPointD ddp) {
  if (dp == TPointD(0, 0))
    return 0;
  else
    return fabs(cross(dp, ddp) / pow(norm2(dp), 1.5));
}

// Find the max curvature points of a stroke.
// Input:
//      stroke.
//      angoloLim =  Value (radians) of the Corner between two tangent vector.
//                   Up this value the two corner can be considered angular.
//      curvMaxLim = Value of the max curvature.
//                   Up this value the point can be considered a max curvature
//                   point.
// Output:
//      parameterValues = vector of max curvature parameter points

static void findMaxCurvPoints(TStroke *stroke, const float &angoloLim,
                              const float &curvMaxLim,
                              std::vector<double> &parameterValues) {
  TPointD tg1, tg2;  // Tangent vectors

  TPointD dp, ddp;  // First and Second derivate.

  parameterValues.clear();
  int cpn = stroke ? stroke->getControlPointCount() : 0;
  for (int j = 2; j < cpn; j += 2) {
    TPointD p0 = stroke->getControlPoint(j - 2);
    TPointD p1 = stroke->getControlPoint(j - 1);
    TPointD p2 = stroke->getControlPoint(j);

    TPointD q = p1 - (p0 + p2) * 0.5;

    // Search corner point
    if (j > 2) {
      tg2 = -p0 + p2 + 2 * q;  // Tangent vector to this chunk at t=0
      double prod_scal =
          tg2 * tg1;  // Inner product between tangent vectors at t=0.
      assert(tg1 != TPointD(0, 0) || tg2 != TPointD(0, 0));
      // Compute corner between two tangent vectors
      double angolo =
          acos(prod_scal / (pow(norm2(tg2), 0.5) * pow(norm2(tg1), 0.5)));

      // Add corner point
      if (angolo > angoloLim) {
        double w = getWfromChunkAndT(stroke, (UINT)(0.5 * (j - 2)),
                                     0);  //  transform lacal t to global t
        parameterValues.push_back(w);
      }
    }
    tg1 = -p0 + p2 - 2 * q;  // Tangent vector to this chunk at t=1

    // End search corner point

    // Search max curvature point
    // Value of t where the curvature function has got an extreme.
    // (Point where first derivate is null)
    double estremo_int = 0;
    double t           = -1;
    if (q != TPointD(0, 0)) {
      t = 0.25 * (2 * q.x * q.x + 2 * q.y * q.y - q.x * p0.x + q.x * p2.x -
                  q.y * p0.y + q.y * p2.y) /
          (q.x * q.x + q.y * q.y);

      dp  = -p0 + p2 + 2 * q - 4 * t * q;  // First derivate of the curve
      ddp = -4 * q;                        // Second derivate of the curve
      estremo_int = curvature(dp, ddp);

      double h    = 0.01;
      dp          = -p0 + p2 + 2 * q - 4 * (t + h) * q;
      double c_dx = curvature(dp, ddp);
      dp          = -p0 + p2 + 2 * q - 4 * (t - h) * q;
      double c_sx = curvature(dp, ddp);
      // Check the point is a max and not a minimum
      if (estremo_int < c_dx && estremo_int < c_sx) {
        estremo_int = 0;
      }
    }
    double curv_max = estremo_int;

    // Compute curvature at the extreme of interval [0,1]
    // Compute curvature at t=0 (Left extreme)
    dp                = -p0 + p2 + 2 * q;
    double estremo_sx = curvature(dp, ddp);

    // Compute curvature at t=1 (Right extreme)
    dp                = -p0 + p2 - 2 * q;
    double estremo_dx = curvature(dp, ddp);

    // Compare curvature at the extreme of interval [0,1] with the internal
    // value
    double t_ext;
    if (estremo_sx >= estremo_dx)
      t_ext = 0;
    else
      t_ext           = 1;
    double maxEstremi = std::max(estremo_dx, estremo_sx);
    if (maxEstremi > estremo_int) {
      t        = t_ext;
      curv_max = maxEstremi;
    }

    // Add max curvature point
    if (t >= 0 && t <= 1 && curv_max > curvMaxLim) {
      double w = getWfromChunkAndT(stroke, (UINT)(0.5 * (j - 2)),
                                   t);  // transform local t to global t
      parameterValues.push_back(w);
    }
    // End search max curvature point
  }
  // Delete duplicate of parameterValues
  // Because some max cuvature point can coincide with the corner point
  if ((int)parameterValues.size() > 1) {
    std::sort(parameterValues.begin(), parameterValues.end());
    parameterValues.erase(
        std::unique(parameterValues.begin(), parameterValues.end()),
        parameterValues.end());
  }
}

static void addStroke(TTool::Application *application, const TVectorImageP &vi,
                      TStroke *stroke, bool breakAngles, bool frameCreated,
                      bool levelCreated, TXshSimpleLevel *sLevel = NULL,
                      TFrameId fid = TFrameId::NO_FRAME) {
  QMutexLocker lock(vi->getMutex());

  if (application->getCurrentObject()->isSpline()) {
    application->getCurrentXsheet()->notifyXsheetChanged();
    return;
  }

  std::vector<double> corners;
  std::vector<TStroke *> strokes;

  const float angoloLim =
      1;  // Value (radians) of the Corner between two tangent vector.
          // Up this value the two corner can be considered angular.
  const float curvMaxLim = 0.8;  // Value of the max curvature.
  // Up this value the point can be considered a max curvature point.

  findMaxCurvPoints(stroke, angoloLim, curvMaxLim, corners);
  TXshSimpleLevel *sl;
  if (!sLevel) {
    sl = application->getCurrentLevel()->getSimpleLevel();
  } else {
    sl = sLevel;
  }
  TFrameId id = application->getCurrentTool()->getTool()->getCurrentFid();
  if (id == TFrameId::NO_FRAME && fid != TFrameId::NO_FRAME) id = fid;
  if (!corners.empty()) {
    if (breakAngles)
      split(stroke, corners, strokes);
    else
      strokes.push_back(new TStroke(*stroke));

    int n = strokes.size();

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i < n; i++) {
      std::vector<TFilledRegionInf> *fillInformation =
          new std::vector<TFilledRegionInf>;
      ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                       stroke->getBBox());
      TStroke *str = new TStroke(*strokes[i]);
      vi->addStroke(str);
      TUndoManager::manager()->add(new UndoPencil(str, fillInformation, sl, id,
                                                  frameCreated, levelCreated));
    }
    TUndoManager::manager()->endBlock();
  } else {
    std::vector<TFilledRegionInf> *fillInformation =
        new std::vector<TFilledRegionInf>;
    ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                     stroke->getBBox());
    TStroke *str = new TStroke(*stroke);
    vi->addStroke(str);
    TUndoManager::manager()->add(new UndoPencil(str, fillInformation, sl, id,
                                                frameCreated, levelCreated));
  }

  // Update regions. It will call roundStroke() in
  // TVectorImage::Imp::findIntersections().
  // roundStroke() will slightly modify all the stroke positions.
  // It is needed to update information for Fill Check.
  vi->findRegions();

  for (int k = 0; k < (int)strokes.size(); k++) delete strokes[k];
  strokes.clear();

  application->getCurrentTool()->getTool()->notifyImageChanged();
}

//-------------------------------------------------------------------
//
// Gennaro: end
//
//-------------------------------------------------------------------

//===================================================================
//
// Helper functions and classes
//
//-------------------------------------------------------------------

namespace {

//-------------------------------------------------------------------

void addStrokeToImage(TTool::Application *application, const TVectorImageP &vi,
                      TStroke *stroke, bool breakAngles, bool frameCreated,
                      bool levelCreated, TXshSimpleLevel *sLevel = NULL,
                      TFrameId id = TFrameId::NO_FRAME) {
  QMutexLocker lock(vi->getMutex());
  addStroke(application, vi.getPointer(), stroke, breakAngles, frameCreated,
            levelCreated, sLevel, id);
  // la notifica viene gia fatta da addStroke!
  // getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
}

//---------------------------------------------------------------------------------------------------------

enum DrawOrder { OverAll = 0, UnderAll, PaletteOrder };

void getAboveStyleIdSet(int styleId, TPaletteP palette,
                        QSet<int> &aboveStyles) {
  if (!palette) return;
  for (int p = 0; p < palette->getPageCount(); p++) {
    TPalette::Page *page = palette->getPage(p);
    for (int s = 0; s < page->getStyleCount(); s++) {
      int tmpId = page->getStyleId(s);
      if (tmpId == styleId) return;
      if (tmpId != 0) aboveStyles.insert(tmpId);
    }
  }
}

//=========================================================================================================

class RasterBrushUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_selective;
  bool m_isPaletteOrder;
  bool m_isPencil;
  bool m_isStraight;
  bool m_modifierLockAlpha;
  TPointD m_dpiScale;
  double m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;

public:
  RasterBrushUndo(TTileSetCM32 *tileSet, const std::vector<TThickPoint> &points,
                  int styleId, bool selective, TXshSimpleLevel *level,
                  const TFrameId &frameId, bool isPencil, bool isFrameCreated,
                  bool isLevelCreated, bool isPaletteOrder, bool lockAlpha,
                  TPointD dpiScale, double symmetryLines, double rotation,
                  TPointD centerPoint, bool useLineSymmetry,
                  bool isStraight = false)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_isPencil(isPencil)
      , m_isStraight(isStraight)
      , m_isPaletteOrder(isPaletteOrder)
      , m_modifierLockAlpha(lockAlpha)
      , m_dpiScale(dpiScale)
      , m_brushCount(symmetryLines)
      , m_rotation(rotation)
      , m_centerPoint(centerPoint)
      , m_useLineSymmetry(useLineSymmetry) {}

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TToonzImageP image = getImage();
    TRasterCM32P ras   = image->getRaster();

    CMRasterBrush m_cmRasterBrush(ras, BRUSH, NONE, m_styleId, m_points[0],
                                  m_selective, 0, m_modifierLockAlpha,
                                  !m_isPencil, m_isPaletteOrder);
    if (m_brushCount > 1)
      m_cmRasterBrush.addSymmetryBrushes(m_brushCount, m_rotation,
                                         m_centerPoint, m_useLineSymmetry,
                                         m_dpiScale);

    if (m_isPaletteOrder) {
      QSet<int> aboveStyleIds;
      getAboveStyleIdSet(m_styleId, image->getPalette(), aboveStyleIds);
      m_cmRasterBrush.setAboveStyleIds(aboveStyleIds);
    }
    m_cmRasterBrush.setPointsSequence(m_points);
    m_cmRasterBrush.generateStroke(m_isPencil, m_isStraight);
    image->setSavebox(
        image->getSavebox() +
        m_cmRasterBrush.getBBox(m_cmRasterBrush.getPointsSequence()));
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }
  QString getToolName() override { return QString("Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//=========================================================================================================

class RasterBluredBrushUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  DrawOrder m_drawOrder;
  int m_maxThick;
  double m_hardness;
  bool m_isStraight;
  bool m_modifierLockAlpha;
  TPointD m_dpiScale;
  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;

public:
  RasterBluredBrushUndo(TTileSetCM32 *tileSet,
                        const std::vector<TThickPoint> &points, int styleId,
                        DrawOrder drawOrder, bool lockAlpha,
                        TXshSimpleLevel *level, const TFrameId &frameId,
                        int maxThick, double hardness, bool isFrameCreated,
                        bool isLevelCreated, TPointD dpiScale,
                        double symmetryLines, double rotation,
                        TPointD centerPoint, bool useLineSymmetry,
                        bool isStraight = false)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_drawOrder(drawOrder)
      , m_maxThick(maxThick)
      , m_isStraight(isStraight)
      , m_hardness(hardness)
      , m_modifierLockAlpha(lockAlpha)
      , m_dpiScale(dpiScale)
      , m_brushCount(symmetryLines)
      , m_rotation(rotation)
      , m_centerPoint(centerPoint)
      , m_useLineSymmetry(useLineSymmetry) {}

  void redo() const override {
    if (m_points.size() == 0) return;
    insertLevelAndFrameIfNeeded();
    TToonzImageP image     = getImage();
    TRasterCM32P ras       = image->getRaster();
    TRasterCM32P backupRas = ras->clone();
    TRaster32P workRaster(ras->getSize());
    QRadialGradient brushPad = ToolUtils::getBrushPad(m_maxThick, m_hardness);
    workRaster->clear();
    RasterBlurredBrush brush(workRaster, m_maxThick, brushPad, false);

    if (m_brushCount > 1)
      brush.addSymmetryBrushes(m_brushCount, m_rotation, m_centerPoint,
                               m_useLineSymmetry, m_dpiScale);

    if (m_drawOrder == PaletteOrder) {
      QSet<int> aboveStyleIds;
      getAboveStyleIdSet(m_styleId, image->getPalette(), aboveStyleIds);
      brush.setAboveStyleIds(aboveStyleIds);
    }

    std::vector<TThickPoint> points;
    points.push_back(m_points[0]);
    TRect bbox = brush.getBoundFromPoints(points);
    brush.addPoint(m_points[0], 1);
    brush.updateDrawing(ras, ras, bbox, m_styleId, (int)m_drawOrder,
                        m_modifierLockAlpha);
    if (m_isStraight) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      points.push_back(m_points[2]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], m_points[1], m_points[2], 1, 1);
      brush.updateDrawing(ras, backupRas, bbox, m_styleId, (int)m_drawOrder,
                          m_modifierLockAlpha);
    } else if (m_points.size() > 1) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], (m_points[1] + m_points[0]) * 0.5, m_points[1],
                   1, 1);
      brush.updateDrawing(ras, backupRas, bbox, m_styleId, (int)m_drawOrder,
                          m_modifierLockAlpha);
      int i;
      for (i = 1; i + 2 < (int)m_points.size(); i = i + 2) {
        points.clear();
        points.push_back(m_points[i]);
        points.push_back(m_points[i + 1]);
        points.push_back(m_points[i + 2]);
        bbox = brush.getBoundFromPoints(points);
        brush.addArc(m_points[i], m_points[i + 1], m_points[i + 2], 1, 1);
        brush.updateDrawing(ras, backupRas, bbox, m_styleId, (int)m_drawOrder,
                            m_modifierLockAlpha);
      }
    }
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//=========================================================================================================

class MyPaintBrushUndo final : public TRasterUndo {
  TPoint m_offset;
  QString m_id;

public:
  MyPaintBrushUndo(TTileSetCM32 *tileSet, TXshSimpleLevel *level,
                   const TFrameId &frameId, bool isFrameCreated,
                   bool isLevelCreated, const TRasterCM32P &ras,
                   const TPoint &offset)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_offset(offset) {
    static int counter = 0;
    m_id = QString("MyPaintBrushUndo") + QString::number(counter++);
    TImageCache::instance()->add(m_id.toStdString(),
                                 TToonzImageP(ras, TRect(ras->getSize())));
  }

  ~MyPaintBrushUndo() { TImageCache::instance()->remove(m_id); }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TToonzImageP image = getImage();
    TRasterCM32P ras   = image->getRaster();

    TImageP srcImg =
        TImageCache::instance()->get(m_id.toStdString(), false)->cloneImage();
    TToonzImageP tSrcImg = srcImg;
    assert(tSrcImg);
    ras->copy(tSrcImg->getRaster(), m_offset);
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//=========================================================================================================

double computeThickness(double pressure, const TDoublePairProperty &property) {
  double t                    = pressure * pressure * pressure;
  double thick0               = property.getValue().first;
  double thick1               = property.getValue().second;
  if (thick1 < 0.0001) thick0 = thick1 = 0.0;
  return (thick0 + (thick1 - thick0) * t) * 0.5;
}

//---------------------------------------------------------------------------------------------------------

int computeThickness(double pressure, const TIntPairProperty &property) {
  double t   = pressure * pressure * pressure;
  int thick0 = property.getValue().first;
  int thick1 = property.getValue().second;
  return tround(thick0 + (thick1 - thick0) * t);
}

}  // namespace

//--------------------------------------------------------------------------------------------------

static void CatmullRomInterpolate(const TThickPoint &P0, const TThickPoint &P1,
                                  const TThickPoint &P2, const TThickPoint &P3,
                                  int samples,
                                  std::vector<TThickPoint> &points) {
  double x0 = P1.x;
  double x1 = (-P0.x + P2.x) * 0.5f;
  double x2 = P0.x - 2.5f * P1.x + 2.0f * P2.x - 0.5f * P3.x;
  double x3 = -0.5f * P0.x + 1.5f * P1.x - 1.5f * P2.x + 0.5f * P3.x;

  double y0 = P1.y;
  double y1 = (-P0.y + P2.y) * 0.5f;
  double y2 = P0.y - 2.5f * P1.y + 2.0f * P2.y - 0.5f * P3.y;
  double y3 = -0.5f * P0.y + 1.5f * P1.y - 1.5f * P2.y + 0.5f * P3.y;

  double z0 = P1.thick;
  double z1 = (-P0.thick + P2.thick) * 0.5f;
  double z2 = P0.thick - 2.5f * P1.thick + 2.0f * P2.thick - 0.5f * P3.thick;
  double z3 =
      -0.5f * P0.thick + 1.5f * P1.thick - 1.5f * P2.thick + 0.5f * P3.thick;

  for (int i = 1; i <= samples; ++i) {
    double t  = i / (double)(samples + 1);
    double t2 = t * t;
    double t3 = t2 * t;
    TThickPoint p;
    p.x     = x0 + x1 * t + x2 * t2 + x3 * t3;
    p.y     = y0 + y1 * t + y2 * t2 + y3 * t3;
    p.thick = z0 + z1 * t + z2 * t2 + z3 * t3;
    points.push_back(p);
  }
}

//--------------------------------------------------------------------------------------------------

static void Smooth(std::vector<TThickPoint>& points, const int radius,
    const int readIndex, const int level) {
  int n = (int)points.size();
  if (radius < 1 || n < 3) {
    return;
  }

  std::vector<TThickPoint> result;

  float d = 1.0f / (radius * 2 + 1);

  int endSamples = 10;
  int startId = std::max(readIndex - endSamples * 3 - radius * level, 1);

  for (int i = startId; i < n - 1; ++i) {
    int lower = i - radius;
    int upper = i + radius;

    TThickPoint total;
    total.x     = 0;
    total.y     = 0;
    total.thick = 0;

    for (int j = lower; j <= upper; ++j) {
      int idx = j;
      if (idx < 0) {
        idx = 0;
      } else if (idx >= n) {
        idx = n - 1;
      }
      total.x += points[idx].x;
      total.y += points[idx].y;
      total.thick += points[idx].thick;
    }

    total.x *= d;
    total.y *= d;
    total.thick *= d;
    result.push_back(total);
  }

  auto result_itr = result.begin();
  for (int i = startId; i < n - 1; ++i, ++result_itr) {
      points[i].x = (*result_itr).x;
      points[i].y = (*result_itr).y;
      points[i].thick = (*result_itr).thick;
  }

  if (points.size() >= 3) {
    std::vector<TThickPoint> pts;
    CatmullRomInterpolate(points[0], points[0], points[1], points[2],
        endSamples, pts);
    std::vector<TThickPoint>::iterator it = points.begin() + 1;
    points.insert(it, pts.begin(), pts.end());

    pts.clear();
    CatmullRomInterpolate(points[n - 3], points[n - 2], points[n - 1],
                          points[n - 1], 10, pts);
    it = points.begin();
    it += n - 1;
    points.insert(it, pts.begin(), pts.end());
  }
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::beginStroke(int smooth) {
  m_smooth      = smooth;
  m_outputIndex = 0;
  m_readIndex   = -1;
  m_rawPoints.clear();
  m_outputPoints.clear();
  m_resampledIndex = 0;
  m_resampledPoints.clear();
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::addPoint(const TThickPoint &point) {
  if (m_rawPoints.size() > 0 && m_rawPoints.back().x == point.x &&
      m_rawPoints.back().y == point.y) {
    return;
  }
  m_rawPoints.push_back(point);
  generatePoints();
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::endStroke() {
  generatePoints();
  // force enable the output all segments
  m_outputIndex = m_outputPoints.size() - 1;
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::clearPoints() {
  m_outputIndex = 0;
  m_readIndex   = -1;
  m_outputPoints.clear();
  m_rawPoints.clear();
  m_resampledIndex = 0;
  m_resampledPoints.clear();
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::getSmoothPoints(std::vector<TThickPoint> &smoothPoints) {
  int n = m_outputPoints.size();
  for (int i = m_readIndex + 1; i <= m_outputIndex && i < n; ++i) {
    smoothPoints.push_back(m_outputPoints[i]);
  }
  m_readIndex = m_outputIndex;
}

//--------------------------------------------------------------------------------------------------

void SmoothStroke::generatePoints() {
  int n = (int)m_rawPoints.size();
  if (n == 0) {
    return;
  }

  // if m_smooth = 0, then skip whole smoothing process
  if (m_smooth == 0) {
    for (int i = m_outputIndex; i < (int)m_outputPoints.size(); ++i) {
      if (m_outputPoints[i] != m_rawPoints[i]) {
        break;
      }
      ++m_outputIndex;
    }
    m_outputPoints = m_rawPoints;
    return;
  }

  std::vector<TThickPoint> smoothedPoints = m_resampledPoints;
  // Add more stroke samples before applying the smoothing
  // This is because the raw inputs points are too few to support smooth result,
  // especially on stroke ends
  int resampleStartId = m_resampledIndex;
  for (int i = resampleStartId; i < n - 1; ++i) {
      const TThickPoint& p1 = m_rawPoints[i];
      const TThickPoint& p2 = m_rawPoints[i + 1];
      const TThickPoint& p0 = i - 1 >= 0 ? m_rawPoints[i - 1] : p1;
      const TThickPoint& p3 = i + 2 < n ? m_rawPoints[i + 2] : p2;

      std::vector<TThickPoint> tmpResampled;
      tmpResampled.push_back(p1);
      // define subsample amount according to distance between points
      int samples = std::min((int)tdistance(p1, p2), 8);
      if (samples >= 1)
          CatmullRomInterpolate(p0, p1, p2, p3, samples, tmpResampled);

      if (i + 2 < n) {
          m_resampledIndex = i + 1;
          std::copy(tmpResampled.begin(), tmpResampled.end(),
              std::back_inserter(m_resampledPoints));
      }
      std::copy(tmpResampled.begin(), tmpResampled.end(),
          std::back_inserter(smoothedPoints));
  }
  smoothedPoints.push_back(m_rawPoints.back());
  // Apply the 1D box filter
  // Multiple passes result in better quality and fix the stroke ends break
  // issue
  // level is passed to define range where the points are smoothed
  for (int level = 2; level >= 0; --level) {
      Smooth(smoothedPoints, m_smooth, m_readIndex, level);
  }
  // Compare the new smoothed stroke with old one
  // Enable the output for unchanged parts
  int outputNum = (int)m_outputPoints.size();
  for (int i = m_outputIndex; i < outputNum; ++i) {
    if (m_outputPoints[i] != smoothedPoints[i]) {
      break;
    }
    ++m_outputIndex;
  }
  m_outputPoints = smoothedPoints;
}

//===================================================================
//
// ToonzRasterBrushTool
//
//-----------------------------------------------------------------------------

ToonzRasterBrushTool::ToonzRasterBrushTool(std::string name, int targetType)
    : TTool(name)
    , m_rasThickness("Size", 1, 1000, 1, 5)
    , m_smooth("Smooth:", 0, 50, 0)
    , m_hardness("Hardness:", 0, 100, 100)
    , m_preset("Preset:")
    , m_drawOrder("Draw Order:")
    , m_pencil("Pencil", false)
    , m_pressure("Pressure", true)
    , m_modifierSize("ModifierSize", -3, 3, 0, true)
    , m_cmRasterBrush(0)
    , m_styleId(0)
    , m_bluredBrush(0)
    , m_active(false)
    , m_enabled(false)
    , m_isPrompting(false)
    , m_firstTime(true)
    , m_presetsLoaded(false)
    , m_targetType(targetType)
    , m_workingFrameId(TFrameId())
    , m_notifier(0)
    , m_modifierLockAlpha("Lock Alpha", false)
    , m_snapGrid("Grid", false) {
  bind(targetType);

  m_rasThickness.setNonLinearSlider();

  m_prop[0].bind(m_rasThickness);
  m_prop[0].bind(m_hardness);
  m_prop[0].bind(m_modifierSize);
  m_prop[0].bind(m_smooth);
  m_prop[0].bind(m_drawOrder);
  m_prop[0].bind(m_modifierLockAlpha);
  m_prop[0].bind(m_pencil);
  m_pencil.setId("PencilMode");

  m_drawOrder.addValue(L"Over All");
  m_drawOrder.addValue(L"Under All");
  m_drawOrder.addValue(L"Palette Order");
  m_drawOrder.setId("DrawOrder");

  m_prop[0].bind(m_pressure);

  m_prop[0].bind(m_snapGrid);
  m_snapGrid.setId("SnapGrid");

  m_prop[0].bind(m_preset);
  m_preset.setId("BrushPreset");
  m_preset.addValue(CUSTOM_WSTR);
  m_pressure.setId("PressureSensitivity");
  m_modifierLockAlpha.setId("LockAlpha");
}

//-------------------------------------------------------------------------------------------------------

ToolOptionsBox *ToonzRasterBrushTool::createOptionsBox() {
  TPaletteHandle *currPalette =
      TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
  return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//-------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::drawLine(const TPointD &point, const TPointD &centre,
                                    bool horizontal, bool isDecimal) {
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

void ToonzRasterBrushTool::drawEmptyCircle(TPointD pos, int thick,
                                           bool isLxEven, bool isLyEven,
                                           bool isPencil) {
  if (isLxEven) pos.x += 0.5;
  if (isLyEven) pos.y += 0.5;

  if (!isPencil)
    tglDrawCircle(pos, (thick + 1) * 0.5);
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

//-------------------------------------------------------------------------------------------------------

TPointD ToonzRasterBrushTool::getCenteredCursorPos(
    const TPointD &originalCursorPos) {
  if (m_isMyPaintStyleSelected) return originalCursorPos;
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

//-------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::updateTranslation() {
  m_rasThickness.setQStringName(tr("Size"));
  m_hardness.setQStringName(tr("Hardness:"));
  m_smooth.setQStringName(tr("Smooth:"));
  m_drawOrder.setQStringName(tr("Draw Order:"));
  m_drawOrder.setItemUIName(L"Over All", tr("Over All"));
  m_drawOrder.setItemUIName(L"Under All", tr("Under All"));
  m_drawOrder.setItemUIName(L"Palette Order", tr("Palette Order"));
  m_modifierSize.setQStringName(tr("Size"));

  // m_filled.setQStringName(tr("Filled"));
  m_preset.setQStringName(tr("Preset:"));
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));
  m_pencil.setQStringName(tr("Pencil"));
  m_pressure.setQStringName(tr("Pressure"));
  m_modifierLockAlpha.setQStringName(tr("Lock Alpha"));
  m_snapGrid.setQStringName(tr("Grid"));
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::updateWorkAndBackupRasters(const TRect &rect) {
  TToonzImageP ti = TImageP(getImage(false, 1));
  if (!ti) return;

  TRasterCM32P ras = ti->getRaster();

  if (m_isMyPaintStyleSelected) {
    const int denominator = 8;
    TRect enlargedRect    = rect + m_lastRect;
    int dx                = (enlargedRect.getLx() - 1) / denominator + 1;
    int dy                = (enlargedRect.getLy() - 1) / denominator + 1;

    if (m_lastRect.isEmpty()) {
      enlargedRect.x0 -= dx;
      enlargedRect.y0 -= dy;
      enlargedRect.x1 += dx;
      enlargedRect.y1 += dy;

      TRect _rect = enlargedRect * ras->getBounds();
      if (_rect.isEmpty()) return;

      m_workRas->extract(_rect)->copy(ras->extract(_rect));
      m_backupRas->extract(_rect)->copy(ras->extract(_rect));
    } else {
      if (enlargedRect.x0 < m_lastRect.x0) enlargedRect.x0 -= dx;
      if (enlargedRect.y0 < m_lastRect.y0) enlargedRect.y0 -= dy;
      if (enlargedRect.x1 > m_lastRect.x1) enlargedRect.x1 += dx;
      if (enlargedRect.y1 > m_lastRect.y1) enlargedRect.y1 += dy;

      TRect _rect = enlargedRect * ras->getBounds();
      if (_rect.isEmpty()) return;

      TRect _lastRect    = m_lastRect * ras->getBounds();
      QList<TRect> rects = ToolUtils::splitRect(_rect, _lastRect);
      for (int i = 0; i < rects.size(); i++) {
        m_workRas->extract(rects[i])->copy(ras->extract(rects[i]));
        m_backupRas->extract(rects[i])->copy(ras->extract(rects[i]));
      }
    }

    m_lastRect = enlargedRect;
    return;
  }

  TRect _rect     = rect * ras->getBounds();
  TRect _lastRect = m_lastRect * ras->getBounds();

  if (_rect.isEmpty()) return;

  if (m_lastRect.isEmpty()) {
    m_workRas->extract(_rect)->clear();
    m_backupRas->extract(_rect)->copy(ras->extract(_rect));
    return;
  }

  QList<TRect> rects = ToolUtils::splitRect(_rect, _lastRect);
  for (int i = 0; i < rects.size(); i++) {
    m_workRas->extract(rects[i])->clear();
    m_backupRas->extract(rects[i])->copy(ras->extract(rects[i]));
  }
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onActivate() {
  if (!m_notifier) m_notifier = new ToonzRasterBrushToolNotifier(this);

  if (m_firstTime) {
    m_firstTime = false;

    std::wstring wpreset =
        QString::fromStdString(RasterBrushPreset.getValue()).toStdWString();
    if (wpreset != CUSTOM_WSTR) {
      initPresets();
      if (!m_preset.isValue(wpreset)) wpreset = CUSTOM_WSTR;
      m_preset.setValue(wpreset);
      RasterBrushPreset = m_preset.getValueAsString();
      loadPreset();
    } else
      loadLastBrush();
  }
  m_brushPad = ToolUtils::getBrushPad(m_rasThickness.getValue().second,
                                      m_hardness.getValue() * 0.01);
  setWorkAndBackupImages();

  m_brushTimer.start();
  // TODO:app->editImageOrSpline();
}

//--------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onDeactivate() {
  /*---
   * ドラッグ中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ処理を行う
   * ---*/
  if (m_tileSaver) {
    bool isValid = m_enabled && m_active;
    m_enabled    = false;
    m_active     = false;
    if (isValid) {
      finishRasterBrush(m_mousePos,
                        1); /*-- 最後のストロークの筆圧は1とする --*/
    }
  }
  m_workRas   = TRaster32P();
  m_backupRas = TRasterCM32P();
}

//--------------------------------------------------------------------------------------------------

bool ToonzRasterBrushTool::askRead(const TRect &rect) { return askWrite(rect); }

//--------------------------------------------------------------------------------------------------

bool ToonzRasterBrushTool::askWrite(const TRect &rect) {
  if (rect.isEmpty()) return true;
  m_strokeRect += rect;
  m_strokeSegmentRect += rect;
  updateWorkAndBackupRasters(rect);
  m_tileSaver->save(rect);
  return true;
}

//--------------------------------------------------------------------------------------------------

bool ToonzRasterBrushTool::preLeftButtonDown() {
  touchImage();
  if (m_isFrameCreated) {
    setWorkAndBackupImages();
    // When the xsheet frame is selected, whole viewer will be updated from
    // SceneViewer::onXsheetChanged() on adding a new frame.
    // We need to take care of a case when the level frame is selected.
    if (m_application->getCurrentFrame()->isEditingLevel()) invalidate();
  }
  return true;
}

//--------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::leftButtonDown(const TPointD &pos,
                                          const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  int col   = app->getCurrentColumn()->getColumnIndex();
  m_enabled = col >= 0 || app->getCurrentFrame()->isEditingLevel();
  // todo: gestire autoenable
  if (!m_enabled) return;

  TPointD centeredPos = getCenteredCursorPos(pos);

  m_currentColor = TPixel32::Black;
  m_active       = !!getImage(true);
  if (!m_active) {
    m_active = !!touchImage();
  }
  if (!m_active) return;

  if (m_active) {
    // nel caso che il colore corrente sia un cleanup/studiopalette color
    // oppure il colore di un colorfield
    m_styleId       = app->getCurrentLevelStyleIndex();
    TColorStyle *cs = app->getCurrentLevelStyle();
    if (cs) {
      TRasterStyleFx *rfx = cs ? cs->getRasterStyleFx() : 0;
      m_active = cs != 0 && (cs->isStrokeStyle() || (rfx && rfx->isInkStyle()));
      m_currentColor   = cs->getAverageColor();
      m_currentColor.m = 255;
    } else {
      m_styleId      = 1;
      m_currentColor = TPixel32::Black;
    }
  }

  TXshLevel *level = app->getCurrentLevel()->getLevel();
  if (level == NULL) {
    m_active = false;
    return;
  }

  if ((e.isShiftPressed() || e.isCtrlPressed()) && !e.isAltPressed()) {
    m_isStraight = true;
    m_firstPoint = pos;
    m_lastPoint  = pos;
  }

  if (e.isAltPressed()) {
    m_isStraight    = true;
    m_firstPoint    = pos;
    m_lastPoint     = pos;
  }

  if (m_snapGrid.getValue()) {
    m_firstPoint = pos;
    m_lastPoint  = pos;
  }

  // assert(0<=m_styleId && m_styleId<2);
  TImageP img = getImage(true);
  TToonzImageP ri(img);
  TRasterCM32P ras = ri->getRaster();
  if (ras) {
    TPointD rasCenter = ras->getCenterD();
    m_tileSet         = new TTileSetCM32(ras->getSize());
    m_tileSaver       = new TTileSaverCM32(ras, m_tileSet);
    double maxThick   = m_rasThickness.getValue().second;
    double thickness  = (m_pressure.getValue())
                           ? computeThickness(e.m_pressure, m_rasThickness) * 2
                           : maxThick;

    /*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する
     * ---*/
    if (m_pressure.getValue() && e.m_pressure == 1.0)
      thickness = m_rasThickness.getValue().first;

    TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
    TRectD invalidateRect(centeredPos - halfThick, centeredPos + halfThick);
    TPointD dpi;
    ri->getDpi(dpi.x, dpi.y);
    TRectD previousTipRect(m_brushPos - halfThick, m_brushPos + halfThick);
    if (dpi.x > Stage::inch || dpi.y > Stage::inch)
      previousTipRect *= dpi.x / Stage::inch;
    invalidateRect += previousTipRect;

    // if the drawOrder mode = "Palette Order",
    // get styleId list which is above the current style in the palette
    DrawOrder drawOrder = (DrawOrder)m_drawOrder.getIndex();
    QSet<int> aboveStyleIds;
    if (drawOrder == PaletteOrder) {
      getAboveStyleIdSet(m_styleId, ri->getPalette(), aboveStyleIds);
    }

    // mypaint brush case
    if (m_isMyPaintStyleSelected) {
      TPointD point(centeredPos + rasCenter);
      double pressure =
          m_pressure.getValue() && e.isTablet() ? e.m_pressure : 0.5;
      m_oldPressure = pressure;
      updateCurrentStyle();
      if (!(m_workRas && m_backupRas)) setWorkAndBackupImages();
      m_workRas->lock();
      mypaint::Brush mypaintBrush;
      TMyPaintBrushStyle *mypaintStyle =
          dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
      {  // applyToonzBrushSettings
        mypaintBrush.fromBrush(mypaintStyle->getBrush());
        double modifierSize = m_modifierSize.getValue() * log(2.0);
        float baseSize =
            mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
        mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                                  baseSize + modifierSize);
      }

      m_toonz_brush = new MyPaintToonzBrush(m_workRas, *this, mypaintBrush);

      SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
          TTool::getTool("T_Symmetry", TTool::RasterImage));
      if (symmetryTool && symmetryTool->isGuideEnabled()) {
        TPointD dpiScale       = getViewer()->getDpiScale();
        SymmetryObject symmObj = symmetryTool->getSymmetryObject();
        m_toonz_brush->addSymmetryBrushes(
            symmObj.getLines(), symmObj.getRotation(), symmObj.getCenterPoint(),
            symmObj.isUsingLineSymmetry(), dpiScale);
      }

      m_strokeRect.empty();
      m_strokeSegmentRect.empty();
      m_toonz_brush->beginStroke();
      m_toonz_brush->strokeTo(point, pressure, restartBrushTimer());
      TRect updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty()) {
        // ras->extract(updateRect)->copy(m_workRas->extract(updateRect));
        m_toonz_brush->updateDrawing(ri->getRaster(), m_backupRas, m_strokeRect,
                                     m_styleId, m_modifierLockAlpha.getValue());
      }
      m_lastRect = m_strokeRect;

      TThickPoint thickPoint(point, pressure);
      std::vector<TThickPoint> pts;
      if (m_smooth.getValue() == 0) {
        pts.push_back(thickPoint);
      } else {
        m_smoothStroke.beginStroke(m_smooth.getValue());
        m_smoothStroke.addPoint(thickPoint);
        m_smoothStroke.getSmoothPoints(pts);
      }

      TPointD thickOffset(m_maxCursorThick * 0.5, m_maxCursorThick * 0.5);
      invalidateRect = convert(m_strokeSegmentRect) - rasCenter;
      invalidateRect +=
          TRectD(centeredPos - thickOffset, centeredPos + thickOffset);
      invalidateRect +=
          TRectD(m_brushPos - thickOffset, m_brushPos + thickOffset);
    } else if (m_hardness.getValue() == 100 || m_pencil.getValue()) {
      /*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる
       * --*/
      if (!m_pencil.getValue()) thickness -= 1.0;

      TThickPoint thickPoint(centeredPos + convert(ras->getCenter()),
                             thickness);
      m_cmRasterBrush = new CMRasterBrush(
          ras, BRUSH, NONE, m_styleId, thickPoint, drawOrder != OverAll, 0,
          m_modifierLockAlpha.getValue(), !m_pencil.getValue(),
          drawOrder == PaletteOrder);

      SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
          TTool::getTool("T_Symmetry", TTool::RasterImage));
      if (symmetryTool && symmetryTool->isGuideEnabled()) {
        TPointD dpiScale       = getViewer()->getDpiScale();
        SymmetryObject symmObj = symmetryTool->getSymmetryObject();
        m_cmRasterBrush->addSymmetryBrushes(
            symmObj.getLines(), symmObj.getRotation(), symmObj.getCenterPoint(),
            symmObj.isUsingLineSymmetry(), dpiScale);
      }

      if (drawOrder == PaletteOrder)
        m_cmRasterBrush->setAboveStyleIds(aboveStyleIds);

      m_tileSaver->save(m_cmRasterBrush->getLastRect());
      m_cmRasterBrush->generateLastPieceOfStroke(m_pencil.getValue());

      std::vector<TThickPoint> pts;
      if (m_smooth.getValue() == 0) {
        pts.push_back(thickPoint);
      } else {
        m_smoothStroke.beginStroke(m_smooth.getValue());
        m_smoothStroke.addPoint(thickPoint);
        m_smoothStroke.getSmoothPoints(pts);
      }
    } else {
      m_points.clear();
      TThickPoint point(centeredPos + rasCenter, thickness);
      m_points.push_back(point);
      m_bluredBrush =
          new RasterBlurredBrush(m_workRas, maxThick, m_brushPad, false);

      SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
          TTool::getTool("T_Symmetry", TTool::RasterImage));
      if (symmetryTool && symmetryTool->isGuideEnabled()) {
        TPointD dpiScale       = getViewer()->getDpiScale();
        SymmetryObject symmObj = symmetryTool->getSymmetryObject();
        m_bluredBrush->addSymmetryBrushes(
            symmObj.getLines(), symmObj.getRotation(), symmObj.getCenterPoint(),
            symmObj.isUsingLineSymmetry(), dpiScale);
      }

      if (drawOrder == PaletteOrder)
        m_bluredBrush->setAboveStyleIds(aboveStyleIds);

      m_strokeRect = m_bluredBrush->getBoundFromPoints(m_points);
      updateWorkAndBackupRasters(m_strokeRect);
      m_tileSaver->save(m_strokeRect);
      m_bluredBrush->addPoint(point, 1);
      m_bluredBrush->updateDrawing(ri->getRaster(), m_backupRas, m_strokeRect,
                                   m_styleId, drawOrder,
                                   m_modifierLockAlpha.getValue());
      m_lastRect = m_strokeRect;

      std::vector<TThickPoint> pts;
      if (m_smooth.getValue() == 0) {
        pts.push_back(point);
      } else {
        m_smoothStroke.beginStroke(m_smooth.getValue());
        m_smoothStroke.addPoint(point);
        m_smoothStroke.getSmoothPoints(pts);
      }
    }
    /*-- 作業中のFidを登録 --*/
    m_workingFrameId = getFrameId();

    invalidate(invalidateRect.enlarge(2));
  }
  // updating m_brushPos is needed to refresh viewer properly
  m_mousePos = pos;
  m_brushPos = getCenteredCursorPos(pos);
  m_perspectiveIndex = -1;
}

//-------------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::leftButtonDrag(const TPointD &pos,
                                          const TMouseEvent &e) {
  if (!m_enabled || !m_active) {
    m_mousePos = pos;
    m_brushPos = getCenteredCursorPos(pos);
    return;
  }

  TRectD invalidateRect;
  invalidateRect = TRectD(m_firstPoint, m_lastPoint).enlarge(2);
  m_lastPoint    = pos;
  TPointD usePos = pos;

  if (e.isAltPressed() || m_snapGrid.getValue()) {
    double distance = (m_brushPos.x - m_maxCursorThick + 1) * 0.5;
    TRectD brushRect =
        TRectD(TPointD(m_brushPos.x - distance, m_brushPos.y - distance),
               TPointD(m_brushPos.x + distance, m_brushPos.y + distance));
    invalidateRect += (brushRect);

    PerspectiveTool *perspectiveTool = dynamic_cast<PerspectiveTool *>(
        TTool::getTool("T_PerspectiveGrid", TTool::ToonzImage));
    std::vector<PerspectiveObject *> perspectiveObjs =
        perspectiveTool->getPerspectiveObjects();
    TPointD pointToUse = TPointD(0.0, 0.0);
    TPointD dpiScale   = getViewer()->getDpiScale();
    TPointD refPoint   = m_firstPoint;
    refPoint.x *= dpiScale.x;
    refPoint.y *= dpiScale.y;
    if (e.isAltPressed() || m_perspectiveIndex < 0) {
      // let's get info about our current location
      double denominator = m_lastPoint.x - m_firstPoint.x;
      double numerator   = m_lastPoint.y - m_firstPoint.y;
      if (areAlmostEqual(denominator, 0.0, 0.0001)) {
        denominator = denominator < 0 ? -0.0001 : 0.0001;
      }
      if (areAlmostEqual(numerator, 0.0, 0.0001)) {
        numerator = numerator < 0 ? -0.0001 : 0.0001;
      }
      double slope = (numerator / denominator);
      double angle = std::atan(slope) * (180 / 3.14159);

      // now let's get the angle of each of the assistant points
      std::vector<double> anglesToAssistants;

      for (auto data : perspectiveObjs) {
        TPointD point = data->getReferencePoint(refPoint);
        point.x /= dpiScale.x;
        point.y /= dpiScale.y;
        double newDenominator = point.x - m_firstPoint.x;
        double newNumerator   = point.y - m_firstPoint.y;
        if (areAlmostEqual(newDenominator, 0.0, 0.0001)) {
          newDenominator = newDenominator < 0 ? -0.0001 : 0.0001;
        }
        if (areAlmostEqual(newNumerator, 0.0, 0.0001)) {
          newNumerator = newNumerator < 0 ? -0.0001 : 0.0001;
        }

        double newSlope = (newNumerator / newDenominator);
        double newAngle = std::atan(newSlope) * (180 / 3.14159);
        anglesToAssistants.push_back(newAngle);
      }

      // figure out which angle is closer
      double difference = 360;

      for (int i = 0; i < anglesToAssistants.size(); i++) {
        double newDifference = abs(angle - anglesToAssistants.at(i));
        if (newDifference < difference || (180 - newDifference) < difference) {
          difference = std::min(newDifference, (180 - newDifference));
          pointToUse = perspectiveObjs.at(i)->getReferencePoint(refPoint);
          pointToUse.x /= dpiScale.x;
          pointToUse.y /= dpiScale.y;
          m_perspectiveIndex = i;
        }
      }
    } else {
      pointToUse =
          perspectiveObjs.at(m_perspectiveIndex)->getReferencePoint(refPoint);
      pointToUse.x /= dpiScale.x;
      pointToUse.y /= dpiScale.y;
    }

    double distanceFirstToLast =
        std::sqrt(std::pow((m_lastPoint.x - m_firstPoint.x), 2) +
                  std::pow((m_lastPoint.y - m_firstPoint.y), 2));
    double distanceLastToAssistant =
        std::sqrt(std::pow((pointToUse.x - m_lastPoint.x), 2) +
                  std::pow((pointToUse.y - m_lastPoint.y), 2));
    double distanceFirstToAssistant =
        std::sqrt(std::pow((pointToUse.x - m_firstPoint.x), 2) +
                  std::pow((pointToUse.y - m_firstPoint.y), 2));

    if (distanceFirstToAssistant == 0.0) distanceFirstToAssistant = 0.001;

    double ratio = distanceFirstToLast / distanceFirstToAssistant;

    double newX;
    double newY;

    // flip the direction if the last point is farther than the first point
    if (distanceFirstToAssistant < distanceLastToAssistant &&
        distanceFirstToLast < distanceLastToAssistant) {
      newX = ((1 + ratio) * m_firstPoint.x) - (ratio * pointToUse.x);
      newY = ((1 + ratio) * m_firstPoint.y) - (ratio * pointToUse.y);
    } else {
      newX = ((1 - ratio) * m_firstPoint.x) + (ratio * pointToUse.x);
      newY = ((1 - ratio) * m_firstPoint.y) + (ratio * pointToUse.y);
    }

    usePos = m_lastPoint = TPointD(newX, newY);
    invalidateRect += TRectD(m_firstPoint, m_lastPoint).enlarge(2);
  } else if (e.isCtrlPressed()) {
    double distance = (m_brushPos.x - m_maxCursorThick + 1) * 0.5;
    TRectD brushRect =
        TRectD(TPointD(m_brushPos.x - distance, m_brushPos.y - distance),
               TPointD(m_brushPos.x + distance, m_brushPos.y + distance));
    invalidateRect += (brushRect);

    double denominator = m_lastPoint.x - m_firstPoint.x;
    if (denominator == 0) denominator = 0.001;
    double slope    = ((m_lastPoint.y - m_firstPoint.y) / denominator);
    double radAngle = std::atan(abs(slope));
    double angle    = radAngle * (180 / 3.14159);
    if (abs(angle) >= 82.5) {
      // make it vertical
      m_lastPoint.x = m_firstPoint.x;
    } else if (abs(angle) < 7.5) {
      // make it horizontal
      m_lastPoint.y = m_firstPoint.y;
    } else {
      double xDistance = m_lastPoint.x - m_firstPoint.x;
      double yDistance = m_lastPoint.y - m_firstPoint.y;

      double totalDistance =
          std::sqrt(std::pow(xDistance, 2) + std::pow(yDistance, 2));
      double xLength = 0.0;
      double yLength = 0.0;
      if (angle >= 7.5 && angle < 22.5) {
        yLength = std::sin(15 * (3.14159 / 180)) * totalDistance;
        xLength = std::cos(15 * (3.14159 / 180)) * totalDistance;
      } else if (angle >= 22.5 && angle < 37.5) {
        yLength = std::sin(30 * (3.14159 / 180)) * totalDistance;
        xLength = std::cos(30 * (3.14159 / 180)) * totalDistance;
      } else if (angle >= 37.5 && angle < 52.5) {
        yLength = std::sin(45 * (3.14159 / 180)) * totalDistance;
        xLength = std::cos(45 * (3.14159 / 180)) * totalDistance;
      } else if (angle >= 52.5 && angle < 67.5) {
        yLength = std::sin(60 * (3.14159 / 180)) * totalDistance;
        xLength = std::cos(60 * (3.14159 / 180)) * totalDistance;
      } else if (angle >= 67.5 && angle < 82.5) {
        yLength = std::sin(75 * (3.14159 / 180)) * totalDistance;
        xLength = std::cos(75 * (3.14159 / 180)) * totalDistance;
      }

      if (yDistance == abs(yDistance)) {
        m_lastPoint.y = m_firstPoint.y + yLength;
      } else {
        m_lastPoint.y = m_firstPoint.y - yLength;
      }
      if (xDistance == abs(xDistance)) {
        m_lastPoint.x = m_firstPoint.x + xLength;
      } else {
        m_lastPoint.x = m_firstPoint.x - xLength;
      }
    }
  }

  // if (m_dragCount < 3 && !m_isMyPaintStyleSelected && thickness > 0.0) {
  //  if (m_hardness.getValue() == 100 || m_pencil.getValue()) {
  //    std::vector<TThickPoint> sequence =
  //    m_rasterTrack->getPointsSequence();
  //    if (sequence.size() > 0) sequence[0].thick = thickness;
  //    m_rasterTrack->setPointsSequence(sequence);
  //  } else if (m_points.size() > 0) {
  //    m_points[0].thick == thickness;
  //     // below is code to allow variable thickess
  //     // but it causes artifacting at the start of the stroke.
  //     TToonzImageP ti   = TImageP(getImage(true));
  //     TPointD rasCenter = ti->getRaster()->getCenterD();
  //     TThickPoint point(getCenteredCursorPos(m_firstPoint) + rasCenter,
  //                      thickness);
  //     m_points.push_back(point);
  //     m_bluredBrush->addPoint(point, 1);
  //     DrawOrder drawOrder = (DrawOrder)m_drawOrder.getIndex();

  //     TImageP img = getImage(true);
  //     TToonzImageP ri(img);
  //     m_strokeRect = m_bluredBrush->getBoundFromPoints(m_points);
  //     updateWorkAndBackupRasters(m_strokeRect);
  //     m_tileSaver->save(m_strokeRect);
  //     m_bluredBrush->updateDrawing(ri->getRaster(), m_backupRas,
  //     m_strokeRect,
  //                                 m_styleId, drawOrder);
  //     m_points.erase(m_points.begin());
  //  }
  //}

  // if (m_dragCount < 3 && m_isMyPaintStyleSelected) {
  //  TToonzImageP ti   = TImageP(getImage(true));
  //  TRasterP ras      = ti->getRaster();
  //  TPointD rasCenter = ti->getRaster()->getCenterD();
  //  TPointD point(getCenteredCursorPos(m_firstPoint) + rasCenter);
  //  double pressure =
  //      m_pressure.getValue() && e.isTablet() ? e.m_pressure : 0.5;

  //  m_strokeSegmentRect.empty();
  //  m_toonz_brush->strokeTo(point, pressure, restartBrushTimer());
  //  TRect updateRect = m_strokeSegmentRect * ras->getBounds();
  //  if (!updateRect.isEmpty()) {
  //    m_toonz_brush->updateDrawing(ras, m_backupRas, m_strokeSegmentRect,
  //                                 m_styleId);
  //  }
  //  m_lastRect = m_strokeRect;

  //  TPointD thickOffset(m_maxCursorThick * 0.5, m_maxCursorThick * 0.5);
  //  invalidateRect = convert(m_strokeSegmentRect) - rasCenter;
  //  invalidateRect +=
  //      TRectD(getCenteredCursorPos(m_firstPoint) - thickOffset,
  //             getCenteredCursorPos(m_firstPoint) + thickOffset);
  //  invalidateRect +=
  //      TRectD(m_brushPos - thickOffset, m_brushPos + thickOffset);
  //}

  // m_oldThickness = thickness;
  // double pressure =
  //    m_pressure.getValue() && e.isTablet() ? e.m_pressure : 0.5;
  // m_oldPressure = pressure;

  // m_dragCount++;

  if (m_isStraight) {
    m_mousePos = pos;
    m_brushPos = getCenteredCursorPos(pos);
    invalidate(invalidateRect);
    return;
  }

  TPointD centeredPos = getCenteredCursorPos(usePos);

  TToonzImageP ti   = TImageP(getImage(true));
  TPointD rasCenter = ti->getRaster()->getCenterD();
  int maxThickness  = m_rasThickness.getValue().second;
  double thickness  = (m_pressure.getValue())
                         ? computeThickness(e.m_pressure, m_rasThickness) * 2
                         : maxThickness;
  if (m_isMyPaintStyleSelected) {
    TRasterP ras = ti->getRaster();
    TPointD point(centeredPos + rasCenter);
    double pressure =
        m_pressure.getValue() && e.isTablet() ? e.m_pressure : 0.5;

    TThickPoint thickPoint(point, pressure);
    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.getSmoothPoints(pts);
    }

    invalidateRect.empty();
    double brushTimer = restartBrushTimer();
    for (size_t i = 0; i < pts.size(); ++i) {
      const TThickPoint &thickPoint2 = pts[i];
      m_strokeSegmentRect.empty();
      m_toonz_brush->strokeTo(thickPoint2, thickPoint2.thick, brushTimer);
      TRect updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty()) {
        // ras->extract(updateRect)->copy(m_workRaster->extract(updateRect));
        m_toonz_brush->updateDrawing(ras, m_backupRas, m_strokeSegmentRect,
                                     m_styleId, m_modifierLockAlpha.getValue());
      }

      m_lastRect = m_strokeRect;

      TPointD thickOffset(m_maxCursorThick * 0.5, m_maxCursorThick * 0.5);
      invalidateRect += convert(m_strokeSegmentRect) - rasCenter;
      invalidateRect +=
          TRectD(centeredPos - thickOffset, centeredPos + thickOffset);
      invalidateRect +=
          TRectD(m_brushPos - thickOffset, m_brushPos + thickOffset);
    }
  } else if (m_cmRasterBrush &&
             (m_hardness.getValue() == 100 || m_pencil.getValue())) {
    /*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる
     * --*/
    if (!m_pencil.getValue()) thickness -= 1.0;

    TThickPoint thickPoint(centeredPos + rasCenter, thickness);
    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.getSmoothPoints(pts);
    }
    for (size_t i = 0; i < pts.size(); ++i) {
      const TThickPoint &thickPoint = pts[i];
      m_cmRasterBrush->add(thickPoint);
      m_tileSaver->save(m_cmRasterBrush->getLastRect());
      m_cmRasterBrush->generateLastPieceOfStroke(m_pencil.getValue());
      std::vector<TThickPoint> brushPoints =
          m_cmRasterBrush->getPointsSequence();
      int m = (int)brushPoints.size();
      std::vector<TThickPoint> points;
      if (m_cmRasterBrush->hasSymmetryBrushes())
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
      invalidateRect += ToolUtils::getBounds(points, maxThickness) - rasCenter;
    }
  } else {
    // antialiased brush
    assert(m_workRas.getPointer() && m_backupRas.getPointer());
    TThickPoint thickPoint(centeredPos + rasCenter, thickness);
    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.getSmoothPoints(pts);
    }
    for (size_t i = 0; i < pts.size(); ++i) {
      TThickPoint old = m_points.back();

      const TThickPoint &point = pts[i];
      TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
      m_points.push_back(mid);
      m_points.push_back(point);

      TRect bbox;
      int m = (int)m_points.size();
      std::vector<TThickPoint> points;
      if (m == 3) {
        // ho appena cominciato. devo disegnare un segmento
        TThickPoint pa = m_points.front();
        points.push_back(pa);
        points.push_back(mid);
        bbox = m_bluredBrush->getBoundFromPoints(points);
        updateWorkAndBackupRasters(bbox + m_lastRect);
        m_tileSaver->save(bbox);
        m_bluredBrush->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
        m_lastRect += bbox;
      } else {
        points.push_back(m_points[m - 4]);
        points.push_back(old);
        points.push_back(mid);
        bbox = m_bluredBrush->getBoundFromPoints(points);
        updateWorkAndBackupRasters(bbox + m_lastRect);
        m_tileSaver->save(bbox);
        m_bluredBrush->addArc(m_points[m - 4], old, mid, 1, 1);
        m_lastRect += bbox;
      }

      std::vector<TThickPoint> symmPts =
          m_bluredBrush->getSymmetryPoints(points);
      points.insert(points.end(), symmPts.begin(), symmPts.end());

      invalidateRect += ToolUtils::getBounds(points, maxThickness) - rasCenter;

      m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox,
                                   m_styleId, m_drawOrder.getIndex(),
                                   m_modifierLockAlpha.getValue());
      m_strokeRect += bbox;
    }
  }

  // clear & draw brush tip when drawing smooth stroke
  if (m_smooth.getValue() != 0) {
    TPointD halfThick(m_maxThick * 0.5, m_maxThick * 0.5);
    invalidateRect += TRectD(m_brushPos - halfThick, m_brushPos + halfThick);
    invalidateRect += TRectD(centeredPos - halfThick, centeredPos + halfThick);
  }

  m_mousePos = pos;
  m_brushPos = getCenteredCursorPos(pos);

  invalidate(invalidateRect.enlarge(2));
}

//---------------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::leftButtonUp(const TPointD &pos,
                                        const TMouseEvent &e) {
  bool isValid = m_enabled && m_active;
  m_enabled    = false;
  m_active     = false;
  if (!isValid) {
    return;
  }
  TPointD centeredPos = getCenteredCursorPos(pos);
  if (e.isCtrlPressed() || m_snapGrid.getValue() || e.isAltPressed())
    centeredPos   = getCenteredCursorPos(m_lastPoint);
  double pressure = m_pressure.getValue() && e.isTablet() ? e.m_pressure : 0.5;
  // if (!e.isTablet()) m_oldThickness = -1.0;
  if (m_isStraight && m_isMyPaintStyleSelected && m_oldPressure > 0.0)
    pressure = m_oldPressure;
  finishRasterBrush(centeredPos, pressure);
  int tc = ToonzCheck::instance()->getChecks();
  if (tc & ToonzCheck::eGap || tc & ToonzCheck::eAutoclose) invalidate();
  m_perspectiveIndex = -1;
}

//---------------------------------------------------------------------------------------------------------------
/*!
 * ドラッグ中にツールが切り替わった場合に備え、onDeactivate時とMouseRelease時にと同じ終了処理を行う
 */
void ToonzRasterBrushTool::finishRasterBrush(const TPointD &pos,
                                             double pressureVal) {
  TToonzImageP ti = TImageP(getImage(true));

  if (!ti) return;

  TPointD rasCenter         = ti->getRaster()->getCenterD();
  TTool::Application *app   = TTool::getApplication();
  TXshLevel *level          = app->getCurrentLevel()->getLevel();
  TXshSimpleLevelP simLevel = level->getSimpleLevel();

  bool isEditingLevel = m_application->getCurrentFrame()->isEditingLevel();
  bool renameColumn   = m_isFrameCreated;
  if (!isEditingLevel && renameColumn) TUndoManager::manager()->beginBlock();

  /*--
   * 描画中にカレントフレームが変わっても、描画開始時のFidに対してUndoを記録する
   * --*/
  TFrameId frameId =
      m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId;
  if (m_isMyPaintStyleSelected) {
    TRasterCM32P ras = ti->getRaster();
    TPointD point(pos + rasCenter);
    double pressure = m_pressure.getValue() ? pressureVal : 0.5;

    TThickPoint thickPoint(point, pressure);
    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0 || m_isStraight) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.endStroke();
      m_smoothStroke.getSmoothPoints(pts);
    }
    TRectD invalidateRect;
    double brushTimer = restartBrushTimer();
    for (size_t i = 0; i < pts.size(); ++i) {
      const TThickPoint &thickPoint2 = pts[i];

      m_strokeSegmentRect.empty();
      m_toonz_brush->strokeTo(thickPoint2, thickPoint2.thick, brushTimer);
      if (i == pts.size() - 1) m_toonz_brush->endStroke();
      TRect updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty()) {
        // ras->extract(updateRect)->copy(m_workRaster->extract(updateRect));
        m_toonz_brush->updateDrawing(ras, m_backupRas, m_strokeSegmentRect,
                                     m_styleId, m_modifierLockAlpha.getValue());
      }
      TPointD thickOffset(m_maxCursorThick * 0.5,
                          m_maxCursorThick * 0.5);  // TODO
      invalidateRect += convert(m_strokeSegmentRect) - rasCenter;
      invalidateRect += TRectD(pos - thickOffset, pos + thickOffset);
    }
    invalidate(invalidateRect.enlarge(2.0));

    if (m_toonz_brush) {
      delete m_toonz_brush;
      m_toonz_brush = 0;
    }

    m_lastRect.empty();
    m_workRas->unlock();

    if (m_tileSet->getTileCount() > 0) {
      TRasterCM32P subras = ras->extract(m_strokeRect)->clone();
      TUndoManager::manager()->add(new MyPaintBrushUndo(
          m_tileSet, simLevel.getPointer(), frameId, m_isFrameCreated,
          m_isLevelCreated, subras, m_strokeRect.getP00()));
    }

  } else if (m_cmRasterBrush &&
             (m_hardness.getValue() == 100 || m_pencil.getValue())) {
    double thickness = m_pressure.getValue()
                           ? computeThickness(pressureVal, m_rasThickness)
                           : m_rasThickness.getValue().second;

    /*--- ストロークの最初にMaxサイズの円が描かれてしまう不具合を防止する ---*/
    if (m_pressure.getValue() && pressureVal == 1.0)
      thickness = m_rasThickness.getValue().first;

    /*-- Pencilモードでなく、Hardness=100 の場合のブラシサイズを1段階下げる --*/
    if (!m_pencil.getValue()) thickness -= 1.0;
    if (m_isStraight) {
      // if (m_oldThickness > 0.0) {
      //  thickness = m_oldThickness;
      //} else
      thickness = m_cmRasterBrush->getPointsSequence(true).at(0).thick;
    }
    TRectD invalidateRect;
    TThickPoint thickPoint(pos + rasCenter, thickness);
    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0 || m_isStraight) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.endStroke();
      m_smoothStroke.getSmoothPoints(pts);
    }
    for (size_t i = 0; i < pts.size(); ++i) {
      const TThickPoint &thickPoint = pts[i];
      m_cmRasterBrush->add(thickPoint);
      if (m_isStraight) {
        m_tileSaver->save(m_cmRasterBrush->getLastRect(true));
        m_cmRasterBrush->generateLastPieceOfStroke(m_pencil.getValue(), true,
                                                   true);
      } else {
        m_tileSaver->save(m_cmRasterBrush->getLastRect());
        m_cmRasterBrush->generateLastPieceOfStroke(m_pencil.getValue(), true);
      }

      std::vector<TThickPoint> brushPoints =
          m_cmRasterBrush->getPointsSequence();
      int m = (int)brushPoints.size();
      std::vector<TThickPoint> points;
      if (m_cmRasterBrush->hasSymmetryBrushes())
        points = brushPoints;
      else {
        if (m_isStraight) {
          points.push_back(brushPoints[0]);
          points.push_back(brushPoints[2]);
        } else if (m == 3) {
          points.push_back(brushPoints[0]);
          points.push_back(brushPoints[1]);
        } else {
          points.push_back(brushPoints[m - 4]);
          points.push_back(brushPoints[m - 3]);
          points.push_back(brushPoints[m - 2]);
        }
      }
      int maxThickness = m_rasThickness.getValue().second;
      invalidateRect += ToolUtils::getBounds(points, maxThickness) - rasCenter;
    }
    invalidate(invalidateRect.enlarge(2));

    if (m_tileSet->getTileCount() > 0) {
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
      TUndoManager::manager()->add(new RasterBrushUndo(
          m_tileSet, m_cmRasterBrush->getPointsSequence(true),
          m_cmRasterBrush->getStyleId(), m_cmRasterBrush->isSelective(),
          simLevel.getPointer(), frameId, m_pencil.getValue(), m_isFrameCreated,
          m_isLevelCreated, m_cmRasterBrush->isPaletteOrder(),
          m_cmRasterBrush->isAlphaLocked(), dpiScale, symmetryLines, rotation,
          centerPoint, useLineSymmetry, m_isStraight));
    }
    delete m_cmRasterBrush;
    m_cmRasterBrush = 0;
  } else {
    double maxThickness = m_rasThickness.getValue().second;
    double thickness    = (m_pressure.getValue())
                           ? computeThickness(pressureVal, m_rasThickness)
                           : maxThickness;
    if (m_isStraight) {
      // if (m_oldThickness > 0.0)
      //  thickness = m_oldThickness;
      // else
      thickness = m_points[0].thick;
    }
    TPointD rasCenter = ti->getRaster()->getCenterD();
    TRectD invalidateRect;
    TThickPoint thickPoint(pos + rasCenter, thickness);
    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0 || m_isStraight) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.endStroke();
      m_smoothStroke.getSmoothPoints(pts);
    }
    // we need to skip the for-loop here if pts.size() == 0 or else
    // (pts.size() - 1) becomes ULLONG_MAX since size_t is unsigned
    if (pts.size() > 0) {
      // this doens't get run for a straight line
      for (size_t i = 0; i < pts.size() - 1; ++i) {
        TThickPoint old = m_points.back();

        const TThickPoint &point = pts[i];
        TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
        m_points.push_back(mid);
        m_points.push_back(point);

        TRect bbox;
        int m = (int)m_points.size();
        std::vector<TThickPoint> points;
        if (m == 3) {
          // ho appena cominciato. devo disegnare un segmento
          TThickPoint pa = m_points.front();
          points.push_back(pa);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
          m_lastRect += bbox;
        } else {
          points.push_back(m_points[m - 4]);
          points.push_back(old);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(m_points[m - 4], old, mid, 1, 1);
          m_lastRect += bbox;
        }

        std::vector<TThickPoint> allPts = points;
        std::vector<TThickPoint> symmPts =
            m_bluredBrush->getSymmetryPoints(points);
        allPts.insert(allPts.end(), symmPts.begin(), symmPts.end());

        invalidateRect +=
            ToolUtils::getBounds(allPts, maxThickness) - rasCenter;

        m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox,
                                     m_styleId, m_drawOrder.getIndex(),
                                     m_modifierLockAlpha.getValue());
        m_strokeRect += bbox;
      }
      if (!m_isStraight && m_points.size() > 1) {
        TThickPoint point = pts.back();
        m_points.push_back(point);
      }
      int m = m_points.size();
      std::vector<TThickPoint> points;
      if (!m_isStraight && m_points.size() > 1) {
        points.push_back(m_points[m - 3]);
        points.push_back(m_points[m - 2]);
        points.push_back(m_points[m - 1]);
      } else {
        const TThickPoint point = m_points[0];
        TThickPoint mid((thickPoint + point) * 0.5,
                        (point.thick + thickPoint.thick) * 0.5);
        m_points.push_back(mid);
        m_points.push_back(thickPoint);
        points.push_back(m_points[0]);
        points.push_back(m_points[1]);
        points.push_back(m_points[2]);
      }
      TRect bbox = m_bluredBrush->getBoundFromPoints(points);
      updateWorkAndBackupRasters(bbox);
      m_tileSaver->save(bbox);
      m_bluredBrush->addArc(points[0], points[1], points[2], 1, 1);
      m_bluredBrush->updateDrawing(ti->getRaster(), m_backupRas, bbox,
                                   m_styleId, m_drawOrder.getIndex(),
                                   m_modifierLockAlpha.getValue());

      std::vector<TThickPoint> allPts = points;
      std::vector<TThickPoint> symmPts =
          m_bluredBrush->getSymmetryPoints(points);
      allPts.insert(allPts.end(), symmPts.begin(), symmPts.end());

      invalidateRect += ToolUtils::getBounds(allPts, maxThickness) - rasCenter;

      m_lastRect += bbox;
      m_strokeRect += bbox;
    }
    if (!invalidateRect.isEmpty()) invalidate(invalidateRect.enlarge(2));
    m_lastRect.empty();

    delete m_bluredBrush;
    m_bluredBrush = 0;

    if (m_tileSet->getTileCount() > 0) {
      TPointD dpiScale     = getViewer()->getDpiScale();
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

      TUndoManager::manager()->add(new RasterBluredBrushUndo(
          m_tileSet, m_points, m_styleId, (DrawOrder)m_drawOrder.getIndex(),
          m_modifierLockAlpha.getValue(), simLevel.getPointer(), frameId,
          m_rasThickness.getValue().second, m_hardness.getValue() * 0.01,
          m_isFrameCreated, m_isLevelCreated, dpiScale, symmetryLines, rotation,
          centerPoint, useLineSymmetry, m_isStraight));
    }
  }

  // Column name renamed to level name only if was originally empty
  if (!isEditingLevel && renameColumn) {
    int col            = app->getCurrentColumn()->getColumnIndex();
    TXshColumn *column = app->getCurrentXsheet()->getXsheet()->getColumn(col);
    int r0, r1;
    column->getRange(r0, r1);
    if (r0 == r1) {
      TStageObjectId columnId = TStageObjectId::ColumnId(col);
      std::string columnName =
          QString::fromStdWString(simLevel->getName()).toStdString();
      TStageObjectCmd::rename(columnId, columnName, app->getCurrentXsheet());
    }

    TUndoManager::manager()->endBlock();
  }

  delete m_tileSaver;
  m_isStraight    = false;
  m_oldPressure   = -1.0;
  // m_oldThickness  = -1.0;
  // m_dragCount     = 0;
  m_tileSaver = 0;

  /*-- FIdを指定して、描画中にフレームが動いても、
  　　描画開始時のFidのサムネイルが更新されるようにする。--*/
  notifyImageChanged(frameId);

  m_strokeRect.empty();

  ToolUtils::updateSaveBox();

  /*-- 作業中のフレームをリセット --*/
  m_workingFrameId = TFrameId();
}
//---------------------------------------------------------------------------------------------------------------
// 明日はここをMyPaintのときにカーソルを消せるように修正する！！！！！！
void ToonzRasterBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
#if (!defined(LINUX) && !defined(FREEBSD))
  qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
#endif

  struct Locals {
    ToonzRasterBrushTool *m_this;

    void setValue(TDoublePairProperty &prop,
                  const TDoublePairProperty::Value &value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addMinMax(TDoublePairProperty &prop, double add) {
      if (add == 0.0) return;
      const TDoublePairProperty::Range &range = prop.getRange();

      TDoublePairProperty::Value value = prop.getValue();
      value.first  = tcrop(value.first + add, range.first, range.second);
      value.second = tcrop(value.second + add, range.first, range.second);

      setValue(prop, value);
    }

    void addMinMaxSeparate(TDoublePairProperty &prop, double min, double max) {
      if (min == 0.0 && max == 0.0) return;
      const TDoublePairProperty::Range &range = prop.getRange();

      TDoublePairProperty::Value value = prop.getValue();
      value.first += min;
      value.second += max;
      if (value.first > value.second) value.first = value.second;
      value.first  = tcrop(value.first, range.first, range.second);
      value.second = tcrop(value.second, range.first, range.second);

      setValue(prop, value);
    }

  } locals = {this};

  // if (e.isAltPressed() && !e.isCtrlPressed()) {
  // const TPointD &diff = pos - m_mousePos;
  // double add = (fabs(diff.x) > fabs(diff.y)) ? diff.x : diff.y;

  // locals.addMinMax(
  //  TToonzImageP(getImage(false, 1)) ? m_rasThickness : m_thickness, add);
  //} else

  double thickness =
      (m_isMyPaintStyleSelected) ? (double)(m_maxCursorThick + 1) : m_maxThick;
  TPointD halfThick(thickness * 0.5, thickness * 0.5);
  TRectD invalidateRect(m_brushPos - halfThick, m_brushPos + halfThick);

  if (e.isCtrlPressed() && e.isAltPressed() && !e.isShiftPressed() &&
      Preferences::instance()->useCtrlAltToResizeBrushEnabled()) {
    // Resize the brush if CTRL+ALT is pressed and the preference is enabled.
    const TPointD &diff = m_windowMousePos - -e.m_pos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMaxSeparate(m_rasThickness, min, max);

    double radius = m_rasThickness.getValue().second * 0.5;
    invalidateRect += TRectD(m_brushPos - TPointD(radius, radius),
                             m_brushPos + TPointD(radius, radius));

  } else {
    m_mousePos = pos;
    m_brushPos = getCenteredCursorPos(pos);
    m_windowMousePos = -e.m_pos;

    invalidateRect += TRectD(pos - halfThick, pos + halfThick);
  }

  invalidate(invalidateRect.enlarge(2));

  if (m_minThick == 0 && m_maxThick == 0) {
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;
  }
}

//-------------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::draw() {
  int devPixRatio = m_viewer->getDevPixRatio();
 
  glLineWidth(1.0 * devPixRatio);

  if (m_isStraight) {
    tglColor(TPixel32::Red);
    tglDrawSegment(m_firstPoint, m_lastPoint);
  }
  if (m_minThick == 0 && m_maxThick == 0 &&
      !Preferences::instance()->getShow0ThickLines())
    return;

  TImageP img = getImage(false, 1);

  if (getApplication()->getCurrentObject()->isSpline()) return;

  // If toggled off, don't draw brush outline
  if (!Preferences::instance()->isCursorOutlineEnabled()) return;

  // Draw the brush outline - change color when the Ink / Paint check is
  // activated
  if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::eInk1))
    glColor3d(0.5, 0.8, 0.8);
  // normally draw in red
  else
    glColor3d(1.0, 0.0, 0.0);

  if (m_isMyPaintStyleSelected) {
    tglDrawCircle(m_brushPos, (m_minCursorThick + 1) * 0.5);
    tglDrawCircle(m_brushPos, (m_maxCursorThick + 1) * 0.5);
    return;
  }

  if (TToonzImageP ti = img) {
    TRasterP ras = ti->getRaster();
    int lx       = ras->getLx();
    int ly       = ras->getLy();
    drawEmptyCircle(m_brushPos, tround(m_minThick), lx % 2 == 0, ly % 2 == 0,
                    m_pencil.getValue());
    drawEmptyCircle(m_brushPos, tround(m_maxThick), lx % 2 == 0, ly % 2 == 0,
                    m_pencil.getValue());
  } else {
    drawEmptyCircle(m_brushPos, tround(m_minThick), true, true,
                    m_pencil.getValue());
    drawEmptyCircle(m_brushPos, tround(m_maxThick), true, true,
                    m_pencil.getValue());
  }
}

//--------------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onEnter() {
  TImageP img = getImage(false);

  m_minThick = m_rasThickness.getValue().first;
  m_maxThick = m_rasThickness.getValue().second;
  updateCurrentStyle();

  Application *app = getApplication();

  m_styleId       = app->getCurrentLevelStyleIndex();
  TColorStyle *cs = app->getCurrentLevelStyle();
  if (cs) {
    TRasterStyleFx *rfx = cs->getRasterStyleFx();
    m_active            = cs->isStrokeStyle() || (rfx && rfx->isInkStyle());
    m_currentColor      = cs->getAverageColor();
    m_currentColor.m    = 255;
  } else {
    m_currentColor = TPixel32::Black;
  }
  m_active = img;
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onLeave() {
  m_minThick       = 0;
  m_maxThick       = 0;
  m_minCursorThick = 0;
  m_maxCursorThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *ToonzRasterBrushTool::getProperties(int idx) {
  if (!m_presetsLoaded) initPresets();

  return &m_prop[idx];
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onImageChanged() {
  if (!isEnabled()) return;

  setWorkAndBackupImages();
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::setWorkAndBackupImages() {
  TToonzImageP ti = (TToonzImageP)getImage(false, 1);
  if (!ti) return;
  TRasterP ras   = ti->getRaster();
  TDimension dim = ras->getSize();

  double hardness = m_hardness.getValue() * 0.01;
  if (!m_isMyPaintStyleSelected && hardness == 1.0 &&
      ras->getPixelSize() == 4) {
    m_workRas   = TRaster32P();
    m_backupRas = TRasterCM32P();
  } else {
    if (!m_workRas || m_workRas->getLx() > dim.lx ||
        m_workRas->getLy() > dim.ly)
      m_workRas = TRaster32P(dim);
    if (!m_backupRas || m_backupRas->getLx() > dim.lx ||
        m_backupRas->getLy() > dim.ly)
      m_backupRas = TRasterCM32P(dim);

    m_strokeRect.empty();
    m_lastRect.empty();
  }
}

//------------------------------------------------------------------

bool ToonzRasterBrushTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  if (propertyName == m_preset.getName()) {
    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last saved brush settings
      loadLastBrush();

    RasterBrushPreset  = m_preset.getValueAsString();
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
    return true;
  }

  RasterBrushMinSize       = m_rasThickness.getValue().first;
  RasterBrushMaxSize       = m_rasThickness.getValue().second;
  BrushSmooth              = m_smooth.getValue();
  BrushDrawOrder           = m_drawOrder.getIndex();
  RasterBrushPencilMode    = m_pencil.getValue();
  BrushPressureSensitivity = m_pressure.getValue();
  RasterBrushHardness      = m_hardness.getValue();
  RasterBrushModifierSize  = m_modifierSize.getValue();
  BrushLockAlpha           = m_modifierLockAlpha.getValue();
  BrushSnapGrid            = m_snapGrid.getValue();

  // Recalculate/reset based on changed settings
  if (propertyName == m_rasThickness.getName()) {
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;
  }

  if (propertyName == m_hardness.getName()) setWorkAndBackupImages();

  if (propertyName == m_hardness.getName() ||
      propertyName == m_rasThickness.getName()) {
    m_brushPad = getBrushPad(m_rasThickness.getValue().second,
                             m_hardness.getValue() * 0.01);
    TRectD rect(m_mousePos - TPointD(m_maxThick + 2, m_maxThick + 2),
                m_mousePos + TPointD(m_maxThick + 2, m_maxThick + 2));
    invalidate(rect);
  }

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    RasterBrushPreset  = m_preset.getValueAsString();
    m_propertyUpdating = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

  return true;
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    m_presetsManager.load(ToonzFolder::getMyModuleDir() +
                          "brush_smartraster.txt");
  }

  // Rebuild the presets property entries
  const std::set<BrushData> &presets = m_presetsManager.presets();

  m_preset.deleteAllValues();
  m_preset.addValue(CUSTOM_WSTR);
  m_preset.setItemUIName(CUSTOM_WSTR, tr("<custom>"));

  std::set<BrushData>::const_iterator it, end = presets.end();
  for (it = presets.begin(); it != end; ++it) m_preset.addValue(it->m_name);
}

//----------------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::loadPreset() {
  const std::set<BrushData> &presets = m_presetsManager.presets();
  std::set<BrushData>::const_iterator it;

  it = presets.find(BrushData(m_preset.getValue()));
  if (it == presets.end()) return;

  const BrushData &preset = *it;

  try  // Don't bother with RangeErrors
  {
    m_rasThickness.setValue(
        TDoublePairProperty::Value(std::max(preset.m_min, 1.0), preset.m_max));
    m_hardness.setValue(preset.m_hardness, true);
    m_smooth.setValue(preset.m_smooth, true);
    m_drawOrder.setIndex(preset.m_drawOrder);
    m_pencil.setValue(preset.m_pencil);
    m_pressure.setValue(preset.m_pressure);
    m_modifierSize.setValue(preset.m_modifierSize);
    m_modifierLockAlpha.setValue(preset.m_modifierLockAlpha);

    // Recalculate based on updated presets
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;

    setWorkAndBackupImages();

    m_brushPad = ToolUtils::getBrushPad(preset.m_max, preset.m_hardness * 0.01);
  } catch (...) {
  }
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::addPreset(QString name) {
  // Build the preset
  BrushData preset(name.toStdWString());

  preset.m_min = m_rasThickness.getValue().first;
  preset.m_max = m_rasThickness.getValue().second;

  preset.m_smooth            = m_smooth.getValue();
  preset.m_hardness          = m_hardness.getValue();
  preset.m_drawOrder         = m_drawOrder.getIndex();
  preset.m_pencil            = m_pencil.getValue();
  preset.m_pressure          = m_pressure.getValue();
  preset.m_modifierSize      = m_modifierSize.getValue();
  preset.m_modifierLockAlpha = m_modifierLockAlpha.getValue();

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_name);
  RasterBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);
  initPresets();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  RasterBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::loadLastBrush() {
  m_rasThickness.setValue(
      TDoublePairProperty::Value(RasterBrushMinSize, RasterBrushMaxSize));
  m_drawOrder.setIndex(BrushDrawOrder);
  m_pencil.setValue(RasterBrushPencilMode ? 1 : 0);
  m_hardness.setValue(RasterBrushHardness);
  m_pressure.setValue(BrushPressureSensitivity ? 1 : 0);
  m_smooth.setValue(BrushSmooth);
  m_modifierSize.setValue(RasterBrushModifierSize);
  m_modifierLockAlpha.setValue(BrushLockAlpha ? 1 : 0);
  m_snapGrid.setValue(BrushSnapGrid ? 1 : 0);

  // Recalculate based on prior values
  m_minThick = m_rasThickness.getValue().first;
  m_maxThick = m_rasThickness.getValue().second;

  setWorkAndBackupImages();

  m_brushPad = getBrushPad(m_rasThickness.getValue().second,
                           m_hardness.getValue() * 0.01);
}

//------------------------------------------------------------------
/*!	Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す
 */
bool ToonzRasterBrushTool::isPencilModeActive() {
  return getTargetType() == TTool::ToonzImage && m_pencil.getValue();
}

//---------------------------------------------------------------------------------------------------

void ToonzRasterBrushTool::onCanvasSizeChanged() {
  onDeactivate();
  setWorkAndBackupImages();
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::onColorStyleChanged() {
  // in case the style switched while drawing
  if (m_tileSaver) {
    bool isValid = m_enabled && m_active;
    m_enabled    = false;
    if (isValid) {
      finishRasterBrush(m_mousePos, 1);
    }
  }

  TTool::Application *app = getApplication();
  TMyPaintBrushStyle *mpbs =
      dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
  m_isMyPaintStyleSelected = (mpbs) ? true : false;
  setWorkAndBackupImages();
  getApplication()->getCurrentTool()->notifyToolChanged();
}

//------------------------------------------------------------------

double ToonzRasterBrushTool::restartBrushTimer() {
  double dtime = m_brushTimer.nsecsElapsed() * 1e-9;
  m_brushTimer.restart();
  return dtime;
}

//------------------------------------------------------------------

void ToonzRasterBrushTool::updateCurrentStyle() {
  if (m_isMyPaintStyleSelected) {
    TTool::Application *app = TTool::getApplication();
    TMyPaintBrushStyle *brushStyle =
        dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
    if (!brushStyle) {
      // brush changed to normal abnormally. Complete color style change.
      onColorStyleChanged();
      return;
    }
    double radiusLog = brushStyle->getBrush().getBaseValue(
                           MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC) +
                       m_modifierSize.getValue() * log(2.0);
    double radius    = exp(radiusLog);
    m_minCursorThick = m_maxCursorThick = (int)std::round(2.0 * radius);
  }
}
//==========================================================================================================

ToonzRasterBrushToolNotifier::ToonzRasterBrushToolNotifier(
    ToonzRasterBrushTool *tool)
    : m_tool(tool) {
  if (TTool::Application *app = m_tool->getApplication()) {
    if (TXshLevelHandle *levelHandle = app->getCurrentLevel()) {
      bool ret = connect(levelHandle, SIGNAL(xshCanvasSizeChanged()), this,
                         SLOT(onCanvasSizeChanged()));
      assert(ret);
    }
    if (TPaletteHandle *paletteHandle = app->getCurrentPalette()) {
      bool ret;
      ret = connect(paletteHandle, SIGNAL(colorStyleChanged(bool)), this,
                    SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(colorStyleSwitched()), this,
                           SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(paletteSwitched()), this,
                           SLOT(onColorStyleChanged()));
      assert(ret);
    }
  }
  onColorStyleChanged();
}

//==========================================================================================================

// Tools instantiation

ToonzRasterBrushTool toonzPencil("T_Brush",
                                 TTool::ToonzImage | TTool::EmptyTarget);

//*******************************************************************************
//    Brush Data implementation
//*******************************************************************************

BrushData::BrushData()
    : m_name()
    , m_min(0.0)
    , m_max(0.0)
    , m_smooth(0.0)
    , m_hardness(0.0)
    , m_opacityMin(0.0)
    , m_opacityMax(0.0)
    , m_drawOrder(0)
    , m_pencil(false)
    , m_pressure(false)
    , m_modifierSize(0.0)
    , m_modifierOpacity(0.0)
    , m_modifierEraser(0.0)
    , m_modifierLockAlpha(0.0) {}

//----------------------------------------------------------------------------------------------------------

BrushData::BrushData(const std::wstring &name)
    : m_name(name)
    , m_min(0.0)
    , m_max(0.0)
    , m_smooth(0.0)
    , m_hardness(0.0)
    , m_opacityMin(0.0)
    , m_opacityMax(0.0)
    , m_drawOrder(0)
    , m_pencil(false)
    , m_pressure(false)
    , m_modifierSize(0.0)
    , m_modifierOpacity(0.0)
    , m_modifierEraser(0.0)
    , m_modifierLockAlpha(0.0) {}

//----------------------------------------------------------------------------------------------------------

void BrushData::saveData(TOStream &os) {
  os.openChild("Name");
  os << m_name;
  os.closeChild();
  os.openChild("Thickness");
  os << m_min << m_max;
  os.closeChild();
  os.openChild("Smooth");
  os << m_smooth;
  os.closeChild();
  os.openChild("Hardness");
  os << m_hardness;
  os.closeChild();
  os.openChild("Opacity");
  os << m_opacityMin << m_opacityMax;
  os.closeChild();
  os.openChild("Draw_Order");
  os << m_drawOrder;
  os.closeChild();
  os.openChild("Pencil");
  os << (int)m_pencil;
  os.closeChild();
  os.openChild("Pressure_Sensitivity");
  os << (int)m_pressure;
  os.closeChild();
  os.openChild("Modifier_Size");
  os << m_modifierSize;
  os.closeChild();
  os.openChild("Modifier_Opacity");
  os << m_modifierOpacity;
  os.closeChild();
  os.openChild("Modifier_Eraser");
  os << (int)m_modifierEraser;
  os.closeChild();
  os.openChild("Modifier_LockAlpha");
  os << (int)m_modifierLockAlpha;
  os.closeChild();
}

//----------------------------------------------------------------------------------------------------------

void BrushData::loadData(TIStream &is) {
  std::string tagName;
  int val;

  while (is.matchTag(tagName)) {
    if (tagName == "Name")
      is >> m_name, is.matchEndTag();
    else if (tagName == "Thickness")
      is >> m_min >> m_max, is.matchEndTag();
    else if (tagName == "Smooth")
      is >> m_smooth, is.matchEndTag();
    else if (tagName == "Hardness")
      is >> m_hardness, is.matchEndTag();
    else if (tagName == "Opacity")
      is >> m_opacityMin >> m_opacityMax, is.matchEndTag();
    else if (tagName == "Selective" ||
             tagName == "Draw_Order")  // "Selective" is left to keep backward
                                       // compatibility
      is >> m_drawOrder, is.matchEndTag();
    else if (tagName == "Pencil")
      is >> val, m_pencil = val, is.matchEndTag();
    else if (tagName == "Pressure_Sensitivity")
      is >> val, m_pressure = val, is.matchEndTag();
    else if (tagName == "Modifier_Size")
      is >> m_modifierSize, is.matchEndTag();
    else if (tagName == "Modifier_Opacity")
      is >> m_modifierOpacity, is.matchEndTag();
    else if (tagName == "Modifier_Eraser")
      is >> val, m_modifierEraser = val, is.matchEndTag();
    else if (tagName == "Modifier_LockAlpha")
      is >> val, m_modifierLockAlpha = val, is.matchEndTag();
    else
      is.skipCurrentTag();
  }
}

//----------------------------------------------------------------------------------------------------------

PERSIST_IDENTIFIER(BrushData, "BrushData");

//*******************************************************************************
//    Brush Preset Manager implementation
//*******************************************************************************

void BrushPresetManager::load(const TFilePath &fp) {
  m_fp = fp;

  std::string tagName;
  BrushData data;

  TIStream is(m_fp);
  try {
    while (is.matchTag(tagName)) {
      if (tagName == "version") {
        VersionNumber version;
        is >> version.first >> version.second;

        is.setVersion(version);
        is.matchEndTag();
      } else if (tagName == "brushes") {
        while (is.matchTag(tagName)) {
          if (tagName == "brush") {
            is >> data, m_presets.insert(data);
            is.matchEndTag();
          } else
            is.skipCurrentTag();
        }

        is.matchEndTag();
      } else
        is.skipCurrentTag();
    }
  } catch (...) {
  }
}

//------------------------------------------------------------------

void BrushPresetManager::save() {
  TSystem::touchParentDir(m_fp);

  TOStream os(m_fp);

  os.openChild("version");
  os << 1 << 19;
  os.closeChild();

  os.openChild("brushes");

  std::set<BrushData>::iterator it, end = m_presets.end();
  for (it = m_presets.begin(); it != end; ++it) {
    os.openChild("brush");
    os << (TPersist &)*it;
    os.closeChild();
  }

  os.closeChild();
}

//------------------------------------------------------------------

void BrushPresetManager::addPreset(const BrushData &data) {
  m_presets.erase(data);  // Overwriting insertion
  m_presets.insert(data);
  save();
}

//------------------------------------------------------------------

void BrushPresetManager::removePreset(const std::wstring &name) {
  m_presets.erase(BrushData(name));
  save();
}
