#include "vectorbrush.h"

#include "tstroke.h"
#include "tgeometry.h"

//------------------------------------------------------------------

void VectorBrush::addSymmetryBrushes(double lines, double rotation,
                                     TPointD centerPoint, bool useLineSymmetry,
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

//------------------------------------------------------------------

StrokeGenerator VectorBrush::getMainBrush() { return m_tracks[0]; }

//------------------------------------------------------------------

void VectorBrush::reset() {
  clear();

  m_brushCount      = 1;
  m_rotation        = 0.0;
  m_centerPoint     = TPointD();
  m_useLineSymmetry = false;
  m_rasCenter       = TPointD();
  m_dpiScale        = TPointD();
}

//------------------------------------------------------------------

void VectorBrush::clear() {
  for (int i = 0; i < m_brushCount; i++) m_tracks[i].clear();
}

//------------------------------------------------------------------

void VectorBrush::add(const TThickPoint &point, double pixelSize2) {
  if (!hasSymmetryBrushes()) {
    m_tracks[0].add(point, pixelSize2);
    return;
  }

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj      = symmetryTool->getSymmetryObject();
  std::vector<TPointD> points = symmetryTool->getSymmetryPoints(
      point, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);

  for (int i = 0; i < points.size(); i++)
    m_tracks[i].add(TThickPoint(points[i], point.thick), pixelSize2);
}

//------------------------------------------------------------------

TRectD VectorBrush::getLastModifiedRegion() {
  TRectD rect;

  for (int i = 0; i < m_brushCount; i++)
    rect += m_tracks[i].getLastModifiedRegion();

  return rect;
}

//------------------------------------------------------------------

void VectorBrush::removeMiddlePoints() {
  for (int i = 0; i < m_brushCount; i++) m_tracks[i].removeMiddlePoints();
}

//------------------------------------------------------------------

TRectD VectorBrush::getModifiedRegion() {
  TRectD rect;

  for (int i = 0; i < m_brushCount; i++)
    rect += m_tracks[i].getModifiedRegion();

  return rect;
}

//------------------------------------------------------------------

bool VectorBrush::isEmpty() { return m_tracks[0].isEmpty(); }

//------------------------------------------------------------------

TStroke *VectorBrush::makeStroke(double error, int brushIndex,
                                 UINT onlyLastPoints) {
  return m_tracks[brushIndex].makeStroke(error, onlyLastPoints);
}

//------------------------------------------------------------------

std::vector<TStroke *> VectorBrush::makeSymmetryStrokes(double error,
                                                        UINT onlyLastPoints) {
  std::vector<TStroke *> strokes;

  for (int i = 1; i < m_brushCount; i++)
    strokes.push_back(makeStroke(error, i, onlyLastPoints));

  return strokes;
}

//------------------------------------------------------------------

void VectorBrush::filterPoints() {
  for (int i = 0; i < m_brushCount; i++) m_tracks[i].filterPoints();
}

//------------------------------------------------------------------

TPointD VectorBrush::getFirstPoint(int brushIndex) {
  return m_tracks[brushIndex].getFirstPoint();
}

//------------------------------------------------------------------

void VectorBrush::drawAllFragments() {
  for (int i = 0; i < m_brushCount; i++) m_tracks[i].drawAllFragments();
}
