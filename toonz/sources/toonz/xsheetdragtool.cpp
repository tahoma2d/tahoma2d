

#include "xsheetdragtool.h"

// Tnz6 includes
#include "xsheetviewer.h"
#include "cellselection.h"
#include "columncommand.h"
#include "keyframeselection.h"
#include "cellkeyframeselection.h"
#include "tapp.h"
#include "xshcolumnviewer.h"
#include "columnselection.h"
#include "filmstripselection.h"
#include "castselection.h"
#include "iocommand.h"
#include "keyframemover.h"
#include "xshcellmover.h"

// TnzTools includes
#include "tools/cursors.h"
#include "tools/cursormanager.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "historytypes.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcolumn.h"
#include "toonz/txshlevelcolumn.h"
#include "toonz/tcolumnfxset.h"
#include "toonz/txshcell.h"
#include "toonz/fxdag.h"
#include "toonz/sceneproperties.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectkeyframe.h"
#include "toonz/onionskinmask.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshnoteset.h"
#include "toutputproperties.h"
#include "toonz/preferences.h"
#include "toonz/columnfan.h"
#include "toonz/navigationtags.h"
#include "toonz/txshfoldercolumn.h"

// TnzBase includes
#include "tfx.h"

// TnzCore includes
#include "tundo.h"

// Qt includes
#include <QPainter>
#include <QMouseEvent>
#include <QUrl>
#include <QDropEvent>

//=============================================================================
// XsheetGUI DragTool
//-----------------------------------------------------------------------------

XsheetGUI::DragTool::DragTool(XsheetViewer *viewer) : m_viewer(viewer) {}

//-----------------------------------------------------------------------------

XsheetGUI::DragTool::~DragTool() {}

//-----------------------------------------------------------------------------

void XsheetGUI::DragTool::refreshCellsArea() { getViewer()->updateCells(); }

//-----------------------------------------------------------------------------

void XsheetGUI::DragTool::refreshColumnsArea() { getViewer()->updateColumns(); }

//-----------------------------------------------------------------------------

void XsheetGUI::DragTool::refreshRowsArea() { getViewer()->updateRows(); }

//-----------------------------------------------------------------------------

void XsheetGUI::DragTool::onClick(const QMouseEvent *event) {
  QPoint xy        = event->pos();
  CellPosition pos = getViewer()->xyToPosition(xy);
  onClick(pos);
}

//-----------------------------------------------------------------------------

void XsheetGUI::DragTool::onDrag(const QMouseEvent *event) {
  QPoint xy        = event->pos();
  CellPosition pos = getViewer()->xyToPosition(xy);
  onDrag(pos);
}

//-----------------------------------------------------------------------------

void XsheetGUI::DragTool::onRelease(const QMouseEvent *event) {
  QPoint xy        = event->pos();
  CellPosition pos = getViewer()->xyToPosition(xy);
  onRelease(pos);
}

//=============================================================================
// XsheetSelection DragTool
//-----------------------------------------------------------------------------

class XsheetSelectionDragTool final : public XsheetGUI::DragTool {
  int m_firstRow, m_firstCol;
  Qt::KeyboardModifiers m_modifier;
  bool m_keySelection;

public:
  XsheetSelectionDragTool(XsheetViewer *viewer)
      : DragTool(viewer)
      , m_firstRow(0)
      , m_firstCol(0)
      , m_modifier()
      , m_keySelection(false) {}
  // activate when clicked the cell
  void onClick(const QMouseEvent *event) override {
    m_modifier       = event->modifiers();
    CellPosition pos = getViewer()->xyToPosition(event->pos());
    int row          = pos.frame();
    int col          = pos.layer();
    m_firstCol       = col;
    m_firstRow       = row;

    m_keySelection = Preferences::instance()->isShowDragBarsEnabled()
                         ? m_modifier & Qt::ControlModifier
                         : m_modifier & Qt::AltModifier;

    int r0, c0, r1, c1;
    bool shiftPressed = false;
    if (m_modifier & Qt::ShiftModifier) {
      shiftPressed = true;
      getViewer()->getCellSelection()->getSelectedCells(r0, c0, r1, c1);
    }

    // First, check switching of the selection types. This may clear the
    // previous selection.
    if (m_keySelection)
      getViewer()->getCellKeyframeSelection()->makeCurrent();
    else
      getViewer()->getCellSelection()->makeCurrent();
    if (shiftPressed) {
      if (r0 <= r1 && c0 <= c1) {
        if (abs(row - r0) < abs(row - r1)) {
          m_firstRow = r1;
          r0         = row;
        } else {
          m_firstRow = r0;
          r1         = row;
        }
        if (abs(col - c0) < abs(col - c1)) {
          m_firstCol = c1;
          c0         = col;
        } else {
          m_firstCol = c0;
          c1         = col;
        }
        if (m_keySelection)
          getViewer()->getCellKeyframeSelection()->selectCellsKeyframes(r0, c0,
                                                                        r1, c1);
        else
          getViewer()->getCellSelection()->selectCells(r0, c0, r1, c1);
      } else {
        if (m_keySelection)
          getViewer()->getCellKeyframeSelection()->selectCellsKeyframes(
              row, col, row, col);
        else
          getViewer()->getCellSelection()->selectCells(row, col, row, col);
        getViewer()->setCurrentColumn(col);
        if (Preferences::instance()->isMoveCurrentEnabled())
          getViewer()->setCurrentRow(row);
      }
    } else {
      getViewer()->setCurrentColumn(col);
      if (Preferences::instance()->isMoveCurrentEnabled())
        getViewer()->setCurrentRow(row);
      if (m_keySelection)
        getViewer()->getCellKeyframeSelection()->selectCellKeyframe(row, col);
      else
        getViewer()->getCellSelection()->selectCell(row, col);
    }
    refreshCellsArea();
    refreshRowsArea();
  }
  void onDrag(const CellPosition &pos) override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int row = pos.frame(), col = pos.layer();
    int firstCol =
        Preferences::instance()->isXsheetCameraColumnVisible() ? -1 : 0;
    if (col < firstCol || (!getViewer()->orientation()->isVerticalTimeline() &&
                           col >= xsh->getColumnCount()))
      return;
    if (row < 0) row = 0;
    if (m_keySelection)
      getViewer()->getCellKeyframeSelection()->selectCellsKeyframes(
          m_firstRow, m_firstCol, row, col);
    else
      getViewer()->getCellSelection()->selectCells(m_firstRow, m_firstCol, row,
                                                   col);
    refreshCellsArea();
    refreshRowsArea();
  }
  void onRelease(const QMouseEvent *event) override {
    TApp::instance()->getCurrentSelection()->notifySelectionChanged();
    refreshRowsArea();
  }
};

//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeSelectionTool(
    XsheetViewer *viewer) {
  return new XsheetSelectionDragTool(viewer);
}

//-----------------------------------------------------------------------------
namespace {

class UndoPlayRangeModifier final : public TUndo {
  int m_oldR0, m_oldR1, m_newR0, m_newR1, m_newStep, m_oldStep;

public:
  UndoPlayRangeModifier(int oldR0, int newR0, int oldR1, int newR1, int oldStep,
                        int newStep, bool oldEnabled, bool newEnabled)
      : m_oldR0(oldR0)
      , m_newR0(newR0)
      , m_oldR1(oldR1)
      , m_newR1(newR1)
      , m_oldStep(oldStep)
      , m_newStep(newStep) {}

  void undo() const override {
    XsheetGUI::setPlayRange(m_oldR0, m_oldR1, m_oldStep, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    XsheetGUI::setPlayRange(m_newR0, m_newR1, m_newStep, false);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    if (m_oldR0 < 0 || m_oldR1 < 0)
      return QObject::tr("Modify Play Range  : %1 - %2")
          .arg(QString::number(m_newR0 + 1))
          .arg(QString::number(m_newR1 + 1));

    return QObject::tr("Modify Play Range  : %1 - %2  >  %3 - %4")
        .arg(QString::number(m_oldR0 + 1))
        .arg(QString::number(m_oldR1 + 1))
        .arg(QString::number(m_newR0 + 1))
        .arg(QString::number(m_newR1 + 1));
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

//-----------------------------------------------------------------------------

void XsheetGUI::setPlayRange(int r0, int r1, int step, bool withUndo) {
  if (r0 > r1) {
    r0 = 0;
    r1 = -1;
  }
  if (withUndo) {
    int oldR0, oldR1, oldStep;
    XsheetGUI::getPlayRange(oldR0, oldR1, oldStep);
    TUndoManager::manager()->add(new UndoPlayRangeModifier(
        oldR0, r0, oldR1, r1, oldStep, step, true, true));
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
  }
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->getProperties()->getPreviewProperties()->setRange(r0, r1, step);
  TApp::instance()->getCurrentScene()->notifySceneChanged();
}

//-----------------------------------------------------------------------------

bool XsheetGUI::getPlayRange(int &r0, int &r1, int &step) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  scene->getProperties()->getPreviewProperties()->getRange(r0, r1, step);
  if (r0 > r1) {
    r0 = -1;
    r1 = -2;
    return false;
  } else
    return true;
}

//-----------------------------------------------------------------------------

bool XsheetGUI::isPlayRangeEnabled() {
  int playR0, playR1, step;
  XsheetGUI::getPlayRange(playR0, playR1, step);
  return playR0 <= playR1;
}

//=============================================================================
// LevelMover tool
//-----------------------------------------------------------------------------
namespace {

//=============================================================================
// CellMovementUndo
//-----------------------------------------------------------------------------

}  // namespace

//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeLevelMoverTool(
    XsheetViewer *viewer) {
  return new LevelMoverTool(viewer);
}

//=============================================================================
// LevelExtender DragTool
//-----------------------------------------------------------------------------

namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// LevelExtenderUndo
//-----------------------------------------------------------------------------

class LevelExtenderUndo final : public TUndo {
  int m_colCount;
  int m_rowCount;
  int m_col, m_row, m_deltaRow;
  std::vector<TXshCell> m_cells;  // righe x colonne

  bool m_insert;
  bool m_invert;  // upper-directional

  bool m_refreshSound;

public:
  LevelExtenderUndo(bool insert = true, bool invert = false,
                    bool refreshSound = false)
      : m_colCount(0)
      , m_rowCount(0)
      , m_col(0)
      , m_row(0)
      , m_deltaRow(0)
      , m_insert(insert)
      , m_invert(invert)
      , m_refreshSound(refreshSound) {}

  void setCells(TXsheet *xsh, int row, int col, int rowCount, int colCount) {
    assert(rowCount > 0 && colCount > 0);
    m_row      = row;
    m_col      = col;
    m_rowCount = rowCount;
    m_colCount = colCount;
    m_cells.resize(rowCount * colCount, TXshCell());
    int k = 0;
    for (int r = row; r < row + rowCount; r++)
      for (int c = col; c < col + colCount; c++)
        m_cells[k++] = xsh->getCell(r, c);
  }

  void setDeltaRow(int drow) {
    assert(drow != 0);
    m_deltaRow = drow;
  }

  void removeCells() const {
    assert(m_deltaRow != 0);
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int count    = abs(m_deltaRow);
    int r        = m_row + m_rowCount - count;
    for (int c = m_col; c < m_col + m_colCount; c++) {
      TXshColumn *column = xsh->getColumn(c);
      xsh->removeCells(r, c, count);
    }
  }

  void insertCells() const {
    assert(m_deltaRow != 0);
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int count    = abs(m_deltaRow);
    int r0       = m_row + m_rowCount - count;
    int r1       = m_row + m_rowCount - 1;
    for (int c = 0; c < m_colCount; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (column && column->getFolderColumn()) continue;
      bool isSoundColumn     = (column && column->getSoundColumn());
      bool isSoundTextColumn = (column && column->getSoundTextColumn());
      int col                = m_col + c;
      xsh->insertCells(r0, col, count);
      int r;
      TXshCell prevCell = xsh->getCell(r0, c);
      for (r = r0; r <= r1; r++) {
        int k         = (r - m_row) * m_colCount + c;
        TXshCell cell = m_cells[k];
        if (!isSoundTextColumn &&
            (isSoundColumn ||
             (Preferences::instance()->isImplicitHoldEnabled() &&
              prevCell == cell)))
          xsh->setCell(r, col, TXshCell());
        else {
          xsh->setCell(r, col, cell);
          prevCell = cell;
        }
      }
    }
  }

  // for upper-directional smart tab
  // also used for non-insert(overwriting) extension
  void clearCells() const {
    assert(m_deltaRow != 0);
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int count    = abs(m_deltaRow);
    for (int c = m_col; c < m_col + m_colCount; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (m_invert)
        xsh->clearCells(m_row, c, count);
      else
        xsh->clearCells(m_row + m_rowCount - count, c, count);
    }
  }

  // for upper-directional smart tab
  // also used for non-insert(overwriting) extension
  void setCells() const {
    assert(m_deltaRow != 0);
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int count    = abs(m_deltaRow);

    int r0, r1;
    if (m_invert) {
      r0 = m_row;
      r1 = m_row + count - 1;
    } else {
      r0 = m_row + m_rowCount - count - 1;
      r1 = m_row + m_rowCount - 1;
    }
    for (int c = 0; c < m_colCount; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (column && column->getFolderColumn()) continue;
      bool isSoundColumn = (column && column->getSoundColumn());
      int col = m_col + c;
      for (int r = r0; r <= r1; r++) {
        int k = (r - m_row) * m_colCount + c;
        if (isSoundColumn)
          xsh->setCell(r, col, TXshCell());
        else
          xsh->setCell(r, col, m_cells[k]);
      }
    }
  }

  void undo() const override {
    // undo for shrinking operation -> revert cells
    if (m_deltaRow < 0) (m_insert) ? insertCells() : setCells();
    // undo for stretching operation -> remove cells
    else if (m_deltaRow > 0)
      (m_insert) ? removeCells() : clearCells();
    else {
      assert(0);
    }
    TSelection *selection =
        TApp::instance()->getCurrentSelection()->getSelection();
    if (selection) selection->selectNone();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_refreshSound)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  void redo() const override {
    // redo for shrinking operation -> remove cells
    if (m_deltaRow < 0) (m_insert) ? removeCells() : clearCells();
    // redo for stretching operation -> revert cells
    else if (m_deltaRow > 0)
      (m_insert) ? insertCells() : setCells();
    else {
      assert(0);
    }
    TSelection *selection =
        TApp::instance()->getCurrentSelection()->getSelection();
    if (selection) selection->selectNone();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    if (m_refreshSound)
      TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  int getSize() const override {
    return sizeof(*this) + sizeof(TXshCell) * m_cells.size();
  }

  QString getHistoryString() override {
    return QObject::tr("Use Level Extender");
  }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
// CellBuilder
//-----------------------------------------------------------------------------

class CellBuilder {
  std::vector<TXshCell> m_sourceCells;
  bool m_sequenceFound;
  int m_base, m_inc, m_step, m_offset, m_count;
  int m_firstRow;

  // upper-directional smart tab
  bool m_invert;

public:
  // CellBuilder is constructed when the smart tab is clicked.
  // row : top row in the selection,   col : current column,  rowCount :
  // selected row count
  CellBuilder(TXsheet *xsh, int row, int col, int rowCount, int invert = false)
      : m_firstRow(row)
      , m_sequenceFound(false)
      , m_base(0)
      , m_inc(0)
      , m_step(0)
      , m_offset(0)
      , m_count(0)
      , m_sourceCells(rowCount, TXshCell())
      , m_invert(invert) {
    if (!m_invert)
      xsh->getCells(row, col, rowCount, &m_sourceCells[0]);
    else {
      std::vector<TXshCell> cells(rowCount, TXshCell());
      xsh->getCells(row, col, rowCount, &cells[0]);
      for (int r = 0; r < rowCount; r++)
        m_sourceCells[r] = cells[rowCount - 1 - r];
      // the "first row" becomes the bottom-most row for the upper-directional
      // smart tab
      m_firstRow = row + rowCount - 1;
    }
    analyzeCells();
  }
  CellBuilder()
      : m_firstRow(0)
      , m_sequenceFound(false)
      , m_base(0)
      , m_inc(0)
      , m_step(0)
      , m_offset(0)
      , m_count(0)
      , m_sourceCells() {}

  TFrameId computeFrame(int i) const {
    assert(i >= 0);
    int k = i + m_offset;
    // if there is a large cycle
    if (m_count > 0) k = k % m_count;
    k = m_base + (k / m_step) * m_inc;
    if (m_inc < 0)
      while (k < 1) k += m_base;
    return TFrameId(k);
  }

  void analyzeCells() {
    // se e' vuoto non c'e' sequenza
    if (m_sourceCells.empty()) return;
    TXshCell cell = m_sourceCells[0];
    if (!cell.m_level) return;

    // controllo che tutte le celle appartengano allo stesso livello
    // e che i frameId non abbiano suffissi (es. 0012b)
    // there is no rule if there are different levels in the selection or
    // letters in the frame numbers
    int count = m_sourceCells.size();
    int i;
    for (i = 1; i < count; i++)
      if (m_sourceCells[i].m_level != cell.m_level ||
          !m_sourceCells[i].m_frameId.getLetter().isEmpty())
        return;

    // check if all the selected cells have the same frame number
    // 'm_base' e' il primo frame number
    m_base = cell.m_frameId.getNumber();
    // cerco il primo cambiamento di frame
    for (i = 1; i < count; i++)
      if (m_sourceCells[i].m_frameId != cell.m_frameId) break;

    // there is no rule if all the selected cells are the same.
    // se sono tutti uguali la sequenza e' banale (e la tratto come
    // se non ci fosse)
    if (i == count) return;

    // check if there is a incremental rule.
    // 'm_inc' e' l'incremento di frame number
    m_inc = m_sourceCells[i].m_frameId.getNumber() - m_base;
    assert(m_inc != 0);
    // 'm_step' e' il numero di volte che viene ripetuto un frame
    m_step = i;
    // se 'm_offset'>0 vuol dire che m_step>1 e la selezione e' partita m_offset
    // celle dopo un cambio di frame
    m_offset = 0;
    if (m_step > 1) {
      //先頭と異なるセルを仮にループ終端とする
      TFrameId fid(m_sourceCells[m_step].m_frameId);
      //次にループ終端と異なるセルがくるまでセルを送る
      for (i++; i < count && m_sourceCells[i].m_frameId == fid; i++) {
      }
      //次のループ終端候補が見つかった
      int step = i - m_step;
      //ループ距離が縮んでいたら規則なし
      if (step < m_step) return;
      m_offset = step - m_step;
      m_step   = step;
    }

    // la sequenza potrebbe essere ciclica. cerco di trovare
    // l'ultimo elemento
    for (; i < count; i++)
      if (m_sourceCells[i].m_frameId != computeFrame(i)) break;
    // check if there are the double loops
    if (i < count) {
      TFrameId first(m_base);
      if (m_sourceCells[i].m_frameId != first) return;
      if (m_step > 1 && ((i + m_offset) % m_step) != 0) return;
      m_count = i + m_offset;

      // controllo che le celle restanti verifichino la sequenza
      for (; i < count; i++)
        if (m_sourceCells[i].m_frameId != computeFrame(i)) return;
    }

    // eureka!
    m_sequenceFound = true;
  }

  TXshCell generate(int row) const {
    int i = row - m_firstRow;

    if (m_invert) i = -i;

    if (i < 0 || m_sourceCells.empty())
      return TXshCell();
    else if (i < (int)m_sourceCells.size())
      return m_sourceCells[i];
    else if (m_sequenceFound) {
      TXshCell cell;
      cell.m_level   = m_sourceCells[0].m_level;
      cell.m_frameId = computeFrame(i);
      return cell;
    } else {
      i = i % m_sourceCells.size();
      return m_sourceCells[i];
    }
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

//=============================================================================
// LevelExtenderTool
//-----------------------------------------------------------------------------

class LevelExtenderTool final : public XsheetGUI::DragTool {
  int m_colCount;
  int m_rowCount;
  int m_c0, m_r0, m_r1;
  std::vector<CellBuilder> m_columns;
  LevelExtenderUndo *m_undo;

  bool m_invert;  // upper directional smart tab
  bool m_insert;

  bool m_refreshSound;

public:
  LevelExtenderTool(XsheetViewer *viewer, bool insert = true,
                    bool invert = false, bool refreshSound = false)
      : XsheetGUI::DragTool(viewer)
      , m_colCount(0)
      , m_undo(0)
      , m_insert(insert)
      , m_invert(invert)
      , m_refreshSound(refreshSound) {}

  // called when the smart tab is clicked
  void onClick(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    int r0, c0, r1, c1;
    getViewer()->getCellSelection()->getSelectedCells(r0, c0, r1, c1);
    if (m_invert)
      assert(r0 == row + 1);
    else
      assert(r1 == row - 1);
    m_c0       = c0;
    m_r0       = r0;
    m_r1       = r1;
    m_colCount = c1 - c0 + 1;
    m_rowCount = r1 - r0 + 1;
    if (m_colCount <= 0 || m_rowCount <= 0) return;

    // if m_insert is false but there are no empty rows under the tab,
    // then switch m_insert to true so that the operation works anyway
    if (!m_insert && !m_invert) {
      TXsheet *xsh = getViewer()->getXsheet();
      for (int c = c0; c <= c1; c++) {
        TXshColumn *column = xsh->getColumn(c);
        if (!column) continue;
        if (!column->isCellEmpty(r1 + 1)) {
          m_insert = true;  // switch the behavior
          break;
        }
      }
    }

    m_columns.reserve(m_colCount);
    TXsheet *xsh = getViewer()->getXsheet();
    for (int c = c0; c <= c1; c++) {
      TXshColumn *column = xsh->getColumn(c);
      if (column && column->getSoundColumn()) m_refreshSound = true;
      m_columns.push_back(CellBuilder(xsh, r0, c, m_rowCount, m_invert));
    }
    m_undo = new LevelExtenderUndo(m_insert, m_invert, m_refreshSound);
    m_undo->setCells(xsh, r0, c0, m_rowCount, m_colCount);
  }

  void onDrag(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    if (!m_invert)
      onCellChange(row, col);
    else
      onCellChangeInvert(row, col);
    refreshCellsArea();
  }

  void onCellChange(int row, int col) {
    if (m_colCount <= 0 || m_rowCount <= 0) return;
    if (row <= m_r0) row = m_r0 + 1;
    int r1 = row - 1;  // r1 e' la riga inferiore della nuova selezione
    if (r1 < m_r0) r1 = m_r0;
    int dr = r1 - m_r1;
    if (dr == 0) return;
    TXsheet *xsh = getViewer()->getXsheet();
    // shrink
    if (dr < 0) {
      for (int c = 0; c < m_colCount; c++) {
        TXshColumn *column = xsh->getColumn(m_c0 + c);
        if (m_insert)
          xsh->removeCells(row, m_c0 + c, -dr);
        else {
          for (int r = row; r <= m_r1; r++)
            xsh->setCell(r, m_c0 + c, TXshCell());
        }
      }
    }
    // extend
    else {
      // check how many vacant rows
      if (!m_insert) {
        int tmp_dr;
        bool found = false;
        for (tmp_dr = 1; tmp_dr <= dr; tmp_dr++) {
          for (int c = 0; c < m_colCount; c++) {
            TXshColumn *column = xsh->getColumn(m_c0 + c);
            if (!column) continue;
            if (!column->isCellEmpty(m_r1 + tmp_dr)) {
              found = true;
              break;
            }
          }
          if (found) break;
        }
        if (tmp_dr == 1) return;
        dr = tmp_dr - 1;
        r1 = m_r1 + dr;
      }

      for (int c = 0; c < m_colCount; c++) {
        TXshColumn *column = xsh->getColumn(m_c0 + c);
        if (column && column->getFolderColumn()) continue;
        bool isSoundColumn     = (column && column->getSoundColumn());
        bool isSountTextColumn = (column && column->getSoundTextColumn());
        if (m_insert) xsh->insertCells(m_r1 + 1, m_c0 + c, dr);
        TXshCell prevCell = xsh->getCell(m_r1, m_c0 + c);
        for (int r = m_r1 + 1; r <= r1; r++) {
          TXshCell cell = m_columns[c].generate(r);
          if (!isSountTextColumn &&
              (isSoundColumn ||
               (Preferences::instance()->isImplicitHoldEnabled() &&
                prevCell == cell)))
            xsh->setCell(r, m_c0 + c, TXshCell());
          else {
            xsh->setCell(r, m_c0 + c, cell);
            prevCell = cell;
          }
        }
      }
    }
    m_r1 = r1;
    getViewer()->getCellSelection()->selectCells(m_r0, m_c0, m_r1,
                                                 m_c0 + m_colCount - 1);
  }

  // for upper-directinoal smart tab
  void onCellChangeInvert(int row, int col) {
    if (m_colCount <= 0 || m_rowCount <= 0) return;
    if (row >= m_r1) row = m_r1 - 1;
    int r0 = row + 1;
    if (r0 > m_r1) r0 = m_r1;

    if (r0 < 0) r0 = 0;

    TXsheet *xsh = getViewer()->getXsheet();
    // check how many vacant rows
    int emptyRow;
    for (emptyRow = m_r0 - 1; emptyRow >= 0; emptyRow--) {
      bool found = false;
      for (int c = 0; c < m_colCount; c++) {
        TXshColumn *column = xsh->getColumn(m_c0 + c);
        if (!column) continue;
        if (!column->isCellEmpty(emptyRow)) {
          emptyRow += 1;
          found = true;
          break;
        }
      }
      if (found) break;
    }

    // upper-directional tab can extend cells only in the mpty area
    if (r0 < emptyRow) r0 = emptyRow;

    int dr = r0 - m_r0;
    if (dr == 0) return;

    // shrink
    if (dr > 0) {
      // clear cells
      for (int c = 0; c < m_colCount; c++) {
        TXshColumn *column = xsh->getColumn(m_c0 + c);
        if (!column) continue;
        xsh->clearCells(m_r0, m_c0 + c, dr);
      }
    }
    // extend
    else {
      for (int c = 0; c < m_colCount; c++) {
        TXshColumn *column = xsh->getColumn(m_c0 + c);
        if (column && column->getFolderColumn()) continue;
        bool isSoundColumn = (column && column->getSoundColumn());
        for (int r = r0; r <= m_r0 - 1; r++) {
          if (isSoundColumn)
            xsh->setCell(r, m_c0 + c, TXshCell());
          else
            xsh->setCell(r, m_c0 + c, m_columns[c].generate(r));
        }
      }
    }
    m_r0 = r0;
    getViewer()->getCellSelection()->selectCells(m_r0, m_c0, m_r1,
                                                 m_c0 + m_colCount - 1);
  }

  void onRelease(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    int delta = m_r1 - (m_r0 + m_rowCount - 1);
    if (delta == 0)
      delete m_undo;
    else {
      if (delta > 0)
        m_undo->setCells(getViewer()->getXsheet(), m_r0, m_c0, m_r1 - m_r0 + 1,
                         m_colCount);
      m_undo->setDeltaRow(delta);
      TUndoManager::manager()->add(m_undo);
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      if (m_refreshSound)
        TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
    }
    m_undo = 0;
  }
};

//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeLevelExtenderTool(
    XsheetViewer *viewer, bool insert, bool invert) {
  return new LevelExtenderTool(viewer, insert, invert);
}

//=============================================================================
// LevelExtender DragTool
//-----------------------------------------------------------------------------

namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// SoundLevelModifierUndo
//-----------------------------------------------------------------------------

class SoundLevelModifierUndo final : public TUndo {
  int m_col;
  TXshSoundColumnP m_newSoundColumn;
  TXshSoundColumnP m_oldSoundColumn;

  TXsheetHandle *m_xshHandle;

public:
  SoundLevelModifierUndo(int col, TXshSoundColumn *oldSoundColumn)
      : m_col(col), m_newSoundColumn(), m_oldSoundColumn(oldSoundColumn) {
    m_xshHandle = TApp::instance()->getCurrentXsheet();
  }

  void setNewColumn(TXshSoundColumn *newSoundcolumn) {
    m_newSoundColumn = dynamic_cast<TXshSoundColumn *>(newSoundcolumn->clone());
  }

  TXshSoundColumn *getColumn() const {
    TXsheet *xsh       = m_xshHandle->getXsheet();
    TXshColumn *column = xsh->getColumn(m_col);
    assert(column);
    // La colonna sound deve esserci, metto un controllo di sicurezza.
    TXshSoundColumn *soundColumn = column->getSoundColumn();
    assert(soundColumn);
    return soundColumn;
  }

  void undo() const override {
    TXshSoundColumn *soundColumn = getColumn();
    if (!soundColumn) return;
    soundColumn->assignLevels(m_oldSoundColumn.getPointer());
    m_xshHandle->notifyXsheetChanged();
  }

  void redo() const override {
    TXshSoundColumn *soundColumn = getColumn();
    if (!soundColumn) return;
    soundColumn->assignLevels(m_newSoundColumn.getPointer());
    m_xshHandle->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override {
    return QObject::tr("Modify Sound Level");
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

}  // namespace

//=============================================================================
// SoundLevelModifierTool
//-----------------------------------------------------------------------------

class SoundLevelModifierTool final : public XsheetGUI::DragTool {
  int m_col;
  int m_firstRow;
  TXshSoundColumnP m_oldColumn;
  SoundLevelModifierUndo *m_undo;

  enum ModifierType {
    UNKNOWN_TYPE = 0,
    START_TYPE   = 1,
    END_TYPE     = 2,
  } m_modifierType;

public:
  SoundLevelModifierTool(XsheetViewer *viewer)
      : XsheetGUI::DragTool(viewer)
      , m_col(-1)
      , m_firstRow(-1)
      , m_oldColumn()
      , m_modifierType(UNKNOWN_TYPE)
      , m_undo(0) {}

  ModifierType getType(int r0, int r1, int delta) {
    if (m_firstRow == r0 && m_firstRow == r1) {
      if (delta == 0)
        return UNKNOWN_TYPE;
      else if (delta < 0)
        return START_TYPE;
      else
        return END_TYPE;
    }
    if (m_firstRow == r0)
      return START_TYPE;
    else if (m_firstRow == r1)
      return END_TYPE;
    return UNKNOWN_TYPE;
  }

  ~SoundLevelModifierTool() { delete m_undo; }

  TXshSoundColumn *getColumn() const {
    TXshColumn *column = getViewer()->getXsheet()->getColumn(m_col);
    assert(column);
    // La colonna sound deve esserci, metto un controllo di sicurezza.
    TXshSoundColumn *soundColumn = column->getSoundColumn();
    assert(soundColumn);
    return soundColumn;
  }

  void onClick(const CellPosition &pos) override {
    m_firstRow                   = pos.frame();
    m_col                        = pos.layer();
    TXshSoundColumn *soundColumn = getColumn();
    if (!soundColumn) return;
    m_oldColumn = dynamic_cast<TXshSoundColumn *>(soundColumn->clone());
    m_undo      = new SoundLevelModifierUndo(m_col, m_oldColumn.getPointer());
    getViewer()->update();
  }

  void onDrag(const CellPosition &pos) override {
    onChange(pos.frame());
    refreshCellsArea();
  }

  void onChange(int row) {
    TXshSoundColumn *soundColumn = getColumn();
    if (!soundColumn) return;

    int delta = row - m_firstRow;
    if (delta == 0) {
      soundColumn->assignLevels(m_oldColumn.getPointer());
      return;
    }
    int r0, r1;
    m_oldColumn->getLevelRange(m_firstRow, r0, r1);
    ModifierType type = getType(r0, r1, delta);
    if (m_modifierType == UNKNOWN_TYPE && type != UNKNOWN_TYPE)
      m_modifierType = type;
    else if (m_modifierType != type || type == UNKNOWN_TYPE)
      return;

    soundColumn->assignLevels(m_oldColumn.getPointer());
    soundColumn->modifyCellRange(m_firstRow, delta,
                                 m_modifierType == START_TYPE);

    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }

  void onRelease(const CellPosition &pos) override {
    int row = pos.frame();
    if (row - m_firstRow == 0) {
      m_undo = 0;
      return;
    }

    TXshColumn *column           = getViewer()->getXsheet()->getColumn(m_col);
    TXshSoundColumn *soundColumn = getColumn();
    if (!soundColumn) return;

    soundColumn->updateCells(row, 1);

    m_undo->setNewColumn(soundColumn);
    TUndoManager::manager()->add(m_undo);
    m_undo = 0;
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
};

//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeSoundLevelModifierTool(
    XsheetViewer *viewer) {
  return new SoundLevelModifierTool(viewer);
}

//=============================================================================
// KeyFrame Mover DragTool
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeKeyframeMoverTool(
    XsheetViewer *viewer) {
  return new KeyframeMoverTool(viewer);
}

//=============================================================================
// Cell KeyFrame Mover DragTool
//-----------------------------------------------------------------------------

class CellKeyframeMoverTool final : public LevelMoverTool {
  KeyframeMoverTool *m_keyframeMoverTool;

protected:
  bool canMove(const TPoint &pos) override {
    if (m_keyframeMoverTool->hasSelection() &&
        !m_keyframeMoverTool->canMove(pos))
      return false;
    return LevelMoverTool::canMove(pos);
  }

public:
  CellKeyframeMoverTool(XsheetViewer *viewer) : LevelMoverTool(viewer) {
    m_keyframeMoverTool = new KeyframeMoverTool(viewer, true);
  }

  void onClick(const QMouseEvent *e) override {
    LevelMoverTool::onClick(e);
    m_keyframeMoverTool->onClick(e);
  }

  void onDrag(const QMouseEvent *e) override {
    LevelMoverTool::onDrag(e);
    if (m_validPos) m_keyframeMoverTool->onDrag(e);
  }
  void onRelease(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    TUndoManager::manager()->beginBlock();
    LevelMoverTool::onRelease(pos);
    m_keyframeMoverTool->onRelease(pos);
    TUndoManager::manager()->endBlock();
  }

  void drawCellsArea(QPainter &p) override {
    LevelMoverTool::drawCellsArea(p);
    m_keyframeMoverTool->drawCellsArea(p);
  }
};

//=============================================================================

XsheetGUI::DragTool *XsheetGUI::DragTool::makeCellKeyframeMoverTool(
    XsheetViewer *viewer) {
  return new CellKeyframeMoverTool(viewer);
}

//=============================================================================
// KeyFrame Handle Mover DragTool
//-----------------------------------------------------------------------------

namespace {
//-----------------------------------------------------------------------------

//=============================================================================
// KeyFrameHandleUndo
//-----------------------------------------------------------------------------

class KeyFrameHandleUndo final : public TUndo {
  TStageObjectId m_objId;
  int m_row;
  TStageObject::Keyframe m_oldKeyframe, m_newKeyframe;

public:
  KeyFrameHandleUndo(TStageObjectId id, int row) : m_objId(id), m_row(row) {
    TStageObject *pegbar =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getStageObject(
            m_objId);
    assert(pegbar);
    m_oldKeyframe = pegbar->getKeyframe(m_row);
  }

  void onAdd() override {
    TStageObject *pegbar =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getStageObject(
            m_objId);
    assert(pegbar);
    m_newKeyframe = pegbar->getKeyframe(m_row);
  }

  void setKeyframe(const TStageObject::Keyframe &k) const {
    TStageObject *pegbar =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getStageObject(
            m_objId);
    assert(pegbar);
    pegbar->setKeyframeWithoutUndo(m_row, k);
    TApp::instance()->getCurrentObject()->notifyObjectIdChanged(false);
  }

  void undo() const override { setKeyframe(m_oldKeyframe); }
  void redo() const override { setKeyframe(m_newKeyframe); }
  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Move keyframe handle  : %1  Handle of the keyframe %2")
        .arg(QString::fromStdString(m_objId.toString()))
        .arg(QString::number(m_row + 1));
  }

  int getHistoryType() override { return HistoryType::Xsheet; }
};

//=============================================================================
// KeyFrameHandleMoverTool
//-----------------------------------------------------------------------------

class KeyFrameHandleMoverTool final : public XsheetGUI::DragTool {
public:
  enum HandleType { EaseOut = 0, EaseIn = 1 };

private:
  TStageObject *m_stageObject;
  TStageObject::Keyframe m_k;
  int m_startRow;  // E' la riga corrispondente alla key della handle corrente!
  HandleType m_handleType;
  KeyFrameHandleUndo *m_undo;
  int m_r0, m_r1;  // m_r0 e' la riga in cui si clicca, m_r1 e' la riga che
                   // varia nel drag
  bool m_enable;

public:
  KeyFrameHandleMoverTool(XsheetViewer *viewer, HandleType handleType,
                          int keyRow)
      : XsheetGUI::DragTool(viewer)
      , m_stageObject(0)
      , m_handleType(handleType)
      , m_startRow(keyRow)
      , m_r0(0)
      , m_r1(0)
      , m_enable(true) {}

  void onClick(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    m_r0 = m_r1  = row;
    TXsheet *xsh = getViewer()->getXsheet();
    TStageObjectId cameraId =
        TStageObjectId::CameraId(xsh->getCameraColumnIndex());

    TStageObjectId objId = col >= 0 ? TStageObjectId::ColumnId(col) : cameraId;
    if (xsh->getColumn(col) && xsh->getColumn(col)->isLocked()) {
      m_enable = false;
      return;
    }
    m_stageObject = xsh->getStageObject(objId);
    assert(m_stageObject);
    m_k    = m_stageObject->getKeyframe(m_startRow);
    m_undo = new KeyFrameHandleUndo(objId, m_startRow);
  }

  void onDrag(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    if (!m_enable) return;
    m_r1 = row;
    onCellChange(row, col);
    TApp::instance()->getCurrentObject()->notifyObjectIdChanged(true);
    refreshCellsArea();
  }

  void onCellChange(int row, int col) {
    int dr = row - m_startRow;
    if (m_handleType == EaseOut) {
      if (dr <= 0) dr = 0;
      if (m_k.m_easeOut == dr) return;
      m_k.m_easeOut = dr;
    } else {
      if (dr >= 0) dr = 0;
      if (m_k.m_easeIn == dr) return;
      m_k.m_easeIn = -dr;
    }
    m_stageObject->setKeyframeWithoutUndo(m_startRow, m_k);
  }

  void onRelease(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    if (!m_enable) return;
    if (m_r0 == m_r1)
      delete m_undo;
    else {
      TUndoManager::manager()->add(m_undo);
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    }
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeKeyFrameHandleMoverTool(
    XsheetViewer *viewer, bool isEaseOut, int keyRow) {
  return new KeyFrameHandleMoverTool(viewer,
                                     isEaseOut
                                         ? KeyFrameHandleMoverTool::EaseOut
                                         : KeyFrameHandleMoverTool::EaseIn,
                                     keyRow);
}

//=============================================================================
// OnionSkinMaskModifier tool
//-----------------------------------------------------------------------------

namespace {

//=============================================================================
// OnionSkinMaskModifierTool
//-----------------------------------------------------------------------------

class NoteMoveTool final : public XsheetGUI::DragTool {
  TPointD m_startPos, m_delta;
  int m_startRow, m_startCol;

public:
  NoteMoveTool(XsheetViewer *viewer) : DragTool(viewer) {}

  void onClick(const QMouseEvent *e) override {
    TXshNoteSet *notes = getViewer()->getXsheet()->getNotes();
    int currentIndex   = getViewer()->getCurrentNoteIndex();
    m_startPos         = notes->getNotePos(currentIndex);
    m_startRow         = notes->getNoteRow(currentIndex);
    m_startCol         = notes->getNoteCol(currentIndex);
    QPoint p           = e->pos();
    TPointD mousePos(p.x(), p.y());
    QPoint xy = getViewer()->positionToXY(CellPosition(m_startRow, m_startCol));
    TPointD cellTopLeft(xy.x(), xy.y());
    m_delta = mousePos - (cellTopLeft + m_startPos);
  }

  void onChange(TPointD pos) {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    pos          = pos - m_delta;
    CellPosition cellPosition = getViewer()->xyToPosition(pos);
    int row                   = cellPosition.frame();
    int col                   = cellPosition.layer();

    if (row < 0) row = 0;
    if (col < 0 || (!getViewer()->orientation()->isVerticalTimeline() &&
                    (col == 0 || col >= xsh->getColumnCount())))
      return;

    QPoint xy      = getViewer()->positionToXY(CellPosition(row, col));
    TPointD newPos = pos - TPointD(xy.x(), xy.y());

    if (newPos.x < 0) newPos.x = 0;
    if (newPos.y < 0) newPos.y = 0;

    TXshNoteSet *notes = getViewer()->getXsheet()->getNotes();
    int currentIndex   = getViewer()->getCurrentNoteIndex();
    notes->setNoteRow(currentIndex, row);
    notes->setNoteCol(currentIndex, col);
    notes->setNotePos(currentIndex, newPos);
  }

  void onDrag(const QMouseEvent *event) override {
    QPoint p = event->pos();
    onChange(TPointD(p.x(), p.y()));
    refreshCellsArea();
  }

  void onRelease(const QMouseEvent *event) override {
    QPoint p = event->pos();
    onChange(TPointD(p.x(), p.y()));

    TXshNoteSet *notes = getViewer()->getXsheet()->getNotes();
    int currentIndex   = getViewer()->getCurrentNoteIndex();
    TPointD pos        = notes->getNotePos(currentIndex);
    int row            = notes->getNoteRow(currentIndex);
    int col            = notes->getNoteCol(currentIndex);
    if (m_startPos == pos && m_startRow == row && m_startCol == col) return;

    refreshCellsArea();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeNoteMoveTool(
    XsheetViewer *viewer) {
  return new NoteMoveTool(viewer);
}

//=============================================================================
// OnionSkinMaskModifier tool
//-----------------------------------------------------------------------------

namespace {

//=============================================================================
// OnionSkinMaskModifierTool
//-----------------------------------------------------------------------------

class OnionSkinMaskModifierTool final : public XsheetGUI::DragTool {
  OnionSkinMaskModifier m_modifier;
  bool m_isFos;

public:
  OnionSkinMaskModifierTool(XsheetViewer *viewer, bool isFos)
      : DragTool(viewer)
      , m_modifier(TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask(),
                   TApp::instance()->getCurrentFrame()->getFrame())
      , m_isFos(isFos) {}

  void onClick(const CellPosition &pos) override {
    int row            = pos.frame();
    OnionSkinMask mask = m_modifier.getMask();
    TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(mask);
    m_modifier.click(row, m_isFos);
    TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
  }

  void onDrag(const CellPosition &pos) override {
    int row = pos.frame();
    if (row < 0) row = 0;
    onRowChange(row);
    TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
  }

  void onRowChange(int row) {
    m_modifier.drag(row);
    OnionSkinMask mask = m_modifier.getMask();
    TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(mask);
  }

  void onRelease(const CellPosition &pos) override {
    int row = pos.frame();
    m_modifier.release(row);
    OnionSkinMask mask = m_modifier.getMask();
    TApp::instance()->getCurrentOnionSkin()->setOnionSkinMask(mask);
    TApp::instance()->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeKeyOnionSkinMaskModifierTool(
    XsheetViewer *viewer, bool isFos) {
  return new OnionSkinMaskModifierTool(viewer, isFos);
}

//=============================================================================
// CurrentFrameModifier tool
//-----------------------------------------------------------------------------

namespace {

//=============================================================================
// CurrentFrameModifier
//-----------------------------------------------------------------------------

class CurrentFrameModifier final : public XsheetGUI::DragTool {
public:
  CurrentFrameModifier(XsheetViewer *viewer) : DragTool(viewer) {}

  void onClick(const CellPosition &pos) override {
    int row = pos.frame();
    TApp::instance()->getCurrentFrame()->setFrame(row);
    refreshRowsArea();
  }

  void onDrag(const CellPosition &pos) override {
    int row = pos.frame();
    if (row < 0) row = 0;
    int lastRow = TApp::instance()->getCurrentFrame()->getFrameIndex();
    if (lastRow == row) return;
    onRowChange(row);
    refreshRowsArea();
  }

  void onRowChange(int row) {
    TApp *app = TApp::instance();
    app->getCurrentFrame()->setFrame(row);
    int columnIndex = app->getCurrentColumn()->getColumnIndex();
    app->getCurrentFrame()->scrubXsheet(row, row, getViewer()->getXsheet());
  }

  void onRelease(const CellPosition &pos) override {
    getViewer()->getXsheet()->stopScrub();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeCurrentFrameModifierTool(
    XsheetViewer *viewer) {
  return new CurrentFrameModifier(viewer);
}

//=============================================================================
// PlayRangeModifier tool
//-----------------------------------------------------------------------------

namespace {

//=============================================================================
// PlayRangeModifier
//-----------------------------------------------------------------------------

class PlayRangeModifier final : public XsheetGUI::DragTool {
  bool m_movingFirst;
  int m_oldR0, m_oldR1, m_oldStep;

public:
  PlayRangeModifier(XsheetViewer *viewer)
      : DragTool(viewer), m_movingFirst(false) {}

  void onClick(const CellPosition &pos) override {
    int row = pos.frame();
    XsheetGUI::getPlayRange(m_oldR0, m_oldR1, m_oldStep);
    assert(m_oldR0 == row || m_oldR1 == row);
    m_movingFirst = m_oldR0 == row;
    refreshRowsArea();
  }

  void onDrag(const CellPosition &pos) override {
    int row = pos.frame();
    if (row < 0) row = 0;
    onRowChange(row);
    refreshRowsArea();
  }

  void onRowChange(int row) {
    if (row < 0) return;
    int r0, r1, step;
    XsheetGUI::getPlayRange(r0, r1, step);
    if (m_movingFirst) {
      if (row <= r1)
        r0 = row;
      else if (row > r1) {
        r0            = (r1 > 0) ? r1 : 0;
        r1            = row;
        m_movingFirst = false;
      }
    } else {
      if (row >= r0)
        r1 = row;
      else if (row < r0) {
        r1            = r0;
        r0            = row;
        m_movingFirst = true;
      }
    }
    XsheetGUI::setPlayRange(r0, r1, step, false);
  }

  void onRelease(const CellPosition &pos) override {
    int row = pos.frame();
    int newR0, newR1, newStep;
    XsheetGUI::getPlayRange(newR0, newR1, newStep);
    if (m_oldR0 != newR0 || m_oldR1 != newR1) {
      TUndoManager::manager()->add(new UndoPlayRangeModifier(
          m_oldR0, newR0, m_oldR1, newR1, m_oldStep, newStep, true, true));
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
    }
    refreshRowsArea();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makePlayRangeModifierTool(
    XsheetViewer *viewer) {
  return new PlayRangeModifier(viewer);
}

//=============================================================================
// ColumnSelectionTool
//-----------------------------------------------------------------------------

namespace {

class ColumnSelectionTool final : public XsheetGUI::DragTool {
  int m_firstColumn;
  bool m_enabled;
  std::set<int> m_colSet;

public:
  ColumnSelectionTool(XsheetViewer *viewer)
      : DragTool(viewer), m_firstColumn(-1), m_enabled(false) {}

  void onClick(const QMouseEvent *event) override {
    TColumnSelection *selection = getViewer()->getColumnSelection();
    CellPosition cellPosition   = getViewer()->xyToPosition(event->pos());
    int col                     = cellPosition.layer();
    m_firstColumn               = col;
    bool isSelected             = selection->isColumnSelected(col);
    if (event->modifiers() & Qt::ControlModifier) {
      selection->selectColumn(col, !isSelected);
      m_enabled = true;

      if (isSelected) {
        TXshColumn *column = getViewer()->getXsheet()->getColumn(col);
        if (column && column->getColumnType() == TXshColumn::eFolderType) {
          int folderId = column->getFolderColumn()->getFolderColumnFolderId();
          for (int c = col - 1; c >= 0; c--) {
            TXshColumn *folderItemCol = getViewer()->getXsheet()->getColumn(c);
            if (!folderItemCol || !folderItemCol->isContainedInFolder(folderId))
              break;
            selection->selectColumn(c, !isSelected);
          }
        }
      }

      m_colSet = selection->getIndices();
    } else if (event->modifiers() & Qt::ShiftModifier) {
      // m_enabled = true;
      if (isSelected) return;
      int ia = col, ib = col;
      int columnCount = getViewer()->getXsheet()->getColumnCount();
      while (ia > 0 && !selection->isColumnSelected(ia - 1)) --ia;
      if (ia == 0) ia = col;
      while (ib < columnCount - 1 && !selection->isColumnSelected(ib + 1)) ++ib;
      if (ib == columnCount - 1) ib = col;
      int i;
      for (i = ia; i <= ib; i++) selection->selectColumn(i, true);
    } else {
      m_enabled = true;
      selection->selectNone();
      selection->selectColumn(col, true);
    }

    // Look for folders and add contents to folder list if not already included
    std::set<int> orig = selection->getIndices();
    std::set<int>::iterator it;
    for (it = orig.begin(); it != orig.end(); it++) {
      TXshColumn *column = getViewer()->getXsheet()->getColumn(*it);
      if (!column || column->getColumnType() != TXshColumn::eFolderType)
        continue;
      int folderId = column->getFolderColumn()->getFolderColumnFolderId();
      for (int c = (*it) - 1; c >= 0; c--) {
        TXshColumn *folderItemCol = getViewer()->getXsheet()->getColumn(c);
        if (!folderItemCol || !folderItemCol->isContainedInFolder(folderId))
          break;
        selection->selectColumn(c, true);
      }
    }

    selection->makeCurrent();
    getViewer()->update();
  }

  void onDrag(const CellPosition &pos) override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    int col      = pos.layer();
    if (!m_enabled) return;
    int firstCol =
        Preferences::instance()->isXsheetCameraColumnVisible() ? -1 : 0;
    if (col < firstCol || (!getViewer()->orientation()->isVerticalTimeline() &&
                           col >= xsh->getColumnCount()))
      return;
    TColumnSelection *selection = getViewer()->getColumnSelection();
    selection->selectNone();
    int i, ia = m_firstColumn, ib = col;
    if (ia > ib) std::swap(ia, ib);
    for (i = ia; i <= ib; i++) selection->selectColumn(i, true);
    if (m_colSet.size()) {
      std::set<int>::iterator it;
      for (it = m_colSet.begin(); it != m_colSet.end(); it++)
        selection->selectColumn((*it), true);
    }
    getViewer()->update();
    refreshCellsArea();
    return;
  }
  void onRelease(const CellPosition &pos) override {
    m_colSet.clear();
    TSelectionHandle::getCurrent()->notifySelectionChanged();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeColumnSelectionTool(
    XsheetViewer *viewer) {
  return new ColumnSelectionTool(viewer);
}

//=============================================================================
// Column Movement
//-----------------------------------------------------------------------------

static void moveColumns(const std::set<int> &oldIndices,
                        const std::set<int> &newIndices,
                        const std::vector<QStack<int>> &newFolders) {
  if (oldIndices.empty() || newIndices.empty() || oldIndices.size() != newIndices.size()) return;

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  std::vector<int> oldSet;
  std::vector<int> newSet;
  std::vector<QStack<int>> newFolderSet;
  if ((*newIndices.begin() - *oldIndices.begin()) > 0) {
    oldSet.assign(oldIndices.rbegin(), oldIndices.rend());
    newSet.assign(newIndices.rbegin(), newIndices.rend());
    newFolderSet.assign(newFolders.rbegin(), newFolders.rend());
  } else {
    oldSet.assign(oldIndices.begin(), oldIndices.end());
    newSet.assign(newIndices.begin(), newIndices.end());
    newFolderSet.assign(newFolders.begin(), newFolders.end());
  }

  int currentCol = app->getCurrentColumn()->getColumnIndex();
  int newCurrentCol = currentCol;

  int i, m = oldSet.size();
  for (i = 0; i < m; i++) {
    xsh->moveColumn(oldSet[i], newSet[i]);

    TXshColumn *column = xsh->getColumn(newSet[i]);
    column->setFolderIdStack(newFolderSet[i]);

    if (oldSet[i] == currentCol) newCurrentCol = newSet[i];
  }

  app->getCurrentColumn()->setColumnIndex(newCurrentCol);
}
//-----------------------------------------------------------------------------

class ColumnMoveUndo final : public TUndo {
  std::set<int> m_oldIndices;
  std::set<int> m_newIndices;

  std::vector<QStack<int>> m_oldFolders, m_newFolders;

public:
  // nota: indices sono gli indici DOPO aver fatto il movimento
  ColumnMoveUndo(const std::set<int> &oldIndices,
                 const std::vector<QStack<int>> oldFolders,
                 const std::set<int> &newIndices,
                 const std::vector<QStack<int>> newFolders)
      : m_oldIndices(oldIndices)
      , m_newIndices(newIndices)
      , m_oldFolders(oldFolders)
      , m_newFolders(newFolders) {
    assert(!oldIndices.empty());
    assert(*oldIndices.begin() >= 0);
    assert(!newIndices.empty());
    assert(*newIndices.begin() >= 0);
  }
  void undo() const override {
    moveColumns(m_newIndices, m_oldIndices, m_oldFolders);

    TSelection *selection =
        TApp::instance()->getCurrentSelection()->getSelection();
    if (selection) selection->selectNone();

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (std::set<int>::iterator it = m_oldIndices.begin();
         it != m_oldIndices.end(); ++it) {
      TXshColumn *column   = xsh->getColumn(*it);
      bool isColumnVisible = column->isColumnVisible();
      for (auto o : Orientations::all()) {
        ColumnFan *columnFan = xsh->getColumnFan(o);
        if (isColumnVisible)
          columnFan->show(*it);
        else
          columnFan->hide(*it);
      }
    }

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  void redo() const override {
    moveColumns(m_oldIndices, m_newIndices, m_newFolders);

    TSelection *selection =
        TApp::instance()->getCurrentSelection()->getSelection();
    if (selection) selection->selectNone();

    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (std::set<int>::iterator it = m_newIndices.begin();
         it != m_newIndices.end(); ++it) {
      TXshColumn *column   = xsh->getColumn(*it);
      bool isColumnVisible = column->isColumnVisible();
      for (auto o : Orientations::all()) {
        ColumnFan *columnFan = xsh->getColumnFan(o);
        if (isColumnVisible)
          columnFan->show(*it);
        else
          columnFan->hide(*it);
      }
    }

    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
  int getSize() const override {
    return sizeof(*this) + m_oldIndices.size() * sizeof(int) +
           m_newIndices.size() * sizeof(int);
  }

  QString getHistoryString() override { return QObject::tr("Move Columns"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

class ColumnMoveDragTool final : public XsheetGUI::DragTool {
  QPoint m_firstPos, m_curPos;
  int m_firstCol, m_targetCol;
  bool m_dropOnColumnFolder, m_folderChanged, m_dragged, m_modifierPressed;
  QStack<int> m_addToFolder;

public:
  ColumnMoveDragTool(XsheetViewer *viewer)
      : XsheetGUI::DragTool(viewer)
      , m_firstCol(-1)
      , m_targetCol(-1)
      , m_dropOnColumnFolder(false)
      , m_folderChanged(false)
      , m_dragged(false)
      , m_modifierPressed(false) {}

  bool canDrop(const CellPosition &pos) {
    int col = pos.layer();
    if (col < 0) return false;

    TColumnSelection *selection = getViewer()->getColumnSelection();
    if (!selection || selection->isEmpty()) return false;

    std::set<int> indices = selection->getIndices();
    if (indices.find(col) != indices.end()) return false;

    return true;
  }

  void onClick(const QMouseEvent *event) override {
    m_targetCol          = -1;
    m_firstCol           = -1;
    m_dropOnColumnFolder = false;
    m_folderChanged      = false;
    m_dragged            = false;
    m_modifierPressed    = event->modifiers() != Qt::NoModifier;
    m_addToFolder.clear();

    m_firstPos                  = event->pos();
    CellPosition pos            = getViewer()->xyToPosition(m_firstPos);
    int col                     = pos.layer();
    TColumnSelection *selection = getViewer()->getColumnSelection();
    if (!selection->isColumnSelected(col)) {
      if (event->modifiers() & Qt::ControlModifier) {
        selection->selectColumn(col, true);
      } else if (event->modifiers() & Qt::ShiftModifier) {
        int ia = col, ib = col;
        int columnCount = getViewer()->getXsheet()->getColumnCount();
        while (ia > 0 && !selection->isColumnSelected(ia - 1)) --ia;
        if (ia == 0) ia = col;
        while (ib < columnCount - 1 && !selection->isColumnSelected(ib + 1))
          ++ib;
        if (ib == columnCount - 1) ib = col;
        int i;
        for (i = ia; i <= ib; i++) selection->selectColumn(i, true);
      } else {
        selection->selectNone();
        selection->selectColumn(col);
      }
      selection->makeCurrent();
    }
    std::set<int> indices = selection->getIndices();
    indices.erase(-1);
    if (indices.empty()) return;
    m_firstCol = *indices.begin(); //col;
    assert(m_firstCol >= 0);

    // Look for folders and add contents to folder list if not already included
    std::set<int> orig = indices;
    std::set<int>::iterator it;
    for (it = orig.begin(); it != orig.end(); it++) {
      TXshColumn *column = getViewer()->getXsheet()->getColumn(*it);
      if (!column || column->getColumnType() != TXshColumn::eFolderType)
        continue;
      int folderId = column->getFolderColumn()->getFolderColumnFolderId();
      for (int c = (*it) - 1; c >= 0; c--) {
        TXshColumn *folderItemCol = getViewer()->getXsheet()->getColumn(c);
        if (!folderItemCol || !folderItemCol->isContainedInFolder(folderId))
          break;
        indices.insert(c);
      }
    }

    selection->selectNone();
    for (std::set<int>::iterator it = indices.begin(); it != indices.end();
         ++it)
      selection->selectColumn(*it, true);

    getViewer()->update();

    if (!getViewer()->orientation()->isVerticalTimeline())
      TUndoManager::manager()->beginBlock();
  }
  void onDrag(const QMouseEvent *e) override {
    m_curPos             = e->pos();
    m_targetCol          = -1;
    m_dropOnColumnFolder = false;
    m_folderChanged      = false;
    m_addToFolder.clear();

    CellPosition pos = getViewer()->xyToPosition(m_curPos);
    int col          = pos.layer();

    CellPosition firstPos = getViewer()->xyToPosition(m_firstPos);
    int firstCol          = firstPos.layer();

    if (!canDrop(CellPosition(0, col))) return;

    const Orientation *o = getViewer()->orientation();
    TXsheet *xsh         = getViewer()->getXsheet();
    TXshColumn *column   = xsh->getColumn(col);
    if (!o->isVerticalTimeline() && !column) return;

    QPoint orig          = getViewer()->positionToXY(pos);
    int dPos = o->isVerticalTimeline() ? m_curPos.x() - m_firstPos.x()
                                       : m_firstPos.y() - m_curPos.y();
    // Minimum movement
    if (dPos > -5 && dPos < 5) return;

    m_dragged = true;

    QRect rect = getViewer()
                     ->orientation()
                     ->rect(PredefinedRect::LAYER_HEADER)
                     .translated(orig);
    if (!o->isVerticalTimeline())
      rect.adjust(0, 0, getViewer()->getTimelineBodyOffset(), 0);

    TXshFolderColumn *folderColumn = column ? column->getFolderColumn() : 0;
    int folderAdj = folderColumn && folderColumn->isExpanded() ? 5 : 0;

    // See if we're on top of a folder column
    if (column && folderColumn &&
        (o->isVerticalTimeline()
             ? rect.adjusted(5, 0, -folderAdj, 0).contains(m_curPos)
             : rect.adjusted(0, 5, 0, -folderAdj).contains(m_curPos))) {
      m_dropOnColumnFolder = true;
      int folderId = column->getFolderColumn()->getFolderColumnFolderId();
      m_addToFolder = column->getFolderColumn()->getFolderIdStack();
      m_addToFolder.push(folderId);
      m_folderChanged = true;
    }

    if (!m_dropOnColumnFolder) {
      int origCol = col;

      // Split rect in 1/2, keeping Top/Right
      if (o->isVerticalTimeline())
        rect.adjust(rect.width() / 2, 0, 0, 0);
      else
        rect.adjust(0, 0, 0, -rect.height() / 2);

      if (dPos > 0 && !rect.contains(m_curPos))
        col = firstCol == col ? col : col - 1;
      else if (dPos <= 0 && rect.contains(m_curPos))
        col = firstCol == col ? col : col + 1;

      if (col < 0) return;

      // See if we are on the bottom of folder
      if (column && column->isInFolder() && !column->getFolderColumn() &&
          ((dPos > 0 && origCol != col) || (dPos < 0 && origCol == col))) {
        TXshColumn *prev = xsh->getColumn(origCol - 1);
        if (prev && prev->getFolderId() != column->getFolderId()) {
          m_addToFolder = column->getFolderIdStack();
          m_folderChanged = true;
        }
      }

      if (m_addToFolder.isEmpty()) {
        TXshColumn *priorColumn =
            getViewer()->getXsheet()->getColumn(dPos < 0 ? col - 1 : col);
        if (priorColumn) { 
          m_addToFolder = priorColumn->getFolderIdStack();
          m_folderChanged = true;
        }
      }
    }

    m_targetCol = col;
  }
  void onRelease(const CellPosition &pos) override {
    int offset = 0;

    TColumnSelection *selection = getViewer()->getColumnSelection();
    std::set<int> oldIndices    = selection->getIndices();
    oldIndices.erase(-1);

    if (!oldIndices.empty() && m_targetCol >= 0) {
      offset = m_targetCol - m_firstCol;
      if (offset > 0) offset -= (oldIndices.size() - 1);
    }

    if (offset != 0 || m_folderChanged) {
      TXsheet *xsh = getViewer()->getXsheet();
      std::set<int> newIndices;
      std::vector<int> vOldIndices, vNewIndices;
      std::vector<QStack<int>> vOldFolders, vNewFolders;
      vOldIndices.assign(oldIndices.begin(), oldIndices.end());
      int newCol = m_firstCol + offset;

      // Dropping into folder
      if (!m_addToFolder.isEmpty() && offset && xsh->getColumn(m_targetCol)->getFolderColumn() &&
          xsh->getColumn(m_targetCol)->getFolderColumn()->getFolderColumnFolderId() == m_addToFolder.back()) {
        xsh->openCloseFolder(m_targetCol, true);
        if (offset > 0)
          newCol--;
      }

      if (newCol < 0) return;

      std::vector<int>::reverse_iterator it;
      int i = 0;
      int subfolder = -1;
      QStack<int> folderList = m_addToFolder;
      for (it = vOldIndices.rbegin(); it != vOldIndices.rend(); it++, i++) {
        newIndices.insert(newCol + i);

        TXshColumn *column        = xsh->getColumn(*it);
        QStack<int> folderIdStack = column->getFolderIdStack();
        vOldFolders.insert(vOldFolders.begin(), folderIdStack);

        if (subfolder >= 0 && !column->isContainedInFolder(subfolder)) {
          folderList.pop();
          if (folderList.size())
            subfolder = folderList.top();
          else
            subfolder = -1;
        }

        vNewFolders.insert(vNewFolders.begin(), folderList);

        if (column->getFolderColumn()) {
          subfolder = column->getFolderColumn()->getFolderColumnFolderId();
          folderList.push(subfolder);
        }
      }

      moveColumns(oldIndices, newIndices, vNewFolders);

      selection->selectNone();
      for (std::set<int>::iterator it = newIndices.begin();
           it != newIndices.end(); ++it) {
        selection->selectColumn(*it, true);

        TXshColumn *column = xsh->getColumn(*it);
        bool isColumnVisible = column->isColumnVisible();
        for (auto o : Orientations::all()) {
          ColumnFan *columnFan = xsh->getColumnFan(o);
          if (isColumnVisible)
            columnFan->show(*it);
          else
            columnFan->hide(*it);
        }
      }

      TUndoManager::manager()->add(new ColumnMoveUndo(
          oldIndices, vOldFolders, newIndices, vNewFolders));
      TApp::instance()->getCurrentScene()->setDirtyFlag(true);
      TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    }

    if (!getViewer()->orientation()->isVerticalTimeline())
      TUndoManager::manager()->endBlock();

    // Reset current selection if we didn't move
    if (!Preferences::instance()->isShowDragBarsEnabled() && !m_dragged &&
        !m_modifierPressed && !oldIndices.empty() && oldIndices.size() > 1) {
      selection->selectNone();
      selection->selectColumn(pos.layer());
      getViewer()->setCurrentColumn(pos.layer());
    }
  }
  void drawColumnsArea(QPainter &p) {
    if (m_targetCol < 0) return;

    TColumnSelection *selection = getViewer()->getColumnSelection();
    if (!selection || selection->isEmpty()) return;

    TXsheet *xsh = getViewer()->getXsheet();

    std::set<int> indices  = selection->getIndices();
    CellPosition pos       = getViewer()->xyToPosition(m_curPos);
    int origCol            = pos.layer();
    TXshColumn *origColumn = xsh->getColumn(origCol);

    const Orientation *o = getViewer()->orientation();
    QPoint orig = getViewer()->positionToXY(CellPosition(0, m_targetCol));

    TStageObjectId columnId = getViewer()->getObjectId(m_targetCol);
    TXshColumn *column      = xsh->getColumn(m_targetCol);
    if (!o->isVerticalTimeline() && !column) return;

    QRect rect = getViewer()
                     ->orientation()
                     ->rect(PredefinedRect::LAYER_HEADER)
                     .translated(orig);
    if (!o->isVerticalTimeline())
      rect.adjust(0, 0, getViewer()->getTimelineBodyOffset(), 0);
    else
      rect.adjust(0, 0, 0, getViewer()->getXsheetBodyOffset());

    if (!o->isVerticalTimeline()) {
      QRect buttonsRect =
          o->rect(PredefinedRect::BUTTONS_AREA).translated(orig);
      rect.adjust(buttonsRect.width(), 0, 0, 0);
    }

    int dPos = o->isVerticalTimeline() ? m_curPos.x() - m_firstPos.x()
                                       : m_firstPos.y() - m_curPos.y();

    // Minimum movement
    if (dPos > -5 && dPos < 5) return;

    int topCol = *indices.rbegin();

    int columnDepth = origColumn ? origColumn->folderDepth() : 0;
    QRect indicatorRect =
        o->rect(PredefinedRect::FOLDER_INDICATOR_AREA).translated(orig);
    if (o->isVerticalTimeline())
      rect.adjust(0, indicatorRect.height() * columnDepth, 0, 0);
    else
      rect.adjust(indicatorRect.width() * columnDepth, 0, 0, 0);

    p.setPen(QColor(190, 220, 255));
    p.setBrush(Qt::NoBrush);
    if (m_dropOnColumnFolder) {
      for (int i = 0; i < 3; i++)  // thick border within name area
        p.drawRect(QRect(rect.topLeft() + QPoint(i, i),
                         rect.size() - QSize(2 * i, 2 * i)));
    } else {
      if (o->isVerticalTimeline()) {
        QPoint begin = dPos > 0 ? rect.topRight() : rect.topLeft();
        QPoint end   = dPos > 0 ? rect.bottomRight() : rect.bottomLeft();

        p.drawLine(begin + QPoint(-1, 0), end + QPoint(-1, 0));
        p.drawLine(begin, end);
        p.drawLine(begin + QPoint(1, 0), end + QPoint(1, 0));
      } else {
        QPoint begin = dPos > 0 ? rect.topLeft() : rect.bottomLeft();
        QPoint end   = dPos > 0 ? rect.topRight() : rect.bottomRight();

        p.drawLine(begin + QPoint(0, -1), end + QPoint(0, -1));
        p.drawLine(begin, end);
        p.drawLine(begin + QPoint(0, 1), end + QPoint(0, 1));
      }
    }
  }
};

XsheetGUI::DragTool *XsheetGUI::DragTool::makeColumnMoveTool(
    XsheetViewer *viewer) {
  return new ColumnMoveDragTool(viewer);
}

//=============================================================================
//  Parent Change
//-----------------------------------------------------------------------------

class ChangePegbarParentUndo final : public TUndo {
  TStageObjectId m_oldParentId;
  TStageObjectId m_newParentId;
  TStageObjectId m_child;

public:
  ChangePegbarParentUndo(const TStageObjectId &child,
                         const TStageObjectId &oldParentId,
                         const TStageObjectId &newParentId)
      : m_oldParentId(oldParentId)
      , m_newParentId(newParentId)
      , m_child(child) {}

  void undo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    xsh->setStageObjectParent(m_child, m_oldParentId);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    xsh->setStageObjectParent(m_child, m_newParentId);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof(*this); }

  QString getHistoryString() override { return QObject::tr("Change Pegbar"); }
  int getHistoryType() override { return HistoryType::Xsheet; }
};

//-----------------------------------------------------------------------------

class ChangePegbarParentDragTool final : public XsheetGUI::DragTool {
  int m_firstCol, m_lastCol;

public:
  ChangePegbarParentDragTool(XsheetViewer *viewer)
      : XsheetGUI::DragTool(viewer), m_firstCol(-1), m_lastCol(-1) {}

  void onClick(const CellPosition &pos) override {
    m_firstCol = m_lastCol = pos.layer();
  }
  void onDrag(const CellPosition &pos) override { m_lastCol = pos.layer(); }
  void onRelease(const CellPosition &pos) override {
    // TUndoManager::manager()->add(new ColumnMoveUndo(indices, delta));
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    TStageObjectId columnId(getViewer()->getObjectId(m_firstCol));
    if (m_firstCol == -1)
      columnId = TStageObjectId::CameraId(
          getViewer()->getXsheet()->getCameraColumnIndex());
    TStageObjectId parentId(getViewer()->getObjectId(m_lastCol));
    if (m_lastCol == -1)
      parentId = TStageObjectId::CameraId(
          getViewer()->getXsheet()->getCameraColumnIndex());
    if (getViewer()->getXsheet()->getColumn(m_lastCol) &&
        (getViewer()->getXsheet()->getColumn(m_lastCol)->getSoundColumn() ||
         getViewer()->getXsheet()->getColumn(m_lastCol)->getFolderColumn()))
      return;

    if (m_firstCol == m_lastCol && m_firstCol != -1) {
      // vuol dire che la colonna torna al suo padre di default
      // la prima colonna e' attaccata alla pegbar 0 le altre alla pegbar 1.
      // brutto che questa cosa sia qui. Bisognerebbe spostarlo altrove (in
      // tnzlib)
      parentId = TStageObjectId::TableId;
    }
    if (m_firstCol == -1 && m_lastCol <= -1) {
      parentId = TStageObjectId::NoneId;
    }

    if (parentId == xsh->getStageObjectParent(columnId)) return;
    TUndoManager::manager()->add(new ChangePegbarParentUndo(
        columnId, xsh->getStageObjectParent(columnId), parentId));
    TApp::instance()->getCurrentScene()->setDirtyFlag(true);

    xsh->setStageObjectParent(columnId, parentId);
    getViewer()->update();
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  }
};

//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeColumnLinkTool(
    XsheetViewer *viewer) {
  return new ChangePegbarParentDragTool(viewer);
}

//=============================================================================
// Volume adjust
//-----------------------------------------------------------------------------

namespace {

class VolumeDragTool final : public XsheetGUI::DragTool {
  int m_index;
  bool m_enabled;

public:
  VolumeDragTool(XsheetViewer *viewer)
      : DragTool(viewer)
      , m_index(TApp::instance()->getCurrentColumn()->getColumnIndex())
      , m_enabled(false) {
    TXshColumn *column           = viewer->getXsheet()->getColumn(m_index);
    m_enabled                    = column != 0 && !column->isLocked();
    TXshSoundColumn *soundColumn = column->getSoundColumn();
    assert(soundColumn);
    if (soundColumn->isPlaying()) {
      viewer->update();
      // soundColumn->stop();
    }
  }

  void onClick(const QMouseEvent *) override {}

  void onDrag(const QMouseEvent *event) override {
    if (!m_enabled) return;

    const Orientation *o = getViewer()->orientation();
    QRect track          = o->rect(PredefinedRect::VOLUME_TRACK);
    NumberRange range    = o->frameSide(track);
    int frameAxis        = o->frameAxis(event->pos());
    if (o->isVerticalTimeline() &&
        !o->flag(PredefinedFlag::VOLUME_AREA_VERTICAL)) {
      range = o->layerSide(track);
      frameAxis =
          o->layerAxis(event->pos()) - getViewer()->columnToLayerAxis(m_index);
    }

    double v = range.ratio(frameAxis);
    if (o->flag(PredefinedFlag::VOLUME_AREA_VERTICAL)) v = 1 - v;

    TXsheet *xsh       = getViewer()->getXsheet();
    TXshColumn *column = xsh->getColumn(m_index);
    if (!column) return;
    TXshSoundColumn *soundColumn = column->getSoundColumn();
    if (!soundColumn) return;
    soundColumn->setVolume(v);

    getViewer()->update();
  }

  void onRelease(const QMouseEvent *) override {
    TApp::instance()->getCurrentXsheet()->notifyXsheetSoundChanged();
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeVolumeDragTool(
    XsheetViewer *viewer) {
  return new VolumeDragTool(viewer);
}

//=============================================================================
//  SoundScrub tool
//-----------------------------------------------------------------------------

namespace {

class SoundScrubTool final : public XsheetGUI::DragTool {
  TXshSoundColumn *m_soundColumn;
  int m_startRow;
  std::pair<int, int> m_playRange;
  int m_row, m_oldRow;
  int m_timerId;

public:
  SoundScrubTool(XsheetViewer *viewer, TXshSoundColumn *sc)
      : DragTool(viewer)
      , m_soundColumn(sc)
      , m_startRow(-1)
      , m_playRange(0, -1)
      , m_row(0)
      , m_oldRow(0)
      , m_timerId(0) {}

  void onClick(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    TColumnSelection *selection = getViewer()->getColumnSelection();
    selection->selectNone();
    m_startRow = row;
    getViewer()->setScrubHighlight(row, m_startRow, col);
    getViewer()->updateCells();
  }

  void onDrag(const CellPosition &pos) override {
    int row = pos.frame(), col = pos.layer();
    onCellChange(row, col);
  }

  void onCellChange(int row, int col) {
    assert(m_startRow >= 0);
    getViewer()->setScrubHighlight(row, m_startRow, col);
    getViewer()->updateCells();
  }

  void onRelease(const CellPosition &pos) override {
    int r0 = std::min(pos.frame(), m_startRow);
    int r1 = std::max(pos.frame(), m_startRow);
    assert(m_soundColumn);
    TApp *app         = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
    app->getCurrentFrame()->scrubColumn(r0, r1, m_soundColumn, fps);
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeSoundScrubTool(
    XsheetViewer *viewer, TXshSoundColumn *sc) {
  return new SoundScrubTool(viewer, sc);
}

//=============================================================================
//  DataDragTool
//-----------------------------------------------------------------------------

namespace {

//=============================================================================
// DragAndDropData
//-----------------------------------------------------------------------------

class DragAndDropData {
public:
  std::vector<std::pair<TXshSimpleLevelP, std::vector<TFrameId>>> m_levels;
  std::vector<TFilePath> m_paths;

  DragAndDropData() {}

  void addSimpleLevel(
      std::pair<TXshSimpleLevelP, std::vector<TFrameId>> level) {
    m_levels.push_back(level);
  }
  void setSimpleLevels(
      std::vector<std::pair<TXshSimpleLevelP, std::vector<TFrameId>>> levels) {
    m_levels = levels;
  }

  TRect getLevelFrameRect(bool isVertical) {
    if (!m_paths.empty()) return TRect();
    int maxRow      = 0;
    int columnCount = m_levels.size();
    int i;
    for (i = 0; i < columnCount; i++) {
      std::vector<TFrameId> fids = m_levels[i].second;
      int size                   = fids.size();
      if (maxRow < size) maxRow = size;
    }
    if (!isVertical) return TRect(0, 0, maxRow - 1, columnCount - 1);

    return TRect(0, 0, columnCount - 1, maxRow - 1);
  }

  void addPath(TFilePath path) { m_paths.push_back(path); }
  void setLevelPath(std::vector<TFilePath> paths) { m_paths = paths; }
};

//=============================================================================
//  DataDragTool
//-----------------------------------------------------------------------------

enum CellMovementType { NO_MOVEMENT, INSERT_CELLS, OVERWRITE_CELLS };

class DataDragTool final : public XsheetGUI::DragTool {
  DragAndDropData *m_data;
  bool m_valid;
  TPoint m_curPos;  // screen xy of drag begin
  CellMovementType m_type;

protected:
  bool canChange(int row, int col) {
    int c           = col;
    int r           = row;
    TXsheet *xsh    = getViewer()->getXsheet();
    bool isVeritcal = getViewer()->orientation()->isVerticalTimeline();
    TRect rect      = m_data->getLevelFrameRect(isVeritcal);
    int rectCols    = isVeritcal ? rect.getLx() : rect.getLy();
    int rectRows    = isVeritcal ? rect.getLy() : rect.getLx();
    for (c = col; c < rectCols + col; c++) {
      for (r = row; r < rectRows + row; r++) {
        if (xsh->getColumn(c) &&
            xsh->getColumn(c)->getColumnType() !=
                TXshColumn::ColumnType::eLevelType)
          return false;
        if (!xsh->getCell(r, c, false).isEmpty()) return false;
      }
    }
    return true;
  }

public:
  DataDragTool(XsheetViewer *viewer)
      : DragTool(viewer)
      , m_data(new DragAndDropData())
      , m_valid(false)
      , m_type(NO_MOVEMENT) {}

  void onClick(const QDropEvent *e) override {
    if (e->mimeData()->hasUrls()) {
      QList<QUrl> urls = e->mimeData()->urls();
      int i;
      for (i = 0; i < urls.size(); i++)
        m_data->addPath(TFilePath(urls[i].toLocalFile().toStdWString()));
    } else if (e->mimeData()->hasFormat(CastItems::getMimeFormat())) {
      const CastItems *cast = dynamic_cast<const CastItems *>(e->mimeData());
      int i;
      for (i = 0; i < cast->getItemCount(); i++) {
        CastItem *item      = cast->getItem(i);
        TXshSimpleLevel *sl = item->getSimpleLevel();
        if (!sl) continue;
        std::vector<TFrameId> fids;
        sl->getFids(fids);
        m_data->addSimpleLevel(std::make_pair(sl, fids));
      }
    } else if (e->mimeData()->hasFormat("application/vnd.toonz.drawings")) {
      TFilmstripSelection *s =
          dynamic_cast<TFilmstripSelection *>(TSelection::getCurrent());
      if (!s) return;
      TXshSimpleLevel *sl =
          TApp::instance()->getCurrentLevel()->getSimpleLevel();
      if (!sl) return;
      std::vector<TFrameId> fids;
      std::set<TFrameId> fidsSet = s->getSelectedFids();
      for (auto const &fid : fidsSet) {
        fids.push_back(fid);
      }
      m_data->addSimpleLevel(std::make_pair(sl, fids));
    }
    refreshCellsArea();
  }
  void onDrag(const QDropEvent *e) override {
    TPoint pos(e->pos().x(), e->pos().y());
    CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
    int row                   = cellPosition.frame();
    int col                   = cellPosition.layer();

    m_valid = true;
    if (e->keyboardModifiers() & Qt::ShiftModifier)
      m_type = INSERT_CELLS;
    else if (e->keyboardModifiers() & Qt::AltModifier)
      m_type = OVERWRITE_CELLS;
    else
      m_valid = canChange(row, col);
    m_curPos = pos;
    refreshCellsArea();
  }
  void onRelease(const QDropEvent *e) override {
    TPoint pos(e->pos().x(), e->pos().y());
    CellPosition cellPosition = getViewer()->xyToPosition(e->pos());
    int row                   = cellPosition.frame();
    int col                   = cellPosition.layer();
    if (m_type != NO_MOVEMENT && !m_valid) return;

    bool insert    = m_type == INSERT_CELLS;
    bool overWrite = m_type == OVERWRITE_CELLS;

    if (!m_data->m_paths
             .empty())  // caso in cui ho i path e deve caricare i livelli
    {
      IoCmd::LoadResourceArguments args;

      args.row0 = row;
      args.col0 = col;

      args.resourceDatas.assign(m_data->m_paths.begin(), m_data->m_paths.end());

      IoCmd::loadResources(args);
    } else if (!m_data->m_levels.empty()) {
      int i;
      for (i = 0; i < (int)m_data->m_levels.size(); i++) {
        TXshSimpleLevel *sl        = m_data->m_levels[i].first.getPointer();
        std::vector<TFrameId> fids = m_data->m_levels[i].second;
        if (!sl || fids.empty()) continue;
        IoCmd::exposeLevel(sl, row, col, fids, insert, overWrite);
      }
    }
    refreshCellsArea();
  }
  void drawCellsArea(QPainter &p) override {
    const Orientation *o           = getViewer()->orientation();
    CellPosition beginDragPosition = getViewer()->xyToPosition(m_curPos);
    TPoint pos(beginDragPosition.layer(),
               beginDragPosition.frame());  // row and cell coordinates
    bool isVertical = getViewer()->orientation()->isVerticalTimeline();
    if (!isVertical) {
      pos.x = beginDragPosition.frame();
      pos.y = beginDragPosition.layer();
    }
    TRect rect =
        m_data->getLevelFrameRect(isVertical);  // row and cell coordinates
    if (rect.isEmpty()) return;
    rect += pos;
    if (rect.x1 < 0 || rect.y1 < 0) return;
    if (rect.x0 < 0) rect.x0 = 0;
    if (rect.y0 < 0) rect.y0 = 0;
    QRect screenCell;
    if (o->isVerticalTimeline())
      screenCell = getViewer()->rangeToXYRect(
          CellRange(CellPosition(rect.y0, rect.x0),
                    CellPosition(rect.y1 + 1, rect.x1 + 1)));
    else {
      int newY0  = std::max(rect.y0, rect.y1);
      int newY1  = std::min(rect.y0, rect.y1);
      screenCell = getViewer()->rangeToXYRect(CellRange(
          CellPosition(rect.x0, newY0), CellPosition(rect.x1 + 1, newY1 - 1)));
    }
    p.setPen(m_valid ? QColor(190, 220, 255) : QColor(255, 0, 0));
    int i;
    for (i = 0; i < 3; i++)  // thick border within cell
      p.drawRect(QRect(screenCell.topLeft() + QPoint(i, i),
                       screenCell.size() - QSize(2 * i, 2 * i)));
  }
};

//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeDragAndDropDataTool(
    XsheetViewer *viewer) {
  return new DataDragTool(viewer);
}

//=============================================================================
//  NavigationTagDragTool
//-----------------------------------------------------------------------------

namespace {

class NavigationTagDragTool final : public XsheetGUI::DragTool {
  int m_taggedRow;

public:
  NavigationTagDragTool(XsheetViewer *viewer) : DragTool(viewer) {}

  void onClick(const CellPosition &pos) override {
    int row = pos.frame();
    m_taggedRow = row;
    refreshRowsArea();
  }

  void onDrag(const CellPosition &pos) override {
    int row          = pos.frame();
    if (row < 0) row = 0;
    onRowChange(row);
    refreshRowsArea();
  }

  void onRowChange(int row) {
    if (row < 0) return;

    TXsheet *xsh            = TApp::instance()->getCurrentXsheet()->getXsheet();
    NavigationTags *navTags = xsh->getNavigationTags();

    if (m_taggedRow == row || navTags->isTagged(row)) return;

    navTags->moveTag(m_taggedRow, row);
    m_taggedRow = row;
  }
};
//-----------------------------------------------------------------------------
}  // namespace
//-----------------------------------------------------------------------------

XsheetGUI::DragTool *XsheetGUI::DragTool::makeNavigationTagDragTool(
    XsheetViewer *viewer) {
  return new NavigationTagDragTool(viewer);
}
