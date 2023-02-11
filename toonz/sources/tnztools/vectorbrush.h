#pragma once

#ifndef VECTORBRUSH_H
#define VECTORBRUSH_H

#include "tgeometry.h"
#include "toonz/strokegenerator.h"

#include "symmetrytool.h"

//************************************************************************
//   Vector Brush
//************************************************************************
class VectorBrush {
private:
  StrokeGenerator m_tracks[MAX_SYMMETRY];

  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;
  TPointD m_rasCenter, m_dpiScale;

public:
  VectorBrush() : m_brushCount(1), m_rotation(0), m_useLineSymmetry(false) {}
  ~VectorBrush() {}

  void addSymmetryBrushes(double lines, double rotation, TPointD centerPoint,
                          bool useLineSymmetry, TPointD dpiScale);
  bool hasSymmetryBrushes() { return m_brushCount > 1; }

  StrokeGenerator getMainBrush();
  int getBrushCount() { return m_brushCount; }

  void reset();
  void clear();

  void add(const TThickPoint &point, double pixelSize2);

  TRectD getLastModifiedRegion();
  void removeMiddlePoints();
  TRectD getModifiedRegion();
  bool isEmpty();

  TStroke *makeStroke(double error, int brushIndex = 0,
                      UINT onlyLastPoints = 0);
  std::vector<TStroke *> makeSymmetryStrokes(double error,
                                             UINT onlyLastPoints = 0);

  void filterPoints();

  TPointD getFirstPoint(int brushIndex = 0);

  void drawAllFragments();
};

#endif  // VECTORBRUSH_H
