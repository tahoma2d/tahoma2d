#include "graphwidget.h"
#include "tapp.h"
//#include "toonz/txsheethandle.h"
//#include "toonz/txsheet.h"
#include "toonz/tobjecthandle.h"
//#include "toonz/tstageobjecttree.h"
//#include "toonz/tstageobjectcmd.h"
//#include "toonzqt/menubarcommand.h"
//#include "toonz/tscenehandle.h"
#include "toonzqt/gutil.h"
//#include "toonzqt/menubarcommand.h"
//#include "menubarcommandids.h"
//#include "tstroke.h"
#include "tgeometry.h"

//#include "pane.h"

#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>

//-----------------------------------------------------------------------------
namespace {

// minimum distance between control points
const double cpMargin = 4.0;

//-----------------------------------------------------------------------------

// not sure if i'll be using this.

double distanceSquared(QPoint p1, QPoint p2) {
  int newX = p1.x() - p2.x();
  int newY = p1.y() - p2.y();
  return (newX * newX) + (newY * newY);
}

//-----------------------------------------------------------------------------

double getPercentAtPoint(QPointF point, QPainterPath path) {
  int i;
  for (i = 1; i < 100; i++) {
    double p          = double(i) * 0.01;
    QPointF pathPoint = path.pointAtPercent(p);
    if (std::abs(pathPoint.x() - point.x()) < 3 &&
        std::abs(pathPoint.y() - point.y()) < 3)
      return p;
  }
  return 0;
}

//-----------------------------------------------------------------------------

QList<QPointF> getIntersectedPoint(QRectF rect, QPainterPath path) {
  int y0 = rect.top();
  int y1 = rect.bottom();
  QList<QPointF> points;
  double g      = 1.0 / 256.0;
  QPointF prec  = path.pointAtPercent(0);
  QPointF point = path.pointAtPercent(g);
  int j         = 0;
  for (j = 2; j < 256; j++) {
    QPointF next = path.pointAtPercent(double(j) * g);
    if (prec.y() >= y0 && prec.y() <= y1) {
      if (point.y() < y0)
        points.push_back(QPointF(prec.x(), y0));
      else if (point.y() > y1)
        points.push_back(QPointF(prec.x(), y1));
    }
    if (next.y() >= y0 && next.y() <= y1) {
      if (point.y() < y0)
        points.push_back(QPointF(next.x(), y0));
      else if (point.y() > y1)
        points.push_back(QPointF(next.x(), y1));
    }
    prec  = point;
    point = next;
  }
  return points;
}

//-----------------------------------------------------------------------------

double qtNorm2(const QPointF& p) { return p.x() * p.x() + p.y() * p.y(); }

double qtDistance2(const QPointF& p1, const QPointF& p2) {
  return qtNorm2(p2 - p1);
}

//---------------------------------------------------------
// referred to the truncateSpeeds() in tdoubleparam.cpp
void truncateSpeeds(double aFrame, double bFrame, QVector2D& aSpeedTrunc,
                    QVector2D& bSpeedTrunc) {
  double deltaX = bFrame - aFrame;
  if (aSpeedTrunc.x() < 0) aSpeedTrunc.setX(0);
  if (bSpeedTrunc.x() > 0) bSpeedTrunc.setX(0);

  if (aFrame + aSpeedTrunc.x() > bFrame) {
    if (aSpeedTrunc.x() != 0) {
      aSpeedTrunc = aSpeedTrunc * (deltaX / aSpeedTrunc.x());
    }
  }

  if (bFrame + bSpeedTrunc.x() < aFrame) {
    if (bSpeedTrunc.x() != 0) {
      bSpeedTrunc = -bSpeedTrunc * (deltaX / bSpeedTrunc.x());
    }
  }
}

}  // anonymous namespace
//-----------------------------------------------------------------------------

//=============================================================================

GraphWidget::GraphWidget(QWidget* parent)
    : QWidget(parent)
    //, m_histogramView(histogramView)
    , m_currentControlPointIndex(-1)
    , m_mouseButton(Qt::NoButton)
    , m_maxXValue(255.0)
    , m_maxYValue(255.0)
    , m_LeftRightMargin(0)
    , m_TopMargin(0)
    , m_BottomMargin(0)
    , m_isLinear(false)
    , m_isEnlarged(false) {
  //setFixedSize(m_curveHeight + 2 * m_LeftRightMargin,
  //             m_curveHeight + m_TopMargin + m_BottomMargin);
  setAttribute(Qt::WA_KeyCompression);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
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

//-----------------------------------------------------------------------------

void GraphWidget::setPoints(QList<TPointD> points) {
  if (!m_points.isEmpty()) m_points.clear();
  for (const TPointD& point : points)
    m_points.push_back(QPointF(point.x, point.y));
  // update slider position according to the first and the last control points
  int firstIndex = 3;
  int lastIndex  = m_points.size() - 4;
  emit firstLastXPostionChanged(m_points.at(firstIndex).x(),
                                m_points.at(lastIndex).x());
  update();
}

//-----------------------------------------------------------------------------

QList<TPointD> GraphWidget::getPoints() {
  QList<TPointD> points;
  if (m_points.isEmpty()) return points;
  for (const QPointF& point : m_points)
    points.push_back(TPointD(point.x(), point.y()));
  return points;
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::convertPointToLocal(QPointF point) {
    float daWidth = (float)width();
    float daHeight = (float)height();

    float outX = (point.x() / m_maxXValue) * daWidth;
    float outY = (point.y() / m_maxXValue) * daHeight;

    return QPointF(outX, outY);
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::convertPointFromLocal(QPointF point) {
    float daWidth = (float)width();
    float daHeight = (float)height();

    float outX = (point.x() / daWidth) * m_maxXValue;
    float outY = (point.y() / daHeight) * m_maxYValue;

    return QPointF(outX, outY);
}

//-----------------------------------------------------------------------------

void GraphWidget::setFirstLastXPosition(std::pair<double, double> values,
                                        bool isDragging) {
  QPointF newX0(values.first, 0);
  QPointF newX1(values.second, 0);

  int indexX0 = 3;
  int indexX1 = m_points.size() - 4;
  QPointF x0  = m_points.at(indexX0);
  QPointF x1  = m_points.at(indexX1);
  if (x0.x() != newX0.x()) {
    QPointF delta(newX0.x() - x0.x(), 0);
    moveCentralControlPoint(indexX0, delta);
    update();
  }
  if (x1.x() != newX1.x()) {
    QPointF delta(newX1.x() - x1.x(), 0);
    moveCentralControlPoint(indexX1, delta);
    update();
  }
  m_currentControlPointIndex = -1;

  if (!isDragging) emit controlPointChanged(false);
}

//-----------------------------------------------------------------------------

void GraphWidget::setLinear(bool isLinear) {
  if (m_isLinear == isLinear) return;
  m_isLinear = isLinear;
  update();
}

//-----------------------------------------------------------------------------

//void GraphWidget::setEnlarged(bool isEnlarged) {
//  if (m_isEnlarged == isEnlarged) return;
//  m_isEnlarged     = isEnlarged;
//  int widgetHeight = (m_isEnlarged) ? m_curveHeight * 2 : m_curveHeight;
//  setFixedSize(widgetHeight + 2 * m_LeftRightMargin + 2,
//               widgetHeight + m_TopMargin + m_BottomMargin);
//  // m_histogramView->setGraphHeight(widgetHeight);
//  // m_verticalChannelBar->setFixedHeight(widgetHeight + 22);
//  update();
//}

//-----------------------------------------------------------------------------

// void GraphWidget::setLabelRange(ChannelBar::Range range) {
//    m_histogramView->channelBar()->setLabelRange(range);
//    m_verticalChannelBar->setLabelRange(range);
//}

//-----------------------------------------------------------------------------

QPointF GraphWidget::checkPoint(const QPointF p) {
  QPointF checkedP = p;
  if (p.x() < 0)
    checkedP.setX(0);
  else if (p.x() > m_maxXValue)
    checkedP.setX(m_maxXValue);
  if (p.y() < 0)
    checkedP.setY(0);
  else if (p.y() > m_maxYValue)
    checkedP.setY(m_maxYValue);
  return checkedP;
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::getVisibleHandlePos(int index) {
  QRectF rect(0.0, 0.0, width(), height());
  QPointF handlePos(m_points.at(index));
  //handlePos = convertPointToLocal(handlePos);
  if (isCentralControlPoint(index) || rect.contains(handlePos))
    return handlePos;

  if (isLeftControlPoint(index)) {
    QPointF cp = m_points.at(index + 1);
    QVector2D lHandle(handlePos - cp);
    if (handlePos.x() < 0) {
      float ratio = -cp.x() / lHandle.x();
      handlePos   = cp + lHandle.toPointF() * ratio;
    }
    if (handlePos.y() < 0) {
      float ratio = -cp.y() / lHandle.y();
      handlePos   = cp + lHandle.toPointF() * ratio;
    } else if (handlePos.y() > 256) {
      float ratio = (256 - cp.y()) / lHandle.y();
      handlePos   = cp + lHandle.toPointF() * ratio;
    }
  } else {  // isRightControlPoint
    QPointF cp = m_points.at(index - 1);
    QVector2D rHandle(handlePos - cp);
    if (handlePos.x() > m_maxXValue) {
      float ratio = (m_maxXValue - cp.x()) / rHandle.x();
      handlePos   = cp + rHandle.toPointF() * ratio;
    }
    if (handlePos.y() < 0) {
      float ratio = -cp.y() / rHandle.y();
      handlePos   = cp + rHandle.toPointF() * ratio;
    } else if (handlePos.y() > m_maxYValue) {
      float ratio = (m_maxYValue - cp.y()) / rHandle.y();
      handlePos   = cp + rHandle.toPointF() * ratio;
    }
  }
  return handlePos;
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::viewToStrokePoint(const QPointF& p) {
  double x = p.x() - m_LeftRightMargin - 1;
  double y = p.y() - m_TopMargin;
  if (m_isEnlarged) {
    x *= 0.5;
    y *= 0.5;
  }

  return convertPointFromLocal(QPointF(x, ((double)height()) - y));
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::getInvertedPoint(QPointF p) {
    double x = p.x() - m_LeftRightMargin - 1;
    double y = p.y() - m_TopMargin;
    if (m_isEnlarged) {
        x *= 0.5;
        y *= 0.5;
    }

    else return QPointF(x, height() - y);
}

//-----------------------------------------------------------------------------

int GraphWidget::getClosestPointIndex(QPointF& pos,
                                      double& minDistance2) {
  int closestPointIndex = -1;
  minDistance2          = 0;
  enum pointType { Handle = 0, ControlPoint, PseudoHandle } closestPointType;
  //QRectF rect(0, 0, width(), m_curveHeight);
  int i;
  for (i = 3; i < (int)m_points.size() - 3; i++) {
    if (m_isLinear && !isCentralControlPoint(i)) continue;
    QPointF visiblePoint = getVisibleHandlePos(i);

    pointType type =
        (isCentralControlPoint(i))
            ? ControlPoint
            : (visiblePoint == m_points.at(i)) ? Handle : PseudoHandle;

    double distance2 = qtDistance2(pos, visiblePoint);
    if (closestPointIndex < 0 || distance2 < minDistance2 ||
        (distance2 == minDistance2 && type < closestPointType)) {
      minDistance2      = distance2;
      closestPointIndex = i;
      closestPointType  = type;
    }
  }
  return closestPointIndex;
}

//-----------------------------------------------------------------------------

void GraphWidget::movePoint(int index, const QPointF delta) {
  QPointF p = m_points.at(index);
  p += delta;
  setPoint(index, p);

  int firstIndex = 3;
  int lastIndex  = m_points.size() - 4;
  if (index == firstIndex)
    emit firstLastXPostionChanged(p.x(), m_points.at(lastIndex).x());
  if (index == lastIndex)
    emit firstLastXPostionChanged(m_points.at(firstIndex).x(), p.x());
}

//-----------------------------------------------------------------------------

void GraphWidget::setPoint(int index, const QPointF p) {
  m_points.removeAt(index);
  m_points.insert(index, p);

  int firstIndex = 3;
  int lastIndex  = m_points.size() - 4;
  if (index == firstIndex)
    emit firstLastXPostionChanged(p.x(), m_points.at(lastIndex).x());
  if (index == lastIndex)
    emit firstLastXPostionChanged(m_points.at(firstIndex).x(), p.x());
}

//-----------------------------------------------------------------------------

void GraphWidget::moveCurrentControlPoint(QPointF delta) {
  assert(m_currentControlPointIndex != -1);
  int pointCount = m_points.size();
  // in case moving the control point
  if (isCentralControlPoint(m_currentControlPointIndex))
    moveCentralControlPoint(m_currentControlPointIndex, delta);
  // in case moving the left handle
  else if (isLeftControlPoint(m_currentControlPointIndex)) {
    QPointF cp   = m_points.at(m_currentControlPointIndex + 1);
    QPointF left = m_points.at(m_currentControlPointIndex);

    // handle should not move across the control point
    if (left.x() + delta.x() > cp.x()) delta.setX(cp.x() - left.x());

    left += delta;
    setPoint(m_currentControlPointIndex, left);

    // rotate the opposite handle keeping the handle length unchanged
    if (m_currentControlPointIndex < pointCount - 5) {
      QVector2D lHandle(cp - left);
      if (!lHandle.isNull()) {
        QPointF right = m_points.at(m_currentControlPointIndex + 2);
        QVector2D rHandle(right - cp);
        QVector2D newRHandle = lHandle.normalized() * rHandle.length();
        setPoint(m_currentControlPointIndex + 2, cp + newRHandle.toPointF());
      }
    }
    emit controlPointChanged(true);
  }
  // in case moving the right handle
  else {
    assert(isRightControlPoint(m_currentControlPointIndex));
    QPointF cp    = m_points.at(m_currentControlPointIndex - 1);
    QPointF right = m_points.at(m_currentControlPointIndex);

    // handle should not move across the control point
    if (right.x() + delta.x() < cp.x()) delta.setX(cp.x() - right.x());

    right += delta;
    setPoint(m_currentControlPointIndex, right);

    // rotate the opposite handle keeping the handle length unchanged
    if (m_currentControlPointIndex > 4) {
      QVector2D rHandle(cp - right);
      if (!rHandle.isNull()) {
        QPointF left = m_points.at(m_currentControlPointIndex - 2);
        QVector2D lHandle(left - cp);
        QVector2D newLHandle = rHandle.normalized() * lHandle.length();
        setPoint(m_currentControlPointIndex - 2, cp + newLHandle.toPointF());
      }
    }
    emit controlPointChanged(true);
  }
  update();
  emit updateCurrentPosition(m_currentControlPointIndex,
                             m_points.at(m_currentControlPointIndex));
}

//-----------------------------------------------------------------------------

void GraphWidget::moveCentralControlPoint(int index, QPointF delta) {
  int pointCount = m_points.size();

  assert(index < pointCount - 3 && index > 2);

  QPointF p = m_points.at(index);
  QPointF d = delta;
  // Trovo il valore di delta im modo tale che il punto di controllo non sia
  // trascinato fuori dal range consentito
  QPointF newPoint = checkPoint(p + delta);
  d                = newPoint - p;

  QPointF nextP       = m_points.at(index + 3);
  QPointF precP       = m_points.at(index - 3);
  double nextDistance = nextP.x() - (p.x() + d.x());
  double precDistance = (p.x() + d.x()) - precP.x();

  if (nextDistance < cpMargin)
    d = QPointF(nextP.x() - p.x() - cpMargin, d.y());
  else if (precDistance < cpMargin)
    d = QPointF(precP.x() - p.x() + cpMargin, d.y());

  if (d.isNull()) return;

  // Punto di controllo speciale: il primo visualizzato.
  if (index == 3) {
    QPointF dY = QPointF(0, d.y());
    movePoint(index - 1, dY);
    movePoint(index - 2, dY);
    movePoint(index - 3, dY);
  }
  // Punto di controllo speciale: l'ultimo visualizzato.
  if (index == pointCount - 4) {
    QPointF dY = QPointF(0, d.y());
    movePoint(index + 1, dY);
    movePoint(index + 2, dY);
    movePoint(index + 3, dY);
  }
  if (index > 3) movePoint(index - 1, d);
  if (index < pointCount - 4) movePoint(index + 1, d);
  movePoint(index, d);
  emit controlPointChanged(true);
}

//-----------------------------------------------------------------------------

void GraphWidget::addControlPoint(double percent) {
  QPainterPath path = getPainterPath();
  QPointF p         = convertPointFromLocal(path.pointAtPercent(percent));

  // Cerco il punto di controllo precedente
  int pointCount = m_points.size();
  int beforeControlPointIndex;
  for (beforeControlPointIndex = pointCount - 1; beforeControlPointIndex >= 0;
       beforeControlPointIndex--) {
    QPointF point = m_points.at(beforeControlPointIndex);
    if (isCentralControlPoint(beforeControlPointIndex) && point.x() < p.x())
      break;
  }

  if (beforeControlPointIndex == 0 || beforeControlPointIndex == pointCount - 4)
    return;

  QPointF p0 = checkPoint(m_points.at(beforeControlPointIndex));
  // Se sono troppo vicino al punto di controllo precedente ritorno
  if (std::abs(p.x() - p0.x()) <= cpMargin) return;
  double beforeControlPointPercent = getPercentAtPoint(convertPointToLocal(p0), path);
  QPointF p1 = checkPoint(m_points.at(beforeControlPointIndex + 1));
  QPointF p2 = checkPoint(m_points.at(beforeControlPointIndex + 2));
  QPointF p3 = checkPoint(m_points.at(beforeControlPointIndex + 3));
  // Se sono troppo vicino al punto di controllo successivo ritorno
  if (std::abs(p3.x() - p.x()) <= cpMargin) return;
  double nextControlPointPercent = getPercentAtPoint(convertPointToLocal(p3), path);

  // Calcolo la velocita' e quindi il coiffciente angolare.
  double t =
      percent * 100 / (nextControlPointPercent - beforeControlPointPercent);
  double s = t - 1;
  QPointF speed =
      3.0 * ((p1 - p0) * s * s + 2 * (p2 - p0) * s * t + (p3 - p2) * t * t);
  double m = speed.y() / speed.x();

  int newControlPointIndex = beforeControlPointIndex + 3;
  m_points.insert(newControlPointIndex - 1,
                  QPointF(p.x() - 16, p.y() - 16 * m));
  m_points.insert(newControlPointIndex, p);
  m_points.insert(newControlPointIndex + 1,
                  QPointF(p.x() + 16, p.y() + 16 * m));

  m_currentControlPointIndex = newControlPointIndex;
  m_preMousePos              = p;
  emit controlPointAdded(newControlPointIndex);
  update();
}

//-----------------------------------------------------------------------------

void GraphWidget::removeCurrentControlPoint() {
  removeControlPoint(m_currentControlPointIndex);
}

//-----------------------------------------------------------------------------

void GraphWidget::selectNextControlPoint() {
  int controlPointCount = (int)m_points.size();

  if (controlPointCount == 0) return;

  int firstVisibleControlPoint = 3;
  int lastVisibleControlPoint  = m_points.size() - 4;

  m_currentControlPointIndex++;
  if (m_currentControlPointIndex < firstVisibleControlPoint ||
      m_currentControlPointIndex > lastVisibleControlPoint)
    m_currentControlPointIndex = firstVisibleControlPoint;

  emit updateCurrentPosition(m_currentControlPointIndex,
                             m_points.at(m_currentControlPointIndex));
  update();
}

//-----------------------------------------------------------------------------

void GraphWidget::selectPreviousControlPoint() {
  int controlPointCount = (int)m_points.size();

  if (controlPointCount == 0) return;

  int firstVisibleControlPoint = 3;
  int lastVisibleControlPoint  = m_points.size() - 4;

  m_currentControlPointIndex--;
  if (m_currentControlPointIndex < firstVisibleControlPoint ||
      m_currentControlPointIndex > lastVisibleControlPoint)
    m_currentControlPointIndex = lastVisibleControlPoint;

  emit updateCurrentPosition(m_currentControlPointIndex,
                             m_points.at(m_currentControlPointIndex));
  update();
}

//-----------------------------------------------------------------------------

void GraphWidget::removeControlPoint(int index) {
  // Non posso eliminare il primo punto di controllo visibile quindi lo rimetto
  // in condizione iniziale
  if (index <= 4) {
    setPoint(0, QPointF(-40, 0));
    setPoint(1, QPointF(-20, 0));
    setPoint(2, QPointF(-20, 0));
    setPoint(3, QPointF(0, 0));
    setPoint(4, QPointF(16, 16));
    update();
    emit controlPointChanged(false);
    return;
  }
  // Non posso eliminare il l'ultimo punto di controllo visibile quindi lo
  // rimetto in condizione iniziale
  if (index >= m_points.size() - 5) {
    int i = m_points.size() - 5;
    setPoint(i, QPointF(239, 239));
    setPoint(i + 1, QPointF(255, 255));
    setPoint(i + 2, QPointF(275, 255));
    setPoint(i + 3, QPointF(275, 255));
    setPoint(i + 4, QPointF(295, 255));
    update();
    emit controlPointChanged(false);
    return;
  }

  int firstIndex = 0;
  if (isCentralControlPoint(index))
    firstIndex = index - 1;
  else if (isLeftControlPoint(index))
    firstIndex = index;
  else
    firstIndex = index - 2;

  m_points.removeAt(firstIndex);
  m_points.removeAt(firstIndex);
  m_points.removeAt(firstIndex);

  emit controlPointRemoved(firstIndex + 1);
  m_currentControlPointIndex = firstIndex - 2;

  emit updateCurrentPosition(m_currentControlPointIndex,
                             m_points.at(m_currentControlPointIndex));

  update();
}

//-----------------------------------------------------------------------------

QPainterPath GraphWidget::getPainterPath() {
  int pointCount = m_points.size();
  if (pointCount == 0) return QPainterPath();

  QPointF p0 = convertPointToLocal(m_points.at(0));
  QPainterPath path(p0);
  int i;
  for (i = 1; i < pointCount; i++) {
    QPointF p1 = convertPointToLocal(m_points.at(i));
    QPointF p2 = convertPointToLocal(m_points.at(++i));
    QPointF p3 = convertPointToLocal(m_points.at(++i));
    path.moveTo(p0);
    if (!m_isLinear) {
      // truncate speed
      QVector2D aSpeed(p1 - p0);
      QVector2D bSpeed(p2 - p3);
      truncateSpeeds(p0.x(), p3.x(), aSpeed, bSpeed);
      path.cubicTo(p0 + aSpeed.toPointF(), p3 + bSpeed.toPointF(), p3);
    } else
      path.lineTo(p3);
    p0 = p3;
  }

  // Cerco le eventuali intersezioni con il bordo.
  QRectF rect(0, 0, width(), height());
  // QRectF rect(m_LeftRightMargin, m_TopMargin, m_curveHeight, m_curveHeight);
  QRectF r = path.boundingRect();
  if (!rect.contains(QRect(rect.left(), r.top(), rect.width(), r.height()))) {
    QList<QPointF> points = getIntersectedPoint(rect, path);
    // Se trovo punti di intersezione (per come e' definita la curva devono
    // essere pari)
    // faccio l'unione del path calcolato e di nuovi path lineari.
    int j = 0;
    for (j = 0; j < points.size(); j++) {
      QPointF p0 = points.at(j);
      QPointF p1 = points.at(++j);
      QPainterPath line(p0);
      line.lineTo(p1);
      path.addPath(line);
    }
  }

  return path;
}

//-----------------------------------------------------------------------------

void GraphWidget::paintEvent(QPaintEvent* e) {
  QPainter painter(this);

  double scale = (m_isEnlarged) ? 2.0 : 1.0;
  // Disegno il reticolato
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(QColor(250, 250, 250));

  //// Disegno l'histogram.
  // m_histogramView->draw(&painter, QPoint(m_LeftRightMargin - 10, 0),
  //    m_curveHeight * scale);

  //// Disegno la barra verticale a sinistra.
  // m_verticalChannelBar->draw(
  //    &painter,
  //    QPoint(0, -2));  //-1 == m_topMargin- il margine della barra(=10+1).

  QRectF r = rect().adjusted(m_LeftRightMargin, m_TopMargin, -m_LeftRightMargin,
                             -m_BottomMargin);
  // Disegno la curva entro i limiti del grafo
  painter.setClipRect(r, Qt::IntersectClip);

  painter.translate(m_LeftRightMargin + 1, height() - m_BottomMargin);
  painter.scale(scale, -scale);

  QPainterPath path = getPainterPath();
  if (path.isEmpty()) return;
  painter.setRenderHint(QPainter::Antialiasing, true);
  QPen blackPen(Qt::black);
  QPen bluePen(Qt::blue);
  blackPen.setWidthF(1.0 / scale);
  bluePen.setWidthF(1.0 / scale);

  painter.setPen(blackPen);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);

  int n     = m_points.size();
  QPointF p = convertPointToLocal(m_points.at(3));
  for (int i = 3; i < n - 3; i++) {
    QBrush brush(Qt::white);
    int rad       = (m_isEnlarged) ? 1 : 2;
    QPointF nextP = convertPointToLocal(m_points.at(i + 1));
    if (isCentralControlPoint(i))
      rad = (m_isEnlarged) ? 2 : 3;
    else if (m_isLinear) {
      p = nextP;
      continue;
    }
    painter.setPen(blackPen);

    QPointF handlePos = p;
    if (!m_isLinear) {
      if (isLeftControlPoint(i)) {
        painter.drawLine(p, nextP);
      } else if (isCentralControlPoint(i) && i < n - 4)
        painter.drawLine(p, nextP);

      handlePos = convertPointToLocal(getVisibleHandlePos(i));
    }

    painter.setBrush((m_currentControlPointIndex != i)
                         ? Qt::white
                         : (p == handlePos) ? Qt::black : Qt::blue);
    painter.setPen((p == handlePos) ? blackPen : bluePen);

    QRectF pointRect(handlePos.x() - rad, handlePos.y() - rad, 2 * rad,
                     2 * rad);
    painter.drawEllipse(pointRect);
    p = nextP;
  }
}

//-----------------------------------------------------------------------------

void GraphWidget::mouseMoveEvent(QMouseEvent* e) {
  QPoint tempPos = e->pos();
  int x          = tempPos.x();
  int y          = tempPos.y();

  if (m_constrainToBounds) {
      if (x < 0)
          x = 0;
      else if (x > width())
      x = width();
      if (y < 0)
          y = 0;
      else if (y > height())
      y = height();
    tempPos.setX(x);
    tempPos.setY(y);
  }
  QPointF posF = viewToStrokePoint(QPointF(tempPos));
  if (m_mouseButton == Qt::LeftButton && m_currentControlPointIndex != -1) {
    moveCurrentControlPoint(posF - m_preMousePos);
    m_preMousePos = posF;
  } else if (m_currentControlPointIndex == -1)
    emit updateCurrentPosition(-1, posF);
}

//-----------------------------------------------------------------------------

void GraphWidget::mousePressEvent(QMouseEvent* e) {
  m_mouseButton = e->button();
  setFocus();
  if (m_mouseButton == Qt::LeftButton) {
    QPointF posF = viewToStrokePoint(QPointF(e->pos()));
    double minDistance;
    int controlPointIndex = getClosestPointIndex(posF, minDistance);
    if (m_lockExtremePoints) {
      if (controlPointIndex <= 3 || controlPointIndex >= m_points.size() - 4) {
        m_currentControlPointIndex = -1;
        update();
        return;
      }
    }
    // Se la distanza e' piccola seleziono il control point corrente
    if (minDistance < 20) {
      m_currentControlPointIndex = controlPointIndex;
      m_preMousePos              = posF;
    } else {
      m_currentControlPointIndex = -1;
      // Se sono sufficentemente lontano da un punto di controllo, ma abbastanza
      // vicino alla curva
      // aggiungo un punto di controllo
      double percent = getPercentAtPoint(getInvertedPoint(e->pos()), getPainterPath());
      if (percent != 0 && minDistance > 20) addControlPoint(percent);
    }
    QPointF currentPointPos = (m_currentControlPointIndex == -1)
                                  ? posF
                                  : m_points.at(m_currentControlPointIndex);

    emit updateCurrentPosition(m_currentControlPointIndex, currentPointPos);
    update();
  }
}

//-----------------------------------------------------------------------------

void GraphWidget::mouseReleaseEvent(QMouseEvent* e) {
  // fx preview updates here ( it does not update while mouse dragging )
  if (m_mouseButton == Qt::LeftButton && m_currentControlPointIndex != -1 &&
      e->button() == Qt::LeftButton)
    emit controlPointChanged(false);
  m_mouseButton = Qt::NoButton;
}

//-----------------------------------------------------------------------------

void GraphWidget::keyPressEvent(QKeyEvent* e) {
  if (m_currentControlPointIndex == -1) return;

  if (e->key() == Qt::Key_Delete) {
    removeCurrentControlPoint();
    return;
  }

  bool controlPressed = e->modifiers() & Qt::ControlModifier;
  bool shiftPressed   = e->modifiers() & Qt::ShiftModifier;
  float delta         = (shiftPressed) ? 10.0 : 1.0;

  if (e->key() == Qt::Key_Right) {
    if (controlPressed)
      selectNextControlPoint();
    else
      moveCurrentControlPoint(QPointF(delta, 0.0));
  } else if (e->key() == Qt::Key_Left) {
    if (controlPressed)
      selectPreviousControlPoint();
    else
      moveCurrentControlPoint(QPointF(-delta, 0.0));
  } else if (e->key() == Qt::Key_Up)
    moveCurrentControlPoint(QPointF(0.0, delta));
  else if (e->key() == Qt::Key_Down)
    moveCurrentControlPoint(QPointF(0.0, -delta));
}

//--------------------------------------------------------

bool GraphWidget::eventFilter(QObject* object, QEvent* event) {
  if (event->type() == QEvent::Shortcut ||
      event->type() == QEvent::ShortcutOverride) {
    if (!object->inherits("FxSettings")) {
      event->accept();
      return true;
    }
  }
  return false;
}

//--------------------------------------------------------

void GraphWidget::focusInEvent(QFocusEvent* fe) {
  QWidget::focusInEvent(fe);
  qApp->installEventFilter(this);
}

//--------------------------------------------------------

void GraphWidget::focusOutEvent(QFocusEvent* fe) {
  emit focusOut();
  QWidget::focusOutEvent(fe);
  qApp->removeEventFilter(this);
  update();
}
