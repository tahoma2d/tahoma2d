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

  void refreshStatusBar() { updateInfoText(); }

protected:
  StatusLabel *m_currentFrameLabel, *m_infoLabel;
  std::unordered_map<std::string, QString> m_infoMap;
  std::unordered_map<std::string, QString> m_hintMap;
  void showEvent(QShowEvent*) override;
  std::unordered_map<std::string, QString> makeMap(QString spacer,
                                                   QString cmdTextSeparator,
                                                   QString cmd2TextSeparator);

protected slots:
  void updateInfoText();

private:
  friend class MainWindow;
};

#endif  // STATUSBAR_H
