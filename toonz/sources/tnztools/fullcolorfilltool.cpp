#include "fullcolorfilltool.h"

#include "toonz/stage2.h"
#include "tools/cursors.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/trasterimageutils.h"
#include "toonz/ttileset.h"
#include "toonz/ttilesaver.h"
#include "toonz/levelproperties.h"
#include "toonz/preferences.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tcolumnhandle.h"

#include "tools/toolhandle.h"
#include "tools/toolutils.h"

#include "tenv.h"
#include "tpalette.h"
#include "tsystem.h"
#include "symmetrytool.h"
#include "tinbetween.h"

using namespace ToolUtils;

TEnv::IntVar FullColorMinFillDepth("InknpaintFullColorMinFillDepth", 4);
TEnv::IntVar FullColorMaxFillDepth("InknpaintFullColorMaxFillDepth", 12);
TEnv::IntVar FullColorFillReferenced("InknpaintFullColorFillReferenced", 0);

TEnv::StringVar FullColorRasterGapSetting("InknpaintFullColorRasterGapSetting",
                                          "Ignore Gaps");
TEnv::IntVar FullColorFillRange("InknpaintFullColorFillRange", 0);

extern TEnv::DoubleVar AutocloseDistance;

namespace {

//=============================================================================
// FullColorFillUndo
//-----------------------------------------------------------------------------

class FullColorFillUndo final : public TFullColorRasterUndo {
  FillParameters m_params;
  bool m_saveboxOnly;
  TXsheet *m_xsheet;
  int m_frameIndex;

public:
  FullColorFillUndo(TTileSetFullColor *tileSet, const FillParameters &params,
                    TXshSimpleLevel *sl, const TFrameId &fid, bool saveboxOnly,
                    TXsheet *xsheet = 0, int frameIndex = -1)
      : TFullColorRasterUndo(tileSet, sl, fid, false, false, 0)
      , m_params(params)
      , m_saveboxOnly(saveboxOnly)
      , m_xsheet(xsheet)
      , m_frameIndex(frameIndex) {}

  void redo() const override {
    TRasterImageP image = getImage();
    if (!image) return;
    TRaster32P r;
    if (m_saveboxOnly) {
      TRectD temp = image->getBBox();
      TRect ttemp = convert(temp);
      r           = image->getRaster()->extract(ttemp);
    } else
      r = image->getRaster();

    fullColorFill(r, m_params, 0, m_xsheet, m_frameIndex);

    TTool::Application *app = TTool::getApplication();
    if (app) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
    }
  }

  int getSize() const override {
    return sizeof(*this) + TFullColorRasterUndo::getSize();
  }

  QString getToolName() override {
    return QString("Fill Tool : %1")
        .arg(QString::fromStdWString(m_params.m_fillType));
  }
  int getHistoryType() override { return HistoryType::FillTool; }
};

//=============================================================================
// doFill
//-----------------------------------------------------------------------------

void doFill(const TImageP &img, const TPointD &pos, FillParameters &params,
            bool isShiftFill, TXshSimpleLevel *sl, const TFrameId &fid,
            TXsheet *xsheet, int frameIndex, bool fillGaps = false,
            bool closeGaps = false, int closeStyleIndex = -1) {
  TTool::Application *app = TTool::getApplication();
  if (!app || !sl) return;

  if (TRasterImageP ri = TRasterImageP(img)) {
    TPoint offs(0, 0);
    TRaster32P ras = ri->getRaster();
    // only accept 32bpp images for now
    if (!ras.getPointer() || ras->isEmpty()) return;

    ras->lock();

    TTileSetFullColor *tileSet = new TTileSetFullColor(ras->getSize());
    TTileSaverFullColor tileSaver(ras, tileSet);
    TDimension imageSize = ras->getSize();
    TPointD p(imageSize.lx % 2 ? 0.0 : 0.5, imageSize.ly % 2 ? 0.0 : 0.5);

    /*-- params.m_p = convert(pos-p)では、マイナス座標でずれが生じる --*/
    TPointD tmp_p = pos - p;
    params.m_p = TPoint((int)floor(tmp_p.x + 0.5), (int)floor(tmp_p.y + 0.5));

    params.m_p += ras->getCenter();
    params.m_p -= offs;
    params.m_shiftFill = isShiftFill;

    TRect rasRect(ras->getSize());
    if (!rasRect.contains(params.m_p)) {
      ras->unlock();
      return;
    }

    fullColorFill(ras, params, &tileSaver, xsheet, frameIndex, fillGaps,
                  closeGaps, closeStyleIndex, AutocloseDistance);

    if (tileSaver.getTileSet()->getTileCount() != 0) {
      static int count = 0;
      TSystem::outputDebug("RASTERFILL" + std::to_string(count++) + "\n");
      if (offs != TPoint())
        for (int i = 0; i < tileSet->getTileCount(); i++) {
          TTileSet::Tile *t = tileSet->editTile(i);
          t->m_rasterBounds = t->m_rasterBounds + offs;
        }
      TUndoManager::manager()->add(
          new FullColorFillUndo(tileSet, params, sl, fid,
                                Preferences::instance()->getFillOnlySavebox()));
    }

    sl->getProperties()->setDirtyFlag(true);

    ras->unlock();
  }

  TTool *t = app->getCurrentTool()->getTool();
  if (t) t->notifyImageChanged();
}
};

//=============================================================================
// FullColorFillTool
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

FullColorFillTool::FullColorFillTool()
    : TTool("T_Fill")
    , m_fillDepth("Fill Depth", 0, 15, 4, 12)
    , m_referenced("Refer Visible", false)
    , m_closeStyleIndex("Style Index:", L"current")  // W_ToolOptions_InkIndex
    , m_rasterGapDistance("Distance:", 1, 100, 10)
    , m_closeRasterGaps("Gaps:")
    , m_frameRange("Frame Range:") 
    , m_currCell(-1, -1) {
  bind(TTool::RasterImage);
  m_prop.bind(m_fillDepth);
  m_prop.bind(m_closeRasterGaps);
  m_prop.bind(m_rasterGapDistance);
  m_prop.bind(m_closeStyleIndex);
  m_prop.bind(m_referenced);
  m_prop.bind(m_frameRange);

  m_closeRasterGaps.setId("CloseGaps");
  m_rasterGapDistance.setId("RasterGapDistance");
  m_closeStyleIndex.setId("RasterGapInkIndex");
  m_frameRange.setId("FrameRange");

  m_closeRasterGaps.addValue(IGNOREGAPS);
  m_closeRasterGaps.addValue(FILLGAPS);
  m_closeRasterGaps.addValue(CLOSEANDFILLGAPS);

  m_frameRange.addValue(L"Off");
  m_frameRange.addValue(LINEAR_INTERPOLATION);
  m_frameRange.addValue(EASE_IN_INTERPOLATION);
  m_frameRange.addValue(EASE_OUT_INTERPOLATION);
  m_frameRange.addValue(EASE_IN_OUT_INTERPOLATION);
}

void FullColorFillTool::updateTranslation() {
  m_fillDepth.setQStringName(tr("Fill Depth"));
  m_referenced.setQStringName(tr("Refer Visible"));
  m_rasterGapDistance.setQStringName(tr("Distance:"));
  m_closeStyleIndex.setQStringName(tr("Style Index:"));
  m_closeRasterGaps.setQStringName(tr("Gaps:"));
  m_closeRasterGaps.setItemUIName(IGNOREGAPS, tr("Ignore Gaps"));
  m_closeRasterGaps.setItemUIName(FILLGAPS, tr("Fill Gaps"));
  m_closeRasterGaps.setItemUIName(CLOSEANDFILLGAPS, tr("Close and Fill"));

  m_frameRange.setQStringName(tr("Frame Range:"));
  m_frameRange.setItemUIName(L"Off", tr("Off"));
  m_frameRange.setItemUIName(LINEAR_INTERPOLATION, tr("Linear"));
  m_frameRange.setItemUIName(EASE_IN_INTERPOLATION, tr("Ease In"));
  m_frameRange.setItemUIName(EASE_OUT_INTERPOLATION, tr("Ease Out"));
  m_frameRange.setItemUIName(EASE_IN_OUT_INTERPOLATION, tr("Ease In/Out"));
}

FillParameters FullColorFillTool::getFillParameters() const {
  FillParameters params;
  int styleId           = TTool::getApplication()->getCurrentLevelStyleIndex();
  params.m_styleId      = styleId;
  params.m_minFillDepth = (int)m_fillDepth.getValue().first;
  params.m_maxFillDepth = (int)m_fillDepth.getValue().second;
  params.m_referenced   = m_referenced.getValue();

  if (m_level) params.m_palette = m_level->getPalette();
  return params;
}

void FullColorFillTool::leftButtonDown(const TPointD &pos,
                                       const TMouseEvent &e) {
  m_clickPoint  = pos;
  TApplication *app = TTool::getApplication();
  TXshLevel *xl = app->getCurrentLevel()->getLevel();
  m_level       = xl ? xl->getSimpleLevel() : 0;
  FillParameters params = getFillParameters();

  int closeStyleIndex = m_closeStyleIndex.getStyleIndex();
  if (closeStyleIndex == -1) {
    closeStyleIndex = app->getCurrentPalette()->getStyleIndex();
  }

  TXsheetHandle *xsh = app->getCurrentXsheet();
  TXsheet *xsheet =
      params.m_referenced && !app->getCurrentFrame()->isEditingLevel() && xsh
          ? xsh->getXsheet()
          : 0;

  if (m_frameRange.getIndex()) {
    bool isEditingLevel = app->getCurrentFrame()->isEditingLevel();

    if (!m_firstClick) {
      m_currCell = std::pair<int, int>(getColumnIndex(), getFrame());

      m_firstClick   = true;
      m_firstPoint   = pos;
      m_firstFrameId = m_veryFirstFrameId = getCurrentFid();
      m_firstFrameIdx = app->getCurrentFrame()->getFrameIndex();

      invalidate();
    } else {
      qApp->processEvents();

      TFrameId fid   = getCurrentFid();
      m_lastFrameIdx = app->getCurrentFrame()->getFrameIndex();

      TImageP image     = getImage(false);
      TToonzImageP ti   = image;
      TRasterCM32P ras  = ti ? ti->getRaster() : TRasterCM32P();
      TPointD rasCenter = ti ? ras->getCenterD() : TPointD(0, 0);
      TPointD dpiScale  = getViewer()->getDpiScale();
      if (isEditingLevel)
        processSequence(pos, params, closeStyleIndex, m_level.getPointer(),
                        m_firstFrameId, fid, m_frameRange.getIndex());
      else
        processSequence(pos, params, closeStyleIndex, xsheet, m_firstFrameIdx,
                        m_lastFrameIdx, m_frameRange.getIndex());
      if (e.isShiftPressed()) {
        m_firstPoint = pos;
        if (isEditingLevel)
          m_firstFrameId = getCurrentFid();
        else {
          m_firstFrameIdx = m_lastFrameIdx;
          app->getCurrentFrame()->setFrame(m_firstFrameIdx);
        }
      } else {
        m_firstClick = false;
        if (app->getCurrentFrame()->isEditingScene()) {
          app->getCurrentColumn()->setColumnIndex(m_currCell.first);
          app->getCurrentFrame()->setFrame(m_currCell.second);
        } else
          app->getCurrentFrame()->setFid(m_veryFirstFrameId);
      }
      TTool *t = app->getCurrentTool()->getTool();
      if (t) t->notifyImageChanged();
    }

    return;
  }

  int frameIndex = app->getCurrentFrame()->getFrameIndex();

  applyFill(getImage(true), pos, params, e.isShiftPressed(),
            m_level.getPointer(), getCurrentFid(), xsheet, frameIndex,
            m_closeRasterGaps.getIndex() > 0, m_closeRasterGaps.getIndex() > 1,
            closeStyleIndex, false);
  invalidate();
}

void FullColorFillTool::leftButtonDrag(const TPointD &pos,
                                       const TMouseEvent &e) {
  if (m_frameRange.getIndex()) return;

  FillParameters params = getFillParameters();
  if (m_clickPoint == pos) return;
  if (!m_level || !params.m_palette) return;
  TImageP img = getImage(true);
  TPixel32 fillColor =
      params.m_palette->getStyle(params.m_styleId)->getMainColor();
  if (TRasterImageP ri = img) {
    TRaster32P ras = ri->getRaster();
    if (!ras) return;
    TPointD center = ras->getCenterD();
    TPoint ipos    = convert(pos + center);
    if (!ras->getBounds().contains(ipos)) return;
    TPixel32 pix = ras->pixels(ipos.y)[ipos.x];
    if (pix == fillColor) {
      invalidate();
      return;
    }
  } else
    return;

  TApplication *app = TTool::getApplication();
  int frameIndex    = app->getCurrentFrame()->getFrameIndex();

  TXsheetHandle *xsh = app->getCurrentXsheet();
  TXsheet *xsheet =
      params.m_referenced && !app->getCurrentFrame()->isEditingLevel() && xsh
          ? xsh->getXsheet()
          : 0;

  int closeStyleIndex = m_closeStyleIndex.getStyleIndex();
  if (closeStyleIndex == -1) {
    closeStyleIndex = app->getCurrentPalette()->getStyleIndex();
  }

  applyFill(img, pos, params, e.isShiftPressed(), m_level.getPointer(),
            getCurrentFid(), xsheet, frameIndex,
            m_closeRasterGaps.getIndex() > 0, m_closeRasterGaps.getIndex() > 1,
            closeStyleIndex, false);

  invalidate();
}

void FullColorFillTool::resetMulti() {
  m_firstClick   = false;
  m_firstFrameId = -1;
  m_firstPoint   = TPointD();
  TXshLevel *xl  = TTool::getApplication()->getCurrentLevel()->getLevel();
  m_level        = xl ? xl->getSimpleLevel() : 0;
}

bool FullColorFillTool::onPropertyChanged(std::string propertyName) {
  // Fill Depth
  if (propertyName == m_fillDepth.getName()) {
    FullColorMinFillDepth = (int)m_fillDepth.getValue().first;
    FullColorMaxFillDepth = (int)m_fillDepth.getValue().second;
  } else if (propertyName == m_rasterGapDistance.getName()) {
    AutocloseDistance       = m_rasterGapDistance.getValue();
    TTool::Application *app = TTool::getApplication();
    // This is a hack to get the viewer to update with the distance.
    app->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
  }

  else if (propertyName == m_closeStyleIndex.getName()) {
  }

  else if (propertyName == m_closeRasterGaps.getName()) {
    FullColorRasterGapSetting = ::to_string(m_closeRasterGaps.getValue());
  }

  // Frame Range
  else if (propertyName == m_frameRange.getName()) {
    FullColorFillRange = m_frameRange.getIndex();
    resetMulti();
  }

  return true;
}

void FullColorFillTool::onActivate() {
  static bool firstTime = true;
  if (firstTime) {
    m_fillDepth.setValue(TDoublePairProperty::Value(FullColorMinFillDepth,
                                                    FullColorMaxFillDepth));
    firstTime = false;
  }

  m_closeRasterGaps.setValue(
      ::to_wstring(FullColorRasterGapSetting.getValue()));
  m_rasterGapDistance.setValue(AutocloseDistance);
  m_frameRange.setIndex(FullColorFillRange);

  resetMulti();
}

int FullColorFillTool::getCursorId() const {
  int ret = ToolCursor::FillCursor;
  if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
    ret = ret | ToolCursor::Ex_Negate;
  return ret;
}

void FullColorFillTool::draw() {
  if (m_frameRange.getIndex() && m_firstClick) {
    tglColor(TPixel::Red);
    drawCross(m_firstPoint, 6);

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    if (symmetryTool && symmetryTool->isGuideEnabled()) {
      TImageP image     = getImage(false);
      TToonzImageP ti   = image;
      TPointD dpiScale  = getViewer()->getDpiScale();
      TRasterCM32P ras  = ti ? ti->getRaster() : TRasterCM32P();
      TPointD rasCenter = ti ? ras->getCenterD() : TPointD(0, 0);
      TPointD fillPt    = m_firstPoint + rasCenter;
      std::vector<TPointD> symmPts =
          symmetryTool->getSymmetryPoints(fillPt, rasCenter, dpiScale);

      for (int i = 1; i < symmPts.size(); i++) {
        drawCross(symmPts[i] - rasCenter, 6);
      }
    }
  }
}

void FullColorFillTool::applyFill(const TImageP &img, const TPointD &pos,
                                  FillParameters &params, bool isShiftFill,
                                  TXshSimpleLevel *sl, const TFrameId &fid,
                                  TXsheet *xsheet, int frameIndex, bool fillGap,
                                  bool closeGap, int closeStyleIndex,
                                  bool undoBlockStarted) {
  TRasterImageP ri = TRasterImageP(img);

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (ri && symmetryTool && symmetryTool->isGuideEnabled()) {
    if (!undoBlockStarted) TUndoManager::manager()->beginBlock();
  }

  doFill(img, pos, params, isShiftFill, sl, fid, xsheet, frameIndex, fillGap,
         closeGap, closeStyleIndex);

  if (ri && symmetryTool && symmetryTool->isGuideEnabled()) {
    TPointD dpiScale  = getViewer()->getDpiScale();
    TRasterP ras      = ri->getRaster();
    TPointD rasCenter = ras ? ras->getCenterD() : TPointD(0, 0);
    TPointD fillPt    = pos + rasCenter;
    std::vector<TPointD> symmPts =
        symmetryTool->getSymmetryPoints(fillPt, rasCenter, dpiScale);

    for (int i = 0; i < symmPts.size(); i++) {
      if (symmPts[i] == fillPt) continue;
      doFill(img, symmPts[i] - rasCenter, params, isShiftFill, sl, fid, xsheet,
             frameIndex, fillGap, closeGap, closeStyleIndex);
    }

    if (!undoBlockStarted) TUndoManager::manager()->endBlock();
  }
}

//-----------------------------------------------------------------------------

void FullColorFillTool::processSequence(const TPointD &pos,
                                        FillParameters &params,
                                        int closeStyleIndex,
                                        TXshSimpleLevel *sl, TFrameId firstFid,
                                        TFrameId lastFid, int multi) {
  TTool::Application *app = TTool::getApplication();

  bool backward           = false;
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

  TUndoManager::manager()->beginBlock();
  for (int i = 0; i < m; ++i) {
    TFrameId fid = fids[i];
    assert(firstFid <= fid && fid <= lastFid);
    TImageP img             = sl->getFrame(fid, true);
    double t                = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t                       = TInbetween::interpolation(t, algorithm);
    if (backward) t         = 1 - t;
    TPointD p               = m_firstPoint * (1 - t) + pos * t;
    if (app) app->getCurrentFrame()->setFid(fid);
    applyFill(img, p, params, false, sl, fid, 0, -1,
              m_closeRasterGaps.getIndex() > 0,
              m_closeRasterGaps.getIndex() > 1, closeStyleIndex, true);
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void FullColorFillTool::processSequence(const TPointD &pos,
                                        FillParameters &params,
                                        int closeStyleIndex, TXsheet *xsheet,
                                        int firstFidx, int lastFidx,
                                        int multi) {
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
    TPointD p          = m_firstPoint * (1 - t) + pos * t;
    if (app) app->getCurrentFrame()->setFrame(row);
    applyFill(img, p, params, false, m_level.getPointer(), fid, xsheet, row,
              m_closeRasterGaps.getIndex() > 0,
              m_closeRasterGaps.getIndex() > 1, closeStyleIndex, true);
  }
  TUndoManager::manager()->endBlock();
}

FullColorFillTool FullColorRasterFillTool;