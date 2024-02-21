#pragma once

#ifndef TXSHFOLDERCOLUMN_INCLUDED
#define TXSHFOLDERCOLUMN_INCLUDED

#include "toonz/txshcolumn.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZLIB_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
//! The TXshFolderColumn class provides a folder column in xsheet and allows
//! its management through cell concept.
/*!Inherits \b TXshCellColumn. */
//=============================================================================

class DVAPI TXshFolderColumn final : public TXshCellColumn {
  PERSIST_DECLARATION(TXshFolderColumn)

  int m_folderColumnFolderId;
  bool m_expanded;

public:
  TXshFolderColumn();
  ~TXshFolderColumn();

  TXshColumn::ColumnType getColumnType() const override;
  TXshFolderColumn *getFolderColumn() override { return this; }

  // Technically level folder columns are always empty but needs to be regarded
  // as not
  bool isEmpty() const override { return false; }

  bool canSetCell(const TXshCell &cell) const override;

  TXshColumn *clone() const override;

  void loadData(TIStream &is) override;
  void saveData(TOStream &os) override;

  bool isExpanded() { return m_expanded; }
  void setExpanded(bool expanded) { m_expanded = expanded; }

  int getFolderColumnFolderId() { return m_folderColumnFolderId; }
  void setFolderColumnFolderId(int folderId) {
    m_folderColumnFolderId = folderId;
  }
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshFolderColumn>;
#endif
typedef TSmartPointerT<TXshFolderColumn> TXshFolderColumnP;

#endif  // TXSHFOLDERCOLUMN_INCLUDED
