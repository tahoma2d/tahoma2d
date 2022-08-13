#pragma once

#ifndef GPHOTOCAM_H
#define GPHOTOCAM_H

#ifdef WITH_GPHOTO2
#include <gphoto2/gphoto2.h>
#endif

#include "opencv2/opencv.hpp"

#include "traster.h"

#include <QObject>
#include <QThread>
#include <QTimer>

typedef struct {
  const char *modeKey;
  const char *batteryLevelKey;
  const char *shutterSpeedKey;
  const char *apertureKey;
  const char *isoKey;
  const char *whiteBalanceKey;
  const char *pictureStyleKey;
  const char *imageQualityKey;
  const char *imageSizeKey;
  const char *exposureCompensationKey;
  const char *colorTemperatureKey;
  const char *manualFocusDriveKey;
  const char *afPositionKey;
  const char *viewfinderKey;
  const char *focusPointKey;
} GPConfigKeys;

const struct {
  QString manufacturer;
  GPConfigKeys keys;
} cameraConfigKeys[] = {
    // Canon
    {"Canon",
     {"autoexposuremode", "batterylevel", "shutterspeed", "aperture", "iso",
      "whitebalance", "picturestyle", "imageformat", 0, "exposurecompensation",
      "colorTemperature", "manualfocusdrive", "eoszoomposition", "viewfinder",
      "focusinfo"}},
    // Nikon
    {"Nikon",
     {"expprogram", "Battery Level", "shutterspeed", "f-number", "iso",
      "whitebalance", 0, "imagequality", "imagesize", "exposurecompensation", 0,
      "manualfocusdrive", "changeafarea", "viewfinder", "changeafarea"}},
    {0,
     {"expprogram", 0, "shutterspeed", "f-number", "iso", "whitebalance", 0,
      "imagequality", "imagesize", "exposurecompensation", 0, "manualfocusdrive",
      0, "viewfinder", 0}}};

//-----------------------------------------------------------------

class GPhotoCam : public QObject {
  Q_OBJECT

public:
  static GPhotoCam *instance() {
    static GPhotoCam _instance;
    return &_instance;
  };

private:
#ifdef WITH_GPHOTO2
  GPContext *m_gpContext;
#endif

private:
  QStringList m_apertureOptions, m_shutterSpeedOptions, m_isoOptions,
      m_exposureOptions, m_whiteBalanceOptions, m_colorTempOptions,
      m_imageQualityOptions, m_imageSizeOptions, m_pictureStyleOptions,
      m_manualFocusRange;

  GPConfigKeys m_configKeys;

public:
  GPhotoCam();
  ~GPhotoCam();

  int m_count;
  int m_gphotoIndex;
  bool m_sessionOpen;

  QTimer *m_eventTimer;
  bool m_capturePreview;
  bool m_captureImage;
  bool m_exitRequested;
  bool m_previewImageReady;

  QString m_cameraName;
  QString m_cameraManufacturer;
  QString m_cameraModel;

#ifdef WITH_GPHOTO2
  CameraFile *m_previewFile;

  CameraAbilitiesList *m_cameraListMaster;
  GPPortInfoList *m_portInfoList;
  CameraList *m_cameraListDetected;
  Camera *m_camera;
  GPPortInfo m_portInfo;
  CameraFilePath *m_cameraImageFilePath;
#endif

  TDimension m_proxyImageDimensions = TDimension(0, 0);
  TDimension m_fullImageDimensions  = TDimension(0, 0);
  int m_liveViewExposureOffset      = 0;
  QString m_realShutterSpeed;
  QString m_displayedShutterSpeed;
  QString m_displayedAperture;
  QString m_displayedIso;

  bool m_converterSucceeded, l_quitLoop;

  cv::Mat m_gPhotoImage;

  bool m_zooming                  = false;
  bool m_pickLiveViewZoom         = false;
  bool m_liveViewZoomReadyToPick  = true;
  int m_liveViewZoom              = 1;
  TPointD m_liveViewZoomPickPoint = TPointD(0.0, 0.0);
  TPoint m_calculatedZoomPoint    = TPoint(0, 0);
  TPoint m_finalZoomPoint         = TPoint(0, 0);
  TPoint m_zoomRectDimensions     = TPoint(0, 0);
  TRect m_zoomRect                = TRect(0, 0, 0, 0);

  void setGphotoIndex(int index) { m_gphotoIndex = index; }
  int getGphotoIndex() { return m_gphotoIndex; }

  bool m_useCalibration;
  cv::Mat m_calibrationMapX, m_calibrationMapY;

  void enableCalibration(bool useCalibration) {
    m_useCalibration = useCalibration;
  }
  void setCalibration(cv::Mat calibrationMapX, cv::Mat calibrationMapY) {
    m_calibrationMapX = calibrationMapX;
    m_calibrationMapY = calibrationMapY;
  };

  cv::Mat getGPhotoImage() { return m_gPhotoImage; }

  int getCameraCount();

  void resetGphotocam(bool liveViewOpen);

  bool isCanon() { return m_cameraManufacturer.contains("Canon"); };
  bool isNikon() { return m_cameraManufacturer.contains("Nikon"); };

  void loadCameraConfigKeys(QString manufacturer);

#ifdef WITH_GPHOTO2
  bool initializeCamera();
  bool getCamera(int index);
  bool releaseCamera();
  QString getCameraName(int index);
  bool openCameraSession();
  bool closeCameraSession();
  void closeAll();
  bool startLiveView();
  bool endLiveView();
  bool getGPhotocamImage();
  bool takePicture();
  QString getMode();
  QString getCurrentBatteryLevel();
  bool downloadImage();

  QStringList getApertureOptions() { return m_apertureOptions; }
  QStringList getShutterSpeedOptions() { return m_shutterSpeedOptions; }
  QStringList getIsoOptions() { return m_isoOptions; }
  QStringList getExposureOptions() { return m_exposureOptions; }
  QStringList getWhiteBalanceOptions() { return m_whiteBalanceOptions; }
  QStringList getColorTemperatureOptions() { return m_colorTempOptions; }
  QStringList getImageQualityOptions() { return m_imageQualityOptions; }
  QStringList getImageSizeOptions() { return m_imageSizeOptions; }
  QStringList getPictureStyleOptions() { return m_pictureStyleOptions; }
  QStringList getManualFocusRange() { return m_manualFocusRange;  }

  QStringList getCameraConfigList(const char *key);
  QString getCameraConfigValue(const char *key);
  bool setCameraConfigValue(const char *key, QString value);

  bool getAvailableApertures();
  bool getAvailableShutterSpeeds();
  bool getAvailableIso();
  bool getAvailableExposureCompensations();
  bool getAvailableWhiteBalances();
  bool getAvailableImageQualities();
  bool getAvailableImageSizes();
  bool getAvailablePictureStyles();
  bool getAvailableColorTemperatures();
  bool getManualFocusRangeData();

  QString getCurrentAperture();
  QString getCurrentShutterSpeed();
  QString getCurrentIso();
  QString getCurrentExposureCompensation();
  QString getCurrentWhiteBalance();
  QString getCurrentColorTemperature();
  QString getCurrentImageQuality();
  QString getCurrentImageSize();
  QString getCurrentPictureStyle();
  int getCurrentLiveViewOffset();

  bool setAperture(QString aperture);
  bool setShutterSpeed(QString shutterSpeed, bool withOffset = true);
  bool setIso(QString iso);
  bool setExposureCompensation(QString exposure);
  bool setWhiteBalance(QString whiteBalance);
  bool setColorTemperature(QString colorTemp);
  bool setImageQuality(QString imageQuality);
  bool setImageSize(QString imageSize);
  bool setPictureStyle(QString pictureStyle);
  bool setManualFocus(int value);
  void setLiveViewOffset(int offset);

  bool zoomLiveView();
  void calculateZoomPoint();
  void makeZoomPoint(TPointD pos);
  void toggleZoomPicking();
  bool setZoomPoint();
  bool focusNear();
  bool focusFar();
  bool focusNear2();
  bool focusFar2();
  bool focusNear3();
  bool focusFar3();
#endif

public slots:
  void onTimeout();
  void onImageReady(const bool &);
  void onFinished();

signals:
  void newGPhotocamImageReady();
  void modeChanged();
  void batteryChanged();
  void apertureChangedSignal(QString);
  void isoChangedSignal(QString);
  void shutterSpeedChangedSignal(QString);
  void exposureChangedSignal(QString);
  void whiteBalanceChangedSignal(QString);
  void imageQualityChangedSignal(QString);
  void imageSizeChangedSignal(QString);
  void pictureStyleChangedSignal(QString);
  void colorTemperatureChangedSignal(QString);
  void liveViewOffsetChangedSignal(int);
  void focusCheckToggled(bool);
  void pickFocusCheckToggled(bool);
};

#endif  // GPHOTOCAM_H
