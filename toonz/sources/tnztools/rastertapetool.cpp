

#include "tools/tool.h"
#include "tundo.h"
#include "tproperty.h"
#include "tools/cursors.h"
#include "toonz/autoclose.h"
#include "ttoonzimage.h"
#include "toonz/toonzimageutils.h"
#include "tenv.h"
#include "tools/toolutils.h"
#include "toonz/txshsimplelevel.h"

#include "toonz/ttileset.h"
#include "toonz/levelproperties.h"
#include "toonz/stage2.h"

#include "tvectorimage.h"
#include "tstroke.h"
#include "drawutil.h"
#include "tinbetween.h"
#include "symmetrytool.h"
#include "vectorbrush.h"
#include "symmetrystroke.h"

#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "tools/toolhandle.h"
#include "toonz/tonionskinmaskhandle.h"

// For Qt translation support
#include <QCoreApplication>

using namespace ToolUtils;

TEnv::StringVar AutocloseVectorType("InknpaintAutocloseVectorType", "Normal");
TEnv::DoubleVar AutocloseDistance("InknpaintAutocloseDistance", 20.0);
TEnv::DoubleVar AutocloseAngle("InknpaintAutocloseAngle", 60.0);
TEnv::IntVar AutocloseRange("InknpaintAutocloseRange", 0);
TEnv::IntVar AutocloseOpacity("InknpaintAutocloseOpacity", 180);
#define NORMAL_CLOSE L"Normal"
#define RECT_CLOSE L"Rectangular"
#define FREEHAND_CLOSE L"Freehand"
#define POLYLINE_CLOSE L"Polyline"

#define LINEAR_INTERPOLATION L"Linear"
#define EASE_IN_INTERPOLATION L"Ease In"
#define EASE_OUT_INTERPOLATION L"Ease Out"
#define EASE_IN_OUT_INTERPOLATION L"Ease In/Out"

namespace {

//============================================================

class AutocloseParameters {
public:
  int m_closingDistance, m_inkIndex, m_opacity;
  double m_spotAngle;

  AutocloseParameters()
      : m_closingDistance(0), m_inkIndex(0), m_spotAngle(0), m_opacity(1) {}
};

//============================================================

class RasterAutocloseUndo final : public TRasterUndo {
  AutocloseParameters m_params;
  std::vector<TAutocloser::Segment> m_segments;

public:
  RasterAutocloseUndo(TTileSetCM32 *tileSet, const AutocloseParameters &params,
                      const std::vector<TAutocloser::Segment> &segments,
                      TXshSimpleLevel *level, const TFrameId &frameId)
      : TRasterUndo(tileSet, level, frameId, false, false, 0)
      , m_segments(segments)
      , m_params(params) {}

  //-------------------------------------------------------------------

  void redo() const override {
    TToonzImageP image = getImage();
    if (!image) return;
    TAutocloser ac(image->getRaster(), m_params.m_closingDistance,
                   m_params.m_spotAngle, m_params.m_inkIndex,
                   m_params.m_opacity);

    ac.draw(m_segments);
    ToolUtils::updateSaveBox();
    /*-- Viewerを更新させるため --*/
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  //-------------------------------------------------------------------

  int getSize() const override {
    return sizeof(*this) + TRasterUndo::getSize();
  }

  QString getToolName() override { return QString("Autoclose Tool"); }
  int getHistoryType() override { return HistoryType::AutocloseTool; }
};

}  // namespace

//============================================================

class RasterTapeTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(RasterTapeTool)

  bool m_selecting;
  TRectD m_selectingRect;
  TRectD m_firstRect;
  TPointD m_firstPoint;
  bool m_firstFrameSelected;
  TXshSimpleLevelP m_level;

  // TBoolProperty  m_isRect;
  TEnumProperty m_closeType;
  TDoubleProperty m_distance;
  TDoubleProperty m_angle;
  TStyleIndexProperty m_inkIndex;
  TIntProperty m_opacity;
  TPropertyGroup m_prop;
  TEnumProperty m_multi;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_firstFrameIdx;
  bool m_isXsheetCell;
  std::pair<int, int> m_currCell;

  // Aggiunte per disegnare il lazzo a la polyline
  VectorBrush m_track;
  TPointD m_firstPos;
  TPointD m_mousePosition;
  double m_thick;
  TStroke *m_stroke;
  std::vector<TStroke *>m_firstStrokes;
  SymmetryStroke m_polyline;
  bool m_firstTime;

public:
  RasterTapeTool()
      : TTool("T_Tape")
      , m_closeType("Type:")                    // W_ToolOptions_CloseType
      , m_distance("Distance:", 1, 100, 10)     // W_ToolOptions_Distance
      , m_angle("Angle:", 1, 180, 60)           // W_ToolOptions_Angle
      , m_inkIndex("Style Index:", L"current")  // W_ToolOptions_InkIndex
      , m_opacity("Opacity:", 1, 255, 255)
      , m_multi("Frame Range:")  // W_ToolOptions_FrameRange
      , m_selecting(false)
      , m_selectingRect()
      , m_firstRect()
      , m_level(0)
      , m_firstFrameSelected(false)
      , m_isXsheetCell(false)
      , m_currCell(-1, -1)
      , m_firstPos()
      , m_mousePosition()
      , m_thick(0.5)
      , m_stroke(0)
      , m_firstTime(true) {
    bind(TTool::ToonzImage);
    m_prop.bind(m_closeType);
    m_closeType.addValue(NORMAL_CLOSE);
    m_closeType.addValue(RECT_CLOSE);
    m_closeType.addValue(FREEHAND_CLOSE);
    m_closeType.addValue(POLYLINE_CLOSE);
    m_prop.bind(m_multi);
    m_multi.addValue(L"Off");
    m_multi.addValue(LINEAR_INTERPOLATION);
    m_multi.addValue(EASE_IN_INTERPOLATION);
    m_multi.addValue(EASE_OUT_INTERPOLATION);
    m_multi.addValue(EASE_IN_OUT_INTERPOLATION);
    m_prop.bind(m_distance);
    m_prop.bind(m_angle);
    m_prop.bind(m_inkIndex);
    m_prop.bind(m_opacity);

    m_multi.setId("FrameRange");
    m_closeType.setId("Type");
  }

  //------------------------------------------------------------

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  //------------------------------------------------------------

  void updateTranslation() override {
    m_closeType.setQStringName(tr("Type:"));
    m_closeType.setItemUIName(NORMAL_CLOSE, tr("Normal"));
    m_closeType.setItemUIName(RECT_CLOSE, tr("Rectangular"));
    m_closeType.setItemUIName(FREEHAND_CLOSE, tr("Freehand"));
    m_closeType.setItemUIName(POLYLINE_CLOSE, tr("Polyline"));

    m_multi.setQStringName(tr("Frame Range:"));
    m_multi.setItemUIName(L"Off", tr("Off"));
    m_multi.setItemUIName(LINEAR_INTERPOLATION, tr("Linear"));
    m_multi.setItemUIName(EASE_IN_INTERPOLATION, tr("Ease In"));
    m_multi.setItemUIName(EASE_OUT_INTERPOLATION, tr("Ease Out"));
    m_multi.setItemUIName(EASE_IN_OUT_INTERPOLATION, tr("Ease In/Out"));

    m_distance.setQStringName(tr("Distance:"));
    m_inkIndex.setQStringName(tr("Style Index:"));
    m_inkIndex.setValue(tr("current").toStdWString());
    m_opacity.setQStringName(tr("Opacity:"));
    m_angle.setQStringName(tr("Angle:"));
  }

  //------------------------------------------------------------

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override {
    if (m_closeType.getValue() == RECT_CLOSE) {
      if (!m_selecting) return;
      m_selectingRect.x1 = pos.x;
      m_selectingRect.y1 = pos.y;

      if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
        m_polyline.clear();
        m_polyline.setRectangle(
            TPointD(m_selectingRect.x0, m_selectingRect.y0),
            TPointD(m_selectingRect.x1, m_selectingRect.y1));
      }

      invalidate();
    } else if (m_closeType.getValue() == FREEHAND_CLOSE) {
      freehandDrag(pos);
      invalidate();
    }
  }

  //------------------------------------------------------------
  /*--  AutoClose Returns true if executed, false otherwise --*/
  bool applyAutoclose(const TToonzImageP &ti, TFrameId id, const TRectD &selRect = TRectD(),
                      TStroke *stroke = 0) {
    if (!ti) return false;
    // inizializzo gli AutocloseParameters
    AutocloseParameters params;
    params.m_closingDistance = (int)(m_distance.getValue());
    params.m_spotAngle       = (int)(m_angle.getValue());
    params.m_opacity         = m_opacity.getValue();
    std::string inkString    = ::to_string(m_inkIndex.getValue());
    int inkIndex =
        TTool::getApplication()
            ->getCurrentLevelStyleIndex();  // TApp::instance()->getCurrentPalette()->getStyleIndex();
    if (isInt(inkString)) inkIndex = std::stoi(inkString);
    params.m_inkIndex              = inkIndex;

    TPoint delta;
    TRasterCM32P ras, raux = ti->getRaster();
    if (m_closeType.getValue() == RECT_CLOSE && raux && !selRect.isEmpty()) {
      TRectD selArea = selRect;
      if (selRect.x0 > selRect.x1) {
        selArea.x1 = selRect.x0;
        selArea.x0 = selRect.x1;
      }
      if (selRect.y0 > selRect.y1) {
        selArea.y1 = selRect.y0;
        selArea.y0 = selRect.y1;
      }
      TRect myRect(ToonzImageUtils::convertWorldToRaster(selArea, ti));
      ras   = raux->extract(myRect);
      delta = myRect.getP00();
    } else if ((m_closeType.getValue() == FREEHAND_CLOSE ||
                m_closeType.getValue() == POLYLINE_CLOSE ||
                m_closeType.getValue() == RECT_CLOSE) &&
               stroke) {
      TRectD selArea = stroke->getBBox();
      TRect myRect(ToonzImageUtils::convertWorldToRaster(selArea, ti));
      ras   = raux->extract(myRect);
      delta = myRect.getP00();
    } else
      ras = raux;
    if (!ras) return false;

    TAutocloser ac(ras, params.m_closingDistance, params.m_spotAngle,
                   params.m_inkIndex, params.m_opacity);

    std::vector<TAutocloser::Segment> segments;
    ac.compute(segments);

    if ((m_closeType.getValue() == FREEHAND_CLOSE ||
         m_closeType.getValue() == POLYLINE_CLOSE) &&
        stroke)
      checkSegments(segments, stroke, raux, delta);

    std::vector<TAutocloser::Segment> segments2(segments);

    /*-- segmentが取得できなければfalseを返す --*/
    if (segments2.empty()) return false;

    int i;
    if (delta != TPoint(0, 0))
      for (i = 0; i < (int)segments2.size(); i++) {
        segments2[i].first += delta;
        segments2[i].second += delta;
      }

    TTileSetCM32 *tileSet = new TTileSetCM32(raux->getSize());
    for (i = 0; i < (int)segments2.size(); i++) {
      TRect bbox(segments2[i].first, segments2[i].second);
      bbox = bbox.enlarge(2);
      tileSet->add(raux, bbox);
    }

    TXshSimpleLevel *sl =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(
        new RasterAutocloseUndo(tileSet, params, segments2, sl, id));
    ac.draw(segments);
    ToolUtils::updateSaveBox();
    return true;
  }

  //------------------------------------------------------------

  inline TRectD interpolateRect(const TRectD &r1, const TRectD &r2, double t) {
    return TRectD(r1.x0 + (r2.x0 - r1.x0) * t, r1.y0 + (r2.y0 - r1.y0) * t,
                  r1.x1 + (r2.x1 - r1.x1) * t, r1.y1 + (r2.y1 - r1.y1) * t);
  }

  //============================================================

  void multiApplyAutoclose(
      TFrameId firstFid, TFrameId lastFid, std::wstring closeType,
      TRectD firstRect, TRectD lastRect,
      std::vector<TStroke *> firstStrokes = std::vector<TStroke *>(),
      std::vector<TStroke *> lastStrokes  = std::vector<TStroke *>()) {
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

    TVectorImageP firstImage;
    TVectorImageP lastImage;
    if ((closeType == FREEHAND_CLOSE || closeType == POLYLINE_CLOSE) &&
        firstStrokes.size() && lastStrokes.size()) {
      firstImage     = new TVectorImage();
      lastImage      = new TVectorImage();
      for (int i = 0; i < firstStrokes.size(); i++)
        firstImage->addStroke(firstStrokes[i]);
      for (int i = 0; i < lastStrokes.size(); i++)
        lastImage->addStroke(lastStrokes[i]);
    }

    enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
    if (m_multi.getIndex() == 2) {  // EASE_IN_INTERPOLATION)
      algorithm = TInbetween::EaseInInterpolation;
    } else if (m_multi.getIndex() == 3) {  // EASE_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseOutInterpolation;
    } else if (m_multi.getIndex() == 4) {  // EASE_IN_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseInOutInterpolation;
    }

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i < m; ++i) {
      TFrameId fid     = fids[i];
      TToonzImageP img = (TToonzImageP)m_level->getFrame(fid, true);
      if (!img) continue;
      double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
      t        = TInbetween::interpolation(t, algorithm);
      if (closeType == RECT_CLOSE)
        applyAutoclose(img, fid, interpolateRect(firstRect, lastRect, t));
      else if ((closeType == FREEHAND_CLOSE || closeType == POLYLINE_CLOSE) &&
               firstStrokes.size() && lastStrokes.size())
        doClose(t, fid, img, firstImage, lastImage);
      m_level->getProperties()->setDirtyFlag(true);
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();

    //		TNotifier::instance()->notify(TLevelChange());
    //		TNotifier::instance()->notify(TStageChange());
  }

  void multiApplyAutoclose(
      int firstFrame, int lastFrame, std::wstring closeType, TRectD firstRect,
      TRectD lastRect,
      std::vector<TStroke *> firstStrokes = std::vector<TStroke *>(),
      std::vector<TStroke *> lastStrokes  = std::vector<TStroke *>()) {
    bool backward = false;
    if (firstFrame > lastFrame) {
      std::swap(firstFrame, lastFrame);
      backward = true;
    }
    assert(firstFrame <= lastFrame);

    TTool::Application *app = TTool::getApplication();
    TFrameId lastFrameId;
    int col = app->getCurrentColumn()->getColumnIndex();
    int row;

    std::vector<std::pair<int, TXshCell>> cellList;

    for (row = firstFrame; row <= lastFrame; row++) {
      TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(row, col);
      if (cell.isEmpty()) continue;
      TFrameId fid = cell.getFrameId();
      if (lastFrameId == fid) continue;  // Skip held cells
      cellList.push_back(std::pair<int, TXshCell>(row, cell));
      lastFrameId = fid;
    }

    int m = cellList.size();

    TVectorImageP firstImage;
    TVectorImageP lastImage;
    if ((closeType == FREEHAND_CLOSE || closeType == POLYLINE_CLOSE) &&
        firstStrokes.size() && lastStrokes.size()) {
      firstImage = new TVectorImage();
      lastImage  = new TVectorImage();
      for (int i = 0; i < firstStrokes.size(); i++)
        firstImage->addStroke(firstStrokes[i]);
      for (int i = 0; i < lastStrokes.size(); i++)
        lastImage->addStroke(lastStrokes[i]);
    }

    enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
    if (m_multi.getIndex() == 2) {  // EASE_IN_INTERPOLATION)
      algorithm = TInbetween::EaseInInterpolation;
    } else if (m_multi.getIndex() == 3) {  // EASE_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseOutInterpolation;
    } else if (m_multi.getIndex() == 4) {  // EASE_IN_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseInOutInterpolation;
    }

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i < m; ++i) {
      row              = cellList[i].first;
      TXshCell cell    = cellList[i].second;
      TFrameId fid     = cell.getFrameId();
      TToonzImageP img = (TToonzImageP)cell.getImage(true);
      if (!img) continue;
      double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
      t        = TInbetween::interpolation(t, algorithm);
      if (closeType == RECT_CLOSE)
        applyAutoclose(img, fid, interpolateRect(firstRect, lastRect, t));
      else if ((closeType == FREEHAND_CLOSE || closeType == POLYLINE_CLOSE) &&
               firstStrokes.size() && lastStrokes.size())
        doClose(t, fid, img, firstImage, lastImage);
      m_level->getProperties()->setDirtyFlag(true);
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }

  //----------------------------------------------------------------------

  void multiApplyAutoclose(TFrameId firstFrameId, TFrameId lastFrameId) {
    int r0 = firstFrameId.getNumber();
    int r1 = lastFrameId.getNumber();

    if (r0 > r1) {
      std::swap(r0, r1);
      std::swap(firstFrameId, lastFrameId);
    }
    if ((r1 - r0) < 2) return;

    TUndoManager::manager()->beginBlock();
    for (int i = r0; i <= r1; ++i) {
      TFrameId fid(i);
      TImageP img = m_level->getFrame(fid, true);
      applyAutoclose(img, fid);
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();

    //		TNotifier::instance()->notify(TLevelChange());
    //		TNotifier::instance()->notify(TStageChange());
  }

  //----------------------------------------------------------------------

  void multiApplyAutoclose(int firstFrameIdx, int lastFrameIdx) {
    if (firstFrameIdx > lastFrameIdx) {
      std::swap(firstFrameIdx, lastFrameIdx);
    }
    if ((lastFrameIdx - firstFrameIdx) < 2) return;

    TTool::Application *app = TTool::getApplication();
    TFrameId lastFrameId;
    int col = app->getCurrentColumn()->getColumnIndex();

    TUndoManager::manager()->beginBlock();
    for (int i = firstFrameIdx; i <= lastFrameIdx; ++i) {
      TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(i, col);
      if (cell.isEmpty()) continue;
      TFrameId fid = cell.getFrameId();
      if (lastFrameId == fid) continue;  // Skip held cells
      TToonzImageP img = (TToonzImageP)cell.getImage(true);
      if (!img) continue;
      lastFrameId = fid;
      applyAutoclose(img, fid);
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }

  //----------------------------------------------------------------------

  void leftButtonUp(const TPointD &pos, const TMouseEvent &e) override {
    TToonzImageP ti = TToonzImageP(getImage(true));
    if (!ti) return;

    /*-- Rectの座標の向きを揃える --*/
    if (m_selectingRect.x0 > m_selectingRect.x1)
      std::swap(m_selectingRect.x1, m_selectingRect.x0);
    if (m_selectingRect.y0 > m_selectingRect.y1)
      std::swap(m_selectingRect.y1, m_selectingRect.y0);

    TTool::Application *app = TTool::getApplication();

    m_selecting = false;
    TRasterCM32P ras;
    if (m_closeType.getValue() == RECT_CLOSE) {
      if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
        // We'll use polyline
        m_polyline.clear();
        m_polyline.setRectangle(
            TPointD(m_selectingRect.x0, m_selectingRect.y0),
            TPointD(m_selectingRect.x1, m_selectingRect.y1));
      }

      if (m_multi.getIndex()) {
        if (m_firstFrameSelected) {
          if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
            // We'll use polyline
            TFrameId tmp = getCurrentFid();
            std::vector<TStroke *> lastStrokes;
            for (int i = 0; i < m_polyline.getBrushCount(); i++)
              lastStrokes.push_back(m_polyline.makeRectangleStroke(i));
            multiAutocloseRegion(lastStrokes, POLYLINE_CLOSE, e);
          } else {
            if (m_isXsheetCell)
              multiApplyAutoclose(m_firstFrameIdx, getFrame(), RECT_CLOSE,
                                  m_firstRect, m_selectingRect);
            else
              multiApplyAutoclose(m_firstFrameId, getFrameId(), RECT_CLOSE,
                                  m_firstRect, m_selectingRect);
          }
          invalidate(m_selectingRect.enlarge(2));
          if (e.isShiftPressed()) {
            m_firstRect     = m_selectingRect;
            m_firstFrameId  = getFrameId();
            m_firstFrameIdx = getFrame();
            m_firstStrokes.clear();
            if (m_polyline.size() > 1) {
              for (int i = 0; i < m_polyline.getBrushCount(); i++)
                m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
            }
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
          // if (m_isXsheetCell)
          m_currCell = std::pair<int, int>(getColumnIndex(), this->getFrame());
        }
        return;
      } else if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
        TUndoManager::manager()->beginBlock();

        TStroke *stroke = m_polyline.makeRectangleStroke();
        TFrameId fid    = getFrameId();
        applyAutoclose(ti, fid, TRectD(), stroke);

        for (int i = 1; i < m_polyline.getBrushCount(); i++) {
          TStroke *symmStroke = m_polyline.makeRectangleStroke(i);
          applyAutoclose(ti, fid, TRectD(), symmStroke);
        }

        TUndoManager::manager()->endBlock();
        invalidate();
      } else {
        /*-- AutoCloseが実行されたか判定する --*/
        TFrameId fid = getFrameId();
        if (!applyAutoclose(ti, fid, m_selectingRect)) {
          if (m_stroke) {
            delete m_stroke;
            m_stroke = 0;
          }
          invalidate();
          return;
        }
      }

      m_selectingRect.empty();
      m_polyline.reset();

      invalidate();
      notifyImageChanged();
    } else if (m_closeType.getValue() == FREEHAND_CLOSE) {
      closeFreehand(pos);
      double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
      if (m_multi.getIndex()) {
        TFrameId tmp = getFrameId();
        if (m_firstStrokes.size() && m_stroke) {
          std::vector<TStroke *> lastStrokes;
          for (int i = 0; i < m_track.getBrushCount(); i++)
            lastStrokes.push_back(m_track.makeStroke(error, i));
          multiAutocloseRegion(lastStrokes, m_closeType.getValue(), e);
        } else {
          m_firstStrokes.clear();
          multiAutocloseRegion(m_firstStrokes, m_closeType.getValue(), e);
        }
      } else {
        if (m_track.hasSymmetryBrushes()) TUndoManager::manager()->beginBlock();

        TFrameId fid = getFrameId();
        applyAutoclose(ti, fid, TRectD(), m_stroke);

        if (m_track.hasSymmetryBrushes()) {
          std::vector<TStroke *> symmStrokes =
              m_track.makeSymmetryStrokes(error);
          for (int i = 0; i < symmStrokes.size(); i++) {
            applyAutoclose(ti, fid, TRectD(), symmStrokes[i]);
          }

          TUndoManager::manager()->endBlock();
        }
      }
      m_track.reset();
      invalidate();
    }
    if (m_stroke) {
      delete m_stroke;
      m_stroke = 0;
    }
  }

  //------------------------------------------------------------

  void draw() override {
    int devPixRatio   = m_viewer->getDevPixRatio();

    double pixelSize2 = getPixelSize() * getPixelSize();
    m_thick           = sqrt(pixelSize2) / 2.0;
//    TPixel color = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg
//                       ? TPixel32::White
//                       : TPixel32::Black;
    TPixel color = TPixel32::Red;

    glLineWidth((1.0 * devPixRatio));

    if (m_closeType.getValue() == RECT_CLOSE) {
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
    if ((m_closeType.getValue() == FREEHAND_CLOSE ||
         m_closeType.getValue() == POLYLINE_CLOSE) &&
        m_multi.getIndex()) {
      tglColor(color);
      for (int i = 0; i < m_firstStrokes.size(); i++)
        drawStrokeCenterline(*m_firstStrokes[i], 1);
    }
    if (m_closeType.getValue() == POLYLINE_CLOSE && !m_polyline.empty()) {
      m_polyline.drawPolyline(m_mousePosition, color);
    } else if (m_closeType.getValue() == FREEHAND_CLOSE && !m_track.isEmpty()) {
      tglColor(color);
      glPushMatrix();
      m_track.drawAllFragments();
      glPopMatrix();
    } else if (m_multi.getIndex() && m_firstFrameSelected) {
      drawCross(m_firstPoint, 5);

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
          drawCross(symmPts[i] - rasCenter, 5);
        }
      }
    }
  }

  //------------------------------------------------------------

  bool onPropertyChanged(std::string propertyName) override {
    if (propertyName == m_closeType.getName()) {
      AutocloseVectorType = ::to_string(m_closeType.getValue());
      resetMulti();
    }

    else if (propertyName == m_distance.getName()) {
      AutocloseDistance       = m_distance.getValue();
      TTool::Application *app = TTool::getApplication();
      // This is a hack to get the viewer to update with the distance.
      app->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
    }

    else if (propertyName == m_angle.getName())
      AutocloseAngle = m_angle.getValue();

    else if (propertyName == m_inkIndex.getName()) {
    }

    else if (propertyName == m_opacity.getName())
      AutocloseOpacity = m_opacity.getValue();

    else if (propertyName == m_multi.getName()) {
      AutocloseRange = m_multi.getIndex();
      resetMulti();
    }

    if (ToonzCheck::instance()->getChecks() & ToonzCheck::eAutoclose)
      notifyImageChanged();

    return true;
  }

  //----------------------------------------------------------------------

  void resetMulti() {
    m_firstFrameSelected = false;
    m_firstRect.empty();
    m_firstPoint = TPointD();
    m_selectingRect.empty();
    TTool::Application *app = TTool::getApplication();
    m_level                 = app->getCurrentLevel()->getLevel()
                  ? app->getCurrentLevel()->getSimpleLevel()
                  : 0;
    m_firstFrameId = m_veryFirstFrameId = getFrameId();
    m_firstFrameIdx                     = getFrame();
    m_firstStrokes.clear();
  }

  //----------------------------------------------------------------------

  void onImageChanged() override {
    if (!m_multi.getIndex()) return;
    TTool::Application *app = TTool::getApplication();
    TXshSimpleLevel *xshl   = 0;
    if (app->getCurrentLevel()->getLevel())
      xshl = app->getCurrentLevel()->getSimpleLevel();

    if (!xshl || m_level.getPointer() != xshl ||
        (m_closeType.getValue() == RECT_CLOSE && m_selectingRect.isEmpty()) ||
        ((m_closeType.getValue() == FREEHAND_CLOSE ||
          m_closeType.getValue() == POLYLINE_CLOSE) &&
         !m_firstStrokes.size()))
      resetMulti();
    else if ((!m_isXsheetCell && m_firstFrameId == getFrameId()) ||
             (m_isXsheetCell && m_firstFrameIdx == getFrame()))
      m_firstFrameSelected = false;  // nel caso sono passato allo stato 1 e
                                     // torno all'immagine iniziale, torno allo
                                     // stato iniziale
    else {                           // cambio stato.
      m_firstFrameSelected = true;
      if (m_closeType.getValue() == RECT_CLOSE) {
        assert(!m_selectingRect.isEmpty());
        m_firstRect = m_selectingRect;
      }
    }
  }

  //----------------------------------------------------------------------

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    TToonzImageP ti = TToonzImageP(getImage(true));
    if (!ti) return;

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    TPointD dpiScale       = getViewer()->getDpiScale();
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();

    if (m_closeType.getValue() == RECT_CLOSE) {
      m_selecting        = true;
      m_selectingRect.x0 = pos.x;
      m_selectingRect.y0 = pos.y;
      m_selectingRect.x1 = pos.x + 1;
      m_selectingRect.y1 = pos.y + 1;

      if (symmetryTool && symmetryTool->isGuideEnabled()) {
        // We'll use polyline
        m_polyline.reset();
        m_polyline.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                      symmObj.getCenterPoint(),
                                      symmObj.isUsingLineSymmetry(), dpiScale);
        m_polyline.setRectangle(
            TPointD(m_selectingRect.x0, m_selectingRect.y0),
            TPointD(m_selectingRect.x1, m_selectingRect.y1));
      }
      return;
    } else if (m_closeType.getValue() == FREEHAND_CLOSE) {
      startFreehand(pos);
      return;
    } else if (m_closeType.getValue() == POLYLINE_CLOSE) {
      if (symmetryTool && symmetryTool->isGuideEnabled() &&
          !m_polyline.hasSymmetryBrushes()) {
        m_polyline.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                      symmObj.getCenterPoint(),
                                      symmObj.isUsingLineSymmetry(), dpiScale);
      }

      addPointPolyline(pos);
      return;
    } else if (m_closeType.getValue() == NORMAL_CLOSE) {
      if (m_multi.getIndex()) {
        TTool::Application *app = TTool::getApplication();
        if (m_firstFrameSelected) {
          if (m_isXsheetCell)
            multiApplyAutoclose(m_firstFrameIdx, getFrame());
          else
            multiApplyAutoclose(m_firstFrameId, getFrameId());
          invalidate();
          if (m_isXsheetCell) {
            app->getCurrentColumn()->setColumnIndex(m_currCell.first);
            app->getCurrentFrame()->setFrame(m_currCell.second);
          } else
            app->getCurrentFrame()->setFid(m_veryFirstFrameId);
          resetMulti();
        } else {
          m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
          // if (m_isXsheetCell)
          m_currCell = std::pair<int, int>(getColumnIndex(), getFrame());
          m_firstFrameSelected = true;
          m_firstPoint         = pos;
        }
        invalidate();
        return;
      }

      m_selecting = false;
      TFrameId fid = getFrameId();
      applyAutoclose(ti, fid);
      invalidate();
      notifyImageChanged();
    }
  }

  //----------------------------------------------------------------------

  void leftButtonDoubleClick(const TPointD &pos,
                             const TMouseEvent &e) override {
    TToonzImageP ti = TToonzImageP(getImage(true));
    if (m_closeType.getValue() == POLYLINE_CLOSE && ti) {
      closePolyline(pos);

      m_stroke = m_polyline.makePolylineStroke();
      assert(m_stroke->getPoint(0) == m_stroke->getPoint(1));
      if (m_multi.getIndex()) {
        if (m_firstStrokes.size()) {
          std::vector<TStroke *> lastStrokes;
          for (int i = 0; i < m_polyline.getBrushCount(); i++)
            lastStrokes.push_back(m_polyline.makePolylineStroke(i));
          multiAutocloseRegion(lastStrokes, m_closeType.getValue(), e);
        } else {
          m_firstStrokes.clear();
          multiAutocloseRegion(m_firstStrokes, m_closeType.getValue(), e);
        }
      } else {
        if (m_polyline.hasSymmetryBrushes())
          TUndoManager::manager()->beginBlock();

        TFrameId fid = getFrameId();
        applyAutoclose(ti, fid, TRectD(), m_stroke);

        if (m_polyline.hasSymmetryBrushes()) {
          for (int i = 1; i < m_polyline.getBrushCount(); i++) {
            TStroke *symmStroke = m_polyline.makePolylineStroke(i);
            applyAutoclose(ti, fid, TRectD(), symmStroke);
          }

          TUndoManager::manager()->endBlock();
        }
      }
      invalidate();
      m_polyline.reset();
    }
    if (m_stroke) {
      delete m_stroke;
      m_stroke = 0;
    }
  }

  //----------------------------------------------------------------------

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override {
    if (m_closeType.getValue() == POLYLINE_CLOSE) {
      m_mousePosition = pos;
      invalidate();
    }
  }

  //----------------------------------------------------------------------

  void onEnter() override {
    //      getApplication()->editImage();
  }

  //----------------------------------------------------------------------

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  //----------------------------------------------------------------------

  void onActivate() override {
    if (m_firstTime) {
      m_closeType.setValue(::to_wstring(AutocloseVectorType.getValue()));
      m_distance.setValue(AutocloseDistance);
      m_angle.setValue(AutocloseAngle);
      m_opacity.setValue(AutocloseOpacity);
      m_multi.setIndex(AutocloseRange);
      m_firstTime = false;
    }
    //			getApplication()->editImage();
    resetMulti();
  }

  //----------------------------------------------------------------------

  void onDeactivate() override {}

  //----------------------------------------------------------------------

  int getCursorId() const override {
    int ret = ToolCursor::TapeCursor;

    if (m_closeType.getValue() == FREEHAND_CLOSE)
      ret = ret | ToolCursor::Ex_FreeHand;
    else if (m_closeType.getValue() == POLYLINE_CLOSE)
      ret = ret | ToolCursor::Ex_PolyLine;
    else if (m_closeType.getValue() == RECT_CLOSE)
      ret = ret | ToolCursor::Ex_Rectangle;

    if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
      ret = ret | ToolCursor::Ex_Negate;

    return ret;
  }

  //----------------------------------------------------------------------

  //! Viene aggiunto \b pos a \b m_track e disegnato il primo pezzetto del
  //! lazzo. Viene inizializzato \b m_firstPos
  void startFreehand(const TPointD &pos) {
    m_track.reset();

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    TPointD dpiScale       = getViewer()->getDpiScale();
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();

    if (symmetryTool && symmetryTool->isGuideEnabled()) {
      m_track.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                 symmObj.getCenterPoint(),
                                 symmObj.isUsingLineSymmetry(), dpiScale);
    }

    m_firstPos        = pos;
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
  }

  //------------------------------------------------------------------

  //! Viene aggiunto \b pos a \b m_track e disegnato un altro pezzetto del
  //! lazzo.
  void freehandDrag(const TPointD &pos) {
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(pos, m_thick), pixelSize2);
  }

  //------------------------------------------------------------------

  //! Viene chiuso il lazzo (si aggiunge l'ultimo punto ad m_track) e viene
  //! creato lo stroke rappresentante il lazzo.
  void closeFreehand(const TPointD &pos) {
    if (m_track.isEmpty()) return;
    double pixelSize2 = getPixelSize() * getPixelSize();
    m_track.add(TThickPoint(m_firstPos, m_thick), pixelSize2);
    m_track.filterPoints();
    double error = (30.0 / 11) * sqrt(pixelSize2);
    m_stroke     = m_track.makeStroke(error);
    m_stroke->setStyle(1);
  }

  //------------------------------------------------------------------

  //! Viene aggiunto un punto al vettore m_polyline.
  void addPointPolyline(const TPointD &pos) {
    m_firstPos = pos;
    m_polyline.push_back(pos);
  }

  //------------------------------------------------------------------

  //! Agginge l'ultimo pos a \b m_polyline e chiude la spezzata (aggiunge \b
  //! m_polyline.front() alla fine del vettore)
  void closePolyline(const TPointD &pos) {
    if (m_polyline.size() <= 1) return;
    if (m_polyline.back() != pos) m_polyline.push_back(pos);
    if (m_polyline.back() != m_polyline.front())
      m_polyline.push_back(m_polyline.front());
    invalidate();
  }

  //-------------------------------------------------------------------

  //! Elimina i segmenti che non sono contenuti all'interno dello stroke!!!
  void checkSegments(std::vector<TAutocloser::Segment> &segments,
                     TStroke *stroke, const TRasterCM32P &ras,
                     const TPoint &delta) {
    TVectorImage vi;
    TStroke *app = new TStroke();
    *app         = *stroke;
    app->transform(TTranslation(convert(ras->getCenter())));
    vi.addStroke(app);
    vi.findRegions();
    std::vector<TAutocloser::Segment>::iterator it = segments.begin();
    for (; it < segments.end(); it++) {
      if (it == segments.end()) break;

      int i;
      bool isContained = false;
      for (i = 0; i < (int)vi.getRegionCount(); i++) {
        TRegion *reg = vi.getRegion(i);
        if (reg->contains(convert(it->first + delta)) &&
            reg->contains(convert(it->second + delta))) {
          isContained = true;
          break;
        }
      }
      if (!isContained) {
        it = segments.erase(it);
        if (it != segments.end() && it != segments.begin())
          it--;
        else if (it == segments.end())
          break;
      }
    }
  }

  //-------------------------------------------------------------------

  void multiAutocloseRegion(std::vector<TStroke *> laststrokes,
                            const std::wstring eraseType,
                            const TMouseEvent &e) {
    TTool::Application *app = TTool::getApplication();
    if (m_firstStrokes.size()) {
      if (m_isXsheetCell)
        multiApplyAutoclose(m_firstFrameIdx, getFrame(), eraseType, TRectD(),
                            TRectD(), m_firstStrokes, laststrokes);
      else
        multiApplyAutoclose(m_firstFrameId, getFrameId(), eraseType, TRectD(),
                            TRectD(), m_firstStrokes, laststrokes);
      invalidate();
      if (e.isShiftPressed()) {
        m_firstStrokes.clear();
        if (eraseType == POLYLINE_CLOSE) {
          if (m_polyline.size() > 1) {
            for (int i = 0; i < m_polyline.getBrushCount(); i++)
              m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
          }
        } else {
          double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
          for (int i = 0; i < m_track.getBrushCount(); i++)
            m_firstStrokes.push_back(m_track.makeStroke(error, i));
        }
        m_firstFrameId  = getFrameId();
        m_firstFrameIdx = getFrame();
      } else {
        if (m_isXsheetCell) {
          app->getCurrentColumn()->setColumnIndex(m_currCell.first);
          app->getCurrentFrame()->setFrame(m_currCell.second);
        } else
          app->getCurrentFrame()->setFid(m_veryFirstFrameId);
        resetMulti();
      }
    } else {
      m_isXsheetCell = app->getCurrentFrame()->isEditingScene();
      // if (m_isXsheetCell)
      m_currCell    = std::pair<int, int>(getColumnIndex(), getFrame());
      m_firstStrokes.clear();
      if (eraseType == POLYLINE_CLOSE) {
        if (m_polyline.size() > 1) {
          for (int i = 0; i < m_polyline.getBrushCount(); i++)
            m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
        }
      } else {
        double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
        for (int i = 0; i < m_track.getBrushCount(); i++)
          m_firstStrokes.push_back(m_track.makeStroke(error, i));
      }
    }
    return;
  }

  //------------------------------------------------------------------------

  void doClose(double t, TFrameId fid, const TImageP &img, const TVectorImageP &firstImage,
               const TVectorImageP &lastImage) {
    if (t == 0)
      for (int i = 0; i < firstImage->getStrokeCount(); i++)
        applyAutoclose(img, fid, TRectD(), firstImage->getStroke(i));
    else if (t == 1)
      for (int i = 0; i < lastImage->getStrokeCount(); i++)
        applyAutoclose(img, fid, TRectD(), lastImage->getStroke(i));
    else {
//      assert(firstImage->getStrokeCount() == 1);
//      assert(lastImage->getStrokeCount() == 1);
      TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
//      assert(vi->getStrokeCount() == 1);
      for (int i = 0; i < vi->getStrokeCount(); i++)
        applyAutoclose(img, fid, TRectD(), vi->getStroke(i));
    }
  }

  //-------------------------------------------------------------------

} rasterTapeTool;
