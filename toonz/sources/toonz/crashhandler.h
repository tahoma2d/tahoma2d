#pragma once

#ifndef CRASHHANDLER_INCLUDED
#define CRASHHANDLER_INCLUDED

#include "tcommon.h"
#include "tfilepath.h"

#include <QDialog>

class CrashHandler : public QDialog {
  Q_OBJECT;

  TFilePath m_crashFile;
  QString m_crashReport;

public:
  CrashHandler(QWidget *parent, TFilePath crashFile, QString crashTxt);

  void reject();

  static void install();
  static bool trigger(const QString reason, bool showDialog);

public slots:
  void copyClipboard();
  void openWebpage();
};

#endif  // CRASHHANDLER_INCLUDED
