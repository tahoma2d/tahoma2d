#pragma once

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QStatusBar>;
#include <unordered_map>

class QLabel;
class MainWindow;
class TXshLevel;

//-----------------------------------------------------------------------------

class StatusBar final : public QStatusBar {
  Q_OBJECT

public:
  StatusBar(QWidget* parent = nullptr);

  ~StatusBar();

  void setMessageText(QString text);
  void updateFrameText(QString text);

protected:
  QLabel *m_currentFrameLabel, *m_infoLabel;  // , * m_messageLabel;
  std::unordered_map<std::string, QString> m_infoMap;
  void makeMap();

protected slots:
  void updateInfoText();

private:
  friend class MainWindow;
};

#endif  // STATUSBAR_H
