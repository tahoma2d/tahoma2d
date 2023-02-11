#pragma once

#ifndef SYMMETRYSTROKE_H
#define SYMMETRYSTROKE_H

#include "symmetrytool.h"

class SymmetryStroke {
private:
  std::vector<TPointD> m_brush[MAX_SYMMETRY];

  int m_joinDistance;
  bool m_allowSpeed;
  double m_radius;
  int m_edgeCount;

  bool m_drawStartCircle;
  bool m_drawToMousePos;

  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;
  TPointD m_rasCenter, m_dpiScale;

public:
  SymmetryStroke()
      : m_joinDistance(2)
      , m_allowSpeed(false)
      , m_radius(0)
      , m_edgeCount(3)
      , m_brushCount(1)
      , m_rotation(0)
      , m_useLineSymmetry(false)
      , m_drawStartCircle(true)
      , m_drawToMousePos(true){};
  ~SymmetryStroke() {}

  void addSymmetryBrushes(double lines, double rotation, TPointD centerPoint,
                          bool useLineSymmetry, TPointD dpiScale);
  bool hasSymmetryBrushes() { return m_brushCount > 1; }
  int getBrushCount() { return m_brushCount; }

  std::vector<TPointD> *getBrush(int brushIndex = 0) {
    return &m_brush[brushIndex];
  }
  TPointD getPoint(int pointIndex, int brushIndex = 0) const {
    return m_brush[brushIndex][pointIndex];
  }
  TRectD getBBox(int brushIndex = 0);
  std::vector<TPointD> getSymmetryRectangle(int brushIndex = 0);

  void reset();
  void clear();
  void erase(std::vector<TPointD>::const_iterator it);

  bool empty();
  int size() const;

  TPointD back();
  TPointD front();

  // Polyline
  void setJoinDistance(int joinDistance) { m_joinDistance = joinDistance; }
  void setAllowSpeed(bool allowSpeed) { m_allowSpeed = allowSpeed; }
  void setDrawStartCircle(bool drawStartCircle) {
    m_drawStartCircle = drawStartCircle;
  }
  void setDrawToMousePos(bool drawToMousePos) {
    m_drawToMousePos = drawToMousePos;
  }

  void push_back(TPointD point);
  void setPoint(int index, TPointD point);

  void drawPolyline(TPointD mousePos, TPixel color, double pixelSize = 1.0,
                    bool speedMoved = false, bool ctrlPressed = false);

  TStroke *makePolylineStroke(int brushIndex = 0, double thick = 1);

  // Rectangle
  void setRectangle(TPointD lowerLeft, TPointD upperRight);

  void drawRectangle(TPixel color, double pixelSize = 1.0);

  TStroke *makeRectangleStroke(int brushIndex = 0, double thick = 1);

  // Circle
  void setCircle(TPointD center, double radius);

  void drawCircle(TPixel color, double pixelSize = 1.0);

  // Ellipse
  void setEllipse(TPointD lowerLeft, TPointD upperRight);

  void drawEllipse(TPixel color, double pixelSize = 1.0);

  // Polygon
  void setPolygon(TPointD center, double radius, int edgeCount);

  void drawPolygon(TPixel color, double pixelSize = 1.0);

  // Arc
  void drawArc(TPointD mousePos, TPixel color, double pixelSize = 1.0);
};

#endif  // SYMMETRYSTROKE_H