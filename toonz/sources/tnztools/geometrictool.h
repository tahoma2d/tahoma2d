#pragma once

#ifndef GEOMETRICTOOL_H
#define GEOMETRICTOOL_H

#include <QCoreApplication>

class GeometricTool;

class GeometricToolNotifier final : public QObject {
  Q_OBJECT

  GeometricTool *m_tool;

public:
  GeometricToolNotifier(GeometricTool *tool);

protected slots:
  void onColorStyleChanged();
};

#endif  // GEOMETRICTOOL_H
