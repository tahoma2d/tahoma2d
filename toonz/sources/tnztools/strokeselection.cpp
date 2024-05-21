

#include "tools/strokeselection.h"

#include "../../toonz/alignmentpane.h"

// TnzTools includes
#include "tools/imagegrouping.h"
#include "tools/toolhandle.h"
#include "tools/tool.h"
#include "tools/toolutils.h"

// TnzQt includes
#include "toonzqt/selectioncommandids.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/strokesdata.h"
#include "toonzqt/rasterimagedata.h"
#include "toonzqt/dvdialog.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/tobjecthandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tcamera.h"

#include "toonz/tcenterlinevectorizer.h"
#include "toonz/stage.h"
#include "toonz/tstageobject.h"
#include "toonz/toonzscene.h"
#include "toonz/sceneproperties.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobject.h"
#include "toonz/tcolumnhandle.h"

// TnzCore includes
#include "tthreadmessage.h"
#include "tundo.h"
#include "tstroke.h"
#include "tvectorimage.h"
#include "tcolorstyles.h"
#include "tpalette.h"

#include "toonz/tstageobject.h"

// Qt includes
#include <QApplication>
#include <QClipboard>

//=============================================================================
namespace {

bool getGroupBBox(TVectorImageP vi, int strokeIndex, TRectD &gBox) {
  if (!vi->isStrokeGrouped(strokeIndex)) return false;

  gBox             = vi->getStroke(strokeIndex)->getBBox();
  bool boxExpanded = false;

  int groupDepth       = vi->isInsideGroup();
  int strokeGroupDepth = vi->getGroupDepth(strokeIndex);
  if (vi->getGroupDepth(strokeIndex) == groupDepth) return false;
  int s, sCount = int(vi->getStrokeCount());
  for (s = 0; s != sCount; ++s) {
    int sGroupDepth = vi->getGroupDepth(s);
    if ((vi->sameGroup(s, strokeIndex) ||
         vi->sameParentGroupStrokes(s, strokeIndex)) &&
        sGroupDepth > groupDepth) {
      TRectD sBox = vi->getStroke(s)->getBBox();
      gBox += vi->getStroke(s)->getBBox();
    }
  }
  return true;
}

void vectorizeToonzImageData(const TVectorImageP &image,
                             const ToonzImageData *tiData,
                             std::set<int> &indexes, TPalette *palette,
                             const VectorizerConfiguration &config) {
  if (!tiData) return;
  QApplication::setOverrideCursor(Qt::WaitCursor);
  TRasterP ras;
  std::vector<TRectD> rects;
  std::vector<TStroke> strokes;
  std::vector<TStroke> originalStrokes;
  TAffine affine;
  double dpiX, dpiY;
  tiData->getData(ras, dpiX, dpiY, rects, strokes, originalStrokes, affine,
                  image->getPalette());
  TRasterCM32P rasCM = ras;
  TToonzImageP ti(rasCM, rasCM->getBounds());
  VectorizerCore vc;
  TVectorImageP vi = vc.vectorize(ti, config, palette);
  assert(vi);
  vi->setPalette(palette);

  TScale sc(dpiX / Stage::inch, dpiY / Stage::inch);
  int i;
  TRectD selectionBounds;
  for (i = 0; i < (int)rects.size(); i++) selectionBounds += rects[i];
  for (i = 0; i < (int)strokes.size(); i++)
    selectionBounds += strokes[i].getBBox();
  TTranslation tr(selectionBounds.getP00());

  for (i = 0; i < (int)vi->getStrokeCount(); i++) {
    TStroke *stroke = vi->getStroke(i);
    stroke->transform(sc.inv() * affine * tr, true);
  }

  UINT oldImageSize = image->getStrokeCount();
  image->mergeImage(vi, TAffine());
  UINT newImageSize = image->getStrokeCount();
  indexes.clear();
  for (UINT sI = oldImageSize; sI < newImageSize; sI++) indexes.insert(sI);
  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------

void copyStrokesWithoutUndo(TVectorImageP image, std::set<int> &indexes) {
  QClipboard *clipboard = QApplication::clipboard();
  StrokesData *data     = new StrokesData();
  data->setImage(image, indexes);
  clipboard->setMimeData(data, QClipboard::Clipboard);
}

//-----------------------------------------------------------------------------

bool pasteStrokesWithoutUndo(TVectorImageP image, std::set<int> &outIndexes,
                             TSceneHandle *sceneHandle, bool insert = true) {
  QMutexLocker lock(image->getMutex());
  QClipboard *clipboard = QApplication::clipboard();
  const StrokesData *stData =
      dynamic_cast<const StrokesData *>(clipboard->mimeData());
  const ToonzImageData *tiData =
      dynamic_cast<const ToonzImageData *>(clipboard->mimeData());
  const FullColorImageData *fciData =
      dynamic_cast<const FullColorImageData *>(clipboard->mimeData());
  std::set<int> indexes = outIndexes;
  if (stData)
    stData->getImage(image, indexes, insert);
  else if (tiData) {
    ToonzScene *scene = sceneHandle->getScene();
    assert(scene);

    const VectorizerParameters *vParams =
        scene->getProperties()->getVectorizerParameters();
    assert(vParams);

    std::unique_ptr<VectorizerConfiguration> config(
        vParams->getCurrentConfiguration(0.0));
    vectorizeToonzImageData(image, tiData, indexes, image->getPalette(),
                            *config);
  } else if (fciData) {
    DVGui::error(QObject::tr(
        "The copied selection cannot be pasted in the current drawing."));
    return false;
  } else
    return false;

  StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  if (selection) selection->notifyView();
  outIndexes = indexes;  // outIndexes is a reference  to current  selection, so
                         // the notifyImageChanged could reset it!
  return true;
}

//-----------------------------------------------------------------------------

void deleteStrokesWithoutUndo(TVectorImageP image, std::set<int> &indexes) {
  QMutexLocker lock(image->getMutex());
  std::vector<int> indexesV(indexes.begin(), indexes.end());
  TRectD bbox;
  UINT i = 0;
  for (; i < indexesV.size(); i++)
    bbox += image->getStroke(indexesV[i])->getBBox();

  std::vector<TFilledRegionInf> regions;
  ImageUtils::getFillingInformationOverlappingArea(image, regions, bbox);

  TVectorImageP other = image->splitImage(indexesV, true);

  indexes.clear();

  TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  if (selection) selection->notifyView();
}

//-----------------------------------------------------------------------------

void cutStrokesWithoutUndo(TVectorImageP image, std::set<int> &indexes) {
  copyStrokesWithoutUndo(image, indexes);
  deleteStrokesWithoutUndo(image, indexes);
}

//=============================================================================
// CopyStrokesUndo
//-----------------------------------------------------------------------------

class CopyStrokesUndo final : public TUndo {
  QMimeData *m_oldData;
  QMimeData *m_newData;

public:
  CopyStrokesUndo(QMimeData *oldData, QMimeData *newData)
      : m_oldData(oldData), m_newData(newData) {}

  void undo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_oldData), QClipboard::Clipboard);
  }

  void redo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setMimeData(cloneData(m_newData), QClipboard::Clipboard);
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// PasteStrokesUndo
//-----------------------------------------------------------------------------

class PasteStrokesUndo final : public ToolUtils::TToolUndo {
  std::set<int> m_indexes;
  QMimeData *m_oldData;
  TSceneHandle *m_sceneHandle;

public:
  PasteStrokesUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                   std::set<int> &indexes, TPaletteP oldPalette,
                   TSceneHandle *sceneHandle, bool createdFrame,
                   bool createdLevel)
      : TToolUndo(level, frameId, createdFrame, createdLevel, oldPalette)
      , m_indexes(indexes)
      , m_sceneHandle(sceneHandle) {
    QClipboard *clipboard = QApplication::clipboard();
    m_oldData             = cloneData(clipboard->mimeData());
  }

  ~PasteStrokesUndo() { delete m_oldData; }

  void undo() const override {
    TVectorImageP image = m_level->getFrame(m_frameId, true);

    // Se la selezione corrente e' la stroke selection devo svuotarla,
    // altrimenti puo' rimanere selezionato uno stroke che non esiste piu'.
    StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
        TTool::getApplication()->getCurrentSelection()->getSelection());
    if (selection) selection->selectNone();

    std::set<int> indexes = m_indexes;
    deleteStrokesWithoutUndo(image, indexes);

    removeLevelAndFrameIfNeeded();

    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  void redo() const override {
    insertLevelAndFrameIfNeeded();

    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    std::set<int> indexes = m_indexes;

    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *data       = cloneData(clipboard->mimeData());

    clipboard->setMimeData(cloneData(m_oldData), QClipboard::Clipboard);

    pasteStrokesWithoutUndo(image, indexes, m_sceneHandle);
    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();

    clipboard->setMimeData(data, QClipboard::Clipboard);
    TTool::getApplication()
        ->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteChanged();
    TTool::getApplication()->getCurrentXsheet()->notifyXsheetChanged();
    notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }
};

//--------------------------------------------------------------------

class RemoveEndpointsUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  TFrameId m_frameId;
  std::vector<UndoVectorData> m_strokes;

public:
  RemoveEndpointsUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                      std::vector<UndoVectorData> strokes)
      : m_level(level)
      , m_frameId(frameId)
      , m_strokes(strokes)

  {}

  ~RemoveEndpointsUndo() {
    int i;
    for (i = 0; i < (int)m_strokes.size(); i++) delete m_strokes[i].oldStroke;
  }

  void undo() const override {
    TVectorImageP vi = m_level->getFrame(m_frameId, true);
    int i;
    for (i = 0; i < (int)m_strokes.size(); i++) {
      TStroke *newS = new TStroke(*(m_strokes[i].oldStroke));
      newS->setId(m_strokes[i].oldStroke->getId());
      vi->restoreEndpoints(m_strokes[i].index, newS, m_strokes[i].offset);
    }
    StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
        TTool::getApplication()->getCurrentSelection()->getSelection());
    if (selection) selection->selectNone();

    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  void redo() const override {
    int i;
    TVectorImageP vi = m_level->getFrame(m_frameId, true);
    for (i = 0; i < (int)m_strokes.size(); i++) {
      TStroke *s = vi->removeEndpoints(m_strokes[i].index);
      delete s;
      // assert(s==m_strokes[i].second);
    }
    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// AlignStrokesUndo
//-----------------------------------------------------------------------------

class AlignStrokesUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  TFrameId m_frameId;
  std::vector<std::pair<int, TStroke *>> m_strokes;
  ALIGN_TYPE m_alignType;
  int m_alignMethod;
  double m_alignX, m_alignY;

public:
  AlignStrokesUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                   std::vector<std::pair<int, TStroke *>> strokes,
                   ALIGN_TYPE alignType, int alignMethod, double alignX,
                   double alignY)
      : m_level(level)
      , m_frameId(frameId)
      , m_strokes(strokes)
      , m_alignType(alignType)
      , m_alignMethod(alignMethod)
      , m_alignX(alignX)
      , m_alignY(alignY) {}

  ~AlignStrokesUndo() {
    int i;
    for (i = 0; i < (int)m_strokes.size(); i++) delete m_strokes[i].second;
  }

  void undo() const override {
    TVectorImageP vi = m_level->getFrame(m_frameId, true);
    int i;
    for (i = 0; i < (int)m_strokes.size(); i++) {
      TStroke *stroke     = vi->getStroke(m_strokes[i].first);
      TStroke *origStroke = m_strokes[i].second;
      for (int j = 0, count = stroke->getControlPointCount(); j < count; ++j) {
        stroke->setControlPoint(j, origStroke->getControlPoint(j));
      }
    }

    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  void redo() const override {
    int i;
    TVectorImageP vi = m_level->getFrame(m_frameId, true);
    std::vector<std::pair<TStroke *, TPointD>> newPts;

    int strokeCount = m_alignMethod == ALIGN_METHOD::CAMERA_AREA ? -1 : 0;
    double shift;
    std::set<int> groups;
    int groupDepth = vi->isInsideGroup();
    for (i = 0; i < (int)m_strokes.size(); i++) {
      TStroke *stroke      = vi->getStroke(m_strokes[i].first);
      int strokeGroupDepth = vi->getGroupDepth(m_strokes[i].first);
      TRectD bbox          = stroke->getBBox();
      TRectD gBBox;

      if (getGroupBBox(vi, m_strokes[i].first, gBBox)) bbox = gBBox;

      double newX, newY;
      if (m_alignType == ALIGN_TYPE::ALIGN_LEFT)
        newX = m_alignX - bbox.x0;
      else if (m_alignType == ALIGN_TYPE::ALIGN_RIGHT)
        newX = m_alignX - bbox.x1;
      else if (m_alignType == ALIGN_TYPE::ALIGN_TOP)
        newY = m_alignY - bbox.y1;
      else if (m_alignType == ALIGN_TYPE::ALIGN_BOTTOM)
        newY = m_alignY - bbox.y0;
      else if (m_alignType == ALIGN_TYPE::ALIGN_CENTER_H)
        newY = (m_alignY - bbox.y0) - (bbox.getLy() / 2.0);
      else if (m_alignType == ALIGN_TYPE::ALIGN_CENTER_V)
        newX = (m_alignX - bbox.x0) - (bbox.getLx() / 2.0);
      else if (m_alignType == ALIGN_TYPE::DISTRIBUTE_H) {
        // m_alignX = 1st X point. m_alignY = X spacing between points
        int groupId = vi->getGroupByStroke(m_strokes[i].first);
        if (groupId < 0 || strokeGroupDepth == groupDepth)
          strokeCount++;
        else if (groups.find(groupId) == groups.end()) {
          groups.insert(groupId);
          strokeCount++;
        }
        newX = m_alignX - bbox.x0 + (m_alignY * (double)strokeCount) -
               (bbox.getLx() / 2.0);
      } else if (m_alignType == ALIGN_TYPE::DISTRIBUTE_V) {
        // m_alignX = 1st Y point. m_alignY = Y spacing between points
        int groupId = vi->getGroupByStroke(m_strokes[i].first);
        if (groupId < 0 || strokeGroupDepth == groupDepth)
          strokeCount++;
        else if (groups.find(groupId) == groups.end()) {
          groups.insert(groupId);
          strokeCount++;
        }
        newY = m_alignX - bbox.y0 + (m_alignY * (double)strokeCount) -
               (bbox.getLy() / 2.0);
      }

      newPts.push_back(std::make_pair(stroke, TPointD(newX, newY)));
    }

    for (i = 0; i < (int)newPts.size(); i++) {
      TStroke *stroke = newPts[i].first;
      for (int j = 0, count = stroke->getControlPointCount(); j < count; ++j) {
        TThickPoint p = stroke->getControlPoint(j);
        p             = TThickPoint(p + newPts[i].second, p.thick);
        stroke->setControlPoint(j, p);
      }
    }
    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// DeleteFramesUndo
//-----------------------------------------------------------------------------

class DeleteStrokesUndo : public TUndo {
protected:
  TXshSimpleLevelP m_level;
  TFrameId m_frameId;
  std::set<int> m_indexes;
  QMimeData *m_data;
  TSceneHandle *m_sceneHandle;

public:
  DeleteStrokesUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                    std::set<int> indexes, QMimeData *data,
                    TSceneHandle *sceneHandle)
      : m_level(level)
      , m_frameId(frameId)
      , m_indexes(indexes)
      , m_data(data)
      , m_sceneHandle(sceneHandle) {}

  ~DeleteStrokesUndo() { delete m_data; }

  void undo() const override {
    QClipboard *clipboard = QApplication::clipboard();
    QMimeData *oldData    = cloneData(clipboard->mimeData());

    clipboard->setMimeData(cloneData(m_data), QClipboard::Clipboard);
    std::set<int> indexes = m_indexes;
    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    pasteStrokesWithoutUndo(image, indexes, m_sceneHandle, false);
    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();

    clipboard->setMimeData(oldData, QClipboard::Clipboard);
  }

  void redo() const override {
    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    std::set<int> indexes = m_indexes;
    deleteStrokesWithoutUndo(image, indexes);
  }

  int getSize() const override { return sizeof(*this); }
};

//=============================================================================
// CutStrokesUndo
//-----------------------------------------------------------------------------

class CutStrokesUndo final : public DeleteStrokesUndo {
public:
  CutStrokesUndo(TXshSimpleLevel *level, const TFrameId &frameId,
                 std::set<int> indexes, QMimeData *data,
                 TSceneHandle *sceneHandle)
      : DeleteStrokesUndo(level, frameId, indexes, data, sceneHandle) {}

  ~CutStrokesUndo() {}

  void redo() const override {
    TVectorImageP image   = m_level->getFrame(m_frameId, true);
    std::set<int> indexes = m_indexes;
    cutStrokesWithoutUndo(image, indexes);
  }
};

}  // namespace

//=============================================================================
//
// StrokeSelection ctor/dtor
//
//-----------------------------------------------------------------------------

StrokeSelection::StrokeSelection()
    : m_groupCommand(new TGroupCommand())
    , m_sceneHandle()
    , m_updateSelectionBBox(false) {
  m_groupCommand->setSelection(this);
}

//-----------------------------------------------------------------------------

StrokeSelection::~StrokeSelection() {}

//-----------------------------------------------------------------------------

StrokeSelection::StrokeSelection(const StrokeSelection &other)
    : m_vi(other.m_vi)
    , m_indexes(other.m_indexes)
    , m_groupCommand(new TGroupCommand())
    , m_sceneHandle(other.m_sceneHandle)
    , m_updateSelectionBBox(other.m_updateSelectionBBox) {
  m_groupCommand->setSelection(this);
}

//-----------------------------------------------------------------------------

StrokeSelection &StrokeSelection::operator=(const StrokeSelection &other) {
  m_vi                  = other.m_vi;
  m_indexes             = other.m_indexes;
  m_sceneHandle         = other.m_sceneHandle;
  m_updateSelectionBBox = other.m_updateSelectionBBox;

  return *this;
}

//-----------------------------------------------------------------------------

void StrokeSelection::select(int index, bool on) {
  std::vector<int>::iterator it = std::find(m_indexes.begin(), m_indexes.end(), index);
  if (on) {
    if (it == m_indexes.end()) m_indexes.push_back(index);
  } else {
    if (it != m_indexes.end()) m_indexes.erase(it);
  }
}

//-----------------------------------------------------------------------------

void StrokeSelection::toggle(int index) {
  std::vector<int>::iterator it = std::find(m_indexes.begin(), m_indexes.end(), index);
  if (it == m_indexes.end())
    m_indexes.push_back(index);
  else
    m_indexes.erase(it);
}

//=============================================================================
//
// removeEndpoints
//
//-----------------------------------------------------------------------------

void StrokeSelection::removeEndpoints() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;

  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be updated. It is not editable."));
    return;
  }

  std::vector<UndoVectorData> undoData;

  m_vi->findRegions();
  for (auto const &e : m_indexes) {
    double offset;
    TStroke *s = m_vi->removeEndpoints(e, &offset);
    UndoVectorData undoStroke;

    undoStroke.index   = e;
    undoStroke.oldStroke = s;
    undoStroke.offset    = offset;

    if (s) undoData.push_back(undoStroke);
  }
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  TXshSimpleLevel *level =
      TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
  if (!undoData.empty())
    TUndoManager::manager()->add(
        new RemoveEndpointsUndo(level, tool->getCurrentFid(), undoData));

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//=============================================================================
//
// selectAll
//
//-----------------------------------------------------------------------------

void StrokeSelection::selectAll() {
  if (!m_vi) return;

  int sCount = int(m_vi->getStrokeCount());

  for (int s = 0; s < sCount; ++s) {
    if (!m_vi->isEnteredGroupStroke(s)) continue;
    m_indexes.push_back(s);
  }

  StrokeSelection *selection = dynamic_cast<StrokeSelection *>(
      TTool::getApplication()->getCurrentSelection()->getSelection());
  if (selection) selection->notifyView();
}

//=============================================================================
//
// deleteStrokes
//
//-----------------------------------------------------------------------------

void StrokeSelection::deleteStrokes() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be deleted. It is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  TUndo *undo;
  if (isSpline)
    undo = new ToolUtils::UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());

  StrokesData *data = new StrokesData();
  std::set<int> indexSet(m_indexes.begin(), m_indexes.end());
  data->setImage(m_vi, indexSet);
  std::set<int> oldIndexes = indexSet;
  deleteStrokesWithoutUndo(m_vi, indexSet);
  m_indexes.clear();

  if (!isSpline) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(new DeleteStrokesUndo(
        level, tool->getCurrentFid(), oldIndexes, data, m_sceneHandle));
  } else {
    assert(undo);
    if (undo) TUndoManager::manager()->add(undo);
  }
}

//=============================================================================
//
// StrokeSelection::copy()
//
//-----------------------------------------------------------------------------

void StrokeSelection::copy() {
  if (m_indexes.empty()) return;
  QClipboard *clipboard = QApplication::clipboard();
  QMimeData *oldData    = cloneData(clipboard->mimeData());
  std::set<int> indexSet(m_indexes.begin(), m_indexes.end());
  copyStrokesWithoutUndo(m_vi, indexSet);
  QMimeData *newData = cloneData(clipboard->mimeData());

  // TUndoManager::manager()->add(new CopyStrokesUndo(oldData, newData));
}

//=============================================================================
//
// StrokeSelection::paste()
//
//-----------------------------------------------------------------------------

void StrokeSelection::paste() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be pasted. It is not editable."));
    return;
  }

  if (TTool::getApplication()->getCurrentObject()->isSpline()) {
    const StrokesData *stData = dynamic_cast<const StrokesData *>(
        QApplication::clipboard()->mimeData());
    if (!stData) return;
    TVectorImageP splineImg = TImageP(tool->getImage(true));
    TVectorImageP img       = stData->m_image;
    if (!splineImg || !img) return;

    QMutexLocker lock(splineImg->getMutex());
    TUndo *undo = new ToolUtils::UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());
    while (splineImg->getStrokeCount() > 0) splineImg->deleteStroke(0);

    TStroke *stroke = img->getStroke(0);
    splineImg->addStroke(new TStroke(*stroke), false);
    TUndoManager::manager()->add(undo);
    tool->notifyImageChanged();
    tool->invalidate();
    return;
  }

  TVectorImageP tarImg = TImageP(tool->touchImage());
  if (!tarImg) return;
  TPaletteP palette       = tarImg->getPalette();
  TPaletteP oldPalette    = new TPalette();
  if (palette) oldPalette = palette->clone();
  std::set<int> indexSet(m_indexes.begin(), m_indexes.end());
  bool isPaste = pasteStrokesWithoutUndo(tarImg, indexSet, m_sceneHandle);

  if (isPaste) {
    m_indexes.clear();
    std::copy(indexSet.begin(), indexSet.end(), std::back_inserter(m_indexes));
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(new PasteStrokesUndo(
        level, tool->getCurrentFid(), indexSet, oldPalette, m_sceneHandle,
        tool->m_isFrameCreated, tool->m_isLevelCreated));
    m_updateSelectionBBox = isPaste;
  }
  tool->notifyImageChanged();
  tool->getApplication()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->notifyPaletteChanged();
  m_updateSelectionBBox = false;
  tool->invalidate();
}

//=============================================================================
//
// StrokeSelection::cut()
//
//-----------------------------------------------------------------------------

void StrokeSelection::cut() {
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  if (!isEditable()) {
    DVGui::error(
        QObject::tr("The selection cannot be deleted. It is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  TUndo *undo;
  if (isSpline)
    undo = new ToolUtils::UndoPath(
        tool->getXsheet()->getStageObject(tool->getObjectId())->getSpline());

  StrokesData *data = new StrokesData();
  std::set<int> indexSet(m_indexes.begin(), m_indexes.end());
  data->setImage(m_vi, indexSet);
  std::set<int> oldIndexes = indexSet;
  cutStrokesWithoutUndo(m_vi, indexSet);
  m_indexes.clear();
  if (!isSpline) {
    TXshSimpleLevel *level =
        tool->getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndoManager::manager()->add(new CutStrokesUndo(
        level, tool->getCurrentFid(), oldIndexes, data, m_sceneHandle));
  } else {
    assert(undo);
    if (undo) TUndoManager::manager()->add(undo);
  }
}

//=============================================================================
//
// Align and Distribute
//
//-----------------------------------------------------------------------------

// Anchor points
// Align Type
//    left: minX
//    right: maxX
//    top: maxY
//    bottom: minY
//    center h: (maxY + minY) / 2.0;
//    center v: (maxX + minX) / 2.0;
//    distribute h: min midX, max midX
//    distribute v: min midY, max midY
TPointD getAnchorPoint(ALIGN_TYPE alignType, int alignMethod, TVectorImageP vi,
                       std::vector<int> indexes) {
  double x, midX, maxX, midXLen, y, midY, maxY, midYLen;
  TRectD cameraRect = TTool::getApplication()
                          ->getCurrentScene()
                          ->getScene()
                          ->getCurrentCamera()
                          ->getStageRect();

  TApplication *app   = TTool::getApplication();
  bool isEditingLevel = app->getCurrentFrame()->isEditingLevel();

  if (alignMethod == ALIGN_METHOD::CAMERA_AREA && !isEditingLevel) {
    TTool *tool = app->getCurrentTool()->getTool();
    if (tool) {
      int frame    = app->getCurrentFrame()->getFrameIndex();
      int col      = app->getCurrentColumn()->getColumnIndex();
      TXsheet *xsh = tool->getXsheet();
      TStageObjectId currentCamId =
          TStageObjectId::CameraId(xsh->getCameraColumnIndex());
      TStageObjectId levelId = TStageObjectId::ColumnId(col);

      TAffine camAff = xsh->getPlacement(currentCamId, frame);
      TAffine lvlAff = xsh->getPlacement(levelId, frame);

      TPointD blPt =
          lvlAff.inv() * camAff * TPointD(cameraRect.x0, cameraRect.y0);
      TPointD brPt =
          lvlAff.inv() * camAff * TPointD(cameraRect.x1, cameraRect.y0);
      TPointD tlPt =
          lvlAff.inv() * camAff * TPointD(cameraRect.x0, cameraRect.y1);
      TPointD trPt =
          lvlAff.inv() * camAff * TPointD(cameraRect.x1, cameraRect.y1);

      cameraRect.x0 = (blPt.x + tlPt.x) / 2.0;
      cameraRect.y0 = (blPt.y + brPt.y) / 2.0;
      cameraRect.x1 = (brPt.x + trPt.x) / 2.0;
      cameraRect.y1 = (tlPt.y + trPt.y) / 2.0;
    }
  }

  // Camera - except distribution
  if (alignMethod == ALIGN_METHOD::CAMERA_AREA &&
      alignType != ALIGN_TYPE::DISTRIBUTE_H &&
      alignType != ALIGN_TYPE::DISTRIBUTE_V) {
    if (alignType == ALIGN_TYPE::ALIGN_LEFT ||
        alignType == ALIGN_TYPE::ALIGN_BOTTOM) {
      x = cameraRect.x0;
      y = cameraRect.y0;
    } else if (alignType == ALIGN_TYPE::ALIGN_RIGHT ||
               alignType == ALIGN_TYPE::ALIGN_TOP) {
      x = cameraRect.x1;
      y = cameraRect.y1;
    } else if (alignType == ALIGN_TYPE::ALIGN_CENTER_H ||
               alignType == ALIGN_TYPE::ALIGN_CENTER_V) {
      x = (cameraRect.x1 + cameraRect.x0) / 2.0;
      y = (cameraRect.y1 + cameraRect.y0) / 2.0;
    }

    return TPointD(x, y);
  }

  bool ptSet = false;
  double lastArea;
  double lenA, lenB;

  for (auto const &e : indexes) {
    TStroke *stroke = vi->getStroke(e);
    if (!vi->isEnteredGroupStroke(e)) continue;
    TRectD bbox = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(vi, e, gBBox)) bbox = gBBox;
    double area = bbox.getLx() * bbox.getLy();
    midXLen     = (bbox.getLx() / 2.0);
    midX        = bbox.x0 + midXLen;
    midYLen     = (bbox.getLy() / 2.0);
    midY        = bbox.y0 + midYLen;
    if (!ptSet || alignMethod == ALIGN_METHOD::LAST_SELECTED) {
      ptSet    = true;
      lastArea = area;
      if (alignType == ALIGN_TYPE::ALIGN_LEFT ||
          alignType == ALIGN_TYPE::ALIGN_BOTTOM) {
        x = bbox.x0;
        y = bbox.y0;
      } else if (alignType == ALIGN_TYPE::ALIGN_RIGHT ||
                 alignType == ALIGN_TYPE::ALIGN_TOP) {
        x = bbox.x1;
        y = bbox.y1;
      } else if (alignType == ALIGN_TYPE::ALIGN_CENTER_H ||
                 alignType == ALIGN_TYPE::ALIGN_CENTER_V) {
        x    = bbox.x0;
        maxX = bbox.x1;
        y    = bbox.y0;
        maxY = bbox.y1;
      } else if (alignType == ALIGN_TYPE::DISTRIBUTE_H) {
        // Note: y will be used as maxX
        x = y = midX;
        lenA = lenB = midXLen;
      } else if (alignType == ALIGN_TYPE::DISTRIBUTE_V) {
        // Note: x will be used as minY
        x = y = midY;
        lenA = lenB = midYLen;
      }

      if (alignMethod == ALIGN_METHOD::FIRST_SELECTED) break;
    } else {
      if (alignType == ALIGN_TYPE::DISTRIBUTE_H) {
        // Note: y will be used as maxX
        if (alignMethod == ALIGN_METHOD::CAMERA_AREA) {
          if (midX < x) {
            x    = midX;
            lenA = midXLen;
          }
          if (midX >= y) {
            y    = midX;
            lenB = midXLen;
          }
        } else {
          x = std::min(x, midX);
          y = std::max(y, midX);
        }
      } else if (alignType == ALIGN_TYPE::DISTRIBUTE_V) {
        // Note: x will be used as minY
        if (alignMethod == ALIGN_METHOD::CAMERA_AREA) {
          if (midY < x) {
            x    = midY;
            lenA = midYLen;
          }
          if (midY >= y) {
            y    = midY;
            lenB = midYLen;
          }
        } else {
          x = std::min(x, midY);
          y = std::max(y, midY);
        }
      } else if (!alignMethod ||
                 (alignMethod == ALIGN_METHOD::SMALLEST_OBJECT &&
                  area <= lastArea) ||
                 (alignMethod == ALIGN_METHOD::LARGEST_OBJECT &&
                  area >= lastArea)) {
        if (alignType == ALIGN_TYPE::ALIGN_LEFT ||
            alignType == ALIGN_TYPE::ALIGN_BOTTOM) {
          if (alignMethod && area != lastArea) {
            x = bbox.x0;
            y = bbox.y0;
          } else {
            x = std::min(x, bbox.x0);
            y = std::min(y, bbox.y0);
          }
        } else if (alignType == ALIGN_TYPE::ALIGN_RIGHT ||
                   alignType == ALIGN_TYPE::ALIGN_TOP) {
          if (alignMethod && area != lastArea) {
            x = bbox.x1;
            y = bbox.y1;
          } else {
            x = std::max(x, bbox.x1);
            y = std::max(y, bbox.y1);
          }
        } else if (alignType == ALIGN_TYPE::ALIGN_CENTER_H ||
                   alignType == ALIGN_TYPE::ALIGN_CENTER_V) {
          if (alignMethod && area != lastArea) {
            x    = bbox.x0;
            maxX = bbox.x1;
            y    = bbox.y0;
            maxY = bbox.y1;
          } else {
            x    = std::min(x, bbox.x0);
            maxX = std::max(maxX, bbox.x1);
            y    = std::min(y, bbox.y0);
            maxY = std::max(maxY, bbox.y1);
          }
        }
        lastArea = area;
      }
    }
  }

  if (alignType == ALIGN_TYPE::ALIGN_CENTER_H ||
      alignType == ALIGN_TYPE::ALIGN_CENTER_V)
    return TPointD((maxX + x) / 2.0, (maxY + y) / 2.0);
  else if (alignMethod == ALIGN_METHOD::CAMERA_AREA) {
    if (alignType == ALIGN_TYPE::DISTRIBUTE_H)
      return TPointD(cameraRect.x0 + lenA, cameraRect.x1 - lenB);
    else if (alignType == ALIGN_TYPE::DISTRIBUTE_V)
      return TPointD(cameraRect.y0 + lenA, cameraRect.y1 - lenB);
  }

  return TPointD(x, y);
}

void StrokeSelection::alignStrokesLeft() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 2) return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::ALIGN_LEFT, alignMethod, m_vi, m_indexes));
  double newX = anchor.x;

  // Store moving strokes
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    TRectD bbox = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    if (bbox.x0 == newX) continue;
    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo =
        new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                             ALIGN_TYPE::ALIGN_LEFT, alignMethod, newX, 0);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//-----------------------------------------------------------------------------

void StrokeSelection::alignStrokesRight() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 2) return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::ALIGN_RIGHT, alignMethod, m_vi, m_indexes));
  double newX = anchor.x;

  // Store moving strokes
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    TRectD bbox = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    if (bbox.x1 == newX) continue;
    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo =
        new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                             ALIGN_TYPE::ALIGN_RIGHT, alignMethod, newX, 0);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//-----------------------------------------------------------------------------

void StrokeSelection::alignStrokesTop() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 2) return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::ALIGN_TOP, alignMethod, m_vi, m_indexes));
  double newY = anchor.y;

  // Store moving strokes
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    TRectD bbox = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    if (bbox.y1 == newY) continue;
    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo =
        new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                             ALIGN_TYPE::ALIGN_TOP, alignMethod, 0, newY);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//-----------------------------------------------------------------------------

void StrokeSelection::alignStrokesBottom() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 2) return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::ALIGN_BOTTOM, alignMethod, m_vi, m_indexes));
  double newY = anchor.y;

  // Store moving strokes
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    TRectD bbox = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    if (bbox.y0 == newY) continue;
    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo =
        new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                             ALIGN_TYPE::ALIGN_BOTTOM, alignMethod, 0, newY);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//-----------------------------------------------------------------------------

void StrokeSelection::alignStrokesCenterH() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 2) return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::ALIGN_CENTER_H, alignMethod, m_vi, m_indexes));
  double newY = anchor.y;

  // Store moving strokes
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    TRectD bbox = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo =
        new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                             ALIGN_TYPE::ALIGN_CENTER_H, alignMethod, 0, newY);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//-----------------------------------------------------------------------------

void StrokeSelection::alignStrokesCenterV() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 2) return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::ALIGN_CENTER_H, alignMethod, m_vi, m_indexes));
  double newX = anchor.x;

  // Store moving strokes
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    TRectD bbox = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo =
        new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                             ALIGN_TYPE::ALIGN_CENTER_V, alignMethod, newX, 0);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//-----------------------------------------------------------------------------

class MidXStrokeSorter {
  TVectorImageP m_vi;

public:
  MidXStrokeSorter(TVectorImageP vi) : m_vi(vi) {}

  bool operator()(int a, int &b) {
    TStroke *strokeA = m_vi->getStroke(a);
    TStroke *strokeB = m_vi->getStroke(b);
    TRectD gBox;
    TRectD bboxA = strokeA->getBBox();
    if (getGroupBBox(m_vi, a, gBox)) bboxA = gBox;
    TRectD bboxB = strokeB->getBBox();
    if (getGroupBBox(m_vi, b, gBox)) bboxB = gBox;
    double midA = bboxA.x0 + (bboxA.getLx() / 2.0);
    double midB = bboxB.x0 + (bboxB.getLx() / 2.0);

    return (midA < midB);
  }
};

void StrokeSelection::distributeStrokesH() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 3 ||
      m_indexes.size() < 2)
    return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Sort the points based on X
  std::vector<int> sortedStrokes;

  // Remove strokes not in group
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    sortedStrokes.push_back(e);
  }
  std::sort(sortedStrokes.begin(), sortedStrokes.end(), MidXStrokeSorter(m_vi));

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::DISTRIBUTE_H, alignMethod, m_vi, m_indexes));
  double minX = anchor.x, maxX = anchor.y;

  // Store moving strokes
  int strokeCount = 0;
  std::set<int> groups;
  int groupDepth = m_vi->isInsideGroup();
  for (auto const &e : sortedStrokes) {
    TStroke *stroke      = m_vi->getStroke(e);
    int strokeGroupDepth = m_vi->getGroupDepth(e);
    TRectD bbox          = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    double midX = bbox.x0 + (bbox.getLx() / 2.0);
    if (alignMethod != ALIGN_METHOD::CAMERA_AREA &&
        (midX == minX || midX == maxX))
      continue;
    int groupId = m_vi->getGroupByStroke(e);
    if (groupId < 0 || strokeGroupDepth == groupDepth)
      strokeCount++;
    else if (groups.find(groupId) == groups.end()) {
      groups.insert(groupId);
      strokeCount++;
    }

    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (alignMethod == ALIGN_METHOD::CAMERA_AREA) strokeCount -= 2;

  double distX = (maxX - minX) / (double)(strokeCount + 1);

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo = new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                                       ALIGN_TYPE::DISTRIBUTE_H, alignMethod,
                                       minX, distX);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//-----------------------------------------------------------------------------

class MidYStrokeSorter {
  TVectorImageP m_vi;

public:
  MidYStrokeSorter(TVectorImageP vi) : m_vi(vi) {}

  bool operator()(int a, int &b) {
    TStroke *strokeA = m_vi->getStroke(a);
    TStroke *strokeB = m_vi->getStroke(b);
    TRectD gBox;
    TRectD bboxA = strokeA->getBBox();
    if (getGroupBBox(m_vi, a, gBox)) bboxA = gBox;
    TRectD bboxB = strokeB->getBBox();
    if (getGroupBBox(m_vi, b, gBox)) bboxB = gBox;
    double midA = bboxA.y0 + (bboxA.getLy() / 2.0);
    double midB = bboxB.y0 + (bboxB.getLy() / 2.0);

    return (midA < midB);
  }
};

void StrokeSelection::distributeStrokesV() {
  if (!m_vi) return;
  if (m_indexes.empty()) return;
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  int alignMethod = tool->getAlignMethod();
  if (alignMethod != ALIGN_METHOD::CAMERA_AREA && m_indexes.size() < 3 ||
      m_indexes.size() < 2)
    return;

  if (!isEditable()) {
    DVGui::error(QObject::tr("The selection is not editable."));
    return;
  }

  bool isSpline = tool->getApplication()->getCurrentObject()->isSpline();
  if (isSpline) return;

  std::vector<std::pair<int, TStroke *>> undoData;

  m_vi->findRegions();

  // Sort the points based on Y
  std::vector<int> sortedStrokes;

  // Remove strokes not in group
  for (auto const &e : m_indexes) {
    TStroke *stroke = m_vi->getStroke(e);
    if (!m_vi->isEnteredGroupStroke(e)) continue;
    sortedStrokes.push_back(e);
  }
  std::sort(sortedStrokes.begin(), sortedStrokes.end(), MidYStrokeSorter(m_vi));

  // Find anchor point
  TPointD anchor(
      getAnchorPoint(ALIGN_TYPE::DISTRIBUTE_V, alignMethod, m_vi, m_indexes));
  double minY = anchor.x, maxY = anchor.y;

  // Store moving strokes
  int strokeCount = 0;
  std::set<int> groups;
  int groupDepth = m_vi->isInsideGroup();
  for (auto const &e : sortedStrokes) {
    TStroke *stroke      = m_vi->getStroke(e);
    int strokeGroupDepth = m_vi->getGroupDepth(e);
    TRectD bbox          = stroke->getBBox();
    TRectD gBBox;
    if (getGroupBBox(m_vi, e, gBBox)) bbox = gBBox;
    double midY = bbox.y0 + (bbox.getLy() / 2.0);
    if (alignMethod != ALIGN_METHOD::CAMERA_AREA &&
        (midY == minY || midY == maxY))
      continue;
    int groupId = m_vi->getGroupByStroke(e);
    if (groupId < 0 || strokeGroupDepth == groupDepth)
      strokeCount++;
    else if (groups.find(groupId) == groups.end()) {
      groups.insert(groupId);
      strokeCount++;
    }

    undoData.push_back(std::pair<int, TStroke *>(e, new TStroke(*stroke)));
  }

  if (alignMethod == ALIGN_METHOD::CAMERA_AREA) strokeCount -= 2;

  double distY = (maxY - minY) / (double)(strokeCount + 1);

  if (!undoData.empty()) {
    TXshSimpleLevel *level =
        TTool::getApplication()->getCurrentLevel()->getSimpleLevel();
    TUndo *undo = new AlignStrokesUndo(level, tool->getCurrentFid(), undoData,
                                       ALIGN_TYPE::DISTRIBUTE_V, alignMethod,
                                       minY, distY);
    TUndoManager::manager()->add(undo);
    undo->redo();
  }

  m_updateSelectionBBox = true;
  tool->notifyImageChanged();
  m_updateSelectionBBox = false;
}

//=============================================================================
//
// enableCommands
//
//-----------------------------------------------------------------------------

void StrokeSelection::enableCommands() {
  enableCommand(this, MI_Clear, &StrokeSelection::deleteStrokes);
  enableCommand(this, MI_Cut, &StrokeSelection::cut);
  enableCommand(this, MI_Copy, &StrokeSelection::copy);
  enableCommand(this, MI_Paste, &StrokeSelection::paste);

  enableCommand(m_groupCommand.get(), MI_Group, &TGroupCommand::group);
  enableCommand(m_groupCommand.get(), MI_Ungroup, &TGroupCommand::ungroup);
  enableCommand(m_groupCommand.get(), MI_BringToFront, &TGroupCommand::front);
  enableCommand(m_groupCommand.get(), MI_BringForward, &TGroupCommand::forward);
  enableCommand(m_groupCommand.get(), MI_SendBack, &TGroupCommand::back);
  enableCommand(m_groupCommand.get(), MI_SendBackward,
                &TGroupCommand::backward);
  enableCommand(m_groupCommand.get(), MI_EnterGroup,
                &TGroupCommand::enterGroup);
  enableCommand(m_groupCommand.get(), MI_ExitGroup, &TGroupCommand::exitGroup);

  enableCommand(this, MI_RemoveEndpoints, &StrokeSelection::removeEndpoints);
  enableCommand(this, MI_SelectAll, &StrokeSelection::selectAll);

  enableCommand(this, MI_AlignLeft, &StrokeSelection::alignStrokesLeft);
  enableCommand(this, MI_AlignRight, &StrokeSelection::alignStrokesRight);
  enableCommand(this, MI_AlignTop, &StrokeSelection::alignStrokesTop);
  enableCommand(this, MI_AlignBottom, &StrokeSelection::alignStrokesBottom);
  enableCommand(this, MI_AlignCenterHorizontal,
                &StrokeSelection::alignStrokesCenterH);
  enableCommand(this, MI_AlignCenterVertical,
                &StrokeSelection::alignStrokesCenterV);
  enableCommand(this, MI_DistributeHorizontal,
                &StrokeSelection::distributeStrokesH);
  enableCommand(this, MI_DistributeVertical,
                &StrokeSelection::distributeStrokesV);
}

//===================================================================
namespace {
//===================================================================

class UndoSetStrokeStyle final : public TUndo {
  TVectorImageP m_image;
  std::vector<int> m_strokeIndexes;
  std::vector<int> m_oldStyles;
  int m_newStyle;

public:
  UndoSetStrokeStyle(TVectorImageP image, int newStyle)
      : m_image(image), m_newStyle(newStyle) {}

  void addStroke(TStroke *stroke) {
    m_strokeIndexes.push_back(m_image->getStrokeIndex(stroke));
    m_oldStyles.push_back(stroke->getStyle());
  }

  void undo() const override {
    UINT size = m_strokeIndexes.size();
    assert(size == m_oldStyles.size());

    for (UINT i = 0; i != size; i++) {
      int index = m_strokeIndexes[i];
      if (index != -1 && index < (int)m_image->getStrokeCount())
        m_image->getStroke(index)->setStyle(m_oldStyles[i]);
    }

    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  void redo() const override {
    UINT size = m_strokeIndexes.size();
    assert(size == m_oldStyles.size());

    for (UINT i = 0; i != size; i++) {
      int index = m_strokeIndexes[i];
      if (index != -1 && index < (int)m_image->getStrokeCount())
        m_image->getStroke(index)->setStyle(m_newStyle);
    }

    TTool::getApplication()->getCurrentTool()->getTool()->notifyImageChanged();
  }

  int getSize() const override {
    return sizeof(*this) +
           m_strokeIndexes.capacity() * sizeof(m_strokeIndexes[0]) +
           m_oldStyles.capacity() * sizeof(m_oldStyles[0]);
  }
};

}  // namespace

//=============================================================================
//
// changeColorStyle
//
//-----------------------------------------------------------------------------

void StrokeSelection::changeColorStyle(int styleIndex) {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;
  TVectorImageP img(tool->getImage(true));
  if (!img) return;
  TPalette *palette = img->getPalette();
  TColorStyle *cs   = palette->getStyle(styleIndex);
  if (!cs->isStrokeStyle()) return;
  if (m_indexes.empty()) return;

  UndoSetStrokeStyle *undo = new UndoSetStrokeStyle(img, styleIndex);
  std::vector<int>::iterator it;
  for (it = m_indexes.begin(); it != m_indexes.end(); ++it) {
    int index = *it;
    assert(0 <= index && index < (int)img->getStrokeCount());
    TStroke *stroke = img->getStroke(index);
    undo->addStroke(stroke);
    stroke->setStyle(styleIndex);
  }

  tool->notifyImageChanged();
  TUndoManager::manager()->add(undo);
}

//-----------------------------------------------------------------------------

bool StrokeSelection::isEditable() {
  TTool::Application *app = TTool::getApplication();
  TXshSimpleLevel *level  = app->getCurrentLevel()->getSimpleLevel();

  TFrameHandle *frame = app->getCurrentFrame();
  bool filmstrip      = frame->isEditingLevel();

  if (level) {
    if (level->isReadOnly()) return false;

    TFrameId frameId = app->getCurrentTool()->getTool()->getCurrentFid();
    if (level->isFrameReadOnly(frameId)) return false;
  }

  if (!filmstrip) {
    int colIndex  = app->getCurrentTool()->getTool()->getColumnIndex();
    bool isSpline = app->getCurrentObject()->isSpline();
    if (colIndex < 0 && !isSpline) return false;
    int rowIndex = frame->getFrame();
    if (app->getCurrentTool()->getTool()->isColumnLocked(colIndex))
      return false;

    TXsheet *xsh      = app->getCurrentXsheet()->getXsheet();
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(colIndex));
    // Test for Mesh-deformed levels
    const TStageObjectId &parentId = obj->getParent();
    if (parentId.isColumn() && obj->getParentHandle()[0] != 'H') {
      TXshCell cell             = xsh->getCell(rowIndex, parentId.getIndex());
      TXshSimpleLevel *parentSl = cell.getSimpleLevel();
      if (!cell.getFrameId().isStopFrame() && parentSl &&
          parentSl->getType() == MESH_XSHLEVEL)
        return false;
    }
  }

  return true;
}
