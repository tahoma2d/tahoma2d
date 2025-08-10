#pragma once

#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include "toonz/tstageobjectspline.h"
#include "toonzqt/doublefield.h"

#include <QObject>
#include <QWidget>
#include <QMouseEvent>
#include <QImage>
#include <QLabel>
#include <QPushButton>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

class QFrame;
class TThickPoint;

//=============================================================================
// GraphArea
//-----------------------------------------------------------------------------

class DVAPI GraphWidget : public QWidget {
  Q_OBJECT

  Q_PROPERTY(QColor SplineColor READ getSplineColor WRITE setSplineColor
                 DESIGNABLE true)
  QColor m_splineColor;
  QColor getSplineColor() const { return m_splineColor; }
  void setSplineColor(const QColor& color) { m_splineColor = color; }

  Q_PROPERTY(
      QColor GraphColor READ getGraphColor WRITE setGraphColor DESIGNABLE true)
  QColor m_graphColor;
  QColor getGraphColor() const { return m_graphColor; }
  void setGraphColor(const QColor& color) { m_graphColor = color; }

  Q_PROPERTY(QColor NonSelectedPointColor READ getNonSelectedPointColor WRITE
                 setNonSelectedPointColor DESIGNABLE true)
  QColor m_nonSelectedPointColor;
  QColor getNonSelectedPointColor() const { return m_nonSelectedPointColor; }
  void setNonSelectedPointColor(const QColor& color) {
    m_nonSelectedPointColor = color;
  }

  Q_PROPERTY(QColor SelectedPointColor READ getSelectedPointColor WRITE
                 setSelectedPointColor DESIGNABLE true)
  QColor m_selectedPointColor;
  QColor getSelectedPointColor() const { return m_selectedPointColor; }
  void setSelectedPointColor(const QColor& color) {
    m_selectedPointColor = color;
  }

  QList<QPointF> m_points;
  int m_currentControlPointIndex;

  Qt::MouseButton m_mouseButton;

  // int m_maxHeight;
  // int m_maxWidth;

  int m_LeftRightMargin;
  int m_TopMargin;
  int m_BottomMargin;

  bool m_isLinear;
  bool m_lockExtremePoints;
  bool m_lockExtremeXPoints;
  bool m_constrainToBounds;

  QPointF m_preMousePos;

  bool m_isEnlarged;

  double m_minXValue;
  double m_maxXValue;
  double m_minYValue;
  double m_maxYValue;
  double m_cpMargin;
  int m_gridSpacing;
  QPointF m_xyOffset;
  QPointF m_cursorPos;

  bool m_isDragging;

public:
  explicit GraphWidget(QWidget* parent = nullptr);
  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;
  void setMinXValue(double x) {
    m_minXValue = x;
    m_xyOffset.setX(-x);
  }
  void setMaxXValue(double x) { m_maxXValue = x; }
  void setMinYValue(double y) {
    m_minYValue = y;
    m_xyOffset.setY(-y);
  }
  void setMaxYValue(double y) { m_maxYValue = y; }
  void setCpMargin(double m) { m_cpMargin = m; }
  void setGridSpacing(int spacing) { m_gridSpacing = spacing; }
  void setLockExtremePoints(bool locked) { m_lockExtremePoints = locked; }
  void setLockExtremeXPoints(bool xlocked) { m_lockExtremeXPoints = xlocked; }

  void initializeSpline();

  void setPoints(QList<TPointD> points);
  void clearPoints() { m_points.clear(); }
  QList<TPointD> getPoints();

  int getCurrentControlPointIndex() { return m_currentControlPointIndex; };
  void setCurrentControlPointIndex(int index) {
    m_currentControlPointIndex = index;
  };

  TPointD getControlPoint(int index) {
    if (m_points.isEmpty() || index >= m_points.count()) return TPointD(0, 0);
    return TPointD(m_points.at(index).x() - m_xyOffset.x(),
                   m_points.at(index).y() - m_xyOffset.y());
  }

  bool eventFilter(QObject* object, QEvent* event) override;

  void setFirstLastXPosition(std::pair<double, double> values, bool isDragging);

  void setLinear(bool isLinear);
  void moveCurrentControlPoint(QPointF delta);

  // void setEnlarged(bool isEnlarged);

  QPointF convertPointToLocal(QPointF point);
  QPointF convertPointFromLocal(QPointF point);

protected:
  QPointF viewToStrokePoint(const QPointF& p);
  QPointF getInvertedPoint(QPointF p);
  int getClosestPointIndex(QPointF& pos, double& minDistance2);

  bool isCentralControlPoint(const int index) const { return index % 3 == 0; }
  bool isLeftControlPoint(const int index) const { return index % 3 == 2; }
  bool isRightControlPoint(const int index) const { return index % 3 == 1; }

  void setPoint(int index, const QPointF point);
  void movePoint(int index, const QPointF delta);
  QPointF checkPoint(const QPointF p);

  QPointF getVisibleHandlePos(int index);

  void moveCentralControlPoint(int index, QPointF delta);

  void addControlPoint(double percent);

  void removeControlPoint(int index);
  void removeCurrentControlPoint();

  void selectNextControlPoint();
  void selectPreviousControlPoint();

  QPainterPath getPainterPath();
  QPoint clipToBorder(const QPoint p);

  void paintEvent(QPaintEvent*) override;
  void mouseMoveEvent(QMouseEvent*) override;
  void mousePressEvent(QMouseEvent*) override;
  void mouseReleaseEvent(QMouseEvent*) override;
  void keyPressEvent(QKeyEvent* e) override;

  void focusInEvent(QFocusEvent* fe) override;
  void focusOutEvent(QFocusEvent* fe) override;

signals:
  void focusOut();
  void controlPointChanged(bool isDragging);
  void controlPointAdded(int index);
  void controlPointRemoved(int index);

  void firstLastXPostionChanged(double, double);
  void updateCurrentPosition(int, QPointF);
};

//=============================================================================
// Graph GUI
//-----------------------------------------------------------------------------

class DVAPI GraphGUIWidget : public QWidget {
  Q_OBJECT

  GraphWidget *m_graph;
  QLabel *m_minXLabel, *m_midXLabel, *m_maxXLabel, *m_minYLabel, *m_midYLabel,
      *m_maxYLabel;
  QPushButton* m_resetBtn;

  DVGui::DoubleLineEdit *m_outValue, *m_inValue;
  QList<TPointD> m_defaultCurve;

  bool m_updating;

public:
  GraphGUIWidget(QWidget* parent = nullptr);

  void setMinXLabel(QString label) { m_minXLabel->setText(label); }
  void setMidXLabel(QString label) { m_midXLabel->setText(label); }
  void setMaxXLabel(QString label) { m_maxXLabel->setText(label); }
  void setMinYLabel(QString label) { m_minYLabel->setText(label); }
  void setMidYLabel(QString label) { m_midYLabel->setText(label); }
  void setMaxYLabel(QString label) { m_maxYLabel->setText(label); }

  // For passing onto graph
  void setMaximumGraphSize(int w, int h) { m_graph->setMaximumSize(w, h); }
  void setMinXValue(double value) { m_graph->setMinXValue(value); }
  void setMaxXValue(double value) { m_graph->setMaxXValue(value); }
  void setMinYValue(double value) { m_graph->setMinYValue(value); }
  void setMaxYValue(double value) { m_graph->setMaxYValue(value); }
  void setCpMargin(double margin) { m_graph->setCpMargin(margin); }
  void setGridSpacing(int spacing) { m_graph->setGridSpacing(spacing); }
  void setLockExtremePoints(bool isLocked) {
    m_graph->setLockExtremePoints(isLocked);
  }
  void setLockExtremeXPoints(bool xlocked) {
    m_graph->setLockExtremeXPoints(xlocked);
  }
  void setLinear(bool isLinear) { m_graph->setLinear(isLinear); }
  void setCurve(QList<TPointD> points) { m_graph->setPoints(points); }
  void clearPoints() { m_graph->clearPoints(); }
  QList<TPointD> getCurve() { return m_graph->getPoints(); }
  void initializeSpline() { m_graph->initializeSpline(); }

  void setDefaultCurve(QList<TPointD> curve) { m_defaultCurve = curve; }
  QList<TPointD> getDefaultCurve() { return m_defaultCurve; }

protected slots:
  void onCurvePointAdded(int);
  void onCurveChanged(bool isDragging);
  void onCurvePointRemoved(int);
  void onUpdateCurrentPosition(int, QPointF);
  void onCurveReset();

  void onOutValueChanged();
  void onInValueChanged();

signals:
  void curveChanged(bool isDragging);
};

#endif  // GRAPHWIDGET_H
