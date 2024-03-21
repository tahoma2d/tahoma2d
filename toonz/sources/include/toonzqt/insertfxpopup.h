#pragma once

#ifndef INSERTFXPOPUP_H
#define INSERTFXPOPUP_H

#include <QTreeWidget>
#include "toonz/tapplication.h"

#include "../include/toonzqt/dvdialog.h"
#include "../include/tfilepath.h"
#include "../include/tstream.h"

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
  FxTree(QWidget *parent) : QTreeWidget(parent) {}

  void searchItems(const QString &searchWord = QString());

private:
  void displayAll(QTreeWidgetItem *item);
  void hideAll(QTreeWidgetItem *item);

protected:
  void mousePressEvent(QMouseEvent *) override;
};

//=============================================================================
// InsertFxPopup
//-----------------------------------------------------------------------------

class DVAPI InsertFxPopup final : public QFrame {
  Q_OBJECT

  FxTree *m_fxTree;

  TIStream *m_is;
  TFilePath m_presetFolder;

  QIcon m_folderIcon;
  QIcon m_presetIcon;
  QIcon m_fxIcon;

  TApplication *m_app;

public:
  InsertFxPopup(QWidget *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());
  ~InsertFxPopup();

  void setApplication(TApplication *app);

  TFx *createFx();

private:
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
  void contextMenuEvent(QContextMenuEvent *) override;

protected slots:
  void updatePresets();
  void removePreset();
  void onSearchTextChanged(const QString &text);
};

#endif  // INSERTFXPOPUP_H
