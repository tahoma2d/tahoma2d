#pragma once

#ifndef CMRASTERBRUSH_H
#define CMRASTERBRUSH_H

#include "trasterimage.h"
#include "toonz/rasterstrokegenerator.h"

#include <QRadialGradient>

class RasterStrokeGenerator;
class BluredBrush;

//************************************************************************
//   CM Raster Brush
//************************************************************************
class CMRasterBrush {
private:
  std::vector<RasterStrokeGenerator *> m_rasterTracks;

  int m_styleId;  // L'ink-style dello stroke
  bool m_selective;
  bool m_modifierLockAlpha;
  bool m_isPaletteOrder;
  QSet<int> m_aboveStyleIds;

  int m_brushCount;
  double m_rotation;
  TPointD m_centerPoint;
  bool m_useLineSymmetry;
  TPointD m_rasCenter, m_dpiScale;

public:
  CMRasterBrush(const TRasterCM32P &raster, Tasks task, ColorType colorType,
                int styleId, const TThickPoint &p, bool selective,
                int selectedStyle, bool lockAlpha, bool keepAntialias,
                bool isPaletteOrder = false);
  ~CMRasterBrush(){};

  void addSymmetryBrushes(double lines, double rotation, TPointD centerPoint,
                          bool useLineSymmetry, TPointD dpiScale);
  bool hasSymmetryBrushes() { return m_brushCount > 1; }
  int getStyleId() const { return m_styleId; }
  bool isSelective() { return m_selective; }
  bool isAlphaLocked() { return m_modifierLockAlpha; }

  bool isPaletteOrder() { return m_isPaletteOrder; }

  void add(const TThickPoint &p);

  void setAboveStyleIds(QSet<int> &ids);

  void generateStroke(bool isPencil, bool isStraight = false) const;

  std::vector<TThickPoint> getPointsSequence(bool mainOnly = false);
  void setPointsSequence(const std::vector<TThickPoint> &points);

  TRect getBBox(const std::vector<TThickPoint> &points) const;

  TRect getLastRect(bool isStraight = false) const;

  TRect generateLastPieceOfStroke(bool isPencil, bool closeStroke = false,
                                  bool isStraight = false);
};

#endif  // CMRASTERBRUSH_H
