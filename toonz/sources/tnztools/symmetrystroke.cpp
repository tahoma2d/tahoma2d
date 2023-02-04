#include "symmetrystroke.h"

#include "tgl.h"
#include "tstroke.h"
#include "drawutil.h"
#include "tcurves.h"
#include "symmetrytool.h"

//=============================================================================
// SymmetryStroke
//-----------------------------------------------------------------------------

void SymmetryStroke::addSymmetryBrushes(double lines, double rotation,
                                        TPointD centerPoint,
                                        bool useLineSymmetry,
                                        TPointD dpiScale) {
  if (lines < 2) return;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  m_brushCount      = lines;
  m_rotation        = rotation;
  m_centerPoint     = centerPoint;
  m_useLineSymmetry = useLineSymmetry;
  m_dpiScale        = dpiScale;
}

//-----------------------------------------------------------------------------

TRectD SymmetryStroke::getBBox(int brushIndex) {
  TRectD bbox;

  bbox.x0 = bbox.x1 = m_brush[brushIndex][0].x;
  bbox.y0 = bbox.y1 = m_brush[brushIndex][0].y;
  for (int i = 1; i < m_brush[brushIndex].size(); i++) {
    bbox.x0 = std::min(bbox.x0, m_brush[brushIndex][i].x);
    bbox.x1 = std::max(bbox.x1, m_brush[brushIndex][i].x);
    bbox.y0 = std::min(bbox.y0, m_brush[brushIndex][i].y);
    bbox.y1 = std::max(bbox.y1, m_brush[brushIndex][i].y);
  }

  return bbox;
}

std::vector<TPointD> SymmetryStroke::getSymmetryRectangle(int brushIndex) {
  TRectD bbox = getBBox(0);
  std::vector<TPointD> pts;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return pts;

  std::vector<TPointD> bottomLeft = symmetryTool->getSymmetryPoints(
      bbox.getP00(), m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
      m_centerPoint, m_useLineSymmetry);
  std::vector<TPointD> upperLeft = symmetryTool->getSymmetryPoints(
      bbox.getP01(), m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
      m_centerPoint, m_useLineSymmetry);
  std::vector<TPointD> upperRight = symmetryTool->getSymmetryPoints(
      bbox.getP11(), m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
      m_centerPoint, m_useLineSymmetry);
  std::vector<TPointD> bottomRight = symmetryTool->getSymmetryPoints(
      bbox.getP10(), m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
      m_centerPoint, m_useLineSymmetry);

  pts.push_back(bottomLeft[brushIndex]);
  pts.push_back(upperLeft[brushIndex]);
  pts.push_back(upperRight[brushIndex]);
  pts.push_back(bottomRight[brushIndex]);
  pts.push_back(bottomLeft[brushIndex]);

  return pts;
}

//-----------------------------------------------------------------------------

void SymmetryStroke::reset() {
  clear();

  m_joinDistance = 2;
  m_allowSpeed   = false;
  m_radius       = 0;
  m_edgeCount    = 3;

  m_drawStartCircle = true;
  m_drawToMousePos  = true;

  m_brushCount      = 1;
  m_rotation        = 0.0;
  m_centerPoint     = TPointD();
  m_useLineSymmetry = false;
  m_rasCenter       = TPointD();
  m_dpiScale        = TPointD();
}

//-----------------------------------------------------------------------------
void SymmetryStroke::clear() {
  for (int i = 0; i < m_brushCount; i++) m_brush[i].clear();
}

//-----------------------------------------------------------------------------

void SymmetryStroke::erase(std::vector<TPointD>::const_iterator it) {
  int itAdj = -1;
  for (int i = 0; i < m_brush[0].size(); i++) {
    if ((m_brush[0].begin() + i) == it) {
      itAdj = i;
      break;
    }
  }
  if (itAdj < 0) return;

  for (int i = 0; i < m_brushCount; i++)
    m_brush[i].erase((m_brush[i].begin() + itAdj));
}

//-----------------------------------------------------------------------------

void SymmetryStroke::push_back(TPointD point) {
  if (!hasSymmetryBrushes()) {
    m_brush[0].push_back(point);
    return;
  }

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj      = symmetryTool->getSymmetryObject();
  std::vector<TPointD> points = symmetryTool->getSymmetryPoints(
      point, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);

  for (int i = 0; i < points.size(); i++) m_brush[i].push_back(points[i]);
}

//-----------------------------------------------------------------------------

void SymmetryStroke::setPoint(int index, TPointD point) {
  if (!hasSymmetryBrushes()) {
    m_brush[0][index] = point;
    return;
  }

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj      = symmetryTool->getSymmetryObject();
  std::vector<TPointD> points = symmetryTool->getSymmetryPoints(
      point, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);

  for (int i = 0; i < points.size(); i++) {
    m_brush[i][index] = points[i];
  }
}

//-----------------------------------------------------------------------------

bool SymmetryStroke::empty() { return m_brush[0].empty(); }

//-----------------------------------------------------------------------------

int SymmetryStroke::size() const { return m_brush[0].size(); }

//-----------------------------------------------------------------------------

TPointD SymmetryStroke::back() { return m_brush[0].back(); }

//-----------------------------------------------------------------------------

TPointD SymmetryStroke::front() { return m_brush[0].front(); }

//-----------------------------------------------------------------------------

void SymmetryStroke::drawPolyline(TPointD mousePos, TPixel color,
                                  double pixelSize, bool speedMoved,
                                  bool ctrlPressed) {
  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj        = symmetryTool->getSymmetryObject();
  std::vector<TPointD> mousePts = symmetryTool->getSymmetryPoints(
      mousePos, m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
      m_centerPoint, m_useLineSymmetry);

  for (int i = 0; i < m_brushCount; i++) {
    tglColor(color);

    if (!m_allowSpeed) {
      if (m_drawStartCircle) tglDrawCircle(m_brush[i][0], m_joinDistance);
      glBegin(GL_LINE_STRIP);
      for (UINT j = 0; j < m_brush[i].size(); j++) tglVertex(m_brush[i][j]);
      if (m_drawToMousePos) tglVertex(mousePts[i]);
      glEnd();

      continue;
    }

    std::vector<TPointD> points = m_brush[i];
    int count                   = points.size();
    if (count % 4 == 1) {
      // No speedOut
      points.push_back(points[count - 1]);
      count++;
    } else if (ctrlPressed)
      points[count - 1] = points[count - 2];

    if (m_drawToMousePos) {
      points.push_back(0.5 * (mousePts[i] + points[count - 1]));
      points.push_back(mousePts[i]);
      points.push_back(mousePts[i]);
    }

    TStroke *stroke = new TStroke(points);
    drawStrokeCenterline(*stroke, pixelSize);
    delete stroke;

    if (i == 0 && m_brush[i].size() > 1) {
      tglColor(TPixel(79, 128, 255));
      int index = (count < 5) ? count - 1 : count - 5;
      // Draw the previous speedOut (which is the current one) when count < 5
      TPointD p0 = m_brush[i][index];
      TPointD p1 = m_brush[i][index - 1];
      if (tdistance(p0, p1) > 0.1) {
        tglDrawSegment(p0, p1);
        tglDrawDisk(p0, 2 * pixelSize);
        tglDrawDisk(p1, 4 * pixelSize);
      }
      // Draw the current speedIn/Out when count > 5
      if (speedMoved && count > 5) {
        TPointD p0 = m_brush[i][count - 1];
        TPointD p1 = m_brush[i][count - 2];
        TPointD p2 = m_brush[i][count - 3];
        tglDrawSegment(p0, p1);
        tglDrawSegment(p1, p2);
        tglDrawDisk(p0, 2 * pixelSize);
        tglDrawDisk(p1, 4 * pixelSize);
        tglDrawDisk(p2, 2 * pixelSize);
      }
    }

    if (m_drawStartCircle) {
      double dist = m_joinDistance * m_joinDistance;
      if (m_brush[i].size() > 1 &&
          tdistance2(mousePts[i], m_brush[i][0]) < dist * pixelSize) {
        tglColor(TPixel32((color.r + 127) % 255, color.g, (color.b + 127) % 255,
                          color.m));
        tglDrawDisk(m_brush[i][0], m_joinDistance * pixelSize);
      } else {
        tglColor(color);
        tglDrawCircle(m_brush[i][0], m_joinDistance * pixelSize);
      }
    }
  }
}

//-----------------------------------------------------------------------------

TStroke *SymmetryStroke::makePolylineStroke(int brushIndex, double thick) {
  std::vector<TThickPoint> strokePoints;

  for (UINT i = 0; i < m_brush[brushIndex].size() - 1; i++) {
    strokePoints.push_back(TThickPoint(m_brush[brushIndex][i], thick));
    strokePoints.push_back(TThickPoint(
        0.5 * (m_brush[brushIndex][i] + m_brush[brushIndex][i + 1]), thick));
  }
  strokePoints.push_back(TThickPoint(m_brush[brushIndex].back(), thick));

  return new TStroke(strokePoints);
}

//-----------------------------------------------------------------------------

void SymmetryStroke::setRectangle(TPointD lowerLeft, TPointD upperRight) {
  push_back(lowerLeft);
  push_back(TPointD(lowerLeft.x, upperRight.y));
  push_back(upperRight);
  push_back(TPointD(upperRight.x, lowerLeft.y));
  push_back(lowerLeft);
}

//-----------------------------------------------------------------------------

void SymmetryStroke::drawRectangle(TPixel color, double pixelSize) {
  m_drawStartCircle = false;
  m_drawToMousePos  = false;

  drawPolyline(TPointD(), color, pixelSize);
}

//-----------------------------------------------------------------------------

TStroke *SymmetryStroke::makeRectangleStroke(int brushIndex, double thick) {
  return makePolylineStroke(brushIndex, thick);
}

//-----------------------------------------------------------------------------

void SymmetryStroke::setCircle(TPointD center, double radius) {
  m_drawStartCircle = false;
  m_drawToMousePos  = false;

  push_back(center);
  m_radius = radius;
}

//-----------------------------------------------------------------------------

void SymmetryStroke::drawCircle(TPixel color, double pixelSize) {
  tglColor(color);
  for (int i = 0; i < m_brushCount; i++) {
    tglDrawCircle(m_brush[i][0], m_radius);
  }
}

//-----------------------------------------------------------------------------

void SymmetryStroke::setEllipse(TPointD lowerLeft, TPointD upperRight) {
  setRectangle(lowerLeft, upperRight);
}

//-----------------------------------------------------------------------------

void SymmetryStroke::drawEllipse(TPixel color, double pixelSize) {
  m_drawStartCircle = false;
  m_drawToMousePos  = false;

  TRect bbox =
      TRect(m_brush[0][0].x, m_brush[0][0].y, m_brush[0][2].x, m_brush[0][2].y);

  for (int i = 0; i < m_brushCount; i++) {
    tglColor(color);

    TPointD centre = TPointD((m_brush[i][0].x + m_brush[i][2].x) * 0.5,
                             (m_brush[i][0].y + m_brush[i][2].y) * 0.5);

    double dx    = m_brush[i][1].x - m_brush[i][0].x;
    double dy    = m_brush[i][1].y - m_brush[i][0].y;
    double angle = std::atan2(dy, dx) / (3.14159 / 180) - 90;
    if (angle < 0) angle += 360;

    glPushMatrix();
    tglMultMatrix(TRotation(centre, angle) *
                  TScale(centre, bbox.x1 - bbox.x0, bbox.y1 - bbox.y0));
    tglDrawCircle(centre, 0.5);
    glPopMatrix();
  }
}

//-----------------------------------------------------------------------------

void SymmetryStroke::setPolygon(TPointD center, double radius, int edgeCount) {
  m_drawStartCircle = false;
  m_drawToMousePos  = false;

  TPointD lowerLeft  = TPointD(center.x - radius, center.y - radius);
  TPointD upperRight = TPointD(center.x + radius, center.y + radius);

  push_back(center);
  setRectangle(lowerLeft, upperRight);
  m_radius    = radius;
  m_edgeCount = edgeCount;
}

//-----------------------------------------------------------------------------

void SymmetryStroke::drawPolygon(TPixel color, double pixelSize) {
  tglColor(color);

  for (int i = 0; i < m_brushCount; i++) {
    double dx       = m_brush[i][2].x - m_brush[i][1].x;
    double dy       = m_brush[i][2].y - m_brush[i][1].y;
    double rotation = std::atan2(dy, dx) / (3.14159 / 180) - 90;
    if (rotation < 0) rotation += 360;

    double angleDiff = M_2PI / m_edgeCount;
    double angle     = (3 * M_PI + angleDiff) * 0.5;

    glPushMatrix();

    tglMultMatrix(TRotation(m_brush[i][0], rotation));

    glBegin(GL_LINE_LOOP);
    for (int j = 0; j < m_edgeCount; j++) {
      tglVertex(m_brush[i][0] +
                TPointD(cos(angle) * m_radius, sin(angle) * m_radius));
      angle += angleDiff;
    }
    glEnd();

    glPopMatrix();
  }
}

//-----------------------------------------------------------------------------

void SymmetryStroke::drawArc(TPointD mousePos, TPixel color, double pixelSize) {
  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj        = symmetryTool->getSymmetryObject();
  std::vector<TPointD> mousePts = symmetryTool->getSymmetryPoints(
      mousePos, m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
      m_centerPoint, m_useLineSymmetry);

  double thick = 1.0;

  tglColor(color);

  for (int i = 0; i < m_brushCount; i++) {
    std::vector<TThickPoint> points;
    int maxPts = m_brush[i].size();
    for (int j = 0; j < maxPts;) {
      points.clear();
      if ((j + 3) <= maxPts) {
        // We have a triplet - draw completed stroke
        TThickQuadratic q(m_brush[i][j], TThickPoint(m_brush[i][j + 2], thick),
                          m_brush[i][j + 1]);
        TThickQuadratic q0, q1, q00, q01, q10, q11;

        q.split(0.5, q0, q1);
        q0.split(0.5, q00, q01);
        q1.split(0.5, q10, q11);

        points.push_back(TThickPoint(m_brush[i][j], thick));
        points.push_back(TThickPoint(q00.getP1(), thick));
        points.push_back(TThickPoint(q00.getP2(), thick));
        points.push_back(TThickPoint(q01.getP1(), thick));
        points.push_back(TThickPoint(q01.getP2(), thick));
        points.push_back(TThickPoint(q10.getP1(), thick));
        points.push_back(TThickPoint(q10.getP2(), thick));
        points.push_back(TThickPoint(q11.getP1(), thick));
        points.push_back(TThickPoint(m_brush[i][j + 1], thick));
      } else if ((j + 2) <= maxPts) {
        // We have 2 points - draw ends with arc based on mouse
        glLineStipple(1, 0x5555);
        glEnable(GL_LINE_STIPPLE);
        glBegin(GL_LINE_STRIP);
        tglVertex(m_brush[i][j]);
        tglVertex(mousePts[i]);
        tglVertex(m_brush[i][j + 1]);
        glEnd();
        glDisable(GL_LINE_STIPPLE);

        TThickQuadratic q(m_brush[i][j], TThickPoint(mousePts[i], thick),
                          m_brush[i][j + 1]);
        TThickQuadratic q0, q1, q00, q01, q10, q11;

        q.split(0.5, q0, q1);
        q0.split(0.5, q00, q01);
        q1.split(0.5, q10, q11);

        points.push_back(TThickPoint(m_brush[i][j], thick));
        points.push_back(TThickPoint(q00.getP1(), thick));
        points.push_back(TThickPoint(q00.getP2(), thick));
        points.push_back(TThickPoint(q01.getP1(), thick));
        points.push_back(TThickPoint(q01.getP2(), thick));
        points.push_back(TThickPoint(q10.getP1(), thick));
        points.push_back(TThickPoint(q10.getP2(), thick));
        points.push_back(TThickPoint(q11.getP1(), thick));
        points.push_back(TThickPoint(m_brush[i][j + 1], thick));
      } else {
        // We have 1 point - draw straight line from initial point to mouse
        glBegin(GL_LINE_STRIP);
        tglVertex(m_brush[i][j]);
        tglVertex(mousePts[i]);
        glEnd();
        break;
      }

      TStroke *stroke = new TStroke(points);
      drawStrokeCenterline(*stroke, pixelSize);
      delete stroke;

      j += 3;
    }

    if (m_brush[i].size() > 3) {
      double dist = m_joinDistance * m_joinDistance;
      if (tdistance2(mousePts[i], m_brush[i][0]) < dist * pixelSize) {
        tglColor(TPixel32((color.r + 127) % 255, color.g, (color.b + 127) % 255,
                          color.m));
        tglDrawDisk(m_brush[i][0], m_joinDistance * pixelSize);
      } else {
        tglColor(color);
        tglDrawCircle(m_brush[i][0], m_joinDistance * pixelSize);
      }
    }
  }
}
