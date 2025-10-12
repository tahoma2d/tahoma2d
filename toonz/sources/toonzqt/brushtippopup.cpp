#include "toonzqt/brushtippopup.h"

#include "toonz/toonzfolders.h"

#include "tsystem.h"

#include <QTimer>
#include <QGridLayout>
#include <QLabel>
#include <QScrollArea>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QDir>
#include <QToolTip>

//*****************************************************************************
//    BrushTipChooserFrame  implementation
//*****************************************************************************

void BrushTipChooserFrame::paintEvent(QPaintEvent *) {
  QPainter p(this);
  // p.setRenderHint(QPainter::SmoothPixmapTransform);
  bool origAA = p.testRenderHint(QPainter::Antialiasing);
  bool origS  = p.testRenderHint(QPainter::SmoothPixmapTransform);
  if (m_chipPerRow == 0 || getChipCount() == 0) return;

  int w      = parentWidget()->width();
  int chipLx = m_chipSize.width(), chipLy = m_chipSize.height();
  int nX = m_chipPerRow;
  int nY = (getChipCount() + m_chipPerRow - 1) / m_chipPerRow;
  int x0 = m_chipOrigin.x();
  int y0 = m_chipOrigin.y();
  int i, j;
  QRect currentIndexRect = QRect();
  int count              = 0;
  for (i = 0; i < nY; i++)
    for (j = 0; j < nX; j++) {
      QRect rect(x0 + chipLx * j + 2, y0 + chipLy * i + 2, chipLx, chipLy);

      if (count == 0) {
        p.drawImage(rect,
                    *TBrushTipManager::instance()->getDefaultBrushTipImage());
        p.setPen(m_solidChipBoxColor);
        p.drawRect(rect.adjusted(0, 0, -1, -1));
      } else {
        BrushTipData *brushTip =
            TBrushTipManager::instance()->getBrushTip(count - 1);

        if (brushTip && brushTip->m_image && !brushTip->m_image->isNull())
          p.drawImage(rect, *brushTip->m_image);
        p.setPen(m_commonChipBoxColor);
        p.drawRect(rect);
      }

      if (m_currentIndex == count) currentIndexRect = rect;

      count++;
      if (count >= getChipCount()) break;
    }

  if (!currentIndexRect.isEmpty()) {
    // Draw the curentIndex border
    p.setPen(m_selectedChipBoxColor);
    p.drawRect(currentIndexRect);
    p.setPen(m_selectedChipBox2Color);
    p.drawRect(currentIndexRect.adjusted(1, 1, -1, -1));
    p.setPen(m_selectedChipBoxColor);
    p.drawRect(currentIndexRect.adjusted(2, 2, -2, -2));
    p.setPen(m_commonChipBoxColor);
    p.drawRect(currentIndexRect.adjusted(3, 3, -3, -3));
  }
}

//-----------------------------------------------------------------------------

int BrushTipChooserFrame::posToIndex(const QPoint &pos) const {
  if (m_chipPerRow == 0) return -1;

  int x = (pos.x() - m_chipOrigin.x() - 2) / m_chipSize.width();
  if (x >= m_chipPerRow) return -1;

  int y = (pos.y() - m_chipOrigin.y() - 2) / m_chipSize.height();

  int index = x + m_chipPerRow * y;
  if (index < 0 || index >= getChipCount()) return -1;

  return index;
}

//-----------------------------------------------------------------------------

bool BrushTipChooserFrame::event(QEvent *e) {
  // Intercept tooltip events
  if (e->type() != QEvent::ToolTip) return QFrame::event(e);

  // see StyleChooserPage::paintEvent
  QHelpEvent *he = static_cast<QHelpEvent *>(e);

  int chipIdx   = posToIndex(he->pos());
  int chipCount = TBrushTipManager::instance()->getBrushTipCount();
  if (chipIdx == 0) {
    QToolTip::showText(he->globalPos(),
                       QObject::tr("Round", "BrushTipChooserFrame"));
  } else {
    chipIdx--;
    if (chipIdx < 0 || chipIdx >= chipCount) return false;

    BrushTipData *brushTip = TBrushTipManager::instance()->getBrushTip(chipIdx);
    if (brushTip) {
      QString tip = brushTip->m_brushTipName;
      if (!brushTip->m_brushTipFolderName.isEmpty())
        tip += " (" + brushTip->m_brushTipFolderName + ")";
      QToolTip::showText(he->globalPos(), tip);
    }
  }
  return true;
}

//-----------------------------------------------------------------------------

void BrushTipChooserFrame::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::RightButton) return;

  QPoint pos       = event->pos();
  int currentIndex = posToIndex(pos);
  if (currentIndex < 0) return;
  m_currentIndex = currentIndex;
  onSelect(currentIndex);
  update();
}

//-----------------------------------------------------------------------------

void BrushTipChooserFrame::mouseMoveEvent(QMouseEvent *event) {
  QPoint pos       = event->pos();
  int currentIndex = posToIndex(pos);
  if (currentIndex >= 0 && currentIndex < getChipCount())
    setCursor(Qt::PointingHandCursor);
  else
    setCursor(Qt::ArrowCursor);
}

//-----------------------------------------------------------------------------

void BrushTipChooserFrame::setCurrentBrush(BrushTipData *brush) {
  m_currentIndex = 0;
  if (!brush) return;

  int brushCount = TBrushTipManager::instance()->getBrushTipCount();
  for (int i = 0; i < brushCount; i++)
    if (TBrushTipManager::instance()->getBrushTip(i) == brush) {
      m_currentIndex = i + 1;
      return;
    }
}

//-----------------------------------------------------------------------------

void BrushTipChooserFrame::onSelect(int index) {
  if (index < 0 || index >= getChipCount()) return;

  BrushTipData *brushTip = 0;

  if (index != 0) {
    index--;
    brushTip = TBrushTipManager::instance()->getBrushTip(index);

    if (m_currentIndex < 0) return;
  }
  emit brushTipSelected(brushTip);
}

//-----------------------------------------------------------------------------

void BrushTipChooserFrame::computeSize() {
  int w        = width();
  m_chipPerRow = (w - 15) / m_chipSize.width();
  int rowCount = 0;
  if (m_chipPerRow != 0)
    rowCount = (getChipCount() + m_chipPerRow - 1) / m_chipPerRow;
  setMinimumSize(3 * m_chipSize.width(), rowCount * m_chipSize.height() + 10);
  update();
}

//***********************************************************************************
//    Brush Tip Popup
//***********************************************************************************

BrushTipPopup::BrushTipPopup(QWidget *parent)
    : QWidget(parent, Qt::Popup)
    , m_brushTip(0)
    , m_keepClosed(false)
    , m_keepClosedTimer(0) {
  setObjectName("BrushTipPopup");

  m_brushTipFrame = new BrushTipChooserFrame(this);

  QScrollArea *brushTipScrollArea = new QScrollArea(this);
  brushTipScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  brushTipScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  brushTipScrollArea->setWidget(createBrushTipChooser());

  m_spacingLabel = new QLabel(tr("Spacing:"));
  m_spacing      = new DVGui::DoubleField(this, true);
  m_spacing->setRange(0.02, 25.0);
  m_spacing->setValue(0.12); // Default value

  m_rotationLabel = new QLabel(tr("Rotation:"));
  m_rotation      = new DVGui::DoubleField(this, true);
  m_rotation->setRange(-180.0, 180.0);

  m_autoRotate = new DVGui::CheckBox(tr("Auto Rotate"), this);
  m_autoRotate->setChecked(true); // Default value

  m_flipH = new DVGui::CheckBox(tr("Flip Horizontally"), this);

  m_flipV = new DVGui::CheckBox(tr("Flip Vertically"), this);

  m_scatterLabel = new QLabel(tr("Scatter:"));
  m_scatter      = new DVGui::DoubleField(this, true);
  m_scatter->setRange(0.0, 100.0);

  m_keepClosedTimer = new QTimer(this);
  m_keepClosedTimer->setSingleShot(true);

  m_brushImage = new QLabel(this);
  m_brushImage->setFixedSize(64, 64);
  m_brushImage->setScaledContents(true);
  m_brushImage->setPixmap(getBrushTipPixmap());

  m_brushNameLabel = new QLabel(getBrushTipName());
  m_brushNameLabel->setWordWrap(true);

  QVBoxLayout *imageLayout = new QVBoxLayout();
  imageLayout->setContentsMargins(5, 5, 5, 5);
  imageLayout->setSpacing(6);
  {
    imageLayout->addWidget(m_brushImage, 0, Qt::AlignHCenter);
    imageLayout->addWidget(m_brushNameLabel, 0, Qt::AlignHCenter);
  }
  imageLayout->addStretch(1);

  QGridLayout *propertiesLayout = new QGridLayout();
  propertiesLayout->setContentsMargins(5, 5, 5, 5);
  propertiesLayout->setHorizontalSpacing(6);
  propertiesLayout->setVerticalSpacing(6);
  {
    propertiesLayout->addWidget(m_spacingLabel, 0, 0);
    propertiesLayout->addWidget(m_spacing, 0, 1);
    propertiesLayout->addWidget(m_scatterLabel, 1, 0);
    propertiesLayout->addWidget(m_scatter, 1, 1);
    propertiesLayout->addWidget(m_rotationLabel, 2, 0);
    propertiesLayout->addWidget(m_rotation, 2, 1);
    propertiesLayout->addWidget(m_autoRotate, 3, 0, 1, 2);
    propertiesLayout->addWidget(m_flipH, 4, 0, 1, 2);
    propertiesLayout->addWidget(m_flipV, 5, 0, 1, 2);
  }
  propertiesLayout->setRowStretch(6, 10);

  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setSpacing(6);
  {
    mainLayout->addLayout(imageLayout);
    mainLayout->addLayout(propertiesLayout);
    mainLayout->addWidget(brushTipScrollArea);
  }

  setLayout(mainLayout);

  bool ret = connect(m_keepClosedTimer, SIGNAL(timeout()), this,
                     SLOT(resetKeepClosed()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void BrushTipPopup::enableControls(bool enabled) {
  m_brushImage->setPixmap(getBrushTipPixmap());
  m_brushNameLabel->setText(getBrushTipName());

  m_spacingLabel->setEnabled(enabled);
  m_spacing->setEnabled(enabled);
  m_rotationLabel->setEnabled(enabled);
  m_rotation->setEnabled(enabled);
  m_autoRotate->setEnabled(enabled);
  m_flipH->setEnabled(enabled);
  m_flipV->setEnabled(enabled);
  m_scatterLabel->setEnabled(enabled);
  m_scatter->setEnabled(enabled);
}

//-----------------------------------------------------------------------------

void BrushTipPopup::mouseReleaseEvent(QMouseEvent *e) {
  // hide();
}

//-----------------------------------------------------------------------------

void BrushTipPopup::hideEvent(QHideEvent *e) {
  disconnect(m_brushTipFrame, SIGNAL(brushTipSelected(BrushTipData *)), this,
             SLOT(onBrushTipSelected(BrushTipData *)));
  disconnect(m_spacing, SIGNAL(valueChanged(bool)), this,
             SLOT(onBrushTipPropertyChanged()));
  disconnect(m_rotation, SIGNAL(valueChanged(bool)), this,
             SLOT(onBrushTipPropertyChanged()));
  disconnect(m_autoRotate, SIGNAL(stateChanged(int)), this,
             SLOT(onBrushTipPropertyChanged()));
  disconnect(m_flipH, SIGNAL(stateChanged(int)), this,
             SLOT(onBrushTipPropertyChanged()));
  disconnect(m_flipV, SIGNAL(stateChanged(int)), this,
             SLOT(onBrushTipPropertyChanged()));
  disconnect(m_scatter, SIGNAL(valueChanged(bool)), this,
             SLOT(onBrushTipPropertyChanged()));

  m_keepClosedTimer->start(300);
  m_keepClosed = true;
}

//-----------------------------------------------------------------------------

void BrushTipPopup::showEvent(QShowEvent *) {
  connect(m_brushTipFrame, SIGNAL(brushTipSelected(BrushTipData *)), this,
          SLOT(onBrushTipSelected(BrushTipData *)));
  connect(m_spacing, SIGNAL(valueChanged(bool)), this,
          SLOT(onBrushTipPropertyChanged()));
  connect(m_rotation, SIGNAL(valueChanged(bool)), this,
          SLOT(onBrushTipPropertyChanged()));
  connect(m_autoRotate, SIGNAL(stateChanged(int)), this,
          SLOT(onBrushTipPropertyChanged()));
  connect(m_flipH, SIGNAL(stateChanged(int)), this,
          SLOT(onBrushTipPropertyChanged()));
  connect(m_flipV, SIGNAL(stateChanged(int)), this,
          SLOT(onBrushTipPropertyChanged()));
  connect(m_scatter, SIGNAL(valueChanged(bool)), this,
          SLOT(onBrushTipPropertyChanged()));

  enableControls(m_brushTip);
}

//-----------------------------------------------------------------------------

void BrushTipPopup::resetKeepClosed() {
  if (m_keepClosedTimer) m_keepClosedTimer->stop();
  m_keepClosed = false;
}

//-----------------------------------------------------------------------------

QFrame *BrushTipPopup::createBrushTipChooser() {
  QFrame *brushTipOutsideFrame = new QFrame(this);
  brushTipOutsideFrame->setMinimumWidth(222);
  brushTipOutsideFrame->setMinimumHeight(230);

  m_brushTipSearchFrame = new QFrame();
  m_brushTipSearchText  = new QLineEdit();
  m_brushTipSearchClear = new QPushButton(tr("Clear Search"));
  m_brushTipSearchClear->setDisabled(true);
  m_brushTipSearchClear->setSizePolicy(QSizePolicy::Minimum,
                                       QSizePolicy::Preferred);

  /* ------ layout ------ */
  QVBoxLayout *brushTipOutsideLayout = new QVBoxLayout();
  brushTipOutsideLayout->setContentsMargins(0, 0, 0, 0);
  brushTipOutsideLayout->setSpacing(0);
  brushTipOutsideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  {
    QVBoxLayout *brushTipLayout = new QVBoxLayout();
    brushTipLayout->setContentsMargins(0, 0, 0, 0);
    brushTipLayout->setSpacing(0);
    brushTipLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      brushTipLayout->addWidget(m_brushTipFrame);
      brushTipLayout->addStretch();
    }
    QFrame *brushTipFrame = new QFrame(this);
    brushTipFrame->setMinimumWidth(50);
    brushTipFrame->setLayout(brushTipLayout);
    m_brushTipArea = new QScrollArea();
    m_brushTipArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_brushTipArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_brushTipArea->setWidgetResizable(true);
    m_brushTipArea->setWidget(brushTipFrame);
    m_brushTipArea->setMinimumWidth(50);
    brushTipOutsideLayout->addWidget(m_brushTipArea);

    QHBoxLayout *searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(2, 2, 2, 2);
    searchLayout->setSpacing(0);
    searchLayout->setSizeConstraint(QLayout::SetNoConstraint);
    {
      searchLayout->addWidget(m_brushTipSearchText);
      searchLayout->addWidget(m_brushTipSearchClear);
    }
    m_brushTipSearchFrame->setLayout(searchLayout);
    brushTipOutsideLayout->addWidget(m_brushTipSearchFrame);
  }
  brushTipOutsideFrame->setLayout(brushTipOutsideLayout);

  /* ------ signal-slot connections ------ */
  bool ret = true;
  ret =
      ret && connect(m_brushTipSearchText, SIGNAL(textChanged(const QString &)),
                     this, SLOT(onBrushTipSearch(const QString &)));
  ret = ret && connect(m_brushTipSearchClear, SIGNAL(clicked()), this,
                       SLOT(onBrushTipClearSearch()));

  assert(ret);
  return brushTipOutsideFrame;
}

//-----------------------------------------------------------------------------

void BrushTipPopup::onBrushTipSearch(const QString &search) {
  m_brushTipSearchClear->setDisabled(search.isEmpty());
  m_brushTipFrame->applyFilter(search);
  m_brushTipFrame->computeSize();
}

//-----------------------------------------------------------------------------

void BrushTipPopup::onBrushTipClearSearch() {
  m_brushTipSearchText->setText("");
  m_brushTipSearchText->setFocus();
}

//-----------------------------------------------------------------------------

void BrushTipPopup::onBrushTipSelected(BrushTipData *brushTip) {
  m_brushTip = brushTip;

  enableControls(m_brushTip);

  emit brushTipSelected(brushTip);
}

void BrushTipPopup::onBrushTipPropertyChanged() {
  emit brushTipPropertyChanged();
}