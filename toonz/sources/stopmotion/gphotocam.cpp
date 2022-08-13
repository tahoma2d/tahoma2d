#include "gphotocam.h"
#include "stopmotion.h"
#include "tapp.h"

#include "toonz/tscenehandle.h"
#include "toonz/tcamera.h"
#include "toonz/toonzscene.h"
#include "toonz/stage.h"

#include <QCoreApplication>
#include <QFile>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#define EVENTTIMER_INTERVAL 40
#define DOGPSLEEP_INTERVAL 100

void doGPSleep(int ms) {
  if (ms <= 0) return;

#ifdef Q_OS_WIN
  Sleep(uint(ms));
#else
  struct timespec ts = {ms / 1000, (ms % 1000) * 1000 * 1000};
  nanosleep(&ts, NULL);
#endif
}

#ifdef WITH_GPHOTO2
#include <QDebug>

//-----------------------------------------------------------------------------

static void logdump(GPLogLevel level, const char *domain, const char *str,
                    void *data) {
  if (level == GP_LOG_ERROR)
    qDebug() << "[GP_ERROR] " << QString(str);
  else if (level == GP_LOG_VERBOSE) {
    qDebug() << "[GP_VERBOSE] " << QString(str);
  } else if (level == GP_LOG_DEBUG) {
    qDebug() << "[GP_DEBUG] " << QString(str);
  } else if (level == GP_LOG_DATA) {
    qDebug() << "[GP_DATA] " << QString(str);
  } else
    qDebug() << "[GP_UNKNOWN] " << QString(str);
}
#endif

//-----------------------------------------------------------------------------

GPhotoCam::GPhotoCam() {
  m_count       = 0;
  m_gphotoIndex = -1;
  m_sessionOpen = false;

  m_eventTimer        = new QTimer(this);
  m_capturePreview    = false;
  m_captureImage      = false;
  m_previewImageReady = false;

#ifdef WITH_GPHOTO2
  m_cameraListMaster    = NULL;
  m_portInfoList        = NULL;
  m_cameraListDetected  = NULL;
  m_camera              = NULL;
  m_cameraImageFilePath = new CameraFilePath;

  m_gpContext = gp_context_new();
  gp_log_add_func(GP_LOG_ERROR, logdump, NULL);

  // Load all the camera drivers we have...
  int retVal = gp_abilities_list_new(&m_cameraListMaster);
  if (retVal >= GP_OK) gp_abilities_list_load(m_cameraListMaster, m_gpContext);

  // Load all the port drivers we have...
  retVal = gp_port_info_list_new(&m_portInfoList);
  if (retVal >= GP_OK) gp_port_info_list_load(m_portInfoList);

  gp_file_new(&m_previewFile);
  connect(m_eventTimer, SIGNAL(timeout()), this, SLOT(onTimeout()));
#endif
}

//-----------------------------------------------------------------------------

GPhotoCam::~GPhotoCam() {
#ifdef WITH_GPHOTO2
  gp_file_unref(m_previewFile);
#endif
}

int GPhotoCam::getCameraCount() {
  m_count = 0;

#ifdef WITH_GPHOTO2
  if (m_cameraListDetected == NULL)
    gp_list_new(&m_cameraListDetected);
  else
    gp_list_reset(m_cameraListDetected);

  if (m_cameraListDetected != NULL) {
    m_count = gp_camera_autodetect(m_cameraListDetected, m_gpContext);
    if (m_count < 0) m_count = 0;
  }
#endif

  return m_count;
}

//-----------------------------------------------------------------------------

void GPhotoCam::resetGphotocam(bool liveViewOpen) {
#ifdef WITH_GPHOTO2
  m_proxyImageDimensions = TDimension(0, 0);

  if (m_sessionOpen && getCameraCount() > 0) {
    if (liveViewOpen) {
      endLiveView();
    }
    closeCameraSession();
  }

#endif
}

//-----------------------------------------------------------------

void GPhotoCam::loadCameraConfigKeys(QString manufacturer) {
  int i = sizeof(cameraConfigKeys) - 1;

  if (manufacturer.contains("Canon"))
    i = 0;
  else if (manufacturer.contains("Nikon"))
    i = 1;

  m_configKeys.modeKey         = cameraConfigKeys[i].keys.modeKey;
  m_configKeys.batteryLevelKey = cameraConfigKeys[i].keys.batteryLevelKey;
  m_configKeys.shutterSpeedKey = cameraConfigKeys[i].keys.shutterSpeedKey;
  m_configKeys.apertureKey     = cameraConfigKeys[i].keys.apertureKey;
  m_configKeys.isoKey          = cameraConfigKeys[i].keys.isoKey;
  m_configKeys.whiteBalanceKey = cameraConfigKeys[i].keys.whiteBalanceKey;
  m_configKeys.pictureStyleKey = cameraConfigKeys[i].keys.pictureStyleKey;
  m_configKeys.imageQualityKey = cameraConfigKeys[i].keys.imageQualityKey;
  m_configKeys.imageSizeKey    = cameraConfigKeys[i].keys.imageSizeKey;
  m_configKeys.exposureCompensationKey =
      cameraConfigKeys[i].keys.exposureCompensationKey;
  m_configKeys.colorTemperatureKey =
      cameraConfigKeys[i].keys.colorTemperatureKey;
  m_configKeys.manualFocusDriveKey =
      cameraConfigKeys[i].keys.manualFocusDriveKey;
  m_configKeys.afPositionKey = cameraConfigKeys[i].keys.afPositionKey;
  m_configKeys.viewfinderKey = cameraConfigKeys[i].keys.viewfinderKey;
  m_configKeys.focusPointKey = cameraConfigKeys[i].keys.focusPointKey;
}

//-----------------------------------------------------------------

enum EventTrigger {
  NoTrigger = 0,
  BatteryLevel,
  ImageSize,
  ImageQuality,
  Whitebalance,
  Aperture,
  ShutterSpeed,
  Mode,
  ISO,
  ExposureCompensation,
  PictureStyle,
  ColorTemperature
};

EventTrigger checkEventCode(QString manufacturer, char *evtdata) {
  // Generic
  if (strstr(evtdata, "5001")) return EventTrigger::BatteryLevel;
  if (strstr(evtdata, "5003")) return EventTrigger::ImageSize;
  if (strstr(evtdata, "5004")) return EventTrigger::ImageQuality;
  if (strstr(evtdata, "5005")) return EventTrigger::Whitebalance;
  if (strstr(evtdata, "5007")) return EventTrigger::Aperture;
  if (strstr(evtdata, "500d")) return EventTrigger::ShutterSpeed;
  if (strstr(evtdata, "500e")) return EventTrigger::Mode;
  if (strstr(evtdata, "500f")) return EventTrigger::ISO;
  if (strstr(evtdata, "5010")) return EventTrigger::ExposureCompensation;

  if (manufacturer.contains("Canon")) {
    if (strstr(evtdata, "d005") || strstr(evtdata, "d105") ||
        strstr(evtdata, "d138"))
      return EventTrigger::Mode;
    if (strstr(evtdata, "d006") || strstr(evtdata, "d120"))
      return EventTrigger::ImageQuality;
    if (strstr(evtdata, "d008")) return EventTrigger::ImageSize;
    if (strstr(evtdata, "d013") || strstr(evtdata, "d109"))
      return EventTrigger::Whitebalance;
    if (strstr(evtdata, "d01c") || strstr(evtdata, "d103"))
      return EventTrigger::ISO;
    if (strstr(evtdata, "d01d") || strstr(evtdata, "d101"))
      return EventTrigger::Aperture;
    if (strstr(evtdata, "d01e") || strstr(evtdata, "d102"))
      return EventTrigger::ShutterSpeed;
    if (strstr(evtdata, "d01f") || strstr(evtdata, "d104"))
      return EventTrigger::ExposureCompensation;
    if (strstr(evtdata, "d10a")) return EventTrigger::ColorTemperature;
    if (strstr(evtdata, "d110")) return EventTrigger::PictureStyle;
    if (strstr(evtdata, "d111") || strstr(evtdata, "d1a6"))
      return EventTrigger::BatteryLevel;
  } else if (manufacturer.contains("Nikon")) {
    if (strstr(evtdata, "d038")) return EventTrigger::Mode;
    if (strstr(evtdata, "d058")) return EventTrigger::ExposureCompensation;
    if (strstr(evtdata, "f002") || strstr(evtdata, "d100"))
      return EventTrigger::ISO;
    if (strstr(evtdata, "f003") || strstr(evtdata, "d087"))
      return EventTrigger::Aperture;
    if (strstr(evtdata, "f004")) return EventTrigger::ShutterSpeed;
    if (strstr(evtdata, "f009") || strstr(evtdata, "d031"))
      return EventTrigger::ImageQuality;
    if (strstr(evtdata, "f00a")) return EventTrigger::ImageSize;
    if (strstr(evtdata, "f00c")) return EventTrigger::Whitebalance;
  } else if (manufacturer.contains("Kodak")) {
    if (strstr(evtdata, "d001")) return EventTrigger::ColorTemperature;
  } else if (manufacturer.contains("Fuji")) {
    if (strstr(evtdata, "d218")) return EventTrigger::Aperture;
    if (strstr(evtdata, "d242")) return EventTrigger::BatteryLevel;
    if (strstr(evtdata, "d017")) return EventTrigger::ColorTemperature;
    if (strstr(evtdata, "d018")) return EventTrigger::ImageQuality;
    if (strstr(evtdata, "d219")) return EventTrigger::ShutterSpeed;
  } else if (manufacturer.contains("Olympus")) {
    if (strstr(evtdata, "d002")) return EventTrigger::Aperture;
    if (strstr(evtdata, "d008")) return EventTrigger::ExposureCompensation;
    if (strstr(evtdata, "d00d")) return EventTrigger::ImageQuality;
    if (strstr(evtdata, "d007")) return EventTrigger::ISO;
    if (strstr(evtdata, "d01c")) return EventTrigger::ShutterSpeed;
    if (strstr(evtdata, "d01e")) return EventTrigger::Whitebalance;
  } else if (manufacturer.contains("Panasonic")) {
    if (strstr(evtdata, "02000040")) return EventTrigger::Aperture;
    if (strstr(evtdata, "02000080")) return EventTrigger::Mode;
    if (strstr(evtdata, "02000060")) return EventTrigger::ExposureCompensation;
    if (strstr(evtdata, "020000a2")) return EventTrigger::ImageQuality;
    if (strstr(evtdata, "02000020")) return EventTrigger::ISO;
    if (strstr(evtdata, "02000030")) return EventTrigger::ShutterSpeed;
    if (strstr(evtdata, "02000050")) return EventTrigger::Whitebalance;
  } else if (manufacturer.contains("RICOH")) {
    if (strstr(evtdata, "d00f")) return EventTrigger::ShutterSpeed;
  } else if (manufacturer.contains("Sigma")) {
    if (strstr(evtdata, "d002")) return EventTrigger::Aperture;
    if (strstr(evtdata, "d006")) return EventTrigger::BatteryLevel;
    if (strstr(evtdata, "d005")) return EventTrigger::ExposureCompensation;
    if (strstr(evtdata, "d004")) return EventTrigger::ISO;
    if (strstr(evtdata, "d001")) return EventTrigger::ShutterSpeed;
  } else if (manufacturer.contains("Sony")) {
    if (strstr(evtdata, "d6c5")) return EventTrigger::Aperture;
    if (strstr(evtdata, "d218") || strstr(evtdata, "d6f1"))
      return EventTrigger::BatteryLevel;
    if (strstr(evtdata, "d20f") || strstr(evtdata, "d6f0"))
      return EventTrigger::ColorTemperature;
    if (strstr(evtdata, "d224") || strstr(evtdata, "d6c3"))
      return EventTrigger::ExposureCompensation;
    if (strstr(evtdata, "d6b7")) return EventTrigger::ImageSize;
    if (strstr(evtdata, "d203")) return EventTrigger::ImageQuality;
    if (strstr(evtdata, "d21e") || strstr(evtdata, "d6f2"))
      return EventTrigger::ISO;
    if (strstr(evtdata, "d20d") || strstr(evtdata, "d6ea"))
      return EventTrigger::ShutterSpeed;
    if (strstr(evtdata, "d6b8")) return EventTrigger::Whitebalance;
  }

  return EventTrigger::NoTrigger;
}

//-----------------------------------------------------------------

void GPhotoCam::onTimeout() {
#ifdef WITH_GPHOTO2
  bool debug = false;
#ifdef _MSC_VER
  debug = true;
#endif

  CameraEventType evttype;
  CameraFilePath *path;
  void *evtdata;
  int retVal;

  if (!m_gpContext || !m_camera || m_exitRequested) return;

  if (m_captureImage) {
    m_captureImage = false;

    if (!isCanon() || m_cameraName.contains("450D") ||
        m_cameraName.contains("1000D") || m_cameraName.contains("40D")) {
      // Nikon: Turn off autofocus from a libgphoto2 point of view.
      // Does not work for Canon but may work for others.
      retVal = gp_setting_set("ptp2", "autofocus", "off");
      retVal = gp_camera_trigger_capture(m_camera, m_gpContext);
    } else {
      retVal = setCameraConfigValue("eosremoterelease", "Immediate");
      if (retVal >= GP_OK)
        setCameraConfigValue("eosremoterelease", "Release Full");
    }
  }

  // Capture preview when enabled
  if (m_capturePreview && !m_previewImageReady) {
    retVal = gp_camera_capture_preview(m_camera, m_previewFile, m_gpContext);
    m_previewImageReady = true;
  }

  while (1) {
    if (!m_gpContext || !m_camera || m_exitRequested) break;

    retVal =
        gp_camera_wait_for_event(m_camera, 1, &evttype, &evtdata, m_gpContext);
    if (retVal < GP_OK) break;
    if (m_exitRequested) break;

    switch (evttype) {
    case GP_EVENT_FILE_ADDED:
      path = (CameraFilePath *)evtdata;
      if (debug)
        qDebug() << "[GphotoCamEventHandler] FILE ADDED ON CAMERA: "
                 << path->folder << "/" << path->name;

      m_cameraImageFilePath = path;
      downloadImage();
      free(evtdata);
      break;
#ifdef GP_EVENT_FILE_CHANGED
    case GP_EVENT_FILE_CHANGED:
      path = (CameraFilePath *)evtdata;
      if (debug)
        qDebug() << "[GphotoCamEventHandler] File CHANGED on the camera: "
                 << path->folder << "/" << path->name;
      free(evtdata);
      break;
#endif
    case GP_EVENT_FOLDER_ADDED:
      path = (CameraFilePath *)evtdata;
      if (debug)
        qDebug() << "[GphotoCamEventHandler] FOLDER ADDED ON CAMERA: "
                 << path->folder << "/" << path->name;
      free(evtdata);
      break;
    case GP_EVENT_CAPTURE_COMPLETE:
      if (debug) qDebug() << "[GphotoCamEventHandler] CAPTURE COMPLETE";
      break;
    case GP_EVENT_TIMEOUT:
      // if (debug) qDebug() << "[GphotoCamEventHandler] TIMEOUT";
      break;
    case GP_EVENT_UNKNOWN:
      if (evtdata) {
        if (debug)
          qDebug() << "[GphotoCamEventHandler] UNKNOWN EVENT: "
                   << (char *)evtdata;

        EventTrigger event =
            checkEventCode(m_cameraManufacturer, (char *)evtdata);

        switch (event) {
        case EventTrigger::BatteryLevel: {
          QString value = getCameraConfigValue(m_configKeys.batteryLevelKey);
          emit batteryChanged();
          break;
        }
        case EventTrigger::ImageSize: {
          QString value = getCameraConfigValue(m_configKeys.imageQualityKey);
          emit imageSizeChangedSignal(value);
          break;
        }
        case EventTrigger::ImageQuality: {
          QString value = getCameraConfigValue(m_configKeys.imageQualityKey);
          emit imageQualityChangedSignal(value);
          break;
        }
        case EventTrigger::Whitebalance: {
          QString value = getCameraConfigValue(m_configKeys.whiteBalanceKey);
          emit whiteBalanceChangedSignal(value);
          break;
        }
        case EventTrigger::Aperture: {
          QString value = getCameraConfigValue(m_configKeys.apertureKey);
          emit apertureChangedSignal(value);
          break;
        }
        case EventTrigger::ShutterSpeed: {
          QString value = getCameraConfigValue(m_configKeys.shutterSpeedKey);
          emit shutterSpeedChangedSignal(value);
          break;
        }
        case EventTrigger::Mode: {
          QString value = getCameraConfigValue(m_configKeys.modeKey);
          emit modeChanged();
          break;
        }
        case EventTrigger::ISO: {
          QString value = getCameraConfigValue(m_configKeys.isoKey);
          emit isoChangedSignal(value);
          break;
        }
        case EventTrigger::ExposureCompensation: {
          QString value =
              getCameraConfigValue(m_configKeys.exposureCompensationKey);
          emit exposureChangedSignal(value);
          break;
        }
        case EventTrigger::PictureStyle: {
          QString value = getCameraConfigValue(m_configKeys.pictureStyleKey);
          emit pictureStyleChangedSignal(value);
          break;
        }
        case EventTrigger::ColorTemperature: {
          QString value =
              getCameraConfigValue(m_configKeys.colorTemperatureKey);
          emit colorTemperatureChangedSignal(value);
          break;
        }
        default:
          break;
        }

        free(evtdata);
      } else if (debug)
        qDebug() << "[GphotoCamEventHandler] UNKNOWN EVENT (no data).";
      break;
    default:
      if (debug)
        qDebug() << "[GphotoCamEventHandler] DEFAULT EVENT - Type:" << evttype;
      break;
    }
    if (evttype == GP_EVENT_TIMEOUT) break;
  }
#endif
}

//-----------------------------------------------------------------

void GPhotoCam::onImageReady(const bool &status) {
  m_converterSucceeded = status;
}

//-----------------------------------------------------------------

void GPhotoCam::onFinished() { l_quitLoop = true; }

//-----------------------------------------------------------------------------

#ifdef WITH_GPHOTO2

QString GPhotoCam::getCameraName(int index) {
  if (!m_count) return 0;

  const char *name;

  gp_list_get_name(m_cameraListDetected, index, &name);

  m_cameraName = QString(name);
  return m_cameraName;
}

//-----------------------------------------------------------------------------

bool GPhotoCam::getCamera(int index) {
  if (!m_count) return false;

  int retVal = gp_camera_new(&m_camera);

  if (retVal < GP_OK) return false;

  const char *cameraName, *cameraPort;
  gp_list_get_name(m_cameraListDetected, index, &cameraName);
  gp_list_get_value(m_cameraListDetected, index, &cameraPort);

  // Set camera abilities
  CameraAbilities abilities;
  int abilityIndex =
      gp_abilities_list_lookup_model(m_cameraListMaster, cameraName);
  if (abilityIndex < 0) return false;

  retVal = gp_abilities_list_get_abilities(m_cameraListMaster, abilityIndex,
                                           &abilities);
  if (retVal < GP_OK) return false;

  retVal = gp_camera_set_abilities(m_camera, abilities);
  if (retVal < GP_OK) {
    gp_camera_unref(m_camera);
    return false;
  }

  // Set camera port
  // Reload port list in case something changed
  if (m_portInfoList) {
    gp_port_info_list_free(m_portInfoList);
    gp_port_info_list_new(&m_portInfoList);
  }

  retVal = gp_port_info_list_load(m_portInfoList);
  if (retVal < GP_OK) {
    gp_camera_unref(m_camera);
    return false;
  }

  int portIndex = gp_port_info_list_lookup_path(m_portInfoList, cameraPort);
  if (portIndex == GP_ERROR_UNKNOWN_PORT) {
    gp_camera_unref(m_camera);
    return false;
  }

  retVal = gp_port_info_list_get_info(m_portInfoList, portIndex, &m_portInfo);
  if (retVal < GP_OK) {
    gp_camera_unref(m_camera);
    return false;
  }

  retVal = gp_camera_set_port_info(m_camera, m_portInfo);
  if (retVal < GP_OK) {
    gp_camera_unref(m_camera);
    return false;
  }

  return true;
}

//-----------------------------------------------------------------------------

bool GPhotoCam::initializeCamera() {
  if (!m_camera) return false;

  int retVal = gp_camera_init(m_camera, m_gpContext);
  if (retVal < GP_OK) return false;

  m_cameraManufacturer = getCameraConfigValue("manufacturer");
  m_cameraModel        = getCameraConfigValue("cameramodel");

  loadCameraConfigKeys(m_cameraManufacturer);

  return true;
}
  
//-----------------------------------------------------------------------------

bool GPhotoCam::releaseCamera() {
  int retVal = GP_OK;

  if (!m_camera) return retVal;

  if (m_eventTimer->isActive()) {
    m_exitRequested = true;
    // Give time for handler to stop
    doGPSleep(DOGPSLEEP_INTERVAL);

    m_eventTimer->stop();
  }

  retVal = gp_camera_exit(m_camera, m_gpContext);

  m_exitRequested = false;

  m_camera      = 0;
  m_gphotoIndex = -1;

  return (retVal >= GP_OK);
}

//-----------------------------------------------------------------------------

bool GPhotoCam::openCameraSession() {
  if (!m_camera) return false;

  m_sessionOpen = true;

  m_eventTimer->start(EVENTTIMER_INTERVAL);

  return true;
}

//-----------------------------------------------------------------------------

bool GPhotoCam::closeCameraSession() {

  m_sessionOpen = false;

  if (!m_camera) return true;

  if (m_liveViewExposureOffset != 0) {
    setShutterSpeed(m_displayedShutterSpeed, false);
    doGPSleep(DOGPSLEEP_INTERVAL);
  }

  m_eventTimer->stop();

  return true;
}

//-----------------------------------------------------------------------------

void GPhotoCam::closeAll() {
#ifdef WITH_GPHOTO2
  if (m_camera) releaseCamera();
#endif
}

//-----------------------------------------------------------------

bool GPhotoCam::startLiveView() {
  if (!m_camera || !m_sessionOpen) return false;

  int retVal = gp_camera_capture_preview(m_camera, m_previewFile, m_gpContext);
  if (retVal < GP_OK) return false;

  m_capturePreview = true;

  m_previewImageReady = true;

  StopMotion::instance()->m_liveViewStatus = 1;

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::endLiveView() {
  StopMotion::instance()->m_liveViewStatus = 0;

  m_capturePreview = false;

  bool status = setCameraConfigValue(m_configKeys.viewfinderKey, "0");

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::getGPhotocamImage() {
  if (!m_camera || !m_capturePreview) return false;

  int retVal;
  CameraFile *file = m_previewFile;

  if (!file || !m_previewImageReady) return false;

  const char *dataPtr;
  unsigned long dataSize;
  retVal = gp_file_get_data_and_size(file, &dataPtr, &dataSize);
  if (retVal < GP_OK) {
    m_previewImageReady = false;
    return false;
  }

  JpgConverter *converter = new JpgConverter;
  converter->setDataPtr((unsigned char *)dataPtr, dataSize);

  connect(converter, SIGNAL(imageReady(bool)), this, SLOT(onImageReady(bool)),
          Qt::QueuedConnection);
  connect(converter, SIGNAL(finished()), this, SLOT(onFinished()),
          Qt::QueuedConnection);

  converter->start();

  while (!l_quitLoop)
    QCoreApplication::processEvents(QEventLoop::AllEvents |
                                    QEventLoop::WaitForMoreEvents);

  l_quitLoop = false;
  StopMotion::instance()->m_liveViewImage = converter->getImage();
  StopMotion::instance()->m_hasLiveViewImage = true;

  m_previewImageReady = false;

  if (!m_converterSucceeded) return false;

  uchar *imgBuf = StopMotion::instance()->m_liveViewImage->getRawData();
  int height    = StopMotion::instance()->m_liveViewImage->getLy();
  int width     = StopMotion::instance()->m_liveViewImage->getLx();

  cv::Mat imgData(height, width, CV_8UC4, (void *)imgBuf);

  // perform calibration
  if (m_useCalibration) {
    cv::remap(imgData, imgData, m_calibrationMapX, m_calibrationMapY,
              cv::INTER_LINEAR);
  }

  m_gPhotoImage = imgData;

  delete converter;

  QString focusPosition = getCameraConfigValue(m_configKeys.focusPointKey);
  if (m_configKeys.focusPointKey == "focusinfo") {
    int i = focusPosition.indexOf("size=");
    i += 5;
    int j         = focusPosition.indexOf(",", i);
    focusPosition = focusPosition.mid(i, (j - i));
  }

  QStringList sizeDimensions = focusPosition.split("x");
  if (sizeDimensions.size() > 1) {
    m_fullImageDimensions =
        TDimension(sizeDimensions.at(0).toInt(), sizeDimensions.at(1).toInt());
    m_zoomRectDimensions =
        TPoint((m_fullImageDimensions.lx / 5), (m_fullImageDimensions.ly / 5));
    if (m_zoomRect == TRect(0, 0, 0, 0)) {
      m_zoomRect = TRect(0, m_zoomRectDimensions.y, m_zoomRectDimensions.x, 0);
      calculateZoomPoint();
    }
  }

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::takePicture() {
  if (m_liveViewExposureOffset != 0) {
    bool status = setShutterSpeed(m_displayedShutterSpeed, false);
    if (!status) {
      DVGui::error(tr("An error occurred.  Please try again."));
      return status;
    }
    doGPSleep(DOGPSLEEP_INTERVAL);
  }

  m_captureImage = true;
  return true;
}

bool GPhotoCam::downloadImage() {
  int fd, retVal;
  CameraFile *file;

  QString imageFilename = m_cameraImageFilePath->name;
  bool isRaw            = (imageFilename.at(imageFilename.length() - 1) == '2');

  QString filename = isRaw ? StopMotion::instance()->m_tempRaw
                           : StopMotion::instance()->m_tempFile;

  // Get image off the camera
  QByteArray ba  = filename.toLocal8Bit();
  const char *fn = ba.data();
#ifdef WIN32
  fd = _open(fn, O_CREAT | O_WRONLY | O_BINARY, 0644);
#else
  fd = open(fn, O_CREAT | O_WRONLY, 0644);
#endif

  retVal = gp_file_new_from_fd(&file, fd);
  if (retVal < GP_OK) return false;

  retVal = gp_camera_file_get(m_camera, m_cameraImageFilePath->folder,
                              m_cameraImageFilePath->name, GP_FILE_TYPE_NORMAL,
                              file, m_gpContext);
  if (retVal < GP_OK) return false;

  retVal = gp_file_unref(file);

  retVal = gp_camera_file_delete(m_camera, m_cameraImageFilePath->folder,
                                 m_cameraImageFilePath->name, m_gpContext);

  // Currently does not support creating images from Raw files
  if (isRaw) {
    return true;
  }

  // Open file and process
  retVal = gp_file_new(&file);
  if (retVal < GP_OK) return false;

#ifdef WIN32
  fd = _open(fn, O_RDONLY | O_BINARY);
#else
  fd = open(fn, O_RDONLY);
#endif

  retVal = gp_file_new_from_fd(&file, fd);
  if (retVal < GP_OK) return false;

  const char *dataPtr;
  unsigned long dataSize;

  retVal = gp_file_get_data_and_size(file, &dataPtr, &dataSize);
  if (retVal < GP_OK) return false;

  JpgConverter *converter = new JpgConverter;
  converter->setDataPtr((unsigned char *)dataPtr, dataSize);

  connect(converter, SIGNAL(imageReady(bool)), this, SLOT(onImageReady(bool)),
          Qt::QueuedConnection);
  connect(converter, SIGNAL(finished()), this, SLOT(onFinished()),
          Qt::QueuedConnection);

  converter->start();

  while (!l_quitLoop)
    QCoreApplication::processEvents(QEventLoop::AllEvents |
                                    QEventLoop::WaitForMoreEvents);

  l_quitLoop           = false;
  TRaster32P tempImage = converter->getImage();

  if (!m_converterSucceeded || !tempImage || tempImage->isEmpty()) return false;

  uchar *imgBuf = tempImage->getRawData();
  int height    = tempImage->getLy();
  int width     = tempImage->getLx();

  // Get camera size info
  TCamera *camera =
      TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
  TDimension res = camera->getRes();
  if (m_proxyImageDimensions == TDimension(0, 0)) {
    m_proxyImageDimensions = res;
  }

  cv::Mat imgData(height, width, CV_8UC4, (void *)imgBuf);

  // perform calibration
  if (m_useCalibration) {
    cv::remap(imgData, imgData, m_calibrationMapX, m_calibrationMapY,
              cv::INTER_LINEAR);
  }

  m_gPhotoImage = imgData;

  // calculate the size of the new image
  // and resize it down
  double r = (double)width / (double)height;
  cv::Size dim(m_proxyImageDimensions.lx, m_proxyImageDimensions.lx / r);
  int newWidth  = dim.width;
  int newHeight = dim.height;
  cv::Mat imgResized(dim, CV_8UC4);
  cv::resize(imgData, imgResized, dim);
  int newSize       = imgResized.total() * imgResized.elemSize();
  uchar *resizedBuf = imgResized.data;

  // copy the image to m_newImage
  StopMotion::instance()->m_newImage = TRaster32P(newWidth, newHeight);
  StopMotion::instance()->m_newImage->lock();
  uchar *rawData = StopMotion::instance()->m_newImage->getRawData();
  memcpy(rawData, resizedBuf, newSize);
  StopMotion::instance()->m_newImage->unlock();

  gp_file_unref(file);

  emit(newGPhotocamImageReady());

  delete converter;

  return true;
}

//-----------------------------------------------------------------

QStringList GPhotoCam::getCameraConfigList(const char *key) {
  QStringList configList;
  CameraWidget *widget = NULL, *child = NULL;
  CameraWidgetType widgetType;
  int retVal;

  if (!key) return configList;

  retVal = gp_camera_get_config(m_camera, &widget, m_gpContext);
  if (retVal < GP_OK) return configList;

  retVal = gp_widget_get_child_by_name(widget, key, &child);
  if (retVal < GP_OK)
    retVal = gp_widget_get_child_by_label(widget, key, &child);
  if (retVal < GP_OK) {
    gp_widget_free(widget);
    return configList;
  }

  retVal = gp_widget_get_type(child, &widgetType);
  if (retVal < GP_OK) {
    gp_widget_free(widget);
    return configList;
  }

  switch (widgetType) {
  case GP_WIDGET_MENU:
  case GP_WIDGET_RADIO: {
    int choices = gp_widget_count_choices(child);
    for (int i = 0; i < choices; i++) {
      const char *choice;
      retVal = gp_widget_get_choice(child, i, &choice);
      if (retVal < GP_OK) continue;

      configList.push_back(choice);
    }
    break;
  }
  case GP_WIDGET_RANGE: {
    float rmin, rmax, rstep;
    retVal = gp_widget_get_range(child, &rmin, &rmax, &rstep);
    if (retVal < GP_OK) break;

    configList.push_back(QString::number(rmin));
    configList.push_back(QString::number(rmax));
    configList.push_back(QString::number(rstep));
    break;
  }
  default:
    break;
  }

  gp_widget_free(widget);

  return configList;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCameraConfigValue(const char *key) {
  CameraWidget *widget = NULL, *child = NULL;
  CameraWidgetType widgetType;
  int retVal;

  QString currentValue = "-";

  if (!key) return currentValue;

  retVal = gp_camera_get_single_config(m_camera, key, &child, m_gpContext);
  if (retVal == GP_OK)
    widget = child;
  else {
    retVal = gp_camera_get_config(m_camera, &widget, m_gpContext);
    if (retVal < GP_OK) return currentValue;

    retVal = gp_widget_get_child_by_name(widget, key, &child);
    if (retVal < GP_OK)
      retVal = gp_widget_get_child_by_label(widget, key, &child);
    if (retVal < GP_OK) {
      gp_widget_free(widget);
      return currentValue;
    }
  }

  retVal = gp_widget_get_type(child, &widgetType);
  if (retVal < GP_OK) {
    gp_widget_free(widget);
    return currentValue;
  }

  switch (widgetType) {
  case GP_WIDGET_MENU:
  case GP_WIDGET_RADIO:
  case GP_WIDGET_TEXT: {
    char *cvalue;
    retVal                            = gp_widget_get_value(child, &cvalue);
    if (retVal >= GP_OK) currentValue = cvalue;
    break;
  }
  case GP_WIDGET_RANGE: {
    float fvalue;
    retVal                            = gp_widget_get_value(child, &fvalue);
    if (retVal >= GP_OK) currentValue = QString::number(fvalue);
    break;
  }
  case GP_WIDGET_TOGGLE: {
    int ivalue;
    retVal                            = gp_widget_get_value(child, &ivalue);
    if (retVal >= GP_OK) currentValue = QString::number(ivalue);
    break;
  }
  case GP_WIDGET_DATE:
  case GP_WIDGET_BUTTON:
  case GP_WIDGET_SECTION:
  case GP_WIDGET_WINDOW:
  default:
    break;
  }

  gp_widget_free(widget);

  return currentValue;
}

//-----------------------------------------------------------------

bool GPhotoCam::setCameraConfigValue(const char *key, QString value) {
  CameraWidget *widget = NULL, *child = NULL;
  CameraWidgetType widgetType;
  int retVal;

  if (!key) return false;

  retVal = gp_camera_get_single_config(m_camera, key, &child, m_gpContext);
  if (retVal == GP_OK)
    widget = child;
  else {
    retVal = gp_camera_get_config(m_camera, &widget, m_gpContext);
    if (retVal < GP_OK) return false;

    retVal = gp_widget_get_child_by_name(widget, key, &child);
    if (retVal < GP_OK)
      retVal = gp_widget_get_child_by_label(widget, key, &child);
    if (retVal < GP_OK) {
      gp_widget_free(widget);
      return false;
    }
  }

  retVal = gp_widget_get_type(child, &widgetType);
  if (retVal < GP_OK) {
    gp_widget_free(widget);
    return false;
  }

  switch (widgetType) {
  case GP_WIDGET_MENU:
  case GP_WIDGET_RADIO: {
    bool found  = false;
    int choices = gp_widget_count_choices(child);
    for (int i = 0; i < choices; i++) {
      const char *choice;
      retVal = gp_widget_get_choice(child, i, &choice);
      if (retVal < GP_OK) continue;
      if (choice != value) continue;
      found = true;
      break;
    }

    retVal = !found ? GP_ERROR
                    : gp_widget_set_value(child, value.toStdString().c_str());
    break;
  }
  case GP_WIDGET_TEXT:
    retVal = gp_widget_set_value(child, value.toStdString().c_str());
    break;
  case GP_WIDGET_RANGE: {
    float fvalue = value.toFloat();
    retVal       = gp_widget_set_value(child, &fvalue);
    break;
  }
  case GP_WIDGET_TOGGLE: {
    int ivalue = value.toInt();
    retVal     = gp_widget_set_value(child, &ivalue);
    break;
  }
  case GP_WIDGET_DATE:
  case GP_WIDGET_BUTTON:
  case GP_WIDGET_SECTION:
  case GP_WIDGET_WINDOW:
  default:
    retVal = GP_ERROR;
    break;
  }

  if (retVal >= GP_OK)
    retVal = gp_camera_set_single_config(m_camera, key, child, m_gpContext);

  gp_widget_free(widget);

  return (retVal >= GP_OK);
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableApertures() {
  m_apertureOptions.clear();

  m_apertureOptions = getCameraConfigList(m_configKeys.apertureKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableShutterSpeeds() {
  m_shutterSpeedOptions.clear();

  m_shutterSpeedOptions = getCameraConfigList(m_configKeys.shutterSpeedKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableIso() {
  m_isoOptions.clear();

  m_isoOptions = getCameraConfigList(m_configKeys.isoKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableExposureCompensations() {
  m_exposureOptions.clear();

  m_exposureOptions = getCameraConfigList(m_configKeys.exposureCompensationKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableWhiteBalances() {
  m_whiteBalanceOptions.clear();

  m_whiteBalanceOptions = getCameraConfigList(m_configKeys.whiteBalanceKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableImageQualities() {
  m_imageQualityOptions.clear();

  m_imageQualityOptions = getCameraConfigList(m_configKeys.imageQualityKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableImageSizes() {
  m_imageSizeOptions.clear();

  m_imageSizeOptions = getCameraConfigList(m_configKeys.imageSizeKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailablePictureStyles() {
  m_pictureStyleOptions.clear();

  m_pictureStyleOptions = getCameraConfigList(m_configKeys.pictureStyleKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getAvailableColorTemperatures() {
  m_colorTempOptions.clear();

  m_colorTempOptions = getCameraConfigList(m_configKeys.colorTemperatureKey);

  return true;
}

//-----------------------------------------------------------------

bool GPhotoCam::getManualFocusRangeData() {
  m_manualFocusRange.clear();

  m_manualFocusRange = getCameraConfigList(m_configKeys.manualFocusDriveKey);

  return true;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentAperture() {
  QString currentValue = getCameraConfigValue(m_configKeys.apertureKey);

  m_displayedAperture = currentValue;

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentShutterSpeed() {
  QString currentValue = getCameraConfigValue(m_configKeys.shutterSpeedKey);

  m_realShutterSpeed = currentValue;

  if (m_liveViewExposureOffset == 0) {
    m_displayedShutterSpeed = m_realShutterSpeed;
    currentValue            = m_realShutterSpeed;
  } else
    currentValue = m_displayedShutterSpeed;

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentIso() {
  QString currentValue = getCameraConfigValue(m_configKeys.isoKey);

  m_displayedIso = currentValue;

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentExposureCompensation() {
  QString currentValue =
      getCameraConfigValue(m_configKeys.exposureCompensationKey);

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentWhiteBalance() {
  QString currentValue = getCameraConfigValue(m_configKeys.whiteBalanceKey);

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentColorTemperature() {
  QString currentValue = getCameraConfigValue(m_configKeys.colorTemperatureKey);

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentImageQuality() {
  QString currentValue = getCameraConfigValue(m_configKeys.imageQualityKey);

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentImageSize() {
  QString currentValue = getCameraConfigValue(m_configKeys.imageSizeKey);

  return currentValue;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentPictureStyle() {
  QString currentValue = getCameraConfigValue(m_configKeys.pictureStyleKey);

  return currentValue;
}

//-----------------------------------------------------------------

int GPhotoCam::getCurrentLiveViewOffset() { return m_liveViewExposureOffset; }

//-----------------------------------------------------------------

QString GPhotoCam::getMode() {
  QString value = getCameraConfigValue(m_configKeys.modeKey);

  return value;
}

//-----------------------------------------------------------------

QString GPhotoCam::getCurrentBatteryLevel() {
  QString value = getCameraConfigValue(m_configKeys.batteryLevelKey);

  return value;
}

//-----------------------------------------------------------------

bool GPhotoCam::setAperture(QString aperture) {
  bool status = setCameraConfigValue(m_configKeys.apertureKey, aperture);

  if (status) m_displayedAperture = aperture;

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setShutterSpeed(QString shutterSpeed, bool withOffset) {
  if (withOffset) {
    m_realShutterSpeed      = shutterSpeed;
    m_displayedShutterSpeed = shutterSpeed;
    if (m_liveViewExposureOffset != 0) {
      int index = m_shutterSpeedOptions.indexOf(shutterSpeed);
      // brighter goes down in shutter speed, so subtract

      if (m_liveViewExposureOffset > 0) {
        int newValue = index - m_liveViewExposureOffset;
        // get brighter, go down
        if (newValue >= 0) {
          m_realShutterSpeed = m_shutterSpeedOptions.at(newValue);
        } else
          m_realShutterSpeed = m_shutterSpeedOptions.at(0);
      } else {
        // get darker, go up
        int newValue = index - m_liveViewExposureOffset;
        if (newValue < m_shutterSpeedOptions.size()) {
          m_realShutterSpeed = m_shutterSpeedOptions.at(newValue);
        } else
          m_realShutterSpeed =
              m_shutterSpeedOptions.at(m_shutterSpeedOptions.size() - 1);
      }
    }
  }

  QString usedSpeed = withOffset ? m_realShutterSpeed : shutterSpeed;

  bool status = setCameraConfigValue(m_configKeys.shutterSpeedKey, usedSpeed);

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setIso(QString iso) {
  bool status                = setCameraConfigValue(m_configKeys.isoKey, iso);
  if (status) m_displayedIso = iso;

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setExposureCompensation(QString exposure) {
  bool status =
      setCameraConfigValue(m_configKeys.exposureCompensationKey, exposure);

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setWhiteBalance(QString whiteBalance) {
  bool status =
      setCameraConfigValue(m_configKeys.whiteBalanceKey, whiteBalance);

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setColorTemperature(QString colorTemp) {
  bool status =
      setCameraConfigValue(m_configKeys.colorTemperatureKey, colorTemp);

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setImageQuality(QString imageQuality) {
  bool status =
      setCameraConfigValue(m_configKeys.imageQualityKey, imageQuality);

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setImageSize(QString imageSize) {
  bool status = setCameraConfigValue(m_configKeys.imageSizeKey, imageSize);

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setPictureStyle(QString pictureStyle) {
  bool status =
      setCameraConfigValue(m_configKeys.pictureStyleKey, pictureStyle);

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::setManualFocus(int value){
  QString strvalue = QString::number(value);

  bool status =
      setCameraConfigValue(m_configKeys.manualFocusDriveKey, strvalue);

  return status;
}

//-----------------------------------------------------------------

void GPhotoCam::setLiveViewOffset(int compensation) {
  m_liveViewExposureOffset = compensation;
  emit(liveViewOffsetChangedSignal(m_liveViewExposureOffset));
}

//-----------------------------------------------------------------

bool GPhotoCam::zoomLiveView() {
  if (!m_sessionOpen || !StopMotion::instance()->m_liveViewStatus > 0)
    return false;

  if (m_pickLiveViewZoom) toggleZoomPicking();

  if (m_liveViewZoom == 1) {
    m_liveViewZoom = 5;
    m_zooming      = true;
    StopMotion::instance()->toggleNumpadForFocusCheck(true);
  } else if (m_liveViewZoom == 5) {
    m_liveViewZoom = 1;
    m_zooming      = false;
    StopMotion::instance()->toggleNumpadForFocusCheck(false);
  }

  bool status =
      setCameraConfigValue("eoszoom", QString::number(m_liveViewZoom));
  if (status) {
    if (m_liveViewZoom == 5) setZoomPoint();
    emit(focusCheckToggled(m_liveViewZoom > 1));
  }

  return status;
}

//-----------------------------------------------------------------

void GPhotoCam::calculateZoomPoint() {
  double minimumDpi = 0.0;

  if (m_proxyImageDimensions == TDimension(0, 0)) {
    TCamera *camera =
        TApp::instance()->getCurrentScene()->getScene()->getCurrentCamera();
    TDimensionD size = camera->getSize();
    minimumDpi       = std::min(m_fullImageDimensions.lx / size.lx,
                          m_fullImageDimensions.ly / size.ly);
  } else {
    TDimensionD size =
        TDimensionD((double)m_proxyImageDimensions.lx / Stage::standardDpi,
                    (double)m_proxyImageDimensions.ly / Stage::standardDpi);
    minimumDpi = std::min(m_fullImageDimensions.lx / size.lx,
                          m_fullImageDimensions.ly / size.ly);
  }
  TPointD fullImageDpi = TPointD(minimumDpi, minimumDpi);

  bool outOfBounds = false;
  if (m_liveViewZoomPickPoint == TPointD(0.0, 0.0)) {
    m_calculatedZoomPoint =
        TPoint(m_fullImageDimensions.lx / 2, m_fullImageDimensions.ly / 2);
    m_finalZoomPoint.x = m_calculatedZoomPoint.x - (m_zoomRectDimensions.x / 2);
    m_finalZoomPoint.y = m_calculatedZoomPoint.y - (m_zoomRectDimensions.y / 2);
  } else {
    // get the image size in Tahoma2D dimensions
    double maxFullWidth =
        (double)m_fullImageDimensions.lx / fullImageDpi.x * Stage::inch;
    double maxFullHeight =
        (double)m_fullImageDimensions.ly / fullImageDpi.y * Stage::inch;
    // Tahoma2D coordinates are based on center at 0, 0
    // convert that to top left based coordinates
    double newX = m_liveViewZoomPickPoint.x + maxFullWidth / 2.0;
    double newY = -m_liveViewZoomPickPoint.y + maxFullHeight / 2.0;
    // convert back to the normal image dimensions to talk to the camera
    m_calculatedZoomPoint.x = newX / Stage::inch * fullImageDpi.x;
    m_calculatedZoomPoint.y = newY / Stage::inch * fullImageDpi.x;
    // the Canon SDK wants the top left corner of the zoom rect, not the center
    m_finalZoomPoint.x = m_calculatedZoomPoint.x - (m_zoomRectDimensions.x / 2);
    m_finalZoomPoint.y = m_calculatedZoomPoint.y - (m_zoomRectDimensions.y / 2);
    // finally make sure everything actually fits on the image
    if (m_finalZoomPoint.x < 0) {
      m_finalZoomPoint.x = 0;
      outOfBounds        = true;
    }
    if (m_finalZoomPoint.y < 0) {
      m_finalZoomPoint.y = 0;
      outOfBounds        = true;
    }
    if (m_finalZoomPoint.x >
        m_fullImageDimensions.lx - (m_zoomRectDimensions.x)) {
      m_finalZoomPoint.x = m_fullImageDimensions.lx - (m_zoomRectDimensions.x);
      outOfBounds        = true;
    }
    if (m_finalZoomPoint.y >
        m_fullImageDimensions.ly - (m_zoomRectDimensions.y)) {
      m_finalZoomPoint.y = m_fullImageDimensions.ly - (m_zoomRectDimensions.y);
      outOfBounds        = true;
    }

    // if out of bounds, recalculate backwards to get the correct location
    if (outOfBounds) {
      TPoint tempCalculated;
      // recenter the point in the rect
      tempCalculated.x = m_finalZoomPoint.x + (m_zoomRectDimensions.x / 2);
      tempCalculated.y = m_finalZoomPoint.y + (m_zoomRectDimensions.y / 2);
      // convert to Tahoma2D Dimensions
      newX = tempCalculated.x / fullImageDpi.x * Stage::inch;
      newY = tempCalculated.y / fullImageDpi.y * Stage::inch;
      // get center based coordinates
      m_liveViewZoomPickPoint.x = newX - (maxFullWidth / 2.0);
      m_liveViewZoomPickPoint.y = (newY - (maxFullHeight / 2.0)) * -1;
    }
  }

  // calculate the zoom rectangle position on screen

  // get the image size in Tahoma2D dimensions
  double maxFullWidth =
      (double)m_fullImageDimensions.lx / fullImageDpi.x * Stage::inch;
  double maxFullHeight =
      (double)m_fullImageDimensions.ly / fullImageDpi.y * Stage::inch;
  m_zoomRect =
      TRect(m_finalZoomPoint.x, m_finalZoomPoint.y + m_zoomRectDimensions.y,
            m_finalZoomPoint.x + m_zoomRectDimensions.x, m_finalZoomPoint.y);
  TRect tempCalculated;

  // convert to Tahoma2D Dimensions
  tempCalculated.x0 = m_zoomRect.x0 / fullImageDpi.x * Stage::inch;
  tempCalculated.y0 = m_zoomRect.y0 / fullImageDpi.y * Stage::inch;
  tempCalculated.x1 = m_zoomRect.x1 / fullImageDpi.x * Stage::inch;
  tempCalculated.y1 = m_zoomRect.y1 / fullImageDpi.y * Stage::inch;
  // get center based coordinates
  m_zoomRect.x0 = tempCalculated.x0 - (maxFullWidth / 2.0);
  m_zoomRect.y0 = (tempCalculated.y0 - (maxFullHeight / 2.0)) * -1;
  m_zoomRect.x1 = tempCalculated.x1 - (maxFullWidth / 2.0);
  m_zoomRect.y1 = (tempCalculated.y1 - (maxFullHeight / 2.0)) * -1;
}

//-----------------------------------------------------------------

void GPhotoCam::makeZoomPoint(TPointD pos) {
  m_liveViewZoomPickPoint = pos;
  calculateZoomPoint();
}

//-----------------------------------------------------------------

void GPhotoCam::toggleZoomPicking() {
  if (m_sessionOpen && StopMotion::instance()->m_liveViewStatus > 0 &&
      !m_zooming) {
    if (m_pickLiveViewZoom) {
      m_pickLiveViewZoom = false;
      StopMotion::instance()->toggleNumpadForFocusCheck(false);
    } else {
      m_pickLiveViewZoom = true;
      if (m_liveViewZoomPickPoint == TPointD(0.0, 0.0)) {
        makeZoomPoint(m_liveViewZoomPickPoint);
      }
      StopMotion::instance()->toggleNumpadForFocusCheck(true);
    }
  }
  emit(pickFocusCheckToggled(m_pickLiveViewZoom));
}

//-----------------------------------------------------------------

bool GPhotoCam::setZoomPoint() {
  // make sure this is set AFTER starting zoom
  int retVal;

  m_liveViewZoomReadyToPick = false;

  calculateZoomPoint();

  TPoint zoomPoint;
  zoomPoint.x = m_finalZoomPoint.x;
  zoomPoint.y = m_finalZoomPoint.y;

  QString zoomPointVal = QString("%1,%2").arg(zoomPoint.x).arg(zoomPoint.y);

  bool status = setCameraConfigValue(m_configKeys.afPositionKey, zoomPointVal);

  m_liveViewZoomReadyToPick = true;

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::focusNear() {
  bool status =
      setCameraConfigValue(m_configKeys.manualFocusDriveKey, "Near 1");

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::focusFar() {
  bool status = setCameraConfigValue(m_configKeys.manualFocusDriveKey, "Far 1");

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::focusNear2() {
  bool status =
      setCameraConfigValue(m_configKeys.manualFocusDriveKey, "Near 2");

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::focusFar2() {
  bool status = setCameraConfigValue(m_configKeys.manualFocusDriveKey, "Far 2");

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::focusNear3() {
  bool status =
      setCameraConfigValue(m_configKeys.manualFocusDriveKey, "Near 3");

  return status;
}

//-----------------------------------------------------------------

bool GPhotoCam::focusFar3() {
  bool status = setCameraConfigValue(m_configKeys.manualFocusDriveKey, "Far 3");

  return status;
}
#endif
