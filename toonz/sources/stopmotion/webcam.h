#pragma once

#ifndef WEBCAM_H
#define WEBCAM_H

#include "opencv2/opencv.hpp"

// Toonz Includes
#include "traster.h"
#include "toonzqt/gutil.h"

#include <QObject>

class QCamera;
class QCameraInfo;

class Webcam : public QObject {
  Q_OBJECT

public:
  Webcam();
  ~Webcam();

  void setWebcamDeviceName(QString name) { m_webcamDeviceName = name; }
  QString getWebcamDeviceName() { return m_webcamDeviceName; }

  void setWebcamDescription(QString desc) { m_webcamDescription = desc; }
  QString getWebcamDescription() { return m_webcamDescription; }

  void setWebcamIndex(int index) { m_webcamIndex = index; }
  int getWebcamIndex() { return m_webcamIndex; }

  int getWebcamWidth() { return m_webcamWidth; }
  int getWebcamHeight() { return m_webcamHeight; }
  void setWebcamWidth(int width) { m_webcamWidth = width; }
  void setWebcamHeight(int height) { m_webcamHeight = height; }

  void releaseWebcam();
  void clearWebcam();
  QList<QCameraInfo> getWebcams();
  QCamera* getWebcam() { return m_webcam; }
  void setWebcam(QCamera* camera);
  bool initWebcam(int index = 0);
  bool getWebcamImage(TRaster32P& tempImage);

  bool translateIndex(int index);

  QList<QSize> getWebcamResolutions() { return m_webcamResolutions; }
  int getIndexOfResolution();
  void clearWebcamResolutions();
  void refreshWebcamResolutions();

  void setUseMjpg(bool on);
  bool getUseMjpg() { return m_useMjpg; }
  bool getUseDirectShow() { return m_useDirectShow; }
  void setUseDirectShow(int state);

  bool getWebcamAutofocusStatus();
  void setWebcamAutofocusStatus(bool on);

  int getWebcamFocusValue();
  void setWebcamFocusValue(int value);

  int getWebcamExposureValue();
  void setWebcamExposureValue(int value);

  int getWebcamBrightnessValue();
  void setWebcamBrightnessValue(int value);

  int getWebcamContrastValue();
  void setWebcamContrastValue(int value);

  int getWebcamGainValue();
  void setWebcamGainValue(int value);

  int getWebcamSaturationValue();
  void setWebcamSaturationValue(int value);

  void openSettingsWindow();
  void setLut(cv::Mat lut) { m_lut = lut; }
  void setColorMode(int mode) { m_colorMode = mode; }
  void setWhite(int white) { m_white = white; }
  void setBlack(int black) { m_black = black; }
  void setThreshold(int threshold) { m_threshold = threshold; }
  void setGamma(double gamma) { m_gamma = gamma; }
  void computeLut();
  cv::Mat getWebcamImage() { return m_webcamImage; }

  bool isWebcamActive() { return m_cvWebcam.isOpened(); }

private:
  // Webcam Properties
  QList<QCameraInfo> m_webcams;
  QCamera* m_webcam;
  cv::VideoCapture m_cvWebcam;
  QList<QSize> m_webcamResolutions;
  // Webcam Public Properties
  QString m_webcamDeviceName;
  QString m_webcamDescription;
  int m_webcamIndex    = -1;
  bool m_useDirectShow = true;
  int m_webcamWidth    = 0;
  int m_webcamHeight   = 0;
  bool m_useMjpg       = true;
  int m_colorMode      = 0;
  int m_white          = 255;
  int m_black          = 0;
  int m_threshold      = 128;
  double m_gamma       = 1.0;
  cv::Mat m_lut;
  cv::Mat m_webcamImage;

  int m_webcamFocusValue       = 0;
  bool m_webcamAutofocusStatus = true;

  void adjustLevel(cv::Mat& image);
  void binarize(cv::Mat& image);

signals:
  void useMjpgSignal(bool);
  void useDirectShowSignal(bool);
  void updateHistogram(cv::Mat);
};

#endif  // WEBCAM_H