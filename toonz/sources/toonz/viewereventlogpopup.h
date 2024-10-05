#pragma once

#ifndef VIEWER_EVENT_LOG_H
#define VIEWER_EVENT_LOG_H

#include "saveloadqsettings.h"

#include <QSplitter>
#include <QEvent>

class QTextEdit;
class QCheckBox;
class QPushButton;

//**************************************************************
//    Viewer Event Log Popup
//**************************************************************
enum VIEWEREVENT {
  Enter = 0,
  Leave,
  TabletPress,
  TabletMove,
  TabletRelease,
  TouchBegin,
  TouchUpdate,
  TouchEnd,
  TouchCancel,
  Gesture,
  MouseButtonPress,
  MouseMove,
  MouseButtonRelease,
  MouseButtonDblClick,
  KeyPress,
  KeyRelease,
  Count
};

class ViewerEventLogPopup final : public QSplitter {
  Q_OBJECT

  QTextEdit *m_eventLog;
  QCheckBox *m_toggleAllOnOff, *m_eventCheckBox[VIEWEREVENT::Count];

  QPushButton *m_pauseBtn;

  QString m_lastMsg;
  int m_lastMsgCount;

  bool m_logging;

  int m_eventCount;

public:
  ViewerEventLogPopup(QWidget *parent = 0);

  void addEventMessage(QEvent *e);

  void hideEvent(QHideEvent *e) override;

public slots:
  void onPauseButtonPressed();
  void onCopyButtonPressed();
  void onClearButtonPressed();
  void onToggleAllOnOff();
  void onEventFilterUpdated();
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
