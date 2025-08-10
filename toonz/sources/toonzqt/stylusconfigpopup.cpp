#include "toonzqt/stylusconfigpopup.h"

#include "tstroke.h"

#include <QTimer>
#include <QGridLayout>
#include <QLabel>

//***********************************************************************************
//    Stylus Configuration Popup
//***********************************************************************************

StylusConfigPopup::StylusConfigPopup(QString title, QWidget *parent)
    : QWidget(parent, Qt::Popup)
    , m_configList(0)
    , m_graph(0)
    , m_currentIndex(0)
    , m_configProperties(0)
    , m_keepClosed(false)
    , m_keepClosedTimer(0) {
  setObjectName("StylusConfigPopup");

  m_keepClosedTimer = new QTimer(this);
  m_keepClosedTimer->setSingleShot(true);

  m_graphTitleLabel = new QLabel(title, this);

  m_configList = new QListWidget(this);
  m_configList->setMaximumWidth(170);
  m_configList->setCurrentRow(0);

  m_graph = new GraphGUIWidget();
  m_graph->setMaximumGraphSize(200, 200);
  m_graph->setMaxXValue(100.0);
  m_graph->setMaxYValue(100.0);
  m_graph->setCpMargin(5);
  m_graph->setGridSpacing(4);
  m_graph->setLinear(true);
  m_graph->setLockExtremePoints(false);
  m_graph->initializeSpline();

  m_graph->setMidXLabel(tr("Input"));
  m_graph->setMidYLabel(tr("Output"));

  m_graph->setEnabled(false);
  
  QGridLayout *mainLayout = new QGridLayout();
  mainLayout->setContentsMargins(5, 5, 5, 5);
  mainLayout->setHorizontalSpacing(6);
  mainLayout->setVerticalSpacing(6);
  {
    mainLayout->addWidget(m_graphTitleLabel, 0, 0, 1, 2, Qt::AlignHCenter);
    mainLayout->addWidget(m_configList, 1, 0);
    mainLayout->addWidget(m_graph, 1, 1);
  }
  setLayout(mainLayout);

  bool ret = connect(m_configList, SIGNAL(currentRowChanged(int)), this,
                     SLOT(onCurrentRowChanged(int)));

  ret = ret && connect(m_configList, SIGNAL(itemChanged(QListWidgetItem *)),
                       this, SLOT(onItemChanged(QListWidgetItem *)));

  ret = ret && connect(m_graph, SIGNAL(curveChanged(bool)), this,
                       SLOT(onCurveChanged(bool)));

  ret = ret && connect(m_keepClosedTimer, SIGNAL(timeout()), this,
                       SLOT(resetKeepClosed()));

  assert(ret);
}

//-----------------------------------------------------------------------------

int StylusConfigPopup::addConfiguration(QString name) {
  QListWidgetItem *configItem = new QListWidgetItem(name);
  configItem->setFlags(configItem->flags() | Qt::ItemIsUserCheckable);
  configItem->setCheckState(Qt::Unchecked);
  m_configList->addItem(configItem);

  GraphProperties configuration;
  configuration.enabled          = false;
  configuration.minX             = 0.0;
  configuration.maxX             = 100.0;
  configuration.minY             = 0.0;
  configuration.maxY             = 100.0;
  configuration.defaultCurve =
      QList<TPointD>{TPointD(0.0, 0.0), TPointD(100.0, 100.0)};
  configuration.curve = configuration.defaultCurve;

  m_configProperties.push_back(configuration);

  if (m_configList->count() == 1) {
    m_configList->setCurrentRow(0);
  }

  return m_configProperties.size() - 1;
}

//-----------------------------------------------------------------------------

void StylusConfigPopup::setConfiguration(int configId, double minX, double maxX,
                                         double minY, double maxY,
                                         QList<TPointD> defaultCurve,
                                         QString minXLabel, QString midXLabel,
                                         QString maxXLabel, QString minYLabel,
                                         QString midYLabel, QString maxYLabel) {
  if (configId >= m_configList->count()) return;

  m_configProperties[configId].enabled          = false;
  m_configProperties[configId].minX             = minX;
  m_configProperties[configId].maxX             = maxX;
  m_configProperties[configId].minY             = minY;
  m_configProperties[configId].maxY             = maxY;
  m_configProperties[configId].defaultCurve     = defaultCurve;
  m_configProperties[configId].curve            = defaultCurve;
  m_configProperties[configId].minXLabel        = minXLabel;
  m_configProperties[configId].midXLabel        = midXLabel;
  m_configProperties[configId].maxXLabel        = maxXLabel;
  m_configProperties[configId].minYLabel        = minYLabel;
  m_configProperties[configId].midYLabel        = midYLabel;
  m_configProperties[configId].maxYLabel        = maxYLabel;
}

//-----------------------------------------------------------------------------

void StylusConfigPopup::onCurrentRowChanged(int row) {
  m_currentIndex = row;

  updateGraph();
}

void StylusConfigPopup::updateGraph() {
  if (m_currentIndex < 0) {
    m_graph->setEnabled(false);
    return;
  }

  QString minXLabel = m_configProperties[m_currentIndex].minXLabel;
  QString midXLabel = m_configProperties[m_currentIndex].midXLabel;
  QString maxXLabel = m_configProperties[m_currentIndex].maxXLabel;
  QString minYLabel = m_configProperties[m_currentIndex].minYLabel;
  QString midYLabel = m_configProperties[m_currentIndex].midYLabel;
  QString maxYLabel = m_configProperties[m_currentIndex].maxYLabel;

  m_graph->setMinXLabel(
      minXLabel.isEmpty()
          ? QString().asprintf("%+.2f", m_configProperties[m_currentIndex].minX)
          : minXLabel);
  if (!midXLabel.isEmpty()) m_graph->setMidXLabel(midXLabel);
  m_graph->setMaxXLabel(
      maxXLabel.isEmpty()
          ? QString().asprintf("%+.2f", m_configProperties[m_currentIndex].maxX)
          : maxXLabel);
  m_graph->setMinXValue(m_configProperties[m_currentIndex].minX);
  m_graph->setMaxXValue(m_configProperties[m_currentIndex].maxX);

  m_graph->setMinYLabel(
      minYLabel.isEmpty()
          ? QString().asprintf("%+.2f", m_configProperties[m_currentIndex].minY)
          : minYLabel);
  if (!midYLabel.isEmpty()) m_graph->setMidXLabel(midYLabel);
  m_graph->setMaxYLabel(
      maxYLabel.isEmpty()
          ? QString().asprintf("%+.2f", m_configProperties[m_currentIndex].maxY)
          : maxYLabel);
  m_graph->setMinYValue(m_configProperties[m_currentIndex].minY);
  m_graph->setMaxYValue(m_configProperties[m_currentIndex].maxY);

  double cpMargin = std::min(m_configProperties[m_currentIndex].maxX,
                             m_configProperties[m_currentIndex].maxY) /
                    20;
  if (cpMargin > 20) cpMargin = 20;
  m_graph->setCpMargin(cpMargin);

  m_graph->setDefaultCurve(m_configProperties[m_currentIndex].defaultCurve);
  m_graph->setCurve(m_configProperties[m_currentIndex].curve);

  QListWidgetItem *config = m_configList->item(m_currentIndex);
  m_graph->setEnabled(config->checkState() == Qt::Checked);
}

//-----------------------------------------------------------------------------

void StylusConfigPopup::onItemChanged(QListWidgetItem *config) {
  int configId                         = m_configList->row(config);
  m_configProperties[configId].enabled = config->checkState() == Qt::Checked;

  if (configId == m_currentIndex)
    m_graph->setEnabled(m_configProperties[configId].enabled);

  emit configStateChanged(configId);
}

//-----------------------------------------------------------------------------

void StylusConfigPopup::onCurveChanged(bool isDragging) {
  if (m_currentIndex < 0) return;
  m_configProperties[m_currentIndex].curve = m_graph->getCurve();

  emit configCurveChanged(m_currentIndex, isDragging);
}

//-----------------------------------------------------------------------------

void StylusConfigPopup::mouseReleaseEvent(QMouseEvent *e) {
  // hide();
}

//-----------------------------------------------------------------------------

void StylusConfigPopup::hideEvent(QHideEvent *e) {
  m_keepClosedTimer->start(300);
  m_keepClosed = true;
}

//-----------------------------------------------------------------------------

void StylusConfigPopup::showEvent(QShowEvent*) { updateGraph(); }

//-----------------------------------------------------------------------------

void StylusConfigPopup::resetKeepClosed() {
  if (m_keepClosedTimer) m_keepClosedTimer->stop();
  m_keepClosed = false;
}
