#include "toonzqt/graphwidget.h"
#include "toonz/tobjecthandle.h"
#include "toonzqt/gutil.h"
#include "tgeometry.h"

#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QHBoxLayout>

//-----------------------------------------------------------------------------
namespace {

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
    , m_minXValue(0.0)
    , m_maxXValue(255.0)
    , m_minYValue(0.0)
    , m_maxYValue(255.0)
    , m_xyOffset(0.0, 0.0)
    , m_cpMargin(20.0)
    , m_LeftRightMargin(0)
    , m_TopMargin(0)
    , m_BottomMargin(0)
    , m_constrainToBounds(true)
    , m_isLinear(false)
    , m_isEnlarged(false)
    , m_lockExtremePoints(true)
    , m_lockExtremeXPoints(false)
    , m_cursorPos(-1, -1)
    , m_gridSpacing(8)
    , m_isDragging(false) {
  // setFixedSize(m_curveHeight + 2 * m_LeftRightMargin,
  //             m_curveHeight + m_TopMargin + m_BottomMargin);
  setAttribute(Qt::WA_KeyCompression);
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setObjectName("GraphAreaWidget");
}

//=============================================================================

QSize GraphWidget::sizeHint() const { return QSize(400, 200); }

//=============================================================================

QSize GraphWidget::minimumSizeHint() const { return QSize(100, 100); }

//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------

void GraphWidget::setPoints(QList<TPointD> points) {
  double xMax = m_maxXValue + m_xyOffset.x();

  if (!m_points.isEmpty()) m_points.clear();

  if (m_isLinear) {
    int n = points.count();

    // First point
    setPoint(0, QPointF(-40, points[0].y + m_xyOffset.y()));
    setPoint(1, QPointF(-20, points[0].y + m_xyOffset.y()));
    setPoint(2, QPointF(-20, points[0].y + m_xyOffset.y()));
    setPoint(
        3, QPointF(points[0].x + m_xyOffset.x(), points[0].y + m_xyOffset.y()));
    setPoint(
        4, QPointF(points[0].x + m_xyOffset.x(), points[0].y + m_xyOffset.y()));

    int x = 5;
    for (int i = 1; i <= (n - 2); i++) {
      setPoint(x++, QPointF(points[i].x + m_xyOffset.x(),
                            points[i].y + m_xyOffset.y()));
      setPoint(x++, QPointF(points[i].x + m_xyOffset.x(),
                            points[i].y + m_xyOffset.y()));
      setPoint(x++, QPointF(points[i].x + m_xyOffset.x(),
                            points[i].y + m_xyOffset.y()));
    }

    // Last point
    setPoint(x, QPointF(points[n - 1].x + m_xyOffset.x(),
                        points[n - 1].y + m_xyOffset.y()));
    setPoint(x + 1, QPointF(points[n - 1].x + m_xyOffset.x(),
                            points[n - 1].y + m_xyOffset.y()));
    setPoint(x + 2, QPointF(xMax + 20, points[n - 1].y + m_xyOffset.y()));
    setPoint(x + 3, QPointF(xMax + 20, points[n - 1].y + m_xyOffset.y()));
    setPoint(x + 4, QPointF(xMax + 40, points[n - 1].y + m_xyOffset.y()));
  } else {
    for (const TPointD& point : points)
      m_points.push_back(
          QPointF(point.x + m_xyOffset.x(), point.y + m_xyOffset.y()));
  }

  m_currentControlPointIndex = -1;

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
  if (m_isLinear) {
    int n = ((m_points.count() - 1) / 3) - 1;
    for (int i = 0; i < n; i++)
      points.push_back(TPointD(m_points[(i + 1) * 3].x() - m_xyOffset.x(),
                               m_points[(i + 1) * 3].y() - m_xyOffset.y()));
  } else {
    for (const QPointF& point : m_points)
      points.push_back(
          TPointD(point.x() - m_xyOffset.x(), point.y() - m_xyOffset.y()));
  }
  return points;
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::convertPointToLocal(QPointF point) {
  float daWidth  = (float)width();
  float daHeight = (float)height();

  double xMax = m_maxXValue + m_xyOffset.x();
  double yMax = m_maxYValue + m_xyOffset.y();

  float outX = std::max(0.0, (point.x() / xMax) * daWidth - 1.0);
  float outY = (point.y() / yMax) * daHeight;

  return QPointF(outX, outY);
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::convertPointFromLocal(QPointF point) {
  float daWidth  = (float)width();
  float daHeight = (float)height();

  double xMax = m_maxXValue + m_xyOffset.x();
  double yMax = m_maxYValue + m_xyOffset.y();

  float outX = (point.x() / daWidth) * xMax;
  float outY = (point.y() / daHeight) * yMax;

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

// void GraphWidget::setEnlarged(bool isEnlarged) {
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
  double xMax      = m_maxXValue + m_xyOffset.x();
  double yMax      = m_maxYValue + m_xyOffset.y();
  if (p.x() < 0)
    checkedP.setX(0);
  else if (p.x() > xMax)
    checkedP.setX(xMax);
  if (p.y() < 0)
    checkedP.setY(0);
  else if (p.y() > yMax)
    checkedP.setY(yMax);
  return checkedP;
}

//-----------------------------------------------------------------------------

QPointF GraphWidget::getVisibleHandlePos(int index) {
  double xMax = m_maxXValue + m_xyOffset.x();
  double yMax = m_maxYValue + m_xyOffset.y();
  QRectF rect(0.0, 0.0, xMax, yMax);
  QPointF handlePos(m_points.at(index));
  // handlePos = convertPointToLocal(handlePos);
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
    if (handlePos.x() > xMax) {
      float ratio = (xMax - cp.x()) / rHandle.x();
      handlePos   = cp + rHandle.toPointF() * ratio;
    }
    if (handlePos.y() < 0) {
      float ratio = -cp.y() / rHandle.y();
      handlePos   = cp + rHandle.toPointF() * ratio;
    } else if (handlePos.y() > yMax) {
      float ratio = (yMax - cp.y()) / rHandle.y();
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

  return QPointF(x, height() - y);
}

//-----------------------------------------------------------------------------

int GraphWidget::getClosestPointIndex(QPointF& pos, double& minDistance2) {
  int closestPointIndex = -1;
  minDistance2          = 0;
  enum pointType { Handle = 0, ControlPoint, PseudoHandle } closestPointType;

  int i;
  for (i = 3; i < (int)m_points.size() - 3; i++) {
    if (m_isLinear && !isCentralControlPoint(i)) continue;
    QPointF visiblePoint = getVisibleHandlePos(i);

    pointType type = (isCentralControlPoint(i))         ? ControlPoint
                     : (visiblePoint == m_points.at(i)) ? Handle
                                                        : PseudoHandle;

    double distance2 = qtDistance2(convertPointToLocal(pos),
                                   convertPointToLocal(visiblePoint));
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
  QPointF newP = p;
  if (m_constrainToBounds) newP = checkPoint(p);
  m_points.removeAt(index);
  m_points.insert(index, newP);

  int firstIndex = 3;
  int lastIndex  = m_points.size() - 4;
  if (index == firstIndex)
    emit firstLastXPostionChanged(newP.x(), m_points.at(lastIndex).x());
  if (index == lastIndex)
    emit firstLastXPostionChanged(m_points.at(firstIndex).x(), newP.x());
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
    emit controlPointChanged(m_isDragging);
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
    emit controlPointChanged(m_isDragging);
  }
  update();
  emit updateCurrentPosition(
      m_currentControlPointIndex,
      m_points.at(m_currentControlPointIndex) - m_xyOffset);
}

//-----------------------------------------------------------------------------

void GraphWidget::moveCentralControlPoint(int index, QPointF delta) {
  int pointCount = m_points.size();

  assert(index < pointCount - 3 && index > 2);

  QPointF p = m_points.at(index);
  QPointF d = delta;

  QPointF newPoint = checkPoint(p + delta);
  d                = newPoint - p;

  QPointF nextP       = m_points.at(index + 3);
  QPointF precP       = m_points.at(index - 3);
  double nextDistance = nextP.x() - (p.x() + d.x());
  double precDistance = (p.x() + d.x()) - precP.x();

  if (index != (pointCount - 4) && nextDistance < m_cpMargin)
    d = QPointF(nextP.x() - p.x() - m_cpMargin, d.y());
  else if (index != 3 && precDistance < m_cpMargin)
    d = QPointF(precP.x() - p.x() + m_cpMargin, d.y());

  if (d.isNull()) return;

  if (index == 3) {
    QPointF dY = QPointF(0, d.y());
    movePoint(index - 1, dY);
    movePoint(index - 2, dY);
    movePoint(index - 3, dY);
    // Only allow vertical movement if enabled
    if (m_lockExtremeXPoints) d.setX(0);
  }

  if (index == pointCount - 4) {
    QPointF dY = QPointF(0, d.y());
    movePoint(index + 1, dY);
    movePoint(index + 2, dY);
    movePoint(index + 3, dY);
    // Only allow vertical movement if enabled
    if (m_lockExtremeXPoints) d.setX(0);
  }
  if (index > 3) movePoint(index - 1, d);
  if (index < pointCount - 4) movePoint(index + 1, d);
  movePoint(index, d);
  emit controlPointChanged(m_isDragging);
}

//-----------------------------------------------------------------------------

void GraphWidget::addControlPoint(double percent) {
  double xMax       = m_maxXValue + m_xyOffset.x();
  double yMax       = m_maxYValue + m_xyOffset.y();
  QPainterPath path = getPainterPath();
  QPointF p         = convertPointFromLocal(path.pointAtPercent(percent));

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

  if (std::abs(p.x() - p0.x()) <= m_cpMargin) return;
  double beforeControlPointPercent =
      getPercentAtPoint(convertPointToLocal(p0), path);
  QPointF p1 = checkPoint(m_points.at(beforeControlPointIndex + 1));
  QPointF p2 = checkPoint(m_points.at(beforeControlPointIndex + 2));
  QPointF p3 = checkPoint(m_points.at(beforeControlPointIndex + 3));

  if (std::abs(p3.x() - p.x()) <= m_cpMargin) return;
  double nextControlPointPercent =
      getPercentAtPoint(convertPointToLocal(p3), path);

  double t =
      percent * 100 / (nextControlPointPercent - beforeControlPointPercent);
  double s = t - 1;
  QPointF speed =
      3.0 * ((p1 - p0) * s * s + 2 * (p2 - p0) * s * t + (p3 - p2) * t * t);
  double m = speed.y() / speed.x();

  int newControlPointIndex = beforeControlPointIndex + 3;
  m_points.insert(newControlPointIndex - 1,
                  QPointF(p.x() - (xMax * 0.063), p.y() - (yMax * 0.063) * m));
  m_points.insert(newControlPointIndex, p);
  m_points.insert(newControlPointIndex + 1,
                  QPointF(p.x() + (xMax * 0.063), p.y() + (yMax * 0.063) * m));

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

  if (m_isLinear)
    m_currentControlPointIndex += 3;
  else
    m_currentControlPointIndex++;
  if (m_currentControlPointIndex < firstVisibleControlPoint ||
      m_currentControlPointIndex > lastVisibleControlPoint)
    m_currentControlPointIndex = firstVisibleControlPoint;

  emit updateCurrentPosition(
      m_currentControlPointIndex,
      m_points.at(m_currentControlPointIndex) - m_xyOffset);
  update();
}

//-----------------------------------------------------------------------------

void GraphWidget::selectPreviousControlPoint() {
  int controlPointCount = (int)m_points.size();

  if (controlPointCount == 0) return;

  int firstVisibleControlPoint = 3;
  int lastVisibleControlPoint  = m_points.size() - 4;

  if (m_isLinear)
    m_currentControlPointIndex -= 3;
  else
    m_currentControlPointIndex--;
  if (m_currentControlPointIndex < firstVisibleControlPoint ||
      m_currentControlPointIndex > lastVisibleControlPoint)
    m_currentControlPointIndex = lastVisibleControlPoint;

  emit updateCurrentPosition(
      m_currentControlPointIndex,
      m_points.at(m_currentControlPointIndex) - m_xyOffset);
  update();
}

//-----------------------------------------------------------------------------

void GraphWidget::initializeSpline() {
  double xMax = m_maxXValue + m_xyOffset.x();
  double yMax = m_maxYValue + m_xyOffset.y();

  m_points.clear();

  double minY = 0, maxY = yMax;
  double minHYAdj = 0.063, maxHYAdj = 0.937;

  // First point
  setPoint(0, QPointF(-40, minY));
  setPoint(1, QPointF(-20, minY));
  setPoint(2, QPointF(-20, minY));
  setPoint(3, QPointF(0, minY));
  setPoint(4, QPointF(xMax * 0.063, yMax * minHYAdj));

  // Last point
  setPoint(5, QPointF(xMax * 0.937, yMax * maxHYAdj));
  setPoint(6, QPointF(xMax, maxY));
  setPoint(7, QPointF(xMax + 20, maxY));
  setPoint(8, QPointF(xMax + 20, maxY));
  setPoint(9, QPointF(xMax + 40, maxY));
  update();

  m_currentControlPointIndex = -1;

  emit controlPointChanged(false);
}

//-----------------------------------------------------------------------------

void GraphWidget::removeControlPoint(int index) {
  double xMax = m_maxXValue + m_xyOffset.x();
  double yMax = m_maxYValue + m_xyOffset.y();

  // Don't delete the first cubic
  double minY = 0, maxY = yMax;
  double minHYAdj = 0.063, maxHYAdj = 0.937;
  if (index <= 4) {
    setPoint(0, QPointF(-40, minY));
    setPoint(1, QPointF(-20, minY));
    setPoint(2, QPointF(-20, minY));
    setPoint(3, QPointF(0, minY));
    setPoint(4, QPointF(xMax * 0.063, yMax * minHYAdj));
    update();
    emit updateCurrentPosition(
        m_currentControlPointIndex,
        m_points.at(m_currentControlPointIndex) - m_xyOffset);
    emit controlPointChanged(false);
    return;
  }

  // Don't delete the last cubic
  if (index >= m_points.size() - 5) {
    int i = m_points.size() - 5;
    setPoint(i, QPointF(xMax * 0.937, yMax * maxHYAdj));
    setPoint(i + 1, QPointF(xMax, maxY));
    setPoint(i + 2, QPointF(xMax + 20, maxY));
    setPoint(i + 3, QPointF(xMax + 20, maxY));
    setPoint(i + 4, QPointF(xMax + 40, maxY));
    update();
    emit updateCurrentPosition(
        m_currentControlPointIndex,
        m_points.at(m_currentControlPointIndex) - m_xyOffset);
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
  m_currentControlPointIndex = -1;

  emit updateCurrentPosition(m_currentControlPointIndex,
                             QPointF(0, 0) - m_xyOffset);

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

  QRectF rect(0, 0, width(), height());
  QRectF r = path.boundingRect();
  if (!rect.contains(QRect(rect.left(), r.top(), rect.width(), r.height()))) {
    QList<QPointF> points = getIntersectedPoint(rect, path);

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

QPoint GraphWidget::clipToBorder(const QPoint p) {
  QPoint tempPos = p;
  int x          = tempPos.x();
  int y          = tempPos.y();

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

  return tempPos;
}

//-----------------------------------------------------------------------------

void GraphWidget::paintEvent(QPaintEvent* e) {
  QPainter painter(this);

  double scale = (m_isEnlarged) ? 2.0 : 1.0;
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(m_graphColor);

  // Draw Grid
  QRectF r = rect().adjusted(m_LeftRightMargin, m_TopMargin, -m_LeftRightMargin,
                             -m_BottomMargin);
  double step = r.width() / m_gridSpacing;
  for (int i = 1; i < m_gridSpacing; i++) {
    painter.drawLine(QPoint(step * i, 0), QPoint(step * i, height()));
  }

  step = r.height() / m_gridSpacing;
  for (int i = 1; i < m_gridSpacing; i++) {
    painter.drawLine(QPoint(0, step * i), QPoint(width(), step * i));
  }

  painter.setClipRect(r, Qt::IntersectClip);
  painter.translate(m_LeftRightMargin + 1, height() - m_BottomMargin);
  painter.scale(scale, -scale);

  // Draw Spline
  QPainterPath path = getPainterPath();
  if (path.isEmpty()) return;
  painter.setRenderHint(QPainter::Antialiasing, true);
  QPen blackPen(Qt::black);
  QPen bluePen(Qt::blue);
  blackPen.setWidthF(1.0 / scale);
  bluePen.setWidthF(1.0 / scale);

  painter.setPen(blackPen);
  painter.setPen(m_splineColor);
  painter.setBrush(Qt::NoBrush);
  painter.drawPath(path);

  // Draw Control Points
  int n     = m_points.size();
  QPointF p = convertPointToLocal(m_points.at(3));

  double minDistance;
  int highlightPointIndex = getClosestPointIndex(m_cursorPos, minDistance);
  if (m_cursorPos == QPointF(-1, -1) || minDistance > 20.0 ||
      (m_lockExtremePoints &&
       (highlightPointIndex == 3 || highlightPointIndex == (n - 4))))
    highlightPointIndex = -1;

  for (int i = 3; i < n - 3; i++) {
    QBrush brush(Qt::white);
    int rad       = 4;  //(m_isEnlarged) ? 1 : 2;
    QPointF nextP = convertPointToLocal(m_points.at(i + 1));
    if (isCentralControlPoint(i))
      rad = 5;  //(m_isEnlarged) ? 2 : 3;
    else if (m_isLinear) {
      p = nextP;
      continue;
    }

    painter.setPen(m_splineColor);

    QPointF handlePos = p;
    if (!m_isLinear) {
      if (isLeftControlPoint(i)) {
        painter.drawLine(p, nextP);
      } else if (isCentralControlPoint(i) && i < n - 4)
        painter.drawLine(p, nextP);

      handlePos = convertPointToLocal(getVisibleHandlePos(i));
    }
    painter.setBrush(highlightPointIndex == i
                         ? Qt::yellow
                         : ((m_currentControlPointIndex != i)
                                ? m_nonSelectedPointColor
                            : (p == handlePos) ? m_selectedPointColor
                                               : Qt::blue));
    painter.setPen(m_splineColor);

    QRectF pointRect(handlePos.x() - rad, handlePos.y() - rad, 2 * rad,
                     2 * rad);
    painter.drawEllipse(pointRect);
    p = nextP;
  }
}

//-----------------------------------------------------------------------------

void GraphWidget::mouseMoveEvent(QMouseEvent* e) {
  QPoint tempPos = e->pos();

  if (m_constrainToBounds) tempPos = clipToBorder(tempPos);

  QPointF posF = viewToStrokePoint(QPointF(tempPos));
  m_cursorPos  = posF;
  if (m_mouseButton == Qt::LeftButton && m_currentControlPointIndex != -1) {
    m_isDragging = true;
    moveCurrentControlPoint(posF - m_preMousePos);
    m_preMousePos = posF;
  } else if (m_currentControlPointIndex == -1)
    emit updateCurrentPosition(-1, posF - m_xyOffset);

  update();
}

//-----------------------------------------------------------------------------

void GraphWidget::mousePressEvent(QMouseEvent* e) {
  m_mouseButton = e->button();
  setFocus();
  m_isDragging = false;
  if (m_mouseButton == Qt::LeftButton) {
    QPoint tempPos = e->pos();

    if (m_constrainToBounds) tempPos = clipToBorder(tempPos);

    QPointF posF = viewToStrokePoint(QPointF(tempPos));
    double minDistance;
    int controlPointIndex = getClosestPointIndex(posF, minDistance);
    if (m_lockExtremePoints) {
      if (controlPointIndex <= 3 || controlPointIndex >= m_points.size() - 4) {
        m_currentControlPointIndex = -1;
        update();
        return;
      }
    }

    if (minDistance <= 20.0) {
      m_currentControlPointIndex = controlPointIndex;
      m_preMousePos              = posF;
    } else {
      m_currentControlPointIndex = -1;
      double percent =
          getPercentAtPoint(getInvertedPoint(e->pos()), getPainterPath());
      if (percent != 0 && minDistance > 20) addControlPoint(percent);
    }
    QPointF currentPointPos = (m_currentControlPointIndex == -1)
                                  ? posF
                                  : m_points.at(m_currentControlPointIndex);

    emit updateCurrentPosition(m_currentControlPointIndex,
                               currentPointPos - m_xyOffset);
    update();
  }
}

//-----------------------------------------------------------------------------

void GraphWidget::mouseReleaseEvent(QMouseEvent* e) {
  if (m_isDragging) {
    // Signal we're done changing
    m_isDragging = false;
    emit controlPointChanged(false);
  }
  m_mouseButton = Qt::NoButton;
}

//-----------------------------------------------------------------------------

void GraphWidget::keyPressEvent(QKeyEvent* e) {
  if (m_currentControlPointIndex == -1) return;

#ifdef MACOSX
  if (e->key() == Qt::Key_Backspace) {
#else
  if (e->key() == Qt::Key_Delete) {
#endif
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

//=============================================================================
// Graph GUI
//-----------------------------------------------------------------------------

GraphGUIWidget::GraphGUIWidget(QWidget* parent)
    : QWidget(parent), m_updating(false) {
  // Build default.  Will be customized later
  m_graph = new GraphWidget();
  m_graph->setMinXValue(0.0);
  m_graph->setMaxXValue(100.0);
  m_graph->setMinYValue(0.0);
  m_graph->setMaxYValue(100.0);
  m_graph->initializeSpline();

  m_minXLabel = new QLabel(tr("0"));
  m_midXLabel = new QLabel(tr("X"));
  m_maxXLabel = new QLabel(tr("100"));
  m_minYLabel = new QLabel(tr("0"));
  m_midYLabel = new QLabel(tr("Y"));
  m_maxYLabel = new QLabel(tr("100"));

  m_resetBtn = new QPushButton(tr("Reset Curve"));

  m_outValue = new DVGui::DoubleLineEdit(this);
  m_outValue->setMaximumWidth(40);
  m_outValue->setDecimals(2);
  m_outValue->setEnabled(false);
  m_outValue->setValue(0);

  QVBoxLayout* midYLayout = new QVBoxLayout();
  midYLayout->setContentsMargins(0, 0, 0, 0);
  midYLayout->setSpacing(2);
  midYLayout->addWidget(m_midYLabel);
  midYLayout->addWidget(m_outValue);

  QFrame* midYFrame = new QFrame(this);
  midYFrame->setLayout(midYLayout);

  m_inValue = new DVGui::DoubleLineEdit(this);
  m_inValue->setMaximumWidth(40);
  m_inValue->setDecimals(2);
  m_inValue->setEnabled(false);
  m_inValue->setValue(0);

  QHBoxLayout* midXLayout = new QHBoxLayout();
  midXLayout->setContentsMargins(0, 0, 0, 0);
  midXLayout->setSpacing(2);
  midXLayout->addWidget(m_midXLabel);
  midXLayout->addWidget(m_inValue);

  QFrame* midXFrame = new QFrame(this);
  midXFrame->setLayout(midXLayout);

  QHBoxLayout* graphLayout = new QHBoxLayout();
  graphLayout->setContentsMargins(0, 0, 0, 0);
  graphLayout->setSpacing(0);
  graphLayout->addWidget(m_graph);

  QFrame* graphFrame = new QFrame(this);
  graphFrame->setLayout(graphLayout);
  graphFrame->setObjectName("GraphAreaFrame");

  QGridLayout* graphGUILayout = new QGridLayout();
  graphGUILayout->setContentsMargins(3, 3, 3, 3);
  graphGUILayout->setHorizontalSpacing(3);
  graphGUILayout->setVerticalSpacing(3);
  {
    graphGUILayout->addWidget(m_maxYLabel, 0, 0, Qt::AlignTop | Qt::AlignRight);
    graphGUILayout->addWidget(midYFrame, 1, 0,
                              Qt::AlignVCenter | Qt::AlignRight);
    graphGUILayout->addWidget(m_minYLabel, 2, 0,
                              Qt::AlignBottom | Qt::AlignRight);
    graphGUILayout->addWidget(graphFrame, 0, 1, 3, 3);
    graphGUILayout->addWidget(m_minXLabel, 3, 1, Qt::AlignTop | Qt::AlignLeft);
    graphGUILayout->addWidget(midXFrame, 3, 2, Qt::AlignHCenter);
    graphGUILayout->addWidget(m_maxXLabel, 3, 3, Qt::AlignTop | Qt::AlignRight);
    graphGUILayout->addWidget(m_resetBtn, 4, 1, 1, 3, Qt::AlignCenter);
  }
  setLayout(graphGUILayout);

  bool ret = connect(m_graph, SIGNAL(controlPointAdded(int)), this,
                     SLOT(onCurvePointAdded(int)));

  ret = ret && connect(m_graph, SIGNAL(controlPointChanged(bool)), this,
                       SLOT(onCurveChanged(bool)));

  ret = ret && connect(m_graph, SIGNAL(controlPointRemoved(int)), this,
                       SLOT(onCurvePointRemoved(int)));

  ret = ret && connect(m_graph, SIGNAL(updateCurrentPosition(int, QPointF)),
                       this, SLOT(onUpdateCurrentPosition(int, QPointF)));

  ret =
      ret && connect(m_resetBtn, SIGNAL(clicked()), this, SLOT(onCurveReset()));

  ret = ret && connect(m_outValue, SIGNAL(editingFinished()), this,
                       SLOT(onOutValueChanged()));

  ret = ret && connect(m_inValue, SIGNAL(editingFinished()), this,
                       SLOT(onInValueChanged()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void GraphGUIWidget::onCurvePointAdded(int index) { emit curveChanged(false); }

//-----------------------------------------------------------------------------

void GraphGUIWidget::onCurveChanged(bool isDragging) {
  emit curveChanged(isDragging);
}

//-----------------------------------------------------------------------------

void GraphGUIWidget::onCurvePointRemoved(int index) {
  emit curveChanged(false);
}

//-----------------------------------------------------------------------------

void GraphGUIWidget::onUpdateCurrentPosition(int index, QPointF pt) {
  if (index >= 0) {
    m_updating = true;
    m_outValue->setValue(std::roundf(pt.y() * 100.0) / 100.0);
    m_inValue->setValue(std::roundf(pt.x() * 100.0) / 100.0);
    m_updating = false;
    m_outValue->setEnabled(true);
    m_inValue->setEnabled(true);
  } else {
    m_updating = true;
    m_outValue->setValue(0);
    m_inValue->setValue(0);
    m_updating = false;
    m_outValue->setEnabled(false);
    m_inValue->setEnabled(false);
  }
}

//-----------------------------------------------------------------------------

void GraphGUIWidget::onCurveReset() {
  if (m_defaultCurve.count())
    m_graph->setPoints(m_defaultCurve);
  else
    m_graph->initializeSpline();

  onUpdateCurrentPosition(-1, QPointF(0, 0));

  emit curveChanged(false);
}

void GraphGUIWidget::onOutValueChanged() {
  if (m_updating) return;

  int index = m_graph->getCurrentControlPointIndex();
  if (index < 0) return;

  TPointD oldPt = m_graph->getControlPoint(index);
  double delta  = m_outValue->getValue() - oldPt.y;

  m_graph->moveCurrentControlPoint(QPointF(0, delta));
}

void GraphGUIWidget::onInValueChanged() {
  if (m_updating) return;

  int index = m_graph->getCurrentControlPointIndex();
  if (index < 0) return;

  TPointD oldPt = m_graph->getControlPoint(index);
  double delta  = m_inValue->getValue() - oldPt.x;

  m_graph->moveCurrentControlPoint(QPointF(delta, 0));
}