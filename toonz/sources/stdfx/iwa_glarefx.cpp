#include "iwa_glarefx.h"

#include "trop.h"
#include "tdoubleparam.h"
#include "trasterfx.h"
#include "trasterimage.h"

#include "tparamuiconcept.h"

#include "kiss_fft.h"
#include "iwa_cie_d65.h"
#include "iwa_xyz.h"
#include "iwa_simplexnoise.h"

#include <QPair>
#include <QVector>
#include <QReadWriteLock>
#include <QMutexLocker>
#include <QMap>

namespace {
// FFT coordinate -> Normal corrdinate
inline int getCoord(int i, int j, int lx, int ly) {
  int cx = i - lx / 2;
  int cy = j - ly / 2;

  if (cx < 0) cx += lx;
  if (cy < 0) cy += ly;

  return cy * lx + cx;
}

};  // namespace

//--------------------------------------------
// Iwa_GlareFx
//--------------------------------------------

Iwa_GlareFx::Iwa_GlareFx()
    : m_renderMode(new TIntEnumParam(RendeMode_FilterPreview, "Filter Preview"))
    , m_intensity(0.0)
    , m_size(100.0)
    , m_rotation(0.0)
    , m_noise_factor(0.0)
    , m_noise_size(0.5)
    , m_noise_octave(new TIntEnumParam(1, "1"))
    , m_noise_evolution(0.0)
    , m_noise_offset(TPointD(0, 0)) {
  // Bind the common parameters
  addInputPort("Source", m_source);
  addInputPort("Iris", m_iris);

  bindParam(this, "renderMode", m_renderMode);
  m_renderMode->addItem(RendeMode_Render, "Render");

  bindParam(this, "intensity", m_intensity, false);
  bindParam(this, "size", m_size, false);
  m_size->setMeasureName("fxLength");
  bindParam(this, "rotation", m_rotation, false);

  bindParam(this, "noise_factor", m_noise_factor, false);
  bindParam(this, "noise_size", m_noise_size, false);
  bindParam(this, "noise_octave", m_noise_octave, false);
  m_noise_octave->addItem(2, "2");
  m_noise_octave->addItem(3, "3");

  bindParam(this, "noise_evolution", m_noise_evolution, false);
  bindParam(this, "noise_offset", m_noise_offset, false);
  m_noise_offset->getX()->setMeasureName("fxLength");
  m_noise_offset->getY()->setMeasureName("fxLength");

  m_intensity->setValueRange(-5.0, 5.0);
  m_size->setValueRange(10.0, 500.0);
  m_rotation->setValueRange(-1800, 1800);
  m_noise_factor->setValueRange(0.0, 1.0);
  m_noise_size->setValueRange(0.01, 3.0);
}

//--------------------------------------------------------------
double Iwa_GlareFx::getSizePixelAmount(const double val, const TAffine affine) {
  /*--- Convert to vector --- */
  TPointD vect;
  vect.x = val;
  vect.y = 0.0;
  /*--- Apply geometrical transformation ---*/
  // For the following lines I referred to lines 586-592 of
  // sources/stdfx/motionblurfx.cpp
  TAffine aff(affine);
  aff.a13 = aff.a23 = 0; /* ignore translation */
  vect              = aff * vect;
  /*--- return the length of the vector ---*/
  return sqrt(vect.x * vect.x + vect.y * vect.y);
}
//--------------------------------------------------------------

void Iwa_GlareFx::doCompute(TTile& tile, double frame,
                            const TRenderSettings& settings) {
  // If the iris is not connected, then do nothing
  if (!m_iris.isConnected()) {
    tile.getRaster()->clear();
    return;
  }

  int renderMode = m_renderMode->getValue();
  // If the source is not connected & it is render mode, then do nothing.
  if (!m_source.isConnected() && renderMode == RendeMode_Render) {
    tile.getRaster()->clear();
    return;
  }

  // Get the original size of Iris image
  TRectD irisBBox;
  m_iris->getBBox(frame, irisBBox, settings);
  // Compute the iris tile.
  TTile irisTile;
  m_iris->allocateAndCompute(
      irisTile, irisBBox.getP00(),
      TDimension(static_cast<int>(irisBBox.getLx() + 0.5),
                 static_cast<int>(irisBBox.getLy() + 0.5)),
      tile.getRaster(), frame, settings);

  double size = getSizePixelAmount(m_size->getValue(frame), settings.m_affine);
  int dimIris = int(std::ceil(size) * 2.0);
  dimIris     = kiss_fft_next_fast_size(dimIris);
  while ((tile.getRaster()->getSize().lx - dimIris) % 2 != 0)
    dimIris = kiss_fft_next_fast_size(dimIris + 1);
  double irisResizeFactor = double(dimIris) * 0.5 / size;

  kiss_fft_cpx* kissfft_comp_iris;
  // create the iris data for FFT (in the same size as the source tile)
  TRasterGR8P kissfft_comp_iris_ras(dimIris * sizeof(kiss_fft_cpx), dimIris);
  kissfft_comp_iris_ras->lock();
  kissfft_comp_iris = (kiss_fft_cpx*)kissfft_comp_iris_ras->getRawData();

  {
    // Create the Iris image for FFT
    kiss_fft_cpx* kissfft_comp_iris_before;
    TRasterGR8P kissfft_comp_iris_before_ras(dimIris * sizeof(kiss_fft_cpx),
                                             dimIris);
    kissfft_comp_iris_before_ras->lock();
    kissfft_comp_iris_before =
        (kiss_fft_cpx*)kissfft_comp_iris_before_ras->getRawData();
    convertIris(kissfft_comp_iris_before, dimIris, irisBBox, irisTile);

    // Create the FFT plan for the iris image.
    kiss_fftnd_cfg iris_kissfft_plan;
    while (1) {
      int dims[2]       = {dimIris, dimIris};
      int ndims         = 2;
      iris_kissfft_plan = kiss_fftnd_alloc(dims, ndims, false, 0, 0);
      if (iris_kissfft_plan != NULL) break;
    }
    // Do FFT the iris image.
    kiss_fftnd(iris_kissfft_plan, kissfft_comp_iris_before, kissfft_comp_iris);
    kiss_fft_free(iris_kissfft_plan);
    kissfft_comp_iris_before_ras->unlock();
  }

  double3* glare_pattern;
  TRasterGR8P glare_pattern_ras(dimIris * sizeof(double3), dimIris);
  glare_pattern = (double3*)glare_pattern_ras->getRawData();
  glare_pattern_ras->lock();
  // Resize the power spectrum according to each wavelength and combine into the
  // glare pattern
  double intensity = m_intensity->getValue(frame);
  powerSpectrum2GlarePattern(frame, settings.m_affine, kissfft_comp_iris,
                             glare_pattern, dimIris, intensity,
                             irisResizeFactor);

  kissfft_comp_iris_ras->unlock();

  // clear the raster memory
  tile.getRaster()->clear();
  TRaster32P ras32 = tile.getRaster();
  TRaster64P ras64 = tile.getRaster();
  if (ras32)
    ras32->fill(TPixel32::Transparent);
  else if (ras64)
    ras64->fill(TPixel64::Transparent);

  // filter preview mode
  if (renderMode == RendeMode_FilterPreview) {
    int2 margin = {(dimIris - tile.getRaster()->getSize().lx) / 2,
                   (dimIris - tile.getRaster()->getSize().ly) / 2};

    if (ras32)
      setFilterPreviewToResult<TRaster32P, TPixel32>(ras32, glare_pattern,
                                                     dimIris, margin);
    else if (ras64)
      setFilterPreviewToResult<TRaster64P, TPixel64>(ras64, glare_pattern,
                                                     dimIris, margin);

    return;
  }

  // render mode

  // Range of computation
  TRectD _rectOut(tile.m_pos, TDimensionD(tile.getRaster()->getLx(),
                                          tile.getRaster()->getLy()));
  _rectOut = _rectOut.enlarge(static_cast<double>(dimIris / 2));

  TDimensionI dimOut(static_cast<int>(_rectOut.getLx() + 0.5),
                     static_cast<int>(_rectOut.getLy() + 0.5));

  // Enlarge the size to the "fast size" for kissfft which has no factors other
  // than 2,3, or 5.
  if (dimOut.lx < 10000 && dimOut.ly < 10000) {
    int new_x = kiss_fft_next_fast_size(dimOut.lx);
    int new_y = kiss_fft_next_fast_size(dimOut.ly);
    // margin should be integer
    while ((new_x - dimOut.lx) % 2 != 0)
      new_x = kiss_fft_next_fast_size(new_x + 1);
    while ((new_y - dimOut.ly) % 2 != 0)
      new_y = kiss_fft_next_fast_size(new_y + 1);

    _rectOut = _rectOut.enlarge(static_cast<double>(new_x - dimOut.lx) / 2.0,
                                static_cast<double>(new_y - dimOut.ly) / 2.0);

    dimOut.lx = new_x;
    dimOut.ly = new_y;
  }

  kiss_fft_cpx* kissfft_comp_tmp;
  kiss_fft_cpx* kissfft_comp_glare;
  kiss_fft_cpx* kissfft_comp_source;
  TRasterGR8P kissfft_comp_tmp_ras(dimOut.lx * sizeof(kiss_fft_cpx), dimOut.ly);
  TRasterGR8P kissfft_comp_glare_ras(dimOut.lx * sizeof(kiss_fft_cpx),
                                     dimOut.ly);
  TRasterGR8P kissfft_comp_source_ras(dimOut.lx * sizeof(kiss_fft_cpx),
                                      dimOut.ly);
  kissfft_comp_tmp    = (kiss_fft_cpx*)kissfft_comp_tmp_ras->getRawData();
  kissfft_comp_glare  = (kiss_fft_cpx*)kissfft_comp_glare_ras->getRawData();
  kissfft_comp_source = (kiss_fft_cpx*)kissfft_comp_source_ras->getRawData();
  kissfft_comp_tmp_ras->lock();
  kissfft_comp_glare_ras->lock();
  kissfft_comp_source_ras->lock();

  int dims[2]              = {dimOut.ly, dimOut.lx};
  int ndims                = 2;
  kiss_fftnd_cfg plan_fwd  = kiss_fftnd_alloc(dims, ndims, false, 0, 0);
  kiss_fftnd_cfg plan_bkwd = kiss_fftnd_alloc(dims, ndims, true, 0, 0);

  // store the source image to tmp
  {
    // obtain the source tile
    TTile sourceTile;
    m_source->allocateAndCompute(sourceTile, _rectOut.getP00(), dimOut,
                                 tile.getRaster(), frame, settings);

    if (ras32)
      setSourceTileToBuffer<TRaster32P, TPixel32>(sourceTile.getRaster(),
                                                  kissfft_comp_tmp);
    else if (ras64)
      setSourceTileToBuffer<TRaster64P, TPixel64>(sourceTile.getRaster(),
                                                  kissfft_comp_tmp);
  }
  // FFT the source
  kiss_fftnd(plan_fwd, kissfft_comp_tmp, kissfft_comp_source);

  // compute for each rgb channels
  for (int ch = 0; ch < 3; ch++) {
    kissfft_comp_tmp_ras->clear();
    // store the glare pattern to tmp
    setGlarePatternToBuffer(glare_pattern, kissfft_comp_tmp, ch, dimIris,
                            dimOut);

    // FFT the glare pattern
    kiss_fftnd(plan_fwd, kissfft_comp_tmp, kissfft_comp_glare);

    // multiply the glare and the source
    multiplyFilter(kissfft_comp_glare, kissfft_comp_source,
                   dimOut.lx * dimOut.ly);

    // Backward-FFT the glare pattern to tmp
    kiss_fftnd(plan_bkwd, kissfft_comp_glare,
               kissfft_comp_tmp);  // Backward FFT

    // convert tmp to channel values, store it into the tile
    if (ras32)
      setChannelToResult<TRaster32P, TPixel32>(ras32, kissfft_comp_tmp, ch,
                                               dimOut);
    else if (ras64)
      setChannelToResult<TRaster64P, TPixel64>(ras64, kissfft_comp_tmp, ch,
                                               dimOut);
  }

  kiss_fft_free(plan_fwd);
  kiss_fft_free(plan_bkwd);

  kissfft_comp_source_ras->unlock();
  kissfft_comp_glare_ras->unlock();
}

//------------------------------------------------

void Iwa_GlareFx::powerSpectrum2GlarePattern(
    const double frame, const TAffine affine, kiss_fft_cpx* spectrum,
    double3* glare, int dimIris, double intensity, double irisResizeFactor) {
  auto lerp = [](double val1, double val2, double ratio) {
    return val1 * (1.0 - ratio) + val2 * ratio;
  };

  auto lerpGlarePtn = [&](double i, double j, double* gp) {
    int iId[2], jId[2];
    double iRatio, jRatio;
    iId[0] = int(i);
    iId[1] = (iId[0] < dimIris - 1) ? iId[0] + 1 : iId[0];
    iRatio = i - double(iId[0]);
    jId[0] = int(j);
    jId[1] = (jId[0] < dimIris - 1) ? jId[0] + 1 : jId[0];
    jRatio = j - double(jId[0]);
    if (iRatio == 0.0 && jRatio == 0.0) return gp[jId[0] * dimIris + iId[0]];

    return lerp(lerp(gp[jId[0] * dimIris + iId[0]],
                     gp[jId[0] * dimIris + iId[1]], iRatio),
                lerp(gp[jId[1] * dimIris + iId[0]],
                     gp[jId[1] * dimIris + iId[1]], iRatio),
                jRatio);
  };

  double factor =
      (m_renderMode->getValue() == RendeMode_FilterPreview) ? -5 : -11;

  double* glarePattern_p;
  TRasterGR8P glarePattern_ras(dimIris * sizeof(double), dimIris);
  glarePattern_p = (double*)glarePattern_ras->getRawData();
  glarePattern_ras->lock();
  double* g_p = glarePattern_p;
  for (int j = 0; j < dimIris; j++) {
    for (int i = 0; i < dimIris; i++, g_p++) {
      kiss_fft_cpx sp_p = spectrum[getCoord(i, j, dimIris, dimIris)];
      (*g_p)            = sqrt(sp_p.r * sp_p.r + sp_p.i * sp_p.i) *
               std::exp(intensity + factor);
    }
  }

  // distort the pattern with noise here
  double noise_factor = m_noise_factor->getValue(frame);
  double rotation     = m_rotation->getValue(frame);
  if (noise_factor > 0.0 || m_rotation != 0.0) {
    distortGlarePattern(frame, affine, glarePattern_p, dimIris);
  }

  double3* glare_xyz;
  TRasterGR8P glare_xyz_ras(dimIris * sizeof(double3), dimIris);
  glare_xyz_ras->lock();
  glare_xyz = (double3*)glare_xyz_ras->getRawData();
  glare_xyz_ras->clear();

  double irisRadius = double(dimIris / 2);
  // accumurate xyz values for each optical wavelength
  for (int ram = 0; ram < 34; ram++) {
    double rambda = 0.38 + 0.01 * (double)ram;
    double scale  = 0.55 / rambda;
    scale *= irisResizeFactor;
    for (int j = 0; j < dimIris; j++) {
      double j_scaled = (double(j) - irisRadius) * scale + irisRadius;
      if (j_scaled < 0)
        continue;
      else if (j_scaled > double(dimIris - 1))
        break;

      double3* g_xyz_p = &glare_xyz[j * dimIris];
      for (int i = 0; i < dimIris; i++, g_xyz_p++) {
        double i_scaled = (double(i) - irisRadius) * scale + irisRadius;
        if (i_scaled < 0)
          continue;
        else if (i_scaled > double(dimIris - 1))
          break;

        double gl = lerpGlarePtn(i_scaled, j_scaled, glarePattern_p);
        g_xyz_p->x += gl * cie_d65[ram] * xyz[ram * 3 + 0];
        g_xyz_p->y += gl * cie_d65[ram] * xyz[ram * 3 + 1];
        g_xyz_p->z += gl * cie_d65[ram] * xyz[ram * 3 + 2];
      }
    }
  }
  glarePattern_ras->unlock();

  // convert to rgb
  double3* g_xyz_p = glare_xyz;
  double3* g_out_p = glare;
  for (int i = 0; i < dimIris * dimIris; i++, g_xyz_p++, g_out_p++) {
    (*g_out_p).x = 3.240479f * (*g_xyz_p).x - 1.537150f * (*g_xyz_p).y -
                   0.498535f * (*g_xyz_p).z;
    (*g_out_p).y = -0.969256f * (*g_xyz_p).x + 1.875992f * (*g_xyz_p).y +
                   0.041556f * (*g_xyz_p).z;
    (*g_out_p).z = 0.055648f * (*g_xyz_p).x - 0.204043f * (*g_xyz_p).y +
                   1.057311f * (*g_xyz_p).z;
  }

  glare_xyz_ras->unlock();
}

//------------------------------------------------

void Iwa_GlareFx::distortGlarePattern(const double frame, const TAffine affine,
                                      double* glare, const int dimIris) {
  auto lerp = [](double val1, double val2, double ratio) {
    return val1 * (1.0 - ratio) + val2 * ratio;
  };

  auto lerpGlarePtn = [&](double i, double j, double* gp) {
    int iId[2], jId[2];
    double iRatio, jRatio;
    iId[0] = int(i);
    iId[1] = (iId[0] < dimIris - 1) ? iId[0] + 1 : iId[0];
    iRatio = i - double(iId[0]);
    jId[0] = int(j);
    jId[1] = (jId[0] < dimIris - 1) ? jId[0] + 1 : jId[0];
    jRatio = j - double(jId[0]);
    if (iRatio == 0.0 && jRatio == 0.0) return gp[jId[0] * dimIris + iId[0]];

    return lerp(lerp(gp[jId[0] * dimIris + iId[0]],
                     gp[jId[0] * dimIris + iId[1]], iRatio),
                lerp(gp[jId[1] * dimIris + iId[0]],
                     gp[jId[1] * dimIris + iId[1]], iRatio),
                jRatio);
  };

  double size         = m_noise_size->getValue(frame);
  double evolution    = m_noise_evolution->getValue(frame);
  int octave          = m_noise_octave->getValue();
  double noiseFactor  = m_noise_factor->getValue(frame);
  double offsetFactor = 0.005;
  TPointD offset =
      TScale(offsetFactor) * affine * m_noise_offset->getValue(frame);
  double theta = m_rotation->getValue(frame) * M_PI_180;
  double cos_t = std::cos(theta);
  double sin_t = std::sin(theta);

  QList<double> noise_intensity;
  double intensity_sum = 0.0;
  double tmp_intensity = 1.0;
  for (int i = 0; i < octave; i++) {
    noise_intensity.append(tmp_intensity);
    intensity_sum += tmp_intensity;
    tmp_intensity *= 0.5;
  }
  for (double& n_i : noise_intensity) n_i /= intensity_sum;

  // raster for storing the result
  double* distortedPtn_p;
  TRasterGR8P distortedPtn_ras(dimIris * sizeof(double), dimIris);
  distortedPtn_p = (double*)distortedPtn_ras->getRawData();
  distortedPtn_ras->lock();

  double* dist_p = distortedPtn_p;
  for (int j = 0; j < dimIris; j++) {
    double v = double(j) - double(dimIris) / 2.0;
    for (int i = 0; i < dimIris; i++, dist_p++) {
      double u = double(i) - double(dimIris) / 2.0;

      // obtain the noise coordinate
      double2 noiseUV;
      double len             = std::sqrt(u * u + v * v) * size;
      noiseUV.x              = (len == 0.0) ? 0.0 : u / len;
      noiseUV.y              = (len == 0.0) ? 0.0 : v / len;
      double currentSize     = 1.0;
      double currentEvoScale = 1.0;
      double noiseVal        = 0.5;
      noiseUV.x += offset.x;
      noiseUV.y += offset.y;
      for (int oct = 0; oct < octave; oct++) {
        double2 currentNoiseUV = {noiseUV.x / currentSize,
                                  noiseUV.y / currentSize};
        noiseVal += noise_intensity[oct] *
                    SimplexNoise::noise(currentNoiseUV.x, currentNoiseUV.y,
                                        evolution * currentEvoScale);
        currentSize *= 0.5;
        currentEvoScale *= 2.0;
      }

      double scale = 1.0 / (1.0 + (noiseVal - 1) * noiseFactor);
      double rot_u = u * cos_t - v * sin_t;
      double rot_v = u * sin_t + v * cos_t;

      double distorted_i = rot_u * scale + double(dimIris) / 2.0;
      double distorted_j = rot_v * scale + double(dimIris) / 2.0;

      if (distorted_i < 0.0 || distorted_i >= double(dimIris - 1) ||
          distorted_j < 0.0 || distorted_j >= double(dimIris - 1))
        (*dist_p) = 0.0;
      else
        (*dist_p) = lerpGlarePtn(distorted_i, distorted_j, glare);
    }
  }

  dist_p       = distortedPtn_p;
  double* gl_p = glare;
  for (int i = 0; i < dimIris * dimIris; i++, dist_p++, gl_p++)
    (*gl_p) = (*dist_p);

  distortedPtn_ras->unlock();
}

//------------------------------------------------

template <typename RASTER, typename PIXEL>
void Iwa_GlareFx::setFilterPreviewToResult(const RASTER ras, double3* glare,
                                           int dimIris, int2 margin) {
  auto clamp01 = [](double chan) {
    if (chan < 0.0) return 0.0;
    if (chan > 1.0) return 1.0;
    return chan;
  };
  int j = margin.y;
  for (int out_j = 0; out_j < ras->getLy(); j++, out_j++) {
    if (j < 0)
      continue;
    else if (j >= dimIris)
      break;
    PIXEL* pix = ras->pixels(out_j);
    int i      = margin.x;
    for (int out_i = 0; out_i < ras->getLx(); i++, out_i++, pix++) {
      if (i < 0)
        continue;
      else if (i >= dimIris)
        break;
      double3 gl_p = glare[j * dimIris + i];
      pix->r       = (typename PIXEL::Channel)(clamp01(gl_p.x) *
                                         double(PIXEL::maxChannelValue));
      pix->g       = (typename PIXEL::Channel)(clamp01(gl_p.y) *
                                         double(PIXEL::maxChannelValue));
      pix->b       = (typename PIXEL::Channel)(clamp01(gl_p.z) *
                                         double(PIXEL::maxChannelValue));
      pix->m       = PIXEL::maxChannelValue;
    }
  }
}

//------------------------------------------------

// put the source tile's brightness to fft buffer
template <typename RASTER, typename PIXEL>
void Iwa_GlareFx::setSourceTileToBuffer(const RASTER ras, kiss_fft_cpx* buf) {
  kiss_fft_cpx* buf_p = buf;
  for (int j = 0; j < ras->getLy(); j++) {
    PIXEL* pix = ras->pixels(j);
    for (int i = 0; i < ras->getLx(); i++, pix++, buf_p++) {
      // Value = 0.3R 0.59G 0.11B
      (*buf_p).r = (double(pix->r) * 0.3 + double(pix->g) * 0.59 +
                    double(pix->b) * 0.11) /
                   double(PIXEL::maxChannelValue);
    }
  }
}

//------------------------------------------------

void Iwa_GlareFx::setGlarePatternToBuffer(const double3* glare,
                                          kiss_fft_cpx* buf, const int channel,
                                          const int dimIris,
                                          const TDimensionI& dimOut) {
  int margin_x = (dimOut.lx - dimIris) / 2;
  int margin_y = (dimOut.ly - dimIris) / 2;
  for (int j = margin_y; j < margin_y + dimIris; j++) {
    const double3* glare_p = &glare[(j - margin_y) * dimIris];
    kiss_fft_cpx* buf_p    = &buf[j * dimOut.lx + margin_x];
    for (int i = margin_x; i < margin_x + dimIris; i++, buf_p++, glare_p++) {
      (*buf_p).r = (channel == 0)
                       ? (*glare_p).x
                       : (channel == 1) ? (*glare_p).y : (*glare_p).z;
    }
  }
}

//------------------------------------------------

void Iwa_GlareFx::multiplyFilter(kiss_fft_cpx* glare,
                                 const kiss_fft_cpx* source, const int count) {
  kiss_fft_cpx* g_p       = glare;
  const kiss_fft_cpx* s_p = source;
  for (int i = 0; i < count; i++, g_p++, s_p++) {
    double re = (*g_p).r * (*s_p).r - (*g_p).i * (*s_p).i;
    double im = (*g_p).r * (*s_p).i + (*s_p).r * (*g_p).i;
    (*g_p).r  = re;
    (*g_p).i  = im;
  }
}

//------------------------------------------------
template <typename RASTER, typename PIXEL>
void Iwa_GlareFx::setChannelToResult(const RASTER ras, kiss_fft_cpx* buf,
                                     int channel, const TDimensionI& dimOut) {
  auto clamp01 = [](double chan) {
    if (chan < 0.0) return 0.0;
    if (chan > 1.0) return 1.0;
    return chan;
  };

  int margin_x = (dimOut.lx - ras->getSize().lx) / 2;
  int margin_y = (dimOut.ly - ras->getSize().ly) / 2;

  for (int j = 0; j < ras->getLy(); j++) {
    // kiss_fft_cpx* buf_p = &buf[(j + margin_y)*dimOut.lx + margin_x];
    PIXEL* pix = ras->pixels(j);
    for (int i = 0; i < ras->getLx(); i++, pix++) {
      kiss_fft_cpx fft_val =
          buf[getCoord(i + margin_x, j + margin_y, dimOut.lx, dimOut.ly)];
      double val = fft_val.r / (dimOut.lx * dimOut.ly);
      if (channel == 0)
        pix->r = (typename PIXEL::Channel)(clamp01(val) *
                                           double(PIXEL::maxChannelValue));
      else if (channel == 1)
        pix->g = (typename PIXEL::Channel)(clamp01(val) *
                                           double(PIXEL::maxChannelValue));
      else if (channel == 2) {
        pix->b = (typename PIXEL::Channel)(clamp01(val) *
                                           double(PIXEL::maxChannelValue));
        pix->m = PIXEL::maxChannelValue;
      }
    }
  }
}

//------------------------------------------------
bool Iwa_GlareFx::doGetBBox(double frame, TRectD& bBox,
                            const TRenderSettings& info) {
  bBox = TConsts::infiniteRectD;
  return true;
}

//------------------------------------------------

bool Iwa_GlareFx::canHandle(const TRenderSettings& info, double frame) {
  return false;
}

//------------------------------------------------
// Resize / flip the iris image according to the size ratio.
// Normalize the brightness of the iris image.
// Enlarge the iris to the output size.
void Iwa_GlareFx::convertIris(kiss_fft_cpx* kissfft_comp_iris_before,
                              const int& dimIris, const TRectD& irisBBox,
                              const TTile& irisTile) {
  // the original size of iris image
  double2 irisOrgSize = {irisBBox.getLx(), irisBBox.getLy()};

  // add 1 pixel margins to all sides
  int2 filterSize = {(int)std::ceil(irisOrgSize.x) + 2,
                     (int)std::ceil(irisOrgSize.y) + 2};
  TPointD resizeOffset((double)filterSize.x - irisOrgSize.x,
                       (double)filterSize.y - irisOrgSize.y);

  // Try to set the center of the iris to the center of the screen
  if ((dimIris - filterSize.x) % 2 == 1) filterSize.x++;
  if ((dimIris - filterSize.y) % 2 == 1) filterSize.y++;

  TRaster64P resizedIris(TDimension(filterSize.x, filterSize.y));

  TAffine aff;
  TPointD affOffset(0.5, 0.5);
  if (dimIris % 2 == 1) affOffset += TPointD(0.5, 0.5);

  aff = TTranslation(resizedIris->getCenterD() + affOffset);
  aff *= TTranslation(-(irisTile.getRaster()->getCenterD() + affOffset));

  // resample the iris
  TRop::resample(resizedIris, irisTile.getRaster(), aff);

  // accumulated value
  float irisValAmount = 0.0;

  int iris_j = 0;
  // Initialize
  for (int i = 0; i < dimIris * dimIris; i++) {
    kissfft_comp_iris_before[i].r = 0.0;
    kissfft_comp_iris_before[i].i = 0.0;
  }
  for (int j = (dimIris - filterSize.y) / 2; iris_j < filterSize.y;
       j++, iris_j++) {
    if (j < 0) continue;
    if (j >= dimIris) break;
    TPixel64* pix = resizedIris->pixels(iris_j);
    int iris_i    = 0;
    for (int i = (dimIris - filterSize.x) / 2; iris_i < filterSize.x;
         i++, iris_i++) {
      if (i < 0) continue;
      if (i >= dimIris) break;
      // Value = 0.3R 0.59G 0.11B
      kissfft_comp_iris_before[j * dimIris + i].r =
          ((float)pix->r * 0.3f + (float)pix->g * 0.59f +
           (float)pix->b * 0.11f) /
          (float)USHRT_MAX;
      irisValAmount += kissfft_comp_iris_before[j * dimIris + i].r;
      pix++;
    }
  }

  // Normalize value
  for (int i = 0; i < dimIris * dimIris; i++) {
    kissfft_comp_iris_before[i].r /= irisValAmount;
  }
}

void Iwa_GlareFx::getParamUIs(TParamUIConcept*& concepts, int& length) {
  concepts = new TParamUIConcept[length = 2];

  concepts[0].m_type  = TParamUIConcept::RADIUS;
  concepts[0].m_label = "Size";
  concepts[0].m_params.push_back(m_size);

  concepts[1].m_type  = TParamUIConcept::POINT;
  concepts[1].m_label = "Noise Offset";
  concepts[1].m_params.push_back(m_noise_offset);
}

FX_PLUGIN_IDENTIFIER(Iwa_GlareFx, "iwa_GlareFx")