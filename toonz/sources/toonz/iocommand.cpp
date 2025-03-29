#include <memory>

#include "iocommand.h"

// Toonz includes
#include "menubarcommandids.h"
#include "filebrowserpopup.h"
#include "tapp.h"
#include "history.h"
#include "fileselection.h"
#include "previewer.h"
#include "previewfxmanager.h"
#include "tcontenthistory.h"
#include "filebrowsermodel.h"
#include "mainwindow.h"
#include "overwritepopup.h"
#include "cleanupsettingspopup.h"
#include "psdsettingspopup.h"
#include "filebrowser.h"
#include "versioncontrol.h"
#include "cachefxcommand.h"
#include "xdtsio.h"
#include "expressionreferencemanager.h"
#include "levelcommand.h"
#include "columncommand.h"

// TnzTools includes
#include "tools/toolhandle.h"

// ToonzQt includes
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/swatchviewer.h"
#include "toonzqt/tselectionhandle.h"
#include "toonzqt/dvdialog.h"

// ToonzLib includes
#include "toonz/palettecontroller.h"
#include "toonz/tscenehandle.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/tproject.h"
#include "toonz/txshsimplelevel.h"
#include "toonz/txshchildlevel.h"
#include "toonz/sceneproperties.h"
#include "toonz/levelproperties.h"
#include "toonz/stage2.h"
#include "toonz/imagemanager.h"
#include "toonz/sceneresources.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/txshpalettecolumn.h"
#include "toonz/txshpalettelevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/txshsoundtextcolumn.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/levelset.h"
#include "toonz/namebuilder.h"
#include "toonz/fullcolorpalette.h"
#include "toonz/palettecmd.h"
#include "toonz/toonzimageutils.h"
#include "toonz/imagestyles.h"
#include "toutputproperties.h"
#include "toonz/studiopalette.h"
#include "toonz/tpalettehandle.h"
#include "toonz/tstageobjectcmd.h"

// TnzCore includes
#include "tofflinegl.h"
#include "tvectorimage.h"
#include "tvectorrenderdata.h"
#include "tropcm.h"
#include "tfiletype.h"
#include "tunit.h"
#include "tproperty.h"
#include "tlevel.h"
#include "tlevel_io.h"

// Qt includes
#include <QLabel>
#include <QApplication>
#include <QClipboard>

// boost includes
#include <boost/optional.hpp>
#include <boost/utility/in_place_factory.hpp>

// #define USE_SQLITE_HDPOOL

using namespace DVGui;

//-----------------------------------------------------------------------------
namespace {
//-----------------------------------------------------------------------------

TXshLevel *getLevelByPath(ToonzScene *scene, const TFilePath &actualPath);

// forward declaration
class RenderingSuspender;

//===========================================================================
// class ResourceImportDialog
//---------------------------------------------------------------------------

class ResourceImportDialog final : public ResourceImportStrategy {
  OverwriteDialog *m_dialog;
  bool m_isLastResource;
  bool m_importQuestionAsked;
  bool m_aborted;
  TFilePath m_dstFolder;
  bool m_importEnabled;
  std::map<TFilePath, TFilePath> m_importedFiles;

public:
  enum Resolution { A_IMPORT, A_LOAD, A_CANCEL };

public:
  ResourceImportDialog()
      : ResourceImportStrategy(ResourceImportStrategy::IMPORT_AND_RENAME)
      , m_dialog(0)
      , m_isLastResource(false)
      , m_importQuestionAsked(false)
      , m_aborted(false)
      , m_importEnabled(false) {
    m_dialog = new OverwriteDialog;
  }

  ~ResourceImportDialog() {
    if (m_dialog) m_dialog->deleteLater();
  }

  bool isImportEnabled() const { return m_importEnabled; }
  void setImportEnabled(bool enabled) {
    m_importQuestionAsked = true, m_importEnabled = enabled;
  }

  void setDstFolder(const TFilePath &dstFolder) { m_dstFolder = dstFolder; }
  TFilePath getDstFolder() const { return m_dstFolder; }

  int askImportQuestion(const TFilePath &path) {
    if (m_importQuestionAsked)
      return isImportEnabled() ? A_IMPORT : A_LOAD;
    else {
      m_importQuestionAsked = true;

      QString label =
          QObject::tr(
              "File %1 doesn't belong to the current project.\n"
              "Do you want to import it or load it from its original location?")
              .arg(QString::fromStdWString(path.getWideString()));
      QString checkBoxLabel =
          QObject::tr("Always do this action.")
              .arg(QString::fromStdWString(path.getWideString()));
      QStringList buttons;
      buttons << QObject::tr("Import") << QObject::tr("Load")
              << QObject::tr("Cancel");
      DVGui::MessageAndCheckboxDialog *importDialog =
          DVGui::createMsgandCheckbox(DVGui::QUESTION, label, checkBoxLabel,
                                      buttons, 0, Qt::Unchecked);
      int ret     = importDialog->exec();
      int checked = importDialog->getChecked();
      importDialog->deleteLater();

      if (ret == 0 || ret == 3) {
        m_aborted = true;
        return A_CANCEL;
      }
      if (ret == 1 && checked > 0) {
        Preferences::instance()->setValue(importPolicy, 1);
        TApp::instance()->getCurrentScene()->notifyImportPolicyChanged(1);
      } else if (ret == 2 && checked > 0) {
        Preferences::instance()->setValue(importPolicy, 2);
        TApp::instance()->getCurrentScene()->notifyImportPolicyChanged(2);
      }
      m_importEnabled = (ret == 1);
      return ret == 1 ? A_IMPORT : A_LOAD;
    }
  }

  void setIsLastResource(bool isLastResource) {
    m_isLastResource = isLastResource;
  }

  bool aborted() const { return m_aborted || m_dialog->cancelPressed(); }

  //
  // process the file 'srcPath' (possibly copying it)
  // scrPath can be a coded path (related to srcScene); srcScene can be 0
  // the method returns the processed codedPath (related to scene)
  //
  TFilePath process(ToonzScene *scene, ToonzScene *srcScene,
                    TFilePath srcPath) override {
    TFilePath actualSrcPath     = srcPath;
    if (srcScene) actualSrcPath = srcScene->decodeFilePath(srcPath);

    if (!isImportEnabled()) {
      TFilePath path = scene->codeFilePath(actualSrcPath);
      return path;
    }
    if ((!scene->isExternPath(actualSrcPath) && m_dstFolder.isEmpty()) ||
        m_dialog->cancelPressed())
      return srcPath;

    // find the proper dstPath (coded)
    TFilePath dstPath;
    if (srcPath.getWideString().find(L'+') == 0) {
      dstPath = srcPath;
      // override the folder
      if (m_dstFolder != TFilePath()) {
        // +drawings/oldfolder/level.pli =>
        // +drawings/xsheetfolder/oldfolder/level.pli
        std::wstring head;
        TFilePath tail;
        dstPath.split(head, tail);
        dstPath = TFilePath(head) + m_dstFolder + tail;
      }
    } else {
      dstPath = scene->getImportedLevelPath(srcPath);
      // override the folder
      if (m_dstFolder != TFilePath())
        dstPath = dstPath.withParentDir(dstPath.getParentDir() + m_dstFolder);
    }

    // actual path
    TFilePath actualDstPath = scene->decodeFilePath(dstPath);
    assert(actualDstPath != TFilePath());

    std::map<TFilePath, TFilePath>::iterator it =
        m_importedFiles.find(actualDstPath);
    if (it != m_importedFiles.end()) return it->second;
    m_importedFiles[actualDstPath] = dstPath;

    // possibly, a level already exists
    bool overwritten = false;
    if (TSystem::doesExistFileOrLevel(actualDstPath)) {
      std::wstring newName =
          m_dialog->execute(scene, dstPath, m_isLastResource == false);
      if (m_dialog->cancelPressed()) return srcPath;
      int importMode = m_dialog->getChoice();
      if (importMode == OverwriteDialog::KEEP_OLD)
        return dstPath;
      else if (importMode == OverwriteDialog::OVERWRITE)
        overwritten = true;
      else {
        dstPath                        = dstPath.withName(newName);
        m_importedFiles[actualDstPath] = dstPath;
        actualDstPath                  = actualDstPath.withName(newName);
      }
    }

    // copy resources
    try {
      if (TSystem::doesExistFileOrLevel(actualDstPath))
        TSystem::removeFileOrLevel(actualDstPath);
      if (TSystem::doesExistFileOrLevel(actualSrcPath))
        TXshSimpleLevel::copyFiles(actualDstPath, actualSrcPath);
    } catch (TException &e) {
      DVGui::Dialog *errorDialog = DVGui::createMsgBox(
          DVGui::WARNING,
          "Can't copy resources: " + QString::fromStdWString(e.getMessage()),
          QStringList("OK"), 0);

      errorDialog->exec();
      errorDialog->deleteLater();
    } catch (...) {
      DVGui::Dialog *errorDialog = DVGui::createMsgBox(
          DVGui::WARNING,
          "Can't copy resources: Unhandled exception encountered",
          QStringList("OK"), 0);

      errorDialog->exec();
      errorDialog->deleteLater();
    }
    // notify
    FileBrowser::refreshFolder(actualDstPath.getParentDir());
    IconGenerator::instance()->invalidate(actualDstPath);

    // refresh icons and level path
    if (overwritten) {
      TXshLevel *xl       = getLevelByPath(scene, actualDstPath);
      TXshSimpleLevel *sl = 0;
      if (xl && 0 != (sl = xl->getSimpleLevel())) {
        std::vector<TFrameId> fids;
        sl->getFids(fids);
        sl->setPath(sl->getPath(), false);
        for (int i = 0; i < (int)fids.size(); i++)
          IconGenerator::instance()->invalidate(sl, fids[i]);
      }
    }
    return dstPath;
  }
};

//===========================================================================
// getLevelByPath(scene, actualPath)
//---------------------------------------------------------------------------

TXshLevel *getLevelByPath(ToonzScene *scene, const TFilePath &actualPath) {
  TLevelSet *levelSet = scene->getLevelSet();
  for (int i = 0; i < levelSet->getLevelCount(); i++) {
    TXshLevel *xl = levelSet->getLevel(i);
    if (!xl) continue;
    TFilePath fp = scene->decodeFilePath(xl->getPath());
    if (fp == actualPath) return xl;
  }
  return 0;
}

//===========================================================================
// getSimpleLevelByPath(scene, actualPath)
//---------------------------------------------------------------------------

TXshSimpleLevel *getSimpleLevelByPath(ToonzScene *scene,
                                      const TFilePath &actualPath) {
  TXshLevel *xl = getLevelByPath(scene, actualPath);
  if (!xl) return 0;
  TXshSimpleLevel *sl = xl->getSimpleLevel();
  return sl;
}

//===========================================================================
// beforeCellsInsert(xsh, row, col, rowCount)
//---------------------------------------------------------------------------

bool beforeCellsInsert(TXsheet *xsh, int row, int &col, int rowCount,
                       int newLevelColumnType) {
  bool shiftColumn   = false;
  int i              = 0;
  TXshColumn *column = xsh->getColumn(col);

  for (i = 0; i < rowCount && (xsh->getCell(row + i, col, false).isEmpty());
       i++) {
  }
  int type = (column && !column->isEmpty()) ? column->getColumnType()
                                            : newLevelColumnType;
  // If some used cells in range or column type mismatch must insert a column.
  if (col < 0 || i < rowCount || newLevelColumnType != type || !column) {
    if (column) col += 1;
    TApp::instance()->getCurrentColumn()->setColumnIndex(col);
    shiftColumn = true;
    xsh->insertColumn(col);
  } else {
    // don't overlap
    xsh->removeCells(row, col, rowCount);
  }
  return shiftColumn;
}

//===========================================================================
// getLevelType(actualLevelPath)
//---------------------------------------------------------------------------

int getLevelType(const TFilePath &actualPath) {
  TFileType::Type type = TFileType::getInfo(actualPath);
  if (type == TFileType::RASTER_IMAGE || type == TFileType::RASTER_LEVEL ||
      type == TFileType::CMAPPED_LEVEL) {
    std::string ext = actualPath.getType();
    if (ext == "tzp" || ext == "tzu" || ext == "tzl" || ext == "tlv")
      return TZP_XSHLEVEL;
    else
      return OVL_XSHLEVEL;
  } else if (type == TFileType::VECTOR_LEVEL) {
    return PLI_XSHLEVEL;
  } else
    return UNKNOWN_XSHLEVEL;
}

//===========================================================================
// removeSameNamedUnusedLevel(scene, actualPath, levelName);
//---------------------------------------------------------------------------

void removeSameNamedUnusedLevel(ToonzScene *scene, const TFilePath actualPath,
                                std::wstring levelName) {
  if (QString::fromStdWString(levelName).isEmpty()) {
    // if the option is set in the preferences,
    // remove the scene numbers("c####_") from the file name
    levelName = scene->getLevelNameWithoutSceneNumber(actualPath.getWideName());
  }

  TLevelSet *levelSet = scene->getLevelSet();
  NameModifier nm(levelName);
  levelName = nm.getNext();
  while (1) {
    TXshLevel *existingLevel = levelSet->getLevel(levelName);
    // if the level name is not used in the cast, nothing to do
    if (!existingLevel) return;
    // try if the existing level is unused in the xsheet and remove from the
    // cast
    else if (LevelCmd::removeLevelFromCast(existingLevel, scene, false)) {
      DVGui::info(QObject::tr("Removed unused level %1 from the scene cast. "
                              "(This behavior can be disabled in Preferences.)")
                      .arg(QString::fromStdWString(levelName)));
      return;
    }
    levelName = nm.getNext();
  }
}

//===========================================================================
// class LoadLevelUndo
//---------------------------------------------------------------------------

class LoadLevelUndo final : public TUndo {
  TXshLevelP m_level;
  TFilePath m_levelSetFolder;
  int m_row, m_col, m_rowCount;
  bool m_columnInserted;
  std::vector<TXshCell> m_cells;
  bool m_isFirstTime;

public:
  LoadLevelUndo()
      : m_level()
      , m_levelSetFolder()
      , m_row(0)
      , m_col(0)
      , m_rowCount(0)
      , m_columnInserted(false)
      , m_isFirstTime(true) {}
  void setLevel(TXshLevel *xl) { m_level = xl; }
  void setLevelSetFolder(const TFilePath &levelSetFolder) {
    m_levelSetFolder = levelSetFolder;
  }
  void setIsFirstTime(bool flag) { m_isFirstTime = flag; }
  void setCells(TXsheet *xsh, int row, int col, int rowCount) {
    m_row      = row;
    m_col      = col;
    m_rowCount = rowCount;
    if (rowCount > 0) {
      m_cells.resize(rowCount);
      xsh->getCells(row, col, rowCount, &m_cells[0]);
    } else
      m_cells.clear();
  }
  void setColumnInserted(bool columnInserted) {
    m_columnInserted = columnInserted;
  }

  void undo() const override {
    TApp *app = TApp::instance();
    /*- 最初にシーンに読み込んだ操作のUndoのとき、Castから除く -*/
    if (m_level && m_isFirstTime)
      app->getCurrentScene()->getScene()->getLevelSet()->removeLevel(
          m_level.getPointer());
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    if (m_columnInserted)
      xsh->removeColumn(m_col);
    else {
      xsh->removeCells(m_row, m_col, m_rowCount);
      xsh->insertCells(m_row, m_col, m_rowCount);  // insert empty cells
      // Remove sound or palette column if it is empty.
      if (xsh->getColumn(m_col) && xsh->getColumn(m_col)->isEmpty()) {
        TXshColumn::ColumnType columnType =
            xsh->getColumn(m_col)->getColumnType();
        if (columnType != TXshColumn::eLevelType) {
          TStageObjectTree *stageObjectTree = xsh->getStageObjectTree();
          int lastIndex = stageObjectTree->getStageObjectCount();
          stageObjectTree->getStageObject(TStageObjectId::ColumnId(lastIndex),
                                          true);
          stageObjectTree->swapColumns(m_col, lastIndex);
          xsh->removeColumn(m_col);
          xsh->insertColumn(m_col);
          stageObjectTree->swapColumns(lastIndex, m_col);
        }
      }
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    if (app->getCurrentFrame()->isEditingLevel() &&
        app->getCurrentLevel()->getLevel() == m_level.getPointer())
      app->getCurrentLevel()->setLevel(0);
    app->getCurrentScene()->notifyCastChange();
  }
  void redo() const override {
    TApp *app = TApp::instance();
    if (m_level)
      app->getCurrentScene()->getScene()->getLevelSet()->insertLevel(
          m_level.getPointer());
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    if (m_columnInserted) xsh->insertColumn(m_col);
    if (m_rowCount > 0) {
      xsh->removeCells(m_row, m_col, m_rowCount);
      xsh->insertCells(m_row, m_col, m_rowCount);
      xsh->setCells(m_row, m_col, m_rowCount, &m_cells[0]);
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
    if (app->getCurrentFrame()->isEditingLevel() &&
        app->getCurrentLevel()->getLevel() == 0)
      app->getCurrentLevel()->setLevel(m_level.getPointer());
    app->getCurrentScene()->notifyCastChange();
  }
  int getSize() const override {
    return sizeof(*this) + sizeof(TXshCell) * m_cells.size() +
           sizeof(TXshLevel);
  }
  QString getHistoryString() override {
    return QObject::tr("Load Level  %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
};

//===========================================================================
// class LoadLevelAndReplaceUndo
//---------------------------------------------------------------------------

class LoadAndReplaceLevelUndo final : public TUndo {
  TXshSimpleLevelP m_level;
  QMap<QPair<int, int>, QPair<TXshSimpleLevelP, TFrameId>> m_oldLevels;
  int m_row0, m_col0, m_row1, m_col1;

public:
  LoadAndReplaceLevelUndo(const TXshSimpleLevelP &level, int row0, int col0,
                          int row1, int col1)
      : m_level(level), m_row0(row0), m_row1(row1), m_col0(col0), m_col1(col1) {
    int c, r;
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    for (c = m_col0; c <= m_col1; c++)
      for (r = m_row0; r <= m_row1; r++) {
        TXshCell cell             = xsh->getCell(r, c, false);
        TXshSimpleLevel *oldLevel = cell.getSimpleLevel();
        TFrameId fid              = cell.getFrameId();
        QPair<int, int> cellId(r, c);
        m_oldLevels[cellId] = QPair<TXshSimpleLevelP, TFrameId>(oldLevel, fid);
      }
  }

  void undo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int c, r;
    for (c = m_col0; c <= m_col1; c++)
      for (r = m_row0; r <= m_row1; r++) {
        QPair<int, int> cellId(r, c);
        TXshSimpleLevel *oldLevel = m_oldLevels[cellId].first.getPointer();
        TFrameId fid              = m_oldLevels[cellId].second;
        TXshCell cell             = xsh->getCell(r, c);
        cell.m_level              = oldLevel;
        cell.m_frameId            = fid;
        xsh->setCell(r, c, cell);
      }
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentScene()->notifyCastChange();
  }

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    int c, r;
    for (c = m_col0; c <= m_col1; c++)
      for (r = m_row0; r <= m_row1; r++) {
        QPair<int, int> cellId(r, c);
        TFrameId fid   = m_oldLevels[cellId].second;
        TXshCell cell  = xsh->getCell(r, c);
        cell.m_level   = m_level.getPointer();
        cell.m_frameId = fid;
        xsh->setCell(r, c, cell);
      }
    app->getCurrentXsheet()->notifyXsheetChanged();
    app->getCurrentScene()->notifyCastChange();
  }

  int getSize() const override { return sizeof(*this); }
  QString getHistoryString() override {
    return QObject::tr("Load and Replace Level  %1")
        .arg(QString::fromStdWString(m_level->getName()));
  }
};

//===========================================================================
// loadPalette(scene, actualPath, row, col)
//---------------------------------------------------------------------------

TXshLevel *loadPalette(ToonzScene *scene, TFilePath actualPath,
                       TFilePath castFolder, int row, int &col) {
  TFilePath palettePath     = actualPath;
  TXsheet *xsh              = scene->getXsheet();
  TXshPaletteColumn *column = new TXshPaletteColumn;
  xsh->insertColumn(col, column);
  TXshLevel *level =
      scene->createNewLevel(PLT_XSHLEVEL, palettePath.getWideName());
  level->getPaletteLevel()->setPath(scene->codeFilePath(palettePath));
  level->getPaletteLevel()->load();
  TXshCell cell;
  cell.m_level   = level;
  cell.m_frameId = TFrameId(1);
  xsh->setCell(row, col, cell);
  xsh->updateFrameCount();
  // Undo
  TUndoManager::manager()->beginBlock();
  LoadLevelUndo *undo = new LoadLevelUndo();
  undo->setLevel(level);
  undo->setLevelSetFolder(castFolder);
  undo->setCells(scene->getXsheet(), row, col, 1);
  undo->setColumnInserted(true);
  TUndoManager::manager()->add(undo);

  // Column name renamed to level name only if was originally empty
  TStageObjectId columnId = TStageObjectId::ColumnId(col);
  std::string columnName =
      QString::fromStdWString(level->getName()).toStdString();
  TStageObjectCmd::rename(columnId, columnName,
                          TApp::instance()->getCurrentXsheet());

  TUndoManager::manager()->endBlock();
  return level;
}

//===========================================================================
// substituteLevel() ; TODO: move to a different file
//---------------------------------------------------------------------------

void substituteLevel(TXsheet *xsh, TXshLevel *srcLevel, TXshLevel *dstLevel) {
  std::set<TXshChildLevel *> substitutedSubs;

  for (int c = 0; c < xsh->getColumnCount(); c++) {
    int r0 = 0, r1 = -1;
    xsh->getCellRange(c, r0, r1);
    if (r0 > r1) continue;
    int rowCount = r1 - r0 + 1;
    std::vector<TXshCell> cells(rowCount);
    xsh->getCells(r0, c, rowCount, &cells[0]);
    bool changed = false;
    for (int i = 0; i < rowCount; i++) {
      if (!cells[i].isEmpty()) {
        if (cells[i].m_level.getPointer() == srcLevel) {
          cells[i].m_level = dstLevel;
          changed          = true;
        } else {
          // Recursive on sub-xsheets
          TXshChildLevel *childLevel = cells[i].m_level->getChildLevel();
          if (childLevel)
            if (substitutedSubs.find(childLevel) == substitutedSubs.end()) {
              substituteLevel(childLevel->getXsheet(), srcLevel, dstLevel);
              substitutedSubs.insert(childLevel);
            }
        }
      }
    }
    if (changed) xsh->setCells(r0, c, rowCount, &cells[0]);
  }
}

//===========================================================================
// class ChildLevelResourceImporter
//---------------------------------------------------------------------------

class ChildLevelResourceImporter final : public ResourceProcessor {
  ToonzScene *m_parentScene;
  ToonzScene *m_childScene;

  ResourceImportStrategy &m_importStrategy;
  TFilePath m_levelSetFolder;

public:
  ChildLevelResourceImporter(ToonzScene *parentScene, ToonzScene *childScene,
                             ResourceImportStrategy &importStrategy);

  void setLevelSetFolder(const TFilePath &levelSetFolder) {
    m_levelSetFolder = levelSetFolder;
  }

  void process(TXshSimpleLevel *sl) override;
  void process(TXshPaletteLevel *sl) override;
  void process(TXshSoundLevel *sl) override;
  bool aborted() const override { return m_importStrategy.aborted(); }
};

//---------------------------------------------------------------------------

ChildLevelResourceImporter::ChildLevelResourceImporter(
    ToonzScene *parentScene, ToonzScene *childScene,
    ResourceImportStrategy &importStrategy)
    : m_parentScene(parentScene)
    , m_childScene(childScene)
    , m_importStrategy(importStrategy)
    , m_levelSetFolder() {}

//---------------------------------------------------------------------------

void ChildLevelResourceImporter::process(TXshSimpleLevel *sl) {
  TLevelSet *parentLevelSet = m_parentScene->getLevelSet();
  if (parentLevelSet->hasLevel(sl->getName())) {
    TXshSimpleLevel *other =
        parentLevelSet->getLevel(sl->getName())->getSimpleLevel();
    if (other)  // in case that the retrieved level from scene cast is
                // subXSheetLevel
    {
      TFilePath otherActualPath =
          m_parentScene->decodeFilePath(other->getPath());
      TFilePath slActualPath = m_childScene->decodeFilePath(sl->getPath());
      if (otherActualPath == slActualPath &&
          other->getProperties()->options() == sl->getProperties()->options()) {
        substituteLevel(m_childScene->getXsheet(), sl, other);
        return;
      }
    }
  }

  TFilePath slPath   = sl->getPath();
  std::string suffix = ResourceImporter::extractPsdSuffix(slPath);

  TFilePath path = m_importStrategy.process(m_parentScene, m_childScene,
                                            slPath);  // actualPath);
  if (suffix != "") path = ResourceImporter::buildPsd(path, suffix);

  sl->setPath(path, false);  // m_parentScene->codeFilePath(actualPath), false);
  NameModifier nm(sl->getName());
  std::wstring levelName;
  for (;;) {
    levelName = nm.getNext();
    if (!parentLevelSet->hasLevel(levelName)) break;
  }
  assert(sl->getRefCount() > 0);
  sl->addRef();
  m_childScene->getLevelSet()->removeLevel(sl);
  sl->setName(levelName);
  bool ret = parentLevelSet->insertLevel(sl);
  assert(ret);
  if (m_levelSetFolder != TFilePath())
    parentLevelSet->moveLevelToFolder(m_levelSetFolder, sl);
  sl->setScene(m_parentScene);
  try {
    sl->load();
  } catch (...) {
  }

  // Check if the scene saved with the previous version AND the premultiply
  // option is set to PNG level setting
  if (m_childScene->getVersionNumber() <
      VersionNumber(71, 1)) {  // V1.4 = 71.0 , V1.5 = 71.1
    if (!path.isEmpty() && path.getType() == "png" &&
        sl->getProperties()->doPremultiply())
      sl->getProperties()->setDoPremultiply(false);
  }

  sl->release();
}

//---------------------------------------------------------------------------

void ChildLevelResourceImporter::process(TXshPaletteLevel *pl) {
  TLevelSet *parentLevelSet = m_parentScene->getLevelSet();

  TFilePath plPath = pl->getPath();

  TFilePath path =
      m_importStrategy.process(m_parentScene, m_childScene, plPath);

  pl->setPath(path);
  NameModifier nm(pl->getName());
  std::wstring levelName;
  for (;;) {
    levelName = nm.getNext();
    if (!parentLevelSet->hasLevel(levelName)) break;
  }
  assert(pl->getRefCount() > 0);
  pl->addRef();
  m_childScene->getLevelSet()->removeLevel(pl);
  pl->setName(levelName);
  bool ret = parentLevelSet->insertLevel(pl);
  assert(ret);
  if (m_levelSetFolder != TFilePath())
    parentLevelSet->moveLevelToFolder(m_levelSetFolder, pl);
  pl->setScene(m_parentScene);
  try {
    pl->load();
  } catch (...) {
  }
  pl->release();
}

void ChildLevelResourceImporter::process(TXshSoundLevel *sl) {
  TLevelSet *parentLevelSet = m_parentScene->getLevelSet();
  TFilePath path =
      m_importStrategy.process(m_parentScene, m_childScene, sl->getPath());
  sl->setPath(path);
  NameModifier nm(sl->getName());
  std::wstring levelName;
  for (;;) {
    levelName = nm.getNext();
    if (!parentLevelSet->hasLevel(levelName)) break;
  }
  assert(sl->getRefCount() > 0);
  sl->addRef();
  m_childScene->getLevelSet()->removeLevel(sl);
  sl->setName(levelName);
  bool ret = parentLevelSet->insertLevel(sl);
  assert(ret);
  if (m_levelSetFolder != TFilePath())
    parentLevelSet->moveLevelToFolder(m_levelSetFolder, sl);
  sl->setScene(m_parentScene);
  try {
    sl->loadSoundTrack();
  } catch (...) {
  }
  sl->release();
}

//===========================================================================
// loadChildLevel(parentScene, actualPath, row, col, importStrategy)
//---------------------------------------------------------------------------

TXshLevel *loadChildLevel(ToonzScene *parentScene, TFilePath actualPath,
                          int row, int &col,
                          ResourceImportDialog &importStrategy) {
  auto project = TProjectManager::instance()->loadSceneProject(actualPath);

  // In Tab mode we don't need the project. Otherwise if it is not available
  // we must exit
  if (!project && !TProjectManager::instance()->isTabModeEnabled()) return 0;

  // load the subxsheet
  ToonzScene scene;
  scene.loadTnzFile(actualPath);
  scene.setProject(project);
  std::wstring subSceneName = actualPath.getWideName();

  // camera settings. get the child camera ...
  TXsheet *childXsh = scene.getXsheet();
  TStageObjectId cameraId =
      childXsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *childCameraObject = childXsh->getStageObject(cameraId);
  TCamera *childCamera            = childCameraObject->getCamera();
  // ...and the parent camera...
  TXsheet *parentXsh = parentScene->getXsheet();
  cameraId           = parentXsh->getStageObjectTree()->getCurrentCameraId();
  TStageObject *cameraObject = parentXsh->getStageObject(cameraId);
  TCamera *camera            = cameraObject->getCamera();

  // if the camera settings are different ask the user
  const double eps = 0.00001;
  if (fabs(camera->getSize().lx - childCamera->getSize().lx) > eps ||
      fabs(camera->getSize().ly - childCamera->getSize().ly) > eps ||
      camera->getRes() != childCamera->getRes()) {
    QString question(QObject::tr(
        "The camera settings of the scene you are loading as sub-xsheet are "
        "different from those of your current scene. What you want to do?"));
    QList<QString> list;
    list.append(QObject::tr("Keep the sub-xsheet original camera settings."));
    list.append(QObject::tr(
        "Apply the current scene camera settings to the sub-xsheet."));
    int ret = DVGui::RadioButtonMsgBox(DVGui::WARNING, question, list);
    if (ret == 0) return 0;
    if (ret == 2) {
      childCamera->setRes(camera->getRes());
      childCamera->setSize(camera->getSize());
    }
  }

  ChildLevelResourceImporter childLevelResourceImporter(parentScene, &scene,
                                                        importStrategy);
  TFilePath dstFolder = importStrategy.getDstFolder();
  if (dstFolder != TFilePath()) {
    TLevelSet *levelSet      = parentScene->getLevelSet();
    TFilePath levelSetFolder = levelSet->createFolder(
        levelSet->getDefaultFolder() + dstFolder.getParentDir(),
        dstFolder.getWideName());
    childLevelResourceImporter.setLevelSetFolder(levelSetFolder);
  }

  SceneResources resources(&scene, childXsh);
  resources.accept(&childLevelResourceImporter);

  scene.setProject(parentScene->getProject());
  std::vector<TXshChildLevel *> childLevels;
  for (int i = 0; i < scene.getLevelSet()->getLevelCount(); i++)
    if (TXshChildLevel *cl = scene.getLevelSet()->getLevel(i)->getChildLevel())
      childLevels.push_back(cl);
  for (int i = 0; i < (int)childLevels.size(); i++) {
    TXshChildLevel *cl = childLevels[i];
    parentScene->getLevelSet()->insertLevel(cl);
    // scene.getLevelSet()->removeLevel(cl);
  }

  TXshChildLevel *childLevel =
      parentScene->createNewLevel(CHILD_XSHLEVEL, subSceneName)
          ->getChildLevel();

  int frameCount = scene.getXsheet()->getFrameCount();
  childXsh->setScene(parentScene);
  if (frameCount <= 0) frameCount = 1;
  int dummy, lastOldRow;
  parentXsh->getCellRange(col, dummy, lastOldRow);

  childLevel->setXsheet(childXsh);
  childXsh->getStageObjectTree()->invalidateAll();

  bool shiftColumn = beforeCellsInsert(parentXsh, row, col, frameCount,
                                       TXshColumn::eLevelType);
  if (!shiftColumn) parentXsh->insertCells(row, col, frameCount);

  for (int i = 0; i < frameCount; i++)
    parentXsh->setCell(row + i, col, TXshCell(childLevel, TFrameId(1 + i)));

  for (int i = 0; i < parentScene->getLevelSet()->getLevelCount(); i++)
    parentScene->getLevelSet()->getLevel(i)->setScene(parentScene);

  // Column name renamed to level name only if was originally empty
  bool wasColumnEmpty = shiftColumn;
  if (!wasColumnEmpty) {
    TXshColumn *column = parentXsh->getColumn(col);
    int r0, r1;
    column->getRange(r0, r1);
    if ((r1 - r0 + 1) == frameCount) wasColumnEmpty = true;
  }

  if (wasColumnEmpty) {
    TStageObjectId columnId = TStageObjectId::ColumnId(col);
    std::string columnName =
        QString::fromStdWString(childLevel->getName()).toStdString();
    TStageObjectCmd::rename(columnId, columnName,
                            TApp::instance()->getCurrentXsheet());
    // For now, let's remove the rename undo since loading child level is not
    // undoable
    TUndoManager::manager()->popUndo();
  }

  // Inform the cache fx command that a scene was loaded
  CacheFxCommand::instance()->onSceneLoaded();

  return childLevel;
}

//===========================================================================
// loadLevel(scene, path, castFolder, row, col)
// (path can be coded path)
// for loading the all types of level other than scene/palette/sound
//---------------------------------------------------------------------------

TXshLevel *loadLevel(ToonzScene *scene,
                     const IoCmd::LoadResourceArguments::ResourceData &rd,
                     const TFilePath &castFolder, int row0, int &col0, int row1,
                     int &col1, bool expose, std::vector<TFrameId> &fIds,
                     TFrameId xFrom = TFrameId(), TFrameId xTo = TFrameId(),
                     std::wstring levelName = L"", int step = -1, int inc = -1,
                     int frameCount = -1, bool doesFileActuallyExist = true) {
  TFilePath actualPath = scene->decodeFilePath(rd.m_path);

  LoadLevelUndo *undo                  = 0;
  LoadAndReplaceLevelUndo *replaceUndo = 0;

  TXsheet *xsh = scene->getXsheet();

  TFileType::Type type = TFileType::getInfo(actualPath);
  // try to find the level with the same path in the Scene Cast. If found, reuse
  // it
  TXshLevel *xl     = getLevelByPath(scene, actualPath);
  bool isFirstTime  = !xl;
  std::wstring name = actualPath.getWideName();

  if (isFirstTime && expose &&
      Preferences::instance()->isAutoRemoveUnusedLevelsEnabled()) {
    removeSameNamedUnusedLevel(scene, actualPath, levelName);
  }

  IoCmd::ConvertingPopup *convertingPopup = new IoCmd::ConvertingPopup(
      TApp::instance()->getMainWindow(),
      QString::fromStdWString(name) +
          QString::fromStdString(actualPath.getDottedType()));

  convertingPopup->hide();  // Should be unnecessary

  if (!xl) {
    try {
      std::string format = actualPath.getType();
      if (format == "tzp" || format == "tzu") convertingPopup->show();

      // SVGs are treated as PLI. Ignore the fIds lists
      if (format != "svg" && fIds.size() != 0 && doesFileActuallyExist)
        xl = scene->loadLevel(actualPath, rd.m_options ? &*rd.m_options : 0,
                              levelName, fIds);
      else
        xl = scene->loadLevel(actualPath, rd.m_options ? &*rd.m_options : 0);
      if (!xl) {
        error("Failed to create level " + toQString(actualPath) +
              " : this filetype is not supported.");
        return 0;
      }

      TXshSimpleLevel *sl = xl->getSimpleLevel();
      if (sl && sl->isReadOnly() &&
          (sl->getType() == PLI_XSHLEVEL || sl->getType() == TZP_XSHLEVEL ||
           sl->getType() == OVL_XSHLEVEL)) {
        TLevelSet *levelSet = new TLevelSet;
        levelSet->insertLevel(sl);
        VersionControlManager::instance()->setFrameRange(levelSet, true);
      }

      if (convertingPopup->isVisible()) convertingPopup->hide();
    } catch (TException &e) {
      if (convertingPopup->isVisible()) convertingPopup->hide();

      QString msg = QString::fromStdWString(e.getMessage());
      if (msg == QString("Old 4.1 Palette"))
        error("It is not possible to load the level " + toQString(actualPath) +
              " because its version is not supported.");
      else
        error(QString::fromStdWString(e.getMessage()));

      return 0;
    } catch (...) {
      if (convertingPopup->isVisible()) convertingPopup->hide();
      error("Unhandled exception encountered");
      return 0;
    }

    if (xl->getSimpleLevel() &&
        xl->getSimpleLevel()->getProperties()->isForbidden()) {
      error("It is not possible to load the level " + toQString(actualPath) +
            " because its version is not supported.");
      scene->getLevelSet()->removeLevel(xl);
      return 0;
    }

    if (castFolder != TFilePath())
      scene->getLevelSet()->moveLevelToFolder(castFolder, xl);

    History::instance()->addItem(actualPath);

    if (row1 == -1 && col1 == -1) {
      undo = new LoadLevelUndo();
      undo->setLevel(xl);
      undo->setLevelSetFolder(castFolder);
      undo->setIsFirstTime(isFirstTime);
    }
  }
  // if the level can be obtained (from scene cast or file)
  bool wasColumnEmpty = false;
  if (xl) {
    // placing in the xsheet
    if (expose) {
      // lo importo nell'xsheet
      if (!undo) {
        undo = new LoadLevelUndo();
        undo->setLevel(xl);
        undo->setIsFirstTime(isFirstTime);
      }

      TUndoManager::manager()->beginBlock();

      int levelType = xl->getType();
      TXshColumn::ColumnType newLevelColumnType =
          TXshColumn::toColumnType(levelType);
      bool columnInserted =
          beforeCellsInsert(scene->getXsheet(), row0, col0, xl->getFrameCount(),
                            newLevelColumnType);

      scene->getXsheet()->exposeLevel(row0, col0, xl, fIds, xFrom, xTo, step,
                                      inc, frameCount, doesFileActuallyExist);

      if (frameCount > 0)
        undo->setCells(scene->getXsheet(), row0, col0, frameCount);
      else
        undo->setCells(scene->getXsheet(), row0, col0, xl->getFrameCount());
      undo->setColumnInserted(columnInserted);

      wasColumnEmpty = columnInserted;
      if (!wasColumnEmpty) {
        TXshColumn *column = xsh->getColumn(col0);
        int r0, r1;
        column->getRange(r0, r1);
        if ((r1 - r0 + 1) == xl->getFrameCount()) wasColumnEmpty = true;
      }
    }
    if (row1 != -1 || col1 != -1)
      replaceUndo = new LoadAndReplaceLevelUndo(xl->getSimpleLevel(), row0,
                                                col0, row1, col1);
  }

  if (undo) TUndoManager::manager()->add(undo);

  if (replaceUndo) TUndoManager::manager()->add(replaceUndo);

  // Column name renamed to level name only if was originally empty
  if (expose) {
    if (wasColumnEmpty) {
      TStageObjectId columnId = TStageObjectId::ColumnId(col0);
      std::string columnName =
          QString::fromStdWString(xl->getName()).toStdString();
      TStageObjectCmd::rename(columnId, columnName,
                              TApp::instance()->getCurrentXsheet());
    }
    TUndoManager::manager()->endBlock();
  }

  return xl;
}

//===========================================================================
// loadResource(scene, path, castFolder, row, col, expose)
//---------------------------------------------------------------------------

TXshLevel *loadResource(ToonzScene *scene,
                        const IoCmd::LoadResourceArguments::ResourceData &rd,
                        const TFilePath &castFolder, int row0, int &col0,
                        int row1, int &col1, bool expose,
                        std::vector<TFrameId> fIds = std::vector<TFrameId>(),
                        TFrameId xFrom = TFrameId(), TFrameId xTo = TFrameId(),
                        std::wstring levelName = L"", int step = -1,
                        int inc = -1, int frameCount = -1,
                        bool doesFileActuallyExist = true) {
  IoCmd::LoadResourceArguments::ResourceData actualRd(rd);
  actualRd.m_path = scene->decodeFilePath(rd.m_path);

  TFileType::Type type = TFileType::getInfo(actualRd.m_path);
  return (type == TFileType::PALETTE_LEVEL)
             ? loadPalette(scene, actualRd.m_path, castFolder, row0, col0)
             : loadLevel(scene, actualRd, castFolder, row0, col0, row1, col1,
                         expose, fIds, xFrom, xTo, levelName, step, inc,
                         frameCount, doesFileActuallyExist);
}

//===========================================================================

enum ExposeType { eOverWrite, eShiftCells, eNone };

//===========================================================================
// class ExposeLevelUndo
//---------------------------------------------------------------------------

class ExposeLevelUndo final : public TUndo {
  TXshSimpleLevelP m_sl;
  std::vector<TXshCell> m_oldCells;
  std::vector<TFrameId> m_fids;
  int m_row;
  int m_col;
  int m_frameCount;
  bool m_insertEmptyColumn;
  ExposeType m_type;

public:
  ExposeLevelUndo(TXshSimpleLevel *sl, int row, int col, int frameCount,
                  bool insertEmptyColumn, ExposeType type = eNone)
      : m_sl(sl)
      , m_row(row)
      , m_col(col)
      , m_frameCount(frameCount)
      , m_insertEmptyColumn(insertEmptyColumn)
      , m_fids()
      , m_type(type) {
    if (type == eOverWrite) {
      TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
      int r;
      for (r = row; r < frameCount + row; r++)
        m_oldCells.push_back(xsh->getCell(r, col));
    }
  }

  void setFids(const std::vector<TFrameId> &fids) { m_fids = fids; }

  void undo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    if (m_insertEmptyColumn)
      xsh->removeColumn(m_col);
    else {
      xsh->removeCells(m_row, m_col, m_frameCount);
      if (m_type == eNone) xsh->insertCells(m_row, m_col, m_frameCount);
      if (m_type == eOverWrite) {
        xsh->insertCells(m_row, m_col, m_frameCount);
        if (!m_oldCells.empty())
          xsh->setCells(m_row, m_col, m_frameCount, &m_oldCells[0]);
      }
      app->getCurrentXsheet()->notifyXsheetChanged();
    }
  }

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    if (m_insertEmptyColumn) xsh->insertColumn(m_col);
    int frameCount = 0;
    if (!m_fids.empty()) {
      if (m_type == eShiftCells) xsh->insertCells(m_row, m_col, m_frameCount);
      frameCount = (int)m_fids.size();
      std::vector<TFrameId>::const_iterator it;
      int row = m_row;
      for (it = m_fids.begin(); it != m_fids.end(); ++it) {
        xsh->setCell(row, m_col, TXshCell(m_sl.getPointer(), *it));
        ++row;
      }
    } else {
      xsh->removeCells(m_row, m_col, m_frameCount);
      frameCount = xsh->exposeLevel(m_row, m_col, m_sl.getPointer());
    }
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }

  QString getHistoryString() override {
    return QObject::tr("Expose Level  %1")
        .arg(QString::fromStdWString(m_sl->getName()));
  }
};

//===========================================================================
// class ExposeCommentUndo
//---------------------------------------------------------------------------

class ExposeCommentUndo final : public TUndo {
  TXshSoundTextColumnP m_soundtextColumn;
  int m_col;
  bool m_insertEmptyColumn;
  QString m_columnName;

public:
  ExposeCommentUndo(TXshSoundTextColumn *soundtextColumn, int col,
                    bool insertEmptyColumn, QString columnName)
      : m_soundtextColumn(soundtextColumn)
      , m_col(col)
      , m_insertEmptyColumn(insertEmptyColumn)
      , m_columnName(columnName) {}

  void undo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    xsh->removeColumn(m_col);
    if (!m_insertEmptyColumn) xsh->insertColumn(m_col);
    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  void redo() const override {
    TApp *app    = TApp::instance();
    TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
    if (m_insertEmptyColumn) xsh->insertColumn(m_col);
    xsh->insertColumn(m_col, m_soundtextColumn.getPointer());

    // Setto il nome di dafult della colonna al nome del file magpie.
    TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(m_col));
    std::string str   = m_columnName.toStdString();
    obj->setName(str);

    app->getCurrentXsheet()->notifyXsheetChanged();
  }

  int getSize() const override { return sizeof *this; }
};

//=============================================================================

// This class is intended to serve as inhibitor for Previewer and SwatchViewer's
// activity
class RenderingSuspender {
public:
  RenderingSuspender() {
    Previewer::suspendRendering(true);
    PreviewFxManager::suspendRendering(true);
    SwatchViewer::suspendRendering(true);
  }

  ~RenderingSuspender() {
    Previewer::suspendRendering(false);
    PreviewFxManager::suspendRendering(false);
    SwatchViewer::suspendRendering(false);
  }
};

//=============================================================================

inline TPaletteP dirtyWhite(const TPaletteP &plt) {
  TPaletteP out = TPaletteP(plt->clone());
  for (int i = 0; i < out->getStyleCount(); i++) {
    if (out->getStyle(i)->getMainColor() == TPixel::White) {
      out->getStyle(i)->setMainColor(TPixel(254, 254, 254, 255));
    }
  }
  return out;
}

}  // namespace
//---------------------------------------------------------------------------

// Per ora e' usato solo per i formato "tzp" e "tzu".
IoCmd::ConvertingPopup::ConvertingPopup(QWidget *parent, QString fileName)
    : Dialog(parent) {
  setModal(true);
  setWindowFlags(Qt::Dialog | Qt::WindowTitleHint);
  setMinimumSize(70, 50);
  QVBoxLayout *mainLayout = new QVBoxLayout;
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setSpacing(0);

  QLabel *label = new QLabel(QString(
      QObject::tr("Converting %1 images to tlv format...").arg(fileName)));
  mainLayout->addWidget(label);

  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->addLayout(mainLayout);
}

IoCmd::ConvertingPopup::~ConvertingPopup() {}

//===========================================================================
// IoCmd::saveSceneIfNeeded(message)
//---------------------------------------------------------------------------

bool IoCmd::saveSceneIfNeeded(QString msg) {
  TApp *app = TApp::instance();

  if (app->getCurrentScene()->getDirtyFlag()) {
    QString question;
    question = QObject::tr(
                   "%1: the current scene has been modified.\n"
                   "What would you like to do?")
                   .arg(msg);
    int ret = DVGui::MsgBox(
        question, QObject::tr("Save All"), QObject::tr("Save Scene Only"),
        QObject::tr("Discard Changes"), QObject::tr("Cancel"), 0);
    if (ret == 0 || ret == 4) {
      // cancel (or closed message box window)
      return false;
    } else if (ret == 1) {
      // save all
      if (!IoCmd::saveAll()) return false;
    } else if (ret == 2) {
      // save
      if (!IoCmd::saveScene()) return false;
    } else if (ret == 3) {
    }
  }

  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (scene) {
    QStringList dirtyResources;
    {
      SceneResources resources(scene, 0);
      resources.getDirtyResources(dirtyResources);
    }

    if (!dirtyResources.empty()) {
      // show up to 5 items
      int extraCount = dirtyResources.count() - 5;
      if (extraCount > 0) {
        dirtyResources = dirtyResources.mid(0, 5);
        dirtyResources.append(
            QObject::tr("and %1 more item(s).").arg(extraCount));
      }

      QString question;

      question = msg + ":" +
                 QObject::tr(" The following file(s) have been modified.\n\n");

      question += "  " + dirtyResources.join("\n  ");

      question += "\n" + QObject::tr("\nWhat would you like to do? ");

      int ret =
          DVGui::MsgBox(question, QObject::tr("Save Changes"),
                        msg + QObject::tr(" Anyway"), QObject::tr("Cancel"), 0);
      if (ret == 0 || ret == 3) {
        // cancel (or closed message box window)
        return false;
      } else if (ret == 1) {
        // save non scene files
        IoCmd::saveNonSceneFiles();
      } else if (ret == 2) {
        // do nothing and continue
      }
    }

    RenderingSuspender suspender;

    TFilePath scenePath(scene->getScenePath());
    scene->clear();  // note: this (possibly) removes the "untitled" folder
    app->getCurrentScene()->notifyCastChange();
    // Si deve notificare anche il cambiamento che ha subito l'xsheet.
    app->getCurrentXsheet()->notifyXsheetSwitched();
    FileBrowser::refreshFolder(scenePath.getParentDir());
  }
  app->getCurrentScene()->setDirtyFlag(false);
  return true;
}

//===========================================================================
// IoCmd::newScene()
//---------------------------------------------------------------------------

void IoCmd::newScene() {
  RenderingSuspender suspender;
  TApp *app        = TApp::instance();
  double cameraDpi = 120;  // used to be 64 and 53.33333

  if (!saveSceneIfNeeded(QApplication::tr("New Scene"))) return;

  // temporary clear the current level to prevent UI to access deleted level
  // while switching scenes
  app->getCurrentLevel()->setLevel(nullptr);

  IconGenerator::instance()->clearRequests();
  IconGenerator::instance()->clearSceneIcons();
  ImageManager::instance()->clear();
  FullColorPalette::instance()->clear();

  CacheFxCommand::instance()->onNewScene();
  CacheFxCommand::instance()->onSceneLoaded();

#ifdef USE_SQLITE_HDPOOL
  // INTERMEDIATE CACHE RESULTS MANAGEMENT: Clear all resources not accessed in
  // the last 2 sessions
  TCacheResourcePool::instance()->clearAccessedAfterSessions(
      1);  // 0 would be after this session
#endif

  ToonzScene *scene = new ToonzScene();
  TImageStyle::setCurrentScene(scene);

  TCamera *camera = scene->getCurrentCamera();
  TDimension res(1920, 1080);
  // TDimension res(768, 576);
  camera->setRes(res);
  camera->setSize(
      TDimensionD((double)res.lx / cameraDpi, (double)res.ly / cameraDpi));
  scene->getProperties()->setBgColor(TPixel32::White);
  TProjectManager::instance()->initializeScene(scene);
  if (Preferences::instance()->getPixelsOnly()) {
    TCamera *updateCamera = scene->getCurrentCamera();
    TDimension updateRes  = updateCamera->getRes();
    updateCamera->setSize(TDimensionD((double)updateRes.lx / cameraDpi,
                                      (double)updateRes.ly / cameraDpi));
  }
  // Must set current scene after initializeScene!!
  app->getCurrentScene()->setScene(scene);
  // initializeScene() load project cleanup palette: set it to cleanup palette
  // handle.
  TPalette *palette = scene->getProperties()
                          ->getCleanupParameters()
                          ->m_cleanupPalette.getPointer();
  PaletteController *paletteController = app->getPaletteController();
  paletteController->getCurrentCleanupPalette()->setPalette(palette, -1);
  paletteController->editLevelPalette();

  TFilePath scenePath = scene->getScenePath();
  DvDirModel::instance()->refreshFolder(scenePath.getParentDir());

  Previewer::clearAll();
  PreviewFxManager::instance()->reset();

  app->getCurrentScene()->notifyNameSceneChange();
  IconGenerator::instance()->invalidateSceneIcon();
  ToolHandle *toolH = TApp::instance()->getCurrentTool();
  if (toolH && toolH->getTool()) toolH->getTool()->reset();

  CommandManager::instance()->execute("T_Hand");

  CommandManager::instance()->enable(MI_SaveSubxsheetAs, false);

  app->getCurrentScene()->notifySceneChanged();
  app->getCurrentScene()->setDirtyFlag(false);
  app->getCurrentObject()->setIsSpline(false);
  app->getCurrentColumn()->setColumnIndex(0);

  //CleanupParameters *cp = scene->getProperties()->getCleanupParameters();
  //CleanupParameters::GlobalParameters.assign(cp);
  //CleanupSettingsModel::onSceneSwitched()

  // updateCleanupSettingsPopup();

  CleanupPreviewCheck::instance()->setIsEnabled(false);
  CameraTestCheck::instance()->setIsEnabled(false);

  if (!TApp::instance()->isApplicationStarting())
    QApplication::clipboard()->clear();
  TSelection::setCurrent(0);
  TUndoManager::manager()->reset();

  bool exist = TSystem::doesExistFileOrLevel(
      scene->decodeFilePath(scene->getScenePath()));
  QAction *act = CommandManager::instance()->getAction(MI_RevertScene);
  if (act) act->setEnabled(exist);
}

//===========================================================================
// IoCmd::saveScene(scenePath, flags)
//---------------------------------------------------------------------------

bool IoCmd::saveScene(const TFilePath &path, int flags) {
  bool overwrite     = (flags & SILENTLY_OVERWRITE) != 0;
  bool saveSubxsheet = (flags & SAVE_SUBXSHEET) != 0;
  bool isAutosave    = (flags & AUTO_SAVE) != 0;
  TApp *app          = TApp::instance();

  assert(!path.isEmpty());
  TFilePath scenePath                      = path;
  if (scenePath.getType() == "") scenePath = scenePath.withType("tnz");
  if (scenePath.getType() != "tnz") {
    error(
        QObject::tr("%1 has an invalid file extension.").arg(toQString(path)));
    return false;
  }
  TFileStatus dirStatus(scenePath.getParentDir());
  if (!(dirStatus.doesExist() && dirStatus.isWritable())) {
    error(QObject::tr("%1 is an invalid path.")
              .arg(toQString(scenePath.getParentDir())));
    return false;
  }

  // notify user if the scene will be saved including any "broken" expression
  // reference
  if (!ExpressionReferenceManager::instance()->askIfParamIsIgnoredOnSave(
          saveSubxsheet))
    return false;

  if (!overwrite && TFileStatus(scenePath).doesExist()) {
    QString question;
    question = QObject::tr(
                   "The scene %1 already exists.\nDo you want to overwrite it?")
                   .arg(QString::fromStdString(scenePath.getName()));

    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return false;
  }

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();

  TXsheet *xsheet           = 0;
  if (saveSubxsheet) xsheet = TApp::instance()->getCurrentXsheet()->getXsheet();

  // Automatically remove unused levels
  if (!saveSubxsheet && !isAutosave &&
      Preferences::instance()->isAutoRemoveUnusedLevelsEnabled()) {
    if (LevelCmd::removeUnusedLevelsFromCast(false))
      DVGui::info(
          QObject::tr("Removed unused levels from the scene cast. (This "
                      "behavior can be disabled in Preferences.)"));
  }

  // If the scene will be saved in the different folder, check out the scene
  // cast.
  // if the cast contains the level specified with $scenefolder alias,
  // open a warning popup notifying that such level will lose link.

  // in case of saving subxsheet, the current scene will not be switched to the
  // saved one
  // so the level paths are needed to be reverted after saving
  QHash<TXshLevel *, TFilePath> orgLevelPaths;
  auto revertOrgLevelPaths = [&] {
    QHash<TXshLevel *, TFilePath>::const_iterator i =
        orgLevelPaths.constBegin();
    while (i != orgLevelPaths.constEnd()) {
      if (TXshSimpleLevel *sil = i.key()->getSimpleLevel())
        sil->setPath(i.value(), true);
      else if (TXshPaletteLevel *pal = i.key()->getPaletteLevel())
        pal->setPath(i.value());
      else if (TXshSoundLevel *sol = i.key()->getSoundLevel())
        sol->setPath(i.value());
      ++i;
    }
  };

  bool useSceneSubfolders =
      TProjectManager::instance()->getCurrentProject()->getUseSubScenePath();
  if (!overwrite) {
    bool ret = takeCareSceneFolderItemsOnSaveSceneAs(
        scene, scenePath, xsheet, orgLevelPaths, useSceneSubfolders);
    if (!ret) {
      revertOrgLevelPaths();
      return false;
    }
  }

  TFilePath oldFullPath = scene->decodeFilePath(scene->getScenePath());
  TFilePath newFullPath = scene->decodeFilePath(scenePath);

  QApplication::setOverrideCursor(Qt::WaitCursor);
  if (app->getCurrentScene()->getDirtyFlag())
    scene->getContentHistory(true)->modifiedNow();

  if (oldFullPath != newFullPath) {
    IconGenerator::instance()->clearRequests();
    IconGenerator::instance()->clearSceneIcons();

#ifdef USE_SQLITE_HDPOOL
    // Open the new cache resources HD pool
    TCacheResourcePool::instance()->setPath(
        QString::fromStdWString(
            ToonzFolder::getCacheRootFolder().getWideString()),
        QString::fromStdWString(scene->getProject()->getName().getWideName()),
        QString::fromStdWString(scene->getSceneName()));
#endif
  }

  // Must wait for current save to finish, just in case
  while (TApp::instance()->isSaveInProgress())
    ;

  TApp::instance()->setSaveInProgress(true);
  try {
    scene->save(scenePath, xsheet);
  } catch (const TSystemException &se) {
    DVGui::warning(QString::fromStdWString(se.getMessage()));
  } catch (...) {
    DVGui::error(QObject::tr("Couldn't save %1").arg(toQString(scenePath)));
  }
  TApp::instance()->setSaveInProgress(false);

  // in case of saving subxsheet, revert the level paths after saving
  revertOrgLevelPaths();

  if (!overwrite && !saveSubxsheet)
    app->getCurrentScene()->notifyNameSceneChange();
  FileBrowser::refreshFolder(scenePath.getParentDir());
  IconGenerator::instance()->invalidate(scenePath);

  // Le seguenti notifiche non cambiano il dirty flag del livello o della
  // paletta
  // ma sono necessarie per aggiornare le titlebar dei pannelli.
  app->getCurrentLevel()->notifyLevelTitleChange();
  app->getCurrentPalette()->notifyPaletteTitleChanged();

  app->getCurrentScene()->setDirtyFlag(false);

  app->getCurrentXsheet()->notifyXsheetSoundChanged();

  History::instance()->addItem(scenePath);
  RecentFiles::instance()->addFilePath(
      toQString(scenePath), RecentFiles::Scene,
      QString::fromStdString(app->getCurrentScene()
                                 ->getScene()
                                 ->getProject()
                                 ->getName()
                                 .getName()));

  QApplication::restoreOverrideCursor();

  bool exist = TSystem::doesExistFileOrLevel(
      scene->decodeFilePath(scene->getScenePath()));
  QAction *act = CommandManager::instance()->getAction(MI_RevertScene);
  if (act) act->setEnabled(exist);

  return true;
}

//===========================================================================
// IoCmd::saveScene()
//---------------------------------------------------------------------------

bool IoCmd::saveScene(int flags) {
  TSelection *oldSelection =
      TApp::instance()->getCurrentSelection()->getSelection();
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene->isUntitled()) {
    static SaveSceneAsPopup *popup = 0;
    if (!popup) popup              = new SaveSceneAsPopup();
    int ret                        = popup->exec();
    if (ret == QDialog::Accepted) {
      TApp::instance()->getCurrentScene()->setDirtyFlag(false);
      return true;
    } else {
      TApp::instance()->getCurrentSelection()->setSelection(oldSelection);
      return false;
    }
  } else {
    TFilePath fp = scene->getScenePath();
    // salva la scena con il nome fp. se fp esiste gia' lo sovrascrive
    // NOTE: saveScene already check saveInProgress
    return saveScene(fp, SILENTLY_OVERWRITE | flags);
  }
}

//===========================================================================
// IoCmd::saveLevel(levelPath)
//---------------------------------------------------------------------------

bool IoCmd::saveLevel(const TFilePath &path) {
  assert(!path.isEmpty());

  TApp *app = TApp::instance();
  TXshSimpleLevel *sl =
      dynamic_cast<TXshSimpleLevel *>(app->getCurrentLevel()->getLevel());
  if (!sl) return false;
  std::string ext    = sl->getPath().getType();
  std::string dotts  = sl->getPath().getDots();
  TFilePath realPath = path;
  if (realPath.getType() == "")
    realPath = TFilePath(realPath.getWideString() + ::to_wstring(dotts + ext));

  bool ret = saveLevel(realPath, sl, false);
  if (!ret) {  // save level failed
    return false;
  }

  RecentFiles::instance()->addFilePath(toQString(realPath), RecentFiles::Level);

  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->notifyPaletteSwitched();
  return true;
}

//===========================================================================
// IoCmd::saveLevel()
//---------------------------------------------------------------------------

bool IoCmd::saveLevel() {
  TApp *app = TApp::instance();
  TXshSimpleLevel *sl =
      dynamic_cast<TXshSimpleLevel *>(app->getCurrentLevel()->getLevel());
  if (!sl) return false;
  if (sl->getPath() == TFilePath()) return false;
  TFilePath path =
      app->getCurrentScene()->getScene()->decodeFilePath(sl->getPath());
  if (path == TFilePath()) return false;
  if (path.getWideString()[0] == L'+') return false;
  if (!path.isAbsolute()) return false;

  if (!saveLevel(path, sl, true)) return false;

  sl->setDirtyFlag(false);
  // for update title bar
  app->getCurrentLevel()->notifyLevelChange();
  return true;
}

//===========================================================================
// IoCmd::saveLevel(levelPath, simpleLevel, overwrite)
//---------------------------------------------------------------------------

bool IoCmd::saveLevel(const TFilePath &fp, TXshSimpleLevel *sl,
                      bool overwrite) {
  assert(sl);
  bool fileDoesExist = TSystem::doesExistFileOrLevel(fp);
  if (!overwrite && fileDoesExist) {
    QString question;
    question = QObject::tr(
                   "The level %1 already exists.\nDo you want to overwrite it?")
                   .arg(toQString(fp));
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return false;
  }

  bool overwritePalette = false;
  // Confirmation of Overwrite palette
  // open dialog IF  1) the level is dirty, and 2) confirmation of overwrite
  // palette haven't been asked
  // for PLI file, do nothing
  if (sl->getPalette() && sl->getPalette()->getAskOverwriteFlag() &&
      sl->getPath().getType() != "pli") {
    /*-- ファイルが存在しない場合はパレットも必ず保存する --*/
    if (!fileDoesExist)
      overwritePalette = true;
    else {
      QString question;
      question =
          "Palette " +
          QString::fromStdWString(sl->getPalette()->getPaletteName()) +
          ".tpl has been modified. Do you want to overwrite palette as well ?";
      int ret =
          DVGui::MsgBox(question, QObject::tr("Overwrite Palette") /*ret = 1*/,
                        QObject::tr("Don't Overwrite Palette") /*ret = 2*/, 0);
      if (ret == 1) overwritePalette = true;
    }
  }

  QApplication::setOverrideCursor(Qt::WaitCursor);
  try {
    sl->save(fp, TFilePath(), overwritePalette);
  } catch (TSystemException se) {
    QApplication::restoreOverrideCursor();
    DVGui::warning(QString::fromStdWString(se.getMessage()));
    return false;
  } catch (...) {
    QApplication::restoreOverrideCursor();
    DVGui::error(QObject::tr("Couldn't save %1").arg(toQString(fp)));
    return false;
  }
  IconGenerator::instance()->invalidate(fp);
  FileBrowser::refreshFolder(fp.getParentDir());
  History::instance()->addItem(fp);

  if (sl->getPalette()) {
    if (overwritePalette || sl->getPath().getType() == "pli")
      sl->getPalette()->setDirtyFlag(false);
    else  // ask only once for save palette
      sl->getPalette()->setAskOverwriteFlag(false);
  }

  RecentFiles::instance()->addFilePath(toQString(fp), RecentFiles::Level);
  QApplication::restoreOverrideCursor();
  TApp::instance()->getCurrentLevel()->notifyLevelTitleChange();
  TApp::instance()
      ->getPaletteController()
      ->getCurrentLevelPalette()
      ->notifyPaletteTitleChanged();
  return true;
}

//===========================================================================
// IoCmd::saveLevel(simpleLevel)
//---------------------------------------------------------------------------

bool IoCmd::saveLevel(TXshSimpleLevel *sl) {
  return saveLevel(sl->getPath(), sl, true);
}

//===========================================================================
// IoCmd::saveAll()
//---------------------------------------------------------------------------

bool IoCmd::saveAll(int flags) {
  // try to save as much as possible
  // if anything is wrong, return false
  // NOTE: saveScene already check saveInProgress
  bool result = saveScene(flags);

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  bool untitled     = scene->isUntitled();
  SceneResources resources(scene, 0);
  // Must wait for current save to finish, just in case
  while (TApp::instance()->isSaveInProgress())
    ;

  TApp::instance()->setSaveInProgress(true);
  result = result && resources.save(scene->getScenePath());
  TApp::instance()->setSaveInProgress(false);
  resources.updatePaths();

  // for update title bar
  app->getCurrentLevel()->notifyLevelTitleChange();
  app->getCurrentPalette()->notifyPaletteTitleChanged();
  if (untitled) scene->setUntitled();
  if (!result && !untitled) {
    DVGui::warning(
        QObject::tr("An error occured while saving. \n"
                    "Please check your work and try again."));
  }
  if (result) TApp::instance()->showMessage("Saved");
  return result;
}

//===========================================================================
// IoCmd::saveNonSceneFiles()
//---------------------------------------------------------------------------

void IoCmd::saveNonSceneFiles() {
  // try to save non scene files

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  bool untitled     = scene->isUntitled();
  SceneResources resources(scene, 0);
  // Must wait for current save to finish, just in case
  while (TApp::instance()->isSaveInProgress())
    ;

  TApp::instance()->setSaveInProgress(true);
  resources.save(scene->getScenePath());
  TApp::instance()->setSaveInProgress(false);
  if (untitled) scene->setUntitled();
  resources.updatePaths();

  // for update title bar
  app->getCurrentLevel()->notifyLevelTitleChange();
  app->getCurrentPalette()->notifyPaletteTitleChanged();
}

//===========================================================================
// IoCmd::saveSound(soundPath, soundColumn, overwrite)
//---------------------------------------------------------------------------

bool IoCmd::saveSound(const TFilePath &fp, TXshSoundLevel *sl, bool overwrite) {
  if (!overwrite && TSystem::doesExistFileOrLevel(fp)) {
    QString question;
    question =
        QObject::tr(
            "The soundtrack %1 already exists.\nDo you want to overwrite it?")
            .arg(toQString(fp));
    int ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                            QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return false;
  }
  try {
    sl->save(fp);
  } catch (...) {
    DVGui::error(QObject::tr("Couldn't save %1").arg(toQString(fp)));
    return false;
  }
  QApplication::setOverrideCursor(Qt::WaitCursor);
  FileBrowser::refreshFolder(fp.getParentDir());
  History::instance()->addItem(fp);
  QApplication::restoreOverrideCursor();
  return true;
}

//===========================================================================
// IoCmd::saveSound(soundColumn)
//---------------------------------------------------------------------------

bool IoCmd::saveSound(TXshSoundLevel *sl) {
  return saveSound(sl->getPath(), sl, true);
}

//=========================================================================
// IoCmd::loadScene(scene, scenePath, import)
//---------------------------------------------------------------------------

bool IoCmd::loadScene(ToonzScene &scene, const TFilePath &scenePath,
                      bool import) {
  if (!TSystem::doesExistFileOrLevel(scenePath)) return false;
  scene.load(scenePath);
  // import if needed
  auto currentProject = TProjectManager::instance()->getCurrentProject();
  if (!scene.getProject()->isLoaded()) return false;
  if (scene.getProject()->getProjectPath() !=
      currentProject->getProjectPath()) {
    ResourceImportDialog resourceLoader;
    // resourceLoader.setImportEnabled(true);
    ResourceImporter importer(&scene, currentProject, resourceLoader);
    SceneResources resources(&scene, scene.getXsheet());
    resources.accept(&importer);
    scene.setScenePath(importer.getImportedScenePath());
    scene.setProject(currentProject);
  }
  return true;
}

//===========================================================================
// IoCmd::loadScene(scenePath)
//---------------------------------------------------------------------------

bool IoCmd::loadScene(const TFilePath &path, bool updateRecentFile,
                      bool checkSaveOldScene) {
  RenderingSuspender suspender;

  if (checkSaveOldScene)
    if (!saveSceneIfNeeded(QApplication::tr("Load Scene"))) return false;
  assert(!path.isEmpty());
  TFilePath scenePath                      = path;
  bool importScene                         = false;
  bool isXdts                              = scenePath.getType() == "xdts";
  if (scenePath.getType() == "") scenePath = scenePath.withType("tnz");
  if (scenePath.getType() != "tnz" && !isXdts) {
    QString msg;
    msg = QObject::tr("File %1 doesn't look like a TOONZ Scene")
              .arg(QString::fromStdWString(scenePath.getWideString()));
    DVGui::error(msg);
    return false;
  }
  if (!TSystem::doesExistFileOrLevel(scenePath)) return false;

  TFilePath scenePathTemp(scenePath.getWideString() +
                          QString(".tmp").toStdWString());
  if (TSystem::doesExistFileOrLevel(scenePathTemp)) {
    QString question =
        QObject::tr(
            "A prior save of Scene '%1' was critically interrupted. \n\
\nA partial save file was generated and changes may be manually salvaged from '%2'.\n\
\nDo you wish to continue loading the last good save or stop and try to salvage the prior save?")
            .arg(QString::fromStdWString(scenePath.getWideString()))
            .arg(QString::fromStdWString(scenePathTemp.getWideString()));
    QString continueAnswer = QObject::tr("Continue");
    QString cancelAnswer   = QObject::tr("Cancel");
    int ret = DVGui::MsgBox(question, continueAnswer, cancelAnswer, 0);
    if (ret == 2)
      return false;
    else
      TSystem::removeFileOrLevel(scenePathTemp);
  }

  TProjectManager *pm = TProjectManager::instance();
  auto sceneProject = pm->loadSceneProject(scenePath);
  if (!sceneProject) {
    QString msg;
    msg = QObject::tr(
              "It is not possible to load the scene %1 because it does not "
              "belong to any project.")
              .arg(QString::fromStdWString(scenePath.getWideString()));
    DVGui::warning(msg);
  }
  if (sceneProject && !sceneProject->isCurrent()) {
    pm->setCurrentProjectPath(sceneProject->getProjectPath());
    // Clear existing raster palette so it forces a reloads of the new project's
    // raster palette when loading scene
    FullColorPalette::instance()->clear();
    //    QString currentProjectName = QString::fromStdWString(
    //        pm->getCurrentProject()->getName().getWideString());
    //    QString sceneProjectName =
    //        QString::fromStdWString(sceneProject->getName().getWideString());
    //
    //    /*QString question = "The Scene '"
    //            + QString::fromStdWString(scenePath.getWideString())
    //+ "' belongs to project '" + sceneProjectName + "'.\n"
    //+ "What do you want to do?";*/
    //    QString question =
    //        QObject::tr(
    //            "The Scene '%1' belongs to project '%2'.\nWhat do you want to
    //            do?")
    //            .arg(QString::fromStdWString(scenePath.getWideString()))
    //            .arg(sceneProjectName);
    //    QString importAnswer        = QObject::tr("Import Scene");
    //    QString switchProjectAnswer = QObject::tr("Change Project");
    //    QString cancelAnswer        = QObject::tr("Cancel");
    //    int ret = DVGui::MsgBox(question, importAnswer, switchProjectAnswer,
    //                            cancelAnswer, 0);
    //    if (ret == 3 || ret == 0) {
    //      newScene();
    //      return false;
    //    }
    //    if (ret == 2)
    //      pm->setCurrentProjectPath(sceneProject->getProjectPath());
    //    else
    //      importScene = true;
  }
  QApplication::setOverrideCursor(Qt::WaitCursor);

  TApp *app = TApp::instance();
  // temporary clear the current level to prevent UI to access deleted level
  // while switching scenes
  app->getCurrentLevel()->setLevel(nullptr);

  TUndoManager::manager()->reset();
  IconGenerator::instance()->clearRequests();
  IconGenerator::instance()->clearSceneIcons();
  ImageManager::instance()->clear();

  CacheFxCommand::instance()->onNewScene();

#ifdef USE_SQLITE_HDPOOL
  // INTERMEDIATE CACHE RESULTS MANAGEMENT: Clear all resources not accessed in
  // the last 2 sessions
  TCacheResourcePool::instance()->clearAccessedAfterSessions(
      1);  // 0 would be after this session
#endif

  ToonzScene *scene = new ToonzScene();
  TImageStyle::setCurrentScene(scene);
  printf("%s:%s Progressing:\n", __FILE__, __FUNCTION__);
  try {
    if (isXdts)
      XdtsIo::loadXdtsScene(scene, scenePath);
    else
      /*-- プログレス表示を行いながらLoad --*/
      scene->load(scenePath);
    // import if needed
    TProjectManager *pm      = TProjectManager::instance();
    auto currentProject = pm->getCurrentProject();
    if (!scene->getProject()->isLoaded() ||
        scene->getProject()->getProjectPath() !=
            currentProject->getProjectPath()) {
      ResourceImportDialog resourceLoader;
      // resourceLoader.setImportEnabled(true);
      ResourceImporter importer(scene, currentProject, resourceLoader);
      SceneResources resources(scene, scene->getXsheet());
      resources.accept(&importer);
      scene->setScenePath(importer.getImportedScenePath());
      scene->setProject(currentProject);
    }
    VersionControlManager::instance()->setFrameRange(scene->getLevelSet());
  } catch (...) {
    printf("%s:%s Exception ...:\n", __FILE__, __FUNCTION__);
    QString msg;
    msg = QObject::tr(
              "There were problems loading the scene %1.\n Some files may be "
              "missing.")
              .arg(QString::fromStdWString(scenePath.getWideString()));
    DVGui::warning(msg);
  }
  printf("%s:%s end load:\n", __FILE__, __FUNCTION__);
  auto project = scene->getProject();
  if (!project->isLoaded()) {
    project->setFolder("project", scenePath);
    scene->setProject(project);
  }
  app->getCurrentScene()->setScene(scene);
  app->getCurrentScene()->notifyNameSceneChange();
  app->getCurrentFrame()->setFrame(0);
  app->getCurrentColumn()->setColumnIndex(0);
  TPalette *palette = 0;
  if (app->getCurrentLevel() && app->getCurrentLevel()->getSimpleLevel())
    palette = app->getCurrentLevel()->getSimpleLevel()->getPalette();
  app->getCurrentPalette()->setPalette(palette);
  // In case cleanup palette is showing, switch to level palette
  app->getPaletteController()->editLevelPalette();
  app->getCurrentXsheet()->notifyXsheetSoundChanged();
  app->getCurrentObject()->setIsSpline(false);

  Previewer::clearAll();
  PreviewFxManager::instance()->reset();
  // updateCleanupSettingsPopup();
  /*- CleanupParameterの更新 -*/ //CleanupSettingsModel::onSceneSwitched()
  //CleanupParameters *cp = scene->getProperties()->getCleanupParameters();
  //CleanupParameters::GlobalParameters.assign(cp);
  CacheFxCommand::instance()->onSceneLoaded();

#ifdef USE_SQLITE_HDPOOL
  TCacheResourcePool::instance()->setPath(
      QString::fromStdWString(
          ToonzFolder::getCacheRootFolder().getWideString()),
      QString::fromStdWString(project->getName().getWideName()),
      QString::fromStdWString(scene->getSceneName()));
#endif

  UnitParameters::setFieldGuideAspectRatio(
      scene->getProperties()->getFieldGuideAspectRatio());
  IconGenerator::instance()->invalidateSceneIcon();
  DvDirModel::instance()->refreshFolder(scenePath.getParentDir());
  // set dirty for xdts files since converted tnz is not yet saved
  TApp::instance()->getCurrentScene()->setDirtyFlag(isXdts);
  History::instance()->addItem(scenePath);
  if (updateRecentFile) {
    RecentFiles::instance()->addFilePath(
        toQString(scenePath), RecentFiles::Scene,
        QString::fromStdString(scene->getProject()->getName().getName()));
  }
  RecentFiles::instance()->addFilePath(TProjectManager::instance()
                                           ->getCurrentProjectPath()
                                           .getParentDir()
                                           .getQString(),
                                       RecentFiles::Project);
  QApplication::restoreOverrideCursor();

  int forbiddenLevelCount = 0;
  for (int i = 0; i < scene->getLevelSet()->getLevelCount(); i++) {
    TXshLevel *xl = scene->getLevelSet()->getLevel(i);
    if (xl && xl->getSimpleLevel() &&
        xl->getSimpleLevel()->getProperties()->isForbidden())
      forbiddenLevelCount++;
  }
  if (forbiddenLevelCount > 0) {
    QString msg;
    msg = QObject::tr(
              "There were problems loading the scene %1.\nSome levels have not "
              "been loaded because their version is not supported")
              .arg(QString::fromStdWString(scenePath.getWideString()));
    DVGui::warning(msg);
  }

  bool exist = TSystem::doesExistFileOrLevel(
      scene->decodeFilePath(scene->getScenePath()));
  QAction *act = CommandManager::instance()->getAction(MI_RevertScene);
  if (act) act->setEnabled(exist);

  // check if the output dpi is incompatible with pixels only mode
  if (Preferences::instance()->getPixelsOnly()) {
    TPointD dpi = scene->getCurrentCamera()->getDpi();
    if (!areAlmostEqual(dpi.x, Stage::standardDpi, 0.1) ||
        !areAlmostEqual(dpi.y, Stage::standardDpi, 0.1)) {
      TDimensionD camSize = scene->getCurrentCamera()->getSize();
      TDimension camRes(camSize.lx * Stage::standardDpi,
                        camSize.ly * Stage::standardDpi);
      scene->getCurrentCamera()->setRes(camRes);
      app->getCurrentScene()->setDirtyFlag(true);
      app->getCurrentXsheet()->notifyXsheetChanged();
    }
  }

  // Check if the scene saved with the previous version AND the premultiply
  // option is set to PNG level setting
  if (scene->getVersionNumber() <
      VersionNumber(71, 1)) {  // V1.4 = 71.0 , V1.5 = 71.1
    QStringList modifiedPNGLevelNames;
    std::vector<TXshLevel *> levels;
    scene->getLevelSet()->listLevels(levels);
    for (auto level : levels) {
      if (!level || !level->getSimpleLevel()) continue;
      TFilePath path = level->getPath();
      if (path.isEmpty() || path.getType() != "png") continue;
      if (level->getSimpleLevel()->getProperties()->doPremultiply()) {
        level->getSimpleLevel()->getProperties()->setDoPremultiply(false);
        modifiedPNGLevelNames.append(QString::fromStdWString(level->getName()));
      }
    }
    if (!modifiedPNGLevelNames.isEmpty()) {
      DVGui::info(QObject::tr("The Premultiply options in the following levels "
                              "are disabled, since PNG files are premultiplied "
                              "on loading in the current version: %1")
                      .arg(modifiedPNGLevelNames.join(", ")));
      app->getCurrentScene()->setDirtyFlag(true);
    }
  }

  // Check if any column has visibility toggles with different states and the
  // "unify visibility toggles" option is enabled
  if (Preferences::instance()->isUnifyColumnVisibilityTogglesEnabled())
    ColumnCmd::unifyColumnVisibilityToggles();

  // caching raster levels
  int cacheRasterBehavior =
      Preferences::instance()->getRasterLevelCachingBehavior();
  if (cacheRasterBehavior != 0)  // not "On Demand"
    LevelCmd::loadAllUsedRasterLevelsAndPutInCache(cacheRasterBehavior ==
                                                   2);  // "All Icons & Images"

  printf("%s:%s loadScene() completed :\n", __FILE__, __FUNCTION__);
  
  TApp::instance()->getPaletteController()->editLevelPalette();
  return true;
}

//===========================================================================
// IoCmd::loadScene()
//---------------------------------------------------------------------------

bool IoCmd::loadScene() {
  TSelection *oldSelection =
      TApp::instance()->getCurrentSelection()->getSelection();
  FileSelection *fileSelection =
      dynamic_cast<FileSelection *>(TSelection::getCurrent());
  if (fileSelection) {
    std::vector<TFilePath> files;
    fileSelection->getSelectedFiles(files);
    if (files.size() == 1 && files[0] != TFilePath() &&
        files[0].getType() == "tnz")
      return loadScene(files[0]);
  }

  static LoadScenePopup *popup = 0;
  if (!popup) {
    popup = new LoadScenePopup();
  }
  int ret = popup->exec();
  if (ret == QDialog::Accepted) {
    return true;
  } else {
    TApp::instance()->getCurrentSelection()->setSelection(oldSelection);
    return false;
  }
}
//===========================================================================
// IoCmd::saveSceneVersion()
//---------------------------------------------------------------------------
// Utility static methods for file naming
// I put it here because statics in tfilepath header would need a long rebuild for each code change...
#include <sstream>
#include <iomanip>
static bool fileExists(std::string fileName) {
  std::ifstream infile(fileName.c_str());
  return infile.good();
}
static std::string makeNumberedFilename(std::string prefix, int frame, 
                                        std::string extension, int padding) {
  std::ostringstream filenameSs;
  filenameSs << prefix << std::setfill('0') << std::setw(padding) << frame
             << "." << extension;
  return filenameSs.str();
}

static void incrementNumberedFilename(std::string &str) {
  // input string can be the full path : 
  // string searches are made backward from the end of the string
  int padding           = 0;
  std::string extension = "";
  int lastPoint         = str.rfind(".");
  if (lastPoint != std::string::npos)
    extension = str.substr(lastPoint + 1);  // tnz (no dot!)
  else
    lastPoint = str.length();
  int n = lastPoint - 1;
  while (isdigit(str[n])) {
    n--;
  }
  int lastNum = n + 1;
  auto numStr = str.substr(lastNum, lastPoint - lastNum);

  int intValue;
  std::string prefix = str.substr(0, lastNum);
  if (numStr.length() > 0) {
    std::istringstream iss(numStr);
    iss >> intValue;
    padding = numStr.length();
    intValue++;
  } else {
    intValue = 1;
  }
  // avoid overwriting scene files...
  while (
      fileExists(makeNumberedFilename(prefix, intValue, extension, padding))) {
    intValue++;
  }
  str = makeNumberedFilename(prefix, intValue, extension, padding);
}

bool IoCmd::saveSceneVersion() { 
  auto scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene->isUntitled())
    return saveScene(0);
  auto oldScenePath = scene->getScenePath().getQString().toStdString();
  auto newScenePath = oldScenePath;
  incrementNumberedFilename(newScenePath);
  return saveScene(TFilePath(newScenePath), 0);

  //// I couldn't understand how to use existing toonz numbered file naming methods :
  //// this example removes the version number...
  //auto scenePath = TApp::instance()->getCurrentScene()->getScene()->getScenePath();
  //auto newScenePath = scenePath;
  //scenePath.setUnderscoreFormatAllowed(true);
  //scenePath.setFilePathProperties(false, true, 4);
  //auto frame = scenePath.getFrame();
  //frame.setZeroPadding(4);
  //if (frame == TFrameId::EMPTY_FRAME || frame == TFrameId::NO_FRAME) 
  //  scenePath.withFrame(1);
  //else
  //  scenePath.withFrame(frame.getNumber() + 1);
  //std::cout << "saveSceneVersion oldScenePath: " << scenePath << " frame: " << frame
  //          << " newScenePath: " << newScenePath << std::endl;
  //return false;
}
//===========================================================================
// IoCmd::loadSubScene()
//---------------------------------------------------------------------------

bool IoCmd::loadSubScene() {
  TSelection *oldSelection =
      TApp::instance()->getCurrentSelection()->getSelection();
  FileSelection *fileSelection =
      dynamic_cast<FileSelection *>(TSelection::getCurrent());

  if (fileSelection) {
    IoCmd::LoadResourceArguments args;
    {
      std::vector<TFilePath> filePaths;
      fileSelection->getSelectedFiles(filePaths);

      args.resourceDatas.assign(filePaths.begin(), filePaths.end());
    }

    if (args.resourceDatas.size() == 1 &&
        args.resourceDatas[0].m_path != TFilePath() &&
        args.resourceDatas[0].m_path.getType() == "tnz") {
      loadResources(args);
      return true;
    }
  } else {
    static LoadSubScenePopup *popup = 0;
    if (!popup) {
      popup = new LoadSubScenePopup();
      popup->addFilterType("tnz");
    }
    int ret = popup->exec();
    if (ret == QDialog::Accepted) {
      TApp::instance()->getCurrentScene()->setDirtyFlag(false);
      return true;
    } else {
      TApp::instance()->getCurrentSelection()->setSelection(oldSelection);
      return false;
    }
  }
  return false;
}

bool IoCmd::loadSubScene(const TFilePath &scenePath) {
  IoCmd::LoadResourceArguments args(scenePath);

  if (args.resourceDatas.size() == 1 &&
      args.resourceDatas[0].m_path != TFilePath() &&
      args.resourceDatas[0].m_path.getType() == "tnz") {
    loadResources(args);
    return true;
  }

  return false;
}

std::vector<int>
    loadedPsdLevelIndex;  // memorizza gli indici dei livelli già caricati
                          // serve per identificare i subfolder caricati
                          // Trovare un metodo alternativo.

//! Returns the number of actually loaded levels
static int createSubXSheetFromPSDFolder(IoCmd::LoadResourceArguments &args,
                                        TXsheet *xsh, int &col0,
                                        int psdLevelIndex,
                                        PsdSettingsPopup *popup) {
  assert(popup->isFolder(psdLevelIndex));

  int row0  = 0;
  int &row1 = args.row1, &col1 = args.col1;

  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();

  TXshLevel *cl = scene->createNewLevel(CHILD_XSHLEVEL);
  assert(cl);

  TXshChildLevel *childLevel = cl->getChildLevel();
  assert(childLevel);

  TXsheet *childXsh = childLevel->getXsheet();

  int count = 0, subCol0 = 0;
  for (int i = 0; i < popup->getFramesCount(psdLevelIndex); ++i) {
    if (popup->isSubFolder(psdLevelIndex, i)) {
      // se è un subfolder allora è un livello
      int levelIndex = popup->getSubfolderLevelIndex(psdLevelIndex, i);
      count += createSubXSheetFromPSDFolder(args, childXsh, subCol0, levelIndex,
                                            popup);
    } else {
      TFilePath psdpath = popup->getPsdFramePath(psdLevelIndex, i);
      TXshLevel *xl     = 0;
      try {
        xl = ::loadResource(scene, psdpath, args.castFolder, row0, col0, row1,
                            col1, false);
      } catch (TException &e) {
        error(QString::fromStdWString(e.getMessage()));
      } catch (...) {
        error("Unhandled exception encountered");
      }
      if (xl) {
        // lo importo nell'xsheet
        childXsh->exposeLevel(0, subCol0, xl);
        args.loadedLevels.push_back(xl);

        ++subCol0, ++count;
        loadedPsdLevelIndex.push_back(psdLevelIndex);
      }
    }
  }
  if (childXsh) {
    int rowCount = childXsh->getFrameCount();
    int r;
    for (r = 0; r < rowCount; r++)
      xsh->setCell(r, col0, TXshCell(cl, TFrameId(r + 1)));
    col0++;
  }
  return count;
}

//  Load a psd file
//! Returns the number of actually loaded levels
static int loadPSDResource(IoCmd::LoadResourceArguments &args,
                           bool updateRecentFile, PsdSettingsPopup *popup) {
  int &row0 = args.row0;
  int &col0 = args.col0;
  int &row1 = args.row1;
  int &col1 = args.col1;

  int count            = 0;
  TApp *app            = TApp::instance();
  ToonzScene *scene    = app->getCurrentScene()->getScene();
  TXsheet *xsh         = scene->getXsheet();
  if (row0 == -1) row0 = app->getCurrentFrame()->getFrameIndex();
  if (col0 == -1) col0 = app->getCurrentColumn()->getColumnIndex();

  TXshLevel *cl     = 0;
  TXsheet *childXsh = 0;
  // if the option "expose in sub-xsheet" is ON"
  if (popup->subxsheet()) {
    cl = scene->createNewLevel(CHILD_XSHLEVEL);
    assert(cl);
    TXshChildLevel *childLevel = cl->getChildLevel();
    assert(childLevel);
    childXsh = childLevel->getXsheet();
  }
  int subCol0 = 0;
  loadedPsdLevelIndex.clear();
  // for each layer in psd
  for (int i = 0; i < popup->getPsdLevelCount(); i++) {
    if (popup->isFolder(i) && popup->getFolderOption() == 1) {
      if (find(loadedPsdLevelIndex.begin(), loadedPsdLevelIndex.end(), i) !=
          loadedPsdLevelIndex.end())
        continue;
      if (childXsh)
        count +=
            createSubXSheetFromPSDFolder(args, childXsh, subCol0, i, popup);
      else
        count += createSubXSheetFromPSDFolder(args, xsh, col0, i, popup);
    } else {
      TFilePath psdpath = popup->getPsdPath(i);
      TXshLevel *xl     = 0;

      try {
        xl = ::loadResource(scene, psdpath, args.castFolder, row0, col0, row1,
                            col1, !popup->subxsheet());
      } catch (TException &e) {
        error(QString::fromStdWString(e.getMessage()));
      } catch (...) {
        error("Unhandled exception encountered");
      }
      if (xl) {
        // lo importo nell'xsheet
        if (popup->subxsheet() && childXsh) {
          childXsh->exposeLevel(0, subCol0, xl);
          subCol0++;
        } else {
          // move the current column to the right
          col0++;
          app->getCurrentColumn()->setColumnIndex(col0);
        }
        args.loadedLevels.push_back(xl);
        count++;
      }
    }
  }
  if (childXsh) {
    int rowCount = childXsh->getFrameCount();
    int r;
    for (r = 0; r < rowCount; r++)
      xsh->setCell(row0 + r, col0, TXshCell(cl, TFrameId(r + 1)));
  }

  return count;
}

//===========================================================================
// IoCmd::loadResources(actualPaths[], castFolder, row, col)
//---------------------------------------------------------------------------

typedef IoCmd::LoadResourceArguments::ScopedBlock LoadScopedBlock;

struct LoadScopedBlock::Data {
  std::unique_ptr<DVGui::ProgressDialog>
      m_progressDialog;  //!< Progress dialog displayed on multiple paths.
  int m_loadedCount;     //!< Number of loaded levels.
  bool m_hasSoundLevel;  //!< Whether a sound level was loaded.

public:
  Data() : m_loadedCount(), m_hasSoundLevel() {}
};

//-----------------------------------------------------------------------------

LoadScopedBlock::ScopedBlock() : m_data(new Data) {}

//-----------------------------------------------------------------------------

LoadScopedBlock::~ScopedBlock() {
  if (m_data->m_loadedCount > 0) {
    TApp *app = TApp::instance();

    app->getCurrentXsheet()->notifyXsheetChanged();
    if (m_data->m_hasSoundLevel)
      app->getCurrentXsheet()->notifyXsheetSoundChanged();
    app->getCurrentScene()->notifyCastChange();
    app->getCurrentScene()->setDirtyFlag(true);
    app->getCurrentTool()->onImageChanged(
        (TImage::Type)app->getCurrentImageType());
  }
}

//-----------------------------------------------------------------------------

DVGui::ProgressDialog &LoadScopedBlock::progressDialog() const {
  if (!m_data->m_progressDialog.get()) {
    m_data->m_progressDialog.reset(
        new DVGui::ProgressDialog("", QObject::tr("Cancel"), 0, 0));
  }

  return *m_data->m_progressDialog;
}

//=============================================================================

int IoCmd::loadResources(LoadResourceArguments &args, bool updateRecentFile,
                         LoadScopedBlock *sb) {
  struct locals {
    static bool isDir(const LoadResourceArguments::ResourceData &rd) {
      return QFileInfo(rd.m_path.getQString()).isDir();
    }
  };  // locals

  if (args.resourceDatas.empty()) return 0;

  // Redirect to resource folders loading in case they're all dirs
  if (std::all_of(args.resourceDatas.begin(), args.resourceDatas.end(),
                  locals::isDir))
    return loadResourceFolders(args, sb);

  boost::optional<LoadScopedBlock> sb_;
  if (!sb) sb = (sb_ = boost::in_place()).get_ptr();

  int &row0 = args.row0, &col0 = args.col0, &row1 = args.row1,
      &col1 = args.col1;

  // Setup local variables
  TApp *app         = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh      = scene->getXsheet();
  // use current frame/column if row/col is not set
  if (row0 == -1) row0 = app->getCurrentFrame()->getFrameIndex();
  if (col0 == -1) col0 = app->getCurrentColumn()->getColumnIndex();

  int rCount = args.resourceDatas.size(), loadedCount = 0;
  bool isSoundLevel = false;

  // show wait cursor in case of caching all images because it is time consuming
  if (args.cachingBehavior == LoadResourceArguments::ALL_ICONS_AND_IMAGES)
    QApplication::setOverrideCursor(Qt::WaitCursor);

  // Initialize progress dialog
  DVGui::ProgressDialog *progressDialog =
      (rCount > 1) ? &sb->progressDialog() : 0;

  if (progressDialog) {
    progressDialog->setModal(true);
    progressDialog->setMinimum(0);
    progressDialog->setMaximum(rCount);
    progressDialog->show();
  }

  // Initialize import dialog
  ResourceImportDialog importDialog;
  if (args.importPolicy != LoadResourceArguments::ASK_USER) {
    importDialog.setImportEnabled(args.importPolicy ==
                                  LoadResourceArguments::IMPORT);
  }

  std::vector<TFilePath> paths;
  int all = 0;

  // Loop for all the resources to load
  for (int r = 0; r != rCount; ++r) {
    if (importDialog.aborted()) break;

    QString origName =
        args.resourceDatas[r].m_path.withoutParentDir().getQString();

    LoadResourceArguments::ResourceData rd(args.resourceDatas[r]);
    TFilePath &path = rd.m_path;

    if (!rd.m_path.isLevelName())
      path = TFilePath(path.getLevelNameW()).withParentDir(path.getParentDir());

    if (std::find(paths.begin(), paths.end(), path) != paths.end()) {
      if (!all) {
        QString question =
            QObject::tr(
                "File '%1' will reload level '%2' as a duplicate column in the "
                "scene.\n\nAllow duplicate?")
                .arg(origName)
                .arg(QString::fromStdString(path.getName()));
        QString Yes    = QObject::tr("Allow");
        QString YesAll = QObject::tr("Allow All Dups");
        QString No     = QObject::tr("No");
        QString NoAll  = QObject::tr("No to All Dups");
        int ret        = DVGui::MsgBox(question, Yes, YesAll, No, NoAll, 0);
        switch (ret) {
        case 2:
          all = 1;  // YesAll
        case 1:
          break;  // Yes
        case 4:
          all = 2;  // NoAll
        case 3:
          continue;
        }
      } else if (all == 2)
        continue;
    }
    paths.push_back(path);

    if (progressDialog) {
      if (progressDialog->wasCanceled())
        break;
      else {
        progressDialog->setLabelText(
            DVGui::ProgressDialog::tr("Loading \"%1\"...")
                .arg(path.getQString()));
        progressDialog->setValue(r);

        QCoreApplication::processEvents();
      }
    }

    bool isLastResource = (r == rCount - 1);
    importDialog.setIsLastResource(isLastResource);

    bool isScene = (TFileType::getInfo(path) == TFileType::TOONZSCENE);

    if (scene->isExternPath(path) || isScene) {
      // extern resource: import or link?
      int ret = importDialog.askImportQuestion(path);
      if (ret == ResourceImportDialog::A_CANCEL) break;
    }

    // for the scene file
    TXshLevel *xl = 0;
    if (isScene) {
      TFilePath oldDstFolder = importDialog.getDstFolder();
      TFilePath dstFolder = (Preferences::instance()->isSubsceneFolderEnabled())
                                ? TFilePath(path.getName())
                                : TFilePath();

      importDialog.setDstFolder(dstFolder);
      importDialog.setIsLastResource(false);

      // load the scene as subXsheet
      try {
        xl = loadChildLevel(scene, path, row0, col0, importDialog);
        if (dstFolder != TFilePath())
          app->getCurrentScene()->notifyCastFolderAdded(
              scene->getLevelSet()->getDefaultFolder() + dstFolder);
        // increment the number of resources actually loaded
        ++loadedCount;
        // move the column to the right
        ++col0;
        // register the loaded level to args
        args.loadedLevels.push_back(xl);
        app->getCurrentXsheet()->notifyXsheetSoundChanged();
      } catch (...) {
      }

      importDialog.setIsLastResource(isLastResource);
      importDialog.setDstFolder(oldDstFolder);

      app->getCurrentColumn()->setColumnIndex(col0);

      continue;
    }
    // for other level files
    else {
      try {
        path = importDialog.process(scene, 0, path);
        // path = scene->decodeFilePath(codedPath);
      } catch (std::string msg) {
        error(QString::fromStdString(msg));
        continue;
      } catch (...) {
        error("Unhandled exception encountered");
        continue;
      }

      if (importDialog.aborted()) break;
    }
    if (path.getType() == "psd") {
      static PsdSettingsPopup *popup = 0;
      if (!popup) {
        popup = new PsdSettingsPopup();
      }
      popup->setPath(path);

      int ret = popup->exec();
      if (ret == 0) continue;

      loadedCount += loadPSDResource(args, updateRecentFile, popup);

      if (updateRecentFile)
        RecentFiles::instance()->addFilePath(
            toQString(scene->decodeFilePath(path)), RecentFiles::Level);
    } else {
      // reuse TFrameIds retrieved by FileBrowser
      std::vector<TFrameId> fIds;
      if ((int)args.frameIdsSet.size() > r)  // if there is fIds to be reused
      {
        fIds = args.frameIdsSet[r];
      }

      try {
        xl = ::loadResource(
            scene, rd, args.castFolder, row0, col0, row1, col1, args.expose,
#if (__cplusplus > 199711L)
            std::move(fIds),
#else
            fIds,
#endif
            args.xFrom, args.xTo, args.levelName, args.step, args.inc,
            args.frameCount, args.doesFileActuallyExist);
        if (updateRecentFile) {
          RecentFiles::instance()->addFilePath(
              toQString(scene->decodeFilePath(path)), RecentFiles::Level);
        }
      } catch (TException &e) {
        error(QString::fromStdWString(e.getMessage()));
      } catch (...) {
        error("Unhandled exception encountered");
      }
      // if load success
      if (xl) {
        isSoundLevel = isSoundLevel || xl->getType() == SND_XSHLEVEL;
        // register the loaded level to args
        args.loadedLevels.push_back(xl);
        // increment the number of loaded resources
        ++loadedCount;

        // load the image data of all frames to cache at the beginning
        if (args.cachingBehavior != LoadResourceArguments::ON_DEMAND) {
          TXshSimpleLevel *simpleLevel = xl->getSimpleLevel();
          if (simpleLevel && (simpleLevel->getType() == TZP_XSHLEVEL ||
                              simpleLevel->getType() == OVL_XSHLEVEL)) {
            bool cacheImagesAsWell =
                (args.cachingBehavior ==
                 LoadResourceArguments::ALL_ICONS_AND_IMAGES);
            simpleLevel->loadAllIconsAndPutInCache(cacheImagesAsWell);
          }
        }
      }
    }
  }

  if (loadedCount) app->getCurrentFrame()->setFrameIndex(row0);

  sb->data().m_loadedCount += loadedCount;
  sb->data().m_hasSoundLevel = sb->data().m_hasSoundLevel || isSoundLevel;

  // revert the cursor
  if (args.cachingBehavior == LoadResourceArguments::ALL_ICONS_AND_IMAGES)
    QApplication::restoreOverrideCursor();

  return loadedCount;
}

//===========================================================================
// IoCmd::exposeLevel(simpleLevel, row, col)
//---------------------------------------------------------------------------

bool IoCmd::exposeLevel(TXshSimpleLevel *sl, int row, int col, bool insert,
                        bool overWrite) {
  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  assert(xsh);
  std::vector<TFrameId> fids;
  sl->getFids(fids);
  return exposeLevel(sl, row, col, fids, insert, overWrite);
}

//===========================================================================
// IoCmd::exposeLevel(simpleLevel, row, col)
//---------------------------------------------------------------------------

bool IoCmd::exposeLevel(TXshSimpleLevel *sl, int row, int col,
                        const std::vector<TFrameId> &fids, bool insert,
                        bool overWrite) {
  assert(sl);

  TApp *app    = TApp::instance();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  assert(xsh);
  int frameCount         = fids.size();
  bool insertEmptyColumn = false;
  if (!insert && !overWrite)
    insertEmptyColumn = beforeCellsInsert(
        xsh, row, col, fids.size(), TXshColumn::toColumnType(sl->getType()));
  ExposeType type     = eNone;
  if (insert) type    = eShiftCells;
  if (overWrite) type = eOverWrite;
  TUndoManager::manager()->beginBlock();
  ExposeLevelUndo *undo =
      new ExposeLevelUndo(sl, row, col, frameCount, insertEmptyColumn, type);
  xsh->exposeLevel(row, col, sl, fids, overWrite);
  undo->setFids(fids);
  TUndoManager::manager()->add(undo);

  // Column name renamed to level name only if was originally empty
  bool wasColumnEmpty = insertEmptyColumn;
  if (!wasColumnEmpty) {
    TXshColumn *column = xsh->getColumn(col);
    int r0, r1;
    column->getRange(r0, r1);
    if ((r1 - r0 + 1) == fids.size()) wasColumnEmpty = true;
  }

  if (wasColumnEmpty) {
    TStageObjectId columnId = TStageObjectId::ColumnId(col);
    std::string columnName =
        QString::fromStdWString(sl->getName()).toStdString();
    TStageObjectCmd::rename(columnId, columnName, app->getCurrentXsheet());
  }

  TUndoManager::manager()->endBlock();

  app->getCurrentXsheet()->notifyXsheetChanged();
  return true;
}

//===========================================================================
// exposeComment
//---------------------------------------------------------------------------

bool IoCmd::exposeComment(int row, int &col, QList<QString> commentList,
                          QString fileName) {
  TApp *app                         = TApp::instance();
  TXsheet *xsh                      = app->getCurrentXsheet()->getXsheet();
  TXshSoundTextColumn *textSoundCol = new TXshSoundTextColumn();
  textSoundCol->setXsheet(xsh);
  textSoundCol->createSoundTextLevel(row, commentList);
  bool columnInserted = beforeCellsInsert(xsh, row, col, commentList.size(),
                                          TXshColumn::eSoundTextType);
  xsh->insertColumn(col, textSoundCol);

  // Setto il nome di dafult della colonna al nome del file magpie.
  TStageObject *obj = xsh->getStageObject(TStageObjectId::ColumnId(col));
  std::string str   = fileName.toStdString();
  obj->setName(str);

  TUndoManager::manager()->add(
      new ExposeCommentUndo(textSoundCol, col, columnInserted, fileName));

  return true;
}

//===========================================================================
// importLipSync
//---------------------------------------------------------------------------

bool IoCmd::importLipSync(TFilePath levelPath, QList<TFrameId> frameList,
                          QList<QString> commentList, QString fileName) {
  TApp *app = TApp::instance();
  int col   = app->getCurrentColumn()->getColumnIndex();
  int row   = app->getCurrentFrame()->getFrameIndex();

  IoCmd::LoadResourceArguments args;
  IoCmd::LoadResourceArguments::ScopedBlock sb;

  // Create text column
  IoCmd::exposeComment(row, col, commentList, fileName);

  // Load Level
  args.resourceDatas.push_back(levelPath);
  args.expose = false;

  if (!IoCmd::loadResources(args, false, &sb)) {
    DVGui::error(QObject::tr("It is not possible to load the level %1")
                     .arg(toQString(levelPath)));
    return false;
  }

  // Expose Level in xsheet
  assert(args.loadedLevels.size() == 1);
  TXshLevel *loadedLevel = args.loadedLevels.at(0);

  std::vector<TFrameId> fids;
  for (int i = 0; i < frameList.size(); i++) fids.push_back(frameList.at(i));

  if (!IoCmd::exposeLevel(loadedLevel->getSimpleLevel(), row, col, fids)) {
    DVGui::error(QObject::tr("It is not possible to load the level %1")
                     .arg(toQString(levelPath)));
    return false;
  }

  return true;
}

//===========================================================================
// If the scene will be saved in the different folder, check out the scene
// cast.
// if the cast contains the level specified with $scenefolder alias,
// open a warning popup notifying that such level will lose link.
// return false if cancelled.
bool IoCmd::takeCareSceneFolderItemsOnSaveSceneAs(
    ToonzScene *scene, const TFilePath &newPath, TXsheet *subxsh,
    QHash<TXshLevel *, TFilePath> &orgLevelPaths, bool useSceneSubfolders) {
  auto setPathToLevel = [&](TXshLevel *level, TFilePath fp) {
    // in case of saving subxsheet, the current scene will not be switched to
    // the saved one
    // so the level paths are needed to be reverted after saving
    if (subxsh) orgLevelPaths.insert(level, level->getPath());
    if (TXshSimpleLevel *sil = level->getSimpleLevel())
      sil->setPath(fp, true);
    else if (TXshPaletteLevel *pal = level->getPaletteLevel())
      pal->setPath(fp);
    else if (TXshSoundLevel *sol = level->getSoundLevel())
      sol->setPath(fp);
  };

  TFilePath oldSceneFolder =
      scene->decodeFilePath(scene->getScenePath()).getParentDir();
  TFilePath newSceneFolder = scene->decodeFilePath(newPath).getParentDir();

  // in case of saving in the same folder
  if (oldSceneFolder == newSceneFolder && !useSceneSubfolders) return true;

  TLevelSet *levelSet = scene->getLevelSet();
  std::vector<TXshLevel *> levels;

  // in case of saving subxsheet, checking only used levels.
  if (subxsh) {
    std::set<TXshLevel *> saveSet;
    subxsh->getUsedLevels(saveSet);
    levels = std::vector<TXshLevel *>(saveSet.begin(), saveSet.end());
  }
  // in case of saving the scene (i.e. top xsheet)
  else
    levelSet->listLevels(levels);

  std::string oldSceneName = scene->getScenePath().getName();
  std::string newSceneName = newPath.getName();

  QList<TXshLevel *> sceneFolderLevels, changedFolderLevels;
  QString str;
  int count = 0;
  for (TXshLevel *level : levels) {
    if (!level->getPath().isEmpty()) {
      if (TFilePath("$scenefolder").isAncestorOf(level->getPath())) {
        TFilePath levelFullPath = scene->decodeFilePath(level->getPath());
        // check if the path can be re-coded with the new scene folder path
        if (newSceneFolder.isAncestorOf(levelFullPath)) {
          // just replace the path without warning
          TFilePath fp =
              TFilePath("$scenefolder") + (levelFullPath - newSceneFolder);
          setPathToLevel(level, fp);
        }
        // if re-coding is not possible, then it needs to ask user's preference
        else {
          sceneFolderLevels.append(level);
          if (count < 10) {
            str.append("    " + QString::fromStdWString(level->getName()) +
                       " (" + level->getPath().getQString() + ")\n");
          }
          count++;
        }
      } else if (useSceneSubfolders) {
        if (level->getPath().getParentDir().getName() == oldSceneName)
          changedFolderLevels.append(level);
      }
    }
  }

  // Immediately copy untitled levels to new area
  if (!changedFolderLevels.isEmpty()) {
    enum OVERWRITEPOLICY { ASK, YES_FOR_ALL, NO_FOR_ALL } policy = ASK;
    for (int i = 0; i < changedFolderLevels.size(); i++) {
      TXshLevel *level = changedFolderLevels.at(i);
      TFilePath fp     = level->getPath();
      TFilePath sceneRoot =
          scene->isUntitled()
              ? scene->getScenePath().getParentDir()
              : scene->getScenePath().getParentDir().getParentDir();
      fp = scene->decodeFilePath(fp) - sceneRoot;
      fp = fp.getParentDir().getParentDir() + TFilePath(newSceneName) +
           fp.withoutParentDir();
      fp = TFilePath(newSceneFolder.getParentDir()) + fp;
      // check the level existence
      if (TSystem::doesExistFileOrLevel(fp)) {
        bool overwrite = (policy == YES_FOR_ALL);
        if (policy == ASK) {
          QString question =
              QObject::tr(
                  "File %1 already exists.\nDo you want to overwrite it?")
                  .arg(fp.getQString());
          int ret_overwrite = DVGui::MsgBox(
              question, QObject::tr("Overwrite"),
              QObject::tr("Overwrite for All"), QObject::tr("Don't Overwrite"),
              QObject::tr("Don't Overwrite for All"), 0);
          if (ret_overwrite == 0) return false;
          if (ret_overwrite == 1)
            overwrite = true;
          else if (ret_overwrite == 2) {
            overwrite = true;
            policy    = YES_FOR_ALL;
          } else if (ret_overwrite == 4)
            policy = NO_FOR_ALL;
        }
        if (!overwrite) continue;
      }

      TFilePath srcFp = scene->decodeFilePath(level->getPath());
      if (TSystem::doesExistFileOrLevel(srcFp) &&
          !TSystem::copyFileOrLevel(fp, srcFp))
        warning(QObject::tr("Failed to overwrite %1").arg(fp.getQString()));

      // copy the palette as well
      if (level->getType() == TZP_XSHLEVEL) {
        if (TSystem::doesExistFileOrLevel(srcFp.withType("tpl")) &&
            !TSystem::copyFileOrLevel(fp.withType("tpl"),
                                      srcFp.withType("tpl")))
          warning(QObject::tr("Failed to overwrite %1")
                      .arg(fp.withType("tpl").getQString()));
      }
    }
  }
  // list maximum 10 levels
  if (count > 10)
    str.append(QObject::tr("    + %1 more level(s) \n").arg(count - 10));

  // in case there is no items with $scenefolder
  if (sceneFolderLevels.isEmpty()) return true;

  str = QObject::tr(
            "The following level(s) use path with $scenefolder alias.\n\n") +
        str + QObject::tr(
                  "\nThey will not be opened properly when you load the "
                  "scene next time.\nWhat do you want to do?");

  int ret = DVGui::MsgBox(
      str, QObject::tr("Copy the levels to correspondent paths"),
      QObject::tr("Decode all $scenefolder aliases"),
      QObject::tr("Save the scene only"), QObject::tr("Cancel"), 0);
  if (ret == 4 || ret == 0) return false;

  if (ret == 1) {  // copy the levels case
    enum OVERWRITEPOLICY { ASK, YES_FOR_ALL, NO_FOR_ALL } policy = ASK;
    for (int i = 0; i < sceneFolderLevels.size(); i++) {
      TXshLevel *level = sceneFolderLevels.at(i);
      TFilePath fp     = level->getPath() - TFilePath("$scenefolder");
      fp               = fp.withParentDir(newSceneFolder);
      // check the level existence
      if (TSystem::doesExistFileOrLevel(fp)) {
        bool overwrite = (policy == YES_FOR_ALL);
        if (policy == ASK) {
          QString question =
              QObject::tr(
                  "File %1 already exists.\nDo you want to overwrite it?")
                  .arg(fp.getQString());
          int ret_overwrite = DVGui::MsgBox(
              question, QObject::tr("Overwrite"),
              QObject::tr("Overwrite for All"), QObject::tr("Don't Overwrite"),
              QObject::tr("Don't Overwrite for All"), 0);
          if (ret_overwrite == 0) return false;
          if (ret_overwrite == 1)
            overwrite = true;
          else if (ret_overwrite == 2) {
            overwrite = true;
            policy    = YES_FOR_ALL;
          } else if (ret_overwrite == 4)
            policy = NO_FOR_ALL;
        }
        if (!overwrite) continue;
      }

      TFilePath srcFp = scene->decodeFilePath(level->getPath());
      if (!TSystem::copyFileOrLevel(fp, srcFp))
        warning(QObject::tr("Failed to overwrite %1").arg(fp.getQString()));
      // copy the palette as well
      if (level->getType() == TZP_XSHLEVEL) {
        if (!TSystem::copyFileOrLevel(fp.withType("tpl"),
                                      srcFp.withType("tpl")))
          warning(QObject::tr("Failed to overwrite %1")
                      .arg(fp.withType("tpl").getQString()));
      }
    }
  } else if (ret == 2) {  // decode $scenefolder aliases case
    Preferences::PathAliasPriority oldPriority =
        Preferences::instance()->getPathAliasPriority();
    Preferences::instance()->setValue(pathAliasPriority,
                                      Preferences::ProjectFolderOnly);
    for (int i = 0; i < sceneFolderLevels.size(); i++) {
      TXshLevel *level = sceneFolderLevels.at(i);

      // decode and code again
      TFilePath fp =
          scene->codeFilePath(scene->decodeFilePath(level->getPath()));
      setPathToLevel(level, fp);
    }
    Preferences::instance()->setValue(pathAliasPriority, oldPriority);
  }

  // Save the scene only case (ret == 3), do nothing

  return true;
}

//===========================================================================
// Commands
//---------------------------------------------------------------------------

class SaveSceneCommandHandler final : public MenuItemHandler {
public:
  SaveSceneCommandHandler() : MenuItemHandler(MI_SaveScene) {}
  void execute() override {
    if (Preferences::instance()->getBoolValue(doNotShowPopupSaveScene)) {
      IoCmd::saveScene();
      return;
    }

    QString question;
    question = QObject::tr(
        "This only saves scene data, not unsaved level changes. Did you want "
        "to save only the scene, or save all?");
    QString checkBoxLabel = QObject::tr("Do not show again.");
    QStringList buttons;
    buttons << QObject::tr("Save Scene Only") << QObject::tr("Save All");
    DVGui::MessageAndCheckboxDialog *saveDialog = DVGui::createMsgandCheckbox(
        DVGui::WARNING, question, checkBoxLabel, buttons, 0, Qt::Unchecked);
    int ret     = saveDialog->exec();
    int checked = saveDialog->getChecked();
    saveDialog->deleteLater();

    if (checked > 0)
      Preferences::instance()->setValue(doNotShowPopupSaveScene, true);

    if (ret == 1)
      IoCmd::saveScene();
    else if (ret == 2)
      IoCmd::saveAll();
  }
} saveSceneCommandHandler;

//---------------------------------------------------------------------------

class SaveLevelCommandHandler final : public MenuItemHandler {
public:
  SaveLevelCommandHandler() : MenuItemHandler(MI_SaveLevel) {}
  void execute() override {
    TXshSimpleLevel *sl = TApp::instance()->getCurrentLevel()->getSimpleLevel();
    if (!sl) {
      DVGui::warning(QObject::tr("No Current Level"));
      return;
    }
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) {
      DVGui::warning(QObject::tr("No Current Scene"));
      return;  // non dovrebbe succedere mai
    }
    TFilePath levelPath = sl->getPath();
    levelPath           = scene->decodeFilePath(levelPath);
    QString str         = QString::fromStdWString(levelPath.getWideString());
    if (!(sl->getPath().isAbsolute() || !scene->isUntitled() ||
          (!sl->getPath().isAbsolute() && !str.contains("untitled")))) {
      error(QObject::tr("Save the scene first"));
      return;
    }

    // reset the undo before save level
    if (Preferences::instance()->getBoolValue(resetUndoOnSavingLevel))
      TUndoManager::manager()->reset();

    if (!IoCmd::saveLevel()) error(QObject::tr("Save level Failed"));
  }
} saveLevelCommandHandler;

//-----------------------------------------------------------------------------

class SaveProjectTemplate final : public MenuItemHandler {
public:
  SaveProjectTemplate() : MenuItemHandler(MI_SaveDefaultSettings) {}
  void execute() override {
    QString question;
    question =
        QObject::tr("Are you sure you want to save the Default Settings?");
    int ret =
        DVGui::MsgBox(question, QObject::tr("Save"), QObject::tr("Cancel"), 0);
    if (ret == 2 || ret == 0) return;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    try {
      TProjectManager::instance()->saveTemplate(scene);
    } catch (TSystemException se) {
      DVGui::warning(QString::fromStdWString(se.getMessage()));
      return;
    } catch (...) {
      DVGui::warning("Unhandled exception encountered");
      return;
    }
  }
} saveProjectTemplate;

//-----------------------------------------------------------------------------

class OpenRecentSceneFileCommandHandler final : public MenuItemHandler {
public:
  OpenRecentSceneFileCommandHandler() : MenuItemHandler(MI_OpenRecentScene) {}
  void execute() override {
    QAction *act = CommandManager::instance()->getAction(MI_OpenRecentScene);
    DVMenuAction *menu = dynamic_cast<DVMenuAction *>(act->menu());
    int index          = menu->getTriggeredActionIndex();
    QString path =
        RecentFiles::instance()->getFilePath(index, RecentFiles::Scene);
    IoCmd::loadScene(TFilePath(path.toStdWString()), false);
    RecentFiles::instance()->moveFilePath(index, 0, RecentFiles::Scene);
  }
} openRecentSceneFileCommandHandler;

//-----------------------------------------------------------------------------

class OpenRecentLevelFileCommandHandler final : public MenuItemHandler {
public:
  OpenRecentLevelFileCommandHandler() : MenuItemHandler(MI_OpenRecentLevel) {}
  void execute() override {
    QAction *act = CommandManager::instance()->getAction(MI_OpenRecentLevel);
    DVMenuAction *menu = dynamic_cast<DVMenuAction *>(act->menu());
    int index          = menu->getTriggeredActionIndex();
    QString path =
        RecentFiles::instance()->getFilePath(index, RecentFiles::Level);
    IoCmd::LoadResourceArguments args(TFilePath(path.toStdWString()));
    args.cachingBehavior = (IoCmd::LoadResourceArguments::CacheRasterBehavior)
                               Preferences::instance()
                                   ->getRasterLevelCachingBehavior();
    IoCmd::loadResources(args, false);

    RecentFiles::instance()->moveFilePath(index, 0, RecentFiles::Level);
    if (args.loadedLevels.empty()) {
      QString msg;
      msg = QObject::tr("It is not possible to load the %1 level.").arg(path);
      DVGui::error(msg);
    }
  }
} openRecentLevelFileCommandHandler;

//-----------------------------------------------------------------------------

class ClearRecentSceneFileListCommandHandler final : public MenuItemHandler {
public:
  ClearRecentSceneFileListCommandHandler()
      : MenuItemHandler(MI_ClearRecentScene) {}
  void execute() override {
    RecentFiles::instance()->clearRecentFilesList(RecentFiles::Scene);
  }
} clearRecentSceneFileListCommandHandler;

//-----------------------------------------------------------------------------

class ClearRecentLevelFileListCommandHandler final : public MenuItemHandler {
public:
  ClearRecentLevelFileListCommandHandler()
      : MenuItemHandler(MI_ClearRecentLevel) {}
  void execute() override {
    RecentFiles::instance()->clearRecentFilesList(RecentFiles::Level);
  }
} clearRecentLevelFileListCommandHandler;

//-----------------------------------------------------------------------------

class ClearRecentProjectListCommandHandler final : public MenuItemHandler {
public:
  ClearRecentProjectListCommandHandler()
      : MenuItemHandler(MI_ClearRecentProject) {}
  void execute() override {
    RecentFiles::instance()->clearRecentFilesList(RecentFiles::Project);
  }
} clearRecentProjectListCommandHandler;

//-----------------------------------------------------------------------------

class RevertScene final : public MenuItemHandler {
public:
  RevertScene() : MenuItemHandler(MI_RevertScene) {}
  void execute() override {
    TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
    ToonzScene *scene         = sceneHandle->getScene();
    assert(scene);
    TFilePath path       = scene->getScenePath();
    TFilePath decodePath = scene->decodeFilePath(scene->getScenePath());
    if (!TSystem::doesExistFileOrLevel(decodePath)) {
      DVGui::warning(QObject::tr("The scene %1 doesn't exist.")
                         .arg(toQString(decodePath)));
      return;
    }
    if (sceneHandle->getDirtyFlag()) {
      int ret = DVGui::MsgBox(
          QString(
              QObject::tr("Revert: the current scene has been modified.\nAre "
                          "you sure you want to revert to previous version?")),
          QString(QObject::tr("Revert")), QString(QObject::tr("Cancel")));
      if (ret == 2 || ret == 0) return;
    }
    IoCmd::loadScene(path, false, false);
  }
} RevertScene;

//=============================================================================
// Overwrite palette
//-----------------------------------------------------------------------------
class OverwritePaletteCommandHandler final : public MenuItemHandler {
public:
  OverwritePaletteCommandHandler() : MenuItemHandler(MI_OverwritePalette) {}

  void execute() override {
    TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
    if (!level) {
      DVGui::warning("No current level.");
      return;
    }
    TXshSimpleLevel *sl  = level->getSimpleLevel();
    TXshPaletteLevel *pl = level->getPaletteLevel();
    if (!sl && !pl) {
      DVGui::warning("Current level has no palette.");
      return;
    }
    /*-- SimpleLevel/PaletteLevelの場合毎にパレット/パスの取得の仕方を変える
     * --*/
    TPalette *palette;
    TFilePath palettePath;
    /*- SimpleLevelの場合 -*/
    if (sl) {
      palette = sl->getPalette();
      if (!palette) {
        DVGui::warning("No current palette");
        return;
      }
      if (sl->getPath().getType() == "pli")
        palettePath = sl->getPath();
      else if (sl->getType() & FULLCOLOR_TYPE)
        palettePath = FullColorPalette::instance()->getPath();
      else
        palettePath = sl->getPath().withType("tpl");
    }
    /*- PaletteLevelの場合 -*/
    else if (pl) {
      palette = pl->getPalette();
      if (!palette) {
        DVGui::warning("No current palette");
        return;
      }
      palettePath = pl->getPath();
    } else {
      DVGui::warning("This level is not SimpleLevel or PaletteLevel");
      return;
    }

    QString question;
    int ret;
    if (sl && sl->getPath().getType() == "pli") {
      question = "Saving " + toQString(palettePath) +
                 "\nThis command will overwrite the level data as well.  Are "
                 "you sure ?";
      ret =
          DVGui::MsgBox(question, QObject::tr("OK"), QObject::tr("Cancel"), 0);
    } else {
      question = "Do you want to overwrite current palette to " +
                 toQString(palettePath) + " ?";
      ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                          QObject::tr("Don't Overwrite"), 0);
    }
    if (ret == 2 || ret == 0) return;

    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!palettePath.isAbsolute() && scene) {
      palettePath = scene->decodeFilePath(palettePath);
    }

    if (sl && sl->getPath().getType() == "pli")
      sl->save(palettePath, TFilePath(), true);
    else if (sl && sl->getType() & FULLCOLOR_TYPE)
      FullColorPalette::instance()->savePalette(scene);
    else
      StudioPalette::instance()->save(palettePath, palette);
    /*- Dirtyフラグの変更 -*/
    if (sl)
      sl->getPalette()->setDirtyFlag(false);
    else if (pl)
      pl->getPalette()->setDirtyFlag(false);

    if (Preferences::instance()->getBoolValue(resetUndoOnSavingLevel))
      TUndoManager::manager()->reset();

    TApp::instance()
        ->getPaletteController()
        ->getCurrentLevelPalette()
        ->notifyPaletteDirtyFlagChanged();
  }
} overwritePaletteCommandHandler;

//=============================================================================
// Save Default palette
//-----------------------------------------------------------------------------
class SaveAsDefaultPaletteCommandHandler final : public MenuItemHandler {
public:
  SaveAsDefaultPaletteCommandHandler()
      : MenuItemHandler(MI_SaveAsDefaultPalette) {}

  void execute() override {
    TPalette *palette = TApp::instance()->getCurrentPalette()->getPalette();
    if (!palette) {
      DVGui::warning("No current palette");
      return;
    }

    int levelType = palette->getDefaultPaletteType();
    QString levelTypeStr, displayStr;

    if (levelType == UNKNOWN_XSHLEVEL) {
      TXshLevel *level = TApp::instance()->getCurrentLevel()->getLevel();
      if (level) {
        TXshSimpleLevel *sl = level->getSimpleLevel();
        if (!sl) {
          DVGui::warning("Current level is not a drawing level.");
          return;
        }
        levelType = sl->getType();
      } else
        levelType = Preferences::instance()->getDefLevelType();
    }

    switch (levelType) {
    case TZP_XSHLEVEL:
      levelTypeStr = "smart_raster";
      displayStr   = "Smart Raster";
      break;
    case PLI_XSHLEVEL:
      levelTypeStr = "vector";
      displayStr   = "Vector";
      break;
    case OVL_XSHLEVEL:
      levelTypeStr = "raster";
      displayStr   = "Raster";
      break;
    default:
      DVGui::warning(
          "This is not a Vector, Smart Raster or Raster level palette.");
      return;
    }

    TFilePath fp = ToonzFolder::getMyPalettesDir();
    if (!TFileStatus(fp).doesExist()) TSystem::mkDir(fp);

    TFilePath palettePath =
        fp + TFilePath(levelTypeStr.toStdString() + "_default.tpl");

    TFileStatus pfs(palettePath);
    if (pfs.doesExist()) {
      QString question;
      int ret;
      question = "A default " + displayStr +
                 " palette exists. Are you sure you want to overwrite with the "
                 "current?";
      ret = DVGui::MsgBox(question, QObject::tr("Overwrite"),
                          QObject::tr("Don't Overwrite"), 0);

      if (ret == 2 || ret == 0) return;
    }

    if (!palette->isDefaultPalette()) {
      palette = palette->clone();
      palette->setPaletteName(L"Default " + displayStr.toStdWString() +
                              L" Palette");
      palette->setIsDefaultPalette(true);
    }

    StudioPalette::instance()->save(palettePath, palette);
    TApp::instance()->getPaletteController()->setDefaultPalette(levelType,
                                                                palette);
  }
} saveDefaultPaletteCommandHandler;

//=============================================================================
// Save scene and levels
//-----------------------------------------------------------------------------
class SaveAllCommandHandler final : public MenuItemHandler {
public:
  SaveAllCommandHandler() : MenuItemHandler(MI_SaveAll) {}
  void execute() override { IoCmd::saveAll(); }
} saveAllCommandHandler;

//=============================================================================
// Save all levels
//-----------------------------------------------------------------------------
class SaveAllLevelsCommandHandler : public MenuItemHandler {
public:
  SaveAllLevelsCommandHandler() : MenuItemHandler(MI_SaveAllLevels) {}
  void execute() { IoCmd::saveNonSceneFiles(); }
} saveAllLevelsCommandHandler;
