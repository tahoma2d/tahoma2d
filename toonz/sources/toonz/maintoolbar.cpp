#include "maintoolbar.h"

#include "commandbar.h"
#include "commandbarpopup.h"
#include "tsystem.h"

#include "toonz/toonzfolders.h"

#include <QMenu>
#include <QContextMenuEvent>

MainToolbar::MainToolbar(QWidget *parent)
    : CommandBar(parent, Qt::WindowFlags(), false, CommandBarType::Main) {
  setAllowedAreas(Qt::TopToolBarArea);
  setMovable(false);
  setFloatable(false);
  setFixedHeight(29);
  setIconSize(QSize(20, 20));
}

MainToolbar::~MainToolbar() {}

void MainToolbar::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu                  = new QMenu(this);
  QAction *customizeCommandBar = menu->addAction(tr("Customize Main Toolbar"));
  connect(customizeCommandBar, SIGNAL(triggered()),
          SLOT(doCustomizeCommandBar()));

  menu->addSeparator();

  QAction *resetCommandBar = menu->addAction(tr("Reset Main Toolbar"));
  connect(resetCommandBar, SIGNAL(triggered()), SLOT(doResetCommandBar()));
  resetCommandBar->setEnabled(!isDefault());

  menu->exec(event->globalPos());
}

void MainToolbar::doCustomizeCommandBar() {
  CommandBarPopup *cbPopup = new CommandBarPopup("", CommandBarType::Main);

  if (cbPopup->exec()) {
    fillToolbar(this, CommandBarType::Main);
  }
  delete cbPopup;
}

//-----------------------------------------------------------------------------

void MainToolbar::doResetCommandBar() {
  TFilePath personalPath =
      ToonzFolder::getMyModuleDir() + TFilePath("maintoolbar.xml");

  if (TSystem::doesExistFileOrLevel(personalPath))
    TSystem::deleteFile(personalPath);

  fillToolbar(this, CommandBarType::Main);
}
