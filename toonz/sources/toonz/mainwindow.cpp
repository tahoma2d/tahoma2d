

#include "mainwindow.h"

// Tnz6 includes
#include "menubar.h"
#include "menubarcommandids.h"
#include "xsheetviewer.h"
#include "viewerpane.h"
#include "flipbook.h"
#include "messagepanel.h"
#include "iocommand.h"
#include "tapp.h"
#include "viewerpane.h"
#include "startuppopup.h"
#include "tooloptionsshortcutinvoker.h"
#include "statusbar.h"
#include "aboutpopup.h"

// TnzTools includes
#include "tools/toolcommandids.h"
#include "tools/toolhandle.h"

// TnzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/viewcommandids.h"
#include "toonzqt/updatechecker.h"
#include "toonzqt/paletteviewer.h"

// TnzLib includes
#include "toonz/toonzfolders.h"
#include "toonz/stage2.h"
#include "toonz/stylemanager.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tproject.h"

// TnzBase includes
#include "tenv.h"

// TnzCore includes
#include "tsystem.h"
#include "timagecache.h"
#include "tthread.h"

// Qt includes
#include <QStackedWidget>
#include <QSettings>
#include <QApplication>
#include <QGLPixelBuffer>
#include <QDebug>
#include <QDesktopServices>
#include <QButtonGroup>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#ifdef _WIN32
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif

TEnv::IntVar ViewCameraToggleAction("ViewCameraToggleAction", 1);
TEnv::IntVar ViewTableToggleAction("ViewTableToggleAction", 0);
TEnv::IntVar FieldGuideToggleAction("FieldGuideToggleAction", 0);
TEnv::IntVar ViewBBoxToggleAction("ViewBBoxToggleAction1", 1);
TEnv::IntVar EditInPlaceToggleAction("EditInPlaceToggleAction", 0);
TEnv::IntVar RasterizePliToggleAction("RasterizePliToggleAction", 0);
TEnv::IntVar SafeAreaToggleAction("SafeAreaToggleAction", 0);
TEnv::IntVar ViewColorcardToggleAction("ViewColorcardToggleAction", 1);
TEnv::IntVar ViewGuideToggleAction("ViewGuideToggleAction", 1);
TEnv::IntVar ViewRulerToggleAction("ViewRulerToggleAction", 1);
TEnv::IntVar TCheckToggleAction("TCheckToggleAction", 0);
TEnv::IntVar ICheckToggleAction("ICheckToggleAction", 0);
TEnv::IntVar Ink1CheckToggleAction("Ink1CheckToggleAction", 0);
TEnv::IntVar PCheckToggleAction("PCheckToggleAction", 0);
TEnv::IntVar IOnlyToggleAction("IOnlyToggleAction", 0);
TEnv::IntVar BCheckToggleAction("BCheckToggleAction", 0);
TEnv::IntVar GCheckToggleAction("GCheckToggleAction", 0);
TEnv::IntVar ACheckToggleAction("ACheckToggleAction", 0);
TEnv::IntVar LinkToggleAction("LinkToggleAction", 0);
TEnv::IntVar ShowStatusBarAction("ShowStatusBarAction", 1);
// TEnv::IntVar DockingCheckToggleAction("DockingCheckToggleAction", 1);
TEnv::IntVar ShiftTraceToggleAction("ShiftTraceToggleAction", 0);
TEnv::IntVar EditShiftToggleAction("EditShiftToggleAction", 0);
TEnv::IntVar NoShiftToggleAction("NoShiftToggleAction", 0);
TEnv::IntVar TouchGestureControl("TouchGestureControl", 0);
TEnv::IntVar TransparencySliderValue("TransparencySliderValue", 50);

//=============================================================================
namespace {
//=============================================================================

// layout file name may be overwritten by the argument
std::string layoutsFileName           = "layouts.txt";
const std::string currentRoomFileName = "currentRoom.txt";
bool scrambledRooms                   = false;

//=============================================================================

bool readRoomList(std::vector<TFilePath> &roomPaths,
                  const QString &argumentLayoutFileName) {
  bool argumentLayoutFileLoaded = false;

  TFilePath fp;
  /*-レイアウトファイルが指定されている場合--*/
  if (!argumentLayoutFileName.isEmpty()) {
    fp = ToonzFolder::getRoomsFile(argumentLayoutFileName.toStdString());
    if (!TFileStatus(fp).doesExist()) {
      DVGui::warning("Room layout file " + argumentLayoutFileName +
                     " not found!");
      fp = ToonzFolder::getRoomsFile(layoutsFileName);
      if (!TFileStatus(fp).doesExist()) return false;
    } else {
      argumentLayoutFileLoaded = true;
      layoutsFileName          = argumentLayoutFileName.toStdString();
    }
  } else {
    fp = ToonzFolder::getRoomsFile(layoutsFileName);
    if (!TFileStatus(fp).doesExist()) return false;
  }

  Tifstream is(fp);
  for (;;) {
    char buffer[1024];
    is.getline(buffer, sizeof(buffer));
    if (is.eof()) break;
    char *s = buffer;
    while (*s == ' ' || *s == '\t') s++;
    char *t = s;
    while (*t && *t != '\r' && *t != '\n') t++;
    while (t > s && (t[-1] == ' ' || t[-1] == '\t')) t--;
    t[0] = '\0';
    if (s[0] == '\0') continue;
    TFilePath roomPath = fp.getParentDir() + s;
    roomPaths.push_back(roomPath);
  }

  return argumentLayoutFileLoaded;
}

//-----------------------------------------------------------------------------

void writeRoomList(std::vector<TFilePath> &roomPaths) {
  TFilePath fp = ToonzFolder::getMyRoomsDir() + layoutsFileName;
  TSystem::touchParentDir(fp);
  Tofstream os(fp);
  if (!os) return;
  for (int i = 0; i < (int)roomPaths.size(); i++) {
    TFilePath roomPath = roomPaths[i];
    assert(roomPath.getParentDir() == fp.getParentDir());
    os << roomPath.withoutParentDir() << "\n";
  }
}

//-----------------------------------------------------------------------------

void writeRoomList(std::vector<Room *> &rooms) {
  std::vector<TFilePath> roomPaths;
  for (int i = 0; i < (int)rooms.size(); i++)
    roomPaths.push_back(rooms[i]->getPath());
  writeRoomList(roomPaths);
}

//-----------------------------------------------------------------------------

void makePrivate(Room *room) {
  TFilePath layoutDir = ToonzFolder::getMyRoomsDir();
  TFilePath roomPath  = room->getPath();
  if (roomPath == TFilePath() || roomPath.getParentDir() != layoutDir) {
    int count = 1;
    for (;;) {
      roomPath = layoutDir + ("room" + std::to_string(count++) + ".ini");
      if (!TFileStatus(roomPath).doesExist()) break;
    }
    room->setPath(roomPath);
    TSystem::touchParentDir(roomPath);
    room->save();
  }
}

//-----------------------------------------------------------------------------

void makePrivate(std::vector<Room *> &rooms) {
  for (int i = 0; i < (int)rooms.size(); i++) makePrivate(rooms[i]);
}

// major version  :  7 bits
// minor version  :  8 bits
// revision number: 16 bits
int get_version_code_from(std::string ver) {
  int version = 0;

  // major version: assume that the major version is less than 127.
  std::string::size_type const a = ver.find('.');
  std::string const major = (a == std::string::npos) ? ver : ver.substr(0, a);
  version += std::stoi(major) << 24;
  if ((a == std::string::npos) || (a + 1 == ver.length())) {
    return version;
  }

  // minor version: assume that the minor version is less than 255.
  std::string::size_type const b = ver.find('.', a + 1);
  std::string const minor        = (b == std::string::npos)
                                ? ver.substr(a + 1)
                                : ver.substr(a + 1, b - a - 1);
  version += std::stoi(minor) << 16;
  if ((b == std::string::npos) || (b + 1 == ver.length())) {
    return version;
  }

  // revision number: assume that the revision number is less than 32767.
  version += std::stoi(ver.substr(b + 1));

  return version;
}

}  // namespace
//=============================================================================

//=============================================================================
// Room
//-----------------------------------------------------------------------------

void Room::save() {
  DockLayout *layout = dockLayout();

  // Now save layout state
  DockLayout::State state        = layout->saveState();
  std::vector<QRect> &geometries = state.first;

  QSettings settings(toQString(getPath()), QSettings::IniFormat);
  settings.remove("");

  settings.beginGroup("room");

  int i;
  for (i = 0; i < layout->count(); ++i) {
    settings.beginGroup("pane_" + QString::number(i));
    TPanel *pane = static_cast<TPanel *>(layout->itemAt(i)->widget());
    settings.setValue("name", pane->objectName());
    settings.setValue("geometry", geometries[i]);  // Use passed geometry
    if (SaveLoadQSettings *persistent =
            dynamic_cast<SaveLoadQSettings *>(pane->widget()))
      persistent->save(settings);
    if (pane->getViewType() != -1)
      // If panel has different viewtypes, store current one
      settings.setValue("viewtype", pane->getViewType());
    if (pane->objectName() == "FlipBook") {
      // Store flipbook's identification number
      FlipBook *flip = static_cast<FlipBook *>(pane->widget());
      settings.setValue("index", flip->getPoolIndex());
    }
    settings.endGroup();
  }

  // Adding hierarchy string
  settings.setValue("hierarchy", state.second);
  settings.setValue("name", QVariant(QString(m_name)));

  settings.endGroup();
}

//-----------------------------------------------------------------------------

std::pair<DockLayout *, DockLayout::State> Room::load(const TFilePath &fp) {
  QSettings settings(toQString(fp), QSettings::IniFormat);

  setPath(fp);

  DockLayout *layout = dockLayout();

  settings.beginGroup("room");
  QStringList itemsList = settings.childGroups();

  std::vector<QRect> geometries;
  unsigned int i;
  for (i = 0; i < itemsList.size(); i++) {
    // Panel i
    // NOTE: Panels have to be retrieved in the precise order they were saved.
    // settings.beginGroup(itemsList[i]);  //NO! itemsList has lexicographical
    // ordering!!
    settings.beginGroup("pane_" + QString::number(i));

    TPanel *pane = 0;
    QString paneObjectName;

    // Retrieve panel name
    QVariant name = settings.value("name");
    if (name.canConvert(QVariant::String)) {
      // Allocate panel
      paneObjectName          = name.toString();
      std::string paneStrName = paneObjectName.toStdString();
      pane = TPanelFactory::createPanel(this, paneObjectName);
      if (SaveLoadQSettings *persistent =
              dynamic_cast<SaveLoadQSettings *>(pane->widget()))
        persistent->load(settings);
    }

    if (!pane) {
      // Allocate a message panel
      MessagePanel *message = new MessagePanel(this);
      message->setWindowTitle(name.toString());
      message->setMessage(
          "This panel is not supported by the currently set license!");

      pane = message;
      pane->setPanelType(paneObjectName.toStdString());
      pane->setObjectName(paneObjectName);
    }

    pane->setObjectName(paneObjectName);

    // Add panel to room
    addDockWidget(pane);

    // Store its geometry
    geometries.push_back(settings.value("geometry").toRect());

    // Restore view type if present
    if (settings.contains("viewtype"))
      pane->setViewType(settings.value("viewtype").toInt());

    // Restore flipbook pool indices
    if (paneObjectName == "FlipBook") {
      int index = settings.value("index").toInt();
      dynamic_cast<FlipBook *>(pane->widget())->setPoolIndex(index);
    }

    settings.endGroup();
  }

  // resolve resize events here to avoid unwanted minimize of floating viewer
  qApp->processEvents();

  DockLayout::State state(geometries, settings.value("hierarchy").toString());

  layout->restoreState(state);

  setName(settings.value("name").toString());
  return std::make_pair(layout, state);
}

//-----------------------------------------------------------------------------

void Room::reload() {
  TFilePath fp = getPath();

  QSettings settings(toQString(fp), QSettings::IniFormat);

  DockLayout *layout = dockLayout();
  std::vector<QRect> geometries;

  hide();
  QRect lgeo = layout->geometry();

  for (int i = layout->count() - 1; i >= 0; i--) {
    TPanel *pane = static_cast<TPanel *>(layout->itemAt(i)->widget());
    removeDockWidget(pane);
  }

  DockLayout::State state(geometries, "-1 ");
  layout->restoreState(state);

  load(fp);

  layout->setGeometry(lgeo);
  layout->redistribute();

  show();
}

//=============================================================================
// MainWindow
//-----------------------------------------------------------------------------

#if QT_VERSION >= 0x050500
MainWindow::MainWindow(const QString &argumentLayoutFileName, QWidget *parent,
                       Qt::WindowFlags flags)
#else
MainWindow::MainWindow(const QString &argumentLayoutFileName, QWidget *parent,
                       Qt::WFlags flags)
#endif
    : QMainWindow(parent, flags)
    , m_saveSettingsOnQuit(true)
    , m_oldRoomIndex(0)
    , m_layoutName("") {
  // store a main window pointer in advance of making its contents
  TApp::instance()->setMainWindow(this);

  m_toolsActionGroup = new QActionGroup(this);
  m_toolsActionGroup->setExclusive(true);
  m_currentRoomsChoice = Preferences::instance()->getCurrentRoomChoice();
  makeTransparencyDialog();
  defineActions();
  // user defined shortcuts will be loaded here
  CommandManager::instance()->loadShortcuts();

  // initialize tool options shortcuts
  ToolOptionsShortcutInvoker::instance()->initialize();

  TApp::instance()->getCurrentScene()->setDirtyFlag(false);

  // La menuBar altro non è che una toolbar
  // in cui posso inserire quanti custom widget voglio.
  m_topBar = new TopBar(this);

  addToolBar(m_topBar);
  addToolBarBreak(Qt::TopToolBarArea);

  m_stackedWidget = new QStackedWidget(this);

  // For the style sheet
  m_stackedWidget->setObjectName("MainStackedWidget");
  m_stackedWidget->setFrameStyle(QFrame::StyledPanel);

  // To give a border to the stackedWidget.
  /*QFrame *centralWidget = new QFrame(this);
centralWidget->setFrameStyle(QFrame::StyledPanel);
centralWidget->setObjectName("centralWidget");
QHBoxLayout *centralWidgetLayout = new QHBoxLayout;
centralWidgetLayout->setMargin(3);
centralWidgetLayout->addWidget(m_stackedWidget);
centralWidget->setLayout(centralWidgetLayout);*/

  setCentralWidget(m_stackedWidget);

  m_statusBar = new StatusBar(this);
  setStatusBar(m_statusBar);
  m_statusBar->setVisible(ShowStatusBarAction == 1 ? true : false);
  TApp::instance()->setStatusBar(m_statusBar);

  m_aboutPopup = new AboutPopup(this);

  // Leggo i settings
  readSettings(argumentLayoutFileName);

  // Setto le stanze
  QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();
  connect(m_stackedWidget, SIGNAL(currentChanged(int)),
          SLOT(onCurrentRoomChanged(int)));
  connect(roomTabWidget, SIGNAL(currentChanged(int)), m_stackedWidget,
          SLOT(setCurrentIndex(int)));

  /*-- タイトルバーにScene名を表示する --*/
  connect(TApp::instance()->getCurrentScene(), SIGNAL(nameSceneChanged()), this,
          SLOT(changeWindowTitle()));

  changeWindowTitle();

  // Connetto i comandi che sono in RoomTabWidget
  connect(roomTabWidget, SIGNAL(indexSwapped(int, int)),
          SLOT(onIndexSwapped(int, int)));
  connect(roomTabWidget, SIGNAL(insertNewTabRoom()), SLOT(insertNewRoom()));
  connect(roomTabWidget, SIGNAL(deleteTabRoom(int)), SLOT(deleteRoom(int)));
  connect(roomTabWidget, SIGNAL(renameTabRoom(int, const QString)),
          SLOT(renameRoom(int, const QString)));

  setCommandHandler("MI_Quit", this, &MainWindow::onQuit);
  setCommandHandler("MI_Undo", this, &MainWindow::onUndo);
  setCommandHandler("MI_Redo", this, &MainWindow::onRedo);
  setCommandHandler("MI_NewScene", this, &MainWindow::onNewScene);
  setCommandHandler("MI_LoadScene", this, &MainWindow::onLoadScene);
  setCommandHandler("MI_LoadSubSceneFile", this, &MainWindow::onLoadSubScene);
  setCommandHandler("MI_ResetRoomLayout", this, &MainWindow::resetRoomsLayout);
  setCommandHandler(MI_AutoFillToggle, this, &MainWindow::autofillToggle);

  setCommandHandler(MI_About, this, &MainWindow::onAbout);
  setCommandHandler(MI_OpenOnlineManual, this, &MainWindow::onOpenOnlineManual);
  //  setCommandHandler(MI_SupportTahoma2D, this,
  //  &MainWindow::onSupportTahoma2D);
  setCommandHandler(MI_OpenWhatsNew, this, &MainWindow::onOpenWhatsNew);
  setCommandHandler(MI_OpenCommunityForum, this,
                    &MainWindow::onOpenCommunityForum);
  setCommandHandler(MI_OpenReportABug, this, &MainWindow::onOpenReportABug);

  setCommandHandler(MI_MaximizePanel, this, &MainWindow::maximizePanel);
  setCommandHandler(MI_FullScreenWindow, this, &MainWindow::fullScreenWindow);
  setCommandHandler("MI_NewVectorLevel", this,
                    &MainWindow::onNewVectorLevelButtonPressed);
  setCommandHandler("MI_NewToonzRasterLevel", this,
                    &MainWindow::onNewToonzRasterLevelButtonPressed);
  setCommandHandler("MI_NewRasterLevel", this,
                    &MainWindow::onNewRasterLevelButtonPressed);
  setCommandHandler(MI_ClearCacheFolder, this, &MainWindow::clearCacheFolder);
  // remove ffmpegCache if still exists from crashed exit
  QString ffmpegCachePath =
      ToonzFolder::getCacheRootFolder().getQString() + "//ffmpeg";
  if (TSystem::doesExistFileOrLevel(TFilePath(ffmpegCachePath))) {
    TSystem::rmDirTree(TFilePath(ffmpegCachePath));
  }
}

//-----------------------------------------------------------------------------

MainWindow::~MainWindow() {
  TEnv::saveAllEnvVariables();
  // cleanup ffmpeg cache
  QString ffmpegCachePath =
      ToonzFolder::getCacheRootFolder().getQString() + "//ffmpeg";
  if (TSystem::doesExistFileOrLevel(TFilePath(ffmpegCachePath))) {
    TSystem::rmDirTree(TFilePath(ffmpegCachePath));
  }
}

//-----------------------------------------------------------------------------

void MainWindow::changeWindowTitle() {
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;

  TProject *project   = scene->getProject();
  QString projectName = QString::fromStdString(project->getName().getName());

  QString sceneName = QString::fromStdWString(scene->getSceneName());

  if (sceneName.isEmpty()) sceneName = tr("Untitled");
  if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString("*");

  /*--- レイアウトファイル名を頭に表示させる ---*/
  if (!m_layoutName.isEmpty()) sceneName.prepend(m_layoutName + " : ");

  QString name = sceneName + " [" + projectName + "] : " +
                 QString::fromStdString(TEnv::getApplicationFullName());

  setWindowTitle(name);
}

//-----------------------------------------------------------------------------

void MainWindow::changeWindowTitle(QString &str) { setWindowTitle(str); }

//-----------------------------------------------------------------------------

void MainWindow::startupFloatingPanels() {
  // Show all floating panels
  DockLayout *currDockLayout = getCurrentRoom()->dockLayout();
  int i;
  for (i = 0; i < currDockLayout->count(); ++i) {
    TPanel *currPanel = static_cast<TPanel *>(currDockLayout->widgetAt(i));
    if (currPanel->isFloating()) currPanel->show();
  }
}

//-----------------------------------------------------------------------------

Room *MainWindow::getRoom(int index) const {
  assert(index >= 0 && index < getRoomCount());
  return dynamic_cast<Room *>(m_stackedWidget->widget(index));
}

//-----------------------------------------------------------------------------
/*! Roomを名前から探す
 */
Room *MainWindow::getRoomByName(QString &roomName) {
  for (int i = 0; i < getRoomCount(); i++) {
    Room *room = dynamic_cast<Room *>(m_stackedWidget->widget(i));
    if (room) {
      if (room->getName() == roomName) return room;
    }
  }
  return 0;
}

//-----------------------------------------------------------------------------

int MainWindow::getRoomCount() const { return m_stackedWidget->count(); }

//-----------------------------------------------------------------------------

void MainWindow::refreshWriteSettings() { writeSettings(); }

//-----------------------------------------------------------------------------

void MainWindow::readSettings(const QString &argumentLayoutFileName) {
  QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();

  m_topBar->loadMenubar();

  std::vector<Room *> rooms;

  // leggo l'elenco dei layout
  std::vector<TFilePath> roomPaths;

  if (readRoomList(roomPaths, argumentLayoutFileName)) {
    if (!argumentLayoutFileName.isEmpty()) {
      /*--
       * タイトルバーに表示するレイアウト名を作る：_layoutがあればそこから省く。無ければ.txtのみ省く
       * --*/
      int pos = (argumentLayoutFileName.indexOf("_layout") == -1)
                    ? argumentLayoutFileName.indexOf(".txt")
                    : argumentLayoutFileName.indexOf("_layout");
      m_layoutName = argumentLayoutFileName.left(pos);
    }
  }

  int i;
  for (i = 0; i < (int)roomPaths.size(); i++) {
    TFilePath roomPath = roomPaths[i];
    if (TFileStatus(roomPath).doesExist()) {
      Room *room = new Room(this);
      m_panelStates.push_back(room->load(roomPath));
      m_stackedWidget->addWidget(room);
      roomTabWidget->addTab(room->getName());

      // room->setDockOptions(QMainWindow::DockOptions(
      //  (QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks) &
      //  ~QMainWindow::AllowTabbedDocks));
      rooms.push_back(room);
    }
  }

  // Read the flipbook history
  FlipBookPool::instance()->load(ToonzFolder::getMyModuleDir() +
                                 TFilePath("fliphistory.ini"));

  if (rooms.empty()) {
    // 2D Room
    Room *room2D = create2DRoom();
    m_stackedWidget->addWidget(room2D);
    rooms.push_back(room2D);

    // Stop Motion Room
    Room *roomStopMotion = createStopMotionRoom();
    m_stackedWidget->addWidget(roomStopMotion);
    rooms.push_back(roomStopMotion);

    // Timing Room
    Room *roomTiming = createTimingRoom();
    m_stackedWidget->addWidget(roomTiming);
    rooms.push_back(roomTiming);

    // FX Room
    Room *roomFX = createFXRoom();
    m_stackedWidget->addWidget(roomFX);
    rooms.push_back(roomFX);

    // BrowserRoom
    Room *browserRoom = createBrowserRoom();
    m_stackedWidget->addWidget(browserRoom);
    rooms.push_back(browserRoom);
  }

  /*- If the layout files were loaded from template, then save them as private
   * ones -*/
  makePrivate(rooms);
  writeRoomList(rooms);

  // Imposto la stanza corrente
  TFilePath fp = ToonzFolder::getRoomsFile(currentRoomFileName);
  Tifstream is(fp);
  std::string currentRoomName;
  is >> currentRoomName;
  if (currentRoomName != "") {
    int count = m_stackedWidget->count();
    int index;
    for (index = 0; index < count; index++)
      if (getRoom(index)->getName().toStdString() == currentRoomName) break;
    if (index < count) {
      m_oldRoomIndex = index;
      roomTabWidget->setCurrentIndex(index);
      m_stackedWidget->setCurrentIndex(index);
    }
  }

  RecentFiles::instance()->loadRecentFiles();
}

//-----------------------------------------------------------------------------

void MainWindow::writeSettings() {
  std::vector<Room *> rooms;
  int i;

  // Flipbook history
  DockLayout *currRoomLayout(getCurrentRoom()->dockLayout());
  for (i = 0; i < currRoomLayout->count(); ++i) {
    // Remove all floating flipbooks from current room and return them into
    // the flipbook pool.
    TPanel *panel = static_cast<TPanel *>(currRoomLayout->itemAt(i)->widget());
    if (panel->isFloating() && panel->getPanelType() == "FlipBook") {
      currRoomLayout->removeWidget(panel);
      FlipBook *flipbook = static_cast<FlipBook *>(panel->widget());
      FlipBookPool::instance()->push(flipbook);
      --i;
    }
  }

  FlipBookPool::instance()->save();

  // Room layouts
  for (i = 0; i < m_stackedWidget->count(); i++) {
    Room *room = getRoom(i);
    rooms.push_back(room);
    room->save();
  }
  if (m_currentRoomsChoice == Preferences::instance()->getCurrentRoomChoice()) {
    writeRoomList(rooms);
  }

  // Current room settings
  Tofstream os(ToonzFolder::getMyRoomsDir() + currentRoomFileName);
  os << getCurrentRoom()->getName().toStdString();

  // Main window settings
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("mainwindow.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);

  settings.setValue("MainWindowGeometry", saveGeometry());
}

//-----------------------------------------------------------------------------

Room *MainWindow::create2DRoom() {
  Room *room = new Room(this);
  room->setName("2D");
  room->setObjectName("2DRoom");

  m_topBar->getRoomTabWidget()->addTab("2D");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // StyleEditor
  TPanel *styleEditorPane = TPanelFactory::createPanel(room, "StyleEditor");
  if (styleEditorPane) {
    styleEditorPane->setGeometry(QRect(34, 30, 259, 358));
    geometries.push_back(styleEditorPane->geometry());
    room->addDockWidget(styleEditorPane);
    layout->dockItem(styleEditorPane);
  }

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(34, 764, 1886, 211));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(297, 30, 1623, 730));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // ToolBar
  TPanel *toolBarPane = TPanelFactory::createPanel(room, "ToolBar");
  if (toolBarPane) {
    toolBarPane->setGeometry(QRect(0, 30, 30, 945));
    geometries.push_back(toolBarPane->geometry());
    room->addDockWidget(toolBarPane);
    layout->dockItem(toolBarPane);
  }

  // ToolOptions
  TPanel *toolOptionsPane = TPanelFactory::createPanel(room, "ToolOptions");
  if (toolOptionsPane) {
    toolOptionsPane->setGeometry(QRect(0, 0, 1920, 26));
    geometries.push_back(toolOptionsPane->geometry());
    room->addDockWidget(toolOptionsPane);
    layout->dockItem(toolOptionsPane);
  }

  // LevelPalette
  TPanel *levelPalettePane = TPanelFactory::createPanel(room, "LevelPalette");
  if (levelPalettePane) {
    levelPalettePane->setGeometry(QRect(34, 392, 259, 368));
    geometries.push_back(levelPalettePane->geometry());
    room->addDockWidget(levelPalettePane);
    layout->dockItem(levelPalettePane);
  }

  DockLayout::State state(geometries, "-1 1 [ 4 [ 3 [ [ [ 0 5 ] 2 ] 1 ] ] ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createStopMotionRoom() {
  Room *room = new Room(this);
  room->setName("StopMotion");
  room->setObjectName("StopMotionRoom");

  m_topBar->getRoomTabWidget()->addTab("StopMotion");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(0, 0, 1529, 774));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(0, 778, 1920, 197));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // Stop Motion Controller
  TPanel *stopMotionControllerPane =
      TPanelFactory::createPanel(room, "StopMotionController");
  if (stopMotionControllerPane) {
    stopMotionControllerPane->setGeometry(QRect(1533, 0, 387, 774));
    geometries.push_back(stopMotionControllerPane->geometry());
    room->addDockWidget(stopMotionControllerPane);
    layout->dockItem(stopMotionControllerPane);
  }

  DockLayout::State state(geometries, "-1 1 [ [ 0 2 ] 1 ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createTimingRoom() {
  Room *room = new Room(this);
  room->setName("Timing");
  room->setObjectName("TimingRoom");

  m_topBar->getRoomTabWidget()->addTab("Timing");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(995, 30, 925, 710));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // ToolBar
  TPanel *toolBarPane = TPanelFactory::createPanel(room, "ToolBar");
  if (toolBarPane) {
    toolBarPane->setGeometry(QRect(0, 0, 35, 995));
    geometries.push_back(toolBarPane->geometry());
    room->addDockWidget(toolBarPane);
    layout->dockItem(toolBarPane);
  }

  // ToolOptions
  TPanel *toolOptionsPane = TPanelFactory::createPanel(room, "ToolOptions");
  if (toolOptionsPane) {
    toolOptionsPane->setGeometry(QRect(39, 0, 1881, 26));
    geometries.push_back(toolOptionsPane->geometry());
    room->addDockWidget(toolOptionsPane);
    layout->dockItem(toolOptionsPane);
  }

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(39, 744, 1881, 251));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // Function Editor
  TPanel *functionEditorPane =
      TPanelFactory::createPanel(room, "FunctionEditor");
  if (functionEditorPane) {
    functionEditorPane->setGeometry(QRect(39, 30, 952, 710));
    geometries.push_back(functionEditorPane->geometry());
    room->addDockWidget(functionEditorPane);
    layout->dockItem(functionEditorPane);
  }

  DockLayout::State state(geometries, "-1 0 [ 1 [ 2 [ 4 0 ] 3 ] ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createFXRoom() {
  Room *room = new Room(this);
  room->setName("FX");
  room->setObjectName("FXRoom");

  m_topBar->getRoomTabWidget()->addTab("FX");

  DockLayout *layout = room->dockLayout();

  std::vector<QRect> geometries;

  // Timeline
  TPanel *timelinePane = TPanelFactory::createPanel(room, "Timeline");
  if (timelinePane) {
    timelinePane->setGeometry(QRect(0, 743, 1920, 252));
    geometries.push_back(timelinePane->geometry());
    room->addDockWidget(timelinePane);
    layout->dockItem(timelinePane);
  }

  // SceneViewer
  TPanel *sceneViewerPane = TPanelFactory::createPanel(room, "SceneViewer");
  if (sceneViewerPane) {
    sceneViewerPane->setGeometry(QRect(849, 0, 1071, 739));
    geometries.push_back(sceneViewerPane->geometry());
    room->addDockWidget(sceneViewerPane);
    layout->dockItem(sceneViewerPane);
  }

  // Schematic
  TPanel *schematicPane = TPanelFactory::createPanel(room, "Schematic");
  if (schematicPane) {
    schematicPane->setGeometry(QRect(0, 0, 845, 739));
    schematicPane->setViewType(0);
    geometries.push_back(schematicPane->geometry());
    room->addDockWidget(schematicPane);
    layout->dockItem(schematicPane);
  }

  DockLayout::State state(geometries, "-1 1 [ [ 2 1 ] 0 ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return room;
}

//-----------------------------------------------------------------------------

Room *MainWindow::createBrowserRoom() {
  Room *browserRoom = new Room(this);
  browserRoom->setName("Browser");
  browserRoom->setObjectName("BrowserRoom");

  m_topBar->getRoomTabWidget()->addTab("Browser");

  DockLayout *layout = browserRoom->dockLayout();

  std::vector<QRect> geometries;

  // Browser
  TPanel *browserPane = TPanelFactory::createPanel(browserRoom, "Browser");
  if (browserPane) {
    browserPane->setGeometry(QRect(0, 0, 1920, 497));
    geometries.push_back(browserPane->geometry());
    browserRoom->addDockWidget(browserPane);
    layout->dockItem(browserPane);
  }

  // Scene Cast
  TPanel *sceneCastPanel = TPanelFactory::createPanel(browserRoom, "SceneCast");
  if (sceneCastPanel) {
    sceneCastPanel->setGeometry(QRect(0, 501, 1920, 494));
    geometries.push_back(sceneCastPanel->geometry());
    browserRoom->addDockWidget(sceneCastPanel);
    layout->dockItem(sceneCastPanel, browserPane, Region::bottom);
  }

  DockLayout::State state(geometries, "-1 1 [ 0 1 ] ");

  layout->restoreState(state);

  m_panelStates.push_back(std::make_pair(layout, state));

  return browserRoom;
}

//-----------------------------------------------------------------------------

Room *MainWindow::getCurrentRoom() const {
  return getRoom(m_stackedWidget->currentIndex());
}

//-----------------------------------------------------------------------------

void MainWindow::onUndo() {
  bool ret = TUndoManager::manager()->undo();
  if (!ret) DVGui::error(QObject::tr("No more Undo operations available."));
}

//-----------------------------------------------------------------------------

void MainWindow::onRedo() {
  bool ret = TUndoManager::manager()->redo();
  if (!ret) DVGui::error(QObject::tr("No more Redo operations available."));
}

//-----------------------------------------------------------------------------

void MainWindow::onNewScene() {
  IoCmd::newScene();
  CommandManager *cm = CommandManager::instance();
  cm->setChecked(MI_ShiftTrace, false);
  cm->setChecked(MI_EditShift, false);
  cm->setChecked(MI_NoShift, false);
  cm->setChecked(MI_VectorGuidedDrawing, false);
}

//-----------------------------------------------------------------------------

void MainWindow::onLoadScene() { IoCmd::loadScene(); }

//-----------------------------------------------------------------------------

void MainWindow::onLoadSubScene() { IoCmd::loadSubScene(); }
//-----------------------------------------------------------------------------

void MainWindow::onUpgradeTabPro() {}

//-----------------------------------------------------------------------------

void MainWindow::onAbout() { m_aboutPopup->exec(); }

//-----------------------------------------------------------------------------

void MainWindow::onOpenOnlineManual() {
  QDesktopServices::openUrl(QUrl(tr("http://tahoma2d.readthedocs.io")));
}

//-----------------------------------------------------------------------------

// void MainWindow::onSupportTahoma2D() {
//  QDesktopServices::openUrl(QUrl("http://patreon.com/jeremybullock"));
//}

//-----------------------------------------------------------------------------

void MainWindow::onOpenWhatsNew() {
  QDesktopServices::openUrl(
      QUrl(tr("https://tahoma2d.readthedocs.io/en/latest/whats_new.html")));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenCommunityForum() {
  QDesktopServices::openUrl(QUrl(tr("https://groups.google.com/g/tahoma2d")));
}

//-----------------------------------------------------------------------------

void MainWindow::onOpenReportABug() {
  QString str = QString(
      tr("To report a bug, click on the button below to open a web browser "
         "window for Tahoma2D's Issues page on https://github.com.  Click on "
         "the 'New issue' button and fill out the form."));

  std::vector<QString> buttons = {QObject::tr("Report a Bug"),
                                  QObject::tr("Close")};
  int ret = DVGui::MsgBox(DVGui::INFORMATION, str, buttons, 1);
  if (ret == 1)
    QDesktopServices::openUrl(
        QUrl("https://github.com/tahoma2d/tahoma2d/issues"));
}
//-----------------------------------------------------------------------------

void MainWindow::autofillToggle() {
  TPaletteHandle *h = TApp::instance()->getCurrentPalette();
  h->toggleAutopaint();
}

void MainWindow::resetRoomsLayout() {
  QString question(
      tr("Are you sure you want to reload and restore default rooms?\nCustom "
         "rooms will not be touched."));
  int ret = DVGui::MsgBox(question, QObject::tr("Yes"), QObject::tr("No"));
  if (ret == 0 || ret == 2) return;

  TFilePath layoutDir = ToonzFolder::getMyRoomsDir();
  if (layoutDir != TFilePath()) {
    TFilePath layoutTemplateDir = ToonzFolder::getTemplateRoomsDir();
    TFilePathSet room_fpset;
    try {
      TSystem::readDirectory(room_fpset, layoutTemplateDir, false, true);
      TFilePathSet::iterator it = room_fpset.begin();
      for (int i = 0; it != room_fpset.end(); it++, i++) {
        TFilePath defaultfp = *it;
        if (defaultfp.getType() != "ini") continue;
        TFilePath fp = layoutDir + defaultfp.getLevelName();
        if (TFileStatus(fp).doesExist()) {
          for (i = 0; i < m_stackedWidget->count(); i++) {
            Room *room = getRoom(i);
            if (room->getPath() == fp) {
              room->reload();
              break;
            }
          }
        } else {
          TSystem::copyFile(fp, defaultfp);

          QTabBar *roomTabWidget = m_topBar->getRoomTabWidget();
          Room *room             = new Room(this);
          room->hide();
          m_panelStates.push_back(room->load(fp));
          m_stackedWidget->addWidget(room);
          roomTabWidget->addTab(room->getName());
          room->show();
        }
      }
    } catch (...) {
    }
  }
}

void MainWindow::maximizePanel() {
  DockLayout *currDockLayout = getCurrentRoom()->dockLayout();
  if (currDockLayout->getMaximized() &&
      currDockLayout->getMaximized()->isMaximized()) {
    currDockLayout->getMaximized()->maximizeDock();  // release maximization
    return;
  }

  QPoint p            = mapFromGlobal(QCursor::pos());
  QWidget *currWidget = currDockLayout->containerOf(p);
  DockWidget *currW   = dynamic_cast<DockWidget *>(currWidget);
  if (currW) currW->maximizeDock();
}

void MainWindow::fullScreenWindow() {
  if (isFullScreen())
    showNormal();
  else {
#if defined(_WIN32)
    // http://doc.qt.io/qt-5/windows-issues.html#fullscreen-opengl-based-windows
    this->winId();
    QWindowsWindowFunctions::setHasBorderInFullScreen(this->windowHandle(),
                                                      true);
#endif
    this->showFullScreen();
  }
}

//-----------------------------------------------------------------------------

void MainWindow::onCurrentRoomChanged(int newRoomIndex) {
  Room *oldRoom            = getRoom(m_oldRoomIndex);
  Room *newRoom            = getRoom(newRoomIndex);
  QList<TPanel *> paneList = oldRoom->findChildren<TPanel *>();

  // Change the parent of all the floating dockWidgets
  for (int i = 0; i < paneList.size(); i++) {
    TPanel *pane = paneList.at(i);
    if (pane->isFloating() && !pane->isHidden()) {
      QRect oldGeometry = pane->geometry();
      // Just setting the new parent is not enough for the new layout manager.
      // Must be removed from the old and added to the new.
      oldRoom->removeDockWidget(pane);
      newRoom->addDockWidget(pane);
      // pane->setParent(newRoom);

      // Some flags are lost when the parent changes.
      pane->setFloating(true);
      pane->setGeometry(oldGeometry);
      pane->show();
    }
  }
  m_oldRoomIndex = newRoomIndex;
  TSelection::setCurrent(0);
}

//-----------------------------------------------------------------------------

void MainWindow::onIndexSwapped(int firstIndex, int secondIndex) {
  assert(firstIndex >= 0 && secondIndex >= 0);
  Room *mainWindow = getRoom(firstIndex);
  m_stackedWidget->removeWidget(mainWindow);
  m_stackedWidget->insertWidget(secondIndex, mainWindow);
}

//-----------------------------------------------------------------------------

void MainWindow::insertNewRoom() {
  Room *room = new Room(this);
  room->setName("room");
  if (m_saveSettingsOnQuit) makePrivate(room);
  m_stackedWidget->insertWidget(0, room);

  // Finally, old room index is increased by 1
  m_oldRoomIndex++;
}

//-----------------------------------------------------------------------------

void MainWindow::deleteRoom(int index) {
  Room *room = getRoom(index);

  TFilePath fp = room->getPath();
  try {
    TSystem::deleteFile(fp);
  } catch (...) {
    DVGui::error(tr("Cannot delete") + toQString(fp));
    // Se non ho rimosso la stanza devo rimettere il tab!!
    m_topBar->getRoomTabWidget()->insertTab(index, room->getName());
    return;
  }

  // The old room index must be updated if index < of it
  if (index < m_oldRoomIndex) m_oldRoomIndex--;

  m_stackedWidget->removeWidget(room);
  m_panelStates.erase(m_panelStates.begin() + index);
  delete room;
}

//-----------------------------------------------------------------------------

void MainWindow::renameRoom(int index, const QString name) {
  Room *room = getRoom(index);
  room->setName(name);
  if (m_saveSettingsOnQuit) room->save();
}

//-----------------------------------------------------------------------------

void MainWindow::onMenuCheckboxChanged() {
  QAction *action = qobject_cast<QAction *>(sender());

  int isChecked = action->isChecked();

  CommandManager *cm = CommandManager::instance();

  if (cm->getAction(MI_ViewCamera) == action)
    ViewCameraToggleAction = isChecked;
  else if (cm->getAction(MI_ViewTable) == action)
    ViewTableToggleAction = isChecked;
  else if (cm->getAction(MI_ToggleEditInPlace) == action)
    EditInPlaceToggleAction = isChecked;
  else if (cm->getAction(MI_ViewBBox) == action)
    ViewBBoxToggleAction = isChecked;
  else if (cm->getAction(MI_FieldGuide) == action)
    FieldGuideToggleAction = isChecked;
  else if (cm->getAction(MI_RasterizePli) == action) {
    if (!QGLPixelBuffer::hasOpenGLPbuffers()) isChecked = 0;
    RasterizePliToggleAction                            = isChecked;
  } else if (cm->getAction(MI_SafeArea) == action)
    SafeAreaToggleAction = isChecked;
  else if (cm->getAction(MI_ViewColorcard) == action)
    ViewColorcardToggleAction = isChecked;
  else if (cm->getAction(MI_ViewGuide) == action)
    ViewGuideToggleAction = isChecked;
  else if (cm->getAction(MI_ViewRuler) == action)
    ViewRulerToggleAction = isChecked;
  else if (cm->getAction(MI_TCheck) == action)
    TCheckToggleAction = isChecked;
  // else if (cm->getAction(MI_DockingCheck) == action)
  //  DockingCheckToggleAction = isChecked;
  else if (cm->getAction(MI_ICheck) == action)
    ICheckToggleAction = isChecked;
  else if (cm->getAction(MI_Ink1Check) == action)
    Ink1CheckToggleAction = isChecked;
  else if (cm->getAction(MI_PCheck) == action)
    PCheckToggleAction = isChecked;
  else if (cm->getAction(MI_BCheck) == action)
    BCheckToggleAction = isChecked;
  else if (cm->getAction(MI_GCheck) == action)
    GCheckToggleAction = isChecked;
  else if (cm->getAction(MI_ACheck) == action)
    ACheckToggleAction = isChecked;
  else if (cm->getAction(MI_IOnly) == action)
    IOnlyToggleAction = isChecked;
  else if (cm->getAction(MI_Link) == action)
    LinkToggleAction = isChecked;
  else if (cm->getAction(MI_ShiftTrace) == action)
    ShiftTraceToggleAction = isChecked;
  else if (cm->getAction(MI_EditShift) == action)
    EditShiftToggleAction = isChecked;
  else if (cm->getAction(MI_NoShift) == action)
    NoShiftToggleAction = isChecked;
  else if (cm->getAction(MI_TouchGestureControl) == action)
    TouchGestureControl = isChecked;
}

//-----------------------------------------------------------------------------

void MainWindow::showEvent(QShowEvent *event) {
  getCurrentRoom()->layout()->setEnabled(true);  // See main function in
                                                 // main.cpp
  if (Preferences::instance()->isStartupPopupEnabled() &&
      !m_startupPopupShown) {
    StartupPopup *startupPopup = new StartupPopup();
    startupPopup->show();
    m_startupPopupShown = true;
  }
}
extern const char *applicationName;
extern const char *applicationVersion;
//-----------------------------------------------------------------------------
void MainWindow::checkForUpdates() {
  // Since there is only a single version of Tahoma, we can do a simple check
  // against a string
  QString updateUrl("https://tahoma2d.org/files/tahoma-version.txt");

  m_updateChecker = new UpdateChecker(updateUrl);
  connect(m_updateChecker, SIGNAL(done(bool)), this,
          SLOT(onUpdateCheckerDone(bool)));
}
//-----------------------------------------------------------------------------

void MainWindow::onUpdateCheckerDone(bool error) {
  if (error) {
    // Get the last update date
    return;
  }

  int const software_version =
      get_version_code_from(TEnv::getApplicationVersion());
  int const latest_version =
      get_version_code_from(m_updateChecker->getLatestVersion().toStdString());
  if (software_version < latest_version) {
    QStringList buttons;
    buttons.push_back(QObject::tr("Visit Web Site"));
    buttons.push_back(QObject::tr("Cancel"));
    DVGui::MessageAndCheckboxDialog *dialog = DVGui::createMsgandCheckbox(
        DVGui::INFORMATION,
        QObject::tr("An update is available for this software.\nVisit the Web "
                    "site for more information."),
        QObject::tr("Check for the latest version on launch."), buttons, 0,
        Qt::Checked);
    int ret = dialog->exec();
    if (dialog->getChecked() == Qt::Unchecked)
      Preferences::instance()->setValue(latestVersionCheckEnabled, false);
    dialog->deleteLater();
    if (ret == 1) {
      // Write the new last date to file
      QDesktopServices::openUrl(QObject::tr(
          "https://github.com/tahoma2d/tahoma2d/releases/latest"));
    }
  }

  disconnect(m_updateChecker);
  m_updateChecker->deleteLater();
}
//-----------------------------------------------------------------------------

void MainWindow::closeEvent(QCloseEvent *event) {
  // il riferimento alla variabile booleana doExit viene passato a tutti gli
  // slot che catturano l'emissione
  // del segnale. Impostando a false tale variabile, l'applicazione non viene
  // chiusa!
  bool doExit = true;
  emit exit(doExit);
  if (!doExit) {
    event->ignore();
    return;
  }

  if (!IoCmd::saveSceneIfNeeded(QApplication::tr("Quit"))) {
    event->ignore();
    return;
  }

  // sto facendo quit. interrompo (il prima possibile) le threads
  IconGenerator::instance()->clearRequests();

  if (m_saveSettingsOnQuit) writeSettings();

  // svuoto la directory di output
  TFilePath outputsDir = TEnv::getStuffDir() + "outputs";
  TFilePathSet fps;
  if (TFileStatus(outputsDir).isDirectory()) {
    TSystem::readDirectory(fps, outputsDir);
    TFilePathSet::iterator fpsIt;
    for (fpsIt = fps.begin(); fpsIt != fps.end(); ++fpsIt) {
      TFilePath fp = *fpsIt;
      if (TSystem::doesExistFileOrLevel(fp)) {
        try {
          TSystem::removeFileOrLevel(fp);
        } catch (...) {
        }
      }
    }
  }

  TImageCache::instance()->clear(true);

  event->accept();
  TThread::shutdown();
  qApp->quit();
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createAction(const char *id, const char *name,
                                  const QString &defaultShortcut,
                                  QString newStatusTip, CommandType type,
                                  const char *iconSVGName) {
  QAction *action = new DVAction(tr(name), this);

#if !defined(_WIN32)
  bool visible = false; //Preferences::instance()->getBoolValue(showIconsInMenu);
  action->setIconVisibleInMenu(visible);
#endif

  // In Qt5.15.2 - Windows, QMenu stylesheet has alignment issue when one item
  // has icon and another has not one. (See
  // https://bugreports.qt.io/browse/QTBUG-90242 for details.) To avoid the
  // problem here we attach the temporary icons for every actions which have no
  // icons. The temporary icon is visible in 20x20 pixel size (for command bar)
  // with first 2 upper-case-letters in the command id, and is transparent in
  // 16x16 pixel size so that it is hidden in menu bar. I put the transparent
  // icon instead of using setIconVisibleInMenu(false) to avoid another
  // alignment issue that the shortcut text overlaps command name.
  if (!iconSVGName || !*iconSVGName) {
#ifdef _WIN32
    action->setIcon(createTemporaryIconFromName(name));
#endif
    // do nothing for other platforms
  } else
    action->setIcon(createQIcon(iconSVGName, false, true));
  addAction(action);
#ifdef MACOSX
  // To prevent the wrong menu items (due to MacOS menu naming conventions),
  // from
  // taking Preferences, Quit, or About roles (sometimes happens unexpectedly in
  // translations) - all menu items should have "NoRole"
  //  except for Preferences, Quit, and About
  if (strcmp(id, MI_Preferences) == 0)
    action->setMenuRole(QAction::PreferencesRole);
  else if (strcmp(id, MI_Quit) == 0)
    action->setMenuRole(QAction::QuitRole);
  else if (strcmp(id, MI_About) == 0)
    action->setMenuRole(QAction::AboutRole);
  else
    action->setMenuRole(QAction::NoRole);
#endif
  CommandManager::instance()->define(id, type, defaultShortcut.toStdString(),
                                     action);
  action->setStatusTip(newStatusTip);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRightClickMenuAction(const char *id,
                                                const char *name,
                                                const QString &defaultShortcut,
                                                const char *iconSVGName,
                                                QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      RightClickMenuCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuFileAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuFileCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuEditAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuEditCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuScanCleanupAction(const char *id,
                                                 const char *name,
                                                 const QString &defaultShortcut,
                                                 const char *iconSVGName,
                                                 QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuScanCleanupCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuLevelAction(const char *id, const char *name,
                                           const QString &defaultShortcut,
                                           const char *iconSVGName,
                                           QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuLevelCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuXsheetAction(const char *id, const char *name,
                                            const QString &defaultShortcut,
                                            const char *iconSVGName,
                                            QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuXsheetCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuCellsAction(const char *id, const char *name,
                                           const QString &defaultShortcut,
                                           const char *iconSVGName,
                                           QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuCellsCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuViewAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuViewCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuWindowsAction(const char *id,
                                             const char *name,
                                             const QString &defaultShortcut,
                                             const char *iconSVGName,
                                             QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuWindowsCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuPlayAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuPlayCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuRenderAction(const char *id, const char *name,
                                            const QString &defaultShortcut,
                                            const char *iconSVGName,
                                            QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuRenderCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuHelpAction(const char *id, const char *name,
                                          const QString &defaultShortcut,
                                          const char *iconSVGName,
                                          QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      MenuHelpCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createRGBAAction(const char *id, const char *name,
                                      const QString &defaultShortcut,
                                      const char *iconSVGName,
                                      QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip, RGBACommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createFillAction(const char *id, const char *name,
                                      const QString &defaultShortcut,
                                      const char *iconSVGName,
                                      QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip, FillCommandType,
                      iconSVGName);
}
//-----------------------------------------------------------------------------

QAction *MainWindow::createMenuAction(const char *id, const char *name,
                                      QList<QString> list,
                                      QString newStatusTip) {
  QMenu *menu     = new DVMenuAction(tr(name), this, list);
  QAction *action = menu->menuAction();
  action->setStatusTip(newStatusTip);
  CommandManager::instance()->define(id, MenuCommandType, "", action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createViewerAction(const char *id, const char *name,
                                        const QString &defaultShortcut,
                                        const char *iconSVGName,
                                        QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip, ZoomCommandType,
                      iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createVisualizationButtonAction(const char *id,
                                                     const char *name,
                                                     const char *iconSVGName,
                                                     QString newStatusTip) {
  return createAction(id, name, "", newStatusTip,
                      VisualizationButtonCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createMiscAction(const char *id, const char *name,
                                      const char *defaultShortcut,
                                      QString newStatusTip) {
  QAction *action = new DVAction(tr(name), this);
  action->setStatusTip(newStatusTip);
  CommandManager::instance()->define(id, MiscCommandType, defaultShortcut,
                                     action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolOptionsAction(const char *id,
                                             const char *name,
                                             const QString &defaultShortcut,
                                             QString newStatusTip) {
  QAction *action = new DVAction(tr(name), this);
  addAction(action);
  action->setStatusTip(newStatusTip);
  CommandManager::instance()->define(id, ToolModifierCommandType,
                                     defaultShortcut.toStdString(), action);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createStopMotionAction(const char *id, const char *name,
                                            const QString &defaultShortcut,
                                            const char *iconSVGName,
                                            QString newStatusTip) {
  return createAction(id, name, defaultShortcut, newStatusTip,
                      StopMotionCommandType, iconSVGName);
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToggle(const char *id, const char *name,
                                  const QString &defaultShortcut,
                                  bool startStatus, CommandType type,
                                  const char *iconSVGName,
                                  QString newStatusTip) {
  QAction *action = createAction(id, name, defaultShortcut, newStatusTip, type,
                                 iconSVGName);
  // Remove if the icon is not set. Checkbox will be drawn by style sheet.
  if (!iconSVGName || !*iconSVGName) action->setIcon(QIcon());
#if defined(_WIN32)
  else {
    bool visible = false; //Preferences::instance()->getBoolValue(showIconsInMenu);
    action->setIconVisibleInMenu(visible);
  }
#endif
  action->setCheckable(true);
  if (startStatus == true) action->trigger();
  bool ret =
      connect(action, SIGNAL(changed()), this, SLOT(onMenuCheckboxChanged()));
  assert(ret);
  return action;
}

//-----------------------------------------------------------------------------

QAction *MainWindow::createToolAction(const char *id, const char *iconName,
                                      const char *name,
                                      const QString &defaultShortcut,
                                      QString newStatusTip) {
  QIcon icon      = createQIcon(iconName);
  QAction *action = new DVAction(icon, tr(name), this);
  action->setCheckable(true);
  action->setActionGroup(m_toolsActionGroup);
  action->setStatusTip(newStatusTip);

  // When the viewer is maximized (not fullscreen) the toolbar is hidden and the
  // actions are disabled,
  // so the tool shortcuts don't work.
  // Adding tool actions to the main window solve this problem!
  addAction(action);

  CommandManager::instance()->define(id, ToolCommandType,
                                     defaultShortcut.toStdString(), action);
  return action;
}

//-----------------------------------------------------------------------------

void MainWindow::defineActions() {
  QString separator = "                    ";
  QAction *menuAct;

  // Menu - File

  createMenuFileAction(MI_NewScene, QT_TR_NOOP("&New Scene"), "Ctrl+N",
                       "new_scene", tr("Create a new scene."));
  createMenuFileAction(MI_LoadScene, QT_TR_NOOP("&Load Scene..."), "Ctrl+L",
                                 "load_scene", tr("Load an existing scene."));
  createMenuFileAction(MI_SaveScene, QT_TR_NOOP("&Save Scene"), "Ctrl+Shift+S",
                       "save_scene", tr("Save ONLY the scene.") + separator +
                       tr("This does NOT save levels or images."));
  createMenuFileAction(MI_SaveSceneAs, QT_TR_NOOP("&Save Scene As..."), "",
                       "save_scene_as", tr("Save ONLY the scene with a new name.") +
                       separator + tr("This does NOT save levels or images."));
  createMenuFileAction(MI_SaveAll, QT_TR_NOOP("&Save All"), "Ctrl+S",
                       "saveall", tr("Save the scene info and the levels and images.") +
                       separator + tr("Saves everything."));
  menuAct = createMenuFileAction(MI_RevertScene, QT_TR_NOOP("&Revert Scene"),
                                 "", "revert_scene",
                                 tr("Revert the scene to its previously saved state."));
  menuAct->setEnabled(false);
  QList<QString> files;
  createMenuFileAction(MI_LoadFolder, QT_TR_NOOP("&Load Folder..."), "",
                       "load_folder", tr("Load the contents of a folder into the current scene."));
  createMenuFileAction(MI_LoadSubSceneFile,
                       QT_TR_NOOP("&Load As Sub-Scene..."), "", "load_as_sub_xsheet",
                       tr("Load an existing scene into the current scene as a sub-scene"));
  createMenuAction(MI_OpenRecentScene, QT_TR_NOOP("&Open Recent Scene File"), files,
                   tr("Load a recently used scene."));
  createMenuAction(MI_OpenRecentLevel, QT_TR_NOOP("&Open Recent Level File"), files,
                   tr("Load a recently used level."));
  createMenuFileAction(MI_ClearRecentScene,
                       QT_TR_NOOP("&Clear Recent Scene File List"), "", "",
                       tr("Remove everything from the recent scene list."));
  createMenuFileAction(MI_ClearRecentLevel,
                       QT_TR_NOOP("&Clear Recent level File List"), "", "",
                       tr("Remove everything from the recent level list."));
  createMenuFileAction(MI_ConvertFileWithInput, QT_TR_NOOP("&Convert File..."), "",
                       "convert", tr("Convert an existing file or image sequence to another format."));
  createMenuFileAction(MI_LoadColorModel, QT_TR_NOOP("&Load Color Model..."),
                       "", "load_colormodel", tr("Load an image as a color guide."));
  createMenuFileAction(MI_ImportMagpieFile,
                       QT_TR_NOOP("&Import Toonz Lip Sync File..."), "", "dialogue_import",
                       tr("Import a lip sync file to be applied to a level."));
  createMenuFileAction(MI_NewProject, QT_TR_NOOP("&New Project..."), "",
                       "", tr("Create a new project.") + separator +
                       tr("A project is a container for a collection of "
                       "related scenes and drawings."));
  createMenuAction(MI_OpenRecentProject, QT_TR_NOOP("&Open Recent Project"),
                   files, "");
  createMenuFileAction(MI_LoadProject, QT_TR_NOOP("&Load Project..."), "", "",
                       tr("Load an existing project."));
  createMenuFileAction(MI_ProjectSettings, QT_TR_NOOP("&Project Settings..."),
                       "");
  createMenuFileAction(MI_SaveDefaultSettings, QT_TR_NOOP("&Save Default Settings"), "",
                       "", tr("Use the current scene's settings as a template for "
                       "all new scenes in the current project."));
  createMenuFileAction(MI_SoundTrack, QT_TR_NOOP("&Export Soundtrack"), "",
                       "", tr("Exports the soundtrack to the current scene as a wav file."));
  createMenuFileAction(MI_Preferences, QT_TR_NOOP("&Preferences..."), "Ctrl+U",
                       "gear", tr("Change Tahoma2D's settings."));
  createMenuFileAction(MI_ShortcutPopup, QT_TR_NOOP("&Configure Shortcuts..."), "",
                       "shortcuts", tr("Change the shortcuts of Tahoma2D."));
  createMenuFileAction(MI_PrintXsheet, QT_TR_NOOP("&Print Xsheet"), "",
                       "printer", tr("Print the scene's exposure sheet."));

  createMenuFileAction(MI_ExportXsheetPDF, QT_TR_NOOP("&Export Xsheet to PDF"),
                       "");

  createMenuFileAction(
      MI_ExportXDTS,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Export Exchange Digital Time Sheet (XDTS)"),
      "");
  createMenuFileAction(
      MI_ExportTvpJson,
      QT_TRANSLATE_NOOP("MainWindow", "Export TVPaint JSON File"), "");
  createMenuFileAction("MI_RunScript", QT_TR_NOOP("Run Script..."), "",
                       "run_script", tr("Run a script to perform a series of actions on a scene."));
  createMenuFileAction("MI_OpenScriptConsole",
                       QT_TR_NOOP("Open Script Console..."), "",
                       "console", tr("Open a console window where you can enter script commands."));
  createMenuFileAction(MI_Print, QT_TR_NOOP("&Print Current Frame..."), 
                       "Ctrl+P", "printer");
  createMenuFileAction(MI_Quit, QT_TR_NOOP("&Quit"), "Ctrl+Q", "quit", tr("Bye."));
#ifndef NDEBUG
  createMenuFileAction("MI_ReloadStyle", QT_TR_NOOP("Reload qss"), "");
#endif
  createMenuAction(MI_LoadRecentImage, QT_TR_NOOP("&Load Recent Image Files"),
                   files);
  createMenuFileAction(MI_ClearRecentImage,
                       QT_TR_NOOP("&Clear Recent Flipbook Image List"), "");
  createMenuFileAction(MI_ClearCacheFolder, QT_TR_NOOP("&Clear Cache Folder"),
                       "", "clear_cache");
  createMenuFileAction(MI_ExportCurrentScene,
                       QT_TR_NOOP("&Export Current Scene"), "", "",
                       tr("Export the current scene to another project."));

  // Menu - Edit

  createMenuEditAction(MI_SelectAll, QT_TR_NOOP("&Select All"), "Ctrl+A",
                       "select_all");
  createMenuEditAction(MI_InvertSelection, QT_TR_NOOP("&Invert Selection"), "",
                       "invert_selection");
  createMenuEditAction(MI_Undo, QT_TR_NOOP("&Undo"), "Ctrl+Z", "undo");
  createMenuEditAction(MI_Redo, QT_TR_NOOP("&Redo"), "Ctrl+Y", "redo");
  createMenuEditAction(MI_Cut, QT_TR_NOOP("&Cut"), "Ctrl+X", "cut");
  createMenuEditAction(MI_Copy, QT_TR_NOOP("&Copy"), "Ctrl+C", "content_copy");
  createMenuEditAction(MI_Paste, QT_TR_NOOP("&Paste Insert"), "Ctrl+V",
                       "paste");
  createMenuEditAction(MI_PasteBelow, QT_TR_NOOP("&Paste Insert Below/Before"),
                       "Ctrl+Shift+V", "paste_above_after");
  createMenuEditAction(MI_PasteDuplicate, QT_TR_NOOP("&Paste as a Copy"), "",
                       "paste_duplicate");
  createMenuEditAction(MI_PasteInto, QT_TR_NOOP("&Paste Into"), "",
                       "paste_into");
  createMenuEditAction(MI_Clear, QT_TR_NOOP("&Delete"), "Del", "delete");
  createMenuEditAction(MI_Insert, QT_TR_NOOP("&Insert"), "Ins", "insert");
  createMenuEditAction(MI_InsertBelow, QT_TR_NOOP("&Insert Below/Before"),
                       "Shift+Ins", "insert_above_after");
  createMenuEditAction(MI_Group, QT_TR_NOOP("&Group"), "Ctrl+G", "group");
  createMenuEditAction(MI_Ungroup, QT_TR_NOOP("&Ungroup"), "Ctrl+Shift+G",
                       "ungroup");
  createMenuEditAction(MI_EnterGroup, QT_TR_NOOP("&Enter Group"), "",
                       "enter_group");
  createMenuEditAction(MI_ExitGroup, QT_TR_NOOP("&Exit Group"), "",
                       "leave_group");
  createMenuEditAction(MI_SendBack, QT_TR_NOOP("&Move to Back"), "Ctrl+[",
                       "move_to_back");
  createMenuEditAction(MI_SendBackward, QT_TR_NOOP("&Move Back One"), "[",
                       "move_back_one");
  createMenuEditAction(MI_BringForward, QT_TR_NOOP("&Move Forward One"), "]",
                       "move_forward_one");
  createMenuEditAction(MI_BringToFront, QT_TR_NOOP("&Move to Front"), "Ctrl+]",
                       "move_to_front");
  createMenuFileAction(MI_ClearRecentProject,
                       QT_TR_NOOP("&Clear Recent Project List"), "", "",
                       tr("Remove everything from the recent project list."));
  // createMenuEditAction(MI_PasteNew, QT_TR_NOOP("&Paste New"),  "");
  createMenuEditAction(MI_ClearFrames, QT_TR_NOOP("&Clear Frames"), "");

  // Menu - Cleanup

  createMenuScanCleanupAction(MI_CleanupSettings,
                              QT_TR_NOOP("&Cleanup Settings..."), "",
                              "cleanup_settings");
  menuAct = createToggle(MI_CleanupPreview, QT_TR_NOOP("&Preview Cleanup"), "",
                         0, MenuScanCleanupCommandType, "cleanup_preview");
  CleanupPreviewCheck::instance()->setToggle(menuAct);
  menuAct = createToggle(MI_CameraTest, QT_TR_NOOP("&Camera Test"), "", 0,
                         MenuScanCleanupCommandType, "camera_test");
  CameraTestCheck::instance()->setToggle(menuAct);
  createToggle(MI_OpacityCheck, QT_TR_NOOP("&Opacity Check"), "Alt+1", false,
               MenuScanCleanupCommandType, "opacity_check");
  createMenuScanCleanupAction(MI_Cleanup, QT_TR_NOOP("&Cleanup"), "",
                              "cleanup");

  // Menu - Level

  createMenuLevelAction(MI_NewLevel, QT_TR_NOOP("&New Level..."), "Alt+N",
                        "new_document", tr("Create a new drawing layer."));
  createMenuLevelAction(MI_NewVectorLevel, QT_TR_NOOP("&New Vector Level"), "",
                        "new_vector_level", tr("Create a new vector level.") + 
                        separator + tr("Vectors can be manipulated easily and have "
                        "some extra tools and features."));
  createMenuLevelAction(MI_NewToonzRasterLevel,
                        QT_TR_NOOP("&New Smart Raster Level"), "",
                        "new_toonz_raster_level", tr("Create a new Smart Raster level.") + 
                        separator + tr("Smart Raster levels are color mapped making the colors easier to "
                        "adjust at any time."));
  createMenuLevelAction(MI_NewRasterLevel, QT_TR_NOOP("&New Raster Level"), "",
                        "new_raster_level", tr("Create a new raster level") + separator +
                        tr("Raster levels are traditional drawing levels") + separator +
                        tr("Imported images will be imported as raster levels."));
  createMenuLevelAction(MI_LoadLevel, QT_TR_NOOP("&Load Level..."), "",
                        "load_level", tr("Load an existing level."));
  createMenuLevelAction(MI_SaveLevel, QT_TR_NOOP("&Save Level"), "",
                        "save_level", tr("Save the current level.") + separator +
                        tr("This does not save the scene info."));
  createMenuLevelAction(MI_SaveAllLevels, QT_TR_NOOP("&Save All Levels"), "",
                        "save_all_levels", tr("Save all levels loaded into the scene.") +
                        separator + tr("This does not save the scene info."));
  createMenuLevelAction(MI_SaveLevelAs, QT_TR_NOOP("&Save Level As..."), "",
                        "save_level_as", tr("Save the current level as a different name.") + separator +
                        tr("This does not save the scene info."));
  createMenuLevelAction(MI_ExportLevel, QT_TR_NOOP("&Export Level..."), "",
                        "export_level", tr("Export the current level as an image sequence."));
  createMenuLevelAction(MI_RemoveEndpoints,
                        QT_TR_NOOP("&Remove Vector Overflow"), "",
                        "remove_vector_overflow");
  createMenuLevelAction(MI_AddFrames, QT_TR_NOOP("&Add Frames..."), "",
                        "add_cells");
  createMenuLevelAction(MI_Renumber, QT_TR_NOOP("&Renumber..."), "",
                        "renumber");
  createMenuLevelAction(MI_ReplaceLevel, QT_TR_NOOP("&Replace Level..."), "",
                        "replace_level");
  createMenuLevelAction(MI_RevertToCleanedUp,
                        QT_TR_NOOP("&Revert to Cleaned Up"), "",
                        "revert_level_to_cleanup");
  createMenuLevelAction(MI_RevertToLastSaved, QT_TR_NOOP("&Reload"), "",
                        "reload_level");
  createMenuLevelAction(MI_ExposeResource, QT_TR_NOOP("&Expose in Scene"), "");
  createMenuLevelAction(MI_EditLevel, QT_TR_NOOP("&Display in Level Strip"),
                        "");
  createMenuLevelAction(MI_LevelSettings, QT_TR_NOOP("&Level Settings..."), "",
                        "level_settings");
  createMenuLevelAction(MI_AdjustLevels, QT_TR_NOOP("Adjust Levels..."), "",
                        "histograms");
  createMenuLevelAction(MI_AdjustThickness, QT_TR_NOOP("Adjust Thickness..."),
                        "", "thickness");
  createMenuLevelAction(MI_Antialias, QT_TR_NOOP("&Antialias..."), "",
                        "antialias");
  createMenuLevelAction(MI_Binarize, QT_TR_NOOP("&Binarize..."), "",
                        "binarize");
  createMenuLevelAction(MI_BrightnessAndContrast,
                        QT_TR_NOOP("&Brightness and Contrast..."), "",
                        "brightness_contrast");
  createMenuLevelAction(MI_LinesFade, QT_TR_NOOP("&Color Fade..."), "",
                        "colorfade");
  menuAct = createMenuLevelAction(MI_CanvasSize, QT_TR_NOOP("&Canvas Size..."),
                                  "", "resize");
  menuAct->setDisabled(true);
  createMenuLevelAction(MI_FileInfo, QT_TR_NOOP("&Info..."), "", "level_info");
  createMenuLevelAction(MI_RemoveUnused,
                        QT_TR_NOOP("&Remove All Unused Levels"), "",
                        "remove_unused_levels");
  createMenuLevelAction(MI_ReplaceParentDirectory,
                        QT_TR_NOOP("&Replace Parent Directory..."), "",
                        "replace_parent_directory");
  createMenuLevelAction(MI_NewNoteLevel, QT_TR_NOOP("New Note Level"), "",
                        "new_note_level");
  createMenuLevelAction(MI_ConvertToVectors, 
                        QT_TR_NOOP("Convert to Vectors..."), "");
  createMenuLevelAction(MI_ConvertToToonzRaster,
                        QT_TR_NOOP("Vectors to Smart Raster"), "");
  createMenuLevelAction(
      MI_ConvertVectorToVector,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Replace Vectors with Simplified Vectors"),
      "");
  createMenuLevelAction(MI_Tracking, QT_TR_NOOP("Tracking..."), "",
                        "tracking_options");
  createMenuLevelAction(MI_NewSpline, QT_TR_NOOP("&New Motion Path"), "",
                        "menu_toggle", tr("Create a new motion path.") + separator +
                        tr("Motion paths can be used as animation guides, or you can animate "
                        "objects along a motion path."));

  // Menu - Scene

  createMenuXsheetAction(MI_SceneSettings, QT_TR_NOOP("&Scene Settings..."),
                         "", "scene_settings");
  createMenuXsheetAction(MI_CameraSettings, QT_TR_NOOP("&Camera Settings..."),
                         "", "camera_settings");
  createMenuXsheetAction(MI_OpenChild, QT_TR_NOOP("&Open Sub-Scene"), "",
                         "sub_enter");
  createMenuXsheetAction(MI_CloseChild, QT_TR_NOOP("&Close Sub-Scene"), "",
                         "sub_leave");
  createMenuXsheetAction(MI_ExplodeChild, QT_TR_NOOP("Explode Sub-Scene"), "",
                         "sub_explode");
  createMenuXsheetAction(MI_Collapse, QT_TR_NOOP("Collapse"), "",
                         "sub_collapse");
  createToggle(MI_ToggleEditInPlace, QT_TR_NOOP("&Toggle Edit In Place"), "",
               EditInPlaceToggleAction ? 1 : 0, MenuXsheetCommandType,
               "sub_edit_in_place");
  createMenuXsheetAction(MI_SaveSubxsheetAs,
                         QT_TR_NOOP("&Save Sub-Scene As..."), "",
                         "sub_xsheet_saveas");
  createMenuXsheetAction(MI_CloneChild, QT_TR_NOOP("Clone Sub-Scene"), "",
                         "sub_clone");
  createMenuXsheetAction(MI_ApplyMatchLines,
                         QT_TR_NOOP("&Apply Match Lines..."), "",
                         "apply_match_lines");
  createMenuXsheetAction(MI_MergeCmapped, QT_TR_NOOP("&Merge Tlv Levels..."),
                         "", "merge_levels_tlv");
  createMenuXsheetAction(MI_DeleteMatchLines, QT_TR_NOOP("&Delete Match Lines"),
                         "", "delete_match_lines");
  createMenuXsheetAction(MI_DeleteInk, QT_TR_NOOP("&Delete Lines..."), "",
                         "delete_lines");
  createMenuXsheetAction(MI_MergeColumns, QT_TR_NOOP("&Merge Levels"), "",
                         "merge_levels");
  createMenuXsheetAction(MI_InsertFx, QT_TR_NOOP("&New FX..."), "Ctrl+F",
                         "fx_new");
  createMenuXsheetAction(MI_NewOutputFx, QT_TR_NOOP("&New Output"), "Alt+O",
                         "output");
  createMenuXsheetAction(MI_InsertSceneFrame, QT_TR_NOOP("Insert Frame"), "",
                         "insert_frame");
  createMenuXsheetAction(MI_RemoveSceneFrame, QT_TR_NOOP("Remove Frame"), "",
                         "remove_frame");
  createMenuXsheetAction(MI_InsertGlobalKeyframe,
                         QT_TR_NOOP("Insert Multiple Keys"), "",
                         "insert_multiple_keys");
  createMenuXsheetAction(MI_RemoveGlobalKeyframe,
                         QT_TR_NOOP("Remove Multiple Keys"), "",
                         "remove_multiple_keys");
  createMenuXsheetAction(MI_RemoveEmptyColumns,
                         QT_TR_NOOP("Remove Empty Columns"), "",
                         "remove_empty_columns");
  createMenuXsheetAction(MI_LipSyncPopup,
                         QT_TR_NOOP("&Apply Lip Sync to Column"), "Alt+L",
                         "dialogue");
  createMenuXsheetAction(MI_Resequence, QT_TR_NOOP("Resequence"), "",
                         "resequence");
  createMenuXsheetAction(MI_SetStartMarker, QT_TR_NOOP("Set Start Marker"), "");
  createMenuXsheetAction(MI_SetStopMarker, QT_TR_NOOP("Set Stop Marker"), "");
  createMenuXsheetAction(MI_ClearMarkers, QT_TR_NOOP("Remove Markers"), "");
  createMenuXsheetAction(MI_SetAutoMarkers, QT_TR_NOOP("Set Auto Markers"), "");
  createMenuXsheetAction(MI_PreviewThis,
                         QT_TR_NOOP("Set Markers to Current Frame"), "");

  // Menu - Cells

  createMenuCellsAction(MI_MergeFrames, QT_TR_NOOP("&Merge"), "", "merge");
  createMenuCellsAction(MI_Reverse, QT_TR_NOOP("&Reverse"), "", "reverse");
  createMenuCellsAction(MI_Swing, QT_TR_NOOP("&Swing"), "", "swing");
  createMenuCellsAction(MI_Random, QT_TR_NOOP("&Random"), "", "random");
  createMenuCellsAction(MI_Increment, QT_TR_NOOP("&Autoexpose"), "",
                        "autoexpose");
  createMenuCellsAction(MI_Dup, QT_TR_NOOP("&Repeat..."), "", "repeat");
  createMenuCellsAction(MI_ResetStep, QT_TR_NOOP("&Reset Step"), "",
                        "step_reset");
  createMenuCellsAction(MI_IncreaseStep, QT_TR_NOOP("&Increase Step"), "'",
                        "step_plus");
  createMenuCellsAction(MI_DecreaseStep, QT_TR_NOOP("&Decrease Step"), ";",
                        "step_minus");
  createMenuCellsAction(MI_Step2, QT_TR_NOOP("&Step 2"), "", "step_2");
  createMenuCellsAction(MI_Step3, QT_TR_NOOP("&Step 3"), "", "step_3");
  createMenuCellsAction(MI_Step4, QT_TR_NOOP("&Step 4"), "", "step_4");
  createMenuCellsAction(MI_Each2, QT_TR_NOOP("&Each 2"), "");
  createMenuCellsAction(MI_Each3, QT_TR_NOOP("&Each 3"), "");
  createMenuCellsAction(MI_Each4, QT_TR_NOOP("&Each 4"), "");
  createMenuCellsAction(MI_Rollup, QT_TR_NOOP("&Roll Up"), "", "rollup");
  createMenuCellsAction(MI_Rolldown, QT_TR_NOOP("&Roll Down"), "", "rolldown");
  createMenuCellsAction(MI_TimeStretch, QT_TR_NOOP("&Time Stretch..."), "",
                        "time_stretch");
  createMenuCellsAction(MI_CreateBlankDrawing,
                        QT_TR_NOOP("&Create Blank Drawing"), "Alt+D",
                        "add_cell");
  createMenuCellsAction(MI_Duplicate, QT_TR_NOOP("&Duplicate Drawing  "), "D",
                        "duplicate_drawing");
  createMenuCellsAction(MI_Autorenumber, QT_TR_NOOP("&Autorenumber"), "",
                        "renumber");
  createMenuCellsAction(MI_CloneLevel, QT_TR_NOOP("&Clone Cells"), "",
                        "clone_cells");
  createMenuCellsAction(MI_DrawingSubForward,
                        QT_TR_NOOP("Drawing Substitution Forward"), "W");
  createMenuCellsAction(MI_DrawingSubBackward,
                        QT_TR_NOOP("Drawing Substitution Backward"), "Q");
  createMenuCellsAction(MI_DrawingSubGroupForward,
                        QT_TR_NOOP("Similar Drawing Substitution Forward"),
                        "Alt+W");
  createMenuCellsAction(MI_DrawingSubGroupBackward,
                        QT_TR_NOOP("Similar Drawing Substitution Backward"),
                        "Alt+Q");
  createMenuCellsAction(MI_Reframe1, QT_TR_NOOP("Reframe on 1's"), "", "on_1s");
  createMenuCellsAction(MI_Reframe2, QT_TR_NOOP("Reframe on 2's"), "", "on_2s");
  createMenuCellsAction(MI_Reframe3, QT_TR_NOOP("Reframe on 3's"), "", "on_3s");
  createMenuCellsAction(MI_Reframe4, QT_TR_NOOP("Reframe on 4's"), "", "on_4s");
  createMenuCellsAction(MI_ReframeWithEmptyInbetweens,
                        QT_TR_NOOP("Reframe with Empty Inbetweens..."), "",
                        "on_with_empty");
  createMenuCellsAction(MI_AutoInputCellNumber,
                        QT_TR_NOOP("Auto Input Cell Number..."), "",
                        "auto_input_cell_number");
  createMenuCellsAction(MI_FillEmptyCell, QT_TR_NOOP("&Fill In Empty Cells"),
                        "", "fill_empty_cells");

  // Menu - Play

  createToggle(MI_Link, QT_TR_NOOP("Link Flipbooks"), "",
               LinkToggleAction ? 1 : 0, MenuPlayCommandType, "flipbook_link");
  createMenuPlayAction(MI_Play, QT_TR_NOOP("Play"), "P", "play");
  createMenuPlayAction(MI_ShortPlay, QT_TR_NOOP("Short Play"), "Alt+P");
  createMenuPlayAction(MI_Loop, QT_TR_NOOP("Loop"), "L", "loop");
  createMenuPlayAction(MI_Pause, QT_TR_NOOP("Pause"), "", "pause");
  createMenuPlayAction(MI_FirstFrame, QT_TR_NOOP("First Frame"), "Alt+,",
                       "framefirst");
  createMenuPlayAction(MI_LastFrame, QT_TR_NOOP("Last Frame"), "Alt+.",
                       "framelast");
  createMenuPlayAction(MI_PrevFrame, QT_TR_NOOP("Previous Frame"), "Shift+,",
                       "frameprev");
  createMenuPlayAction(MI_NextFrame, QT_TR_NOOP("Next Frame"), "Shift+.",
                       "framenext");
  createMenuPlayAction(MI_NextDrawing, QT_TR_NOOP("Next Drawing"), ".",
                       "next_drawing");
  createMenuPlayAction(MI_PrevDrawing, QT_TR_NOOP("Previous Drawing"), ",",
                       "prev_drawing");
  createMenuPlayAction(MI_NextStep, QT_TR_NOOP("Next Step"), "", "nextstep");
  createMenuPlayAction(MI_PrevStep, QT_TR_NOOP("Previous Step"), "",
                       "prevstep");
  createMenuPlayAction(MI_NextKeyframe, QT_TR_NOOP("Next Key"), "Ctrl+.",
                       "nextkey");
  createMenuPlayAction(MI_PrevKeyframe, QT_TR_NOOP("Previous Key"), "Ctrl+,",
                       "prevkey");
  
  // Menu - Render
  
  createMenuRenderAction(MI_OutputSettings, QT_TR_NOOP("&Output Settings..."),
                         "Ctrl+O", "output_settings",
                         tr("Control the output settings for the current scene.") +
                         separator + tr("You can render from the output settings window also."));
  createMenuRenderAction(MI_PreviewSettings, QT_TR_NOOP("&Preview Settings..."),
                         "", "preview_settings",
                         tr("Control the settings that will be used to preview the scene."));
  createMenuRenderAction(MI_Render, QT_TR_NOOP("&Render"), "Ctrl+Shift+R",
                         "render_clapboard", tr("Renders according to the settings and "
                         "location set in Output Settings."));
  createMenuRenderAction(MI_FastRender, QT_TR_NOOP("&Fast Render to MP4"),
                         "Alt+R", "fast_render_mp4", tr("Exports an MP4 file to the location specified in the preferences.") +
                         separator + tr("This is quicker than going into the Output Settings "
                         "and setting up an MP4 render."));
  createMenuRenderAction(MI_Preview, 
                         QT_TR_NOOP("&Preview"), "Ctrl+R",
                         "preview", tr("Previews the current scene with all effects applied."));
  createMenuRenderAction(MI_SavePreviewedFrames,
                         QT_TR_NOOP("&Save Previewed Frames"), "",
                         "save_previewed_frames", tr("Save the images created during preview to a specified location."));
  createMenuRenderAction(MI_SaveAndRender, QT_TR_NOOP("&Save and Render"), "", "render",
                         tr("Saves the current scene and renders according to the settings and "
                         "location set in Output Settings."));

  // Menu - View  
 
  createToggle(MI_ViewCamera, QT_TR_NOOP("&Camera Box"), "",
               ViewCameraToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewTable, QT_TR_NOOP("&Table"), "",
               ViewTableToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_FieldGuide, QT_TR_NOOP("&Grids and Overlays"), "Shift+G",
               FieldGuideToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewBBox, QT_TR_NOOP("&Raster Bounding Box"), "",
               ViewBBoxToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_SafeArea, QT_TR_NOOP("&Safe Area"), "",
               SafeAreaToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewColorcard, QT_TR_NOOP("&Camera BG Color"), "",
               ViewColorcardToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewGuide, QT_TR_NOOP("&Guide"), "",
               ViewGuideToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_ViewRuler, QT_TR_NOOP("&Ruler"), "",
               ViewRulerToggleAction ? 1 : 0, MenuViewCommandType);
  createToggle(MI_TCheck, QT_TR_NOOP("&Transparency Check  "), "",
               TCheckToggleAction ? 1 : 0, MenuViewCommandType,
               "transparency_check");
  menuAct = createToggle(MI_ICheck, QT_TR_NOOP("&Ink Check"), "",
                         ICheckToggleAction ? 1 : 0, MenuViewCommandType,
                         "ink_check");
  // make InkCheck and Ink1Check exclusive
  connect(menuAct, SIGNAL(triggered(bool)), this,
          SLOT(onInkCheckTriggered(bool)));
  menuAct = createToggle(MI_Ink1Check, QT_TR_NOOP("&Ink#1 Check"), "",
                         Ink1CheckToggleAction ? 1 : 0, MenuViewCommandType,
                         "ink_no1_check");
  // make InkCheck and Ink1Check exclusive
  connect(menuAct, SIGNAL(triggered(bool)), this,
          SLOT(onInk1CheckTriggered(bool)));
  createToggle(MI_PCheck, QT_TR_NOOP("&Paint Check"), "",
               PCheckToggleAction ? 1 : 0, MenuViewCommandType, "paint_check");
  createToggle(MI_IOnly, QT_TR_NOOP("Inks &Only"), "",
               IOnlyToggleAction ? 1 : 0, MenuViewCommandType, "inks_only");
  createToggle(MI_GCheck, QT_TR_NOOP("&Fill Check"), "",
               GCheckToggleAction ? 1 : 0, MenuViewCommandType, "fill_check");
  createToggle(MI_BCheck, QT_TR_NOOP("&Black BG Check"), "",
               BCheckToggleAction ? 1 : 0, MenuViewCommandType,
               "blackbg_check");
  createToggle(MI_ACheck, QT_TR_NOOP("&Gap Check"), "",
               ACheckToggleAction ? 1 : 0, MenuViewCommandType, "gap_check");
  createToggle(MI_ShiftTrace, QT_TR_NOOP("Shift and Trace"), "", false,
               MenuViewCommandType, "shift_and_trace");
  createToggle(MI_EditShift, QT_TR_NOOP("Edit Shift"), "", false,
               MenuViewCommandType, "shift_and_trace_edit");
  createToggle(MI_NoShift, QT_TR_NOOP("No Shift"), "", false,
               MenuViewCommandType, "shift_and_trace_no_shift");
  CommandManager::instance()->enable(MI_EditShift, false);
  CommandManager::instance()->enable(MI_NoShift, false);
  createAction(MI_ResetShift, QT_TR_NOOP("Reset Shift"), "", "",
               MenuViewCommandType, "shift_and_trace_reset");
  createToggle(MI_VectorGuidedDrawing, QT_TR_NOOP("Vector Guided Drawing"), "",
               Preferences::instance()->isGuidedDrawingEnabled(),
               MenuViewCommandType, "view_guided_drawing");
  if (QGLPixelBuffer::hasOpenGLPbuffers())
    createToggle(MI_RasterizePli, QT_TR_NOOP("&Visualize Vector As Raster"), "",
                 RasterizePliToggleAction ? 1 : 0, MenuViewCommandType,
                 "view_vector_as_raster");
  else
    RasterizePliToggleAction = 0;

  // Menu - Panes

  createMenuWindowsAction(MI_OpenFileBrowser, QT_TR_NOOP("&File Browser"), "",
                         "filebrowser");
  createMenuWindowsAction(MI_OpenFileViewer, QT_TR_NOOP("&Flipbook"), "",
                         "flipbook");
  createMenuWindowsAction(MI_OpenFunctionEditor, QT_TR_NOOP("&Function Editor"),
                          "", "function_editor");
  createMenuWindowsAction(MI_OpenFilmStrip, QT_TR_NOOP("&Level Strip"), "",
                          "level_strip");
  createMenuWindowsAction(MI_OpenPalette, QT_TR_NOOP("&Palette"), "",
                          "palette");
  createMenuWindowsAction(MI_OpenTasks, QT_TR_NOOP("&Tasks"), "", "tasks");
  createMenuWindowsAction(MI_OpenBatchServers, QT_TR_NOOP("&Batch Servers"), "",
                          "batchservers");
  createMenuWindowsAction(MI_OpenTMessage, QT_TR_NOOP("&Message Center"), "",
                          "messagecenter");
  createMenuWindowsAction(MI_OpenColorModel, QT_TR_NOOP("&Color Model"), "",
                          "colormodel");
  createMenuWindowsAction(MI_OpenStudioPalette, QT_TR_NOOP("&Studio Palette"),
                          "", "studiopalette");
  createMenuWindowsAction(MI_OpenSchematic, QT_TR_NOOP("&Schematic"), "",
                          "schematic");
  createMenuWindowsAction(MI_FxParamEditor, QT_TR_NOOP("&FX Editor"), "Ctrl+K",
                          "fx_settings");
  createMenuWindowsAction(MI_OpenCleanupSettings,
                          QT_TR_NOOP("&Cleanup Settings"), "",
                          "cleanup_settings");
  createMenuWindowsAction(MI_OpenFileBrowser2, QT_TR_NOOP("&Scene Cast"), "",
                          "scenecast");
  createMenuWindowsAction(MI_OpenStyleControl, QT_TR_NOOP("&Style Editor"), "",
                          "styleeditor");
  createMenuWindowsAction(MI_OpenToolbar, QT_TR_NOOP("&Toolbar"), "", "tool");
  createMenuWindowsAction(MI_OpenToolOptionBar, QT_TR_NOOP("&Tool Option Bar"),
                          "", "tool_options");
  createMenuWindowsAction(MI_OpenCommandToolbar, QT_TR_NOOP("&Command Bar"), "",
                          "star");
  createMenuWindowsAction(MI_OpenStopMotionPanel,
                          QT_TR_NOOP("&Stop Motion Controls"), "",
                          "camera_capture");
  createMenuWindowsAction(MI_OpenLevelView, QT_TR_NOOP("&Viewer"), "",
                          "viewer");
  createMenuWindowsAction(MI_OpenXshView, QT_TR_NOOP("&Xsheet"), "", "xsheet");
  createMenuWindowsAction(MI_OpenTimelineView, QT_TR_NOOP("&Timeline"), "",
                          "timeline");
  createMenuWindowsAction(MI_OpenComboViewer, QT_TR_NOOP("&ComboViewer"), "",
                          "comboviewer");
  createMenuWindowsAction(MI_OpenHistoryPanel, QT_TR_NOOP("&History"), "Ctrl+H",
                          "history");
  createMenuWindowsAction(MI_AudioRecording, QT_TR_NOOP("Record Audio"),
                          "Alt+A", "recordaudio");
  createMenuWindowsAction(MI_ResetRoomLayout,
                          QT_TR_NOOP("&Reset All Default Rooms"), "");
  createMenuWindowsAction(MI_MaximizePanel, QT_TR_NOOP("Toggle Maximize Panel"),
                          "`", "fit_to_window");
  createMenuWindowsAction(MI_FullScreenWindow,
                          QT_TR_NOOP("Toggle Main Window's Full Screen Mode"),
                          "Ctrl+`", "toggle_fullscreen");
  createMenuWindowsAction(MI_StartupPopup, QT_TR_NOOP("&Startup Popup..."),
                          "Alt+S" /*, "opentoonz"*/);
  createMenuWindowsAction(MI_OpenGuidedDrawingControls,
                          QT_TR_NOOP("Guided Drawing Controls"), "",
                          "guided_drawing");
  // menuAct = createToggle(MI_DockingCheck, QT_TR_NOOP("&Lock Room Panes"), "",
  //                        DockingCheckToggleAction ? 1 : 0, MenuWindowsCommandType);
  // DockingCheck::instance()->setToggle(menuAct);
  createMenuWindowsAction(MI_OpenExport, QT_TR_NOOP("&Export"), "");
  createMenuWindowsAction(MI_OpenMotionPathPanel, QT_TR_NOOP("&Motion Paths"), "");

  // Menu - Help

  createMenuHelpAction(MI_OpenOnlineManual, QT_TR_NOOP("&Online Manual..."), 
                       "F1", "manual");
  createMenuHelpAction(MI_OpenWhatsNew, QT_TR_NOOP("&What's New..."), "",
                       "web");
  createMenuHelpAction(MI_OpenCommunityForum, QT_TR_NOOP("&Community Forum..."),
                       "", "web");
  createMenuHelpAction(MI_OpenReportABug, QT_TR_NOOP("&Report a Bug..."), "",
                       "web");
  createMenuHelpAction(MI_About, QT_TR_NOOP("&About Tahoma2D..."), "", "info");
  //  createMenuHelpAction(MI_SupportTahoma2D, QT_TR_NOOP("&Support
  //  Tahoma2D..."), "",
  //                       "web");

  // Fill

  createFillAction(
      MI_AutoFillToggle,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Toggle Autofill on Current Palette Color"),
      "Shift+A");

  // Right Click

  createRightClickMenuAction(MI_SavePaletteAs,
                             QT_TR_NOOP("&Save Palette As..."), "", "",
                             tr("Save the current style palette as a separate file with a new name."));
  createRightClickMenuAction(MI_OverwritePalette, QT_TR_NOOP("&Save Palette"),
                             "", "", tr("Save the current style palette as a separate file."));
  createRightClickMenuAction(MI_RegeneratePreview,
                             QT_TR_NOOP("&Regenerate Preview"), "", "",
                             tr("Recreates a set of preview images."));
  createRightClickMenuAction(MI_RegenerateFramePr,
                             QT_TR_NOOP("&Regenerate Frame Preview"), "", "",
                             tr("Regenerate the frame preview."));
  createRightClickMenuAction(MI_ClonePreview, QT_TR_NOOP("&Clone Preview"), "",
                             "", tr("Creates a clone of the previewed images."));
  createRightClickMenuAction(MI_FreezePreview,
                             QT_TR_NOOP("&Freeze//Unfreeze Preview"), "", "",
                             tr("Prevent the preview from being updated."));
  CommandManager::instance()->setToggleTexts(
      MI_FreezePreview, tr("Freeze Preview"), QT_TR_NOOP("Unfreeze Preview"));
  createRightClickMenuAction(MI_SavePreset, QT_TR_NOOP("&Save As Preset"), "");
  createRightClickMenuAction(MI_PreviewFx, QT_TR_NOOP("Preview Fx"), "");
  createRightClickMenuAction(MI_PasteValues, QT_TR_NOOP("&Paste Color && Name"),
                             "");
  createRightClickMenuAction(MI_PasteColors, QT_TR_NOOP("Paste Color"), "");
  createRightClickMenuAction(MI_PasteNames, QT_TR_NOOP("Paste Name"), "");
  createRightClickMenuAction(MI_GetColorFromStudioPalette,
                             QT_TR_NOOP("Get Color from Studio Palette"), "");
  createRightClickMenuAction(MI_ToggleLinkToStudioPalette,
                             QT_TR_NOOP("Toggle Link to Studio Palette"), "");
  createRightClickMenuAction(MI_RemoveReferenceToStudioPalette,
                             QT_TR_NOOP("Remove Reference to Studio Palette"),
                             "");
  createRightClickMenuAction(MI_ViewFile, QT_TR_NOOP("&View..."), "",
                             "view_file");
  createRightClickMenuAction(MI_ToggleQuickToolbar,
                             QT_TR_NOOP("Toggle Quick Toolbar"), "");
  createRightClickMenuAction(MI_ToggleXsheetCameraColumn,
                             QT_TR_NOOP("Show/Hide Camera Column"), "");
  createRightClickMenuAction(MI_SetKeyframes, QT_TR_NOOP("&Set Key"), "Z",
                             "set_key");
  createRightClickMenuAction(MI_ShiftKeyframesDown,
                             QT_TR_NOOP("&Shift Keys Down"), "", "shift_keys_down");
  createRightClickMenuAction(MI_ShiftKeyframesUp, QT_TR_NOOP("&Shift Keys Up"),
                             "", "shift_keys_up");
  createRightClickMenuAction(MI_PasteNumbers, QT_TR_NOOP("&Paste Numbers"), "",
                             "paste_numbers");
  createRightClickMenuAction(MI_Histogram, QT_TR_NOOP("&Histogram"), "");
  createRightClickMenuAction(MI_BlendColors, QT_TR_NOOP("&Blend colors"), "");
  createToggle(MI_OnionSkin, QT_TR_NOOP("Onion Skin Toggle"), "/", false,
               RightClickMenuCommandType, "onionskin_toggle");
  createToggle(MI_ZeroThick, QT_TR_NOOP("Zero Thick Lines"), "Shift+/", false,
               RightClickMenuCommandType);
  createToggle(MI_CursorOutline, QT_TR_NOOP("Toggle Cursor Size Outline"), "", false,
               RightClickMenuCommandType);
  createRightClickMenuAction(MI_ToggleCurrentTimeIndicator,
                             QT_TR_NOOP("Toggle Current Time Indicator"), "");
  createRightClickMenuAction(MI_DuplicateFile, QT_TR_NOOP("Duplicate"), "",
                             "duplicate");
  createRightClickMenuAction(MI_ShowFolderContents,
                             QT_TR_NOOP("Show Folder Contents"), "",
                             "show_folder_contents");
  createRightClickMenuAction(MI_ConvertFiles, QT_TR_NOOP("Convert..."), "",
                             "convert");
  createRightClickMenuAction(MI_CollectAssets, QT_TR_NOOP("Collect Assets"),
                             "");
  createRightClickMenuAction(MI_ImportScenes, QT_TR_NOOP("Import Scene"), "",
                             "load_scene");
  createRightClickMenuAction(MI_ExportScenes, QT_TR_NOOP("Export Scene..."), "",
                             "scene_export");
  createRightClickMenuAction(MI_RemoveLevel, QT_TR_NOOP("Remove Level"), "",
                             "remove_level");
  createRightClickMenuAction(MI_AddToBatchRenderList,
                             QT_TR_NOOP("Add As Render Task"), "",
                             "render_add");
  createRightClickMenuAction(MI_AddToBatchCleanupList,
                             QT_TR_NOOP("Add As Cleanup Task"), "",
                             "cleanup_add");
  createRightClickMenuAction(MI_SelectRowKeyframes,
                             QT_TR_NOOP("Select All Keys in this Frame"), "");
  createRightClickMenuAction(MI_SelectColumnKeyframes,
                             QT_TR_NOOP("Select All Keys in this Column"), "");
  createRightClickMenuAction(MI_SelectAllKeyframes,
                             QT_TR_NOOP("Select All Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotBefore,
                             QT_TR_NOOP("Select All Following Keys"), "");
  createRightClickMenuAction(MI_SelectAllKeyframesNotAfter,
                             QT_TR_NOOP("Select All Previous Keys"), "");
  createRightClickMenuAction(MI_SelectPreviousKeysInColumn,
                             QT_TR_NOOP("Select Previous Keys in this Column"),
                             "");
  createRightClickMenuAction(MI_SelectFollowingKeysInColumn,
                             QT_TR_NOOP("Select Following Keys in this Column"),
                             "");
  createRightClickMenuAction(MI_SelectPreviousKeysInRow,
                             QT_TR_NOOP("Select Previous Keys in this Frame"),
                             "");
  createRightClickMenuAction(MI_SelectFollowingKeysInRow,
                             QT_TR_NOOP("Select Following Keys in this Frame"),
                             "");
  createRightClickMenuAction(MI_InvertKeyframeSelection,
                             QT_TR_NOOP("Invert Key Selection"), "");
  createRightClickMenuAction(MI_SetAcceleration, QT_TR_NOOP("Set Acceleration"),
                             "");
  createRightClickMenuAction(MI_SetDeceleration, QT_TR_NOOP("Set Deceleration"),
                             "");
  createRightClickMenuAction(MI_SetConstantSpeed,
                             QT_TR_NOOP("Set Constant Speed"), "");
  createRightClickMenuAction(MI_ResetInterpolation,
                             QT_TR_NOOP("Reset Interpolation"), "");
  createRightClickMenuAction(MI_UseLinearInterpolation,
                             QT_TR_NOOP("Linear Interpolation"), "");
  createRightClickMenuAction(MI_UseSpeedInOutInterpolation,
                             QT_TR_NOOP("Speed In / Speed Out Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseEaseInOutInterpolation,
                             QT_TR_NOOP("Ease In / Ease Out Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseEaseInOutPctInterpolation,
                             QT_TR_NOOP("Ease In / Ease Out (%) Interpolation"),
                             "");
  createRightClickMenuAction(MI_UseExponentialInterpolation,
                             QT_TR_NOOP("Exponential Interpolation"), "");
  createRightClickMenuAction(MI_UseExpressionInterpolation,
                             QT_TR_NOOP("Expression Interpolation"), "");
  createRightClickMenuAction(MI_UseFileInterpolation,
                             QT_TR_NOOP("File Interpolation"), "");
  createRightClickMenuAction(MI_UseConstantInterpolation,
                             QT_TR_NOOP("Constant Interpolation"), "");
  createRightClickMenuAction(MI_FoldColumns, QT_TR_NOOP("Fold Column"), "",
                             "fold_column");
  createRightClickMenuAction(MI_ActivateThisColumnOnly,
                             QT_TR_NOOP("Show This Only"), "");
  createRightClickMenuAction(MI_ActivateSelectedColumns,
                             QT_TR_NOOP("Show Selected"), "");
  createRightClickMenuAction(MI_ActivateAllColumns, QT_TR_NOOP("Show All"), "");
  createRightClickMenuAction(MI_DeactivateSelectedColumns,
                             QT_TR_NOOP("Hide Selected"), "");
  createRightClickMenuAction(MI_DeactivateAllColumns, QT_TR_NOOP("Hide All"),
                             "");
  createRightClickMenuAction(MI_ToggleColumnsActivation,
                             QT_TR_NOOP("Toggle Show/Hide"), "");
  createRightClickMenuAction(MI_EnableThisColumnOnly,
                             QT_TR_NOOP("ON This Only"), "");
  createRightClickMenuAction(MI_EnableSelectedColumns,
                             QT_TR_NOOP("ON Selected"), "");
  createRightClickMenuAction(MI_EnableAllColumns, QT_TR_NOOP("ON All"), "");
  createRightClickMenuAction(MI_DisableAllColumns, QT_TR_NOOP("OFF All"), "");
  createRightClickMenuAction(MI_DisableSelectedColumns,
                             QT_TR_NOOP("OFF Selected"), "");
  createRightClickMenuAction(MI_SwapEnabledColumns, QT_TR_NOOP("Swap ON/OFF"),
                             "");
  createRightClickMenuAction(MI_LockThisColumnOnly,
                             QT_TR_NOOP("Lock This Only"), "Shift+L");
  createRightClickMenuAction(MI_LockSelectedColumns,
                             QT_TR_NOOP("Lock Selected"), "Ctrl+Shift+L");
  createRightClickMenuAction(MI_LockAllColumns, QT_TR_NOOP("Lock All"),
                             "Ctrl+Alt+Shift+L");
  createRightClickMenuAction(MI_UnlockSelectedColumns,
                             QT_TR_NOOP("Unlock Selected"), "Ctrl+Shift+U");
  createRightClickMenuAction(MI_UnlockAllColumns, QT_TR_NOOP("Unlock All"),
                             "Ctrl+Alt+Shift+U");
  createRightClickMenuAction(MI_ToggleColumnLocks,
                             QT_TR_NOOP("Swap Lock/Unlock"), "");
  createRightClickMenuAction(MI_DeactivateUpperColumns,
                             QT_TR_NOOP("Hide Upper Columns"), "");
  createRightClickMenuAction(MI_SeparateColors,
                             QT_TR_NOOP("Separate Colors..."), "",
                             "separate_colors");
  createRightClickMenuAction(MI_OpenPltGizmo, QT_TR_NOOP("&Palette Gizmo"), "",
                             "palettegizmo");
  createRightClickMenuAction(MI_EraseUnusedStyles,
                             QT_TR_NOOP("&Delete Unused Styles"), "");
  // createRightClickMenuAction(MI_LoadSubSceneFile, QT_TR_NOOP("Load As Sub-xsheet"),   "");
  // createRightClickMenuAction(MI_LoadResourceFile, QT_TR_NOOP("Load"), "");
  // createRightClickMenuAction(MI_PremultiplyFile,  QT_TR_NOOP("Premultiply"), "");
  // createRightClickMenuAction(MI_OpenCurrentScene, QT_TR_NOOP("&Current Scene"), "");
  createRightClickMenuAction(MI_SaveStudioPalette, QT_TR_NOOP("&Save Studio Palette"),
                             "", "", tr("Save the current Studio Palette."));
  createRightClickMenuAction(MI_SaveAsDefaultPalette,
                             QT_TR_NOOP("&Save As Default Palette"), "", "",
                             tr("Save the current style palette as the default "
                             "for new levels of the current level type."));
  createToggle(MI_ToggleAutoCreate, QT_TR_NOOP("Toggle Auto-Creation"), "",
               Preferences::instance()->isAutoCreateEnabled(),
               MiscCommandType, "auto_create",
               tr("Toggles the auto-creation of frames when drawing in blank "
                  "cells on the timeline/xsheet."));
  createToggle(MI_ToggleCreationInHoldCells,
               QT_TR_NOOP("Toggle Creation In Hold Cells"), "",
               Preferences::instance()->isCreationInHoldCellsEnabled(),
               MiscCommandType, "create_in_hold",
               tr("Toggles the auto-creation of frames when drawing in held "
                  "cells on the timeline/xsheet."));
  createToggle(MI_ToggleAutoStretch, QT_TR_NOOP("Toggle Auto-Stretch"), "",
               Preferences::instance()->isAutoStretchEnabled(),
               MiscCommandType, "auto_stretch",
               tr("Toggles the auto-stretch of a frame to the next frame"));
  createToggle(MI_ViewerIndicator, QT_TR_NOOP("Toggle Viewer Indicators"), "",
               Preferences::instance()->isViewerIndicatorEnabled(),
               RightClickMenuCommandType);

  // Tools

  createToolAction(T_Edit, "animate", QT_TR_NOOP("Animate Tool"), "A",
                   tr("Animate Tool: Modifies the position, rotation and size "
                   "of the current column"));
  createToolAction(T_Selection, "selection", QT_TR_NOOP("Selection Tool"), "S",
                   tr("Selection Tool: Select parts of your image to transform it."));
  createToolAction(T_Brush, "brush", QT_TR_NOOP("Brush Tool"), "B",
                   tr("Brush Tool: Draws in the work area freehand"));
  createToolAction(T_Geometric, "geometric", QT_TR_NOOP("Geometry Tool"), "G",
                   tr("Geometry Tool: Draws geometric shapes"));
  createToolAction(T_Type, "type", QT_TR_NOOP("Type Tool"), "Y",
                   tr("Type Tool: Adds text"));
  createToolAction(T_Fill, "fill", QT_TR_NOOP("Fill Tool"), "F",
                   tr("Fill Tool: Fills drawing areas with the current style"));
  createToolAction(T_PaintBrush, "paintbrush", QT_TR_NOOP("Smart Raster Paint Tool"),
                   "", tr("Smart Raster Paint: Paints areas in Smart Raster levels"));
  createToolAction(T_Eraser, "eraser", QT_TR_NOOP("Eraser Tool"), "E",
                   tr("Eraser Tool: Erases lines and areas"));
  createToolAction(T_Tape, "tape", QT_TR_NOOP("Tape Tool"), "T",
                   tr("Tape Tool: Closes gaps in raster, joins edges in vector"));
  createToolAction(T_StylePicker, "stylepicker",
                   QT_TR_NOOP("Style Picker Tool"), "K",
                   tr("Style Picker: Selects style on current drawing"));
  createToolAction(T_RGBPicker, "rgbpicker", QT_TR_NOOP("RGB Picker Tool"),
                   "R", tr("RGB Picker: Picks color on screen and applies to current style"));
  createToolAction(T_ControlPointEditor, "controlpointeditor",
                   QT_TR_NOOP("Control Point Editor Tool"), "C",
                   tr("Control Point Editor: Modifies vector lines by editing "
                   "its control points"));
  createToolAction(T_Pinch, "pinch", QT_TR_NOOP("Pinch Tool"), "M",
                   tr("Pinch Tool: Pulls vector drawings"));
  createToolAction(T_Pump, "pump", QT_TR_NOOP("Pump Tool"), "",
                   tr("Pump Tool: Changes vector thickness"));
  createToolAction(T_Magnet, "magnet", QT_TR_NOOP("Magnet Tool"), "",
                   tr("Magnet Tool: Deforms vector lines"));
  createToolAction(T_Bender, "bender", QT_TR_NOOP("Bender Tool"), "",
                   tr("Bender Tool: Bends vector shapes around the first click"));
  createToolAction(T_Iron, "iron", QT_TR_NOOP("Iron Tool"), "",
                   tr("Iron Tool: Smooths out vector lines"));

  createToolAction(T_Cutter, "cutter", QT_TR_NOOP("Cutter Tool"), "",
                   tr("Cutter Tool: Splits vector lines"));
  createToolAction(T_Skeleton, "skeleton", QT_TR_NOOP("Skeleton Tool"), "V",
                   tr("Skeleton Tool: Allows to build a skeleton and animate "
                   "in a cut-out workflow"));
  createToolAction(T_Tracker, "radar", QT_TR_NOOP("Tracker Tool"), "",
                   tr("Tracker Tool: Tracks specific regions in a sequence of images"));
  createToolAction(T_Hook, "hook", QT_TR_NOOP("Hook Tool"), "O");
  createToolAction(T_Zoom, "zoom", QT_TR_NOOP("Zoom Tool"), "Shift+Space",
                   tr("Zoom Tool: Zooms viewer"));
  createToolAction(T_Rotate, "rotate", QT_TR_NOOP("Rotate Tool"), "Ctrl+Space",
                   tr("Rotate Tool: Rotate the viewer"));
  createToolAction(T_Hand, "hand", QT_TR_NOOP("Hand Tool"), "Space",
                   tr("Hand Tool: Pans the workspace"));
  createToolAction(T_Plastic, "plastic", QT_TR_NOOP("Plastic Tool"), "X",
                   tr("Plastic Tool: Builds a mesh that allows to deform and "
                   "animate a level"));
  createToolAction(T_Ruler, "ruler", QT_TR_NOOP("Ruler Tool"), "",
                   tr("Ruler Tool: Measure distances on the canvas"));
  createToolAction(T_Finger, "finger", QT_TR_NOOP("Finger Tool"), "",
                   tr("Finger Tool: Smudges small areas to cover with line"));

  /*-- Animate tool + mode switching shortcuts --*/
  createAction(MI_EditNextMode, QT_TR_NOOP("Animate Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_EditPosition, QT_TR_NOOP("Animate Tool - Position"), "", "",
               ToolCommandType, "edit_position");
  createAction(MI_EditRotation, QT_TR_NOOP("Animate Tool - Rotation"), "", "",
               ToolCommandType, "edit_rotation");
  createAction(MI_EditScale, QT_TR_NOOP("Animate Tool - Scale"), "", "",
               ToolCommandType, "edit_scale");
  createAction(MI_EditShear, QT_TR_NOOP("Animate Tool - Shear"), "", "",
               ToolCommandType, "edit_shear");
  createAction(MI_EditCenter, QT_TR_NOOP("Animate Tool - Center"), "", "",
               ToolCommandType, "edit_center");
  createAction(MI_EditAll, QT_TR_NOOP("Animate Tool - All"), "", "",
               ToolCommandType, "edit_all");

  /*-- Selection tool + type switching shortcuts --*/
  createAction(MI_SelectionNextType, QT_TR_NOOP("Selection Tool - Next Type"), "",
               "", ToolCommandType);
  createAction(MI_SelectionRectangular,
               QT_TR_NOOP("Selection Tool - Rectangular"), "", "", ToolCommandType,
               "selection_rectangular");
  createAction(MI_SelectionFreehand, QT_TR_NOOP("Selection Tool - Freehand"), "", "",
               ToolCommandType, "selection_freehand");
  createAction(MI_SelectionPolyline, QT_TR_NOOP("Selection Tool - Polyline"), "", "",
               ToolCommandType, "selection_polyline");

  /*-- Brush tool + mode switching shortcuts --*/
  createAction(MI_BrushAutoFillOn, QT_TR_NOOP("Brush Tool - Auto Fill On"), "", "",
               ToolCommandType);
  createAction(MI_BrushAutoFillOff, QT_TR_NOOP("Brush Tool - Auto Fill Off"), "", "",
               ToolCommandType);

  /*-- Geometric tool + shape switching shortcuts --*/
  createAction(MI_GeometricNextShape, QT_TR_NOOP("Geometric Tool - Next Shape"), "",
               "", ToolCommandType);
  createAction(MI_GeometricRectangle, QT_TR_NOOP("Geometric Tool - Rectangle"), "",
               "", ToolCommandType, "geometric_rectangle");
  createAction(MI_GeometricCircle, QT_TR_NOOP("Geometric Tool - Circle"), "", "",
               ToolCommandType, "geometric_circle");
  createAction(MI_GeometricEllipse, QT_TR_NOOP("Geometric Tool - Ellipse"), "", "",
               ToolCommandType, "geometric_ellipse");
  createAction(MI_GeometricLine, QT_TR_NOOP("Geometric Tool - Line"), "", "",
               ToolCommandType, "geometric_line");
  createAction(MI_GeometricPolyline, QT_TR_NOOP("Geometric Tool - Polyline"), "",
               "", ToolCommandType, "geometric_polyline");
  createAction(MI_GeometricArc, QT_TR_NOOP("Geometric Tool - Arc"), "", "",
               ToolCommandType, "geometric_arc");
  createAction(MI_GeometricMultiArc, QT_TR_NOOP("Geometric Tool - MultiArc"), "",
               "", ToolCommandType, "geometric_multiarc");
  createAction(MI_GeometricPolygon, QT_TR_NOOP("Geometric Tool - Polygon"), "", "",
               ToolCommandType, "geometric_polygon");

  /*-- Type tool + style switching shortcuts --*/
  createAction(MI_TypeNextStyle, QT_TR_NOOP("Type Tool - Next Style"), "", "",
               ToolCommandType);
  createAction(MI_TypeOblique, QT_TR_NOOP("Type Tool - Oblique"), "", "",
               ToolCommandType);
  createAction(MI_TypeRegular, QT_TR_NOOP("Type Tool - Regular"), "", "",
               ToolCommandType);
  createAction(MI_TypeBoldOblique, QT_TR_NOOP("Type Tool - Bold Oblique"), "", "",
               ToolCommandType);
  createAction(MI_TypeBold, QT_TR_NOOP("Type Tool - Bold"), "", "",
               ToolCommandType);

  /*-- Fill tool + type/mode switching shortcuts --*/
  createAction(MI_FillNextType, QT_TR_NOOP("Fill Tool - Next Type"), "", "",
               ToolCommandType);
  createAction(MI_FillNormal, QT_TR_NOOP("Fill Tool - Normal"), "", "",
               ToolCommandType, "fill_normal");
  createAction(MI_FillRectangular, QT_TR_NOOP("Fill Tool - Rectangular"), "",
               "", ToolCommandType, "fill_rectangular");
  createAction(MI_FillFreehand, QT_TR_NOOP("Fill Tool - Freehand"), "", "",
               ToolCommandType, "fill_freehand");
  createAction(MI_FillPolyline, QT_TR_NOOP("Fill Tool - Polyline"), "", "",
               ToolCommandType, "fill_polyline");
  createAction(MI_FillNextMode, QT_TR_NOOP("Fill Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_FillAreas, QT_TR_NOOP("Fill Tool - Areas"), "", "",
               ToolCommandType);
  createAction(MI_FillLines, QT_TR_NOOP("Fill Tool - Lines"), "", "",
               ToolCommandType);
  createAction(MI_FillLinesAndAreas, QT_TR_NOOP("Fill Tool - Lines & Areas"),
               "", "", ToolCommandType);

  /*-- Eraser tool + type switching shortcuts --*/
  createAction(MI_EraserNextType, QT_TR_NOOP("Eraser Tool - Next Type"), "", "",
               ToolCommandType);
  createAction(MI_EraserNormal, QT_TR_NOOP("Eraser Tool - Normal"), "", "",
               ToolCommandType);
  createAction(MI_EraserRectangular, QT_TR_NOOP("Eraser Tool - Rectangular"), "", "",
               ToolCommandType);
  createAction(MI_EraserFreehand, QT_TR_NOOP("Eraser Tool - Freehand"), "", "",
               ToolCommandType);
  createAction(MI_EraserPolyline, QT_TR_NOOP("Eraser Tool - Polyline"), "", "",
               ToolCommandType);
  createAction(MI_EraserSegment, QT_TR_NOOP("Eraser Tool - Segment"), "", "",
               ToolCommandType);

  /*-- Tape tool + type/mode switching shortcuts --*/
  createAction(MI_TapeNextType, QT_TR_NOOP("Tape Tool - Next Type"), "", "",
               ToolCommandType);
  createAction(MI_TapeNormal, QT_TR_NOOP("Tape Tool - Normal"), "", "",
               ToolCommandType);
  createAction(MI_TapeRectangular, QT_TR_NOOP("Tape Tool - Rectangular"), "", "",
               ToolCommandType);
  createAction(MI_TapeNextMode, QT_TR_NOOP("Tape Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_TapeEndpointToEndpoint,
               QT_TR_NOOP("Tape Tool - Endpoint to Endpoint"), "", "",
               ToolCommandType);
  createAction(MI_TapeEndpointToLine,
               QT_TR_NOOP("Tape Tool - Endpoint to Line"), "", "",
               ToolCommandType);
  createAction(MI_TapeLineToLine, QT_TR_NOOP("Tape Tool - Line to Line"), "", "",
               ToolCommandType);

  /*-- Style Picker tool + mode switching shortcuts --*/
  createAction(MI_PickStyleNextMode,
               QT_TR_NOOP("Style Picker Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_PickStyleAreas, QT_TR_NOOP("Style Picker Tool - Areas"), "", "",
               ToolCommandType);
  createAction(MI_PickStyleLines, QT_TR_NOOP("Style Picker Tool - Lines"), "", "",
               ToolCommandType);
  createAction(MI_PickStyleLinesAndAreas,
               QT_TR_NOOP("Style Picker Tool - Lines & Areas"), "", "",
               ToolCommandType);

  /*-- RGB Picker tool + type switching shortcuts --*/
  createAction(MI_RGBPickerNextType, QT_TR_NOOP("RGB Picker Tool - Next Type"),
               "", "", ToolCommandType);
  createAction(MI_RGBPickerNormal, QT_TR_NOOP("RGB Picker Tool - Normal"), "", "",
               ToolCommandType);
  createAction(MI_RGBPickerRectangular,
               QT_TR_NOOP("RGB Picker Tool - Rectangular"), "", "",
               ToolCommandType);
  createAction(MI_RGBPickerFreehand, QT_TR_NOOP("RGB Picker Tool - Freehand"),
               "", "", ToolCommandType);
  createAction(MI_RGBPickerPolyline, QT_TR_NOOP("RGB Picker Tool - Polyline"),
               "", "", ToolCommandType);

  /*-- Skeleton tool + mode switching shortcuts --*/
  createAction(MI_SkeletonNextMode, QT_TR_NOOP("Skeleton Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_SkeletonBuildSkeleton,
               QT_TR_NOOP("Skeleton Tool - Build Skeleton"), "", "",
               ToolCommandType);
  createAction(MI_SkeletonAnimate, QT_TR_NOOP("Skeleton Tool - Animate"), "", "",
               ToolCommandType);
  createAction(MI_SkeletonInverseKinematics,
               QT_TR_NOOP("Skeleton Tool - Inverse Kinematics"), "", "",
               ToolCommandType);

  /*-- Plastic tool + mode switching shortcuts --*/
  createAction(MI_PlasticNextMode, QT_TR_NOOP("Plastic Tool - Next Mode"), "", "",
               ToolCommandType);
  createAction(MI_PlasticEditMesh, QT_TR_NOOP("Plastic Tool - Edit Mesh"), "", "",
               ToolCommandType);
  createAction(MI_PlasticPaintRigid, QT_TR_NOOP("Plastic Tool - Paint Rigid"),
               "", "", ToolCommandType);
  createAction(MI_PlasticBuildSkeleton,
               QT_TR_NOOP("Plastic Tool - Build Skeleton"), "", "",
               ToolCommandType);
  createAction(MI_PlasticAnimate, QT_TR_NOOP("Plastic Tool - Animate"), "", "",
               ToolCommandType);

  // Tool Modifiers

  createToolOptionsAction(MI_SelectNextGuideStroke,
                          QT_TR_NOOP("Select Next Frame Guide Stroke"), "");
  createToolOptionsAction(MI_SelectPrevGuideStroke,
                          QT_TR_NOOP("Select Previous Frame Guide Stroke"), "");
  createToolOptionsAction(
      MI_SelectBothGuideStrokes,
      QT_TRANSLATE_NOOP("MainWindow",
                        "Select Prev && Next Frame Guide Strokes"),
      "");
  createToolOptionsAction(MI_SelectGuideStrokeReset,
                          QT_TR_NOOP("Reset Guide Stroke Selections"), "");
  createToolOptionsAction(MI_TweenGuideStrokes,
                          QT_TR_NOOP("Tween Selected Guide Strokes"), "");
  createToolOptionsAction(MI_TweenGuideStrokeToSelected,
                          QT_TR_NOOP("Tween Guide Strokes to Selected"), "");
  createToolOptionsAction(MI_SelectGuidesAndTweenMode,
                          QT_TR_NOOP("Select Guide Strokes && Tween Mode"), "");
  createToolOptionsAction(MI_FlipNextGuideStroke,
                          QT_TR_NOOP("Flip Next Guide Stroke Direction"), "");
  createToolOptionsAction(MI_FlipPrevGuideStroke,
                          QT_TR_NOOP("Flip Previous Guide Stroke Direction"),
                          "");
  createToolOptionsAction("A_ToolOption_GlobalKey", QT_TR_NOOP("Global Key"),
                          "");

  createToolOptionsAction("A_IncreaseMaxBrushThickness",
                          QT_TR_NOOP("Brush size - Increase max"), "I");
  createToolOptionsAction("A_DecreaseMaxBrushThickness",
                          QT_TR_NOOP("Brush size - Decrease max"), "U");
  createToolOptionsAction("A_IncreaseMinBrushThickness",
                          QT_TR_NOOP("Brush size - Increase min"), "J");
  createToolOptionsAction("A_DecreaseMinBrushThickness",
                          QT_TR_NOOP("Brush size - Decrease min"), "H");
  createToolOptionsAction("A_IncreaseBrushHardness",
                          QT_TR_NOOP("Brush hardness - Increase"), "");
  createToolOptionsAction("A_DecreaseBrushHardness",
                          QT_TR_NOOP("Brush hardness - Decrease"), "");
  createToolOptionsAction("A_ToolOption_SnapSensitivity",
                          QT_TR_NOOP("Snap Sensitivity"), "");
  createToolOptionsAction("A_ToolOption_AutoGroup", QT_TR_NOOP("Auto Group"),
                          "");
  createToolOptionsAction("A_ToolOption_BreakSharpAngles",
                          QT_TR_NOOP("Break sharp angles"), "");
  createToolOptionsAction("A_ToolOption_FrameRange", QT_TR_NOOP("Frame range"),
                          "F6");
  createToolOptionsAction("A_ToolOption_IK", QT_TR_NOOP("Inverse Kinematics"),
                          "");
  createToolOptionsAction("A_ToolOption_Invert", QT_TR_NOOP("Invert"), "");
  createToolOptionsAction("A_ToolOption_Manual", QT_TR_NOOP("Manual"), "");
  createToolOptionsAction("A_ToolOption_OnionSkin", QT_TR_NOOP("Onion skin"),
                          "");
  createToolOptionsAction("A_ToolOption_Orientation", QT_TR_NOOP("Orientation"),
                          "");
  createToolOptionsAction("A_ToolOption_PencilMode", QT_TR_NOOP("Pencil Mode"),
                          "");
  createToolOptionsAction("A_ToolOption_PreserveThickness",
                          QT_TR_NOOP("Preserve Thickness"), "");
  createToolOptionsAction("A_ToolOption_PressureSensitivity",
                          QT_TR_NOOP("Pressure Sensitivity"), "Shift+P");
  createToolOptionsAction("A_ToolOption_SegmentInk", QT_TR_NOOP("Segment Ink"),
                          "F8");
  createToolOptionsAction("A_ToolOption_Selective", QT_TR_NOOP("Selective"),
                          "F7");
  createToolOptionsAction("A_ToolOption_DrawOrder",
                          QT_TR_NOOP("Brush Tool - Draw Order"), "");
  createToolOptionsAction("A_ToolOption_Smooth", QT_TR_NOOP("Smooth"), "");
  createToolOptionsAction("A_ToolOption_Snap", QT_TR_NOOP("Snap"), "");
  createToolOptionsAction("A_ToolOption_AutoSelectDrawing",
                          QT_TR_NOOP("Auto Select Drawing"), "");
  createToolOptionsAction("A_ToolOption_Autofill", QT_TR_NOOP("Auto Fill"), "");
  createToolOptionsAction("A_ToolOption_JoinVectors",
                          QT_TR_NOOP("Join Vectors"), "");
  createToolOptionsAction("A_ToolOption_ShowOnlyActiveSkeleton",
                          QT_TR_NOOP("Show Only Active Skeleton"), "");
  createToolOptionsAction("A_ToolOption_RasterEraser",
                          QT_TR_NOOP("Brush Tool - Eraser (Raster option)"),
                          "");
  createToolOptionsAction("A_ToolOption_LockAlpha",
                          QT_TR_NOOP("Brush Tool - Lock Alpha"), "");

  createToolOptionsAction("A_ToolOption_BrushPreset",
                          QT_TR_NOOP("Brush Preset"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape",
                          QT_TR_NOOP("Geometric Shape"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Rectangle",
                          QT_TR_NOOP("Geometric Shape Rectangle"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Circle",
                          QT_TR_NOOP("Geometric Shape Circle"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Ellipse",
                          QT_TR_NOOP("Geometric Shape Ellipse"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Line",
                          QT_TR_NOOP("Geometric Shape Line"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Polyline",
                          QT_TR_NOOP("Geometric Shape Polyline"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Arc",
                          QT_TR_NOOP("Geometric Shape Arc"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:MultiArc",
                          QT_TR_NOOP("Geometric Shape MultiArc"), "");
  createToolOptionsAction("A_ToolOption_GeometricShape:Polygon",
                          QT_TR_NOOP("Geometric Shape Polygon"), "");
  createToolOptionsAction("A_ToolOption_GeometricEdge",
                          QT_TR_NOOP("Geometric Edge"), "");
  createToolOptionsAction("A_ToolOption_Mode", QT_TR_NOOP("Mode"), "");
  menuAct = createToolOptionsAction("A_ToolOption_Mode:Areas",
                                    QT_TR_NOOP("Mode - Areas"), "");
  menuAct->setIcon(createQIcon("mode_areas"));
  menuAct = createToolOptionsAction("A_ToolOption_Mode:Lines",
                                    QT_TR_NOOP("Mode - Lines"), "");
  menuAct->setIcon(createQIcon("mode_lines"));
  menuAct = createToolOptionsAction("A_ToolOption_Mode:Lines & Areas",
                                    QT_TR_NOOP("Mode - Lines && Areas"), "");
  menuAct->setIcon(createQIcon("mode_areas_lines"));
  createToolOptionsAction("A_ToolOption_Mode:Endpoint to Endpoint",
                          QT_TR_NOOP("Mode - Endpoint to Endpoint"), "");
  createToolOptionsAction("A_ToolOption_Mode:Endpoint to Line",
                          QT_TR_NOOP("Mode - Endpoint to Line"), "");
  createToolOptionsAction("A_ToolOption_Mode:Line to Line",
                          QT_TR_NOOP("Mode - Line to Line"), "");
  createToolOptionsAction("A_ToolOption_Type", QT_TR_NOOP("Type"), "");

  menuAct = createToolOptionsAction("A_ToolOption_Type:Normal",
                                    QT_TR_NOOP("Type - Normal"), "");
  menuAct->setIcon(createQIcon("type_normal"));

  menuAct = createToolOptionsAction("A_ToolOption_Type:Rectangular",
                                    QT_TR_NOOP("Type - Rectangular"), "F5");
  menuAct->setIcon(createQIcon("type_rectangular"));

  menuAct = createToolOptionsAction("A_ToolOption_Type:Freehand",
                                    QT_TR_NOOP("Type - Freehand"), "");
  menuAct->setIcon(createQIcon("type_lasso"));

  menuAct = createToolOptionsAction("A_ToolOption_Type:Polyline",
                                    QT_TR_NOOP("Type - Polyline"), "");
  menuAct->setIcon(createQIcon("type_polyline"));

  menuAct = createToolOptionsAction("A_ToolOption_Type:Segment",
                                    QT_TR_NOOP("Type - Segment"), "");
  menuAct->setIcon(createQIcon("type_erase_segment"));

  createToolOptionsAction("A_ToolOption_TypeFont", QT_TR_NOOP("TypeTool Font"),
                          "");
  createToolOptionsAction("A_ToolOption_TypeSize", QT_TR_NOOP("TypeTool Size"),
                          "");
  createToolOptionsAction("A_ToolOption_TypeStyle",
                          QT_TR_NOOP("TypeTool Style"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Oblique",
                          QT_TR_NOOP("TypeTool Style - Oblique"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Regular",
                          QT_TR_NOOP("TypeTool Style - Regular"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Bold Oblique",
                          QT_TR_NOOP("TypeTool Style - Bold Oblique"), "");
  createToolOptionsAction("A_ToolOption_TypeStyle:Bold",
                          QT_TR_NOOP("TypeTool Style - Bold"), "");

  createToolOptionsAction("A_ToolOption_EditToolActiveAxis",
                          QT_TR_NOOP("Active Axis"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Position",
                          QT_TR_NOOP("Active Axis - Position"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Rotation",
                          QT_TR_NOOP("Active Axis - Rotation"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Scale",
                          QT_TR_NOOP("Active Axis - Scale"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Shear",
                          QT_TR_NOOP("Active Axis - Shear"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:Center",
                          QT_TR_NOOP("Active Axis - Center"), "");
  createToolOptionsAction("A_ToolOption_EditToolActiveAxis:All",
                          QT_TR_NOOP("Active Axis - All"), "");

  createToolOptionsAction("A_ToolOption_SkeletonMode",
                          QT_TR_NOOP("Skeleton Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Edit Mesh",
                          QT_TR_NOOP("Edit Mesh Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Paint Rigid",
                          QT_TR_NOOP("Paint Rigid Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Build Skeleton",
                          QT_TR_NOOP("Build Skeleton Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Animate",
                          QT_TR_NOOP("Animate Mode"), "");
  createToolOptionsAction("A_ToolOption_SkeletonMode:Inverse Kinematics",
                          QT_TR_NOOP("Inverse Kinematics Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:None",
                          QT_TR_NOOP("None Pick Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Column",
                          QT_TR_NOOP("Column Pick Mode"), "");
  createToolOptionsAction("A_ToolOption_AutoSelect:Pegbar",
                          QT_TR_NOOP("Pegbar Pick Mode"), "");
  menuAct = createToolOptionsAction("A_ToolOption_PickScreen",
                                    QT_TR_NOOP("Pick Screen"), "");
  menuAct->setIcon(createQIcon("pickscreen"));
  createToolOptionsAction("A_ToolOption_Meshify", QT_TR_NOOP("Create Mesh"),
                          "");

  menuAct =
      createToolOptionsAction("A_ToolOption_AutopaintLines",
                              QT_TR_NOOP("Fill Tool - Autopaint Lines"), "");
  menuAct->setIcon(createQIcon("fill_auto"));

  // createToolOptionsAction("A_ToolOption_Link", QT_TR_NOOP("Link"), "");
  createToolOptionsAction("A_ToolOption_AutoClose", QT_TR_NOOP("Auto Close"), "");
  createToolOptionsAction("A_ToolOption_DrawUnder", QT_TR_NOOP("Draw Under"), "");

  createToolOptionsAction("A_ToolOption_FlipHorizontal",
                          QT_TR_NOOP("Flip Selection/Object Horizontally"), "");
  createToolOptionsAction("A_ToolOption_FlipVertical",
                          QT_TR_NOOP("Flip Selection/Object Vertically"), "");
  createToolOptionsAction("A_ToolOption_RotateLeft",
                          QT_TR_NOOP("Rotate Selection/Object Left"), "");
  createToolOptionsAction("A_ToolOption_RotateRight",
                          QT_TR_NOOP("Rotate Selection/Object Right"), "");
  // Visualization

  createViewerAction(V_ZoomIn, QT_TR_NOOP("Zoom In"), "+");
  createViewerAction(V_ZoomOut, QT_TR_NOOP("Zoom Out"), "-");
  createViewerAction(V_ViewReset, QT_TR_NOOP("Reset View"), "Alt+0");
  createViewerAction(V_ZoomFit, QT_TR_NOOP("Fit to Window"), "Alt+9");
  createViewerAction(V_ZoomReset, QT_TR_NOOP("Reset Zoom"), "");
  createViewerAction(V_RotateReset, QT_TR_NOOP("Reset Rotation"), "");
  createViewerAction(V_PositionReset, QT_TR_NOOP("Reset Position"), "");

  createViewerAction(V_ActualPixelSize, QT_TR_NOOP("Actual Pixel Size"), "N");
  createViewerAction(V_FlipX, QT_TR_NOOP("Flip Viewer Horizontally"), "");
  createViewerAction(V_FlipY, QT_TR_NOOP("Flip Viewer Vertically"), "");
  createViewerAction(V_ShowHideFullScreen, QT_TR_NOOP("Show//Hide Full Screen"),
                     "Alt+F");
  CommandManager::instance()->setToggleTexts(V_ShowHideFullScreen,
                                             tr("Full Screen Mode"),
                                             tr("Exit Full Screen Mode"));
  createViewerAction(MI_CompareToSnapshot, QT_TR_NOOP("Compare to Snapshot"),
                     "");
  menuAct = createToggle(MI_ShowStatusBar, QT_TR_NOOP("&Show Status Bar"), "",
                         ShowStatusBarAction ? 1 : 0, MenuViewCommandType);
  connect(menuAct, SIGNAL(triggered(bool)), this,
          SLOT(toggleStatusBar(bool)));
  menuAct = createToggle(MI_ToggleTransparent, QT_TR_NOOP("&Toggle Transparency"),
                         "", 0, MenuViewCommandType);
  connect(menuAct, SIGNAL(triggered(bool)), this,
          SLOT(toggleTransparency(bool)));
  connect(m_transparencyTogglerWindow, &QDialog::finished, [=](int result) {
          toggleTransparency(false);
          menuAct->setChecked(false);
  });

  // Following actions are for adding "Visualization" menu items to the command
  // bar. They are separated from the original actions in order to avoid
  // assigning shortcut keys. They must be triggered only from pressing buttons
  // in the command bar. Assigning shortcut keys and registering as MenuItem
  // will break a logic of ShortcutZoomer. So here we register separate items
  // and bypass the command.
  createVisualizationButtonAction(VB_ViewReset, QT_TR_NOOP("Reset View"),
                                  "reset");
  createVisualizationButtonAction(VB_ZoomFit, QT_TR_NOOP("Fit to Window"),
                                  "fit_to_window");
  createVisualizationButtonAction(VB_ZoomReset, QT_TR_NOOP("Reset Zoom"));
  createVisualizationButtonAction(VB_RotateReset, QT_TR_NOOP("Reset Rotation"));
  createVisualizationButtonAction(VB_PositionReset,
                                  QT_TR_NOOP("Reset Position"));
  createVisualizationButtonAction(
      VB_ActualPixelSize, QT_TR_NOOP("Actual Pixel Size"), "actual_pixel_size");
  createVisualizationButtonAction(
      VB_FlipX, QT_TR_NOOP("Flip Viewer Horizontally"), "fliphoriz_off");
  createVisualizationButtonAction(
      VB_FlipY, QT_TR_NOOP("Flip Viewer Vertically"), "flipvert_off");

  // Misc

  menuAct =
      createToggle(MI_TouchGestureControl, QT_TR_NOOP("&Touch Gesture Control"), 
                   "", TouchGestureControl ? 1 : 0, MiscCommandType, "touch");
  menuAct->setEnabled(true);
  createMiscAction(MI_CameraStage, QT_TR_NOOP("&Camera Settings..."), "");
  menuAct =
      createMiscAction(MI_RefreshTree, QT_TR_NOOP("Refresh Folder Tree"), "");
  menuAct->setIconText(tr("Refresh"));
  menuAct->setIcon(createQIcon("refresh", false, true));
  createMiscAction("A_FxSchematicToggle",
                   QT_TR_NOOP("Toggle FX/Stage schematic"), "");
  // createAction(MI_SavePreview, QT_TR_NOOP("&Save Preview"), "");
  // createAction(MI_TestAnimation, QT_TR_NOOP("Test Animation"), "Ctrl+Return");
  // createAction(MI_Export, QT_TR_NOOP("Export"), "Ctrl+E");

  // RGBA

  createRGBAAction(MI_RedChannel, QT_TR_NOOP("Red Channel"), "");
  createRGBAAction(MI_GreenChannel, QT_TR_NOOP("Green Channel"), "");
  createRGBAAction(MI_BlueChannel, QT_TR_NOOP("Blue Channel"), "");
  createRGBAAction(MI_MatteChannel, QT_TR_NOOP("Alpha Channel"), "");
  createRGBAAction(MI_RedChannelGreyscale, QT_TR_NOOP("Red Channel Greyscale"),
                   "");
  createRGBAAction(MI_GreenChannelGreyscale,
                   QT_TR_NOOP("Green Channel Greyscale"), "");
  createRGBAAction(MI_BlueChannelGreyscale,
                   QT_TR_NOOP("Blue Channel Greyscale"), "");

  // Stop Motion

  createStopMotionAction(MI_StopMotionExportImageSequence,
                         QT_TR_NOOP("&Export Stop Motion Image Sequence"), "",
                         "", tr("Exports the full resolution stop motion image sequence.") +
                         separator + tr("This is especially useful if using a DSLR camera."));
  createStopMotionAction(MI_StopMotionCapture,
                         QT_TR_NOOP("Capture Stop Motion Frame"), "");
  createStopMotionAction(MI_StopMotionRaiseOpacity,
                         QT_TR_NOOP("Raise Stop Motion Opacity"), "");
  createStopMotionAction(MI_StopMotionLowerOpacity,
                         QT_TR_NOOP("Lower Stop Motion Opacity"), "");
  createStopMotionAction(MI_StopMotionToggleLiveView,
                         QT_TR_NOOP("Toggle Stop Motion Live View"), "");
#ifdef WITH_CANON
  createStopMotionAction(MI_StopMotionToggleZoom,
                         QT_TR_NOOP("Toggle Stop Motion Zoom"), "");
  createStopMotionAction(MI_StopMotionPickFocusCheck,
                         QT_TR_NOOP("Pick Focus Check Location"), "");
#endif
  createStopMotionAction(MI_StopMotionLowerSubsampling,
                         QT_TR_NOOP("Lower Stop Motion Level Subsampling"), "");
  createStopMotionAction(MI_StopMotionRaiseSubsampling,
                         QT_TR_NOOP("Raise Stop Motion Level Subsampling"), "");
  createStopMotionAction(MI_StopMotionJumpToCamera,
                         QT_TR_NOOP("Go to Stop Motion Insert Frame"), "");
  createStopMotionAction(MI_StopMotionRemoveFrame,
                         QT_TR_NOOP("Remove frame before Stop Motion Camera"),
                         "");
  createStopMotionAction(MI_StopMotionNextFrame,
                         QT_TR_NOOP("Next Frame including Stop Motion Camera"),
                         "");
  createStopMotionAction(MI_StopMotionToggleUseLiveViewImages,
                         QT_TR_NOOP("Show original live view images."), "");
}

//-----------------------------------------------------------------------------

void MainWindow::onInkCheckTriggered(bool on) {
  if (!on) return;
  QAction *ink1CheckAction =
      CommandManager::instance()->getAction(MI_Ink1Check);
  if (ink1CheckAction) ink1CheckAction->setChecked(false);
}

//-----------------------------------------------------------------------------

void MainWindow::onInk1CheckTriggered(bool on) {
  if (!on) return;
  QAction *inkCheckAction = CommandManager::instance()->getAction(MI_ICheck);
  if (inkCheckAction) inkCheckAction->setChecked(false);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewVectorLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, PLI_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewToonzRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, TZP_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------

void MainWindow::onNewRasterLevelButtonPressed() {
  int defaultLevelType = Preferences::instance()->getDefLevelType();
  Preferences::instance()->setValue(DefLevelType, OVL_XSHLEVEL);
  CommandManager::instance()->execute("MI_NewLevel");
  Preferences::instance()->setValue(DefLevelType, defaultLevelType);
}

//-----------------------------------------------------------------------------
// delete unused files / folders in the cache
void MainWindow::clearCacheFolder() {
  // currently cache folder is used for following purposes
  // 1. $CACHE/[ProcessID] : for disk swap of image cache.
  //    To be deleted on exit. Remains on crash.
  // 2. $CACHE/ffmpeg : ffmpeg cache.
  //    To be cleared on the end of rendering, on exist and on launch.
  // 3. $CACHE/temp : untitled scene data.
  //    To be deleted on switching or exiting scenes. Remains on crash.

  // So, this function will delete all files / folders in $CACHE
  // except the following items:
  // 1. $CACHE/[Current ProcessID]
  // 2. $CACHE/temp/[Current scene folder] if the current scene is untitled

  TFilePath cacheRoot                = ToonzFolder::getCacheRootFolder();
  if (cacheRoot.isEmpty()) cacheRoot = TEnv::getStuffDir() + "cache";

  TFilePathSet filesToBeRemoved;

  TSystem::readDirectory(filesToBeRemoved, cacheRoot, false);

  // keep the imagecache folder
  filesToBeRemoved.remove(cacheRoot + std::to_string(TSystem::getProcessId()));
  // keep the untitled scene data folder
  if (TApp::instance()->getCurrentScene()->getScene()->isUntitled()) {
    filesToBeRemoved.remove(cacheRoot + "temp");
    TFilePathSet untitledData =
        TSystem::readDirectory(cacheRoot + "temp", false);
    untitledData.remove(TApp::instance()
                            ->getCurrentScene()
                            ->getScene()
                            ->getScenePath()
                            .getParentDir());
    filesToBeRemoved.insert(filesToBeRemoved.end(), untitledData.begin(),
                            untitledData.end());
  }

  // return if there is no files/folders to be deleted
  if (filesToBeRemoved.size() == 0) {
    QMessageBox::information(
        this, tr("Clear Cache Folder"),
        tr("There are no unused items in the cache folder."));
    return;
  }

  QString message(tr("Deleting the following items:\n"));
  int count = 0;
  for (const auto &fileToBeRemoved : filesToBeRemoved) {
    QString dirPrefix =
        (TFileStatus(fileToBeRemoved).isDirectory()) ? tr("<DIR> ") : "";
    message +=
        "   " + dirPrefix + (fileToBeRemoved - cacheRoot).getQString() + "\n";
    count++;
    if (count == 5) break;
  }
  if (filesToBeRemoved.size() > 5)
    message +=
        tr("   ... and %1 more items\n").arg(filesToBeRemoved.size() - 5);

  message +=
      tr("\nAre you sure?\n\nN.B. Make sure you are not running another "
         "process of Tahoma2D,\nor you may delete necessary files for it.");

  QMessageBox::StandardButton ret = QMessageBox::question(
      this, tr("Clear Cache Folder"), message,
      QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel));

  if (ret != QMessageBox::Ok) return;

  for (const auto &fileToBeRemoved : filesToBeRemoved) {
    try {
      if (TFileStatus(fileToBeRemoved).isDirectory())
        TSystem::rmDirTree(fileToBeRemoved);
      else
        TSystem::deleteFile(fileToBeRemoved);
    } catch (TException &e) {
      QMessageBox::warning(
          this, tr("Clear Cache Folder"),
          tr("Can't delete %1 : ").arg(fileToBeRemoved.getQString()) +
              QString::fromStdWString(e.getMessage()));
    } catch (...) {
      QMessageBox::warning(
          this, tr("Clear Cache Folder"),
          tr("Can't delete %1 : ").arg(fileToBeRemoved.getQString()));
    }
  }
}

//-----------------------------------------------------------------------------

void MainWindow::toggleStatusBar(bool on) {
  if (!on) {
    m_statusBar->hide();
    ShowStatusBarAction = 0;
  } else {
    m_statusBar->show();
    ShowStatusBarAction = 1;
  }
}

//-----------------------------------------------------------------------------

void MainWindow::toggleTransparency(bool on) {
  if (!on) {
    this->setProperty("windowOpacity", 1.0);
  } else {
    this->setProperty("windowOpacity", (double)TransparencySliderValue / 100);
    m_transparencyTogglerWindow->show();
  }
}

//-----------------------------------------------------------------------------

void MainWindow::makeTransparencyDialog() {
  m_transparencyTogglerWindow = new QDialog();
  m_transparencyTogglerWindow->setWindowFlags(Qt::WindowStaysOnTopHint |
                                              Qt::WindowCloseButtonHint);

  m_transparencyTogglerWindow->setFixedHeight(100);
  m_transparencyTogglerWindow->setFixedWidth(250);
  m_transparencyTogglerWindow->setWindowTitle(tr("Tahoma2D Transparency"));
  QPushButton *toggleButton = new QPushButton(this);
  toggleButton->setText(tr("Close to turn off Transparency."));
  connect(toggleButton, &QPushButton::clicked,
          [=]() { m_transparencyTogglerWindow->accept(); });
  m_transparencySlider = new QSlider(this);
  m_transparencySlider->setRange(-100, -30);
  m_transparencySlider->setValue(TransparencySliderValue * -1);
  m_transparencySlider->setOrientation(Qt::Horizontal);
  connect(m_transparencySlider, &QSlider::valueChanged, [=](int value) {
    TransparencySliderValue = value * -1;
    toggleTransparency(true);
  });

  QVBoxLayout *togglerLayout       = new QVBoxLayout();
  QHBoxLayout *togglerSliderLayout = new QHBoxLayout();
  togglerSliderLayout->addWidget(new QLabel(tr("Amount: "), this));
  togglerSliderLayout->addWidget(m_transparencySlider);
  togglerLayout->addLayout(togglerSliderLayout);
  togglerLayout->addWidget(toggleButton);

  m_transparencyTogglerWindow->setLayout(togglerLayout);
}

//-----------------------------------------------------------------------------

class ToggleStatusBar final : public MenuItemHandler {
public:
  ToggleStatusBar() : MenuItemHandler("MI_ShowStatusBar") {}
  void execute() override {}
} toggleStatusBar;

//-----------------------------------------------------------------------------

class ToggleTransparency final : public MenuItemHandler {
public:
  ToggleTransparency() : MenuItemHandler("MI_ToggleTransparent") {}
  void execute() override {}
} toggleTransparency;

//-----------------------------------------------------------------------------

class ReloadStyle final : public MenuItemHandler {
public:
  ReloadStyle() : MenuItemHandler("MI_ReloadStyle") {}
  void execute() override {
    QString currentStyle = Preferences::instance()->getCurrentStyleSheetPath();
    QFile file(currentStyle);
    file.open(QFile::ReadOnly);
    QString styleSheet = QString(file.readAll());
    qApp->setStyleSheet(styleSheet);
    file.close();
  }
} reloadStyle;

void MainWindow::onQuit() { close(); }

//=============================================================================
// RecentFiles
//=============================================================================

RecentFiles::RecentFiles()
    : m_recentScenes()
    , m_recentSceneProjects()
    , m_recentLevels()
    , m_recentProjects() {}

//-----------------------------------------------------------------------------

RecentFiles *RecentFiles::instance() {
  static RecentFiles _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

RecentFiles::~RecentFiles() {}

//-----------------------------------------------------------------------------

void RecentFiles::addFilePath(QString path, FileType fileType,
                              QString projectName) {
  QList<QString> files =
      (fileType == Scene) ? m_recentScenes : (fileType == Level)
                                                 ? m_recentLevels
                                                 : (fileType == Project)
                                                       ? m_recentProjects
                                                       : m_recentFlipbookImages;
  int i;
  for (i = 0; i < files.size(); i++)
    if (files.at(i) == path) {
      files.removeAt(i);
      if (fileType == Scene) m_recentSceneProjects.removeAt(i);
    }
  files.insert(0, path);
  if (fileType == Scene) m_recentSceneProjects.insert(0, projectName);
  int maxSize = 10;
  if (files.size() > maxSize) {
    files.removeAt(maxSize);
    if (fileType == Scene) m_recentSceneProjects.removeAt(maxSize);
  }

  if (fileType == Scene)
    m_recentScenes = files;
  else if (fileType == Level)
    m_recentLevels = files;
  else if (fileType == Project)
    m_recentProjects = files;
  else
    m_recentFlipbookImages = files;

  refreshRecentFilesMenu(fileType);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::moveFilePath(int fromIndex, int toIndex, FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.move(fromIndex, toIndex);
    m_recentSceneProjects.move(fromIndex, toIndex);
  } else if (fileType == Level)
    m_recentLevels.move(fromIndex, toIndex);
  else if (fileType == Project)
    m_recentProjects.move(fromIndex, toIndex);
  else
    m_recentFlipbookImages.move(fromIndex, toIndex);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::removeFilePath(int index, FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.removeAt(index);
    m_recentSceneProjects.removeAt(index);
  } else if (fileType == Level)
    m_recentLevels.removeAt(index);
  else if (fileType == Project)
    m_recentProjects.removeAt(index);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

QString RecentFiles::getFilePath(int index, FileType fileType) const {
  return (fileType == Scene)
             ? m_recentScenes[index]
             : (fileType == Level)
                   ? m_recentLevels[index]
                   : (fileType == Project) ? m_recentProjects[index]
                                           : m_recentFlipbookImages[index];
}

//-----------------------------------------------------------------------------

QString RecentFiles::getFileProject(int index) const {
  if (index >= m_recentScenes.size() || index >= m_recentSceneProjects.size())
    return "-";
  return m_recentSceneProjects[index];
}

QString RecentFiles::getFileProject(QString fileName) const {
  for (int index = 0; index < m_recentScenes.size(); index++)
    if (m_recentScenes[index] == fileName) return m_recentSceneProjects[index];

  return "-";
}

//-----------------------------------------------------------------------------

void RecentFiles::clearRecentFilesList(FileType fileType) {
  if (fileType == Scene) {
    m_recentScenes.clear();
    m_recentSceneProjects.clear();
  } else if (fileType == Level)
    m_recentLevels.clear();
  else if (fileType == Project)
    m_recentProjects.clear();
  else
    m_recentFlipbookImages.clear();

  refreshRecentFilesMenu(fileType);
  saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::clearAllRecentFilesList(bool saveNow) {
  m_recentScenes.clear();
  m_recentSceneProjects.clear();
  m_recentLevels.clear();
  m_recentFlipbookImages.clear();
  if (saveNow) saveRecentFiles();
}

//-----------------------------------------------------------------------------

void RecentFiles::loadRecentFiles() {
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("RecentFiles.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  int i;

  QList<QVariant> scenes = settings.value(QString("Scenes")).toList();
  if (!scenes.isEmpty()) {
    for (i = 0; i < scenes.size(); i++)
      m_recentScenes.append(scenes.at(i).toString());
  } else {
    QString scene = settings.value(QString("Scenes")).toString();
    if (!scene.isEmpty()) m_recentScenes.append(scene);
  }

  // Load scene's projects info. This is for display purposes only. For
  // backwards compatibility it is stored and maintained separately.
  QList<QVariant> sceneProjects =
      settings.value(QString("SceneProjects")).toList();
  if (!sceneProjects.isEmpty()) {
    for (i = 0; i < sceneProjects.size(); i++)
      m_recentSceneProjects.append(sceneProjects.at(i).toString());
  } else {
    QString sceneProject = settings.value(QString("SceneProjects")).toString();
    if (!sceneProject.isEmpty()) m_recentSceneProjects.append(sceneProject);
  }
  // Should be 1-to-1. If we're short, append projects list with "-".
  while (m_recentSceneProjects.size() < m_recentScenes.size())
    m_recentSceneProjects.append("-");

  QList<QVariant> levels = settings.value(QString("Levels")).toList();
  if (!levels.isEmpty()) {
    for (i = 0; i < levels.size(); i++) {
      QString path = levels.at(i).toString();
      m_recentLevels.append(path);
    }
  } else {
    QString level = settings.value(QString("Levels")).toString();
    if (!level.isEmpty()) m_recentLevels.append(level);
  }

  QList<QVariant> projects = settings.value(QString("Projects")).toList();
  if (!projects.isEmpty()) {
    for (i = 0; i < projects.size(); i++)
      m_recentProjects.append(projects.at(i).toString());
  } else {
    QString project = settings.value(QString("Projects")).toString();
    if (!project.isEmpty()) m_recentProjects.append(project);
  }

  QList<QVariant> flipImages =
      settings.value(QString("FlipbookImages")).toList();
  if (!flipImages.isEmpty()) {
    for (i = 0; i < flipImages.size(); i++)
      m_recentFlipbookImages.append(flipImages.at(i).toString());
  } else {
    QString flipImage = settings.value(QString("FlipbookImages")).toString();
    if (!flipImage.isEmpty()) m_recentFlipbookImages.append(flipImage);
  }

  refreshRecentFilesMenu(Scene);
  refreshRecentFilesMenu(Level);
  refreshRecentFilesMenu(Flip);
  refreshRecentFilesMenu(Project);
}

//-----------------------------------------------------------------------------

void RecentFiles::saveRecentFiles() {
  TFilePath fp = ToonzFolder::getMyModuleDir() + TFilePath("RecentFiles.ini");
  QSettings settings(toQString(fp), QSettings::IniFormat);
  settings.setValue(QString("Scenes"), QVariant(m_recentScenes));
  settings.setValue(QString("SceneProjects"), QVariant(m_recentSceneProjects));
  settings.setValue(QString("Levels"), QVariant(m_recentLevels));
  settings.setValue(QString("Projects"), QVariant(m_recentProjects));
  settings.setValue(QString("FlipbookImages"),
                    QVariant(m_recentFlipbookImages));
}

//-----------------------------------------------------------------------------

void RecentFiles::updateStuffPath(QString oldPath, QString newPath) {
  m_recentScenes.replaceInStrings(oldPath, newPath);
  m_recentLevels.replaceInStrings(oldPath, newPath);
  m_recentFlipbookImages.replaceInStrings(oldPath, newPath);
}

//-----------------------------------------------------------------------------

QList<QString> RecentFiles::getFilesNameList(FileType fileType) {
  QList<QString> files =
      (fileType == Scene) ? m_recentScenes : (fileType == Level)
                                                 ? m_recentLevels
                                                 : (fileType == Project)
                                                       ? m_recentProjects
                                                       : m_recentFlipbookImages;
  QList<QString> names;
  int i;
  for (i = 0; i < files.size(); i++) {
    TFilePath path(files.at(i).toStdWString());
    QString str, number;
    if (fileType != Project)
      names.append(number.number(i + 1) + QString(". ") +
                   str.fromStdWString(path.getWideString()));
    else
      names.append(str.fromStdWString(path.getWideString()));
  }
  return names;
}

//-----------------------------------------------------------------------------

void RecentFiles::refreshRecentFilesMenu(FileType fileType) {
  CommandId id = (fileType == Scene)
                     ? MI_OpenRecentScene
                     : (fileType == Level)
                           ? MI_OpenRecentLevel
                           : (fileType == Project) ? MI_OpenRecentProject
                                                   : MI_LoadRecentImage;
  QAction *act = CommandManager::instance()->getAction(id);
  if (!act) return;
  DVMenuAction *menu = dynamic_cast<DVMenuAction *>(act->menu());
  if (!menu) return;
  QList<QString> names = getFilesNameList(fileType);
  if (names.isEmpty())
    menu->setEnabled(false);
  else {
    CommandId clearActionId =
        (fileType == Scene) ? MI_ClearRecentScene
                            : (fileType == Level) ? MI_ClearRecentLevel
                                                  : (fileType == Project)
                                                        ? MI_ClearRecentProject
                                                        : MI_ClearRecentImage;
    if (fileType == Project) {
      QString number;
      QList<QString> prjNames;
      TProjectManager *pm = TProjectManager::instance();
      for (int i = 0; i < names.size(); i++)
        prjNames.push_back(
            number.number(i + 1) + QString(". ") +
            TFilePath(names.at(i)).withoutParentDir().getQString());
      names = prjNames;
    }
    menu->setActions(names);
    menu->addSeparator();
    QAction *clearAction = CommandManager::instance()->getAction(clearActionId);
    assert(clearAction);
    menu->addAction(clearAction);
    if (!menu->isEnabled()) menu->setEnabled(true);
  }
}
