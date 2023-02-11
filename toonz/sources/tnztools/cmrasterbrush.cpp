#include "cmrasterbrush.h"

#include "symmetrytool.h"
#include "bluredbrush.h"

//*******************************************************************************
//    Raster Brush implementation
//*******************************************************************************

CMRasterBrush::CMRasterBrush(const TRasterCM32P &raster, Tasks task,
                             ColorType colorType, int styleId,
                             const TThickPoint &p, bool selective,
                             int selectedStyle, bool lockAlpha,
                             bool keepAntialias, bool isPaletteOrder)
    : m_styleId(styleId)
    , m_selective(selective)
    , m_modifierLockAlpha(lockAlpha)
    , m_isPaletteOrder(isPaletteOrder)
    , m_brushCount(0)
    , m_rotation(0)
    , m_useLineSymmetry(false) {
  TThickPoint pp               = p;
  if (task == ERASE) m_styleId = 4095;

  RasterStrokeGenerator *rasterTrack = new RasterStrokeGenerator(
      raster, task, colorType, styleId, p, selective, selectedStyle, lockAlpha,
      keepAntialias, isPaletteOrder);
  m_rasterTracks.push_back(rasterTrack);

  m_rasCenter = raster->getCenterD();
}

//------------------------------------------------------------------

void CMRasterBrush::addSymmetryBrushes(double lines, double rotation,
                                       TPointD centerPoint,
                                       bool useLineSymmetry, TPointD dpiScale) {
  if (lines < 2) return;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  m_brushCount      = lines;
  m_rotation        = rotation;
  m_centerPoint     = centerPoint;
  m_useLineSymmetry = useLineSymmetry;
  m_dpiScale        = dpiScale;

  SymmetryObject symmObj      = symmetryTool->getSymmetryObject();
  std::vector<TPointD> points = symmetryTool->getSymmetryPoints(
      m_rasterTracks[0]->getPointsSequence()[0], m_rasCenter, m_dpiScale,
      m_brushCount, m_rotation, m_centerPoint, m_useLineSymmetry);

  for (int i = 0; i < points.size(); i++) {
    std::vector<TThickPoint> symmPoints;
    symmPoints.push_back(TThickPoint(
        points[i], m_rasterTracks[0]->getPointsSequence()[0].thick));
    if (i == 0) {
      m_rasterTracks[0]->setPointsSequence(symmPoints);
      continue;
    }
    RasterStrokeGenerator *symmRasterTrack =
        new RasterStrokeGenerator(m_rasterTracks[0]);
    symmRasterTrack->setPointsSequence(symmPoints);
    m_rasterTracks.push_back(symmRasterTrack);
  }
}

//------------------------------------------------------------------

void CMRasterBrush::add(const TThickPoint &p) {
  if (!hasSymmetryBrushes()) {
    m_rasterTracks[0]->add(p);
    return;
  }

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj      = symmetryTool->getSymmetryObject();
  std::vector<TPointD> points = symmetryTool->getSymmetryPoints(
      p, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);

  for (int i = 0; i < points.size(); i++)
    m_rasterTracks[i]->add(TThickPoint(points[i], p.thick));
}

//------------------------------------------------------------------

void CMRasterBrush::generateStroke(bool isPencil, bool isStraight) const {
  for (int i = 0; i < m_rasterTracks.size(); i++)
    m_rasterTracks[i]->generateStroke(isPencil, isStraight);
}

//------------------------------------------------------------------

std::vector<TThickPoint> CMRasterBrush::getPointsSequence(bool mainOnly) {
  std::vector<TThickPoint> points;

  points = m_rasterTracks[0]->getPointsSequence();

  if (mainOnly) return points;

  for (int i = 1; i < m_rasterTracks.size(); i++) {
    std::vector<TThickPoint> symmPoints =
        m_rasterTracks[i]->getPointsSequence();

    points.insert(points.end(), symmPoints.begin(), symmPoints.end());
  }

  return points;
}

//------------------------------------------------------------------

void CMRasterBrush::setPointsSequence(const std::vector<TThickPoint> &points) {
  m_rasterTracks[0]->setPointsSequence(points);

  if (!hasSymmetryBrushes()) return;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  for (int i = 0; i < points.size(); i++) {
    std::vector<TPointD> symmPoints = symmetryTool->getSymmetryPoints(
        points[i], m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
        m_centerPoint, m_useLineSymmetry);
    for (int j = 0; j < symmPoints.size(); j++) {
      if (i == 0) {
        std::vector<TThickPoint> vpts;
        vpts.push_back(TThickPoint(symmPoints[j], points[i].thick));
        m_rasterTracks[j]->setPointsSequence(vpts);
      } else
        m_rasterTracks[j]->add(TThickPoint(symmPoints[j], points[i].thick));
    }
  }
}

//------------------------------------------------------------------

TRect CMRasterBrush::getBBox(const std::vector<TThickPoint> &getBBox) const {
  return m_rasterTracks[0]->getBBox(getBBox);
}

//------------------------------------------------------------------

TRect CMRasterBrush::getLastRect(bool isStraight) const {
  std::vector<TThickPoint> points;
  for (int i = 0; i < m_rasterTracks.size(); i++) {
    std::vector<TThickPoint> pts = m_rasterTracks[i]->getPointsSequence();
    int size                     = pts.size();
    if (isStraight) {
      points.push_back(pts[0]);
      points.push_back(pts[2]);
    } else if (size == 3) {
      points.push_back(pts[0]);
      points.push_back(pts[1]);
    } else if (size == 1)
      points.push_back(pts[0]);
    else {
      points.push_back(pts[size - 4]);
      points.push_back(pts[size - 3]);
      points.push_back(pts[size - 2]);
      points.push_back(pts[size - 1]);
    }
  }

  return getBBox(points);
}

//------------------------------------------------------------------

void CMRasterBrush::setAboveStyleIds(QSet<int> &ids) {
  for (int i = 0; i < m_rasterTracks.size(); i++)
    m_rasterTracks[i]->setAboveStyleIds(ids);
}

//------------------------------------------------------------------

TRect CMRasterBrush::generateLastPieceOfStroke(bool isPencil, bool closeStroke,
                                               bool isStraight) {
  TRect finalRect;

  for (int i = 0; i < m_rasterTracks.size(); i++)
    finalRect += m_rasterTracks[i]->generateLastPieceOfStroke(
        isPencil, closeStroke, isStraight);

  return finalRect;
}
