

#include "bluredbrush.h"
#include "tropcm.h"
#include "tpixelutils.h"
#include "symmetrytool.h"

#include <QColor>

namespace BluredBrushUtils {

static QVector<QRgb> colorTable;

QImage rasterToQImage(const TRasterP &ras, bool premultiplied = false) {
  QImage image;
  if (TRaster32P ras32 = ras)
    image = QImage(ras->getRawData(), ras->getLx(), ras->getLy(),
                   premultiplied ? QImage::Format_ARGB32_Premultiplied
                                 : QImage::Format_ARGB32);
  else if (TRasterGR8P ras8 = ras) {
    image = QImage(ras->getRawData(), ras->getLx(), ras->getLy(),
                   ras->getWrap(), QImage::Format_Indexed8);
    image.setColorTable(colorTable);
  }
  return image;
}

//----------------------------------------------------------------------------------
// drawOrderMode : 0=OverAll, 1=UnderAll, 2=PaletteOrder
void putOnRasterCM(const TRasterCM32P &out, const TRaster32P &in, int styleId,
                   int drawOrderMode, bool lockAlpha,
                   const QSet<int> &aboveStyleIds) {
  if (!out.getPointer() || !in.getPointer()) return;
  assert(out->getSize() == in->getSize());
  int x, y;
  if (drawOrderMode == 0) {  // OverAll
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
          *outPix = TPixelCM32(outPix->getInk(), outPix->getPaint(),
                               outPix->getTone());
          continue;
        }
        bool sameStyleId = styleId == outPix->getInk();
        // line with lock alpha : use original pixel's tone
        // line with the same style : multiply tones
        // line with different style : pick darker tone
        int tone = lockAlpha
                       ? outPix->getTone()
                       : sameStyleId
                             ? outPix->getTone() * (255 - inPix->m) / 255
                             : std::min(255 - inPix->m, outPix->getTone());
        int ink  = !sameStyleId && outPix->getTone() < 255 - inPix->m
                       ? outPix->getInk()
                       : styleId;
        *outPix  = TPixelCM32(ink, outPix->getPaint(), tone);
      }
    }
  } else if (drawOrderMode == 1) {  // UnderAll
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
          *outPix = TPixelCM32(outPix->getInk(), outPix->getPaint(),
                               outPix->getTone());
          continue;
        }
        bool sameStyleId = styleId == outPix->getInk();
        // line with the same style : multiply tones
        // line with different style : pick darker tone
        int tone = sameStyleId ? outPix->getTone() * (255 - inPix->m) / 255
                               : std::min(255 - inPix->m, outPix->getTone());
        int ink  = !sameStyleId && outPix->getTone() <= 255 - inPix->m
                       ? outPix->getInk()
                       : styleId;
        *outPix  = TPixelCM32(ink, outPix->getPaint(), tone);
      }
    }
  } else {  // PaletteOrder
    for (y = 0; y < out->getLy(); y++) {
      for (x = 0; x < out->getLx(); x++) {
        TPixel32 *inPix = &in->pixels(y)[x];
        if (inPix->m == 0) continue;
        TPixelCM32 *outPix = &out->pixels(y)[x];
        if (lockAlpha && !outPix->isPureInk() && outPix->getPaint() == 0 &&
            outPix->getTone() == 255) {
          *outPix = TPixelCM32(outPix->getInk(), outPix->getPaint(),
                               outPix->getTone());
          continue;
        }
        bool sameStyleId = styleId == outPix->getInk();
        // line with the same style : multiply tones
        // line with different style : pick darker tone
        int tone = sameStyleId ? outPix->getTone() * (255 - inPix->m) / 255
                               : std::min(255 - inPix->m, outPix->getTone());
        bool chooseOutPixInk = outPix->getTone() < 255 - inPix->m ||
                               (outPix->getTone() == 255 - inPix->m &&
                                aboveStyleIds.contains(outPix->getInk()));
        int ink = !sameStyleId && chooseOutPixInk ? outPix->getInk() : styleId;
        *outPix = TPixelCM32(ink, outPix->getPaint(), tone);
      }
    }
  }
}

//----------------------------------------------------------------------------------

void eraseFromRasterCM(const TRasterCM32P &out, const TRaster32P &in,
                       bool selective, int selectedStyleId,
                       const std::wstring &mode) {
  if (!out.getPointer() || !in.getPointer()) return;
  assert(out->getSize() == in->getSize());
  bool eraseLine  = mode == L"Lines" || mode == L"Lines & Areas";
  bool eraseAreas = mode == L"Areas" || mode == L"Lines & Areas";
  int x, y;

  for (y = 0; y < out->getLy(); y++) {
    for (x = 0; x < out->getLx(); x++) {
#ifdef _DEBUG
      assert(y >= 0 && y < in->getLy());
      assert(y >= 0 && y < out->getLy());
#endif
      TPixel32 *inPix = &in->pixels(y)[x];
      if (inPix->m == 0) continue;
      TPixelCM32 *outPix = &out->pixels(y)[x];
      bool eraseInk =
          !selective || (selective && selectedStyleId == outPix->getInk());
      bool erasePaint =
          !selective || (selective && selectedStyleId == outPix->getPaint());
      int paint = eraseAreas && erasePaint ? 0 : outPix->getPaint();
      int tone  = inPix->m > 0 && eraseLine && eraseInk
                      ? std::max(outPix->getTone(), (int)inPix->m)
                      : outPix->getTone();
      *outPix   = TPixelCM32(outPix->getInk(), paint, tone);
    }
  }
}

//----------------------------------------------------------------------------------

TRasterP rasterFromQImage(
    const QImage &image)  // no need of const& - Qt uses implicit sharing...
{
  QImage::Format format = image.format();
  if (format == QImage::Format_ARGB32 ||
      format == QImage::Format_ARGB32_Premultiplied)
    return TRaster32P(image.width(), image.height(), image.width(),
                      (TPixelRGBM32 *)image.bits(), false);
  if (format == QImage::Format_Indexed8)
    return TRasterGR8P(image.width(), image.height(), image.bytesPerLine(),
                       (TPixelGR8 *)image.bits(), false);
  return TRasterP();
}
}  // namespace

//=======================================================
//
// BluredBrush
//
//=======================================================

BluredBrush::BluredBrush(const TRaster32P &ras, int size,
                         const QRadialGradient &gradient, bool doDynamicOpacity)
    : m_ras(ras)
    , m_size(size)
    , m_lastPoint(0, 0)
    , m_oldOpacity(0)
    , m_enableDynamicOpacity(doDynamicOpacity) {
  m_rasImage = BluredBrushUtils::rasterToQImage(m_ras, false);
  m_gradient = gradient;

  if (BluredBrushUtils::colorTable.size() == 0) {
    int i;
    for (i = 0; i < 256; i++)
      BluredBrushUtils::colorTable.append(QColor(i, i, i).rgb());
  }
}

//----------------------------------------------------------------------------------

BluredBrush::BluredBrush(BluredBrush *src) {
  m_ras                  = src->m_ras;
  m_rasImage             = BluredBrushUtils::rasterToQImage(m_ras, false);
  m_size                 = src->m_size;
  m_gradient             = src->m_gradient;
  m_lastPoint            = src->m_lastPoint;
  m_oldOpacity           = src->m_oldOpacity;
  m_enableDynamicOpacity = src->m_enableDynamicOpacity;
  m_aboveStyleIds        = src->m_aboveStyleIds;
}

//----------------------------------------------------------------------------------

BluredBrush::~BluredBrush() {}

//----------------------------------------------------------------------------------

void BluredBrush::addPoint(const TThickPoint &p, double opacity) {
  double radius      = p.thick * 0.5;
  double brushRadius = m_size * 0.5;
  double scaleFactor = radius / brushRadius;

  QPainter painter(&m_rasImage);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(m_gradient);
  painter.setTransform(QTransform(scaleFactor, 0.0, 0.0, scaleFactor,
                                  p.x - radius, p.y - radius),
                       false);
  if (m_enableDynamicOpacity) painter.setOpacity(opacity);
  painter.drawEllipse(0, 0, m_size, m_size);
  painter.end();

  m_lastPoint  = p;
  m_oldOpacity = opacity;
}

//----------------------------------------------------------------------------------

void BluredBrush::addArc(const TThickPoint &pa, const TThickPoint &pb,
                         const TThickPoint &pc, double opacityA,
                         double opacityC) {
  QPainter painter(&m_rasImage);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  // painter.setBrush(m_gradient);

  TThickQuadratic q(pa, pb, pc);
  double brushRadius = m_size * 0.5;
  double t           = 0;
  while (t <= 1) {
    t = getNextPadPosition(q, t);
    if (t > 1) break;
    TThickPoint point  = q.getThickPoint(t);
    double radius      = point.thick * 0.5;
    double scaleFactor = radius / brushRadius;

    painter.setTransform(QTransform(scaleFactor, 0.0, 0.0, scaleFactor,
                                    point.x - radius, point.y - radius),
                         false);
    if (m_enableDynamicOpacity) {
      double opacity = opacityA + ((opacityC - opacityA) * t);
      if (fabs(opacity - m_oldOpacity) > 0.01)
        opacity =
            opacity > m_oldOpacity ? m_oldOpacity + 0.01 : m_oldOpacity - 0.01;
      painter.setOpacity(opacity);
      painter.setCompositionMode(QPainter::CompositionMode_DestinationAtop);
      m_oldOpacity = opacity;
      painter.setBrush(QColor(0, 0, 0, 255));
    } else
      painter.setBrush(m_gradient);
    painter.drawEllipse(0, 0, m_size, m_size);

    m_lastPoint = point;
  }
  painter.end();
}

//----------------------------------------------------------------------------------

double BluredBrush::getNextPadPosition(const TThickQuadratic &q,
                                       double t) const {
  TThickPoint p = m_lastPoint;
  double d      = 0.12 * p.thick;
  d             = d >= 1.0 ? d : 1.0;
  double d2     = d * d;
  if (norm2(q.getP2() - p) < d2) return 2.0;
  double t2        = (t + 1) * 0.5;
  TThickPoint p2   = q.getThickPoint(t2);
  double distance2 = norm2(p2 - p);
  double lastTMin  = t;
  double lastTMax  = 1;
  while (!areAlmostEqual(d2, distance2, 0.25) && t2 != lastTMin &&
         t2 != lastTMax) {
    if (distance2 > d2) {
      lastTMax = t2;
      t2       = (lastTMin + t2) * 0.5;
    } else {
      lastTMin = t2;
      t2       = (lastTMax + t2) * 0.5;
    }
    p2        = q.getThickPoint(t2);
    distance2 = norm2(p2 - p);
  }
  return t2;
}

//----------------------------------------------------------------------------------

void BluredBrush::updateDrawing(const TRasterP ras, const TRasterP rasBackup,
                                const TPixel32 &color, const TRect &bbox,
                                double opacity) const {
  TRect rasRect    = ras->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;
  QImage image = BluredBrushUtils::rasterToQImage(ras, true);
  QRect qTargetRect(targetRect.x0, targetRect.y0, targetRect.getLx(),
                    targetRect.getLy());

  QImage app(qTargetRect.size(), QImage::Format_ARGB32_Premultiplied);
  QPainter p2(&app);
  p2.setBrush(QColor(color.r, color.g, color.b));
  p2.drawRect(app.rect().adjusted(-1, -1, 0, 0));
  p2.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  p2.drawImage(QPoint(), m_rasImage, qTargetRect);
  p2.end();

  if (ras->getPixelSize() == 4) {
    QPainter p(&image);
    p.setClipRect(qTargetRect);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(qTargetRect, BluredBrushUtils::rasterToQImage(rasBackup, true),
                qTargetRect);
    p.end();

    p.begin(&image);
    p.setOpacity(m_enableDynamicOpacity ? 1 : opacity);
    p.drawImage(qTargetRect, app, app.rect());
    p.end();
  } else {
    QImage targetImage = BluredBrushUtils::rasterToQImage(rasBackup).copy(qTargetRect);
    targetImage        = targetImage.convertToFormat(
        QImage::Format_ARGB32_Premultiplied, BluredBrushUtils::colorTable);

    QPainter p(&targetImage);
    p.setOpacity(m_enableDynamicOpacity ? 1 : opacity);
    p.drawImage(QPoint(), app, app.rect());
    p.end();
    targetImage = targetImage.convertToFormat(QImage::Format_Indexed8,
                                              BluredBrushUtils::colorTable);

    TRasterGR8P targetRas = BluredBrushUtils::rasterFromQImage(targetImage);
    ras->copy(targetRas, targetRect.getP00());
  }
}

//----------------------------------------------------------------------------------

void BluredBrush::eraseDrawing(const TRasterP ras, const TRasterP rasBackup,
                               const TRect &bbox, double opacity) const {
  if (!ras) return;

  TRect rasRect    = ras->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;
  QRect qTargetRect(targetRect.x0, targetRect.y0, targetRect.getLx(),
                    targetRect.getLy());
  if (ras->getPixelSize() == 4) {
    QImage image = BluredBrushUtils::rasterToQImage(ras, true);
    QPainter p(&image);
    p.setClipRect(qTargetRect);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(qTargetRect, BluredBrushUtils::rasterToQImage(rasBackup, true),
                qTargetRect);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.setOpacity(opacity);
    p.drawImage(qTargetRect, m_rasImage, qTargetRect);
    p.end();
  } else if (ras->getPixelSize() != 4) {
    QImage targetImage =
        BluredBrushUtils::rasterToQImage(rasBackup).copy(qTargetRect);
    targetImage = targetImage.convertToFormat(
        QImage::Format_ARGB32_Premultiplied, BluredBrushUtils::colorTable);

    QImage app(qTargetRect.size(), QImage::Format_ARGB32_Premultiplied);
    QPainter p2(&app);
    p2.setBrush(QColor(255, 255, 255));
    p2.drawRect(app.rect().adjusted(-1, -1, 0, 0));
    p2.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    p2.drawImage(QPoint(), m_rasImage, qTargetRect);
    p2.end();

    QPainter p(&targetImage);
    p.setOpacity(opacity);
    p.drawImage(QPoint(), app, app.rect());
    p.end();
    targetImage = targetImage.convertToFormat(QImage::Format_Indexed8,
                                              BluredBrushUtils::colorTable);

    TRasterGR8P targetRas = BluredBrushUtils::rasterFromQImage(targetImage);
    ras->copy(targetRas, targetRect.getP00());
  }
}

//----------------------------------------------------------------------------------

void BluredBrush::updateDrawing(const TRasterCM32P rasCM,
                                const TRasterCM32P rasBackupCM,
                                const TRect &bbox, int styleId,
                                int drawOrderMode, bool lockAlpha) const {
  if (!rasCM) return;

  TRect rasRect    = rasCM->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;

  rasCM->copy(rasBackupCM->extract(targetRect), targetRect.getP00());
  BluredBrushUtils::putOnRasterCM(rasCM->extract(targetRect),
                                  m_ras->extract(targetRect), styleId,
                                  drawOrderMode, lockAlpha, m_aboveStyleIds);
}

//----------------------------------------------------------------------------------

void BluredBrush::eraseDrawing(const TRasterCM32P rasCM,
                               const TRasterCM32P rasBackupCM,
                               const TRect &bbox, bool selective,
                               int selectedStyleId,
                               const std::wstring &mode) const {
  if (!rasCM) return;

  TRect rasRect    = rasCM->getBounds();
  TRect targetRect = bbox * rasRect;
  if (targetRect.isEmpty()) return;

  rasCM->extract(targetRect)->copy(rasBackupCM->extract(targetRect));
  BluredBrushUtils::eraseFromRasterCM(rasCM->extract(targetRect),
                                      m_ras->extract(targetRect), selective,
                                      selectedStyleId, mode);
}

//----------------------------------------------------------------------------------

TRect BluredBrush::getBoundFromPoints(
    const std::vector<TThickPoint> &points) const {
  assert(points.size() <= 3);
  TThickPoint p = points[0];
  double radius = p.thick * 0.5;
  TRectD rectD(p - TPointD(radius, radius), p + TPointD(radius, radius));
  int i;
  for (i = 1; i < (int)points.size(); i++) {
    p      = points[i];
    radius = p.thick * 0.5;
    rectD  = rectD +
            TRectD(p - TPointD(radius, radius), p + TPointD(radius, radius));
  }
  TRect rect(tfloor(rectD.x0), tfloor(rectD.y0), tceil(rectD.x1),
             tceil(rectD.y1));
  return rect;
}

//*******************************************************************************
//    Blurred Brush implementation
//*******************************************************************************

RasterBlurredBrush::RasterBlurredBrush(const TRaster32P &ras, int size,
                                       const QRadialGradient &gradient,
                                       bool doDynamicOpacity)
    : m_brushCount(0), m_rotation(0), m_useLineSymmetry(false) {
  BluredBrush *brush = new BluredBrush(ras, size, gradient, doDynamicOpacity);
  m_blurredBrushes.push_back(brush);

  m_rasCenter = ras->getCenterD();
}

//------------------------------------------------------------------

void RasterBlurredBrush::addSymmetryBrushes(double lines, double rotation,
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

  for (int i = 1; i < m_brushCount; i++) {
    BluredBrush *symmBlurredBrush = new BluredBrush(m_blurredBrushes[0]);
    m_blurredBrushes.push_back(symmBlurredBrush);
  }
}

//------------------------------------------------------------------

void RasterBlurredBrush::setAboveStyleIds(QSet<int> &ids) {
  for (int i = 0; i < m_blurredBrushes.size(); i++)
    m_blurredBrushes[i]->setAboveStyleIds(ids);
}

//------------------------------------------------------------------

TRect RasterBlurredBrush::getBoundFromPoints(
    const std::vector<TThickPoint> &points) {
  std::vector<TThickPoint> pts = points;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (hasSymmetryBrushes() && symmetryTool) {
    SymmetryObject symmObj = symmetryTool->getSymmetryObject();
    for (int i = 0; i < points.size(); i++) {
      std::vector<TPointD> symmPts = symmetryTool->getSymmetryPoints(
          points[i], m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
          m_centerPoint, m_useLineSymmetry);

      for (int j = 0; j < symmPts.size(); j++)
        pts.push_back(TThickPoint(symmPts[j], points[i].thick));
    }
  }

  return m_blurredBrushes[0]->getBoundFromPoints(pts);
}

//------------------------------------------------------------------

void RasterBlurredBrush::addPoint(const TThickPoint &p, double opacity) {
  if (!hasSymmetryBrushes()) {
    m_blurredBrushes[0]->addPoint(p, opacity);
    return;
  }

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  std::vector<TPointD> symmPts = symmetryTool->getSymmetryPoints(
      p, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);

  for (int i = 0; i < symmPts.size(); i++)
    m_blurredBrushes[i]->addPoint(TThickPoint(symmPts[i], p.thick), opacity);
}

//------------------------------------------------------------------

void RasterBlurredBrush::addArc(const TThickPoint &pa, const TThickPoint &pb,
                                const TThickPoint &pc, double opacityA,
                                double opacityC) {
  if (!hasSymmetryBrushes()) {
    m_blurredBrushes[0]->addArc(pa, pb, pc, opacityA, opacityC);
    return;
  }

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!symmetryTool) return;

  SymmetryObject symmObj = symmetryTool->getSymmetryObject();

  std::vector<TPointD> symmPtsA = symmetryTool->getSymmetryPoints(
      pa, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);
  std::vector<TPointD> symmPtsB = symmetryTool->getSymmetryPoints(
      pb, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);
  std::vector<TPointD> symmPtsC = symmetryTool->getSymmetryPoints(
      pc, m_rasCenter, m_dpiScale, m_brushCount, m_rotation, m_centerPoint,
      m_useLineSymmetry);

  for (int i = 0; i < m_blurredBrushes.size(); i++)
    m_blurredBrushes[i]->addArc(
        TThickPoint(symmPtsA[i], pa.thick), TThickPoint(symmPtsB[i], pb.thick),
        TThickPoint(symmPtsC[i], pc.thick), opacityA, opacityC);
}

//------------------------------------------------------------------

void RasterBlurredBrush::updateDrawing(const TRasterCM32P rasCM,
                                       const TRasterCM32P rasBackupCM,
                                       const TRect &bbox, int styleId,
                                       int drawOrderMode, bool lockAlpha) {
  for (int i = 0; i < m_blurredBrushes.size(); i++)
    m_blurredBrushes[i]->updateDrawing(rasCM, rasBackupCM, bbox, styleId,
                                       drawOrderMode, lockAlpha);
}

//------------------------------------------------------------------

void RasterBlurredBrush::eraseDrawing(const TRasterCM32P rasCM,
                                      const TRasterCM32P rasBackupCM,
                                      const TRect &bbox, bool selective,
                                      int selectedStyleId,
                                      const std::wstring &mode) {
  for (int i = 0; i < m_blurredBrushes.size(); i++)
    m_blurredBrushes[i]->eraseDrawing(rasCM, rasBackupCM, bbox, selective,
                                      selectedStyleId, mode);
}

//------------------------------------------------------------------

void RasterBlurredBrush::updateDrawing(const TRasterP ras,
                                       const TRasterP rasBackup,
                                       const TPixel32 &color, const TRect &bbox,
                                       double opacity) {
  for (int i = 0; i < m_blurredBrushes.size(); i++)
    m_blurredBrushes[i]->updateDrawing(ras, rasBackup, color, bbox, opacity);
}

//------------------------------------------------------------------

void RasterBlurredBrush::eraseDrawing(const TRasterP ras,
                                      const TRasterP rasBackup,
                                      const TRect &bbox, double opacity) {
  for (int i = 0; i < m_blurredBrushes.size(); i++)
    m_blurredBrushes[i]->eraseDrawing(ras, rasBackup, bbox, opacity);
}

//------------------------------------------------------------------

std::vector<TThickPoint> RasterBlurredBrush::getSymmetryPoints(
    const std::vector<TThickPoint> &points) {
  std::vector<TThickPoint> pts;

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(
      TTool::getTool("T_Symmetry", TTool::RasterImage));
  if (!hasSymmetryBrushes() || !symmetryTool) return pts;

  for (int i = 0; i < points.size(); i++) {
    std::vector<TPointD> symmPts = symmetryTool->getSymmetryPoints(
        points[i], m_rasCenter, m_dpiScale, m_brushCount, m_rotation,
        m_centerPoint, m_useLineSymmetry);

    pts.insert(pts.end(), symmPts.begin(), symmPts.end());
  }

  return pts;
}
