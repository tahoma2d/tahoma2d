

#include "preproductionboard.h"

#include "floatingpanelcommand.h"
#include "menubarcommandids.h"
#include "tsystem.h"
#include "iocommand.h"
#include "batches.h"
#include "tapp.h"
#include "exportscenepopup.h"
#include "tlevel.h"
#include "filebrowser.h"
#include "filedata.h"
#include "tenv.h"
#include "flipbook.h"
#include "tfiletype.h"

#include "toonzqt/dvdialog.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/imageutils.h"
#include "toonzqt/infoviewer.h"
#include "toonzqt/dvmimedata.h"
#include "toonzqt/trepetitionguard.h"

#include "toonz/toonzscene.h"
#include "toonz/sceneresources.h"
#include "toonz/tscenehandle.h"
#include "toonz/tproject.h"

#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QDragLeaveEvent>
#include <QDesktopServices>
#include <QInputDialog>
#include <QClipboard>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QDrag>

using namespace DVGui;

namespace {

bool isAllowedType(std::string ext) {
  return ext == "tnz" || ext == "png" || ext == "tif" || ext == "tiff" ||
         ext == "bmp" || ext == "pli" || ext == "tlv" || ext == "jpg" ||
         ext == "gif" || ext == "mp4" || ext == "mov" || ext == "tnzbrd";
}

bool isExpandable(std::string ext) {
  return ext == "png" || ext == "tif" || ext == "tiff" || ext == "bmp";
}

void getFileFids(TFilePath path, std::vector<TFrameId> &fids) {
  QFileInfo info(QString::fromStdWString(path.getWideString()));
  TLevelP level;
  if (info.exists()) {
    if (path.getType() == "tnz") {
      try {
        ToonzScene scene;
        scene.loadNoResources(path);
        int i;
        for (i = 0; i < scene.getFrameCount(); i++)
          fids.push_back(TFrameId(i + 1));
      } catch (...) {
      }
    } else if (TFileType::isViewable(TFileType::getInfo(path))) {
      try {
        TLevelReaderP lr(path);
        level = lr->loadInfo();
      } catch (...) {
      }
    }
  } else if (path.isLevelName())  // for levels johndoe..tif etc.
  {
    try {
      TLevelReaderP lr(path);
      level = lr->loadInfo();
    } catch (...) {
    }
  }
  if (level.getPointer()) {
    for (TLevel::Iterator it = level->begin(); it != level->end(); ++it)
      fids.push_back(it->first);
  }
}

}  // namespace
//-----------------------------------------------------------------------------

BoardSelection::BoardSelection(BoardList *fileList)
    : m_fileList(fileList), m_exportScenePopup(0) {}

BoardSelection::BoardSelection(const BoardSelection &src)
    : m_infoViewers(src.m_infoViewers)
    , m_exportScenePopup(src.m_exportScenePopup)
    , m_fileList(src.m_fileList) {}

BoardSelection::~BoardSelection() { m_infoViewers.clear(); }

//------------------------------------------------------------------------

void BoardSelection::enableCommands() {
  enableCommand(this, MI_DuplicateFile, &BoardSelection::duplicateFiles);
  enableCommand(this, MI_Clear, &BoardSelection::deleteFiles);
  enableCommand(this, MI_ShowFolderContents,
                &BoardSelection::showFolderContents);
  enableCommand(this, MI_AddToBatchRenderList,
                &BoardSelection::addToBatchRenderList);
  enableCommand(this, MI_AddToBatchCleanupList,
                &BoardSelection::addToBatchCleanupList);
  enableCommand(this, MI_CollectAssets, &BoardSelection::collectAssets);
  enableCommand(this, MI_ImportScenes, &BoardSelection::importScenes);
  enableCommand(this, MI_ExportScenes, &BoardSelection::exportScenes);
  enableCommand(this, MI_SelectAll, &BoardSelection::selectAll);
  enableCommand(this, MI_FileInfo, &BoardSelection::viewFileInfo);
  enableCommand(this, MI_LoadScene, &BoardSelection::loadScene);
  enableCommand(this, MI_Cut, &BoardSelection::cutFiles);
  enableCommand(this, MI_Copy, &BoardSelection::copyFiles);
  enableCommand(this, MI_Paste, &BoardSelection::pasteFiles);
}

//------------------------------------------------------------------------

std::vector<TFilePath> BoardSelection::getSelection() const {
  std::vector<TFilePath> selection;

  foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
    selection.push_back(TFilePath(lwi->data(Qt::UserRole).toString()));
  }

  return selection;
}

//------------------------------------------------------------------------

bool BoardSelection::isEmpty() const {
  return !m_fileList || !m_fileList->selectedItems().size();
}

//------------------------------------------------------------------------

int BoardSelection::getSelectionCount() const {
  return !m_fileList ? 0 : m_fileList->selectedItems().count();
}

//------------------------------------------------------------------------

void BoardSelection::selectNone() {
  if (m_fileList) m_fileList->clearSelection();
}

//------------------------------------------------------------------------

void BoardSelection::addToBatchRenderList() {
  if (isEmpty()) return;

  if (!BatchesController::instance()->getTasksTree()) {
    QAction *taskPopup = CommandManager::instance()->getAction(MI_OpenTasks);
    taskPopup->trigger();
  }

  bool added = false;
  foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(
          QObject::tr(
              "File %1 does not exist. Cannot add to Batch Render List.")
              .arg(QString::fromStdWString(fp.getWideString())));
      continue;
    }

    if (fp.getType() != "tnz") continue;
    BatchesController::instance()->addComposerTask(fp);
    added = true;
  }

  if (added) DVGui::info(QObject::tr("Task added to the Batch Render List."));
}

//------------------------------------------------------------------------

void BoardSelection::addToBatchCleanupList() {
  if (isEmpty()) return;

  if (!BatchesController::instance()->getTasksTree()) {
    QAction *taskPopup = CommandManager::instance()->getAction(MI_OpenTasks);
    taskPopup->trigger();
  }

  bool added = false;
  foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(
          QObject::tr(
              "File %1 does not exist. Cannot add to Batch Cleanup List.")
              .arg(QString::fromStdWString(fp.getWideString())));
      continue;
    }

    if (fp.getType() != "tnz") continue;
    BatchesController::instance()->addCleanupTask(fp);
    added = true;
  }

  if (added) DVGui::info(QObject::tr("Task added to the Batch Cleanup List."));
}

//------------------------------------------------------------------------

void BoardSelection::loadScene() {
  if (getSelectionCount() != 1) return;

  QListWidgetItem *lwi = m_fileList->selectedItems()[0];
  TFilePath fp(lwi->data(Qt::UserRole).toString());

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(QObject::tr("File %1 does not exist. Cannot load scene.")
                     .arg(QString::fromStdWString(fp.getWideString())));
    return;
  }

  if (fp.getType() != "tnz") return;

  IoCmd::loadScene(fp);
}

//------------------------------------------------------------------------

void BoardSelection::deleteFiles() {
  if (isEmpty()) return;

  QString question;
  if (getSelectionCount() == 1) {
    QListWidgetItem *lwi = m_fileList->selectedItems()[0];
    QString fn           = lwi->data(Qt::UserRole).toString();
    question             = QObject::tr("Deleting %1. Are you sure?").arg(fn);
  } else {
    question = QObject::tr("Deleting %n files. Are you sure?", "",
                           getSelectionCount());
  }
  int ret =
      DVGui::MsgBox(question, QObject::tr("Delete"), QObject::tr("Cancel"), 1);
  if (ret == 2 || ret == 0) return;

  foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
    TFilePath fp(lwi->data(Qt::UserRole).toString());
    TSystem::moveFileOrLevelToRecycleBin(fp);
    IconGenerator::instance()->remove(fp);
    m_fileList->takeItem(m_fileList->row(lwi));
  }
  selectNone();
}

//------------------------------------------------------------------------

void BoardSelection::showFolderContents() {
  if (isEmpty()) return;

  QListWidgetItem *lwi = m_fileList->selectedItems()[0];
  TFilePath fp(lwi->data(Qt::UserRole).toString());

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(QObject::tr("File %1 does not exist. Cannot show in folder.")
                     .arg(QString::fromStdWString(fp.getWideString())));
    return;
  }

  TFilePath folderPath = fp.getParentDir();
  if (folderPath.isEmpty()) return;

  if (TSystem::isUNC(folderPath)) {
    bool ok = QDesktopServices::openUrl(
        QUrl(QString::fromStdWString(folderPath.getWideString())));
    if (ok) return;
    // If the above fails, then try opening UNC path with the same way as the
    // local files.. QUrl::fromLocalFile() seems to work for UNC path as well in
    // our environment. (8/10/2021 shun-iwasawa)
  }
  QDesktopServices::openUrl(
      QUrl::fromLocalFile(QString::fromStdWString(folderPath.getWideString())));
}

//------------------------------------------------------------------------

void BoardSelection::viewFileInfo() {
  if (isEmpty()) return;

  foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(QObject::tr("File %1 does not exist. Cannot show file info.")
                       .arg(QString::fromStdWString(fp.getWideString())));
      continue;
    }

    InfoViewer *infoViewer = 0;
    int i;
    for (i = 0; i < m_infoViewers.size(); i++) {
      InfoViewer *v = m_infoViewers.at(i);
      if (!v->isHidden()) continue;
      infoViewer = v;
      break;
    }
    if (!infoViewer) {
      infoViewer = new InfoViewer();
      m_infoViewers.append(infoViewer);
    }
    FileBrowserPopup::setModalBrowserToParent(infoViewer);
    infoViewer->setItem(0, 0, fp);
  }
}

//------------------------------------------------------------------------

void BoardSelection::duplicateFiles() {
  if (isEmpty()) return;

  foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(QObject::tr("File %1 does not exist. Cannot duplicate it.")
                       .arg(QString::fromStdWString(fp.getWideString())));
      continue;
    }

    TFilePath newPath = ImageUtils::duplicate(fp);
    QString name =
        QString::fromStdWString(newPath.withoutParentDir().getWideString());
    QString path = newPath.getQString();
    int newIndex = m_fileList->row(lwi) + 1;
    m_fileList->addFile(name, path, newIndex);
  }
}

//------------------------------------------------------------------------

static int collectAssets(TFilePath scenePath) {
  ToonzScene scene;
  scene.load(scenePath);
  ResourceCollector collector(&scene);
  SceneResources resources(&scene, scene.getXsheet());
  resources.accept(&collector);
  int count = collector.getCollectedResourceCount();
  if (count > 0) {
    scene.save(scenePath);
  }
  return count;
}

//------------------------------------------------------------------------

void BoardSelection::collectAssets() {
  if (isEmpty()) return;

  int collectedAssets = 0;

  int count = getSelectionCount();

  if (count > 1) {
    ProgressDialog progress(QObject::tr("Collecting assets..."),
                            QObject::tr("Abort"), 0, count);
    progress.setWindowModality(Qt::WindowModal);

    int i = 1;
    foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
      TFilePath fp(lwi->data(Qt::UserRole).toString());

      if (!TSystem::doesExistFileOrLevel(fp)) {
        DVGui::error(
            QObject::tr("File %1 does not exist. Cannot collect assets for it.")
                .arg(QString::fromStdWString(fp.getWideString())));
        continue;
      }

      if (fp.getType() == "tnz") collectedAssets += ::collectAssets(fp);
      progress.setValue(i++);
      if (progress.wasCanceled()) break;
    }
    progress.setValue(count);
  } else {
    QListWidgetItem *lwi = m_fileList->selectedItems()[0];
    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(
          QObject::tr("File %1 does not exist. Cannot collect assets for it.")
              .arg(QString::fromStdWString(fp.getWideString())));
      return;
    }

    if (fp.getType() == "tnz") collectedAssets += ::collectAssets(fp);
  }
  if (collectedAssets == 0)
    DVGui::info(QObject::tr("There are no assets to collect"));
  else if (collectedAssets == 1)
    DVGui::info(QObject::tr("One asset imported"));
  else
    DVGui::info(QObject::tr("%1 assets imported").arg(collectedAssets));
}

//------------------------------------------------------------------------

static TFilePath importScene(TFilePath scenePath) {
  ToonzScene scene;
  try {
    IoCmd::loadScene(scene, scenePath, true);
  } catch (TException &e) {
    DVGui::error(QObject::tr("Error loading scene %1 :%2")
                     .arg(toQString(scenePath))
                     .arg(QString::fromStdWString(e.getMessage())));
    return TFilePath();
  } catch (...) {
    DVGui::error(
        QObject::tr("Error loading scene %1").arg(toQString(scenePath)));
    return TFilePath();
  }

  try {
    scene.save(scene.getScenePath());
  } catch (TException &) {
    DVGui::error(QObject::tr("There was an error saving the %1 scene.")
                     .arg(toQString(scenePath)));
    return TFilePath();
  } catch (...) {
    DVGui::error(QObject::tr("There was an error saving the %1 scene.")
                     .arg(toQString(scenePath)));
    return TFilePath();
  }

  return scene.getScenePath();
}

//------------------------------------------------------------------------

void BoardSelection::importScenes() {
  if (isEmpty()) return;

  int importedSceneCount = 0;

  int count = getSelectionCount();

  if (count > 1) {
    ProgressDialog progress(QObject::tr("Importing scenes..."),
                            QObject::tr("Abort"), 0, count);
    progress.setWindowModality(Qt::WindowModal);

    int i = 1;
    foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
      TFilePath fp(lwi->data(Qt::UserRole).toString());

      if (!TSystem::doesExistFileOrLevel(fp)) {
        DVGui::error(
            QObject::tr(
                "File %1 does not exist. Cannot import into current project.")
                .arg(QString::fromStdWString(fp.getWideString())));
        continue;
      }

      if (fp.getType() == "tnz") {
        TFilePath newFp = ::importScene(fp);
        if (!newFp.isEmpty()) {
          lwi->setData(Qt::UserRole, newFp.getQString());
          importedSceneCount++;
        }
      }
      progress.setValue(i++);
      if (progress.wasCanceled()) break;
    }
    progress.setValue(count);
  } else {
    QListWidgetItem *lwi = m_fileList->selectedItems()[0];
    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(
          QObject::tr(
              "File %1 does not exist. Cannot import into current project.")
              .arg(QString::fromStdWString(fp.getWideString())));
      return;
    }

    if (fp.getType() == "tnz") {
      TFilePath newFp = ::importScene(fp);
      if (!newFp.isEmpty()) {
        lwi->setData(Qt::UserRole, newFp.getQString());
        importedSceneCount++;
      }
    }
  }
  if (importedSceneCount == 0)
    DVGui::info(QObject::tr("No scene imported"));
  else if (importedSceneCount == 1)
    DVGui::info(QObject::tr("One scene imported"));
  else
    DVGui::info(QString::number(importedSceneCount) +
                QObject::tr("%1 scenes imported").arg(importedSceneCount));
}

//------------------------------------------------------------------------

void BoardSelection::exportScenes() {
  if (isEmpty()) return;

  if (!m_exportScenePopup)
    m_exportScenePopup = new ExportScenePopup(getSelection());
  else
    m_exportScenePopup->setScenes(getSelection());
  m_exportScenePopup->show();
}

//------------------------------------------------------------------------

void BoardSelection::exportScene(TFilePath scenePath) {
  if (scenePath.isEmpty()) return;

  std::vector<TFilePath> files;
  files.push_back(scenePath);
  if (!m_exportScenePopup)
    m_exportScenePopup = new ExportScenePopup(files);
  else
    m_exportScenePopup->setScenes(files);
  m_exportScenePopup->show();
}

//------------------------------------------------------------------------

void BoardSelection::selectAll() {
  if (!m_fileList) return;

  m_fileList->selectAll();
}

//------------------------------------------------------------------------

void BoardSelection::cutFiles() {
  if (isEmpty()) return;

  foreach (QListWidgetItem *lwi, m_fileList->selectedItems()) {
    m_fileList->takeItem(m_fileList->row(lwi));
  }
}

//------------------------------------------------------------------------

void BoardSelection::copyFiles() {
  if (!getSelectionCount()) return;

  QClipboard *clipboard            = QApplication::clipboard();
  FileData *data                   = new FileData();
  std::vector<TFilePath> selection = getSelection();
  data->setFiles(selection);
  QApplication::clipboard()->setMimeData(data);
  QMimeData *newData = cloneData(clipboard->mimeData());
}

//------------------------------------------------------------------------

void BoardSelection::pasteFiles() {
  if (!m_fileList) return;

  const FileData *data =
      dynamic_cast<const FileData *>(QApplication::clipboard()->mimeData());
  if (!data || !data->m_files.size()) return;

  int dropIndex = m_fileList->getClickedIndex();
  foreach (TFilePath fp, data->m_files) {
    if (!isAllowedType(fp.getType())) continue;
    QString name =
        QString::fromStdWString(fp.withoutParentDir().getWideString());
    QString path = fp.getQString();
    m_fileList->addFile(name, path, dropIndex);
  }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

BoardButtonBar::BoardButtonBar(QWidget *parent, BoardList *fileList)
    : QToolBar(parent) {
  setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  setIconSize(QSize(17, 17));
  setObjectName("buttonBar");

  QIcon newBoardIcon = createQIcon("new_board");
  QAction *newAction = new QAction(newBoardIcon, tr("Create Board"), this);
  newAction->setIconText(tr("New"));
  addAction(newAction);

  QIcon resetBoardIcon = createQIcon("reset_board");
  QAction *resetAction = new QAction(resetBoardIcon, tr("Reset Board"), this);
  resetAction->setIconText(tr("Reset"));
  addAction(resetAction);

  QIcon clearIcon      = createQIcon("clear");
  QAction *clearAction = new QAction(clearIcon, tr("Clear Board"), this);
  clearAction->setIconText(tr("Clear"));
  addAction(clearAction);

  addSeparator();

  QIcon newSceneIcon = createQIcon("new_scene");
  QAction *newScene  = new QAction(newSceneIcon, tr("Create New Scene"), this);
  newScene->setIconText(tr("Create Scene"));
  addAction(newScene);

  addSeparator();

  QIcon saveIcon      = createQIcon("save");
  QAction *saveAction = new QAction(saveIcon, tr("Save Board"), this);
  saveAction->setIconText(tr("Save"));
  addAction(saveAction);

  QIcon saveAsIcon      = createQIcon("saveas");
  QAction *saveAsAction = new QAction(saveAsIcon, tr("Save Board As"), this);
  saveAsAction->setIconText(tr("Save As"));
  addAction(saveAsAction);

  QIcon loadIcon      = createQIcon("load");
  QAction *loadAction = new QAction(loadIcon, tr("Load Board"), this);
  loadAction->setIconText(tr("Load"));
  addAction(loadAction);

  addSeparator();

  QIcon playIcon      = createQIcon("play");
  QAction *playAction = new QAction(playIcon, tr("Quick Play"), this);
  playAction->setIconText(tr("Quick Play"));
  addAction(playAction);

  connect(newAction, SIGNAL(triggered()), SIGNAL(newBoard()));
  connect(resetAction, SIGNAL(triggered()), SIGNAL(resetBoard()));
  connect(clearAction, SIGNAL(triggered()), SIGNAL(clearBoard()));
  connect(newScene, SIGNAL(triggered()), SIGNAL(newScene()));
  connect(saveAction, SIGNAL(triggered()), SIGNAL(saveBoard()));
  connect(saveAsAction, SIGNAL(triggered()), SIGNAL(saveAsBoard()));
  connect(loadAction, SIGNAL(triggered()), this, SLOT(onLoadBoard()));
  connect(playAction, SIGNAL(triggered()), SIGNAL(playBoard()));
}

//-----------------------------------------------------------------------------

void BoardButtonBar::onLoadBoard() { emit loadBoard(TFilePath()); }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

BoardList::BoardList(QWidget *parent, const QSize &iconSize)
    : QListWidget(parent)
    , m_clickedIndex(-1)
    , m_startDrag(false)
    , m_iconSize(iconSize)
    , m_maxWidgetSize(iconSize) {
  setObjectName("BoardList");

  setFlow(QListView::Flow::LeftToRight);

  setViewMode(QListWidget::IconMode);
  setResizeMode(QListView::Adjust);
  setWrapping(true);

  setMovement(QListWidget::Free);

  setSelectionMode(QAbstractItemView::ExtendedSelection);

  setDragDropMode(QAbstractItemView::DragDrop);
  setDefaultDropAction(Qt::MoveAction);
  setDragEnabled(true);
  setAcceptDrops(true);
  setDropIndicatorShown(true);

  setIconSize(m_iconSize);
  setUniformItemSizes(true);

  setMouseTracking(true);

  horizontalScrollBar()->setCursor(Qt::ArrowCursor);
  verticalScrollBar()->setCursor(Qt::ArrowCursor);

  m_selection = new BoardSelection(this);

  bool ret = connect(this, SIGNAL(itemClicked(QListWidgetItem *)), this,
                     SLOT(onItemClicked(QListWidgetItem *)));
  ret = ret && connect(this, SIGNAL(itemDoubleClicked(QListWidgetItem *)), this,
                       SLOT(onItemDoubleClicked(QListWidgetItem *)));
  ret = ret && connect(selectionModel(), &QItemSelectionModel::selectionChanged,
                       this, &BoardList::onSelectionChanged);

  ret = ret && connect(&m_frameReader, SIGNAL(calculatedFrameCount()), this,
                       SLOT(onFramesCountsUpdated()));
  ret = ret && connect(IconGenerator::instance(), SIGNAL(iconGenerated()), this,
                       SLOT(onIconGenerated()));
  assert(ret);

  clear();
}

//-----------------------------------------------------------------------------

BoardList::~BoardList() {}

//-----------------------------------------------------------------------------

QPixmap BoardList::createThumbnail(const TFilePath &fp) {
  QPixmap source;
  QPixmap pixmap(m_iconSize);

  bool isMissing = false;
  if (!TSystem::doesExistFileOrLevel(fp)) {
    static QImage broken(generateIconImage("broken_icon", qreal(1.0),
                                           m_iconSize, Qt::KeepAspectRatio));

    source    = QPixmap().fromImage(broken);
    isMissing = true;
  } else if (fp.getType() != "tnz") {
    if (fp.isLevelName() || !isExpandable(fp.getType())) {
      std::string id = "$:TNZBRD:" + fp.getQString().toStdString();
      source = IconGenerator::instance()->getSizedIcon(fp, TFrameId::NO_FRAME,
                                                       m_iconSize, id);
      // always delayed the 1st time generating. Return empty pixmap to be
      // handled later
      if (source.isNull()) return QPixmap();
    } else if (TFileStatus(fp).doesExist())
      source = QPixmap(fp.getQString());
  } else {
    TFilePath iconPath = ToonzScene::getIconPath(fp);
    if (TFileStatus(iconPath).doesExist())
      source = QPixmap(iconPath.getQString());
  }

  if (isMissing)
    pixmap.fill(Qt::transparent);
  else
    pixmap.fill(Qt::white);
  if (!source.isNull()) {
    QPainter painter(&pixmap);
    QPixmap scaledPixmap =
        source.scaled(m_iconSize, Qt::AspectRatioMode::KeepAspectRatio);
    painter.drawPixmap((m_iconSize.width() - scaledPixmap.width()) / 2,
                       (m_iconSize.height() - scaledPixmap.height()) / 2,
                       scaledPixmap);
    QPen pen(isMissing ? Qt::red : palette().text().color());
    pen.setStyle(Qt::DotLine);
    painter.setPen(pen);
    painter.drawRect((m_iconSize.width() - scaledPixmap.width()) / 2,
                     (m_iconSize.height() - scaledPixmap.height()) / 2,
                     scaledPixmap.width() - 1, scaledPixmap.height() - 1);
  }
  return pixmap;
}

//-----------------------------------------------------------------------------

void BoardList::toggleIconWrap() {
  if (resizeMode() == QListView::Adjust) {
    setResizeMode(QListView::Fixed);
    setWrapping(false);
  } else {
    setResizeMode(QListView::Adjust);
    setWrapping(true);
  }
}

//-----------------------------------------------------------------------------

// We'll custom wrap text since setWordWrap(true) causes text to be elided.
QString wrapText(const QString &text, const QFont &font, int maxWidthPixels) {
  QFontMetrics fm(font);
  QString wrappedText;
  QString currentLine;

  for (int i = 0; i < text.length(); ++i) {
    QChar character        = text.at(i);
    QString nextCharString = QString(character);

    if (fm.horizontalAdvance(currentLine + nextCharString) <= maxWidthPixels) {
      currentLine += nextCharString;
    } else {
      wrappedText += currentLine + "\n";
      currentLine = nextCharString;
    }
  }
  wrappedText += currentLine;

  return wrappedText;
}

void BoardList::addFile(QListWidgetItem *lwi, int atIndex) {
  if (!lwi) return;

  if (!count()) m_maxWidgetSize = m_iconSize;

  if (atIndex < 0)
    addItem(lwi);
  else
    insertItem(atIndex, lwi);

  // Size of uniform icon size is based on last element. Let's force last items
  // to the largest one
  QStyledItemDelegate *delegate =
      qobject_cast<QStyledItemDelegate *>(itemDelegate());
  QModelIndex idx = model()->index(row(lwi), 0);
  QSize iconSize  = delegate->sizeHint(viewOptions(), idx);
  if (iconSize != m_maxWidgetSize) {
    m_maxWidgetSize = m_maxWidgetSize.expandedTo(iconSize);
    item(count() - 1)->setSizeHint(m_maxWidgetSize);
  }
}

void BoardList::addFile(const QString &name, const QString &path, int atIndex) {
  TFilePath fp(path);
  QPixmap pixmap = createThumbnail(fp);
  bool hasPixmap = !pixmap.isNull();

  QString itemName = name;
  int frames       = 1;

  frames = m_frameReader.getFrameCount(fp);

  itemName = wrapText(itemName, font(), m_iconSize.width());

  if (frames >= 0) itemName += QString("\n[%1]").arg(frames);

  QListWidgetItem *lwi = new QListWidgetItem(itemName);
  lwi->setData(Qt::UserRole, path);
  lwi->setToolTip(name);

  if (hasPixmap) {
    QIcon icon(pixmap);
    lwi->setIcon(icon);
  }

  addFile(lwi, atIndex);
}

//-----------------------------------------------------------------------------

void BoardList::mousePressEvent(QMouseEvent *event) {
  m_clickedIndex = row(itemAt(event->pos()));

  if (m_clickedIndex < 0) {
    clearSelection();
    return;
  }

  m_startDrag         = count() ? true : false;
  m_dragStartPosition = event->pos();

  QListWidget::mousePressEvent(event);
  m_selection->makeCurrent();
}

//-----------------------------------------------------------------------------

void BoardList::mouseMoveEvent(QMouseEvent *event) {
  if (m_startDrag &&
      (event->pos() - m_dragStartPosition).manhattanLength() > 20) {
    startDragDrop();
    m_startDrag = false;
  }
}

//-----------------------------------------------------------------------------

void BoardList::mouseReleaseEvent(QMouseEvent *event) { m_startDrag = false; }

//-----------------------------------------------------------------------------

void BoardList::dragEnterEvent(QDragEnterEvent *event) {
  if (event->source() == this) {
    QListWidget::dragEnterEvent(event);
    event->acceptProposedAction();
    return;
  }

  const QMimeData *mimeData = event->mimeData();
  for (const QUrl &url : mimeData->urls()) {
    TFilePath fp(url.toLocalFile().toStdWString());
    if (!isAllowedType(fp.getType())) return;
  }

  event->setDropAction(Qt::CopyAction);
  event->accept();
}

//-----------------------------------------------------------------------------

void BoardList::dragMoveEvent(QDragMoveEvent *event) {
  QListWidget::dragMoveEvent(event);
  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void BoardList::dropEvent(QDropEvent *event) {
  int dropIndex = row(itemAt(event->pos()));

  if (event->source() == this) {
    if (dropIndex == m_clickedIndex) return;  // We didn't move

    // Bug in QT 5.15.2 drops QListWidgetItem in unexpected places. This forces
    // it to the right place.
    foreach (QListWidgetItem *lwi, selectedItems()) {
      addFile(lwi->clone(), dropIndex++);
    }

    if (event->dropAction() == Qt::MoveAction) {
      foreach (QListWidgetItem *lwi, selectedItems()) {
        takeItem(row(lwi));
      }
    }
    event->acceptProposedAction();
    return;
  }

  const QMimeData *mimeData = event->mimeData();
  for (const QUrl &url : mimeData->urls()) {
    TFilePath fp(url.toLocalFile().toStdWString());
    if (!isAllowedType(fp.getType())) continue;
    if (fp.getType() == "tnzbrd") {
      emit loadBoard(fp);
      return;
    }
    QString name =
        QString::fromStdWString(fp.withoutParentDir().getWideString());
    QString path = fp.getQString();
    addFile(name, path, dropIndex++);
  }

  event->acceptProposedAction();
}

//-----------------------------------------------------------------------------

void BoardList::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  CommandManager *cm = CommandManager::instance();

  QAction *toggleIconWrapAction = new QAction(
      QIcon(createQIcon("wrap")), tr("Toggle Thumbnail Wrap"), &menu);
  bool ret = connect(toggleIconWrapAction, SIGNAL(triggered()), this,
                     SLOT(onToggleIconWrap()));

  QAction *playAction =
      new QAction(QIcon(createQIcon("play")), tr("Quick Play"), &menu);
  ret = ret &&
        connect(playAction, SIGNAL(triggered()), this, SLOT(onQuickPlay()));

  int itemCount     = count();
  int selectedCount = selectedItems().size();

  if (!selectedCount) {
    bool addSeparator = false;
    const FileData *data =
        dynamic_cast<const FileData *>(QApplication::clipboard()->mimeData());
    if (data && data->m_files.size()) {
      menu.addAction(cm->getAction(MI_Paste));
      addSeparator = true;
    }
    if (itemCount) {
      menu.addAction(playAction);
      menu.addAction(cm->getAction(MI_SelectAll));
      addSeparator = true;
    }

    if (addSeparator) menu.addSeparator();

    menu.addAction(toggleIconWrapAction);

    menu.exec(event->globalPos());
    return;
  }

  std::string ext;
  bool hasScene     = false;
  bool isOnlyScenes = true;
  TFilePath fp;
  foreach (QListWidgetItem *lwi, selectedItems()) {
    fp  = TFilePath(lwi->data(Qt::UserRole).toString());
    ext = fp.getType();

    if (ext != "tnz") isOnlyScenes = false;
    if (ext == "tnz") hasScene = true;
  }

  if (isOnlyScenes && selectedCount == 1)
    menu.addAction(cm->getAction(MI_LoadScene));

  QAction *action =
      new QAction(QIcon(createQIcon("import")), tr("Load As Sub-Scene"), &menu);
  ret = ret &&
        connect(action, SIGNAL(triggered()), this, SLOT(onLoadResources()));
  menu.addAction(action);

  menu.addSeparator();

  menu.addAction(cm->getAction(MI_Cut));
  menu.addAction(cm->getAction(MI_Copy));
  menu.addAction(cm->getAction(MI_Paste));

  if (selectedCount == 1 && isExpandable(ext)) {
    int frameCount = m_frameReader.getFrameCount(fp);
    if (fp.isLevelName() && frameCount > 1) {
      action = new QAction(QIcon(createQIcon("expand")), tr("Expand Sequence"),
                           &menu);
      ret    = ret &&
            connect(action, SIGNAL(triggered()), this, SLOT(onExpandSequnce()));
      menu.addAction(action);
    } else if (fp.withoutParentDir().getWideString() !=
               fp.withoutParentDir().getLevelNameW()) {
      action = new QAction(QIcon(createQIcon("collapse")),
                           tr("Collapse Sequence"), &menu);
      ret    = ret && connect(action, SIGNAL(triggered()), this,
                              SLOT(onCollapseSequnce()));
      menu.addAction(action);
    }
  }
  menu.addSeparator();

  if (selectedCount == 1) {
    action = new QAction(QIcon(createQIcon("rename")), tr("Rename"), &menu);
    ret =
        ret && connect(action, SIGNAL(triggered()), this, SLOT(onRenameFile()));
    menu.addAction(action);
  }
  menu.addAction(cm->getAction(MI_DuplicateFile));
  menu.addAction(cm->getAction(MI_Clear));

  if (selectedCount == 1) menu.addAction(cm->getAction(MI_ShowFolderContents));
  menu.addAction(cm->getAction(MI_SelectAll));
  menu.addAction(cm->getAction(MI_FileInfo));

  if (isOnlyScenes) {
    menu.addSeparator();

    menu.addAction(cm->getAction(MI_AddToBatchRenderList));
    menu.addAction(cm->getAction(MI_AddToBatchCleanupList));

    if (m_clickedIndex >= 0) {
      menu.addSeparator();
      menu.addAction(cm->getAction(MI_CollectAssets));
      menu.addAction(cm->getAction(MI_ImportScenes));
      menu.addAction(cm->getAction(MI_ExportScenes));
    }
  }

  menu.addSeparator();
  menu.addAction(playAction);

  menu.addSeparator();
  menu.addAction(toggleIconWrapAction);

  menu.exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void BoardList::paintEvent(QPaintEvent *event) {
  QListWidget::paintEvent(event);

  if (count() == 0) {
    QPainter painter(viewport());

    QColor placeholderColor = palette().placeholderText().color();
    painter.setPen(placeholderColor);
    painter.drawText(viewport()->rect(),
                     Qt::AlignCenter |
                         Qt::TextWordWrap,  // Align center, wrap text if needed
                     tr("Drag/Drop to Add Files"));
  }
}

//-----------------------------------------------------------------------------

void BoardList::onItemClicked(QListWidgetItem *lwi) {
  emit itemClicked(m_clickedIndex);
}

//-----------------------------------------------------------------------------

void BoardList::onItemDoubleClicked(QListWidgetItem *lwi) {
  TFilePath fp(lwi->data(Qt::UserRole).toString());
  if (fp.getType() == "tnz")
    IoCmd::loadScene(fp, true, true);
  else
    FlipBook *fb = ::viewFile(fp);
}

//-----------------------------------------------------------------------------

void BoardList::onLoadResources() {
  if (m_selection->isEmpty()) return;

  std::vector<TFilePath> filePaths = m_selection->getSelection();

  std::vector<TFilePath> paths;
  foreach (TFilePath fp, filePaths) {
    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(
          QObject::tr(
              "File %1 does not exist. Cannot load it into the current scene.")
              .arg(QString::fromStdWString(fp.getWideString())));
      continue;
    }
    paths.push_back(fp);
  }

  if (paths.empty()) return;

  IoCmd::LoadResourceArguments args;
  args.resourceDatas.assign(paths.begin(), paths.end());

  IoCmd::loadResources(args);
}

//-----------------------------------------------------------------------------

void BoardList::onToggleIconWrap() {
  toggleIconWrap();

  emit toggleIconWrapChanged(isWrapping());
}

//-----------------------------------------------------------------------------

void BoardList::onSelectionChanged(const QItemSelection &selected,
                                   const QItemSelection &deselected) {
  m_selection->makeCurrent();
}

//-------------------------------------------------------------------------------

void BoardList::onRenameFile() {
  if (!count()) return;

  QListWidgetItem *lwi = selectedItems()[0];
  TFilePath fp(lwi->data(Qt::UserRole).toString());

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(QObject::tr("File %1 does not exist. Cannot rename it.")
                     .arg(QString::fromStdWString(fp.getWideString())));
    return;
  }

  QString oldName = QString::fromStdWString(
      fp.withoutParentDir().withType("").getWideString());
  while (oldName.endsWith('_') || oldName.endsWith('.') ||
         oldName.endsWith(' '))
    oldName.chop(1);

  RenameAsToonzPopup popup(oldName, -1);

  if (popup.exec() != QDialog::Accepted) return;

  std::string name = popup.getName().toStdString();

  TFilePath newFile = fp.withName(name);

  if (TSystem::doesExistFileOrLevel(newFile)) {
    int ret = DVGui::MsgBox(
        QObject::tr("Warning: file or level %1 already exists; overwrite?")
            .arg(toQString(newFile)),
        QObject::tr("Yes"), QObject::tr("No"), 1);
    if (ret == 2 || ret == 0) return;
    TSystem::removeFileOrLevel(newFile);
  }

  QString newName =
      QString::fromStdWString(newFile.withoutParentDir().getWideString());
  QString newPath = newFile.getQString();

  if (!popup.doCreatecopy()) {
    TSystem::renameFileOrLevel(newFile, fp, true);
    int frames = m_frameReader.getFrameCount(TFilePath(newPath));
    lwi->setText(QString("%1\n[%2]").arg(newName).arg(frames));
    lwi->setData(Qt::UserRole, newPath);
  } else {
    TSystem::copyFileOrLevel(newFile, fp);
    addFile(newName, newPath, row(lwi) + 1);
  }
}

//-------------------------------------------------------------------------------

void BoardList::onFramesCountsUpdated() {
  for (int i = 0; i < count(); i++) {
    QListWidgetItem *lwi = item(i);

    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (fp.getType() == "tnz") continue;

    int frames = m_frameReader.getFrameCount(fp);
    if (frames <= 0) continue;

    QString name =
        QString::fromStdWString(fp.withoutParentDir().getWideString());
    blockSignals(true);
    lwi->setText(QString("%1\n[%2]").arg(name).arg(frames));
    blockSignals(false);
  }
}

//-------------------------------------------------------------------------------

void BoardList::onIconGenerated() {
  for (int i = 0; i < count(); i++) {
    QListWidgetItem *lwi = item(i);

    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (fp.getType() == "tnz") continue;

    if (!lwi->icon().isNull()) continue;

    QPixmap pixmap = this->createThumbnail(fp);
    if (pixmap.isNull()) continue;

    QIcon icon(pixmap);
    blockSignals(true);
    lwi->setIcon(icon);
    blockSignals(false);
  }
}

//-------------------------------------------------------------------------------

void BoardList::onExpandSequnce() {
  if (m_clickedIndex < 0) return;

  QListWidgetItem *w = item(m_clickedIndex);
  TFilePath fp(w->data(Qt::UserRole).toString());

  if (!isExpandable(fp.getType())) return;

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(QObject::tr("File %1 does not exist. Cannot expand it.")
                     .arg(QString::fromStdWString(fp.getWideString())));
    return;
  }

  QString filter = QString::fromStdString(
      fp.getName() + (fp.getSepChar() == "." ? ".*." : "_*.") + fp.getType());
  QDir dir(fp.getParentDir().getQString(), filter);
  QStringList list = dir.entryList();

  takeItem(m_clickedIndex);
  for (int i = 0; i < list.size(); i++) {
    TFilePath f  = fp.getParentDir() + TFilePath(list.at(i));
    QString name = list.at(i);
    QString path = f.getQString();
    addFile(name, path, m_clickedIndex + i);
  }
}

//-------------------------------------------------------------------------------

void BoardList::onCollapseSequnce() {
  if (m_clickedIndex < 0) return;

  QListWidgetItem *w = item(m_clickedIndex);
  TFilePath fp(w->data(Qt::UserRole).toString());

  if (!isExpandable(fp.getType())) return;

  for (int i = count() - 1; i >= 0; i--) {
    TFilePath f(item(i)->data(Qt::UserRole).toString());

    if (f.withFrame().getQString() != fp.withFrame().getQString()) continue;
    takeItem(i);
    if (i == m_clickedIndex) {
      QString name = QString::fromStdString(fp.getLevelName());
      QString path = fp.withFrame().getQString();
      addFile(name, path, m_clickedIndex);
    }
  }
}

//-------------------------------------------------------------------------------

void BoardList::onQuickPlay() { emit playBoard(); }

//-----------------------------------------------------------------------------

void BoardList::startDragDrop() {
  TRepetitionGuard guard;
  if (!guard.hasLock()) return;

  bool firstFile = true;
  QPixmap icon;
  QList<QUrl> urls;
  foreach (QListWidgetItem *lwi, selectedItems()) {
    TFilePath fp(lwi->data(Qt::UserRole).toString());
    if (TSystem::doesExistFileOrLevel(fp)) {
      urls.append(
          QUrl::fromLocalFile(QString::fromStdWString(fp.getWideString())));
      if (!firstFile) continue;
      firstFile = false;
      icon      = lwi->icon().pixmap(m_iconSize);
    }
  }
  if (urls.isEmpty()) return;

  QMimeData *mimeData = new QMimeData;
  mimeData->setUrls(urls);

  QDrag *drag = new QDrag(this);
  QPixmap dropThumbnail =
      scalePixmapKeepingAspectRatio(icon, m_iconSize, Qt::transparent);
  if (!dropThumbnail.isNull()) drag->setPixmap(dropThumbnail);
  drag->setMimeData(mimeData);

  Qt::DropAction dropAction = drag->exec(Qt::CopyAction | Qt::MoveAction);
}

//=============================================================================
// PreproductionBoard
//-----------------------------------------------------------------------------

PreproductionBoard::PreproductionBoard(QWidget *parent, Qt::WindowFlags flags,
                                       bool noContextMenu,
                                       bool multiSelectionEnabled)
    : QFrame(parent), m_iconsWrapped(false), m_dirtyFlag(false) {
  // style sheet
  setObjectName("PreproductionBoard");
  setFrameStyle(QFrame::StyledPanel);

  BoardButtonBar *buttonBar = new BoardButtonBar(this, m_fileList);

  TDimension iconSize = Preferences::instance()->getIconSizePB();
  m_fileList          = new BoardList(this, QSize(iconSize.lx, iconSize.ly));

  // layout
  QVBoxLayout *mainLayout = new QVBoxLayout();
  mainLayout->setContentsMargins(3, 3, 3, 3);
  mainLayout->setSpacing(2);
  {
    mainLayout->addWidget(buttonBar);
    mainLayout->addWidget(m_fileList, 1);
  }
  setLayout(mainLayout);

  bool ret = connect(buttonBar, SIGNAL(newBoard()), this, SLOT(onNewBoard()));
  ret      = ret &&
        connect(buttonBar, SIGNAL(resetBoard()), this, SLOT(onResetBoard()));
  ret = ret &&
        connect(buttonBar, SIGNAL(clearBoard()), this, SLOT(onClearBoard()));
  ret = ret && connect(buttonBar, SIGNAL(newScene()), this, SLOT(onNewScene()));
  ret =
      ret && connect(buttonBar, SIGNAL(saveBoard()), this, SLOT(onSaveBoard()));
  ret = ret &&
        connect(buttonBar, SIGNAL(saveAsBoard()), this, SLOT(onSaveAsBoard()));
  ret = ret && connect(buttonBar, SIGNAL(loadBoard(TFilePath)), this,
                       SLOT(onLoadBoard(TFilePath)));
  ret =
      ret && connect(buttonBar, SIGNAL(playBoard()), this, SLOT(onPlayBoard()));

  ret = ret && connect(m_fileList, SIGNAL(loadBoard(TFilePath)), this,
                       SLOT(onLoadBoard(TFilePath)));
  ret = ret &&
        connect(m_fileList, SIGNAL(playBoard()), this, SLOT(onPlayBoard()));
  ret = ret && connect(m_fileList, &BoardList::toggleIconWrapChanged,
                       [=](bool wrapped) { m_iconsWrapped = wrapped; });
  ret = ret && connect(m_fileList, SIGNAL(itemChanged(QListWidgetItem *)), this,
                       SLOT(onListChanged()));
  QAbstractItemModel *model = m_fileList->model();
  ret = ret && connect(model, SIGNAL(rowsInserted(const QModelIndex, int, int)),
                       this, SLOT(onListChanged()));
  ret = ret && connect(model, SIGNAL(rowsRemoved(const QModelIndex, int, int)),
                       this, SLOT(onListChanged()));
  ret = ret && connect(model,
                       SIGNAL(rowsMoved(const QModelIndex, int, int,
                                        const QModelIndex, int)),
                       this, SLOT(onListChanged()));
  updateTitle();
}

//-----------------------------------------------------------------------------

PreproductionBoard::~PreproductionBoard() {}

//-----------------------------------------------------------------------------

void PreproductionBoard::setDirtyFlag(bool state) {
  if (state == m_dirtyFlag) return;

  m_dirtyFlag = state;
  updateTitle();
}

void PreproductionBoard::save(QSettings &settings, bool forPopupIni) const {
  UINT isWrapped = 0;
  isWrapped      = m_iconsWrapped ? 1 : 0;
  settings.setValue("wrapped", isWrapped);
}

//-----------------------------------------------------------------------------

void PreproductionBoard::load(QSettings &settings) {
  UINT isWrapped = settings.value("wrapped", 1).toUInt();
  m_iconsWrapped = isWrapped == 1;
  if (m_fileList->isWrapping() != m_iconsWrapped) m_fileList->toggleIconWrap();
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onNewBoard() {
  if (m_dirtyFlag) {
    int ret = DVGui::MsgBox(
        QString(tr("Are you sure you want to discard your changes?")),
        tr("Yes"), tr("No"));
    if (ret == 1)
      setDirtyFlag(false);
    else
      return;
  }

  onClearBoard();
  m_filePath = TFilePath();
  setDirtyFlag(false);
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onResetBoard() {
  if (m_dirtyFlag) {
    int ret = DVGui::MsgBox(
        QString(tr("Are you sure you want to discard your changes?")),
        tr("Yes"), tr("No"));
    if (ret != 1) return;
  }

  if (m_filePath.isEmpty()) {
    onClearBoard();
    setDirtyFlag(false);
  } else {
    setDirtyFlag(false);
    doLoad(m_filePath);
  }
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onClearBoard() {
  if (!m_fileList->count()) return;
  m_fileList->clear();
  setDirtyFlag(true);
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onNewScene() {
  TFilePath parentFolder =
      TProjectManager::instance()->getCurrentProject()->getScenesPath();
  QString sceneName;
  TFilePath scenePath;
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (scene->isUntitled()) {
    bool ok;
    sceneName = QInputDialog::getText(
        this, tr("Create Scene"), tr("Scene name:"), QLineEdit::Normal,
        QString(), &ok,
        (windowFlags() & ~Qt::WindowContextHelpButtonHint &
         ~Qt::WindowMinMaxButtonsHint & ~Qt::WindowSystemMenuHint));
    if (!ok || sceneName == "") return;
  } else {
    sceneName = QString::fromWCharArray(scene->getSceneName().c_str());
  }
  QString prefix;
  QString number;
  for (int j = 0; j < sceneName.length(); j++) {
    QChar c;
    c = sceneName.at(sceneName.length() - 1 - j);
    if (c.isDigit()) {
      number = QString(c) + number;
    } else {
      prefix = sceneName;
      prefix.truncate(sceneName.length() - j);
      break;
    }
  }
  if (number.length() == 0) {
    // prefix+="-";
    number = "000";
  }
  int i = number.toInt();
  do {
    QString number_ext =
        QStringLiteral("%1").arg(++i, number.length(), 10, QLatin1Char('0'));
    scenePath = parentFolder +
                (prefix.toStdWString() + number_ext.toStdWString() + L".tnz");
  } while (TFileStatus(scenePath).doesExist());

  if (!IoCmd::saveSceneIfNeeded(QObject::tr("Change project"))) return;
  IoCmd::newScene();
  IoCmd::saveScene(scenePath, false);

  QString name =
      QString::fromStdWString(scenePath.withoutParentDir().getWideString());
  QString path = scenePath.getQString();
  m_fileList->addFile(name, path);
}

//-----------------------------------------------------------------------------
void PreproductionBoard::doSave(TFilePath fp) {
  setDirtyFlag(false);

  TOStream os(fp);

  std::map<std::string, std::string> attr;
  attr["version"] = "1.0";

  os.openChild("tnzboard", attr);
  os.child("generator") << TEnv::getApplicationFullName();

  os.openChild("board");
  for (int i = 0; i < m_fileList->count(); i++) {
    QListWidgetItem *lwi = m_fileList->item(i);
    os.child("item") << lwi->data(Qt::UserRole).toString();
  }
  os.closeChild();  // "board"
  os.closeChild();  // "tnzboard"
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onSaveBoard() {
  if (m_filePath.isEmpty())
    onSaveAsBoard();
  else
    doSave(m_filePath);
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onSaveAsBoard() {
  if (m_fileList->count() == 0) {
    DVGui::warning(tr("The board is empty!"));
    return;
  }

  static SaveBoardPopup *popup = 0;
  if (!popup) popup = new SaveBoardPopup();

  if (popup->exec()) {
    m_filePath = popup->getSaveFilePath();
    if (!m_filePath.isEmpty()) doSave(m_filePath);
  }
}

//------------------------------------------------------------------------------

void PreproductionBoard::doLoad(TFilePath fp) {
  if (m_dirtyFlag) {
    int ret =
        DVGui::MsgBox(QString(tr("The current board has been modified.\nDoyou "
                                 "want to save your changes?")),
                      tr("Save"), tr("Discard"), tr("Cancel"));
    if (ret == 1)
      onSaveBoard();
    else if (ret == 3 || ret == 0)
      return;
  }

  m_fileList->clear();

  try {
    TIStream is(fp);
    if (is) {
    } else
      throw TException(fp.getWideString() + L": Can't open file");

    m_filePath = fp;

    std::string tagName = "";
    if (!is.matchTag(tagName)) throw TException("Bad file format");

    if (tagName == "tnzboard") {
      std::string rootTagName = tagName;
      std::string v           = is.getTagAttribute("version");
      while (is.matchTag(tagName)) {
        if (tagName == "generator") {
          std::string program = is.getString();
        } else if (tagName == "board") {
          while (!is.eos()) {
            std::string brdTagName;
            while (is.matchTag(brdTagName)) {
              if (brdTagName == "item") {
                QString file;
                is >> file;
                TFilePath fp(file);
                QString name = QString::fromStdWString(
                    fp.withoutParentDir().getWideString());
                QString path = fp.getQString();
                m_fileList->addFile(name, path);
              } else {
                throw TException(brdTagName + " : unexpected board tag");
              }

              if (!is.matchEndTag())
                throw TException(tagName + " : missing end board tag");
            }
          }
        } else {
          throw TException(tagName + " : unexpected tnzboard tag");
        }

        if (!is.matchEndTag())
          throw TException(tagName + " : missing end tnzboard tag");
      }
      if (!is.matchEndTag())
        throw TException(rootTagName + " : missing end tag");
    } else {
      throw TException("Bad file format");
    }
  } catch (TException &e) {
    throw e;
  } catch (...) {
    throw TException("Loading error.");
  }

  setDirtyFlag(false);
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onLoadBoard(TFilePath fp) {
  if (!fp.isEmpty()) {
    m_filePath = fp;
    doLoad(m_filePath);
    return;
  }
  static LoadBoardPopup *popup = 0;

  if (!popup) popup = new LoadBoardPopup();

  if (popup->exec()) {
    m_filePath = popup->getLoadFilePath();
    if (!m_filePath.isEmpty()) doLoad(m_filePath);
  }
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onPlayBoard() {
  if (!m_fileList->count()) {
    DVGui::error(tr("There is nothing in the board to play."));
    return;
  }

  QList<QListWidgetItem *> playList;

  if (m_fileList->selectedItems().size())
    playList = m_fileList->selectedItems();
  else {
    // Play all if nothing is selected
    for (int i = 0; i < m_fileList->count(); i++) {
      playList.append(m_fileList->item(i));
    }
  }

  FrameCountReader frameReader;
  FlipBook *flipbook = 0;
  bool append        = false;

  foreach (QListWidgetItem *lwi, playList) {
    TFilePath fp(lwi->data(Qt::UserRole).toString());

    if (!TSystem::doesExistFileOrLevel(fp)) {
      DVGui::error(
          QObject::tr("File %1 does not exist. Cannot add it to the flipbook.")
              .arg(QString::fromStdWString(fp.getWideString())));
      continue;
    }

    int frameCount = frameReader.getFrameCount(fp);
    if (frameCount <= 0) continue;

    int startFrame = 1;
    int endFrame   = frameCount;

    if (fp.isLevelName() || fp.getType() == "pli" || fp.getType() == "tlv")
      startFrame = endFrame = -1;
    else if (fp.withoutParentDir().getWideString() !=
             fp.withoutParentDir().getLevelNameW()) {
      TFrameId fid = fp.getFrame();
      startFrame = endFrame = fid.getNumber();
    }

    if (!flipbook) {
      flipbook = FlipBookPool::instance()->pop();
    }

    ::viewFile(fp, startFrame, endFrame, -1, -1, 0, flipbook, append);
    append = true;
  }
}

//-----------------------------------------------------------------------------

void PreproductionBoard::onListChanged() {
  if (m_dirtyFlag) return;

  setDirtyFlag(true);
}

//------------------------------------------------------------------------------

void PreproductionBoard::onExit(bool &ret) {
  if (!m_dirtyFlag) return;

  QWidget *w = parentWidget();

  if (w && !w->isVisible()) w->show();

  int answer =
      DVGui::MsgBox(QString(tr("A production board has been modified.\nDo "
                               "you want to save your changes?")),
                    tr("Save"), tr("Discard"), tr("Cancel"), 1);

  ret = true;
  if (answer == 3)
    ret = false;
  else if (answer == 1)
    onSaveBoard();
}

//-----------------------------------------------------------------------------

void PreproductionBoard::updateTitle() {
  QString title = tr("Preproduction Board");
  title += " - " + (m_filePath.isEmpty()
                        ? tr("untitled")
                        : QString::fromStdString(m_filePath.getName()));
  title += m_dirtyFlag ? "*" : "";

  QWidget *w = parentWidget();

  w->setWindowTitle(title);
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

LoadBoardPopup::LoadBoardPopup() : FileBrowserPopup(tr("Load Board")) {
  addFilterType("tnzbrd");
  setOkText(tr("Load"));
}

//-----------------------------------------------------------------------------

bool LoadBoardPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  if (!TSystem::doesExistFileOrLevel(fp)) {
    DVGui::error(toQString(fp) + tr(" does not exist."));
    return false;
  } else if (fp.getType() != "tnzbrd") {
    DVGui::error(toQString(fp) +
                 tr("It is possible to load only TNZBRD files."));
    return false;
  }

  m_filePath = fp;
  return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

SaveBoardPopup::SaveBoardPopup() : FileBrowserPopup(tr("Save Board")) {
  addFilterType("tnzbrd");
  setOkText(tr("Save"));
}

//-----------------------------------------------------------------------------

bool SaveBoardPopup::execute() {
  if (m_selectedPaths.empty()) return false;

  const TFilePath &fp = *m_selectedPaths.begin();

  m_filePath = fp.withType("tnzbrd");
  return true;
}

//=============================================================================

OpenFloatingPanel openPreproductionBoardPane(
    MI_OpenPreproductionBoard, "PreproductionBoard",
    QObject::tr("Preproduction Board"));
