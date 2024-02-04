

#include "toonz/txshfoldercolumn.h"

// TnzLib includes
#include "toonz/txshcell.h"

#include "tstream.h"

PERSIST_IDENTIFIER(TXshFolderColumn, "folderColumn")

//=============================================================================

TXshFolderColumn::TXshFolderColumn()
    : m_expanded(true), m_folderColumnFolderId(0) {}

//-----------------------------------------------------------------------------

TXshFolderColumn::~TXshFolderColumn() {}

//-----------------------------------------------------------------------------

TXshColumn::ColumnType TXshFolderColumn::getColumnType() const {
  return eFolderType;
}

//-----------------------------------------------------------------------------

bool TXshFolderColumn::canSetCell(const TXshCell &cell) const { return false; }

//-----------------------------------------------------------------------------

TXshColumn *TXshFolderColumn::clone() const {
  TXshFolderColumn *column = new TXshFolderColumn();
  column->setXsheet(getXsheet());
  column->setStatusWord(getStatusWord());
  column->m_cells = m_cells;
  column->m_first = m_first;

  column->m_expanded = m_expanded;
  column->m_folderColumnFolderId = m_folderColumnFolderId;
  column->setFolderIdStack(getFolderIdStack());
  return column;
}

//-----------------------------------------------------------------------------

void TXshFolderColumn::loadData(TIStream &is) {
  std::string tagName;
  while (is.openChild(tagName)) {
    if (tagName == "status") {
      int status;
      is >> status;
      setStatusWord(status);
      if (status & eCamstandTransparent43) {
        setOpacity(128);
        status = status & ~eCamstandTransparent43;
      }
    } else if (tagName == "camerastand_opacity") {
      int opacity;
      is >> opacity;
      setOpacity((UCHAR)opacity);
    } else if (tagName == "filter_color_id") {
      int id;
      is >> id;
      setColorFilterId(id);
    } else if (tagName == "columnFolderId") {
      int id;
      is >> id;
      setFolderColumnFolderId(id);
    } else if (tagName == "expanded") {
      int id;
      is >> id;
      setExpanded(id ? true : false);
    } else if (loadCellMarks(tagName, is)) {
      // do nothing
    } else if (loadFolderInfo(tagName, is)) {
      // do nothing
    } else
      throw TException("TXshFolderColumn, unknown tag: " + tagName);
    is.closeChild();
  }
}

//-----------------------------------------------------------------------------

void TXshFolderColumn::saveData(TOStream &os) {
  os.child("status") << getStatusWord();
  if (getOpacity() < 255) os.child("camerastand_opacity") << (int)getOpacity();
  if (getColorFilterId() != 0)
    os.child("filter_color_id") << (int)getColorFilterId();

  os.child("columnFolderId") << getFolderColumnFolderId();
  os.child("expanded") << (isExpanded() ? 1 : 0);

  // cell marks
  saveCellMarks(os);
  // folder info
  saveFolderInfo(os);
}
