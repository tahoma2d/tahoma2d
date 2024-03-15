#pragma once

#ifndef INSERTFXPOPUP_H
#define INSERTFXPOPUP_H

#include <QTreeWidget>
#include "toonzqt/dvdialog.h"
#include "tfilepath.h"
#include "tstream.h"

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

// forward declaration
class QTreeWidget;
class QTreeWidgetItem;
class TFx;

#include <QIcon>

//=============================================================================
// FxTree
//-----------------------------------------------------------------------------

class FxTree final : public QTreeWidget {
  Q_OBJECT

public:
  void searchItems(const QString &searchWord = QString());

private:
  void displayAll(QTreeWidgetItem *item);
  void hideAll(QTreeWidgetItem *item);
};

//=============================================================================
// InsertFxPopup
//-----------------------------------------------------------------------------

class InsertFxPopup final : public QFrame {
  Q_OBJECT

  FxTree *m_fxTree;

  TIStream *m_is;
  TFilePath m_presetFolder;

  QIcon m_folderIcon;
  QIcon m_presetIcon;
  QIcon m_fxIcon;

public:
  InsertFxPopup(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~InsertFxPopup();

private:
  TFx *createFx();

  void makeItem(QTreeWidgetItem *parent, std::string fxid);

  void loadFolder(QTreeWidgetItem *parent);
  /*!Return true if preset is loaded.*/
  bool loadPreset(QTreeWidgetItem *item);

  bool loadFx(TFilePath fp);
  void loadMacro();

public slots:
  void onItemDoubleClicked(QTreeWidgetItem *w, int c);
  void onInsert();
  void onReplace();
  void onAdd();

protected:
//  void showEvent(QShowEvent *) override;
//  void hideEvent(QHideEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;

protected slots:
  void updatePresets();
  void removePreset();
  void onSearchTextChanged(const QString &text);
};

#endif  // INSERTFXPOPUP_H
