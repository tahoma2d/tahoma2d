#pragma once

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QStatusBar>;

class QLabel;
class MainWindow;
class TXshLevel;
//-----------------------------------------------------------------------------

class StatusBar final : public QStatusBar {
  Q_OBJECT


public:

  StatusBar(QWidget* parent = nullptr);

  ~StatusBar();

protected:
    QLabel* m_currentFrameLabel, * m_infoLabel, * m_messageLabel;

protected slots:
    void updateFrameText();
    void updateInfoText();
    void updateMessageText();
    void onXshLevelSwitched(TXshLevel*);

private:
    friend class MainWindow;
};

#endif  // STATUSBAR_H
