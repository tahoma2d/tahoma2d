

#include "tapp.h"

#include "tenv.h"

// Tnz6 includes
#include "cleanupsettingspopup.h"
#include "iocommand.h"
#include "mainwindow.h"
#include "cellselection.h"
#include "sceneviewer.h"
#include "statusbar.h"
#include "menubarcommandids.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

// TnzQt includes
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tstageobject.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tonionskinmaskhandle.h"
#include "toonz/tfxhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/sceneproperties.h"
#include "toonz/cleanupparameters.h"
#include "toonz/stage2.h"
#include "toutputproperties.h"
#include "toonz/palettecontroller.h"
#include "toonz/levelset.h"
#include "toonz/toonzscene.h"
#include "toonz/txshlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcamera.h"
#include "toonz/preferences.h"
#include "toonz/fullcolorpalette.h"
#include "toonz/txshsoundcolumn.h"

#include "toonzqt/tabbar.h"

// TnzCore includes
#include "tbigmemorymanager.h"
#include "ttoonzimage.h"
#include "trasterimage.h"
#include "tunit.h"
#include "tsystem.h"

// Qt includes
#include <QTimer>
#include <QDebug>
#include <QEvent>
#include <QCoreApplication>
#include <QApplication>
#include <QDesktopWidget>
#include <QKeyEvent>

//===================================================================

namespace {

double getCurrentCameraSize() {
  return TApp::instance()
      ->getCurrentScene()
      ->getScene()
      ->getCurrentCamera()
      ->getSize()
      .lx;
}

std::pair<double, double> getCurrentDpi() {
  TPointD dpi = TApp::instance()
                    ->getCurrentScene()
                    ->getScene()
                    ->getCurrentCamera()
                    ->getDpi();
  return std::make_pair(dpi.x, dpi.y);
}

}  // namespace

TEnv::IntVar ShowTitleBarsWhenLocked("ShowTitleBarsWhenLocked", 0);
TEnv::IntVar CanHideTitleBarsWhenLocked("CanHideTitleBarsWhenLocked", 1);

//=============================================================================
// TApp
//-----------------------------------------------------------------------------

TApp::TApp()
    : m_currentScene(0)
    , m_currentXsheet(0)
    , m_currentFrame(0)
    , m_currentColumn(0)
    , m_currentLevel(0)
    , m_currentTool(0)
    , m_currentObject(0)
    , m_currentSelection(0)
    , m_currentOnionSkinMask(0)
    , m_currentFx(0)
    , m_mainWindow(0)
    , m_autosaveTimer(0)
    , m_autosaveSuspended(false)
    , m_isStarting(false)
    , m_isPenCloseToTablet(false)
    , m_canHideTitleBars(CanHideTitleBarsWhenLocked == 1 ? true : false)
    , m_showTitleBars(ShowTitleBarsWhenLocked == 1 ? true : false) {
  m_currentScene         = new TSceneHandle();
  m_currentXsheet        = new TXsheetHandle();
  m_currentFrame         = new TFrameHandle();
  m_currentColumn        = new TColumnHandle();
  m_currentLevel         = new TXshLevelHandle();
  m_currentTool          = new ToolHandle();
  m_currentObject        = new TObjectHandle();
  m_currentOnionSkinMask = new TOnionSkinMaskHandle();
  m_currentFx            = new TFxHandle();
  m_currentSelection     = TSelectionHandle::getCurrent();

  m_paletteController = new PaletteController();

  bool ret = true;

  ret = ret && QObject::connect(m_currentXsheet, SIGNAL(xsheetChanged()), this,
                                SLOT(onXsheetChanged()));

  ret = ret && QObject::connect(m_currentXsheet, SIGNAL(xsheetSoundChanged()),
                                this, SLOT(onXsheetSoundChanged()));

  ret = ret && QObject::connect(m_currentScene, SIGNAL(sceneSwitched()), this,
                                SLOT(onSceneSwitched()));

  ret = ret && QObject::connect(m_currentScene, SIGNAL(sceneChanged()), this,
                                SLOT(onSceneChanged()));

  ret = ret && QObject::connect(m_currentXsheet, SIGNAL(xsheetSwitched()), this,
                                SLOT(onXsheetSwitched()));

  ret = ret && QObject::connect(m_currentFrame, SIGNAL(frameSwitched()), this,
                                SLOT(onFrameSwitched()));

  ret = ret && QObject::connect(m_currentFrame, SIGNAL(frameSwitched()), this,
                                SLOT(onImageChanged()));

  ret = ret && QObject::connect(m_currentFx, SIGNAL(fxSwitched()), this,
                                SLOT(onFxSwitched()));

  ret = ret && QObject::connect(m_currentColumn, SIGNAL(columnIndexSwitched()),
                                this, SLOT(onColumnIndexSwitched()));

  ret = ret && QObject::connect(m_currentColumn, SIGNAL(columnIndexSwitched()),
                                this, SLOT(onImageChanged()));

  ret = ret &&
        QObject::connect(m_currentLevel, SIGNAL(xshLevelSwitched(TXshLevel *)),
                         this, SLOT(onImageChanged()));

  ret = ret &&
        QObject::connect(m_currentLevel, SIGNAL(xshLevelSwitched(TXshLevel *)),
                         this, SLOT(onXshLevelSwitched(TXshLevel *)));

  ret = ret && QObject::connect(m_currentLevel, SIGNAL(xshLevelChanged()), this,
                                SLOT(onXshLevelChanged()));

  ret = ret && QObject::connect(m_currentObject, SIGNAL(objectSwitched()), this,
                                SLOT(onObjectSwitched()));

  ret = ret && QObject::connect(m_currentObject, SIGNAL(splineChanged()), this,
                                SLOT(onSplineChanged()));

  ret = ret && QObject::connect(m_paletteController->getCurrentLevelPalette(),
                                SIGNAL(paletteChanged()), this,
                                SLOT(onPaletteChanged()));

  ret = ret && QObject::connect(m_paletteController->getCurrentLevelPalette(),
                                SIGNAL(colorStyleSwitched()), this,
                                SLOT(onLevelColorStyleSwitched()));

  ret = ret && QObject::connect(m_paletteController->getCurrentLevelPalette(),
                                SIGNAL(colorStyleChangedOnMouseRelease()), this,
                                SLOT(onLevelColorStyleChanged()));

  ret = ret && QObject::connect(m_paletteController->getCurrentCleanupPalette(),
                                SIGNAL(paletteChanged()), m_currentScene,
                                SIGNAL(sceneChanged()));

  TMeasureManager::instance()->addCameraMeasures(getCurrentCameraSize);

  m_autosaveTimer = new QTimer(this);
  ret             = ret &&
        connect(m_autosaveTimer, SIGNAL(timeout()), this, SLOT(autosave()));

  Preferences *preferences = Preferences::instance();

  if (preferences->isRasterOptimizedMemory()) {
    if (!TBigMemoryManager::instance()->init(
            (int)(/*15*1024*/ TSystem::getFreeMemorySize(true) * .8)))
      DVGui::warning(tr("Error allocating memory: not enough memory."));
  }
  ret = ret &&
        connect(preferences, SIGNAL(stopAutoSave()), SLOT(onStopAutoSave()));
  ret = ret &&
        connect(preferences, SIGNAL(startAutoSave()), SLOT(onStartAutoSave()));
  ret = ret && connect(m_currentTool, SIGNAL(toolEditingFinished()),
                       SLOT(onToolEditingFinished()));

  if (preferences->isAutosaveEnabled())
    m_autosaveTimer->start(preferences->getAutosavePeriod() * 1000 * 60);

  UnitParameters::setCurrentDpiGetter(getCurrentDpi);
  assert(ret);

  if (!m_canHideTitleBars) m_showTitleBars = true;
}

//-----------------------------------------------------------------------------

TApp *TApp::instance() {
  static TApp _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

TApp::~TApp() {}

//-----------------------------------------------------------------------------

void TApp::init() {
  m_isStarting = true;
  IoCmd::newScene();
  m_currentColumn->setColumnIndex(0);
  m_currentFrame->setFrame(0);
  m_isStarting = false;
}

//-----------------------------------------------------------------------------

TMainWindow *TApp::getCurrentRoom() const {
  MainWindow *mainWindow = dynamic_cast<MainWindow *>(getMainWindow());
  if (mainWindow)
    return mainWindow->getCurrentRoom();
  else
    return 0;
}

//-----------------------------------------------------------------------------

void TApp::writeSettings() {
  MainWindow *mainWindow = dynamic_cast<MainWindow *>(getMainWindow());
  if (mainWindow) mainWindow->refreshWriteSettings();
}

//-----------------------------------------------------------------------------

TPaletteHandle *TApp::getCurrentPalette() const {
  return m_paletteController->getCurrentPalette();
}

//-----------------------------------------------------------------------------

TColorStyle *TApp::getCurrentLevelStyle() const {
  return m_paletteController->getCurrentLevelPalette()->getStyle();
}

//-----------------------------------------------------------------------------

int TApp::getCurrentLevelStyleIndex() const {
  return m_paletteController->getCurrentLevelPalette()->getStyleIndex();
}

//-----------------------------------------------------------------------------

void TApp::setCurrentLevelStyleIndex(int index, bool forceUpdate) {
  m_paletteController->getCurrentLevelPalette()->setStyleIndex(index,
                                                               forceUpdate);
}

//-----------------------------------------------------------------------------

int TApp::getCurrentImageType() {
  /*-- 現在のセルの種類に関係無く、Splineを選択中はベクタを編集できるようにする
   * --*/
  if (getCurrentObject()->isSpline()) return TImage::VECTOR;

  TXshSimpleLevel *sl = 0;

  if (getCurrentFrame()->isEditingScene()) {
    int row = getCurrentFrame()->getFrame();
    int col = getCurrentColumn()->getColumnIndex();
    if (col < 0) {
      int levelType = Preferences::instance()->getDefLevelType();
      return (levelType == PLI_XSHLEVEL)
                 ? TImage::VECTOR
                 :  // RASTER image type includes both TZI_XSHLEVEL
                 (levelType == TZP_XSHLEVEL)
                     ? TImage::TOONZ_RASTER
                     : TImage::RASTER;  // and OVL_XSHLEVEL level types
    }

    TXsheet *xsh = getCurrentXsheet()->getXsheet();
    if (xsh->getColumn(col) && xsh->getColumn(col)->getSoundColumn())
      return TImage::VECTOR;
    TXshCell cell = xsh->getCell(row, col);
    if (cell.isEmpty()) {
      int r0, r1;
      xsh->getCellRange(col, r0, r1);
      if (0 <= r0 && r0 <= r1) {
        /*-- Columnに格納されている一番上のLevelのTypeに合わせる--*/
        cell = xsh->getCell(r0, col);
      } else /*-- Columnが空の場合 --*/
      {
        int levelType = Preferences::instance()->getDefLevelType();
        return (levelType == PLI_XSHLEVEL)
                   ? TImage::VECTOR
                   : (levelType == TZP_XSHLEVEL) ? TImage::TOONZ_RASTER
                                                 : TImage::RASTER;
      }
    }

    sl = cell.getSimpleLevel();
  } else if (getCurrentFrame()->isEditingLevel())
    sl = getCurrentLevel()->getSimpleLevel();

  if (sl) {
    switch (sl->getType()) {
    case TZP_XSHLEVEL:
      return TImage::TOONZ_RASTER;
    case OVL_XSHLEVEL:
      return TImage::RASTER;
    case PLI_XSHLEVEL:
    default:
      return TImage::VECTOR;
    case MESH_XSHLEVEL:
      return TImage::MESH;
    }
  }

  return TImage::VECTOR;
}

//-----------------------------------------------------------------------------

void TApp::updateXshLevel() {
  TXshLevel *xl = 0;
  if (m_currentFrame->isEditingScene()) {
    int frame       = m_currentFrame->getFrame();
    int column      = m_currentColumn->getColumnIndex();
    TXsheet *xsheet = m_currentXsheet->getXsheet();

    bool isSoundColumn = false;
    if (xsheet->getColumn(column) &&
        xsheet->getColumn(column)->getSoundColumn()) {
      isSoundColumn = true;
      if (xsheet->getColumn(column)->getSoundColumn()->m_levels.size() > 0) {
        xl = static_cast<TXshLevel *>(xsheet->getColumn(column)
                                          ->getSoundColumn()
                                          ->m_levels.at(0)
                                          ->getSoundLevel());
      }
    } else if (xsheet && column >= 0 && frame >= 0 &&
               !xsheet->isColumnEmpty(column)) {
      TXshCell cell = xsheet->getCell(frame, column);
      xl            = cell.m_level.getPointer();

      // If I'm on an empty cell next to cells of a certain level
      // I take this as the current level.
      if (!xl && frame > 0) {
        TXshCell cell = xsheet->getCell(frame - 1, column);
        xl            = cell.m_level.getPointer();
      }

      // If we're on an empty cell and auto create is enabled,
      // the current level will be the last level before us
      if (!xl && Preferences::instance()->isAutoCreateEnabled()) {
        int r0, r1;
        xsheet->getCellRange(column, r0, r1);
        for (int r = std::max(r0, std::min(r1, frame)); r >= r0; r--) {
          TXshCell cell = xsheet->getCell(r, column);
          if (cell.isEmpty()) continue;
          xl = cell.m_level.getPointer();
          break;
        }
      }
    }

    m_currentLevel->setLevel(xl);

    // level could be the same, but palette could have changed
    if (xl && xl->getSimpleLevel()) {
      TPalette *currentPalette =
          m_paletteController->getCurrentPalette()->getPalette();
      int styleIndex =
          m_paletteController->getCurrentLevelPalette()->getStyleIndex();

      m_paletteController->getCurrentLevelPalette()->setPalette(
          xl->getSimpleLevel()->getPalette(), styleIndex);
      if (xl->getType() == OVL_XSHLEVEL && currentPalette &&
          currentPalette->isCleanupPalette())

        m_paletteController->editCleanupPalette();
    } else if (xl && xl->getPaletteLevel()) {
      int styleIndex =
          m_paletteController->getCurrentLevelPalette()->getStyleIndex();
      m_paletteController->getCurrentLevelPalette()->setPalette(
          xl->getPaletteLevel()->getPalette(), styleIndex);
    } else {
      TPalette *defaultPalette = 0;
      // Show default level type palette on empty columns when autocreate is
      // enabled
      if (column >= 0 && xsheet->isColumnEmpty(column) &&
          Preferences::instance()->isAutoCreateEnabled()) {
        int defaultLevelType = Preferences::instance()->getDefLevelType();
        if (defaultLevelType == OVL_XSHLEVEL) {
          ToonzScene *scene = m_currentScene->getScene();
          defaultPalette    = FullColorPalette::instance()->getPalette(scene);
        }

        if (!defaultPalette)
          defaultPalette =
              m_paletteController->getDefaultPalette(defaultLevelType);
      }
      m_paletteController->getCurrentLevelPalette()->setPalette(defaultPalette,
                                                                1);
    }
  }
}

//-----------------------------------------------------------------------------

void TApp::updateCurrentFrame() {
  ToonzScene *scene = m_currentScene->getScene();
  m_currentFrame->setSceneFrameSize(scene->getFrameCount());
  int f0, f1, step;
  scene->getProperties()->getPreviewProperties()->getRange(f0, f1, step);
  if (f0 > f1) {
    f0 = 0;
    f1 = scene->getFrameCount() - 1;
  }
  if (f0 != m_currentFrame->getStartFrame() ||
      f1 != m_currentFrame->getEndFrame()) {
    m_currentFrame->setFrameRange(f0, f1);
    std::vector<TFrameId> fids;
    TXshSimpleLevel *sl = m_currentLevel->getSimpleLevel();
    if (sl) {
      sl->getFids(fids);
      m_currentFrame->setFrameIds(fids);
    }
  }
}

//-----------------------------------------------------------------------------

void TApp::onSceneSwitched() {
  // update XSheet
  m_currentXsheet->setXsheet(m_currentScene->getScene()->getXsheet());

  TPalette *palette = m_currentScene->getScene()
                          ->getProperties()
                          ->getCleanupParameters()
                          ->m_cleanupPalette.getPointer();
  m_paletteController->getCurrentCleanupPalette()->setPalette(palette, -1);

  m_paletteController->editLevelPalette();

  // reset current frame
  m_currentFrame->setFrame(0);

  // clear current onionSkinMask
  m_currentOnionSkinMask->clear();

  // update currentFrames
  updateCurrentFrame();

  // update current tool
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}

//-----------------------------------------------------------------------------

void TApp::onImageChanged() {
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}

//-----------------------------------------------------------------------------

void TApp::onXsheetSwitched() {
  // update current object
  int columnIndex = m_currentColumn->getColumnIndex();
  if (columnIndex >= 0)
    m_currentObject->setObjectId(TStageObjectId::ColumnId(columnIndex));

  // update xsheetlevel
  updateXshLevel();

  // no Fx is set to current.
  m_currentFx->setFx(0);
}

//-----------------------------------------------------------------------------

void TApp::onXsheetChanged() {
  updateXshLevel();
  updateCurrentFrame();
  // update current tool
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}

//-----------------------------------------------------------------------------

void TApp::onXsheetSoundChanged() {
  m_currentXsheet->getXsheet()->invalidateSound();
}

//-----------------------------------------------------------------------------

void TApp::onFrameSwitched() {
  updateXshLevel();
  int row = m_currentFrame->getFrameIndex();
  TCellSelection *sel =
      dynamic_cast<TCellSelection *>(TSelection::getCurrent());

  if (sel && !sel->isRowSelected(row) &&
      !Preferences::instance()->isUseArrowKeyToShiftCellSelectionEnabled()) {
    sel->selectNone();
  }
}

//-----------------------------------------------------------------------------

void TApp::onFxSwitched() {
  //  if(m_currentFx->getFx() != 0)
  //    m_currentTool->setTool(T_Edit);
}

//-----------------------------------------------------------------------------

void TApp::onColumnIndexSwitched() {
  // update xsheetlevel
  updateXshLevel();

  // update current object
  int columnIndex = m_currentColumn->getColumnIndex();
  if (columnIndex >= 0)
    m_currentObject->setObjectId(TStageObjectId::ColumnId(columnIndex));
  else {
    TXsheet *xsh = getCurrentXsheet()->getXsheet();
    m_currentObject->setObjectId(
        TStageObjectId::CameraId(xsh->getCameraColumnIndex()));
  }
}

//-----------------------------------------------------------------------------

void TApp::onXshLevelSwitched(TXshLevel *) {
  TXshLevel *level = m_currentLevel->getLevel();
  CommandManager::instance()->enable(MI_CanvasSize, false);
  if (level) {
    TXshSimpleLevel *simpleLevel = level->getSimpleLevel();

    // Devo aggiornare la paletta corrente
    if (simpleLevel) {
      m_paletteController->getCurrentLevelPalette()->setPalette(
          simpleLevel->getPalette());

      TPalette *currentPalette =
          m_paletteController->getCurrentPalette()->getPalette();

      if (simpleLevel->getType() == OVL_XSHLEVEL && currentPalette &&
          currentPalette->isCleanupPalette())
        m_paletteController->editCleanupPalette();

      bool isRasterLevel = (simpleLevel->getType() == TZP_XSHLEVEL ||
                            simpleLevel->getType() == OVL_XSHLEVEL ||
                            simpleLevel->getType() == TZI_XSHLEVEL);
      if (isRasterLevel && (simpleLevel->getPath().getType() == "psd" ||
                            simpleLevel->getPath().getType() == "gif" ||
                            simpleLevel->getPath().getType() == "mp4" ||
                            simpleLevel->getPath().getType() == "webm" ||
                            simpleLevel->getPath().getType() == "mov"))
        isRasterLevel = false;
      CommandManager::instance()->enable(MI_CanvasSize, isRasterLevel);

      return;
    }

    TXshPaletteLevel *paletteLevel = level->getPaletteLevel();
    if (paletteLevel) {
      m_paletteController->getCurrentLevelPalette()->setPalette(
          paletteLevel->getPalette());
      return;
    }
  }

  m_paletteController->getCurrentLevelPalette()->setPalette(0);
}

//-----------------------------------------------------------------------------

void TApp::onXshLevelChanged() {
  TXshLevel *level = m_currentLevel->getLevel();
  std::vector<TFrameId> fids;
  if (level != 0) level->getFids(fids);
  m_currentFrame->setFrameIds(fids);
  // update current tool
  m_currentTool->onImageChanged((TImage::Type)getCurrentImageType());
}
//-----------------------------------------------------------------------------

void TApp::onObjectSwitched() {
  if (m_currentObject->isSpline()) {
    TXsheet *xsh = m_currentXsheet->getXsheet();
    TStageObject *currentObject =
        xsh->getStageObject(m_currentObject->getObjectId());
    TStageObjectSpline *ps = currentObject->getSpline();
    m_currentObject->setSplineObject(ps);
  } else
    m_currentObject->setSplineObject(0);
  onImageChanged();
}

//-----------------------------------------------------------------------------

void TApp::onSplineChanged() {
  if (m_currentObject->isSpline()) {
    TXsheet *xsh = m_currentXsheet->getXsheet();
    TStageObject *currentObject =
        xsh->getStageObject(m_currentObject->getObjectId());
    currentObject->setOffset(
        currentObject->getOffset());  // invalidate currentObject
  }
}

//-----------------------------------------------------------------------------

void TApp::onSceneChanged() {
  updateCurrentFrame();
  m_currentTool->updateMatrix();
}

//-----------------------------------------------------------------------------

void TApp::onPaletteChanged() { m_currentScene->setDirtyFlag(true); }

//-----------------------------------------------------------------------------

void TApp::onLevelColorStyleSwitched() {
  TXshLevel *sl = m_currentLevel->getLevel();
  if (!sl || (sl->getType() != TZP_XSHLEVEL && sl->getType() != PLI_XSHLEVEL))
    return;

  TPaletteHandle *ph = m_paletteController->getCurrentLevelPalette();
  assert(ph);

  if (ToonzCheck::instance()->setColorIndex(ph->getStyleIndex())) {
    ToonzCheck *tc = ToonzCheck::instance();
    int mask       = tc->getChecks();

    if (mask & ToonzCheck::eInk || mask & ToonzCheck::ePaint) {
      IconGenerator::Settings s;
      s.m_blackBgCheck      = mask & ToonzCheck::eBlackBg;
      s.m_transparencyCheck = mask & ToonzCheck::eTransparency;
      s.m_inksOnly          = mask & ToonzCheck::eInksOnly;
      s.m_inkIndex   = mask & ToonzCheck::eInk ? tc->getColorIndex() : -1;
      s.m_paintIndex = mask & ToonzCheck::ePaint ? tc->getColorIndex() : -1;

      IconGenerator::instance()->setSettings(s);

      if (sl->getType() == PLI_XSHLEVEL) {
        std::vector<TFrameId> fids;
        sl->getFids(fids);
        for (int i = 0; i < (int)fids.size(); i++)
          IconGenerator::instance()->invalidate(sl, fids[i]);
      }

      m_currentLevel->notifyLevelViewChange();
    }

    /*-- 表示オプションが切り替わっただけなので、DirtyFlagを立てない --*/
    m_currentScene->notifySceneChanged(false);
  }
}

//-----------------------------------------------------------------------------

static void notifyPaletteChanged(TXshSimpleLevel *simpleLevel) {
  simpleLevel->onPaletteChanged();
  // palette change can update icons only for ToonzVector / ToonzRaster types
  if (simpleLevel->getType() != TZP_XSHLEVEL &&
      simpleLevel->getType() != PLI_XSHLEVEL)
    return;
  std::vector<TFrameId> fids;
  simpleLevel->getFids(fids);
  // ToonzRaster level does not need to re-generate icons along with palette
  // changes since the icons are cached as color mapped images and the current
  // palette is applied just before using it. So here we just emit the signal to
  // update related panels.
  if (simpleLevel->getType() == TZP_XSHLEVEL)
    IconGenerator::instance()->notifyIconGenerated();
  else {  // ToonzVecor needs to re-generate icons since it includes colors in
          // the cache.
    for (int i = 0; i < (int)fids.size(); i++)
      IconGenerator::instance()->invalidate(simpleLevel, fids[i]);
  }
}

//-----------------------------------------------------------------------------

void TApp::onLevelColorStyleChanged() {
  onPaletteChanged();
  TXshLevel *level = m_currentLevel->getLevel();
  if (!level) return;
  TPalette *palette            = getCurrentPalette()->getPalette();
  TXshSimpleLevel *simpleLevel = level->getSimpleLevel();
  if (simpleLevel && simpleLevel->getPalette() == palette) {
    notifyPaletteChanged(simpleLevel);
  } else {
    TLevelSet *levelSet = getCurrentScene()->getScene()->getLevelSet();
    for (int i = 0; i < levelSet->getLevelCount(); i++) {
      if (levelSet->getLevel(i)) {
        simpleLevel = levelSet->getLevel(i)->getSimpleLevel();
        if (simpleLevel && simpleLevel->getPalette() == palette) {
          notifyPaletteChanged(simpleLevel);
        }
      }
    }
  }
}

//-----------------------------------------------------------------------------

void TApp::autosave() {
  ToonzScene *scene = getCurrentScene()->getScene();

  if (!getCurrentScene()->getDirtyFlag()) return;

  if (getCurrentTool()->isToolBusy()) {
    m_autosaveSuspended = true;
    return;
  } else
    m_autosaveSuspended = false;

  // DVGui::ProgressDialog pb(
  //    "Autosaving scene..." + toQString(scene->getScenePath()), 0, 0, 1);
  // pb.show();
  Preferences *pref = Preferences::instance();
  if (pref->isAutosaveSceneEnabled() && pref->isAutosaveOtherFilesEnabled()) {
    IoCmd::saveAll(IoCmd::AUTO_SAVE);
  } else if (pref->isAutosaveSceneEnabled()) {
    IoCmd::saveScene(IoCmd::AUTO_SAVE);
  } else if (pref->isAutosaveOtherFilesEnabled()) {
    IoCmd::saveNonSceneFiles();
  }

  // pb.setValue(1);
}

//-----------------------------------------------------------------------------

void TApp::onToolEditingFinished() {
  if (!Preferences::instance()->isAutosaveEnabled() || !m_autosaveSuspended)
    return;
  autosave();
}

//-----------------------------------------------------------------------------

void TApp::onStartAutoSave() {
  assert(Preferences::instance()->isAutosaveEnabled());
  m_autosaveTimer->start(Preferences::instance()->getAutosavePeriod() * 1000 *
                         60);
}

//-----------------------------------------------------------------------------

void TApp::onStopAutoSave() {
  // assert(!Preferences::instance()->isAutosaveEnabled());
  m_autosaveTimer->stop();
}

//-----------------------------------------------------------------------------

bool TApp::eventFilter(QObject *watched, QEvent *e) {
  if (e->type() == QEvent::TabletEnterProximity) {
    m_isPenCloseToTablet = true;
  } else if (e->type() == QEvent::TabletLeaveProximity) {
    // if the user is painting very quickly with the pen, a number of events
    // could be still in the queue
    // the must be processed as tabled events (not mouse events)
    // qApp->processEvents();

    m_isPenCloseToTablet = false;
    emit tabletLeft();
  }
  if (e->type() == QEvent::KeyRelease) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
    std::string keyStr  = QKeySequence(keyEvent->key() + keyEvent->modifiers())
                             .toString()
                             .toStdString();
    QAction *action = CommandManager::instance()->getActionFromShortcut(keyStr);
    std::string actionId = CommandManager::instance()->getIdFromAction(action);
    if (actionId == T_Hand || actionId == T_Zoom || actionId == T_Rotate) {
      if (!keyEvent->isAutoRepeat()) {
        SceneViewer *viewer = getActiveViewer();
        if (viewer) {
          viewer->resetNavigation();
        }
      }
    }
  }
  if (watched && watched->objectName() == "StyleEditorTabBar" && e &&
      e->type() == QEvent::MouseMove) {
    // m_statusBar->clearMessage();
    DVGui::TabBar *bar = static_cast<DVGui::TabBar *>(watched);
    int index          = bar->tabAt(static_cast<QMouseEvent *>(e)->pos());
    if (bar->tabText(index) == tr("Color")) {
      // bar->setStatusTip(tr("Adjust the color value(s) of the selected
      // style."));
      m_statusBar->showMessage(
          tr("Adjust the color value(s) of the selected style."), 0);
    } else if (bar->tabText(index) == tr("Texture")) {
      // bar->setStatusTip(tr("Apply a texture to the selected style.  This only
      // works on Vector and Smart Raster Levels.  Smart Raster levels require
      // preview to be on to see the style."));
      m_statusBar->showMessage(
          tr("Apply a texture to the selected style.                    This "
             "only works on Vector and Smart Raster Levels.                    "
             "Smart Raster levels require preview to be on to see the style."));
    } else if (bar->tabText(index) == tr("Vector")) {
      // bar->setStatusTip(tr("Apply unique strokes and fills to vector
      // styles."));
      m_statusBar->showMessage(
          tr("Apply unique strokes and fills to vector styles."));
    } else if (bar->tabText(index) == tr("Raster")) {
      // bar->setStatusTip(tr("Use MyPaint brushes on Smart Raster and Raster
      // levels."));
      m_statusBar->showMessage(
          tr("Use MyPaint brushes on Smart Raster and Raster levels."));
    } else if (bar->tabText(index) == tr("Settings")) {
      // bar->setStatusTip(tr("Adjust the settings depending on the style you
      // have selected."));
      m_statusBar->showMessage(
          tr("Adjust the settings depending on the style you have selected."));
    }
  }
  if (watched->objectName() == "StyleEditorTabBar" &&
      e->type() == QEvent::Leave) {
    m_statusBar->clearMessage();
  }

  return false;  // I want just peek at the event. It must be processed anyway.
}

//-----------------------------------------------------------------------------

void TApp::setStatusBar(StatusBar *statusBar) { m_statusBar = statusBar; }

//-----------------------------------------------------------------------------

void TApp::setStatusBarFrameInfo(QString text) {
  if (m_statusBar) m_statusBar->updateFrameText(text);
}

//-----------------------------------------------------------------------------

void TApp::showMessage(QString message, int duration) {
  if (m_statusBar) m_statusBar->showMessage(message, duration);
}

//-----------------------------------------------------------------------------

void TApp::refreshStatusBar() {
  if (m_statusBar) m_statusBar->refreshStatusBar();
}

//-----------------------------------------------------------------------------

QString TApp::getCurrentRoomName() const {
  Room *currentRoom = dynamic_cast<Room *>(getCurrentRoom());
  if (!currentRoom) return QString();

  return currentRoom->getName();
}

//-----------------------------------------------------------------------------

void TApp::sendShowTitleBars(bool on, bool force) {
  if (m_canHideTitleBars || force) {
    m_showTitleBars         = on;
    ShowTitleBarsWhenLocked = on ? 1 : 0;
    emit(showTitleBars(on));
  } else if (!m_canHideTitleBars) {
    m_showTitleBars         = true;
    ShowTitleBarsWhenLocked = 1;
    emit(showTitleBars(true));
  }
}

//-----------------------------------------------------------------------------

void TApp::setShowTitleBars(bool on) {
  if (m_canHideTitleBars) {
    m_showTitleBars         = on;
    ShowTitleBarsWhenLocked = on ? 1 : 0;
  } else {
    m_showTitleBars         = true;
    ShowTitleBarsWhenLocked = 1;
  }
}

//-----------------------------------------------------------------------------

void TApp::setCanHideTitleBars(bool on) {
  m_canHideTitleBars         = on;
  CanHideTitleBarsWhenLocked = on ? 1 : 0;
}
