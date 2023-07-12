#pragma once

#ifndef EXPORTCAMERATRACKPOPUP_H
#define EXPORTCAMERATRACKPOPUP_H

// Tnz6 includes
#include "toonzqt/dvdialog.h"
#include <QScrollArea>
#include <QWidget>
#include <QSet>

namespace DVGui {
class FileField;
class ColorField;
class IntLineEdit;
class DoubleField;
}  // namespace DVGui

class QComboBox;
class QCheckBox;
class QFontComboBox;
class QLineEdit;

//*********************************************************************************
//    Export Camera Track Popup  declaration
//*********************************************************************************

struct ExportCameraTrackInfo {
  // target column
  int columnId = -1;
  // appearance settimgs
  double bgOpacity;
  QColor lineColor;
  // camera rect settings
  bool cameraRectOnKeys;
  bool cameraRectOnTags;
  QSet<int> cameraRectFrames;
  // track line settings
  bool lineTL, lineTR, lineCenter, lineBL, lineBR;
  int graduationInterval;
  // frame number settings
  Qt::Corner numberAt;
  bool numbersOnLine;
  QFont font;
};

//-----------------------------------------------------------------------------

class CameraTrackPreviewPane final : public QWidget {
  Q_OBJECT
  QPixmap m_pixmap;
  double m_scaleFactor;

protected:
  void paintEvent(QPaintEvent* event) override;

public:
  CameraTrackPreviewPane(QWidget* parent);
  void setPixmap(QPixmap pm);
  void doZoom(double d_scale);
  void fitScaleTo(QSize size);
};

class CameraTrackPreviewArea final : public QScrollArea {
  Q_OBJECT
  QPoint m_mousePos;

protected:
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

public:
  CameraTrackPreviewArea(QWidget* parent) : QScrollArea(parent) {}
protected slots:
  void fitToWindow();
};

//-----------------------------------------------------------------------------

class ExportCameraTrackPopup final : public DVGui::Dialog {
  Q_OBJECT
  CameraTrackPreviewPane* m_previewPane;
  CameraTrackPreviewArea* m_previewArea;

  QComboBox* m_targetColumnCombo;
  DVGui::DoubleField* m_bgOpacityField;
  DVGui::ColorField* m_lineColorFld;

  QCheckBox *m_cameraRectOnKeysCB, *m_cameraRectOnTagsCB;
  QLineEdit* m_cameraRectFramesEdit;

  QCheckBox *m_lineTL_CB, *m_lineTR_CB, *m_lineCenter_CB, *m_lineBL_CB,
      *m_lineBR_CB;
  QComboBox* m_graduationIntervalCombo;

  QComboBox* m_numberAtCombo;
  QCheckBox* m_numbersOnLineCB;
  QFontComboBox* m_fontCombo;
  DVGui::IntLineEdit* m_fontSizeEdit;

public:
  ExportCameraTrackPopup();

  // bool execute() override;

private:
  void initialize();
  void saveSettings();
  void loadSettings();
  void updateTargetColumnComboItems();

  QImage generateCameraTrackImg(const ExportCameraTrackInfo& info,
                                bool isPreview);
  void getInfoFromUI(ExportCameraTrackInfo& info);

protected:
  void showEvent(QShowEvent* event) override { initialize(); }
  void closeEvent(QCloseEvent* event) override { saveSettings(); }

private slots:
  void updatePreview();
  void onExport();
};

#endif  // EXPORTCAMERATRACKPOPUP_H