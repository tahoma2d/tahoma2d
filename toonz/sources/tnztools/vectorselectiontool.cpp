

#include "vectorselectiontool.h"

// TnzTools includes
#include "tools/toolhandle.h"
#include "tools/imagegrouping.h"
#include "tools/cursors.h"

// TnzQt includes
#include "toonzqt/selectioncommandids.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/imageutils.h"

// TnzLib includes
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobject.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "drawutil.h"

using namespace ToolUtils;
using namespace DragSelectionTool;

//********************************************************************************
//    Global variables
//********************************************************************************

namespace {

VectorSelectionTool l_vectorSelectionTool(TTool::Vectors);
TEnv::IntVar l_strokeSelectConstantThickness("SelectionToolConstantThickness",
                                             0);
TEnv::IntVar l_strokeSelectIncludeIntersection(
    "SelectionToolIncludeIntersection", 0);
TEnv::StringVar l_strokeSelectionTarget("SelectionToolVectorTarget",
                                        "Standard");
TEnv::StringVar l_strokeSelectionType("SelectionToolVectorType", "Rectangular");

const int l_dragThreshold = 10;  //!< Distance, in pixels, the user has to
                                 //!  move from a button press to trigger a
                                 //!  selection drag.
}  // namespace

//********************************************************************************
//    Local namespace  stuff
//********************************************************************************

namespace {

FourPoints getFourPointsFromVectorImage(const TVectorImageP &img,
                                        const std::set<int> &styleIds,
                                        double &maxThickness) {
  FourPoints p;

  if (styleIds.empty()) {
    p = img->getBBox();

    for (UINT i = 0; i < img->getStrokeCount(); i++) {
      TStroke *s = img->getStroke(i);

      for (int j = 0; j < s->getControlPointCount(); j++) {
        double thick = s->getControlPoint(j).thick;
        if (maxThickness < thick) maxThickness = thick;
      }
    }
  } else {
    TRectD bbox;

    for (UINT i = 0; i < img->getStrokeCount(); i++) {
      TStroke *s = img->getStroke(i);
      if (!styleIds.count(s->getStyle())) continue;

      if (bbox.isEmpty())
        bbox = s->getBBox();
      else
        bbox += s->getBBox();

      for (int j = 0; j < s->getControlPointCount(); j++) {
        double thick = s->getControlPoint(j).thick;
        if (maxThickness < thick) maxThickness = thick;
      }
    }

    p = bbox;
  }

  return p;
}

//-----------------------------------------------------------------------------

bool getStrokeIndexFromPos(UINT &index, const TVectorImageP &vi,
                           const TPointD &pos, double pixelSize, TAffine aff) {
  if (!vi) return false;
  double t, dist2 = 0;
  double maxDist   = 5 * pixelSize;
  double maxDist2  = maxDist * maxDist;
  double checkDist = maxDist2 * 4;

  if (vi->getNearestStroke(pos, t, index, dist2)) {
    TStroke *strokeRef = vi->getStroke(index);
    TThickPoint cursor = strokeRef->getThickPoint(t);
    double len         = cursor.thick * pixelSize * sqrt(aff.det());
    checkDist          = std::max(checkDist, (len * len));
  }

  return (dist2 < checkDist);
}

//-----------------------------------------------------------------------------

static bool currentOrNotSelected(const VectorSelectionTool &tool,
                                 const TFrameId &fid) {
  return (tool.getCurrentFid() == fid ||
          (tool.isSelectedFramesType() &&
           tool.getSelectedFrames().count(fid) == 0));
}

//-----------------------------------------------------------------------------

inline void notifySelectionChanged() {
  TTool::getApplication()->getCurrentSelection()->notifySelectionChanged();
}

}  // namespace

//********************************************************************************
//    VectorFreeDeformer  implementation
//********************************************************************************

VectorFreeDeformer::VectorFreeDeformer(TVectorImageP vi,
                                       std::set<int> strokeIndexes)
    : FreeDeformer()
    , m_vi(vi)
    , m_strokeIndexes(strokeIndexes)
    , m_preserveThickness(false)
    , m_computeRegion(false)
    , m_flip(false) {
  TRectD r;

  std::set<int>::iterator it, iEnd = m_strokeIndexes.end();
  for (it = m_strokeIndexes.begin(); it != iEnd; ++it) {
    TStroke *stroke = m_vi->getStroke(*it);
    r += stroke->getBBox();
    m_originalStrokes.push_back(new TStroke(*stroke));
  }

  m_originalP00 = r.getP00();
  m_originalP11 = r.getP11();
  m_newPoints.push_back(m_originalP00);
  m_newPoints.push_back(r.getP10());
  m_newPoints.push_back(m_originalP11);
  m_newPoints.push_back(r.getP01());
}

//-----------------------------------------------------------------------------

VectorFreeDeformer::~VectorFreeDeformer() {
  clearPointerContainer(m_originalStrokes);
}

//-----------------------------------------------------------------------------

void VectorFreeDeformer::setPreserveThickness(bool preserveThickness) {
  m_preserveThickness = preserveThickness;
}

//-----------------------------------------------------------------------------

void VectorFreeDeformer::setComputeRegion(bool computeRegion) {
  m_computeRegion = computeRegion;
}

//-----------------------------------------------------------------------------

void VectorFreeDeformer::setFlip(bool flip) { m_flip = flip; }

//-----------------------------------------------------------------------------

void VectorFreeDeformer::setPoint(int index, const TPointD &p) {
  m_newPoints[index] = p;
}

//-----------------------------------------------------------------------------

void VectorFreeDeformer::setPoints(const TPointD &p0, const TPointD &p1,
                                   const TPointD &p2, const TPointD &p3) {
  m_newPoints[0] = p0;
  m_newPoints[1] = p1;
  m_newPoints[2] = p2;
  m_newPoints[3] = p3;
}

//-----------------------------------------------------------------------------

void VectorFreeDeformer::deformRegions() {
  if (m_strokeIndexes.empty() || !m_computeRegion) return;

  std::vector<int> selectedIndexes(m_strokeIndexes.begin(),
                                   m_strokeIndexes.end());

  m_vi->notifyChangedStrokes(selectedIndexes, m_originalStrokes, m_flip);
  m_computeRegion = false;
}

//-----------------------------------------------------------------------------

void VectorFreeDeformer::deformImage() {
  // debug
  assert(m_strokeIndexes.size() == m_originalStrokes.size());

  // release
  if (m_strokeIndexes.size() != m_originalStrokes.size()) {
    return;
  }

  QMutexLocker lock(m_vi->getMutex());

  std::size_t i = 0;
  for (auto it = m_strokeIndexes.begin(), end = m_strokeIndexes.end();
       it != end; ++it) {
    TStroke *stroke         = m_vi->getStroke(*it);
    TStroke *originalStroke = m_originalStrokes[i++];

    assert(stroke->getControlPointCount() ==
           originalStroke->getControlPointCount());
    for (int j = 0, count = stroke->getControlPointCount(); j < count; ++j) {
      TThickPoint p = deform(originalStroke->getControlPoint(j));
      stroke->setControlPoint(j, p);
    }
  }

  if (m_computeRegion) deformRegions();
}

//-----------------------------------------------------------------------------

TThickPoint VectorFreeDeformer::deform(TThickPoint point) {
  double vs  = m_originalP11.x - m_originalP00.x;
  double s   = (vs == 0) ? 0 : (point.x - m_originalP00.x) / vs;
  double vt  = m_originalP11.y - m_originalP00.y;
  double t   = (vt == 0) ? 0 : (point.y - m_originalP00.y) / vt;
  TPointD A  = m_newPoints[0];
  TPointD B  = m_newPoints[1];
  TPointD C  = m_newPoints[2];
  TPointD D  = m_newPoints[3];
  TPointD AD = (1 - t) * A + t * D;
  TPointD BC = (1 - t) * B + t * C;
  TPointD p  = (1 - s) * AD + s * BC;

  double thickness = point.thick;
  if (!m_preserveThickness) {
    double eps          = 1.e-2;
    TPointD p0x         = TPointD(p.x - eps, p.x);
    TPointD p1x         = TPointD(p.x + eps, p.x);
    TPointD p0y         = TPointD(p.x, p.y - eps);
    TPointD p1y         = TPointD(p.x, p.y + eps);
    m_preserveThickness = true;
    TThickPoint newp0x  = deform(p0x);
    TThickPoint newp1x  = deform(p1x);
    TThickPoint newp0y  = deform(p0y);
    TThickPoint newp1y  = deform(p1y);
    m_preserveThickness = false;
    double newA         = fabs(cross(newp1x - newp0x, newp1y - newp0y));
    double a            = 4 * eps * eps;
    thickness *= sqrt(newA / a);
  }
  return TThickPoint(p, thickness);
}

//********************************************************************************
//    UndoChangeStrokes  implementation
//********************************************************************************

DragSelectionTool::UndoChangeStrokes::UndoChangeStrokes(
    TXshSimpleLevel *level, const TFrameId &frameId, VectorSelectionTool *tool,
    const StrokeSelection &selection)
    : ToolUtils::TToolUndo(level, frameId)
    , m_tool(tool)
    , m_selectionCount(tool->getSelectionCount())  // Not related to selection
    , m_oldBBox(tool->getBBox())
    , m_oldCenter(tool->getCenter())
    , m_oldDeformValues(tool->m_deformValues)
    , m_newDeformValues()
    , m_flip(false) {
  TVectorImageP vi = m_level->getFrame(m_frameId, false);
  if (!vi) {
    assert(vi);
    return;
  }

  const StrokeSelection::IndexesContainer &indexes = selection.getSelection();
  m_indexes.assign(indexes.begin(), indexes.end());

  registerStrokes(true);
}

//-----------------------------------------------------------------------------

DragSelectionTool::UndoChangeStrokes::UndoChangeStrokes(
    TXshSimpleLevel *level, const TFrameId &frameId, VectorSelectionTool *tool,
    const LevelSelection &selection)
    : ToolUtils::TToolUndo(level, frameId)
    , m_tool(tool)
    , m_selectionCount(tool->getSelectionCount())  // Not related to selection
    , m_oldBBox(tool->getBBox())
    , m_oldCenter(tool->getCenter())
    , m_oldDeformValues(tool->m_deformValues)
    , m_newDeformValues()
    , m_flip(false) {
  TVectorImageP vi = m_level->getFrame(m_frameId, false);
  if (!vi) {
    assert(vi);
    return;
  }

  m_indexes = getSelectedStrokes(*vi, selection);

  registerStrokes(true);
}

//-----------------------------------------------------------------------------

DragSelectionTool::UndoChangeStrokes::~UndoChangeStrokes() {
  clearPointerContainer(m_oldStrokes);
  clearPointerContainer(m_newStrokes);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoChangeStrokes::registerStrokes(bool beforeModify) {
  TVectorImageP vi = m_level->getFrame(m_frameId, false);
  if (!vi) {
    assert(vi);
    return;
  }

  std::vector<TStroke *> &strokes = beforeModify ? m_oldStrokes : m_newStrokes;

  TRectD bbox;

  int s, sCount = int(m_indexes.size());
  for (s = 0; s != sCount; ++s) {
    TStroke *stroke = vi->getStroke(m_indexes[s]);
    bbox += stroke->getBBox();

    strokes.push_back(new TStroke(*stroke));
  }

  if (beforeModify && !bbox.isEmpty()) {
    ImageUtils::getFillingInformationOverlappingArea(vi, m_regionsData, bbox);
  } else {
    m_newBBox         = m_tool->getBBox();
    m_newCenter       = m_tool->getCenter();
    m_newDeformValues = m_tool->m_deformValues;
  }
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoChangeStrokes::transform(
    const std::vector<TStroke *> &strokes, FourPoints bbox, TPointD center,
    DragSelectionTool::DeformValues deformValues) const {
  TVectorImageP image = m_level->getFrame(m_frameId, true);
  if (!image) {
    assert(image);
    return;
  }

  int s, sCount = int(m_indexes.size());
  for (s = 0; s != sCount; ++s) {
    int index = m_indexes[s];

    TStroke *sourcesStroke = strokes[s], *stroke = image->getStroke(index);

    int cp, cpCount = stroke->getControlPointCount();
    for (cp = 0; cp != cpCount; ++cp)
      stroke->setControlPoint(cp, sourcesStroke->getControlPoint(cp));
  }

  image->notifyChangedStrokes(m_indexes, strokes, m_flip);

  if (!m_tool->isSelectionEmpty() &&
      m_selectionCount == m_tool->getSelectionCount()) {
    m_tool->setBBox(bbox);
    m_tool->setCenter(center);
  } else
    m_tool->computeBBox();

  m_tool->notifyImageChanged();
  m_tool->m_deformValues = deformValues;
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoChangeStrokes::restoreRegions() const {
  TVectorImageP vi = m_level->getFrame(m_frameId, true);
  if (!vi) {
    assert(vi);
    return;
  }

  ImageUtils::assignFillingInformation(*vi, m_regionsData);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoChangeStrokes::undo() const {
  transform(m_oldStrokes, m_oldBBox, m_oldCenter, m_oldDeformValues);
  restoreRegions();
  m_tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::UndoChangeStrokes::redo() const {
  transform(m_newStrokes, m_newBBox, m_newCenter, m_newDeformValues);
  m_tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

int DragSelectionTool::UndoChangeStrokes::getSize() const {
  return sizeof(*this) + sizeof(*m_tool);
}

//=============================================================================
// UndoChangeOutlineStyle
//-----------------------------------------------------------------------------

class UndoChangeOutlineStyle final : public ToolUtils::TToolUndo {
  std::vector<TStroke::OutlineOptions> m_oldOptions, m_newOptions;
  FourPoints m_oldBBox, m_newBBox;
  VectorSelectionTool *m_tool;
  std::vector<int> m_indexes;
  int m_selectionCount;

public:
  UndoChangeOutlineStyle(TXshSimpleLevel *level, const TFrameId &frameId,
                         VectorSelectionTool *tool);
  ~UndoChangeOutlineStyle() {}
  void registerStrokes(bool beforeModify = false);
  void transform(const std::vector<TStroke::OutlineOptions> &options,
                 FourPoints bbox) const;
  void undo() const override;
  void redo() const override;
  int getSize() const override;
};

//-----------------------------------------------------------------------------

UndoChangeOutlineStyle::UndoChangeOutlineStyle(TXshSimpleLevel *level,
                                               const TFrameId &frameId,
                                               VectorSelectionTool *tool)
    : ToolUtils::TToolUndo(level, frameId)
    , m_tool(tool)
    , m_oldBBox(tool->getBBox())
    , m_selectionCount(tool->getSelectionCount()) {
  TVectorImageP image = m_level->getFrame(m_frameId, false);
  assert(!!image);
  if (!image) return;
  StrokeSelection *strokeSelection =
      dynamic_cast<StrokeSelection *>(tool->getSelection());
  int i;
  for (i = 0; i < (int)image->getStrokeCount(); i++) {
    if (!strokeSelection->isSelected(i) && !m_tool->isLevelType() &&
        !m_tool->isSelectedFramesType())
      continue;
    m_indexes.push_back(i);
  }
  registerStrokes(true);
}

//-----------------------------------------------------------------------------

void UndoChangeOutlineStyle::registerStrokes(bool beforeModify) {
  TVectorImageP image = m_level->getFrame(m_frameId, false);
  assert(!!image);
  if (!image) return;
  int i;
  for (i = 0; i < (int)m_indexes.size(); i++) {
    if (beforeModify)
      m_oldOptions.push_back(image->getStroke(m_indexes[i])->outlineOptions());
    else
      m_newOptions.push_back(image->getStroke(m_indexes[i])->outlineOptions());
  }
  if (!beforeModify) m_newBBox = m_tool->getBBox();
}

//-----------------------------------------------------------------------------

void UndoChangeOutlineStyle::transform(
    const std::vector<TStroke::OutlineOptions> &options,
    FourPoints bbox) const {
  TVectorImageP image = m_level->getFrame(m_frameId, true);
  assert(!!image);
  if (!image) return;
  int i;
  for (i = 0; i < (int)m_indexes.size(); i++) {
    int index                                 = m_indexes[i];
    image->getStroke(index)->outlineOptions() = options[i];
  }
  if (!m_tool->isSelectionEmpty() &&
      m_selectionCount == m_tool->getSelectionCount())
    m_tool->setBBox(bbox);
  else
    m_tool->computeBBox();
  m_tool->notifyImageChanged();
  TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void UndoChangeOutlineStyle::undo() const {
  transform(m_oldOptions, m_oldBBox);
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
  m_tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

void UndoChangeOutlineStyle::redo() const {
  transform(m_newOptions, m_newBBox);
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
  m_tool->notifyImageChanged();
}

//-----------------------------------------------------------------------------

int UndoChangeOutlineStyle::getSize() const {
  // NOTE: This is definitely wrong... sizeof(vector) DOES NOT correspond to its
  // actual size - as it is internally allocated with new!!
  return sizeof(*this) + sizeof(*m_tool);
}

//=============================================================================
// VectorDeformTool::VFDScopedBlock
//-----------------------------------------------------------------------------

struct VectorDeformTool::VFDScopedBlock  //! Type bounding the scope of free
                                         //! deformers in the tool.
{
  VFDScopedBlock(SelectionTool *tool) : m_tool(tool) {
    m_tool->setNewFreeDeformer();
  }
  ~VFDScopedBlock() { m_tool->clearDeformers(); }

private:
  SelectionTool *m_tool;  //!< [\p external]  Tool owning the deformers.
};

//=============================================================================
// VectorDeformTool
//-----------------------------------------------------------------------------

DragSelectionTool::VectorDeformTool::VectorDeformTool(VectorSelectionTool *tool)
    : DeformTool(tool), m_undo() {
  if (!TTool::getApplication()->getCurrentObject()->isSpline()) {
    m_undo.reset(new UndoChangeStrokes(
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel(),
        tool->getCurrentFid(), tool, tool->strokeSelection()));
  }
}

//-----------------------------------------------------------------------------

DragSelectionTool::VectorDeformTool::~VectorDeformTool() {
  // DO NOT REMOVE - DESTRUCTS TYPES INCOMPLETE IN THE HEADER
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorDeformTool::applyTransform(FourPoints bbox,
                                                         bool onFastDragging) {
  SelectionTool *tool = getTool();

  std::unique_ptr<VFDScopedBlock> localVfdScopedBlock;
  if (!m_vfdScopedBlock) {
    std::unique_ptr<VFDScopedBlock> &vfdScopedBlock =
        m_isDragging ? m_vfdScopedBlock : localVfdScopedBlock;

    vfdScopedBlock.reset(new VFDScopedBlock(tool));
  }

  VectorFreeDeformer *freeDeformer =
      static_cast<VectorFreeDeformer *>(tool->getFreeDeformer());

  const bool stayedTheSame = bbox.getP00() == freeDeformer->getPoint(0) &&
                             bbox.getP10() == freeDeformer->getPoint(1) &&
                             bbox.getP11() == freeDeformer->getPoint(2) &&
                             bbox.getP01() == freeDeformer->getPoint(3);

  freeDeformer->setPoints(bbox.getP00(), bbox.getP10(), bbox.getP11(),
                          bbox.getP01());
  freeDeformer->setComputeRegion(!m_isDragging);
  freeDeformer->setPreserveThickness(tool->isConstantThickness());
  freeDeformer->setFlip(isFlip());

  if (!TTool::getApplication()->getCurrentObject()->isSpline() && m_undo)
    m_undo->setFlip(isFlip());

  freeDeformer->deformImage();

  tool->invalidate();

  if (!m_isDragging) tool->notifyImageChanged();

  if (!stayedTheSame) tool->m_deformValues.m_isSelectionModified = true;

  if (!m_isDragging && (tool->isLevelType() || tool->isSelectedFramesType()))
    transformWholeLevel();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorDeformTool::addTransformUndo() {
  if (TTool::getApplication()->getCurrentObject()->isSpline())
    TUndoManager::manager()->add(
        new UndoPath(getTool()
                         ->getXsheet()
                         ->getStageObject(getTool()->getObjectId())
                         ->getSpline()));
  else if (m_undo) {
    m_undo->registerStrokes();
    TUndoManager::manager()->add(m_undo.release());
  }
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorDeformTool::transformWholeLevel() {
  VectorSelectionTool *tool = dynamic_cast<VectorSelectionTool *>(m_tool);
  assert(tool);

  assert(!tool->levelSelection().isEmpty());

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();

  std::vector<TFrameId> fids;
  level->getFids(fids);

  // Remove unwanted fids
  fids.erase(std::remove_if(
                 fids.begin(), fids.end(),
                 [tool](const TFrameId &fid) { return currentOrNotSelected(*tool, fid); }),
             fids.end());

  TUndoManager::manager()->beginBlock();
  {
    addTransformUndo();  // For current frame

    int f, fCount = int(fids.size());
    for (f = 0; f != fCount; ++f) {
      const TFrameId &fid = fids[f];
      int t               = f + 1;  // Current frame's data is always at index 0
                                    // The others are thus shifted by 1
      // Skip nonselected frames
      if (tool->getCurrentFid() == fid ||
          (tool->isSelectedFramesType() &&
           tool->getSelectedFrames().count(fid) == 0))
        continue;

      TVectorImageP vi = level->getFrame(fid, true);
      if (!vi) continue;

      std::unique_ptr<UndoChangeStrokes> undo(
          new UndoChangeStrokes(level, fid, tool, tool->levelSelection()));

      std::set<int> strokesIndices;

      for (int s = 0; s < (int)vi->getStrokeCount(); ++s)
        strokesIndices.insert(s);

      FourPoints bbox = tool->getBBox(t);

      VectorFreeDeformer *freeDeformer =
          static_cast<VectorFreeDeformer *>(tool->getFreeDeformer(t));
      freeDeformer->setPoints(bbox.getPoint(0), bbox.getPoint(1),
                              bbox.getPoint(2), bbox.getPoint(3));
      freeDeformer->setComputeRegion(true);
      freeDeformer->setPreserveThickness(tool->isConstantThickness());
      freeDeformer->setFlip(isFlip());
      freeDeformer->deformImage();

      undo->registerStrokes();

      TUndoManager::manager()->add(undo.release());
    }
  }
  TUndoManager::manager()->endBlock();

  // Finally, notify changed frames
  std::for_each(fids.begin(), fids.end(),
  // NOTE: current frame is not here - it should be,
  // but it's currently unnecessary, in fact...
  [this](const TFrameId &fid) { m_tool->notifyImageChanged(fid); });

  // notifyImageChanged(fid) must be invoked OUTSIDE of the loop - since it
  // seems to imply notifyImageChanged()
  // on CURRENT image - which seems wrong, but it's too low-level and I'm not
  // changing it.
  // This reasonably leads to computeBBox(), but bboxes are taken as INPUT to
  // this transformation...            >:(
}

//-----------------------------------------------------------------------------

bool DragSelectionTool::VectorDeformTool::isFlip() {
  TPointD scaleValue = getTool()->m_deformValues.m_scaleValue;
  return m_startScaleValue.x * scaleValue.x < 0 ||
         m_startScaleValue.y * scaleValue.y < 0;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorDeformTool::leftButtonUp(const TPointD &pos,
                                                       const TMouseEvent &e) {
  std::unique_ptr<VFDScopedBlock> vfdScopedBlock(std::move(m_vfdScopedBlock));

  SelectionTool *tool = getTool();
  VectorFreeDeformer *deformer =
      dynamic_cast<VectorFreeDeformer *>(tool->getFreeDeformer());
  if (!deformer) return;

  deformer->setComputeRegion(true);
  deformer->setFlip(isFlip());
  deformer->deformRegions();

  if (!tool->isLevelType() && !tool->isSelectedFramesType())
    addTransformUndo();
  else
    transformWholeLevel();

  m_isDragging = false;

  tool->notifyImageChanged();

  VectorSelectionTool *selectionTool =
      dynamic_cast<VectorSelectionTool *>(m_tool);
  selectionTool->setResetCenter(true);
}

//=============================================================================
// VectorRotationTool
//-----------------------------------------------------------------------------

DragSelectionTool::VectorRotationTool::VectorRotationTool(
    VectorSelectionTool *tool)
    : VectorDeformTool(tool), m_rotation(new Rotation(this)) {}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorRotationTool::transform(TAffine aff,
                                                      double angle) {
  SelectionTool *tool = getTool();
  FourPoints newBbox(tool->getBBox() * aff);
  TPointD center = m_rotation->getStartCenter();
  int i;
  for (i = 0; i < tool->getBBoxsCount(); i++) {
    aff = TRotation(center, angle);
    tool->setBBox(tool->getBBox(i) * aff, i);
  }
  applyTransform(newBbox);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorRotationTool::leftButtonDrag(
    const TPointD &pos, const TMouseEvent &e) {
  VectorSelectionTool *tool = dynamic_cast<VectorSelectionTool *>(m_tool);
  tool->setResetCenter(false);
  m_rotation->leftButtonDrag(pos, e);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorRotationTool::draw() { m_rotation->draw(); }

//=============================================================================
// VectorFreeDeformTool
//-----------------------------------------------------------------------------

DragSelectionTool::VectorFreeDeformTool::VectorFreeDeformTool(
    VectorSelectionTool *tool)
    : VectorDeformTool(tool), m_freeDeform(new FreeDeform(this)) {}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorFreeDeformTool::leftButtonDrag(
    const TPointD &pos, const TMouseEvent &e) {
  VectorSelectionTool *tool = dynamic_cast<VectorSelectionTool *>(m_tool);
  tool->setResetCenter(false);
  m_freeDeform->leftButtonDrag(pos, e);
}

//=============================================================================
// VectorMoveSelectionTool
//-----------------------------------------------------------------------------

DragSelectionTool::VectorMoveSelectionTool::VectorMoveSelectionTool(
    VectorSelectionTool *tool)
    : VectorDeformTool(tool), m_moveSelection(new MoveSelection(this)) {}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorMoveSelectionTool::transform(TAffine aff) {
  SelectionTool *tool = getTool();
  int i;
  for (i = 0; i < (int)tool->getBBoxsCount(); i++)
    tool->setBBox(tool->getBBox(i) * aff, i);
  getTool()->setCenter(aff * tool->getCenter());
  applyTransform(tool->getBBox());
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorMoveSelectionTool::leftButtonDown(
    const TPointD &pos, const TMouseEvent &e) {
  m_moveSelection->leftButtonDown(pos, e);
  VectorDeformTool::leftButtonDown(pos, e);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorMoveSelectionTool::leftButtonDrag(
    const TPointD &pos, const TMouseEvent &e) {
  VectorSelectionTool *tool = dynamic_cast<VectorSelectionTool *>(m_tool);
  tool->setResetCenter(false);
  if (e.isCtrlPressed() ||
      norm2(pos - getStartPos()) > l_dragThreshold * getTool()->getPixelSize())
    m_moveSelection->leftButtonDrag(pos, e);
  else  // snap to the original position
    m_moveSelection->leftButtonDrag(getStartPos(), e);
}

//=============================================================================
// VectorScaleTool
//-----------------------------------------------------------------------------

DragSelectionTool::VectorScaleTool::VectorScaleTool(VectorSelectionTool *tool,
                                                    ScaleType type)
    : VectorDeformTool(tool), m_scale(new Scale(this, type)) {}

//-----------------------------------------------------------------------------

TPointD DragSelectionTool::VectorScaleTool::transform(int index, TPointD newPos,
                                                      bool onFastDragging) {
  SelectionTool *tool = getTool();
  TPointD scaleValue  = tool->m_deformValues.m_scaleValue;

  std::vector<FourPoints> startBboxs = m_scale->getStartBboxs();
  TPointD center                     = m_scale->getStartCenter();
  FourPoints bbox = m_scale->bboxScaleInCenter(index, startBboxs[0], newPos,
                                               scaleValue, center, true);
  if (bbox == startBboxs[0]) return scaleValue;

  bool scaleInCenter = m_scale->scaleInCenter();
  // Se non ho scalato rispetto al centro calcolo la posizione del nuovo centro
  if (!scaleInCenter)
    tool->setCenter(m_scale->getNewCenter(index, startBboxs[0], scaleValue));

  if (tool->isLevelType() || tool->isSelectedFramesType()) {
    int i;
    for (i = 1; i < (int)tool->getBBoxsCount(); i++) {
      FourPoints oldBbox = startBboxs[i];
      TPointD frameCenter =
          scaleInCenter ? center
                        : oldBbox.getPoint(getSymmetricPointIndex(index));
      TPointD newp =
          m_scale->getScaledPoint(index, oldBbox, scaleValue, frameCenter);
      FourPoints newBbox = m_scale->bboxScaleInCenter(
          index, oldBbox, newp, scaleValue, frameCenter, false);
      tool->setBBox(newBbox, i);
      if (!scaleInCenter)
        tool->setCenter(m_scale->getNewCenter(index, oldBbox, scaleValue), i);
    }
  }
  tool->setBBox(bbox);
  applyTransform(bbox);
  return scaleValue;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorScaleTool::leftButtonDown(const TPointD &pos,
                                                        const TMouseEvent &e) {
  m_scale->leftButtonDown(pos, e);
  VectorDeformTool::leftButtonDown(pos, e);
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorScaleTool::leftButtonDrag(const TPointD &pos,
                                                        const TMouseEvent &e) {
  VectorSelectionTool *tool = dynamic_cast<VectorSelectionTool *>(m_tool);
  tool->setResetCenter(false);
  m_scale->leftButtonDrag(pos, e);
}

//=============================================================================
// VectorChangeThicknessTool
//-----------------------------------------------------------------------------

DragSelectionTool::VectorChangeThicknessTool::VectorChangeThicknessTool(
    VectorSelectionTool *tool)
    : DragTool(tool), m_curPos(), m_firstPos(), m_thicknessChange(0) {
  TVectorImageP vi = TImageP(tool->getImage(false));
  assert(vi);

  setStrokesThickness(*vi);

  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  m_undo.reset(new UndoChangeStrokes(level, tool->getCurrentFid(), tool,
                                     tool->strokeSelection()));
}

//-----------------------------------------------------------------------------

VectorChangeThicknessTool::~VectorChangeThicknessTool() {}

//-----------------------------------------------------------------------------

namespace {
namespace SetStrokeThickness {

using namespace DragSelectionTool;

struct Data {
  VectorChangeThicknessTool &m_tool;
  const TVectorImage &m_vi;
};
}  // namespace SetStrokeThickness
}  // namespace

void DragSelectionTool::VectorChangeThicknessTool::setStrokesThickness(
    TVectorImage &vi) {
  struct locals {
    static void setThickness(const SetStrokeThickness::Data &data, int s) {
      const TStroke &stroke = *data.m_vi.getStroke(s);

      std::vector<double> strokeThickness;

      int cp, cpCount = stroke.getControlPointCount();
      strokeThickness.reserve(cpCount);

      for (cp = 0; cp != cpCount; ++cp)
        strokeThickness.push_back(stroke.getControlPoint(cp).thick);

      data.m_tool.m_strokesThickness[s] = strokeThickness;
    }

  };  // locals

  SetStrokeThickness::Data data = {*this, vi};

  VectorSelectionTool *vsTool = static_cast<VectorSelectionTool *>(m_tool);
  const LevelSelection &levelSelection = vsTool->levelSelection();

  if (levelSelection.isEmpty()) {
    StrokeSelection *strokeSelection =
        static_cast<StrokeSelection *>(m_tool->getSelection());
    const std::vector<int> &selectedStrokeIdxs = strokeSelection->getSelection();

    std::for_each(selectedStrokeIdxs.begin(), selectedStrokeIdxs.end(),
      [&data](int s) { locals::setThickness(data, s); });
  } else {
    std::vector<int> strokeIdxs = getSelectedStrokes(vi, levelSelection);

    std::for_each(strokeIdxs.begin(), strokeIdxs.end(),
      [&data](int s) { locals::setThickness(data, s); });
  }
}

//-----------------------------------------------------------------------------

namespace {
namespace ChangeImageThickness {

using namespace DragSelectionTool;

struct Data {
  VectorChangeThicknessTool &m_tool;
  TVectorImage &m_vi;
  double m_newThickness;
};
}  // namespace ChangeImageThickness
}  // namespace

void DragSelectionTool::VectorChangeThicknessTool::changeImageThickness(
    TVectorImage &vi, double newThickness) {
  struct locals {
    static void changeThickness(const ChangeImageThickness::Data &data, int s) {
      TStroke &stroke = *data.m_vi.getStroke(s);

      for (int cp = 0; cp < (int)stroke.getControlPointCount(); ++cp) {
        double thickness =
            tcrop(data.m_tool.m_strokesThickness[s][cp] + data.m_newThickness,
                  0.0, 255.0);

        TThickPoint point(TPointD(stroke.getControlPoint(cp)), thickness);

        stroke.setControlPoint(cp, point);
      }
    }

  };  // locals

  ChangeImageThickness::Data data = {*this, vi, newThickness};

  VectorSelectionTool *vsTool = static_cast<VectorSelectionTool *>(getTool());
  const LevelSelection &levelSelection = vsTool->levelSelection();
  
  if (levelSelection.isEmpty()) {
    StrokeSelection *strokeSelection =
        static_cast<StrokeSelection *>(m_tool->getSelection());
    const std::vector<int> &selectedStrokeIdxs = strokeSelection->getSelection();

    std::for_each(selectedStrokeIdxs.begin(), selectedStrokeIdxs.end(),
      [&data](int s) { locals::changeThickness(data, s); });
  } else {
    std::vector<int> strokeIdxs = getSelectedStrokes(vi, levelSelection);

    std::for_each(strokeIdxs.begin(), strokeIdxs.end(),
      [&data](int s) { locals::changeThickness(data, s); });
  }
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorChangeThicknessTool::addUndo() {
  TVectorImageP curVi = TImageP(getTool()->getImage(true));
  if (!curVi) return;

  m_undo->registerStrokes();

  SelectionTool *tool = getTool();
  if (tool->isLevelType() || tool->isSelectedFramesType()) {
    VectorSelectionTool *vtool = dynamic_cast<VectorSelectionTool *>(tool);
    assert(vtool && !vtool->levelSelection().isEmpty());

    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();

    // Retrieve frames available in the level
    std::vector<TFrameId> fids;
    level->getFids(fids);

    // Remove unwanted frames
    fids.erase(std::remove_if(fids.begin(), fids.end(),
      [vtool](const TFrameId &fid) { return currentOrNotSelected(*vtool, fid); }),
               fids.end());

    TUndoManager::manager()->beginBlock();
    {
      // Current frame added separately
      TUndoManager::manager()->add(m_undo.release());  // Inside an undo block

      // Iterate remaining ones
      int f, fCount = int(fids.size());
      for (f = 0; f != fCount; ++f) {
        const TFrameId &fid = fids[f];

        TVectorImageP vi = level->getFrame(fid, true);
        if (!vi) {
          assert(vi);
          continue;
        }

        // Transform fid's selection
        std::unique_ptr<UndoChangeStrokes> undo(
            new UndoChangeStrokes(level, fid, vtool, vtool->levelSelection()));

        setStrokesThickness(*vi);
        changeImageThickness(*vi, m_thicknessChange);

        m_strokesThickness.clear();
        undo->registerStrokes();

        TUndoManager::manager()->add(undo.release());
      }
    }
    TUndoManager::manager()->endBlock();

    // Finally, notify changed frames
    std::for_each(fids.begin(), fids.end(),
      [this](const TFrameId &fid) { m_tool->notifyImageChanged(fid); });
  } else
    TUndoManager::manager()->add(m_undo.release());  // Outside any undo block
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorChangeThicknessTool::leftButtonDown(
    const TPointD &pos, const TMouseEvent &e) {
  m_curPos   = pos;
  m_firstPos = e.m_pos;
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorChangeThicknessTool::leftButtonDrag(
    const TPointD &pos, const TMouseEvent &e) {
  TPointD delta = e.m_pos - m_firstPos;
  TVectorImageP vi = TImageP(getTool()->getImage(true));
  if (!vi) return;
  VectorSelectionTool *tool = dynamic_cast<VectorSelectionTool *>(m_tool);
  tool->setResetCenter(false);
  m_thicknessChange = delta.y * m_tool->getViewer()->getPixelSize() * 0.2;
  changeImageThickness(*vi, m_thicknessChange);
  getTool()->m_deformValues.m_maxSelectionThickness = m_thicknessChange;
  getTool()->computeBBox();
  getTool()->invalidate();
  m_curPos = pos;
  getTool()->notifyImageChanged();
  TTool::getApplication()->getCurrentTool()->notifyToolChanged();
}

//-----------------------------------------------------------------------------

void DragSelectionTool::VectorChangeThicknessTool::leftButtonUp(
    const TPointD &pos, const TMouseEvent &e) {
  TVectorImageP curVi = TImageP(getTool()->getImage(true));
  if (!curVi) return;
  addUndo();
  m_strokesThickness.clear();
}

//=============================================================================
namespace {
//-----------------------------------------------------------------------------

bool getGroupBBox(const TVectorImage &vi, int strokeIndex, TRectD &gBox) {
  if (!vi.isStrokeGrouped(strokeIndex)) return false;

  gBox = vi.getStroke(strokeIndex)->getBBox();

  int s, sCount = int(vi.getStrokeCount());
  for (s = 0; s != sCount; ++s) {
    if (vi.sameGroup(s, strokeIndex)) gBox += vi.getStroke(s)->getBBox();
  }

  return true;
}

//=============================================================================
// UndoEnterGroup
//-----------------------------------------------------------------------------

class UndoEnterGroup final : public TUndo {
  int m_strokeIndex;
  TVectorImageP m_vi;

public:
  UndoEnterGroup(TVectorImageP vi, int strokeIndex)
      : m_vi(vi), m_strokeIndex(strokeIndex) {}
  void undo() const override {
    m_vi->exitGroup();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    m_vi->enterGroup(m_strokeIndex);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override { return QObject::tr("Enter Group"); }
};

//=============================================================================
// UndoExitGroup
//-----------------------------------------------------------------------------

class UndoExitGroup final : public TUndo {
  int m_strokeIndex;
  TVectorImageP m_vi;

public:
  UndoExitGroup(TVectorImageP vi, int strokeIndex)
      : m_vi(vi), m_strokeIndex(strokeIndex) {}
  void undo() const override {
    m_vi->enterGroup(m_strokeIndex);
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    m_vi->exitGroup();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Exit Group"); }
};

}  // namespace

//=============================================================================
// VectorSelectionTool
//-----------------------------------------------------------------------------

VectorSelectionTool::VectorSelectionTool(int targetType)
    : SelectionTool(targetType)
    , m_selectionTarget("Mode:")
    , m_includeIntersection("Include Intersection", false)
    , m_constantThickness("Preserve Thickness", false)
    , m_levelSelection(m_strokeSelection)
    , m_capStyle("Cap")
    , m_joinStyle("Join")
    , m_miterJoinLimit("Miter:", 0, 100, 4)
    , m_selectionCount(0)
    , m_canEnterGroup(true)
    , m_resetCenter(true) {
  assert(targetType == TTool::Vectors);
  m_prop.bind(m_selectionTarget);
  m_prop.bind(m_includeIntersection);
  m_prop.bind(m_constantThickness);

  m_selectionTarget.addValue(NORMAL_TYPE);
  m_selectionTarget.addValue(SELECTED_FRAMES_TYPE);
  m_selectionTarget.addValue(ALL_LEVEL_TYPE);
  m_selectionTarget.addValue(SAME_STYLE_TYPE);
  m_selectionTarget.addValue(STYLE_SELECTED_FRAMES_TYPE);
  m_selectionTarget.addValue(STYLE_LEVEL_TYPE);
  m_selectionTarget.addValue(BOUNDARY_TYPE);
  m_selectionTarget.addValue(BOUNDARY_SELECTED_FRAMES_TYPE);
  m_selectionTarget.addValue(BOUNDARY_LEVEL_TYPE);

  m_strokeSelection.setView(this);

  m_includeIntersection.setId("IncludeIntersection");
  m_constantThickness.setId("PreserveThickness");
  m_selectionTarget.setId("SelectionMode");

  m_capStyle.addValue(BUTT_WSTR, QString::fromStdWString(BUTT_WSTR));
  m_capStyle.addValue(ROUNDC_WSTR, QString::fromStdWString(ROUNDC_WSTR));
  m_capStyle.addValue(PROJECTING_WSTR,
                      QString::fromStdWString(PROJECTING_WSTR));
  m_capStyle.setId("Cap");

  m_joinStyle.addValue(MITER_WSTR, QString::fromStdWString(MITER_WSTR));
  m_joinStyle.addValue(ROUNDJ_WSTR, QString::fromStdWString(ROUNDJ_WSTR));
  m_joinStyle.addValue(BEVEL_WSTR, QString::fromStdWString(BEVEL_WSTR));
  m_joinStyle.setId("Join");

  m_miterJoinLimit.setId("Miter");

  m_outlineProps.bind(m_capStyle);
  m_outlineProps.bind(m_joinStyle);
  m_outlineProps.bind(m_miterJoinLimit);
}

//------------------------------------------------------------------------------

void VectorSelectionTool::setNewFreeDeformer() {
  clearDeformers();

  TVectorImageP vi = TImageP(getImage(
      true));  // BAD: Should not be done at tool creation...
  if (!vi) return;

  // Current freeDeformer always at index 0
  std::vector<int> &selection = m_strokeSelection.getSelection();
  std::set<int> selectionSet(selection.begin(), selection.end());
  m_freeDeformers.push_back(new VectorFreeDeformer(vi, selectionSet));

  if (isLevelType() || isSelectedFramesType()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();

    // All SELECTED frames' subsequent freeDeformers are stored sequentially
    // after that
    std::vector<TFrameId> fids;
    level->getFids(fids);

    fids.erase(std::remove_if(
                   fids.begin(), fids.end(),
                   [this](const TFrameId &fid) { return currentOrNotSelected(*this, fid); }),
               fids.end());

    std::vector<TFrameId>::iterator ft, fEnd = fids.end();
    for (ft = fids.begin(); ft != fEnd; ++ft) {
      if (TVectorImageP levelVi = level->getFrame(*ft, false)) {
        const std::vector<int> &selectedStrokeIdxs =
            ::getSelectedStrokes(*levelVi, m_levelSelection);
        std::set<int> strokesIndices(selectedStrokeIdxs.begin(),
                                     selectedStrokeIdxs.end());

        m_freeDeformers.push_back(
            new VectorFreeDeformer(levelVi, strokesIndices));
      }
    }
  }
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::isLevelType() const {
  return m_levelSelection.framesMode() == LevelSelection::FRAMES_ALL;
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::isSelectedFramesType() const {
  return m_levelSelection.framesMode() == LevelSelection::FRAMES_SELECTED;
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::isSameStyleType() const {
  return m_levelSelection.filter() == LevelSelection::SELECTED_STYLES;
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::isModifiableSelectionType() const {
  return (m_levelSelection.isEmpty() ||
          m_levelSelection.filter() == LevelSelection::SELECTED_STYLES);
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::updateTranslation() {
  m_selectionTarget.setQStringName(tr("Mode:"));
  m_selectionTarget.setItemUIName(NORMAL_TYPE, tr("Standard"));
  m_selectionTarget.setItemUIName(SELECTED_FRAMES_TYPE, tr("Selected Frames"));
  m_selectionTarget.setItemUIName(ALL_LEVEL_TYPE, tr("Whole Level"));
  m_selectionTarget.setItemUIName(SAME_STYLE_TYPE, tr("Same Style"));
  m_selectionTarget.setItemUIName(STYLE_SELECTED_FRAMES_TYPE,
                                  tr("Same Style on Selected Frames"));
  m_selectionTarget.setItemUIName(STYLE_LEVEL_TYPE,
                                  tr("Same Style on Whole Level"));
  m_selectionTarget.setItemUIName(BOUNDARY_TYPE, tr("Boundary Strokes"));
  m_selectionTarget.setItemUIName(BOUNDARY_SELECTED_FRAMES_TYPE,
                                  tr("Boundaries on Selected Frames"));
  m_selectionTarget.setItemUIName(BOUNDARY_LEVEL_TYPE,
                                  tr("Boundaries on Whole Level"));

  m_includeIntersection.setQStringName(tr("Include Intersection"));
  m_constantThickness.setQStringName(tr("Preserve Thickness"));

  m_capStyle.setQStringName(tr("Cap"));
  m_capStyle.setItemUIName(BUTT_WSTR, tr("Butt cap"));
  m_capStyle.setItemUIName(ROUNDC_WSTR, tr("Round cap"));
  m_capStyle.setItemUIName(PROJECTING_WSTR, tr("Projecting cap"));

  m_joinStyle.setQStringName(tr("Join"));
  m_joinStyle.setItemUIName(MITER_WSTR, tr("Miter join"));
  m_joinStyle.setItemUIName(ROUNDJ_WSTR, tr("Round join"));
  m_joinStyle.setItemUIName(BEVEL_WSTR, tr("Bevel join"));

  m_miterJoinLimit.setQStringName(tr("Miter:"));
  SelectionTool::updateTranslation();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::updateSelectionTarget() {
  // Make the correct selection current
  if (m_selectionTarget.getIndex() == NORMAL_TYPE_IDX) {
    std::vector<int>
        selectedStrokes;  // Retain previously selected strokes across
    selectedStrokes.swap(
        m_strokeSelection.getSelection());  // current selection change

    m_strokeSelection.makeCurrent();  // Empties any (different) previously
                                      // current selection on its own
    selectedStrokes.swap(m_strokeSelection.getSelection());
    return;
  }

  m_levelSelection.makeCurrent();  // Same here

  // Choose frames mode
  LevelSelection::FramesMode framesMode;
  switch (m_selectionTarget.getIndex()) {
  case SAME_STYLE_TYPE_IDX:
  case BOUNDARY_TYPE_IDX:
    framesMode = LevelSelection::FRAMES_CURRENT;
    break;

  case ALL_LEVEL_TYPE_IDX:
  case STYLE_LEVEL_TYPE_IDX:
  case BOUNDARY_LEVEL_TYPE_IDX:
    framesMode = LevelSelection::FRAMES_ALL;
    break;

  case SELECTED_FRAMES_TYPE_IDX:
  case STYLE_SELECTED_FRAMES_TYPE_IDX:
  case BOUNDARY_SELECTED_FRAMES_TYPE_IDX:
    framesMode = LevelSelection::FRAMES_SELECTED;
    break;
  }

  if (framesMode != m_levelSelection.framesMode()) clearSelectedStrokes();

  m_levelSelection.framesMode() = framesMode;

  // Choose filter
  LevelSelection::Filter filter;
  switch (m_selectionTarget.getIndex()) {
  case SELECTED_FRAMES_TYPE_IDX:
  case ALL_LEVEL_TYPE_IDX:
    filter = LevelSelection::WHOLE;
    selectedStyles().clear();
    break;

  case SAME_STYLE_TYPE_IDX:
  case STYLE_SELECTED_FRAMES_TYPE_IDX:
  case STYLE_LEVEL_TYPE_IDX:
    filter = LevelSelection::SELECTED_STYLES;
    break;

  case BOUNDARY_TYPE_IDX:
  case BOUNDARY_SELECTED_FRAMES_TYPE_IDX:
  case BOUNDARY_LEVEL_TYPE_IDX:
    filter = LevelSelection::BOUNDARY_STROKES;
    selectedStyles().clear();
    break;
  }

  if (filter != m_levelSelection.filter()) clearSelectedStrokes();

  m_levelSelection.filter() = filter;
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::finalizeSelection() {
  TVectorImageP vi = TImageP(getImage(false));
  if (vi && !m_levelSelection.isEmpty()) {
    std::vector<int> &selection = m_strokeSelection.getSelection();
    selection.clear();

    // Apply base additive selection
    if (!isSelectedFramesType() || m_selectedFrames.count(getCurrentFid())) {
      // Apply filters
      std::vector<int> selectedStrokes =
          getSelectedStrokes(*vi, m_levelSelection);
      selectedStrokes.swap(selection);
    }
  }

  computeBBox();

  TTool::getApplication()
      ->getCurrentTool()
      ->notifyToolChanged();  // Refreshes toolbar values
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::clearSelectedStrokes() {
  m_strokeSelection.selectNone();
  m_levelSelection.styles().clear();
  m_deformValues.reset();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::modifySelectionOnClick(TImageP image,
                                                 const TPointD &pos,
                                                 const TMouseEvent &e) {
  TVectorImageP vi = TVectorImageP(image);
  assert(m_strokeSelection.getImage() == vi);

  if (!vi) return;

  updateSelectionTarget();  // Make selection current. This is necessary in case
  // some other selection context made another selection current.
  m_firstPos = m_curPos = pos;
  m_selectingRect       = FourPoints();
  m_selecting           = false;
  m_justSelected        = false;

  updateAction(pos, e);
  if (m_what != Inside && m_what != Outside && m_what != ADD_SELECTION) return;

  UINT index         = 0;
  bool modifiableSel = isModifiableSelectionType(),
       strokeAtPos   = getStrokeIndexFromPos(index, vi, pos, getPixelSize(),
                                           getViewer()->getViewMatrix()),
       addStroke     = strokeAtPos && !m_strokeSelection.isSelected(index),
       toggleStroke  = strokeAtPos && e.isShiftPressed();

  m_selecting =
      (modifiableSel && !strokeAtPos  // There must be no stroke under cursor
       && (e.isShiftPressed()  // and either the user is explicitly performing
                               // additional selection
           || (m_strokeSelectionType.getIndex() !=
               POLYLINE_SELECTION_IDX)  // or the tool support immediate
                                        // selection on clear
           || m_strokeSelection
                  .isEmpty()));  // or the strokes list was already cleared

  bool clearTargets     = !(strokeAtPos || e.isShiftPressed() || m_selecting),
       clearSelection   = (addStroke || !strokeAtPos) && !e.isShiftPressed(),
       selectionChanged = clearSelection;

  assert(clearTargets ? clearSelection : true);

  if (clearTargets) m_levelSelection.selectNone();

  if (clearSelection) {
    m_strokeSelection.selectNone();
    selectedStyles().clear();  // Targets are preserved here
  }

  if (strokeAtPos)
    selectionChanged = m_justSelected = selectStroke(index, toggleStroke);

  if (selectionChanged) {
    m_deformValues.reset();  // Resets selection values shown in the toolbar

    finalizeSelection();
    notifySelectionChanged();

    invalidate();
  }
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::leftButtonDoubleClick(const TPointD &pos,
                                                const TMouseEvent &e) {
  TVectorImageP vi = TImageP(getImage(false));
  if (!vi) return;

  if (m_strokeSelectionType.getIndex() == POLYLINE_SELECTION_IDX &&
      !m_polyline.empty()) {
    closePolyline(pos);
    selectRegionVectorImage(m_includeIntersection.getValue(),
                            m_strokeSelectionType.getIndex());

    m_polyline.reset();
    m_selecting = false;
    invalidate();

    return;
  }

  int strokeIndex;
  if ((strokeIndex = vi->pickGroup(pos)) >= 0) {
    if (vi->canEnterGroup(strokeIndex) && m_canEnterGroup) {
      if (vi->enterGroup(strokeIndex)) {
        clearSelectedStrokes();
        TUndoManager::manager()->add(new UndoEnterGroup(vi, strokeIndex));
      }
    }
  } else if ((strokeIndex = vi->exitGroup()) >= 0)
    TUndoManager::manager()->add(new UndoExitGroup(vi, strokeIndex));

  finalizeSelection();
  invalidate();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::leftButtonDrag(const TPointD &pos,
                                         const TMouseEvent &e) {
  if (m_dragTool) {
    if (!m_strokeSelection.isEditable()) return;

    m_dragTool->leftButtonDrag(pos, e);
    return;
  }

  TVectorImageP vi = TImageP(getImage(false));
  if (!vi) return;

  double pixelSize        = getPixelSize();
  TTool::Application *app = TTool::getApplication();
  if (!app || m_justSelected || !m_selecting ||
      tdistance2(pos, m_curPos) < 9.0 * pixelSize * pixelSize)
    return;

  m_curPos = pos;

  if (m_strokeSelectionType.getIndex() == FREEHAND_SELECTION_IDX) {
    freehandDrag(pos);
    invalidate();
  } else if (m_strokeSelectionType.getIndex() == RECT_SELECTION_IDX) {
    bool selectOverlappingStroke = (m_firstPos.x > pos.x);

    TRectD rect(m_firstPos, pos);
    m_selectingRect = rect;

    if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
      m_polyline.clear();
      m_polyline.push_back(m_selectingRect.getP00());
      m_polyline.push_back(m_selectingRect.getP01());
      m_polyline.push_back(m_selectingRect.getP11());
      m_polyline.push_back(m_selectingRect.getP10());
      m_polyline.push_back(m_selectingRect.getP00());

      if (!m_shiftPressed) clearSelectedStrokes();

      m_stroke = m_polyline.makePolylineStroke(0);
      selectRegionVectorImage(m_includeIntersection.getValue(),
                              POLYLINE_SELECTION_IDX);
      invalidate();
      m_stroke = 0;
      return;
    }

    std::vector<int> oldSelection;
    if (m_shiftPressed) oldSelection = m_strokeSelection.getSelection();

    clearSelectedStrokes();

    QMutexLocker lock(vi->getMutex());

    m_strokeSelection.setImage(vi);  // >_<  Shouldn't be done here...?

    bool selectionChanged = false;

    int s, sCount = vi->getStrokeCount();
    for (s = 0; s != sCount; ++s) {
      if (!vi->isEnteredGroupStroke(s)) continue;

      TStroke *stroke = vi->getStroke(s);

      if (m_strokeSelection.isSelected(s)) continue;

      bool inSelection = selectOverlappingStroke
                             ? rect.overlaps(stroke->getBBox())
                             : rect.contains(stroke->getBBox());

      if (inSelection || (m_shiftPressed &&
                          std::find(oldSelection.begin(), oldSelection.end(),
                                    s) != oldSelection.end()))
        selectionChanged = (selectStroke(s, false) || selectionChanged);
    }

    if (selectionChanged) finalizeSelection();

    invalidate();
  }
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::leftButtonDown(const TPointD &pos,
                                         const TMouseEvent &e) {
  if (getViewer() && getViewer()->getGuidedStrokePickerMode()) {
    getViewer()->doPickGuideStroke(pos);
    return;
  }

  SelectionTool::leftButtonDown(pos, e);
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::leftButtonUp(const TPointD &pos,
                                       const TMouseEvent &e) {
  m_leftButtonMousePressed = false;
  m_shiftPressed           = false;

  if (m_dragTool) {
    if (!m_strokeSelection.isEditable()) {
      delete m_dragTool;
      m_dragTool = 0;
      return;
    }

    m_dragTool->leftButtonUp(pos, e);
    delete m_dragTool;
    m_dragTool = 0;
    invalidate();
    return;
  }

  if (!m_selecting) return;

  // Complete selection
  TVectorImageP vi = TImageP(getImage(false));

  if (vi) {
    if (m_strokeSelectionType.getIndex() == RECT_SELECTION_IDX) {
      m_polyline.reset();

      notifySelectionChanged();
    } else if (m_strokeSelectionType.getIndex() == FREEHAND_SELECTION_IDX) {
      QMutexLocker lock(vi->getMutex());

      closeFreehand(pos);

      if (m_stroke->getControlPointCount() > 3)
        selectRegionVectorImage(m_includeIntersection.getValue(),
                                m_strokeSelectionType.getIndex());

      delete m_stroke;  // >:(
      m_stroke = 0;
      m_track.clear();
    }
  }

  m_selecting    = false;
  m_justSelected = false;
  invalidate();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::addContextMenuItems(QMenu *menu) {
  menu->addAction(CommandManager::instance()->getAction(MI_RemoveEndpoints));
  menu->addSeparator();

  m_strokeSelection.getGroupCommand()->addMenuItems(menu);
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::drawInLevelType(const TVectorImage &vi) {
  glPushMatrix();

  FourPoints bbox = getBBox();
  if (!bbox.isEmpty()) {
    TPixel32 frameColor(127, 127, 127);
    double pixelSize = getPixelSize();

    drawFourPoints(bbox, TPixel32::Black, 0x5555, true);
    drawFourPoints(bbox.enlarge(pixelSize * (-4)), frameColor, 0xffff, true);
    drawFourPoints(bbox.enlarge(pixelSize * (-2)), frameColor, 0x8888, true);

    drawCommandHandle(&vi);
  }

  drawSelectedStrokes(vi);

  if (m_selecting && !m_selectingRect.isEmpty()) drawRectSelection(&vi);

  if (m_strokeSelectionType.getIndex() == POLYLINE_SELECTION_IDX)
    drawPolylineSelection();

  glPopMatrix();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::drawSelectedStrokes(const TVectorImage &vi) {
  glEnable(GL_LINE_STIPPLE);

  double pixelSize = getPixelSize();

  int s, sCount = vi.getStrokeCount();
  for (s = 0; s != sCount; ++s) {
    if (m_strokeSelection.isSelected(s)) {
      TStroke *stroke = vi.getStroke(s);

      glLineStipple(1, 0xF0F0);
      tglColor(TPixel32::Black);
      drawStrokeCenterline(*stroke, pixelSize);

      glLineStipple(1, 0x0F0F);
      tglColor(TPixel32::White);
      drawStrokeCenterline(*stroke, pixelSize);
    }
  }

  glDisable(GL_LINE_STIPPLE);
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::drawGroup(const TVectorImage &vi) {
  int s, sCount = vi.getStrokeCount();
  for (s = 0; s != sCount; ++s) {
    if (m_strokeSelection.isSelected(s)) {
      TRectD gBox;
      if (getGroupBBox(vi, s, gBox)) drawRect(gBox, TPixel::Blue, 0xffff);
    }
  }
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::draw() {
  int devPixRatio  = m_viewer->getDevPixRatio();
  TVectorImageP vi = TImageP(getImage(false));
  if (!vi) return;

  glLineWidth(1.0 * devPixRatio);

  if (isLevelType() || isSelectedFramesType()) {
    drawInLevelType(*vi);
    return;
  }

  glPushMatrix();

  if (m_strokeSelection.isEmpty())  // o_o  WTF!?
    m_bboxs.clear();                //

  // common draw
  if (getBBoxsCount() > 0) drawCommandHandle(vi.getPointer());

  if (m_selecting && !m_selectingRect.isEmpty())
    drawRectSelection(vi.getPointer());

  TRectD bbox = vi->getBBox();
  TPixel32 frameColor(140, 140, 140);
  tglColor(frameColor);
  drawRect(bbox, frameColor, 0x5555, true);

  drawSelectedStrokes(*vi);

  if (m_strokeSelectionType.getIndex() == POLYLINE_SELECTION_IDX)
    drawPolylineSelection();
  else if (m_strokeSelectionType.getIndex() == FREEHAND_SELECTION_IDX)
    drawFreehandSelection();

  if (m_levelSelection.isEmpty()) drawGroup(*vi);

  glPopMatrix();
}

//-----------------------------------------------------------------------------

TSelection *VectorSelectionTool::getSelection() {
  TImage *image    = getImage(false);
  TVectorImageP vi = TImageP(image);
  if (!vi) return 0;

  return &m_strokeSelection;
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::isSelectionEmpty() {
  TVectorImageP vi = TImageP(getImage(
      false));           // We want at least current image to be visible.
  if (!vi)              // This is somewhat in line to preventing tool actions
    return true;        // on non-visible levels.

  return m_strokeSelection
      .isEmpty();  // Same here, something should be visibly selected.
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::computeBBox() {
  m_bboxs.clear();
  if (canResetCenter()) m_centers.clear();

  TVectorImageP vi = TImageP(getImage(false));
  if (!vi) return;

  if (isLevelType() || isSelectedFramesType()) {
    double maxThickness = 0;

    // Calculate current image's bbox - it is always the first one (index 0)
    if (vi && (isLevelType() || m_selectedFrames.count(getCurrentFid()) > 0)) {
      FourPoints bbox =
          getFourPointsFromVectorImage(vi, selectedStyles(), maxThickness);

      m_bboxs.push_back(bbox);
      m_centers.push_back((bbox.getP00() + bbox.getP11()) * 0.5);
    }

    // All subsequent SELECTED frames' bboxes come sequentially starting from 1
    if (TXshSimpleLevel *level =
            TTool::getApplication()->getCurrentLevel()->getSimpleLevel()) {
      std::vector<TFrameId> fids;
      level->getFids(fids);

      for (int i = 0; i < (int)fids.size(); ++i) {
        if (getCurrentFid() == fids[i] ||
            (isSelectedFramesType() && m_selectedFrames.count(fids[i]) == 0))
          continue;

        TVectorImageP vi = level->getFrame(fids[i], false);
        if (!vi) continue;

        FourPoints p =
            getFourPointsFromVectorImage(vi, selectedStyles(), maxThickness);

        m_bboxs.push_back(p);
        m_centers.push_back(0.5 * (p.getP00() + p.getP11()));
        m_deformValues.m_maxSelectionThickness = maxThickness;
      }
    }
  } else if (vi) {
    TRectD newBbox;
    double maxThickness = 0;

    for (int i = 0; i < (int)vi->getStrokeCount(); i++) {
      TStroke *stroke = vi->getStroke(i);

      if (m_strokeSelection.isSelected(i)) {
        newBbox += stroke->getBBox();

        for (int j = 0; j < stroke->getControlPointCount(); j++) {
          double thick = stroke->getControlPoint(j).thick;
          if (maxThickness < thick) maxThickness = thick;
        }
      }
    }

    m_deformValues.m_maxSelectionThickness = maxThickness;
    FourPoints bbox;
    bbox = newBbox;
    m_bboxs.push_back(bbox);
    if (canResetCenter())
      m_centers.push_back(0.5 * (bbox.getP11() + bbox.getP00()));
  }

  ++m_selectionCount;
}

void VectorSelectionTool::notifyImageChanged() {
  TXshSimpleLevel* level;
  std::set<TFrameId> frames;

  switch (m_levelSelection.framesMode()) {
  case LevelSelection::FramesMode::FRAMES_NONE:
  case LevelSelection::FramesMode::FRAMES_CURRENT:
    TTool::notifyImageChanged(getCurrentFid());
    break;
  case LevelSelection::FramesMode::FRAMES_SELECTED:
    frames = getSelectedFrames();
    for (TFrameId fid : frames) {
      TTool::notifyImageChanged(fid);
    }
    break;
  case LevelSelection::FramesMode::FRAMES_ALL:
    level     = TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    int count = level->getFrameCount();
    for (int i=0; i < count;i++) {
      TTool::notifyImageChanged(level->getFrameId(i));
    }
    break;
  }
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::selectStroke(int index, bool toggle) {
  TVectorImageP vi = TImageP(getImage(false));
  assert(vi);
  assert(m_strokeSelection.getImage() == vi);

  if (!vi->isEnteredGroupStroke(index))  // If index is not in current group
    return false;

  if (index < 0 || index >= int(vi->getStrokeCount()))  // Should be asserted...
    return false;

  bool wasSelected = m_strokeSelection.isSelected(index),
       selectState = !(wasSelected && toggle);

  // (De)Select additional strokes related to ours
  std::set<int> &selectedStyles = this->selectedStyles();

  if (isSameStyleType())  // Color selection
  {
    TStroke *refStroke = vi->getStroke(index);
    assert(refStroke);

    int style = refStroke->getStyle();

    if (selectState)
      selectedStyles.insert(style);
    else
      selectedStyles.erase(style);
  } else if (vi->isStrokeGrouped(index) &&
             vi->selectable(index))  // Group selection
  {
    int s, sCount = vi->getStrokeCount();
    for (s = 0; s != sCount; ++s) {
      if (vi->selectable(s) && vi->sameSubGroup(index, s))
        m_strokeSelection.select(s, selectState);
    }
  } else  // Single stroke selection
    m_strokeSelection.select(index, selectState);

  return (selectState != wasSelected);
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::onActivate() {
  if (m_firstTime) {
    m_includeIntersection.setValue(l_strokeSelectIncludeIntersection ? 1 : 0);
    m_constantThickness.setValue(l_strokeSelectConstantThickness ? 1 : 0);
    m_strokeSelection.setSceneHandle(
        TTool::getApplication()->getCurrentScene());
    m_selectionTarget.setValue(
        ::to_wstring(l_strokeSelectionTarget.getValue()));
    m_strokeSelectionType.setValue(
        ::to_wstring(l_strokeSelectionType.getValue()));
  }

  SelectionTool::onActivate();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::onDeactivate() {
  if (isLevelType()) return;

  SelectionTool::onDeactivate();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::doOnActivate() {
  TVectorImageP vi = TImageP(getImage(false));
  m_strokeSelection.setImage(vi);

  updateSelectionTarget();

  finalizeSelection();
  invalidate();
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::onImageChanged() {
  TVectorImageP vi          = TImageP(getImage(false));
  TVectorImageP selectedImg = m_strokeSelection.getImage();

  if (vi != selectedImg) {
    m_strokeSelection.selectNone();
    m_strokeSelection.setImage(vi);

    if (!(vi && selectedImg)  // Retain the styles selection ONLY
        || vi->getPalette() !=
               selectedImg->getPalette())  // if palettes still match
      selectedStyles().clear();
  } else {
    // Remove any eventual stroke index outside the valid range
    if (!m_strokeSelection.isEmpty()) {
      const std::vector<int> indexes = m_strokeSelection.getSelection();
      int strokesCount             = selectedImg->getStrokeCount();

      std::vector<int>::const_iterator it;
      for (it = indexes.begin(); it != indexes.end(); ++it) {
        int index = *it;

        if (index >= strokesCount) m_strokeSelection.select(index, false);
      }
    }
  }

  finalizeSelection();
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::isDragging() const {
  return m_enabled && m_leftButtonMousePressed;
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::doOnDeactivate() {
  m_strokeSelection.selectNone();
  m_levelSelection.selectNone();
  m_deformValues.reset();

  m_polyline.clear();

  TTool::getApplication()->getCurrentSelection()->setSelection(0);

  invalidate();
}

//-----------------------------------------------------------------------------

TPropertyGroup *VectorSelectionTool::getProperties(int idx) {
  switch (idx) {
  case 0:
    return &m_prop;
  case 1:
    return &m_outlineProps;
  default:
    return 0;
  };
}

//-----------------------------------------------------------------------------

bool VectorSelectionTool::onPropertyChanged(std::string propertyName) {
  if (!m_strokeSelection.isEditable()) return false;

  if (propertyName == m_strokeSelectionType.getName())
    l_strokeSelectionType = ::to_string(m_strokeSelectionType.getValue());

  if (SelectionTool::onPropertyChanged(propertyName)) return true;

  if (propertyName == m_includeIntersection.getName())
    l_strokeSelectIncludeIntersection = (int)(m_includeIntersection.getValue());
  else if (propertyName == m_constantThickness.getName())
    l_strokeSelectConstantThickness = (int)(m_constantThickness.getValue());
  else if (propertyName == m_selectionTarget.getName()) {
    l_strokeSelectionTarget = ::to_string(m_selectionTarget.getValue());
    doOnActivate();
  } else if (propertyName == m_capStyle.getName()) {
    if (m_strokeSelection.isEmpty()) return true;

    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    UndoChangeOutlineStyle *undo =
        new UndoChangeOutlineStyle(level, getCurrentFid(), this);

    int newCapStyle = m_capStyle.getIndex();

    TVectorImageP vi = m_strokeSelection.getImage();

    const std::vector<int> &indices = m_strokeSelection.getSelection();

    std::vector<int>::const_iterator it;
    for (it = indices.begin(); it != indices.end(); ++it) {
      TStroke *stroke = vi->getStroke(*it);

      stroke->outlineOptions().m_capStyle =
          (TStroke::OutlineOptions::CapStyle)newCapStyle;
      stroke->invalidate();
    }

    computeBBox();
    invalidate();

    level->setDirtyFlag(true);

    undo->registerStrokes();
    TUndoManager::manager()->add(undo);

    notifyImageChanged();
  } else if (propertyName == m_joinStyle.getName()) {
    if (m_strokeSelection.isEmpty()) return true;

    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    UndoChangeOutlineStyle *undo =
        new UndoChangeOutlineStyle(level, getCurrentFid(), this);

    int newJoinStyle = m_joinStyle.getIndex();

    TVectorImageP vi = m_strokeSelection.getImage();

    const std::vector<int> &indices = m_strokeSelection.getSelection();

    std::vector<int>::const_iterator it;
    for (it = indices.begin(); it != indices.end(); ++it) {
      TStroke *stroke = vi->getStroke(*it);

      stroke->outlineOptions().m_joinStyle =
          (TStroke::OutlineOptions::JoinStyle)newJoinStyle;
      stroke->invalidate();
    }

    computeBBox();
    invalidate();

    level->setDirtyFlag(true);

    undo->registerStrokes();
    TUndoManager::manager()->add(undo);

    notifyImageChanged();
  } else if (propertyName == m_miterJoinLimit.getName()) {
    if (m_strokeSelection.isEmpty()) return true;

    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    UndoChangeOutlineStyle *undo =
        new UndoChangeOutlineStyle(level, getCurrentFid(), this);

    int upper = m_miterJoinLimit.getValue();

    TVectorImageP vi = m_strokeSelection.getImage();

    const std::vector<int> &indices = m_strokeSelection.getSelection();

    std::vector<int>::const_iterator it;
    for (it = indices.begin(); it != indices.end(); ++it) {
      TStroke *stroke = vi->getStroke(*it);

      stroke->outlineOptions().m_miterUpper = upper;
      stroke->invalidate();
    }

    computeBBox();
    invalidate();

    level->setDirtyFlag(true);

    undo->registerStrokes();
    TUndoManager::manager()->add(undo);

    notifyImageChanged();
  } else
    return false;

  return true;
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::selectionOutlineStyle(int &capStyle, int &joinStyle) {
  // Check the selection's outline style group validity
  const std::vector<int> &selection = m_strokeSelection.getSelection();
  if (selection.empty()) {
    capStyle = joinStyle = -1;
    return;
  }

  TVectorImageP vi = m_strokeSelection.getImage();
  const TStroke::OutlineOptions &beginOptions =
      vi->getStroke(*selection.begin())->outlineOptions();

  capStyle  = beginOptions.m_capStyle;
  joinStyle = beginOptions.m_joinStyle;

  std::vector<int>::const_iterator it;
  for (it = selection.begin(); it != selection.end(); ++it) {
    const TStroke::OutlineOptions &options =
        vi->getStroke(*it)->outlineOptions();

    if (capStyle != options.m_capStyle) capStyle = -1;
    if (joinStyle != options.m_joinStyle) joinStyle = -1;
    if (capStyle < 0 && joinStyle < 0) return;
  }
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::selectRegionVectorImage(bool includeIntersect,
                                                  int selectionIndex) {
  if (!m_stroke) return;

  TVectorImageP vi(getImage(false));
  if (!vi) return;

  m_strokeSelection.setImage(vi);

  TVectorImage selectImg;
  selectImg.addStroke(new TStroke(*m_stroke));

  std::vector<TStroke *> symmStrokes;
  if (selectionIndex == POLYLINE_SELECTION_IDX) {
    if (m_polyline.size() > 1 && m_polyline.hasSymmetryBrushes()) {
      for (int i = 1; i < m_polyline.getBrushCount(); i++)
        symmStrokes.push_back(m_polyline.makePolylineStroke(i));
    }
  } else if (selectionIndex == FREEHAND_SELECTION_IDX) {
    if (m_track.hasSymmetryBrushes()) {
      double pixelSize = getPixelSize();
      double error     = (30.0 / 11) * pixelSize;
      symmStrokes      = m_track.makeSymmetryStrokes(error);
    }
  }
  for (int i = 0; i < symmStrokes.size(); i++)
    selectImg.addStroke(symmStrokes[i]);

  selectImg.findRegions();

  int sCount = int(vi->getStrokeCount()),
      rCount = int(selectImg.getRegionCount());

  bool selectionChanged = false;

  for (int s = 0; s != sCount; ++s) {
    TStroke *currentStroke = vi->getStroke(s);

    for (int r = 0; r != rCount; ++r) {
      TRegion *region = selectImg.getRegion(r);

      if (region->contains(*currentStroke, true))
        selectionChanged = selectStroke(s, false) || selectionChanged;
    }

    if (includeIntersect) {
      std::vector<DoublePair> intersections;
      intersect(m_stroke, currentStroke, intersections, false);
      if (intersections.size() > 0) {
        selectionChanged = selectStroke(s, false) || selectionChanged;
      }
      for (int i = 0; i < symmStrokes.size(); i++) {
        intersections.clear();
        intersect(symmStrokes[i], currentStroke, intersections, false);
        if (intersections.size() > 0) {
          selectionChanged = selectStroke(s, false) || selectionChanged;
        }
      }

    }
  }

  if (selectionChanged) {
    finalizeSelection();
    notifySelectionChanged();
    invalidate();
  }
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::updateAction(TPointD pos, const TMouseEvent &e) {
  TImageP image    = getImage(false);
  TVectorImageP vi = (TVectorImageP)image;
  if (!vi) return;

  SelectionTool::updateAction(pos, e);
  if (m_what != Outside || m_cursorId != ToolCursor::StrokeSelectCursor) return;

  if (!m_strokeSelection.isEditable()) return;

  FourPoints bbox = getBBox();
  UINT index      = 0;

  if ((isLevelType() &&
       bbox.contains(pos))  // What about isSelectedFramesType()??
      || (getStrokeIndexFromPos(index, vi, pos, getPixelSize(),
                                getViewer()->getViewMatrix()) &&
          m_strokeSelection.isSelected(index))) {
    m_what = Inside;
    m_cursorId =
        isLevelType() ? ToolCursor::LevelSelectCursor : ToolCursor::MoveCursor;
  }
}

//-----------------------------------------------------------------------------

void VectorSelectionTool::onSelectedFramesChanged() {
  if (isSelectedFramesType())  // False also in case m_levelSelection is not
                               // current
    finalizeSelection();
}
