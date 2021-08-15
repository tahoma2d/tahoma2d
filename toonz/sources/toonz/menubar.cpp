

#include "menubar.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "cellselection.h"
#include "mainwindow.h"
#include "docklayout.h"
#include "shortcutpopup.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/toonzfolders.h"

// TnzTools includes
#include "tools/toolcommandids.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tconvert.h"
#include "tsystem.h"

// Qt includes
#include <QIcon>
#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QShortcut>
#include <QDesktopServices>
#include <QCheckBox>
#include <QtDebug>
#include <QXmlStreamReader>
#include <QLabel>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

void UrlOpener::open() { QDesktopServices::openUrl(m_url); }

UrlOpener dvHome(QUrl("http://www.toonz.com/"));
UrlOpener manual(QUrl("file:///C:/gmt/butta/M&C in EU.pdf"));

TEnv::IntVar LockRoomTabToggle("LockRoomTabToggle", 1);

//=============================================================================
// RoomTabWidget
//-----------------------------------------------------------------------------

RoomTabWidget::RoomTabWidget(QWidget *parent)
    : QTabBar(parent)
    , m_clickedTabIndex(-1)
    , m_tabToDeleteIndex(-1)
    , m_tabToResetIndex(-1)
    , m_renameTabIndex(-1)
    , m_renameTextField(new DVGui::LineEdit(this))
    , m_isLocked(LockRoomTabToggle != 0) {
  m_renameTextField->hide();
  DockingCheck::instance()->setIsEnabled(LockRoomTabToggle != 0);
  TApp::instance()->setShowTitleBars(LockRoomTabToggle == 0);
  connect(m_renameTextField, SIGNAL(editingFinished()), this,
          SLOT(updateTabName()));
}

//-----------------------------------------------------------------------------

RoomTabWidget::~RoomTabWidget() {}

//-----------------------------------------------------------------------------

void RoomTabWidget::swapIndex(int firstIndex, int secondIndex) {
  QString firstText = tabText(firstIndex);
  removeTab(firstIndex);
  insertTab(secondIndex, firstText);
  emit indexSwapped(firstIndex, secondIndex);

  setCurrentIndex(secondIndex);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mousePressEvent(QMouseEvent *event) {
  m_renameTextField->hide();
  if (event->button() == Qt::LeftButton) {
    if (event->modifiers() == (Qt::ControlModifier | Qt::AltModifier)) {
      TApp::instance()->setCanHideTitleBars(
          !TApp::instance()->getCanHideTitleBars());
      bool canHide = TApp::instance()->getCanHideTitleBars();
      if (m_isLocked && !canHide) {
        TApp::instance()->sendShowTitleBars(true, true);
      } else if (m_isLocked && canHide) {
        TApp::instance()->sendShowTitleBars(false, true);
      } else if (!m_isLocked) {
        TApp::instance()->sendShowTitleBars(true, true);
      }
      return;
    }
    m_clickedTabIndex = tabAt(event->pos());
    if (m_clickedTabIndex < 0) return;
    setCurrentIndex(m_clickedTabIndex);
  }
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_isLocked) return;
  if (event->buttons()) {
    int tabIndex = tabAt(event->pos());
    if (tabIndex == m_clickedTabIndex || tabIndex < 0 || tabIndex >= count() ||
        m_clickedTabIndex < 0)
      return;
    swapIndex(m_clickedTabIndex, tabIndex);
    m_clickedTabIndex = tabIndex;
  }
}

//-----------------------------------------------------------------------------

void RoomTabWidget::mouseReleaseEvent(QMouseEvent *event) {
  m_clickedTabIndex = -1;
}

//-----------------------------------------------------------------------------
/*! Set a text field with focus in event position to edit tab name.
 */
void RoomTabWidget::mouseDoubleClickEvent(QMouseEvent *event) {
  if (m_isLocked) return;
  int index = tabAt(event->pos());
  if (index < 0) return;
  m_renameTabIndex     = index;
  DVGui::LineEdit *fld = m_renameTextField;
  fld->setText(tabText(index));
  fld->setGeometry(tabRect(index));
  fld->show();
  fld->selectAll();
  fld->setFocus(Qt::OtherFocusReason);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::contextMenuEvent(QContextMenuEvent *event) {
  if (m_isLocked) return;
  m_tabToDeleteIndex = -1;
  QMenu *menu        = new QMenu(this);

  int index = tabAt(event->pos());

  if (index >= 0) {
    m_tabToResetIndex       = index;
    QAction *resetRoomSaved = menu->addAction(
        tr("Reset Room \"%1\" to Saved Layout").arg(tabText(index)));
    connect(resetRoomSaved, SIGNAL(triggered()), SLOT(resetTabSaved()));
    if (m_tabToResetIndex == currentIndex())
      resetRoomSaved->setEnabled(true);
    else
      resetRoomSaved->setEnabled(false);

    QAction *resetRoomDefault = menu->addAction(
        tr("Reset Room \"%1\" to Default Layout").arg(tabText(index)));
    connect(resetRoomDefault, SIGNAL(triggered()), SLOT(resetTabDefault()));
    resetRoomDefault->setEnabled(false);
    if (m_tabToResetIndex == currentIndex()) {
      MainWindow *mainWin =
          dynamic_cast<MainWindow *>(TApp::instance()->getMainWindow());
      assert(mainWin);
      Room *room   = mainWin->getRoom(index);
      TFilePath fp = room->getPath();
      TFilePath defaultfp =
          ToonzFolder::getTemplateRoomsDir() + TFilePath(fp.getLevelName());
      if (TFileStatus(defaultfp).doesExist())
        resetRoomDefault->setEnabled(true);
    }
  }

  QAction *newRoom = menu->addAction(tr("New Room"));
  connect(newRoom, SIGNAL(triggered()), SLOT(addNewTab()));

  if (index >= 0) {
    m_tabToDeleteIndex = index;
    QAction *deleteRoom =
        menu->addAction(tr("Delete Room \"%1\"").arg(tabText(index)));
    connect(deleteRoom, SIGNAL(triggered()), SLOT(deleteTab()));
    deleteRoom->setEnabled(false);
    if (index != currentIndex()) deleteRoom->setEnabled(true);
  }
  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void RoomTabWidget::updateTabName() {
  int index = m_renameTabIndex;
  if (index < 0) return;
  m_renameTabIndex = -1;
  QString newName  = m_renameTextField->text();
  newName          = newName.simplified();
  newName.replace(" ", "");
  setTabText(index, newName);
  m_renameTextField->hide();
  emit renameTabRoom(index, newName);
}

//-----------------------------------------------------------------------------

void RoomTabWidget::addNewTab() {
  insertTab(0, tr("Room"));
  emit insertNewTabRoom();
}

//-----------------------------------------------------------------------------

void RoomTabWidget::deleteTab() {
  assert(m_tabToDeleteIndex != -1);

  QString question(tr("Are you sure you want to remove room \"%1\"?")
                       .arg(tabText(m_tabToDeleteIndex)));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
  if (ret == 0 || ret == 2) return;

  emit deleteTabRoom(m_tabToDeleteIndex);
  removeTab(m_tabToDeleteIndex);
  m_tabToDeleteIndex = -1;
}

//-----------------------------------------------------------------------------

void RoomTabWidget::resetTabSaved() {
  assert(m_tabToResetIndex != -1);

  QString question(
      tr("Are you sure you want to reset room \"%1\" to the last saved layout?")
          .arg(tabText(m_tabToResetIndex)));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
  if (ret == 0 || ret == 2) return;

  MainWindow *mainWin =
      dynamic_cast<MainWindow *>(TApp::instance()->getMainWindow());
  assert(mainWin);
  Room *room = mainWin->getRoom(m_tabToResetIndex);

  room->reload();
  setTabText(m_tabToResetIndex, room->getName());

  m_tabToResetIndex = -1;
}

//-----------------------------------------------------------------------------

void RoomTabWidget::resetTabDefault() {
  assert(m_tabToResetIndex != -1);

  QString question(
      tr("Are you sure you want to reset room \"%1\" to the default layout?")
          .arg(tabText(m_tabToResetIndex)));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
  if (ret == 0 || ret == 2) return;

  MainWindow *mainWin =
      dynamic_cast<MainWindow *>(TApp::instance()->getMainWindow());
  assert(mainWin);
  Room *room   = mainWin->getRoom(m_tabToResetIndex);
  TFilePath fp = room->getPath();
  TFilePath defaultfp =
      ToonzFolder::getTemplateRoomsDir() + TFilePath(fp.getLevelName());
  try {
    TSystem::copyFile(fp, defaultfp);
    room->reload();
    setTabText(m_tabToResetIndex, room->getName());
  } catch (...) {
    DVGui::MsgBoxInPopup(DVGui::CRITICAL, QObject::tr("Failed to reset room!"));
  }

  m_tabToResetIndex = -1;
}

//-----------------------------------------------------------------------------

void RoomTabWidget::setIsLocked(bool lock) {
  m_isLocked        = lock;
  LockRoomTabToggle = (lock) ? 1 : 0;
  DockingCheck::instance()->setIsEnabled(lock);
  if (m_isLocked) {
    TApp::instance()->sendShowTitleBars(false);
  } else {
    TApp::instance()->sendShowTitleBars(true, true);
  }
}

//---------------------------------------------------------------------------------

void TopBar::loadMenubar() {
  // Menu' FILE
  QMenu *fileMenu = addMenu(ShortcutTree::tr("File"), m_menuBar);
  addMenuItem(fileMenu, MI_NewScene);
  addMenuItem(fileMenu, MI_LoadScene);
  addMenuItem(fileMenu, MI_SaveAll);
  QMenu *saveOtherMenu = fileMenu->addMenu(tr("Other Save Options"));
  {
    addMenuItem(saveOtherMenu, MI_SaveScene);
    addMenuItem(saveOtherMenu, MI_SaveSceneAs);
  }
  addMenuItem(fileMenu, MI_OpenRecentScene);
  addMenuItem(fileMenu, MI_RevertScene);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_LoadFolder);
  addMenuItem(fileMenu, MI_LoadSubSceneFile);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_ConvertFileWithInput);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_LoadColorModel);
  fileMenu->addSeparator();
  QMenu *projectManagementMenu = fileMenu->addMenu(tr("Project Management"));
  {
    addMenuItem(projectManagementMenu, MI_NewProject);
    addMenuItem(projectManagementMenu, MI_LoadProject);
    addMenuItem(projectManagementMenu, MI_OpenRecentProject);
    projectManagementMenu->addSeparator();
    addMenuItem(projectManagementMenu, MI_ProjectSettings);
    addMenuItem(projectManagementMenu, MI_SaveDefaultSettings);
    // projectManagementMenu->addSeparator();
    // addMenuItem(projectManagementMenu, MI_ClearRecentProject);
  }
  fileMenu->addSeparator();
  QMenu *importMenu = fileMenu->addMenu(tr("Import"));
  { addMenuItem(importMenu, MI_ImportMagpieFile); }
  QMenu *exportMenu = fileMenu->addMenu(tr("Export"));
  {
    addMenuItem(exportMenu, MI_ExportCurrentScene);
    addMenuItem(exportMenu, MI_SoundTrack);
    addMenuItem(exportMenu, MI_ExportXDTS);
    addMenuItem(exportMenu, MI_ExportXsheetPDF);
    addMenuItem(exportMenu, MI_StopMotionExportImageSequence);
    addMenuItem(exportMenu, MI_ExportTvpJson);
  }
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_PrintXsheet);
  addMenuItem(fileMenu, MI_Print);
  addMenuItem(fileMenu, MI_Export);
  fileMenu->addSeparator();
  QMenu *scriptMenu = fileMenu->addMenu(tr("Script"));
  {
    addMenuItem(scriptMenu, "MI_RunScript");
    addMenuItem(scriptMenu, "MI_OpenScriptConsole");
  }
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_Preferences);
  addMenuItem(fileMenu, MI_ShortcutPopup);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_ClearCacheFolder);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_Quit);

  // Menu' EDIT
  QMenu *editMenu = addMenu(ShortcutTree::tr("Edit"), m_menuBar);
  addMenuItem(editMenu, MI_Undo);
  addMenuItem(editMenu, MI_Redo);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Cut);
  addMenuItem(editMenu, MI_Copy);
  addMenuItem(editMenu, MI_Paste);
  addMenuItem(editMenu, MI_PasteBelow);
  // addMenuItem(editMenu, MI_PasteNew);
  addMenuItem(editMenu, MI_PasteInto);
  addMenuItem(editMenu, MI_PasteDuplicate);
  addMenuItem(editMenu, MI_Insert);
  addMenuItem(editMenu, MI_InsertBelow);
  addMenuItem(editMenu, MI_Clear);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_SelectAll);
  addMenuItem(editMenu, MI_InvertSelection);
  editMenu->addSeparator();
  QMenu *groupMenu = editMenu->addMenu(tr("Group"));
  {
    addMenuItem(groupMenu, MI_Group);
    addMenuItem(groupMenu, MI_Ungroup);
    addMenuItem(groupMenu, MI_EnterGroup);
    addMenuItem(groupMenu, MI_ExitGroup);
  }
  editMenu->addSeparator();
  QMenu *arrangeMenu = editMenu->addMenu(tr("Arrange"));
  {
    addMenuItem(arrangeMenu, MI_BringToFront);
    addMenuItem(arrangeMenu, MI_BringForward);
    addMenuItem(arrangeMenu, MI_SendBackward);
    addMenuItem(arrangeMenu, MI_SendBack);
  }

  // Menu' Scene
  QMenu *sceneMenu = addMenu(ShortcutTree::tr("Scene"), m_menuBar);
  addMenuItem(sceneMenu, MI_SceneSettings);
  addMenuItem(sceneMenu, MI_CameraSettings);
  sceneMenu->addSeparator();
  addMenuItem(sceneMenu, MI_OpenChild);
  addMenuItem(sceneMenu, MI_CloseChild);
  addMenuItem(sceneMenu, MI_SaveSubxsheetAs);
  addMenuItem(sceneMenu, MI_Collapse);
  addMenuItem(sceneMenu, MI_Resequence);
  addMenuItem(sceneMenu, MI_CloneChild);
  addMenuItem(sceneMenu, MI_ExplodeChild);
  addMenuItem(sceneMenu, MI_ToggleEditInPlace);
  sceneMenu->addSeparator();
  addMenuItem(sceneMenu, MI_ApplyMatchLines);
  addMenuItem(sceneMenu, MI_MergeCmapped);
  sceneMenu->addSeparator();
  addMenuItem(sceneMenu, MI_MergeColumns);
  addMenuItem(sceneMenu, MI_DeleteMatchLines);
  addMenuItem(sceneMenu, MI_DeleteInk);
  sceneMenu->addSeparator();
  addMenuItem(sceneMenu, MI_InsertFx);
  addMenuItem(sceneMenu, MI_NewOutputFx);
  sceneMenu->addSeparator();
  addMenuItem(sceneMenu, MI_InsertSceneFrame);
  addMenuItem(sceneMenu, MI_RemoveSceneFrame);
  addMenuItem(sceneMenu, MI_InsertGlobalKeyframe);
  addMenuItem(sceneMenu, MI_RemoveGlobalKeyframe);
  sceneMenu->addSeparator();
  addMenuItem(sceneMenu, MI_LipSyncPopup);
  sceneMenu->addSeparator();
  addMenuItem(sceneMenu, MI_RemoveEmptyColumns);

  // Menu' LEVEL
  QMenu *levelMenu = addMenu(ShortcutTree::tr("Level"), m_menuBar);
  QMenu *newMenu   = levelMenu->addMenu(tr("New"));
  {
    addMenuItem(newMenu, MI_NewLevel);
    newMenu->addSeparator();
    addMenuItem(newMenu, MI_NewToonzRasterLevel);
    addMenuItem(newMenu, MI_NewVectorLevel);
    addMenuItem(newMenu, MI_NewRasterLevel);
    addMenuItem(newMenu, MI_NewNoteLevel);
  }
  addMenuItem(levelMenu, MI_LoadLevel);
  addMenuItem(levelMenu, MI_SaveLevel);
  addMenuItem(levelMenu, MI_SaveLevelAs);
  addMenuItem(levelMenu, MI_SaveAllLevels);
  addMenuItem(levelMenu, MI_OpenRecentLevel);
  addMenuItem(levelMenu, MI_ExportLevel);
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_AddFrames);
  addMenuItem(levelMenu, MI_ClearFrames);
  addMenuItem(levelMenu, MI_Renumber);
  addMenuItem(levelMenu, MI_ReplaceLevel);
  addMenuItem(levelMenu, MI_RevertToCleanedUp);
  addMenuItem(levelMenu, MI_RevertToLastSaved);
  addMenuItem(levelMenu, MI_Tracking);
  levelMenu->addSeparator();
  QMenu *adjustMenu = levelMenu->addMenu(tr("Adjust"));
  {
    addMenuItem(adjustMenu, MI_BrightnessAndContrast);
    addMenuItem(adjustMenu, MI_AdjustLevels);
    addMenuItem(adjustMenu, MI_AdjustThickness);
    addMenuItem(adjustMenu, MI_Antialias);
    addMenuItem(adjustMenu, MI_Binarize);
    addMenuItem(adjustMenu, MI_LinesFade);
  }
  QMenu *optimizeMenu = levelMenu->addMenu(tr("Optimize"));
  {
    addMenuItem(optimizeMenu, MI_RemoveEndpoints);
    addMenuItem(optimizeMenu, MI_ConvertVectorToVector);
  }
  QMenu *convertMenu = levelMenu->addMenu(tr("Convert"));
  {
    addMenuItem(convertMenu, MI_ConvertToVectors);
    addMenuItem(convertMenu, MI_ConvertToToonzRaster);
  }
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_ExposeResource);
  addMenuItem(levelMenu, MI_EditLevel);
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_CanvasSize);
  addMenuItem(levelMenu, MI_LevelSettings);
  addMenuItem(levelMenu, MI_FileInfo);
  addMenuItem(levelMenu, MI_ReplaceParentDirectory);
  levelMenu->addSeparator();
  addMenuItem(levelMenu, MI_RemoveUnused);

  // Menu' CELLS
  QMenu *cellsMenu = addMenu(ShortcutTree::tr("Cells"), m_menuBar);
  addMenuItem(cellsMenu, MI_Reverse);
  addMenuItem(cellsMenu, MI_Swing);
  addMenuItem(cellsMenu, MI_Random);
  addMenuItem(cellsMenu, MI_Increment);
  addMenuItem(cellsMenu, MI_Dup);
  cellsMenu->addSeparator();
  QMenu *reframeMenu = cellsMenu->addMenu(tr("Reframe"));
  {
    addMenuItem(reframeMenu, MI_Reframe1);
    addMenuItem(reframeMenu, MI_Reframe2);
    addMenuItem(reframeMenu, MI_Reframe3);
    addMenuItem(reframeMenu, MI_Reframe4);
    addMenuItem(reframeMenu, MI_ReframeWithEmptyInbetweens);
  }
  QMenu *stepMenu = cellsMenu->addMenu(tr("Step"));
  {
    addMenuItem(stepMenu, MI_IncreaseStep);
    addMenuItem(stepMenu, MI_DecreaseStep);
    addMenuItem(stepMenu, MI_ResetStep);
    addMenuItem(stepMenu, MI_Step2);
    addMenuItem(stepMenu, MI_Step3);
    addMenuItem(stepMenu, MI_Step4);
  }
  QMenu *eachMenu = cellsMenu->addMenu(tr("Each"));
  {
    addMenuItem(eachMenu, MI_Each2);
    addMenuItem(eachMenu, MI_Each3);
    addMenuItem(eachMenu, MI_Each4);
  }
  addMenuItem(cellsMenu, MI_Rollup);
  addMenuItem(cellsMenu, MI_Rolldown);
  addMenuItem(cellsMenu, MI_TimeStretch);
  addMenuItem(cellsMenu, MI_AutoInputCellNumber);
  cellsMenu->addSeparator();
  QMenu *drawingSubMenu = cellsMenu->addMenu(tr("Drawing Substitution"));
  {
    addMenuItem(drawingSubMenu, MI_DrawingSubForward);
    addMenuItem(drawingSubMenu, MI_DrawingSubBackward);
    addMenuItem(drawingSubMenu, MI_DrawingSubGroupForward);
    addMenuItem(drawingSubMenu, MI_DrawingSubGroupBackward);
  }
  cellsMenu->addSeparator();
  addMenuItem(cellsMenu, MI_Autorenumber);
  addMenuItem(cellsMenu, MI_CreateBlankDrawing);
  addMenuItem(cellsMenu, MI_Duplicate);
  addMenuItem(cellsMenu, MI_MergeFrames);
  addMenuItem(cellsMenu, MI_CloneLevel);
  cellsMenu->addSeparator();
  addMenuItem(cellsMenu, MI_FillEmptyCell);

  // Menu' PLAY
  QMenu *playMenu = addMenu(ShortcutTree::tr("Play"), m_menuBar);
  addMenuItem(playMenu, MI_Play);
  addMenuItem(playMenu, MI_Pause);
  addMenuItem(playMenu, MI_Loop);
  playMenu->addSeparator();
  addMenuItem(playMenu, MI_FirstFrame);
  addMenuItem(playMenu, MI_LastFrame);
  addMenuItem(playMenu, MI_PrevFrame);
  addMenuItem(playMenu, MI_NextFrame);
  addMenuItem(playMenu, MI_PrevStep);
  addMenuItem(playMenu, MI_NextStep);
  playMenu->addSeparator();
  addMenuItem(playMenu, MI_PrevDrawing);
  addMenuItem(playMenu, MI_NextDrawing);
  addMenuItem(playMenu, MI_PrevKeyframe);
  addMenuItem(playMenu, MI_NextKeyframe);
  playMenu->addSeparator();
  addMenuItem(playMenu, MI_Link);

  // Menu' RENDER
  QMenu *renderMenu = addMenu(ShortcutTree::tr("Render"), m_menuBar);
  addMenuItem(renderMenu, MI_PreviewSettings);
  addMenuItem(renderMenu, MI_Preview);
  // addMenuItem(renderMenu, MI_SavePreview);
  addMenuItem(renderMenu, MI_SavePreviewedFrames);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_OutputSettings);
  addMenuItem(renderMenu, MI_Render);
  addMenuItem(renderMenu, MI_SaveAndRender);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_FastRender);

  // Menu' SCAN CLEANUP
  QMenu *scanCleanupMenu = addMenu(ShortcutTree::tr("Cleanup"), m_menuBar);
  addMenuItem(scanCleanupMenu, MI_CleanupSettings);
  addMenuItem(scanCleanupMenu, MI_CleanupPreview);
  addMenuItem(scanCleanupMenu, MI_CameraTest);
  addMenuItem(scanCleanupMenu, MI_Cleanup);

  // Menu' VIEW
  QMenu *viewMenu = addMenu(ShortcutTree::tr("View"), m_menuBar);
  addMenuItem(viewMenu, MI_ViewTable);
  addMenuItem(viewMenu, MI_ViewCamera);
  addMenuItem(viewMenu, MI_ViewColorcard);
  addMenuItem(viewMenu, MI_ViewBBox);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_SafeArea);
  addMenuItem(viewMenu, MI_FieldGuide);
  addMenuItem(viewMenu, MI_ViewRuler);
  addMenuItem(viewMenu, MI_ViewGuide);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_TCheck);
  addMenuItem(viewMenu, MI_ICheck);
  addMenuItem(viewMenu, MI_Ink1Check);
  addMenuItem(viewMenu, MI_PCheck);
  addMenuItem(viewMenu, MI_IOnly);
  addMenuItem(viewMenu, MI_BCheck);
  addMenuItem(viewMenu, MI_GCheck);
  addMenuItem(viewMenu, MI_ACheck);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_ShiftTrace);
  addMenuItem(viewMenu, MI_EditShift);
  addMenuItem(viewMenu, MI_NoShift);
  addMenuItem(viewMenu, MI_ResetShift);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_VectorGuidedDrawing);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_RasterizePli);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_ShowStatusBar);
  addMenuItem(viewMenu, MI_ToggleTransparent);
  viewMenu->addSeparator();
  addMenuItem(viewMenu, MI_MaximizePanel);
  addMenuItem(viewMenu, MI_FullScreenWindow);

  // Menu' WINDOWS
  QMenu *windowsMenu = addMenu(ShortcutTree::tr("Panels"), m_menuBar);
  // QMenu *workspaceMenu = windowsMenu->addMenu(tr("Workspace"));
  //{
  //  addMenuItem(workspaceMenu, MI_DockingCheck);
  //  workspaceMenu->addSeparator();
  //  addMenuItem(workspaceMenu, MI_ResetRoomLayout);
  //}
  addMenuItem(windowsMenu, MI_OpenCommandToolbar);
  addMenuItem(windowsMenu, MI_OpenToolbar);
  addMenuItem(windowsMenu, MI_OpenToolOptionBar);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenStyleControl);
  addMenuItem(windowsMenu, MI_OpenPalette);
  addMenuItem(windowsMenu, MI_OpenStudioPalette);
  addMenuItem(windowsMenu, MI_OpenColorModel);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenComboViewer);
  addMenuItem(windowsMenu, MI_OpenLevelView);
  addMenuItem(windowsMenu, MI_OpenFileViewer);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenXshView);
  addMenuItem(windowsMenu, MI_OpenTimelineView);
  addMenuItem(windowsMenu, MI_OpenFunctionEditor);
  addMenuItem(windowsMenu, MI_OpenSchematic);
  addMenuItem(windowsMenu, MI_FxParamEditor);
  addMenuItem(windowsMenu, MI_OpenFilmStrip);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenFileBrowser);
  addMenuItem(windowsMenu, MI_OpenFileBrowser2);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenCleanupSettings);
  addMenuItem(windowsMenu, MI_OpenTasks);
  addMenuItem(windowsMenu, MI_OpenBatchServers);
  addMenuItem(windowsMenu, MI_OpenTMessage);
  addMenuItem(windowsMenu, MI_OpenHistoryPanel);
  addMenuItem(windowsMenu, MI_AudioRecording);
  addMenuItem(windowsMenu, MI_OpenStopMotionPanel);
  addMenuItem(windowsMenu, MI_OpenMotionPathPanel);
  addMenuItem(windowsMenu, MI_StartupPopup);
  addMenuItem(windowsMenu, MI_OpenGuidedDrawingControls);
  // windowsMenu->addSeparator();
  // addMenuItem(windowsMenu, MI_OpenExport);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_OpenExport);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_ResetRoomLayout);

  // Menu' HELP
  QMenu *helpMenu = addMenu(ShortcutTree::tr("Help"), m_menuBar);
  addMenuItem(helpMenu, MI_OpenOnlineManual);
  addMenuItem(helpMenu, MI_OpenWhatsNew);
  addMenuItem(helpMenu, MI_OpenCommunityForum);
  helpMenu->addSeparator();
  //  addMenuItem(helpMenu, MI_SupportTahoma2D);
  addMenuItem(helpMenu, MI_OpenReportABug);
  helpMenu->addSeparator();
  addMenuItem(helpMenu, MI_About);

#ifndef NDEBUG
  addMenuItem(fileMenu, "MI_ReloadStyle");
#endif
}

//-----------------------------------------------------------------------------

QMenu *TopBar::addMenu(const QString &menuName, QMenuBar *menuBar) {
  QMenu *menu = new QMenu(menuName, menuBar);
  menuBar->addMenu(menu);
  return menu;
}

//-----------------------------------------------------------------------------

void TopBar::addMenuItem(QMenu *menu, const char *cmdId) {
  QAction *action = CommandManager::instance()->getAction(cmdId);
  if (!action) return;
  assert(action);  // check MainWindow::defineActions() if assert fails
  menu->addAction(action);
}

//=============================================================================
// DvTopBar
//-----------------------------------------------------------------------------

TopBar::TopBar(QWidget *parent) : QToolBar(parent) {
  setAllowedAreas(Qt::TopToolBarArea);
  setMovable(false);
  setFloatable(false);
  setObjectName("TopBar");

  m_containerFrame = new QFrame(this);
  m_roomTabBar     = new RoomTabWidget(this);
  m_menuBar        = new QMenuBar(this);
  m_lockRoomCB     = new QCheckBox(this);
  m_messageLabel   = new QLabel(this);
  m_messageLabel->setStyleSheet("color: lightgreen;");

  m_containerFrame->setObjectName("TopBarTabContainer");
  m_roomTabBar->setObjectName("TopBarTab");
  m_roomTabBar->setDrawBase(false);
  m_lockRoomCB->setObjectName("EditToolLockButton");
  m_lockRoomCB->setToolTip(tr("Lock Rooms"));
  m_lockRoomCB->setChecked(m_roomTabBar->isLocked());
  m_lockRoomCB->setStatusTip(
      tr("Unlocking this enables creating new rooms and rearranging the "
         "workspace."));
  m_lockRoomCB->setWhatsThis(
      tr("Locking this prevents the workspace from being changed and prevents "
         "new rooms from being created.  Unlock this to change the workspace "
         "or create new rooms."));

  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setSpacing(0);
  mainLayout->setMargin(0);
  {
    QVBoxLayout *menuLayout = new QVBoxLayout();
    menuLayout->setSpacing(0);
    menuLayout->setMargin(0);
    {
      menuLayout->addStretch(1);
      menuLayout->addWidget(m_menuBar, 0);
      menuLayout->addStretch(1);
    }
    mainLayout->addLayout(menuLayout);
    mainLayout->addStretch(1);
    mainLayout->addWidget(m_messageLabel, 0);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(m_roomTabBar, 0);
    mainLayout->addSpacing(2);
    mainLayout->addWidget(m_lockRoomCB, 0);
  }
  m_containerFrame->setLayout(mainLayout);
  addWidget(m_containerFrame);

  bool ret = true;
  ret      = ret && connect(m_lockRoomCB, SIGNAL(toggled(bool)), m_roomTabBar,
                       SLOT(setIsLocked(bool)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void TopBar::setMessage(QString message) {
  m_messageLabel->setText(message);
  QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
  m_messageLabel->setGraphicsEffect(eff);
  QPropertyAnimation *a = new QPropertyAnimation(eff, "opacity");
  a->setDuration(4000);
  a->setStartValue(1);
  a->setEndValue(0);
  a->setEasingCurve(QEasingCurve::OutBack);
  a->start(QPropertyAnimation::DeleteWhenStopped);
  connect(a, SIGNAL(finished()), m_messageLabel, SLOT(clear()));
}
