#include "graphwidget.h"
#include "tapp.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectcmd.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/gutil.h"
#include "toonzqt/menubarcommand.h"
#include "menubarcommandids.h"
#include "tstroke.h"
#include "tgeometry.h"

#include "pane.h"

#include <QPixmap>
#include <QPainter>
#include <QPainterPath>


namespace {
    double distanceSquared(QPoint p1, QPoint p2) {
        int newX = p1.x() - p2.x();
        int newY = p1.y() - p2.y();
        return (newX * newX) + (newY * newY);
    }
};

//=============================================================================

GraphWidget::GraphWidget(QWidget* parent)
    : QWidget(parent)
{
    antialiased = false;
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    m_stroke = 0;
}

//=============================================================================

QPointF GraphWidget::convertPointToInvertedLocal(TThickPoint point) {
    float daWidth = (float)width();
    float daHeight = (float)height();

    float outX = (point.x / m_maxXValue) * daWidth;
    float outY = (point.y / m_maxXValue) * daHeight;

    outY = std::abs(outY - daHeight);
    return QPointF(outX, outY);
}

//=============================================================================

TThickPoint GraphWidget::convertInvertedLocalToPoint(QPointF point) {
    float daWidth = (float)width();
    float daHeight = (float)height();

    float outX = (point.x() / daWidth) * m_maxXValue;
    float outY = (point.y() / daHeight) * m_maxYValue;

    outY = std::abs(outY - m_maxYValue);
    return TThickPoint(outX, outY);
}

//=============================================================================

QSize GraphWidget::sizeHint() const
{
    return QSize(400, 200);
}

//=============================================================================

QSize GraphWidget::minimumSizeHint() const
{
    return QSize(100, 100);
}

//=============================================================================

void GraphWidget::paintEvent(QPaintEvent* /* event */)
{
    
    QPainter painter(this);
    painter.setPen(QColor(230, 230, 230));
    painter.setBrush(brush);
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (m_stroke) {
        std::vector<TThickPoint> points;
        m_stroke->getControlPoints(points);
        if (points.size() > 0) {
            assert(points.size() % 2);

            //for (int i = 0; i + 2 < points.size(); i += 2) {
                painter.setPen(QColor(230, 230, 230));
                painter.setBrush(Qt::NoBrush);
                QPointF start = convertPointToInvertedLocal(points.at(0));
                QPointF control1 = convertPointToInvertedLocal(points.at(1));
                QPointF control2 = convertPointToInvertedLocal(points.at(3));
                QPointF end = convertPointToInvertedLocal(points.at(4));

                QPainterPath path;
                path.moveTo(start);
                path.cubicTo(control1, control2, end);
                painter.drawPath(path);
                if (1 == m_selectedControlPoint) painter.setPen(Qt::red);
                painter.setBrush(Qt::red);
                painter.drawEllipse(control1, 6, 6);
                painter.drawEllipse(control2, 6, 6);
            //}
            painter.setPen(QColor(230, 230, 230));
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(convertPointToInvertedLocal(points.at(0)), convertPointToInvertedLocal(points.at(1)));
            painter.drawLine(convertPointToInvertedLocal(points.at(3)), convertPointToInvertedLocal(points.at(4)));
        }
    }

    

    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(palette().dark().color());
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRect(0, 0, width() - 1, height() - 1));
}

//=============================================================================

void GraphWidget::mousePressEvent(QMouseEvent* event) {
    if (!m_stroke) return;
    float daWidth = (float)width();
    float daHeight = (float)height();
    int x = event->pos().x();
    int y = event->pos().y();
    double distance = 50000;
    int selectedPoint = -0;
    TThickPoint unlocal(convertInvertedLocalToPoint(QPointF(x, y)));

    std::vector<QPoint> localPoints;
    int i = 0;
    for (; i < m_stroke->getControlPointCount(); i++) {
        localPoints.push_back(convertPointToInvertedLocal(m_stroke->getControlPoint(i)).toPoint());
    }
    i = 1;
    // don't select the first or last points
    for (; i < localPoints.size() - 1; i++) {
        double newDistance = distanceSquared(event->pos(), localPoints.at(i));
        if (newDistance < distance) {
            distance = newDistance;
            selectedPoint = i;
        }
    }

    if (distance < 50.0) {
        m_selectedControlPoint = selectedPoint;
    }
    else m_selectedControlPoint = -1;
    update();
}

//=============================================================================

void GraphWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_selectedControlPoint < 1 || !m_stroke) return;
    int x = std::min(std::max(1, event->pos().x()), width() - 1);
    int y = std::min(std::max(1, event->pos().y()), height() - 1);
    TThickPoint unlocal(convertInvertedLocalToPoint(QPointF(x, y)));
    m_stroke->setControlPoint(m_selectedControlPoint, unlocal);
    bool changed = false;
    if ((m_selectedControlPoint + 1) % 4 == 0) {
        TThickPoint prevPoint1 = m_stroke->getControlPoint(m_selectedControlPoint - 1);
        TThickPoint prevPoint2 = m_stroke->getControlPoint(m_selectedControlPoint - 2);
        double newX = (unlocal.x + prevPoint2.x) / 2;
        double newY = (unlocal.y + prevPoint2.y) / 2;
        prevPoint1 = TThickPoint(newX, newY);
        m_stroke->setControlPoint(m_selectedControlPoint - 1, prevPoint1);
        changed = true;
    }
    else if ((m_selectedControlPoint + 1) % 2 == 0) {
        TThickPoint nextPoint1 = m_stroke->getControlPoint(m_selectedControlPoint + 1);
        TThickPoint nextPoint2 = m_stroke->getControlPoint(m_selectedControlPoint + 2);
        double newX = (unlocal.x + nextPoint2.x) / 2;
        double newY = (unlocal.y + nextPoint2.y) / 2;
        nextPoint1 = TThickPoint(newX, newY);
        m_stroke->setControlPoint(m_selectedControlPoint + 1, nextPoint1);
        changed = true;
    }
    if (changed) TApp::instance()->getCurrentScene()->notifySceneChanged();
    update();
}

//=============================================================================

void GraphWidget::mouseReleaseEvent(QMouseEvent* event) {
    m_selectedControlPoint = -1;
    update();
}

