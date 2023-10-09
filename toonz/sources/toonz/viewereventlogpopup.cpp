#include "viewereventlogpopup.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QTabletEvent>
#include <QTextEdit>
#include <QVBoxLayout>

//=============================================================================
// ViewerEventLog
//-----------------------------------------------------------------------------

ViewerEventLogPopup::ViewerEventLogPopup(QWidget *parent)
    : QSplitter(parent), m_logging(false), m_lastMsgCount(0) {
  setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
  setWindowTitle("Viewer Event Log");

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

  QPushButton *clearBtn = new QPushButton(tr("Clear Log"), this);
  connect(clearBtn, SIGNAL(pressed()), this, SLOT(onClearButtonPressed()));

  m_eventLog = new QTextEdit(this);
  m_eventLog->setReadOnly(true);

  QFrame *logBox          = new QFrame(this);
  QVBoxLayout *vLogLayout = new QVBoxLayout(logBox);

  vLogLayout->addWidget(m_eventLog);
  vLogLayout->addWidget(clearBtn);

  logBox->setLayout(vFilterLayout);

  addWidget(logBox);

  //------end right side (the task sheet)

  setStretchFactor(1, 2);
}

//--------------------------------------------------

void ViewerEventLogPopup::addEventMessage(QEvent *e) {
  if (!m_logging) return;

  QString eventMsg = "Unknown event";

  switch (e->type()) {
  case QEvent::Enter: {
    if (!m_eventEnter->isChecked()) return;
    eventMsg = "Entered viewer";
  } break;

  case QEvent::Leave: {
    if (!m_eventLeave->isChecked()) return;
    eventMsg = "Left viewer";
  } break;

  case QEvent::TabletPress: {
    if (!m_eventTabletPress->isChecked()) return;

    QTabletEvent *te = dynamic_cast<QTabletEvent *>(e);
    eventMsg = "Stylus pressed at X=" + QString::number(te->pos().x()) + " Y=" +
               QString::number(te->pos().y()) + " Pressure=" +
               QString::number(te->pressure());
  } break;

  case QEvent::TabletMove: {
    if (!m_eventTabletMove->isChecked()) return;

    QTabletEvent *te = dynamic_cast<QTabletEvent *>(e);
    QString operation =
        ((te->buttons() & Qt::LeftButton) ||
         (te->buttons() & Qt::RightButton) || (te->buttons() & Qt::MidButton))
            ? "dragged"
            : "moved";
    eventMsg = "Stylus " + operation + " to X=" +
               QString::number(te->pos().x()) + " Y=" +
               QString::number(te->pos().y()) + " Pressure=" +
               QString::number(te->pressure());
  } break;

  case QEvent::TabletRelease: {
    if (!m_eventTabletRelease->isChecked()) return;

    eventMsg = "Stylus released";
  } break;

  case QEvent::TouchBegin: {
    if (!m_eventTouchBegin->isChecked()) return;

    eventMsg = "Touch begins";
  } break;

  case QEvent::TouchEnd: {
    if (!m_eventTouchEnd->isChecked()) return;

    eventMsg = "Touch ended";
  } break;

  case QEvent::TouchCancel: {
    if (!m_eventTouchCancel->isChecked()) return;

    eventMsg = "Touch cancelled";
  } break;

  case QEvent::Gesture: {
    if (!m_eventGesture->isChecked()) return;

    eventMsg = "Gesture encountered";
  } break;

  case QEvent::MouseButtonPress: {
    if (!m_eventMouseButtonPress->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString usedButton =
        (me->buttons() & Qt::LeftButton)
            ? "LEFT"
            : (me->buttons() & Qt::RightButton)
                  ? "RIGHT"
                  : (me->buttons() & Qt::MidButton) ? "MIDDLE" : "NO";

    eventMsg = "Mouse " + usedButton + " button pressed at X=" +
               QString::number(me->pos().x()) + " Y=" +
               QString::number(me->pos().y());
  } break;

  case QEvent::MouseMove: {
    if (!m_eventMouseMove->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString operation =
        ((me->buttons() & Qt::LeftButton) ||
         (me->buttons() & Qt::RightButton) || (me->buttons() & Qt::MidButton))
            ? "dragged"
            : "moved";
    eventMsg = "Mouse " + operation + " to X=" +
               QString::number(me->pos().x()) + " Y=" +
               QString::number(me->pos().y());
  } break;

  case QEvent::MouseButtonRelease: {
    if (!m_eventMouseButtonRelease->isChecked()) return;

    eventMsg = "Mouse button released";
  } break;

  case QEvent::MouseButtonDblClick: {
    if (!m_eventMouseButtonDblClick->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString usedButton =
        (me->buttons() & Qt::LeftButton)
            ? "LEFT"
            : (me->buttons() & Qt::RightButton)
                  ? "RIGHT"
                  : (me->buttons() & Qt::MidButton) ? "MIDDLE" : "NO";

    eventMsg = "Mouse " + usedButton + " button double-clicked at X=" +
               QString::number(me->pos().x()) + " Y=" +
               QString::number(me->pos().y());
  } break;

  case QEvent::KeyPress: {
    if (!m_eventKeyPress->isChecked()) return;

    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(e);
    QString keyStr =
        QKeySequence(keyEvent->key() + keyEvent->modifiers()).toString();
    eventMsg = "Key pressed: " + keyStr;
  } break;

  case QEvent::KeyRelease: {
    if (!m_eventKeyRelease->isChecked()) return;

    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(e);
    QString keyStr =
        QKeySequence(keyEvent->key() + keyEvent->modifiers()).toString();
    eventMsg = "Key released: " + keyStr;
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

void ViewerEventLogPopup::onClearButtonPressed() { m_eventLog->clear(); }

//--------------------------------------------------

void ViewerEventLogPopup::showEvent(QShowEvent *e) {
  m_logging = true;
  m_eventLog->clear();
}

//--------------------------------------------------

void ViewerEventLogPopup::hideEvent(QHideEvent *e) { m_logging = false; }
