

#include "tstroke.h"
#include "tools/toolutils.h"
#include "tools/tool.h"
#include "tmathutil.h"
#include "tools/cursors.h"
#include "drawutil.h"
#include "tcolorstyles.h"
#include "tundo.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tproperty.h"
#include "toonz/ttilesaver.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/observer.h"
#include "toonz/toonzimageutils.h"
#include "toonz/levelproperties.h"
#include "toonz/stage2.h"
#include "toonz/ttileset.h"
#include "toonz/preferences.h"
#include "tgl.h"
#include "tenv.h"

#include "trop.h"

#include "tinbetween.h"
#include "ttile.h"

#include "toonz/tpalettehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "tools/toolhandle.h"

// For Qt translation support
#include <QCoreApplication>

#include "tools/stylepicker.h"
#include "toonzqt/styleselection.h"

#include "cmrasterbrush.h"
#include "symmetrytool.h"

using namespace ToolUtils;

#define LINES L"Lines"
#define AREAS L"Areas"
#define ALL L"Lines & Areas"

TEnv::StringVar PaintBrushColorType("InknpaintPaintBrushColorType", "Areas");
TEnv::IntVar PaintBrushSelective("InknpaintPaintBrushSelective", 0);
TEnv::DoubleVar PaintBrushSizeMax("InknpaintPaintBrushSizeMax", 10);
TEnv::DoubleVar PaintBrushSizeMin("InknpaintPaintBrushSizeMin", 10);
TEnv::IntVar PaintBrushPressureSensitivity("InknpaintBrushPressureSensitivity",
                                           1);
TEnv::IntVar PaintBrushModifierLockAlpha("PaintBrushModifierLockAlpha", 0);

//-----------------------------------------------------------------------------

const int BackgroundStyle = 0;

//-----------------------------------------------------------------------------

namespace {

class BrushUndo final : public TRasterUndo {
  std::vector<TThickPoint> m_points;
  int m_styleId;
  bool m_selective;
  ColorType m_colorType;
  bool m_modifierLockAlpha;
  TPointD m_dpiScale;
  double m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;

public:
  BrushUndo(TTileSetCM32 *tileSet, const std::vector<TThickPoint> &points,
            ColorType colorType, int styleId, bool selective,
            TXshSimpleLevel *level, const TFrameId &frameId, bool lockAlpha,
            TPointD dpiScale, double symmetryLines, double rotation,
            TPointD centerPoint, bool useLineSymmetry)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_points(points)
      , m_styleId(styleId)
      , m_selective(selective)
      , m_colorType(colorType)
      , m_modifierLockAlpha(lockAlpha)
      , m_dpiScale(dpiScale)
      , m_brushCount(symmetryLines)
      , m_rotation(rotation)
      , m_centerPoint(centerPoint)
      , m_useLineSymmetry(useLineSymmetry) {}

  void redo() const override {
    TToonzImageP image = m_level->getFrame(m_frameId, true);
    TRasterCM32P ras   = image->getRaster();
    CMRasterBrush m_cmRasterBrush(ras, PAINTBRUSH, m_colorType, m_styleId,
                                  m_points[0], m_selective, 0,
                                  m_modifierLockAlpha, false);
    if (m_brushCount > 1)
      m_cmRasterBrush.addSymmetryBrushes(m_brushCount, m_rotation,
                                         m_centerPoint, m_useLineSymmetry,
                                         m_dpiScale);

    m_cmRasterBrush.setPointsSequence(m_points);
    m_cmRasterBrush.generateStroke(true);
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

  QString getToolName() override { return QString("Paint Brush Tool"); }
  int getHistoryType() override { return HistoryType::PaintBrushTool; }
};

//-------------------------------------------------------------------------------------------

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

double computeThickness(double pressure, const TDoublePairProperty &property) {
  double t                    = pressure * pressure * pressure;
  double thick0               = property.getValue().first;
  double thick1               = property.getValue().second;
  if (thick1 < 0.0001) thick0 = thick1 = 0.0;
  return (thick0 + (thick1 - thick0) * t) * 0.5;
}

}  // namespace

//-----------------------------------------------------------------------------

class PaintBrushTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(PaintBrushTool)

  CMRasterBrush *m_cmRasterBrush;

  bool m_firstTime;

  bool m_selecting;
  TTileSaverCM32 *m_tileSaver;

  TPointD m_mousePos;

  TDoublePairProperty m_rasThickness;
  TBoolProperty m_pressure;
  TBoolProperty m_onlyEmptyAreas;
  TEnumProperty m_colorType;
  TPropertyGroup m_prop;
  int m_cursor;
  ColorType m_colorTypeBrush;
  /*--
     描画開始時のFrameIdを保存し、マウスリリース時（Undoの登録時）に別のフレームに
          移動している場合に備える --*/
  TFrameId m_workingFrameId;

  double m_minThick, m_maxThick;

  Tasks m_task;

  TBoolProperty m_modifierLockAlpha;

  int getStyleUnderCursor(const TPointD &pos);

public:
  PaintBrushTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void draw() override;
  void update(TToonzImageP ti, TRectD area);

  void updateTranslation() override;

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  void onEnter() override;
  void onLeave() override;
  void onActivate() override;
  void onDeactivate() override;
  bool onPropertyChanged(std::string propertyName) override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  int getCursorId() const override { return m_cursor; }
  TPointD getCenteredCursorPos(const TPointD &originalCursorPos);
  void fixMousePos(TPointD pos, bool precise = false);

  int getColorClass() const { return 2; }

  /*---
   * 描画中にツールが切り替わった場合に備え、onDeactivateにもMouseReleaseと同じ終了処理を行う
   * ---*/
  void finishBrush(double pressureValue);
  /*--- Brush、PaintBrush、EraserToolがPencilModeのときにTrueを返す。
  　　　PaintBrushはピクセルのStyleIndexを入れ替えるツールのため、
     　 アンチエイリアスは存在しない、いわば常にPencilMode ---*/
  bool isPencilModeActive() override { return true; }
};

PaintBrushTool paintBrushTool;

//=============================================================================
//
//  InkPaintTool implementation
//
//-----------------------------------------------------------------------------

PaintBrushTool::PaintBrushTool()
    : TTool("T_PaintBrush")
    , m_cmRasterBrush(0)
    , m_selecting(false)
    , m_tileSaver(0)
    , m_cursor(ToolCursor::EraserCursor)
    // sostituire i nomi con quelli del current, tipo W_ToolOptions...
    , m_rasThickness("Size:", 1, 1000, 10, 5)
    , m_colorType("Mode:")                     // W_ToolOptions_InkOrPaint
    , m_onlyEmptyAreas("Selective", false)     // W_ToolOptions_Selective
    , m_firstTime(true)
    , m_pressure("Pressure", true)
    , m_task(PAINTBRUSH)
    , m_workingFrameId(TFrameId())
    , m_modifierLockAlpha("Lock Alpha", false) {
  m_rasThickness.setNonLinearSlider();

  m_colorType.addValue(LINES);
  m_colorType.addValue(AREAS);
  m_colorType.addValue(ALL);

  bind(TTool::ToonzImage);

  m_prop.bind(m_rasThickness);
  m_prop.bind(m_colorType);
  m_prop.bind(m_onlyEmptyAreas);
  m_prop.bind(m_pressure);
  m_prop.bind(m_modifierLockAlpha);

  m_onlyEmptyAreas.setId("Selective");
  m_colorType.setId("Mode");
  m_pressure.setId("PressureSensitivity");
  m_modifierLockAlpha.setId("LockAlpha");
}

//-----------------------------------------------------------------------------

void PaintBrushTool::updateTranslation() {
  m_rasThickness.setQStringName(tr("Size:"));

  m_colorType.setQStringName(tr("Mode:"));
  m_colorType.setItemUIName(LINES, tr("Lines"));
  m_colorType.setItemUIName(AREAS, tr("Areas"));
  m_colorType.setItemUIName(ALL, tr("Lines & Areas"));

  m_onlyEmptyAreas.setQStringName(tr("Selective", NULL));

  m_pressure.setQStringName(tr("Pressure"));

  m_modifierLockAlpha.setQStringName(tr("Lock Alpha"));
}

//-------------------------------------------------------------------------------------------------------

TPointD PaintBrushTool::getCenteredCursorPos(const TPointD &originalCursorPos) {
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

void PaintBrushTool::fixMousePos(TPointD pos, bool precise) {
  m_mousePos = getCenteredCursorPos(pos);
  if (precise) {
    TPointD pp(tround(m_mousePos.x), tround(m_mousePos.y));
    m_mousePos = pp;
  }
}

//-----------------------------------------------------------------------------

void PaintBrushTool::draw() {
  int devPixRatio = m_viewer->getDevPixRatio();

  // If toggled off, don't draw brush outline
  if (!Preferences::instance()->isCursorOutlineEnabled()) return;

  TToonzImageP ti = (TToonzImageP)getImage(false);
  if (!ti) return;
  TRasterP ras = ti->getRaster();
  int lx       = ras->getLx();
  int ly       = ras->getLy();

  if ((ToonzCheck::instance()->getChecks() & ToonzCheck::eInk) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::ePaint) ||
      (ToonzCheck::instance()->getChecks() & ToonzCheck::eInk1))
    glColor3d(0.5, 0.8, 0.8);
  else
    glColor3d(1.0, 0.0, 0.0);

  glLineWidth(1.0 * devPixRatio);
  drawEmptyCircle(tround(m_rasThickness.getValue().second), m_mousePos, true,
                  lx % 2 == 0, ly % 2 == 0);
  drawEmptyCircle(tround(m_rasThickness.getValue().first), m_mousePos, true,
                  lx % 2 == 0, ly % 2 == 0);
}

//-----------------------------------------------------------------------------

const UINT pointCount = 20;

//-----------------------------------------------------------------------------

bool PaintBrushTool::onPropertyChanged(std::string propertyName) {
  PaintBrushColorType           = ::to_string(m_colorType.getValue());
  PaintBrushSelective           = (int)(m_onlyEmptyAreas.getValue());
  PaintBrushSizeMin             = m_rasThickness.getValue().first;
  PaintBrushSizeMax             = m_rasThickness.getValue().second;
  PaintBrushPressureSensitivity = m_pressure.getValue();
  PaintBrushModifierLockAlpha = (int)(m_modifierLockAlpha.getValue());

  /*-- Size ---*/
  if (propertyName == m_rasThickness.getName()) {
    m_minThick = m_rasThickness.getValue().first;
    m_maxThick = m_rasThickness.getValue().second;
  }

  // Selective
  else if (propertyName == m_onlyEmptyAreas.getName()) {
    if (m_onlyEmptyAreas.getValue() && m_modifierLockAlpha.getValue())
      m_modifierLockAlpha.setValue(false);
  }

  // Areas, Lines etc.
  else if (propertyName == m_colorType.getName()) {
    /*--- ColorModelのCursor更新のためにSIGNALを出す ---*/
    TTool::getApplication()->getCurrentTool()->notifyToolChanged();
  }

  // Lock Alpha
  else if (propertyName == m_modifierLockAlpha.getName()) {
    if (m_modifierLockAlpha.getValue() && m_onlyEmptyAreas.getValue())
      m_onlyEmptyAreas.setValue(false);
  }
  return true;
}

//-----------------------------------------------------------------------------

void PaintBrushTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  fixMousePos(pos);
  m_task      = PAINTBRUSH;
  m_selecting = true;
  TImageP image(getImage(true));
  if (m_colorType.getValue() == LINES) m_colorTypeBrush = INK;
  if (m_colorType.getValue() == AREAS) m_colorTypeBrush = PAINT;
  if (m_colorType.getValue() == ALL) m_colorTypeBrush   = INKNPAINT;

  if (TToonzImageP ti = image) {
    TRasterCM32P ras = ti->getRaster();
    if (ras) {
      double maxThick = m_rasThickness.getValue().second;
      double thickness =
          (m_pressure.getValue())
              ? computeThickness(e.m_pressure, m_rasThickness) * 2
              : maxThick;

      if (m_pressure.getValue() && e.m_pressure == 1.0)
        thickness = m_rasThickness.getValue().first;

      int styleId = TTool::getApplication()->getCurrentLevelStyleIndex();

      if (e.isCtrlPressed()) {
        int styleIdUnderCursor              = getStyleUnderCursor(m_mousePos);
        if (styleIdUnderCursor > 0) styleId = styleIdUnderCursor;
        m_task                              = FINGER;
      } else if (e.isShiftPressed()) {
        int styleIdUnderCursor = getStyleUnderCursor(m_mousePos);
        if (styleIdUnderCursor > 0) {
          styleId = styleIdUnderCursor;
          getApplication()->setCurrentLevelStyleIndex(styleId);
        }
        m_selecting = false;
        return;
      }

      TTileSetCM32 *tileSet = new TTileSetCM32(ras->getSize());
      m_tileSaver           = new TTileSaverCM32(ras, tileSet);
      m_cmRasterBrush       = new CMRasterBrush(
          ras, m_task, m_colorTypeBrush, styleId,
          TThickPoint(m_mousePos + convert(ras->getCenter()), thickness),
          m_onlyEmptyAreas.getValue(), 0, m_modifierLockAlpha.getValue(),
          false);

      SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
          TTool::getTool("T_Symmetry", TTool::RasterImage));
      if (symmetryTool && symmetryTool->isGuideEnabled()) {
        TPointD dpiScale       = getViewer()->getDpiScale();
        SymmetryObject symmObj = symmetryTool->getSymmetryObject();
        m_cmRasterBrush->addSymmetryBrushes(
            symmObj.getLines(), symmObj.getRotation(), symmObj.getCenterPoint(),
            symmObj.isUsingLineSymmetry(), dpiScale);
      }

      /*-- 現在のFidを記憶 --*/
      m_workingFrameId = getFrameId();
      m_tileSaver->save(m_cmRasterBrush->getLastRect());
      m_cmRasterBrush->generateLastPieceOfStroke(true);
      invalidate();
    }
  }
}

//-----------------------------------------------------------------------------

void PaintBrushTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  if (!m_selecting) return;

  fixMousePos(pos);
  if (TToonzImageP ri = TImageP(getImage(true))) {
    /*--- マウスを動かしながらショートカットでこのツールに切り替わった場合、
            いきなりleftButtonDragから呼ばれることがあり、m_cmRasterBrushが無い可能性がある
　---*/
    if (m_cmRasterBrush) {
      double maxThick = m_rasThickness.getValue().second;
      double thickness =
          (m_pressure.getValue())
              ? computeThickness(e.m_pressure, m_rasThickness) * 2
              : maxThick;

      // If we were using FINGER mode before, but stopped mid drag, end previous
      // stroke and switch
      if (m_task == FINGER && !e.isCtrlPressed()) {
        double pressure = m_pressure.getValue() && e.isTablet() ? e.m_pressure : 0.5;
        finishBrush(pressure);
        leftButtonDown(pos, e);
      }

      m_cmRasterBrush->add(TThickPoint(
          m_mousePos + convert(ri->getRaster()->getCenter()), thickness));
      m_tileSaver->save(m_cmRasterBrush->getLastRect());
      TRect modifiedBbox = m_cmRasterBrush->generateLastPieceOfStroke(true);
      invalidate();
    }
  }
}

//-----------------------------------------------------------------------------

void PaintBrushTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (!m_selecting) return;
  m_task = PAINTBRUSH;

  fixMousePos(pos);

  double pressure = m_pressure.getValue() && e.isTablet() ? e.m_pressure : 0.5;

  finishBrush(pressure);
}

//-----------------------------------------------------------------------------

void PaintBrushTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  fixMousePos(pos, true);
  invalidate();
}

//-----------------------------------------------------------------------------

void PaintBrushTool::onEnter() {
  if (m_firstTime) {
    m_onlyEmptyAreas.setValue(PaintBrushSelective ? 1 : 0);
    m_colorType.setValue(::to_wstring(PaintBrushColorType.getValue()));
    m_rasThickness.setValue(
        TDoublePairProperty::Value(PaintBrushSizeMin, PaintBrushSizeMax));
    m_pressure.setValue(PaintBrushPressureSensitivity ? 1 : 0);
    m_modifierLockAlpha.setValue(PaintBrushModifierLockAlpha ? 1 : 0);
    m_firstTime = false;
  }

  m_minThick = m_rasThickness.getValue().first;
  m_maxThick = m_rasThickness.getValue().second;
  m_task     = PAINTBRUSH;

  if ((TToonzImageP)getImage(false))
    m_cursor = ToolCursor::PenCursor;
  else
    m_cursor = ToolCursor::CURSOR_NO;
}

//-----------------------------------------------------------------------------

void PaintBrushTool::onLeave() {
  m_minThick = 0;
  m_maxThick = 0;
  m_task     = PAINTBRUSH;
}

//-----------------------------------------------------------------------------

void PaintBrushTool::onActivate() { onEnter(); }

//-----------------------------------------------------------------------------

void PaintBrushTool::onDeactivate() {
  /*--マウスドラッグ中(m_selecting=true)にツールが切り替わったときに描画の終了処理を行う---*/
  if (m_selecting) finishBrush(1.0);
}

//-----------------------------------------------------------------------------
/*!
 * 描画中にツールが切り替わった場合に備え、onDeactivateでもMouseReleaseと同じ終了処理を行う
 */
void PaintBrushTool::finishBrush(double pressureValue) {
  if (TToonzImageP ti = (TToonzImageP)getImage(true)) {
    if (m_cmRasterBrush) {
      double maxThick = m_rasThickness.getValue().second;
      double thickness =
          (m_pressure.getValue())
              ? computeThickness(pressureValue, m_rasThickness) * 2
              : maxThick;

      if (m_pressure.getValue() && pressureValue == 1.0)
        thickness = m_rasThickness.getValue().first;

      m_cmRasterBrush->add(TThickPoint(
          m_mousePos + convert(ti->getRaster()->getCenter()), thickness));
      m_tileSaver->save(m_cmRasterBrush->getLastRect());
      m_cmRasterBrush->generateLastPieceOfStroke(true, true);

      TTool::Application *app   = TTool::getApplication();
      TXshLevel *level          = app->getCurrentLevel()->getLevel();
      TXshSimpleLevelP simLevel = level->getSimpleLevel();

      /*-- 描画中にフレームが動いても、描画開始時のFidに対してUndoを記録する
       * --*/
      TFrameId frameId =
          m_workingFrameId.isEmptyFrame() ? getCurrentFid() : m_workingFrameId;

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

      TUndoManager::manager()->add(new BrushUndo(
          m_tileSaver->getTileSet(), m_cmRasterBrush->getPointsSequence(true),
          m_colorTypeBrush, m_cmRasterBrush->getStyleId(),
          m_cmRasterBrush->isSelective(), simLevel.getPointer(), frameId,
          m_cmRasterBrush->isAlphaLocked(), dpiScale, symmetryLines, rotation,
          centerPoint, useLineSymmetry));
      ToolUtils::updateSaveBox();

      /*--- FIdを指定して、描画中にフレームが変わっても、
              クリック時のFidのサムネイルが更新されるようにする。---*/
      notifyImageChanged(frameId);

      invalidate();
      delete m_cmRasterBrush;
      m_cmRasterBrush = 0;
      delete m_tileSaver;

      /*--- 作業中のフレームをリセット ---*/
      m_workingFrameId = TFrameId();
    }

    ToolUtils::updateSaveBox();
  }

  m_selecting = false;
}

int PaintBrushTool::getStyleUnderCursor(const TPointD &pos) {
  int modeValue = 2;  // Stylepicker modes: 0=AREAS, 1=LINES, 2=ALL

  TImageP image   = getImage(false);
  TToonzImageP ti = image;
  TXshSimpleLevel *level =
      getApplication()->getCurrentLevel()->getSimpleLevel();
  if (!ti || !level) return -1;

  if (!m_viewer->getGeometry().contains(pos)) return -1;

  int subsampling = level->getImageSubsampling(getCurrentFid());

  StylePicker picker(getViewer()->viewerWidget(), image);

  int styleId =
      picker.pickStyleId(TScale(1.0 / subsampling) * pos,
                         getPixelSize() * getPixelSize(), 1.0, modeValue);

  if (styleId <= 0) return -1;

  return styleId;
}
