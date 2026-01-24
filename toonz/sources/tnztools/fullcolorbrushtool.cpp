

#include "fullcolorbrushtool.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/cursors.h"
#include "tools/toolutils.h"
#include "tools/toolhandle.h"
#include "tools/tooloptions.h"

#include "bluredbrush.h"
#include "mypainttoonzbrush.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tframehandle.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/tstageobject.h"
#include "toonz/palettecontroller.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/preferences.h"
#include "toonz/toonzfolders.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tstageobjectcmd.h"

// TnzCore includes
#include "tgl.h"
#include "tproperty.h"
#include "trasterimage.h"
#include "tenv.h"
#include "tpalette.h"
#include "trop.h"
#include "tstream.h"
#include "tstroke.h"
#include "timagecache.h"
#include "tpixelutils.h"
#include "tsystem.h"

#include "perspectivetool.h"

// Qt includes
#include <QCoreApplication>  // Qt translation support

#define DEFAULTPRESSURECURVE                                                   \
  QList<TPointD> {                                                             \
    TPointD(-40.0, 0.0), TPointD(-20.0, 0.0), TPointD(-20.0, 0.0),             \
        TPointD(0.0, 0.0), TPointD(6.3, 6.3), TPointD(93.7, 93.7),             \
        TPointD(100.0, 100.0), TPointD(120.0, 100.0), TPointD(120.0, 100.0),   \
        TPointD(140.0, 100.0)                                                  \
  }

#define DEFAULTSIZETILTCURVE                                                   \
  QList<TPointD> {                                                             \
    TPointD(-10.0, 100.0), TPointD(10.0, 100.0), TPointD(10.0, 100.0),         \
        TPointD(30.0, 100.0), TPointD(34.0, 93.0), TPointD(86.0, 7.0),         \
        TPointD(90.0, 0.0), TPointD(110.0, 0.0), TPointD(110.0, 0.0),          \
        TPointD(130.0, 0.0)                                                    \
  }

#define DEFAULTOPACITYTILTCURVE                                                \
  QList<TPointD> {                                                             \
    TPointD(-10.0, 0.0), TPointD(10.0, 0.0), TPointD(10.0, 0.0),               \
        TPointD(30.0, 0.0), TPointD(34.0, 7.0), TPointD(86.0, 93.0),           \
        TPointD(90.0, 100.0), TPointD(110.0, 100.0), TPointD(110.0, 100.0),    \
        TPointD(130.0, 100.0)                                                  \
  }

//----------------------------------------------------------------------------------

TEnv::DoubleVar FullcolorBrushMinSize("FullcolorBrushMinSize", 1);
TEnv::DoubleVar FullcolorBrushMaxSize("FullcolorBrushMaxSize", 5);
TEnv::DoubleVar FullcolorBrushSmooth("FullcolorBrushSmooth", 0);
TEnv::DoubleVar FullcolorBrushHardness("FullcolorBrushHardness", 100);
TEnv::DoubleVar FullcolorMinOpacity("FullcolorMinOpacity", 0);
TEnv::DoubleVar FullcolorMaxOpacity("FullcolorMaxOpacity", 100);
TEnv::DoubleVar FullcolorModifierSize("FullcolorModifierSize", 0);
TEnv::DoubleVar FullcolorModifierOpacity("FullcolorModifierOpacity", 100);
TEnv::IntVar FullcolorModifierEraser("FullcolorModifierEraser", 0);
TEnv::IntVar FullcolorModifierLockAlpha("FullcolorModifierLockAlpha", 0);
TEnv::IntVar FullcolorModifierPaintBehind("FullcolorModifierPaintBehind", 0);
TEnv::StringVar FullcolorBrushPreset("FullcolorBrushPreset", "<custom>");
TEnv::IntVar FullcolorBrushSnapGrid("FullcolorBrushSnapGrid", 0);
TEnv::IntVar FullcolorPressureSensitivity("FullcolorPressureSensitivity", 1);
TEnv::PointListVar FullcolorPressureCurvePts("FullcolorPressureCurvePts",
                                             DEFAULTPRESSURECURVE);
TEnv::IntVar FullcolorTiltSensitivity("FullcolorTiltSensitivity", 0);
TEnv::PointListVar FullcolorTiltCurvePts("FullcolorTiltCurvePts",
                                         DEFAULTSIZETILTCURVE);
TEnv::IntVar FullcolorOPressureSensitivity("FullcolorOPressureSensitivity", 0);
TEnv::PointListVar FullcolorOPressureCurvePts("FullcolorOPressureCurvePts",
                                              DEFAULTPRESSURECURVE);
TEnv::IntVar FullcolorOTiltSensitivity("FullcolorOTiltSensitivity", 0);
TEnv::PointListVar FullcolorOTiltCurvePts("FullcolorOTiltCurvePts",
                                          DEFAULTOPACITYTILTCURVE);
TEnv::IntVar FullcolorMyPaintPressureSensitivity(
    "FullcolorMyPaintPressureSensitivity", 1);
TEnv::IntVar FullcolorMyPaintTiltSensitivity("FullcolorMyPaintTiltSensitivity",
                                             0);
TEnv::DoubleVar FullcolorBrushTipSpacing("FullcolorBrushTipSpacing", 1);
TEnv::DoubleVar FullcolorBrushTipRotation("FullcolorBrushTipRotation", 0);
TEnv::IntVar FullcolorBrushTipAutoRotate("FullcolorBrushTipAutoRotate", 1);
TEnv::IntVar FullcolorBrushTipFlipHorizontal("FullcolorBrushTipFlipHorizontal",
                                             0);
TEnv::IntVar FullcolorBrushTipFlipVertical("FullcolorBrushTipFlipVertical", 0);
TEnv::DoubleVar FullcolorBrushTipScatter("FullcolorBrushTipScatter", 0);
TEnv::StringVar FullcolorBrushTipId("FullcolorBrushTipId", DEFAULTBRUSHTIPID);

//----------------------------------------------------------------------------------

#define CUSTOM_WSTR L"<custom>"

//----------------------------------------------------------------------------------

namespace {
double computeThickness(bool pressureEnabled, double pressure, bool tiltEnabled,
                        double tiltMagnitude,
                        const TDoublePairProperty &property) {
  double thick0 = property.getValue().first;
  double thick1 = property.getValue().second;
  if (thick1 < 0.0001) thick0 = thick1 = 0.0;

  double pThick = 0.0, tThick = 0.0;

  if (pressureEnabled) {
    double t = pressure * pressure * pressure;
    pThick   = thick0 + (thick1 - thick0) * t;
    if (tiltEnabled) pThick *= 0.5;
  }

  if (tiltEnabled) {
    double t = tiltMagnitude * tiltMagnitude * tiltMagnitude;
    tThick   = thick0 + (thick1 - thick0) * t;
    if (pressureEnabled) tThick *= 0.5;
  }

  return pThick + tThick;
}

//----------------------------------------------------------------------------------

class FullColorBrushUndo final : public ToolUtils::TFullColorRasterUndo {
  TPoint m_offset;
  QString m_id;

public:
  FullColorBrushUndo(TTileSetFullColor *tileSet, TXshSimpleLevel *level,
                     const TFrameId &frameId, bool isFrameCreated,
                     const TRasterP &ras, const TPoint &offset)
      : ToolUtils::TFullColorRasterUndo(tileSet, level, frameId, isFrameCreated,
                                        false, 0)
      , m_offset(offset) {
    static int counter = 0;

    m_id = QString("FullColorBrushUndo") + QString::number(counter++);
    TImageCache::instance()->add(m_id.toStdString(), TRasterImageP(ras));
  }

  ~FullColorBrushUndo() { TImageCache::instance()->remove(m_id); }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TRasterImageP image = getImage();
    TRasterP ras        = image->getRaster();

    TRasterImageP srcImg =
        TImageCache::instance()->get(m_id.toStdString(), false);
    ras->copy(srcImg->getRaster(), m_offset);

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) + ToolUtils::TFullColorRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Raster Brush Tool"); }
  int getHistoryType() override { return HistoryType::BrushTool; }
};

}  // namespace

//************************************************************************
//    FullColor Brush Tool implementation
//************************************************************************

FullColorBrushTool::FullColorBrushTool(std::string name)
    : TTool(name)
    , m_thickness("Size", 1, 1000, 1, 5, true)
    , m_smooth("Smooth:", 0, 50, 0)
    , m_mypaintPressure("ModifierPressure", true)
    , m_opacity("Opacity", 0, 100, 0, 100, true)
    , m_hardness("Hardness:", 0, 100, 100)
    , m_modifierSize("ModifierSize", -3, 3, 0, true)
    , m_modifierOpacity("ModifierOpacity", 0, 100, 100, true)
    , m_modifierEraser("ModifierEraser", false)
    , m_modifierLockAlpha("Lock Alpha", false)
    , m_modifierPaintBehind("Paint Behind", false)
    , m_preset("Preset:")
    , m_minCursorThick(0)
    , m_maxCursorThick(0)
    , m_enabledPressure(false)
    , m_toonz_brush(0)
    , m_tileSet(0)
    , m_tileSaver(0)
    , m_notifier(0)
    , m_presetsLoaded(false)
    , m_firstTime(true)
    , m_snapGrid("Grid", false)
    , m_mypaintTilt("ModifierTilt", false)
    , m_enabledTilt(false)
    , m_enabledOPressure(false)
    , m_enabledOTilt(false)
    , m_sizeStylusProperty("Stylus Settings - Size")
    , m_opacityStylusProperty("Stylus Settings - Opacity")
    , m_oldOpacity(1)
    , m_bluredBrush(0)
    , m_isMyPaintStyleSelected(false)
    , m_highFreqBrushTimer(0.0)
    , m_brushTip("Brush Tip")
    , m_enabled(false) {
  bind(TTool::RasterImage | TTool::EmptyTarget);

  m_thickness.setNonLinearSlider();

  m_sizeStylusProperty.setUseLinearCurves(false);
  m_sizeStylusProperty.setDefaultTiltCurve(DEFAULTSIZETILTCURVE);
  m_opacityStylusProperty.setUseLinearCurves(false);
  m_opacityStylusProperty.setDefaultTiltCurve(DEFAULTOPACITYTILTCURVE);

  m_prop.bind(m_brushTip);
  m_prop.bind(m_thickness);
  m_prop.bind(m_sizeStylusProperty);
  m_prop.bind(m_modifierSize);
  m_prop.bind(m_hardness);
  m_prop.bind(m_smooth);
  m_prop.bind(m_opacity);
  m_prop.bind(m_opacityStylusProperty);
  m_prop.bind(m_modifierOpacity);
  m_prop.bind(m_modifierEraser);
  m_prop.bind(m_modifierPaintBehind);
  m_prop.bind(m_modifierLockAlpha);
  m_prop.bind(m_mypaintPressure);
  m_prop.bind(m_mypaintTilt);
  m_prop.bind(m_snapGrid);
  m_prop.bind(m_preset);

  m_preset.setId("BrushPreset");
  m_modifierEraser.setId("RasterEraser");
  m_modifierPaintBehind.setId("PaintBehind");
  m_modifierLockAlpha.setId("LockAlpha");
  m_snapGrid.setId("SnapGrid");
  m_mypaintPressure.setId("PressureSensitivity");
  m_mypaintTilt.setId("TiltSensitivity");
  m_sizeStylusProperty.setId("SizeStylusConfig");
  m_opacityStylusProperty.setId("OpacityStylusConfig");
  m_brushTip.setId("BrushTip");

  m_brushTimer.start();
}

//---------------------------------------------------------------------------------------------------

ToolOptionsBox *FullColorBrushTool::createOptionsBox() {
  TPaletteHandle *currPalette =
      TTool::getApplication()->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = TTool::getApplication()->getCurrentTool();
  return new BrushToolOptionsBox(0, this, currPalette, currTool);
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onCanvasSizeChanged() {
  onDeactivate();
  setWorkAndBackupImages();
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onColorStyleChanged() {
  getApplication()->getCurrentTool()->notifyToolChanged();
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::updateTranslation() {
  m_thickness.setQStringName(tr("Size"));
  m_smooth.setQStringName(tr("Smooth:"));
  m_opacity.setQStringName(tr("Opacity"));
  m_hardness.setQStringName(tr("Hardness:"));
  m_preset.setQStringName(tr("Preset:"));
  m_modifierSize.setQStringName(tr("Size"));
  m_modifierOpacity.setQStringName(tr("Opacity"));
  m_modifierEraser.setQStringName(tr("Eraser"));
  m_modifierLockAlpha.setQStringName(tr("Lock Alpha"));
  m_modifierPaintBehind.setQStringName(tr("Paint Behind"));
  m_snapGrid.setQStringName(tr("Grid"));
  m_mypaintPressure.setQStringName(tr("Pressure"));
  m_mypaintTilt.setQStringName(tr("Tilt"));
  m_sizeStylusProperty.setQStringName(tr("Stylus Settings - Size"));
  m_opacityStylusProperty.setQStringName(tr("Stylus Settings - Opacity"));
  m_brushTip.setQStringName(tr("Brush Tip"));
}

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::onActivate() {
  if (!m_notifier) m_notifier = new FullColorBrushToolNotifier(this);

  updateCurrentStyle();

  if (m_firstTime) {
    m_firstTime = false;

    std::wstring wpreset =
        QString::fromStdString(FullcolorBrushPreset.getValue()).toStdWString();
    if (wpreset != CUSTOM_WSTR) {
      initPresets();
      if (!m_preset.isValue(wpreset)) wpreset = CUSTOM_WSTR;
      m_preset.setValue(wpreset);
      FullcolorBrushPreset = m_preset.getValueAsString();
      loadPreset();
    } else
      loadLastBrush();
  }

  m_brushPad = ToolUtils::getBrushPad(m_thickness.getValue().second,
                                      m_hardness.getValue() * 0.01);

  setWorkAndBackupImages();
  onColorStyleChanged();
}

//--------------------------------------------------------------------------------------------------

void FullColorBrushTool::onDeactivate() {
  if (m_mousePressed) leftButtonUp(m_mousePos, m_mouseEvent);
  m_workRaster = TRaster32P();
  m_backUpRas  = TRasterP();
  m_enabled    = false;
}

//--------------------------------------------------------------------------------------------------

void FullColorBrushTool::updateWorkAndBackupRasters(const TRect &rect) {
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

    if (m_isMyPaintStyleSelected) {
      if (!m_modifierPaintBehind.getValue())
        m_workRaster->extract(_rect)->copy(ras->extract(_rect));
    } else
      m_workRaster->extract(_rect)->clear();
    m_backUpRas->extract(_rect)->copy(ras->extract(_rect));
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
      if (m_isMyPaintStyleSelected) {
        if (!m_modifierPaintBehind.getValue())
          m_workRaster->extract(rects[i])->copy(ras->extract(rects[i]));
      } else
        m_workRaster->extract(rects[i])->clear();
      m_backUpRas->extract(rects[i])->copy(ras->extract(rects[i]));
    }
  }

  m_lastRect = enlargedRect;
}

//--------------------------------------------------------------------------------------------------

bool FullColorBrushTool::askRead(const TRect &rect) { return askWrite(rect); }

//--------------------------------------------------------------------------------------------------

bool FullColorBrushTool::askWrite(const TRect &rect) {
  if (rect.isEmpty()) return true;
  m_strokeRect += rect;
  m_strokeSegmentRect += rect;
  updateWorkAndBackupRasters(rect);
  m_tileSaver->save(rect);
  return true;
}

//--------------------------------------------------------------------------------------------------

bool FullColorBrushTool::preLeftButtonDown() {
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

//---------------------------------------------------------------------------------------------------

void FullColorBrushTool::leftButtonDown(const TPointD &pos,
                                        const TMouseEvent &e) {
  TPointD previousBrushPos = m_brushPos;
  m_brushPos = m_mousePos = pos;
  m_mousePressed          = true;
  m_mouseEvent            = e;
  Viewer *viewer          = getViewer();
  if (!viewer) return;

  TRasterImageP ri = (TRasterImageP)getImage(true);
  if (!ri) ri = (TRasterImageP)touchImage();

  if (!ri) return;

  TTool::Application *app = TTool::getApplication();
  if (!app) return;
  TXshLevel *level = app->getCurrentLevel()->getLevel();
  if (level == NULL) {
    m_active = false;
    return;
  }

  int col   = app->getCurrentColumn()->getColumnIndex();
  m_enabled = col >= 0 || app->getCurrentFrame()->isEditingLevel();
  if (!m_enabled) return;

  if ((e.isShiftPressed() || e.isCtrlPressed()) && !e.isAltPressed()) {
    m_isStraight = true;
    m_firstPoint = pos;
    m_lastPoint  = pos;
  }

  if (e.isAltPressed()) {
    m_isStraight = true;
    m_firstPoint = pos;
    m_lastPoint  = pos;
  }

  if (m_snapGrid.getValue()) {
    m_firstPoint = pos;
    m_lastPoint  = pos;
  }

  /* update color here since the current style might be switched with numpad
   * shortcut keys */
  updateCurrentStyle();

  TRasterP ras = ri->getRaster();

  if (!(m_workRaster && m_backUpRas)) setWorkAndBackupImages();

  m_workRaster->lock();

  if (!m_isMyPaintStyleSelected || m_modifierPaintBehind.getValue())
    m_workRaster->clear();

  TPointD rasCenter = ras->getCenterD();
  TPointD point(pos + rasCenter);

  if (!e.isTablet()) {
    m_enabledPressure  = false;
    m_enabledOPressure = false;
    m_enabledTilt      = false;
    m_enabledOTilt     = false;
  }

  double pressure, opressure;
  if (m_isMyPaintStyleSelected)  // mypaint brush case
    pressure = (m_enabledPressure || m_enabledOPressure) ? e.m_pressure : 0.5;
  else {
    pressure = (m_enabledPressure || m_enabledOPressure) ? e.m_pressure : 1.0;
    if ((m_enabledPressure || m_enabledOPressure) && e.m_pressure == 1.0)
      pressure = 0.1;
  }
  m_oldPressure = opressure = pressure;

  // Convert QTabletEvent tilt range (-60 to 60) to MyPaint range (-1.0 to 1.0)
  // Note: xTilt value sign must be flipped so it angles correctly
  double xTilt = (e.m_tiltX / -60.0);
  double yTilt = (e.m_tiltY / 60.0);
  // rotation of stylus
  double arctan = atan2(yTilt, xTilt);
  m_tiltAngle = (!e.isTablet() || (!xTilt && !yTilt)) ? 0.0 : arctan * M_180_PI;

  double tiltMagnitude, otiltMagnitude;
  double tiltX   = (m_enabledTilt || m_enabledOTilt) ? xTilt : 0.0;
  double tiltY   = (m_enabledTilt || m_enabledOTilt) ? yTilt : 0.0;
  otiltMagnitude = tiltMagnitude = std::sqrt(tiltX * tiltX + tiltY * tiltY);

  m_tileSet   = new TTileSetFullColor(ras->getSize());
  m_tileSaver = new TTileSaverFullColor(ras, m_tileSet);

  if (m_isMyPaintStyleSelected) {
    mypaint::Brush mypaintBrush;
    applyToonzBrushSettings(mypaintBrush);
    m_toonz_brush = new MyPaintToonzBrush(m_workRaster, *this, mypaintBrush);

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
    m_toonz_brush->strokeTo(point, pressure, tiltX, tiltY, restartBrushTimer());
    m_highFreqBrushTimer = 0.0;
    TRect updateRect = m_strokeSegmentRect * ras->getBounds();
    if (!updateRect.isEmpty()) {
      TRaster32P sourceRas = m_modifierPaintBehind.getValue()
                                 ? (TRaster32P)m_backUpRas
                                 : m_workRaster;
      m_toonz_brush->updateDrawing(ras, sourceRas, m_strokeSegmentRect,
                                   m_modifierPaintBehind.getValue());
    }

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
    TRectD invalidateRect = convert(m_strokeSegmentRect) - rasCenter;
    invalidateRect +=
        TRectD(m_brushPos - thickOffset, m_brushPos + thickOffset);
    invalidateRect +=
        TRectD(previousBrushPos - thickOffset, previousBrushPos + thickOffset);
    invalidate(invalidateRect.enlarge(2.0));
  } else {
    // Convert pressure value to % applied to max value
    if (m_enabledPressure)
      pressure =
          m_sizeStylusProperty.getOutputPressureForInput(pressure * 100.0) /
          100.0;

    // Convert tilt values to % applied to max value
    if (m_enabledTilt) {
      // Convert MyPaint magnitude range (0 to 1.0) to graph range (90 - 30)
      double oldTiltM = 90 - (tiltMagnitude * 60);
      tiltMagnitude =
          m_sizeStylusProperty.getOutputTiltForInput(oldTiltM) / 100.0;
    }

    // Convert pressure value to % applied to max value
    if (m_enabledOPressure)
      opressure =
          m_opacityStylusProperty.getOutputPressureForInput(opressure * 100.0) /
          100.0;

    // Convert tilt values to % applied to max value
    if (m_enabledOTilt) {
      // Convert MyPaint magnitude range (0 to 1.0) to graph range (90 - 30)
      double oldTiltM = 90 - (otiltMagnitude * 60);
      otiltMagnitude =
          m_opacityStylusProperty.getOutputTiltForInput(oldTiltM) / 100.0;
    }

    double maxThick = m_thickness.getValue().second;
    double thickness =
        (m_enabledPressure || m_enabledTilt)
            ? computeThickness(m_enabledPressure, pressure, m_enabledTilt,
                               tiltMagnitude, m_thickness)
            : maxThick;

    double opacity =
        ((m_enabledOPressure || m_enabledOTilt)
             ? computeThickness(m_enabledOPressure, opressure, m_enabledOTilt,
                                otiltMagnitude, m_opacity)
             : m_opacity.getValue().second) *
        0.01;
    TDimension dim    = ras->getSize();
    TPointD rasCenter = TPointD(dim.lx * 0.5, dim.ly * 0.5);
    TThickPoint point(
        pos + rasCenter, thickness,
        (m_brushTip.getBrushTip() && m_brushTip.isAutoRotate() ? m_tiltAngle
                                                               : 0));
    TPointD halfThick(maxThick * 0.5, maxThick * 0.5);
    TRectD invalidateRect(pos - halfThick, pos + halfThick);

    m_points.clear();
    m_points.push_back(point);

    double hardness = m_hardness.getValue() * 0.01;

    m_bluredBrush = new RasterBlurredBrush(
        m_workRaster, maxThick, m_brushPad, m_brushTip.getBrushTip(),
        m_brushTip.getSpacing(), m_brushTip.getRotation(),
        m_brushTip.isFlipHorizontal(), m_brushTip.isFlipVertical(),
        m_brushTip.getScatter());

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    if (symmetryTool && symmetryTool->isGuideEnabled()) {
      TPointD dpiScale       = getViewer()->getDpiScale();
      SymmetryObject symmObj = symmetryTool->getSymmetryObject();
      m_bluredBrush->addSymmetryBrushes(
          symmObj.getLines(), symmObj.getRotation(), symmObj.getCenterPoint(),
          symmObj.isUsingLineSymmetry(), dpiScale);
    }

    m_strokeRect = m_bluredBrush->getBoundFromPoints(m_points);
    updateWorkAndBackupRasters(m_strokeRect);
    m_tileSaver->save(m_strokeRect);
    m_bluredBrush->addPoint(point, opacity);
    m_bluredBrush->updateDrawing(ras, m_backUpRas, m_currentColor, m_strokeRect,
                                 opacity, m_modifierPaintBehind.getValue(),
                                 m_modifierLockAlpha.getValue());

    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0) {
      pts.push_back(point);
    } else {
      m_smoothStroke.beginStroke(m_smooth.getValue());
      m_smoothStroke.addPoint(point);
      m_smoothStroke.getSmoothPoints(pts);
    }

    m_oldOpacity = opacity;
    m_lastRect   = m_strokeRect;

    invalidate(invalidateRect.enlarge(2));
  }

  m_perspectiveIndex = -1;
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::leftButtonDrag(const TPointD &pos,
                                        const TMouseEvent &e) {
  if (!m_enabled) {
    m_brushPos = m_mousePos = pos;
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
        TTool::getTool("T_PerspectiveGrid", TTool::RasterImage));
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
    if (abs(angle) >= 82.5)
      m_lastPoint.x = m_firstPoint.x;
    else if (abs(angle) < 7.5)
      m_lastPoint.y = m_firstPoint.y;
    else {
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

  // keep this here for possible eventual variable width
  // if (getApplication()->getCurrentLevelStyle()->getTagId() ==
  //    4001)  // mypaint brush case
  //  m_oldPressure = m_enabledPressure && e.isTablet() ? e.m_pressure : 0.5;
  // else
  //  m_oldPressure = m_enabledPressure ? e.m_pressure : 1.0;

  if (m_isStraight) {
    m_mousePos = pos;
    m_brushPos = pos;
    invalidate(invalidateRect);
    return;
  }

  TPointD previousBrushPos = m_brushPos;
  m_brushPos = m_mousePos = pos;
  m_mouseEvent            = e;
  TRasterImageP ri        = (TRasterImageP)getImage(true);
  if (!ri) return;

  TRasterP ras      = ri->getRaster();
  TPointD rasCenter = ras->getCenterD();
  TPointD point(usePos + rasCenter);
  double pressure, opressure;
  if (m_isMyPaintStyleSelected)  // mypaint brush case
    pressure = (m_enabledPressure || m_enabledOPressure) ? e.m_pressure : 0.5;
  else
    pressure = (m_enabledPressure || m_enabledOPressure) ? e.m_pressure : 1.0;
  opressure = pressure;

  // Convert QTabletEvent tilt range (-60 to 60) to MyPaint range (-1.0 to 1.0)
  // Note: xTilt value sign must be flipped so it angles correctly
  double xTilt = (e.m_tiltX / -60.0);
  double yTilt = (e.m_tiltY / 60.0);
  // rotation of stylus
  double arctan = atan2(yTilt, xTilt);
  m_tiltAngle = (!e.isTablet() || (!xTilt && !yTilt)) ? 0.0 : arctan * M_180_PI;

  double tiltMagnitude, otiltMagnitude;
  double tiltX   = (m_enabledTilt || m_enabledOTilt) ? xTilt : 0.0;
  double tiltY   = (m_enabledTilt || m_enabledOTilt) ? yTilt : 0.0;
  otiltMagnitude = tiltMagnitude = std::sqrt(tiltX * tiltX + tiltY * tiltY);

  if (m_isMyPaintStyleSelected) {
    if (!m_toonz_brush) return;

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
    if ((brushTimer + m_highFreqBrushTimer) < 0.01) {
      m_highFreqBrushTimer += brushTimer;
      return;
    }
    brushTimer += m_highFreqBrushTimer;
    m_highFreqBrushTimer = 0.0;
    for (size_t i = 0; i < pts.size(); ++i) {
      const TThickPoint &thickPoint2 = pts[i];
      m_strokeSegmentRect.empty();
      m_toonz_brush->strokeTo(thickPoint2, thickPoint2.thick, tiltX, tiltY,
                              brushTimer);
      TRect updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty()) {
        TRaster32P sourceRas = m_modifierPaintBehind.getValue()
                                   ? (TRaster32P)m_backUpRas
                                   : m_workRaster;
        m_toonz_brush->updateDrawing(ras, sourceRas, m_strokeSegmentRect,
                                     m_modifierPaintBehind.getValue());
      }

      TPointD thickOffset(m_maxCursorThick * 0.5, m_maxCursorThick * 0.5);
      invalidateRect += convert(m_strokeSegmentRect) - rasCenter;
      invalidateRect +=
          TRectD(m_brushPos - thickOffset, m_brushPos + thickOffset);
      invalidateRect += TRectD(previousBrushPos - thickOffset,
                               previousBrushPos + thickOffset);
    }
    invalidate(invalidateRect.enlarge(2.0));
  } else {
    if (!m_bluredBrush) return;

    // Convert pressure value to % applied to max value
    if (m_enabledPressure)
      pressure =
          m_sizeStylusProperty.getOutputPressureForInput(pressure * 100.0) /
          100.0;

    // Convert tilt values to % applied to max value
    if (m_enabledTilt) {
      // Convert MyPaint magnitude range (0 to 1.0) to graph range (90 - 30)
      double oldTiltM = 90 - (tiltMagnitude * 60);
      tiltMagnitude =
          m_sizeStylusProperty.getOutputTiltForInput(oldTiltM) / 100.0;
    }

    // Convert pressure value to % applied to max value
    if (m_enabledOPressure)
      opressure =
          m_opacityStylusProperty.getOutputPressureForInput(opressure * 100.0) /
          100.0;

    // Convert tilt values to % applied to max value
    if (m_enabledOTilt) {
      // Convert MyPaint magnitude range (0 to 1.0) to graph range (90 - 30)
      double oldTiltM = 90 - (otiltMagnitude * 60);
      otiltMagnitude =
          m_opacityStylusProperty.getOutputTiltForInput(oldTiltM) / 100.0;
    }

    double maxThickness = m_thickness.getValue().second;
    double thickness =
        (m_enabledPressure || m_enabledTilt)
            ? computeThickness(m_enabledPressure, pressure, m_enabledTilt,
                               tiltMagnitude, m_thickness)
            : maxThickness;

    double opacity =
        ((m_enabledOPressure || m_enabledOTilt)
             ? computeThickness(m_enabledOPressure, opressure, m_enabledOTilt,
                                otiltMagnitude, m_opacity)
             : m_opacity.getValue().second) *
        0.01;
    TDimension size   = m_workRaster->getSize();
    TPointD rasCenter = TPointD(size.lx * 0.5, size.ly * 0.5);
    TThickPoint thickPoint(
        pos + rasCenter, thickness,
        (m_brushTip.getBrushTip() && m_brushTip.isAutoRotate() ? m_tiltAngle
                                                               : 0));
    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.getSmoothPoints(pts);
    }

    for (size_t i = 0; i < pts.size(); ++i) {
      TThickPoint old = m_points.back();
      if (norm2(thickPoint - old) < 4) break;

      const TThickPoint &point = pts[i];
      TThickPoint mid((old + point) * 0.5, (point.thick + old.thick) * 0.5,
                      (m_brushTip.getBrushTip() && m_brushTip.isAutoRotate()
                           ? (point.rotation + old.rotation) * 0.5
                           : 0));
      m_points.push_back(mid);
      m_points.push_back(point);

      TRect bbox;
      int m = m_points.size();
      TRectD invalidateRect;
      std::vector<TThickPoint> points;
      if (m == 3) {
        // ho appena cominciato. devo disegnare un segmento
        TThickPoint pa = m_points.front();
        points.push_back(pa);
        points.push_back(mid);
        bbox           = m_bluredBrush->getBoundFromPoints(points);
        updateWorkAndBackupRasters(bbox + m_lastRect);
        m_tileSaver->save(bbox);
        m_bluredBrush->addArc(pa, (pa + mid) * 0.5, mid, m_oldOpacity, opacity);
        m_lastRect += bbox;
      } else {
        // caso generale: disegno un arco
        points.push_back(m_points[m - 4]);
        points.push_back(old);
        points.push_back(mid);
        bbox           = m_bluredBrush->getBoundFromPoints(points);
        updateWorkAndBackupRasters(bbox + m_lastRect);
        m_tileSaver->save(bbox);
        m_bluredBrush->addArc(m_points[m - 4], old, mid, m_oldOpacity, opacity);
        m_lastRect += bbox;
      }
      m_oldOpacity = opacity;
      m_bluredBrush->updateDrawing(
          ri->getRaster(), m_backUpRas, m_currentColor, bbox, opacity,
          m_modifierPaintBehind.getValue(), m_modifierLockAlpha.getValue());

      std::vector<TThickPoint> symmPts =
          m_bluredBrush->getSymmetryPoints(points);
      points.insert(points.end(), symmPts.begin(), symmPts.end());

      invalidateRect += ToolUtils::getBounds(points, maxThickness) - rasCenter;
      if (m_brushTip.getBrushTip() && m_brushTip.getScatter())
        invalidateRect += convert(bbox + m_lastRect) - rasCenter;

      invalidate(invalidateRect.enlarge(2));
      m_strokeRect += bbox;
    }
  }
}

//---------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::leftButtonUp(const TPointD &pos,
                                      const TMouseEvent &e) {
  TPointD previousBrushPos = m_brushPos;
  m_brushPos = m_mousePos = pos;

  m_enabled = false;

  TRasterImageP ri = (TRasterImageP)getImage(true);
  if (!ri) return;

  TRasterP ras      = ri->getRaster();
  TPointD rasCenter = ras->getCenterD();
  TPointD point;
  if (e.isCtrlPressed() || m_snapGrid.getValue() || e.isAltPressed())
    point = TPointD(m_lastPoint + rasCenter);
  else
    point = TPointD(pos + rasCenter);

  // Clicked with no movement. Make it look like it moved
  if (m_strokeRect.isEmpty()) point += TPointD(1, 1);

  double pressure, opressure;
  if (m_isMyPaintStyleSelected)  // mypaint brush case
    pressure = (m_enabledPressure || m_enabledOPressure) ? e.m_pressure : 0.5;
  else {
    pressure = (m_enabledPressure || m_enabledOPressure) ? e.m_pressure : 1.0;
    if ((m_enabledPressure || m_enabledOPressure) && e.m_pressure == 1.0)
      pressure = 0.1;
  }
  opressure = pressure;

  if (m_isStraight) {
    opressure = pressure = m_oldPressure;
  }

  // Convert QTabletEvent tilt range (-60 to 60) to MyPaint range (-1.0 to 1.0)
  // Note: xTilt value sign must be flipped so it angles correctly
  double xTilt = (e.m_tiltX / -60.0);
  double yTilt = (e.m_tiltY / 60.0);
  // rotation of stylus
  double arctan = atan2(yTilt, xTilt);
  m_tiltAngle = (!e.isTablet() || (!xTilt && !yTilt)) ? 0.0 : arctan * M_180_PI;

  double tiltMagnitude, otiltMagnitude;
  double tiltX   = (m_enabledTilt || m_enabledOTilt) ? xTilt : 0.0;
  double tiltY   = (m_enabledTilt || m_enabledOTilt) ? yTilt : 0.0;
  otiltMagnitude = tiltMagnitude = std::sqrt(tiltX * tiltX + tiltY * tiltY);

  if (m_isMyPaintStyleSelected) {
    if (!m_toonz_brush) return;

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
    m_highFreqBrushTimer = 0.0;
    for (size_t i = 0; i < pts.size(); ++i) {
      const TThickPoint &thickPoint2 = pts[i];
      m_strokeSegmentRect.empty();
      m_toonz_brush->strokeTo(thickPoint2, thickPoint2.thick, tiltX, tiltY,
                              brushTimer);
      if (i == pts.size() - 1) m_toonz_brush->endStroke();
      TRect updateRect = m_strokeSegmentRect * ras->getBounds();
      if (!updateRect.isEmpty()) {
        TRaster32P sourceRas = m_modifierPaintBehind.getValue()
                                   ? (TRaster32P)m_backUpRas
                                   : m_workRaster;
        m_toonz_brush->updateDrawing(ras, sourceRas, m_strokeSegmentRect,
                                     m_modifierPaintBehind.getValue());
      }

      TPointD thickOffset(m_maxCursorThick * 0.5, m_maxCursorThick * 0.5);
      invalidateRect += convert(m_strokeSegmentRect) - rasCenter;
      invalidateRect +=
          TRectD(m_brushPos - thickOffset, m_brushPos + thickOffset);
      invalidateRect += TRectD(previousBrushPos - thickOffset,
                               previousBrushPos + thickOffset);
    }
    invalidate(invalidateRect.enlarge(2.0));

    if (m_toonz_brush) {
      delete m_toonz_brush;
      m_toonz_brush = 0;
    }
  } else {
    if (!m_bluredBrush) return;

    // Convert pressure value to % applied to max value
    if (m_enabledPressure)
      pressure =
          m_sizeStylusProperty.getOutputPressureForInput(pressure * 100.0) /
          100.0;

    // Convert tilt values to % applied to max value
    if (m_enabledTilt) {
      // Convert MyPaint magnitude range (0 to 1.0) to graph range (90 - 30)
      double oldTiltM = 90 - (tiltMagnitude * 60);
      tiltMagnitude =
          m_sizeStylusProperty.getOutputTiltForInput(oldTiltM) / 100.0;
    }

    // Convert pressure value to % applied to max value
    if (m_enabledOPressure)
      opressure =
          m_opacityStylusProperty.getOutputPressureForInput(opressure * 100.0) /
          100.0;

    // Convert tilt values to % applied to max value
    if (m_enabledOTilt) {
      // Convert MyPaint magnitude range (0 to 1.0) to graph range (90 - 30)
      double oldTiltM = 90 - (otiltMagnitude * 60);
      otiltMagnitude =
          m_opacityStylusProperty.getOutputTiltForInput(oldTiltM) / 100.0;
    }

    double maxThickness = m_thickness.getValue().second;
    double thickness =
        (m_enabledPressure || m_enabledTilt)
            ? computeThickness(m_enabledPressure, pressure, m_enabledTilt,
                               tiltMagnitude, m_thickness)
            : maxThickness;

    double opacity =
        ((m_enabledOPressure || m_enabledOTilt)
             ? computeThickness(m_enabledOPressure, opressure, m_enabledOTilt,
                                otiltMagnitude, m_opacity)
             : m_opacity.getValue().second) *
        0.01;
    TPointD rasCenter = ri->getRaster()->getCenterD();
    TThickPoint thickPoint(
        point, thickness,
        (m_brushTip.getBrushTip() && m_brushTip.isAutoRotate() ? m_tiltAngle
                                                               : 0));

    std::vector<TThickPoint> pts;
    if (m_smooth.getValue() == 0 || m_isStraight) {
      pts.push_back(thickPoint);
    } else {
      m_smoothStroke.addPoint(thickPoint);
      m_smoothStroke.endStroke();
      m_smoothStroke.getSmoothPoints(pts);
    }
    if (pts.size() > 0) {
      for (size_t i = 0; i < pts.size(); ++i) {
        TThickPoint old = m_points.back();

        const TThickPoint &thickPoint2 = pts[i];
        TThickPoint mid((old + thickPoint2) * 0.5,
                        (thickPoint2.thick + old.thick) * 0.5,
                        (m_brushTip.getBrushTip() && m_brushTip.isAutoRotate()
                             ? (thickPoint2.rotation + old.rotation) * 0.5
                             : 0));
        m_points.push_back(mid);
        m_points.push_back(thickPoint2);
        
        TRect bbox;
        int m = m_points.size();
        std::vector<TThickPoint> points;
        if (m == 3) {
          TThickPoint pa = m_points.front();
          points.push_back(pa);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(pa, (pa + mid) * 0.5, mid, m_oldOpacity,
                                opacity);
          m_lastRect += bbox;
        } else {
          // caso generale: disegno un arco
          points.push_back(m_points[m - 4]);
          points.push_back(old);
          points.push_back(mid);
          bbox = m_bluredBrush->getBoundFromPoints(points);
          updateWorkAndBackupRasters(bbox + m_lastRect);
          m_tileSaver->save(bbox);
          m_bluredBrush->addArc(m_points[m - 4], old, mid, m_oldOpacity,
                                opacity);
          m_lastRect += bbox;
        }
        bbox = m_bluredBrush->getBoundFromPoints(points);
        updateWorkAndBackupRasters(bbox);
        m_tileSaver->save(bbox);
        m_bluredBrush->addArc(points[0], points[1], points[2], m_oldOpacity,
                              opacity);
        m_bluredBrush->updateDrawing(
            ri->getRaster(), m_backUpRas, m_currentColor, bbox, opacity,
            m_modifierPaintBehind.getValue(), m_modifierLockAlpha.getValue());

        std::vector<TThickPoint> symmPts =
            m_bluredBrush->getSymmetryPoints(points);
        points.insert(points.end(), symmPts.begin(), symmPts.end());

        TRectD invalidateRect =
            ToolUtils::getBounds(points, maxThickness) - rasCenter;
        if (m_brushTip.getBrushTip() && m_brushTip.getScatter())
          invalidateRect += convert(bbox + m_lastRect) - rasCenter;

        invalidate(invalidateRect.enlarge(2));
        m_strokeRect += bbox;
      }
    }
    m_lastRect.empty();

    if (m_bluredBrush) {
      delete m_bluredBrush;
      m_bluredBrush = 0;
    }
  }

  m_lastRect.empty();
  m_workRaster->unlock();

  bool isEditingLevel = m_application->getCurrentFrame()->isEditingLevel();
  bool renameColumn   = m_isFrameCreated;
  if (!isEditingLevel && renameColumn) TUndoManager::manager()->beginBlock();
  TTool::Application *app   = TTool::getApplication();
  TXshLevel *level          = app->getCurrentLevel()->getLevel();
  TXshSimpleLevelP simLevel = level->getSimpleLevel();

  if (m_tileSet->getTileCount() > 0) {
    delete m_tileSaver;
    TFrameId frameId = getCurrentFid();
    TRasterP subras  = ras->extract(m_strokeRect)->clone();
    TUndoManager::manager()->add(new FullColorBrushUndo(
        m_tileSet, simLevel.getPointer(), frameId, m_isFrameCreated, subras,
        m_strokeRect.getP00()));
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

  notifyImageChanged();
  m_strokeRect.empty();
  m_mousePressed     = false;
  m_isStraight       = false;
  m_oldPressure      = -1.0;
  m_perspectiveIndex = -1;
  m_tiltAngle        = 0;
}

//---------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  struct Locals {
    FullColorBrushTool *m_this;

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

  // locals.addMinMax(m_thickness, int(add));
  //} else
  if (e.isCtrlPressed() && e.isAltPressed() && !e.isShiftPressed() &&
      Preferences::instance()->useCtrlAltToResizeBrushEnabled()) {
    const TPointD &diff = m_windowMousePos - -e.m_pos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMaxSeparate(m_thickness, int(min), int(max));
  } else {
    m_brushPos = pos;
  }

  m_mousePos       = pos;
  m_windowMousePos = -e.m_pos;

  invalidate();
}

//-------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::draw() {
  int devPixRatio = m_viewer->getDevPixRatio();

  if (TRasterImageP ri = TRasterImageP(getImage(false))) {
    glLineWidth(1.0 * devPixRatio);

    if (m_isStraight) {
      tglColor(TPixel32::Red);
      tglDrawSegment(m_firstPoint, m_lastPoint);
    }
    // If toggled off, don't draw brush outline
    if (!Preferences::instance()->isCursorOutlineEnabled()) return;

    TRasterP ras = ri->getRaster();

    double alpha       = 1.0;
    double alphaRadius = 3.0;
    double pixelSize   = sqrt(tglGetPixelSize2());

    // circles with lesser radius looks more bold
    // to avoid these effect we'll reduce alpha for small radiuses
    double minX     = m_minCursorThick / (alphaRadius * pixelSize);
    double maxX     = m_maxCursorThick / (alphaRadius * pixelSize);
    double minAlpha = alpha * (1.0 - 1.0 / (1.0 + minX));
    double maxAlpha = alpha * (1.0 - 1.0 / (1.0 + maxX));

    glPushAttrib(GL_ALL_ATTRIB_BITS);
    tglEnableBlending();
    tglEnableLineSmooth(true, 0.5);

    if (!m_isMyPaintStyleSelected && m_brushTip.getBrushTip() &&
        !m_brushTip.getBrushTip()->m_imageContour.empty()) {
      QSize chipSize   = TBrushTipManager::instance()->getChipSize();
      double brushSize = (m_maxCursorThick + 1);

      QTransform transform;
      transform.translate(m_brushPos.x, m_brushPos.y);
      transform.rotate(m_brushTip.getRotation());
      if (m_brushTip.isAutoRotate()) transform.rotate(m_tiltAngle);
      if (m_brushTip.isFlipHorizontal()) transform.scale(-1.0, 1.0);
      if (m_brushTip.isFlipVertical()) transform.scale(1.0, -1.0);
      transform.scale(brushSize / chipSize.width(),
                      brushSize / chipSize.height());

      std::vector<QPolygonF> contours =
          m_brushTip.getBrushTip()->m_imageContour;
      for (int i = 0; i < contours.size(); i++) {
        QPolygonF poly = transform.map(contours[i]);
        for (int i = 1; i < poly.count(); i++) {
          glColor4d(1.0, 1.0, 1.0, maxAlpha);
          tglDrawSegment(
              TPointD(poly[i - 1].x() - pixelSize, poly[i - 1].y() - pixelSize),
              TPointD(poly[i].x() - pixelSize, poly[i].y() - pixelSize));
          glColor4d(0.0, 0.0, 0.0, maxAlpha);
          tglDrawSegment(TPointD(poly[i - 1].x(), poly[i - 1].y()),
                         TPointD(poly[i].x(), poly[i].y()));
        }
        glColor4d(1.0, 1.0, 1.0, maxAlpha);
        tglDrawSegment(
            TPointD(poly.last().x() - pixelSize, poly.last().y() - pixelSize),
            TPointD(poly.first().x() - pixelSize,
                    poly.first().y() - pixelSize));
        glColor4d(0.0, 0.0, 0.0, maxAlpha);
        tglDrawSegment(TPointD(poly.last().x(), poly.last().y()),
                       TPointD(poly.first().x(), poly.first().y()));
      }
    } else {
      if (m_minCursorThick < m_maxCursorThick - pixelSize) {
        glColor4d(1.0, 1.0, 1.0, minAlpha);
        tglDrawCircle(m_brushPos, (m_minCursorThick + 1) * 0.5 - pixelSize);
        glColor4d(0.0, 0.0, 0.0, minAlpha);
        tglDrawCircle(m_brushPos, (m_minCursorThick + 1) * 0.5);
      }

      glColor4d(1.0, 1.0, 1.0, maxAlpha);
      tglDrawCircle(m_brushPos, (m_maxCursorThick + 1) * 0.5 - pixelSize);
      glColor4d(0.0, 0.0, 0.0, maxAlpha);
      tglDrawCircle(m_brushPos, (m_maxCursorThick + 1) * 0.5);
    }

    glPopAttrib();
  }
}

//--------------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onEnter() {
  TImageP img = getImage(false);
  TRasterImageP ri(img);
  if (ri) {
    m_minThick = m_thickness.getValue().first;
    m_maxThick = m_thickness.getValue().second;
  } else {
    m_minThick = 0;
    m_maxThick = 0;
  }

  updateCurrentStyle();
}

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onLeave() {
  m_minThick       = 0;
  m_maxThick       = 0;
  m_minCursorThick = 0;
  m_maxCursorThick = 0;
}

//----------------------------------------------------------------------------------------------------------

TPropertyGroup *FullColorBrushTool::getProperties(int targetType) {
  if (!m_presetsLoaded) initPresets();
  return &m_prop;
}

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::onImageChanged() { setWorkAndBackupImages(); }

//----------------------------------------------------------------------------------------------------------

void FullColorBrushTool::setWorkAndBackupImages() {
  TRasterImageP ri = (TRasterImageP)getImage(false, 1);
  if (!ri) return;

  TRasterP ras   = ri->getRaster();
  TDimension dim = ras->getSize();

  if (!m_workRaster || m_workRaster->getLx() != dim.lx ||
      m_workRaster->getLy() != dim.ly)
    m_workRaster = TRaster32P(dim);

  if (!m_backUpRas || m_backUpRas->getLx() != dim.lx ||
      m_backUpRas->getLy() != dim.ly ||
      m_backUpRas->getPixelSize() != ras->getPixelSize())
    m_backUpRas = ras->create(dim.lx, dim.ly);

  m_strokeRect.empty();
  m_lastRect.empty();
}

//------------------------------------------------------------------

bool FullColorBrushTool::onPropertyChanged(std::string propertyName) {
  if (m_propertyUpdating) return true;

  updateCurrentStyle();

  if (propertyName == "Preset:") {
    if (m_preset.getValue() != CUSTOM_WSTR)
      loadPreset();
    else  // Chose <custom>, go back to last saved brush settings
      loadLastBrush();

    FullcolorBrushPreset = m_preset.getValueAsString();
    m_propertyUpdating   = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
    return true;
  } else if (propertyName == m_modifierLockAlpha.getName()) {
    if (m_modifierLockAlpha.getValue()) {
      m_modifierPaintBehind.setValue(false);
      m_modifierEraser.setValue(false);
    }
  } else if (propertyName == m_modifierEraser.getName()) {
    if (m_modifierEraser.getValue()) {
      m_modifierLockAlpha.setValue(false);
      m_modifierPaintBehind.setValue(false);
    }
  } else if (propertyName == m_modifierPaintBehind.getName()) {
    if (m_modifierPaintBehind.getValue()) {
      m_modifierLockAlpha.setValue(false);
      m_modifierEraser.setValue(false);
    }
  }

  // Recalculate/reset based on changed settings
  if (propertyName == m_thickness.getName()) {
    m_minThick = m_thickness.getValue().first;
    m_maxThick = m_thickness.getValue().second;
  }

  if (propertyName == m_hardness.getName() ||
      propertyName == m_thickness.getName()) {
    m_brushPad = ToolUtils::getBrushPad(m_thickness.getValue().second,
                                        m_hardness.getValue() * 0.01);
    TRectD rect(m_brushPos - TPointD(m_maxThick + 2, m_maxThick + 2),
                m_brushPos + TPointD(m_maxThick + 2, m_maxThick + 2));
    invalidate(rect);
  }

  FullcolorBrushMinSize         = m_thickness.getValue().first;
  FullcolorBrushMaxSize         = m_thickness.getValue().second;
  FullcolorBrushSmooth          = m_smooth.getValue();
  FullcolorBrushHardness        = m_hardness.getValue();
  FullcolorMinOpacity           = m_opacity.getValue().first;
  FullcolorMaxOpacity           = m_opacity.getValue().second;
  FullcolorModifierSize         = m_modifierSize.getValue();
  FullcolorModifierOpacity      = m_modifierOpacity.getValue();
  FullcolorModifierEraser       = m_modifierEraser.getValue() ? 1 : 0;
  FullcolorModifierLockAlpha    = m_modifierLockAlpha.getValue() ? 1 : 0;
  FullcolorModifierPaintBehind  = m_modifierPaintBehind.getValue() ? 1 : 0;
  FullcolorBrushSnapGrid        = m_snapGrid.getValue() ? 1 : 0;
  FullcolorPressureSensitivity  = m_sizeStylusProperty.isPressureEnabled();
  FullcolorPressureCurvePts     = m_sizeStylusProperty.getPressureCurve();
  FullcolorTiltSensitivity      = m_sizeStylusProperty.isTiltEnabled();
  FullcolorTiltCurvePts         = m_sizeStylusProperty.getTiltCurve();
  FullcolorOPressureSensitivity = m_opacityStylusProperty.isPressureEnabled();
  FullcolorOPressureCurvePts    = m_opacityStylusProperty.getPressureCurve();
  FullcolorOTiltSensitivity     = m_opacityStylusProperty.isTiltEnabled();
  FullcolorOTiltCurvePts        = m_opacityStylusProperty.getTiltCurve();
  FullcolorMyPaintPressureSensitivity = m_mypaintPressure.getValue();
  FullcolorMyPaintTiltSensitivity     = m_mypaintTilt.getValue();
  FullcolorBrushTipSpacing            = m_brushTip.getSpacing();
  FullcolorBrushTipRotation           = m_brushTip.getRotation();
  FullcolorBrushTipAutoRotate         = m_brushTip.isAutoRotate() ? 1 : 0;
  FullcolorBrushTipFlipHorizontal     = m_brushTip.isFlipHorizontal() ? 1 : 0;
  FullcolorBrushTipFlipVertical       = m_brushTip.isFlipVertical() ? 1 : 0;
  FullcolorBrushTipScatter            = m_brushTip.getScatter();
  FullcolorBrushTipId                 = m_brushTip.getBrushTip()
                                            ? m_brushTip.getBrushTip()->m_idName
                                            : DEFAULTBRUSHTIPID;

  if (m_preset.getValue() != CUSTOM_WSTR) {
    m_preset.setValue(CUSTOM_WSTR);
    FullcolorBrushPreset = m_preset.getValueAsString();
    m_propertyUpdating   = true;
    getApplication()->getCurrentTool()->notifyToolChanged();
    m_propertyUpdating = false;
  }

  return true;
}

//------------------------------------------------------------------

void FullColorBrushTool::initPresets() {
  if (!m_presetsLoaded) {
    // If necessary, load the presets from file
    m_presetsLoaded = true;
    m_presetsManager.load(ToonzFolder::getMyModuleDir() + "brush_raster.txt");
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

void FullColorBrushTool::loadPreset() {
  const std::set<BrushData> &presets = m_presetsManager.presets();
  std::set<BrushData>::const_iterator it;

  it = presets.find(BrushData(m_preset.getValue()));
  if (it == presets.end()) return;

  const BrushData &preset = *it;

  try  // Don't bother with RangeErrors
  {
    m_thickness.setValue(
        TIntPairProperty::Value(std::max((int)preset.m_min, 1), preset.m_max));
    m_smooth.setValue(preset.m_smooth, true);
    m_hardness.setValue(preset.m_hardness, true);
    m_opacity.setValue(
        TDoublePairProperty::Value(preset.m_opacityMin, preset.m_opacityMax));
    m_mypaintPressure.setValue(preset.m_mypaintPressure);
    m_modifierSize.setValue(preset.m_modifierSize);
    m_modifierOpacity.setValue(preset.m_modifierOpacity);
    m_modifierEraser.setValue(preset.m_modifierEraser);
    m_modifierLockAlpha.setValue(preset.m_modifierLockAlpha);
    m_modifierPaintBehind.setValue(preset.m_modifierPaintBehind);
    m_mypaintTilt.setValue(preset.m_mypaintTilt);

    m_sizeStylusProperty.setPressureEnabled(preset.m_pressure);
    m_sizeStylusProperty.setPressureCurve(preset.m_pressureCurve);
    m_sizeStylusProperty.setTiltEnabled(preset.m_tilt);
    m_sizeStylusProperty.setTiltCurve(preset.m_tiltCurve);

    m_opacityStylusProperty.setPressureEnabled(preset.m_opressure);
    m_opacityStylusProperty.setPressureCurve(preset.m_opressureCurve);
    m_opacityStylusProperty.setTiltEnabled(preset.m_otilt);
    m_opacityStylusProperty.setTiltCurve(preset.m_otiltCurve);

    m_brushTip.setSpacing(preset.m_spacing);
    m_brushTip.setRotation(preset.m_rotation);
    m_brushTip.setAutoRotate(preset.m_autoRotate);
    m_brushTip.setFlipHorizontal(preset.m_flipH);
    m_brushTip.setFlipVertical(preset.m_flipV);
    m_brushTip.setScatter(preset.m_scatter);
    m_brushTip.setBrushTip(
        TBrushTipManager::instance()->getBrushTipById(preset.m_brushTipId));
  } catch (...) {
  }
}

//------------------------------------------------------------------

void FullColorBrushTool::addPreset(QString name) {
  // Build the preset
  BrushData preset(name.toStdWString());

  preset.m_min                 = m_thickness.getValue().first;
  preset.m_max                 = m_thickness.getValue().second;
  preset.m_smooth              = m_smooth.getValue();
  preset.m_hardness            = m_hardness.getValue();
  preset.m_opacityMin          = m_opacity.getValue().first;
  preset.m_opacityMax          = m_opacity.getValue().second;
  preset.m_modifierSize        = m_modifierSize.getValue();
  preset.m_modifierOpacity     = m_modifierOpacity.getValue();
  preset.m_modifierEraser      = m_modifierEraser.getValue();
  preset.m_modifierLockAlpha   = m_modifierLockAlpha.getValue();
  preset.m_modifierPaintBehind = m_modifierPaintBehind.getValue();
  preset.m_pressure            = m_sizeStylusProperty.isPressureEnabled();
  preset.m_pressureCurve       = m_sizeStylusProperty.getPressureCurve();
  preset.m_tilt                = m_sizeStylusProperty.isTiltEnabled();
  preset.m_tiltCurve           = m_sizeStylusProperty.getTiltCurve();
  preset.m_opressure           = m_opacityStylusProperty.isPressureEnabled();
  preset.m_opressureCurve      = m_opacityStylusProperty.getPressureCurve();
  preset.m_otilt               = m_opacityStylusProperty.isTiltEnabled();
  preset.m_otiltCurve          = m_opacityStylusProperty.getTiltCurve();
  preset.m_mypaintPressure     = m_mypaintPressure.getValue();
  preset.m_mypaintTilt         = m_mypaintTilt.getValue();
  preset.m_spacing             = m_brushTip.getSpacing();
  preset.m_rotation            = m_brushTip.getRotation();
  preset.m_autoRotate          = m_brushTip.isAutoRotate();
  preset.m_flipH               = m_brushTip.isFlipHorizontal();
  preset.m_flipV               = m_brushTip.isFlipVertical();
  preset.m_scatter             = m_brushTip.getScatter();
  preset.m_brushTipId          = m_brushTip.getBrushTip()
                                     ? m_brushTip.getBrushTip()->m_idName
                                     : DEFAULTBRUSHTIPID;

  // Pass the preset to the manager
  m_presetsManager.addPreset(preset);

  // Reinitialize the associated preset enum
  initPresets();

  // Set the value to the specified one
  m_preset.setValue(preset.m_name);
  FullcolorBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void FullColorBrushTool::removePreset() {
  std::wstring name(m_preset.getValue());
  if (name == CUSTOM_WSTR) return;

  m_presetsManager.removePreset(name);
  initPresets();

  // No parameter change, and set the preset value to custom
  m_preset.setValue(CUSTOM_WSTR);
  FullcolorBrushPreset = m_preset.getValueAsString();
}

//------------------------------------------------------------------

void FullColorBrushTool::loadLastBrush() {
  m_thickness.setValue(
      TDoublePairProperty::Value(FullcolorBrushMinSize, FullcolorBrushMaxSize));
  m_smooth.setValue(FullcolorBrushSmooth);
  m_opacity.setValue(
      TDoublePairProperty::Value(FullcolorMinOpacity, FullcolorMaxOpacity));
  m_hardness.setValue(FullcolorBrushHardness);
  m_modifierSize.setValue(FullcolorModifierSize);
  m_modifierOpacity.setValue(FullcolorModifierOpacity);
  m_modifierEraser.setValue(FullcolorModifierEraser ? true : false);
  m_modifierLockAlpha.setValue(FullcolorModifierLockAlpha ? true : false);
  m_modifierPaintBehind.setValue(FullcolorModifierPaintBehind ? true : false);
  m_snapGrid.setValue(FullcolorBrushSnapGrid ? true : false);
  m_mypaintPressure.setValue(FullcolorMyPaintPressureSensitivity ? 1 : 0);
  m_mypaintTilt.setValue(FullcolorMyPaintTiltSensitivity ? 1 : 0);

  m_sizeStylusProperty.setPressureEnabled(FullcolorPressureSensitivity);
  m_sizeStylusProperty.setPressureCurve(FullcolorPressureCurvePts);
  m_sizeStylusProperty.setTiltEnabled(FullcolorTiltSensitivity);
  m_sizeStylusProperty.setTiltCurve(FullcolorTiltCurvePts);

  m_opacityStylusProperty.setPressureEnabled(FullcolorOPressureSensitivity ? 1
                                                                           : 0);
  m_opacityStylusProperty.setPressureCurve(FullcolorOPressureCurvePts);
  m_opacityStylusProperty.setTiltEnabled(FullcolorOTiltSensitivity ? 1 : 0);
  m_opacityStylusProperty.setTiltCurve(FullcolorOTiltCurvePts);

  m_brushTip.setSpacing(FullcolorBrushTipSpacing);
  m_brushTip.setRotation(FullcolorBrushTipRotation);
  m_brushTip.setAutoRotate(FullcolorBrushTipAutoRotate ? 1 : 0);
  m_brushTip.setFlipHorizontal(FullcolorBrushTipFlipHorizontal ? 1 : 0);
  m_brushTip.setFlipVertical(FullcolorBrushTipFlipVertical ? 1 : 0);
  m_brushTip.setScatter(FullcolorBrushTipScatter);
  m_brushTip.setBrushTip(
      TBrushTipManager::instance()->getBrushTipById(FullcolorBrushTipId));
}

//------------------------------------------------------------------

void FullColorBrushTool::updateCurrentStyle() {
  m_currentColor = TPixel32::Black;
  if (TTool::Application *app = getApplication()) {
    if (app->getCurrentObject()->isSpline()) {
      m_currentColor = TPixel32::Red;
    } else if (TPalette *plt = app->getCurrentPalette()->getPalette()) {
      int style               = app->getCurrentLevelStyleIndex();
      TColorStyle *colorStyle = plt->getStyle(style);
      m_currentColor          = colorStyle->getMainColor();
      TMyPaintBrushStyle *mpbs =
          dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
      m_isMyPaintStyleSelected = (mpbs) ? true : false;
    }
  }

  double prevMinCursorThick = m_minCursorThick;
  double prevMaxCursorThick = m_maxCursorThick;

  m_enabledPressure  = m_isMyPaintStyleSelected
                           ? m_mypaintPressure.getValue()
                           : m_sizeStylusProperty.isPressureEnabled();
  m_enabledOPressure = m_isMyPaintStyleSelected
                           ? false
                           : m_opacityStylusProperty.isPressureEnabled();
  if (TMyPaintBrushStyle *brushStyle = getBrushStyle()) {
    double radiusLog = brushStyle->getBrush().getBaseValue(
                           MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC) +
                       m_modifierSize.getValue() * log(2.0);
    double radius    = exp(radiusLog);
    m_minCursorThick = m_maxCursorThick = (int)round(2.0 * radius);
  } else {
    m_minCursorThick = std::max(m_thickness.getValue().first, 1.0);
    m_maxCursorThick =
        std::max(m_thickness.getValue().second, m_minCursorThick);
    if (!m_enabledPressure && !m_enabledTilt)
      m_minCursorThick = m_maxCursorThick;
  }

  m_enabledTilt  = m_isMyPaintStyleSelected
                       ? m_mypaintTilt.getValue()
                       : m_sizeStylusProperty.isTiltEnabled();
  m_enabledOTilt = m_isMyPaintStyleSelected
                       ? false
                       : m_opacityStylusProperty.isTiltEnabled();

  // if this function is called from onEnter(), the clipping rect will not be
  // set in order to update whole viewer.
  if (prevMinCursorThick == 0 && prevMaxCursorThick == 0) return;

  if (m_minCursorThick != prevMinCursorThick ||
      m_maxCursorThick != prevMaxCursorThick) {
    TRectD rect(
        m_brushPos - TPointD(m_maxCursorThick + 2, m_maxCursorThick + 2),
        m_brushPos + TPointD(m_maxCursorThick + 2, m_maxCursorThick + 2));
    invalidate(rect);
  }
}

//------------------------------------------------------------------

double FullColorBrushTool::restartBrushTimer() {
  double dtime = m_brushTimer.nsecsElapsed() * 1e-9;
  m_brushTimer.restart();
  return dtime;
}

//------------------------------------------------------------------

TMyPaintBrushStyle *FullColorBrushTool::getBrushStyle() {
  if (TTool::Application *app = getApplication())
    return dynamic_cast<TMyPaintBrushStyle *>(app->getCurrentLevelStyle());
  return 0;
}

//------------------------------------------------------------------

void FullColorBrushTool::applyToonzBrushSettings(mypaint::Brush &mypaintBrush) {
  TMyPaintBrushStyle *mypaintStyle = getBrushStyle();

  if (!mypaintStyle) return;

  const double precision = 1e-5;

  double modifierSize    = m_modifierSize.getValue() * log(2.0);
  double modifierOpacity = 0.01 * m_modifierOpacity.getValue();
  bool modifierEraser    = m_modifierEraser.getValue();
  bool modifierLockAlpha = m_modifierLockAlpha.getValue();

  TPixelD color = PixelConverter<TPixelD>::from(m_currentColor);
  double colorH = 0.0;
  double colorS = 0.0;
  double colorV = 0.0;
  RGB2HSV(color.r, color.g, color.b, &colorH, &colorS, &colorV);

  mypaintBrush.fromBrush(mypaintStyle->getBrush());

  float baseSize =
      mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC);
  float baseOpacity = mypaintBrush.getBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE);

  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_RADIUS_LOGARITHMIC,
                            baseSize + modifierSize);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_OPAQUE,
                            baseOpacity * modifierOpacity);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_H, colorH / 360.0);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_S, colorS);
  mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_COLOR_V, colorV);

  if (modifierEraser) {
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_ERASER, 1.0);
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_LOCK_ALPHA, 0.0);
  } else if (modifierLockAlpha) {
    // lock-alpha already disables eraser
    mypaintBrush.setBaseValue(MYPAINT_BRUSH_SETTING_LOCK_ALPHA, 1.0);
  }
}

//==========================================================================================================

FullColorBrushToolNotifier::FullColorBrushToolNotifier(FullColorBrushTool *tool)
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
      assert(ret);
      ret = connect(paletteHandle, SIGNAL(colorStyleSwitched()), this,
                    SLOT(onColorStyleChanged()));
      assert(ret);
    }
  }
}

//==========================================================================================================

FullColorBrushTool fullColorPencil("T_Brush");
