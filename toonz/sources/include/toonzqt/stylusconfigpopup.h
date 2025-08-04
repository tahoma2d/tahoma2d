#pragma once

#ifndef STYLUSCONFIGPOPUP_H
#define STYLUSCONFIGPOPUP_H

#include "graphwidget.h"

#include <QWidget>
#include <QGroupBox>

#include <QListWidget>

class GraphGUIWidget;

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
//
// StylusConfigTarget
//
//=============================================================================

class DVAPI StylusConfigPopup final : public QWidget {
  Q_OBJECT

  struct GraphProperties {
    bool enabled;
    double minX, maxX, minY, maxY;
    QList<TPointD> defaultCurve, curve;
    QString minXLabel, midXLabel, maxXLabel, minYLabel, midYLabel, maxYLabel;
  };

  QLabel *m_graphTitleLabel;
  GraphGUIWidget *m_graph;
  QListWidget *m_configList;
  std::vector<GraphProperties> m_configProperties;
  int m_currentIndex;

  QTimer *m_keepClosedTimer;
  bool m_keepClosed;

public:
  StylusConfigPopup(QString title, QWidget *parent);

  GraphGUIWidget *getGraphGUI() { return m_graph; }

  void setGraphTitle(QString title) { m_graphTitleLabel->setText(title); }

  int addConfiguration(QString name);

  void setConfiguration(int configId, double minX, double maxX, double minY,
                        double maxY, QList<TPointD> defaultCurve,
                        QString minXLabel = "", QString midXLabel = "",
                        QString maxXLabel = "", QString minYLabel = "",
                        QString midYLabel = "", QString maxYLabel = "");

  void setConfigEnabled(int configId, bool enabled) {
    m_configProperties[configId].enabled = enabled;
    m_configList->item(configId)->setCheckState(enabled ? Qt::Checked
                                                        : Qt::Unchecked);
  }
  bool isConfigEnabled(int configId) {
    return m_configProperties[configId].enabled;
  };

  void setConfigCurve(int configId, QList<TPointD> curve) {
    m_configProperties[configId].curve = curve;
    m_graph->setCurve(curve);
  }
  QList<TPointD> getConfigCurve(int configId) {
    return m_configProperties[configId].curve;
  };

  void setConfigDefaultCurve(int configId, QList<TPointD> curve) {
    m_configProperties[configId].defaultCurve = curve;
  }
  QList<TPointD> getConfigDefaultCurve(int configId) {
    return m_configProperties[configId].defaultCurve;
  };

  void updateGraph();

  bool isKeepClosed() { return m_keepClosed; }
  void setKeepClosed(bool keepClosed) { m_keepClosed = keepClosed; }

protected:
  void mouseReleaseEvent(QMouseEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void showEvent(QShowEvent *) override;

protected slots:
  void onCurrentRowChanged(int);
  void onItemChanged(QListWidgetItem *);
  void onCurveChanged(bool);
  void resetKeepClosed();

signals:
  void configStateChanged(int);
  void configCurveChanged(int, bool);
};

#endif  // STYLUSCONFIGPOPUP