#pragma once

#ifndef GRAPHWIDGET_H
#define GRAPHWIDGET_H

#include "toonz/tstageobjectspline.h"
#include "toonzqt/intfield.h"

#include <QObject>
#include <QWidget>
#include <QMouseEvent>
#include <QImage>


class QFrame;
class TThickPoint;


//=============================================================================
// GraphArea
//-----------------------------------------------------------------------------

class GraphWidget : public QWidget
{
    Q_OBJECT

    TStroke *m_stroke;
    double m_maxXValue;
    double m_maxYValue;
    int m_selectedControlPoint = -1;
public:
    explicit GraphWidget(QWidget* parent = nullptr);

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


#endif  // GRAPHWIDGET_H
