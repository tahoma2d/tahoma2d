

#include "tools/tool.h"
#include "tools/toolutils.h"
#include "tthreadmessage.h"
#include "tgl.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "tmathutil.h"
#include "tools/cursors.h"
#include "tproperty.h"
#include "symmetrytool.h"
#include "symmetrystroke.h"

#include "toonzqt/imageutils.h"

#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobject.h"
#include "tools/toolhandle.h"
#include "toonz/stage2.h"
#include "tenv.h"
#include "tinbetween.h"

// For Qt translation support
#include <QCoreApplication>

using namespace ToolUtils;

#define POINT2POINT L"Endpoint to Endpoint"
#define POINT2LINE L"Endpoint to Line"
#define LINE2LINE L"Line to Line"
#define NORMAL L"Normal"
#define RECT L"Rectangular"

#define LINEAR_INTERPOLATION L"Linear"
#define EASE_IN_INTERPOLATION L"Ease In"
#define EASE_OUT_INTERPOLATION L"Ease Out"
#define EASE_IN_OUT_INTERPOLATION L"Ease In/Out"

TEnv::StringVar TapeMode("InknpaintTapeMode1", "Endpoint to Endpoint");
TEnv::IntVar TapeSmooth("InknpaintTapeSmooth", 0);
TEnv::IntVar TapeJoinStrokes("InknpaintTapeJoinStrokes", 0);
TEnv::StringVar TapeType("InknpaintTapeType1", "Normal");
TEnv::DoubleVar AutocloseFactor("InknpaintAutocloseFactor", 4.0);
TEnv::IntVar TapeRange("InknpaintTapeRange", 0);

namespace {

class UndoAutoclose final : public ToolUtils::TToolUndo {
  int m_oldStrokeId1;
  int m_oldStrokeId2;
  int m_pos1, m_pos2;

  VIStroke *m_oldStroke1;
  VIStroke *m_oldStroke2;

  std::vector<TFilledRegionInf> *m_fillInformation;

  int m_row;
  int m_column;
  std::vector<int> m_changedStrokes;

public:
  VIStroke *m_newStroke;
  int m_newStrokeId;
  int m_newStrokePos;

  UndoAutoclose(TXshSimpleLevel *level, const TFrameId &frameId, int pos1,
                int pos2, std::vector<TFilledRegionInf> *fillInformation,
                const std::vector<int> &changedStrokes)
      : ToolUtils::TToolUndo(level, frameId)
      , m_oldStroke1(0)
      , m_oldStroke2(0)
      , m_pos1(pos1)
      , m_pos2(pos2)
      , m_newStrokePos(-1)
      , m_fillInformation(fillInformation)
      , m_changedStrokes(changedStrokes) {
    TVectorImageP image = level->getFrame(m_frameId, true);
    if (pos1 != -1) {
      m_oldStrokeId1 = image->getStroke(pos1)->getId();
      m_oldStroke1   = cloneVIStroke(image->getVIStroke(pos1));
    }
    if (pos2 != -1 && pos1 != pos2 && image) {
      m_oldStrokeId2 = image->getStroke(pos2)->getId();
      m_oldStroke2   = cloneVIStroke(image->getVIStroke(pos2));
    }
    TTool::Application *app = TTool::getApplication();
    if (app) {
      m_row    = app->getCurrentFrame()->getFrame();
      m_column = app->getCurrentColumn()->getColumnIndex();
    }
  }

  ~UndoAutoclose() {
    deleteVIStroke(m_newStroke);
    if (m_oldStroke1) deleteVIStroke(m_oldStroke1);
    if (m_oldStroke2) deleteVIStroke(m_oldStroke2);
    if (m_isLastInBlock) delete m_fillInformation;
  }

  void undo() const override {
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
    int strokeIndex = image->getStrokeIndexById(m_newStrokeId);
    if (strokeIndex != -1) image->removeStroke(strokeIndex);

    if (m_oldStroke1)
      image->insertStrokeAt(cloneVIStroke(m_oldStroke1), m_pos1);
    if (m_oldStroke2)
      image->insertStrokeAt(cloneVIStroke(m_oldStroke2), m_pos2);

    image->notifyChangedStrokes(m_changedStrokes, std::vector<TStroke *>());

    if (!m_isLastInBlock) return;

    for (UINT i = 0; i < m_fillInformation->size(); i++) {
      TRegion *reg = image->getRegion((*m_fillInformation)[i].m_regionId);
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
    if (m_oldStroke1) {
      int strokeIndex = image->getStrokeIndexById(m_oldStrokeId1);
      if (strokeIndex != -1) image->removeStroke(strokeIndex);
    }

    if (m_oldStroke2) {
      int strokeIndex = image->getStrokeIndexById(m_oldStrokeId2);
      if (strokeIndex != -1) image->removeStroke(strokeIndex);
    }

    VIStroke *stroke = cloneVIStroke(m_newStroke);
    image->insertStrokeAt(stroke, m_pos1 == -1 ? m_newStrokePos : m_pos1,
                          false);

    image->notifyChangedStrokes(m_changedStrokes, std::vector<TStroke *>());

    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           m_fillInformation->capacity() * sizeof(TFilledRegionInf) + 500;
  }

  QString getToolName() override { return QString("Autoclose Tool"); }
  int getHistoryType() override { return HistoryType::AutocloseTool; }
};

}  // namespace

//=============================================================================
// Autoclose Tool
//-----------------------------------------------------------------------------

class VectorTapeTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(VectorTapeTool)

  bool m_secondPoint;
  int m_strokeIndex1, m_strokeIndex2;
  double m_w1, m_w2, m_pixelSize;
  TPointD m_pos;
  bool m_firstTime;
  TRectD m_selectionRect;
  TPointD m_startRect;

  TBoolProperty m_smooth;
  TBoolProperty m_joinStrokes;
  TEnumProperty m_mode;
  TPropertyGroup m_prop;
  TDoubleProperty m_autocloseFactor;
  TEnumProperty m_type;
  TEnumProperty m_multi;

  SymmetryStroke m_polyline;

  TRectD m_firstRect;
  bool m_firstFrameSelected;
  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_firstFrameIdx;
  std::pair<int, int> m_currCell;
  SymmetryStroke m_firstPolyline;
  TXshSimpleLevelP m_level;

public:
  VectorTapeTool()
      : TTool("T_Tape")
      , m_secondPoint(false)
      , m_strokeIndex1(-1)
      , m_strokeIndex2(-1)
      , m_w1(-1.0)
      , m_w2(-1.0)
      , m_pixelSize(1)
      , m_smooth("Smooth", false)  // W_ToolOptions_Smooth
      , m_joinStrokes("JoinStrokes", false)
      , m_mode("Mode")
      , m_type("Type")
      , m_autocloseFactor("Distance", 0.1, 100, 0.5)
      , m_firstTime(true)
      , m_selectionRect()
      , m_startRect()
      , m_multi("Frame Range:")
      , m_firstFrameSelected(false)
      , m_currCell(-1, -1)
      , m_level(0) {
    bind(TTool::Vectors);

    m_prop.bind(m_type);
    m_prop.bind(m_mode);
    m_prop.bind(m_multi);
    m_multi.addValue(L"Off");
    m_multi.addValue(LINEAR_INTERPOLATION);
    m_multi.addValue(EASE_IN_INTERPOLATION);
    m_multi.addValue(EASE_OUT_INTERPOLATION);
    m_multi.addValue(EASE_IN_OUT_INTERPOLATION);
    m_prop.bind(m_autocloseFactor);
    m_prop.bind(m_joinStrokes);
    m_prop.bind(m_smooth);

    m_mode.addValue(POINT2POINT);
    m_mode.addValue(POINT2LINE);
    m_mode.addValue(LINE2LINE);
    m_smooth.setId("Smooth");
    m_type.addValue(NORMAL);
    m_type.addValue(RECT);

    m_mode.setId("Mode");
    m_type.setId("Type");
    m_joinStrokes.setId("JoinVectors");
    m_autocloseFactor.setId("Distance");
    m_multi.setId("FrameRange");
  }

  //-----------------------------------------------------------------------------

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  //-----------------------------------------------------------------------------

  bool onPropertyChanged(std::string propertyName) override {
    TapeMode       = ::to_string(m_mode.getValue());
    TapeSmooth     = (int)(m_smooth.getValue());
    TapeRange      = m_multi.getIndex();
    std::wstring s = m_type.getValue();
    if (!s.empty()) TapeType = ::to_string(s);
    TapeJoinStrokes = (int)(m_joinStrokes.getValue());
    AutocloseFactor = (double)(m_autocloseFactor.getValue());
    m_selectionRect = TRectD();
    m_startRect     = TPointD();

    if (propertyName == "Distance" &&
        (ToonzCheck::instance()->getChecks() & ToonzCheck::eAutoclose))
      notifyImageChanged();
    return true;
  }

  //-----------------------------------------------------------------------------
  void updateTranslation() override {
    m_smooth.setQStringName(tr("Smooth"));
    m_joinStrokes.setQStringName(tr("Join Vectors"));
    m_autocloseFactor.setQStringName(tr("Distance"));

    m_mode.setQStringName(tr("Mode:"));
    m_mode.setItemUIName(POINT2POINT, tr("Endpoint to Endpoint"));
    m_mode.setItemUIName(POINT2LINE, tr("Endpoint to Line"));
    m_mode.setItemUIName(LINE2LINE, tr("Line to Line"));

    m_type.setQStringName(tr("Type:"));
    m_type.setItemUIName(NORMAL, tr("Normal"));
    m_type.setItemUIName(RECT, tr("Rectangular"));

    m_multi.setQStringName(tr("Frame Range:"));
    m_multi.setItemUIName(L"Off", tr("Off"));
    m_multi.setItemUIName(LINEAR_INTERPOLATION, tr("Linear"));
    m_multi.setItemUIName(EASE_IN_INTERPOLATION, tr("Ease In"));
    m_multi.setItemUIName(EASE_OUT_INTERPOLATION, tr("Ease Out"));
    m_multi.setItemUIName(EASE_IN_OUT_INTERPOLATION, tr("Ease In/Out"));
  }

  //-----------------------------------------------------------------------------

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  void drawConnection(TThickPoint point1, TThickPoint point2) {
    tglColor(TPixelD(0.1, 0.9, 0.1));

    m_pixelSize  = getPixelSize();
    double thick = std::max(6.0 * m_pixelSize, point1.thick);

    tglDrawCircle(point1, thick);

    if (point2 == TThickPoint()) return;

    thick = std::max(6.0 * m_pixelSize, point2.thick);

    tglDrawCircle(point2, thick);
    tglDrawSegment(point1, point2);
  }

  void draw() override {
    TVectorImageP vi(getImage(false));
    if (!vi) return;

    // TAffine viewMatrix = getViewer()->getViewMatrix();
    // glPushMatrix();
    // tglMultMatrix(viewMatrix);
    TPixel color = TPixel32::Red;
    if (m_type.getValue() == RECT) {
      if (m_multi.getIndex() && m_firstFrameSelected) {
        if (m_firstPolyline.size() > 1) {
          m_firstPolyline.drawRectangle(color);
        } else
          ToolUtils::drawRect(m_firstRect, color, 0x3F33, true);
      }

      if (!m_selectionRect.isEmpty())
        if (m_polyline.size() > 1) {
          m_polyline.drawRectangle(color);
        } else
          ToolUtils::drawRect(m_selectionRect, color, 0x3F33, true);
      return;
    }

    if (m_strokeIndex1 == -1 || m_strokeIndex1 >= (int)(vi->getStrokeCount()))
      return;

    TStroke *stroke1   = vi->getStroke(m_strokeIndex1);
    TThickPoint point1 = stroke1->getPoint(m_w1);
    // TThickPoint point1 = stroke1->getControlPoint(m_cpIndex1);
    TThickPoint point2;

    if (m_secondPoint) {
      if (m_strokeIndex2 != -1) {
        TStroke *stroke2 = vi->getStroke(m_strokeIndex2);
        point2           = stroke2->getPoint(m_w2);
      } else {
        point2 = TThickPoint(m_pos, 4 * m_pixelSize);
      }
    }

    drawConnection(point1, point2);

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    if (symmetryTool && !symmetryTool->isGuideEnabled()) return;

    TPointD dpiScale       = getViewer()->getDpiScale();

    std::vector<TPointD> firstPts = symmetryTool->getSymmetryPoints(
        point1, TPointD(), getViewer()->getDpiScale());
    std::vector<TPointD> secondPts = symmetryTool->getSymmetryPoints(
        point2, TPointD(), getViewer()->getDpiScale());

    for (int i = 1; i < firstPts.size(); i++) {
      int strokeIndex1 = vi->getStrokeIndexAtPos(firstPts[i], getPixelSize());
      if (strokeIndex1 == -1) continue;
      drawConnection(firstPts[i], secondPts[i]);
    }
    // glPopMatrix();
  }

  //-----------------------------------------------------------------------------

  void findFirstStroke(TPointD pos, TVectorImageP vi, int &strokeIndex1,
                       double &w1) {
    double minDistance2 = 10000000000.;

    int i, strokeNumber = vi->getStrokeCount();

    TStroke *stroke;
    double distance2, outW;
    TPointD point;
    int cpMax;

    for (i = 0; i < strokeNumber; i++) {
      stroke = vi->getStroke(i);
      if (m_mode.getValue() == LINE2LINE) {
        if (stroke->getNearestW(pos, outW, distance2) &&
            distance2 < minDistance2) {
          minDistance2 = distance2;
          strokeIndex1 = i;
          if (areAlmostEqual(outW, 0.0, 1e-3))
            w1 = 0.0;
          else if (areAlmostEqual(outW, 1.0, 1e-3))
            w1 = 1.0;
          else
            w1 = outW;
        }
      } else if (!stroke->isSelfLoop()) {
        point = stroke->getControlPoint(0);
        if ((distance2 = tdistance2(pos, point)) < minDistance2) {
          minDistance2 = distance2;
          strokeIndex1 = i;
          w1           = 0.0;
        }

        cpMax = stroke->getControlPointCount() - 1;
        point = stroke->getControlPoint(cpMax);
        if ((distance2 = tdistance2(pos, point)) < minDistance2) {
          minDistance2 = distance2;
          strokeIndex1 = i;
          w1           = 1.0;
        }
      }
    }
  }

  //-----------------------------------------------------------------------------

  void findSecondStroke(TPointD pos, TVectorImageP vi, int strokeIndex1,
                        double w1, int &strokeIndex, double &w) {
    double minDistance2 = 900 * m_pixelSize;

    int i, strokeNumber = vi->getStrokeCount();

    TStroke *stroke;
    double distance2, outW;
    TPointD point;
    int cpMax;

    for (i = 0; i < strokeNumber; i++) {
      if (!vi->sameGroup(strokeIndex1, i) &&
          (vi->isStrokeGrouped(strokeIndex1) || vi->isStrokeGrouped(i)))
        continue;
      stroke = vi->getStroke(i);
      if (m_mode.getValue() != POINT2POINT) {
        if (stroke->getNearestW(pos, outW, distance2) &&
            distance2 < minDistance2) {
          minDistance2 = distance2;
          strokeIndex  = i;
          if (areAlmostEqual(outW, 0.0, 1e-3))
            w = 0.0;
          else if (areAlmostEqual(outW, 1.0, 1e-3))
            w = 1.0;
          else
            w = outW;
        }
      }

      if (!stroke->isSelfLoop()) {
        cpMax = stroke->getControlPointCount() - 1;
        if (!(strokeIndex1 == i && (w1 == 0.0 || cpMax < 3))) {
          point = stroke->getControlPoint(0);
          if ((distance2 = tdistance2(pos, point)) < minDistance2) {
            minDistance2 = distance2;
            strokeIndex  = i;
            w            = 0.0;
          }
        }

        if (!(strokeIndex1 == i && (w1 == 1.0 || cpMax < 3))) {
          point = stroke->getControlPoint(cpMax);
          if ((distance2 = tdistance2(pos, point)) < minDistance2) {
            minDistance2 = distance2;
            strokeIndex  = i;
            w            = 1.0;
          }
        }
      }
    }
  }

  //-----------------------------------------------------------------------------

  void mouseMove(const TPointD &pos, const TMouseEvent &) override {
    TVectorImageP vi(getImage(false));
    if (!vi) return;

    if (m_type.getValue() == RECT) return;

    m_strokeIndex1 = -1;
    m_secondPoint  = false;

    findFirstStroke(pos, vi, m_strokeIndex1, m_w1);

    invalidate();
  }

  //-----------------------------------------------------------------------------

  void leftButtonDown(const TPointD &pos, const TMouseEvent &) override {
    if (!(TVectorImageP)getImage(false)) return;

    if (m_type.getValue() == RECT) {
      SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
          TTool::getTool("T_Symmetry", TTool::RasterImage));
      TPointD dpiScale       = getViewer()->getDpiScale();
      SymmetryObject symmObj = symmetryTool->getSymmetryObject();

      m_startRect = pos;

      if (symmetryTool && symmetryTool->isGuideEnabled()) {
        m_selectionRect = TRectD(m_startRect.x, m_startRect.y,
                                 m_startRect.x + 1, m_startRect.y + 1);

        // We'll use polyline
        m_polyline.reset();
        m_polyline.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                      symmObj.getCenterPoint(),
                                      symmObj.isUsingLineSymmetry(), dpiScale);
        m_polyline.setRectangle(
            TPointD(m_selectionRect.x0, m_selectionRect.y0),
            TPointD(m_selectionRect.x1, m_selectionRect.y1));
      }
    } else if (m_strokeIndex1 != -1)
      m_secondPoint = true;
  }

  //-----------------------------------------------------------------------------

  void leftButtonDrag(const TPointD &pos, const TMouseEvent &) override {
    TVectorImageP vi(getImage(false));
    if (!vi) return;

    if (m_type.getValue() == RECT) {
      m_selectionRect = TRectD(
          std::min(m_startRect.x, pos.x), std::min(m_startRect.y, pos.y),
          std::max(m_startRect.x, pos.x), std::max(m_startRect.y, pos.y));

      if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
        m_polyline.clear();
        m_polyline.setRectangle(
            TPointD(m_selectionRect.x0, m_selectionRect.y0),
            TPointD(m_selectionRect.x1, m_selectionRect.y1));
      }

      invalidate();
      return;
    }

    if (m_strokeIndex1 == -1 || !m_secondPoint) return;

    m_strokeIndex2 = -1;

    findSecondStroke(pos, vi, m_strokeIndex1, m_w1, m_strokeIndex2, m_w2);

    m_pos = pos;

    invalidate();
  }

  //-----------------------------------------------------------------------------

  void joinPointToPoint(const TVectorImageP &vi,
                        std::vector<TFilledRegionInf> *fillInfo) {
    int minindex = std::min(m_strokeIndex1, m_strokeIndex2);
    int maxindex = std::max(m_strokeIndex1, m_strokeIndex2);

    UndoAutoclose *autoCloseUndo = 0;
    TUndo *undo                  = 0;
    if (TTool::getApplication()->getCurrentObject()->isSpline())
      undo =
          new UndoPath(getXsheet()->getStageObject(getObjectId())->getSpline());
    else {
      TXshSimpleLevel *level =
          TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
      std::vector<int> v(1);
      v[0]          = minindex;
      autoCloseUndo = new UndoAutoclose(level, getCurrentFid(), minindex,
                                        maxindex, fillInfo, v);
    }
    VIStroke *newStroke = vi->joinStroke(
        m_strokeIndex1, m_strokeIndex2,
        (m_w1 == 0.0)
            ? 0
            : vi->getStroke(m_strokeIndex1)->getControlPointCount() - 1,
        (m_w2 == 0.0)
            ? 0
            : vi->getStroke(m_strokeIndex2)->getControlPointCount() - 1,
        m_smooth.getValue());

    if (autoCloseUndo) {
      autoCloseUndo->m_newStroke   = cloneVIStroke(newStroke);
      autoCloseUndo->m_newStrokeId = vi->getStroke(minindex)->getId();
      undo                         = autoCloseUndo;
    }

    vi->notifyChangedStrokes(minindex);
    notifyImageChanged();
    TUndoManager::manager()->add(undo);
  }

  //-----------------------------------------------------------------------------

  void joinPointToLine(const TVectorImageP &vi,
                       std::vector<TFilledRegionInf> *fillInfo) {
    TUndo *undo                  = 0;
    UndoAutoclose *autoCloseUndo = 0;
    if (TTool::getApplication()->getCurrentObject()->isSpline())
      undo =
          new UndoPath(getXsheet()->getStageObject(getObjectId())->getSpline());
    else {
      std::vector<int> v(2);
      v[0] = m_strokeIndex1;
      v[1] = m_strokeIndex2;
      TXshSimpleLevel *level =
          TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
      autoCloseUndo = new UndoAutoclose(level, getCurrentFid(), m_strokeIndex1,
                                        -1, fillInfo, v);
    }

    VIStroke *newStroke;

    newStroke = vi->extendStroke(
        m_strokeIndex1, vi->getStroke(m_strokeIndex2)->getThickPoint(m_w2),
        (m_w1 == 0.0)
            ? 0
            : vi->getStroke(m_strokeIndex1)->getControlPointCount() - 1,
        m_smooth.getValue());

    if (autoCloseUndo) {
      autoCloseUndo->m_newStroke   = cloneVIStroke(newStroke);
      autoCloseUndo->m_newStrokeId = vi->getStroke(m_strokeIndex1)->getId();
      undo                         = autoCloseUndo;
    }

    vi->notifyChangedStrokes(m_strokeIndex1);
    notifyImageChanged();
    TUndoManager::manager()->add(undo);
  }

  //-----------------------------------------------------------------------------

  void joinLineToLine(const TVectorImageP &vi,
                      std::vector<TFilledRegionInf> *fillInfo) {
    if (TTool::getApplication()->getCurrentObject()->isSpline())
      return;  // Caanot add vectros to spline... Spline can be only one
               // std::vector

    TThickPoint p1 = vi->getStroke(m_strokeIndex1)->getThickPoint(m_w1);
    TThickPoint p2 = vi->getStroke(m_strokeIndex2)->getThickPoint(m_w2);

    UndoAutoclose *autoCloseUndo = 0;
    std::vector<int> v(2);
    v[0] = m_strokeIndex1;
    v[1] = m_strokeIndex2;

    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    autoCloseUndo =
        new UndoAutoclose(level, getCurrentFid(), -1, -1, fillInfo, v);

    std::vector<TThickPoint> points(3);
    points[0]          = p1;
    points[1]          = 0.5 * (p1 + p2);
    points[2]          = p2;
    TStroke *auxStroke = new TStroke(points);
    auxStroke->setStyle(TTool::getApplication()->getCurrentLevelStyleIndex());
    auxStroke->outlineOptions() =
        vi->getStroke(m_strokeIndex1)->outlineOptions();

    int pos = vi->addStrokeToGroup(auxStroke, m_strokeIndex1);
    if (pos < 0) return;
    VIStroke *newStroke = vi->getVIStroke(pos);

    autoCloseUndo->m_newStrokePos = pos;
    autoCloseUndo->m_newStroke    = cloneVIStroke(newStroke);
    autoCloseUndo->m_newStrokeId  = vi->getStroke(pos)->getId();
    vi->notifyChangedStrokes(v, std::vector<TStroke *>());
    notifyImageChanged();
    TUndoManager::manager()->add(autoCloseUndo);
  }

  //-----------------------------------------------------------------------------

  void inline rearrangeClosingPoints(const TVectorImageP &vi,
                                     std::pair<int, double> &closingPoint,
                                     const TPointD &p) {
    int erasedIndex = std::max(m_strokeIndex1, m_strokeIndex2);
    int joinedIndex = std::min(m_strokeIndex1, m_strokeIndex2);

    if (closingPoint.first == joinedIndex)
      closingPoint.second = vi->getStroke(joinedIndex)->getW(p);
    else if (closingPoint.first == erasedIndex) {
      closingPoint.first  = joinedIndex;
      closingPoint.second = vi->getStroke(joinedIndex)->getW(p);
    } else if (closingPoint.first > erasedIndex)
      closingPoint.first--;
  }

  //-------------------------------------------------------------------------------------

#define p2p 1
#define p2l 2
#define l2p 3
#define l2l 4

  inline TRectD interpolateRect(const TRectD &r1, const TRectD &r2, double t) {
    return TRectD(r1.x0 + (r2.x0 - r1.x0) * t, r1.y0 + (r2.y0 - r1.y0) * t,
                  r1.x1 + (r2.x1 - r1.x1) * t, r1.y1 + (r2.y1 - r1.y1) * t);
  }

  void tapeRect(const TVectorImageP &vi, const TRectD &rect,
                bool undoBlockStarted) {
    std::vector<TFilledRegionInf> *fillInformation =
        new std::vector<TFilledRegionInf>;
    ImageUtils::getFillingInformationOverlappingArea(vi, *fillInformation,
                                                     rect);

    bool initUndoBlock = false;

    std::vector<std::pair<int, double>> startPoints, endPoints;
    getClosingPoints(rect, m_autocloseFactor.getValue(), vi, startPoints,
                     endPoints);

    assert(startPoints.size() == endPoints.size());

    std::vector<TPointD> startP(startPoints.size()), endP(startPoints.size());

    if (!startPoints.empty()) {
      if (!undoBlockStarted) TUndoManager::manager()->beginBlock();
      for (UINT i = 0; i < startPoints.size(); i++) {
        startP[i] = vi->getStroke(startPoints[i].first)
                        ->getPoint(startPoints[i].second);
        endP[i] =
            vi->getStroke(endPoints[i].first)->getPoint(endPoints[i].second);
      }
    }

    for (UINT i = 0; i < startPoints.size(); i++) {
      m_strokeIndex1 = startPoints[i].first;
      m_strokeIndex2 = endPoints[i].first;
      m_w1           = startPoints[i].second;
      m_w2           = endPoints[i].second;
      int type       = doTape(vi, fillInformation, m_joinStrokes.getValue());
      if (type == p2p && m_strokeIndex1 != m_strokeIndex2) {
        for (UINT j = i + 1; j < startPoints.size(); j++) {
          rearrangeClosingPoints(vi, startPoints[j], startP[j]);
          rearrangeClosingPoints(vi, endPoints[j], endP[j]);
        }
      } else if (type == p2l ||
                 (type == p2p && m_strokeIndex1 == m_strokeIndex2)) {
        for (UINT j = i + 1; j < startPoints.size(); j++) {
          if (startPoints[j].first == m_strokeIndex1)
            startPoints[j].second =
                vi->getStroke(m_strokeIndex1)->getW(startP[j]);
          if (endPoints[j].first == m_strokeIndex1)
            endPoints[j].second = vi->getStroke(m_strokeIndex1)->getW(endP[j]);
        }
      }
    }
    if (!startPoints.empty() && !undoBlockStarted)
      TUndoManager::manager()->endBlock();
  }

  //----------------------------------------------------------------------

  void multiTapeRect(TFrameId firstFrameId, TFrameId lastFrameId) {
    TTool::Application *app = TTool::getApplication();

    bool backward = false;
    if (firstFrameId > lastFrameId) {
      std::swap(firstFrameId, lastFrameId);
      backward = true;
    }
    assert(firstFrameId <= lastFrameId);
    std::vector<TFrameId> allFids;
    m_level->getFids(allFids);

    std::vector<TFrameId>::iterator i0 = allFids.begin();
    while (i0 != allFids.end() && *i0 < firstFrameId) i0++;
    if (i0 == allFids.end()) return;
    std::vector<TFrameId>::iterator i1 = i0;
    while (i1 != allFids.end() && *i1 <= lastFrameId) i1++;
    assert(i0 < i1);
    std::vector<TFrameId> fids(i0, i1);
    int m = fids.size();
    assert(m > 0);

    enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
    if (m_multi.getIndex() == 2) {  // EASE_IN_INTERPOLATION)
      algorithm = TInbetween::EaseInInterpolation;
    } else if (m_multi.getIndex() == 3) {  // EASE_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseOutInterpolation;
    } else if (m_multi.getIndex() == 4) {  // EASE_IN_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseInOutInterpolation;
    }

    TUndoManager::manager()->beginBlock();
    for (int i = 0; i <= m; ++i) {
      TFrameId fid     = fids[i];
      TVectorImageP vi = (TVectorImageP)m_level->getFrame(fid, true);
      if (!vi) continue;
      double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
      t        = TInbetween::interpolation(t, algorithm);
      app->getCurrentFrame()->setFid(fid);

      tapeRect(vi, interpolateRect(m_firstRect, m_selectionRect, t), true);

      if (m_firstPolyline.hasSymmetryBrushes()) {
        for (int i = 1; i < m_firstPolyline.getBrushCount(); i++) {
          TStroke *firstSymmStroke = m_firstPolyline.makeRectangleStroke(i);
          TRectD firstSymmSelectionRect = firstSymmStroke->getBBox();
          TStroke *symmStroke           = m_polyline.makeRectangleStroke(i);
          TRectD symmSelectionRect      = symmStroke->getBBox();
          tapeRect(
              vi, interpolateRect(firstSymmSelectionRect, symmSelectionRect, t),
              true);
        }
      }
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }

  //----------------------------------------------------------------------

  void multiTapeRect(int firstFrameIdx, int lastFrameIdx) {
    bool backward = false;
    if (firstFrameIdx > lastFrameIdx) {
      std::swap(firstFrameIdx, lastFrameIdx);
      backward = true;
    }
    assert(firstFrameIdx <= lastFrameIdx);

    TTool::Application *app = TTool::getApplication();
    TFrameId lastFrameId;
    int col = app->getCurrentColumn()->getColumnIndex();
    int row;

    std::vector<std::pair<int, TXshCell>> cellList;

    for (row = firstFrameIdx; row <= lastFrameIdx; row++) {
      TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(row, col);
      if (cell.isEmpty()) continue;
      TFrameId fid = cell.getFrameId();
      if (lastFrameId == fid) continue;  // Skip held cells
      cellList.push_back(std::pair<int, TXshCell>(row, cell));
      lastFrameId = fid;
    }

    int m = cellList.size();

    enum TInbetween::TweenAlgorithm algorithm = TInbetween::LinearInterpolation;
    if (m_multi.getIndex() == 2) {  // EASE_IN_INTERPOLATION)
      algorithm = TInbetween::EaseInInterpolation;
    } else if (m_multi.getIndex() == 3) {  // EASE_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseOutInterpolation;
    } else if (m_multi.getIndex() == 4) {  // EASE_IN_OUT_INTERPOLATION)
      algorithm = TInbetween::EaseInOutInterpolation;
    }

    TUndoManager::manager()->beginBlock();
    for (int i = firstFrameIdx; i <= lastFrameIdx; ++i) {
      for (int i = 0; i < m; ++i) {
        row              = cellList[i].first;
        TXshCell cell    = cellList[i].second;
        TFrameId fid     = cell.getFrameId();
        TVectorImageP vi = (TVectorImageP)cell.getImage(true);
        if (!vi) continue;
        double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
        t        = TInbetween::interpolation(t, algorithm);
        app->getCurrentFrame()->setFrame(row);

        tapeRect(vi, interpolateRect(m_firstRect, m_selectionRect, t), true);

        if (m_firstPolyline.hasSymmetryBrushes()) {
          for (int i = 1; i < m_firstPolyline.getBrushCount(); i++) {
            TStroke *firstSymmStroke = m_firstPolyline.makeRectangleStroke(i);
            TRectD firstSymmSelectionRect = firstSymmStroke->getBBox();
            TStroke *symmStroke           = m_polyline.makeRectangleStroke(i);
            TRectD symmSelectionRect      = symmStroke->getBBox();
            tapeRect(vi, interpolateRect(firstSymmSelectionRect,
                                         symmSelectionRect, t),
                     true);
          }
        }
      }
    }
    TUndoManager::manager()->endBlock();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int doTape(const TVectorImageP &vi,
             std::vector<TFilledRegionInf> *fillInformation, bool joinStrokes) {
    int type;
    if (!joinStrokes)
      type = l2l;
    else {
      type = (m_w1 == 0.0 || m_w1 == 1.0)
                 ? ((m_w2 == 0.0 || m_w2 == 1.0) ? p2p : p2l)
                 : ((m_w2 == 0.0 || m_w2 == 1.0) ? l2p : l2l);
      if (type == l2p) {
        std::swap(m_strokeIndex1, m_strokeIndex2);
        std::swap(m_w1, m_w2);
        type = p2l;
      }
    }

    switch (type) {
    case p2p:
      joinPointToPoint(vi, fillInformation);
      break;
    case p2l:
      joinPointToLine(vi, fillInformation);
      break;
    case l2l:
      joinLineToLine(vi, fillInformation);
      break;
    default:
      assert(false);
      break;
    }
    return type;
  }

  void tapeStrokes(TVectorImageP vi, int strokeIndex1, int strokeIndex2) {
    m_strokeIndex1 = strokeIndex1;
    m_strokeIndex2 = strokeIndex2;

    QMutexLocker lock(vi->getMutex());
    std::vector<TFilledRegionInf> *fillInformation =
        new std::vector<TFilledRegionInf>;
    ImageUtils::getFillingInformationOverlappingArea(
        vi, *fillInformation, vi->getStroke(strokeIndex1)->getBBox() +
                                  vi->getStroke(strokeIndex2)->getBBox());

    doTape(vi, fillInformation, m_joinStrokes.getValue());
  }

  //-------------------------------------------------------------------------------

  void leftButtonUp(const TPointD &, const TMouseEvent &e) override {
    TTool::Application *app = TTool::getApplication();

    TVectorImageP vi(getImage(true));

    if (vi && m_type.getValue() == RECT) {
      bool isEditingLevel = app->getCurrentFrame()->isEditingLevel();

      if (m_multi.getIndex()) {
        if (!m_firstFrameSelected) {
          m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
          m_firstRect    = m_selectionRect;
          m_firstFrameId = m_veryFirstFrameId = getFrameId();
          m_firstFrameIdx                     = getFrame();
          m_firstPolyline                     = m_polyline;
          m_level = app->getCurrentLevel()->getLevel()
                        ? app->getCurrentLevel()->getSimpleLevel()
                        : 0;
          m_firstFrameSelected = true;
        } else {
          if (app->getCurrentFrame()->isEditingScene())
            multiTapeRect(m_firstFrameIdx, getFrame());
          else
            multiTapeRect(m_firstFrameId, getFrameId());

          invalidate(m_selectionRect.enlarge(2));
          if (e.isShiftPressed()) {
            m_currCell     = std::pair<int, int>(getColumnIndex(), getFrame());
            m_firstRect    = m_selectionRect;
            m_firstFrameId = m_veryFirstFrameId = getFrameId();
            m_firstFrameIdx                     = getFrame();
            m_firstPolyline                     = m_polyline;
          } else {
            if (app->getCurrentFrame()->isEditingScene()) {
              app->getCurrentColumn()->setColumnIndex(m_currCell.first);
              app->getCurrentFrame()->setFrame(m_currCell.second);
            } else
              app->getCurrentFrame()->setFid(m_veryFirstFrameId);
            m_firstFrameSelected = false;
          }

          m_selectionRect = TRectD();
          m_startRect     = TPointD();
          m_polyline.reset();
        }
        return;
      }

      if (m_polyline.hasSymmetryBrushes())
        TUndoManager::manager()->beginBlock();

      tapeRect(vi, m_selectionRect, m_polyline.hasSymmetryBrushes());

      if (m_polyline.hasSymmetryBrushes()) {
        for (int i = 1; i < m_polyline.getBrushCount(); i++) {
          TStroke *symmStroke      = m_polyline.makeRectangleStroke(i);
          TRectD symmSelectionRect = symmStroke->getBBox();
          tapeRect(vi, symmSelectionRect, true);
        }

        TUndoManager::manager()->endBlock();
      }

      m_selectionRect = TRectD();
      m_startRect     = TPointD();
      m_polyline.reset();
      notifyImageChanged();
      invalidate();
      return;
    }

    if (!vi || m_strokeIndex1 == -1 || !m_secondPoint || m_strokeIndex2 == -1) {
      m_strokeIndex1 = -1;
      m_strokeIndex2 = -1;
      m_w1           = -1.0;
      m_w2           = -1.0;
      m_secondPoint  = false;
      return;
    }

    m_secondPoint = false;

    SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
        TTool::getTool("T_Symmetry", TTool::RasterImage));
    if (symmetryTool && symmetryTool->isGuideEnabled())
      TUndoManager::manager()->beginBlock();

    TStroke *stroke1   = vi->getStroke(m_strokeIndex1);
    TStroke *stroke2   = vi->getStroke(m_strokeIndex2);
    TThickPoint point1 = stroke1->getPoint(m_w1);
    TThickPoint point2 = stroke2->getPoint(m_w2);

    tapeStrokes(vi, m_strokeIndex1, m_strokeIndex2);

    if (symmetryTool && symmetryTool->isGuideEnabled()) {
      std::vector<TPointD> firstPts = symmetryTool->getSymmetryPoints(
          point1, TPointD(), getViewer()->getDpiScale());
      std::vector<TPointD> secondPts = symmetryTool->getSymmetryPoints(
          point2, TPointD(), getViewer()->getDpiScale());

      for (int i = 1; i < firstPts.size(); i++) {
        int strokeIndex1 = vi->getStrokeIndexAtPos(firstPts[i], getPixelSize());
        if (strokeIndex1 == -1) continue;
        int strokeIndex2 =
            vi->getStrokeIndexAtPos(secondPts[i], getPixelSize());
        if (strokeIndex2 == -1) continue;
        tapeStrokes(vi, strokeIndex1, strokeIndex2);
      }

      TUndoManager::manager()->endBlock();
    }

    invalidate();

    m_strokeIndex2 = -1;
    m_w1           = -1.0;
    m_w2           = -1.0;
  }

  //-----------------------------------------------------------------------------

  void onEnter() override {
    //      getApplication()->editImage();
    //    m_selectionRect = TRectD();
    m_startRect     = TPointD();
  }

  void onActivate() override {
    if (!m_firstTime) return;

    std::wstring s = ::to_wstring(TapeMode.getValue());
    if (s != L"") m_mode.setValue(s);
    s = ::to_wstring(TapeType.getValue());
    if (s != L"") m_type.setValue(s);
    m_autocloseFactor.setValue(AutocloseFactor);
    m_smooth.setValue(TapeSmooth ? 1 : 0);
    m_joinStrokes.setValue(TapeJoinStrokes ? 1 : 0);
    m_multi.setIndex(TapeRange);
    m_firstTime     = false;
    m_selectionRect = TRectD();
    m_startRect     = TPointD();
  }

  int getCursorId() const override {
    int ret = ToolCursor::TapeCursor;
    if (m_type.getValue() == RECT) ret = ret | ToolCursor::Ex_Rectangle;
    if (ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg)
      ret = ret | ToolCursor::Ex_Negate;
    return ret;
  }

} vectorTapeTool;

// TTool *getAutocloseTool() {return &autocloseTool;}
