#include "tenv.h"
#include "tproperty.h"

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "toonz/txsheethandle.h"
#include "tools/toolhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "tools/strokeselection.h"

#include "tmathutil.h"
#include "tstroke.h"
#include "tools/cursors.h"
#include "tundo.h"
#include "tvectorimage.h"
#include "tthreadmessage.h"

#include "toonzqt/imageutils.h"
#include "toonzqt/tselectionhandle.h"

#include "tgl.h"

using namespace ToolUtils;

TEnv::IntVar SnapAtIntersection("CutterToolSnapAtIntersection", 0);

//=============================================================================
namespace {

//=============================================================================
// UndoCutter
//-----------------------------------------------------------------------------

class UndoCutter final : public ToolUtils::TToolUndo {
  int m_newStrokeId1;
  int m_newStrokeId2;
  int m_pos;

  VIStroke *m_oldStroke;

  std::vector<TFilledRegionInf> *m_fillInformation;
  std::vector<DoublePair> *m_sortedWRanges;

  int m_row;
  int m_column;

public:
  UndoCutter(TXshSimpleLevel *level, const TFrameId &frameId,
             VIStroke *oldStroke, int pos, int newStrokeId1, int newStrokeId2,
             std::vector<TFilledRegionInf> *fillInformation,
             std::vector<DoublePair> *sortedWRanges)
      : TToolUndo(level, frameId)
      , m_oldStroke(oldStroke)
      , m_newStrokeId1(newStrokeId1)
      , m_newStrokeId2(newStrokeId2)
      , m_pos(pos)
      , m_fillInformation(fillInformation)
      , m_sortedWRanges(sortedWRanges) {
    TTool::Application *app = TTool::getApplication();
    if (app) {
      m_row    = app->getCurrentFrame()->getFrame();
      m_column = app->getCurrentColumn()->getColumnIndex();
    }
  }

  ~UndoCutter() {
    deleteVIStroke(m_oldStroke);
    delete m_sortedWRanges;
    delete m_fillInformation;
  }

  void undo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;
    if (dynamic_cast<StrokeSelection *>(
            TTool::getApplication()->getCurrentSelection()->getSelection()))
      TTool::getApplication()->getCurrentSelection()->setSelection(0);

    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentColumn()->setColumnIndex(m_column);
      app->getCurrentFrame()->setFrame(m_row);
    } else
      app->getCurrentFrame()->setFid(m_frameId);
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(!!image);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    VIStroke *stroke;

    stroke = image->getStrokeById(m_newStrokeId1);
    if (stroke) image->deleteStroke(stroke);

    stroke = image->getStrokeById(m_newStrokeId2);
    if (stroke) image->deleteStroke(stroke);

    stroke = cloneVIStroke(m_oldStroke);

    image->insertStrokeAt(stroke, m_pos);

    UINT size = m_fillInformation->size();
    if (!size) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
      return;
    }

    image->findRegions();
    TRegion *reg;
    for (UINT i = 0; i < size; i++) {
      reg = image->getRegion((*m_fillInformation)[i].m_regionId);
      assert(reg);
      if (reg) reg->setStyle((*m_fillInformation)[i].m_styleId);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;
    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentColumn()->setColumnIndex(m_column);
      app->getCurrentFrame()->setFrame(m_row);
    } else
      app->getCurrentFrame()->setFid(m_frameId);
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(!!image);
    if (!image) return;
    QMutexLocker lock(image->getMutex());

    bool isSelfLoop = image->getStroke(m_pos)->isSelfLoop();
    image->splitStroke(m_pos, *m_sortedWRanges);

    image->getStroke(m_pos)->setId(m_newStrokeId1);
    if (!isSelfLoop && m_sortedWRanges->size() == 2)
      image->getStroke(m_pos + 1)->setId(m_newStrokeId2);

    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           m_fillInformation->capacity() * sizeof(TFilledRegionInf) + 500;
  }

  QString getToolName() override { return QString("Cutter Tool"); }
};

//=============================================================================
// CutterTool
//-----------------------------------------------------------------------------

class CutterTool final : public TTool {
public:
  bool m_mouseDown;

  TPointD m_vTan;

  TThickPoint m_cursor;
  TPointD m_speed;
  int m_cursorId;
  double m_pW;

  int m_lockedStrokeIndex;

  TPropertyGroup m_prop;
  TBoolProperty m_snapAtIntersection;

  CutterTool()
      : TTool("T_Cutter")
      , m_mouseDown(false)
      , m_cursorId(ToolCursor::CutterCursor)
      , m_snapAtIntersection("Snap At Intersection", false) {
    bind(TTool::VectorImage);
    m_prop.bind(m_snapAtIntersection);
    m_snapAtIntersection.setId("Snap");
  }

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void draw() override {
    // TAffine viewMatrix = getViewer()->getViewMatrix();
    // glPushMatrix();
    // tglMultMatrix(viewMatrix);

    int devPixRatio = m_viewer->getDevPixRatio();

    const double pixelSize = getPixelSize();

    double len = m_cursor.thick + 15 * pixelSize;

    if (m_speed != TPointD(0, 0)) {
      TPointD v = m_speed;
      TPointD p = (TPointD)m_cursor;

      v = rotate90(v);
      v = normalize(v);

      v = v * (len);

      glLineWidth(1.0 * devPixRatio);

      tglColor(TPixelD(0.1, 0.9, 0.1));
      tglDrawSegment(p - v, p + v);
    }
    // glPopMatrix();
  }

  double getNearestSnapAtIntersection(TStroke *selfStroke, double w) {
    TVectorImageP vi = TImageP(getImage(false));
    if (!vi) {
      return w;
    }

    std::vector<DoublePair> intersections;
    int i, strokeNumber = vi->getStrokeCount();
    double diff;
    double nearestW = 1000;
    double minDiff  = 1000;

    // check self intersection first
    intersect(selfStroke, selfStroke, intersections, false);
    for (auto &intersection : intersections) {
      if (areAlmostEqual(intersection.first, 0, 1e-6)) {
        continue;
      }
      if (areAlmostEqual(intersection.second, 1, 1e-6)) {
        continue;
      }

      diff = std::abs(intersection.first - w);
      if (diff < minDiff) {
        minDiff  = diff;
        nearestW = intersection.first;
      }

      diff = std::abs(intersection.second - w);
      if (diff < minDiff) {
        minDiff  = diff;
        nearestW = intersection.second;
      }

      if (selfStroke->isSelfLoop()) {
        diff = std::abs(1 - intersection.first) + w;
        if (diff < minDiff) {
          minDiff  = diff;
          nearestW = intersection.first;
        }

        diff = intersection.first + std::abs(1 - w);
        if (diff < minDiff) {
          minDiff  = diff;
          nearestW = intersection.first;
        }

        diff = std::abs(1 - intersection.second) + w;
        if (diff < minDiff) {
          minDiff  = diff;
          nearestW = intersection.second;
        }

        diff = intersection.second + std::abs(1 - w);
        if (diff < minDiff) {
          minDiff  = diff;
          nearestW = intersection.second;
        }
      }
    }

    for (i = 0; i < strokeNumber; ++i) {
      TStroke *stroke = vi->getStroke(i);
      if (stroke == selfStroke) {
        continue;
      }

      intersect(selfStroke, stroke, intersections, false);
      for (auto &intersection : intersections) {
        diff = std::abs(intersection.first - w);
        if (diff < minDiff) {
          minDiff  = diff;
          nearestW = intersection.first;
        }

        if (selfStroke->isSelfLoop()) {
          diff = std::abs(1 - intersection.first) + w;
          if (diff < minDiff) {
            minDiff  = diff;
            nearestW = intersection.first;
          }

          diff = intersection.first + std::abs(1 - w);
          if (diff < minDiff) {
            minDiff  = diff;
            nearestW = intersection.first;
          }
        }
      }
    }

    if (nearestW >= 0 && nearestW <= 1) {
      return nearestW;
    }
    return w;
  }

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override {
    if (getViewer() && getViewer()->getGuidedStrokePickerMode()) {
      getViewer()->doPickGuideStroke(pos);
      return;
    }

    TVectorImageP vi = TImageP(getImage(true));
    if (!vi) return;
    QMutexLocker sl(vi->getMutex());

    double dist, pW;
    UINT strokeIndex;

    TStroke *strokeRef;

    if (getNearestStrokeWithLock(pos, pW, strokeIndex, dist,
                                 e.isCtrlPressed()) &&
        pW >= 0 && pW <= 1) {
      double w;

      strokeRef = vi->getStroke(strokeIndex);

      double hitPointLen = strokeRef->getLength(pW);
      double totalLen    = strokeRef->getLength();

      double len = hitPointLen;

      if (!strokeRef->isSelfLoop()) {
        if (len < TConsts::epsilon)
          w = 0;
        else
          w = strokeRef->getParameterAtLength(len);

        if (len > totalLen - TConsts::epsilon)
          w = 1;
        else
          w = strokeRef->getParameterAtLength(len);
      } else {
        if (len < 0) len += totalLen;

        if (len > totalLen) len -= totalLen;

        w = strokeRef->getParameterAtLength(len);
      }

      if (m_snapAtIntersection.getValue()) {
        w = getNearestSnapAtIntersection(strokeRef, w);
      }

      std::vector<DoublePair> *sortedWRanges = new std::vector<DoublePair>;

      if (strokeRef->isSelfLoop()) {
        sortedWRanges->push_back(std::make_pair(0, w));
        sortedWRanges->push_back(std::make_pair(w, 1));
      } else {
        if (w == 0 || w == 1)
          sortedWRanges->push_back(std::make_pair(0, 1));
        else {
          sortedWRanges->push_back(std::make_pair(0, w));
          sortedWRanges->push_back(std::make_pair(w, 1));
        }
      }

      std::vector<TFilledRegionInf> *fillInformation =
          new std::vector<TFilledRegionInf>;
      ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                       strokeRef->getBBox());

      VIStroke *oldStroke = cloneVIStroke(vi->getVIStroke(strokeIndex));
      bool isSelfLoop     = vi->getStroke(strokeIndex)->isSelfLoop();
      vi->splitStroke(strokeIndex, *sortedWRanges);

      TUndo *nundo;

      TXshSimpleLevel *sl =
          TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
      assert(sl);
      TFrameId id = getCurrentFid();
      if (isSelfLoop || sortedWRanges->size() == 1) {
        nundo = new UndoCutter(sl, id, oldStroke, strokeIndex,
                               vi->getStroke(strokeIndex)->getId(), -1,
                               fillInformation, sortedWRanges);
      } else {
        assert(strokeIndex + 1 < vi->getStrokeCount());
        nundo = new UndoCutter(sl, id, oldStroke, strokeIndex,
                               vi->getStroke(strokeIndex)->getId(),
                               vi->getStroke(strokeIndex + 1)->getId(),
                               fillInformation, sortedWRanges);
      }

      TUndoManager::manager()->add(nundo);

      invalidate();
      notifyImageChanged();
    }
    invalidate();
  }

  void mouseMove(const TPointD &pos, const TMouseEvent &e) override {
    TVectorImageP vi = TImageP(getImage(true));
    if (!vi) {
      m_speed = TPointD(0, 0);
      return;
    }

    // select nearest stroke and finds its parameter
    double dist, pW;
    UINT stroke;

    if (getNearestStrokeWithLock(pos, pW, stroke, dist, e.isCtrlPressed())) {
      TStroke *strokeRef = vi->getStroke(stroke);

      if (m_snapAtIntersection.getValue()) {
        pW = getNearestSnapAtIntersection(strokeRef, pW);
      }

      m_speed  = strokeRef->getSpeed(pW);
      m_cursor = strokeRef->getThickPoint(pW);
      m_pW     = pW;
    } else {
      m_speed = TPointD(0, 0);
    }
    invalidate();
  }

  void onLeave() override { m_speed = TPointD(0, 0); }

  void onActivate() override {
    m_snapAtIntersection.setValue(SnapAtIntersection ? 1 : 0);
  }
  void onEnter() override {
    if ((TVectorImageP)getImage(false))
      m_cursorId = ToolCursor::CutterCursor;
    else
      m_cursorId = ToolCursor::CURSOR_NO;
  }

  int getCursorId() const override {
    if (m_viewer && m_viewer->getGuidedStrokePickerMode())
      return m_viewer->getGuidedStrokePickerCursor();
    return m_cursorId;
  }

  void updateTranslation() override {
    m_snapAtIntersection.setQStringName(QObject::tr("Snap At Intersection"));
  }

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  bool onPropertyChanged(std::string propertyName) override {
    SnapAtIntersection = (int)(m_snapAtIntersection.getValue());
    return true;
  }

  bool getNearestStrokeWithLock(const TPointD &p, double &outW,
                                UINT &strokeIndex, double &dist2, bool lock) {
    TVectorImageP vi = TImageP(getImage(false));
    if (!vi) return false;

    if (m_lockedStrokeIndex >= vi->getStrokeCount()) {
      m_lockedStrokeIndex = -1;
    }

    if (lock && m_lockedStrokeIndex >= 0) {
      TStroke *stroke = vi->getStroke(m_lockedStrokeIndex);
      strokeIndex     = m_lockedStrokeIndex;
      return stroke->getNearestW(p, outW, dist2);
    }

    UINT index;
    if (vi->getNearestStroke(p, outW, index, dist2)) {
      m_lockedStrokeIndex = index;
      strokeIndex         = index;
      return true;
    }

    return false;
  }

} cutterTool;

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

// TTool *getCutterTool() {return &cutterTool;}
