

#include "xshcellmover.h"

// Tnz6 includes
#include "tapp.h"
#include "xsheetviewer.h"
#include "cellselection.h"
#include "columncommand.h"
#include "columnselection.h"

// TnzTools includes
#include "tools/cursors.h"
#include "tools/cursormanager.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"
#include "toonzqt/stageobjectsdata.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/preferences.h"
#include "toonz/txsheet.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tscenehandle.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/txsheethandle.h"
#include "toonz/fxdag.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcolumnhandle.h"

// TnzBase includes
#include "tfx.h"

// Qt includes
#include <QMouseEvent>
#include <QPainter>
#include <QDebug>

//=============================================================================

CellsMover::CellsMover()
    : m_rowCount(0)
    , m_colCount(0)
    , m_pos(0, 0)
    , m_startPos(0, 0)
    , m_columnsData(0)
    , m_qualifiers(0)
    , m_uffa(0)
    , m_orientation(nullptr) {}

CellsMover::~CellsMover() {
  delete m_columnsData;
  m_columnsData = 0;
}

//
// initialize the mover: allocate memory, get cells and column data
//
void CellsMover::start(int r0, int c0, int r1, int c1, int qualifiers,
                       const Orientation *o) {
  m_rowCount = r1 - r0 + 1;
  m_colCount = c1 - c0 + 1;
  assert(m_rowCount > 0);
  assert(m_colCount > 0);
  m_orientation = o;
  m_startPos    = m_pos =
      (o->isVerticalTimeline() ? TPoint(c0, r0) : TPoint(r0, c0));
  m_qualifiers = qualifiers;
  m_cells.resize(m_rowCount * m_colCount, TXshCell());
  m_oldCells.resize(m_rowCount * m_colCount, TXshCell());
  getCells(m_cells, r0, c0);
  getColumnsData(c0, c1);

  if (m_qualifiers & eInsertCells && !(m_qualifiers & eOverwriteCells))
    getImplicitCellInfo();
}

//
// helper method: returns the current xsheet
//
TXsheet *CellsMover::getXsheet() const {
  return TApp::instance()->getCurrentXsheet()->getXsheet();
}

//
// keeping the implicit cell arrangement when start shift + dragging
//

void CellsMover::getImplicitCellInfo() {
  int startCol =
      (m_orientation->isVerticalTimeline()) ? m_startPos.x : m_startPos.y;
  int startRow =
      (m_orientation->isVerticalTimeline()) ? m_startPos.y : m_startPos.x;

  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) {
    TXshColumn *column = xsh->getColumn(startCol + i);
    if (!column || column->getCellColumn() == 0 || column->isLocked()) continue;
    TXshCellColumn *cellCol = column->getCellColumn();
    QMap<int, TXshCell> cellInfo;
    if (!column->isEmpty()) {
      int r0, r1;
      column->getRange(r0, r1);
      TXshCell currentCell;
      for (int r = r0; r <= r1 + 1; r++) {
        TXshCell tmpCell = cellCol->getCell(r, false);
        if (tmpCell != currentCell) {
          if (m_qualifiers & eCopyCells)
            cellInfo.insert(r, tmpCell);
          else {
            if (r < startRow)
              cellInfo.insert(r, tmpCell);
            else if (r >= startRow + m_rowCount)
              cellInfo.insert(r - m_rowCount, tmpCell);
          }
          currentCell = tmpCell;
        }
      }
    }

    m_implicitCellInfo.append(cellInfo);
  }
}

//
// cells <- xsheet
//

void CellsMover::getCells(std::vector<TXshCell> &cells, int r, int c) const {
  for (int i = 0; i < (int)cells.size(); i++) cells[i] = TXshCell();
  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) {
    TXshColumn *column = xsh->getColumn(c + i);
    if (column && column->isLocked()) continue;
    xsh->getCells(r, c + i, m_rowCount, &cells[m_rowCount * i]);
  }
}

//
// xsheet <- cells
//
void CellsMover::setCells(const std::vector<TXshCell> &cells, int r,
                          int c) const {
  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) {
    TXshColumn *column = xsh->getColumn(c + i);
    if (column)  // skip if columns is locked or doesn't support cells operation
      if (column->getCellColumn() == 0 || column->isLocked()) continue;
    xsh->setCells(r, c + i, m_rowCount, &cells[m_rowCount * i]);
  }
}

//
// m_oldCells <- xsheet
//
void CellsMover::saveCells() {
  int r = m_pos.y;
  int c = m_pos.x;
  if (!getOrientation()->isVerticalTimeline()) {
    r = m_pos.x;
    c = m_pos.y;
  }
  getCells(m_oldCells, r, c);
}

//
// xsheet <- m_cells; insert cells if qualifiers contain eInsertCells
//
void CellsMover::moveCells(const TPoint &pos) const {
  int r = pos.y;
  int c = pos.x;
  if (!getOrientation()->isVerticalTimeline()) {
    r = pos.x;
    c = pos.y;
  }
  TXsheet *xsh = getXsheet();
  if (m_qualifiers & eInsertCells) {
    for (int i = 0; i < m_colCount; i++) {
      TXshColumn *column = xsh->getColumn(c + i);
      if (column) {
        if (column->getCellColumn() == 0 || column->isLocked()) continue;
        xsh->insertCells(r, c + i, m_rowCount);
      }
    }
  }
  setCells(m_cells, r, c);

  // Act like implicit hold when dragging cells with pressing Shift key
  // ( and WITHOUT Alt key ) and dragging within the same column.
  if (m_qualifiers & eInsertCells && !(m_qualifiers & eOverwriteCells)) {
    int startCol =
        (m_orientation->isVerticalTimeline()) ? m_startPos.x : m_startPos.y;
    if (startCol == c) {
      int infoId = 0;
      for (int i = 0; i < m_colCount; i++) {
        TXshColumn *column = xsh->getColumn(c + i);
        if (!column || column->getCellColumn() == 0 || column->isLocked())
          continue;

        QMap<int, TXshCell>::const_iterator itr =
            m_implicitCellInfo[infoId].lowerBound(r);
        if (itr == m_implicitCellInfo[infoId].end() || itr.key() == r) {
          infoId++;
          continue;
        }
        // a cell at the bottom of the inserted cells
        TXshCell bottomCell = xsh->getCell(r + m_rowCount - 1, c + i, false);
        for (int tmp_r = r + m_rowCount; tmp_r <= itr.key() - 1 + m_rowCount;
             tmp_r++)
          if (Preferences::instance()->isImplicitHoldEnabled())
            xsh->setCell(tmp_r, c + i, TXshCell(0, TFrameId::EMPTY_FRAME));
          else
            xsh->setCell(tmp_r, c + i, bottomCell);

        infoId++;
      }
    }
  }
}

void CellsMover::undoMoveCells(const TPoint &pos) const {
  TXsheet *xsh = getXsheet();
  int r        = pos.y;
  int c        = pos.x;
  if (!m_orientation->isVerticalTimeline()) {
    r = pos.x;
    c = pos.y;
  }

  if (m_qualifiers & eInsertCells) {
    // Act like implicit hold when dragging cells with pressing Shift key
    // ( and WITHOUT Alt key ) and dragging within the same column.
    if (!(m_qualifiers & eOverwriteCells) && r > 0) {
      int startCol =
          (m_orientation->isVerticalTimeline()) ? m_startPos.x : m_startPos.y;
      if (startCol == c) {
        int infoId = 0;
        for (int i = 0; i < m_colCount; i++) {
          TXshColumn *column = xsh->getColumn(c + i);
          if (!column || column->getCellColumn() == 0 || column->isLocked())
            continue;

          // clear the cells and restore original arrangement
          TXshCellColumn *cellCol = column->getCellColumn();
          int r0, r1;
          cellCol->getRange(r0, r1);
          cellCol->clearCells(r0, r1 - r0 + 1);
          QList<int> implicitKeys = m_implicitCellInfo[infoId].keys();
          for (int k = 0; k < implicitKeys.size(); k++) {
            int from = implicitKeys[k];
            int to   = k + 1 >= implicitKeys.size() ? from : (implicitKeys[k + 1] - 1);
            for (int f = from; f <= to; f++)
              cellCol->setCell(f, m_implicitCellInfo[infoId].value(from));
          }
          infoId++;
        }
        return;
      }
    }

    for (int i = 0; i < m_colCount; i++) xsh->removeCells(r, c + i, m_rowCount);
  } else {
    for (int i = 0; i < m_colCount; i++) xsh->clearCells(r, c + i, m_rowCount);
    if (m_qualifiers & eOverwriteCells) setCells(m_oldCells, r, c);
  }
}

bool CellsMover::canMoveCells(const TPoint &pos) {
  TXsheet *xsh = getXsheet();
  int r        = pos.y;
  int c        = pos.x;
  int cStart   = m_startPos.x;
  if (!m_orientation->isVerticalTimeline()) {
    r      = pos.x;
    c      = pos.y;
    cStart = m_startPos.y;
  }
  if (c < 0 ||
      (!m_orientation->isVerticalTimeline() && c >= xsh->getColumnCount()))
    return false;
  if (c != cStart) {
    int count = 0;
    // controlla il tipo
    int i = 0;
    while (i < m_rowCount * m_colCount) {
      TXshColumn::ColumnType srcType = getColumnTypeFromCell(i);
      int dstIndex                   = c + i;
      TXshColumn *dstColumn          = xsh->getColumn(dstIndex);
      if (srcType == TXshColumn::eZeraryFxType ||
          srcType == TXshColumn::eSoundTextType)
        return false;
      if (dstColumn && !dstColumn->isEmpty() &&
          dstColumn->getColumnType() != srcType)
        return false;
      if (!dstColumn || dstColumn->isLocked() == false) count++;
      i += m_rowCount;
    }
    if (count == 0) return false;
  }
  if ((m_qualifiers & CellsMover::eInsertCells) ||
      (m_qualifiers & CellsMover::eOverwriteCells))
    return true;
  int count = 0;
  for (int i = 0; i < m_colCount; i++) {
    for (int j = 0; j < m_rowCount; j++) {
      if (!xsh->getCell(r + j, c + i, false).isEmpty()) return false;
      count++;
    }
  }
  if (count == 0) return false;
  return true;
}
//
// m_columnsData <- xsheet
//

void CellsMover::getColumnsData(int c0, int c1) {
  TXsheet *xsh  = getXsheet();
  int colCount  = c1 - c0 + 1;
  m_columnsData = new StageObjectsData();
  std::set<int> ii;
  for (int i = 0; i < colCount; i++) ii.insert(c0 + i);
  m_columnsData->storeColumns(
      ii, xsh,
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
  m_columnsData->storeColumnFxs(
      ii, xsh,
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
}

//
// xsheet <- m_columnsData, at column c
//
// Note: the restored columns are empty (i.e. no cells are copied)
//

void CellsMover::restoreColumns(int c) const {
  std::set<int> ii;
  for (int i = 0; i < m_colCount; i++) ii.insert(c + i);
  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) xsh->removeColumn(c);
  std::list<int> restoredSplineIds;
  m_columnsData->restoreObjects(
      ii, restoredSplineIds, xsh,
      StageObjectsData::eDoClone | StageObjectsData::eResetFxDagPositions);
  for (int i = 0; i < m_colCount; i++) {
    TXshColumn *column = xsh->getColumn(c + i);
    if (!column) continue;
    TXshCellColumn *cellColumn = column->getCellColumn();
    if (!cellColumn) continue;
    int r0 = 0, r1 = -1;
    cellColumn->getRange(r0, r1);
    if (r0 <= r1) cellColumn->clearCells(r0, r1 - r0 + 1);
  }
}

//
// the columns [c,c+m_colCount-1] becme empty
//

void CellsMover::emptyColumns(int c) const {
  std::set<int> ii;
  for (int i = 0; i < m_colCount; i++) ii.insert(c + i);
  ColumnCmd::deleteColumns(ii, false, true);
  TXsheet *xsh = getXsheet();
  for (int i = 0; i < m_colCount; i++) xsh->insertColumn(c);
}

TXshColumn::ColumnType CellsMover::getColumnTypeFromCell(int index) const {
  const TXshLevelP &xl = m_cells[index].m_level;
  return xl ? TXshColumn::toColumnType(xl->getType()) : TXshColumn::eLevelType;
}

//=============================================================================

class LevelMoverUndo final : public TUndo {
  CellsMover m_cellsMover;

public:
  LevelMoverUndo() {}

  std::vector<int> m_columnsChanges;

  CellsMover *getCellsMover() { return &m_cellsMover; }

  void undo() const override {
    if (m_cellsMover.getOrientation()->isVerticalTimeline())
      m_cellsMover.undoMoveCells();
    else {
      int r = m_cellsMover.getPos().y;
      int c = m_cellsMover.getPos().x;
      m_cellsMover.undoMoveCells(TPoint(c, r));
    }

    int ca       = m_cellsMover.getStartPos().x;
    int cb       = m_cellsMover.getPos().x;
    int colCount = m_cellsMover.getColumnCount();
    if (!m_cellsMover.getOrientation()->isVerticalTimeline()) {
      ca = m_cellsMover.getStartPos().y;
      cb = m_cellsMover.getPos().y;
    }

    if (ca != cb) {
      if (m_cellsMover.m_uffa & 2) m_cellsMover.emptyColumns(cb);
      if (m_cellsMover.m_uffa & 1) m_cellsMover.restoreColumns(ca);
    }
    // Undo of moving cells with copy qualifiers does not need to restore the
    // cells at start pos
    if (!(m_cellsMover.getQualifiers() & CellsMover::eCopyCells)) {
      if (m_cellsMover.getOrientation()->isVerticalTimeline())
        m_cellsMover.moveCells(m_cellsMover.getStartPos());
      else {
        int rStart = m_cellsMover.getStartPos().x;
        int cStart = m_cellsMover.getStartPos().y;
        const TPoint useStart(rStart, cStart);
        m_cellsMover.moveCells(useStart);
      }
    }
    TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_cellsMover.getColumnTypeFromCell(0) == TXshColumn::eSoundType)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }
  void redo() const override {
    int ca       = m_cellsMover.getStartPos().x;
    int cb       = m_cellsMover.getPos().x;
    int colCount = m_cellsMover.getColumnCount();
    if (!m_cellsMover.getOrientation()->isVerticalTimeline()) {
      int ca = m_cellsMover.getStartPos().y;
      int cb = m_cellsMover.getPos().y;
    }

    if (ca != cb) {
      if (m_cellsMover.m_uffa & 1) m_cellsMover.emptyColumns(ca);
      if (m_cellsMover.m_uffa & 2) m_cellsMover.restoreColumns(cb);
    }

    if ((m_cellsMover.getQualifiers() & CellsMover::eCopyCells) == 0 &&
        (m_cellsMover.getQualifiers() & CellsMover::eOverwriteCells) == 0)
      m_cellsMover.undoMoveCells(m_cellsMover.getStartPos());
    if (m_cellsMover.getQualifiers() &
        CellsMover::eOverwriteCells) {  // rimuove le celle vecchie
      int c, ra = m_cellsMover.getStartPos().y,
             rowCount = m_cellsMover.getRowCount();
      if (!m_cellsMover.getOrientation()->isVerticalTimeline())
        ra = m_cellsMover.getStartPos().x;
      TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
      for (c = ca; c < ca + colCount; c++) xsh->clearCells(ra, c, rowCount);
    }
    m_cellsMover.moveCells();

    TApp::instance()->getCurrentXsheet()->getXsheet()->updateFrameCount();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_cellsMover.getColumnTypeFromCell(0) == TXshColumn::eSoundType)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Move Level"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================

LevelMoverTool::LevelMoverTool(XsheetViewer *viewer)
    : XsheetGUI::DragTool(viewer)
    , m_range(0, 0)
    , m_qualifiers(0)
    , m_validPos(false)
    , m_undo(0)
    , m_moved(false)
    , m_columnsMoved(false)
    , m_dragged(false) {}

LevelMoverTool::~LevelMoverTool() {}

// isTotallyEmptyColumn == column empty, no fx
bool LevelMoverTool::isTotallyEmptyColumn(int col) const {
  if (col < 0) return false;
  TXsheet *xsh       = getViewer()->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  if (!column) return true;
  if (!column->isEmpty()) return false;
  TFx *fx = column->getFx();
  if (fx && fx->getOutputConnectionCount() != 0) return false;
  // bisogna controllare lo stage object
  return true;
}

bool LevelMoverTool::canMove(const TPoint &pos) {
  if (!m_undo) return false;
  CellsMover *cellsMover = m_undo->getCellsMover();
  if (!cellsMover) return false;
  return cellsMover->canMoveCells(pos);
}

bool LevelMoverTool::canMoveColumns(const TPoint &pos) {
  const Orientation *o = getViewer()->orientation();
  TXsheet *xsh         = getViewer()->getXsheet();
  int c                = pos.x;
  int cLast            = m_lastPos.x;
  int cRange           = m_range.lx;
  if (!o->isVerticalTimeline()) {
    c      = pos.y;
    cLast  = m_lastPos.y;
    cRange = m_range.ly;
  }
  if (c < 0 || (!o->isVerticalTimeline() && c >= xsh->getColumnCount()))
    return false;
  if (c != cLast) {
    int count = 0;
    // controlla il tipo
    for (int i = 0; i < cRange; i++) {
      int srcIndex = cLast + i;
      int dstIndex = c + i;
      if (!o->isVerticalTimeline() && dstIndex >= xsh->getColumnCount())
        return false;
      TXshColumn *srcColumn = xsh->getColumn(srcIndex);
      if (srcColumn && srcColumn->isLocked()) continue;
      TXshColumn *dstColumn          = xsh->getColumn(dstIndex);
      TXshColumn::ColumnType srcType = TXshColumn::eLevelType;
      if (srcColumn) srcType = srcColumn->getColumnType();
      if (srcType == TXshColumn::eZeraryFxType) return false;
      /*
      qDebug() << "check: " << srcIndex << ":" <<
      (srcColumn ? QString::number(srcType) : "empty") <<
      " => " << dstIndex << ":" <<
      (dstColumn ? QString::number(dstColumn->getColumnType()) : "empty");
      */

      if (dstColumn && !dstColumn->isEmpty() &&
          dstColumn->getColumnType() != srcType) {
        return false;
      }
      if (!dstColumn || dstColumn->isLocked() == false) {
        count++;
      }
    }
    if (count == 0) return false;
  }
  return true;
}

void LevelMoverTool::onClick(const QMouseEvent *e) {
  const Orientation *o      = getViewer()->orientation();
  QPoint pos                = e->pos();
  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  int row                   = cellPosition.frame();
  int col                   = cellPosition.layer();

  m_qualifiers = 0;
  if (Preferences::instance()->getDragCellsBehaviour() == 1)
    m_qualifiers |= CellsMover::eMoveColumns;
  if (e->modifiers() & Qt::ControlModifier)
    m_qualifiers |= CellsMover::eCopyCells;
  if (e->modifiers() & Qt::ShiftModifier)
    m_qualifiers |= CellsMover::eInsertCells;
  if (e->modifiers() & Qt::AltModifier)
    m_qualifiers |= CellsMover::eOverwriteCells;

  int cursorType = ToolCursor::CURSOR_ARROW;
  if (m_qualifiers & CellsMover::eCopyCells)
    cursorType = ToolCursor::SplineEditorCursorAdd;
  setToolCursor(getViewer(), cursorType);

  int r0, c0, r1, c1;
  getViewer()->getCellSelection()->getSelectedCells(r0, c0, r1, c1);

  m_validPos             = true;
  m_undo                 = new LevelMoverUndo();
  CellsMover *cellsMover = m_undo->getCellsMover();
  XsheetViewer *viewer   = getViewer();
  cellsMover->start(r0, c0, r1, c1, m_qualifiers, o);
  m_undo->m_columnsChanges.resize(c1 - c0 + 1, 0);

  m_moved = true;
  if (m_qualifiers & CellsMover::eCopyCells) {
    if (m_qualifiers & CellsMover::eInsertCells) {
      cellsMover->moveCells();
      m_moved = true;
    } else
      m_moved = false;
  }
  m_columnsMoved = false;

  int colCount = c1 - c0 + 1;
  int rowCount = r1 - r0 + 1;
  m_range.lx   = o->isVerticalTimeline() ? colCount : rowCount;
  m_range.ly   = o->isVerticalTimeline() ? rowCount : colCount;
  m_startPos.x = o->isVerticalTimeline() ? c0 : r0;
  m_startPos.y = o->isVerticalTimeline() ? r0 : c0;
  m_lastPos = m_aimedPos = m_startPos;
  m_grabOffset = m_startPos - (o->isVerticalTimeline() ? TPoint(col, row)
                                                       : TPoint(row, col));

  TXsheet *xsh = getViewer()->getXsheet();

  // move
  m_validPos                      = true;
  m_lastPos                       = m_startPos;
  m_undo->getCellsMover()->m_uffa = 0;

  if (!getViewer()->orientation()->isVerticalTimeline())
    TUndoManager::manager()->beginBlock();
}

void LevelMoverTool::onCellChange(int row, int col) {
  const Orientation *o = getViewer()->orientation();
  TXsheet *xsh         = getViewer()->getXsheet();

  TPoint pos = (o->isVerticalTimeline() ? TPoint(col, row) : TPoint(row, col)) +
               m_grabOffset;
  int origX   = pos.x;
  int origY   = pos.y;
  int currEnd = xsh->getColumnCount() - 1;

  if (pos.y < 0)
    pos.y = 0;
  else if (!o->isVerticalTimeline() && pos.y > currEnd)
    pos.y = currEnd;

  if (pos.x < 0) pos.x = 0;

  TPoint delta = pos - m_aimedPos;
  int dCol     = delta.x;
  if (!o->isVerticalTimeline()) dCol = delta.y;

  CellsMover *cellsMover = m_undo->getCellsMover();
  std::set<int> ii;
  TRect currSelection(m_aimedPos, m_range);

  if (!o->isVerticalTimeline()) {
    int newBegin = currSelection.y0 + dCol;
    int newEnd   = currSelection.y1 + dCol;

    if (newBegin < 0 ||
        (!getViewer()->orientation()->isVerticalTimeline() && newEnd > currEnd))
      return;
  }

  if (pos == m_aimedPos) return;

  m_dragged = true;

  m_aimedPos = pos;

  m_validPos = canMoveColumns(pos);
  if (!m_validPos) return;
  if (m_moved) {
    cellsMover->undoMoveCells();
  }
  m_validPos = canMove(pos);

  if (m_validPos) {
    //----------------------
    if (dCol != 0 && (m_qualifiers & CellsMover::eMoveColumns)) {
      // spostamento di colonne
      int colCount = m_range.lx;
      int colLast  = m_lastPos.x;
      int colPos   = pos.x;
      int colStart = m_startPos.x;
      if (!o->isVerticalTimeline()) {
        colCount = m_range.ly;
        colLast  = m_lastPos.y;
        colPos   = pos.y;
        colStart = m_startPos.y;
      }

      bool emptySrc = true;
      for (int i = 0; i < colCount; i++) {
        TXshColumn *column = xsh->getColumn(colLast + i);
        if (column && !column->isEmpty()) {
          emptySrc = false;
          break;
        }
      }

      bool lockedDst = false;
      bool emptyDst  = true;
      for (int i = 0; i < colCount; i++) {
        if (!isTotallyEmptyColumn(colPos + i)) emptyDst = false;
        TXshColumn *column = xsh->getColumn(colPos + i);
        if (column && column->isLocked()) lockedDst = true;
      }

      if (emptySrc) {
        if (colLast == colStart)
          m_undo->getCellsMover()->m_uffa |=
              1;  // first column(s) has/have been cleared
        cellsMover->emptyColumns(colLast);
      }

      if (emptyDst && !lockedDst) {
        if (colPos == colStart)
          m_undo->getCellsMover()->m_uffa &=
              ~1;  // first column(s) has/have been restored
        else
          m_undo->getCellsMover()->m_uffa |=
              2;  // columns data have been copied on the current position
        cellsMover->restoreColumns(colPos);
      } else {
        m_undo->getCellsMover()->m_uffa &=
            ~2;  // no columns data have been copied on the current position
      }
    }

    //----------------------

    m_lastPos = pos;
    cellsMover->setPos(pos.y, pos.x);
    cellsMover->saveCells();
    cellsMover->moveCells();
    m_moved = true;
  } else {
    cellsMover->moveCells();
  }

  xsh->updateFrameCount();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  if (cellsMover->getColumnTypeFromCell(0) == TXshColumn::eSoundType)
    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  TRect selection(m_aimedPos, m_range);
  if (o->isVerticalTimeline())
    getViewer()->getCellSelection()->selectCells(selection.y0, selection.x0,
                                                 selection.y1, selection.x1);
  else
    getViewer()->getCellSelection()->selectCells(selection.x0, selection.y0,
                                                 selection.x1, selection.y1);
}

void LevelMoverTool::onDrag(const QMouseEvent *e) {
  CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
  onCellChange(cellPosition.frame(), cellPosition.layer());
  refreshCellsArea();
  refreshColumnsArea();
}

void LevelMoverTool::onRelease(const CellPosition &pos) {
  m_validPos = false;
  m_range.lx = 0;
  m_range.ly = 0;
  setToolCursor(getViewer(), ToolCursor::CURSOR_ARROW);
  refreshCellsArea();
  CellsMover *cellMover = m_undo->getCellsMover();
  int startX            = cellMover->getStartPos().x;
  int startY            = cellMover->getStartPos().y;
  int posX              = cellMover->getPos().x;
  int posY              = cellMover->getPos().y;
  bool copyInserted     = m_qualifiers & CellsMover::eCopyCells &&
                      m_qualifiers & CellsMover::eInsertCells;
  if (m_lastPos != m_startPos || copyInserted)
    TUndoManager::manager()->add(m_undo);
  m_undo = 0;

  if (!getViewer()->orientation()->isVerticalTimeline())
    TUndoManager::manager()->endBlock();

  // Reset current selection if we didn't move
  TCellSelection *selection = getViewer()->getCellSelection();
  if (!Preferences::instance()->isShowDragBarsEnabled() && !m_dragged &&
      (cellMover->getRowCount() > 1 || cellMover->getColumnCount() > 1)) {
    selection->selectNone();
    selection->selectCell(pos.frame(), pos.layer());
    getViewer()->setCurrentRow(pos.frame());
    getViewer()->setCurrentColumn(pos.layer());
  }
}

void LevelMoverTool::drawCellsArea(QPainter &p) {
  const Orientation *o = getViewer()->orientation();
  TRect rect(m_aimedPos, m_range);
  // voglio visualizzare il rettangolo nella posizione desiderata
  // che potrebbe anche non essere possibile, ad esempio perche' fuori
  // dall'xsheet quindi devo fare un clipping. Il rettangolo potrebbe
  // anche essere interamente fuori
  if (rect.x1 < 0 || rect.y1 < 0) return;
  if (rect.x0 < 0) rect.x0 = 0;
  if (rect.y0 < 0) rect.y0 = 0;

  QRect screen;
  if (o->isVerticalTimeline())
    screen = getViewer()->rangeToXYRect(
        CellRange(CellPosition(rect.y0, rect.x0),
                  CellPosition(rect.y1 + 1, rect.x1 + 1)));
  else {
    int newY0 = std::max(rect.y0, rect.y1);
    int newY1 = std::min(rect.y0, rect.y1);
    screen    = getViewer()->rangeToXYRect(CellRange(
        CellPosition(rect.x0, newY0), CellPosition(rect.x1 + 1, newY1 - 1)));
  }
  p.setPen((m_aimedPos == m_lastPos && m_validPos) ? QColor(190, 220, 255)
                                                   : QColor(255, 0, 0));
  int i;
  for (i = 0; i < 3; i++)  // thick border inside cell
    p.drawRect(QRect(screen.topLeft() + QPoint(i, i),
                     screen.size() - QSize(2 * i, 2 * i)));
}
