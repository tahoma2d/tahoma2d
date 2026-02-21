#pragma once

#ifndef TXSHPEGBARCOLUMN_INCLUDED
#define TXSHPEGBARCOLUMN_INCLUDED

#include "toonz/txshcolumn.h"
#include "tstageobjectid.h"

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
//! The TXshPegbarColumn class provides a pegbar column in xsheet and allows
//! its management through cell concept.
/*!Inherits \b TXshCellColumn. */
//=============================================================================

class DVAPI TXshPegbarColumn final : public TXshCellColumn {
  PERSIST_DECLARATION(TXshPegbarColumn)

  TStageObjectId m_pegbarObjectId;

public:
  TXshPegbarColumn();
  ~TXshPegbarColumn();

  TXshColumn::ColumnType getColumnType() const override;
  TXshPegbarColumn* getPegbarColumn() override { return this; }

  // Technically level pegbar columns are always empty but needs to be regarded
  // as not
  bool isEmpty() const override { return false; }

  bool canSetCell(const TXshCell& cell) const override;

  TXshColumn* clone() const override;

  void loadData(TIStream& is) override;
  void saveData(TOStream& os) override;

  TStageObjectId getPegbarObjectId() { return m_pegbarObjectId; }
  void setPegbarObjectId(TStageObjectId pegbarObjectId) {
    m_pegbarObjectId = pegbarObjectId;
  }
};

#ifdef _WIN32
template class DV_EXPORT_API TSmartPointerT<TXshPegbarColumn>;
#endif
typedef TSmartPointerT<TXshPegbarColumn> TXshPegbarColumnP;

#endif  // TXSHPEGBARCOLUMN_INCLUDED
