#pragma once

#ifndef PREPRODUCTIONBOARD_H
#define PREPRODUCTIONBOARD_H

#include "tfilepath.h"
#include "dvitemview.h"
#include "saveloadqsettings.h"
#include "filebrowserpopup.h"
#include "filebrowser.h"

#include <QFrame>
#include <QListWidget>
#include <QToolBar>

class InfoViewer;
class ExportScenePopup;
class BoardList;

class BoardSelection final : public QObject, public TSelection {
  Q_OBJECT

  QList<InfoViewer *> m_infoViewers;
  ExportScenePopup *m_exportScenePopup;

  BoardList *m_fileList;

public:
  BoardSelection(BoardList *fileList);
  BoardSelection(const BoardSelection &src);
  ~BoardSelection();

  // commands
  void enableCommands() override;

  std::vector<TFilePath> getSelection() const;

  bool isEmpty() const override;
  int getSelectionCount() const;
  void selectNone() override;

  void duplicateFiles();
  void deleteFiles();
  void showFolderContents();
  void viewFileInfo();

  void addToBatchRenderList();
  void addToBatchCleanupList();

  void loadScene();

  void collectAssets();
  void importScenes();
  void exportScenes();
  void exportScene(TFilePath scenePath);
  void selectAll();
  void cutFiles();
  void copyFiles();
  void pasteFiles();
};

//-----------------------------------------------------------------------------

class BoardList : public QListWidget {
  Q_OBJECT

  BoardSelection *m_selection;
  QSize m_maxWidgetSize;
  FrameCountReader m_frameReader;

  int m_clickedIndex;

  QPoint m_dragStartPosition;
  bool m_startDrag;

public:
  BoardList(QWidget *parent, const QSize &iconSize);
  ~BoardList();

  int countFiles() { return count(); }
  void addFile(const QString &name, const QString &path, int atIndex = -1);
  void addFile(QListWidgetItem *item, int atIndex = -1);
  int getClickedIndex() const { return m_clickedIndex; }

  void toggleIconWrap();

protected:
  QPixmap createThumbnail(const TFilePath &fp);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dropEvent(QDropEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void startDragDrop();

  QSize m_iconSize;

protected slots:
  void onItemClicked(QListWidgetItem *item);
  void onItemDoubleClicked(QListWidgetItem *item);
  void onLoadResources();
  void onToggleIconWrap();
  void onSelectionChanged(const QItemSelection &selected,
                          const QItemSelection &deselected);
  void onRenameFile();
  void onFramesCountsUpdated();
  void onIconGenerated();
  void onExpandSequnce();
  void onCollapseSequnce();
  void onQuickPlay();

signals:
  void itemClicked(int index);
  void toggleIconWrapChanged(bool isWrapping);
  void loadBoard(TFilePath);
  void playBoard();
};

//-----------------------------------------------------------------------------

class BoardButtonBar final : public QToolBar {
  Q_OBJECT

public:
  BoardButtonBar(QWidget *parent, BoardList *fileList);

protected slots:
  void onLoadBoard();

signals:
  void newBoard();
  void resetBoard();
  void clearBoard();
  void newScene();
  void saveBoard();
  void saveAsBoard();
  void loadBoard(TFilePath);
  void playBoard();
};

//-----------------------------------------------------------------------------

class PreproductionBoard final : public QFrame, public SaveLoadQSettings {
  Q_OBJECT

  BoardList *m_fileList;
  TFilePath m_filePath;
  bool m_iconsWrapped;

  bool m_dirtyFlag;

public:
  PreproductionBoard(QWidget *parent, Qt::WindowFlags flags = Qt::WindowFlags(),
                     bool noContextMenu         = false,
                     bool multiSelectionEnabled = false);
  ~PreproductionBoard();

  void setDirtyFlag(bool state);

  // SaveLoadQSettings
  virtual void save(QSettings &settings,
                    bool forPopupIni = false) const override;
  virtual void load(QSettings &settings) override;

  void doSave(TFilePath fp);
  void doLoad(TFilePath fp);

  void updateTitle();

public slots:
  void onExit(bool &);

protected slots:
  void onNewBoard();
  void onResetBoard();
  void onClearBoard();
  void onNewScene();
  void onSaveBoard();
  void onSaveAsBoard();
  void onLoadBoard(TFilePath);
  void onPlayBoard();
  void onListChanged();
};

//-----------------------------------------------------------------------------
class LoadBoardPopup final : public FileBrowserPopup {
  Q_OBJECT

  TFilePath m_filePath;

public:
  LoadBoardPopup();

  bool execute() override;

  TFilePath getLoadFilePath() { return m_filePath; }
};

//-----------------------------------------------------------------------------

class SaveBoardPopup final : public FileBrowserPopup {
  Q_OBJECT

  TFilePath m_filePath;

public:
  SaveBoardPopup();

  bool execute() override;

  TFilePath getSaveFilePath() { return m_filePath; }
};

//-----------------------------------------------------------

#endif  // PREPRODUCTIONBOARD_H
