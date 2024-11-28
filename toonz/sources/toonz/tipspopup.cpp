

#include "tipspopup.h"

#include "mainwindow.h"
#include "tenv.h"
#include "tsystem.h"
#include "tapp.h"
#include "menubarcommandids.h"

#include "toonz/preferences.h"

#include <QDir>
#include <QTextStream>
#include <QCheckBox>
#include <QTextBrowser>
#include <QScrollArea>

//=============================================================================
/*! \class TipsPopup
                \brief The TipsPopup class provides a dialog to
   bring up Tahoma2D tips

                Inherits \b Dialog.
*/
//-----------------------------------------------------------------------------

TipsPopup::TipsPopup()
    : DVGui::Dialog(TApp::instance()->getMainWindow(), true, false,
                    "TipsPopup") {
  setWindowTitle(tr("Tahoma2D Tips"));
  setObjectName("TipsPopup");
  setModal(false);

  // Pages
  m_tips = new QTextBrowser(this);
  m_tips->setOpenExternalLinks(true);

  QFrame *tipsFrame       = new QFrame(this);
  QVBoxLayout *tipsLayout = new QVBoxLayout();
  tipsLayout->setSpacing(0);
  tipsLayout->setContentsMargins(5, 5, 5, 5);
  {
    tipsLayout->addWidget(m_tips, 1);
    tipsLayout->addStretch();
  }
  tipsFrame->setLayout(tipsLayout);

  QScrollArea *tipsArea = new QScrollArea();
  tipsArea->setMinimumSize(500, 300);
  tipsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tipsArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tipsArea->setWidgetResizable(true);
  tipsArea->setWidget(tipsFrame);

  // Buttons
  m_showAtStartCB = new QCheckBox(tr("Show this at startup"), this);

  m_showAtStartCB->setChecked(Preferences::instance()->isTipsPopupEnabled());
  m_showAtStartCB->setStyleSheet("QCheckBox{ background-color: none; }");

  //---- Top layout
  m_topLayout->setContentsMargins(0, 0, 0, 0);
  m_topLayout->setSpacing(0);
  {
    m_topLayout->addWidget(tipsArea, 1);
    m_topLayout->addStretch();
  }

  //---- button layout
  m_buttonLayout->setContentsMargins(0, 0, 0, 0);
  m_buttonLayout->setSpacing(10);
  {
    m_buttonLayout->addWidget(m_showAtStartCB, 0, Qt::AlignLeft);
    m_buttonLayout->addStretch();
  }

  bool ret = true;

  ret = ret && connect(m_showAtStartCB, SIGNAL(stateChanged(int)), this,
                       SLOT(onShowAtStartChanged(int)));

  assert(ret);

  loadTips();
}

//-----------------------------------------------------------------------------

void TipsPopup::onShowAtStartChanged(int index) {
  Preferences::instance()->setValue(tipsPopupEnabled, index == Qt::Checked);
}

//-----------------------------------------------------------------------------

void TipsPopup::loadTips() {
  m_tips->setText(tr("Nothing to see here. Keep movin'..."));

  TFilePath tipsFp = TEnv::getStuffDir() + "doc" + "tips.md";

  QString language = Preferences::instance()->getCurrentLanguage();
  if (language != "English") {
    TFilePath tipsLanguageFp =
        TEnv::getStuffDir() + "doc" + language.toStdString() + "tips.md";
    if (TFileStatus(tipsLanguageFp).doesExist()) tipsFp = tipsLanguageFp;
  }

  if (!TFileStatus(tipsFp).doesExist()) return;

  m_tips->clear();
  
  QFile f(tipsFp.getQString());
  if (f.open(QFile::ReadOnly | QFile::Text)) {
    QTextStream ts(&f);
    ts.setCodec("UTF-8");
    m_tips->setMarkdown(ts.readAll());
    f.close();
  }
}

OpenPopupCommandHandler<TipsPopup> openTipsPopup(MI_OpenTips);
