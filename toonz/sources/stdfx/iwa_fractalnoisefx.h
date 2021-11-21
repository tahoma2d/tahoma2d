#pragma once

//******************************************************************
// Iwa FractalNoise Fx
// An Fx emulating Fractal Noise effect in Adobe AfterEffect
//******************************************************************

#ifndef IWA_FRACTALNOISEFX_H
#define IWA_FRACTALNOISEFX_H

#include "tfxparam.h"
#include "tparamset.h"
#include "stdfx.h"

class Iwa_FractalNoiseFx final : public TStandardZeraryFx {
  FX_PLUGIN_DECLARATION(Iwa_FractalNoiseFx)

  enum FractalType {
    Basic = 0,
    TurbulentSmooth,
    TurbulentBasic,
    TurbulentSharp,
    Dynamic,
    DynamicTwist,
    Max,
    Rocky,
    FractalTypeCount
  };

  enum NoiseType { Block = 0, Smooth, NoiseTypeCount };

  struct FNParam {
    FractalType fractalType;
    NoiseType noiseType;
    bool invert;
    double rotation;
    TDimensionD scale;
    TPointD offsetTurbulence;
    bool perspectiveOffset;
    double complexity;
    double subInfluence;
    double subScaling;
    double subRotation;
    TPointD subOffset;
    double evolution;
    bool cycleEvolution;
    double cycleEvolutionRange;
    double dynamicIntensity;

    bool doConical;
    double conicalEvolution;
    double conicalAngle;
    double cameraFov;
    double zScale;

    bool alphaRendering;
  };

protected:
  // Fractal Type �t���N�^���̎��
  TIntEnumParamP m_fractalType;
  // Noise Type �m�C�Y�̎��
  TIntEnumParamP m_noiseType;
  // Invert ���]
  TBoolParamP m_invert;
  /// Contrast �R���g���X�g
  /// Brightness ���邳
  /// Overflow �I�[�o�[�t���[

  //- - - Transform �g�����X�t�H�[�� - - -
  // Rotation ��]
  TDoubleParamP m_rotation;
  // Uniform Scaling�@�c������Œ�
  TBoolParamP m_uniformScaling;
  // Scale �X�P�[��
  TDoubleParamP m_scale;
  // Scale Width �X�P�[���̕�
  TDoubleParamP m_scaleW;
  // Scale Height �X�P�[���̍���
  TDoubleParamP m_scaleH;
  // Offset Turbulence ���C���̃I�t�Z�b�g
  TPointParamP m_offsetTurbulence;
  // Perspective Offset ���߃I�t�Z�b�g
  TBoolParamP m_perspectiveOffset;

  // Complexity ���G�x
  TDoubleParamP m_complexity;

  //- - - Sub Settings �T�u�ݒ� - - -
  // Sub Influence �T�u�e���i���j
  TDoubleParamP m_subInfluence;
  // Sub Scaling�@�T�u�X�P�[��
  TDoubleParamP m_subScaling;
  // Sub Rotation �T�u��]
  TDoubleParamP m_subRotation;
  // Sub Offset �T�u�̃I�t�Z�b�g
  TPointParamP m_subOffset;
  // Center Subscale �T�u�X�P�[���𒆐S
  /// TBoolParamP m_centerSubscale;

  // Evolution �W�J
  TDoubleParamP m_evolution;

  //- - - Evolution Options �W�J�̃I�v�V���� - - -
  // Cycle Evolution �T�C�N���W�J
  TBoolParamP m_cycleEvolution;
  // Cycle (in Evolution) �T�C�N���i�����j
  TDoubleParamP m_cycleEvolutionRange;
  /// Random Seed �����_���V�[�h
  /// Opacity  �s�����x
  /// Blending Mode �`�惂�[�h

  // �_�C�i�~�b�N�̓x����
  TDoubleParamP m_dynamicIntensity;

  //- - - Conical Noise - - - 
  TBoolParamP m_doConical;
  TDoubleParamP m_conicalEvolution;
  TDoubleParamP m_conicalAngle;
  TDoubleParamP m_cameraFov;
  TDoubleParamP m_zScale;

  // - - - additional parameters - - -
  TBoolParamP m_alphaRendering;

public:
  Iwa_FractalNoiseFx();
  bool canHandle(const TRenderSettings &info, double frame) override {
    return true;
  }
  bool doGetBBox(double frame, TRectD &bBox,
                 const TRenderSettings &ri) override;
  void doCompute(TTile &tile, double frame, const TRenderSettings &ri) override;

  void obtainParams(FNParam &param, const double frame, const TAffine &aff);

  template <typename RASTER, typename PIXEL>
  void outputRaster(const RASTER outRas, double *out_buf, const FNParam &param);

  void getParamUIs(TParamUIConcept *&concepts, int &length) override;

  // For Dynamic and Dynamic Twist patterns, the position offsets using gradient
  // / rotation of the parent pattern
  TPointD getSamplePos(double x, double y, const TDimension outDim,
                       const double *out_buf, const int gen, const double scale,
                       const FNParam &param);
  // convert the noise
  void convert(double *buf, const FNParam &param);
  // composite the base noise pattern
  void composite(double *out, double *buf, const double influence,
                 const FNParam &param);
  // finalize pattern (converting the color space)
  void finalize(double *out, const FNParam &param);
};

#endif