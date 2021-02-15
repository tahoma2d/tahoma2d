#pragma once

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <QStatusBar>
#include <unordered_map>
#include <QLabel>

// class StatusLabel;
class MainWindow;
class TXshLevel;

class StatusLabel : public QLabel {
  Q_OBJECT

public:
  StatusLabel(const QString& text, QWidget* parent = nullptr)
      : QLabel(text, parent) {}
  ~StatusLabel(){};
};

//-----------------------------------------------------------------------------

class StatusBar final : public QStatusBar {
  Q_OBJECT

public:
  StatusBar(QWidget* parent = nullptr);

  ~StatusBar();

  void updateFrameText(QString text);

protected:
  StatusLabel *m_currentFrameLabel, *m_infoLabel;
  std::unordered_map<std::string, QString> m_infoMap;
  void showEvent(QShowEvent*) override;
  void makeMap();

protected slots:
  void updateInfoText();

private:
  friend class MainWindow;
};

#endif  // STATUSBAR_H
