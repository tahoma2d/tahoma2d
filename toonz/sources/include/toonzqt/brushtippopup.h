#pragma once

#ifndef BRUSHTIPPOPUP_H
#define BRUSHTIPPOPUP_H

#include "tcommon.h"
#include "doublefield.h"
#include "checkbox.h"
#include "tfilepath.h"

#include "toonzqt/gutil.h"
#include "toonz/brushtipmanager.h"

#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================
// Brush Tip Chooser Page
//-----------------------------------------------------------------------------

class BrushTipChooserFrame final : public QFrame {
  Q_OBJECT

protected:
  QPoint m_chipOrigin;
  QSize m_chipSize;
  int m_chipPerRow;

  QColor m_commonChipBoxColor;
  QColor m_solidChipBoxColor;
  QColor m_selectedChipBoxColor;
  QColor m_selectedChipBox2Color;

  Q_PROPERTY(QColor CommonChipBoxColor READ getCommonChipBoxColor WRITE
                 setCommonChipBoxColor)
  Q_PROPERTY(QColor SolidChipBoxColor READ getSolidChipBoxColor WRITE
                 setSolidChipBoxColor)
  Q_PROPERTY(QColor SelectedChipBoxColor READ getSelectedChipBoxColor WRITE
                 setSelectedChipBoxColor)
  Q_PROPERTY(QColor SelectedChipBox2Color READ getSelectedChipBox2Color WRITE
                 setSelectedChipBox2Color)

  QColor getCommonChipBoxColor() const { return m_commonChipBoxColor; }
  QColor getSolidChipBoxColor() const { return m_solidChipBoxColor; }
  QColor getSelectedChipBoxColor() const { return m_selectedChipBoxColor; }
  QColor getSelectedChipBox2Color() const { return m_selectedChipBox2Color; }

  void setSolidChipBoxColor(const QColor &color) {
    m_solidChipBoxColor = color;
  }
  void setCommonChipBoxColor(const QColor &color) {
    m_commonChipBoxColor = color;
  }
  void setSelectedChipBoxColor(const QColor &color) {
    m_selectedChipBoxColor = color;
  }
  void setSelectedChipBox2Color(const QColor &color) {
    m_selectedChipBox2Color = color;
  }

public:
  BrushTipChooserFrame(QWidget *parent = 0)
      : QFrame(parent)
      , m_chipOrigin(5, 3)
      , m_chipPerRow(0)
      , m_currentIndex(0) {
    setFocusPolicy(Qt::NoFocus);

    // It is necessary for the style sheets
    setObjectName("BrushTipChooserFrame");
    setFrameStyle(QFrame::StyledPanel);
    setMouseTracking(true);

    m_chipSize = TBrushTipManager::instance()->getChipSize();
  }

  ~BrushTipChooserFrame() {}

  int getChipCount() const {
    int chipCount = TBrushTipManager::instance()->getBrushTipCount() + 1;
    if (!TBrushTipManager::instance()->getSearchText().isEmpty() &&
        chipCount == 1)
      return 0;
    return chipCount;
  }

  void setCurrentBrush(BrushTipData *brush);

  void applyFilter() { TBrushTipManager::instance()->applyFilter(); }
  void applyFilter(const QString text) {
    TBrushTipManager::instance()->applyFilter(text);
  }

  void onSelect(int index);

protected:
  int m_currentIndex;

  int posToIndex(const QPoint &pos) const;

  void paintEvent(QPaintEvent *) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  bool event(QEvent *e) override;
  void showEvent(QShowEvent *) override {
    connect(TBrushTipManager::instance(), SIGNAL(itemsUpdated()), this,
            SLOT(computeSize()));
    TBrushTipManager::instance()->loadItems();
  }

  void hideEvent(QHideEvent *) override {
    disconnect(TBrushTipManager::instance(), SIGNAL(itemsUpdated()), this,
               SLOT(computeSize()));
  }

public slots:
  void computeSize();

signals:
  void brushTipSelected(BrushTipData *);
};

//=============================================================================
//
// BrushTipPopup
//
//=============================================================================

class DVAPI BrushTipPopup final : public QWidget {
  Q_OBJECT

  BrushTipData *m_brushTip;
  DVGui::DoubleField *m_spacing, *m_rotation, *m_scatter;
  DVGui::CheckBox *m_autoRotate;
  DVGui::CheckBox *m_flipH;
  DVGui::CheckBox *m_flipV;

  QLabel *m_brushImage;
  QLabel *m_spacingLabel, *m_rotationLabel, *m_scatterLabel;
  QLabel *m_brushNameLabel;

  QScrollArea *m_brushTipArea;
  BrushTipChooserFrame *m_brushTipFrame;
  QFrame *m_brushTipSearchFrame;
  QLineEdit *m_brushTipSearchText;
  QPushButton *m_brushTipSearchClear;

  QTimer *m_keepClosedTimer;
  bool m_keepClosed;

public:
  BrushTipPopup(QWidget *parent);

  QFrame *createBrushTipChooser();
  void enableControls(bool enabled);

  QPixmap getBrushTipPixmap() {
    return QPixmap::fromImage(
        m_brushTip ? *m_brushTip->m_image
                   : *TBrushTipManager::instance()->getDefaultBrushTipImage());
  }
  QPixmap getDefaultBrushTipPixmap() {
    return QPixmap::fromImage(
        *TBrushTipManager::instance()->getDefaultBrushTipImage());
  }
  QString getBrushTipName() {
    return m_brushTip ? m_brushTip->m_brushTipName : QObject::tr("Round");
  }

  void setBrushTip(BrushTipData *brushTip) {
    m_brushTip = brushTip;
    m_brushImage->setPixmap(getBrushTipPixmap());
    m_brushNameLabel->setText(getBrushTipName());
    m_brushTipFrame->setCurrentBrush(brushTip);
    enableControls(m_brushTip);
  }
  BrushTipData *getBrushTip() { return m_brushTip; }

  void setSpacing(double spacing) { m_spacing->setValue(spacing); }
  double getSpacing() { return m_spacing->getValue(); }

  void setRotation(double rotation) { m_rotation->setValue(rotation); }
  double getRotation() { return m_rotation->getValue(); }

  void setAutoRotate(double autoRotate) {
    m_autoRotate->setChecked(autoRotate);
  }
  bool isAutoRotate() { return m_autoRotate->isChecked(); }

  void setFlipHorizontal(double flip) { m_flipH->setChecked(flip); }
  bool isFlipHorizontal() { return m_flipH->isChecked(); }

  void setFlipVertical(double flip) { m_flipV->setChecked(flip); }
  bool isFlipVertical() { return m_flipV->isChecked(); }

  void setScatter(double scatter) { m_scatter->setValue(scatter); }
  double getScatter() { return m_scatter->getValue(); }

  bool isKeepClosed() { return m_keepClosed; }
  void setKeepClosed(bool keepClosed) { m_keepClosed = keepClosed; }

protected:
  void mouseReleaseEvent(QMouseEvent *e) override;
  void hideEvent(QHideEvent *e) override;
  void showEvent(QShowEvent *) override;

protected slots:
  void resetKeepClosed();
  void onBrushTipSearch(const QString &);
  void onBrushTipClearSearch();
  void onBrushTipSelected(BrushTipData *);
  void onBrushTipPropertyChanged();

signals:
  void brushTipSelected(BrushTipData *);
  void brushTipPropertyChanged();
};

#endif  // BRUSHTIPPOPUP