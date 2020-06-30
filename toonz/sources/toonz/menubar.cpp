

#include "menubar.h"

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "cellselection.h"
#include "mainwindow.h"
#include "menubarpopup.h"
#include "docklayout.h"

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
  QAction *newRoom   = menu->addAction(tr("New Room"));
  connect(newRoom, SIGNAL(triggered()), SLOT(addNewTab()));

  int index = tabAt(event->pos());
  if (index >= 0) {
    m_tabToDeleteIndex = index;
    if (index != currentIndex()) {
      QAction *deleteRoom =
          menu->addAction(tr("Delete Room \"%1\"").arg(tabText(index)));
      connect(deleteRoom, SIGNAL(triggered()), SLOT(deleteTab()));
    }
    //#if defined(_WIN32) || defined(_CYGWIN_)
    //    /*- customize menubar -*/
    //    QAction *customizeMenuBar = menu->addAction(
    //        tr("Customize Menu Bar of Room \"%1\"").arg(tabText(index)));
    //    connect(customizeMenuBar, SIGNAL(triggered()),
    //    SLOT(onCustomizeMenuBar()));
    //#endif
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

  QString question(tr("Are you sure you want to remove room %1")
                       .arg(tabText(m_tabToDeleteIndex)));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
  if (ret == 0 || ret == 2) return;

  emit deleteTabRoom(m_tabToDeleteIndex);
  removeTab(m_tabToDeleteIndex);
  m_tabToDeleteIndex = -1;
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

//-----------------------------------------------------------------------------

void RoomTabWidget::onCustomizeMenuBar() {
  /*- use m_tabToDeleteIndex for index of a room of which menubar is to be
   * customized -*/
  assert(m_tabToDeleteIndex != -1);

  emit customizeMenuBar(m_tabToDeleteIndex);
}

//=============================================================================
// StackedMenuBar
//-----------------------------------------------------------------------------

StackedMenuBar::StackedMenuBar(QWidget *parent) : QStackedWidget(parent) {
  setObjectName("StackedMenuBar");
}

//---------------------------------------------------------------------------------

void StackedMenuBar::createMenuBarByName(const QString &roomName) {
  std::cout << "create " << roomName.toStdString() << std::endl;
#if defined(_WIN32) || defined(_CYGWIN_)
  addWidget(createFullMenuBar());
#else
  /* OSX では stacked menu が動いていないのでとりあえず full のみ作成する */
  addWidget(createFullMenuBar());
#endif
}

//---------------------------------------------------------------------------------

void StackedMenuBar::loadAndAddMenubar(const TFilePath &fp) {
#if defined(_WIN32) || defined(_CYGWIN_)
  QMenuBar *menuBar = loadMenuBar(fp);
  if (menuBar)
    addWidget(menuBar);
  else
    addWidget(createFullMenuBar());
#else
  /* OSX では stacked menu が動いていないのでとりあえず full のみ作成する */
  addWidget(createFullMenuBar());
#endif
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::loadMenuBar(const TFilePath &fp) {
  QFile file(toQString(fp));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return 0;
  }

  QXmlStreamReader reader(&file);

  QMenuBar *menuBar = new QMenuBar(this);
  if (reader.readNextStartElement()) {
    if (reader.name() == "menubar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "menu") {
          QString title = reader.attributes().value("title").toString();
          /*- Menu title will be translated if the title is registered in
           * translation file -*/
          QMenu *menu = new QMenu(tr(title.toStdString().c_str()));

          if (readMenuRecursive(reader, menu))
            menuBar->addMenu(menu);
          else {
            reader.raiseError(tr("Failed to load menu %1").arg(title));
            delete menu;
          }

        } else if (reader.name() == "command") {
          QString cmdName = reader.readElementText();

          QAction *action = CommandManager::instance()->getAction(
              cmdName.toStdString().c_str());
          if (action)
            menuBar->addAction(action);
          else
            reader.raiseError(tr("Failed to add command %1").arg(cmdName));
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  }

  if (reader.hasError()) {
    delete menuBar;
    return 0;
  }
  return menuBar;
}

//---------------------------------------------------------------------------------

bool StackedMenuBar::readMenuRecursive(QXmlStreamReader &reader, QMenu *menu) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "menu") {
      QString title  = reader.attributes().value("title").toString();
      QMenu *subMenu = new QMenu(tr(title.toStdString().c_str()));

      if (readMenuRecursive(reader, subMenu))
        menu->addMenu(subMenu);
      else {
        reader.raiseError(tr("Failed to load menu %1").arg(title));
        delete subMenu;
        return false;
      }

    } else if (reader.name() == "command") {
      QString cmdName = reader.readElementText();
      addMenuItem(menu, cmdName.toStdString().c_str());
    } else if (reader.name() == "command_debug") {
#ifndef NDEBUG
      QString cmdName = reader.readElementText();
      addMenuItem(menu, cmdName.toStdString().c_str());
#else
      reader.skipCurrentElement();
#endif
    } else if (reader.name() == "separator") {
      menu->addSeparator();
      reader.skipCurrentElement();
    } else
      reader.skipCurrentElement();
  }

  return !reader.hasError();
}

//---------------------------------------------------------------------------------

QMenuBar *StackedMenuBar::createFullMenuBar() {
  QMenuBar *fullMenuBar = new QMenuBar(this);
  // Menu' FILE
  QMenu *fileMenu = addMenu(tr("File"), fullMenuBar);
  addMenuItem(fileMenu, MI_NewScene);
  addMenuItem(fileMenu, MI_LoadScene);
  addMenuItem(fileMenu, MI_SaveAll);
  addMenuItem(fileMenu, MI_SaveScene);
  addMenuItem(fileMenu, MI_SaveSceneAs);
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
  addMenuItem(fileMenu, MI_NewProject);
  fileMenu->addSeparator();
  addMenuItem(fileMenu, MI_SaveDefaultSettings);
  // QMenu *projectManagementMenu = fileMenu->addMenu(tr("Project Management"));
  //{
  //  //addMenuItem(projectManagementMenu, MI_ProjectSettings);
  //}
  fileMenu->addSeparator();
  QMenu *importMenu = fileMenu->addMenu(tr("Import"));
  { addMenuItem(importMenu, MI_ImportMagpieFile); }
  QMenu *exportMenu = fileMenu->addMenu(tr("Export"));
  {
    addMenuItem(exportMenu, MI_SoundTrack);
    addMenuItem(exportMenu, MI_ExportXDTS);
    addMenuItem(exportMenu, MI_StopMotionExportImageSequence);
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
  QMenu *editMenu = addMenu(tr("Edit"), fullMenuBar);
  addMenuItem(editMenu, MI_Undo);
  addMenuItem(editMenu, MI_Redo);
  editMenu->addSeparator();
  addMenuItem(editMenu, MI_Cut);
  addMenuItem(editMenu, MI_Copy);
  addMenuItem(editMenu, MI_Paste);
  addMenuItem(editMenu, MI_PasteAbove);
  // addMenuItem(editMenu, MI_PasteNew);
  addMenuItem(editMenu, MI_PasteInto);
  addMenuItem(editMenu, MI_PasteDuplicate);
  addMenuItem(editMenu, MI_Insert);
  addMenuItem(editMenu, MI_InsertAbove);
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
    addMenuItem(arrangeMenu, MI_SendBack);
    addMenuItem(arrangeMenu, MI_SendBackward);
  }

  // Menu' SCAN CLEANUP
  QMenu *scanCleanupMenu = addMenu(tr("Scan && Cleanup"), fullMenuBar);
  addMenuItem(scanCleanupMenu, MI_DefineScanner);
  addMenuItem(scanCleanupMenu, MI_ScanSettings);
  addMenuItem(scanCleanupMenu, MI_Scan);
  addMenuItem(scanCleanupMenu, MI_SetScanCropbox);
  addMenuItem(scanCleanupMenu, MI_ResetScanCropbox);
  scanCleanupMenu->addSeparator();
  addMenuItem(scanCleanupMenu, MI_CleanupSettings);
  addMenuItem(scanCleanupMenu, MI_CleanupPreview);
  addMenuItem(scanCleanupMenu, MI_CameraTest);
  addMenuItem(scanCleanupMenu, MI_Cleanup);
  scanCleanupMenu->addSeparator();
  addMenuItem(scanCleanupMenu, MI_PencilTest);
#ifdef LINETEST
  scanCleanupMenu->addSeparator();
  addMenuItem(scanCleanupMenu, MI_Autocenter);
#endif

  // Menu' LEVEL
  QMenu *levelMenu = addMenu(tr("Level"), fullMenuBar);
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

  // Menu' XSHEET
  QMenu *xsheetMenu = addMenu(tr("Xsheet"), fullMenuBar);
  addMenuItem(xsheetMenu, MI_SceneSettings);
  addMenuItem(xsheetMenu, MI_CameraSettings);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_OpenChild);
  addMenuItem(xsheetMenu, MI_CloseChild);
  addMenuItem(xsheetMenu, MI_SaveSubxsheetAs);
  addMenuItem(xsheetMenu, MI_Collapse);
  addMenuItem(xsheetMenu, MI_Resequence);
  addMenuItem(xsheetMenu, MI_CloneChild);
  addMenuItem(xsheetMenu, MI_ExplodeChild);
  addMenuItem(xsheetMenu, MI_ToggleEditInPlace);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_ApplyMatchLines);
  addMenuItem(xsheetMenu, MI_MergeCmapped);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_MergeColumns);
  addMenuItem(xsheetMenu, MI_DeleteMatchLines);
  addMenuItem(xsheetMenu, MI_DeleteInk);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_InsertFx);
  addMenuItem(xsheetMenu, MI_NewOutputFx);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_InsertSceneFrame);
  addMenuItem(xsheetMenu, MI_RemoveSceneFrame);
  addMenuItem(xsheetMenu, MI_InsertGlobalKeyframe);
  addMenuItem(xsheetMenu, MI_RemoveGlobalKeyframe);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_LipSyncPopup);
  xsheetMenu->addSeparator();
  addMenuItem(xsheetMenu, MI_RemoveEmptyColumns);

  // Menu' CELLS
  QMenu *cellsMenu = addMenu(tr("Cells"), fullMenuBar);
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
  QMenu *playMenu = addMenu(tr("Play"), fullMenuBar);
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
  QMenu *renderMenu = addMenu(tr("Render"), fullMenuBar);
  addMenuItem(renderMenu, MI_PreviewSettings);
  addMenuItem(renderMenu, MI_Preview);
  // addMenuItem(renderMenu, MI_SavePreview);
  addMenuItem(renderMenu, MI_SavePreviewedFrames);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_OutputSettings);
  addMenuItem(renderMenu, MI_Render);
  renderMenu->addSeparator();
  addMenuItem(renderMenu, MI_FastRender);

  // Menu' VIEW
  QMenu *viewMenu = addMenu(tr("View"), fullMenuBar);
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

  // Menu' WINDOWS
  QMenu *windowsMenu = addMenu(tr("Windows"), fullMenuBar);
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
  addMenuItem(windowsMenu, MI_StartupPopup);
  addMenuItem(windowsMenu, MI_OpenGuidedDrawingControls);
  windowsMenu->addSeparator(); 
  addMenuItem(windowsMenu, MI_OpenExport);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_MaximizePanel);
  addMenuItem(windowsMenu, MI_FullScreenWindow);
  windowsMenu->addSeparator();
  addMenuItem(windowsMenu, MI_ResetRoomLayout);

  // Menu' HELP
  QMenu *helpMenu = addMenu(tr("Help"), fullMenuBar);
  addMenuItem(helpMenu, MI_OpenOnlineManual);
  addMenuItem(helpMenu, MI_OpenWhatsNew);
  addMenuItem(helpMenu, MI_OpenCommunityForum);
  helpMenu->addSeparator();
  addMenuItem(helpMenu, MI_OpenReportABug);
  helpMenu->addSeparator();
  addMenuItem(helpMenu, MI_About);

#ifndef NDEBUG
  addMenuItem(fileMenu, "MI_ReloadStyle");
#endif

  return fullMenuBar;
}

//-----------------------------------------------------------------------------

QMenu *StackedMenuBar::addMenu(const QString &menuName, QMenuBar *menuBar) {
  QMenu *menu = new QMenu(menuName, menuBar);
  menuBar->addMenu(menu);
  return menu;
}

//-----------------------------------------------------------------------------

void StackedMenuBar::addMenuItem(QMenu *menu, const char *cmdId) {
  QAction *action = CommandManager::instance()->getAction(cmdId);
  if (!action) return;
  assert(action);  // check MainWindow::defineActions() if assert fails
  menu->addAction(action);
}

//-----------------------------------------------------------------------------

void StackedMenuBar::onIndexSwapped(int firstIndex, int secondIndex) {
  assert(firstIndex >= 0 && secondIndex >= 0);
  QWidget *menuBar = widget(firstIndex);
  removeWidget(menuBar);
  insertWidget(secondIndex, menuBar);
}

//-----------------------------------------------------------------------------

void StackedMenuBar::insertNewMenuBar() {
  insertWidget(0, createFullMenuBar());
}

//-----------------------------------------------------------------------------

void StackedMenuBar::deleteMenuBar(int index) {
  QWidget *menuBar = widget(index);
  removeWidget(menuBar);
  delete menuBar;
}

//-----------------------------------------------------------------------------

void StackedMenuBar::doCustomizeMenuBar(int index) {
  MainWindow *mainWin =
      dynamic_cast<MainWindow *>(TApp::instance()->getMainWindow());
  assert(mainWin);
  Room *room = mainWin->getRoom(index);
  if (!room) return;

  MenuBarPopup mbPopup(room);

  if (mbPopup.exec()) {
    /*- OKが押され、roomname_menubar.xmlが更新された状態 -*/
    /*- xmlファイルからメニューバーを作り直して格納 -*/
    std::string mbFileName = room->getPath().getName() + "_menubar.xml";
    TFilePath mbPath       = ToonzFolder::getMyRoomsDir() + mbFileName;
    if (!TFileStatus(mbPath).isReadable()) {
      DVGui::warning(tr("Cannot open menubar settings file %1")
                         .arg(QString::fromStdString(mbFileName)));
      return;
    }
    QMenuBar *newMenu = loadMenuBar(mbPath);
    if (!newMenu) {
      DVGui::warning(tr("Failed to create menubar"));
      return;
    }

    QWidget *oldMenu = widget(index);
    removeWidget(oldMenu);
    insertWidget(index, newMenu);
    delete oldMenu;

    setCurrentIndex(index);
  }
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
  m_stackedMenuBar = new StackedMenuBar(this);
  m_lockRoomCB     = new QCheckBox(this);
  m_messageLabel   = new QLabel(this);
  m_messageLabel->setStyleSheet("color: lightgreen;");

  m_containerFrame->setObjectName("TopBarTabContainer");
  m_roomTabBar->setObjectName("TopBarTab");
  m_roomTabBar->setDrawBase(false);
  m_lockRoomCB->setObjectName("EditToolLockButton");
  m_lockRoomCB->setToolTip(tr("Lock Rooms Tab"));
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
      menuLayout->addWidget(m_stackedMenuBar, 0);
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
  ret      = ret && connect(m_roomTabBar, SIGNAL(currentChanged(int)),
                       m_stackedMenuBar, SLOT(setCurrentIndex(int)));

  ret = ret && connect(m_roomTabBar, SIGNAL(indexSwapped(int, int)),
                       m_stackedMenuBar, SLOT(onIndexSwapped(int, int)));
  ret = ret && connect(m_roomTabBar, SIGNAL(insertNewTabRoom()),
                       m_stackedMenuBar, SLOT(insertNewMenuBar()));
  ret = ret && connect(m_roomTabBar, SIGNAL(deleteTabRoom(int)),
                       m_stackedMenuBar, SLOT(deleteMenuBar(int)));
  ret = ret && connect(m_roomTabBar, SIGNAL(customizeMenuBar(int)),
                       m_stackedMenuBar, SLOT(doCustomizeMenuBar(int)));
  ret = ret && connect(m_lockRoomCB, SIGNAL(toggled(bool)), m_roomTabBar,
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
