#include "commandbarpopup.h"

// Tnz includes
#include "tapp.h"
#include "menubar.h"
#include "shortcutpopup.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/toonzfolders.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QtDebug>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDataStream>
#include <QMimeData>
#include <QDrag>
#include <QMouseEvent>
#include <QPainter>
#include <QApplication>
#include <QLabel>

namespace {
//-----------------------------------------------------------------------------

static QByteArray packStringList(const QStringList& lst) {
  QByteArray ba;
  QDataStream ds(&ba, QIODevice::WriteOnly);
  ds << (qint32)lst.size();
  int i;
  for (i = 0; i < lst.size(); i++) ds << lst.at(i);
  return ba;
}

//-----------------------------------------------------------------------------
static QStringList unpackStringList(const QByteArray& ba) {
  QStringList lst;
  QDataStream ds(ba);
  qint32 n = 0;
  ds >> n;
  int i;
  for (i = 0; i < n; i++) {
    QString s;
    ds >> s;
    lst.append(s);
  }
  return lst;
}
}  // namespace

//=============================================================================
// CommandItem
//-----------------------------------------------------------------------------

CommandItem::CommandItem(QTreeWidgetItem* parent, QAction* action)
    : QTreeWidgetItem(parent, UserType), m_action(action) {
  setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled |
           Qt::ItemNeverHasChildren);
  QString tempText = m_action->text();
  // removing accelerator key indicator
  tempText = tempText.replace(QRegExp("&([^& ])"), "\\1");
  // removing doubled &s
  tempText = tempText.replace("&&", "&");
  setText(0, tempText);
  setToolTip(0, QObject::tr("[Drag] to move position"));
}

//=============================================================================
// SeparatorItem
//-----------------------------------------------------------------------------

SeparatorItem::SeparatorItem(QTreeWidgetItem* parent)
    : QTreeWidgetItem(parent, UserType) {
  setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled |
           Qt::ItemNeverHasChildren);
  setText(0, QObject::tr("----Separator----"));
  setToolTip(0, QObject::tr("[Drag] to move position"));
}

//=============================================================================
// CommandListTree
//-----------------------------------------------------------------------------

CommandListTree::CommandListTree(const QString& dropTargetString,
                                 QWidget* parent, bool withSeparator)
    : m_dropTargetString(dropTargetString), QTreeWidget(parent) {
  setObjectName("SolidLineFrame");
  setAlternatingRowColors(true);
  setDragEnabled(true);
  setDragDropMode(QAbstractItemView::DragOnly);
  setColumnCount(1);
  setIconSize(QSize(21, 18));
  header()->close();

  QIcon menuFolderIcon(createQIcon("folder_project", true));
  invisibleRootItem()->setIcon(0, menuFolderIcon);

  QTreeWidgetItem* menuCommandFolder = new QTreeWidgetItem(this);
  menuCommandFolder->setFlags(Qt::ItemIsEnabled);
  menuCommandFolder->setText(
      0, "1");  // set tentative name for "Menu Commands" folder
  menuCommandFolder->setExpanded(true);
  menuCommandFolder->setIcon(0, invisibleRootItem()->icon(0));

  addFolder(ShortcutTree::tr("File"), MenuFileCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Edit"), MenuEditCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Scan & Cleanup"), MenuScanCleanupCommandType,
            menuCommandFolder);
  addFolder(ShortcutTree::tr("Level"), MenuLevelCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Xsheet"), MenuXsheetCommandType,
            menuCommandFolder);
  addFolder(ShortcutTree::tr("Cells"), MenuCellsCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Play"), MenuPlayCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Render"), MenuRenderCommandType,
            menuCommandFolder);
  addFolder(ShortcutTree::tr("View"), MenuViewCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("Windows"), MenuWindowsCommandType,
            menuCommandFolder);
  addFolder(ShortcutTree::tr("Help"), MenuHelpCommandType, menuCommandFolder);
  addFolder(ShortcutTree::tr("SubMenu Commands"), MenuCommandType,
            menuCommandFolder);

  // set tentative name for "Tools" folder
  QTreeWidgetItem* toolsFolder = addFolder("2", ToolCommandType);

  QTreeWidgetItem* advancedFolder = new QTreeWidgetItem(this);
  advancedFolder->setFlags(Qt::ItemIsEnabled);
  advancedFolder->setText(0, "3");  // set tentative name for "Advanced" folder
  advancedFolder->setIcon(0, invisibleRootItem()->icon(0));

  addFolder(ShortcutTree::tr("Fill"), FillCommandType, advancedFolder);
  QTreeWidgetItem* rcmSubFolder =
      addFolder(ShortcutTree::tr("Right-click Menu Commands"),
                RightClickMenuCommandType, advancedFolder);
  addFolder(ShortcutTree::tr("Drawing Mark"), DrawingMarkCommandType, rcmSubFolder);
  addFolder(ShortcutTree::tr("Cell Mark"), CellMarkCommandType, rcmSubFolder);
  addFolder(ShortcutTree::tr("Tool Modifiers"), ToolModifierCommandType,
            advancedFolder);
  addFolder(ShortcutTree::tr("Visualization"), VisualizationButtonCommandType,
            advancedFolder);
  addFolder(ShortcutTree::tr("Misc"), MiscCommandType, advancedFolder);
  addFolder(ShortcutTree::tr("RGBA Channels"), RGBACommandType, advancedFolder);

  sortItems(0, Qt::AscendingOrder);
  // set the actual names after sorting items
  menuCommandFolder->setText(0, ShortcutTree::tr("Menu Commands"));
  toolsFolder->setText(0, ShortcutTree::tr("Tools"));
  advancedFolder->setText(0, ShortcutTree::tr("Advanced"));

  if (withSeparator) {
    SeparatorItem* sep = new SeparatorItem(0);
    sep->setToolTip(0, QObject::tr("[Drag&Drop] to copy separator to %1")
                           .arg(m_dropTargetString));
    addTopLevelItem(sep);
  }
}

//-----------------------------------------------------------------------------

QTreeWidgetItem* CommandListTree::addFolder(const QString& title,
                                            int commandType,
                                            QTreeWidgetItem* parentFolder) {
  QTreeWidgetItem* folder;
  if (!parentFolder)
    folder = new QTreeWidgetItem(this);
  else
    folder = new QTreeWidgetItem(parentFolder);
  assert(folder);
  folder->setText(0, title);
  folder->setIcon(0, invisibleRootItem()->icon(0));

  std::vector<QAction*> actions;
  CommandManager::instance()->getActions((CommandType)commandType, actions);
  for (int i = 0; i < (int)actions.size(); i++) {
    CommandItem* item = new CommandItem(folder, actions[i]);
    item->setToolTip(0, QObject::tr("[Drag&Drop] to copy command to %1")
                            .arg(m_dropTargetString));
  }
  return folder;
}

//-----------------------------------------------------------------------------

void CommandListTree::mousePressEvent(QMouseEvent* event) {
  QTreeWidget::mousePressEvent(event);

  QList<QTreeWidgetItem*> selected = selectedItems();
  QStringList addList;
  QString dragPixmapTxt;

  foreach (QTreeWidgetItem* item2, selected) {
    CommandItem* commandItem     = dynamic_cast<CommandItem*>(item2);
    SeparatorItem* separatorItem = dynamic_cast<SeparatorItem*>(item2);
    QString itemStr;

    if (commandItem) {
      std::string id =
          CommandManager::instance()->getIdFromAction(commandItem->getAction());

      addList.append(QString::fromStdString(id));
      itemStr = commandItem->text(0);
    } else if (separatorItem) {
      addList.append("separator");
      itemStr = tr("----Separator----");
    }

    if (!itemStr.isEmpty()) {
      if (!dragPixmapTxt.isEmpty()) dragPixmapTxt.append("\n");
      dragPixmapTxt.append(itemStr);
    }
  }

  if (!addList.isEmpty()) {
    QMimeData* mimeData = new QMimeData;
    mimeData->setData("application/vnd.toonz.commandlist",
                      packStringList(addList));

    std::string dragStr = dragPixmapTxt.toStdString();

    mimeData->setText(QString::fromStdString(dragStr));

    QFontMetrics fm(QApplication::font());
    QPixmap pix(
        fm.boundingRect(QRect(0, 0, 500, 500), Qt::TextWordWrap, dragPixmapTxt)
            .adjusted(-2, -2, 2, 2)
            .size());
    QPainter painter(&pix);
    painter.fillRect(pix.rect(),
                     palette().color(QPalette::Active, QPalette::Highlight));
    painter.setPen(
        palette().color(QPalette::Active, QPalette::HighlightedText));
    painter.drawText(pix.rect(), Qt::AlignLeft, dragPixmapTxt);

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);
    drag->setPixmap(pix);

    drag->exec(Qt::CopyAction);
  }
}

//-----------------------------------------------------------------------------

void CommandListTree::displayAll(QTreeWidgetItem* item) {
  int childCount = item->childCount();
  for (int i = 0; i < childCount; ++i) {
    displayAll(item->child(i));
  }
  item->setHidden(false);
  item->setExpanded(false);
}

//-------------------------------------------------------------------

void CommandListTree::hideAll(QTreeWidgetItem* item) {
  int childCount = item->childCount();
  for (int i = 0; i < childCount; ++i) {
    hideAll(item->child(i));
  }
  item->setHidden(true);
  item->setExpanded(false);
}

//-----------------------------------------------------------------------------

void CommandListTree::searchItems(const QString& searchWord) {
  // if search word is empty, show all items
  if (searchWord.isEmpty()) {
    int itemCount = topLevelItemCount();
    for (int i = 0; i < itemCount; ++i) {
      displayAll(topLevelItem(i));
    }

    // revert to the initial state - expanding "Menu Commands" tree
    findItems(ShortcutTree::tr("Menu Commands"), Qt::MatchExactly)[0]
        ->setExpanded(true);
    update();
    return;
  }

  // hide all items first
  int itemCount = topLevelItemCount();
  for (int i = 0; i < itemCount; ++i) {
    hideAll(topLevelItem(i));
  }

  QList<QTreeWidgetItem*> foundItems =
      findItems(searchWord, Qt::MatchContains | Qt::MatchRecursive, 0);
  if (foundItems.isEmpty()) {  // if nothing is found, do nothing but update
    update();
    return;
  }

  // for each item found, show it and show its parent
  for (auto item : foundItems) {
    while (item) {
      item->setHidden(false);
      item->setExpanded(true);
      item = item->parent();
    }
  }

  update();
}

//=============================================================================
// CommandBarTree
//-----------------------------------------------------------------------------

CommandBarTree::CommandBarTree(TFilePath& path, TFilePath& defaultPath,
                               QWidget* parent)
    : QTreeWidget(parent) {
  setObjectName("SolidLineFrame");
  setAlternatingRowColors(true);
  setDragEnabled(true);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::MoveAction);
  setDragDropMode(QAbstractItemView::DragDrop);
  setIconSize(QSize(21, 17));

  setColumnCount(1);
  header()->close();

  /*- Load path if it does exist. If not, then load from the template. -*/
  TFilePath fp;
  if (TFileStatus(path).isWritable())
    fp = path;
  else if (TFileStatus(defaultPath).isWritable())
    fp = defaultPath;
  else {
    if (path.getName() == "quicktoolbar") {
      fp = ToonzFolder::getTemplateModuleDir() + TFilePath("quicktoolbar.xml");
    } else if (path.getName() == "maintoolbar") {
      fp = ToonzFolder::getTemplateModuleDir() + TFilePath("maintoolbar.xml");
    } else {
      fp = ToonzFolder::getTemplateModuleDir() + TFilePath("commandbar.xml");
    }
  }

  loadMenuTree(fp);
}

//-----------------------------------------------------------------------------

void CommandBarTree::loadMenuTree(const TFilePath& fp) {
  QFile file(toQString(fp));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return;
  }

  QXmlStreamReader reader(&file);

  if (reader.readNextStartElement()) {
    if (reader.name() == "commandbar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "command") {
          QString cmdName = reader.readElementText();

          QAction* action = CommandManager::instance()->getAction(
              cmdName.toStdString().c_str());
          if (action) {
            CommandItem* item = new CommandItem(0, action);
            addTopLevelItem(item);
          }
        } else if (reader.name() == "separator") {
          SeparatorItem* sep = new SeparatorItem(0);
          addTopLevelItem(sep);
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  }

  if (reader.hasError()) {
    qDebug() << "Cannot read menubar xml";
  }
}

//-----------------------------------------------------------------------------

void CommandBarTree::loadMenuRecursive(QXmlStreamReader& reader,
                                       QTreeWidgetItem* parentItem) {
  while (reader.readNextStartElement()) {
    if (reader.name() == "command") {
      QString cmdName = reader.readElementText();
      QAction* action =
          CommandManager::instance()->getAction(cmdName.toStdString().c_str());
      if (action) CommandItem* item = new CommandItem(parentItem, action);
    } else if (reader.name() == "command_debug") {
#ifndef NDEBUG
      QString cmdName = reader.readElementText();
      QAction* action =
          CommandManager::instance()->getAction(cmdName.toStdString().c_str());
      if (action) CommandItem* item = new CommandItem(parentItem, action);
#else
      reader.skipCurrentElement();
#endif
    } else if (reader.name() == "separator") {
      SeparatorItem* sep = new SeparatorItem(parentItem);
      reader.skipCurrentElement();
    } else
      reader.skipCurrentElement();
  }
}

//-----------------------------------------------------------------------------

void CommandBarTree::saveMenuTree(TFilePath& path) {
  QFile file(toQString(path));
  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    return;
  }

  QXmlStreamWriter writer(&file);
  writer.setAutoFormatting(true);
  writer.writeStartDocument();

  writer.writeStartElement("commandbar");
  { saveMenuRecursive(writer, invisibleRootItem()); }
  writer.writeEndElement();

  writer.writeEndDocument();
}

//-----------------------------------------------------------------------------

void CommandBarTree::saveMenuRecursive(QXmlStreamWriter& writer,
                                       QTreeWidgetItem* parentItem) {
  for (int c = 0; c < parentItem->childCount(); c++) {
    CommandItem* command = dynamic_cast<CommandItem*>(parentItem->child(c));
    SeparatorItem* sep   = dynamic_cast<SeparatorItem*>(parentItem->child(c));

    if (command)
      writer.writeTextElement(
          "command",
          QString::fromStdString(CommandManager::instance()->getIdFromAction(
              command->getAction())));
    else if (sep)
      writer.writeEmptyElement("separator");
  }
}

//-----------------------------------------------------------------------------

void CommandBarTree::dropEvent(QDropEvent* event) {
  QList<QTreeWidgetItem*> selected = selectedItems();

  QTreeWidget::dropEvent(event);

  // On move, ExtendedMode clears prior selection but doesn't reselect. 
  if (!selectedItems().size() && selected.size())
    foreach (QTreeWidgetItem* item, selected) item->setSelected(true);
};

//-----------------------------------------------------------------------------

bool CommandBarTree::dropMimeData(QTreeWidgetItem* parent, int index,
                                  const QMimeData* data,
                                  Qt::DropAction action) {
  QStringList commandList =
      unpackStringList(data->data("application/vnd.toonz.commandlist"));

  if (commandList.isEmpty()) return false;

  clearSelection();

  foreach (QString txt, commandList) {
    QTreeWidgetItem* item;
    if (txt == "separator")
      item = new SeparatorItem(0);
    else {
      QAction* act =
          CommandManager::instance()->getAction(txt.toStdString().c_str());
      if (!act) continue;
      item = new CommandItem(0, act);
    }

    if (parent)
      parent->insertChild(index, item);
    else
      insertTopLevelItem(index, item);

    item->setSelected(true);
  }

  return true;
}

//-----------------------------------------------------------------------------

QStringList CommandBarTree::mimeTypes() const {
  QStringList qstrList;
  qstrList.append("text/plain");
  return qstrList;
}

//-----------------------------------------------------------------------------

void CommandBarTree::contextMenuEvent(QContextMenuEvent* event) {
  QTreeWidgetItem* item = itemAt(event->pos());
  if (item != currentItem()) setCurrentItem(item);
  if (!item) return;

  QMenu* menu = new QMenu(this);
  QAction* action;

  QList<QTreeWidgetItem*> selected = selectedItems();

  if (selected.count() == 1)
    action = menu->addAction(tr("Remove \"%1\"").arg(item->text(0)));
  else
    action = menu->addAction(tr("Remove Selected Commands"));

  connect(action, SIGNAL(triggered()), this, SLOT(removeItem()));

  menu->exec(event->globalPos());
  delete menu;
}

//-----------------------------------------------------------------------------

void CommandBarTree::removeItem() {
  QList<QTreeWidgetItem*> selected = selectedItems();

  if (selected.isEmpty()) return;

  foreach (QTreeWidgetItem* item, selected) {
    takeTopLevelItem(indexOfTopLevelItem(item));
    delete item;
  }
}

//=============================================================================
//-----------------------------------------------------------------------------

void CommandListTree::onItemClicked(const QModelIndex& index) {
  isExpanded(index) ? collapse(index) : expand(index);
}

//=============================================================================
// CommandBarPopup
//-----------------------------------------------------------------------------

CommandBarPopup::CommandBarPopup(QString barId, CommandBarType barType)
    : Dialog(TApp::instance()->getMainWindow(), true, false,
             "CustomizeCommandBar") {
  QLabel* commandBarLabel;
  if (barType == CommandBarType::Quick) {
    m_defaultPath = m_path =
        ToonzFolder::getMyModuleDir() + TFilePath("quicktoolbar.xml");
    commandBarLabel = new QLabel(tr("Quick Toolbar"));
    setWindowTitle(tr("Customize Quick Toolbar"));
  } else if (barType == CommandBarType::Main) {
    m_defaultPath = m_path =
        ToonzFolder::getMyModuleDir() + TFilePath("maintoolbar.xml");
    commandBarLabel = new QLabel(tr("Main Toolbar"));
    setWindowTitle(tr("Customize Main Toolbar"));
  } else {
    m_defaultPath = m_path =
        ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
    if (!barId.isEmpty()) {
      m_path = ToonzFolder::getMyModuleDir() + TFilePath("commandbars") +
               TFilePath("commandbar_" + barId + ".xml");
      TSystem::touchParentDir(m_path);
    }
    commandBarLabel = new QLabel(tr("Command Bar"));
    setWindowTitle(tr("Customize Command Bar"));
  }

  m_commandListTree = new CommandListTree(commandBarLabel->text(), this);
  m_commandListTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_menuBarTree = new CommandBarTree(m_path, m_defaultPath, this);
  m_menuBarTree->setSelectionMode(QAbstractItemView::ExtendedSelection);

  QPushButton* okBtn     = new QPushButton(tr("OK"), this);
  okBtn->setFocusPolicy(Qt::NoFocus);

  QPushButton* cancelBtn = new QPushButton(tr("Cancel"), this);
  cancelBtn->setFocusPolicy(Qt::NoFocus);

  m_moveItemUpBtn = new QPushButton(this);
  m_moveItemUpBtn->setFixedSize(QSize(22, 20));
  m_moveItemUpBtn->setIconSize(QSize(20, 20));
  m_moveItemUpBtn->setToolTip(tr("Move Up"));
  m_moveItemUpBtn->setIcon(createQIcon("folder_arrow_up"));

  m_moveItemDownBtn = new QPushButton(this);
  m_moveItemDownBtn->setFixedSize(QSize(22, 20));
  m_moveItemDownBtn->setIconSize(QSize(20, 20));
  m_moveItemDownBtn->setToolTip(tr("Move Down"));
  m_moveItemDownBtn->setIcon(createQIcon("folder_arrow_down"));

  m_removeItemBtn = new QPushButton(this);
  m_removeItemBtn->setFixedSize(QSize(22, 20));
  m_removeItemBtn->setIconSize(QSize(20, 20));
  m_removeItemBtn->setToolTip(tr("Remove"));
  m_removeItemBtn->setIcon(createQIcon("folder_arrow_left"));

  m_addItemBtn = new QPushButton(this);
  m_addItemBtn->setFixedSize(QSize(22, 20));
  m_addItemBtn->setIconSize(QSize(20, 20));
  m_addItemBtn->setToolTip(tr("Add"));
  m_addItemBtn->setIcon(createQIcon("folder_arrow_right"));

  m_addItemBtn->setEnabled(false);
  m_moveItemUpBtn->setEnabled(false);
  m_moveItemDownBtn->setEnabled(false);
  m_removeItemBtn->setEnabled(false);

  m_saveAsDefaultCB = new QCheckBox(tr("Save as Default"));

  QLabel* commandItemListLabel = new QLabel(tr("Commands"), this);

  QFont f("Arial", 15, QFont::Bold);
  commandBarLabel->setFont(f);
  commandItemListLabel->setFont(f);

  QLabel* noticeLabel =
      new QLabel(tr("Duplicated commands will be ignored. Only "
                    "the last one will appear in the toolbar."),
                 this);
  QFont nf("Arial", 9, QFont::Normal);
  nf.setItalic(true);
  noticeLabel->setFont(nf);

  QLineEdit* searchEdit = new QLineEdit(this);

  //--- layout
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->setSpacing(0);
  {
    QGridLayout* mainUILay = new QGridLayout();
    mainUILay->setContentsMargins(5, 5, 5, 5);
    mainUILay->setHorizontalSpacing(8);
    mainUILay->setVerticalSpacing(5);
    {
      // Left Column (Commands)
      mainUILay->addWidget(commandItemListLabel, 0, 0);
      QHBoxLayout* searchLay = new QHBoxLayout();
      searchLay->setContentsMargins(0, 0, 0, 0);
      searchLay->setSpacing(5);
      {
        searchLay->addWidget(new QLabel(tr("Search:"), this), 0);
        searchLay->addWidget(searchEdit);
      }
      mainUILay->addLayout(searchLay, 1, 0);
      mainUILay->addWidget(m_commandListTree, 2, 0);

      // Center Column (Buttons)
      QVBoxLayout* buttonLay = new QVBoxLayout();
      buttonLay->setContentsMargins(0, 0, 0, 0);
      buttonLay->setSpacing(10);
      {
        buttonLay->addStretch(1);
        buttonLay->addWidget(m_moveItemUpBtn);
        buttonLay->addWidget(m_addItemBtn);
        buttonLay->addWidget(m_removeItemBtn);
        buttonLay->addWidget(m_moveItemDownBtn);
        buttonLay->addStretch(1);
      }
      mainUILay->addLayout(buttonLay, 2, 1);

      // Right Column (Command Bar)
      mainUILay->addWidget(commandBarLabel, 0, 2);
      mainUILay->addWidget(m_menuBarTree, 1, 2, 2, 1);

      // Bottom Row
      mainUILay->addWidget(noticeLabel, 3, 0, 1, 3);
    }
    mainUILay->setRowStretch(0, 0);
    mainUILay->setRowStretch(1, 0);
    mainUILay->setRowStretch(2, 1);
    mainUILay->setColumnStretch(0, 1);
    mainUILay->setColumnStretch(1, 1);
    mainUILay->setColumnStretch(2, 1);

    m_topLayout->addLayout(mainUILay, 1);
  }

  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->setSpacing(0);
  {
    QGridLayout* buttonGridLay = new QGridLayout();
    buttonGridLay->setContentsMargins(0, 0, 0, 0);
    buttonGridLay->setHorizontalSpacing(8);
    buttonGridLay->setVerticalSpacing(0);
    {
      if (barType == CommandBarType::Command)
        buttonGridLay->addWidget(m_saveAsDefaultCB, 0, 0);

      QHBoxLayout* buttonsLay = new QHBoxLayout();
      buttonsLay->setContentsMargins(0, 0, 0, 0);
      buttonsLay->setSpacing(30);
      {
        buttonsLay->addWidget(okBtn, 0);
        buttonsLay->addWidget(cancelBtn, 0);
      }
      buttonGridLay->addLayout(buttonsLay, 0, 1);

      buttonGridLay->setColumnStretch(0, 1);
      buttonGridLay->setColumnStretch(1, 1);
      buttonGridLay->setColumnStretch(2, 1);
    }
    m_buttonLayout->addLayout(buttonGridLay, 0);
  }

  //--- signal/slot connections

  bool ret = connect(okBtn, SIGNAL(clicked()), this, SLOT(onOkPressed()));
  ret      = ret && connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
  ret = ret && connect(searchEdit, SIGNAL(textChanged(const QString&)), this,
                       SLOT(onSearchTextChanged(const QString&)));


  ret = ret &&
        connect(m_moveItemUpBtn, SIGNAL(clicked()), this, SLOT(onMoveItemUp()));
  ret = ret && connect(m_moveItemDownBtn, SIGNAL(clicked()), this,
                       SLOT(onMoveItemDown()));
  ret =
      ret && connect(m_addItemBtn, SIGNAL(clicked()), this, SLOT(onAddItem()));
  ret = ret &&
        connect(m_removeItemBtn, SIGNAL(clicked()), this, SLOT(onRemoveItem()));

  ret = ret && connect(m_commandListTree, SIGNAL(itemSelectionChanged()), this,
                       SLOT(onCommandListSelectionChanged()));
  ret = ret && connect(m_menuBarTree, SIGNAL(itemSelectionChanged()), this,
                       SLOT(onMenuBarSelectionChanged()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void CommandBarPopup::onOkPressed() {
  if (m_saveAsDefaultCB->isChecked())
    m_menuBarTree->saveMenuTree(m_defaultPath);
  else
    m_menuBarTree->saveMenuTree(m_path);

  accept();
}

void CommandBarPopup::onSearchTextChanged(const QString& text) {
  static bool busy = false;
  if (busy) return;
  busy = true;
  m_commandListTree->searchItems(text);
  busy = false;
}

//-----------------------------------------------------------------------------

void CommandBarPopup::onMoveItemUp() {
  QList<QTreeWidgetItem*> selected = m_menuBarTree->selectedItems();

  if (selected.isEmpty()) return;

  int topIndex = m_menuBarTree->topLevelItemCount() - 1;

  foreach (QTreeWidgetItem* item, selected) {
    topIndex = std::min(topIndex, m_menuBarTree->indexOfTopLevelItem(item));
    if (topIndex == 0) return;
  }

  foreach (QTreeWidgetItem* item, selected) {
    m_menuBarTree->takeTopLevelItem(m_menuBarTree->indexOfTopLevelItem(item));
    m_menuBarTree->insertTopLevelItem(topIndex - 1, item);
    topIndex++;
  }

  m_menuBarTree->clearSelection();

  foreach (QTreeWidgetItem* item, selected) item->setSelected(true);
}

//-----------------------------------------------------------------------------

void CommandBarPopup::onMoveItemDown() {
  QList<QTreeWidgetItem*> selected = m_menuBarTree->selectedItems();

  if (selected.isEmpty()) return;

  int topIndex  = 0;
  int lastIndex = m_menuBarTree->topLevelItemCount() - 1;

  foreach (QTreeWidgetItem* item, selected) {
    topIndex = std::max(topIndex, m_menuBarTree->indexOfTopLevelItem(item));
    if (topIndex >= lastIndex) return;
  }

  foreach (QTreeWidgetItem* item, selected)
    m_menuBarTree->removeItemWidget(item, 0);

  foreach (QTreeWidgetItem* item, selected) {
    m_menuBarTree->takeTopLevelItem(m_menuBarTree->indexOfTopLevelItem(item));
    m_menuBarTree->insertTopLevelItem(topIndex + 1, item);
  }

  m_menuBarTree->clearSelection();

  foreach (QTreeWidgetItem* item, selected) item->setSelected(true);
}

//-----------------------------------------------------------------------------

void CommandBarPopup::onAddItem() {
  QList<QTreeWidgetItem*> selected = m_commandListTree->selectedItems();
  QList<QTreeWidgetItem*> newSelection;

  if (selected.isEmpty()) return;

  int topIndex = m_menuBarTree->topLevelItemCount();

  foreach (QTreeWidgetItem* item, m_menuBarTree->selectedItems())
    topIndex = std::min(topIndex, m_menuBarTree->indexOfTopLevelItem(item));

  foreach (QTreeWidgetItem* item, selected) {
    CommandItem* commandItem     = dynamic_cast<CommandItem*>(item);
    SeparatorItem* separatorItem = dynamic_cast<SeparatorItem*>(item);

    std::string itemStr;
    if (commandItem) {
      itemStr =
          CommandManager::instance()->getIdFromAction(commandItem->getAction());
      QAction* act = CommandManager::instance()->getAction(itemStr.c_str());
      if (!act) continue;
      CommandItem* newItem = new CommandItem(0, act);
      m_menuBarTree->insertTopLevelItem(topIndex++, newItem);
      newSelection.append(newItem);
    } else if (separatorItem) {
      SeparatorItem* sep = new SeparatorItem(0);
      m_menuBarTree->insertTopLevelItem(topIndex++, sep);
      newSelection.append(sep);
    }
  }

  if (newSelection.isEmpty()) return;

  m_menuBarTree->clearSelection();

  foreach (QTreeWidgetItem* item, newSelection) item->setSelected(true);
}

//-----------------------------------------------------------------------------

void CommandBarPopup::onRemoveItem() {
  QList<QTreeWidgetItem*> selected = m_menuBarTree->selectedItems();

  if (selected.isEmpty()) return;

  foreach (QTreeWidgetItem* item, selected) {
    m_menuBarTree->takeTopLevelItem(m_menuBarTree->indexOfTopLevelItem(item));
    delete item;
  }
}

//-----------------------------------------------------------------------------

void CommandBarPopup::onCommandListSelectionChanged() {
  QList<QTreeWidgetItem*> selected = m_commandListTree->selectedItems();

  bool hasSelection = false;

  foreach (QTreeWidgetItem* item, selected) {
    CommandItem* commandItem     = dynamic_cast<CommandItem*>(item);
    SeparatorItem* separatorItem = dynamic_cast<SeparatorItem*>(item);
    if (commandItem || separatorItem) {
      hasSelection = true;
      break;
    }
  }

  m_addItemBtn->setEnabled(hasSelection);
}

//-----------------------------------------------------------------------------

void CommandBarPopup::onMenuBarSelectionChanged() {
  bool hasSelection = m_menuBarTree->selectedItems().size() > 0;

  m_moveItemUpBtn->setEnabled(hasSelection);
  m_moveItemDownBtn->setEnabled(hasSelection);
  m_removeItemBtn->setEnabled(hasSelection);
}
