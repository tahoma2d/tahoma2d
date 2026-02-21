

#include "toonz/txshpegbarcolumn.h"

// TnzLib includes
#include "toonz/txshcell.h"

#include "tstream.h"

PERSIST_IDENTIFIER(TXshPegbarColumn, "pegbarColumn")

//=============================================================================

TXshPegbarColumn::TXshPegbarColumn() {}

//-----------------------------------------------------------------------------

TXshPegbarColumn::~TXshPegbarColumn() {}

//-----------------------------------------------------------------------------

TXshColumn::ColumnType TXshPegbarColumn::getColumnType() const {
  return ePegbarType;
}

//-----------------------------------------------------------------------------

bool TXshPegbarColumn::canSetCell(const TXshCell& cell) const { return false; }

//-----------------------------------------------------------------------------

TXshColumn* TXshPegbarColumn::clone() const {
  TXshPegbarColumn* column = new TXshPegbarColumn();
  column->setXsheet(getXsheet());
  column->setStatusWord(getStatusWord());
  column->m_cells          = m_cells;
  column->m_first          = m_first;
  column->m_pegbarObjectId = m_pegbarObjectId;
  column->setFolderIdStack(getFolderIdStack());
  return column;
}

//-----------------------------------------------------------------------------

void TXshPegbarColumn::loadData(TIStream& is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "status") {
      int status;
      is >> status;
      setStatusWord(status);
    } else if (tagName == "pegbarObjectId") {
      std::string idStr;
      is >> idStr;
      TStageObjectId id = toStageObjectId(idStr);
      setPegbarObjectId(id);
    } else if (loadCellMarks(tagName, is)) {
      // do nothing
    } else if (loadFolderInfo(tagName, is)) {
      // do nothing
    } else
      throw TException("TXshPegbarColumn, unknown tag: " + tagName);
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TXshPegbarColumn::saveData(TOStream& os) {
  os.child("status") << getStatusWord();
  os.child("pegbarObjectId") << m_pegbarObjectId.toString();

  // cell marks
  saveCellMarks(os);
  // folder info
  saveFolderInfo(os);
}
