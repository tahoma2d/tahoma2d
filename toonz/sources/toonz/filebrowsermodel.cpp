

#include "filebrowsermodel.h"
#include "tutil.h"
#include "tsystem.h"
#include "tconvert.h"
#include "tenv.h"
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonzqt/gutil.h"

#include "filebrowser.h"
#include "history.h"
#include "iocommand.h"

#include "tapp.h"
#include "toonz/tscenehandle.h"
#include "mainwindow.h"

#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>

#ifdef _WIN32
#include <shlobj.h>
#include <winnetwk.h>
#include <wctype.h>
#endif
#ifdef MACOSX
#include <Cocoa/Cocoa.h>
#endif
#ifdef LINUX
#include <QStandardPaths>
#endif

namespace {
TFilePath getMyDocumentsPath() {
#ifdef _WIN32
  WCHAR szPath[MAX_PATH];
  if (SHGetSpecialFolderPath(NULL, szPath, CSIDL_PERSONAL, 0)) {
    return TFilePath(szPath);
  }
  return TFilePath();
#elif defined MACOSX
  NSArray *foundref = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);
  if (!foundref) return TFilePath();
  int c = [foundref count];
  assert(c == 1);
  NSString *documentsDirectory = [foundref objectAtIndex:0];
  return TFilePath((const char *)[documentsDirectory
      cStringUsingEncoding:NSASCIIStringEncoding]);
#else
  QDir dir = QDir::home();
  if (!dir.cd("Documents")) return TFilePath();
  return TFilePath(dir.absolutePath().toStdString());
#endif
}

// Desktop Path
TFilePath getDesktopPath() {
#ifdef _WIN32
  WCHAR szPath[MAX_PATH];
  if (SHGetSpecialFolderPath(NULL, szPath, CSIDL_DESKTOP, 0)) {
    return TFilePath(szPath);
  }
  return TFilePath();
#elif defined MACOSX
  NSArray *foundref = NSSearchPathForDirectoriesInDomains(
      NSDesktopDirectory, NSUserDomainMask, YES);
  if (!foundref) return TFilePath();
  int c = [foundref count];
  assert(c == 1);
  NSString *desktopDirectory = [foundref objectAtIndex:0];
  return TFilePath((const char *)[desktopDirectory
      cStringUsingEncoding:NSASCIIStringEncoding]);
#elif defined LINUX
  QString desktopPath =
      QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
  if (desktopPath.isEmpty()) {
    QDir dir = QDir::home();
    if (!dir.cd("Desktop")) return TFilePath();
    desktopPath = dir.absolutePath();
  }
  return TFilePath(desktopPath.toStdString());
#else
  QDir dir = QDir::home();
  if (!dir.cd("Desktop")) return TFilePath();
  return TFilePath(dir.absolutePath().toStdString());
#endif
}

// Downloads Path
TFilePath getDownloadsPath() {
  QStringList stdLocs =
      QStandardPaths::standardLocations(QStandardPaths::DownloadLocation);
  if (stdLocs.isEmpty()) return TFilePath();
  return TFilePath(stdLocs[0]);
}
}  // namespace

//=============================================================================
//
// DvDirModelNode
//
//-----------------------------------------------------------------------------

DvDirModelNode::DvDirModelNode(DvDirModelNode *parent, std::wstring name)
    : m_parent(parent)
    , m_name(name)
    , m_oldName()
    , m_row(-1)
    , m_childrenValid(false)
    , m_renameEnabled(false)
    , m_nodeType("") {}

//-----------------------------------------------------------------------------

DvDirModelNode::~DvDirModelNode() { clearPointerContainer(m_children); }

//-----------------------------------------------------------------------------

void DvDirModelNode::removeChildren(int row, int count) {
  assert(0 <= row && row + count <= (int)m_children.size());
  int i;
  for (i = 0; i < count; i++) delete m_children[row + i];
  m_children.erase(m_children.begin() + row, m_children.begin() + row + count);
  for (i = 0; i < (int)m_children.size(); i++) m_children[i]->setRow(i);
}

//-----------------------------------------------------------------------------

void DvDirModelNode::addChild(DvDirModelNode *child) {
  child->setRow(m_children.size());
  m_children.push_back(child);
}

//-----------------------------------------------------------------------------

void DvDirModelNode::insertChild(int row, DvDirModelNode *child) {
  child->setRow(row);
  m_children.insert(m_children.begin() + row, child);
}

//-----------------------------------------------------------------------------

int DvDirModelNode::getChildCount() {
  if (!m_childrenValid) refreshChildren();
  return (int)m_children.size();
}

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirModelNode::getChild(int index) {
  if (!m_childrenValid) refreshChildren();
  assert(0 <= index && index < getChildCount());
  return m_children[index];
}

//-----------------------------------------------------------------------------

bool DvDirModelNode::hasChildren() {
  if (m_childrenValid)
    return getChildCount() > 0;
  else
    return true;
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelNode::getPixmap(bool isOpen) const { return QPixmap(); }

//-----------------------------------------------------------------------------

void DvDirModelNode::setTemporaryName(const std::wstring &newName) {
  m_oldName = getName();
  m_name    = newName;
}

//-----------------------------------------------------------------------------

void DvDirModelNode::restoreName() {
  if (m_oldName != L"") {
    m_name    = m_oldName;
    m_oldName = L"";
  }
}

//=============================================================================
//
// DvDirModelFileFolderNode [File folder]
//
//-----------------------------------------------------------------------------

DvDirModelFileFolderNode::DvDirModelFileFolderNode(DvDirModelNode *parent,
                                                   std::wstring name,
                                                   const TFilePath &path)
    : DvDirModelNode(parent, name)
    , m_path(path)
    , m_isProjectFolder(false)
    , m_existsChecked(false)
    , m_exists(true)
    , m_hasChildren(false)
    , m_peeks(true) {
  m_nodeType = "FileFolder";
}

//-----------------------------------------------------------------------------

DvDirModelFileFolderNode::DvDirModelFileFolderNode(DvDirModelNode *parent,
                                                   const TFilePath &path)
    : DvDirModelNode(parent, path.withoutParentDir().getWideString())
    , m_path(path)
    , m_isProjectFolder(false)
    , m_existsChecked(false)
    , m_exists(true)
    , m_hasChildren(false)
    , m_peeks(true) {
  m_nodeType = "FileFolder";
}

//-----------------------------------------------------------------------------

bool DvDirModelFileFolderNode::exists() {
  return m_existsChecked ? m_exists
                         : m_peeks
             ? m_existsChecked = true,
               m_exists = TFileStatus(m_path).doesExist() : true;
}

//-----------------------------------------------------------------------------

void DvDirModelFileFolderNode::visualizeContent(FileBrowser *browser) {
  browser->setFolder(m_path);
}

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirModelFileFolderNode::makeChild(std::wstring name) {
  return createNode(this, m_path + name);
}

//-----------------------------------------------------------------------------

DvDirModelFileFolderNode *DvDirModelFileFolderNode::createNode(
    DvDirModelNode *parent, const TFilePath &path) {
  DvDirModelFileFolderNode *node;
  // check the project nodes under the Project Root Node
  if (  // QString::fromStdWString(parent->getName()).startsWith("Project root")
        // &&
      TProjectManager::instance()->isProject(path))
    node = new DvDirModelProjectNode(parent, path);
  else {
    node = new DvDirModelFileFolderNode(parent, path);
    if (path.getName().find("_files") == std::string::npos)
      node->enableRename(true);
  }
  return node;
}

//-----------------------------------------------------------------------------

bool DvDirModelFileFolderNode::setName(std::wstring newName) {
  if (isSpaceString(QString::fromStdWString(newName))) return true;

  TFilePath srcPath = getPath();
  TFilePath dstPath = getPath().getParentDir() + newName;
  if (srcPath == dstPath) return false;
  try {
    TSystem::renameFile(dstPath, srcPath);
  } catch (...) {
    return false;
  }
  m_path = dstPath;
  m_name = newName;
  return true;
}

//-----------------------------------------------------------------------------

bool DvDirModelFileFolderNode::hasChildren() {
  if (m_childrenValid) return m_hasChildren;

  if (m_peeks) {
    // Using QDirIterator and only checking existence of the first item
    QDir dir(m_path.getQString());
    dir.setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    QDirIterator it(dir);
    return (it.hasNext());
  } else
    return true;  // Not peeking nodes allow users to actively scan for
                  // sub-folders
}

//------------------------------------------------------------------------------

void DvDirModelFileFolderNode::refreshChildren() {
  m_childrenValid = true;

  std::vector<std::wstring> names;
  getChildrenNames(names);

  std::vector<DvDirModelNode *> oldChildren = m_children;

  // synchronize nodes for each items in the current folder
  QModelIndex index = DvDirModel::instance()->getIndexByNode(this);
  int i, j;
  for (i = 0; i < (int)names.size(); i++) {
    std::wstring name = names[i];

    // check if the item is already in the children nodes
    for (j = i; j < m_children.size() && m_children[j]->getName() != name;
         j++) {
    }
    // if the child is already registered
    if (j < m_children.size()) {
      if (i != j) {
        // swap node order
        DvDirModel::instance()->notifyBeginMoveRows(index, j, j, index, i);
        std::swap(m_children[i], m_children[j]);
        DvDirModel::instance()->notifyEndMoveRows();
      }
    }
    // if not, create a new child and insert
    else {
      DvDirModelNode *child = makeChild(name);
      if (DvDirModelFileFolderNode *folderNode =
              dynamic_cast<DvDirModelFileFolderNode *>(child))
        folderNode->setPeeking(m_peeks);
      assert(child);
      DvDirModel::instance()->notifyBeginInsertRows(index, i, i);
      addChild(child);
      DvDirModel::instance()->notifyEndInsertRows();
    }
  }
  // delete rest of the children nodes
  for (j = m_children.size() - 1; j >= i; j--) {
    DvDirModelNode *child = m_children[j];
    DvDirModel::instance()->notifyBeginRemoveRows(index, j, j);
    m_children.erase(m_children.begin() + j);
    DvDirModel::instance()->notifyEndRemoveRows();

    if (!!child && child->hasChildren())
      child->removeChildren(0, child->getChildCount());

    delete child;
  }

  m_hasChildren = (m_children.size() > 0);
}

//-----------------------------------------------------------------------------

void DvDirModelFileFolderNode::getChildrenNames(
    std::vector<std::wstring> &names) const {
  if (m_path.isEmpty()) return;
  TFileStatus folderPathStatus(m_path);
  if (folderPathStatus.isLink()) return;

  if (!folderPathStatus.isDirectory()) return;

  QStringList dirItems;
  TSystem::readDirectory_DirItems(dirItems, m_path);
  for (const QString &name : dirItems) names.push_back(name.toStdWString());

  QDir dir(toQString(m_path));
}

//-----------------------------------------------------------------------------

int DvDirModelFileFolderNode::rowByName(const std::wstring &name) const {
  std::vector<std::wstring> names;
  getChildrenNames(names);
  std::vector<std::wstring>::iterator it =
      std::find(names.begin(), names.end(), name);
  if (it == names.end())
    return -1;
  else
    return std::distance(names.begin(), it);
}

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirModelFileFolderNode::getNodeByPath(const TFilePath &path) {
  if (path == m_path) return this;
  if (!m_path.isAncestorOf(path)) return 0;
  int i, n = getChildCount();
  for (i = 0; i < n; i++) {
    DvDirModelNode *node = getChild(i)->getNodeByPath(path);
    if (node) return node;
  }
  return 0;
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelFileFolderNode::getPixmap(bool isOpen) const {
  static QPixmap openFolderPixmap  = generateIconPixmap("folder_on");
  static QPixmap closeFolderPixmap = generateIconPixmap("folder");
  return isOpen ? openFolderPixmap : closeFolderPixmap;
}

//=============================================================================
//
// DvDirModelSpecialFileFolderNode [Special File Folder]
//
//-----------------------------------------------------------------------------

DvDirModelSpecialFileFolderNode::DvDirModelSpecialFileFolderNode(
    DvDirModelNode *parent, std::wstring name, const TFilePath &path)
    : DvDirModelFileFolderNode(parent, name, path) {}

//-----------------------------------------------------------------------------

QPixmap DvDirModelSpecialFileFolderNode::getPixmap(bool isOpen) const {
  return m_pixmap;
}

//-----------------------------------------------------------------------------

void DvDirModelSpecialFileFolderNode::setPixmap(const QPixmap &pixmap) {
  m_pixmap = pixmap;
}

//=============================================================================
//
// DvDirVersionControlNode [Special File Folder]
//
//-----------------------------------------------------------------------------

DvDirVersionControlNode::DvDirVersionControlNode(DvDirModelNode *parent,
                                                 std::wstring name,
                                                 const TFilePath &path)
    : DvDirModelFileFolderNode(parent, name, path)
    , m_isSynched(false)
    , m_isUnversioned(false)
    , m_oldName() {
  this->setExists(TFileStatus(path).doesExist());
  setIsUnderVersionControl(
      VersionControl::instance()->isFolderUnderVersionControl(toQString(path)));
}

//-----------------------------------------------------------------------------

QList<TFilePath> DvDirVersionControlNode::getMissingFiles() const {
  QList<TFilePath> list;
  QMap<QString, SVNStatus>::const_iterator i = m_statusMap.constBegin();
  while (i != m_statusMap.constEnd()) {
    SVNStatus s = i.value();
    if (s.m_item == "missing" ||
        (s.m_item == "none" && s.m_repoStatus == "added")) {
      TFilePath path(getPath() + TFilePath(s.m_path.toStdWString()));
      std::string dots = path.getDots();
      if (dots != "") {
        if (dots == "..") path = path.getParentDir() + path.getLevelNameW();
        if (!list.contains(path)) list.append(path);
      }
    }
    i++;
  }
  return list;
}

//-----------------------------------------------------------------------------

QStringList DvDirVersionControlNode::getMissingFiles(
    const QRegExp &filter) const {
  QStringList list;
  QMap<QString, SVNStatus>::const_iterator i = m_statusMap.constBegin();
  for (; i != m_statusMap.constEnd(); i++) {
    SVNStatus s = i.value();
    if (s.m_item == "missing" ||
        (s.m_item == "none" && s.m_repoStatus == "added")) {
      TFilePath path(s.m_path.toStdWString());
      if (!filter.exactMatch(
              QString::fromStdWString(path.withoutParentDir().getWideString())))
        continue;
      std::string dots = path.getDots();
      if (dots != "") list.append(toQString(path));
    }
  }
  return list;
}

//-----------------------------------------------------------------------------

QList<TFilePath> DvDirVersionControlNode::getMissingFolders() const {
  QList<TFilePath> list;
  QMap<QString, SVNStatus>::const_iterator i = m_statusMap.constBegin();
  while (i != m_statusMap.constEnd()) {
    SVNStatus s = i.value();
    if (s.m_item == "missing" ||
        (s.m_item == "none" && s.m_repoStatus == "added")) {
      TFilePath path(getPath() + TFilePath(s.m_path.toStdWString()));
      if (path.getDots() == "" || path.getType() == "tnz") {
        QString currentFolderName =
            QString::fromStdWString(getPath().getWideName());
        if (currentFolderName != s.m_path) {
          if (!s.m_path.contains("\\"))
            list.append(path);
          else {
            QString newPath = s.m_path.replace(currentFolderName + "\\", "");
            if (!newPath.contains("\\"))
              list.append(getPath() + TFilePath(newPath.toStdWString()));
          }
        }
      }
    }
    i++;
  }
  return list;
}

//-----------------------------------------------------------------------------

void DvDirVersionControlNode::getChildrenNames(
    std::vector<std::wstring> &names) const {
  DvDirModelFileFolderNode::getChildrenNames(names);

  QList<TFilePath> missingFolders = getMissingFolders();
  int count                       = missingFolders.size();

  for (int i = 0; i < count; i++) {
    std::wstring name = missingFolders.at(i).getWideName();
    if (find(names.begin(), names.end(), name) == names.end())
      names.push_back(name);
  }
}

//-----------------------------------------------------------------------------

void DvDirVersionControlNode::setIsUnderVersionControl(bool value) {
  // Set the flags and enable/disable renaming accordingly
  m_isUnderVersionControl = value;
  enableRename(!value);
}

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirVersionControlNode::makeChild(std::wstring name) {
  if (TProjectManager::instance()->isProject(getPath() + name))
    return new DvDirVersionControlProjectNode(this, name, getPath() + name);
  return new DvDirVersionControlNode(this, name, getPath() + name);
}

//-----------------------------------------------------------------------------

QPixmap DvDirVersionControlNode::getPixmap(bool isOpen) const {
  static QPixmap openFolderPixmap(generateIconPixmap("folder_on"));
  static QPixmap closeFolderPixmap(generateIconPixmap("folder"));
  static QPixmap openMissingPixmap(
      svgToPixmap(":Resources/vcfolder_mis_open.svg"));
  static QPixmap closeMissingPixmap(
      svgToPixmap(":Resources/vcfolder_mis_close.svg"));
  static QPixmap openSceneFolderPixmap(
      svgToPixmap(":Resources/browser_scene_open.svg"));
  static QPixmap closeSceneFolderPixmap(
      svgToPixmap(":Resources/browser_scene_close.svg"));

  if (TFileStatus(getPath()).doesExist()) {
    if (getPath().getType() == "tnz")
      return isOpen ? openSceneFolderPixmap : closeSceneFolderPixmap;
    else
      return isOpen ? openFolderPixmap : closeFolderPixmap;
  } else
    return isOpen ? openMissingPixmap : closeMissingPixmap;
}

//-----------------------------------------------------------------------------

void DvDirVersionControlNode::insertVersionControlStatus(
    const QString &fileName, SVNStatus status) {
  m_statusMap.insert(fileName, status);
}

//-----------------------------------------------------------------------------

SVNStatus DvDirVersionControlNode::getVersionControlStatus(
    const QString &fileName) {
  if (m_statusMap.contains(fileName)) return m_statusMap.value(fileName);

  SVNStatus s         = SVNStatus();
  s.m_isLocked        = false;
  s.m_isPartialEdited = false;
  s.m_isPartialLocked = false;
  return s;
}

//-----------------------------------------------------------------------------

QStringList DvDirVersionControlNode::refreshVersionControl(
    const QList<SVNStatus> &status) {
  TFilePath nodePath = getPath();
  if (nodePath.isEmpty()) return QStringList();

  int listSize   = status.size();
  bool isSynched = true;

  QStringList checkPartialLockList;

  for (int i = 0; i < listSize; i++) {
    SVNStatus s = status.at(i);

    // It is not needed to check and cache SVNStatus for files that are
    // "outside" the node "scope"
    if (s.m_path == "." || s.m_path == ".." || s.m_path.split("\\").size() > 2)
      continue;

    QString fileName = s.m_path;
    if (nodePath.getType() == "") insertVersionControlStatus(fileName, s);

    // Update also the status of the "scene" child folders
    TFilePath path(fileName.toStdWString());
    if (path.getType() == "tnz") {
      DvDirVersionControlNode *childVcNode =
          dynamic_cast<DvDirVersionControlNode *>(
              getNodeByPath(nodePath + path));
      if (childVcNode) {
        childVcNode->setIsUnderVersionControl(true);
        if (s.m_repoStatus == "modified" || s.m_item == "modified" ||
            s.m_item == "missing" || s.m_item == "none")
          childVcNode->setIsSynched(false);
        else if (s.m_item == "unversioned")
          childVcNode->setIsUnversioned(true);
        else
          childVcNode->setIsSynched(true);
      }
    }
    // If a folder is unversioned, I set its status and even the unversioned
    // status for its children
    else if (path.getType() == "" && s.m_item == "unversioned") {
      DvDirVersionControlNode *childVcNode =
          dynamic_cast<DvDirVersionControlNode *>(
              getNodeByPath(nodePath + path));
      if (childVcNode) {
        childVcNode->setIsUnversioned(true);
        int childCount = childVcNode->getChildCount();
        for (int i = 0; i < childCount; i++) {
          DvDirVersionControlNode *subChildNode =
              dynamic_cast<DvDirVersionControlNode *>(childVcNode->getChild(i));
          if (subChildNode) subChildNode->setIsUnversioned(true);
        }
      }
    }

    // Check also the 'partial-lock' property
    std::string type = TFilePath(s.m_path.toStdWString()).getType();
    if (type == "tlv" || type == "pli") {
      if (s.m_item != "unversioned" && s.m_item != "missing" &&
          s.m_repoStatus != "added")
        checkPartialLockList.append(s.m_path);
    }

    if (s.m_repoStatus == "modified" || s.m_item == "modified" ||
        s.m_item == "unversioned" || s.m_item == "missing" ||
        s.m_item == "none")
      isSynched = false;
  }

  setIsSynched(isSynched);
  return checkPartialLockList;
}

//-----------------------------------------------------------------------------

DvDirVersionControlRootNode *
DvDirVersionControlNode::getVersionControlRootNode() {
  DvDirModelNode *parent = getParent();
  if (parent) return parent->getVersionControlRootNode();
  return 0;
}

//=============================================================================
//
// DvDirVersionControlRootNode [Special File Folder]
//
//-----------------------------------------------------------------------------

DvDirVersionControlRootNode::DvDirVersionControlRootNode(DvDirModelNode *parent,
                                                         std::wstring name,
                                                         const TFilePath &path)
    : DvDirVersionControlNode(parent, name, path) {
  setPixmap(svgToPixmap(":Resources/vcroot.svg"));
  setIsUnderVersionControl(true);
}

//-----------------------------------------------------------------------------

void DvDirVersionControlRootNode::refreshChildren() {
  DvDirModelFileFolderNode::refreshChildren();
}

//=============================================================================
// DvDirVersionControlProjectNode
//-----------------------------------------------------------------------------

DvDirVersionControlProjectNode::DvDirVersionControlProjectNode(
    DvDirModelNode *parent, std::wstring name, const TFilePath &path)
    : DvDirVersionControlNode(parent, name, path) {
  m_nodeType = "Project";
  setIsUnderVersionControl(true);
}

//-----------------------------------------------------------------------------

TFilePath DvDirVersionControlProjectNode::getProjectPath() const {
  return TProjectManager::instance()->projectFolderToProjectPath(getPath());
}

//-----------------------------------------------------------------------------

bool DvDirVersionControlProjectNode::isCurrent() const {
  TProjectManager *pm = TProjectManager::instance();
  return pm->getCurrentProjectPath().getParentDir() == getPath();
}

//-----------------------------------------------------------------------------

void DvDirVersionControlProjectNode::makeCurrent() {
  TProjectManager *pm   = TProjectManager::instance();
  TFilePath projectPath = getProjectPath();
  if (!IoCmd::saveSceneIfNeeded(QObject::tr("Change project"))) return;
  pm->setCurrentProjectPath(projectPath);
  IoCmd::newScene();
}

//-----------------------------------------------------------------------------

QPixmap DvDirVersionControlProjectNode::getPixmap(bool isOpen) const {
  static QPixmap openPixmap(
      svgToPixmap(":Resources/browser_vcproject_open.svg"));
  static QPixmap closePixmap(
      svgToPixmap(":Resources/browser_vcproject_close.svg"));
  static QPixmap openMissingPixmap(
      svgToPixmap(":Resources/vcfolder_mis_open.svg"));
  static QPixmap closeMissingPixmap(
      svgToPixmap(":Resources/vcfolder_mis_close.svg"));

  if (TFileStatus(getPath()).doesExist())
    return isOpen ? openPixmap : closePixmap;
  else
    return isOpen ? openMissingPixmap : closeMissingPixmap;
}

//-----------------------------------------------------------------------------

void DvDirVersionControlProjectNode::refreshChildren() {
  DvDirModelFileFolderNode::refreshChildren();
  int i;
  TProjectManager *pm = TProjectManager::instance();
  auto project        = std::make_shared<TProject>();
  project->load(getProjectPath());
  for (i = 0; i < getChildCount(); i++) {
    DvDirModelFileFolderNode *node =
        dynamic_cast<DvDirModelFileFolderNode *>(getChild(i));
    if (node) {
      int k = project->getFolderIndexFromPath(node->getPath());
      node->setIsProjectFolder(k >= 0);
    }
  }
}

//-----------------------------------------------------------------------------

void DvDirVersionControlProjectNode::getChildrenNames(
    std::vector<std::wstring> &names) const {
  DvDirVersionControlNode::getChildrenNames(names);
  TProjectManager *pm = TProjectManager::instance();
  auto project        = std::make_shared<TProject>();
  project->load(getProjectPath());
  int i;
  for (i = 0; i < project->getFolderCount(); i++) {
    std::string folderName = project->getFolderName(i);
    TFilePath folderPath   = project->getFolder(i);
    // if(folderPath.isAbsolute() || folderPath.getParentDir() != TFilePath())
    if (folderPath.isAbsolute() && project->isConstantFolder(i)) {
      names.push_back(L"+" + ::to_wstring(folderName));
    }
  }
}

//=============================================================================
//
// DvDirModelProjectNode [Project Folder]
//
//-----------------------------------------------------------------------------

DvDirModelProjectNode::DvDirModelProjectNode(DvDirModelNode *parent,
                                             const TFilePath &path)
    : DvDirModelFileFolderNode(parent, path) {
  m_nodeType = "Project";
}

//-----------------------------------------------------------------------------

TFilePath DvDirModelProjectNode::getProjectPath() const {
  return TProjectManager::instance()->projectFolderToProjectPath(getPath());
}

//-----------------------------------------------------------------------------

bool DvDirModelProjectNode::isCurrent() const {
  TProjectManager *pm = TProjectManager::instance();
  return pm->getCurrentProjectPath().getParentDir() == getPath();
}

//-----------------------------------------------------------------------------

void DvDirModelProjectNode::makeCurrent() {
  TProjectManager *pm   = TProjectManager::instance();
  TFilePath projectPath = getProjectPath();
  if (!IoCmd::saveSceneIfNeeded(QObject::tr("Change project"))) return;
  TFilePath projectFolder = getPath();
  pm->setCurrentProjectPath(projectPath);
  RecentFiles::instance()->addFilePath(projectFolder.getQString(),
                                       RecentFiles::Project);
  IoCmd::newScene();
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelProjectNode::getPixmap(bool isOpen) const {
  static QPixmap openProjectPixmap  = generateIconPixmap("folder_project_on");
  static QPixmap closeProjectPixmap = generateIconPixmap("folder_project");
  return isOpen ? openProjectPixmap : closeProjectPixmap;
}

//-----------------------------------------------------------------------------

void DvDirModelProjectNode::refreshChildren() {
  DvDirModelFileFolderNode::refreshChildren();
  int i;
  TProjectManager *pm = TProjectManager::instance();
  auto project        = std::make_shared<TProject>();
  project->load(getProjectPath());
  for (i = 0; i < getChildCount(); i++) {
    DvDirModelFileFolderNode *node =
        dynamic_cast<DvDirModelFileFolderNode *>(getChild(i));
    if (node) {
      int k = project->getFolderIndexFromPath(node->getPath());
      node->setIsProjectFolder(k >= 0);
    }
  }
}

//-----------------------------------------------------------------------------

void DvDirModelProjectNode::getChildrenNames(
    std::vector<std::wstring> &names) const {
  DvDirModelFileFolderNode::getChildrenNames(names);
  TProjectManager *pm = TProjectManager::instance();
  auto project        = std::make_shared<TProject>();
  project->load(getProjectPath());
  int i;
  for (i = 0; i < project->getFolderCount(); i++) {
    std::string folderName = project->getFolderName(i);
    TFilePath folderPath   = project->getFolder(i);
    // if(folderPath.isAbsolute() || folderPath.getParentDir() != TFilePath())
    if (folderPath.isAbsolute() && project->isConstantFolder(i)) {
      names.push_back(L"+" + ::to_wstring(folderName));
    }
  }
}

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirModelProjectNode::makeChild(std::wstring name) {
  if (name != L"" && name[0] == L'+') {
    TProjectManager *pm = TProjectManager::instance();
    auto project        = std::make_shared<TProject>();
    project->load(getProjectPath());
    std::string folderName = ::to_string(name.substr(1));
    TFilePath folderPath   = project->getFolder(folderName);
    DvDirModelNode *node = new DvDirModelFileFolderNode(this, name, folderPath);
    return node;
  } else
    return DvDirModelFileFolderNode::makeChild(name);
}

//=============================================================================
//
// DvDirModelDayNode [Day folder]
//
//-----------------------------------------------------------------------------

DvDirModelDayNode::DvDirModelDayNode(DvDirModelNode *parent,
                                     std::wstring dayDateString)
    : DvDirModelNode(parent, dayDateString)
    , m_dayDateString(::to_string(dayDateString)) {
  m_nodeType = "Day";
}

//-----------------------------------------------------------------------------

void DvDirModelDayNode::visualizeContent(FileBrowser *browser) {
  browser->setHistoryDay(m_dayDateString);
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelDayNode::getPixmap(bool isOpen) const {
  static QPixmap openFolderPixmap  = generateIconPixmap("folder_on");
  static QPixmap closeFolderPixmap = generateIconPixmap("folder");
  return isOpen ? openFolderPixmap : closeFolderPixmap;
}

//=============================================================================
//
// DvDirModelHistoryNode [History]
//
//-----------------------------------------------------------------------------

DvDirModelHistoryNode::DvDirModelHistoryNode(DvDirModelNode *parent)
    : DvDirModelNode(parent, L"History") {
  m_nodeType = "History";
}

//-----------------------------------------------------------------------------

void DvDirModelHistoryNode::refreshChildren() {
  QModelIndex index = DvDirModel::instance()->getIndexByNode(this);
  m_childrenValid   = true;
  if (!m_children.empty()) {
    DvDirModel::instance()->notifyBeginRemoveRows(index, 0,
                                                  m_children.size() - 1);
    clearPointerContainer(m_children);
    DvDirModel::instance()->notifyEndRemoveRows();
  }

  History *h   = History::instance();
  int dayCount = h->getDayCount();
  if (dayCount > 0) {
    DvDirModel::instance()->notifyBeginInsertRows(index, 0, dayCount);
    for (int i = 0; i < dayCount; i++) {
      const History::Day *day = h->getDay(i);
      DvDirModelNode *child =
          new DvDirModelDayNode(this, ::to_wstring(day->getDate()));
      addChild(child);
    }
    DvDirModel::instance()->notifyEndInsertRows();
  }
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelHistoryNode::getPixmap(bool isOpen) const {
  QIcon icon            = createQIcon("history");
  static QPixmap pixmap = icon.pixmap(16);
  return pixmap;
}

//=============================================================================
//
// DvDirModelMyComputerNode [My Computer]
//
//-----------------------------------------------------------------------------

DvDirModelMyComputerNode::DvDirModelMyComputerNode(DvDirModelNode *parent)
    : DvDirModelNode(parent, L"My Computer") {
  // setIcon(QFileIconProvider().icon(QFileIconProvider::Computer));
  m_nodeType = "MyComputer";
}

//-----------------------------------------------------------------------------

void DvDirModelMyComputerNode::refreshChildren() {
  QModelIndex index = DvDirModel::instance()->getIndexByNode(this);
  m_childrenValid   = true;

  if (!m_children.empty()) {
    DvDirModel::instance()->notifyBeginRemoveRows(index, 0,
                                                  m_children.size() - 1);
    clearPointerContainer(m_children);
    DvDirModel::instance()->notifyEndRemoveRows();
  }

  TFilePathSet fps = TSystem::getDisks();

#ifdef MACOSX
  fps.push_back(TFilePath("/Volumes/"));
#endif

  DvDirModel::instance()->notifyBeginInsertRows(index, 0, fps.size() - 1);
  TFilePathSet::iterator it;
  for (it = fps.begin(); it != fps.end(); ++it) {
    DvDirModelNode *child =
        new DvDirModelFileFolderNode(this, it->getWideString(), *it);
    addChild(child);
  }
  DvDirModel::instance()->notifyEndInsertRows();
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelMyComputerNode::getPixmap(bool isOpen) const {
  QIcon icon            = createQIcon("my_computer");
  static QPixmap pixmap = icon.pixmap(16);
  return pixmap;
}

//=============================================================================
//
// DvDirModelNetworkNode [Network]
//
//-----------------------------------------------------------------------------

DvDirModelNetworkNode::DvDirModelNetworkNode(DvDirModelNode *parent)
    : DvDirModelNode(parent, L"Network") {
  m_nodeType = "Network";
}

//-----------------------------------------------------------------------------

void DvDirModelNetworkNode::refreshChildren() {
  QModelIndex index = DvDirModel::instance()->getIndexByNode(this);
  m_childrenValid   = true;

  if (!m_children.empty()) {
    DvDirModel::instance()->notifyBeginRemoveRows(index, 0,
                                                  m_children.size() - 1);
    clearPointerContainer(m_children);
    DvDirModel::instance()->notifyEndRemoveRows();
  }

#ifdef _WIN32

  // Enumerate network nodes
  HANDLE enumHandle;
  DWORD err =
      WNetOpenEnum(RESOURCE_CONTEXT, RESOURCETYPE_DISK, 0, NULL, &enumHandle);
  assert(err == NO_ERROR);

  DWORD count = -1,
        bufferSize =
            16 << 10;  // Use 16 kB as recommended on Windows references
  LPNETRESOURCE buffer = (LPNETRESOURCE)malloc(bufferSize);

  do {
    // Get the next bunch of network devices
    memset(buffer, 0, bufferSize);
    err = WNetEnumResource(enumHandle, &count, buffer, &bufferSize);

    if (err == NO_ERROR) {
      for (DWORD i = 0; i < count; ++i) {
        // Only list disk-type devices - in any case, the remote (UNC) name
        // should exist
        if (buffer[i].dwType == RESOURCETYPE_DISK && buffer[i].lpRemoteName) {
          std::wstring wstr(buffer[i].lpRemoteName);

          // Build a NOT PEEKING folder node. This is important since network
          // access is SLOW.
          DvDirModelFileFolderNode *child =
              new DvDirModelFileFolderNode(this, wstr, TFilePath(wstr));
          child->setPeeking(false);

          DvDirModel::instance()->notifyBeginInsertRows(
              index, m_children.size(), m_children.size());
          addChild(child);
          DvDirModel::instance()->notifyEndInsertRows();
        }
      }
    } else if (err != ERROR_NO_MORE_ITEMS)
      break;  // Excluding NO_ERROR and ERROR_NO_MORE_ITEMS, quit

  } while (err != ERROR_NO_MORE_ITEMS);

  err = WNetCloseEnum(enumHandle);
  assert(err == NO_ERROR);

  free(buffer);

#endif
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelNetworkNode::getPixmap(bool isOpen) const {
  QIcon icon            = createQIcon("network");
  static QPixmap pixmap = icon.pixmap(16);
  return pixmap;
}

//=============================================================================
//
// DvDirModelStuffFolderNode [Tahoma2D]
//
//-----------------------------------------------------------------------------

DvDirModelStuffFolderNode::DvDirModelStuffFolderNode(DvDirModelNode *parent)
    : DvDirModelNode(parent, L"Tahoma2D") {
  m_nodeType = "StuffFolder";
}

//-----------------------------------------------------------------------------

void DvDirModelStuffFolderNode::refreshChildren() {
  m_childrenValid = true;
  if (!m_children.empty()) clearPointerContainer(m_children);

  DvDirModelSpecialFileFolderNode *child = new DvDirModelSpecialFileFolderNode(
      this, L"Library", ToonzFolder::getLibraryFolder());
  child->setPixmap(generateIconPixmap("library"));
  addChild(child);

  child = new DvDirModelSpecialFileFolderNode(
      this, L"Fx Macros",
      ToonzFolder::getFxPresetFolder() + TFilePath("presets/macroFx"));
  child->setPixmap(generateIconPixmap("fx_logo"));
  addChild(child);

  child = new DvDirModelSpecialFileFolderNode(this, L"Fx Plugins",
                                              ToonzFolder::getPluginsFolder());
  child->setPixmap(generateIconPixmap("plugins"));
  addChild(child);

  child = new DvDirModelSpecialFileFolderNode(
      this, L"Studio Palettes", ToonzFolder::getStudioPaletteFolder());
  child->setPixmap(generateIconPixmap("palette"));
  addChild(child);
}

//-----------------------------------------------------------------------------

QPixmap DvDirModelStuffFolderNode::getPixmap(bool isOpen) const {
  QIcon icon            = createQIcon("tahoma2d");
  static QPixmap pixmap = icon.pixmap(16);
  return pixmap;
}

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirModelNetworkNode::createNetworkFolderNode(
    const TFilePath &path) {
  QDir networkDir(path.getQString());
  while (networkDir.cdUp()) {
  }
  TFilePath networkDirPath(networkDir.absolutePath());

  DvDirModelFileFolderNode *child = new DvDirModelFileFolderNode(
      this, networkDirPath.getWideString(), networkDirPath);
  child->setPeeking(false);

  addChild(child);

  return child->getNodeByPath(path);
}

//=============================================================================
//
// DvDirModelRootNode [Root]
//
//-----------------------------------------------------------------------------

DvDirModelRootNode::DvDirModelRootNode()
    : DvDirModelNode(0, L"Root")
    , m_myComputerNode(0)
    , m_networkNode(0)
    , m_currentProjectNode(0)
    , m_sandboxProjectNode(0) {
  m_nodeType = "Root";
}

//-----------------------------------------------------------------------------

void DvDirModelRootNode::add(std::wstring name, const TFilePath &path) {
  DvDirModelNode *child = new DvDirModelFileFolderNode(this, name, path);
  child->setRow((int)m_children.size());
  m_children.push_back(child);
}

//-----------------------------------------------------------------------------

void DvDirModelRootNode::refreshDefaultProjectPath() {
// Windows has 1 more entry (Network) than macOS/Linux
#ifdef WIN32
  int row = 8;
#else
  int row = 7;
#endif

  if (m_projectDirNodes.size() > 0) {
    removeChildren(row, m_projectDirNodes.size());
    m_projectDirNodes.clear();
  }

  QString defaultProjectPaths =
      Preferences::instance()->getDefaultProjectPath();
  if (!defaultProjectPaths.isEmpty()) {
    QStringList projectRoots =
        defaultProjectPaths.split(";", Qt::SkipEmptyParts);
    int folderCount = 0;
    for (int i = 0; i < projectRoots.size(); i++) {
      TFilePath projectRootDir(projectRoots.at(i));
      if (!TFileStatus(projectRootDir).isDirectory()) continue;
      std::wstring folderName = L"Projects";
      if (projectRoots.size() > 1)
        folderName +=
            L" (" + projectRootDir.withoutParentDir().getWideString() + L")";
      DvDirModelSpecialFileFolderNode *projectFolderNode =
          new DvDirModelSpecialFileFolderNode(this, folderName, projectRootDir);
      projectFolderNode->setPixmap(generateIconPixmap("projects_folder"));
      m_projectDirNodes.push_back(projectFolderNode);
      insertChild(row + folderCount, projectFolderNode);
      folderCount++;
    }
  }
}

//-----------------------------------------------------------------------------

void DvDirModelRootNode::refreshChildren() {
  m_childrenValid = true;
  if (m_children.empty()) {
    addChild(m_myComputerNode = new DvDirModelMyComputerNode(this));

#ifdef _WIN32
    addChild(m_networkNode = new DvDirModelNetworkNode(this));
#endif
    DvDirModelSpecialFileFolderNode *child;
    child = new DvDirModelSpecialFileFolderNode(this, L"My Documents",
                                                getMyDocumentsPath());
    child->setPixmap(generateIconPixmap("my_documents"));
    m_specialNodes.push_back(child);
    addChild(child);

    child =
        new DvDirModelSpecialFileFolderNode(this, L"Desktop", getDesktopPath());
    child->setPixmap(generateIconPixmap("desktop"));
    m_specialNodes.push_back(child);
    addChild(child);

    child = new DvDirModelSpecialFileFolderNode(this, L"Downloads",
                                                getDownloadsPath());
    child->setPixmap(generateIconPixmap("downloads"));
    m_specialNodes.push_back(child);
    addChild(child);

    DvDirModelStuffFolderNode *childstuff = new DvDirModelStuffFolderNode(this);
    for (int i = 0; i < childstuff->getChildCount(); i++) {
      DvDirModelSpecialFileFolderNode *node =
          dynamic_cast<DvDirModelSpecialFileFolderNode *>(
              childstuff->getChild(i));
      m_specialNodes.push_back(node);
    }
    addChild(childstuff);

    child = new DvDirModelSpecialFileFolderNode(
        this, L"Favorites", ToonzFolder::getMyFavoritesFolder());
    child->setPixmap(generateIconPixmap("favorites"));
    m_specialNodes.push_back(child);
    addChild(child);

    addChild(new DvDirModelHistoryNode(this));

    QString defaultProjectPaths =
        Preferences::instance()->getDefaultProjectPath();
    if (!defaultProjectPaths.isEmpty()) {
      QStringList projectRoots =
          defaultProjectPaths.split(";", Qt::SkipEmptyParts);
      for (int i = 0; i < projectRoots.size(); i++) {
        TFilePath projectRootDir(projectRoots.at(i));
        if (!TFileStatus(projectRootDir).isDirectory()) continue;
        std::wstring folderName = L"Projects";
        if (projectRoots.size() > 1)
          folderName +=
              L" (" + projectRootDir.withoutParentDir().getWideString() + L")";
        DvDirModelSpecialFileFolderNode *projectFolderNode =
            new DvDirModelSpecialFileFolderNode(this, folderName,
                                                projectRootDir);
        projectFolderNode->setPixmap(
            QPixmap(generateIconPixmap("projects_folder")));
        m_projectDirNodes.push_back(projectFolderNode);
        addChild(projectFolderNode);
      }
    }

    TProjectManager *pm          = TProjectManager::instance();
    TFilePath sandboxProjectPath = pm->getSandboxProjectFolder();
    m_projectPaths.insert(sandboxProjectPath);
    m_sandboxProjectNode = new DvDirModelProjectNode(this, sandboxProjectPath);
    addChild(m_sandboxProjectNode);
    m_projectNodes.push_back(m_sandboxProjectNode);

    // SVN Repositories
    QList<SVNRepository> repositories =
        VersionControl::instance()->getRepositories();
    int count = repositories.size();
    for (int i = 0; i < count; i++) {
      SVNRepository repo                = repositories.at(i);
      DvDirVersionControlRootNode *node = new DvDirVersionControlRootNode(
          this, repo.m_name.toStdWString(),
          TFilePath(repo.m_localPath.toStdWString()));
      node->setRepositoryPath(repo.m_repoPath.toStdWString());
      node->setLocalPath(repo.m_localPath.toStdWString());
      node->setUserName(repo.m_username.toStdWString());
      node->setPassword(repo.m_password.toStdWString());
      m_versionControlNodes.push_back(node);
      addChild(node);
    }

    // scenefolder node (access to the parent folder of the current scene file)
    m_sceneFolderNode =
        new DvDirModelSceneFolderNode(this, L"Scene Folder", TFilePath());
    m_sceneFolderNode->setPixmap(QPixmap(":Resources/clapboard.png"));
  } else {
    RecentFiles *recent        = RecentFiles::instance();
    QList<QString> recentFiles = recent->getFilesNameList(RecentFiles::Project);
    int recentCount            = recentFiles.size();
    TProjectManager *pm        = TProjectManager::instance();
    for (auto path : recentFiles) {
      TFilePath projectPath(path);
      if (TSystem::doesExistFileOrLevel(projectPath) &&
          m_projectPaths.find(projectPath) == m_projectPaths.end() &&
          pm->isProject(projectPath)) {
        DvDirModelProjectNode *addedProjectNode =
            new DvDirModelProjectNode(this, projectPath);
        m_projectPaths.insert(projectPath);
        addChild(addedProjectNode);
        m_projectNodes.push_back(addedProjectNode);
      }
    }

    TFilePath projectPath = pm->getCurrentProjectPath().getParentDir();
    if (m_projectPaths.find(projectPath) == m_projectPaths.end()) {
      std::string rootString = projectPath.getQString().toStdString();
      m_currentProjectNode   = new DvDirModelProjectNode(this, projectPath);
      m_projectPaths.insert(projectPath);
      addChild(m_currentProjectNode);
      updateSceneFolderNodeVisibility();
    }
  }
}

//-----------------------------------------------------------------------------
DvDirModelNode *DvDirModelRootNode::getNodeByPath(const TFilePath &path) {
  // Check first for version control nodes
  DvDirModelNode *node = 0 ;
  int i;

  // search in #1 the project folders, #2 sandbox, #3 other folders in file
  // system

  // check for the scene folder if it is the first priority
  Preferences::PathAliasPriority priority =
      Preferences::instance()->getPathAliasPriority();
  if (priority == Preferences::SceneFolderAlias &&
      !m_sceneFolderNode->getPath().isEmpty()) {
    node = m_sceneFolderNode->getNodeByPath(path);
    if (node) return node;
  }

  // path could be a project, under some project root
  for (i = 0; i < (int)m_projectNodes.size(); i++) {
    node = m_projectNodes[i];
    DvDirModelProjectNode *projectNode =
        dynamic_cast<DvDirModelProjectNode *>(node);
    // search in the project folders
    if (projectNode) {
      if (projectNode->getPath() == path) return node;
      for (int j = 0; j < m_projectNodes[i]->getChildCount(); j++) {
        // for the normal folder in the project folder
        node = projectNode->getNodeByPath(path);
        if (node) return node;

        if (projectNode->isCurrent()) {
          // for the aliases in the project folder ("+drawings" etc.)
          for (int k = 0; k < projectNode->getChildCount(); k++) {
            node = projectNode->getChild(k)->getNodeByPath(path);
            if (node) return node;
          }
        }
      }
    }
  }

  // it could be on a repository, under version control
  for (i = 0; i < (int)m_versionControlNodes.size(); i++) {
    node = m_versionControlNodes[i]->getNodeByPath(path);
    if (node) return node;
  }

  if (m_projectPaths.size() > 0 && m_currentProjectNode &&
      m_currentProjectNode->getPath() == path)
    return m_currentProjectNode;

  // or it could be the sandbox project or in the sandbox project
  if (m_sandboxProjectNode && m_sandboxProjectNode->getPath() == path)
    return m_sandboxProjectNode;
  if (m_sandboxProjectNode) {
    for (i = 0; i < m_sandboxProjectNode->getChildCount(); i++) {
      DvDirModelNode *node =
          m_sandboxProjectNode->getChild(i)->getNodeByPath(path);
      if (node) return node;
    }
  }

  // check for the scene folder if it is the second priority
  if (priority == Preferences::ProjectFolderAliases &&
      !m_sceneFolderNode->getPath().isEmpty()) {
    node = m_sceneFolderNode->getNodeByPath(path);
    if (node) return node;
  }

  // check for the special folders (My Documents / Desktop / Library)
  for (DvDirModelSpecialFileFolderNode *specialNode : m_specialNodes) {
    DvDirModelNode *node = specialNode->getNodeByPath(path);
    if (node) return node;
  }

  // check for the project root folders
  for (DvDirModelSpecialFileFolderNode *projectDirNode : m_projectDirNodes) {
    DvDirModelNode *node = projectDirNode->getNodeByPath(path);
    if (node) return node;
  }

  // it could be a regular folder, somewhere in the file system
  if (m_myComputerNode) {
    for (i = 0; i < m_myComputerNode->getChildCount(); i++) {
      DvDirModelNode *node = m_myComputerNode->getChild(i)->getNodeByPath(path);
      if (node) return node;
    }
  }

  // it could be a network folder
  if (m_networkNode) {
    QString pathStr = path.getQString();
    if (pathStr.startsWith("\\\\") || pathStr.startsWith("//")) {
      for (i = 0; i < m_networkNode->getChildCount(); i++) {
        DvDirModelNode *node = m_networkNode->getChild(i)->getNodeByPath(path);
        if (node) return node;
      }

      // try to find in the network
      if (QDir(pathStr).exists()) {
        DvDirModelNode *node = m_networkNode->createNetworkFolderNode(path);
        if (node) return node;
      }
    }
  }

  return 0;
}

//-----------------------------------------------------------------------------
// update the path of sceneLocationNode

void DvDirModelRootNode::setSceneLocation(const TFilePath &path) {
  if (path == m_sceneFolderNode->getPath()) return;
  m_sceneFolderNode->setPath(path);

  Preferences::PathAliasPriority priority =
      Preferences::instance()->getPathAliasPriority();
  if (priority == Preferences::ProjectFolderOnly) return;

  updateSceneFolderNodeVisibility();
}

//-----------------------------------------------------------------------------

void DvDirModelRootNode::updateSceneFolderNodeVisibility(bool forceHide) {
  bool show = (forceHide) ? false : !m_sceneFolderNode->getPath().isEmpty();
  if (show && m_sceneFolderNode->getRow() == -1) {
    int row = getChildCount();
    DvDirModel::instance()->notifyBeginInsertRows(QModelIndex(), row, row);
    addChild(m_sceneFolderNode);
    DvDirModel::instance()->notifyEndInsertRows();
  } else if (!show && m_sceneFolderNode->getRow() != -1) {
    int row = m_sceneFolderNode->getRow();
    DvDirModel::instance()->notifyBeginRemoveRows(QModelIndex(), row, row);
    // remove the last child of the root node
    m_children.erase(m_children.begin() + row, m_children.begin() + row + 1);
    DvDirModel::instance()->notifyEndRemoveRows();
    m_sceneFolderNode->setRow(-1);
  }
}
//=============================================================================
//
// DvDirModel
//
//-----------------------------------------------------------------------------

DvDirModel::DvDirModel() {
  m_root = new DvDirModelRootNode();
  m_root->refreshChildren();

  // when the scene switched, update the path of the scene location node
  TSceneHandle *sceneHandle = TApp::instance()->getCurrentScene();
  bool ret                  = true;
  ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
                       SLOT(onSceneSwitched()));
  ret = ret && connect(sceneHandle, SIGNAL(nameSceneChanged()), this,
                       SLOT(onSceneSwitched()));
  ret = ret && connect(sceneHandle, SIGNAL(preferenceChanged(const QString &)),
                       this, SLOT(onPreferenceChanged(const QString &)));
  assert(ret);

  onSceneSwitched();
}

//-----------------------------------------------------------------------------

DvDirModel::~DvDirModel() { delete m_root; }

//-----------------------------------------------------------------------------

void DvDirModel::onFolderChanged(const TFilePath &path) { refreshFolder(path); }

//-----------------------------------------------------------------------------

void DvDirModel::refresh(const QModelIndex &index) {
  if (!index.isValid()) return;
  DvDirModelNode *node = getNode(index);
  if (!node) return;

  emit layoutAboutToBeChanged();

  node->refreshChildren();

  emit layoutChanged();
}

//-----------------------------------------------------------------------------

void DvDirModel::refreshFolder(const TFilePath &folderPath,
                               const QModelIndex &i) {
  DvDirModelNode *node = getNode(i);
  if (!node) return;
  if (node->isFolder(folderPath))
    refresh(i);
  else if (node->areChildrenValid()) {
    int count = rowCount(i);
    int r;
    for (r = 0; r < count; r++) refreshFolder(folderPath, index(r, 0, i));
  }
}

//-----------------------------------------------------------------------------

void DvDirModel::refreshFolderChild(const QModelIndex &i) {
  DvDirModelNode *node = getNode(i);
  if (!node || !node->areChildrenValid()) return;

  if (node->isFolder() || dynamic_cast<DvDirModelMyComputerNode *>(node))
    refresh(i);
  int count = rowCount(i);
  int r;
  for (r = 0; r < count; r++) refreshFolderChild(index(r, 0, i));
}

//-----------------------------------------------------------------------------

void DvDirModel::forceRefresh() { onSceneSwitched(); }

//-----------------------------------------------------------------------------

DvDirModelNode *DvDirModel::getNode(const QModelIndex &index) const {
  if (index.isValid())
    return static_cast<DvDirModelNode *>(index.internalPointer());
  else
    return m_root;
}

//-----------------------------------------------------------------------------

QModelIndex DvDirModel::index(int row, int column,
                              const QModelIndex &parent) const {
  if (column != 0) return QModelIndex();
  DvDirModelNode *parentNode = m_root;
  if (parent.isValid()) parentNode = getNode(parent);
  if (row < 0 || row >= parentNode->getChildCount()) return QModelIndex();
  DvDirModelNode *node = parentNode->getChild(row);
  return createIndex(row, column, node);
}

//-----------------------------------------------------------------------------

QModelIndex DvDirModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) return QModelIndex();
  DvDirModelNode *node       = getNode(index);
  DvDirModelNode *parentNode = node->getParent();
  if (!parentNode || parentNode == m_root)
    return QModelIndex();
  else
    return createIndex(parentNode->getRow(), 0, parentNode);
}

//-----------------------------------------------------------------------------

QModelIndex DvDirModel::childByName(const QModelIndex &parent,
                                    const std::wstring &name) const {
  if (!parent.isValid()) return QModelIndex();
  DvDirModelNode *parentNode = getNode(parent);
  if (!parentNode) return QModelIndex();
  int row = parentNode->rowByName(name);
  if (row < 0 || row >= parentNode->getChildCount()) return QModelIndex();
  DvDirModelNode *childNode = parentNode->getChild(row);
  return createIndex(row, 0, childNode);
}

//-----------------------------------------------------------------------------

int DvDirModel::rowCount(const QModelIndex &parent) const {
  DvDirModelNode *node = getNode(parent);
  return node->getChildCount();
}

//-----------------------------------------------------------------------------

bool DvDirModel::hasChildren(const QModelIndex &parent) const {
  DvDirModelNode *node = getNode(parent);
  return node->hasChildren();
}

//-----------------------------------------------------------------------------

QVariant DvDirModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();
  DvDirModelNode *node = getNode(index);
  if (role == Qt::DisplayRole || role == Qt::EditRole)
    return QString::fromStdWString(node->getName());
  else if (role == Qt::DecorationRole) {
    return QVariant();
  } else if (role == Qt::ForegroundRole) {
    if (!node || !node->isRenameEnabled())
      return QBrush(Qt::blue);
    else
      return QVariant();
  } else
    return QVariant();
}

//-----------------------------------------------------------------------------

Qt::ItemFlags DvDirModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags flags = QAbstractItemModel::flags(index);
  if (index.isValid()) {
    DvDirModelNode *node = getNode(index);
    if (node && node->isRenameEnabled()) flags |= Qt::ItemIsEditable;
  }
  return flags;
}

//-----------------------------------------------------------------------------
/*! used only for name / rename of items
 */
bool DvDirModel::setData(const QModelIndex &index, const QVariant &value,
                         int role) {
  if (!index.isValid()) return false;
  DvDirModelNode *node = getNode(index);
  if (!node || !node->isRenameEnabled()) return false;
  QString newName = value.toString();
  if (newName == "") return false;
  if (!node->setName(newName.toStdWString())) return false;
  emit dataChanged(index, index);
  return true;
}

//-----------------------------------------------------------------------------

bool DvDirModel::removeRows(int row, int count,
                            const QModelIndex &parentIndex) {
  if (!parentIndex.isValid()) return false;
  DvDirModelNode *node = getNode(parentIndex);
  if (!node) return false;
  emit beginRemoveRows(parentIndex, row, row + count - 1);
  node->removeChildren(row, count);
  emit endRemoveRows();
  return true;
}

//-----------------------------------------------------------------------------

QModelIndex DvDirModel::getIndexByPath(const TFilePath &path) const {
  return getIndexByNode(m_root->getNodeByPath(path));
}

//-----------------------------------------------------------------------------

QModelIndex DvDirModel::getIndexByNode(DvDirModelNode *node) const {
  if (!node) return QModelIndex();
  return createIndex(node->getRow(), 0, node);
}

//-----------------------------------------------------------------------------

QModelIndex DvDirModel::getCurrentProjectIndex() const {
  return getIndexByPath(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());
}

//-----------------------------------------------------------------------------

DvDirModel *DvDirModel::instance() {
  static DvDirModel _instance;
  return &_instance;
}

//-----------------------------------------------------------------------------

void DvDirModel::onSceneSwitched() {
  DvDirModelRootNode *rootNode = dynamic_cast<DvDirModelRootNode *>(m_root);
  if (rootNode) {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene) {
      int projectPaths = rootNode->getProjectPathsSize();
      m_root->refreshChildren();
      if (rootNode->getProjectPathsSize() != projectPaths) {
        TProjectManager *pm   = TProjectManager::instance();
        TFilePath projectPath = pm->getCurrentProjectPath().getParentDir();
        QModelIndex index     = getIndexByPath(projectPath);
        refresh(index);
        emit(projectAdded());
      }
      if (scene->isUntitled())
        rootNode->setSceneLocation(TFilePath());
      else
        rootNode->setSceneLocation(scene->getScenePath().getParentDir());
    }
  }
}

//-----------------------------------------------------------------------------

void DvDirModel::onPreferenceChanged(const QString &prefName) {
  DvDirModelRootNode *rootNode = dynamic_cast<DvDirModelRootNode *>(m_root);
  if (!rootNode) return;
  if (prefName == "PathAliasPriority") {
    Preferences::PathAliasPriority priority =
        Preferences::instance()->getPathAliasPriority();
    rootNode->updateSceneFolderNodeVisibility(priority ==
                                              Preferences::ProjectFolderOnly);
  } else if (prefName == "DefaultProjectPath") {
    rootNode->refreshDefaultProjectPath();
    emit layoutChanged();
  }
}
