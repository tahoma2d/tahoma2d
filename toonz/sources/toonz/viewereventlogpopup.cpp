#include "viewereventlogpopup.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTabletEvent>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QApplication>
#include <QClipboard>

//=============================================================================
// ViewerEventLog
//-----------------------------------------------------------------------------

ViewerEventLogPopup::ViewerEventLogPopup(QWidget *parent)
    : QSplitter(parent), m_logging(false), m_lastMsgCount(0) {
  setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("Viewer Event Log"));

  // style sheet
  setObjectName("ViewerEventLog");
  setFrameStyle(QFrame::StyledPanel);

  //-------Left side
  m_eventEnter              = new QCheckBox(tr("Enter"), this);
  m_eventLeave              = new QCheckBox(tr("Leave"), this);
  m_eventTabletPress        = new QCheckBox(tr("Stylus Press"), this);
  m_eventTabletMove         = new QCheckBox(tr("Stylus Move"), this);
  m_eventTabletRelease      = new QCheckBox(tr("Stylus Release"), this);
  m_eventTouchBegin         = new QCheckBox(tr("Touch Begin"), this);
  m_eventTouchEnd           = new QCheckBox(tr("Touch End"), this);
  m_eventTouchCancel        = new QCheckBox(tr("Touch Cancel"), this);
  m_eventGesture            = new QCheckBox(tr("Gesture"), this);
  m_eventMouseButtonPress   = new QCheckBox(tr("Mouse Button Press"), this);
  m_eventMouseMove          = new QCheckBox(tr("Mouse Move"), this);
  m_eventMouseButtonRelease = new QCheckBox(tr("Mouse Button Release"), this);
  m_eventMouseButtonDblClick =
      new QCheckBox(tr("Mouse Button Double-Click"), this);
  m_eventKeyPress   = new QCheckBox(tr("Key Press"), this);
  m_eventKeyRelease = new QCheckBox(tr("Key Release"), this);

  m_eventEnter->setChecked(true);
  m_eventLeave->setChecked(true);
  m_eventTabletPress->setChecked(true);
  m_eventTabletMove->setChecked(true);
  m_eventTabletRelease->setChecked(true);
  m_eventTouchBegin->setChecked(true);
  m_eventTouchEnd->setChecked(true);
  m_eventTouchCancel->setChecked(true);
  m_eventGesture->setChecked(true);
  m_eventMouseButtonPress->setChecked(true);
  m_eventMouseMove->setChecked(true);
  m_eventMouseButtonRelease->setChecked(true);
  m_eventMouseButtonDblClick->setChecked(true);
  m_eventKeyPress->setChecked(true);
  m_eventKeyRelease->setChecked(true);

  QFrame *filterBox          = new QFrame(this);
  QVBoxLayout *vFilterLayout = new QVBoxLayout(filterBox);
  vFilterLayout->setMargin(10);
  vFilterLayout->setSpacing(5);

  vFilterLayout->addWidget(new QLabel(tr("Capture events:"), this));
  vFilterLayout->addWidget(m_eventEnter);
  vFilterLayout->addWidget(m_eventLeave);
  vFilterLayout->addWidget(m_eventTabletPress);
  vFilterLayout->addWidget(m_eventTabletMove);
  vFilterLayout->addWidget(m_eventTabletRelease);
  vFilterLayout->addWidget(m_eventTouchBegin);
  vFilterLayout->addWidget(m_eventTouchEnd);
  vFilterLayout->addWidget(m_eventTouchCancel);
  vFilterLayout->addWidget(m_eventGesture);
  vFilterLayout->addWidget(m_eventMouseButtonPress);
  vFilterLayout->addWidget(m_eventMouseMove);
  vFilterLayout->addWidget(m_eventMouseButtonRelease);
  vFilterLayout->addWidget(m_eventMouseButtonDblClick);
  vFilterLayout->addWidget(m_eventKeyPress);
  vFilterLayout->addWidget(m_eventKeyRelease);

  vFilterLayout->addStretch();

  filterBox->setLayout(vFilterLayout);

  addWidget(filterBox);
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
}

//--------------------------------------------------

void ViewerEventLogPopup::addEventMessage(QEvent *e) {
  if (!m_logging) return;

  QString eventMsg = tr("Unknown event");

  switch (e->type()) {
  case QEvent::Enter: {
    if (!m_eventEnter->isChecked()) return;
    eventMsg = tr("Entered viewer");
  } break;

  case QEvent::Leave: {
    if (!m_eventLeave->isChecked()) return;
    eventMsg = tr("Left viewer");
  } break;

  case QEvent::TabletPress: {
    if (!m_eventTabletPress->isChecked()) return;

    QTabletEvent *te = dynamic_cast<QTabletEvent *>(e);
    float pressure   = (int)(te->pressure() * 1000 + 0.5);
    eventMsg         = tr("Stylus pressed at X=%1 Y=%2 Pressure=%3%")
                   .arg(te->pos().x())
                   .arg(te->pos().y())
                   .arg(pressure / 10.0);
  } break;

  case QEvent::TabletMove: {
    if (!m_eventTabletMove->isChecked()) return;

    QTabletEvent *te = dynamic_cast<QTabletEvent *>(e);
    QString operation =
        ((te->buttons() & Qt::LeftButton) ||
         (te->buttons() & Qt::RightButton) || (te->buttons() & Qt::MidButton))
            ? tr("dragged")
            : tr("moved");
    float pressure = (int)(te->pressure() * 1000 + 0.5);
    eventMsg       = tr("Stylus %1 to X=%2 Y=%3 Pressure=%4%")
                   .arg(operation)
                   .arg(te->pos().x())
                   .arg(te->pos().y())
                   .arg(pressure / 10.0);
  } break;

  case QEvent::TabletRelease: {
    if (!m_eventTabletRelease->isChecked()) return;

    eventMsg = tr("Stylus released");
  } break;

  case QEvent::TouchBegin: {
    if (!m_eventTouchBegin->isChecked()) return;

    eventMsg = tr("Touch begins");
  } break;

  case QEvent::TouchEnd: {
    if (!m_eventTouchEnd->isChecked()) return;

    eventMsg = tr("Touch ended");
  } break;

  case QEvent::TouchCancel: {
    if (!m_eventTouchCancel->isChecked()) return;

    eventMsg = tr("Touch cancelled");
  } break;

  case QEvent::Gesture: {
    if (!m_eventGesture->isChecked()) return;

    eventMsg = tr("Gesture encountered");
  } break;

  case QEvent::MouseButtonPress: {
    if (!m_eventMouseButtonPress->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString usedButton =
        (me->buttons() & Qt::LeftButton)
            ? tr("LEFT")
            : (me->buttons() & Qt::RightButton)
                  ? tr("RIGHT")
                  : (me->buttons() & Qt::MidButton) ? tr("MIDDLE") : tr("NO");

    eventMsg = tr("Mouse %1 button pressed at X=%2 Y=%3")
                   .arg(usedButton)
                   .arg(me->pos().x())
                   .arg(me->pos().y());
  } break;

  case QEvent::MouseMove: {
    if (!m_eventMouseMove->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString operation =
        ((me->buttons() & Qt::LeftButton) ||
         (me->buttons() & Qt::RightButton) || (me->buttons() & Qt::MidButton))
            ? tr("dragged")
            : tr("moved");
    eventMsg = tr("Mouse %1 to X=%2 Y=%3")
                   .arg(operation)
                   .arg(me->pos().x())
                   .arg(me->pos().y());
  } break;

  case QEvent::MouseButtonRelease: {
    if (!m_eventMouseButtonRelease->isChecked()) return;

    eventMsg = tr("Mouse button released");
  } break;

  case QEvent::MouseButtonDblClick: {
    if (!m_eventMouseButtonDblClick->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString usedButton =
        (me->buttons() & Qt::LeftButton)
            ? tr("LEFT")
            : (me->buttons() & Qt::RightButton)
                  ? tr("RIGHT")
                  : (me->buttons() & Qt::MidButton) ? tr("MIDDLE") : tr("NO");

    eventMsg = tr("Mouse %1 button double-clicked at X=%2 Y=%3")
                   .arg(usedButton)
                   .arg(me->pos().x())
                   .arg(me->pos().y());
  } break;

  case QEvent::KeyPress: {
    if (!m_eventKeyPress->isChecked()) return;

    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(e);
    QString keyStr =
        QKeySequence(keyEvent->key() + keyEvent->modifiers()).toString();
    eventMsg = tr("Key pressed: %1").arg(keyStr);
  } break;

  case QEvent::KeyRelease: {
    if (!m_eventKeyRelease->isChecked()) return;

    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(e);
    QString keyStr =
        QKeySequence(keyEvent->key() + keyEvent->modifiers()).toString();
    eventMsg = tr("Key released: %1").arg(keyStr);
  } break;

  default:
    return;
  }

  if (m_lastMsg == eventMsg) {
    m_lastMsgCount++;
    return;
  }

  m_lastMsg = eventMsg;

  if (m_lastMsgCount > 1)
    eventMsg += " [x" + QString::number(m_lastMsgCount) + "]";

  m_eventLog->append(eventMsg);
  m_eventLog->moveCursor(QTextCursor::End);

  m_lastMsgCount = 1;
}

//--------------------------------------------------

void ViewerEventLogPopup::onPauseButtonPressed() {
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

void ViewerEventLogPopup::onCopyButtonPressed() {
  QClipboard *clipboard = QApplication::clipboard();
  clipboard->setText(m_eventLog->toPlainText());
}

//--------------------------------------------------

void ViewerEventLogPopup::onClearButtonPressed() { m_eventLog->clear(); }

//--------------------------------------------------

void ViewerEventLogPopup::showEvent(QShowEvent *e) {
  m_logging = true;
  m_pauseBtn->setText("Pause");
  m_eventLog->clear();
}

//--------------------------------------------------

void ViewerEventLogPopup::hideEvent(QHideEvent *e) { m_logging = false; }
