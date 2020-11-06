#pragma once

/*------------
 Iwa_BloomFx
 Based on the LightBloom plugin fx by Dwango, with modifications as follows:
 - parameters are reduced & simplified to gamma, gain and size
 - reduced the block noise appeared especially when the gamma is with higher
value
 - enabled to render alpha gradation
 - enabled to adjust size with the viewer gadget
--------------*/

#ifndef IWA_BLOOMFX_H
#define IWA_BLOOMFX_H

#include "stdfx.h"
#include "tfxparam.h"
#include <opencv2/opencv.hpp>

class Iwa_BloomFx : public TStandardRasterFx {
  FX_PLUGIN_DECLARATION(Iwa_BloomFx)
protected:
  TRasterFxPort m_source;
  TDoubleParamP m_gamma;
  TDoubleParamP m_gain;
  TDoubleParamP m_size;
  TBoolParamP m_alpha_rendering;

  double getSizePixelAmount(const double val, const TAffine affine);
  template <typename RASTER, typename PIXEL>
  void setSourceTileToMat(const RASTER ras, cv::Mat &ingMat,
                          const double gamma);

  template <typename RASTER, typename PIXEL>
  void setMatToOutput(const RASTER ras, const RASTER srcRas, cv::Mat &ingMat,
                      const double gamma, const double gain,
                      const bool withAlpha, const int margin);

public:
  Iwa_BloomFx();
  void doCompute(TTile &tile, double frame,
                 const TRenderSettings &settings) override;
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &info) override;
  bool canHandle(const TRenderSettings &info, double frame) override;
  void getParamUIs(TParamUIConcept *&concepts, int &length) override;
};

#endif