#pragma once

#ifndef VIEWER_EVENT_LOG_H
#define VIEWER_EVENT_LOG_H

#include <QSplitter>
#include <QEvent>

class QTextEdit;
class QCheckBox;
class QPushButton;

//**************************************************************
//    Viewer Event Log Popup
//**************************************************************

class ViewerEventLogPopup final : public QSplitter {
  Q_OBJECT

  QTextEdit *m_eventLog;
  QCheckBox *m_eventEnter, *m_eventLeave, *m_eventTabletPress,
      *m_eventTabletMove, *m_eventTabletRelease, *m_eventTouchBegin,
      *m_eventTouchEnd, *m_eventTouchCancel, *m_eventGesture,
      *m_eventMouseButtonPress, *m_eventMouseMove, *m_eventMouseButtonRelease,
      *m_eventMouseButtonDblClick, *m_eventKeyPress, *m_eventKeyRelease;

  QPushButton *m_pauseBtn;

  QString m_lastMsg;
  int m_lastMsgCount;

  bool m_logging;

public:
  ViewerEventLogPopup(QWidget *parent = 0);

  void addEventMessage(QEvent *e);

  void showEvent(QShowEvent *e) override;
  void hideEvent(QHideEvent *e) override;

public slots:
  void onPauseButtonPressed();
  void onCopyButtonPressed();
  void onClearButtonPressed();
};

//**************************************************************
//    Viewer Event Log Manager
//**************************************************************

class ViewerEventLogManager {  // singleton

  ViewerEventLogPopup *m_viewerEventLogPopup;

  ViewerEventLogManager() {}

public:
  static ViewerEventLogManager *instance() {
    static ViewerEventLogManager _instance;
    return &_instance;
  }

  void setViewerEventLogPopup(ViewerEventLogPopup *popup) {
    m_viewerEventLogPopup = popup;
  }

  void addEventMessage(QEvent *e) {
    if (!m_viewerEventLogPopup) return;
    m_viewerEventLogPopup->addEventMessage(e);
  }
};

#endif  // VIEWER_EVENT_LOG_H
