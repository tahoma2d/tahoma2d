#pragma once

#ifndef STOPMOTIONLIGHT_H
#define STOPMOTIONLIGHT_H

#include <QObject>
#include <QPixmap>
#include "traster.h"

class QDialog;
class QLabel;
class QHBoxLayout;

//=============================================================================
// StopMotionLight
//-----------------------------------------------------------------------------

class StopMotionLight : public QObject {
  Q_OBJECT

public:
  StopMotionLight();
  ~StopMotionLight();

  QDialog *m_fullScreen1, *m_fullScreen2, *m_fullScreen3;
  QLabel *m_label1, *m_label2, *m_label3;
  QHBoxLayout *m_layout1, *m_layout2, *m_layout3;
  bool m_useScreen1Overlay = false;
  bool m_useScreen2Overlay = false;
  bool m_useScreen3Overlay = false;
  bool m_showSceneOn1      = false;
  bool m_showSceneOn2      = false;
  bool m_showSceneOn3      = false;
  bool m_blackCapture      = true;
  bool m_overlaysReady     = false;
  bool m_shown             = false;
  int m_screenCount        = 1;
  bool m_screensSameSizes  = true;
  TPixel32 m_screen1Color, m_screen2Color,
      m_screen3Color = TPixel32(0, 0, 0, 255);
  std::vector<TDimension> m_screenSizes;
  QPixmap m_scenePixmap;
  int m_pixmapFrame = -1;

  void setBlackCapture(bool on);
  bool getBlackCapture() { return m_blackCapture; }
  void setScreen1Color(TPixel32 color);
  void setScreen2Color(TPixel32 color);
  void setScreen3Color(TPixel32 color);
  void setScreen1UseOverlay(bool on);
  void setScreen2UseOverlay(bool on);
  void setScreen3UseOverlay(bool on);
  void setShowSceneOn1(bool on);
  void setShowSceneOn2(bool on);
  void setShowSceneOn3(bool on);
  void showOverlays();
  void hideOverlays();
  bool useOverlays();
  QPixmap getSceneImage(TDimension size);

signals:
  void blackCaptureSignal(bool);
  void screen1ColorChanged(TPixel32);
  void screen2ColorChanged(TPixel32);
  void screen3ColorChanged(TPixel32);
  void screen1OverlayChanged(bool);
  void screen2OverlayChanged(bool);
  void screen3OverlayChanged(bool);
  void showSceneOn1Changed(bool);
  void showSceneOn2Changed(bool);
  void showSceneOn3Changed(bool);
};
#endif  // STOPMOTIONLIGHT_H