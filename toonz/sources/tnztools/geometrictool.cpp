
#include "geometrictool.h"

#include "toonz/tpalettehandle.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "tools/tool.h"
#include "tcolorstyles.h"
#include "tpalette.h"
#include "tools/toolutils.h"
#include "tstroke.h"
#include "tmathutil.h"
#include "tools/cursors.h"
#include "tproperty.h"
#include "ttoonzimage.h"
#include "drawutil.h"
#include "tvectorimage.h"
#include "tenv.h"
#include "bluredbrush.h"
#include "symmetrystroke.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/toonzimageutils.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectspline.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/dvdialog.h"
#include "toonz/trasterimageutils.h"
#include "toonz/preferences.h"
#include "tpixelutils.h"
#include "historytypes.h"
#include "toonzvectorbrushtool.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tstageobjectcmd.h"

#include "toonz/mypaintbrushstyle.h"

#include "tinbetween.h"

// For Qt translation support
#include <QCoreApplication>

using namespace ToolUtils;

class GeometricTool;
class MultiLinePrimitive;
class MultiArcPrimitive;

TEnv::DoubleVar GeometricSize("InknpaintGeometricSize", 1);
TEnv::DoubleVar GeometricRasterSize("InknpaintGeometricRasterSize", 1);
TEnv::StringVar GeometricType("InknpaintGeometricType", "Rectangle");
TEnv::IntVar GeometricEdgeCount("InknpaintGeometricEdgeCount", 3);
TEnv::IntVar GeometricSelective("InknpaintGeometricSelective", 0);
TEnv::IntVar GeometricRotate("InknpaintGeometricRotate", 0);
TEnv::IntVar GeometricGroupIt("InknpaintGeometricGroupIt", 0);
TEnv::IntVar GeometricAutofill("InknpaintGeometricAutofill", 0);
TEnv::IntVar GeometricSmooth("InknpaintGeometricSmooth", 0);
TEnv::IntVar GeometricPencil("InknpaintGeometricPencil", 0);
TEnv::DoubleVar GeometricBrushHardness("InknpaintGeometricHardness", 100);
TEnv::DoubleVar GeometricOpacity("InknpaintGeometricOpacity", 100);
TEnv::IntVar GeometricCapStyle("InknpaintGeometricCapStyle", 0);
TEnv::IntVar GeometricJoinStyle("InknpaintGeometricJoinStyle", 0);
TEnv::IntVar GeometricMiterValue("InknpaintGeometricMiterValue", 4);
TEnv::IntVar GeometricSnap("InknpaintGeometricSnap", 0);
TEnv::IntVar GeometricSnapSensitivity("InknpaintGeometricSnapSensitivity", 0);
TEnv::IntVar GeometricDrawBehind("InknpaintGeometricDrawBehind", 0);
TEnv::DoubleVar GeometricModifierSize("InknpaintGeometricModifierSize", 0);
TEnv::IntVar GeometricModifierEraser("InknpaintGeometricModifierEraser", 0);
TEnv::IntVar GeometricModifierJoinStyle("InknpaintGeometricModifierJoinStyle",
                                        0);
TEnv::DoubleVar GeometricModifierPressure("InknpaintGeometricModifierPressure",
                                          0.5);
TEnv::IntVar GeometricRange("InknpaintGeometricRange", 0);

//-------------------------------------------------------------------

#define ROUNDC_WSTR L"round_cap"
#define BUTT_WSTR L"butt_cap"
#define PROJECTING_WSTR L"projecting_cap"
#define ROUNDJ_WSTR L"round_join"
#define BEVEL_WSTR L"bevel_join"
#define MITER_WSTR L"miter_join"
#define LOW_WSTR L"Low"
#define MEDIUM_WSTR L"Medium"
#define HIGH_WSTR L"High"

#define LINEAR_INTERPOLATION L"Linear"
#define EASE_IN_INTERPOLATION L"Ease In"
#define EASE_OUT_INTERPOLATION L"Ease Out"
#define EASE_IN_OUT_INTERPOLATION L"Ease In/Out"

const double SNAPPING_LOW    = 5.0;
const double SNAPPING_MEDIUM = 25.0;
const double SNAPPING_HIGH   = 100.0;

//=============================================================================
// Utility Functions
//-----------------------------------------------------------------------------

static TPointD rectify(const TPointD &oldPos, const TPointD &pos) {
  const double h             = sqrt(2.0) / 2.0;
  const TPointD directions[] = {TPointD(1, 0),  TPointD(h, h),  TPointD(0, 1),
                                TPointD(-h, h), TPointD(-1, 0), TPointD(-h, -h),
                                TPointD(0, -1), TPointD(h, -h)};
  TPointD v        = pos - oldPos;
  int j            = 0;
  double bestValue = v * directions[j];
  for (int k = 1; k < 8; k++) {
    double value = v * directions[k];
    if (value > bestValue) {
      bestValue = value;
      j         = k;
    }
  }
  return oldPos + bestValue * directions[j];
}

//-----------------------------------------------------------------------------

static TPointD computeSpeed(TPointD p0, TPointD p1, double factor) {
  TPointD d = p1 - p0;
  return (d == TPointD()) ? TPointD() : d * (factor / norm(d));
}

//-----------------------------------------------------------------------------

static TRect drawBluredBrush(const TRasterImageP &ri, TStroke *stroke,
                             int thick, double hardness, double opacity) {
  TStroke *s       = new TStroke(*stroke);
  TPointD riCenter = ri->getRaster()->getCenterD();
  s->transform(TTranslation(riCenter));
  int styleId    = s->getStyle();
  TPalette *plt  = ri->getPalette();
  TPixel32 color = plt->getStyle(styleId)->getMainColor();

  TRectD bbox      = s->getBBox();
  TRect bboxI      = convert(bbox).enlarge(1);
  TRect rectRender = bboxI * ri->getRaster()->getBounds();
  if (rectRender.isEmpty()) return TRect();

  TRaster32P workRas(ri->getRaster()->getSize());
  TRaster32P backupRas = ri->getRaster()->clone();
  workRas->clear();
  QRadialGradient brushPad = ToolUtils::getBrushPad(thick, hardness);
  BluredBrush brush(workRas, thick, brushPad, false);

  int i, chunkCount = s->getChunkCount();
  for (i = 0; i < chunkCount; i++) {
    const TThickQuadratic *q = s->getChunk(i);
    std::vector<TThickPoint> points;
    points.push_back(q->getThickP0());
    points.push_back(q->getThickP1());
    points.push_back(q->getThickP2());
    // i punti contenuti nello stroke sembra che haano la tickness pari al
    // raggio e non al diametro del punto!
    points[0].thick *= 2;
    points[1].thick *= 2;
    points[2].thick *= 2;
    TRect chunkBox = brush.getBoundFromPoints(points);
    brush.addArc(points[0], points[1], points[2], 1, 1);
    brush.updateDrawing(ri->getRaster(), backupRas, color, chunkBox, opacity);
  }

  delete s;
  return rectRender;
}

//-----------------------------------------------------------------------------

static TRect drawBluredBrush(const TToonzImageP &ti, TStroke *stroke, int thick,
                             double hardness, bool selective) {
  TStroke *s       = new TStroke(*stroke);
  TPointD riCenter = ti->getRaster()->getCenterD();
  s->transform(TTranslation(riCenter));
  int styleId = s->getStyle();

  TRectD bbox      = s->getBBox();
  TRect bboxI      = convert(bbox).enlarge(1);
  TRect rectRender = bboxI * ti->getRaster()->getBounds();
  if (rectRender.isEmpty()) return TRect();

  TRaster32P workRas(ti->getRaster()->getSize());
  TRasterCM32P backupRas = ti->getRaster()->clone();
  workRas->clear();
  QRadialGradient brushPad = ToolUtils::getBrushPad(thick, hardness);
  BluredBrush brush(workRas, thick, brushPad, false);

  int i, chunkCount = s->getChunkCount();
  for (i = 0; i < chunkCount; i++) {
    const TThickQuadratic *q = s->getChunk(i);
    std::vector<TThickPoint> points;
    points.push_back(q->getThickP0());
    points.push_back(q->getThickP1());
    points.push_back(q->getThickP2());
    // i punti contenuti nello stroke sembra che haano la tickness pari al
    // raggio e non al diametro del punto!
    points[0].thick *= 2;
    points[1].thick *= 2;
    points[2].thick *= 2;
    TRect chunkBox = brush.getBoundFromPoints(points);
    brush.addArc(points[0], points[1], points[2], 1, 1);
    brush.updateDrawing(ti->getRaster(), backupRas, chunkBox, styleId,
                        selective, false);
  }

  delete s;
  return rectRender;
}

//=========================================================================================================

class UndoMyPaintBrushCM32 final : public TRasterUndo {
  TPoint m_offset;
  QString m_id;
  std::string m_primitiveName;

public:
  UndoMyPaintBrushCM32(TTileSetCM32 *tileSet, TXshSimpleLevel *level,
                       const TFrameId &frameId, bool isFrameCreated,
                       bool isLevelCreated, const TRasterCM32P &ras,
                       const TPoint &offset, std::string primitiveName)
      : TRasterUndo(tileSet, level, frameId, isFrameCreated, isLevelCreated, 0)
      , m_offset(offset)
      , m_primitiveName(primitiveName) {
    static int counter = 0;
    m_id = QString("UndoMyPaintBrushCM32") + QString::number(counter++);
    TImageCache::instance()->add(m_id.toStdString(),
                                 TToonzImageP(ras, TRect(ras->getSize())));
  }

  ~UndoMyPaintBrushCM32() { TImageCache::instance()->remove(m_id); }

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

  QString getToolName() override {
    return QString("Geometric Tool : %1")
        .arg(QString::fromStdString(m_primitiveName));
  }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//-----------------------------------------------------------------------------

class UndoMyPaintBrushFullColor final : public ToolUtils::TFullColorRasterUndo {
  TPoint m_offset;
  QString m_id;
  std::string m_primitiveName;

public:
  UndoMyPaintBrushFullColor(TTileSetFullColor *tileSet, TXshSimpleLevel *level,
                            const TFrameId &frameId, bool isFrameCreated,
                            bool isLevelCreated, const TRasterP &ras,
                            const TPoint &offset, std::string primitiveName)
      : ToolUtils::TFullColorRasterUndo(tileSet, level, frameId, isFrameCreated,
                                        isLevelCreated, 0)
      , m_offset(offset)
      , m_primitiveName(primitiveName) {
    static int counter = 0;
    m_id = QString("UndoMyPaintBrushFullColor") + QString::number(counter++);
    TImageCache::instance()->add(m_id.toStdString(), TRasterImageP(ras));
  }

  ~UndoMyPaintBrushFullColor() { TImageCache::instance()->remove(m_id); }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TRasterImageP image = getImage();
    TRasterP ras        = image->getRaster();

    TRasterImageP tSrcImg =
        TImageCache::instance()->get(m_id.toStdString(), false);
    ras->copy(tSrcImg->getRaster(), m_offset);
    ToolUtils::updateSaveBox();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + ToolUtils::TFullColorRasterUndo::getSize();
  }

  QString getToolName() override {
    return QString("Geometric Tool : %1")
        .arg(QString::fromStdString(m_primitiveName));
  }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

//-----------------------------------------------------------------------------

class MultilinePrimitiveUndo final : public TUndo {
  SymmetryStroke m_oldVertex;
  SymmetryStroke m_newVertex;
  MultiLinePrimitive *m_tool;

public:
  MultilinePrimitiveUndo(const SymmetryStroke &vertex, MultiLinePrimitive *tool)
      : TUndo(), m_oldVertex(vertex), m_tool(tool), m_newVertex() {}

  ~MultilinePrimitiveUndo() {}

  void undo() const override;
  void redo() const override;
  void setNewVertex(const SymmetryStroke &vertex) {
    m_newVertex = vertex;
  }

  int getSize() const override { return sizeof(this); }

  QString getToolName();
  int getHistoryType() override { return HistoryType::GeometricTool; }
};

//-----------------------------------------------------------------------------

class MultiArcPrimitiveUndoData {
private:
  TStroke *m_stroke;
  TStroke *m_strokeTemp;
  TPointD m_startPoint, m_endPoint, m_centralPoint;
  int m_clickNumber;

public:
  MultiArcPrimitiveUndoData(TStroke *stroke, TStroke *strokeTemp,
                            TPointD startPoint, TPointD endPoint,
                            TPointD centralPoint, int clickNumber)
      : m_stroke(0)
      , m_strokeTemp(0)
      , m_startPoint(startPoint)
      , m_endPoint(endPoint)
      , m_centralPoint(centralPoint)
      , m_clickNumber(clickNumber) {
    if (stroke) {
      m_stroke = new TStroke(*stroke);
    }
    if (strokeTemp) {
      m_strokeTemp = new TStroke(*strokeTemp);
    }
  }

  ~MultiArcPrimitiveUndoData() {
    delete m_stroke;
    delete m_strokeTemp;
  }

  void replace(MultiArcPrimitive *tool) const;
};

//-----------------------------------------------------------------------------

class MultiArcPrimitiveUndo final : public TUndo {
  MultiArcPrimitive *m_tool;
  MultiArcPrimitiveUndoData m_undo;
  MultiArcPrimitiveUndoData *m_redo;

public:
  MultiArcPrimitiveUndo(MultiArcPrimitive *tool, TStroke *stroke,
                        TStroke *strokeTemp, TPointD startPoint,
                        TPointD endPoint, TPointD centralPoint, int clickNumber)
      : TUndo()
      , m_tool(tool)
      , m_undo(stroke, strokeTemp, startPoint, endPoint, centralPoint,
               clickNumber)
      , m_redo(0) {}

  ~MultiArcPrimitiveUndo() { delete m_redo; }

  void setRedoData(TStroke *stroke, TStroke *strokeTemp, TPointD startPoint,
                   TPointD endPoint, TPointD centralPoint, int clickNumber) {
    m_redo = new MultiArcPrimitiveUndoData(stroke, strokeTemp, startPoint,
                                           endPoint, centralPoint, clickNumber);
  }

  void undo() const override;
  void redo() const override;

  int getSize() const override { return sizeof(this); }

  QString getToolName();
  int getHistoryType() override { return HistoryType::GeometricTool; }
};

//-----------------------------------------------------------------------------

class FullColorBluredPrimitiveUndo final : public UndoFullColorPencil {
  int m_thickness;
  double m_hardness;

public:
  FullColorBluredPrimitiveUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                               TStroke *stroke, int thickness, double hardness,
                               double opacity, bool doAntialias,
                               bool createdFrame, bool createdLevel)
      : UndoFullColorPencil(level, frameId, stroke, opacity, doAntialias,
                            createdFrame, createdLevel)
      , m_thickness(thickness)
      , m_hardness(hardness) {
    TRasterP raster = getImage()->getRaster();
    TDimension d    = raster->getSize();
    m_tiles         = new TTileSetFullColor(d);
    TRect rect      = convert(stroke->getBBox()) +
                 TPoint((int)(d.lx * 0.5), (int)(d.ly * 0.5));
    m_tiles->add(raster, rect.enlarge(2));
    m_stroke = new TStroke(*stroke);
  }

  ~FullColorBluredPrimitiveUndo() {}

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TRasterImageP ri = getImage();
    if (!ri) return;
    drawBluredBrush(ri, m_stroke, m_thickness, m_hardness, m_opacity);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return UndoFullColorPencil::getSize() + sizeof(this);
  }
};

//-----------------------------------------------------------------------------
/*-- Hardness<100 のときの GeometricToolのUndo --*/
class CMBluredPrimitiveUndo final : public UndoRasterPencil {
  int m_thickness;
  double m_hardness;
  bool m_selective;

public:
  CMBluredPrimitiveUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                        TStroke *stroke, int thickness, double hardness,
                        bool selective, bool doAntialias, bool createdFrame,
                        bool createdLevel, std::string primitiveName)
      : UndoRasterPencil(level, frameId, stroke, selective, false, doAntialias,
                         createdFrame, createdLevel, primitiveName)
      , m_thickness(thickness)
      , m_hardness(hardness)
      , m_selective(selective) {
    TRasterCM32P raster = getImage()->getRaster();
    TDimension d        = raster->getSize();
    m_tiles             = new TTileSetCM32(d);
    TRect rect          = convert(stroke->getBBox()) +
                 TPoint((int)(d.lx * 0.5), (int)(d.ly * 0.5));
    m_tiles->add(raster, rect.enlarge(2));
    m_stroke = new TStroke(*stroke);
  }

  ~CMBluredPrimitiveUndo() {}

  void redo() const override {
    insertLevelAndFrameIfNeeded();
    TToonzImageP ti = getImage();
    if (!ti) return;
    drawBluredBrush(ti, m_stroke, m_thickness, m_hardness, m_selective);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return UndoRasterPencil::getSize() + sizeof(this);
  }
};

//-----------------------------------------------------------------------------

class PrimitiveParam {
  Q_DECLARE_TR_FUNCTIONS(PrimitiveParam)

public:
  TDoubleProperty m_toolSize;
  TIntProperty m_rasterToolSize;
  TDoubleProperty m_opacity;
  TDoubleProperty m_hardness;
  TEnumProperty m_type;
  TIntProperty m_edgeCount;
  TBoolProperty m_rotate;
  TBoolProperty m_autogroup;
  TBoolProperty m_autofill;
  TBoolProperty m_smooth;
  TBoolProperty m_selective;
  TBoolProperty m_pencil;
  TEnumProperty m_capStyle;
  TEnumProperty m_joinStyle;
  TIntProperty m_miterJoinLimit;
  TBoolProperty m_snap;
  TEnumProperty m_snapSensitivity;
  TBoolProperty m_sendToBack;
  TPropertyGroup m_prop[2];
  TDoubleProperty m_modifierSize;
  TBoolProperty m_modifierEraser;
  TEnumProperty m_modifierJoinStyle;
  TDoubleProperty m_modifierPressure;
  TEnumProperty m_frameRange;

  int m_targetType;

  // for snapping
  int m_strokeIndex1;
  double m_w1, m_pixelSize, m_currThickness, m_minDistance2;
  bool m_foundSnap = false;
  TPointD m_snapPoint;

  PrimitiveParam(int targetType)
      : m_type("Shape:")  // "W_ToolOptions_ShapeType")
      , m_toolSize("Size:", 0, 100,
                   1)  // "W_ToolOptions_ShapeThickness", 0,30,1)
      , m_rasterToolSize("Size:", 1, 100, 1)
      , m_opacity("Opacity:", 0, 100, 100)
      , m_hardness("Hardness:", 0, 100, 100)
      , m_edgeCount("Polygon Sides:", 3, 15, 3)
      , m_rotate("rotate", false)
      , m_autogroup("Auto Group", false)
      , m_autofill("Auto Fill", false)
      , m_smooth("Smooth", false)
      , m_selective("Selective", false)
      , m_pencil("Pencil Mode", false)
      , m_capStyle("Cap")
      , m_joinStyle("Join")
      , m_miterJoinLimit("Miter:", 0, 100, 4)
      , m_snap("Snap", false)
      , m_snapSensitivity("Sensitivity:")
      , m_sendToBack("Draw Under", 0)
      , m_targetType(targetType)
      , m_modifierSize("ModifierSize", -3, 3, 0, true)
      , m_modifierEraser("ModifierEraser", false)
      , m_modifierJoinStyle("ModifierJoin")
      , m_modifierPressure("ModifierPressure", -0.49, 0.5, 0)
      , m_frameRange("Range:") {
    if (targetType & TTool::Vectors) m_prop[0].bind(m_toolSize);
    if (targetType & TTool::ToonzImage || targetType & TTool::RasterImage) {
      m_prop[0].bind(m_modifierSize);
      m_prop[0].bind(m_rasterToolSize);
      m_prop[0].bind(m_hardness);
      m_prop[0].bind(m_modifierPressure);
    }
    if (targetType & TTool::RasterImage) {
      m_prop[0].bind(m_opacity);
      m_prop[0].bind(m_modifierEraser);
    }
    m_prop[0].bind(m_type);

    m_prop[0].bind(m_edgeCount);
    m_prop[0].bind(m_rotate);
    if (targetType & TTool::Vectors) {
      m_prop[0].bind(m_autogroup);
      m_prop[0].bind(m_autofill);
      m_prop[0].bind(m_sendToBack);
      m_sendToBack.setId("DrawUnder");
      m_prop[0].bind(m_snap);
      m_snap.setId("Snap");
      m_prop[0].bind(m_snapSensitivity);
      m_snapSensitivity.addValue(LOW_WSTR);
      m_snapSensitivity.addValue(MEDIUM_WSTR);
      m_snapSensitivity.addValue(HIGH_WSTR);
      m_snapSensitivity.setId("SnapSensitivity");
    }
    if (targetType & TTool::ToonzImage) {
      m_prop[0].bind(m_selective);
      m_prop[0].bind(m_pencil);
      m_pencil.setId("PencilMode");
    }
    m_prop[0].bind(m_smooth);
    m_prop[0].bind(m_frameRange);

    m_frameRange.addValue(L"Off");
    m_frameRange.addValue(LINEAR_INTERPOLATION);
    m_frameRange.addValue(EASE_IN_INTERPOLATION);
    m_frameRange.addValue(EASE_OUT_INTERPOLATION);
    m_frameRange.addValue(EASE_IN_OUT_INTERPOLATION);
    m_frameRange.setId("FrameRange");

    m_capStyle.addValue(BUTT_WSTR, QString::fromStdWString(BUTT_WSTR));
    m_capStyle.addValue(ROUNDC_WSTR, QString::fromStdWString(ROUNDC_WSTR));
    m_capStyle.addValue(PROJECTING_WSTR,
                        QString::fromStdWString(PROJECTING_WSTR));
    m_capStyle.setId("Cap");

    m_joinStyle.addValue(MITER_WSTR, QString::fromStdWString(MITER_WSTR));
    m_joinStyle.addValue(ROUNDJ_WSTR, QString::fromStdWString(ROUNDJ_WSTR));
    m_joinStyle.addValue(BEVEL_WSTR, QString::fromStdWString(BEVEL_WSTR));
    m_joinStyle.setId("Join");

    m_modifierJoinStyle.addValue(MITER_WSTR,
                                 QString::fromStdWString(MITER_WSTR));
    m_modifierJoinStyle.addValue(ROUNDJ_WSTR,
                                 QString::fromStdWString(ROUNDJ_WSTR));
    m_modifierJoinStyle.setId("Join");

    m_miterJoinLimit.setId("Miter");

    m_prop[1].bind(m_capStyle);
    m_prop[1].bind(m_joinStyle);
    m_prop[1].bind(m_modifierJoinStyle);
    m_prop[1].bind(m_miterJoinLimit);

    m_selective.setId("Selective");
    m_rotate.setId("Rotate");
    m_autogroup.setId("AutoGroup");
    m_autofill.setId("Autofill");
    m_smooth.setId("Smooth");
    m_type.setId("GeometricShape");
    m_edgeCount.setId("GeometricEdge");
  }

  void updateTranslation() {
    m_type.setQStringName(tr("Shape:"));
    m_type.setItemUIName(L"Rectangle", tr("Rectangle"));
    m_type.setItemUIName(L"Circle", tr("Circle"));
    m_type.setItemUIName(L"Ellipse", tr("Ellipse"));
    m_type.setItemUIName(L"Line", tr("Line"));
    m_type.setItemUIName(L"Polyline", tr("Polyline"));
    m_type.setItemUIName(L"Arc", tr("Arc"));
    m_type.setItemUIName(L"MultiArc", tr("MultiArc"));
    m_type.setItemUIName(L"Polygon", tr("Polygon"));

    m_toolSize.setQStringName(tr("Size:"));
    m_rasterToolSize.setQStringName(tr("Thickness:"));
    m_opacity.setQStringName(tr("Opacity:"));
    m_hardness.setQStringName(tr("Hardness:"));
    m_edgeCount.setQStringName(tr("Polygon Sides:"));
    m_rotate.setQStringName(tr("Rotate"));
    m_autogroup.setQStringName(tr("Auto Group"));
    m_autofill.setQStringName(tr("Auto Fill"));
    m_smooth.setQStringName(tr("Smooth"));
    m_selective.setQStringName(tr("Selective"));
    m_pencil.setQStringName(tr("Pencil Mode"));
    m_modifierSize.setQStringName(tr("Size"));
    m_modifierEraser.setQStringName(tr("Eraser"));
    m_modifierPressure.setQStringName(tr("Pressure"));

    m_capStyle.setQStringName(tr("Cap"));
    m_capStyle.setItemUIName(BUTT_WSTR, tr("Butt cap"));
    m_capStyle.setItemUIName(ROUNDC_WSTR, tr("Round cap"));
    m_capStyle.setItemUIName(PROJECTING_WSTR, tr("Projecting cap"));

    m_joinStyle.setQStringName(tr("Join"));
    m_joinStyle.setItemUIName(MITER_WSTR, tr("Miter join"));
    m_joinStyle.setItemUIName(ROUNDJ_WSTR, tr("Round join"));
    m_joinStyle.setItemUIName(BEVEL_WSTR, tr("Bevel join"));

    m_modifierJoinStyle.setQStringName(tr("Join"));
    m_modifierJoinStyle.setItemUIName(MITER_WSTR, tr("Miter join"));
    m_modifierJoinStyle.setItemUIName(ROUNDJ_WSTR, tr("Round join"));

    m_miterJoinLimit.setQStringName(tr("Miter:"));
    m_snap.setQStringName(tr("Snap"));
    m_snapSensitivity.setQStringName(tr(""));
    if (m_targetType & TTool::Vectors) {
      m_snapSensitivity.setItemUIName(LOW_WSTR, tr("Low"));
      m_snapSensitivity.setItemUIName(MEDIUM_WSTR, tr("Med"));
      m_snapSensitivity.setItemUIName(HIGH_WSTR, tr("High"));
      m_sendToBack.setQStringName(tr("Draw Under"));
    }

    m_frameRange.setQStringName(tr("Range:"));
    m_frameRange.setItemUIName(L"Off", tr("Off"));
    m_frameRange.setItemUIName(LINEAR_INTERPOLATION, tr("Linear"));
    m_frameRange.setItemUIName(EASE_IN_INTERPOLATION, tr("Ease In"));
    m_frameRange.setItemUIName(EASE_OUT_INTERPOLATION, tr("Ease Out"));
    m_frameRange.setItemUIName(EASE_IN_OUT_INTERPOLATION, tr("Ease In/Out"));
  }
};

//=============================================================================
// Abstract Class Primitive
//-----------------------------------------------------------------------------

class Primitive {
protected:
  bool m_isEditing, m_rasterTool, m_isPrompting, m_doSnap;
  GeometricTool *m_tool;
  PrimitiveParam *m_param;
  bool m_isMyPaintStyleSelected;

public:
  Primitive(PrimitiveParam *param, GeometricTool *tool, bool rasterTool)
      : m_param(param)
      , m_tool(tool)
      , m_isEditing(false)
      , m_rasterTool(rasterTool)
      , m_isPrompting(false)
      , m_isMyPaintStyleSelected(false) {}

  double getThickness() const {
    if (m_rasterTool)
      return m_param->m_rasterToolSize.getValue() * 0.5;
    else
      return m_param->m_toolSize.getValue() * 0.5;
  }

  void setIsPrompting(bool value) { m_isPrompting = value; }
  void setMyPaintStyleSelected(bool myPaintSelected) {
    m_isMyPaintStyleSelected = myPaintSelected;
  }

  virtual std::string getName() const = 0;

  virtual ~Primitive() {}

  virtual void leftButtonDown(const TPointD &p, const TMouseEvent &e){};
  virtual void leftButtonDrag(const TPointD &p, const TMouseEvent &e){};
  virtual void leftButtonUp(const TPointD &p, const TMouseEvent &e){};
  virtual void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e){};
  virtual void rightButtonDown(const TPointD &p, const TMouseEvent &e){};
  virtual void mouseMove(const TPointD &p, const TMouseEvent &e){};
  virtual bool keyDown(QKeyEvent *event) { return false; }
  virtual void onEnter(){};
  virtual void draw(){};
  virtual void onActivate(){};
  virtual void onDeactivate(){};
  virtual void onImageChanged(){};
  TPointD calculateSnap(TPointD pos, const TMouseEvent &e);
  void drawSnap();
  TPointD getSnap(TPointD pos);
  void resetSnap();
  TPointD checkGuideSnapping(TPointD pos, const TMouseEvent &e);
  bool getSmooth() { return m_param->m_smooth.getValue(); }

  virtual TStroke *makeStroke(int index = 0) = 0;
  virtual std::vector<TStroke *> getStrokes() {
    return std::vector<TStroke *>();
  }
  virtual bool canTouchImageOnPreLeftClick() { return true; }
};

//-----------------------------------------------------------------------------

void Primitive::resetSnap() {
  m_param->m_strokeIndex1 = -1;
  m_param->m_w1           = -1;
  m_param->m_foundSnap    = false;
}

//-----------------------------------------------------------------------------

TPointD Primitive::calculateSnap(TPointD pos, const TMouseEvent &e) {
  m_param->m_foundSnap = false;
  if (Preferences::instance()->getVectorSnappingTarget() == 1) return pos;
  TVectorImageP vi(TTool::getImage(false));
  TPointD snapPoint = pos;
  m_doSnap          = (m_param->m_snap.getValue() &&
              !(e.isCtrlPressed() && e.isShiftPressed())) ||
             (!m_param->m_snap.getValue() &&
              (e.isCtrlPressed() && e.isShiftPressed()));
  if (vi && m_doSnap) {
    double minDistance2     = m_param->m_minDistance2;
    m_param->m_strokeIndex1 = -1;

    int i, strokeNumber = vi->getStrokeCount();

    TStroke *stroke;
    double distance2, outW;

    for (i = 0; i < strokeNumber; i++) {
      stroke = vi->getStroke(i);
      if (stroke->getNearestW(pos, outW, distance2) &&
          distance2 < minDistance2) {
        minDistance2            = distance2;
        m_param->m_strokeIndex1 = i;
        if (areAlmostEqual(outW, 0.0, 1e-3))
          m_param->m_w1 = 0.0;
        else if (areAlmostEqual(outW, 1.0, 1e-3))
          m_param->m_w1 = 1.0;
        else
          m_param->m_w1      = outW;
        TThickPoint point1   = stroke->getPoint(m_param->m_w1);
        snapPoint            = TPointD(point1.x, point1.y);
        m_param->m_foundSnap = true;
        m_param->m_snapPoint = snapPoint;
      }
    }
  }
  return snapPoint;
}

//-----------------------------------------------------------------------------

TPointD Primitive::getSnap(TPointD pos) {
  if (m_doSnap && m_param->m_foundSnap)
    return m_param->m_snapPoint;
  else
    return pos;
}

// Primitive::drawSnap and Primitive::checkGuideSnapping are below the
// Geometric Tool definition since they use the m_tool property of Primitive
// but it isn't defined yet up here.

//=============================================================================
// Rectangle Primitive Class Declaration
//-----------------------------------------------------------------------------

class RectanglePrimitive final : public Primitive {
  TRectD m_selectingRect;
  TPointD m_pos;
  TPointD m_startPoint;
  TPixel32 m_color;

  SymmetryStroke m_rectangle;

public:
  RectanglePrimitive(PrimitiveParam *param, GeometricTool *tool,
                     bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Rectangle";
  }  // W_ToolOptions_ShapeRect"; }

  TStroke *makeStroke(int index = 0) override;
  std::vector<TStroke *> getStrokes() override;
  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &realPos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
};

//=============================================================================
// Circle Primitive Class Declaration
//-----------------------------------------------------------------------------

class CirclePrimitive final : public Primitive {
  TPointD m_centre;
  TPointD m_pos;
  double m_radius;
  TPixel32 m_color;

  SymmetryStroke m_circle;

public:
  CirclePrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Circle";
  }  // W_ToolOptions_ShapeCircle";}

  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  TStroke *makeStroke(int index = 0) override;
  std::vector<TStroke *> getStrokes() override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
};

//=============================================================================
// MultiLine Primitive Class Declaration
//-----------------------------------------------------------------------------

const double joinDistance = 5.0;

class MultiLinePrimitive : public Primitive {
protected:
  SymmetryStroke m_vertex;
  TPointD m_mousePosition;
  TPixel32 m_color;
  bool m_closed, m_isSingleLine;
  bool m_speedMoved;        //!< True after moveSpeed(), false after addVertex()
  bool m_beforeSpeedMoved;  //!< Old value of m_speedMoved
  bool m_ctrlDown;          //!< Whether ctrl is hold down
  bool m_shiftDown;
  bool m_altDown;
  MultilinePrimitiveUndo *m_undo;

public:
  MultiLinePrimitive(PrimitiveParam *param, GeometricTool *tool,
                     bool reasterTool)
      : Primitive(param, tool, reasterTool)
      , m_closed(false)
      , m_isSingleLine(false)
      , m_speedMoved(false)
      , m_beforeSpeedMoved(false)
      , m_ctrlDown(false)
      , m_shiftDown(false)
      , m_altDown(false) {}

  std::string getName() const override {
    return "Polyline";
  }  // W_ToolOptions_ShapePolyline";}

  void addVertex(const TPointD &pos);
  void moveSpeed(const TPointD &delta);
  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  bool keyDown(QKeyEvent *event) override;
  TStroke *makeStroke(int index = 0) override;
  std::vector<TStroke *> getStrokes() override;
  void endLine();
  void onActivate() override;
  void onDeactivate() override {
    m_vertex.reset();
    m_speedMoved       = false;
    m_beforeSpeedMoved = false;
  }
  void onEnter() override;
  void onImageChanged() override;
  void setVertexes(const SymmetryStroke &vertex) { m_vertex = vertex; };
  void setSpeedMoved(bool speedMoved) { m_speedMoved = speedMoved; };

  // Only execute touchImage when clicking the first point of the polyline
  bool canTouchImageOnPreLeftClick() override { return m_vertex.empty(); }
};

//-----------------------------------------------------------------------------

void MultilinePrimitiveUndo::undo() const {
  m_tool->setVertexes(m_oldVertex);
  int count       = m_oldVertex.size();
  bool speedMoved = (count != 0 && count % 4 != 1);
  m_tool->setSpeedMoved(speedMoved);
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}

//-----------------------------------------------------------------------------

void MultilinePrimitiveUndo::redo() const {
  m_tool->setVertexes(m_newVertex);
  int count       = m_newVertex.size();
  bool speedMoved = (count != 0 && count % 4 != 1);
  m_tool->setSpeedMoved(speedMoved);
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}

//----------------------------------------------------------------------------

QString MultilinePrimitiveUndo::getToolName() {
  return QString("Geometric Tool %1")
      .arg(QString::fromStdString(m_tool->getName()));
}

//=============================================================================
// Line Primitive Class Declaration
//-----------------------------------------------------------------------------

class LinePrimitive final : public MultiLinePrimitive {
public:
  LinePrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : MultiLinePrimitive(param, tool, reasterTool) {
    m_isSingleLine = true;
  }

  std::string getName() const override {
    return "Line";
  }  // W_ToolOptions_ShapePolyline";}

  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e) override {}
};

//=============================================================================
// Ellipse Primitive Class Declaration
//-----------------------------------------------------------------------------

class EllipsePrimitive final : public Primitive {
  TPointD m_pos;
  TRectD m_selectingRect;
  TPointD m_startPoint;
  TPixel32 m_color;

  SymmetryStroke m_ellipse;

public:
  EllipsePrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Ellipse";
  }  // W_ToolOptions_ShapeEllipse";}

  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &realPos, const TMouseEvent &e) override;
  TStroke *makeStroke(int index = 0) override;
  std::vector<TStroke *> getStrokes() override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
};

//=============================================================================
// Multi Arc Primitive Class Declaration
//-----------------------------------------------------------------------------

class MultiArcPrimitive : public Primitive {
  TStroke *m_stroke;
  TStroke *m_strokeTemp;
  TPointD m_startPoint, m_endPoint, m_centralPoint;
  int m_clickNumber;
  TPixel32 m_color;
  int m_undoCount;

  SymmetryStroke m_arc;

protected:
  bool m_isSingleArc;

public:
  MultiArcPrimitive(PrimitiveParam *param, GeometricTool *tool,
                    bool reasterTool)
      : Primitive(param, tool, reasterTool)
      , m_stroke(0)
      , m_strokeTemp(0)
      , m_clickNumber(0)
      , m_isSingleArc(false)
      , m_undoCount(0) {}

  ~MultiArcPrimitive() { delete m_stroke; }

  std::string getName() const override { return "MultiArc"; }

  TStroke *makeStroke(int index = 0) override;
  std::vector<TStroke *> getStrokes() override;
  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDoubleClick(const TPointD &, const TMouseEvent &e) override;
  bool keyDown(QKeyEvent *event) override;
  void onEnter() override;
  void onDeactivate() override {
    delete m_stroke;
    delete m_strokeTemp;
    m_stroke      = 0;
    m_strokeTemp  = 0;
    m_clickNumber = 0;
    TUndoManager::manager()->popUndo(m_undoCount);
    m_undoCount = 0;
  }

  void replaceData(TStroke *stroke, TStroke *strokeTemp, TPointD startPoint,
                   TPointD endPoint, TPointD centralPoint, int clickNumber) {
    delete m_stroke;
    delete m_strokeTemp;
    m_stroke       = stroke;
    m_strokeTemp   = strokeTemp;
    m_startPoint   = startPoint;
    m_endPoint     = endPoint;
    m_centralPoint = centralPoint;
    m_clickNumber  = clickNumber;

    if (m_arc.hasSymmetryBrushes()) {
      m_arc.clear();
      m_arc.push_back(m_startPoint);
      if (m_clickNumber == 2) m_arc.push_back(m_endPoint);
      if (m_clickNumber == 3) m_arc.push_back(m_centralPoint);
    }
  }

  void decreaseUndo() { --m_undoCount; }

  void increaseUndo() { ++m_undoCount; }

  // Only execute touchImage when clicking the first point of the multi arc
  bool canTouchImageOnPreLeftClick() override { return m_clickNumber == 0; }
};

//-----------------------------------------------------------------------------

void MultiArcPrimitiveUndoData::replace(MultiArcPrimitive *tool) const {
  TStroke *stroke     = 0;
  TStroke *strokeTemp = 0;
  if (m_stroke) {
    stroke = new TStroke(*m_stroke);
  }
  if (m_strokeTemp) {
    strokeTemp = new TStroke(*m_strokeTemp);
  }
  tool->replaceData(stroke, strokeTemp, m_startPoint, m_endPoint,
                    m_centralPoint, m_clickNumber);
}

//-----------------------------------------------------------------------------

void MultiArcPrimitiveUndo::undo() const {
  m_undo.replace(m_tool);
  m_tool->decreaseUndo();
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}

//-----------------------------------------------------------------------------

void MultiArcPrimitiveUndo::redo() const {
  m_redo->replace(m_tool);
  m_tool->increaseUndo();
  TTool::getApplication()->getCurrentTool()->getTool()->invalidate();
}

//----------------------------------------------------------------------------

QString MultiArcPrimitiveUndo::getToolName() {
  return QString("Geometric Tool %1")
      .arg(QString::fromStdString(m_tool->getName()));
}

//=============================================================================
// Arc Primitive Class Declaration
//-----------------------------------------------------------------------------

class ArcPrimitive final : public MultiArcPrimitive {
public:
  ArcPrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : MultiArcPrimitive(param, tool, reasterTool) {
    m_isSingleArc = true;
  }

  std::string getName() const override {
    return "Arc";
  }  // _ToolOptions_ShapeArc";}
};

//=============================================================================
// Polygon Primitive Class Declaration
//-----------------------------------------------------------------------------

class PolygonPrimitive final : public Primitive {
  TPointD m_startPoint, m_centre;
  double m_radius;
  TPixel32 m_color;

  SymmetryStroke m_polygon;

public:
  PolygonPrimitive(PrimitiveParam *param, GeometricTool *tool, bool reasterTool)
      : Primitive(param, tool, reasterTool) {}

  std::string getName() const override {
    return "Polygon";
  }  // W_ToolOptions_ShapePolygon";}

  TStroke *makeStroke(int index = 0) override;
  std::vector<TStroke *> getStrokes() override;
  void draw() override;
  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
};

//=============================================================================
// Geometric Tool
//-----------------------------------------------------------------------------

class GeometricTool final : public TTool, public RasterController {
protected:
  Primitive *m_primitive;
  std::map<std::wstring, Primitive *> m_primitiveTable;
  PrimitiveParam m_param;
  std::wstring m_typeCode;
  bool m_active;
  bool m_firstTime;

  // for both rotation and move
  bool m_isRotatingOrMoving;
  bool m_wasCtrlPressed;
  std::vector<TStroke *> m_rotatedStroke;
  TPointD m_originalCursorPos;
  TPointD m_currentCursorPos;
  TPixel32 m_color;

  // for rotation
  double m_lastRotateAngle;
  TPointD m_rotateCenter;

  // for move
  TPointD m_lastMoveStrokePos;

  bool m_isMyPaintStyleSelected;
  GeometricToolNotifier *m_notifier;

  TTileSaverCM32 *m_tileSaverCM32;
  TTileSaverFullColor *m_tileSaverFullColor;
  TRaster32P m_workRaster;
  TRect m_strokeRect, m_lastRect, m_strokeSegmentRect;

  std::vector<TStroke *> m_firstStrokes;
  std::pair<int, int> m_currCell;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_firstFrameIdx, m_lastFrameIdx;
  bool m_shiftPressed;

public:
  GeometricTool(int targetType)
      : TTool("T_Geometric")
      , m_primitive(0)
      , m_param(targetType)
      , m_active(false)
      , m_isRotatingOrMoving(false)
      , m_firstTime(true)
      , m_isMyPaintStyleSelected(false)
      , m_notifier(0)
      , m_tileSaverCM32(0)
      , m_tileSaverFullColor(0)
      , m_currCell(-1, -1)
      , m_shiftPressed(false) {
    bind(targetType);
    if ((targetType & TTool::RasterImage) || (targetType & TTool::ToonzImage)) {
      addPrimitive(new RectanglePrimitive(&m_param, this, true));
      addPrimitive(new CirclePrimitive(&m_param, this, true));
      addPrimitive(new EllipsePrimitive(&m_param, this, true));
      addPrimitive(new LinePrimitive(&m_param, this, true));
      addPrimitive(new MultiLinePrimitive(&m_param, this, true));
      addPrimitive(new ArcPrimitive(&m_param, this, true));
      addPrimitive(new MultiArcPrimitive(&m_param, this, true));
      addPrimitive(new PolygonPrimitive(&m_param, this, true));
    } else  // targetType == 1
    {
      // vector
      addPrimitive(m_primitive = new RectanglePrimitive(&m_param, this, false));
      addPrimitive(new CirclePrimitive(&m_param, this, false));
      addPrimitive(new EllipsePrimitive(&m_param, this, false));
      addPrimitive(new LinePrimitive(&m_param, this, false));
      addPrimitive(new MultiLinePrimitive(&m_param, this, false));
      addPrimitive(new ArcPrimitive(&m_param, this, false));
      addPrimitive(new MultiArcPrimitive(&m_param, this, false));
      addPrimitive(new PolygonPrimitive(&m_param, this, false));
    }
  }

  ~GeometricTool() {
    m_rotatedStroke.clear();
    std::map<std::wstring, Primitive *>::iterator it;
    for (it = m_primitiveTable.begin(); it != m_primitiveTable.end(); ++it)
      delete it->second;
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void updateTranslation() override { m_param.updateTranslation(); }

  void addPrimitive(Primitive *p) {
    // TODO: aggiungere il controllo per evitare nomi ripetuti
    std::wstring name = ::to_wstring(p->getName());
    // wstring name = TStringTable::translate(p->getName());

    m_primitiveTable[name] = p;
    m_param.m_type.addValue(name);
  }

  void changeType(std::wstring name) {
    std::map<std::wstring, Primitive *>::iterator it =
        m_primitiveTable.find(name);
    if (it != m_primitiveTable.end()) {
      if (m_primitive) m_primitive->onDeactivate();
      m_primitive = it->second;
    }
  }

  bool preLeftButtonDown() override {
    if (getViewer() && getViewer()->getGuidedStrokePickerMode()) return false;
    if (getApplication()->getCurrentObject()->isSpline()) return true;

    // in the halfway through the drawing of Polyline / MultiArc primitive, OT
    // should not call touchImage or the m_frameCreated / m_levelCreated flags
    // will be reset.
    if (m_primitive && !m_primitive->canTouchImageOnPreLeftClick()) return true;
    // NEEDS to be done even if(m_active), due
    // to the HORRIBLE m_frameCreated / m_levelCreated
    // mechanism. touchImage() is the ONLY function
    // resetting them to false...                       >_<
    m_active = !!touchImage();
    return true;
  }

  void leftButtonDown(const TPointD &p, const TMouseEvent &e) override {
    if (getViewer() && getViewer()->getGuidedStrokePickerMode()) {
      getViewer()->doPickGuideStroke(p);
      return;
    }
    if (m_isRotatingOrMoving) {
      addStroke();
      return;
    }
    if (m_primitive) m_primitive->leftButtonDown(p, e);
    invalidate();
  }
  void leftButtonDrag(const TPointD &p, const TMouseEvent &e) override {
    if (!m_active) return;
    if (m_primitive) m_primitive->leftButtonDrag(p, e);
    invalidate();
  }
  void leftButtonUp(const TPointD &p, const TMouseEvent &e) override {
    if (!m_active) return;

    bool isEditingLevel = m_application->getCurrentFrame()->isEditingLevel();
    bool renameColumn   = m_isFrameCreated;
    if (!isEditingLevel && renameColumn) TUndoManager::manager()->beginBlock();
    m_shiftPressed = e.isShiftPressed();

    if (m_primitive) m_primitive->leftButtonUp(p, e);

    // Column name renamed to level name only if was originally empty
    if (!isEditingLevel && renameColumn) {
      int col = m_application->getCurrentColumn()->getColumnIndex();
      TXshColumn *column =
          m_application->getCurrentXsheet()->getXsheet()->getColumn(col);
      int r0, r1;
      column->getRange(r0, r1);
      if (r0 == r1) {
        TXshLevel *level = m_application->getCurrentLevel()->getLevel();
        TXshSimpleLevelP simLevel = level->getSimpleLevel();
        TStageObjectId columnId = TStageObjectId::ColumnId(col);
        std::string columnName =
            QString::fromStdWString(simLevel->getName()).toStdString();
        TStageObjectCmd::rename(columnId, columnName,
                                m_application->getCurrentXsheet());
      }

      TUndoManager::manager()->endBlock();
    }

    invalidate();
  }
  void leftButtonDoubleClick(const TPointD &p, const TMouseEvent &e) override {
    if (!m_active) return;
    if (m_primitive) m_primitive->leftButtonDoubleClick(p, e);
    invalidate();
  }

  bool keyDown(QKeyEvent *event) override {
    return m_primitive->keyDown(event);
  }

  void onImageChanged() override {
    if (m_primitive) m_primitive->onImageChanged();
    m_isRotatingOrMoving = false;
    m_rotatedStroke.clear();
    invalidate();
  }

  void rightButtonDown(const TPointD &p, const TMouseEvent &e) override {
    if (m_primitive) m_primitive->rightButtonDown(p, e);
    invalidate();
  }

  void mouseMove(const TPointD &p, const TMouseEvent &e) override {
    m_currentCursorPos = p;
    if (m_isRotatingOrMoving) {
      // move
      if (e.isCtrlPressed()) {
        // if ctrl wasn't pressed, it means the user has switched from
        // rotation to move. Thus, re-initiate move-relevant variables
        if (!m_wasCtrlPressed) {
          m_wasCtrlPressed = true;

          m_originalCursorPos = m_currentCursorPos;
          m_lastMoveStrokePos = TPointD(0, 0);
        }

        // move the stroke to the original location
        double x = -m_lastMoveStrokePos.x;
        double y = -m_lastMoveStrokePos.y;
        m_rotatedStroke[0]->transform(TTranslation(x, y));

        // move the stroke according to current mouse position
        double dx           = m_currentCursorPos.x - m_originalCursorPos.x;
        double dy           = m_currentCursorPos.y - m_originalCursorPos.y;
        m_lastMoveStrokePos = TPointD(dx, dy);
        m_rotatedStroke[0]->transform(TTranslation(dx, dy));

        if (m_rotatedStroke.size() > 1) {
          SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
              TTool::getTool("T_Symmetry", TTool::RasterImage));
          TPointD dpiScale       = getViewer()->getDpiScale();

          std::vector<TPointD> origlocs = symmetryTool->getSymmetryPoints(
              TPointD(x, y), TPointD(), dpiScale);
          std::vector<TPointD> deltas = symmetryTool->getSymmetryPoints(
              TPointD(dx, dy), TPointD(), dpiScale);
          for (int i = 1; i < origlocs.size(); i++) {
            m_rotatedStroke[i]->transform(
                TTranslation(origlocs[i].x, origlocs[i].y));
            m_rotatedStroke[i]->transform(
                TTranslation(deltas[i].x, deltas[i].y));
          }
        }
        invalidate();
        return;
      }

      // if ctrl was pressed, it means the user has switched from
      // move to rotation. Thus, re-initiate rotation-relevant variables
      if (m_wasCtrlPressed) {
        m_wasCtrlPressed = false;

        m_lastRotateAngle   = 0;
        m_originalCursorPos = m_currentCursorPos;
        TRectD bbox         = m_rotatedStroke[0]->getBBox();
        m_rotateCenter      = 0.5 * (bbox.getP11() + bbox.getP00());
      }

      // rotate
      // first, rotate the stroke back to original
      m_rotatedStroke[0]->transform(
          TRotation(m_rotateCenter, -m_lastRotateAngle));

      // then, rotate it according to mouse position
      // this formula is from: https://stackoverflow.com/a/31334882
      TPointD center = m_rotateCenter;
      TPointD org    = m_originalCursorPos;
      TPointD cur    = m_currentCursorPos;
      double angle1  = atan2(cur.y - center.y, cur.x - center.x);
      double angle2  = atan2(org.y - center.y, org.x - center.x);
      double angle   = (angle1 - angle2) * 180 / 3.14;
      if (e.isShiftPressed()) {
        angle = ((int)angle / 45) * 45;
      }
      m_rotatedStroke[0]->transform(TRotation(m_rotateCenter, angle));

      if (m_rotatedStroke.size() > 1) {
        SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
            TTool::getTool("T_Symmetry", TTool::RasterImage));
        TPointD dpiScale       = getViewer()->getDpiScale();
        SymmetryObject symmObj = symmetryTool->getSymmetryObject();

        std::vector<TPointD> origCenters = symmetryTool->getSymmetryPoints(
            m_rotateCenter, TPointD(), dpiScale);
        for (int i = 1; i < origCenters.size(); i++) {
          double lastRotateAngle = m_lastRotateAngle;
          double newRotateAngle  = angle;
          if (symmObj.isUsingLineSymmetry() && (i % 2) != 0) {
            lastRotateAngle *= -1;
            newRotateAngle *= -1;
          }
          m_rotatedStroke[i]->transform(
              TRotation(origCenters[i], -lastRotateAngle));
          m_rotatedStroke[i]->transform(
              TRotation(origCenters[i], newRotateAngle));
        }
      }

      m_lastRotateAngle = angle;
      invalidate();
      return;
    }
    if (m_primitive) m_primitive->mouseMove(p, e);
  }

  void onActivate() override {
    if (!m_notifier) m_notifier = new GeometricToolNotifier(this);

    if (m_firstTime) {
      m_param.m_toolSize.setValue(GeometricSize);
      m_param.m_rasterToolSize.setValue(GeometricRasterSize);
      m_param.m_modifierSize.setValue(GeometricModifierSize);
      m_param.m_modifierEraser.setValue(GeometricModifierEraser ? 1 : 0);
      m_param.m_modifierPressure.setValue(GeometricModifierPressure);
      m_param.m_opacity.setValue(GeometricOpacity);
      m_param.m_hardness.setValue(GeometricBrushHardness);
      m_param.m_selective.setValue(GeometricSelective ? 1 : 0);
      m_param.m_rotate.setValue(GeometricRotate ? 1 : 0);
      m_param.m_autogroup.setValue(GeometricGroupIt ? 1 : 0);
      m_param.m_smooth.setValue(GeometricSmooth ? 1 : 0);
      m_param.m_autofill.setValue(GeometricAutofill ? 1 : 0);
      std::wstring typeCode = ::to_wstring(GeometricType.getValue());
      m_param.m_type.setValue(typeCode);
      GeometricType = ::to_string(typeCode);
      m_typeCode    = typeCode;
      changeType(typeCode);
      m_param.m_edgeCount.setValue(GeometricEdgeCount);
      m_param.m_pencil.setValue(GeometricPencil ? 1 : 0);
      m_param.m_capStyle.setIndex(GeometricCapStyle);
      m_param.m_joinStyle.setIndex(GeometricJoinStyle);
      m_param.m_modifierJoinStyle.setIndex(GeometricModifierJoinStyle);
      m_param.m_miterJoinLimit.setValue(GeometricMiterValue);
      m_firstTime = false;
      m_param.m_snap.setValue(GeometricSnap);
      m_param.m_sendToBack.setValue(GeometricDrawBehind);
      if (m_targetType & TTool::Vectors) {
        m_param.m_snapSensitivity.setIndex(GeometricSnapSensitivity);
        switch (GeometricSnapSensitivity) {
        case 0:
          m_param.m_minDistance2 = SNAPPING_LOW;
          break;
        case 1:
          m_param.m_minDistance2 = SNAPPING_MEDIUM;
          break;
        case 2:
          m_param.m_minDistance2 = SNAPPING_HIGH;
          break;
        }
      }
      m_param.m_frameRange.setIndex(GeometricRange);

      resetMulti();
    }
    m_primitive->resetSnap();
    /*--
       ショートカットでいきなりスタート（＝onEnterを通らない場合）のとき、
            LineToolが反応しないことがある対策 --*/
    m_active = (getImage(false) != 0 ||
                Preferences::instance()->isAutoCreateEnabled());

    if (m_primitive) m_primitive->onActivate();
  }

  void onDeactivate() override {
    int devPixRatio = m_viewer->getDevPixRatio();

    if (m_isRotatingOrMoving) {
      tglColor(m_color);
      glLineWidth(1.0 * devPixRatio);
      for (int i = 0; i < m_rotatedStroke.size(); i++)
        drawStrokeCenterline(*m_rotatedStroke[i], sqrt(tglGetPixelSize2()));
      return;
    }
    if (m_primitive) m_primitive->onDeactivate();
  }

  void onEnter() override {
    m_active = getImage(false) != 0;
    if (m_active && m_primitive) m_primitive->onEnter();
  }

  void draw() override {
    int devPixRatio = m_viewer->getDevPixRatio();

    glLineWidth(1.0 * devPixRatio);

    if (m_param.m_frameRange.getIndex() && m_firstStrokes.size()) {
      tglColor(m_color);
      for (int i = 0; i < m_firstStrokes.size(); i++)
        drawStrokeCenterline(*m_firstStrokes[i], sqrt(tglGetPixelSize2()));
    }
    if (m_isRotatingOrMoving) {
      tglColor(m_color);
      for (int i = 0; i < m_rotatedStroke.size(); i++)
        drawStrokeCenterline(*m_rotatedStroke[i], sqrt(tglGetPixelSize2()));
      return;
    }
    if (m_primitive) m_primitive->draw();
  }

  int getCursorId() const override {
    if (m_viewer && m_viewer->getGuidedStrokePickerMode())
      return m_viewer->getGuidedStrokePickerCursor();
    return ToolCursor::PenCursor;
  }

  int getColorClass() const { return 1; }

  TPropertyGroup *getProperties(int idx) override {
    return &m_param.m_prop[idx];
  }

  bool onPropertyChanged(std::string propertyName) override {
    /*---	変更されたPropertyごとに処理を分ける。
            注意：m_toolSizeとm_rasterToolSizeは同じName(="Size:")なので、
            扱っている画像がラスタかどうかで区別する ---*/
    if (propertyName == m_param.m_toolSize.getName()) {
      TImageP img = getImage(false);
      TToonzImageP ri(img); /*-- ラスタかどうかの判定 --*/
      if (ri)
        GeometricRasterSize = m_param.m_rasterToolSize.getValue();
      else
        GeometricSize = m_param.m_toolSize.getValue();
    } else if (propertyName == m_param.m_modifierSize.getName())
      GeometricModifierSize = m_param.m_modifierSize.getValue();
    else if (propertyName == m_param.m_modifierEraser.getName())
      GeometricModifierEraser = m_param.m_modifierEraser.getValue();
    else if (propertyName == m_param.m_modifierPressure.getName())
      GeometricModifierPressure = m_param.m_modifierPressure.getValue();
    else if (propertyName == m_param.m_type.getName()) {
      std::wstring typeCode = m_param.m_type.getValue();
      GeometricType         = ::to_string(typeCode);
      if (typeCode != m_typeCode) {
        m_typeCode = typeCode;
        changeType(typeCode);
        TTool::getApplication()->refreshStatusBar();
      }
    } else if (propertyName == m_param.m_edgeCount.getName())
      GeometricEdgeCount = m_param.m_edgeCount.getValue();
    else if (propertyName == m_param.m_rotate.getName())
      GeometricRotate = m_param.m_rotate.getValue();
    else if (propertyName == m_param.m_autogroup.getName()) {
      if (!m_param.m_autogroup.getValue()) {
        m_param.m_autofill.setValue(false);
        // this is ugly: it's needed to refresh the GUI of the toolbar after
        // having set to false the autofill...
        TTool::getApplication()->getCurrentTool()->setTool(
            "");  // necessary, otherwise next setTool is ignored...
        TTool::getApplication()->getCurrentTool()->setTool(
            QString::fromStdString(getName()));
      }
      GeometricGroupIt = m_param.m_autogroup.getValue();
    } else if (propertyName == m_param.m_autofill.getName()) {
      if (m_param.m_autofill.getValue()) {
        m_param.m_autogroup.setValue(true);
        // this is ugly: it's needed to refresh the GUI of the toolbar after
        // having set to false the autofill...
        TTool::getApplication()->getCurrentTool()->setTool(
            "");  // necessary, otherwise next setTool is ignored...
        TTool::getApplication()->getCurrentTool()->setTool(
            QString::fromStdString(getName()));
      }
      GeometricGroupIt = m_param.m_autofill.getValue();
    } else if (propertyName == m_param.m_smooth.getName()) {
      GeometricSmooth = m_param.m_smooth.getValue();
    } else if (propertyName == m_param.m_selective.getName())
      GeometricSelective = m_param.m_selective.getValue();
    else if (propertyName == m_param.m_pencil.getName())
      GeometricPencil = m_param.m_pencil.getValue();
    else if (propertyName == m_param.m_hardness.getName())
      GeometricBrushHardness = m_param.m_hardness.getValue();
    else if (propertyName == m_param.m_opacity.getName())
      GeometricOpacity = m_param.m_opacity.getValue();
    else if (propertyName == m_param.m_capStyle.getName())
      GeometricCapStyle = m_param.m_capStyle.getIndex();
    else if (propertyName == m_param.m_joinStyle.getName())
      GeometricJoinStyle = m_param.m_joinStyle.getIndex();
    else if (propertyName == m_param.m_modifierJoinStyle.getName())
      GeometricModifierJoinStyle = m_param.m_modifierJoinStyle.getIndex();
    else if (propertyName == m_param.m_miterJoinLimit.getName())
      GeometricMiterValue = m_param.m_miterJoinLimit.getValue();
    else if (propertyName == m_param.m_sendToBack.getName())
      GeometricDrawBehind = m_param.m_sendToBack.getValue();
    else if (propertyName == m_param.m_snap.getName())
      GeometricSnap = m_param.m_snap.getValue();
    else if (propertyName == m_param.m_snapSensitivity.getName()) {
      GeometricSnapSensitivity = m_param.m_snapSensitivity.getIndex();
      switch (GeometricSnapSensitivity) {
      case 0:
        m_param.m_minDistance2 = SNAPPING_LOW;
        break;
      case 1:
        m_param.m_minDistance2 = SNAPPING_MEDIUM;
        break;
      case 2:
        m_param.m_minDistance2 = SNAPPING_HIGH;
        break;
      }
    }
    // Frame Range
    else if (propertyName == m_param.m_frameRange.getName()) {
      GeometricRange = m_param.m_frameRange.getIndex();
      resetMulti();
    }
    return false;
  }

  void resetMulti() {
    m_firstFrameId = -1;
    m_firstStrokes.clear();
  }

  void addStroke() {
    if (!m_primitive) return;
    // TStroke *stroke = m_primitive->makeStroke();
    // if (!stroke) return;

    std::vector<TStroke *> strokes;
    if (!m_isRotatingOrMoving) {
      strokes = m_primitive->getStrokes();
      if (!strokes.size() || !strokes[0]) return;

      if (m_param.m_rotate.getValue()) {
        m_isRotatingOrMoving = true;
        m_rotatedStroke      = strokes;
        TRectD bbox          = strokes[0]->getBBox();
        m_rotateCenter       = 0.5 * (bbox.getP11() + bbox.getP00());
        m_originalCursorPos  = m_currentCursorPos;
        m_lastRotateAngle    = 0;
        m_lastMoveStrokePos  = TPointD(0, 0);
        m_wasCtrlPressed     = false;

        m_color = TPixel32::Red;
        return;
      }
    } else {
      strokes              = m_rotatedStroke;
      m_isRotatingOrMoving = false;
      m_rotatedStroke.clear();
    }

    TApplication *app = TTool::getApplication();

    if (m_param.m_frameRange.getIndex()) {
      if (!m_firstStrokes.size()) {
        m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
        m_firstStrokes = strokes;
        m_firstFrameId = m_veryFirstFrameId = getCurrentFid();
        m_firstFrameIdx = app->getCurrentFrame()->getFrameIndex();
        m_color         = TPixel32::Red;
        invalidate();
      } else {
        bool isEditingLevel = app->getCurrentFrame()->isEditingLevel();
        qApp->processEvents();

        TFrameId fid   = getCurrentFid();
        m_lastFrameIdx = app->getCurrentFrame()->getFrameIndex();

        TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(
            m_currCell.second, m_currCell.first);
        TXshSimpleLevel *sl = cell.getSimpleLevel();

        if (isEditingLevel)
          processSequence(sl, m_firstStrokes, strokes, m_firstFrameId, fid,
                          m_param.m_frameRange.getIndex());
        else
          processSequence(m_firstStrokes, strokes, m_firstFrameIdx,
                          m_lastFrameIdx, m_param.m_frameRange.getIndex());

        if (m_shiftPressed) {
          m_firstStrokes = strokes;
          if (isEditingLevel)
            m_firstFrameId = getCurrentFid();
          else {
            m_firstFrameIdx = m_lastFrameIdx;
            app->getCurrentFrame()->setFrame(m_firstFrameIdx);
          }
        } else {
          m_firstStrokes.clear();
          if (app->getCurrentFrame()->isEditingScene()) {
            app->getCurrentColumn()->setColumnIndex(m_currCell.first);
            app->getCurrentFrame()->setFrame(m_currCell.second);
          } else
            app->getCurrentFrame()->setFid(m_veryFirstFrameId);
        }
      }
      return;
    }

    if (strokes.size() > 1) TUndoManager::manager()->beginBlock();
    for (int i = 0; i < strokes.size(); i++) {
      TStroke *stroke = strokes[i];
      addStrokeToImage(getImage(true), stroke);
    }
    if (strokes.size() > 1) TUndoManager::manager()->endBlock();
  }

  void processSequence(TXshSimpleLevel *sl, std::vector<TStroke *> firstStrokes,
                       std::vector<TStroke *> lastStrokes, TFrameId firstFid,
                       TFrameId lastFid, int multi) {
    TTool::Application *app = TTool::getApplication();

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

    enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
    if (multi == 2) {  // EASE_IN_INTERPOLATION)
      algorithm = TInbetween::EaseInInterpolation;
    } else if (multi == 3) {  // EASE_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseOutInterpolation;
    } else if (multi == 4) {  // EASE_IN_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseInOutInterpolation;
    }

    TVectorImageP firstImage = new TVectorImage();
    TVectorImageP lastImage  = new TVectorImage();

    for (int i = 0; i < firstStrokes.size(); i++)
      firstImage->addStroke(new TStroke(*firstStrokes[i]), false);

    for (int i = 0; i < lastStrokes.size(); i++)
      lastImage->addStroke(new TStroke(*lastStrokes[i]), false);

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i < m; ++i) {
      TFrameId fid = fids[i];
      assert(firstFid <= fid && fid <= lastFid);
      TImageP img     = sl->getFrame(fid, true);
      double t        = m > 1 ? (double)i / (double)(m - 1) : 0.5;
      t               = TInbetween::interpolation(t, algorithm);
      if (backward) t = 1 - t;

      if (app) app->getCurrentFrame()->setFid(fid);

      TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
      for (int j = 0; j < vi->getStrokeCount(); j++) {
        TStroke *stroke = new TStroke(*vi->getStroke(j));
        addStrokeToImage(img.getPointer(), stroke);
      }
    }
    TUndoManager::manager()->endBlock();
  }

  void processSequence(std::vector<TStroke *> firstStrokes,
                       std::vector<TStroke *> lastStrokes, int firstFidx,
                       int lastFidx, int multi) {
    bool backwardidx = false;
    if (firstFidx > lastFidx) {
      std::swap(firstFidx, lastFidx);
      backwardidx = true;
    }

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
    if (multi == 2) {  // EASE_IN_INTERPOLATION)
      algorithm = TInbetween::EaseInInterpolation;
    } else if (multi == 3) {  // EASE_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseOutInterpolation;
    } else if (multi == 4) {  // EASE_IN_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseInOutInterpolation;
    }

    TVectorImageP firstImage = new TVectorImage();
    TVectorImageP lastImage  = new TVectorImage();

    for (int i = 0; i < firstStrokes.size(); i++)
      firstImage->addStroke(new TStroke(*firstStrokes[i]), false);

    for (int i = 0; i < lastStrokes.size(); i++)
      lastImage->addStroke(new TStroke(*lastStrokes[i]), false);

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i < m; i++) {
      row           = cellList[i].first;
      TXshCell cell = cellList[i].second;
      TFrameId fid  = cell.getFrameId();
      TImageP img   = cell.getImage(true);
      if (!img) continue;
      double t           = m > 1 ? (double)i / (double)(m - 1) : 1.0;
      t                  = TInbetween::interpolation(t, algorithm);
      if (backwardidx) t = 1 - t;

      if (app) app->getCurrentFrame()->setFrame(row);

      TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
      for (int j = 0; j < vi->getStrokeCount(); j++) {
        TStroke *stroke = new TStroke(*vi->getStroke(j));
        addStrokeToImage(img.getPointer(), stroke);
      }
    }
    TUndoManager::manager()->endBlock();
  }

  void addStrokeToImage(TImage *image, TStroke *stroke) {
    if (!stroke) return;

    TToonzImageP ti(image);
    TVectorImageP vi(image);
    TRasterImageP ri(image);

    TStroke::OutlineOptions &options = stroke->outlineOptions();
    options.m_capStyle               = m_param.m_capStyle.getIndex();
    options.m_joinStyle              = isMyPaintStyleSelected() && (ti || ri)
                              ? m_param.m_modifierJoinStyle.getIndex()
                              : m_param.m_joinStyle.getIndex();
    options.m_miterUpper = m_param.m_miterJoinLimit.getValue();

    TXshSimpleLevel *sl =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TFrameId id = getCurrentFid();

    /*-- ToonzImageの場合 --*/
    if (ti) {
      int styleId    = TTool::getApplication()->getCurrentLevelStyleIndex();
      bool selective = m_param.m_selective.getValue();

      bool filled = false;

      stroke->setStyle(styleId);
      double hardness = m_param.m_hardness.getValue() * 0.01;
      TRect savebox;
      if (isMyPaintStyleSelected()) {
        double modifierSize     = m_param.m_modifierSize.getValue();
        double modifierPressure = m_param.m_modifierPressure.getValue();
        TMyPaintBrushStyle *mypaintStyle = dynamic_cast<TMyPaintBrushStyle *>(
            getApplication()->getCurrentLevelStyle());
        bool isMiterJoin = m_param.m_modifierJoinStyle.getValue() == MITER_WSTR;
        TRasterCM32P ras = ti->getRaster();

        savebox = drawMyPaintBrushCM32(
            ras, stroke, modifierSize, modifierPressure, isMiterJoin,
            mypaintStyle, sl, id, m_isFrameCreated, m_isLevelCreated,
            m_primitive->getName());
      } else if (hardness == 1 || m_param.m_pencil.getValue()) {
        TUndoManager::manager()->add(new UndoRasterPencil(
            sl, id, stroke, selective, filled, !m_param.m_pencil.getValue(),
            m_isFrameCreated, m_isLevelCreated, m_primitive->getName()));
        savebox = ToonzImageUtils::addInkStroke(ti, stroke, styleId, selective,
                                                filled, TConsts::infiniteRectD,
                                                !m_param.m_pencil.getValue());
      } else {
        int thickness = m_param.m_rasterToolSize.getValue();
        TUndoManager::manager()->add(new CMBluredPrimitiveUndo(
            sl, id, stroke, thickness, hardness, selective, false,
            m_isFrameCreated, m_isLevelCreated, m_primitive->getName()));
        savebox = drawBluredBrush(ti, stroke, thickness, hardness, selective);
      }
      ToolUtils::updateSaveBox();
      delete stroke;
    }
    /*-- VectorImageの場合 --*/
    else if (vi) {
      int addedStrokeIndex = -1;
      bool isSpline        = false;
      if (TTool::getApplication()->getCurrentObject()->isSpline()) {
        isSpline = true;
        // if (!ToolUtils::isJustCreatedSpline(vi.getPointer())) {
        //  m_primitive->setIsPrompting(true);
        //  QString question("Are you sure you want to replace the motion
        //  path?");
        //  int ret =
        //      DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"),
        //      0);
        //  m_primitive->setIsPrompting(false);
        //  if (ret == 2 || ret == 0) return;
        //}
        QMutexLocker lock(vi->getMutex());
        TUndo *undo = new UndoPath(
            getXsheet()->getStageObject(getObjectId())->getSpline());
        while (vi->getStrokeCount() > 0) vi->deleteStroke(0);
        vi->addStroke(stroke, false);
        TUndoManager::manager()->add(undo);
      } else {
        int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
        if (styleId >= 0) stroke->setStyle(styleId);
        QMutexLocker lock(vi->getMutex());
        std::vector<TFilledRegionInf> *fillInformation =
            new std::vector<TFilledRegionInf>;
        ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                         stroke->getBBox());

        addedStrokeIndex =
            vi->addStroke(stroke, true, m_param.m_sendToBack.getValue() > 0);

        TUndoManager::manager()->add(new UndoPencil(
            stroke, fillInformation, sl, id, m_isFrameCreated, m_isLevelCreated,
            m_param.m_autogroup.getValue(), m_param.m_autofill.getValue(),
            m_param.m_sendToBack.getValue() > 0));

        if ((Preferences::instance()->getGuidedDrawingType() == 1 ||
             Preferences::instance()->getGuidedDrawingType() == 2) &&
            Preferences::instance()->getGuidedAutoInbetween()) {
          TTool *tool =
              TTool::getTool(T_Brush, TTool::ToolTargetType::VectorImage);
          ToonzVectorBrushTool *vbTool = (ToonzVectorBrushTool *)tool;
          if (vbTool) {
            vbTool->setViewer(m_viewer);
            vbTool->doGuidedAutoInbetween(id, vi, stroke, false,
                                          m_param.m_autogroup.getValue(),
                                          m_param.m_autofill.getValue(), false,
                                          m_param.m_sendToBack.getValue() > 0);
          }
        }
      }
      if (m_param.m_autogroup.getValue() && stroke->isSelfLoop()) {
        int index = vi->getStrokeCount() - 1;
        if (m_param.m_sendToBack.getValue() > 0 && !isSpline)
          index = addedStrokeIndex;
        vi->group(index, 1);
        if (m_param.m_autofill.getValue()) {
          // to avoid filling other strokes, I enter into the new stroke group
          int currentGroup = vi->exitGroup();
          vi->enterGroup(index);
          vi->selectFill(stroke->getBBox().enlarge(1, 1), 0, stroke->getStyle(),
                         false, true, false);
          if (currentGroup != -1)
            vi->enterGroup(currentGroup);
          else
            vi->exitGroup();
        }
      }
    }
    /*-- RasterImageの場合 --*/
    else if (ri) {
      int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();
      stroke->setStyle(styleId);
      double opacity  = m_param.m_opacity.getValue() * 0.01;
      double hardness = m_param.m_hardness.getValue() * 0.01;
      TRect savebox;

      if (isMyPaintStyleSelected()) {
        double modifierSize     = m_param.m_modifierSize.getValue();
        double modifierPressure = m_param.m_modifierPressure.getValue();
        double modifierOpacity  = m_param.m_opacity.getValue();
        bool modifierEraser     = m_param.m_modifierEraser.getValue();
        bool isMiterJoin = m_param.m_modifierJoinStyle.getValue() == MITER_WSTR;

        TMyPaintBrushStyle *mypaintStyle = dynamic_cast<TMyPaintBrushStyle *>(
            getApplication()->getCurrentLevelStyle());

        TRasterP ras = ri->getRaster();

        savebox = drawMyPaintBrushFullColor(
            ras, ri->getPalette(), stroke, modifierSize, modifierPressure,
            isMiterJoin, modifierOpacity, modifierEraser, mypaintStyle, sl, id,
            m_isFrameCreated, m_isLevelCreated, m_primitive->getName());

        delete m_tileSaverFullColor;
        m_tileSaverFullColor = 0;
      } else if (hardness == 1) {
        TUndoManager::manager()->add(new UndoFullColorPencil(
            sl, id, stroke, opacity, true, m_isFrameCreated, m_isLevelCreated));
        savebox = TRasterImageUtils::addStroke(ri, stroke, TRectD(), opacity);
      } else {
        int thickness = m_param.m_rasterToolSize.getValue();
        TUndoManager::manager()->add(new FullColorBluredPrimitiveUndo(
            sl, id, stroke, thickness, hardness, opacity, true,
            m_isFrameCreated, m_isLevelCreated));
        savebox = drawBluredBrush(ri, stroke, thickness, hardness, opacity);
      }
      ToolUtils::updateSaveBox();
      delete stroke;
    }
    notifyImageChanged();
    m_active = false;
  }

  TRect drawMyPaintBrushCM32(TRasterCM32P ras, TStroke *stroke,
                             double modifierSize, double modifierPressure,
                             bool isMiterJoin, TMyPaintBrushStyle *mypaintStyle,
                             TXshSimpleLevel *level, TFrameId frameId,
                             bool isFrameCreated, bool isLevelCreated,
                             std::string primitiveName) {
    TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
    m_tileSaverCM32       = new TTileSaverCM32(ras, tileSet);
    m_workRaster          = TRaster32P(ras->getSize());

    TStroke *s       = new TStroke(*stroke);
    TPointD riCenter = ras->getCenterD();
    s->transform(TTranslation(riCenter));
    int styleId = s->getStyle();

    TRasterCM32P backupRas = ras->clone();
    m_workRaster->lock();
    m_workRaster->clear();

    mypaint::Brush mypaintBrush;
    {  // applyToonzBrushSettings
      mypaintBrush.fromBrush(mypaintStyle->getBrush());
      double size = modifierSize * log(2.0);
      float baseSize =
          mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                                baseSize + size);
    }

    MyPaintToonzBrush *toonz_brush =
        new MyPaintToonzBrush(m_workRaster, *this, mypaintBrush);

    // When drawing strokes, these shapes allow for hard angles.  To do this,
    // we'll draw stroke in sections as if there are multiple strokes
    bool drawHardAngles =
        isMiterJoin &&
        (primitiveName == "Rectangle" || primitiveName == "Polyline" ||
         primitiveName == "MultiArc" || primitiveName == "Polygon");

    // For closed shapes, MyPaint will draw the stroke with rounded angles by
    // default except at start/end points. Shift starting point to 2nd point of
    // stroke so 1st point will be rounded also
    bool shiftStartPoint = s->isSelfLoop() && !drawHardAngles;

    toonz_brush->beginStroke();

    m_strokeRect.empty();
    m_lastRect.empty();

    double pressure = 0.5 + modifierPressure;

    int cpCount = s->getControlPointCount();
    double t;
    TPointD pos;
    TRect updateRect;
    int i = shiftStartPoint ? 1 : 0;
    for (; i < cpCount; i++) {
      t   = s->getParameterAtControlPoint(i);
      pos = s->getPoint(t);
      m_strokeSegmentRect.empty();
      toonz_brush->strokeTo(pos, pressure, 10.0);
      updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty())
        toonz_brush->updateDrawing(ras, backupRas, m_strokeSegmentRect, styleId,
                                   false);
      m_lastRect = m_strokeRect;

      if (drawHardAngles) {
        m_strokeSegmentRect.empty();
        toonz_brush->endStroke();
        updateRect = m_strokeSegmentRect * ras->getBounds();
        if (!updateRect.isEmpty())
          toonz_brush->updateDrawing(ras, backupRas, m_strokeSegmentRect,
                                     styleId, false);
        toonz_brush->beginStroke();
        toonz_brush->strokeTo(pos, pressure, 10.0);
      }
    }
    if (shiftStartPoint) {
      i   = 1;
      t   = s->getParameterAtControlPoint(i);
      pos = s->getPoint(t);
      m_strokeSegmentRect.empty();
      toonz_brush->strokeTo(pos, pressure, 10.0);
      updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty())
        toonz_brush->updateDrawing(ras, backupRas, m_strokeSegmentRect, styleId,
                                   false);
      m_lastRect = m_strokeRect;
    }

    m_strokeSegmentRect.empty();
    toonz_brush->endStroke();
    updateRect = m_strokeSegmentRect * ras->getBounds();
    if (!updateRect.isEmpty())
      toonz_brush->updateDrawing(ras, backupRas, m_strokeSegmentRect, styleId,
                                 false);

    m_workRaster->unlock();

    if (!m_strokeRect.isEmpty()) {
      TRasterCM32P subras = ras->extract(m_strokeRect)->clone();
      TUndoManager::manager()->add(new UndoMyPaintBrushCM32(
          tileSet, level, frameId, isFrameCreated, isLevelCreated, subras,
          m_strokeRect.getP00(), primitiveName));
    }

    delete toonz_brush;
    delete s;
    delete m_tileSaverCM32;
    m_tileSaverCM32 = 0;

    return m_strokeRect;
  }

  TRect drawMyPaintBrushFullColor(TRasterP ras, TPalette *palette,
                                  TStroke *stroke, double modifierSize,
                                  double modifierPressure, bool isMiterJoin,
                                  double modifierOpacity, bool modifierEraser,
                                  TMyPaintBrushStyle *mypaintStyle,
                                  TXshSimpleLevel *level, TFrameId frameId,
                                  bool isFrameCreated, bool isLevelCreated,
                                  std::string primitiveName) {
    TTileSetFullColor *tileSet = new TTileSetFullColor(ras->getSize());
    m_tileSaverFullColor       = new TTileSaverFullColor(ras, tileSet);
    m_workRaster               = TRaster32P(ras->getSize());

    TStroke *s       = new TStroke(*stroke);
    TPointD riCenter = ras->getCenterD();
    s->transform(TTranslation(riCenter));
    int styleId = s->getStyle();

    mypaint::Brush mypaintBrush;
    {  // applyToonzBrushSettings

      double size    = modifierSize * log(2.0);
      double opacity = 0.01 * modifierOpacity;

      TColorStyle *colorStyle = palette->getStyle(styleId);
      TPixel32 currentColor   = colorStyle->getMainColor();

      TPixelD color = PixelConverter<TPixelD>::from(currentColor);
      double colorH = 0.0;
      double colorS = 0.0;
      double colorV = 0.0;
      RGB2HSV(color.r, color.g, color.b, &colorH, &colorS, &colorV);

      mypaintBrush.fromBrush(mypaintStyle->getBrush());

      float baseSize =
          mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                                baseSize + size);

      float baseOpacity =
          mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE);
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE,
                                baseOpacity * opacity);

      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_H, colorH / 360.0);
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_S, colorS);
      mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_V, colorV);

      if (modifierEraser)
        mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_ERASER, 1.0);
    }

    TRasterP backupRas = ras->clone();

    m_workRaster->lock();
    m_workRaster->clear();

    MyPaintToonzBrush *toonz_brush =
        new MyPaintToonzBrush(m_workRaster, *this, mypaintBrush);

    // When drawing strokes, these shapes allow for hard angles.  To do this,
    // we'll draw stroke in sections as if there are multiple strokes
    bool drawHardAngles =
        isMiterJoin &&
        (primitiveName == "Rectangle" || primitiveName == "Polyline" ||
         primitiveName == "MultiArc" || primitiveName == "Polygon");

    // For closed shapes, MyPaint will draw the stroke with rounded angles by
    // default except at start/end points. Shift starting point to 2nd point of
    // stroke so 1st point will be rounded also
    bool shiftStartPoint = s->isSelfLoop() && !drawHardAngles;

    toonz_brush->beginStroke();

    m_strokeRect.empty();
    m_lastRect.empty();

    double pressure = 0.5 + modifierPressure;

    int cpCount = s->getControlPointCount();
    double t;
    TPointD pos;
    TRect updateRect;
    int i = shiftStartPoint ? 1 : 0;
    for (; i < cpCount; i++) {
      t   = s->getParameterAtControlPoint(i);
      pos = s->getPoint(t);
      m_strokeSegmentRect.empty();
      toonz_brush->strokeTo(pos, pressure, 10.0);
      updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty())
        ras->extract(updateRect)->copy(m_workRaster->extract(updateRect));

      if (drawHardAngles) {
        toonz_brush->endStroke();
        updateRect = m_strokeSegmentRect * ras->getBounds();
        if (!updateRect.isEmpty())
          ras->extract(updateRect)->copy(m_workRaster->extract(updateRect));
        toonz_brush->beginStroke();
        toonz_brush->strokeTo(pos, pressure, 10.0);
      }
    }
    if (shiftStartPoint) {
      i   = 1;
      t   = s->getParameterAtControlPoint(i);
      pos = s->getPoint(t);
      m_strokeSegmentRect.empty();
      toonz_brush->strokeTo(pos, pressure, 10.0);
      updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty())
        ras->extract(updateRect)->copy(m_workRaster->extract(updateRect));
    }

    toonz_brush->endStroke();
    updateRect = m_strokeSegmentRect * ras->getBounds();
    if (!updateRect.isEmpty())
      ras->extract(updateRect)->copy(m_workRaster->extract(updateRect));

    m_workRaster->unlock();

    if (!m_strokeRect.isEmpty()) {
      TRasterP subras = ras->extract(m_strokeRect)->clone();
      TUndoManager::manager()->add(new UndoMyPaintBrushFullColor(
          tileSet, level, frameId, isFrameCreated, isLevelCreated, subras,
          m_strokeRect.getP00(), primitiveName));
    }

    delete toonz_brush;
    delete s;
    delete m_tileSaverFullColor;
    m_tileSaverFullColor = 0;

    return m_strokeRect;
  }

  void updateFullColorWorkRaster(const TRect &rect) {
    if (rect.isEmpty()) return;

    TRasterImageP ri = TImageP(getImage(false, 1));
    if (!ri) return;

    TRasterP ras = ri->getRaster();

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

      m_workRaster->extract(_rect)->copy(ras->extract(_rect));
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
        m_workRaster->extract(rects[i])->copy(ras->extract(rects[i]));
      }
    }

    m_lastRect = enlargedRect;
  }

  bool isMyPaintStyleSelected() override { return m_isMyPaintStyleSelected; }

  void onColorStyleChanged() {
    TTool::Application *app = getApplication();
    TMyPaintBrushStyle *mpbs =
        dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
    m_isMyPaintStyleSelected = (mpbs) ? true : false;
    if (m_primitive)
      m_primitive->setMyPaintStyleSelected(m_isMyPaintStyleSelected);
    getApplication()->getCurrentTool()->notifyToolChanged();
  }

  bool askRead(const TRect &rect) override { return askWrite(rect); }

  bool askWrite(const TRect &rect) override {
    if (rect.isEmpty()) return true;

    m_strokeRect += rect;
    m_strokeSegmentRect += rect;

    if (m_tileSaverCM32)
      m_tileSaverCM32->save(rect);
    else if (m_tileSaverFullColor) {
      updateFullColorWorkRaster(rect);
      m_tileSaverFullColor->save(rect);
    }
    return true;
  }
};

GeometricTool GeometricVectorTool(TTool::Vectors | TTool::EmptyTarget);
GeometricTool GeometricRasterTool(TTool::ToonzImage | TTool::EmptyTarget);
GeometricTool GeometricRasterFullColorTool(TTool::RasterImage |
                                           TTool::EmptyTarget);

//-------------------------------------------------------------------------------------------------------------

GeometricToolNotifier::GeometricToolNotifier(GeometricTool *tool)
    : m_tool(tool) {
  if (TTool::Application *app = m_tool->getApplication()) {
    if (TPaletteHandle *paletteHandle = app->getCurrentPalette()) {
      bool ret;
      ret = connect(paletteHandle, SIGNAL(colorStyleChanged(bool)), this,
                    SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(colorStyleSwitched()), this,
                           SLOT(onColorStyleChanged()));
      ret = ret && connect(paletteHandle, SIGNAL(paletteSwitched()), this,
                           SLOT(onColorStyleChanged()));
    }
  }
  onColorStyleChanged();
}

//-------------------------------------------------------------------------------------------------------------

void GeometricToolNotifier::onColorStyleChanged() {
  m_tool->onColorStyleChanged();
}

//-------------------------------------------------------------------------------------------------------------

void Primitive::drawSnap() {
  // snapping

  if ((m_param->m_targetType & TTool::Vectors) && m_doSnap) {
    m_param->m_pixelSize = m_tool->getPixelSize();
    double thick         = 6.0 * m_param->m_pixelSize;
    if (m_param->m_foundSnap) {
      tglColor(TPixelD(0.1, 0.9, 0.1));
      tglDrawCircle(m_param->m_snapPoint, thick);
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

TPointD Primitive::checkGuideSnapping(TPointD pos, const TMouseEvent &e) {
  if (Preferences::instance()->getVectorSnappingTarget() == 0) {
    if (m_param->m_foundSnap)
      return m_param->m_snapPoint;
    else
      return pos;
  }
  m_doSnap = (m_param->m_snap.getValue() &&
              !(e.isCtrlPressed() && e.isShiftPressed())) ||
             (!m_param->m_snap.getValue() &&
              (e.isCtrlPressed() && e.isShiftPressed()));
  if ((m_param->m_targetType & TTool::Vectors) && m_doSnap) {
    int vGuideCount = 0, hGuideCount = 0;
    double guideDistance  = sqrt(m_param->m_minDistance2);
    TTool::Viewer *viewer = m_tool->getViewer();
    if (viewer) {
      vGuideCount = viewer->getVGuideCount();
      hGuideCount = viewer->getHGuideCount();
    }
    double distanceToVGuide = -1.0, distanceToHGuide = -1.0;
    double vGuide, hGuide;
    bool useGuides = false;
    if (vGuideCount) {
      for (int j = 0; j < vGuideCount; j++) {
        double guide        = viewer->getVGuide(j);
        double tempDistance = std::abs(guide - pos.y);
        if (tempDistance < guideDistance &&
            (distanceToVGuide < 0 || tempDistance < distanceToVGuide)) {
          distanceToVGuide = tempDistance;
          vGuide           = guide;
          useGuides        = true;
        }
      }
    }
    if (hGuideCount) {
      for (int j = 0; j < hGuideCount; j++) {
        double guide        = viewer->getHGuide(j);
        double tempDistance = std::abs(guide - pos.x);
        if (tempDistance < guideDistance &&
            (distanceToHGuide < 0 || tempDistance < distanceToHGuide)) {
          distanceToHGuide = tempDistance;
          hGuide           = guide;
          useGuides        = true;
        }
      }
    }
    if (useGuides && m_param->m_foundSnap) {
      double currYDistance = std::abs(m_param->m_snapPoint.y - pos.y);
      double currXDistance = std::abs(m_param->m_snapPoint.x - pos.x);
      double hypotenuse =
          sqrt(pow(currYDistance, 2.0) + pow(currXDistance, 2.0));
      if ((distanceToVGuide >= 0 && distanceToVGuide < hypotenuse) ||
          (distanceToHGuide >= 0 && distanceToHGuide < hypotenuse))
        useGuides = true;
      else
        useGuides = false;
    }
    if (useGuides) {
      assert(distanceToHGuide >= 0 || distanceToVGuide >= 0);
      if (distanceToHGuide < 0 ||
          (distanceToVGuide <= distanceToHGuide && distanceToVGuide >= 0)) {
        m_param->m_snapPoint.y = vGuide;
        m_param->m_snapPoint.x = pos.x;

      } else {
        m_param->m_snapPoint.y = pos.y;
        m_param->m_snapPoint.x = hGuide;
      }
      m_param->m_foundSnap = true;
    }
    if (m_param->m_foundSnap)
      return m_param->m_snapPoint;
    else
      return pos;
  } else
    return pos;
}

//=============================================================================
// Rectangle Primitive Class Implementation
//-----------------------------------------------------------------------------

void RectanglePrimitive::draw() {
  drawSnap();

  if (m_isEditing || m_isPrompting ||
      areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1)) {
//    tglColor(m_isEditing ? m_color : TPixel32::Green);
    tglColor(TPixel32::Red);

    if (m_rectangle.hasSymmetryBrushes()) {
      double pixelSize = m_tool->getPixelSize();
      m_rectangle.drawRectangle(TPixel32::Red, pixelSize);
      return;
    }

    glBegin(GL_LINE_LOOP);
    tglVertex(m_selectingRect.getP00());
    tglVertex(m_selectingRect.getP01());
    tglVertex(m_selectingRect.getP11());
    tglVertex(m_selectingRect.getP10());
    glEnd();
  }
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    // app->getCurrentTool()->getTool()->touchImage();
    const TColorStyle *style = app->getCurrentLevelStyle();
    m_isEditing              = style != 0 && style->isStrokeStyle();
    m_color = (style) ? style->getAverageColor() : TPixel32::Black;
  }

  if (!m_isEditing) return;
  TPointD newPos = getSnap(pos);
  if (m_param->m_pencil.getValue() &&
      (m_param->m_targetType & TTool::ToonzImage ||
       m_param->m_targetType & TTool::RasterImage)) {
    if (m_param->m_rasterToolSize.getValue() % 2 != 0)
      m_startPoint = TPointD((int)pos.x, (int)pos.y);
    else
      m_startPoint = TPointD((int)pos.x + 0.5, (int)pos.y + 0.5);
  } else
    m_startPoint     = newPos;
  m_selectingRect.x0 = m_startPoint.x;
  m_selectingRect.y0 = m_startPoint.y;
  m_selectingRect.x1 = m_startPoint.x;
  m_selectingRect.y1 = m_startPoint.y;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = m_tool->getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (!app->getCurrentObject()->isSpline() && symmetryTool &&
      symmetryTool->isGuideEnabled()) {
    m_rectangle.reset();
    m_rectangle.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                   symmObj.getCenterPoint(),
                                   symmObj.isUsingLineSymmetry(), dpiScale);
    m_rectangle.setRectangle(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                             TPointD(m_selectingRect.x1, m_selectingRect.y1));
  }
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::leftButtonDrag(const TPointD &realPos,
                                        const TMouseEvent &e) {
  if (!m_isEditing) return;

  TPointD pos;
  if (e.isShiftPressed() && !e.isCtrlPressed()) {
    double distance = tdistance(realPos, m_startPoint) * M_SQRT1_2;
    pos.x           = (realPos.x > m_startPoint.x) ? m_startPoint.x + distance
                                         : m_startPoint.x - distance;
    pos.y = (realPos.y > m_startPoint.y) ? m_startPoint.y + distance
                                         : m_startPoint.y - distance;
  } else {
    pos = calculateSnap(realPos, e);
    pos = checkGuideSnapping(realPos, e);
  }

  if (m_param->m_pencil.getValue() &&
      (m_param->m_targetType & TTool::ToonzImage ||
       m_param->m_targetType & TTool::RasterImage)) {
    if (m_param->m_rasterToolSize.getValue() % 2 != 0)
      pos = TPointD((int)pos.x, (int)pos.y);
    else
      pos = TPointD((int)pos.x + 0.5, (int)pos.y + 0.5);
  }

  m_selectingRect.x1 = pos.x;
  m_selectingRect.y1 = pos.y;
  if (!e.isAltPressed()) {
    m_selectingRect.x0 = m_startPoint.x;
    m_selectingRect.y0 = m_startPoint.y;
  } else {
    m_selectingRect.x0 = m_startPoint.x + m_startPoint.x - pos.x;
    m_selectingRect.y0 = m_startPoint.y + m_startPoint.y - pos.y;
  }

  if (m_rectangle.hasSymmetryBrushes()) {
    m_rectangle.clear();
    m_rectangle.setRectangle(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                             TPointD(m_selectingRect.x1, m_selectingRect.y1));
  }
}

//-----------------------------------------------------------------------------

std::vector<TStroke *> RectanglePrimitive::getStrokes() {
  std::vector<TStroke *> strokes;

  TStroke *stroke = makeStroke(0);
  strokes.push_back(stroke);

  for (int i = 1; i < m_rectangle.getBrushCount(); i++) {
    stroke = makeStroke(i);
    if (!stroke) continue;
    strokes.push_back(stroke);
  }

  return strokes;
}

//-----------------------------------------------------------------------------

TStroke *RectanglePrimitive::makeStroke(int index) {
  if (areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1))
    return 0;

  TRectD selArea;
  selArea.x0 = std::min(m_selectingRect.x0, m_selectingRect.x1);
  selArea.y0 = std::min(m_selectingRect.y0, m_selectingRect.y1);
  selArea.x1 = std::max(m_selectingRect.x0, m_selectingRect.x1);
  selArea.y1 = std::max(m_selectingRect.y0, m_selectingRect.y1);

  double thick = getThickness();

  TStroke *stroke = 0;
  if (m_param->m_targetType & TTool::Vectors) {
    std::vector<TThickPoint> points(17);

    points[0] = TThickPoint(selArea.x1, selArea.y1, thick);
    points[1] = TThickPoint(selArea.x1, selArea.y1, thick) + TPointD(-0.01, 0);

    points[3] = TThickPoint(selArea.x0, selArea.y1, thick) + TPointD(0.01, 0);
    points[4] = TThickPoint(selArea.x0, selArea.y1, thick);
    points[5] = TThickPoint(selArea.x0, selArea.y1, thick) + TPointD(0, -0.01);

    points[7] = TThickPoint(selArea.x0, selArea.y0, thick) + TPointD(0, 0.01);
    points[8] = TThickPoint(selArea.x0, selArea.y0, thick);
    points[9] = TThickPoint(selArea.x0, selArea.y0, thick) + TPointD(0.01, 0);

    points[11] = TThickPoint(selArea.x1, selArea.y0, thick) + TPointD(-0.01, 0);
    points[12] = TThickPoint(selArea.x1, selArea.y0, thick);
    points[13] = TThickPoint(selArea.x1, selArea.y0, thick) + TPointD(0, 0.01);

    points[15] = points[0] + TPointD(0, -0.01);
    points[16] = points[0];

    points[2]  = 0.5 * (points[1] + points[3]);
    points[6]  = 0.5 * (points[5] + points[7]);
    points[10] = 0.5 * (points[9] + points[11]);
    points[14] = 0.5 * (points[13] + points[15]);

    stroke = new TStroke(points);
  } else if (m_param->m_targetType & TTool::ToonzImage ||
             m_param->m_targetType & TTool::RasterImage) {
    std::vector<TThickPoint> points(9);
    double middleX = (selArea.x0 + selArea.x1) * 0.5;
    double middleY = (selArea.y0 + selArea.y1) * 0.5;

    points[0] = TThickPoint(selArea.x1, selArea.y1, thick);
    points[1] = TThickPoint(middleX, selArea.y1, thick);
    points[2] = TThickPoint(selArea.x0, selArea.y1, thick);
    points[3] = TThickPoint(selArea.x0, middleY, thick);
    points[4] = TThickPoint(selArea.x0, selArea.y0, thick);
    points[5] = TThickPoint(middleX, selArea.y0, thick);
    points[6] = TThickPoint(selArea.x1, selArea.y0, thick);
    points[7] = TThickPoint(selArea.x1, middleY, thick);
    points[8] = points[0];
    stroke    = new TStroke(points);
  }
  stroke->setSelfLoop();

  if (index > 0 && m_rectangle.hasSymmetryBrushes()) {
    std::vector<TPointD> *brush = m_rectangle.getBrush(index);

    TPointD center = TPointD((selArea.x0 + selArea.x1) * 0.5,
                             (selArea.y0 + selArea.y1) * 0.5);

    double dx    = brush->at(1).x - brush->at(0).x;
    double dy    = brush->at(1).y - brush->at(0).y;
    double angle = std::atan2(dy, dx) / (3.14159 / 180) - 90;
    if (angle < 0) angle += 360;

    stroke->transform(TRotation(center, angle));
    stroke->transform(TTranslation(-center));

    center = TPointD((brush->at(0).x + brush->at(2).x) * 0.5,
                     (brush->at(0).y + brush->at(2).y) * 0.5);
    stroke->transform(TTranslation(center));
  }

  return stroke;
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;
  m_tool->addStroke();
  m_rectangle.reset();
  resetSnap();
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos, e);
  newPos         = checkGuideSnapping(pos, e);
  m_pos          = newPos;
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void RectanglePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// Circle Primitive Class Implementation
//-----------------------------------------------------------------------------

void CirclePrimitive::draw() {
  drawSnap();
  if (m_isEditing || m_isPrompting) {
//    tglColor(m_isEditing ? m_color : TPixel32::Green);

    if (m_circle.hasSymmetryBrushes()) {
      double pixelSize = m_tool->getPixelSize();
      m_circle.drawCircle(TPixel32::Red, pixelSize);
      return;
    }

    tglColor(TPixel32::Red);
    tglDrawCircle(m_centre, m_radius);
  }
}

//-----------------------------------------------------------------------------

void CirclePrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  m_pos    = getSnap(pos);
  m_centre = m_pos;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = m_tool->getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (!app->getCurrentObject()->isSpline() && symmetryTool &&
      symmetryTool->isGuideEnabled()) {
    m_circle.reset();
    m_circle.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                symmObj.getCenterPoint(),
                                symmObj.isUsingLineSymmetry(), dpiScale);
    m_circle.setCircle(m_centre, 0);
  }
}

//-----------------------------------------------------------------------------

void CirclePrimitive::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_isEditing) return;

  m_pos    = pos;
  m_pos    = calculateSnap(pos, e);
  m_pos    = checkGuideSnapping(pos, e);
  m_radius = tdistance(m_centre, m_pos);

  if (m_circle.hasSymmetryBrushes()) {
    m_circle.clear();
    m_circle.setCircle(m_centre, m_radius);
  }
}

//-----------------------------------------------------------------------------

std::vector<TStroke *> CirclePrimitive::getStrokes() {
  std::vector<TStroke *> strokes;

  TStroke *stroke = makeStroke(0);
  strokes.push_back(stroke);

  for (int i = 1; i < m_circle.getBrushCount(); i++) {
    stroke = makeStroke(i);
    if (!stroke) continue;
    strokes.push_back(stroke);
  }

  return strokes;
}

//-----------------------------------------------------------------------------

TStroke *CirclePrimitive::makeStroke(int index) {
  TPointD center = m_centre;

  if (index > 0 && m_circle.hasSymmetryBrushes())
    center = m_circle.getPoint(0, index);

  return makeEllipticStroke(getThickness(), center, m_radius, m_radius);
}

//-----------------------------------------------------------------------------

void CirclePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;
  if (isAlmostZero(m_radius)) return;

  m_tool->addStroke();
  m_circle.reset();
  m_radius = 0;
}

//-----------------------------------------------------------------------------

void CirclePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_pos = calculateSnap(pos, e);
  m_pos = checkGuideSnapping(pos, e);
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void CirclePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// MultiLine Primitive Class Implementation
//-----------------------------------------------------------------------------

void MultiLinePrimitive::addVertex(const TPointD &pos) {
  int count = m_vertex.size();
  // Inserisco il primo punto
  if (count == 0) {
    m_vertex.push_back(pos);
    return;
  }

  TPointD vertex = m_vertex.getPoint(count - 1);

  // Caso particolare in cui inizio una curva e la chiudo subito cliccando sul
  // punto di pertenza
  if (count == 1 && pos == vertex) {
    m_vertex.push_back(pos);
    m_vertex.push_back(pos);
    m_vertex.push_back(pos);
    return;
  }

  // Calcolo lo speedOut
  TPointD speedOutPoint;
  if (!m_speedMoved)  // Se non e' stato mosso lo speedOut devo calcolarlo e
                      // inserirlo
  {
    speedOutPoint = vertex + computeSpeed(vertex, pos, 0.01);
    m_vertex.push_back(speedOutPoint);
  } else {
    if (m_ctrlDown) {
      m_vertex.setPoint(
          count - 1, (m_vertex.getPoint(count - 2) +
                      computeSpeed(m_vertex.getPoint(count - 2), pos, 0.01)));
    }
    speedOutPoint = m_vertex.getPoint(count - 1);
  }

  // Calcolo lo speedIn
  TPointD speedInPoint = pos + computeSpeed(pos, speedOutPoint, 0.01);
  // Calcolo il "punto di mezzo" e lo inserisco
  TPointD middlePoint = 0.5 * (speedInPoint + speedOutPoint);

  // Inserisco il "punto di mezzo"
  m_vertex.push_back(middlePoint);
  // Inserisco lo speedIn
  m_vertex.push_back(speedInPoint);
  // Inserisco il nuovo punto
  m_vertex.push_back(pos);
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::moveSpeed(const TPointD &delta) {
  int count = m_vertex.size();
  assert(count > 0);
  TPointD lastPoint        = m_vertex.getPoint(count - 1);
  TPointD newSpeedOutPoint = lastPoint - delta;
  if (m_speedMoved) {
    m_vertex.setPoint(count - 1, newSpeedOutPoint);
    if (m_altDown) return;
  } else {
    m_vertex.push_back(newSpeedOutPoint);
    ++count;
  }
  if (count < 5) {
    assert(count == 2);
    return;
  }

  TPointD vertex = m_vertex.getPoint(count - 2);

  TPointD v(0, 0);
  if (newSpeedOutPoint != vertex) v = normalize(newSpeedOutPoint - vertex);

  double speedOut         = tdistance(newSpeedOutPoint, vertex);
  TPointD newSpeedInPoint = vertex - TPointD(speedOut * v.x, speedOut * v.y);

  m_vertex.setPoint(count - 3, newSpeedInPoint);
  if (tdistance(m_vertex.getPoint(count - 5), m_vertex.getPoint(count - 6)) <=
      0.02)
    // see ControlPointEditorStroke::isSpeedOutLinear() from
    // controlpointselection.cpp
    m_vertex.setPoint(count - 5,
                      (m_vertex.getPoint(count - 6) +
                       computeSpeed(m_vertex.getPoint(count - 6),
                                    m_vertex.getPoint(count - 3), 0.01)));
  m_vertex.setPoint(count - 4, (0.5 * (m_vertex.getPoint(count - 3) +
                                       m_vertex.getPoint(count - 5))));
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::draw() {
  UINT size = m_vertex.size();

  drawSnap();

  if ((m_isEditing || m_isPrompting) && size > 0) {
    TPixel color     = TPixel32::Red;
    double pixelSize = m_tool->getPixelSize();
    m_vertex.drawPolyline(m_mousePosition, color, pixelSize, m_speedMoved,
                          m_ctrlDown);
  }
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  if (!app->getCurrentObject()->isSpline() && m_vertex.size() == 0) {
    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    TPointD dpiScale       = m_tool->getViewer()->getDpiScale();
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();

    if (symmetryTool && symmetryTool->isGuideEnabled()) {
      m_vertex.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                  symmObj.getCenterPoint(),
                                  symmObj.isUsingLineSymmetry(), dpiScale);
    }
  }

  m_vertex.setJoinDistance(joinDistance);
  m_vertex.setAllowSpeed(true);

  m_undo = new MultilinePrimitiveUndo(m_vertex, this);
  TUndoManager::manager()->add(m_undo);
  m_mousePosition = pos;

  TPointD newPos;
  newPos = getSnap(pos);

  // Se clicco nell'ultimo vertice chiudo la linea.
  TPointD _pos       = pos;
  if (m_closed) _pos = m_vertex.getBrush()->front();

  if ((e.isShiftPressed() && !e.isCtrlPressed()) && !m_vertex.empty())
    addVertex(rectify(m_vertex.getBrush()->back(), _pos));
  else
    addVertex(newPos);
  m_undo->setNewVertex(m_vertex);

  m_beforeSpeedMoved = m_speedMoved;
  m_speedMoved       = false;
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonDrag(const TPointD &pos,
                                        const TMouseEvent &e) {
  if (m_vertex.size() == 0 || m_isSingleLine) return;
  TPointD newPos = pos;
  if (m_vertex.size() >= 2 && m_shiftDown)
    newPos = rectify(m_vertex.getPoint(m_vertex.size() - 2), pos);
  if (m_speedMoved ||
      tdistance2(m_vertex.getPoint(m_vertex.size() - 1), newPos) >
          sq(7.0 * m_tool->getPixelSize())) {
    moveSpeed(m_mousePosition - newPos);
    m_speedMoved = true;
    m_undo->setNewVertex(m_vertex);
    m_mousePosition = newPos;
  }
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonDoubleClick(const TPointD &,
                                               const TMouseEvent &e) {
  endLine();
  resetSnap();
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (m_closed) endLine();
  resetSnap();
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_ctrlDown  = e.isCtrlPressed() && !e.isShiftPressed();
  m_shiftDown = e.isShiftPressed();
  m_altDown   = e.isAltPressed();
  TPointD newPos;
  newPos = calculateSnap(pos, e);
  newPos = checkGuideSnapping(pos, e);

  if (m_isEditing) {
    if ((m_shiftDown && !e.isCtrlPressed()) && !m_vertex.empty())
      m_mousePosition = rectify(m_vertex.getBrush()->back(), newPos);
    else
      m_mousePosition = newPos;

    double dist = joinDistance * joinDistance;

    if (!m_vertex.empty() && (tdistance2(pos, m_vertex.getBrush()->front()) <
                              dist * m_tool->getPixelSize())) {
      m_closed        = true;
      m_mousePosition = m_vertex.getBrush()->front();
    } else
      m_closed = false;

  } else
    m_mousePosition = newPos;
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

bool MultiLinePrimitive::keyDown(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    endLine();
    return true;
  }

  if (event->key() != Qt::Key_Escape || !m_isEditing) return false;

  UINT size = m_vertex.size();
  if (size <= 1) return 0;

  if (!m_isSingleLine) TUndoManager::manager()->popUndo((size - 1) / 4 + 1);

  m_isEditing        = false;
  m_speedMoved       = false;
  m_beforeSpeedMoved = false;
  m_closed           = false;

  m_vertex.reset();
  return true;
}

//-----------------------------------------------------------------------------

std::vector<TStroke *> MultiLinePrimitive::getStrokes() {
  std::vector<TStroke *> strokes;

  TStroke *stroke = makeStroke(0);
  strokes.push_back(stroke);

  for (int i = 1; i < m_vertex.getBrushCount(); i++) {
    stroke = makeStroke(i);
    if (!stroke) continue;
    strokes.push_back(stroke);
  }

  return strokes;
}

//-----------------------------------------------------------------------------

TStroke *MultiLinePrimitive::makeStroke(int index) {
  double thick = getThickness();

  /*---
   * Pencilの場合は、線幅を減らす。Thickness1の線を1ピクセルにするため。（thick
   * = 0 になる）---*/
  if (m_param->m_pencil.getValue()) thick -= 1.0;

  UINT size = m_vertex.size();
  if (size <= 1) return 0;

  if (!m_isSingleLine && index == 0)
    TUndoManager::manager()->popUndo((size - 1) / 4 + 1);

  TStroke *stroke = 0;
  std::vector<TThickPoint> points;
  int i;
  for (i = 0; i < (int)size; i++) {
    TPointD vertex = m_vertex.getPoint(i, index);
    points.push_back(TThickPoint(vertex, thick));
  }
  stroke = new TStroke(points);
  if (m_closed) stroke->setSelfLoop();

  return stroke;
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::endLine() {
  if (!m_isEditing) return;

  m_isEditing        = false;
  m_speedMoved       = false;
  m_beforeSpeedMoved = false;

  if (!m_isSingleLine && !m_vertex.empty() &&
      m_vertex.size() % 4 != 1 /* && !m_rasterTool*/) {
    m_vertex.erase(--m_vertex.getBrush()->end());
    assert(m_vertex.size() == 3 || m_vertex.size() % 4 == 1);
  }

  m_tool->addStroke();

  if (m_closed) m_closed = false;

  m_vertex.reset();
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::onActivate() {
  m_isEditing = false;
  m_closed    = false;
  m_vertex.reset();
  m_speedMoved       = false;
  m_beforeSpeedMoved = false;
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//-----------------------------------------------------------------------------

void MultiLinePrimitive::onImageChanged() { onActivate(); }

//=============================================================================
// Line Primitive Class Implementation
//-----------------------------------------------------------------------------

void LinePrimitive::draw() {
  UINT size = m_vertex.size();

  drawSnap();

  if (m_isEditing || m_isPrompting) {
    double pixelSize = m_tool->getPixelSize();
    m_vertex.drawPolyline(m_mousePosition, TPixel32::Red, pixelSize);
  }
}

//-----------------------------------------------------------------------------

void LinePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos, e);
  newPos         = checkGuideSnapping(pos, e);
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void LinePrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color     = TPixel32::Red;
    m_isEditing = true;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = m_tool->getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (!app->getCurrentObject()->isSpline() && symmetryTool &&
      symmetryTool->isGuideEnabled()) {
    m_vertex.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                symmObj.getCenterPoint(),
                                symmObj.isUsingLineSymmetry(), dpiScale);
  }

  m_vertex.setDrawStartCircle(false);

  TPointD newPos = getSnap(pos);

  m_mousePosition = newPos;

  TPointD _pos = newPos;

  if (m_param->m_pencil.getValue() &&
      (m_param->m_targetType & TTool::ToonzImage ||
       m_param->m_targetType & TTool::RasterImage)) {
    if (m_param->m_rasterToolSize.getValue() % 2 != 0)
      _pos = TPointD((int)newPos.x, (int)newPos.y);
    else
      _pos = TPointD((int)newPos.x + 0.5, (int)newPos.y + 0.5);
  }

  if (m_vertex.size() == 0)
    addVertex(_pos);
  else {
    if ((e.isShiftPressed() && !e.isCtrlPressed()) && !m_vertex.empty())
      addVertex(rectify(m_vertex.getBrush()->back(), pos));
    else
      addVertex(_pos);
    endLine();
  }
}

//-----------------------------------------------------------------------------

void LinePrimitive::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_isEditing) return;
  TPointD newPos = calculateSnap(pos, e);
  newPos         = checkGuideSnapping(pos, e);

  m_mousePosition = newPos;
}
//-----------------------------------------------------------------------------

void LinePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  // snapping
  TPointD newPos = getSnap(pos);

  m_mousePosition = newPos;
  if ((e.isShiftPressed() && !e.isCtrlPressed()) && !m_vertex.empty())
    m_vertex.push_back(rectify(m_vertex.getBrush()->back(), pos));
  else
    m_vertex.push_back(newPos);

  endLine();

  resetSnap();
}
//-----------------------------------------------------------------------------

//=============================================================================
// Ellipse Primitive Class Implementation
//-----------------------------------------------------------------------------

void EllipsePrimitive::draw() {
  drawSnap();
  if (m_isEditing || m_isPrompting ||
      areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1)) {
    if (m_ellipse.hasSymmetryBrushes()) {
      double pixelSize = m_tool->getPixelSize();
      m_ellipse.drawEllipse(TPixel32::Red, pixelSize);
      m_ellipse.drawRectangle(TPixel32::Red, pixelSize);
      return;
    }

//    tglColor(m_isEditing ? m_color : TPixel32::Green);
    tglColor(TPixel32::Red);
    TPointD centre = TPointD((m_selectingRect.x0 + m_selectingRect.x1) * 0.5,
                             (m_selectingRect.y0 + m_selectingRect.y1) * 0.5);

    glPushMatrix();
    tglMultMatrix(TScale(centre, m_selectingRect.x1 - m_selectingRect.x0,
                         m_selectingRect.y1 - m_selectingRect.y0));
    tglDrawCircle(centre, 0.5);

    glPopMatrix();
    drawRect(m_selectingRect, m_color, 0x5555, true);
  }
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_isEditing = true;
    m_color     = TPixel32::Red;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  TPointD newPos     = getSnap(pos);
  m_startPoint       = newPos;
  m_selectingRect.x0 = newPos.x;
  m_selectingRect.y0 = newPos.y;
  m_selectingRect.x1 = newPos.x;
  m_selectingRect.y1 = newPos.y;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = m_tool->getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (!app->getCurrentObject()->isSpline() && symmetryTool &&
      symmetryTool->isGuideEnabled()) {
    m_ellipse.reset();
    m_ellipse.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                 symmObj.getCenterPoint(),
                                 symmObj.isUsingLineSymmetry(), dpiScale);
    m_ellipse.setEllipse(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                         TPointD(m_selectingRect.x1, m_selectingRect.y1));
  }
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::leftButtonDrag(const TPointD &realPos,
                                      const TMouseEvent &e) {
  if (!m_isEditing) return;

  TPointD pos;
  if (e.isShiftPressed() && !e.isCtrlPressed()) {
    double distance = tdistance(realPos, m_startPoint) * M_SQRT1_2;
    pos.x           = (realPos.x > m_startPoint.x) ? m_startPoint.x + distance
                                         : m_startPoint.x - distance;
    pos.y = (realPos.y > m_startPoint.y) ? m_startPoint.y + distance
                                         : m_startPoint.y - distance;
  } else {
    pos = calculateSnap(realPos, e);
    pos = checkGuideSnapping(realPos, e);
  }
  m_pos = pos;

  m_selectingRect.x1 = pos.x;
  m_selectingRect.y1 = pos.y;
  if (!e.isAltPressed()) {
    m_selectingRect.x0 = m_startPoint.x;
    m_selectingRect.y0 = m_startPoint.y;
  } else {
    m_selectingRect.x0 = m_startPoint.x + m_startPoint.x - pos.x;
    m_selectingRect.y0 = m_startPoint.y + m_startPoint.y - pos.y;
  }

  if (m_ellipse.hasSymmetryBrushes()) {
    m_ellipse.clear();
    m_ellipse.setEllipse(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                         TPointD(m_selectingRect.x1, m_selectingRect.y1));
  }
}

//-----------------------------------------------------------------------------

std::vector<TStroke *> EllipsePrimitive::getStrokes() {
  std::vector<TStroke *> strokes;

  TStroke *stroke = makeStroke(0);
  strokes.push_back(stroke);

  for (int i = 1; i < m_ellipse.getBrushCount(); i++) {
    stroke = makeStroke(i);
    if (!stroke) continue;
    strokes.push_back(stroke);
  }

  return strokes;
}

//-----------------------------------------------------------------------------

TStroke *EllipsePrimitive::makeStroke(int index) {
  if (areAlmostEqual(m_selectingRect.x0, m_selectingRect.x1) ||
      areAlmostEqual(m_selectingRect.y0, m_selectingRect.y1))
    return 0;

  TStroke *stroke;
  stroke = makeEllipticStroke(
      getThickness(), TPointD(0.5 * (m_selectingRect.x0 + m_selectingRect.x1),
                              0.5 * (m_selectingRect.y0 + m_selectingRect.y1)),
      fabs(0.5 * (m_selectingRect.x1 - m_selectingRect.x0)),
      fabs(0.5 * (m_selectingRect.y1 - m_selectingRect.y0)));

  if (index > 0 && m_ellipse.hasSymmetryBrushes()) {
    std::vector<TPointD> *brush = m_ellipse.getBrush(index);

    TPointD center = TPointD((m_selectingRect.x0 + m_selectingRect.x1) * 0.5,
                             (m_selectingRect.y0 + m_selectingRect.y1) * 0.5);

    double dx    = brush->at(1).x - brush->at(0).x;
    double dy    = brush->at(1).y - brush->at(0).y;
    double angle = std::atan2(dy, dx) / (3.14159 / 180) - 90;
    if (angle < 0) angle += 360;

    stroke->transform(TRotation(center, angle));
    stroke->transform(TTranslation(-center));

    center = TPointD((brush->at(0).x + brush->at(2).x) * 0.5,
                     (brush->at(0).y + brush->at(2).y) * 0.5);
    stroke->transform(TTranslation(center));
  }

  return stroke;
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;

  m_tool->addStroke();
  m_ellipse.reset();
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  m_pos = calculateSnap(pos, e);
  m_pos = checkGuideSnapping(pos, e);
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void EllipsePrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_color = TPixel32::Red;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// Arc Primitive Class Implementation
//-----------------------------------------------------------------------------

void MultiArcPrimitive::draw() {
  drawSnap();

  double pixelSize = m_tool->getPixelSize();

  switch (m_clickNumber) {
  case 1:
    if (m_arc.hasSymmetryBrushes()) {
      double pixelSize = m_tool->getPixelSize();
      m_arc.drawArc(m_endPoint, TPixel32::Red, pixelSize);
    } else {
      tglColor(m_color);
      tglDrawSegment(m_startPoint, m_endPoint);
    }

    if (m_stroke) {
      drawStrokeCenterline(*m_stroke, sqrt(tglGetPixelSize2()));
      TPointD firstPoint = m_stroke->getControlPoint(0);
      if (firstPoint == m_endPoint) {
        tglColor(TPixel32((m_color.r + 127) % 255, m_color.g,
                          (m_color.b + 127) % 255, m_color.m));
      }
      tglDrawCircle(m_stroke->getControlPoint(0), joinDistance * pixelSize);
    }

    break;

  case 2:
//    tglColor(m_isPrompting ? TPixel32::Green : m_color);
    tglColor(TPixel32::Red);
    if (!m_isPrompting) {
      if (m_arc.hasSymmetryBrushes()) {
        double pixelSize = m_tool->getPixelSize();
        m_arc.drawArc(m_centralPoint, TPixel32::Red, pixelSize);
      } else {
        glLineStipple(1, 0x5555);
        glEnable(GL_LINE_STIPPLE);
        glBegin(GL_LINE_STRIP);
        tglVertex(m_startPoint);
        tglVertex(m_centralPoint);
        tglVertex(m_endPoint);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
      }
    }

    if (m_stroke) drawStrokeCenterline(*m_stroke, sqrt(tglGetPixelSize2()));

    if (m_strokeTemp)
      drawStrokeCenterline(*m_strokeTemp, sqrt(tglGetPixelSize2()));

    if (m_stroke) {
      TPointD firstPoint = m_stroke->getControlPoint(0);
      if (firstPoint == m_endPoint) {
        tglColor(TPixel32((m_color.r + 127) % 255, m_color.g,
                          (m_color.b + 127) % 255, m_color.m));
      }
      tglDrawCircle(m_stroke->getControlPoint(0), joinDistance * pixelSize);
    }
    break;
  };
}

//-----------------------------------------------------------------------------

std::vector<TStroke *> MultiArcPrimitive::getStrokes() {
  std::vector<TStroke *> strokes;

  TStroke *stroke = makeStroke(0);
  strokes.push_back(stroke);

  for (int i = 1; i < m_arc.getBrushCount(); i++) {
    stroke = makeStroke(i);
    if (!stroke) continue;
    strokes.push_back(stroke);
  }

  return strokes;
}

//-----------------------------------------------------------------------------

TStroke *MultiArcPrimitive::makeStroke(int index) {
  if (index == 0) return new TStroke(*m_stroke);

  std::vector<TThickPoint> points;
  double thick = getThickness();

  for (int i = 0; i < m_arc.size();) {
    if ((i + 3) > m_arc.size()) break;
    TThickQuadratic q(m_arc.getPoint(i, index),
                      TThickPoint(m_arc.getPoint(i + 2, index), thick),
                      m_arc.getPoint(i + 1, index));
    TThickQuadratic q0, q1, q00, q01, q10, q11;

    q.split(0.5, q0, q1);
    q0.split(0.5, q00, q01);
    q1.split(0.5, q10, q11);

    if (i == 0) points.push_back(TThickPoint(m_arc.getPoint(i, index), thick));
    points.push_back(TThickPoint(q00.getP1(), thick));
    points.push_back(TThickPoint(q00.getP2(), thick));
    points.push_back(TThickPoint(q01.getP1(), thick));
    points.push_back(TThickPoint(q01.getP2(), thick));
    points.push_back(TThickPoint(q10.getP1(), thick));
    points.push_back(TThickPoint(q10.getP2(), thick));
    points.push_back(TThickPoint(q11.getP1(), thick));
    points.push_back(TThickPoint(m_arc.getPoint(i + 1, index), thick));

    i += 3;
  }

  TStroke *stroke = new TStroke(points);

  return stroke;
}

//-----------------------------------------------------------------------------

void MultiArcPrimitive::leftButtonDown(const TPointD &pos,
                                       const TMouseEvent &e) {
  TTool::Application *app = TTool::getApplication();

  if (m_clickNumber == 0) {
    TPointD newPos = calculateSnap(pos, e);
    newPos         = checkGuideSnapping(pos, e);
    m_startPoint   = newPos;

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    TPointD dpiScale       = m_tool->getViewer()->getDpiScale();
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();

    if (!app->getCurrentObject()->isSpline() && symmetryTool &&
        symmetryTool->isGuideEnabled()) {
      m_arc.reset();
      m_arc.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                               symmObj.getCenterPoint(),
                               symmObj.isUsingLineSymmetry(), dpiScale);
      m_arc.push_back(m_startPoint);
    }
 
    m_arc.setJoinDistance(joinDistance);
  }
}

//-----------------------------------------------------------------------------

void MultiArcPrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  TPointD newPos = getSnap(pos);

  std::vector<TThickPoint> points(9);
  double thick     = getThickness();
  double dist      = joinDistance * joinDistance;
  bool strokeAdded = false;

  MultiArcPrimitiveUndo *undo =
      new MultiArcPrimitiveUndo(this, m_stroke, m_strokeTemp, m_startPoint,
                                m_endPoint, m_centralPoint, m_clickNumber);

  if (app->getCurrentObject()->isSpline()) {
    m_isEditing = true;
    m_color     = TPixel32::Red;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  switch (m_clickNumber) {
  case 0:
    m_endPoint = newPos;

    if (!m_isEditing) return;

    m_clickNumber++;
    break;

  case 1:
    if (m_arc.hasSymmetryBrushes()) {
      m_arc.push_back(m_endPoint);
    }

    m_centralPoint = newPos;
    points[0]      = TThickPoint(m_startPoint, thick);
    points[8]      = TThickPoint(m_endPoint, thick);
    points[4]      = TThickPoint(0.5 * (points[0] + points[8]), thick);
    points[2]      = TThickPoint(0.5 * (points[0] + points[4]), thick);
    points[6]      = TThickPoint(0.5 * (points[4] + points[8]), thick);

    points[1]    = TThickPoint(0.5 * (points[0] + points[2]), thick);
    points[3]    = TThickPoint(0.5 * (points[2] + points[4]), thick);
    points[5]    = TThickPoint(0.5 * (points[4] + points[6]), thick);
    points[7]    = TThickPoint(0.5 * (points[6] + points[8]), thick);
    m_strokeTemp = new TStroke(points);
    m_clickNumber++;
    break;

  case 2:
    if (m_arc.hasSymmetryBrushes()) {
      m_arc.push_back(m_centralPoint);
    }

    m_startPoint = newPos;
    if (!m_isSingleArc) {
      m_clickNumber = 1;
      if (m_stroke) {
        TVectorImageP vi = new TVectorImage();
        vi->addStroke(m_stroke);
        vi->addStroke(m_strokeTemp);
        m_strokeTemp = 0;
        vi->joinStroke(0, 1, m_stroke->getControlPointCount() - 1, 0,
                       getSmooth());

        m_stroke           = new TStroke(*vi->getStroke(0));
        int count          = m_stroke->getControlPointCount();
        TPointD firstPoint = m_stroke->getControlPoint(0);
        TPointD lastPoint  = m_stroke->getControlPoint(count - 1);
        m_startPoint       = lastPoint;
        if (firstPoint == lastPoint) {
          vi->joinStroke(0, 0, 0, m_stroke->getControlPointCount() - 1,
                         getSmooth());
          delete m_stroke;
          m_stroke = new TStroke(*vi->getStroke(0));
          TUndoManager::manager()->popUndo(m_undoCount);
          m_undoCount = 0;
          m_tool->addStroke();
          m_arc.reset();
          onDeactivate();
          strokeAdded = true;
        }
      } else {
        m_stroke     = m_strokeTemp;
        m_strokeTemp = 0;
        m_startPoint = m_endPoint;
      }
      if (m_arc.hasSymmetryBrushes()) {
        m_arc.push_back(m_startPoint);
      }
    } else {
      m_stroke     = m_strokeTemp;
      m_strokeTemp = 0;
      TUndoManager::manager()->popUndo(m_undoCount);
      m_undoCount = 0;
      m_tool->addStroke();
      m_arc.reset();
      onDeactivate();
      strokeAdded = true;
    }
    break;
  }

  if (strokeAdded) {
    delete undo;
  } else {
    undo->setRedoData(m_stroke, m_strokeTemp, m_startPoint, m_endPoint,
                      m_centralPoint, m_clickNumber);
    TUndoManager::manager()->add(undo);
    ++m_undoCount;
  }

  resetSnap();
}

//-----------------------------------------------------------------------------

void MultiArcPrimitive::leftButtonDoubleClick(const TPointD &,
                                              const TMouseEvent &e) {
  if (m_stroke) {
    TUndoManager::manager()->popUndo(m_undoCount);
    m_undoCount = 0;
    m_tool->addStroke();
    m_arc.reset();
  }
  onDeactivate();
}

//-----------------------------------------------------------------------------

bool MultiArcPrimitive::keyDown(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    if (m_stroke) {
      TUndoManager::manager()->popUndo(m_undoCount);
      m_undoCount = 0;
      m_tool->addStroke();
      m_arc.reset();
    }
    onDeactivate();
    return true;
  }
  return false;
}

//-----------------------------------------------------------------------------

void MultiArcPrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos, e);
  newPos         = checkGuideSnapping(pos, e);

  double dist = joinDistance * joinDistance;

  switch (m_clickNumber) {
  case 0:
    m_startPoint = newPos;
    break;
  case 1:
    if (e.isShiftPressed() && !e.isCtrlPressed())
      m_endPoint = rectify(m_startPoint, pos);
    else
      m_endPoint = newPos;

    if (m_stroke) {
      TPointD firstPoint = m_stroke->getControlPoint(0);
      if (tdistance2(m_endPoint, firstPoint) < dist * m_tool->getPixelSize())
        m_endPoint = firstPoint;
    }
    break;
  case 2:
    m_centralPoint = newPos;
    TThickQuadratic q(m_startPoint, TThickPoint(m_centralPoint, getThickness()),
                      m_endPoint);
    TThickQuadratic q0, q1, q00, q01, q10, q11;

    q.split(0.5, q0, q1);
    q0.split(0.5, q00, q01);
    q1.split(0.5, q10, q11);

    assert(q00.getP2() == q01.getP0());
    assert(q01.getP2() == q10.getP0());
    assert(q10.getP2() == q11.getP0());

    m_strokeTemp->setControlPoint(1, q00.getP1());
    m_strokeTemp->setControlPoint(2, q00.getP2());
    m_strokeTemp->setControlPoint(3, q01.getP1());
    m_strokeTemp->setControlPoint(4, q01.getP2());
    m_strokeTemp->setControlPoint(5, q10.getP1());
    m_strokeTemp->setControlPoint(6, q10.getP2());
    m_strokeTemp->setControlPoint(7, q11.getP1());
    break;
  }
  m_tool->invalidate();
}

//-----------------------------------------------------------------------------

void MultiArcPrimitive::onEnter() {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline())
    m_color = TPixel32::Red;
  else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) m_color       = style->getAverageColor();
  }
}

//=============================================================================
// Polygon Primitive Class Declaration
//-----------------------------------------------------------------------------

void PolygonPrimitive::draw() {
  drawSnap();
  if (!m_isEditing && !m_isPrompting) return;

  if (m_polygon.hasSymmetryBrushes()) {
    double pixelSize = m_tool->getPixelSize();
    m_polygon.drawPolygon(TPixel32::Red, pixelSize);
    return;
  }

//  tglColor(m_isEditing ? m_color : TPixel32::Green);
  tglColor(TPixel32::Red);

  int edgeCount = m_param->m_edgeCount.getValue();
  if (edgeCount == 0) return;

  double angleDiff = M_2PI / edgeCount;
  double angle     = (3 * M_PI + angleDiff) * 0.5;

  glBegin(GL_LINE_LOOP);
  for (int i = 0; i < edgeCount; i++) {
    tglVertex(m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius));
    angle += angleDiff;
  }
  glEnd();
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::leftButtonDown(const TPointD &pos, const TMouseEvent &) {
  TTool::Application *app = TTool::getApplication();
  if (!app) return;

  if (app->getCurrentObject()->isSpline()) {
    m_isEditing = true;
    m_color     = TPixel32::Red;
  } else {
    const TColorStyle *style = app->getCurrentLevelStyle();
    if (style) {
      m_isEditing = style->isStrokeStyle();
      m_color     = style->getAverageColor();
    } else {
      m_isEditing = false;
      m_color     = TPixel32::Black;
    }
  }

  if (!m_isEditing) return;

  m_centre = getSnap(pos);
  m_radius = 0;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = m_tool->getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (!app->getCurrentObject()->isSpline() && symmetryTool &&
      symmetryTool->isGuideEnabled()) {
    m_polygon.reset();
    m_polygon.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                 symmObj.getCenterPoint(),
                                 symmObj.isUsingLineSymmetry(), dpiScale);
    m_polygon.setPolygon(m_centre, m_radius, m_param->m_edgeCount.getValue());
  }
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::leftButtonDrag(const TPointD &pos,
                                      const TMouseEvent &e) {
  if (!m_isEditing) return;
  TPointD newPos = calculateSnap(pos, e);
  newPos         = checkGuideSnapping(pos, e);
  m_radius       = tdistance(m_centre, newPos);

  if (m_polygon.hasSymmetryBrushes()) {
    m_polygon.clear();
    m_polygon.setPolygon(m_centre, m_radius, m_param->m_edgeCount.getValue());
  }
}

//-----------------------------------------------------------------------------

std::vector<TStroke *> PolygonPrimitive::getStrokes() {
  std::vector<TStroke *> strokes;

  TStroke *stroke = makeStroke(0);
  strokes.push_back(stroke);

  for (int i = 1; i < m_polygon.getBrushCount(); i++) {
    stroke = makeStroke(i);
    if (!stroke) continue;
    strokes.push_back(stroke);
  }

  return strokes;
}

//-----------------------------------------------------------------------------

TStroke *PolygonPrimitive::makeStroke(int index) {
  double thick = getThickness();

  int edgeCount = m_param->m_edgeCount.getValue();
  if (edgeCount == 0) return 0;

  double angleDiff = M_2PI / (double)edgeCount;
  double angle     = (3 * M_PI + angleDiff) * 0.5;

  TStroke *stroke = 0;
  if (m_param->m_targetType & TTool::Vectors) {
    std::vector<TThickPoint> points(4 * edgeCount + 1);
    int i;
    // Posiziono gli angoli
    for (i = 0; i <= (int)points.size(); i += 4) {
      points[i] = TThickPoint(
          m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius),
          thick);
      angle += angleDiff;
    }
    // posiziono i punti medi e i punti per gestire la linearita'
    for (i = 0; i < (int)points.size() - 1; i += 4) {
      TPointD vertex           = convert(points[i]);
      TPointD nextVertex       = convert(points[i + 4]);
      TPointD speed            = computeSpeed(vertex, nextVertex, 0.01);
      TPointD speedOutPoint    = vertex + speed;
      TPointD speedInNextPoint = nextVertex - speed;
      TPointD middlePoint      = 0.5 * (speedOutPoint + speedInNextPoint);
      points[i + 1]            = TThickPoint(speedOutPoint, thick);
      points[i + 2]            = TThickPoint(middlePoint, thick);
      points[i + 3]            = TThickPoint(speedInNextPoint, thick);
    }
    stroke = new TStroke(points);
  } else if (m_param->m_targetType & TTool::ToonzImage ||
             m_param->m_targetType & TTool::RasterImage) {
    std::vector<TThickPoint> points(edgeCount + edgeCount + 1);
    points[0] = TThickPoint(
        m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius),
        thick);
    for (int i = 1; i <= edgeCount; i++) {
      angle += angleDiff;
      points[i + i] = TThickPoint(
          m_centre + TPointD(cos(angle) * m_radius, sin(angle) * m_radius),
          thick);
      points[i + i - 1] = (points[i + i - 2] + points[i + i]) * 0.5;
    }
    stroke = new TStroke(points);
  }
  stroke->setSelfLoop();

  if (index > 0 && m_polygon.hasSymmetryBrushes()) {
    std::vector<TPointD> *brush = m_polygon.getBrush(index);

    TPointD center = m_polygon.getPoint(0, 0);

    double dx    = brush->at(2).x - brush->at(1).x;
    double dy    = brush->at(2).y - brush->at(1).y;
    double angle = std::atan2(dy, dx) / (3.14159 / 180) - 90;
    if (angle < 0) angle += 360;

    stroke->transform(TRotation(center, angle));
    stroke->transform(TTranslation(-center));

    center = brush->at(0);
    stroke->transform(TTranslation(center));
  }

  return stroke;
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::leftButtonUp(const TPointD &pos, const TMouseEvent &) {
  if (!m_isEditing) return;
  m_isEditing = false;

  m_tool->addStroke();
  m_radius = 0;
  m_polygon.reset();
}

//-----------------------------------------------------------------------------

void PolygonPrimitive::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  TPointD newPos = calculateSnap(pos, e);
  newPos         = checkGuideSnapping(pos, e);
  m_tool->invalidate();
}
