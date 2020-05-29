#include "stopmotionlight.h"

#include "tenv.h"
#include "stopmotion.h"

#include <QDialog>
#include <QApplication>
#include <QDesktopWidget>
#include <QWindow>
#include <QPainter>
#include <QLabel>
#include <QScreen>

#include "tapp.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/toonzscene.h"
#include "toonz/tframehandle.h"

TEnv::IntVar StopMotionBlackCapture("StopMotionBlackCapture", 0);

//=============================================================================
//=============================================================================

StopMotionLight::StopMotionLight() {
  m_blackCapture = StopMotionBlackCapture;
  m_screenCount  = QApplication::desktop()->screenCount();

  for (int i = 0; i < m_screenCount; i++) {
    QScreen* screen      = QGuiApplication::screens().at(i);
    QRect screenGeometry = screen->geometry();
    int height           = screenGeometry.height();
    int width            = screenGeometry.width();
    m_screenSizes.push_back(TDimension(width, height));
  }

  for (int i = 1; i < m_screenSizes.size(); i++) {
    m_screensSameSizes =
        m_screensSameSizes && m_screenSizes[i] == m_screenSizes[i - 1];
  }

  m_fullScreen1 = new QDialog();
  m_fullScreen1->setModal(false);
  m_fullScreen1->setStyleSheet("background-color:black;");
  m_label1  = new QLabel();
  m_layout1 = new QHBoxLayout();
  m_layout1->addWidget(m_label1);
  m_layout1->setMargin(0);
  m_layout1->setSpacing(0);
  m_fullScreen1->setLayout(m_layout1);

  if (m_screenCount > 1) {
    m_fullScreen2 = new QDialog();
    m_fullScreen2->setModal(false);
    m_fullScreen2->setStyleSheet("background-color:black;");
    m_label2  = new QLabel();
    m_layout2 = new QHBoxLayout();
    m_layout2->addWidget(m_label2);
    m_layout2->setMargin(0);
    m_layout2->setSpacing(0);
    m_fullScreen2->setLayout(m_layout2);

    if (m_screenCount > 2) {
      m_fullScreen3 = new QDialog();
      m_fullScreen3->setModal(false);
      m_fullScreen3->setStyleSheet("background-color:black;");
      m_label3  = new QLabel();
      m_layout3 = new QHBoxLayout();
      m_layout3->addWidget(m_label3);
      m_layout3->setMargin(0);
      m_layout3->setSpacing(0);
      m_fullScreen3->setLayout(m_layout3);
    }
  }
}
StopMotionLight::~StopMotionLight() {}

//-----------------------------------------------------------------

void StopMotionLight::setBlackCapture(bool on) {
  m_blackCapture         = on;
  StopMotionBlackCapture = int(on);
  emit(blackCaptureSignal(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen1Color(TPixel32 color) {
  m_screen1Color = color;
  emit(screen1ColorChanged(color));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen2Color(TPixel32 color) {
  m_screen2Color = color;
  emit(screen2ColorChanged(color));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen3Color(TPixel32 color) {
  m_screen3Color = color;
  emit(screen3ColorChanged(color));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen1UseOverlay(bool on) {
  m_useScreen1Overlay = on;
  emit(screen1OverlayChanged(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen2UseOverlay(bool on) {
  m_useScreen2Overlay = on;
  emit(screen2OverlayChanged(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setScreen3UseOverlay(bool on) {
  m_useScreen3Overlay = on;
  emit(screen3OverlayChanged(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setShowSceneOn1(bool on) {
  m_showSceneOn1 = on;
  emit(showSceneOn1Changed(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setShowSceneOn2(bool on) {
  m_showSceneOn2 = on;
  emit(showSceneOn2Changed(on));
}

//-----------------------------------------------------------------

void StopMotionLight::setShowSceneOn3(bool on) {
  m_showSceneOn3 = on;
  emit(showSceneOn3Changed(on));
}

//-----------------------------------------------------------------

void StopMotionLight::showOverlays() {
  if (getBlackCapture()) {
    m_fullScreen1->setStyleSheet("background-color: rgb(0,0,0);");
    if (m_screenCount > 1) {
      m_fullScreen2->setStyleSheet("background-color: rgb(0,0,0);");
    }
    if (m_screenCount > 2) {
      m_fullScreen3->setStyleSheet("background-color: rgb(0,0,0);");
    }
  } else {
    QString style1 = QString("background-color: rgb(%1,%2,%3);")
                         .arg(m_screen1Color.r)
                         .arg(m_screen1Color.g)
                         .arg(m_screen1Color.b);
    m_fullScreen1->setStyleSheet(style1);

    if (m_screenCount > 1) {
      QString style2 = QString("background-color: rgb(%1,%2,%3);")
                           .arg(m_screen2Color.r)
                           .arg(m_screen2Color.g)
                           .arg(m_screen2Color.b);
      m_fullScreen2->setStyleSheet(style2);
    }
    if (m_screenCount > 2) {
      QString style3 = QString("background-color: rgb(%1,%2,%3);")
                           .arg(m_screen3Color.r)
                           .arg(m_screen3Color.g)
                           .arg(m_screen3Color.b);
      m_fullScreen3->setStyleSheet(style3);
    }
  }
  m_shown          = false;
  bool isTimeLapse = StopMotion::instance()->m_isTimeLapse;
  if ((getBlackCapture() || m_useScreen1Overlay) && !isTimeLapse) {
    if (m_showSceneOn1 && !m_blackCapture) {
      m_label1->setPixmap(getSceneImage(m_screenSizes[0]));
    } else {
      m_label1->clear();
    }
    m_fullScreen1->showFullScreen();
    m_fullScreen1->setGeometry(QApplication::desktop()->screenGeometry(0));
    m_shown = true;
  }
  if (m_screenCount > 1 && (getBlackCapture() || m_useScreen2Overlay) &&
      !isTimeLapse) {
    if (m_showSceneOn2 && !m_blackCapture) {
      m_label2->setPixmap(getSceneImage(m_screenSizes[1]));
    } else {
      m_label2->clear();
    }
    m_fullScreen2->showFullScreen();
    m_fullScreen2->setGeometry(QApplication::desktop()->screenGeometry(1));
    m_shown = true;
  }
  if (m_screenCount > 2 && (getBlackCapture() || m_useScreen3Overlay) &&
      !isTimeLapse) {
    if (m_showSceneOn3 && !m_blackCapture) {
      m_label3->setPixmap(getSceneImage(m_screenSizes[2]));
    } else {
      m_label3->clear();
    }
    m_fullScreen3->showFullScreen();
    m_fullScreen3->setGeometry(QApplication::desktop()->screenGeometry(2));
    m_shown = true;
  }

  if (m_shown) {
    // this allows the full screen qdialogs to go full screen before
    // taking a photo
    qApp->processEvents(QEventLoop::AllEvents, 1500);
  }
  m_overlaysReady = true;
}

//-----------------------------------------------------------------

void StopMotionLight::hideOverlays() {
  if (m_shown) {
    if ((getBlackCapture() || m_useScreen1Overlay)) {
#ifndef WIN32
      m_fullScreen1->showNormal();
#endif
      m_fullScreen1->close();
      m_fullScreen1->windowHandle()->close();
    }
    if (m_screenCount > 1 && (getBlackCapture() || m_useScreen2Overlay)) {
#ifndef WIN32
      m_fullScreen2->showNormal();
#endif
      m_fullScreen2->close();
      m_fullScreen2->windowHandle()->close();
    }
    if (m_screenCount > 2 && (getBlackCapture() || m_useScreen3Overlay)) {
#ifndef WIN32
      m_fullScreen3->showNormal();
#endif
      m_fullScreen3->close();
      m_fullScreen3->windowHandle()->close();
    }
  }
  m_overlaysReady = false;
}

//-----------------------------------------------------------------

bool StopMotionLight::useOverlays() {
  return m_blackCapture || m_useScreen1Overlay || m_useScreen2Overlay ||
         m_useScreen3Overlay;
}

//-----------------------------------------------------------------

QPixmap StopMotionLight::getSceneImage(TDimension size) {
  TXsheet* xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

  int frame              = StopMotion::instance()->getXSheetFrameNumber() - 1;
  if (frame == -1) frame = 0;

  bool isEmpty = true;
  int rowCount = xsh->getColumnCount();
  for (int i = 0; i < rowCount; i++) {
    isEmpty = isEmpty && xsh->getColumn(i)->isCellEmpty(frame);
  }

  if (isEmpty) {
    QPixmap pixmap;
    return pixmap;
  }

  if (frame == m_pixmapFrame && m_screensSameSizes) {
    return m_scenePixmap;
  }

  TRaster32P image(size);
  TApp::instance()->getCurrentScene()->getScene()->renderFrame(image, frame,
                                                               xsh);

  int m_lx       = image->getLx();
  int m_ly       = image->getLy();
  int m_bpp      = image->getPixelSize();
  int totalBytes = m_lx * m_ly * m_bpp;
  image->yMirror();

  // lock raster to get data
  image->lock();
  void* buffin = image->getRawData();
  assert(buffin);
  void* buffer = malloc(totalBytes);
  memcpy(buffer, buffin, totalBytes);

  image->unlock();

  QImage qi = QImage((uint8_t*)buffer, m_lx, m_ly, QImage::Format_ARGB32);
  QImage qi2(qi.size(), QImage::Format_ARGB32);
  qi2.fill(QColor(Qt::white).rgb());
  QPainter painter(&qi2);
  painter.drawImage(0, 0, qi);
  free(buffer);
  QHBoxLayout* layout = new QHBoxLayout();
  m_scenePixmap       = QPixmap::fromImage(qi2);
  m_pixmapFrame       = frame;
  return m_scenePixmap;
}