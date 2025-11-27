#include "gphotoeventlogpopup.h"

#include "stopmotion.h"
#include "menubarcommandids.h"
#include "toonzqt/menubarcommand.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTabletEvent>
#include <QGestureEvent>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QApplication>
#include <QClipboard>

//=============================================================================
// GPhotoEventLog
//-----------------------------------------------------------------------------

GPhotoEventLogPopup::GPhotoEventLogPopup(QWidget *parent)
    : QSplitter(parent), m_logging(true), m_lastMsgCount(0) {
  setWindowFlags(Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("GPhoto Event Log"));

  // style sheet
  setObjectName("GPhotoEventLog");
  setFrameStyle(QFrame::StyledPanel);

  //-------Left side
  m_toggleAllOnOff = new QCheckBox(tr("Capture messages:"), this);
  m_toggleAllOnOff->setChecked(false);

  for (int i = 0; i < GPHOTOMSG::Count; i++) {
    m_eventCheckBox[i] = new QCheckBox(this);
    switch (i) {
    case GPHOTOMSG::GPEvent:
      m_eventCheckBox[i]->setText(tr("Event"));
      m_eventCheckBox[i]->setChecked(true);
      m_msgTypeCount++;
      break;
    case GPHOTOMSG::GPError:
      m_eventCheckBox[i]->setText(tr("Error"));
      m_eventCheckBox[i]->setChecked(true);
      m_msgTypeCount++;
      break;
    case GPHOTOMSG::GPDebug:
      m_eventCheckBox[i]->setText(tr("Debug"));
      break;
    }
  }

  QPushButton *listconfigBtn = new QPushButton(tr("List All Configs"), this);

  QFrame *filterBox          = new QFrame(this);
  QVBoxLayout *vFilterLayout = new QVBoxLayout(filterBox);
  vFilterLayout->setContentsMargins(10, 10, 10, 10);
  vFilterLayout->setSpacing(5);
  {
    vFilterLayout->addWidget(m_toggleAllOnOff);

    QVBoxLayout *vEventListLayout = new QVBoxLayout(filterBox);
    vEventListLayout->setContentsMargins(10, 0, 10, 10);
    vEventListLayout->setSpacing(5);
    {
      for (int i = 0; i < GPHOTOMSG::Count; i++) {
        vEventListLayout->addWidget(m_eventCheckBox[i]);
      }
    }
    vFilterLayout->addLayout(vEventListLayout);
  }
  vFilterLayout->addStretch();
  vFilterLayout->addWidget(listconfigBtn);

  filterBox->setLayout(vFilterLayout);

  addWidget(filterBox);


  connect(listconfigBtn, SIGNAL(pressed()), this, SLOT(onListAllConfigButtonPressed()));

  connect(m_toggleAllOnOff, SIGNAL(stateChanged(int)), this,
          SLOT(onToggleAllOnOff()));

  for (int i = 0; i < GPHOTOMSG::Count; i++) {
    connect(m_eventCheckBox[i], SIGNAL(stateChanged(int)), this,
            SLOT(onEventFilterUpdated()));
  }
  //------end left side

  //------begin right side

  m_pauseBtn = new QPushButton(tr("Pause"), this);
  connect(m_pauseBtn, SIGNAL(pressed()), this, SLOT(onPauseButtonPressed()));
  QPushButton *copyBtn = new QPushButton(tr("Copy to Clipboard"), this);
  connect(copyBtn, SIGNAL(pressed()), this, SLOT(onCopyButtonPressed()));
  QPushButton *clearBtn = new QPushButton(tr("Clear Log"), this);
  connect(clearBtn, SIGNAL(pressed()), this, SLOT(onClearButtonPressed()));

  m_eventLog = new QTextEdit(this);
  m_eventLog->setReadOnly(true);
  m_eventLog->setLineWrapMode(QTextEdit::LineWrapMode::WidgetWidth);
  m_eventLog->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  QFrame *logBox = new QFrame(this);

  QVBoxLayout *vLogLayout = new QVBoxLayout(logBox);

  vLogLayout->addWidget(m_eventLog);

  QHBoxLayout *btnLayout = new QHBoxLayout();
  btnLayout->addWidget(m_pauseBtn);
  btnLayout->addWidget(copyBtn);
  btnLayout->addWidget(clearBtn);

  vLogLayout->addLayout(btnLayout);

  logBox->setLayout(vFilterLayout);

  addWidget(logBox);

  //------end right side (the task sheet)

  setStretchFactor(1, 2);

  GPhotoEventLogManager::instance()->setGPhotoEventLogPopup(this);
}

//--------------------------------------------------

void GPhotoEventLogPopup::onToggleAllOnOff() {
  bool enable = m_msgTypeCount != GPHOTOMSG::Count ? true : false;

  m_msgTypeCount = enable ? GPHOTOMSG::Count : 0;

  for (int i = 0; i < GPHOTOMSG::Count; i++) {
    m_eventCheckBox[i]->blockSignals(true);
    m_eventCheckBox[i]->setChecked(enable);
    m_eventCheckBox[i]->blockSignals(false);
  }
}

//--------------------------------------------------

void GPhotoEventLogPopup::onEventFilterUpdated() {
  QWidget *senderWidget = static_cast<QWidget *>(sender());
  if (static_cast<QCheckBox *>(senderWidget)->isChecked())
    m_msgTypeCount++;
  else
    m_msgTypeCount--;

  m_toggleAllOnOff->blockSignals(true);
  m_toggleAllOnOff->setChecked(false);
  m_toggleAllOnOff->blockSignals(false);
}

//--------------------------------------------------

void GPhotoEventLogPopup::addEventMessage(GPHOTOMSG msgType, QString msg) {
  if (!m_logging) return;

  QString eventMsg;

  switch (msgType) {
  case GPHOTOMSG::GPEvent: {
    if (!m_eventCheckBox[GPHOTOMSG::GPEvent]->isChecked()) return;
    eventMsg = "[GP_EVENT]: " + msg;
  } break;

  case GPHOTOMSG::GPError: {
    if (!m_eventCheckBox[GPHOTOMSG::GPError]->isChecked()) return;
    eventMsg = "[GP_ERROR]: " + msg;
  } break;

  case GPHOTOMSG::GPDebug: {
    if (!m_eventCheckBox[GPHOTOMSG::GPDebug]->isChecked()) return;
    eventMsg = "[GP_DEBUG]: " + msg;
  } break;

  default:
    eventMsg = "[GP_UNKNOWN]: " + msg;
    return;
  }

  if (m_lastMsg == eventMsg) {
    m_lastMsgCount++;
    m_eventLog->undo();
    m_eventLog->append(m_lastMsg + " [x" + QString::number(m_lastMsgCount) +
                       "]");
    m_eventLog->moveCursor(QTextCursor::End);
    return;
  }

  m_eventLog->append(eventMsg);
  m_eventLog->moveCursor(QTextCursor::End);

  m_lastMsg      = eventMsg;
  m_lastMsgCount = 1;
}

//--------------------------------------------------

void GPhotoEventLogPopup::onPauseButtonPressed() {
  m_logging = !m_logging;
  if (m_logging) {
    m_pauseBtn->setText("Pause");
    m_eventLog->append("*** Event logging resumed ***");
  } else {
    m_pauseBtn->setText("Resume");
    m_eventLog->append("*** Event logging paused ***");
  }
}

//--------------------------------------------------

void GPhotoEventLogPopup::onCopyButtonPressed() {
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(m_eventLog->toPlainText());
}

//--------------------------------------------------

void GPhotoEventLogPopup::onClearButtonPressed() { m_eventLog->clear(); }

//--------------------------------------------------

void GPhotoEventLogPopup::onListAllConfigButtonPressed() {
  StopMotion *stopMotion = StopMotion::instance();
  if (!stopMotion) return;

  GPhotoCam *cam = stopMotion->m_gphotocam;
  if (!cam) return;

  if (!cam->m_sessionOpen) {
    m_eventLog->append(tr("List All Configs: No camera detected."));
    return;
  }

  QList<GPConfig> gpConfigs = cam->getCameraAllConfigs();
  if (gpConfigs.isEmpty()) {
    m_eventLog->append(tr("List All Configs: No configurations retrieved."));
    return;
  }

  m_eventLog->append(tr("========== All Configs =========="));
  for (int i = 0; i < gpConfigs.count(); i++) {
    GPConfig config = gpConfigs[i];
    if(i > 0) m_eventLog->append("-----");
    std::string s = config.section;
    std::string k = config.key;
    QString fullKey =
        "/main/" + QString::fromStdString(s) + "/" + QString::fromStdString(k);
    m_eventLog->append(fullKey);
    m_eventLog->append(tr("Label: ")+config.label);
    m_eventLog->append(tr("Readonly: ") + config.readOnly);
    m_eventLog->append(tr("Type: ") + config.widgetType);
    m_eventLog->append(tr("Current: ") + config.currentVal);
    if (config.widgetType == "DATE") {
      // Convert to local time
      time_t epoch      = config.currentVal.toInt();
      std::tm *local_tm = std::localtime(&epoch);
      char buffer[80];
      std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", local_tm);
      m_eventLog->append(tr("Printable: ") + QString::fromStdString(buffer));
    }
    for (int j = 0; j < config.choices.count(); j++) {
      if (config.widgetType == "MENU" || config.widgetType == "RADIO") {
        m_eventLog->append(tr("Choice ") + (QString::number(j+1)) + ": " + config.choices[j]);
      } else if (config.widgetType == "RANGE") {
        switch (j) {
        case 0:
          m_eventLog->append(tr("Bottom: ") + config.choices[j]);
          break;
        case 1:
          m_eventLog->append(tr("Top: ") + config.choices[j]);
          break;
        case 2:
          m_eventLog->append(tr("Step: ") + config.choices[j]);
          break;
        }
      }
    }
  }
  m_eventLog->append("=================================");
}

//--------------------------------------------------

void GPhotoEventLogPopup::hideEvent(QHideEvent *e) {
  if (m_logging) onPauseButtonPressed();
}

//==================================================

OpenPopupCommandHandler<GPhotoEventLogPopup> openGPhotoEventLogPopup(
    MI_OpenGPhotoEventLog);
