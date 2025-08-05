#pragma once

#ifndef GPHOTO_EVENT_LOG_H
#define GPHOTO_EVENT_LOG_H

#include "saveloadqsettings.h"
#include "gphotocam.h"

#include <QSplitter>
#include <QEvent>

class QTextEdit;
class QCheckBox;
class QPushButton;

//**************************************************************
//    GPhoto Event Log Popup
//**************************************************************
enum GPHOTOMSG {
  GPEvent = 0,
  GPError,
  GPDebug,
  Count
};

class GPhotoEventLogPopup final : public QSplitter {
  Q_OBJECT

  QTextEdit *m_eventLog;
  QCheckBox *m_toggleAllOnOff, *m_eventCheckBox[GPHOTOMSG::Count];

  QPushButton *m_pauseBtn;

  QString m_lastMsg;
  int m_lastMsgCount;

  bool m_logging;

  int m_msgTypeCount;

public:
  GPhotoEventLogPopup(QWidget *parent = 0);

  void addEventMessage(GPHOTOMSG msgType, QString msg);

  void hideEvent(QHideEvent *e) override;

public slots:
  void onPauseButtonPressed();
  void onCopyButtonPressed();
  void onClearButtonPressed();
  void onListAllConfigButtonPressed();
  void onToggleAllOnOff();
  void onEventFilterUpdated();
};

//**************************************************************
//    GPhoto Event Log Manager
//**************************************************************

class GPhotoEventLogManager {  // singleton

  GPhotoEventLogPopup *m_gphotoEventLogPopup;

  GPhotoEventLogManager() {}

public:
  static GPhotoEventLogManager *instance() {
    static GPhotoEventLogManager _instance;
    return &_instance;
  }

  void setGPhotoEventLogPopup(GPhotoEventLogPopup *popup) {
    m_gphotoEventLogPopup = popup;
  }

  void addEventMessage(GPHOTOMSG msgType, QString msg) {
    if (!m_gphotoEventLogPopup) return;
    m_gphotoEventLogPopup->addEventMessage(msgType, msg);
  }
};

#endif  // GPHOTO_EVENT_LOG_H
