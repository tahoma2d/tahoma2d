
#include <algorithm>

#include "mypainttoonzbrush.h"
#include "tropcm.h"
#include "tpixelutils.h"
#include <toonz/mypainthelpers.hpp>

#include "symmetrytool.h"

#include <QColor>

namespace {
void putOnRasterCM(const TRasterCM32P &out, const TRaster32P &in, int styleId,
                   bool lockAlpha) {
  if (!out.getPointer() || !in.getPointer()) return;
  assert(out->getSize() == in->getSize());
  int x, y;
  for (y = 0; y < out->getLy(); y++) {
    for (x = 0; x < out->getLx(); x++) {
#ifdef _DEBUG
      assert(x >= 0 && x < in->getLx());
      assert(y >= 0 && y < in->getLy());
      assert(x >= 0 && x < out->getLx());
      assert(y >= 0 && y < out->getLy());
#endif
      TPixel32 *inPix = &in->pixels(y)[x];
      if (inPix->m == 0) continue;
      TPixelCM32 *outPix = &out->pixels(y)[x];
      if (lockAlpha && !outPix->isPureInk() && outPix->getPaint() == 0 &&
          outPix->getTone() == 255) {
        *outPix =
            TPixelCM32(outPix->getInk(), outPix->getPaint(), outPix->getTone());
        continue;
      }
      bool sameStyleId = styleId == outPix->getInk();
      // line with lock alpha : use original pixel's tone
      // line with the same style : multiply tones
      // line with different style : pick darker tone
      int tone = lockAlpha ? outPix->getTone()
                           : sameStyleId
                                 ? outPix->getTone() * (255 - inPix->m) / 255
                                 : std::min(255 - inPix->m, outPix->getTone());
      int ink = !sameStyleId && outPix->getTone() < 255 - inPix->m
                    ? outPix->getInk()
                    : styleId;
      *outPix = TPixelCM32(ink, outPix->getPaint(), tone);
    }
  }
}
}  // namespace

//=======================================================
//
// Raster32PMyPaintSurface::Internal
//
//=======================================================

class Raster32PMyPaintSurface::Internal
    : public mypaint::helpers::SurfaceCustom<readPixel, writePixel, askRead,
                                             askWrite> {
public:
  typedef SurfaceCustom Parent;
  Internal(Raster32PMyPaintSurface &owner)
      : SurfaceCustom(owner.m_ras->pixels(), owner.m_ras->getLx(),
                      owner.m_ras->getLy(), owner.m_ras->getPixelSize(),
                      owner.m_ras->getRowSize(), &owner) {}
};

//=======================================================
//
// Raster32PMyPaintSurface
//
//=======================================================

Raster32PMyPaintSurface::Raster32PMyPaintSurface(const TRaster32P &ras)
    : m_ras(ras), controller(), internal() {
  assert(ras);
  internal = new Internal(*this);
}

Raster32PMyPaintSurface::Raster32PMyPaintSurface(const TRaster32P &ras,
                                                 RasterController &controller)
    : m_ras(ras), controller(&controller), internal() {
  assert(ras);
  internal = new Internal(*this);
}

Raster32PMyPaintSurface::~Raster32PMyPaintSurface() { delete internal; }

bool Raster32PMyPaintSurface::getColor(float x, float y, float radius,
                                       float &colorR, float &colorG,
                                       float &colorB, float &colorA) {
  return internal->getColor(x, y, radius, colorR, colorG, colorB, colorA);
}

bool Raster32PMyPaintSurface::drawDab(const mypaint::Dab &dab) {
  return internal->drawDab(dab);
}

bool Raster32PMyPaintSurface::getAntialiasing() const {
  return internal->antialiasing;
}

void Raster32PMyPaintSurface::setAntialiasing(bool value) {
  internal->antialiasing = value;
}

//=======================================================
//
// MyPaintToonzBrush
//
//=======================================================

MyPaintToonzBrush::MyPaintToonzBrush(const TRaster32P &ras,
                                     RasterController &controller,
                                     const mypaint::Brush &brush)
    : m_ras(ras)
    , m_mypaintSurface(m_ras, controller)
    , m_brushCount(1)
    , m_rotation(0)
    , reset(true)
    , m_rasCenter(ras->getCenterD()) {
  brushes[0] = brush;

  // read brush antialiasing settings
  float aa = brushes[0].getBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING);
  m_mypaintSurface.setAntialiasing(aa > 0.5f);

  // reset brush antialiasing to zero to avoid radius and hardness correction
  brushes[0].setBaseValue(MYPAINT_BRUSH_SETTING_ANTI_ALIASING, 0.f);
  for (int i = 0; i < MYPAINT_BRUSH_INPUTS_COUNT; ++i)
    brushes[0].setMappingN(MYPAINT_BRUSH_SETTING_ANTI_ALIASING,
                           (MyPaintBrushInput)i, 0);
}

void MyPaintToonzBrush::addSymmetryBrushes(double lines, double rotation,
                                           TPointD centerPoint,
                                           bool useLineSymmetry,
                                           TPointD dpiScale) {
  if (lines < 2) return;

  m_brushCount      = lines;
  m_rotation        = rotation;
  m_centerPoint     = centerPoint;
  m_useLineSymmetry = useLineSymmetry;
  m_dpiScale        = dpiScale;

  // Copy main brush
  for (int i = 1; i < m_brushCount; i++) brushes[i] = brushes[0];
}

void MyPaintToonzBrush::beginStroke() {
  for (int i = 0; i < m_brushCount; i++) {
    brushes[i].reset();
    brushes[i].newStroke();
  }
  reset = true;
}

void MyPaintToonzBrush::endStroke() {
  if (!reset) {
    strokeTo(TPointD(current.x, current.y), current.pressure, 0.f);
    beginStroke();
  }
}

void MyPaintToonzBrush::strokeTo(const TPointD &point, double pressure,
                                 double dtime) {
  Params next(point.x, point.y, pressure, 0.0);

  std::vector<TPointD> prevPoints, currPoints, nextPoints;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));

  if (reset) {
    current  = next;
    previous = current;
    reset    = false;

    if (symmetryTool && hasSymmetryBrushes()) {
      SymmetryObject symmObj = symmetryTool->getSymmetryObject();
      currPoints             = symmetryTool->getSymmetryPoints(
          TPointD(current.x, current.y), m_rasCenter, m_dpiScale, m_brushCount,
          m_rotation, m_centerPoint, m_useLineSymmetry);
    } else
      currPoints.push_back(TPointD(current.x, current.y));

    for (int i = 0; i < currPoints.size(); i++) {
      // we need to jump to initial point (heuristic)
      brushes[i].setState(MYPAINT_BRUSH_STATE_X, currPoints[i].x);
      brushes[i].setState(MYPAINT_BRUSH_STATE_Y, currPoints[i].y);
      brushes[i].setState(MYPAINT_BRUSH_STATE_ACTUAL_X, currPoints[i].x);
      brushes[i].setState(MYPAINT_BRUSH_STATE_ACTUAL_Y, currPoints[i].y);
    }
    return;
  } else {
    next.time = current.time + dtime;
  }

  // accuracy
  const double threshold    = 1.0;
  const double thresholdSqr = threshold * threshold;
  const int maxLevel        = 16;

  if (symmetryTool && hasSymmetryBrushes()) {
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();
    prevPoints             = symmetryTool->getSymmetryPoints(
        TPointD(previous.x, previous.y), m_rasCenter, m_dpiScale, m_brushCount,
        m_rotation, m_centerPoint, m_useLineSymmetry);
    currPoints = symmetryTool->getSymmetryPoints(
        TPointD(current.x, current.y), m_rasCenter, m_dpiScale, m_brushCount,
        m_rotation, m_centerPoint, m_useLineSymmetry);
    nextPoints = symmetryTool->getSymmetryPoints(
        TPointD(next.x, next.y), m_rasCenter, m_dpiScale, m_brushCount,
        m_rotation, m_centerPoint, m_useLineSymmetry);
  } else {
    prevPoints.push_back(TPointD(previous.x, previous.y));
    currPoints.push_back(TPointD(current.x, current.y));
    nextPoints.push_back(TPointD(next.x, next.y));
  }

  for (int i = 0; i < nextPoints.size(); i++) {
    Params prevPt(prevPoints[i].x, prevPoints[i].y, previous.pressure,
                  previous.time);
    Params currPt(currPoints[i].x, currPoints[i].y, current.pressure,
                  current.time);
    Params nextPt(nextPoints[i].x, nextPoints[i].y, next.pressure, next.time);

    // set initial segment
    Segment stack[maxLevel + 1];
    Params p0;
    Segment *segment    = stack;
    Segment *maxSegment = segment + maxLevel;
    p0.setMedian(prevPt, currPt);
    segment->p1 = currPt;
    segment->p2.setMedian(currPt, nextPt);

    // process
    while (true) {
      double dx = segment->p2.x - p0.x;
      double dy = segment->p2.y - p0.y;
      if (dx * dx + dy * dy > thresholdSqr && segment != maxSegment) {
        Segment *sub = segment + 1;
        sub->p1.setMedian(p0, segment->p1);
        segment->p1.setMedian(segment->p1, segment->p2);
        sub->p2.setMedian(sub->p1, segment->p1);
        segment = sub;
      } else {
        brushes[i].strokeTo(m_mypaintSurface, segment->p2.x, segment->p2.y,
                            segment->p2.pressure, 0.f, 0.f,
                            segment->p2.time - p0.time);
        if (segment == stack) break;
        p0 = segment->p2;
        --segment;
      }
    }
  }

  // keep parameters for future interpolation
  previous = current;
  current  = next;

  // shift time
  previous.time = 0.0;
  current.time  = dtime;
}

//----------------------------------------------------------------------------------

void MyPaintToonzBrush::updateDrawing(const TRasterCM32P rasCM,
                                      const TRasterCM32P rasBackupCM,
                                      const TRect &bbox, int styleId,
                                      bool lockAlpha) const {
  if (!rasCM) return;

  TRect rasRect    = rasCM->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;

  rasCM->copy(rasBackupCM->extract(targetRect), targetRect.getP00());
  putOnRasterCM(rasCM->extract(targetRect), m_ras->extract(targetRect), styleId,
                lockAlpha);
}