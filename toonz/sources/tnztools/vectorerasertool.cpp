

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/toolutils.h"
#include "tools/tool.h"
#include "tools/cursors.h"

// TnzQt includes
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/stage2.h"
#include "toonz/preferences.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tmathutil.h"
#include "tundo.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "ttoonzimage.h"
#include "tproperty.h"
#include "tgl.h"
#include "tinbetween.h"
#include "drawutil.h"
#include "symmetrytool.h"
#include "vectorbrush.h"
#include "symmetrystroke.h"

// Qt includes
#include <QCoreApplication>  // For Qt translation support

using namespace ToolUtils;

TEnv::DoubleVar EraseVectorMinSize("InknpaintEraseVectorMinSize", 1);
TEnv::DoubleVar EraseVectorSize("InknpaintEraseVectorSize", 10);
TEnv::StringVar EraseVectorType("InknpaintEraseVectorType", "Normal");
TEnv::IntVar EraseVectorSelective("InknpaintEraseVectorSelective", 0);
TEnv::IntVar EraseVectorInvert("InknpaintEraseVectorInvert", 0);
TEnv::IntVar EraseVectorRange("InknpaintEraseVectorRange", 0);
TEnv::IntVar EraseVectorPressure("InknpaintEraseVectorPressure", 1);

//=============================================================================
// Eraser Tool
//=============================================================================

namespace {

#define NORMAL_ERASE L"Normal"
#define RECT_ERASE L"Rectangular"
#define FREEHAND_ERASE L"Freehand"
#define POLYLINE_ERASE L"Polyline"
#define SEGMENT_ERASE L"Segment"

#define LINEAR_INTERPOLATION L"Linear"
#define EASE_IN_INTERPOLATION L"Ease In"
#define EASE_OUT_INTERPOLATION L"Ease Out"
#define EASE_IN_OUT_INTERPOLATION L"Ease In/Out"

//-----------------------------------------------------------------------------

double computeThickness(double pressure, const TDoublePairProperty &property) {
  double t                    = pressure * pressure * pressure;
  double thick0               = property.getValue().first;
  double thick1               = property.getValue().second;
  if (thick1 < 0.0001) thick0 = thick1 = 0.0;
  return (thick0 + (thick1 - thick0) * t);
}

//-----------------------------------------------------------------------------

const double minDistance2 = 16.0;  // 4 pixel

//-----------------------------------------------------------------------------

void mapToVector(const std::map<int, VIStroke *> &theMap,
                 std::vector<int> &theVect) {
  assert(theMap.size() == theVect.size());
  std::map<int, VIStroke *>::const_iterator it = theMap.begin();
  UINT i = 0;
  for (; it != theMap.end(); ++it) {
    theVect[i++] = it->first;
  }
}

//-----------------------------------------------------------------------------

class UndoEraser final : public ToolUtils::TToolUndo {
  std::vector<TFilledRegionInf> m_oldFillInformation, m_newFillInformation;

  int m_row;
  int m_column;

public:
  std::map<int, VIStroke *> m_originalStrokes;
  std::map<int, VIStroke *> m_newStrokes;

  UndoEraser(TXshSimpleLevel *level, const TFrameId &frameId)
      : ToolUtils::TToolUndo(level, frameId) {
    TVectorImageP image = level->getFrame(m_frameId, true);
    if (!image) return;
    TTool::Application *app = TTool::getApplication();
    if (app) {
      m_row    = app->getCurrentFrame()->getFrame();
      m_column = app->getCurrentColumn()->getColumnIndex();
    }
    ImageUtils::getFillingInformationInArea(image, m_oldFillInformation,
                                            image->getBBox());
  }

  void onAdd() override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(!!image);
    if (!image) return;
    ImageUtils::getFillingInformationInArea(image, m_newFillInformation,
                                            image->getBBox());
  }

  ~UndoEraser() {
    std::map<int, VIStroke *>::const_iterator it;
    for (it = m_originalStrokes.begin(); it != m_originalStrokes.end(); ++it)
      deleteVIStroke(it->second);
    for (it = m_newStrokes.begin(); it != m_newStrokes.end(); ++it)
      deleteVIStroke(it->second);
  }

  void addOldStroke(int index, VIStroke *stroke) {
    VIStroke *s = cloneVIStroke(stroke);
    m_originalStrokes.insert(std::map<int, VIStroke *>::value_type(index, s));
  }

  void addNewStroke(int index, VIStroke *stroke) {
    VIStroke *s = cloneVIStroke(stroke);
    m_newStrokes.insert(std::map<int, VIStroke *>::value_type(index, s));
  }

  void undo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TFrameId currentFid;
    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentColumn()->setColumnIndex(m_column);
      app->getCurrentFrame()->setFrame(m_row);
      currentFid = TFrameId(m_row + 1);
    } else {
      app->getCurrentFrame()->setFid(m_frameId);
      currentFid = m_frameId;
    }

    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(image);
    if (!image) return;
    QMutexLocker lock(image->getMutex());
    std::vector<int> newStrokeIndex(m_newStrokes.size());
    mapToVector(m_newStrokes, newStrokeIndex);

    image->removeStrokes(newStrokeIndex, true, false);

    std::map<int, VIStroke *>::const_iterator it = m_originalStrokes.begin();
    UINT i = 0;
    VIStroke *s;
    for (; it != m_originalStrokes.end(); ++it) {
      s = cloneVIStroke(it->second);
      image->insertStrokeAt(s, it->first);
    }

    if (image->isComputedRegionAlmostOnce())
      image->findRegions();  // in futuro togliere. Serve perche' la
                             // removeStrokes, se gli si dice
    // di non calcolare le regioni, e' piu' veloce ma poi chrash tutto

    UINT size = m_oldFillInformation.size();
    if (!size) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
      return;
    }

    TRegion *reg;
    i = 0;
    for (; i < size; i++) {
      reg = image->getRegion(m_oldFillInformation[i].m_regionId);
      assert(reg);
      if (reg) reg->setStyle(m_oldFillInformation[i].m_styleId);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    TTool::Application *app = TTool::getApplication();
    if (!app) return;

    TFrameId currentFid;
    if (app->getCurrentFrame()->isEditingScene()) {
      app->getCurrentColumn()->setColumnIndex(m_column);
      app->getCurrentFrame()->setFrame(m_row);
      currentFid = TFrameId(m_row + 1);
    } else {
      app->getCurrentFrame()->setFid(m_frameId);
      currentFid = m_frameId;
    }
    TVectorImageP image = m_level->getFrame(m_frameId, true);
    assert(image);
    if (!image) return;

    QMutexLocker lock(image->getMutex());
    std::vector<int> oldStrokeIndex(m_originalStrokes.size());
    mapToVector(m_originalStrokes, oldStrokeIndex);

    image->removeStrokes(oldStrokeIndex, true, false);

    std::map<int, VIStroke *>::const_iterator it = m_newStrokes.begin();
    UINT i = 0;
    VIStroke *s;
    for (; it != m_newStrokes.end(); ++it) {
      s = cloneVIStroke(it->second);
      image->insertStrokeAt(s, it->first);
    }

    if (image->isComputedRegionAlmostOnce())
      image->findRegions();  // in futuro togliere. Serve perche' la
                             // removeStrokes, se gli si dice
    // di non calcolare le regioni, e' piu' veloce ma poi chrash tutto

    UINT size = m_newFillInformation.size();
    if (!size) {
      app->getCurrentXsheet()->notifyXsheetChanged();
      notifyImageChanged();
      return;
    }

    TRegion *reg;
    i = 0;
    for (; i < size; i++) {
      reg = image->getRegion(m_newFillInformation[i].m_regionId);
      assert(reg);
      if (reg) reg->setStyle(m_newFillInformation[i].m_styleId);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           (m_oldFillInformation.capacity() + m_newFillInformation.capacity()) *
               sizeof(TFilledRegionInf) +
           500;
  }

  QString getToolName() override { return QString("Vector Eraser Tool"); }

  int getHistoryType() override { return HistoryType::EraserTool; }
};

}  // namespace

//=============================================================================
//  EraserTool class declaration
//-----------------------------------------------------------------------------

class EraserTool final : public TTool {
  Q_DECLARE_TR_FUNCTIONS(EraserTool)

public:
  EraserTool();
  ~EraserTool();

  ToolType getToolType() const override { return TTool::LevelWriteTool; }

  void draw() override;

  void applyPressure(double pressure, bool isTablet);
  void startErase(
      TVectorImageP vi,
      const TPointD &pos);  //, const TImageLocation &imageLocation);
  void erase(TVectorImageP vi, const TPointD &pos);
  void erase(TVectorImageP vi,
             TRectD &rect);  //, const TImageLocation &imageLocation);

  void stopErase(TVectorImageP vi);

  void leftButtonDown(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonDrag(const TPointD &pos, const TMouseEvent &e) override;
  void leftButtonUp(const TPointD &pos, const TMouseEvent &) override;
  void leftButtonDoubleClick(const TPointD &pos, const TMouseEvent &e) override;
  void mouseMove(const TPointD &pos, const TMouseEvent &e) override;
  bool onPropertyChanged(std::string propertyName) override;
  void onEnter() override;
  void onLeave() override;
  void onActivate() override;

  TPropertyGroup *getProperties(int targetType) override { return &m_prop; }

  int getCursorId() const override { return ToolCursor::EraserCursor; }
  void onImageChanged() override;

  /*-- ドラッグ中にツールが切り替わった場合、Eraseの終了処理を行う --*/
  void onDeactivate() override;

private:
  typedef void (EraserTool::*EraseFunction)(const TVectorImageP vi,
                                            const TFrameId &fid,
                                            TStroke *stroke);

  TPropertyGroup m_prop;

  TEnumProperty m_eraseType;
  TDoublePairProperty m_toolSize;
  TBoolProperty m_pressure;
  TBoolProperty m_selective;
  TBoolProperty m_invertOption;
  TEnumProperty m_multi;

  double m_pointSize, m_distance2;

  TPointD m_mousePos,  //!< Current mouse position.
      m_oldMousePos,   //!< Previous mouse position.
      m_brushPos,      //!< Position the brush will be painted at.
      m_firstPos;      //!< Either The first point inserted either in m_track or
                       //! m_polyline
  //!  (depending on selected erase mode).
  TPointD m_windowMousePos;

  UndoEraser *m_undo;
  std::vector<int> m_indexes;

  TRectD m_selectingRect, m_firstRect;

  TFrameId m_firstFrameId, m_veryFirstFrameId;
  int m_firstFrameIdx;

  TXshSimpleLevelP m_level;
  std::pair<int, int> m_currCell;

  VectorBrush m_track;  //!< Lazo selection generator.

  SymmetryStroke m_polyline;  //!< Polyline points.

  TStroke *m_stroke;  //!< Stores the stroke generated by m_track.
  std::vector<TStroke *>
      m_firstStrokes;  //!< Stores the first stroke in the "frame range" case.
  TImageP m_activeImage = NULL;  // needed if an frame is changed mid-erase
  double m_thick;

  bool m_firstTime, m_active, m_firstFrameSelected;

private:
  void resetMulti();

  void updateTranslation() override;

  // Metodi per disegnare la linea della modalita' Freehand
  void startFreehand(const TPointD &pos);
  void freehandDrag(const TPointD &pos);
  void closeFreehand(const TPointD &pos);

  // Metodi per disegnare la linea della modalita' Polyline
  void addPointPolyline(const TPointD &pos);
  void closePolyline(const TPointD &pos);

  void eraseRegion(const TVectorImageP vi, const TFrameId &fid,
                   TStroke *stroke);

  void eraseSegments(const TVectorImageP vi, const TFrameId &fid,
                     TStroke *eraseStroke);

  void multiEraseRect(TFrameId firstFrameId, TFrameId lastFrameId,
                      TRectD firstRect, TRectD lastRect, bool invert);
  void multiEraseRect(int firstFrameIdx, int lastFrameIdx, TRectD firstRect,
                      TRectD lastRect, bool invert);
  void doMultiErase(TFrameId &firstFrameId, TFrameId &lastFrameId,
                    const std::vector<TStroke *> firstStrokes,
                    const std::vector<TStroke *> lastStrokes,
                    EraseFunction eraseFunction);
  void doMultiErase(int firstFrameIdx, int lastFrameIdx,
                    const std::vector<TStroke *> firstStrokes,
                    const std::vector<TStroke *> lastStrokes,
                    EraseFunction eraseFunction);
  void doErase(double t, const TXshSimpleLevelP &sl, const TFrameId &fid,
               const TVectorImageP &firstImage, const TVectorImageP &lastImage,
               EraseFunction eraseFunction);
  void multiErase(std::vector<TStroke *> stroke, const std::wstring eraseType,
                  const TMouseEvent &e, EraseFunction eraseFunction);

  void doEraseAtPos(TVectorImageP vi, const TPointD &pos);
} eraserTool;

//=============================================================================
//  EraserTool implementation
//-----------------------------------------------------------------------------

EraserTool::EraserTool()
    : TTool("T_Eraser")
    , m_eraseType("Type:")  // "W_ToolOptions_Erasetype"
    , m_toolSize("Size:", 1, 1000, 1, 10)  // "W_ToolOptions_EraserToolSize"
    , m_selective("Selective", false)   // "W_ToolOptions_Selective"
    , m_invertOption("Invert", false)   // "W_ToolOptions_Invert"
    , m_multi("Frame Range:")     // "W_ToolOptions_FrameRange"
    , m_pointSize(-1)
    , m_undo(0)
    , m_currCell(-1, -1)
    , m_stroke(0)
    , m_thick(5)
    , m_active(false)
    , m_firstTime(true)
    , m_pressure("Pressure", true) {
  bind(TTool::VectorImage);

  m_toolSize.setNonLinearSlider();

  m_prop.bind(m_toolSize);
  m_prop.bind(m_eraseType);
  m_eraseType.addValue(NORMAL_ERASE);
  m_eraseType.addValue(RECT_ERASE);
  m_eraseType.addValue(FREEHAND_ERASE);
  m_eraseType.addValue(POLYLINE_ERASE);
  m_eraseType.addValue(SEGMENT_ERASE);
  m_prop.bind(m_pressure);
  m_prop.bind(m_selective);
  m_prop.bind(m_invertOption);
  m_prop.bind(m_multi);
  m_multi.addValue(L"Off");
  m_multi.addValue(LINEAR_INTERPOLATION);
  m_multi.addValue(EASE_IN_INTERPOLATION);
  m_multi.addValue(EASE_OUT_INTERPOLATION);
  m_multi.addValue(EASE_IN_OUT_INTERPOLATION);

  m_pressure.setId("PressureSensitivity");
  m_selective.setId("Selective");
  m_invertOption.setId("Invert");
  m_multi.setId("FrameRange");
  m_eraseType.setId("Type");
}

//-----------------------------------------------------------------------------

EraserTool::~EraserTool() {
  if (m_stroke) delete m_stroke;

  m_firstStrokes.clear();
}

//-----------------------------------------------------------------------------

void EraserTool::updateTranslation() {
  m_toolSize.setQStringName(tr("Size:"));
  m_pressure.setQStringName(tr("Pressure"));
  m_selective.setQStringName(tr("Selective"));
  m_invertOption.setQStringName(tr("Invert"));
  m_eraseType.setQStringName(tr("Type:"));
  m_eraseType.setItemUIName(NORMAL_ERASE, tr("Normal"));
  m_eraseType.setItemUIName(RECT_ERASE, tr("Rectangular"));
  m_eraseType.setItemUIName(FREEHAND_ERASE, tr("Freehand"));
  m_eraseType.setItemUIName(POLYLINE_ERASE, tr("Polyline"));
  m_eraseType.setItemUIName(SEGMENT_ERASE, tr("Segment"));

  m_multi.setQStringName(tr("Frame Range:"));
  m_multi.setItemUIName(L"Off", tr("Off"));
  m_multi.setItemUIName(LINEAR_INTERPOLATION, tr("Linear"));
  m_multi.setItemUIName(EASE_IN_INTERPOLATION, tr("Ease In"));
  m_multi.setItemUIName(EASE_OUT_INTERPOLATION, tr("Ease Out"));
  m_multi.setItemUIName(EASE_IN_OUT_INTERPOLATION, tr("Ease In/Out"));
}

//-----------------------------------------------------------------------------

void EraserTool::draw() {
  int devPixRatio = m_viewer->getDevPixRatio();

  if (!m_multi.getIndex() && m_pointSize <= 0) return;

  double pixelSize2 = getPixelSize() * getPixelSize();
  m_thick           = pixelSize2 / 2.0;

  TImageP image(getImage(false));
  TVectorImageP vi = image;

  glLineWidth(1.0 * devPixRatio);

  if (vi) {
//    bool blackBg = ToonzCheck::instance()->getChecks() & ToonzCheck::eBlackBg;
//    TPixel color = blackBg ? TPixel32::White : TPixel32::Red;
    TPixel color = TPixel32::Red;
    if (m_eraseType.getValue() == RECT_ERASE) {
      if (m_multi.getIndex() && m_firstFrameSelected) {
        if (m_firstStrokes.size()) {
          tglColor(color);
          for (int i = 0; i < m_firstStrokes.size(); i++)
            drawStrokeCenterline(*m_firstStrokes[i], 1);
        } else
          drawRect(m_firstRect, color, 0x3F33, true);
      }

      if (m_active || (m_multi.getIndex() && !m_firstFrameSelected)) {
        if (m_polyline.size() > 1) {
          glPushMatrix();
          m_polyline.drawRectangle(color);
          glPopMatrix();
        } else
          drawRect(m_selectingRect, color, 0xFFFF, true);
      }
    }
    if (m_eraseType.getValue() == NORMAL_ERASE) {
      // If toggled off, don't draw brush outline
      if (!Preferences::instance()->isCursorOutlineEnabled()) return;

      tglColor(TPixel32(255, 0, 255));

      double minRange = 1;
      double maxRange = 100;

      double minSize = 2;
      double maxSize = 100;

      double min = ((m_toolSize.getValue().first - minRange) /
                        (maxRange - minRange) * (maxSize - minSize) +
                    minSize) *
                   0.5;

      double max = ((m_toolSize.getValue().second - minRange) /
                        (maxRange - minRange) * (maxSize - minSize) +
                    minSize) *
                   0.5;

      tglDrawCircle(m_brushPos, min);
      tglDrawCircle(m_brushPos, max);
    }
    if ((m_eraseType.getValue() == FREEHAND_ERASE ||
         m_eraseType.getValue() == POLYLINE_ERASE ||
         m_eraseType.getValue() == SEGMENT_ERASE) &&
        m_multi.getIndex()) {
      tglColor(color);
//      glPushAttrib(GL_ALL_ATTRIB_BITS);
//      glEnable(GL_BLEND);
//      glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
//      if (m_firstFrameSelected) {
//        glLineStipple(1, 0x3F33);
//        glEnable(GL_LINE_STIPPLE);
//      }
      for (int i = 0; i < m_firstStrokes.size(); i++)
        drawStrokeCenterline(*m_firstStrokes[i], 1);
//      glPopAttrib();
    }
    if (m_eraseType.getValue() == POLYLINE_ERASE && !m_polyline.empty()) {
      m_polyline.drawPolyline(m_mousePos, color);
    } else if ((m_eraseType.getValue() == FREEHAND_ERASE ||
                m_eraseType.getValue() == SEGMENT_ERASE) &&
               !m_track.isEmpty()) {
      tglColor(color);
      glPushMatrix();
      m_track.drawAllFragments();
      glPopMatrix();
    }
  }
}

//-----------------------------------------------------------------------------

void EraserTool::resetMulti() {
  m_firstFrameSelected = false;
  m_firstRect.empty();

  m_selectingRect.empty();
  TTool::Application *application = TTool::getApplication();
  if (!application) return;

  m_firstFrameId = m_veryFirstFrameId = getCurrentFid();
  m_firstFrameIdx                     = getFrame();
  m_level = application->getCurrentLevel()->getLevel()
                ? application->getCurrentLevel()->getLevel()->getSimpleLevel()
                : 0;

  m_firstStrokes.clear();
}

//-----------------------------------------------------------------------------

void EraserTool::applyPressure(double pressure, bool isTablet) {
  double maxThick  = m_toolSize.getValue().second;
  double thickness = (m_pressure.getValue() && isTablet)
                      ? computeThickness(pressure, m_toolSize)
                      : maxThick;
  double minRange = 1;
  double maxRange = 100;

  double minSize = 2;
  double maxSize = 100;

  m_pointSize =
      ((thickness - minRange) / (maxRange - minRange) * (maxSize - minSize) +
       minSize) *
      0.5;
}

//-----------------------------------------------------------------------------
  
void EraserTool::startErase(
    TVectorImageP vi,
    const TPointD &pos)  //, const TImageLocation &imageLocation)
{
  UINT size = vi->getStrokeCount();
  m_indexes.resize(size);
  for (UINT i = 0; i < size; i++) m_indexes[i] = i;

  if (m_undo) delete m_undo;
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  m_undo        = new UndoEraser(level, getCurrentFid());
  m_oldMousePos = pos;
  m_distance2   = minDistance2 * getPixelSize() * getPixelSize();
  doEraseAtPos(vi, pos);
}

//-----------------------------------------------------------------------------

void EraserTool::erase(TVectorImageP vi, const TPointD &pos) {
  std::vector<int>::iterator it = m_indexes.begin();
  m_distance2 += tdistance2(m_oldMousePos, pos);

  if (m_distance2 < minDistance2 * getPixelSize() * getPixelSize()) return;

  m_distance2   = 0;
  m_oldMousePos = pos;

  // quadrato circoscritto alla circonferenza
  TRectD circumscribedSquare(pos.x - m_pointSize, pos.y - m_pointSize,
                             pos.x + m_pointSize, pos.y + m_pointSize);
  if (!circumscribedSquare.overlaps(vi->getBBox())) {
    invalidate();
    return;
  }

  std::vector<double> intersections;
  std::vector<DoublePair> sortedWRanges;

  std::vector<TStroke *> splitStrokes;
  double rectEdge_2 = m_pointSize * M_SQRT1_2;

  // quadrato iscritto nella circonferenza
  TRectD enrolledSquare(pos.x - rectEdge_2, pos.y - rectEdge_2,
                        pos.x + rectEdge_2, pos.y + rectEdge_2);

  UINT i            = 0;
  double pointSize2 = sq(m_pointSize);

  std::vector<int> oneStrokeIndex(1);
  int index = TTool::getApplication()->getCurrentLevelStyleIndex();
  QMutexLocker lock(vi->getMutex());
  while (i < vi->getStrokeCount()) {
    assert(it != m_indexes.end());

    TStroke *oldStroke = vi->getStroke(i);
    if (!vi->inCurrentGroup(i) ||
        (m_selective.getValue() && oldStroke->getStyle() != index)) {
      i++;
      it++;
      continue;
    }

    TRectD strokeBBox = oldStroke->getBBox();

    if (!circumscribedSquare.overlaps(strokeBBox)) {  // stroke all'esterno del
                                                      // quadrato circoscritto
                                                      // alla circonferenxa
      i++;
      it++;
      continue;
    }

    if (enrolledSquare.contains(strokeBBox)) {  // stroke tutta contenuta nel
                                                // quadrato iscritto nella
                                                // circonferenza
      if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));
      oneStrokeIndex[0] = i;
      vi->removeStrokes(oneStrokeIndex, true, true);

      it = m_indexes.erase(it);
      continue;
    }

    splitStrokes.clear();
    intersections.clear();
    /*int intersNum=*/intersect(*oldStroke, pos, m_pointSize, intersections);

    /*
#ifdef _DEBUG
if(intersections.size()==2 && intersections[0]==intersections[1])
{
intersections.clear();
intersect( *oldStroke, pos, m_pointSize, intersections );
}
#endif
*/

    if (intersections.empty()) {
      // BASTEREBBE CONTROLLARE UN SOLO PUNTO PERCHE' SE LA
      // STROKE NON INTERSECA IL CERCHIO E CONTENTIENE ALMENO
      // UN PUNTO, LA CONTIENE TUTTA. MA SICCOME NON MI FIDO
      // DELLA INTERSECT, NE CONTROLLO UN PAIO E AVVANTAGGIO (CON L' AND)
      // IL NON CONTENIMENTO, DATO CHE E' MEGLIO CANCELLARE UNA COSA IN
      // MENO, CHE UNA IN PIU'
      if (tdistance2(oldStroke->getPoint(0), pos) < pointSize2 &&
          tdistance2(oldStroke->getPoint(1), pos) <
              pointSize2) {  // stroke tutta contenuta nella circonferenxa
        if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));
        oneStrokeIndex[0] = i;
        vi->removeStrokes(oneStrokeIndex, true, true);
        it = m_indexes.erase(it);
      } else {  // non colpita
        i++;
        it++;
      }
      continue;
    }

    //------------------------ almeno un'intersezione
    //---------------------------------------------------------

    if (intersections.size() == 1) {
      if (oldStroke->isSelfLoop()) {  // una sola intersezione di una stroke
                                      // chiusa con un cerchio dovrebbe accadere
        // solo in caso di sfioramento, quindi faccio finta di nulla
        i++;
        it++;
        continue;
      }

      if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));

      double w0            = intersections[0];
      TThickPoint hitPoint = oldStroke->getThickPoint(w0);
      int chunck;
      double t;
      oldStroke->getChunkAndT(w0, chunck, t);

      int chunckIndex;
      double w1;
      if (tdistance2(oldStroke->getPoint(0), pos) < pointSize2) {
        chunckIndex = oldStroke->getChunkCount() - 1;
        w1          = 1;
      } else {
        chunckIndex = 0;
        w1          = 0;
      }

      UINT cI;
      std::vector<TThickPoint> points;
      if (w1 == 0) {
        for (cI = chunckIndex; cI < (UINT)chunck; cI++) {
          points.push_back(oldStroke->getChunk(cI)->getThickP0());
          points.push_back(oldStroke->getChunk(cI)->getThickP1());
        }

        TThickQuadratic t1, t2;
        oldStroke->getChunk(chunck)->split(t, t1, t2);
        points.push_back(t1.getThickP0());
        points.push_back(t1.getThickP1());
        points.push_back(hitPoint);
      } else {
        TThickQuadratic t1, t2;
        oldStroke->getChunk(chunck)->split(t, t1, t2);
        points.push_back(hitPoint);
        points.push_back(t2.getThickP1());
        points.push_back(t2.getThickP2());

        for (cI = chunck + 1; cI <= (UINT)chunckIndex; cI++) {
          points.push_back(oldStroke->getChunk(cI)->getThickP1());
          points.push_back(oldStroke->getChunk(cI)->getThickP2());
        }
      }

      oldStroke->reshape(&(points[0]), points.size());

      vi->notifyChangedStrokes(i);  // per adesso cosi', pero' e' lento

      *it = -1;
      i++;
      it++;
      continue;
    }

    //---------- piu'
    // intersezioni--------------------------------------------------------

    if (intersections.size() & 1 &&
        oldStroke->isSelfLoop()) {  // non dovrebbe mai accadere
      assert(0);
      i++;
      it++;
      continue;
    }

    if (intersections.size() == 2 &&
        intersections[0] == intersections[1]) {  // solo sfiorata
      i++;
      it++;
      continue;
    }

    UINT oldStrokeSize = vi->getStrokeCount();

    if (*it != -1) m_undo->addOldStroke(*it, vi->getVIStroke(i));

    sortedWRanges.clear();

    if (tdistance2(vi->getStroke(i)->getPoint(0), pos) > pointSize2) {
      if (intersections[0] == 0.0)
        intersections.erase(intersections.begin());
      else
        intersections.insert(intersections.begin(), 0.0);
    }

    if (intersections[0] != 1.0) intersections.push_back(1.0);

    sortedWRanges.reserve(intersections.size() / 2);

    for (UINT j = 0; j < intersections.size() - 1; j += 2)
      sortedWRanges.push_back(
          std::make_pair(intersections[j], intersections[j + 1]));

#ifdef _DEBUG

    for (UINT kkk = 0; kkk < sortedWRanges.size() - 1; kkk++) {
      assert(sortedWRanges[kkk].first < sortedWRanges[kkk].second);
      assert(sortedWRanges[kkk].second <= sortedWRanges[kkk + 1].first);
    }
    assert(sortedWRanges.back().first < sortedWRanges.back().second);

#endif

    vi->splitStroke(i, sortedWRanges);

    UINT addedStroke = vi->getStrokeCount() -
                       (oldStrokeSize - 1);  //-1 perche' e' quella tolta

    i += addedStroke;

    *it = -1;
    m_indexes.insert(it, addedStroke - 1, -1);
    it = m_indexes.begin() + i;
  }

  invalidate();
}

//-----------------------------------------------------------------------------

void EraserTool::erase(TVectorImageP vi, TRectD &rect) {
  if (rect.x0 > rect.x1) std::swap(rect.x1, rect.x0);
  if (rect.y0 > rect.y1) std::swap(rect.y1, rect.y0);
  int i     = 0;
  int index = TTool::getApplication()->getCurrentLevelStyleIndex();
  std::vector<int> eraseStrokes;

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  m_undo = new UndoEraser(level, getCurrentFid());
  for (i = 0; i < (int)vi->getStrokeCount(); i++) {
    if (!vi->inCurrentGroup(i)) continue;
    TStroke *stroke = vi->getStroke(i);
    if (!m_invertOption.getValue()) {
      if ((!m_selective.getValue() || stroke->getStyle() == index) &&
          rect.contains(stroke->getBBox())) {
        m_undo->addOldStroke(i, vi->getVIStroke(i));
        eraseStrokes.push_back(i);
      }
    } else {
      if ((!m_selective.getValue() || stroke->getStyle() == index) &&
          !rect.contains(stroke->getBBox())) {
        m_undo->addOldStroke(i, vi->getVIStroke(i));
        eraseStrokes.push_back(i);
      }
    }
  }
  for (i = (int)eraseStrokes.size() - 1; i >= 0; i--)
    vi->deleteStroke(eraseStrokes[i]);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
  invalidate();
}

//-----------------------------------------------------------------------------

void EraserTool::stopErase(TVectorImageP vi) {
  assert(m_undo != 0);

  if (m_undo) {
    UINT size = m_indexes.size();

    assert(size == vi->getStrokeCount());
    UINT i = 0;
    for (; i < size; i++) {
      if (m_indexes[i] == -1) m_undo->addNewStroke(i, vi->getVIStroke(i));
    }
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
  }

  m_active = false;
  m_indexes.clear();
  invalidate();
  notifyImageChanged();
}

//-----------------------------------------------------------------------------

void EraserTool::leftButtonDown(const TPointD &pos, const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;

  m_active = true;

  TImageP image(getImage(true));
  m_activeImage = image;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (m_eraseType.getValue() == NORMAL_ERASE) {
    applyPressure(e.m_pressure, e.isTablet());
    if (TVectorImageP vi = image) startErase(vi, pos /*,imageLocation*/);
  } else if (m_eraseType.getValue() == RECT_ERASE) {
    m_selectingRect.x0 = pos.x;
    m_selectingRect.y0 = pos.y;
    m_selectingRect.x1 = pos.x + 1;
    m_selectingRect.y1 = pos.y + 1;

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

    invalidate();
  } else if (m_eraseType.getValue() == FREEHAND_ERASE ||
             m_eraseType.getValue() == SEGMENT_ERASE) {
    startFreehand(pos);
  } else if (m_eraseType.getValue() == POLYLINE_ERASE) {
    if (!m_invertOption.getValue() && symmetryTool &&
        symmetryTool->isGuideEnabled() && !m_polyline.hasSymmetryBrushes()) {
      m_polyline.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                                    symmObj.getCenterPoint(),
                                    symmObj.isUsingLineSymmetry(), dpiScale);
    }

    addPointPolyline(pos);
  }
}

//-----------------------------------------------------------------------------

void EraserTool::leftButtonDrag(const TPointD &pos, const TMouseEvent &e) {
  m_brushPos = m_mousePos = pos;

  if (!m_active) return;

  TImageP image(getImage(true));
  if (m_eraseType.getValue() == RECT_ERASE) {
    m_selectingRect.x1 = pos.x;
    m_selectingRect.y1 = pos.y;

    if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
      m_polyline.clear();
      m_polyline.setRectangle(TPointD(m_selectingRect.x0, m_selectingRect.y0),
                              TPointD(m_selectingRect.x1, m_selectingRect.y1));
    }

    invalidate();
    return;
  } else if (m_eraseType.getValue() == NORMAL_ERASE) {
    if (!m_undo) leftButtonDown(pos, e);
    applyPressure(e.m_pressure, e.isTablet());
    if (TVectorImageP vi = image) doEraseAtPos(vi, pos);
  } else if (m_eraseType.getValue() == FREEHAND_ERASE ||
             m_eraseType.getValue() == SEGMENT_ERASE) {
    freehandDrag(pos);
  }
}

//-----------------------------------------------------------------------------

void EraserTool::multiEraseRect(TFrameId firstFrameId, TFrameId lastFrameId,
                                TRectD firstRect, TRectD lastRect,
                                bool invert) {
  int r0 = firstFrameId.getNumber();
  int r1 = lastFrameId.getNumber();

  if (r0 > r1) {
    std::swap(r0, r1);
    std::swap(firstFrameId, lastFrameId);
    std::swap(firstRect, lastRect);
  }
  if ((r1 - r0) < 1) return;

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
    assert(firstFrameId <= fid && fid <= lastFrameId);
    TVectorImageP img = (TVectorImageP)m_level->getFrame(fid, true);
    assert(img);
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t           = TInbetween::interpolation(t, algorithm);
    TRectD rect = interpolateRect(firstRect, lastRect, t);
    // m_level->setFrame(fid, img); //necessario: se la getFrame ha scompattato
    // una img compressa, senza setFrame le modifiche sulla img fatte qui
    // andrebbero perse.

    // Setto fid come corrente per notificare il cambiamento dell'immagine
    TTool::Application *app = TTool::getApplication();
    if (app) app->getCurrentFrame()->setFid(fid);

    erase(img, rect);

    notifyImageChanged();
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void EraserTool::multiEraseRect(int firstFrameIdx, int lastFrameIdx,
                                TRectD firstRect, TRectD lastRect,
                                bool invert) {
  if (firstFrameIdx > lastFrameIdx) {
    std::swap(firstFrameIdx, lastFrameIdx);
    std::swap(firstRect, lastRect);
  }
  if ((lastFrameIdx - firstFrameIdx) < 1) return;

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
    TVectorImageP img = (TVectorImageP)cell.getImage(true);
    if (!img) continue;
    double t    = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t           = TInbetween::interpolation(t, algorithm);
    TRectD rect = interpolateRect(firstRect, lastRect, t);

    if (app) app->getCurrentFrame()->setFrame(row);

    erase(img, rect);

    notifyImageChanged();
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void EraserTool::onImageChanged() {
  if (m_active) {
    stopErase(m_activeImage);
  }
  if (!m_multi.getIndex()) return;
  TTool::Application *application = TTool::getApplication();
  if (!application) return;
  TXshSimpleLevel *xshl = 0;
  if (application->getCurrentLevel()->getLevel())
    xshl = application->getCurrentLevel()->getLevel()->getSimpleLevel();

  bool isEditingLevel = application->getCurrentFrame()->isEditingLevel();

  if (!xshl || m_level.getPointer() != xshl ||
      (m_eraseType.getValue() == RECT_ERASE && m_selectingRect.isEmpty()) ||
      ((m_eraseType.getValue() == FREEHAND_ERASE ||
        m_eraseType.getValue() == POLYLINE_ERASE ||
        m_eraseType.getValue() == SEGMENT_ERASE) &&
       !m_firstStrokes.size()))
    resetMulti();
  else if ((isEditingLevel && m_firstFrameId == getCurrentFid()) ||
           (!isEditingLevel && m_firstFrameIdx == getFrame()))
    m_firstFrameSelected = false;  // nel caso sono passato allo stato 1 e torno
                                   // all'immagine iniziale, torno allo stato
                                   // iniziale
  else {                           // cambio stato.
    m_firstFrameSelected = true;
    if (m_eraseType.getValue() == RECT_ERASE) {
      assert(!m_selectingRect.isEmpty());
      m_firstRect = m_selectingRect;
    }
  }
}

//-----------------------------------------------------------------------------

void EraserTool::leftButtonUp(const TPointD &pos, const TMouseEvent &e) {
  if (!m_active) return;
  m_active = false;
  TImageP image(getImage(true));
  TVectorImageP vi = image;

  TTool::Application *application = TTool::getApplication();
  if (!vi || !application) return;
  if (m_eraseType.getValue() == NORMAL_ERASE) {
    if (!m_undo) leftButtonDown(pos, e);
    applyPressure(e.m_pressure, e.isTablet());
    stopErase(vi);
  } else if (m_eraseType.getValue() == RECT_ERASE) {
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
      if (m_firstFrameSelected) {
        bool isEditingLevel = application->getCurrentFrame()->isEditingLevel();

        if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
          // We'll use polyline
          TFrameId tmp = getCurrentFid();
          std::vector<TStroke *> lastStrokes;
          for (int i = 0; i < m_polyline.getBrushCount(); i++)
            lastStrokes.push_back(m_polyline.makeRectangleStroke(i));
          multiErase(lastStrokes, POLYLINE_ERASE, e, &EraserTool::eraseRegion);
        } else {
          if (isEditingLevel)
            multiEraseRect(m_firstFrameId, getCurrentFid(), m_firstRect,
                           m_selectingRect, m_invertOption.getValue());
          else
            multiEraseRect(m_firstFrameIdx, getFrame(), m_firstRect,
                           m_selectingRect, m_invertOption.getValue());
        }

        if (e.isShiftPressed()) {
          m_firstRect     = m_selectingRect;
          m_firstFrameId  = getCurrentFid();
          m_firstFrameIdx = getFrame();
          m_firstStrokes.clear();
          if (m_polyline.size() > 1) {
            for (int i = 0; i < m_polyline.getBrushCount(); i++)
              m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
          }
        } else {
          if (application->getCurrentFrame()->isEditingScene()) {
            application->getCurrentColumn()->setColumnIndex(m_currCell.first);
            application->getCurrentFrame()->setFrame(m_currCell.second);
          } else
            application->getCurrentFrame()->setFid(m_veryFirstFrameId);
          resetMulti();
          m_polyline.reset();
        }
        invalidate();  // invalidate(m_selectingRect.enlarge(2));
      } else {
        m_firstStrokes.clear();
        if (m_polyline.size() > 1) {
          for (int i = 0; i < m_polyline.getBrushCount(); i++)
            m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
        }
        if (application->getCurrentFrame()->isEditingScene())
          m_currCell = std::pair<int, int>(
              application->getCurrentColumn()->getColumnIndex(),
              application->getCurrentFrame()->getFrame());
        invalidate();
      }
      return;
    } else if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
      TUndoManager::manager()->beginBlock();

      TStroke *stroke = m_polyline.makeRectangleStroke();
      eraseRegion(vi, getCurrentFid(), stroke);

      for (int i = 1; i < m_polyline.getBrushCount(); i++) {
        TStroke *symmStroke = m_polyline.makeRectangleStroke(i);
        eraseRegion(vi, getCurrentFid(), symmStroke);
      }

      TUndoManager::manager()->endBlock();
      invalidate();
    } else {
      erase(vi, m_selectingRect);
      notifyImageChanged();
    }
    m_selectingRect.empty();
    m_polyline.reset();
  } else if (m_eraseType.getValue() == FREEHAND_ERASE) {
    closeFreehand(pos);
    if (m_multi.getIndex()) {
      double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
      std::vector<TStroke *> lastStrokes;
      for (int i = 0; i < m_track.getBrushCount(); i++)
        lastStrokes.push_back(m_track.makeStroke(error, i));
      multiErase(lastStrokes, m_eraseType.getValue(), e,
                 &EraserTool::eraseRegion);
      invalidate();
    } else {
      if (m_track.hasSymmetryBrushes()) TUndoManager::manager()->beginBlock();

      eraseRegion(vi, getCurrentFid(), m_stroke);

      if (m_track.hasSymmetryBrushes()) {
        double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
        std::vector<TStroke *> symmStrokes = m_track.makeSymmetryStrokes(error);
        for (int i = 0; i < symmStrokes.size(); i++) {
          eraseRegion(vi, getCurrentFid(), symmStrokes[i]);
        }

        TUndoManager::manager()->endBlock();
      }

      invalidate();
      notifyImageChanged();
    }
    m_track.clear();
  } else if (m_eraseType.getValue() == SEGMENT_ERASE) {
    double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
    m_stroke     = m_track.makeStroke(error);
    m_stroke->setStyle(1);
    if (m_multi.getIndex()) {
      std::vector<TStroke *> lastStrokes;
      for (int i = 0; i < m_track.getBrushCount(); i++)
        lastStrokes.push_back(m_track.makeStroke(error, i));
      multiErase(lastStrokes, m_eraseType.getValue(), e,
                 &EraserTool::eraseSegments);
      invalidate();
    } else {
      if (m_track.hasSymmetryBrushes()) TUndoManager::manager()->beginBlock();

      eraseSegments(vi, getCurrentFid(), m_stroke);

      if (m_track.hasSymmetryBrushes()) {
        double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
        std::vector<TStroke *> symmStrokes = m_track.makeSymmetryStrokes(error);
        for (int i = 0; i < symmStrokes.size(); i++) {
          eraseSegments(vi, getCurrentFid(), symmStrokes[i]);
        }

        TUndoManager::manager()->endBlock();
      }
      invalidate();
      notifyImageChanged();
    }
    m_track.clear();
  }
}

//-----------------------------------------------------------------------------

//! Viene chiusa la polyline e si da il via alla cancellazione.
/*!Viene creato lo \b stroke rappresentante la polyline disegnata.
  Se e' selzionato il metodo di cancellazione "frame range" viene richiamato il
  metodo \b multiEreserRegion, altrimenti
  viene richiamato il metodo \b eraseRegion*/
void EraserTool::leftButtonDoubleClick(const TPointD &pos,
                                       const TMouseEvent &e) {
  TVectorImageP vi = TImageP(getImage(true));
  if (m_eraseType.getValue() == POLYLINE_ERASE) {
    closePolyline(pos);

    TStroke *stroke = m_polyline.makePolylineStroke();
    assert(stroke->getPoint(0) == stroke->getPoint(1));
    if (m_multi.getIndex()) {
      std::vector<TStroke *> lastStrokes;
      for (int i = 0; i < m_polyline.getBrushCount(); i++)
        lastStrokes.push_back(m_polyline.makePolylineStroke(i));
      multiErase(lastStrokes, m_eraseType.getValue(), e,
                 &EraserTool::eraseRegion);
    } else {
      if (m_polyline.hasSymmetryBrushes())
        TUndoManager::manager()->beginBlock();

      eraseRegion(vi, getCurrentFid(), stroke);

      if (m_polyline.hasSymmetryBrushes()) {
        for (int i = 1; i < m_polyline.getBrushCount(); i++) {
          TStroke *symmStroke = m_polyline.makePolylineStroke(i);
          eraseRegion(vi, getCurrentFid(), symmStroke);
        }

        TUndoManager::manager()->endBlock();
      }

      m_active = false;
      notifyImageChanged();
    }
    m_polyline.reset();
    invalidate();
  }
}

//-----------------------------------------------------------------------------

void EraserTool::mouseMove(const TPointD &pos, const TMouseEvent &e) {
  struct Locals {
    EraserTool *m_this;

    void setValue(TDoublePairProperty &prop,
                  const TDoublePairProperty::Value &value) {
      prop.setValue(value);

      m_this->onPropertyChanged(prop.getName());
      TTool::getApplication()->getCurrentTool()->notifyToolChanged();
    }

    void addMinMax(TDoublePairProperty &prop, double add) {
      const TDoublePairProperty::Range &range = prop.getRange();

      TDoublePairProperty::Value value = prop.getValue();
      value.second =
          tcrop<double>(value.second + add, range.first, range.second);
      value.first = tcrop<double>(value.first + add, range.first, range.second);

      setValue(prop, value);
    }

    void addMinMaxSeparate(TDoublePairProperty &prop, double min, double max) {
      if (min == 0.0 && max == 0.0) return;
      const TDoublePairProperty::Range &range = prop.getRange();

      TDoublePairProperty::Value value = prop.getValue();
      value.first += min;
      value.second += max;
      if (value.first > value.second) value.first = value.second;
      value.first  = tcrop<double>(value.first, range.first, range.second);
      value.second = tcrop<double>(value.second, range.first, range.second);

      setValue(prop, value);
    }

  } locals = {this};

  if (m_eraseType.getValue() == NORMAL_ERASE && e.isCtrlPressed() && e.isAltPressed() && !e.isShiftPressed()) {
    // User wants to alter the maximum brush size
    const TPointD &diff = m_windowMousePos - -e.m_pos;
    double max          = diff.x / 2;
    double min          = diff.y / 2;

    locals.addMinMaxSeparate(m_toolSize, min, max);
  } else {
    m_brushPos = pos;
  }

  m_oldMousePos = m_mousePos = pos;
  m_windowMousePos = -e.m_pos;
  invalidate();
}

//----------------------------------------------------------------------

bool EraserTool::onPropertyChanged(std::string propertyName) {
  EraseVectorType          = ::to_string(m_eraseType.getValue());
  EraseVectorMinSize       = m_toolSize.getValue().first;
  EraseVectorSize          = m_toolSize.getValue().second;
  EraseVectorPressure      = m_pressure.getValue();
  EraseVectorSelective     = m_selective.getValue();
  EraseVectorInvert        = m_invertOption.getValue();
  EraseVectorRange         = m_multi.getIndex();

  double x = m_toolSize.getValue().second;

  double minRange = 1;
  double maxRange = 100;

  double minSize = 2;
  double maxSize = 100;

  m_pointSize =
      ((x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize) *
      0.5;
  invalidate();

  return true;
}

//-----------------------------------------------------------------------------

void EraserTool::onEnter() {
  if (m_firstTime) {
    m_toolSize.setValue(
        TDoublePairProperty::Value(EraseVectorMinSize, EraseVectorSize));
    m_eraseType.setValue(::to_wstring(EraseVectorType.getValue()));
    m_pressure.setValue(EraseVectorPressure ? 1 : 0);
    m_selective.setValue(EraseVectorSelective ? 1 : 0);
    m_invertOption.setValue(EraseVectorInvert ? 1 : 0);
    m_multi.setIndex(EraseVectorRange);
    m_firstTime = false;
  }

  double x = m_toolSize.getValue().second;

  double minRange = 1;
  double maxRange = 100;

  double minSize = 2;
  double maxSize = 100;

  m_pointSize =
      ((x - minRange) / (maxRange - minRange) * (maxSize - minSize) + minSize) *
      0.5;

  //  getApplication()->editImage();
}

//-----------------------------------------------------------------------------

void EraserTool::onLeave() {
  draw();
  m_pointSize = -1;
}

//-----------------------------------------------------------------------------

void EraserTool::onActivate() {
  resetMulti();
  m_polyline.reset();
  onEnter();
}

//-----------------------------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato il primo pezzetto del lazzo.
//! Viene inizializzato \b m_firstPos
void EraserTool::startFreehand(const TPointD &pos) {
  m_track.reset();

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  TPointD dpiScale       = getViewer()->getDpiScale();
  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  if (!m_invertOption.getValue() && symmetryTool &&
      symmetryTool->isGuideEnabled()) {
    m_track.addSymmetryBrushes(symmObj.getLines(), symmObj.getRotation(),
                               symmObj.getCenterPoint(),
                               symmObj.isUsingLineSymmetry(), dpiScale);
  }

  m_firstPos = pos;
  m_track.add(TThickPoint(pos, m_thick), getPixelSize() * getPixelSize());
}

//-----------------------------------------------------------------------------

//! Viene aggiunto \b pos a \b m_track e disegnato un altro pezzetto del lazzo.
void EraserTool::freehandDrag(const TPointD &pos) {
#if defined(MACOSX)
//		m_viewer->enableRedraw(false);
#endif
  m_track.add(TThickPoint(pos, m_thick), getPixelSize() * getPixelSize());
  invalidate(m_track.getModifiedRegion());
}

//-----------------------------------------------------------------------------

//! Viene chiuso il lazzo (si aggiunge l'ultimo punto ad m_track) e viene creato
//! lo stroke rappresentante il lazzo.
void EraserTool::closeFreehand(const TPointD &pos) {
#if defined(MACOSX)
//		m_viewer->enableRedraw(true);
#endif
  if (m_track.isEmpty()) return;
  m_track.add(TThickPoint(m_firstPos, m_thick),
              getPixelSize() * getPixelSize());
  m_track.filterPoints();
  double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
  m_stroke     = m_track.makeStroke(error);
  m_stroke->setStyle(1);
}

//-----------------------------------------------------------------------------

//! Viene aggiunto un punto al vettore m_polyline.
void EraserTool::addPointPolyline(const TPointD &pos) {
  m_firstPos = pos;
  m_polyline.push_back(pos);
}

//-----------------------------------------------------------------------------

//! Agginge l'ultimo pos a \b m_polyline e chiude la spezzata (aggiunge \b
//! m_polyline.front() alla fine del vettore)
void EraserTool::closePolyline(const TPointD &pos) {
  if (m_polyline.size() <= 1) return;
  if (m_polyline.back() != pos) m_polyline.push_back(pos);
  if (m_polyline.back() != m_polyline.front())
    m_polyline.push_back(m_polyline.front());
  invalidate();
}

//-----------------------------------------------------------------------------

static bool doublePairCompare(DoublePair p1, DoublePair p2) {
  return p1.first < p2.first;
}

void EraserTool::eraseSegments(const TVectorImageP vi, const TFrameId &fid,
                               TStroke *eraseStroke) {
  if (!vi || !eraseStroke) return;

  int strokeNumber = vi->getStrokeCount();
  int colorStyle   = TTool::getApplication()->getCurrentLevelStyleIndex();
  std::vector<int> touchedStrokeIndex;
  std::vector<std::vector<double>> touchedStrokeW;
  std::vector<std::vector<DoublePair>> touchedStrokeRanges;

  // find all touched strokes and where it is touched
  for (int i = 0; i < strokeNumber; ++i) {
    std::vector<DoublePair> intersections;
    std::vector<double> ws;
    TStroke *stroke = vi->getStroke(i);
    bool touched    = false;

    if (m_selective.getValue() && stroke->getStyle() != colorStyle) {
      continue;
    }

    intersect(eraseStroke, stroke, intersections, false);

    for (auto &intersection : intersections) {
      touched = true;
      ws.push_back(intersection.second);
    }

    if (touched) {
      touchedStrokeIndex.push_back(i);
      touchedStrokeW.push_back(ws);
    }
  }

  // if the eraser did not touch any strokes, return
  if (touchedStrokeIndex.size() == 0) {
    return;
  }

  // find closest intersections of each end of the touched place of each stroke
  for (int i = 0; i < touchedStrokeIndex.size(); ++i) {
    std::vector<DoublePair> range;
    for (auto w : touchedStrokeW[i]) {
      std::vector<DoublePair> intersections;
      double lowerW = 0.0, higherW = 1.0;
      double higherW0 = 1.0,
             lowerW1  = 0.0;  // these two value are used when the stroke is
                              // self-loop-ed, it assumes touched W is 0 or 1 to
                              // find closet intersection

      int strokeIndex = touchedStrokeIndex[i];
      TStroke *stroke = vi->getStroke(strokeIndex);

      // check self intersection first
      intersect(stroke, stroke, intersections, false);
      for (auto &intersection : intersections) {
        if (areAlmostEqual(intersection.first, 0, 1e-6)) {
          continue;
        }
        if (areAlmostEqual(intersection.second, 1, 1e-6)) {
          continue;
        }

        if (intersection.first < w) {
          lowerW = std::max(lowerW, intersection.first);
        } else {
          higherW = std::min(higherW, intersection.first);
        }

        if (intersection.second < w) {
          lowerW = std::max(lowerW, intersection.second);
        } else {
          higherW = std::min(higherW, intersection.second);
        }

        lowerW1  = std::max(lowerW1, intersection.first);
        higherW0 = std::min(higherW0, intersection.first);
        lowerW1  = std::max(lowerW1, intersection.second);
        higherW0 = std::min(higherW0, intersection.second);
      }

      // then check intersection with other strokes
      for (int j = 0; j < strokeNumber; ++j) {
        if (j == strokeIndex) {
          continue;
        }

        TStroke *intersectedStroke = vi->getStroke(j);
        intersect(stroke, intersectedStroke, intersections, false);
        for (auto &intersection : intersections) {
          if (intersection.first < w) {
            lowerW = std::max(lowerW, intersection.first);
          } else {
            higherW = std::min(higherW, intersection.first);
          }
          lowerW1  = std::max(lowerW1, intersection.first);
          higherW0 = std::min(higherW0, intersection.first);
        }
      }

      range.push_back(std::make_pair(lowerW, higherW));
      if (stroke->isSelfLoop()) {
        if (lowerW == 0.0) {
          range.push_back(std::make_pair(lowerW1, 1.0));
        } else if (higherW == 1.0) {
          range.push_back(std::make_pair(0.0, higherW0));
        }
      }
    }
    touchedStrokeRanges.push_back(range);
  }

  // merge all ranges of the same stroke by using interval merging algorithm
  for (auto &ranges : touchedStrokeRanges) {
    std::vector<DoublePair> merged;

    std::sort(ranges.begin(), ranges.end(), doublePairCompare);

    merged.push_back(ranges[0]);
    for (auto &range : ranges) {
      if (merged.back().second < range.first &&
          !areAlmostEqual(merged.back().second, range.first, 1e-3)) {
        merged.push_back(range);
      } else if (merged.back().second < range.second) {
        merged.back().second = range.second;
      }
    }

    ranges = merged;
  }

  // create complement range
  for (auto &ranges : touchedStrokeRanges) {
    std::vector<DoublePair> complement;

    double last = 0.0;
    for (auto &range : ranges) {
      if (!areAlmostEqual(last, range.first, 1e-3)) {
        complement.push_back(std::make_pair(last, range.first));
      }
      last = range.second;
    }

    if (!areAlmostEqual(last, 1.0, 1e-3)) {
      complement.push_back(std::make_pair(last, 1.0));
    }
    ranges = complement;
  }

  // calculate how many lines are added for calculating the final index of added
  // strokes
  int added = 0;
  for (int i = touchedStrokeIndex.size() - 1; i >= 0; --i) {
    bool willbeJoined = vi->getStroke(touchedStrokeIndex[i])->isSelfLoop() &&
                        touchedStrokeRanges[i][0].first == 0.0 &&
                        touchedStrokeRanges[i].back().second == 1.0;

    added += touchedStrokeRanges[i].size();
    if (willbeJoined) {
      --added;
    }
  }

  // do the erasing and construct the undo action
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  UndoEraser *undo = new UndoEraser(level, fid);
  for (int i = touchedStrokeIndex.size() - 1; i >= 0; --i) {
    undo->addOldStroke(touchedStrokeIndex[i],
                       vi->getVIStroke(touchedStrokeIndex[i]));

    if (touchedStrokeRanges[i].size() == 0) {
      vi->deleteStroke(touchedStrokeIndex[i]);
    } else {
      bool willbeJoined = vi->getStroke(touchedStrokeIndex[i])->isSelfLoop() &&
                          touchedStrokeRanges[i][0].first == 0.0 &&
                          touchedStrokeRanges[i].back().second == 1.0;

      vi->splitStroke(touchedStrokeIndex[i], touchedStrokeRanges[i]);

      int size = touchedStrokeRanges[i].size();
      if (willbeJoined) {
        --size;
      }
      added -= size;
      for (int j = 0; j < size; ++j) {
        int finalIndex   = touchedStrokeIndex[i] + j - i + added;
        int currentIndex = touchedStrokeIndex[i] + j;
        undo->addNewStroke(finalIndex, vi->getVIStroke(currentIndex));
      }
    }
  }

  TUndoManager::manager()->add(undo);
  return;
}

//-----------------------------------------------------------------------------

//! Cancella stroke presenti in \b vi e contenuti nella regione delimitata da \b
//! stroke.
/*!Vengono cercati gli stroke da cancellare facendo i dovuti controlli sui
   parametri \b m_invertOption e \b m_selective.
        Se uno stroke deve essere cancellato viene inserito in \b eraseStrokes.
        Gli stroke vengono cancellati tutti alla fine.*/
void EraserTool::eraseRegion(
    const TVectorImageP vi, const TFrameId &fid,
    TStroke *stroke)  //, const TImageLocation &imageLocation)
{
  if (!vi || !stroke) return;
  TVectorImage eraseImg;
  TStroke *eraseStroke = new TStroke(*stroke);
  eraseImg.addStroke(eraseStroke);
  eraseImg.findRegions();
  int strokeIndex, regionIndex, colorStyle;
  colorStyle = TTool::getApplication()->getCurrentLevelStyleIndex();
  std::vector<int> eraseStrokes;

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  m_undo = new UndoEraser(level, fid);

  if (!m_invertOption.getValue()) {
    for (strokeIndex = 0; strokeIndex < (int)vi->getStrokeCount();
         strokeIndex++) {
      if (!vi->inCurrentGroup(strokeIndex)) continue;
      TStroke *currentStroke = vi->getStroke(strokeIndex);
      for (regionIndex = 0; regionIndex < (int)eraseImg.getRegionCount();
           regionIndex++) {
        TRegion *region = eraseImg.getRegion(regionIndex);
        if ((!m_selective.getValue() ||
             (m_selective.getValue() &&
              currentStroke->getStyle() == colorStyle)) &&
            region->contains(*currentStroke, true)) {
          eraseStrokes.push_back(strokeIndex);
          m_undo->addOldStroke(strokeIndex, vi->getVIStroke(strokeIndex));
        }
      }
    }
  } else {
    for (strokeIndex = 0; strokeIndex < (int)vi->getStrokeCount();
         strokeIndex++) {
      TStroke *currentStroke = vi->getStroke(strokeIndex);
      bool eraseIt           = false;
      for (regionIndex = 0; regionIndex < (int)eraseImg.getRegionCount();
           regionIndex++) {
        TRegion *region = eraseImg.getRegion(regionIndex);
        if (!m_selective.getValue() ||
            (m_selective.getValue() && currentStroke->getStyle() == colorStyle))
          eraseIt = true;
        if (region->contains(*currentStroke, true)) {
          eraseIt = false;
          break;
        }
      }
      if (eraseIt) {
        m_undo->addOldStroke(strokeIndex, vi->getVIStroke(strokeIndex));
        eraseStrokes.push_back(strokeIndex);
      }
    }
  }
  int i;
  for (i = (int)eraseStrokes.size() - 1; i >= 0; i--)
    vi->deleteStroke(eraseStrokes[i]);
  TUndoManager::manager()->add(m_undo);
  m_undo = 0;
}

//-----------------------------------------------------------------------------

//! Viene richiamata la doErase sui frame compresi tra \b firstFrameId e \b
//! lastFrameId.
/*!Viene caricato il vettore \b fids con i TFrameId delle immagini sulle quali
 * si deve effettuare una cancellazione.*/
void EraserTool::doMultiErase(TFrameId &firstFrameId, TFrameId &lastFrameId,
                              const std::vector<TStroke *> firstStrokes,
                              const std::vector<TStroke *> lastStrokes,
                              EraserTool::EraseFunction eraseFunction) {
  TXshSimpleLevel *sl =
      TTool::getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel();
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
  if (firstFrameId > lastFrameId) {
    std::swap(firstFrameId, lastFrameId);
    backward = true;
  }
  assert(firstFrameId <= lastFrameId);

  std::vector<TFrameId> allFids;
  sl->getFids(allFids);
  std::vector<TFrameId>::iterator i0 = allFids.begin();
  while (i0 != allFids.end() && *i0 < firstFrameId) i0++;
  if (i0 == allFids.end()) return;
  std::vector<TFrameId>::iterator i1 = i0;
  while (i1 != allFids.end() && *i1 <= lastFrameId) i1++;
  assert(i0 < i1);
  std::vector<TFrameId> fids(i0, i1);
  int m = fids.size();
  assert(m > 0);

  TTool::Application *app = TTool::getApplication();

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
    assert(firstFrameId <= fid && fid <= lastFrameId);
    double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t        = TInbetween::interpolation(t, algorithm);
    // Setto il fid come corrente per notificare il cambiamento dell'immagine
    if (app) app->getCurrentFrame()->setFid(fid);

    doErase(backward ? 1 - t : t, sl, fid, firstImage, lastImage,
            eraseFunction);
    notifyImageChanged();
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

void EraserTool::doMultiErase(int firstFrameIdx, int lastFrameIdx,
                              const std::vector<TStroke *> firstStrokes,
                              const std::vector<TStroke *> lastStrokes,
                              EraserTool::EraseFunction eraseFunction) {
  TXshSimpleLevel *sl =
      TTool::getApplication()->getCurrentLevel()->getLevel()->getSimpleLevel();
  TVectorImageP firstImage = new TVectorImage();
  TVectorImageP lastImage  = new TVectorImage();
  for (int i = 0; i < firstStrokes.size(); i++)
    firstImage->addStroke(firstStrokes[i]);
  for (int i = 0; i < lastStrokes.size(); i++)
    lastImage->addStroke(lastStrokes[i]);

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
    TVectorImageP img = (TVectorImageP)cell.getImage(true);
    if (!img) continue;
    double t = m > 1 ? (double)i / (double)(m - 1) : 0.5;
    t        = TInbetween::interpolation(t, algorithm);
    // Setto il fid come corrente per notificare il cambiamento dell'immagine
    if (app) app->getCurrentFrame()->setFrame(row);

    doErase(backward ? 1 - t : t, sl, fid, firstImage, lastImage,
            eraseFunction);
    notifyImageChanged();
  }
  TUndoManager::manager()->endBlock();
}

//-----------------------------------------------------------------------------

//! Viene richiamata la \b eraseRegion per il frame giusto nel caso della
//! modalita' "Frame Range".
/*!Nei casi in cui \b t e' diverso da zero e uno, viene generata una nuova \b
   TVectorImageP richiamando la \b TInbetween.
        La nuova immagine contiene lo stroke da dare alla eraseRegion. \b fid e'
   il TFrameId dell'immagine sulla quale
        bisogna effettuare la cancellazione.*/
void EraserTool::doErase(double t, const TXshSimpleLevelP &sl,
                         const TFrameId &fid, const TVectorImageP &firstImage,
                         const TVectorImageP &lastImage,
                         EraserTool::EraseFunction eraseFunction) {
  //	TImageLocation imageLocation(m_level->getName(),fid);
  TVectorImageP img = sl->getFrame(fid, true);
  if (t == 0)
    for (int i = 0; i < firstImage->getStrokeCount(); i++)
    (this->*eraseFunction)(img, fid, firstImage->getStroke(i));  //,imageLocation);
  else if (t == 1)
    for (int i = 0; i < lastImage->getStrokeCount(); i++)
      (this->*eraseFunction)(img, fid, lastImage->getStroke(i));  //,imageLocation);
  else {
//    assert(firstImage->getStrokeCount() == 1);
//    assert(lastImage->getStrokeCount() == 1);
    TVectorImageP vi = TInbetween(firstImage, lastImage).tween(t);
//    assert(vi->getStrokeCount() == 1);
    for (int i = 0; i < vi->getStrokeCount(); i++)
      (this->*eraseFunction)(img, fid, vi->getStroke(i));  //,imageLocation);
  }
}

//-----------------------------------------------------------------------------

//! Effettua la cancellazione nella modalita' "Frame range".
/*! Se il primo frame e' gia stato selezionato richiama la \b doMultiErase;
 altrimenti viene inizializzato
 \b m_firstStroke.*/
void EraserTool::multiErase(std::vector<TStroke *> lastStrokes, const std::wstring eraseType, const TMouseEvent &e,
                            EraserTool::EraseFunction eraseFunction) {
  TTool::Application *application = TTool::getApplication();
  if (!application) return;

  if (m_firstFrameSelected) {
    bool isEditingLevel = application->getCurrentFrame()->isEditingLevel();

    if (m_firstStrokes.size() && lastStrokes.size()) {
      TFrameId tmpFrameId = getCurrentFid();
      int tmpFrameIdx     = getFrame();
      if (isEditingLevel)
        doMultiErase(m_firstFrameId, tmpFrameId, m_firstStrokes, lastStrokes,
                     eraseFunction);
      else
        doMultiErase(m_firstFrameIdx, tmpFrameIdx, m_firstStrokes, lastStrokes,
                     eraseFunction);
    }
    if (e.isShiftPressed()) {
      m_firstStrokes.clear();
      if (eraseType == POLYLINE_ERASE) {
        if (m_polyline.size() > 1) {
          for (int i = 0; i < m_polyline.getBrushCount(); i++)
            m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
        }      
      } else {
        double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
        for (int i = 0; i < m_track.getBrushCount(); i++)
          m_firstStrokes.push_back(m_track.makeStroke(error, i));
      }
      m_firstFrameId  = getCurrentFid();
      m_firstFrameIdx = getFrame();
    } else {
      if (application->getCurrentFrame()->isEditingScene()) {
        application->getCurrentColumn()->setColumnIndex(m_currCell.first);
        application->getCurrentFrame()->setFrame(m_currCell.second);
      } else
        application->getCurrentFrame()->setFid(m_veryFirstFrameId);
      resetMulti();
      if (eraseType == POLYLINE_ERASE) m_polyline.reset();
    }
  } else {
    m_firstStrokes.clear();
    if (eraseType == POLYLINE_ERASE) {
      if (m_polyline.size() > 1) {
        for (int i = 0; i < m_polyline.getBrushCount(); i++)
          m_firstStrokes.push_back(m_polyline.makeRectangleStroke(i));
      }
    } else {
      double error = (30.0 / 11) * sqrt(getPixelSize() * getPixelSize());
      for (int i = 0; i < m_track.getBrushCount(); i++)
        m_firstStrokes.push_back(m_track.makeStroke(error, i));
    }
    if (application->getCurrentFrame()->isEditingScene())
      m_currCell =
          std::pair<int, int>(application->getCurrentColumn()->getColumnIndex(),
                              application->getCurrentFrame()->getFrame());
  }
}

//-----------------------------------------------------------------------------
/*! When the tool is switched during dragging, Erase end processing is performed
 */
void EraserTool::onDeactivate() {
  if (!m_active) return;

  m_active = false;

  // TODO : finishing erase procedure is only available for normal type.
  // Supporting other types is needed. 2016/1/22 shun_iwasawa
  if (m_eraseType.getValue() != NORMAL_ERASE) return;

  TImageP image(getImage(true));
  TVectorImageP vi                = image;
  TTool::Application *application = TTool::getApplication();
  if (!vi || !application) return;

  stopErase(vi);
}

//-----------------------------------------------------------------------------

void EraserTool::doEraseAtPos(TVectorImageP vi, const TPointD &pos) {
  erase(vi, pos);

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool || !symmetryTool->isGuideEnabled()) return;

  SymmetryObject symmObj = symmetryTool->getSymmetryObject();
  if (symmObj.getLines() < 2) return;

  std::vector<TPointD> points = symmetryTool->getSymmetryPoints(
      pos, TPointD(), getViewer()->getDpiScale());

  for (int i = 0; i < points.size(); i++) {
    if (points[i] == pos) continue;
    erase(vi, points[i]);
  }
}

// TTool *getEraserTool() {return &eraserTool;}
