#pragma once

#ifndef MOTIONPATHPANEL_H
#define MOTIONPATHPANEL_H

#include "toonz/tstageobjectspline.h"
#include "toonzqt/intfield.h"

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QImage>

class TPanelTitleBarButton;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QPushButton;
class QFrame;
class QToolBar;
class QSlider;
class QComboBox;
class TThickPoint;


//=============================================================================
// ClickablePathLabel
//-----------------------------------------------------------------------------

class ClickablePathLabel : public QLabel {
  Q_OBJECT

protected:
  void mouseReleaseEvent(QMouseEvent*) override;
  void enterEvent(QEvent*) override;
  void leaveEvent(QEvent*) override;

public:
  ClickablePathLabel(const QString& text, QWidget* parent = nullptr,
                     Qt::WindowFlags f = Qt::WindowFlags());
  ~ClickablePathLabel();

signals:
  void onMouseRelease(QMouseEvent* event);
};

class MotionPathControl : public QWidget {
  Q_OBJECT

  TStageObjectSpline* m_spline;
  bool m_active;
  QGridLayout* m_controlLayout;
  TPanelTitleBarButton* m_activeButton;
  ClickablePathLabel* m_nameLabel;
  DVGui::IntLineEdit* m_stepsEdit;
  QSlider* m_widthSlider;
  QComboBox* m_colorCombo;

public:
  MotionPathControl(QWidget* parent) : QWidget(parent){};
  ~MotionPathControl(){};

  void createControl(TStageObjectSpline* spline);

protected:
  void fillCombo();
};

//=============================================================================
// GraphArea
//-----------------------------------------------------------------------------

class GraphArea : public QWidget
{
    Q_OBJECT

    TStroke *m_stroke;
    double m_maxXValue;
    double m_maxYValue;
    int m_selectedControlPoint = -1;
public:
    explicit GraphArea(QWidget* parent = nullptr);

    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;
    void setStroke(TStroke *stroke) { m_stroke = stroke; }
    void clearStroke() { m_stroke = 0; }
    void setMaxXValue(int x) { m_maxXValue = x; }
    void setMaxYValue(int y) { m_maxYValue = y; }
    //public slots:
    //void setAntialiased(bool antialiased);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QPointF convertPointToInvertedLocal(TThickPoint point);
    TThickPoint convertInvertedLocalToPoint(QPointF point);

private:
    QPen pen;
    QBrush brush;
    bool antialiased;
    QPixmap pixmap;
};

//=============================================================================
// MotionPathPanel
//-----------------------------------------------------------------------------

class MotionPathPanel final : public QWidget {
  Q_OBJECT

  QHBoxLayout* m_toolLayout;
  QHBoxLayout* m_controlsLayout;
  QGridLayout* m_pathsLayout;
  QVBoxLayout* m_outsideLayout;
  QVBoxLayout* m_insideLayout;
  QFrame* m_mainControlsPage;
  QToolBar* m_toolbar;
  std::vector<MotionPathControl*> m_motionPathControls;
  TStageObjectSpline *m_currentSpline;
  GraphArea *m_graphArea;
public:
  MotionPathPanel(QWidget* parent = 0);
  ~MotionPathPanel();

protected:
  void clearPathsLayout();
  void newPath();

protected slots:
  void refreshPaths();

  // public slots:
};


#endif  // MOTIONPATHPANEL_H
