

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/toolcommandids.h"
#include "tools/toolutils.h"
#include "tools/toolhandle.h"

#include "historytypes.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/trasterimageutils.h"
#include "toonz/stage2.h"
#include "toonz/levelproperties.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tproperty.h"
#include "trasterimage.h"
#include "bluredbrush.h"
#include "trop.h"
#include "tgl.h"
#include "tstroke.h"
#include "drawutil.h"
#include "tinbetween.h"
#include "tpixelutils.h"
#include "cmrasterbrush.h"
#include "symmetrytool.h"
#include "vectorbrush.h"
#include "symmetrystroke.h"

// Qt includes
#include <QCoreApplication>

using namespace ToolUtils;

#define NORMALERASE L"Normal"
#define RECTERASE L"Rectangular"
#define FREEHANDERASE L"Freehand"
#define POLYLINEERASE L"Polyline"

#define LINEAR_INTERPOLATION L"Linear"
#define EASE_IN_INTERPOLATION L"Ease In"
#define EASE_OUT_INTERPOLATION L"Ease Out"
#define EASE_IN_OUT_INTERPOLATION L"Ease In/Out"

TEnv::DoubleVar FullcolorEraseMinSize("FullcolorEraseMinSize", 1);
TEnv::DoubleVar FullcolorEraseSize("FullcolorEraseSize", 5);
TEnv::DoubleVar FullcolorEraseHardness("FullcolorEraseHardness", 100);
TEnv::DoubleVar FullcolorEraserOpacity("FullcolorEraserOpacity", 100);
TEnv::StringVar FullcolorEraserType("FullcolorEraseType", "Normal");
TEnv::IntVar FullcolorEraserInvert("FullcolorEraseInvert", 0);
TEnv::IntVar FullcolorEraserRange("FullcolorEraseRange", 0);
TEnv::IntVar FullcolorEraserPressure("FullcolorEraserPressure", 1);

//**********************************************************************************
//    Local namespace  stuff
//**********************************************************************************

namespace {

int computeThickness(double pressure, const TIntPairProperty &property) {
  double t = pressure * pressure * pressure;
  int thick0 = property.getValue().first;
  int thick1 = property.getValue().second;

  return tround(thick0 + (thick1 - thick0) * t);
}

//----------------------------------------------------------------------------------

void eraseImage(const TRasterImageP &ri, const TRaster32P &image,
                const TPoint &pos, bool invert) {
  TRect rasBounds   = ri->getRaster()->getBounds();
  TRect imageBounds = image->getBounds() + pos;

  if (invert) {
    TRect rect(imageBounds.x0, rasBounds.y0, rasBounds.x1, imageBounds.y0 - 1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));

    rect =
        TRect(imageBounds.x1 + 1, imageBounds.y0, rasBounds.x1, rasBounds.y1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));

    rect =
        TRect(rasBounds.x0, imageBounds.y1 + 1, imageBounds.x1, rasBounds.y1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));

    rect =
        TRect(rasBounds.x0, rasBounds.y0, imageBounds.x0 - 1, imageBounds.y1);
    TRasterImageUtils::eraseRect(
        ri, TRasterImageUtils::convertRasterToWorld(rect, ri));
  }

  TRaster32P workRas;
  if (TRasterGR8P ras = ri->getRaster()) {
    workRas = TRaster32P(imageBounds.getSize());
    TRop::convert(workRas, ras->extract(imageBounds));
  } else
    workRas = ri->getRaster()->extract(imageBounds);

  int y;
  for (y = 0; y < workRas->getLy(); y++) {
    TPixel32 *outPix    = workRas->pixels(y);
    TPixel32 *outEndPix = outPix + workRas->getLx();
    TPixel32 *inPix     = image->pixels(y);
    for (; outPix != outEndPix; outPix++, inPix++) {
      if (outPix->m == 0) continue;
      TPixel32 pix = depremultiply(*outPix);
      pix.m =
          invert ? pix.m * (inPix->m) / 255 : pix.m * (255 - inPix->m) / 255;
      *outPix = premultiply(pix);
    }
  }

  if (TRasterGR8P ras = ri->getRaster()) {
    TRop::addBackground(workRas, TPixel32::White);
    TRop::over(ras, workRas, pos);
  }
}

//----------------------------------------------------------------------------------

class RectFullColorUndo final : public TFullColorRasterUndo {
  TRectD m_modifyArea;
  TStroke *m_stroke;
  std::wstring m_eraseType;
  bool m_invert;

public:
  RectFullColorUndo(TTileSetFullColor *tileSet, const TRectD &modifyArea,
                    TStroke stroke, std::wstring eraseType,
                    TXshSimpleLevel *level, bool invert,
                    const TFrameId &frameId)
      : TFullColorRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_modifyArea(modifyArea)
      , m_eraseType(eraseType)
      , m_invert(invert) {
    m_stroke = new TStroke(stroke);
  }

  ~RectFullColorUndo() { delete m_stroke; }

  void redo() const override {
    TRasterImageP ri = getImage();
    if (!ri) return;

    if (m_eraseType == RECTERASE)
      TRect rect = TRasterImageUtils::eraseRect(ri, m_modifyArea);
    else if (m_eraseType == FREEHANDERASE || m_eraseType == POLYLINEERASE) {
      TPoint pos;

      TRaster32P image =
          convertStrokeToImage(m_stroke, ri->getRaster()->getBounds(), pos);
      if (!image) return;

      eraseImage(ri, image, pos, m_invert);
    }

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return TFullColorRasterUndo::getSize() +
           m_stroke->getControlPointCount() * sizeof(TThickPoint) + 100 +
           sizeof(this);
  }

  QString getToolName() override {
    return QString("Raster Eraser Tool (Rect)");
  }

  int getHistoryType() override { return HistoryType::EraserTool; }
};

//----------------------------------------------------------------------------------

class FullColorEraserUndo final : public TFullColorRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_size;
  double m_hardness;
  double m_opacity;
  TPointD m_dpiScale;
  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;

public:
  FullColorEraserUndo(TTileSetFullColor *tileSet,
                      const std::vector<TThickPoint> &points,
                      TXshSimpleLevel *level, const TFrameId &frameId, int size,
                      double hardness, double opacity, TPointD dpiScale,
                      double symmetryLines, double rotation,
                      TPointD centerPoint, bool useLineSymmetry)
      : TFullColorRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_points(points)
      , m_size(size)
      , m_hardness(hardness)
      , m_opacity(opacity)
      , m_dpiScale(dpiScale)
      , m_brushCount(symmetryLines)
      , m_rotation(rotation)
      , m_centerPoint(centerPoint)
      , m_useLineSymmetry(useLineSymmetry) {}

  void redo() const override {
    if (m_points.size() == 0) return;
    TRasterImageP image      = getImage();
    TRasterP ras             = image->getRaster();
    QRadialGradient brushPad = getBrushPad(m_size, m_hardness);
    TRaster32P workRaster    = TRaster32P(ras->getSize());
    TRasterP backUpRas       = ras->clone();
    workRaster->clear();

    RasterBlurredBrush brush(workRaster, m_size, brushPad, false);

    if (m_brushCount > 1)
      brush.addSymmetryBrushes(m_brushCount, m_rotation, m_centerPoint,
                               m_useLineSymmetry, m_dpiScale);

    std::vector<TThickPoint> points;
    points.push_back(m_points[0]);
    TRect bbox = brush.getBoundFromPoints(points);
    brush.addPoint(m_points[0], 1);
    brush.eraseDrawing(ras, backUpRas, bbox, m_opacity);
    if (m_points.size() > 1) {
      points.clear();
      points.push_back(m_points[0]);
      points.push_back(m_points[1]);
      bbox = brush.getBoundFromPoints(points);
      brush.addArc(m_points[0], (m_points[1] + m_points[0]) * 0.5, m_points[1],
                   1, 1);
      brush.eraseDrawing(ras, backUpRas, bbox, m_opacity);
      int i;
      for (i = 1; i + 2 < (int)m_points.size(); i = i + 2) {
        points.clear();
        points.push_back(m_points[i]);
        points.push_back(m_points[i + 1]);
        points.push_back(m_points[i + 2]);
        bbox = brush.getBoundFromPoints(points);
        brush.addArc(m_points[i], m_points[i + 1], m_points[i + 2], 1, 1);
        brush.eraseDrawing(ras, backUpRas, bbox, m_opacity);
      }
    }

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + TFullColorRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Raster Eraser Tool"); }
  int getHistoryType() override { return HistoryType::EraserTool; }
};

//----------------------------------------------------------------------------------

void eraseStroke(const TRasterImageP &ri, TStroke *stroke,
                 std::wstring eraseType, bool invert,
                 const TXshSimpleLevelP &level, const TFrameId &frameId) {
  assert(stroke);
  TPoint pos;
  TRasterP ras = ri->getRaster();

  TRaster32P image = convertStrokeToImage(stroke, ras->getBounds(), pos);
  if (!image) return;

  TRect rasterErasedArea = image->getBounds() + pos;
  TRect area;
  if (!invert)
    area = rasterErasedArea.enlarge(2);
  else
    area = ras->getBounds();

  TTileSetFullColor *tileSet = new TTileSetFullColor(ras->getSize());
  tileSet->add(ras, area);

  TUndoManager::manager()->add(
      new RectFullColorUndo(tileSet, convert(area), *stroke, eraseType,
                            level.getPointer(), invert, frameId));

  eraseImage(ri, image, pos, invert);
}

}  // namespace

//**********************************************************************************
//    FullColorEraserTool  definition
//**********************************************************************************

class FullColorEraserTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(FullColorEraserTool)

public:
  FullColorEraserTool(std::string name);
  ~FullColorEraserTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override;

  void onActivate() override;
  void onDeactivate() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;

  void draw() override;

  int getCursorId() const override { return ToolCursor::PenCursor; }

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  bool onPropertyChanged(std::string propertyName) override;
  void onImageChanged() override;
  void onEnter() override;

  void update(const TRasterImageP &ri, TRectD selArea,
              const TXshSimpleLevelP &level, bool multi = false,
              const TFrameId &frameId = -1);
  void multiUpdate(TFrameId &firstFid, TFrameId &lastFid,
                   const TRectD firstRect, const TRectD lastRect);
  void multiUpdate(int firstFidx, int lastFidx, const TRectD firstRect,
                   const TRectD lastRect);

  void multiAreaEraser(TFrameId &firstFid, TFrameId &lastFid,
                       std::vector<TStroke *> firstStrokes,
                       std::vector<TStroke *> lastStrokes);
  void multiAreaEraser(int firstFidx, int lastFidx,
                       std::vector<TStroke *> firstStrokes,
                       std::vector<TStroke *> lastStrokes);

  void doMultiEraser(const TImageP &img, double t, const TFrameId &fid,
                     const TVectorImageP &firstImage,
                     const TVectorImageP &lastImage);

  void resetMulti();

private:
  TPropertyGroup m_prop;

  TIntPairProperty m_size;
  TBoolProperty m_pressure;
  TDoubleProperty m_opacity;
  TDoubleProperty m_hardness;
  TEnumProperty m_eraseType;
  TBoolProperty m_invertOption;
  TEnumProperty m_multi;

  TXshSimpleLevelP m_level;
  std::pair<int, int> m_currCell;

  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_firstFrameIdx;

  TRaster32P m_workRaster;
  TRasterP m_backUpRas;

  QRadialGradient m_brushPad;

  std::vector<TThickPoint> m_points;
  RasterBlurredBrush *m_brush;

  TTileSetFullColor *m_tileSet;
  TTileSaverFullColor *m_tileSaver;

  VectorBrush m_track;
  SymmetryStroke m_polyline;
  std::vector<TStroke *> m_firstStrokes;

  TRectD m_selectingRect, m_firstRect;

  TPointD m_mousePos, m_brushPos, m_firstPos;
  TPointD m_windowMousePos;
  TMouseEvent m_mouseEvent;
  double m_thick;

  bool m_firstTime, m_selecting, m_firstFrameSelected, m_isXsheetCell;
  bool m_mousePressed = false;

} fullColorEraser(T_Eraser);  // Tools are statically instantiated

//===================================================================================================

FullColorEraserTool::FullColorEraserTool(std::string name)
    : TTool(name)
    , m_size("Size:", 1, 1000, 1, 5, false)
    , m_opacity("Opacity:", 0, 100, 100)
    , m_hardness("Hardness:", 0, 100, 100)
    , m_eraseType("Type:")
    , m_invertOption("Invert", false)
    , m_multi("Frame Range:")
    , m_currCell(-1, -1)
    , m_brush(0)
    , m_tileSet(0)
    , m_tileSaver(0)
    , m_thick(0.5)
    , m_firstTime(true)
    , m_selecting(false)
    , m_firstFrameSelected(false)
    , m_isXsheetCell(false)
    , m_pressure("Pressure", true) {
  bind(TTool::RasterImage);

  m_size.setNonLinearSlider();

  m_prop.bind(m_size);
  m_prop.bind(m_hardness);
  m_prop.bind(m_opacity);
  m_prop.bind(m_eraseType);
  m_prop.bind(m_pressure);
  m_prop.bind(m_invertOption);
  m_prop.bind(m_multi);

  m_eraseType.addValue(NORMALERASE);
  m_eraseType.addValue(RECTERASE);
  m_eraseType.addValue(FREEHANDERASE);
  m_eraseType.addValue(POLYLINEERASE);

  m_multi.addValue(L"Off");
  m_multi.addValue(LINEAR_INTERPOLATION);
  m_multi.addValue(EASE_IN_INTERPOLATION);
  m_multi.addValue(EASE_OUT_INTERPOLATION);
  m_multi.addValue(EASE_IN_OUT_INTERPOLATION);

  m_eraseType.setId("Type");
  m_pressure.setId("PressureSensitivity");
  m_invertOption.setId("Invert");
  m_multi.setId("FrameRange");
}

//---------------------------------------------------------------------------------------------------

FullColorEraserTool::~FullColorEraserTool() { m_firstStrokes.clear(); }

//---------------------------------------------------------------------------------------------------

void FullColorEraserTool::updateTranslation() {
  m_size.setQStringName(tr("Size:"));
  m_pressure.setQStringName(tr("Pressure"));
  m_opacity.setQStringName(tr("Opacity:"));
  m_hardness.setQStringName(tr("Hardness:"));

  m_eraseType.setQStringName(tr("Type:"));
  m_eraseType.setItemUIName(NORMALERASE, tr("Normal"));
  m_eraseType.setItemUIName(RECTERASE, tr("Rectangular"));
  m_eraseType.setItemUIName(FREEHANDERASE, tr("Freehand"));
  m_eraseType.setItemUIName(POLYLINEERASE, tr("Polyline"));

  m_invertOption.setQStringName(tr("Invert"));

  m_multi.setQStringName(tr("Frame Range:"));
  m_multi.setItemUIName(L"Off", tr("Off"));
  m_multi.setItemUIName(LINEAR_INTERPOLATION, tr("Linear"));
  m_multi.setItemUIName(EASE_IN_INTERPOLATION, tr("Ease In"));
  m_multi.setItemUIName(EASE_OUT_INTERPOLATION, tr("Ease Out"));
  m_multi.setItemUIName(EASE_IN_OUT_INTERPOLATION, tr("Ease In/Out"));
}

//---------------------------------------------------------------------------------------------------

void FullColorEraserTool::onActivate() {
  if (m_firstTime) {
    m_firstTime = false;
    m_size.setValue(
        TIntPairProperty::Value(FullcolorEraseMinSize, FullcolorEraseSize));
    m_opacity.setValue(FullcolorEraserOpacity);
    m_hardness.setValue(FullcolorEraseHardness);
    m_eraseType.setValue(::to_wstring(FullcolorEraserType.getValue()));
    m_pressure.setValue((bool)FullcolorEraserPressure);
    m_invertOption.setValue((bool)FullcolorEraserInvert);
    m_multi.setIndex(FullcolorEraserRange);
    m_firstTime = false;
  }

  m_brushPad =
      getBrushPad(m_size.getValue().second, m_hardness.getValue() * 0.01);

  // setting m_level in resetMulti() will take care when the tool is switched
  // via shortcut ( i.e. the case skipping onEnter() )
  resetMulti();
  // clear previous polyline when the tool is activated
  m_polyline.reset();
}

//--------------------------------------------------------------------------------------------------

void FullColorEraserTool::onDeactivate() {
  if (m_mousePressed) leftButtonUp(m_mousePos, m_mouseEvent);
}

//--------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonDown(const TPointD &pos,
                                         const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;
  m_mouseEvent            = e;
  m_mousePressed          = true;
  TRasterImageP ri        = (TRasterImageP)getImage(true);
  if (!ri) return;
  TRectD invalidateRect;
  TRasterP ras = ri->getRaster();

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (m_eraseType.getValue() == NORMALERASE) {
    TDimension dim  = ras->getSize();
    double opacity  = m_opacity.getValue() * 0.01;
    double hardness = m_hardness.getValue() * 0.01;
    m_workRaster    = TRaster32P(dim);
    m_workRaster->clear();
    m_backUpRas = ras->clone();

    int maxThick  = m_size.getValue().second;
    int thickness = (m_pressure.getValue() && e.isTablet())
                        ? computeThickness(e.m_pressure, m_size)
                        : maxThick;
    TPointD rasCenter = ras->getCenterD();
    TThickPoint point(pos + rasCenter, thickness);
    TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
    invalidateRect = TRectD(pos - halfThick, pos + halfThick);

    m_points.clear();
    m_points.push_back(point);

    m_tileSet   = new TTileSetFullColor(ras->getSize());
    m_tileSaver = new TTileSaverFullColor(ras, m_tileSet);
    m_brush     = new RasterBlurredBrush(m_workRaster, maxThick,
                                     m_brushPad, false);
    if (symmetryTool && symmetryTool->isGuideEnabled()) {
      m_brush->addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                  symmObj.getCenterPoint(),
                                  symmObj.isUsingLineSymmetry(), dpiScale);
    }

    std::vector<TThickPoint> points = m_points;
    std::vector<TThickPoint> symmPts;
    symmPts = m_brush->getSymmetryPoints(m_points);
    points.insert(points.end(), symmPts.begin(), symmPts.end());

    invalidateRect += ToolUtils::getBounds(points, maxThick) - rasCenter;

    TRect bbox = m_brush->getBoundFromPoints(m_points);
    m_tileSaver->save(bbox);
    m_brush->addPoint(point, 1);
    m_brush->eraseDrawing(ras, m_backUpRas, bbox, opacity);
  } else if (m_eraseType.getValue() == RECTERASE) {
    if (m_multi.getIndex() && m_firstRect.isEmpty()) {
      invalidateRect = m_selectingRect;
      m_selectingRect.empty();
      invalidate(invalidateRect.enlarge(2));
    }
    m_selecting        = true;
    m_selectingRect.x0 = pos.x;
    m_selectingRect.y0 = pos.y;
    m_selectingRect.x1 = pos.x + 1;
    m_selectingRect.y1 = pos.y + 1;
    invalidateRect     = m_selectingRect;

    if (!m_invertOption.getValue() && symmetryTool &&
        symmetryTool->isGuideEnabled()) {
      // We'll use polyline
      m_polyline.reset();
      m_polyline.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                    symmObj.getCenterPoint(),
                                    symmObj.isUsingLineSymmetry(), dpiScale);
      m_polyline.setRectangle(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                              TPointD(m_selectingRect.x1, m_selectingRect.y1));
    }
  } else if (m_eraseType.getValue() == FREEHANDERASE ||
             m_eraseType.getValue() == POLYLINEERASE) {
    if (m_multi.getIndex() && m_firstStrokes.size() && !m_firstFrameSelected) {
      m_firstStrokes.clear();
      invalidate();
    }

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
          symmetryTool->isGuideEnabled() && !m_polyline.hasSymmetryBrushes()) {
        m_polyline.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                      symmObj.getCenterPoint(),
                                      symmObj.isUsingLineSymmetry(), dpiScale);
      }

      if (m_polyline.empty() || m_polyline.back() != pos)
        m_polyline.push_back(pos);
    }

    int maxThick = 2 * m_thick;
    TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
    invalidateRect = TRectD(pos - halfThick, pos + halfThick);
  }
  invalidate(invalidateRect.enlarge(2));
}

//-------------------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonDrag(const TPointD &pos,
                                         const TMouseEvent &e) {
  if (!m_mousePressed) return;

  m_brushPos = m_mousePos = pos;
  m_mouseEvent            = e;
  double pixelSize2       = getPixelSize() * getPixelSize();

  TRasterImageP ri = (TRasterImageP)getImage(true);
  if (!ri) return;
  if (m_eraseType.getValue() == NORMALERASE) {
    int maxThick  = m_size.getValue().second;
    int thickness = (m_pressure.getValue() && e.isTablet())
                        ? computeThickness(e.m_pressure, m_size)
                        : maxThick;
    TDimension size   = m_workRaster->getSize();
    TPointD rasCenter = ri->getRaster()->getCenterD();
    TThickPoint point(pos + rasCenter, thickness);

    TThickPoint old = m_points.back();
    if (norm2(point - old) < 4) return;

    TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5);
    m_points.push_back(mid);
    m_points.push_back(point);

    int m          = m_points.size();
    double opacity = m_opacity.getValue() * 0.01;

    TThickPoint pa = m_points.front();
    std::vector<TThickPoint> points;
    points.push_back(pa);
    points.push_back(mid);
    TRect bbox;
    TRectD invalidateRect;
    if (m == 3) {
      TThickPoint pa = m_points.front();
      std::vector<TThickPoint> points;
      points.push_back(pa);
      points.push_back(mid);
      invalidateRect = ToolUtils::getBounds(points, maxThick);
      bbox           = m_brush->getBoundFromPoints(points);
      m_tileSaver->save(bbox);
      m_brush->addArc(pa, (mid + pa) * 0.5, mid, 1, 1);
    } else {
      std::vector<TThickPoint> points;
      points.push_back(m_points[m - 4]);
      points.push_back(old);
      points.push_back(mid);
      invalidateRect = ToolUtils::getBounds(points, maxThick);
      bbox           = m_brush->getBoundFromPoints(points);
      m_tileSaver->save(bbox);
      m_brush->addArc(m_points[m - 4], old, mid, 1, 1);
    }

    std::vector<TThickPoint> allPts = points;
    std::vector<TThickPoint> symmPts;
    symmPts = m_brush->getSymmetryPoints(allPts);
    allPts.insert(allPts.end(), symmPts.begin(), symmPts.end());

    invalidateRect += ToolUtils::getBounds(allPts, maxThick);

    m_brush->eraseDrawing(ri->getRaster(), m_backUpRas, bbox, opacity);
    invalidate(invalidateRect.enlarge(2) - rasCenter);
  }
  if (m_eraseType.getValue() == RECTERASE) {
    assert(m_selecting);
    TRectD oldRect = m_selectingRect;
    if (oldRect.x0 > oldRect.x1) std::swap(oldRect.x1, oldRect.x0);
    if (oldRect.y0 > oldRect.y1) std::swap(oldRect.y1, oldRect.y0);
    m_selectingRect.x1 = pos.x;
    m_selectingRect.y1 = pos.y;
    TRectD invalidateRect(m_selectingRect);
    if (invalidateRect.x0 > invalidateRect.x1)
      std::swap(invalidateRect.x1, invalidateRect.x0);
    if (invalidateRect.y0 > invalidateRect.y1)
      std::swap(invalidateRect.y1, invalidateRect.y0);
    invalidateRect += oldRect;

    if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
      m_polyline.clear();
      m_polyline.setRectangle(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                              TPointD(m_selectingRect.x1, m_selectingRect.y1));
      invalidate();
    } else
      invalidate(invalidateRect.enlarge(2));
  }
  if (m_eraseType.getValue() == FREEHANDERASE) {
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
    invalidate(m_track.getModifiedRegion());
  }
}

//---------------------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonUp(const TPointD &pos,
                                       const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;
  TRasterImageP ri        = (TRasterImageP)getImage(true);
  if (!ri) return;
  if (m_eraseType.getValue() == NORMALERASE) {
    if (!m_brush) return;

    int maxThick  = m_size.getValue().second;
    int thickness = (m_pressure.getValue() && e.isTablet())
                        ? computeThickness(e.m_pressure, m_size)
                        : maxThick;

    if (m_points.size() != 1) {
      TPointD rasCenter = ri->getRaster()->getCenterD();
      TThickPoint point(pos + rasCenter, thickness);
      m_points.push_back(point);
      int m = m_points.size();
      std::vector<TThickPoint> points;
      points.push_back(m_points[m - 3]);
      points.push_back(m_points[m - 2]);
      points.push_back(m_points[m - 1]);
      TRect bbox = m_brush->getBoundFromPoints(points);
      m_tileSaver->save(bbox);
      m_brush->addArc(points[0], points[1], points[2], 1, 1);
      double opacity = m_opacity.getValue() * 0.01;
      m_brush->eraseDrawing(ri->getRaster(), m_backUpRas, bbox, opacity);
      TRectD invalidateRect = ToolUtils::getBounds(points, maxThick);
      std::vector<TThickPoint> symmPts = m_brush->getSymmetryPoints(points);
      points.insert(points.end(), symmPts.begin(), symmPts.end());
      invalidateRect += ToolUtils::getBounds(points, m_thick);
      invalidate(invalidateRect.enlarge(2) - rasCenter);
    }

    delete m_brush;
    m_brush = 0;

    m_workRaster->unlock();
    double opacity  = m_opacity.getValue() * 0.01;
    double hardness = m_hardness.getValue() * 0.01;

    m_workRaster = TRaster32P();
    m_backUpRas  = TRaster32P();

    delete m_tileSaver;
    TTool::Application *app   = TTool::getApplication();
    TXshLevel *level          = app->getCurrentLevel()->getLevel();
    TXshSimpleLevelP simLevel = level->getSimpleLevel();
    TFrameId frameId          = getCurrentFid();

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

    TUndoManager::manager()->add(new FullColorEraserUndo(
        m_tileSet, m_points, simLevel.getPointer(), frameId, maxThick,
        m_hardness.getValue() * 0.01, opacity, dpiScale, symmetryLines,
        rotation, centerPoint, useLineSymmetry));
    notifyImageChanged();
  } else if (m_eraseType.getValue() == RECTERASE) {
    m_selecting = false;

    if (m_selectingRect.x0 > m_selectingRect.x1)
      std::swap(m_selectingRect.x1, m_selectingRect.x0);
    if (m_selectingRect.y0 > m_selectingRect.y1)
      std::swap(m_selectingRect.y1, m_selectingRect.y0);

    if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
      // We'll use polyline
      m_polyline.clear();
      m_polyline.setRectangle(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                              TPointD(m_selectingRect.x1, m_selectingRect.y1));
    }

    if (m_multi.getIndex()) {
      TTool::Application *app = TTool::getApplication();
      if (m_firstFrameSelected) {
        TFrameId tmp = getCurrentFid();
        int tmpx     = getFrame();
        if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
          // We'll use polyline
          std::vector<TStroke *> lastStrokes;
          for (int i = 0; i < m_polyline.getBrushCount(); i++)
            lastStrokes.push_back(m_polyline.makeRectangleStroke(i));
          if (m_isXsheetCell)
            multiAreaEraser(m_firstFrameIdx, tmpx, m_firstStrokes, lastStrokes);
          else
            multiAreaEraser(m_firstFrameId, tmp, m_firstStrokes, lastStrokes);
        } else {
          if (m_isXsheetCell)
            multiUpdate(m_firstFrameIdx, tmpx, m_firstRect, m_selectingRect);
          else
            multiUpdate(m_firstFrameId, tmp, m_firstRect, m_selectingRect);
        }
        notifyImageChanged();

        if (e.isShiftPressed()) {
          m_firstRect     = m_selectingRect;
          m_firstFrameId  = getFrameId();
          m_firstFrameIdx = getFrame();
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
      TFrameId frameId          = getCurrentFid();
      if (m_invertOption.getValue()) {
        TUndoManager::manager()->beginBlock();
        TRect rect =
            TRasterImageUtils::convertWorldToRaster(m_selectingRect, ri);
        rect *= ri->getSavebox();
        TDimension dim = ri->getRaster()->getSize();
        TRectD rect01 =
            TRectD(TPointD(0., 0.), TPointD((double)rect.x0, (double)dim.ly));
        if (rect01.getLx() > 0 && rect01.getLy() > 0) {
          rect01 = TRasterImageUtils::convertRasterToWorld(convert(rect01), ri);
          update(ri, rect01, simLevel, false, frameId);
        }
        TRectD rect02 = TRectD(convert(rect.getP01()),
                               TPointD((double)rect.x1, (double)dim.ly));
        if (rect02.getLx() > 0 && rect02.getLy() > 0) {
          rect02 = TRasterImageUtils::convertRasterToWorld(convert(rect02), ri);
          update(ri, rect02, simLevel, false, frameId);
        }
        TRectD rect03 =
            TRectD(TPointD((double)rect.x0, 0.), convert(rect.getP10()));
        if (rect03.getLx() > 0 && rect03.getLy() > 0) {
          rect03 = TRasterImageUtils::convertRasterToWorld(convert(rect03), ri);
          update(ri, rect03, simLevel, false, frameId);
        }
        TRectD rect04 = TRectD(TPointD((double)rect.x1, 0.),
                               TPointD((double)dim.lx, (double)dim.ly));
        if (rect04.getLx() > 0 && rect04.getLy() > 0) {
          rect04 = TRasterImageUtils::convertRasterToWorld(convert(rect04), ri);
          update(ri, rect04, simLevel, false, frameId);
        }
        TUndoManager::manager()->endBlock();
        invalidate();
      } else if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
        TUndoManager::manager()->beginBlock();

        TStroke *stroke = m_polyline.makeRectangleStroke();
        eraseStroke(ri, stroke, POLYLINEERASE, m_invertOption.getValue(),
                    /*m_multi.getIndex(),*/ m_level, frameId);

        for (int i = 1; i < m_polyline.getBrushCount(); i++) {
          TStroke *symmStroke = m_polyline.makeRectangleStroke(i);
          symmStroke->setStyle(stroke->getStyle());
          eraseStroke(ri, symmStroke, POLYLINEERASE, m_invertOption.getValue(),
                      /*m_multi.getIndex(),*/ m_level, frameId);
        }

        TUndoManager::manager()->endBlock();
        invalidate();
      } else {
        update(ri, m_selectingRect, simLevel, false, frameId);
        invalidate(m_selectingRect.enlarge(2));
      }
      m_selectingRect.empty();
      m_polyline.reset();
      TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
      m_selecting = false;
      notifyImageChanged();
    }
  } else if (m_eraseType.getValue() == FREEHANDERASE) {
    if (m_track.isEmpty()) return;
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
    m_track.add(TThickPoint(m_firstPos, m_thick), pixelSize2);
    m_track.filterPoints();
    double error    = (30.0 / 11) * sqrt(pixelSize2);
    TStroke *stroke = m_track.makeStroke(error);

    stroke->setStyle(1);

    TTool::Application *app = TTool::getApplication();
    if (m_multi.getIndex())  // stroke multi
    {
      if (m_firstFrameSelected) {
        TFrameId tmp = getCurrentFid();
        int tmpx     = getFrame();
        if (m_firstStrokes.size() && stroke) {
          std::vector<TStroke *> lastStrokes;
          for (int i = 0; i < m_track.getBrushCount(); i++)
            lastStrokes.push_back(m_track.makeStroke(error, i));
          if (m_isXsheetCell)
            multiAreaEraser(m_firstFrameIdx, tmpx, m_firstStrokes, lastStrokes);
          else
            multiAreaEraser(m_firstFrameId, tmp, m_firstStrokes, lastStrokes);
        }
        notifyImageChanged();
        if (e.isShiftPressed()) {
          m_firstStrokes.clear();
          for (int i = 0; i < m_track.getBrushCount(); i++)
            m_firstStrokes.push_back(m_track.makeStroke(error, i));
          invalidate();
          m_firstFrameId  = getFrameId();
          m_firstFrameIdx = getFrame();
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
      TFrameId frameId = getCurrentFid();

      if (m_track.hasSymmetryBrushes()) TUndoManager::manager()->beginBlock();

      eraseStroke(ri, stroke, m_eraseType.getValue(), m_invertOption.getValue(),
                  /*m_multi.getIndex(),*/ m_level, frameId);

      if (m_track.hasSymmetryBrushes()) {
        std::vector<TStroke *> symmStrokes = m_track.makeSymmetryStrokes(error);
        for (int i = 0; i < symmStrokes.size(); i++) {
          symmStrokes[i]->setStyle(stroke->getStyle());
          eraseStroke(ri, symmStrokes[i], m_eraseType.getValue(),
                      m_invertOption.getValue(),
                      /*m_multi.getIndex(),*/ m_level, frameId);
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
  m_mousePressed = false;
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::leftButtonDoubleClick(const TPointD &pos,
                                                const TMouseEvent &e) {
  TRasterImageP ri(getImage(true));
  if (!ri) return;
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

  if (m_multi.getIndex())  // stroke multi
  {
    if (m_firstFrameSelected) {
      TFrameId tmp = getFrameId();
      int tmpx     = getFrame();
      if (m_firstStrokes.size() && stroke) {
        std::vector<TStroke *> lastStrokes;
        for (int i = 0; i < m_polyline.getBrushCount(); i++)
          lastStrokes.push_back(m_polyline.makePolylineStroke(i));
        if (m_isXsheetCell)
          multiAreaEraser(m_firstFrameIdx, tmpx, m_firstStrokes, lastStrokes);
        else
          multiAreaEraser(m_firstFrameId, tmp, m_firstStrokes, lastStrokes);
      }
      if (e.isShiftPressed()) {
        m_firstStrokes.clear();
        for (int i = 0; i < m_polyline.getBrushCount(); i++)
          m_firstStrokes.push_back(m_polyline.makePolylineStroke(i));
        invalidate();
        m_firstFrameId  = getFrameId();
        m_firstFrameIdx = getFrame();
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
      for (int i = 0; i < m_polyline.getBrushCount(); i++)
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

    if (m_polyline.hasSymmetryBrushes()) TUndoManager::manager()->beginBlock();

    eraseStroke(ri, stroke, m_eraseType.getValue(), m_invertOption.getValue(),
                /*m_multi.getIndex(),*/ m_level, frameId);

    if (m_polyline.hasSymmetryBrushes()) {
      for (int i = 1; i < m_polyline.getBrushCount(); i++) {
        TStroke *symmStroke = m_polyline.makePolylineStroke(i);
        symmStroke->setStyle(stroke->getStyle());
        eraseStroke(ri, symmStroke, m_eraseType.getValue(),
                    m_invertOption.getValue(),
                    /*m_multi.getIndex(),*/ m_level, frameId);
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

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  struct Locals {
    FullColorEraserTool *m_this;

    void setValue(TIntPairProperty &prop,
                  const TIntPairProperty::Value &value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addMinMax(TIntPairProperty &prop, double add) {
      const TIntPairProperty::Range &range = prop.getRange();

      TIntPairProperty::Value value = prop.getValue();
      value.second =
          tcrop<double>(value.second + add, range.first, range.second);
      value.first = tcrop<double>(value.first + add, range.first, range.second);

      setValue(prop, value);
    }

    void addMinMaxSeparate(TIntPairProperty &prop, double min, double max) {
      if (min == 0.0 && max == 0.0) return;
      const TIntPairProperty::Range &range = prop.getRange();

      TIntPairProperty::Value value = prop.getValue();
      value.first += min;
      value.second += max;
      if (value.first > value.second) value.first = value.second;
      value.first  = tcrop<double>(value.first, range.first, range.second);
      value.second = tcrop<double>(value.second, range.first, range.second);

      setValue(prop, value);
    }

  } locals = {this};

  if (m_eraseType.getValue() == NORMALERASE && e.isCtrlPressed() && e.isAltPressed() && !e.isShiftPressed()) {
    // User wants to alter the maximum brush size
    const TPointD &diff = m_windowMousePos - -e.m_pos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMaxSeparate(m_size, min, max);
  } else { 
    m_brushPos = pos;
  }

  m_mousePos = pos;
  m_windowMousePos = -e.m_pos;
  invalidate();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::draw() {
  int devPixRatio = m_viewer->getDevPixRatio();

  double pixelSize2 = getPixelSize() * getPixelSize();
  m_thick           = sqrt(pixelSize2) / 2.0;
  TRasterImageP img = (TRasterImageP)getImage(false);
  if (!img) return;

  glLineWidth(1.0 * devPixRatio);
  
//  TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
//                     ? TPixel32::White
//                     : TPixel32::Black;
  TPixel color = TPixel32::Red;
  if (m_eraseType.getValue() == NORMALERASE) {
    // If toggled off, don't draw brush outline
    if (!Preferences::instance()->isCursorOutlineEnabled()) return;

    glColor3d(1.0, 0.0, 0.0);
    tglDrawCircle(m_brushPos, (m_size.getValue().first + 1) * 0.5);
    tglDrawCircle(m_brushPos, (m_size.getValue().second + 1) * 0.5);
  } else if (m_eraseType.getValue() == RECTERASE) {
    if (m_multi.getIndex() && m_firstFrameSelected) {
      if (m_firstStrokes.size()) {
        tglColor(color);
        for (int i = 0; i < m_firstStrokes.size(); i++)
          drawStrokeCenterline(*m_firstStrokes[i], 1);
      } else
        drawRect(m_firstRect, color, 0x3F33, true);
    }

    if (m_selecting || (m_multi.getIndex() && !m_firstFrameSelected)) {
      if (m_polyline.size() > 1) {
        glPushMatrix();
        m_polyline.drawRectangle(color);
        glPopMatrix();
      } else
        drawRect(m_selectingRect, color, 0xFFFF, true);
    }
  }
  if ((m_eraseType.getValue() == FREEHANDERASE ||
       m_eraseType.getValue() == POLYLINEERASE) &&
      m_multi.getIndex()) {
    tglColor(color);
    for (int i = 0; i < m_firstStrokes.size(); i++)
      drawStrokeCenterline(*m_firstStrokes[i], 1);
  }
  if (m_eraseType.getValue() == POLYLINEERASE && !m_polyline.empty()) {
    m_polyline.drawPolyline(m_mousePos, color);
  } else if (m_eraseType.getValue() == FREEHANDERASE && !m_track.isEmpty()) {
    tglColor(color);
    glPushMatrix();
    m_track.drawAllFragments();
    glPopMatrix();
  }
}

//----------------------------------------------------------------------------------------------------------

bool FullColorEraserTool::onPropertyChanged(std::string propertyName) {
  FullcolorEraseMinSize        = m_size.getValue().first;
  FullcolorEraseSize           = m_size.getValue().second;
  FullcolorEraseHardness       = m_hardness.getValue();
  FullcolorEraserOpacity       = m_opacity.getValue();
  FullcolorEraserType          = ::to_string(m_eraseType.getValue());
  FullcolorEraserPressure      = (int)m_pressure.getValue();
  FullcolorEraserInvert        = (int)m_invertOption.getValue();
  FullcolorEraserRange         = m_multi.getIndex();
  if (propertyName == "Hardness:" || propertyName == "Size:") {
    m_brushPad =
        getBrushPad(m_size.getValue().second, m_hardness.getValue() * 0.01);
    TRectD rect(
        m_brushPos - TPointD(FullcolorEraseSize + 2, FullcolorEraseSize + 2),
        m_brushPos + TPointD(FullcolorEraseSize + 2, FullcolorEraseSize + 2));
    invalidate(rect);
  }

  return true;
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::update(const TRasterImageP &ri, TRectD selArea,
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

  TRasterP raster            = ri->getRaster();
  TTileSetFullColor *tileSet = new TTileSetFullColor(raster->getSize());
  tileSet->add(raster, TRasterImageUtils::convertWorldToRaster(selArea, ri));
  TUndo *undo;

  std::wstring eraseType =
      (m_eraseType.getValue() == RECTERASE && m_polyline.hasSymmetryBrushes())
          ? POLYLINEERASE
          : m_eraseType.getValue();
  undo = new RectFullColorUndo(tileSet, selArea, TStroke(), eraseType,
                               level.getPointer(),
                               m_invertOption.getValue(), frameId);
  TRect rect = TRasterImageUtils::eraseRect(ri, selArea);

  TUndoManager::manager()->add(undo);
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::resetMulti() {
  m_isXsheetCell       = false;
  m_firstFrameSelected = false;
  m_firstRect.empty();
  m_selectingRect.empty();
  TTool::Application *app = TTool::getApplication();
  m_level                 = app->getCurrentLevel()->getLevel()
                ? app->getCurrentLevel()->getSimpleLevel()
                : 0;
  m_firstFrameId = m_veryFirstFrameId = getCurrentFid();
  m_firstFrameIdx                     = getFrame();
  m_firstStrokes.clear();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::onImageChanged() {
  if (!m_multi.getIndex()) return;
  TTool::Application *app = TTool::getApplication();
  TXshSimpleLevel *xshl   = 0;
  if (app->getCurrentLevel()->getLevel())
    xshl = app->getCurrentLevel()->getSimpleLevel();

  if (!xshl || m_level.getPointer() != xshl ||
      (m_selectingRect.isEmpty() && !m_firstStrokes.size()))
    resetMulti();
  else if ((!m_isXsheetCell && m_firstFrameId == getCurrentFid()) ||
           (m_isXsheetCell && m_firstFrameIdx == getFrame()))
    m_firstFrameSelected = false;  // nel caso sono passato allo stato 1 e torno
                                   // all'immagine iniziale, torno allo stato
                                   // iniziale
  else {                           // cambio stato.
    m_firstFrameSelected = true;
    if (m_eraseType.getValue() != FREEHANDERASE &&
        m_eraseType.getValue() != POLYLINEERASE) {
      assert(!m_selectingRect.isEmpty());
      m_firstRect = m_selectingRect;
    }
  }
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::multiUpdate(TFrameId &firstFid, TFrameId &lastFid,
                                      const TRectD firstRect,
                                      const TRectD lastRect) {
  bool backward     = false;
  if (firstFid > lastFid) {
    std::swap(firstFid, lastFid);
    backward = true;
  }
  assert(firstFid <= lastFid);
  std::vector<TFrameId> allFids;
  m_level->getFids(allFids);

  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFid) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFid) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
  if (m_multi.getValue() == EASE_IN_INTERPOLATION) {
    algorithm = TInbetween::EaseInInterpolation;
  } else if (m_multi.getValue() == EASE_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseOutInterpolation;
  } else if (m_multi.getValue() == EASE_IN_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseInOutInterpolation;
  }

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TRasterImageP ri = m_level->getFrame(fid, true);
    assert(ri);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t           = TInbetween::interpolation(t, algorithm);
    TRectD rect = interpolateRect(firstRect, lastRect, backward ? 1 - t : t);
    if (m_invertOption.getValue()) {
      TRectD rect01 =
          TRectD(TPointD(-100000., -100000.), TPointD(rect.x0, 100000.));
      update(ri, rect01, m_level, true, fid);
      TRectD rect02 = TRectD(rect.getP01(), TPointD(rect.x1, 100000.));
      update(ri, rect02, m_level, true, fid);
      TRectD rect03 = TRectD(TPointD(rect.x0, -100000.), rect.getP10());
      update(ri, rect03, m_level, true, fid);
      TRectD rect04 =
          TRectD(TPointD(rect.x1, -100000.), TPointD(100000., 100000.));
      update(ri, rect04, m_level, true, fid);
    } else
      update(ri, rect, m_level, true, fid);
    m_level->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::multiUpdate(int firstFidx, int lastFidx,
                                      const TRectD firstRect,
                                      const TRectD lastRect) {
  bool backward = false;
  if (firstFidx > lastFidx) {
    std::swap(firstFidx, lastFidx);
    backward = true;
  }
  assert(firstFidx <= lastFidx);

  TTool::Application *app = TTool::getApplication();
  TFrameId lastFrameId;
  int col = app->getCurrentColumn()->getColumnIndex();
  int row;

  std::vector<std::pair<int, TXshCell>> cellList;

  for (row = firstFidx; row <= lastFidx; row++) {
    TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(row, col);
    if (cell.isEmpty()) continue;
    TFrameId fid = cell.getFrameId();
    if (lastFrameId == fid) continue;  // Skip held cells
    cellList.push_back(std::pair<int, TXshCell>(row, cell));
    lastFrameId = fid;
  }

  int m = cellList.size();

  enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
  if (m_multi.getValue() == EASE_IN_INTERPOLATION) {
    algorithm = TInbetween::EaseInInterpolation;
  } else if (m_multi.getValue() == EASE_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseOutInterpolation;
  } else if (m_multi.getValue() == EASE_IN_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseInOutInterpolation;
  }

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    row           = cellList[i].first;
    TXshCell cell = cellList[i].second;
    TFrameId fid  = cell.getFrameId();
    TRasterImageP ri = (TRasterImageP)cell.getImage(true);
    if (!ri) continue;
    assert(ri);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t           = TInbetween::interpolation(t, algorithm);
    TRectD rect = interpolateRect(firstRect, lastRect, backward ? 1 - t : t);
    if (m_invertOption.getValue()) {
      TRectD rect01 =
          TRectD(TPointD(-100000., -100000.), TPointD(rect.x0, 100000.));
      update(ri, rect01, m_level, true, fid);
      TRectD rect02 = TRectD(rect.getP01(), TPointD(rect.x1, 100000.));
      update(ri, rect02, m_level, true, fid);
      TRectD rect03 = TRectD(TPointD(rect.x0, -100000.), rect.getP10());
      update(ri, rect03, m_level, true, fid);
      TRectD rect04 =
          TRectD(TPointD(rect.x1, -100000.), TPointD(100000., 100000.));
      update(ri, rect04, m_level, true, fid);
    } else
      update(ri, rect, m_level, true, fid);
    m_level->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::onEnter() {
  TRasterImageP ti(getImage(false));
  if (!ti) return;

  TTool::Application *app = TTool::getApplication();
  m_level                 = app->getCurrentLevel()->getLevel()
                ? app->getCurrentLevel()->getSimpleLevel()
                : 0;
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::multiAreaEraser(TFrameId &firstFid, TFrameId &lastFid,
                                          std::vector<TStroke *>firstStrokes,
                                          std::vector<TStroke *>lastStrokes) {
//  TStroke *first           = new TStroke();
//  TStroke *last            = new TStroke();
//  *first                   = *firstStrokes[0];
//  *last                    = *lastStrokes[0];
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
  m_level->getFids(allFids);

  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFid) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFid) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
  if (m_multi.getValue() == EASE_IN_INTERPOLATION) {
    algorithm = TInbetween::EaseInInterpolation;
  } else if (m_multi.getValue() == EASE_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseOutInterpolation;
  } else if (m_multi.getValue() == EASE_IN_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseInOutInterpolation;
  }

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TImageP img = m_level->getFrame(fid, true);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t           = TInbetween::interpolation(t, algorithm);
    doMultiEraser(img, backward ? 1 - t : t, fid, firstImage, lastImage);
    m_level->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::multiAreaEraser(int firstFidx, int lastFidx,
                                          std::vector<TStroke *> firstStrokes,
                                          std::vector<TStroke *> lastStrokes) {
  TVectorImageP firstImage = new TVectorImage();
  TVectorImageP lastImage  = new TVectorImage();
  for (int i = 0; i < firstStrokes.size(); i++)
    firstImage->addStroke(firstStrokes[i]);
  for (int i = 0; i < lastStrokes.size(); i++)
    lastImage->addStroke(lastStrokes[i]);

  bool backward = false;
  if (firstFidx > lastFidx) {
    std::swap(firstFidx, lastFidx);
    backward = true;
  }
  assert(firstFidx <= lastFidx);

  TTool::Application *app = TTool::getApplication();
  TFrameId lastFrameId;
  int col = app->getCurrentColumn()->getColumnIndex();
  int row;

  std::vector<std::pair<int, TXshCell>> cellList;

  for (row = firstFidx; row <= lastFidx; row++) {
    TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(row, col);
    if (cell.isEmpty()) continue;
    TFrameId fid = cell.getFrameId();
    if (lastFrameId == fid) continue;  // Skip held cells
    cellList.push_back(std::pair<int, TXshCell>(row, cell));
    lastFrameId = fid;
  }

  int m = cellList.size();

  enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
  if (m_multi.getValue() == EASE_IN_INTERPOLATION) {
    algorithm = TInbetween::EaseInInterpolation;
  } else if (m_multi.getValue() == EASE_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseOutInterpolation;
  } else if (m_multi.getValue() == EASE_IN_OUT_INTERPOLATION) {
    algorithm = TInbetween::EaseInOutInterpolation;
  }

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    row               = cellList[i].first;
    TXshCell cell     = cellList[i].second;
    TFrameId fid      = cell.getFrameId();
    TImageP img       = (TImageP)cell.getImage(true);
    if (!img) continue;
    double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t        = TInbetween::interpolation(t, algorithm);
    doMultiEraser(img, backward ? 1 - t : t, fid, firstImage, lastImage);
    m_level->getProperties()->setDirtyFlag(true);
    notifyImageChanged(fid);
  }
  TUndoManager::manager()->endBlock();
}

//----------------------------------------------------------------------------------------------------------

void FullColorEraserTool::doMultiEraser(const TImageP &img, double t,
                                        const TFrameId &fid,
                                        const TVectorImageP &firstImage,
                                        const TVectorImageP &lastImage) {
  int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
  std::wstring eraseType = m_eraseType.getValue() == RECTERASE
                               ? POLYLINEERASE
                               : m_eraseType.getValue();
  if (t == 0)
    for (int i = 0; i < firstImage->getStrokeCount(); i++)
      eraseStroke(img, firstImage->getStroke(i), eraseType,
                  m_invertOption.getValue(), /*m_multi.getIndex(),*/ m_level,
                  fid);
  else if (t == 1)
    for (int i = 0; i < lastImage->getStrokeCount(); i++)
      eraseStroke(img, lastImage->getStroke(i), eraseType,
                  m_invertOption.getValue(), /*m_multi.getIndex(),*/ m_level,
                  fid);
  else {
//    assert(firstImage->getStrokeCount() == 1);
//    assert(lastImage->getStrokeCount() == 1);
    TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
//    assert(vi->getStrokeCount() == 1);
    for (int i = 0; i < vi->getStrokeCount(); i++)
      eraseStroke(img, vi->getStroke(i), eraseType, m_invertOption.getValue(),
                  /*m_multi.getIndex(),*/ m_level, fid);
  }
}
