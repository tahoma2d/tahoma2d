#include "viewereventlogpopup.h"

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
// ViewerEventLog
//-----------------------------------------------------------------------------

ViewerEventLogPopup::ViewerEventLogPopup(QWidget *parent)
    : QSplitter(parent), m_logging(true), m_lastMsgCount(0) {
  setWindowFlags(Qt::WindowStaysOnTopHint);
  setWindowTitle(tr("Viewer Event Log"));

  // style sheet
  setObjectName("ViewerEventLog");
  setFrameStyle(QFrame::StyledPanel);

  //-------Left side
  m_toggleAllOnOff = new QCheckBox(tr("Capture events:"), this);
  m_toggleAllOnOff->setChecked(true);

  for (int i = 0; i < VIEWEREVENT::Count; i++) {
    QString label;
    switch (i) {
    case VIEWEREVENT::Enter:
      label = tr("Enter");
      break;
    case VIEWEREVENT::Leave:
      label = tr("Leave");
      break;
    case VIEWEREVENT::TabletPress:
      label = tr("Stylus Press");
      break;
    case VIEWEREVENT::TabletMove:
      label = tr("Stylus Move");
      break;
    case VIEWEREVENT::TabletRelease:
      label = tr("Stylus Release");
      break;
    case VIEWEREVENT::TouchBegin:
      label = tr("Touch Begin");
      break;
    case VIEWEREVENT::TouchUpdate:
      label = tr("Touch Update");
      break;
    case VIEWEREVENT::TouchEnd:
      label = tr("Touch End");
      break;
    case VIEWEREVENT::TouchCancel:
      label = tr("Touch Cancel");
      break;
    case VIEWEREVENT::Gesture:
      label = tr("Gesture");
      break;
    case VIEWEREVENT::MouseButtonPress:
      label = tr("Mouse Button Press");
      break;
    case VIEWEREVENT::MouseMove:
      label = tr("Mouse Move");
      break;
    case VIEWEREVENT::MouseButtonRelease:
      label = tr("Mouse Button Release");
      break;
    case VIEWEREVENT::MouseButtonDblClick:
      label = tr("Mouse Button Double-Click");
      break;
    case VIEWEREVENT::KeyPress:
      label = tr("Key Press");
      break;
    case VIEWEREVENT::KeyRelease:
      label = tr("Key Release");
      break;
    }
    m_eventCheckBox[i] = new QCheckBox(label, this);
    m_eventCheckBox[i]->setChecked(true);
  }

  m_eventCount = VIEWEREVENT::Count;

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
      for (int i = 0; i < VIEWEREVENT::Count; i++) {
        vEventListLayout->addWidget(m_eventCheckBox[i]);
      }
    }
    vFilterLayout->addLayout(vEventListLayout);

  }
  vFilterLayout->addStretch();

  filterBox->setLayout(vFilterLayout);

  addWidget(filterBox);

  connect(m_toggleAllOnOff, SIGNAL(stateChanged(int)), this,
          SLOT(onToggleAllOnOff()));

  for (int i = 0; i < VIEWEREVENT::Count; i++) {
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
  m_eventLog->setLineWrapMode(QTextEdit::LineWrapMode::NoWrap);
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
}

//--------------------------------------------------

void ViewerEventLogPopup::onToggleAllOnOff() {
  bool enable = m_eventCount != VIEWEREVENT::Count ? true : false;

  m_eventCount = enable ? VIEWEREVENT::Count : 0;

  for (int i = 0; i < VIEWEREVENT::Count; i++) {
    m_eventCheckBox[i]->blockSignals(true);
    m_eventCheckBox[i]->setChecked(enable);
    m_eventCheckBox[i]->blockSignals(false);
  }
}

//--------------------------------------------------

void ViewerEventLogPopup::onEventFilterUpdated() {
  QWidget *senderWidget = static_cast<QWidget *>(sender());
  if (static_cast<QCheckBox *>(senderWidget)->isChecked())
    m_eventCount++;
  else
    m_eventCount--;

  m_toggleAllOnOff->blockSignals(true);
  m_toggleAllOnOff->setChecked(false);
  m_toggleAllOnOff->blockSignals(false);
}

//--------------------------------------------------

void ViewerEventLogPopup::addEventMessage(QEvent *e) {
  if (!m_logging) return;

  QString eventMsg = tr("Unknown event");

  switch (e->type()) {
  case QEvent::Enter: {
    if (!m_eventCheckBox[VIEWEREVENT::Enter]->isChecked()) return;
    eventMsg = tr("Entered viewer");
  } break;

  case QEvent::Leave: {
    if (!m_eventCheckBox[VIEWEREVENT::Leave]->isChecked()) return;
    eventMsg = tr("Left viewer");
  } break;

  case QEvent::TabletPress: {
    if (!m_eventCheckBox[VIEWEREVENT::TabletPress]->isChecked()) return;

    QTabletEvent *te = dynamic_cast<QTabletEvent *>(e);
    float pressure   = (int)(te->pressure() * 1000 + 0.5);
    eventMsg = tr("Stylus pressed at X=%1 Y=%2 Pressure=%3% TiltX=%4 TiltY=%5 "
                  "Rotation=%6")
                   .arg(te->pos().x())
                   .arg(te->pos().y())
                   .arg(pressure / 10.0)
                   .arg(te->xTilt())
                   .arg(te->yTilt())
                   .arg(te->rotation());
  } break;

  case QEvent::TabletMove: {
    if (!m_eventCheckBox[VIEWEREVENT::TabletMove]->isChecked()) return;

    QTabletEvent *te = dynamic_cast<QTabletEvent *>(e);
    QString operation =
        ((te->buttons() & Qt::LeftButton) ||
         (te->buttons() & Qt::RightButton) || (te->buttons() & Qt::MiddleButton))
            ? tr("dragged")
            : tr("moved");
    float pressure = (int)(te->pressure() * 1000 + 0.5);
    eventMsg =
        tr("Stylus %1 to X=%2 Y=%3 Pressure=%4% TiltX=%5 TiltY=%6 Rotation=%7")
            .arg(operation)
            .arg(te->pos().x())
            .arg(te->pos().y())
            .arg(pressure / 10.0)
            .arg(te->xTilt())
            .arg(te->yTilt())
            .arg(te->rotation());
  } break;

  case QEvent::TabletRelease: {
    if (!m_eventCheckBox[VIEWEREVENT::TabletRelease]->isChecked()) return;

    eventMsg = tr("Stylus released");
  } break;

  case QEvent::TouchBegin: {
    if (!m_eventCheckBox[VIEWEREVENT::TouchBegin]->isChecked()) return;

    QTouchEvent *te = dynamic_cast<QTouchEvent *>(e);
    QString device  = ((te->device()->type() == QTouchDevice::TouchPad)
                          ? "touchpad"
                          : ((te->device()->type() == QTouchDevice::TouchScreen)
                                 ? "touchscreen"
                                 : "unknown"));
    eventMsg = tr("Touch begins (%1; %2pts)")
                   .arg(device)
                   .arg(te->touchPoints().count());
  } break;

  case QEvent::TouchUpdate: {
    if (!m_eventCheckBox[VIEWEREVENT::TouchUpdate]->isChecked()) return;

    QTouchEvent *te = dynamic_cast<QTouchEvent *>(e);
    QString device  = ((te->device()->type() == QTouchDevice::TouchPad)
                          ? "touchpad"
                          : ((te->device()->type() == QTouchDevice::TouchScreen)
                                 ? "touchscreen"
                                 : "unknown"));
    eventMsg = tr("Touch updated (%1; %2pts)")
                   .arg(device)
                   .arg(te->touchPoints().count());
  } break;

  case QEvent::TouchEnd: {
    if (!m_eventCheckBox[VIEWEREVENT::TouchEnd]->isChecked()) return;

    eventMsg = tr("Touch ended");
  } break;

  case QEvent::TouchCancel: {
    if (!m_eventCheckBox[VIEWEREVENT::TouchCancel]->isChecked()) return;

    eventMsg = tr("Touch cancelled");
  } break;

  case QEvent::Gesture: {
    if (!m_eventCheckBox[VIEWEREVENT::Gesture]->isChecked()) return;
    QGestureEvent *ge = dynamic_cast<QGestureEvent *>(e);
    QString action;
    for (int i = 0; i < ge->gestures().count(); i++) {
      if (i > 0) action += ",";
      QGesture *g = ge->gestures()[i];
      switch (g->gestureType()) {
      case Qt::GestureType::PanGesture:
        action += "pan";
        break;
      case Qt::GestureType::SwipeGesture:
        action += "swipe";
        break;
      case Qt::GestureType::PinchGesture:
        action += "pinch";
        break;
      case Qt::GestureType::TapGesture:
        action += "tap";
        break;
      default:
        action += "unknown";
        break;
      }
      switch (g->state()) {
      case Qt::GestureState::GestureStarted:
        action += "-started";
        break;
      case Qt::GestureState::GestureUpdated:
        action += "-updated";
        break;
      case Qt::GestureState::GestureFinished:
        action += "-finished";
        break;
      case Qt::GestureState::GestureCanceled:
        action += "-cancelled";
        break;
      }
    }

    eventMsg = tr("Gesture encountered (%1)").arg(action);
  } break;

  case QEvent::MouseButtonPress: {
    if (!m_eventCheckBox[VIEWEREVENT::MouseButtonPress]->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString usedButton =
        (me->buttons() & Qt::LeftButton)
            ? tr("LEFT")
            : (me->buttons() & Qt::RightButton)
                  ? tr("RIGHT")
                  : (me->buttons() & Qt::MiddleButton) ? tr("MIDDLE") : tr("NO");

    eventMsg = tr("Mouse %1 button pressed at X=%2 Y=%3")
                   .arg(usedButton)
                   .arg(me->pos().x())
                   .arg(me->pos().y());
  } break;

  case QEvent::MouseMove: {
    if (!m_eventCheckBox[VIEWEREVENT::MouseMove]->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString operation =
        ((me->buttons() & Qt::LeftButton) ||
         (me->buttons() & Qt::RightButton) || (me->buttons() & Qt::MiddleButton))
            ? tr("dragged")
            : tr("moved");
    eventMsg = tr("Mouse %1 to X=%2 Y=%3")
                   .arg(operation)
                   .arg(me->pos().x())
                   .arg(me->pos().y());
  } break;

  case QEvent::MouseButtonRelease: {
    if (!m_eventCheckBox[VIEWEREVENT::MouseButtonRelease]->isChecked()) return;

    eventMsg = tr("Mouse button released");
  } break;

  case QEvent::MouseButtonDblClick: {
    if (!m_eventCheckBox[VIEWEREVENT::MouseButtonDblClick]->isChecked()) return;

    QMouseEvent *me = dynamic_cast<QMouseEvent *>(e);
    QString usedButton =
        (me->buttons() & Qt::LeftButton)
            ? tr("LEFT")
            : (me->buttons() & Qt::RightButton)
                  ? tr("RIGHT")
                  : (me->buttons() & Qt::MiddleButton) ? tr("MIDDLE") : tr("NO");

    eventMsg = tr("Mouse %1 button double-clicked at X=%2 Y=%3")
                   .arg(usedButton)
                   .arg(me->pos().x())
                   .arg(me->pos().y());
  } break;

  case QEvent::KeyPress: {
    if (!m_eventCheckBox[VIEWEREVENT::KeyPress]->isChecked()) return;

    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(e);
    QString keyStr =
        QKeySequence(keyEvent->key() + keyEvent->modifiers()).toString();
    eventMsg = tr("Key pressed: %1").arg(keyStr);
  } break;

  case QEvent::KeyRelease: {
    if (!m_eventCheckBox[VIEWEREVENT::KeyRelease]->isChecked()) return;

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
    m_eventLog->undo();
    m_eventLog->append(m_lastMsg + " [x" + QString::number(m_lastMsgCount) +
                       "]");
    m_eventLog->moveCursor(QTextCursor::End);
    return;
  }

  m_eventLog->append(eventMsg);
  m_eventLog->moveCursor(QTextCursor::End);

  m_lastMsg = eventMsg;
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

void ViewerEventLogPopup::hideEvent(QHideEvent *e) {
  if (m_logging) onPauseButtonPressed();
}
