

#include "commandbar.h"

// Tnz6 includes
#include "tapp.h"
#include "menubarcommandids.h"
#include "tsystem.h"
#include "commandbarpopup.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/toonzfolders.h"

// Qt includes
#include <QWidgetAction>
#include <QXmlStreamReader>
#include <QtDebug>
#include <QMenuBar>
#include <QContextMenuEvent>
#include <QToolButton>

//=============================================================================
// Toolbar
//-----------------------------------------------------------------------------

CommandBar::CommandBar(QWidget *parent, Qt::WindowFlags flags,
                       bool isCollapsible, CommandBarType barType)
    : QToolBar(parent)
    , m_isCollapsible(isCollapsible)
    , m_barType(barType)
    , m_barId("")
    , m_isDefault(true) {
  setObjectName("cornerWidget");
  setObjectName("CommandBar");
  if (barType == CommandBarType::Command) {
    QDateTime date = QDateTime::currentDateTime();
    m_barId        = date.toString("yyyyMMddhhmmss");
  }
  // Sets up default.
  fillToolbar(this, m_barType, m_barId);
  setIconSize(QSize(20, 20));
  QIcon moreIcon(":Resources/more.svg");
  QToolButton *more = findChild<QToolButton *>("qt_toolbar_ext_button");
  more->setIcon(moreIcon);

  if (barType == CommandBarType::Command)
    connect(parentWidget(), SIGNAL(closeButtonPressed()), this,
            SLOT(onCloseButtonPressed()));
}

//-----------------------------------------------------------------------------

// SaveLoadQSettings
void CommandBar::save(QSettings &settings, bool forPopupIni) const {
  // Don't save barId during popup.ini save
  if (forPopupIni) return;
  settings.setValue("barId", m_barId);
}

//-----------------------------------------------------------------------------

void CommandBar::load(QSettings &settings) {
  QVariant barId = settings.value("barId");
  if (barId.canConvert(QVariant::String)) {
    m_barId = barId.toString();
    fillToolbar(this, m_barType, m_barId);
  }
}

//-----------------------------------------------------------------------------

void CommandBar::fillToolbar(CommandBar *toolbar, CommandBarType barType,
                             QString barId) {
  toolbar->clear();
  toolbar->setDefault(true);
  TFilePath personalPath;
  bool fileFound = false;
  if (barType == CommandBarType::Quick) {
    personalPath =
        ToonzFolder::getMyModuleDir() + TFilePath("quicktoolbar.xml");
  } else if (barType == CommandBarType::Main) {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("maintoolbar.xml");
  } else if (!barId.isEmpty()) {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbars") +
                   TFilePath("commandbar_" + barId + ".xml");
    if (!TSystem::doesExistFileOrLevel(personalPath))
      personalPath =
          ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
  } else {
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
  }

  fileFound = TSystem::doesExistFileOrLevel(personalPath);
  toolbar->setDefault(!fileFound);

  if (!fileFound) {
    if (barType == CommandBarType::Quick) {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("quicktoolbar.xml");
    } else if (barType == CommandBarType::Main) {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("maintoolbar.xml");
    } else {
      personalPath =
          ToonzFolder::getTemplateModuleDir() + TFilePath("commandbar.xml");
    }
  }

  QFile file(toQString(personalPath));
  if (!file.open(QFile::ReadOnly | QFile::Text)) {
    qDebug() << "Cannot read file" << file.errorString();
    buildDefaultToolbar(toolbar);
    return;
  }

  QXmlStreamReader reader(&file);

  if (reader.readNextStartElement()) {
    if (reader.name() == "commandbar") {
      while (reader.readNextStartElement()) {
        if (reader.name() == "command") {
          QString cmdName    = reader.readElementText();
          std::string cmdStr = cmdName.toStdString();
          QAction *action =
              CommandManager::instance()->getAction(cmdStr.c_str());
          if (action) toolbar->addAction(action);
        } else if (reader.name() == "separator") {
          toolbar->addSeparator();
          reader.skipCurrentElement();
        } else
          reader.skipCurrentElement();
      }
    } else
      reader.raiseError(QObject::tr("Incorrect file"));
  } else {
    reader.raiseError(QObject::tr("Cannot Read XML File"));
  }

  if (reader.hasError()) {
    buildDefaultToolbar(toolbar);
    return;
  }
}

//-----------------------------------------------------------------------------

void CommandBar::buildDefaultToolbar(CommandBar *toolbar) {
  toolbar->clear();
  TApp *app = TApp::instance();
  {
    QAction *newVectorLevel =
        CommandManager::instance()->getAction("MI_NewVectorLevel");
    toolbar->addAction(newVectorLevel);
    QAction *newToonzRasterLevel =
        CommandManager::instance()->getAction("MI_NewToonzRasterLevel");
    toolbar->addAction(newToonzRasterLevel);
    QAction *newRasterLevel =
        CommandManager::instance()->getAction("MI_NewRasterLevel");
    toolbar->addAction(newRasterLevel);
    toolbar->addSeparator();
    QAction *reframeOnes = CommandManager::instance()->getAction("MI_Reframe1");
    toolbar->addAction(reframeOnes);
    QAction *reframeTwos = CommandManager::instance()->getAction("MI_Reframe2");
    toolbar->addAction(reframeTwos);
    QAction *reframeThrees =
        CommandManager::instance()->getAction("MI_Reframe3");
    toolbar->addAction(reframeThrees);

    toolbar->addSeparator();

    QAction *repeat = CommandManager::instance()->getAction("MI_Dup");
    toolbar->addAction(repeat);

    toolbar->addSeparator();

    QAction *collapse = CommandManager::instance()->getAction("MI_Collapse");
    toolbar->addAction(collapse);
    QAction *open = CommandManager::instance()->getAction("MI_OpenChild");
    toolbar->addAction(open);
    QAction *leave = CommandManager::instance()->getAction("MI_CloseChild");
    toolbar->addAction(leave);
    QAction *editInPlace =
        CommandManager::instance()->getAction("MI_ToggleEditInPlace");
    toolbar->addAction(editInPlace);
  }
}

//-----------------------------------------------------------------------------

void CommandBar::onCloseButtonPressed() {
  if (m_barType != CommandBarType::Command || m_barId.isEmpty()) return;

  TFilePath commandbarFile = ToonzFolder::getMyModuleDir() +
                             TFilePath("commandbars") +
                             TFilePath("commandbar_" + m_barId + ".xml");
  if (!TSystem::doesExistFileOrLevel(commandbarFile)) return;

  TSystem::deleteFile(commandbarFile);
}

//-----------------------------------------------------------------------------

void CommandBar::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu                  = new QMenu(this);
  QAction *customizeCommandBar = menu->addAction(tr("Customize Command Bar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));

  menu->addSeparator();

  QAction *resetCommandBar = menu->addAction(tr("Reset Command Bar"));
  connect(resetCommandBar, SIGNAL(triggered()), SLOT(doResetCommandBar()));
  resetCommandBar->setEnabled(!isDefault());

  menu->exec(event->globalPos());
}

//-----------------------------------------------------------------------------

void CommandBar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup(m_barId);

  if (cbPopup->exec()) {
    fillToolbar(this, m_barType, m_barId);
  }
  delete cbPopup;
}

//-----------------------------------------------------------------------------

void CommandBar::doResetCommandBar() {
  TFilePath personalPath;

  switch (m_barType) {
  case CommandBarType::Main:
    personalPath = ToonzFolder::getMyModuleDir() + TFilePath("maintoolbar.xml");
    break;
  case CommandBarType::Quick:
    personalPath =
        ToonzFolder::getMyModuleDir() + TFilePath("quicktoolbar.xml");
    break;
  default:
    if (!m_barId.isEmpty()) {
      personalPath = ToonzFolder::getMyModuleDir() + TFilePath("commandbars") +
                     TFilePath("commandbar_" + m_barId + ".xml");
      if (!TSystem::doesExistFileOrLevel(personalPath)) {
        personalPath =
            ToonzFolder::getMyModuleDir() + TFilePath("commandbar.xml");
      }
      break;
    }
  }

  if (TSystem::doesExistFileOrLevel(personalPath))
    TSystem::deleteFile(personalPath);

  fillToolbar(this, m_barType, m_barId);
}
