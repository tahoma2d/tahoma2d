#include "webcam.h"
#include "tenv.h"

#ifdef WIN32
#include <Windows.h>
#include <mfobjects.h>
#include <mfapi.h>
#include <mfidl.h>
#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

#include <QCamera>
#include <QCameraInfo>
#include <QApplication>

#ifdef Q_OS_MAC
#import <AVFoundation/AVFoundation.h>
#endif

TEnv::IntVar StopMotionUseDirectShow("StopMotionUseDirectShow", 1);
TEnv::IntVar StopMotionUseMjpg("StopMotionUseMjpg", 1);
//-----------------------------------------------------------------------------

Webcam::Webcam() {
  m_useDirectShow = StopMotionUseDirectShow;
  m_useMjpg       = StopMotionUseMjpg;
  m_lut           = cv::Mat(1, 256, CV_8U);
}

//-----------------------------------------------------------------

Webcam::~Webcam() {}

//-----------------------------------------------------------------

QList<QCameraInfo> Webcam::getWebcams() {
  m_webcams.clear();
  m_webcams = QCameraInfo::availableCameras();
  return m_webcams;
}

//-----------------------------------------------------------------

void Webcam::setWebcam(QCamera* camera) { m_webcam = camera; }

//-----------------------------------------------------------------

bool Webcam::initWebcam(int index) {
#ifdef WIN32
  if (!m_useDirectShow) {
    // m_webcamIndex already resolved by translateIndex() at camera selection
    m_cvWebcam.open(m_webcamIndex);
  } else {
    m_webcamIndex = index;
    m_cvWebcam.open(m_webcamIndex, cv::CAP_DSHOW);
  }
  if (m_cvWebcam.isOpened() == false) {
    return false;
  }
#elif defined(Q_OS_MAC)
  // m_webcamIndex is resolved once by translateIndex() at camera selection time.
  // Do NOT re-enumerate AVFoundation here — device order changes dynamically.
  m_cvWebcam.open(m_webcamIndex, cv::CAP_AVFOUNDATION);
  if (m_cvWebcam.isOpened() == false) {
    return false;
  }
#else
  m_webcamIndex = index;
  m_cvWebcam.open(index);
  if (m_cvWebcam.isOpened() == false) {
    return false;
  }
#endif
}

//-----------------------------------------------------------------

void Webcam::releaseWebcam() { m_cvWebcam.release(); }

//-----------------------------------------------------------------

int Webcam::getIndexOfResolution() {
  return m_webcamResolutions.indexOf(QSize(m_webcamWidth, m_webcamHeight));
}

//-----------------------------------------------------------------

bool Webcam::getWebcamImage(TRaster32P& tempImage) {
  bool error = false;
  cv::Mat imgOriginal;
  cv::Mat imgCorrected;

  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    // mjpg is used by many webcams
    // opencv runs very slow on some webcams without it.
    if (m_useMjpg) {
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('m', 'j', 'p', 'g'));
      m_cvWebcam.set(cv::CAP_PROP_FOURCC,
                     cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }
    m_cvWebcam.set(3, m_webcamWidth);
    m_cvWebcam.set(4, m_webcamHeight);

    if (!m_cvWebcam.isOpened()) {
      error = true;
    }
  }

  bool blnFrameReadSuccessfully =
      m_cvWebcam.read(imgOriginal);  // get next frame

  if (!blnFrameReadSuccessfully ||
      imgOriginal.empty()) {  // if frame not read successfully
    std::cout << "error: frame not read from webcam\n";
    error = true;  // print error message to std out
  }

  if (!error) {
    cv::cvtColor(imgOriginal, imgCorrected, cv::COLOR_BGR2BGRA);
    cv::flip(imgCorrected, imgCorrected, 0);
    emit updateHistogram(imgCorrected);

    bool convertBack = false;
    if (m_colorMode != 0) {
      cv::cvtColor(imgCorrected, imgCorrected, cv::COLOR_RGB2GRAY);
      convertBack = true;
    }

    // color and grayscale mode
    if (m_colorMode != 2)
      adjustLevel(imgCorrected);
    else
      binarize(imgCorrected);

    if (convertBack) {
      cv::cvtColor(imgCorrected, imgCorrected, cv::COLOR_GRAY2BGRA);
    }

    // perform calibration
    if (m_useCalibration) {
      cv::remap(imgCorrected, imgCorrected, m_calibrationMapX,
                m_calibrationMapY, cv::INTER_LINEAR);
    }

    m_webcamImage = imgCorrected;

    int width  = m_cvWebcam.get(3);
    int height = m_cvWebcam.get(4);
    int size   = imgCorrected.total() * imgCorrected.elemSize();

    tempImage = TRaster32P(width, height);
    // m_liveViewImage = TRaster32P(width, height);
    tempImage->lock();
    uchar* imgBuf  = imgCorrected.data;
    uchar* rawData = tempImage->getRawData();
    memcpy(rawData, imgBuf, size);
    tempImage->unlock();
  }
  if (error) {
    return false;
  } else {
    return true;
  }
}

//-----------------------------------------------------------------

void Webcam::setUseDirectShow(int state) {
  m_useDirectShow         = state;
  StopMotionUseDirectShow = state;
  emit(useDirectShowSignal(state));
}

//-----------------------------------------------------------------

void Webcam::setUseMjpg(bool on) {
  m_useMjpg         = on;
  StopMotionUseMjpg = int(on);
  emit(useMjpgSignal(on));
}

//-----------------------------------------------------------------

void Webcam::clearWebcam() {
  m_webcamDescription = QString();
  m_webcamDeviceName  = QString();
  m_webcamIndex       = -1;
}

//-----------------------------------------------------------------

void Webcam::clearWebcamResolutions() { m_webcamResolutions.clear(); }

//-----------------------------------------------------------------

void Webcam::refreshWebcamResolutions() {
  clearWebcamResolutions();
  m_webcamResolutions = getWebcam()->supportedViewfinderResolutions();
  if (m_webcamResolutions.size() == 0) {
    // Qt could not enumerate resolutions (common with virtual cameras like
    // Iriun, Insta360, NDI). Try querying OpenCV directly.
#ifdef Q_OS_MAC
    if (m_cvWebcam.isOpened()) {
      // Probe common resolutions via OpenCV
      QList<QSize> probeList = {
        QSize(3840, 2160), QSize(1920, 1080), QSize(1280, 720),
        QSize(960, 540),   QSize(640, 480),   QSize(640, 360)
      };
      double origW = m_cvWebcam.get(cv::CAP_PROP_FRAME_WIDTH);
      double origH = m_cvWebcam.get(cv::CAP_PROP_FRAME_HEIGHT);
      for (const QSize& s : probeList) {
        m_cvWebcam.set(cv::CAP_PROP_FRAME_WIDTH,  s.width());
        m_cvWebcam.set(cv::CAP_PROP_FRAME_HEIGHT, s.height());
        double w = m_cvWebcam.get(cv::CAP_PROP_FRAME_WIDTH);
        double h = m_cvWebcam.get(cv::CAP_PROP_FRAME_HEIGHT);
        QSize actual((int)w, (int)h);
        if (!m_webcamResolutions.contains(actual))
          m_webcamResolutions.push_back(actual);
      }
      // Restore original resolution
      m_cvWebcam.set(cv::CAP_PROP_FRAME_WIDTH,  origW);
      m_cvWebcam.set(cv::CAP_PROP_FRAME_HEIGHT, origH);
    }
    if (m_webcamResolutions.size() == 0) {
#endif
    // Final fallback: standard resolution list
    m_webcamResolutions.push_back(QSize(640, 360));
    m_webcamResolutions.push_back(QSize(640, 480));
    m_webcamResolutions.push_back(QSize(800, 448));
    m_webcamResolutions.push_back(QSize(800, 600));
    m_webcamResolutions.push_back(QSize(848, 480));
    m_webcamResolutions.push_back(QSize(864, 480));
    m_webcamResolutions.push_back(QSize(960, 540));
    m_webcamResolutions.push_back(QSize(960, 720));
    m_webcamResolutions.push_back(QSize(1024, 576));
    m_webcamResolutions.push_back(QSize(1280, 720));
    m_webcamResolutions.push_back(QSize(1600, 896));
    m_webcamResolutions.push_back(QSize(1600, 900));
    m_webcamResolutions.push_back(QSize(1920, 1080));
    m_webcamResolutions.push_back(QSize(3840, 2160));
#ifdef Q_OS_MAC
    }
#endif
  }
}

//-----------------------------------------------------------------

bool Webcam::getWebcamAutofocusStatus() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return false;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_AUTOFOCUS);
    if (value > 0.0) {
      return true;
    } else {
      return false;
    }
  }
  return false;
}

//-----------------------------------------------------------------
void Webcam::setWebcamAutofocusStatus(bool on) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = on ? 1.0 : 0.0;
    m_cvWebcam.set(cv::CAP_PROP_AUTOFOCUS, value);
    value = m_cvWebcam.get(cv::CAP_PROP_AUTOFOCUS);
  }
}

//-----------------------------------------------------------------
int Webcam::getWebcamFocusValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_FOCUS);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamFocusValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_FOCUS, value);
  value = m_cvWebcam.get(cv::CAP_PROP_FOCUS);
}

//-----------------------------------------------------------------
int Webcam::getWebcamExposureValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_EXPOSURE);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamExposureValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);
  m_cvWebcam.set(cv::CAP_PROP_EXPOSURE, value);
  value = m_cvWebcam.get(cv::CAP_PROP_EXPOSURE);
  getWebcamExposureValue();
}

//-----------------------------------------------------------------
int Webcam::getWebcamBrightnessValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_BRIGHTNESS);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamBrightnessValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_BRIGHTNESS, value);
  value = m_cvWebcam.get(cv::CAP_PROP_BRIGHTNESS);
}

//-----------------------------------------------------------------
int Webcam::getWebcamContrastValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_CONTRAST);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamContrastValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_CONTRAST, value);
  value = m_cvWebcam.get(cv::CAP_PROP_CONTRAST);
}

//-----------------------------------------------------------------
int Webcam::getWebcamGainValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_GAIN);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamGainValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_GAIN, value);
  value = m_cvWebcam.get(cv::CAP_PROP_GAIN);
}

//-----------------------------------------------------------------
int Webcam::getWebcamSaturationValue() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return 0;
    }
  }
  if (m_cvWebcam.isOpened()) {
    double value = m_cvWebcam.get(cv::CAP_PROP_SATURATION);
    return static_cast<int>(value);
  }
  return 0.0;
}

//-----------------------------------------------------------------

void Webcam::setWebcamSaturationValue(int value) {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_SATURATION, value);
  value = m_cvWebcam.get(cv::CAP_PROP_SATURATION);
}

//-----------------------------------------------------------------

void Webcam::openSettingsWindow() {
  if (m_cvWebcam.isOpened() == false) {
    initWebcam(m_webcamIndex);

    if (!m_cvWebcam.isOpened()) {
      return;
    }
  }
  m_cvWebcam.set(cv::CAP_PROP_SETTINGS, 1.0);
}

//-----------------------------------------------------------------

void Webcam::adjustLevel(cv::Mat& image) {
  if (m_black == 0 && m_white == 255 && m_gamma == 1.0) return;

  cv::LUT(image, m_lut, image);
}

//-----------------------------------------------------------------

void Webcam::binarize(cv::Mat& image) {
  cv::threshold(image, image, m_threshold, 255, cv::THRESH_BINARY);
}

//-----------------------------------------------------------------

void Webcam::computeLut() {
  const float maxChannelValueF = 255.0f;

  float value;

  uchar* p = m_lut.data;
  for (int i = 0; i < 256; i++) {
    if (i <= m_black)
      value = 0.0f;
    else if (i >= m_white)
      value = 1.0f;
    else {
      value = (float)(i - m_black) / (float)(m_white - m_black);
      value = std::pow(value, 1.0f / m_gamma);
    }

    p[i] = (uchar)std::floor(value * maxChannelValueF);
  }
}

//-----------------------------------------------------------------

bool Webcam::translateIndex(int index) {
  // We are using Qt to get the camera info and supported resolutions, but
  // we are using OpenCV to actually get the images.
  // The camera index from OpenCV and from Qt don't always agree,
  // So this checks the name against the correct index.
  m_webcamIndex = index;

#ifdef Q_OS_MAC
  // On macOS, use AVFoundation uniqueID (stable hardware identifier) to find
  // the correct OpenCV index, regardless of enumeration order.
  @autoreleasepool {
    NSArray<AVCaptureDevice*>* devices = nil;
    if (@available(macOS 10.15, *)) {
      AVCaptureDeviceDiscoverySession* session =
        [AVCaptureDeviceDiscoverySession
          discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera,
                                            AVCaptureDeviceTypeExternalUnknown]
          mediaType:AVMediaTypeVideo
          position:AVCaptureDevicePositionUnspecified];
      devices = session.devices;
    } else {
      devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    }

    // Match by uniqueID first (stable, not affected by enumeration order)
    NSString* targetUID =
      [NSString stringWithUTF8String:m_webcamDeviceName.toUtf8().constData()];
    fprintf(stderr, "[Ztoryc] deviceName(Qt)=%s desc=%s\n", m_webcamDeviceName.toUtf8().constData(), m_webcamDescription.toUtf8().constData());
    for (NSUInteger i = 0; i < devices.count; i++) {
      fprintf(stderr, "[Ztoryc] AVF device[%lu] uid=%s name=%s\n", (unsigned long)i, [devices[i].uniqueID UTF8String], [devices[i].localizedName UTF8String]);
      if ([devices[i].uniqueID isEqualToString:targetUID]) {
        m_webcamIndex = (int)i;
        return true;
      }
    }

    // Fallback: match by localizedName
    NSString* targetName =
      [NSString stringWithUTF8String:m_webcamDescription.toUtf8().constData()];
    for (NSUInteger i = 0; i < devices.count; i++) {
      if ([devices[i].localizedName isEqualToString:targetName]) {
        m_webcamIndex = (int)i;
        return true;
      }
    }

    // Last resort: use Qt index
    m_webcamIndex = index;
  }
  return true;

#elif defined(WIN32)

// Thanks to:
// https://elcharolin.wordpress.com/2017/08/28/webcam-capture-with-the-media-foundation-sdk/
// for the webcam enumeration here

#define CLEAN_ATTRIBUTES()                                                     \
  if (attributes) {                                                            \
    attributes->Release();                                                     \
    attributes = NULL;                                                         \
  }                                                                            \
  for (DWORD i = 0; i < count; i++) {                                          \
    if (&devices[i]) {                                                         \
      devices[i]->Release();                                                   \
      devices[i] = NULL;                                                       \
    }                                                                          \
  }                                                                            \
  CoTaskMemFree(devices);                                                      \
  return hr;

  HRESULT hr = S_OK;

  // this is important!!
  hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  UINT32 count              = 0;
  IMFAttributes* attributes = NULL;
  IMFActivate** devices     = NULL;

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Create an attribute store to specify enumeration parameters.
  hr = MFCreateAttributes(&attributes, 1);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }

  // The attribute to be requested is devices that can capture video
  hr = attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                           MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // Enummerate the video capture devices
  hr = MFEnumDeviceSources(attributes, &devices, &count);

  if (FAILED(hr)) {
    CLEAN_ATTRIBUTES()
  }
  // if there are any available devices
  if (count > 0) {
    WCHAR* nameString = NULL;
    // Get the human-friendly name of the device
    UINT32 cchName;

    for (int i = 0; i < count; i++) {
      hr = devices[i]->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
                                          &nameString, &cchName);
      std::string desc = m_webcamDescription.toStdString();
      if (nameString == m_webcamDescription.toStdWString()) {
        m_webcamIndex = i;
        break;
      }
      // devices[0]->ShutdownObject();
    }

    CoTaskMemFree(nameString);
  }
  // clean
  CLEAN_ATTRIBUTES()
#else
  return true;
#endif
}