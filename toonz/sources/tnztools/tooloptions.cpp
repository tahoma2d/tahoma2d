

#include "tools/tooloptions.h"

// TnzTools includes
#include "tools/tool.h"
#include "tools/toolhandle.h"
#include "tools/toolcommandids.h"

#include "edittool.h"
#include "selectiontool.h"
#include "vectorselectiontool.h"
#include "rasterselectiontool.h"
#include "toonzrasterbrushtool.h"
#include "fullcolorbrushtool.h"
#include "toonzvectorbrushtool.h"
#include "tooloptionscontrols.h"

//#include "rgbpickertool.h"
#include "rulertool.h"
#include "shifttracetool.h"
#include "perspectivetool.h"
#include "symmetrytool.h"

// TnzQt includes
#include "toonzqt/dvdialog.h"
#include "toonzqt/menubarcommand.h"
#include "toonzqt/gutil.h"
#include "toonzqt/dvscrollwidget.h"
#include "toonzqt/lutcalibrator.h"
#include "toonzqt/viewcommandids.h"

// TnzLib includes
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobject.h"
#include "toonz/txsheethandle.h"
#include "toonz/tstageobjectspline.h"
#include "toonz/tframehandle.h"
#include "toonz/tpalettehandle.h"
#include "toonz/palettecontroller.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/preferences.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/mypaintbrushstyle.h"
#include "toonz/tonionskinmaskhandle.h"

// TnzCore includes
#include "tproperty.h"
#include "tenv.h"

// Qt includes
#include <QPainter>
#include <QToolBar>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QResizeEvent>
#include <QList>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QStackedWidget>

using namespace ToolOptionsControls;

//=============================================================================
// ToolOptionToolBar

ToolOptionToolBar::ToolOptionToolBar(QWidget *parent) : QToolBar(parent) {
  setObjectName("toolOptionsPanel");
  setMaximumHeight(25);
}

//-----------------------------------------------------------------------------

void ToolOptionToolBar::addSpacing(int width) {
  QWidget *spaceW = new QWidget(this);
  spaceW->setFixedWidth(width);
  addWidget(spaceW);
}

//=============================================================================
// ToolOptionsBox

ToolOptionsBox::ToolOptionsBox(QWidget *parent, bool isScrollable)
    : QFrame(parent) {
  setObjectName("toolOptionsPanel");
  setStyleSheet("#toolOptionsPanel {border: 0px; margin: 1px;}");

  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_layout = new QHBoxLayout;
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(5);
  m_layout->addSpacing(5);

  if (isScrollable) {
    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->setSpacing(0);
    setLayout(hLayout);

    // Build the scroll widget vin which the toolbar will be placed
    DvScrollWidget *scrollWidget = new DvScrollWidget;
    hLayout->addWidget(scrollWidget);

    // In the scrollable layout we add a widget of 24 height
    // which contains the tooloptionBar. This is necessary
    // to make the hboxlayout adjust the bar's vertical position.
    QWidget *toolContainer = new QWidget;
    scrollWidget->setWidget(toolContainer);

    toolContainer->setSizePolicy(QSizePolicy::MinimumExpanding,
                                 QSizePolicy::Fixed);
    toolContainer->setFixedHeight(24);
    toolContainer->setObjectName("toolOptionsPanel");
    toolContainer->setLayout(m_layout);
  } else
    setLayout(m_layout);
}

//-----------------------------------------------------------------------------

ToolOptionsBox::~ToolOptionsBox() {
  std::for_each(m_controls.begin(), m_controls.end(),
                std::default_delete<ToolOptionControl>());
  std::for_each(m_labels.begin(), m_labels.end(),
                std::default_delete<QLabel>());
}

//-----------------------------------------------------------------------------

QLabel *ToolOptionsBox::addLabel(QString name) {
  QLabel *label = new QLabel(name);
  label->setFixedHeight(20);
  m_layout->addWidget(label);
  m_labels[name.toStdString()] = label;
  return label;
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::addLabel(std::string propName, QLabel *label) {
  m_labels[propName] = label;
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::addSeparator() {
  DVGui::Separator *separator = new DVGui::Separator();
  separator->setOrientation(false);
  separator->setFixedWidth(17);
  m_layout->addWidget(separator, 0);
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::addControl(ToolOptionControl *control) {
  m_controls[control->propertyName()] = control;
}

//-----------------------------------------------------------------------------

ToolOptionControl *ToolOptionsBox::control(
    const std::string &controlName) const {
  QMap<std::string, ToolOptionControl *>::const_iterator ct =
      m_controls.find(controlName);

  return (ct == m_controls.end()) ? 0 : ct.value();
}

//-----------------------------------------------------------------------------

void ToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//=============================================================================
// ToolOptionControlBuilder

ToolOptionControlBuilder::ToolOptionControlBuilder(ToolOptionsBox *panel,
                                                   TTool *tool,
                                                   TPaletteHandle *pltHandle,
                                                   ToolHandle *toolHandle)
    : m_panel(panel)
    , m_tool(tool)
    , m_pltHandle(pltHandle)
    , m_toolHandle(toolHandle)
    , m_singleValueWidgetType(SLIDER)
    , m_enumWidgetType(COMBOBOX) {}

//-----------------------------------------------------------------------------

QLabel *ToolOptionControlBuilder::addLabel(TProperty *p) {
  QLabel *label = new QLabel(p->getQStringName());
  hLayout()->addWidget(label, 0);
  return label;
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TDoubleProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  ToolOptionSlider *control = new ToolOptionSlider(m_tool, p, m_toolHandle);
  hLayout()->addWidget(control, 100);
  // storing the control in the map for updating values later
  m_panel->addControl(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TDoublePairProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  ToolOptionPairSlider *control = new ToolOptionPairSlider(
      m_tool, p, QObject::tr("Min:"), QObject::tr("Max:"), m_toolHandle);
  hLayout()->addWidget(control, 150);
  m_panel->addControl(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TIntPairProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  ToolOptionIntPairSlider *control = new ToolOptionIntPairSlider(
      m_tool, p, QObject::tr("Min:"), QObject::tr("Max:"), m_toolHandle);
  hLayout()->addWidget(control, 100);
  m_panel->addControl(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TIntProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);

  ToolOptionIntSlider *control =
      new ToolOptionIntSlider(m_tool, p, m_toolHandle);

  if (m_singleValueWidgetType == FIELD) {
    control->enableSlider(false);
    control->setFixedWidth(45);
  }
  hLayout()->addWidget(control, 100);
  m_panel->addControl(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TBoolProperty *p) {
  ToolOptionCheckbox *control = new ToolOptionCheckbox(m_tool, p, m_toolHandle);
  hLayout()->addWidget(control, 0);

  m_panel->addControl(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TStringProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);

  ToolOptionTextField *control = new ToolOptionTextField(m_tool, p);
  m_panel->addControl(control);

  hLayout()->addWidget(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TEnumProperty *p) {
  QWidget *widget;
  ToolOptionControl *control;
  switch (m_enumWidgetType) {
  case POPUPBUTTON: {
    ToolOptionPopupButton *obj = new ToolOptionPopupButton(m_tool, p);
    obj->setToolTip(p->getQStringName());
    control = obj;
    widget  = obj;
    break;
  }

  case FONTCOMBOBOX: {
    if (p->getQStringName() != "") {
      QLabel *label = addLabel(p);
      m_panel->addLabel(p->getName(), label);
    }
    ToolOptionFontCombo *obj = new ToolOptionFontCombo(m_tool, p, m_toolHandle);
    control                  = obj;
    widget                   = obj;
    break;
  }

  case COMBOBOX:
  default: {
    if (p->getQStringName() != "") {
      QLabel *label = addLabel(p);
      m_panel->addLabel(p->getName(), label);
    }
    ToolOptionCombo *obj = new ToolOptionCombo(m_tool, p, m_toolHandle);
    control              = obj;
    widget               = obj;
    break;
  }
  }

  hLayout()->addWidget(widget, 100);
  m_panel->addControl(control);
  hLayout()->addSpacing(5);
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TStyleIndexProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  StyleIndexFieldAndChip *chip =
      new StyleIndexFieldAndChip(m_tool, p, m_pltHandle, m_toolHandle);
  hLayout()->addWidget(chip, 0);
  m_panel->addControl(chip);
}

//----------------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TPointerProperty *p) {
  assert(!"not implemented");
}

//-----------------------------------------------------------------------------

void ToolOptionControlBuilder::visit(TColorChipProperty *p) {
  QLabel *label = addLabel(p);
  m_panel->addLabel(p->getName(), label);
  ColorChipCombo *chipCombo = new ColorChipCombo(m_tool, p);
  hLayout()->addWidget(chipCombo, 0);
  m_panel->addControl(chipCombo);
}

//=============================================================================
// GenericToolOptionsBox

GenericToolOptionsBox::GenericToolOptionsBox(QWidget *parent, TTool *tool,
                                             TPaletteHandle *pltHandle,
                                             int propertyGroupIdx,
                                             ToolHandle *toolHandle,
                                             bool scrollable)
    : ToolOptionsBox(parent, scrollable) {
  setObjectName("toolOptionsPanel");

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(propertyGroupIdx))
    tool->getProperties(propertyGroupIdx)->accept(builder);

  m_layout->addStretch(1);
}

//-----------------------------------------------------------------------------

// show 17x17 icon without style dependency
class SimpleIconViewField : public DraggableIconView {
  QIcon m_icon;

public:
  SimpleIconViewField(const QString &iconName, const QString &toolTipStr = "",
                      QWidget *parent = 0)
      : DraggableIconView(parent), m_icon(createQIcon(iconName.toUtf8())) {
    setMinimumSize(18, 18);
    setToolTip(toolTipStr);
  }

protected:
  void paintEvent(QPaintEvent *e) {
    QPainter p(this);
    p.drawPixmap(QRect(0, 2, 18, 18), m_icon.pixmap(18, 18));
  }
};

//-----------------------------------------------------------------------------

ArrowToolOptionsBox::ArrowToolOptionsBox(
    QWidget *parent, TTool *tool, TPropertyGroup *pg, TFrameHandle *frameHandle,
    TObjectHandle *objHandle, TXsheetHandle *xshHandle, ToolHandle *toolHandle)
    : ToolOptionsBox(parent, true)
    , m_pg(pg)
    , m_splined(false)
    , m_tool(tool)
    , m_frameHandle(frameHandle)
    , m_objHandle(objHandle)
    , m_xshHandle(xshHandle) {
  setFrameStyle(QFrame::StyledPanel);
  setObjectName("toolOptionsPanel");
  setFixedHeight(26);

  EditTool *editTool = dynamic_cast<EditTool *>(tool);

  m_axisOptionWidgets = new QWidget *[AllAxis];

  /* --- General Parts --- */

  // enable to choose target pegbar with combobox
  m_currentStageObjectCombo = new QComboBox(this);
  m_currentStageObjectCombo->setSizeAdjustPolicy(
      QComboBox::SizeAdjustPolicy::AdjustToContents);

  TEnumProperty *activeAxisProp =
      dynamic_cast<TEnumProperty *>(m_pg->getProperty("Active Axis"));
  if (activeAxisProp)
    m_chooseActiveAxisCombo =
        new ToolOptionCombo(m_tool, activeAxisProp, toolHandle);
  TEnumProperty *autoSelectProp =
      dynamic_cast<TEnumProperty *>(m_pg->getProperty("Auto Select Column"));
  if (autoSelectProp)
    m_pickCombo = new ToolOptionCombo(m_tool, autoSelectProp, toolHandle);

  m_pickWidget = new QWidget(this);

  /* --- Position --- */
  m_motionPathPosField =
      new PegbarChannelField(m_tool, TStageObject::T_Path, "field", frameHandle,
                             objHandle, xshHandle, this);
  m_ewPosField =
      new PegbarChannelField(m_tool, TStageObject::T_X, "field", frameHandle,
                             objHandle, xshHandle, this);
  m_nsPosField =
      new PegbarChannelField(m_tool, TStageObject::T_Y, "field", frameHandle,
                             objHandle, xshHandle, this);
  m_zField = new PegbarChannelField(m_tool, TStageObject::T_Z, "field",
                                    frameHandle, objHandle, xshHandle, this);
  m_noScaleZField = new NoScaleField(m_tool, "field");

  m_zLabel             = new ClickableLabel(tr("Z:"), this);
  m_motionPathPosLabel = new ClickableLabel(tr("Position:"), this);
  m_ewPosLabel         = new ClickableLabel(tr("X:"), this);
  m_nsPosLabel         = new ClickableLabel(tr("Y:"), this);

  // Lock X
  TBoolProperty *lockProp =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Position X"));
  if (lockProp)
    m_lockEWPosCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock Y
  lockProp =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Position Y"));
  if (lockProp)
    m_lockNSPosCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // stacking order
  m_soLabel = new ClickableLabel(tr("SO:"), this);
  m_soField = new PegbarChannelField(m_tool, TStageObject::T_SO, "field",
                                     frameHandle, objHandle, xshHandle, this);

  /* --- Rotation --- */
  m_rotationLabel = new ClickableLabel(tr("Rotation:"), this);
  m_rotationField =
      new PegbarChannelField(m_tool, TStageObject::T_Angle, "field",
                             frameHandle, objHandle, xshHandle, this);

  /* --- Scale --- */
  m_globalScaleField =
      new PegbarChannelField(m_tool, TStageObject::T_Scale, "field",
                             frameHandle, objHandle, xshHandle, this);
  m_scaleHField =
      new PegbarChannelField(m_tool, TStageObject::T_ScaleX, "field",
                             frameHandle, objHandle, xshHandle, this);
  m_scaleVField =
      new PegbarChannelField(m_tool, TStageObject::T_ScaleY, "field",
                             frameHandle, objHandle, xshHandle, this);

  m_globalScaleLabel = new ClickableLabel(tr("Global:"), this);
  m_scaleHLabel      = new ClickableLabel(tr("H:"), this);
  m_scaleVLabel      = new ClickableLabel(tr("V:"), this);

  TEnumProperty *scaleConstraintProp =
      dynamic_cast<TEnumProperty *>(m_pg->getProperty("Scale Constraint:"));
  if (scaleConstraintProp)
    m_maintainCombo =
        new ToolOptionCombo(m_tool, scaleConstraintProp, toolHandle);
  // Lock Scale H
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Scale H"));
  if (lockProp)
    m_lockScaleHCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock Scale V
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Scale V"));
  if (lockProp)
    m_lockScaleVCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);

  /* --- Shear --- */
  m_shearHField =
      new PegbarChannelField(m_tool, TStageObject::T_ShearX, "field",
                             frameHandle, objHandle, xshHandle, this);
  m_shearVField =
      new PegbarChannelField(m_tool, TStageObject::T_ShearY, "field",
                             frameHandle, objHandle, xshHandle, this);
  m_shearHLabel = new ClickableLabel(tr("H:"), this);
  m_shearVLabel = new ClickableLabel(tr("V:"), this);

  // Lock Shear H
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Shear H"));
  if (lockProp)
    m_lockShearHCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock Scale V
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Shear V"));
  if (lockProp)
    m_lockShearVCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);

  /* --- Center Position --- */
  m_ewCenterField =
      new PegbarCenterField(m_tool, 0, "field", objHandle, xshHandle, this);
  m_nsCenterField =
      new PegbarCenterField(m_tool, 1, "field", objHandle, xshHandle, this);
  m_ewCenterLabel = new ClickableLabel(tr("X:"), this);
  m_nsCenterLabel = new ClickableLabel(tr("Y:"), this);

  // Lock X Center
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Center X"));
  if (lockProp)
    m_lockEWCenterCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);
  // Lock Y Center
  lockProp = dynamic_cast<TBoolProperty *>(m_pg->getProperty("Lock Center Y"));
  if (lockProp)
    m_lockNSCenterCheckbox =
        new ToolOptionCheckbox(m_tool, lockProp, toolHandle, this);

  TBoolProperty *globalKeyProp =
      dynamic_cast<TBoolProperty *>(m_pg->getProperty("Global Key"));
  if (globalKeyProp)
    m_globalKey =
        new ToolOptionCheckbox(m_tool, globalKeyProp, toolHandle, this);

  m_lockEWPosCheckbox->setObjectName("EditToolLockButton");
  m_lockNSPosCheckbox->setObjectName("EditToolLockButton");
  m_lockEWCenterCheckbox->setObjectName("EditToolLockButton");
  m_lockNSCenterCheckbox->setObjectName("EditToolLockButton");
  m_lockScaleHCheckbox->setObjectName("EditToolLockButton");
  m_lockScaleVCheckbox->setObjectName("EditToolLockButton");
  m_lockShearHCheckbox->setObjectName("EditToolLockButton");
  m_lockShearVCheckbox->setObjectName("EditToolLockButton");
  m_lockEWPosCheckbox->setText("");
  m_lockNSPosCheckbox->setText("");
  m_lockEWCenterCheckbox->setText("");
  m_lockNSCenterCheckbox->setText("");
  m_lockScaleHCheckbox->setText("");
  m_lockScaleVCheckbox->setText("");
  m_lockShearHCheckbox->setText("");
  m_lockShearVCheckbox->setText("");

  m_zField->setPrecision(4);
  m_noScaleZField->setPrecision(4);

  m_hFlipButton = new QPushButton(this);
  m_vFlipButton = new QPushButton(this);

  m_leftRotateButton  = new QPushButton(this);
  m_rightRotateButton = new QPushButton(this);

  m_hFlipButton->setFixedSize(QSize(20, 20));
  m_vFlipButton->setFixedSize(QSize(20, 20));

  m_leftRotateButton->setFixedSize(QSize(20, 20));
  m_rightRotateButton->setFixedSize(QSize(20, 20));

  m_hFlipButton->setIcon(createQIcon("fliphoriz"));
  m_hFlipButton->setIconSize(QSize(20, 20));
  m_vFlipButton->setIcon(createQIcon("flipvert"));
  m_vFlipButton->setIconSize(QSize(20, 20));
  m_leftRotateButton->setIcon(createQIcon("rotateleft"));
  m_leftRotateButton->setIconSize(QSize(20, 20));
  m_rightRotateButton->setIcon(createQIcon("rotateright"));
  m_rightRotateButton->setIconSize(QSize(20, 20));

  m_hFlipButton->setToolTip(tr("Flip Object Horizontally"));
  m_vFlipButton->setToolTip(tr("Flip Object Vertically"));
  m_leftRotateButton->setToolTip(tr("Rotate Object Left"));
  m_rightRotateButton->setToolTip(tr("Rotate Object Right"));

  bool splined                        = isCurrentObjectSplined();
  if (splined != m_splined) m_splined = splined;
  setSplined(m_splined);

  const int ITEM_SPACING  = 10;
  const int LABEL_SPACING = 3;
  /* --- Layout --- */
  QHBoxLayout *mainLay = m_layout;
  {
    mainLay->addWidget(m_currentStageObjectCombo, 0);
    mainLay->addWidget(m_chooseActiveAxisCombo, 0);

    // Pick combobox only available on "All" axis mode
    QHBoxLayout *pickLay = new QHBoxLayout();
    pickLay->setContentsMargins(0, 0, 0, 0);
    pickLay->setSpacing(0);
    {
      pickLay->addSpacing(5);
      pickLay->addWidget(new QLabel(tr("Pick:"), this), 0);
      pickLay->addWidget(m_pickCombo, 0);
    }
    m_pickWidget->setLayout(pickLay);
    mainLay->addWidget(m_pickWidget, 0);

    addSeparator();

    {
      // Position
      QFrame *posFrame    = new QFrame(this);
      QHBoxLayout *posLay = new QHBoxLayout();
      posLay->setContentsMargins(0, 0, 0, 0);
      posLay->setSpacing(0);
      posFrame->setLayout(posLay);
      {
        posLay->addWidget(
            new SimpleIconViewField("edit_position", tr("Position"), this), 0);
        posLay->addSpacing(LABEL_SPACING * 2);
        posLay->addWidget(m_motionPathPosLabel, 0);
        posLay->addWidget(m_motionPathPosField, 0);

        posLay->addWidget(m_ewPosLabel, 0);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(m_ewPosField, 10);
        posLay->addWidget(m_lockEWPosCheckbox, 0);

        posLay->addSpacing(ITEM_SPACING);

        posLay->addWidget(m_nsPosLabel, 0);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(m_nsPosField, 10);
        posLay->addWidget(m_lockNSPosCheckbox, 0);

        posLay->addSpacing(ITEM_SPACING);

        posLay->addWidget(m_zLabel, 0);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(m_zField, 10);
        posLay->addSpacing(LABEL_SPACING);
        posLay->addWidget(new QLabel(tr("("), this), 0);
        posLay->addWidget(m_noScaleZField, 10);
        posLay->addWidget(new QLabel(tr(")"), this), 0);

        posLay->addSpacing(ITEM_SPACING);

        posLay->addWidget(m_soLabel, 0);
        posLay->addWidget(m_soField, 10);

        posLay->addSpacing(ITEM_SPACING);
        posLay->addWidget(new DVGui::Separator("", this, false));

        posLay->addStretch(1);
      }
      m_axisOptionWidgets[Position] = posFrame;
      mainLay->addWidget(m_axisOptionWidgets[Position], 0);

      // Rotation
      QFrame *rotFrame    = new QFrame(this);
      QHBoxLayout *rotLay = new QHBoxLayout();
      rotLay->setContentsMargins(0, 0, 0, 0);
      rotLay->setSpacing(0);
      rotFrame->setLayout(rotLay);
      {
        rotLay->addWidget(
            new SimpleIconViewField("edit_rotation", tr("Rotation"), this), 0);
        rotLay->addSpacing(LABEL_SPACING * 2);
        rotLay->addWidget(m_rotationLabel, 0);
        rotLay->addSpacing(LABEL_SPACING);
        rotLay->addWidget(m_rotationField, 10);
        rotLay->addWidget(m_leftRotateButton);
        rotLay->addWidget(m_rightRotateButton);

        rotLay->addSpacing(ITEM_SPACING);
        rotLay->addWidget(new DVGui::Separator("", this, false));

        rotLay->addStretch(1);
      }
      m_axisOptionWidgets[Rotation] = rotFrame;
      mainLay->addWidget(m_axisOptionWidgets[Rotation], 0);

      // Scale
      QFrame *scaleFrame    = new QFrame(this);
      QHBoxLayout *scaleLay = new QHBoxLayout();
      scaleLay->setContentsMargins(0, 0, 0, 0);
      scaleLay->setSpacing(0);
      scaleFrame->setLayout(scaleLay);
      {
        scaleLay->addWidget(
            new SimpleIconViewField("edit_scale", tr("Scale"), this), 0);
        scaleLay->addSpacing(LABEL_SPACING * 2);

        scaleLay->addWidget(m_globalScaleLabel, 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_globalScaleField, 10);

        scaleLay->addSpacing(ITEM_SPACING);

        scaleLay->addWidget(m_scaleHLabel, 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_scaleHField, 10);
        scaleLay->addWidget(m_hFlipButton);
        scaleLay->addWidget(m_lockScaleHCheckbox, 0);

        scaleLay->addSpacing(ITEM_SPACING);

        scaleLay->addWidget(m_scaleVLabel, 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_scaleVField, 10);
        scaleLay->addWidget(m_vFlipButton);
        scaleLay->addWidget(m_lockScaleVCheckbox, 0);

        scaleLay->addSpacing(ITEM_SPACING);

        scaleLay->addWidget(new QLabel(tr("Maintain:"), this), 0);
        scaleLay->addSpacing(LABEL_SPACING);
        scaleLay->addWidget(m_maintainCombo, 0);

        scaleLay->addSpacing(ITEM_SPACING);
        scaleLay->addWidget(new DVGui::Separator("", this, false));

        scaleLay->addStretch(1);
      }
      m_axisOptionWidgets[Scale] = scaleFrame;
      mainLay->addWidget(m_axisOptionWidgets[Scale], 0);

      // Shear
      QFrame *shearFrame    = new QFrame(this);
      QHBoxLayout *shearLay = new QHBoxLayout();
      shearLay->setContentsMargins(0, 0, 0, 0);
      shearLay->setSpacing(0);
      shearFrame->setLayout(shearLay);
      {
        shearLay->addWidget(
            new SimpleIconViewField("edit_shear", tr("Shear"), this), 0);
        shearLay->addSpacing(LABEL_SPACING * 2);

        shearLay->addWidget(m_shearHLabel, 0);
        shearLay->addSpacing(LABEL_SPACING);
        shearLay->addWidget(m_shearHField, 10);
        shearLay->addWidget(m_lockShearHCheckbox, 0);

        shearLay->addSpacing(ITEM_SPACING);

        shearLay->addWidget(m_shearVLabel, 0);
        shearLay->addSpacing(LABEL_SPACING);
        shearLay->addWidget(m_shearVField, 10);
        shearLay->addWidget(m_lockShearVCheckbox, 0);

        shearLay->addSpacing(ITEM_SPACING);

        shearLay->addWidget(new DVGui::Separator("", this, false));

        shearLay->addStretch(1);
      }
      m_axisOptionWidgets[Shear] = shearFrame;
      mainLay->addWidget(m_axisOptionWidgets[Shear], 0);

      // Center Position
      QFrame *centerPosFrame    = new QFrame(this);
      QHBoxLayout *centerPosLay = new QHBoxLayout();
      centerPosLay->setContentsMargins(0, 0, 0, 0);
      centerPosLay->setSpacing(0);
      centerPosFrame->setLayout(centerPosLay);
      {
        centerPosLay->addWidget(
            new SimpleIconViewField("edit_center", tr("Center Position"), this),
            0);
        centerPosLay->addSpacing(LABEL_SPACING * 2);

        centerPosLay->addWidget(m_ewCenterLabel, 0);
        centerPosLay->addSpacing(LABEL_SPACING);
        centerPosLay->addWidget(m_ewCenterField, 10);
        centerPosLay->addWidget(m_lockEWCenterCheckbox, 0);

        centerPosLay->addSpacing(ITEM_SPACING);

        centerPosLay->addWidget(m_nsCenterLabel, 0);
        centerPosLay->addSpacing(LABEL_SPACING);
        centerPosLay->addWidget(m_nsCenterField, 10);
        centerPosLay->addWidget(m_lockNSCenterCheckbox, 0);

        centerPosLay->addSpacing(ITEM_SPACING);
        centerPosLay->addWidget(new DVGui::Separator("", this, false));

        centerPosLay->addStretch(1);
      }
      m_axisOptionWidgets[CenterPosition] = centerPosFrame;
      mainLay->addWidget(m_axisOptionWidgets[CenterPosition], 0);
    }

    mainLay->addWidget(m_globalKey, 0);

    mainLay->addSpacing(ITEM_SPACING);
  }

  /* --- signal-slot connections --- */
  // swap page when the active axis is changed
  connect(m_chooseActiveAxisCombo, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onCurrentAxisChanged(int)));
  // when the current stage object is changed via combo box, then switch the
  // current stage object in the scene
  connect(m_currentStageObjectCombo, SIGNAL(activated(int)), this,
          SLOT(onCurrentStageObjectComboActivated(int)));

  if (scaleConstraintProp) {
    m_scaleHField->onScaleTypeChanged(m_maintainCombo->currentIndex());
    m_scaleVField->onScaleTypeChanged(m_maintainCombo->currentIndex());
    connect(m_maintainCombo, SIGNAL(currentIndexChanged(int)), m_scaleHField,
            SLOT(onScaleTypeChanged(int)));
    connect(m_maintainCombo, SIGNAL(currentIndexChanged(int)), m_scaleVField,
            SLOT(onScaleTypeChanged(int)));
  }

  // enables adjusting value by dragging on the label
  connectLabelAndField(m_motionPathPosLabel, m_motionPathPosField);
  connectLabelAndField(m_ewPosLabel, m_ewPosField);
  connectLabelAndField(m_nsPosLabel, m_nsPosField);
  connectLabelAndField(m_zLabel, m_zField);
  connectLabelAndField(m_soLabel, m_soField);
  connectLabelAndField(m_rotationLabel, m_rotationField);
  connectLabelAndField(m_globalScaleLabel, m_globalScaleField);
  connectLabelAndField(m_scaleHLabel, m_scaleHField);
  connectLabelAndField(m_scaleVLabel, m_scaleVField);
  connectLabelAndField(m_shearHLabel, m_shearHField);
  connectLabelAndField(m_shearVLabel, m_shearVField);
  connectLabelAndField(m_ewCenterLabel, m_ewCenterField);
  connectLabelAndField(m_nsCenterLabel, m_nsCenterField);

  onCurrentAxisChanged(activeAxisProp->getIndex());

  connect(m_hFlipButton, SIGNAL(clicked()), SLOT(onFlipHorizontal()));
  connect(m_vFlipButton, SIGNAL(clicked()), SLOT(onFlipVertical()));
  connect(m_leftRotateButton, SIGNAL(clicked()), SLOT(onRotateLeft()));
  connect(m_rightRotateButton, SIGNAL(clicked()), SLOT(onRotateRight()));

  connect(editTool, SIGNAL(clickFlipHorizontal()), SLOT(onFlipHorizontal()));
  connect(editTool, SIGNAL(clickFlipVertical()), SLOT(onFlipVertical()));
  connect(editTool, SIGNAL(clickRotateLeft()), SLOT(onRotateLeft()));
  connect(editTool, SIGNAL(clickRotateRight()), SLOT(onRotateRight()));
}

//-----------------------------------------------------------------------------
// enables adjusting value by dragging on the label

void ArrowToolOptionsBox::connectLabelAndField(ClickableLabel *label,
                                               MeasuredValueField *field) {
  connect(label, SIGNAL(onMousePress(QMouseEvent *)), field,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(label, SIGNAL(onMouseMove(QMouseEvent *)), field,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(label, SIGNAL(onMouseRelease(QMouseEvent *)), field,
          SLOT(receiveMouseRelease(QMouseEvent *)));
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::showEvent(QShowEvent *) {
  connect(m_frameHandle, SIGNAL(frameSwitched()), SLOT(onFrameSwitched()));
  // if some stage object is added/removed, then reflect it to the combobox
  connect(m_xshHandle, SIGNAL(xsheetSwitched()), this,
          SLOT(updateStageObjectComboItems()));
  connect(m_xshHandle, SIGNAL(xsheetChanged()), this,
          SLOT(updateStageObjectComboItems()));
  // If the current stage object is switched, then synchronize it to the
  // combobox
  connect(m_objHandle, SIGNAL(objectSwitched()), this,
          SLOT(syncCurrentStageObjectComboItem()));

  // update the item list in m_currentStageObjectCombo
  updateStageObjectComboItems();
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::hideEvent(QShowEvent *) {
  disconnect(m_frameHandle, SIGNAL(frameSwitched()), this,
             SLOT(onFrameSwitched()));

  disconnect(m_xshHandle, SIGNAL(xsheetSwitched()), this,
             SLOT(updateStageObjectComboItems()));
  disconnect(m_xshHandle, SIGNAL(xsheetChanged()), this,
             SLOT(updateStageObjectComboItems()));
  disconnect(m_objHandle, SIGNAL(objectSwitched()), this,
             SLOT(syncCurrentStageObjectComboItem()));
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::setSplined(bool on) {
  m_splined = on;
  // Activate on selecting spline
  m_motionPathPosLabel->setVisible(on);
  m_motionPathPosField->setVisible(on);
  // DEactivate on selecting spline
  m_ewPosField->setVisible(!on);
  m_nsPosField->setVisible(!on);
  m_ewPosLabel->setVisible(!on);
  m_nsPosLabel->setVisible(!on);
  m_lockEWPosCheckbox->setVisible(!on);
  m_lockNSPosCheckbox->setVisible(!on);
}

//-----------------------------------------------------------------------------

bool ArrowToolOptionsBox::isCurrentObjectSplined() const {
  TStageObjectId objId = m_objHandle->getObjectId();
  return m_xshHandle->getXsheet()->getStageObject(objId)->getSpline() != 0;
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::updateStatus() {
  // General
  m_chooseActiveAxisCombo->updateStatus();
  m_pickCombo->updateStatus();

  // Position
  m_motionPathPosField->updateStatus();
  m_ewPosField->updateStatus();
  m_nsPosField->updateStatus();
  m_zField->updateStatus();
  m_noScaleZField->updateStatus();
  m_lockEWPosCheckbox->updateStatus();
  m_lockNSPosCheckbox->updateStatus();
  m_soField->updateStatus();

  // Rotation
  m_rotationField->updateStatus();

  // Scale
  m_globalScaleField->updateStatus();
  m_scaleHField->updateStatus();
  m_scaleVField->updateStatus();
  m_lockScaleHCheckbox->updateStatus();
  m_lockScaleVCheckbox->updateStatus();
  m_maintainCombo->updateStatus();

  // Shear
  m_shearHField->updateStatus();
  m_shearVField->updateStatus();
  m_lockShearHCheckbox->updateStatus();
  m_lockShearVCheckbox->updateStatus();

  // Center Position
  m_ewCenterField->updateStatus();
  m_nsCenterField->updateStatus();
  m_lockEWCenterCheckbox->updateStatus();
  m_lockNSCenterCheckbox->updateStatus();

  m_globalKey->updateStatus();

  bool splined = isCurrentObjectSplined();
  if (splined != m_splined) setSplined(splined);
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::onStageObjectChange() { updateStatus(); }

//-----------------------------------------------------------------------------
/*! update the object list in combobox
 */
void ArrowToolOptionsBox::updateStageObjectComboItems() {
  // clear items
  m_currentStageObjectCombo->clear();

  TXsheet *xsh = m_xshHandle->getXsheet();

  TStageObjectId id;
  for (int i = 0; i < xsh->getStageObjectTree()->getStageObjectCount(); i++) {
    id = xsh->getStageObjectTree()->getStageObject(i)->getId();
    if (id == xsh->getStageObjectTree()->getMotionPathViewerId()) continue;
    if (id.isColumn()) {
      int columnIndex = id.getIndex();
      if (xsh->isColumnEmpty(columnIndex) || xsh->isFolderColumn(columnIndex))
        continue;
    }
    TStageObject *pegbar = xsh->getStageObject(id);
    QString itemName     = (id.isTable())
                           ? tr("Table")
                           : QString::fromStdString(pegbar->getFullName());
    // store the item with ObjectId data
    m_currentStageObjectCombo->addItem(itemName, (int)id.getCode());
  }

  // synchronize
  syncCurrentStageObjectComboItem();
}

//------------------------------------------------------------------------------
/*! synchronize the current item in the combobox to the selected stage object
 */
void ArrowToolOptionsBox::syncCurrentStageObjectComboItem() {
  TStageObjectId curObjId = m_objHandle->getObjectId();
  // TXsheet* xsh = m_xshHandle->getXsheet();
  // if (curObjId == xsh->getStageObjectTree()->getMotionPathViewerId()) {
  //    m_objHandle->setObjectId(xsh->getStageObjectTree()->getStageObject(0)->getId());
  //    curObjId = m_objHandle->getObjectId();
  //}
  int index = m_currentStageObjectCombo->findData((int)curObjId.getCode());

  // if the item is found
  if (index >= 0) m_currentStageObjectCombo->setCurrentIndex(index);
  // if not found, add a new item. (This may happens when selecting the empty
  // column.)
  else {
    TStageObject *pegbar = m_xshHandle->getXsheet()->getStageObject(curObjId);
    QString itemName     = QString::fromStdString(pegbar->getFullName());
    std::string itemNameString = itemName.toStdString();
    // store the item with ObjectId data
    if (itemName == "Peg10000") itemName = "Path";
    m_currentStageObjectCombo->addItem(itemName, (int)curObjId.getCode());
    // move the current index to it
    m_currentStageObjectCombo->setCurrentIndex(
        m_currentStageObjectCombo->findText(itemName));
  }
}

//------------------------------------------------------------------------------
/*!change the current stage object when user changes it via combobox by hand
 */
void ArrowToolOptionsBox::onCurrentStageObjectComboActivated(int index) {
  int code = m_currentStageObjectCombo->itemData(index).toInt();
  TStageObjectId id;
  id.setCode(code);
  if (id == TStageObjectId::NoneId) {
    std::cout << "Warning: "
                 "ArrowToolOptionsBox::onCurrentStageObjectComboActivated \nNo "
                 "stage object linked to the selected item found in the scene."
              << std::endl;
    return;
  }
  // switch the current object
  if (id.isCamera()) {
    TXsheet *xsh = m_xshHandle->getXsheet();
    if (xsh->getCameraColumnIndex() != id.getIndex())
      m_xshHandle->changeXsheetCamera(id.getIndex());
  }
  m_objHandle->setObjectId(id);
}

//------------------------------------------------------------------------------

void ArrowToolOptionsBox::onCurrentAxisChanged(int axisId) {
  // Show the specified axis options, hide all the others
  for (int a = 0; a != AllAxis; ++a)
    m_axisOptionWidgets[a]->setVisible(a == axisId || axisId == AllAxis);

  m_pickWidget->setVisible(axisId == AllAxis);
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::onFlipHorizontal() {
  m_scaleHField->setValue(m_scaleHField->getValue() * -1);
  emit m_scaleHField->measuredValueChanged(m_scaleHField->getMeasuredValue());
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::onFlipVertical() {
  m_scaleVField->setValue(m_scaleVField->getValue() * -1);
  emit m_scaleVField->measuredValueChanged(m_scaleVField->getMeasuredValue());
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::onRotateLeft() {
  m_rotationField->setValue(m_rotationField->getValue() + 90);
  emit m_rotationField->measuredValueChanged(
      m_rotationField->getMeasuredValue());
}

//-----------------------------------------------------------------------------

void ArrowToolOptionsBox::onRotateRight() {
  m_rotationField->setValue(m_rotationField->getValue() - 90);
  emit m_rotationField->measuredValueChanged(
      m_rotationField->getMeasuredValue());
}

//=============================================================================
//
// SelectionToolOptionsBox
//
//=============================================================================

IconViewField::IconViewField(QWidget *parent, IconType iconType)
    : DraggableIconView(parent), m_iconType(iconType) {
  setMinimumSize(21, 25);
}

void IconViewField::paintEvent(QPaintEvent *e) {
  QPainter p(this);
  // La pixmap e' alta 17 px, il widget 23. Per centrarla faccio il draw a 3 px.
  p.drawPixmap(QRect(0, 3, 21, 17), m_pm[m_iconType]);
}

void DraggableIconView::mousePressEvent(QMouseEvent *event) {
  emit onMousePress(event);
}

void DraggableIconView::mouseMoveEvent(QMouseEvent *event) {
  emit onMouseMove(event);
}

void DraggableIconView::mouseReleaseEvent(QMouseEvent *event) {
  emit onMouseRelease(event);
}

//-----------------------------------------------------------------------------

SelectionToolOptionsBox::SelectionToolOptionsBox(QWidget *parent, TTool *tool,
                                                 TPaletteHandle *pltHandle,
                                                 ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_tool(tool)
    , m_isVectorSelction(false)
    , m_setSaveboxCheckbox(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  RasterSelectionTool *rasterSelectionTool =
      dynamic_cast<RasterSelectionTool *>(tool);

  SelectionTool *selectionTool = dynamic_cast<SelectionTool *>(tool);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_scaleXLabel = new ClickableLabel(tr("H:"), this);
  m_scaleXField = new SelectionScaleField(selectionTool, 0, "Scale X");
  m_scaleYLabel = new ClickableLabel(tr("V:"), this);
  m_scaleYField = new SelectionScaleField(selectionTool, 1, "Scale Y");
  m_scaleLink   = new DVGui::CheckBox(tr("Link"), this);

  SimpleIconViewField *rotIconView =
      new SimpleIconViewField("edit_rotation", tr("Rotation"));
  m_rotationField = new SelectionRotationField(selectionTool, tr("Rotation"));

  m_moveXLabel = new ClickableLabel(tr("X:"), this);
  m_moveXField = new SelectionMoveField(selectionTool, 0, "Move X");
  m_moveYLabel = new ClickableLabel(tr("Y:"), this);
  m_moveYField = new SelectionMoveField(selectionTool, 1, "Move Y");

  if (rasterSelectionTool) {
    TBoolProperty *modifySetSaveboxProp =
        rasterSelectionTool->getModifySaveboxProperty();
    if (modifySetSaveboxProp)
      m_setSaveboxCheckbox =
          new ToolOptionCheckbox(rasterSelectionTool, modifySetSaveboxProp);
  }

  m_hFlipButton = new QPushButton(this);
  m_vFlipButton = new QPushButton(this);

  m_leftRotateButton  = new QPushButton(this);
  m_rightRotateButton = new QPushButton(this);

  m_hFlipButton->setFixedSize(QSize(20, 20));
  m_vFlipButton->setFixedSize(QSize(20, 20));

  m_leftRotateButton->setFixedSize(QSize(20, 20));
  m_rightRotateButton->setFixedSize(QSize(20, 20));

  m_hFlipButton->setIcon(createQIcon("fliphoriz"));
  m_hFlipButton->setIconSize(QSize(20, 20));
  m_vFlipButton->setIcon(createQIcon("flipvert"));
  m_vFlipButton->setIconSize(QSize(20, 20));
  m_leftRotateButton->setIcon(createQIcon("rotateleft"));
  m_leftRotateButton->setIconSize(QSize(20, 20));
  m_rightRotateButton->setIcon(createQIcon("rotateright"));
  m_rightRotateButton->setIconSize(QSize(20, 20));

  m_hFlipButton->setToolTip(tr("Flip Selection Horizontally"));
  m_vFlipButton->setToolTip(tr("Flip Selection Vertically"));
  m_leftRotateButton->setToolTip(tr("Rotate Selection Left"));
  m_rightRotateButton->setToolTip(tr("Rotate Selection Right"));

  m_scaleXLabel->setEnabled(false);
  m_scaleYLabel->setEnabled(false);
  m_moveXLabel->setEnabled(false);
  m_moveYLabel->setEnabled(false);

  m_hFlipButton->setEnabled(false);
  m_vFlipButton->setEnabled(false);
  m_leftRotateButton->setEnabled(false);
  m_rightRotateButton->setEnabled(false);

  //--- layout ----

  addSeparator();

  hLayout()->addWidget(new SimpleIconViewField("edit_scale", tr("Scale"), this),
                       0);
  hLayout()->addWidget(m_scaleXLabel, 0);
  hLayout()->addWidget(m_scaleXField, 10);
  hLayout()->addWidget(m_hFlipButton);
  hLayout()->addWidget(m_scaleYLabel, 0);
  hLayout()->addWidget(m_scaleYField, 10);
  hLayout()->addWidget(m_vFlipButton);
  hLayout()->addSpacing(4);
  hLayout()->addWidget(m_scaleLink, 0);

  addSeparator();

  hLayout()->addWidget(rotIconView, 0);
  hLayout()->addWidget(m_rotationField, 10);
  hLayout()->addWidget(m_leftRotateButton);
  hLayout()->addWidget(m_rightRotateButton);

  addSeparator();

  hLayout()->addWidget(
      new SimpleIconViewField("edit_position", tr("Position"), this), 0);
  hLayout()->addWidget(m_moveXLabel, 0);
  hLayout()->addWidget(m_moveXField, 10);
  hLayout()->addWidget(m_moveYLabel, 0);
  hLayout()->addWidget(m_moveYField, 10);
  if (m_setSaveboxCheckbox) {
    addSeparator();
    hLayout()->addWidget(m_setSaveboxCheckbox, 0);
  }

  VectorSelectionTool *vectorSelectionTool =
      dynamic_cast<VectorSelectionTool *>(tool);
  if (vectorSelectionTool) {
    m_isVectorSelction = true;

    // change Thick
    SimpleIconViewField *thicknessIconView =
        new SimpleIconViewField("thickness", tr("Thickness"), this);
    m_thickChangeField = new ThickChangeField(selectionTool, tr("Thickness"));

    connect(thicknessIconView, SIGNAL(onMousePress(QMouseEvent *)),
            m_thickChangeField, SLOT(receiveMousePress(QMouseEvent *)));
    connect(thicknessIconView, SIGNAL(onMouseMove(QMouseEvent *)),
            m_thickChangeField, SLOT(receiveMouseMove(QMouseEvent *)));
    connect(thicknessIconView, SIGNAL(onMouseRelease(QMouseEvent *)),
            m_thickChangeField, SLOT(receiveMouseRelease(QMouseEvent *)));

    addSeparator();
    hLayout()->addWidget(thicknessIconView, 0);
    hLayout()->addWidget(m_thickChangeField, 10);
    // Outline options
    ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
    builder.setEnumWidgetType(ToolOptionControlBuilder::POPUPBUTTON);
    builder.setSingleValueWidgetType(ToolOptionControlBuilder::FIELD);

    addSeparator();
    if (tool && tool->getProperties(1)) tool->getProperties(1)->accept(builder);

    m_capStyle = dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Cap"));
    m_joinStyle =
        dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Join"));

    m_miterField =
        dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Miter:"));
    m_miterField->setEnabled(m_joinStyle->currentIndex() ==
                             TStroke::OutlineOptions::MITER_JOIN);

    onPropertyChanged();
  }

  hLayout()->addStretch(1);
  // assert(ret);
  bool ret = connect(m_scaleXField, SIGNAL(valueChange(bool)),
                     SLOT(onScaleXValueChanged(bool)));
  ret = ret && connect(m_scaleYField, SIGNAL(valueChange(bool)),
                       SLOT(onScaleYValueChanged(bool)));
  if (m_setSaveboxCheckbox)
    ret = ret && connect(m_setSaveboxCheckbox, SIGNAL(toggled(bool)),
                         SLOT(onSetSaveboxCheckboxChanged(bool)));
  connect(m_scaleXLabel, SIGNAL(onMousePress(QMouseEvent *)), m_scaleXField,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(m_scaleXLabel, SIGNAL(onMouseMove(QMouseEvent *)), m_scaleXField,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(m_scaleXLabel, SIGNAL(onMouseRelease(QMouseEvent *)), m_scaleXField,
          SLOT(receiveMouseRelease(QMouseEvent *)));
  connect(m_scaleYLabel, SIGNAL(onMousePress(QMouseEvent *)), m_scaleYField,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(m_scaleYLabel, SIGNAL(onMouseMove(QMouseEvent *)), m_scaleYField,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(m_scaleYLabel, SIGNAL(onMouseRelease(QMouseEvent *)), m_scaleYField,
          SLOT(receiveMouseRelease(QMouseEvent *)));
  connect(rotIconView, SIGNAL(onMousePress(QMouseEvent *)), m_rotationField,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(rotIconView, SIGNAL(onMouseMove(QMouseEvent *)), m_rotationField,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(rotIconView, SIGNAL(onMouseRelease(QMouseEvent *)), m_rotationField,
          SLOT(receiveMouseRelease(QMouseEvent *)));
  connect(m_moveXLabel, SIGNAL(onMousePress(QMouseEvent *)), m_moveXField,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(m_moveXLabel, SIGNAL(onMouseMove(QMouseEvent *)), m_moveXField,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(m_moveXLabel, SIGNAL(onMouseRelease(QMouseEvent *)), m_moveXField,
          SLOT(receiveMouseRelease(QMouseEvent *)));
  connect(m_moveYLabel, SIGNAL(onMousePress(QMouseEvent *)), m_moveYField,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(m_moveYLabel, SIGNAL(onMouseMove(QMouseEvent *)), m_moveYField,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(m_moveYLabel, SIGNAL(onMouseRelease(QMouseEvent *)), m_moveYField,
          SLOT(receiveMouseRelease(QMouseEvent *)));
  connect(m_hFlipButton, SIGNAL(clicked()), SLOT(onFlipHorizontal()));
  connect(m_vFlipButton, SIGNAL(clicked()), SLOT(onFlipVertical()));
  connect(m_leftRotateButton, SIGNAL(clicked()), SLOT(onRotateLeft()));
  connect(m_rightRotateButton, SIGNAL(clicked()), SLOT(onRotateRight()));

  connect(selectionTool, SIGNAL(clickFlipHorizontal()),
          SLOT(onFlipHorizontal()));
  connect(selectionTool, SIGNAL(clickFlipVertical()), SLOT(onFlipVertical()));
  connect(selectionTool, SIGNAL(clickRotateLeft()), SLOT(onRotateLeft()));
  connect(selectionTool, SIGNAL(clickRotateRight()), SLOT(onRotateRight()));

  // assert(ret);

  updateStatus();
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();

  if (m_setSaveboxCheckbox) {
    bool disable = m_setSaveboxCheckbox->checkState() == Qt::Checked;
    for (int i = 0; i < hLayout()->count(); i++) {
      QWidget *w = hLayout()->itemAt(i)->widget();
      if (w && w != m_setSaveboxCheckbox) w->setEnabled(!disable);
    }
    if (disable) return;
  }

  m_scaleXField->updateStatus();
  m_scaleXLabel->setEnabled(m_scaleXField->isEnabled());
  m_scaleYField->updateStatus();
  m_scaleYLabel->setEnabled(m_scaleXField->isEnabled());
  m_rotationField->updateStatus();
  m_moveXField->updateStatus();
  m_moveXLabel->setEnabled(m_moveXField->isEnabled());
  m_moveYField->updateStatus();
  m_moveYLabel->setEnabled(m_moveYField->isEnabled());

  m_hFlipButton->setEnabled(m_scaleXField->isEnabled());
  m_vFlipButton->setEnabled(m_scaleXField->isEnabled());
  m_leftRotateButton->setEnabled(m_rotationField->isEnabled());
  m_rightRotateButton->setEnabled(m_rotationField->isEnabled());

  if (m_isVectorSelction) {
    m_thickChangeField->updateStatus();
    onPropertyChanged();
  }
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onScaleXValueChanged(bool addToUndo) {
  if (!m_scaleLink->isChecked() ||
      m_scaleXField->getValue() == m_scaleYField->getValue())
    return;
  m_scaleYField->setValue(m_scaleXField->getValue());
  m_scaleYField->applyChange(addToUndo);
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onScaleYValueChanged(bool addToUndo) {
  if (!m_scaleLink->isChecked() ||
      m_scaleXField->getValue() == m_scaleYField->getValue())
    return;
  m_scaleXField->setValue(m_scaleYField->getValue());
  m_scaleXField->applyChange(addToUndo);
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onSetSaveboxCheckboxChanged(bool) {
  updateStatus();
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onFlipHorizontal() {
  m_scaleXField->setValue(m_scaleXField->getValue() * -1);
  m_scaleXField->applyChange(true);

  onScaleXValueChanged(true);
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onFlipVertical() {
  m_scaleYField->setValue(m_scaleYField->getValue() * -1);
  m_scaleYField->applyChange(true);

  onScaleYValueChanged(true);
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onRotateLeft() {
  m_rotationField->setValue(m_rotationField->getValue() + 90);
  m_rotationField->applyChange(true);
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onRotateRight() {
  m_rotationField->setValue(m_rotationField->getValue() - 90);
  m_rotationField->applyChange(true);
}

//-----------------------------------------------------------------------------

void SelectionToolOptionsBox::onPropertyChanged() {
  // Check the selection's outline styles group.
  VectorSelectionTool *tool = (VectorSelectionTool *)m_tool;

  int capStyle, joinStyle;
  tool->selectionOutlineStyle(capStyle, joinStyle);

  // Show a void icon when no homogeneous style is found
  if (capStyle < 0)
    m_capStyle->setIcon(QPixmap());
  else
    // m_capStyle->setIcon(m_capStyle->currentItem()->icon());
    m_capStyle->setCurrentIndex(capStyle);

  if (joinStyle < 0)
    m_joinStyle->setIcon(QPixmap());
  else
    m_joinStyle->setCurrentIndex(joinStyle);
  // m_joinStyle->setIcon(m_joinStyle->currentItem()->icon());

  // Enable the miter field only in case the join is of type miter
  m_miterField->setEnabled(joinStyle == TStroke::OutlineOptions::MITER_JOIN);
}

//=============================================================================
//
// GeometricToolOptionsBox
//
//=============================================================================

GeometricToolOptionsBox::GeometricToolOptionsBox(QWidget *parent, TTool *tool,
                                                 TPaletteHandle *pltHandle,
                                                 ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_targetType(tool->getTargetType())
    , m_poligonSideLabel(0)
    , m_hardnessLabel(0)
    , m_hardnessField(0)
    , m_poligonSideField(0)
    , m_shapeField(0)
    , m_snapCheckbox(0)
    , m_smoothCheckbox(0)
    , m_snapSensitivityCombo(0)
    , m_tool(tool)
    , m_pencilMode(0) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);
  builder.setEnumWidgetType(ToolOptionControlBuilder::POPUPBUTTON);
  builder.setSingleValueWidgetType(ToolOptionControlBuilder::FIELD);
  addSeparator();
  if (tool && tool->getProperties(1)) tool->getProperties(1)->accept(builder);

  hLayout()->addStretch(1);
  m_hardnessField =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Hardness:"));
  if (m_hardnessField)
    m_hardnessLabel = m_labels.value(m_hardnessField->propertyName());

  m_shapeField = dynamic_cast<ToolOptionCombo *>(m_controls.value("Shape:"));
  m_poligonSideField =
      dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Polygon Sides:"));
  if (m_poligonSideField)
    m_poligonSideLabel = m_labels.value(m_poligonSideField->propertyName());
  m_pencilMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Pencil Mode"));

  if (m_shapeField->getProperty()->getValue() != L"Polygon") {
    m_poligonSideLabel->setEnabled(false);
    m_poligonSideField->setEnabled(false);
  }

  m_smoothCheckbox =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Smooth"));
  if (m_shapeField->getProperty()->getValue() != L"MultiArc") {
    m_smoothCheckbox->setEnabled(false);
  }

  bool ret = connect(m_shapeField, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onShapeValueChanged(int)));

  if (m_pencilMode) {
    if (m_pencilMode->isChecked()) {
      m_hardnessLabel->setEnabled(false);
      m_hardnessField->setEnabled(false);
    }
    ret = ret && connect(m_pencilMode, SIGNAL(toggled(bool)), this,
                         SLOT(onPencilModeToggled(bool)));
  }

  if (tool->getTargetType() & TTool::Vectors) {
    m_drawOrderCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Draw Under"));
    m_snapCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Snap"));
    m_snapSensitivityCombo =
        dynamic_cast<ToolOptionCombo *>(m_controls.value("Sensitivity:"));
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
  }

  ToolOptionPopupButton *m_joinStyle =
      dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Join"));
  m_miterField =
      dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Miter:"));
  m_miterField->setEnabled(m_joinStyle->currentIndex() ==
                           TStroke::OutlineOptions::MITER_JOIN);

  assert(m_joinStyle && m_miterField);
  connect(m_joinStyle, SIGNAL(activated(int)), this,
          SLOT(onJoinStyleChanged(int)));

  assert(ret);
  filterControls();
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::filterControls() {
  // show or hide widgets which modify imported brush (mypaint)
  bool isMyPaintSelected = m_tool->isMyPaintStyleSelected();

  for (QMap<std::string, QLabel *>::iterator it = m_labels.begin();
       it != m_labels.end(); it++) {
    bool visible =
        !isMyPaintSelected || (m_tool->getTargetType() & TTool::Vectors)
            ? (it.key().substr(0, 8) != "Modifier")
            : ((it.key().substr(0, 8) == "Modifier") ||
               (it.key() == "Opacity:") || (it.key() == "Shape:") ||
               (it.key() == "Polygon Sides:") || (it.key() == "rotate") ||
               (it.key() == "Snap") || (it.key() == "Draw Under") ||
               (it.key() == "Smooth" || (it.key() == "Range:")));
    it.value()->setVisible(visible);
  }

  for (QMap<std::string, ToolOptionControl *>::iterator it = m_controls.begin();
       it != m_controls.end(); it++) {
    bool visible =
        !isMyPaintSelected || (m_tool->getTargetType() & TTool::Vectors)
            ? (it.key().substr(0, 8) != "Modifier")
            : ((it.key().substr(0, 8) == "Modifier") ||
               (it.key() == "Opacity:") || (it.key() == "Shape:") ||
               (it.key() == "Polygon Sides:") || (it.key() == "rotate") ||
               (it.key() == "Snap") || (it.key() == "Draw Under") ||
               (it.key() == "Smooth" || (it.key() == "Range:")));
    if (QWidget *widget = dynamic_cast<QWidget *>(it.value()))
      widget->setVisible(visible);
  }
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::updateStatus() {
  filterControls();

  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
  if (m_tool->getTargetType() & TTool::Vectors) {
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
  }
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::onShapeValueChanged(int index) {
  const TEnumProperty::Range &range = m_shapeField->getProperty()->getRange();
  bool polygonEnabled               = range[index] == L"Polygon";
  m_poligonSideLabel->setEnabled(polygonEnabled);
  m_poligonSideField->setEnabled(polygonEnabled);

  m_smoothCheckbox->setEnabled(range[index] == L"MultiArc");
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::onPencilModeToggled(bool value) {
  m_hardnessLabel->setEnabled(!value);
  m_hardnessField->setEnabled(!value);
}

//-----------------------------------------------------------------------------

void GeometricToolOptionsBox::onJoinStyleChanged(int joinStyle) {
  m_miterField->setEnabled(joinStyle == TStroke::OutlineOptions::MITER_JOIN);
}

//=============================================================================
//
// TypeToolOptionsBox
//
//=============================================================================

TypeToolOptionsBox::TypeToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent), m_tool(tool) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  builder.setEnumWidgetType(ToolOptionControlBuilder::FONTCOMBOBOX);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);
  builder.setEnumWidgetType(ToolOptionControlBuilder::COMBOBOX);
  if (tool && tool->getProperties(1)) tool->getProperties(1)->accept(builder);

  m_layout->addStretch(0);

  bool ret = true;

  ToolOptionFontCombo *fontField =
      dynamic_cast<ToolOptionFontCombo *>(m_controls.value("Font:"));
  ret &&connect(fontField, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onFieldChanged()));

  //#ifndef MACOSX
  ToolOptionCombo *styleField =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Style:"));
  ret &&connect(styleField, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onFieldChanged()));
  ret &&connect(toolHandle, SIGNAL(toolComboBoxListChanged(std::string)),
                styleField, SLOT(reloadComboBoxList(std::string)));
  //#endif

  ToolOptionCombo *sizeField =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Size:"));
  ret &&connect(sizeField, SIGNAL(currentIndexChanged(int)), this,
                SLOT(onFieldChanged()));

  ToolOptionCheckbox *orientationField = dynamic_cast<ToolOptionCheckbox *>(
      m_controls.value("Vertical Orientation"));
  ret = ret && connect(orientationField, SIGNAL(stateChanged(int)), this,
                       SLOT(onFieldChanged()));

  assert(ret);
}

//-----------------------------------------------------------------------------

void TypeToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void TypeToolOptionsBox::onFieldChanged() {
  assert(m_tool);
  m_tool->getViewer()->setFocus();
}

//=============================================================================
//
// PaintbrushToolOptionsBox
//
//=============================================================================

PaintbrushToolOptionsBox::PaintbrushToolOptionsBox(QWidget *parent, TTool *tool,
                                                   TPaletteHandle *pltHandle,
                                                   ToolHandle *toolHandle)
    : ToolOptionsBox(parent) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);
  hLayout()->addStretch(1);

  m_colorMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode:"));
  m_selectiveMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Selective"));
  m_lockAlphaMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Lock Alpha"));

  if (m_colorMode->getProperty()->getValue() == L"Lines") {
    m_selectiveMode->setVisible(false);
    m_lockAlphaMode->setVisible(false);
  }

  bool ret = connect(m_colorMode, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onColorModeChanged(int)));
  assert(ret);
}

//-----------------------------------------------------------------------------

void PaintbrushToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void PaintbrushToolOptionsBox::onColorModeChanged(int index) {
  const TEnumProperty::Range &range = m_colorMode->getProperty()->getRange();
  bool enabled                      = range[index] != L"Lines";
  m_selectiveMode->setVisible(enabled);
  m_lockAlphaMode->setVisible(enabled);
}

//=============================================================================
//
// FullColorFillToolOptionsBox
//
//=============================================================================

FullColorFillToolOptionsBox::FullColorFillToolOptionsBox(
    QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
    ToolHandle *toolHandle)
    : ToolOptionsBox(parent) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_rasterGapSettings =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Gaps:"));
  m_rasterGapSlider =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Distance:"));
  m_styleIndex =
      dynamic_cast<StyleIndexFieldAndChip *>(m_controls.value("Style Index:"));
  m_rasterGapLabel  = m_labels.value(m_rasterGapSettings->propertyName());
  m_styleIndexLabel = m_labels.value(m_styleIndex->propertyName());
  m_gapSliderLabel  = m_labels.value(m_rasterGapSlider->propertyName());

  bool ret = connect(m_rasterGapSettings, SIGNAL(currentIndexChanged(int)),
                     this, SLOT(onGapSettingChanged(int)));

  checkGapSettingsVisibility();

  m_layout->addStretch(0);
}

//-----------------------------------------------------------------------------

void FullColorFillToolOptionsBox::checkGapSettingsVisibility() {
  if (m_rasterGapSettings->getProperty()->getValue() == L"Ignore Gaps") {
    m_styleIndex->hide();
    m_styleIndexLabel->hide();
    m_rasterGapSlider->hide();
    m_gapSliderLabel->hide();
  } else if (m_rasterGapSettings->getProperty()->getValue() == L"Fill Gaps") {
    m_styleIndex->hide();
    m_styleIndexLabel->hide();
    m_rasterGapSlider->show();
    m_gapSliderLabel->show();
  } else if (m_rasterGapSettings->getProperty()->getValue() ==
             L"Close and Fill") {
    m_styleIndex->show();
    m_styleIndexLabel->show();
    m_rasterGapSlider->show();
    m_gapSliderLabel->show();
  }
}

//-----------------------------------------------------------------------------

void FullColorFillToolOptionsBox::onGapSettingChanged(int index) {
  if (index == 0) {
    m_styleIndex->hide();
    m_styleIndexLabel->hide();
    m_rasterGapSlider->hide();
    m_gapSliderLabel->hide();
  } else if (index == 1) {
    m_styleIndex->hide();
    m_styleIndexLabel->hide();
    m_rasterGapSlider->show();
    m_gapSliderLabel->show();
  } else if (index == 2) {
    m_styleIndex->show();
    m_styleIndexLabel->show();
    m_rasterGapSlider->show();
    m_gapSliderLabel->show();
  }
}

//=============================================================================
//
// FillToolOptionsBox
//
//=============================================================================

FillToolOptionsBox::FillToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_targetType(tool->getTargetType())
    , m_fillDepthLabel(0)
    , m_fillDepthField(0)
    , m_gapSliderLabel(0)
    , m_styleIndexLabel(0)
    , m_segmentMode(0)
    , m_referenced(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_layout->addStretch(0);

  m_toolType  = dynamic_cast<ToolOptionCombo *>(m_controls.value("Type:"));
  m_colorMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode:"));
  m_selectiveMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Selective"));
  m_fillDepthField =
      dynamic_cast<ToolOptionPairSlider *>(m_controls.value("Fill Depth"));
  if (m_fillDepthField)
    m_fillDepthLabel = m_labels.value(m_fillDepthField->propertyName());
  m_segmentMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Segment"));
  m_onionMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Onion Skin"));
  m_referenced =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Refer Visible"));
  m_multiFrameMode =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Frame Range:"));
  m_autopaintMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Autopaint Lines"));
  m_rasterGapSettings =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Gaps:"));
  m_rasterGapSlider =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Distance:"));
  m_styleIndex =
      dynamic_cast<StyleIndexFieldAndChip *>(m_controls.value("Style Index:"));
  if (m_rasterGapSettings)
    m_rasterGapLabel = m_labels.value(m_rasterGapSettings->propertyName());
  if (m_styleIndex)
    m_styleIndexLabel = m_labels.value(m_styleIndex->propertyName());
  if (m_rasterGapSlider)
    m_gapSliderLabel = m_labels.value(m_rasterGapSlider->propertyName());

  bool ret = connect(m_colorMode, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onColorModeChanged(int)));
  ret = ret && connect(m_toolType, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onToolTypeChanged(int)));
  ret = ret && connect(m_onionMode, SIGNAL(toggled(bool)), this,
                       SLOT(onOnionModeToggled(bool)));
  ret = ret && connect(m_multiFrameMode, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onMultiFrameModeChanged(int)));
  if (m_targetType == TTool::ToonzImage) {
    ret = ret && connect(m_rasterGapSettings, SIGNAL(currentIndexChanged(int)),
                         this, SLOT(onGapSettingChanged(int)));
  }
  assert(ret);

  checkGapSettingsVisibility();

  if (m_colorMode->getProperty()->getValue() == L"Lines") {
    m_selectiveMode->setEnabled(false);
    if (m_fillDepthLabel && m_fillDepthField) {
      m_fillDepthLabel->setEnabled(false);
      m_fillDepthField->setEnabled(false);
    }
    if (m_toolType->getProperty()->getValue() == L"Normal" ||
        m_multiFrameMode->getProperty()->getIndex())
      m_onionMode->setEnabled(false);
    if (m_autopaintMode) m_autopaintMode->setEnabled(false);
    if (m_referenced) m_referenced->setEnabled(false);
  }
  if (m_toolType->getProperty()->getValue() != L"Normal") {
    if (m_segmentMode) m_segmentMode->setEnabled(false);
    if (m_colorMode->getProperty()->getValue() == L"Lines" ||
        m_multiFrameMode->getProperty()->getIndex())
      m_onionMode->setEnabled(false);
  }
  if (m_onionMode->isChecked()) m_multiFrameMode->setEnabled(false);
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onColorModeChanged(int index) {
  const TEnumProperty::Range &range = m_colorMode->getProperty()->getRange();
  bool enabled                      = range[index] != L"Lines";
  m_selectiveMode->setEnabled(enabled);
  if (m_autopaintMode) m_autopaintMode->setEnabled(enabled);
  if (m_fillDepthLabel && m_fillDepthField) {
    m_fillDepthLabel->setEnabled(enabled);
    m_fillDepthField->setEnabled(enabled);
  }
  if (m_segmentMode) {
    enabled = range[index] != L"Areas";
    m_segmentMode->setEnabled(
        enabled ? m_toolType->getProperty()->getValue() == L"Normal" : false);
  }
  enabled =
      range[index] != L"Lines" && !m_multiFrameMode->getProperty()->getIndex();
  m_onionMode->setEnabled(enabled);
  if (m_referenced) m_referenced->setEnabled(enabled);
  checkGapSettingsVisibility();
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::checkGapSettingsVisibility() {
  if (m_targetType == TTool::ToonzImage) {
    m_rasterGapSettings->show();
    m_rasterGapLabel->show();
    const TEnumProperty::Range &range = m_colorMode->getProperty()->getRange();
    if (range[m_colorMode->currentIndex()] == L"Lines") {
      m_rasterGapSettings->hide();
      m_rasterGapLabel->hide();
      m_styleIndex->hide();
      m_styleIndexLabel->hide();
      m_rasterGapSlider->hide();
      m_gapSliderLabel->hide();
      return;
    }

    else if (m_rasterGapSettings->getProperty()->getValue() == L"Ignore Gaps") {
      if (m_styleIndex) {
        m_styleIndex->hide();
        m_styleIndexLabel->hide();
      }
      if (m_rasterGapSlider) {
        m_rasterGapSlider->hide();
        m_gapSliderLabel->hide();
      }
    } else if (m_rasterGapSettings->getProperty()->getValue() == L"Fill Gaps") {
      if (m_styleIndex) {
        m_styleIndex->hide();
        m_styleIndexLabel->hide();
      }
      if (m_rasterGapSlider) {
        m_rasterGapSlider->show();
        m_gapSliderLabel->show();
      }
    } else if (m_rasterGapSettings->getProperty()->getValue() ==
               L"Close and Fill") {
      if (m_styleIndex) {
        m_styleIndex->show();
        m_styleIndexLabel->show();
      }
      if (m_rasterGapSlider) {
        m_rasterGapSlider->show();
        m_gapSliderLabel->show();
      }
    }
  }
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onToolTypeChanged(int index) {
  const TEnumProperty::Range &range = m_toolType->getProperty()->getRange();
  bool enabled                      = range[index] == L"Normal";
  if (m_referenced) m_referenced->setEnabled(enabled);
  if (m_segmentMode)
    m_segmentMode->setEnabled(
        enabled ? m_colorMode->getProperty()->getValue() != L"Areas" : false);
  enabled = enabled || (m_colorMode->getProperty()->getValue() != L"Lines" &&
                        !m_multiFrameMode->getProperty()->getIndex());
  m_onionMode->setEnabled(enabled);
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onGapSettingChanged(int index) {
  if (index == 0) {
    if (m_styleIndex) {
      m_styleIndex->hide();
      m_styleIndexLabel->hide();
    }
    if (m_rasterGapSlider) {
      m_rasterGapSlider->hide();
      m_gapSliderLabel->hide();
    }
  } else if (index == 1) {
    if (m_styleIndex) {
      m_styleIndex->hide();
      m_styleIndexLabel->hide();
    }
    if (m_rasterGapSlider) {
      m_rasterGapSlider->show();
      m_gapSliderLabel->hide();
    }
  } else if (index == 2) {
    if (m_styleIndex) {
      m_styleIndex->show();
      m_styleIndexLabel->show();
    }
    if (m_rasterGapSlider) {
      m_rasterGapSlider->show();
      m_gapSliderLabel->show();
    }
  }
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onOnionModeToggled(bool value) {
  m_multiFrameMode->setEnabled(!value);
}

//-----------------------------------------------------------------------------

void FillToolOptionsBox::onMultiFrameModeChanged(int value) {
  m_onionMode->setEnabled(!value);
}

//=============================================================================
//
// BrushToolOptionsBox classes
//
//=============================================================================

class BrushToolOptionsBox::PresetNamePopup final : public DVGui::Dialog {
  DVGui::LineEdit *m_nameFld;

public:
  PresetNamePopup() : Dialog(0, true) {
    setWindowTitle(tr("Preset Name"));
    m_nameFld = new DVGui::LineEdit();
    addWidget(m_nameFld);

    QPushButton *okBtn = new QPushButton(tr("OK"), this);
    okBtn->setDefault(true);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

    addButtonBarWidget(okBtn, cancelBtn);
  }

  QString getName() { return m_nameFld->text(); }
  void removeName() { m_nameFld->setText(QString("")); }
};

//=============================================================================
//
// BrushToolOptionsBox
//
//=============================================================================

BrushToolOptionsBox::BrushToolOptionsBox(QWidget *parent, TTool *tool,
                                         TPaletteHandle *pltHandle,
                                         ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_tool(tool)
    , m_presetNamePopup(0)
    , m_pencilMode(0)
    , m_hardnessLabel(0)
    , m_joinStyleCombo(0)
    , m_snapCheckbox(0)
    , m_snapSensitivityCombo(0)
    , m_drawOrderCheckbox(0)
    , m_miterField(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_hardnessField =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Hardness:"));
  if (m_hardnessField)
    m_hardnessLabel = m_labels.value(m_hardnessField->propertyName());
  m_pencilMode = dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Pencil"));
  m_presetCombo = dynamic_cast<ToolOptionCombo *>(m_controls.value("Preset:"));

  // Preset +/- buttons
  m_addPresetButton    = new QPushButton(QString("+"));
  m_removePresetButton = new QPushButton(QString("-"));

  m_addPresetButton->setFixedSize(QSize(20, 20));
  m_removePresetButton->setFixedSize(QSize(20, 20));

  hLayout()->addWidget(m_addPresetButton);
  hLayout()->addWidget(m_removePresetButton);

  connect(m_addPresetButton, SIGNAL(clicked()), this, SLOT(onAddPreset()));
  connect(m_removePresetButton, SIGNAL(clicked()), this,
          SLOT(onRemovePreset()));

  if (tool->getTargetType() & TTool::ToonzImage) {
    assert(m_pencilMode);
    bool ret = connect(m_pencilMode, SIGNAL(toggled(bool)), this,
                       SLOT(onPencilModeToggled(bool)));
    assert(ret);

    if (m_pencilMode->isChecked()) {
      m_hardnessLabel->setEnabled(false);
      m_hardnessField->setEnabled(false);
    }
  } else if (tool->getTargetType() & TTool::Vectors) {
    // Further vector options
    builder.setEnumWidgetType(ToolOptionControlBuilder::POPUPBUTTON);
    builder.setSingleValueWidgetType(ToolOptionControlBuilder::FIELD);

    addSeparator();
    m_drawOrderCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Draw Under"));
    m_autoCloseCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Auto Close"));
    m_autoGroupCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Auto Group"));
    m_autoFillCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Auto Fill"));
    m_snapCheckbox =
        dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Snap"));
    m_snapSensitivityCombo =
        dynamic_cast<ToolOptionCombo *>(m_controls.value("Sensitivity:"));

    if (tool && tool->getProperties(1)) tool->getProperties(1)->accept(builder);
    m_joinStyleCombo =
        dynamic_cast<ToolOptionPopupButton *>(m_controls.value("Join"));
    m_miterField =
        dynamic_cast<ToolOptionIntSlider *>(m_controls.value("Miter:"));
    m_miterField->setEnabled(m_joinStyleCombo->currentIndex() ==
                             TStroke::OutlineOptions::MITER_JOIN);
  }
  hLayout()->addStretch(1);
  filterControls();
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::filterControls() {
  // show or hide widgets which modify imported brush (mypaint)

  bool showModifiers = false;
  if (m_tool->getTargetType() & TTool::RasterImage) {
    FullColorBrushTool *fullColorBrushTool =
        dynamic_cast<FullColorBrushTool *>(m_tool);
    showModifiers = fullColorBrushTool->getBrushStyle();
  } else if (m_tool->getTargetType() & TTool::ToonzImage) {
    showModifiers = m_tool->isMyPaintStyleSelected();
  } else {  // (m_tool->getTargetType() & TTool::Vectors)
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
    return;
  }

  for (QMap<std::string, QLabel *>::iterator it = m_labels.begin();
       it != m_labels.end(); it++) {
    bool isModifier = (it.key().substr(0, 8) == "Modifier");
    bool isCommon =
        (it.key() == "Lock Alpha" || it.key() == "Pressure" ||
         it.key() == "Preset:" || it.key() == "Grid" || it.key() == "Smooth:");
    bool visible = isCommon || (isModifier == showModifiers);
    it.value()->setVisible(visible);
  }

  for (QMap<std::string, ToolOptionControl *>::iterator it = m_controls.begin();
       it != m_controls.end(); it++) {
    bool isModifier = (it.key().substr(0, 8) == "Modifier");
    bool isCommon =
        (it.key() == "Lock Alpha" || it.key() == "Pressure" ||
         it.key() == "Preset:" || it.key() == "Grid" || it.key() == "Smooth:");
    bool visible = isCommon || (isModifier == showModifiers);
    if (QWidget *widget = dynamic_cast<QWidget *>(it.value()))
      widget->setVisible(visible);
  }
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::updateStatus() {
  filterControls();

  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();

  if (m_miterField)
    m_miterField->setEnabled(m_joinStyleCombo->currentIndex() ==
                             TStroke::OutlineOptions::MITER_JOIN);
  if (m_snapCheckbox)
    m_snapSensitivityCombo->setHidden(!m_snapCheckbox->isChecked());
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::onPencilModeToggled(bool value) {
  m_hardnessLabel->setEnabled(!value);
  m_hardnessField->setEnabled(!value);
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::onAddPreset() {
  // Initialize preset name popup
  if (!m_presetNamePopup) m_presetNamePopup = new PresetNamePopup;

  if (!m_presetNamePopup->getName().isEmpty()) m_presetNamePopup->removeName();

  // Retrieve the preset name
  bool ret = m_presetNamePopup->exec();
  if (!ret) return;

  QString name(m_presetNamePopup->getName());
  m_presetNamePopup->removeName();

  switch (m_tool->getTargetType() & TTool::CommonImages) {
  case TTool::VectorImage: {
    static_cast<ToonzVectorBrushTool *>(m_tool)->addPreset(name);
    break;
  }
  case TTool::ToonzImage: {
    static_cast<ToonzRasterBrushTool *>(m_tool)->addPreset(name);
    break;
  }

  case TTool::RasterImage: {
    static_cast<FullColorBrushTool *>(m_tool)->addPreset(name);
    break;
  }
  }

  m_presetCombo->loadEntries();
}

//-----------------------------------------------------------------------------

void BrushToolOptionsBox::onRemovePreset() {
  switch (m_tool->getTargetType() & TTool::CommonImages) {
  case TTool::VectorImage: {
    static_cast<ToonzVectorBrushTool *>(m_tool)->removePreset();
    break;
  }
  case TTool::ToonzImage: {
    static_cast<ToonzRasterBrushTool *>(m_tool)->removePreset();
    break;
  }

  case TTool::RasterImage: {
    static_cast<FullColorBrushTool *>(m_tool)->removePreset();
    break;
  }
  }

  m_presetCombo->loadEntries();
}

//=============================================================================
//
// EraserToolOptionsBox
//
//=============================================================================

EraserToolOptionsBox::EraserToolOptionsBox(QWidget *parent, TTool *tool,
                                           TPaletteHandle *pltHandle,
                                           ToolHandle *toolHandle)
    : ToolOptionsBox(parent), m_pencilMode(0), m_colorMode(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  hLayout()->addStretch(1);

  m_toolType = dynamic_cast<ToolOptionCombo *>(m_controls.value("Type:"));
  m_hardnessField =
      dynamic_cast<ToolOptionSlider *>(m_controls.value("Hardness:"));
  if (m_hardnessField)
    m_hardnessLabel = m_labels.value(m_hardnessField->propertyName());
  m_colorMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode:"));
  if (m_colorMode)
    m_colorModeLabel = m_labels.value(m_colorMode->propertyName());
  m_pressure = dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Pressure"));
  m_invertMode = dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Invert"));
  m_multiFrameMode =
      dynamic_cast<ToolOptionCombo *>(m_controls.value("Frame Range:"));
  m_pencilMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Pencil Mode"));
  m_eraseOnlySavebox =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Savebox"));

  bool ret = true;
  if (m_pencilMode) {
    ret = ret && connect(m_pencilMode, SIGNAL(toggled(bool)), this,
                         SLOT(onPencilModeToggled(bool)));
    ret = ret && connect(m_colorMode, SIGNAL(currentIndexChanged(int)), this,
                         SLOT(onColorModeChanged(int)));
  }

  ret = ret && connect(m_toolType, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onToolTypeChanged(int)));

  if (m_pencilMode && m_pencilMode->isChecked()) {
    assert(m_hardnessField && m_hardnessLabel);
    m_hardnessField->setEnabled(false);
    m_hardnessLabel->setEnabled(false);
  }

  if (m_toolType && m_toolType->getProperty()->getValue() == L"Normal") {
    m_invertMode->setEnabled(false);
    m_multiFrameMode->setEnabled(false);
    m_pressure->setEnabled(true);
  }

  if (m_toolType && m_toolType->getProperty()->getValue() == L"Segment") {
    if (m_colorMode) {
      m_colorMode->setEnabled(false);
      m_colorModeLabel->setEnabled(false);
    }
    m_invertMode->setEnabled(false);
    m_pressure->setEnabled(false);
  }

  if (m_eraseOnlySavebox) {
    if (m_toolType->getProperty()->getValue() == L"Segment")
      m_eraseOnlySavebox->setEnabled(true);
    else
      m_eraseOnlySavebox->setEnabled(false);
  }

  if (m_colorMode && m_colorMode->getProperty()->getValue() == L"Areas") {
    assert(m_hardnessField && m_hardnessLabel && m_pencilMode);
    m_pencilMode->setEnabled(false);
    m_hardnessField->setEnabled(false);
    m_hardnessLabel->setEnabled(false);
  }
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::onPencilModeToggled(bool value) {
  if (m_hardnessField && m_hardnessLabel) {
    m_hardnessField->setEnabled(!value);
    m_hardnessLabel->setEnabled(!value);
  }
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::onToolTypeChanged(int index) {
  const TEnumProperty::Range &range = m_toolType->getProperty()->getRange();
  bool value                        = range[index] != L"Normal";
  m_multiFrameMode->setEnabled(value);
  bool isSegment = range[index] == L"Segment";
  if (m_colorMode) {
    m_colorMode->setDisabled(isSegment);
    m_colorModeLabel->setDisabled(isSegment);
  }
  m_pressure->setEnabled(!value);
  m_invertMode->setEnabled(!isSegment && value);
  if (m_eraseOnlySavebox) m_eraseOnlySavebox->setEnabled(isSegment);
}

//-----------------------------------------------------------------------------

void EraserToolOptionsBox::onColorModeChanged(int index) {
  const TEnumProperty::Range &range = m_colorMode->getProperty()->getRange();
  bool enabled                      = range[index] != L"Areas";
  if (m_pencilMode && m_hardnessField && m_hardnessLabel) {
    m_pencilMode->setEnabled(enabled);
    m_hardnessField->setEnabled(enabled ? !m_pencilMode->isChecked() : false);
    m_hardnessLabel->setEnabled(enabled ? !m_pencilMode->isChecked() : false);
  }
}

//=============================================================================
//
// RulerToolOptionsBox
//
//=============================================================================
class ToolOptionsBarSeparator final : public QWidget {
public:
  ToolOptionsBarSeparator(QWidget *parent) : QWidget(parent) {
    setFixedSize(2, 26);
  }

  void paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setPen(QColor(64, 64, 64));
    p.drawLine(0, 0, 0, 25);
    p.setPen(QColor(0, 0, 0, 125));
    p.drawLine(1, 0, 1, 25);
  }
};

RulerToolOptionsBox::RulerToolOptionsBox(QWidget *parent, TTool *tool)
    : ToolOptionsBox(parent), m_tool(tool) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_Xfld = new MeasuredValueField(this);
  m_Yfld = new MeasuredValueField(this);
  m_Wfld = new MeasuredValueField(this);
  m_Hfld = new MeasuredValueField(this);
  m_Afld = new MeasuredValueField(this);
  m_Lfld = new MeasuredValueField(this);

  m_XpixelFld = new QLabel(this);
  m_YpixelFld = new QLabel(this);
  m_WpixelFld = new QLabel(this);
  m_HpixelFld = new QLabel(this);

  m_Afld->setMeasure("angle");

  if (Preferences::instance()->getUnits() == "pixel") {
    m_Xfld->setMeasure("length.x");
    m_Yfld->setMeasure("length.y");
    m_Wfld->setMeasure("length.x");
    m_Hfld->setMeasure("length.y");
    m_Lfld->setMeasure("length.x");
  }

  m_Xfld->setObjectName("RulerToolOptionValues");
  m_Yfld->setObjectName("RulerToolOptionValues");
  m_Wfld->setObjectName("RulerToolOptionValues");
  m_Hfld->setObjectName("RulerToolOptionValues");
  m_Afld->setObjectName("RulerToolOptionValues");
  m_Lfld->setObjectName("RulerToolOptionValues");
  setStyleSheet(
      "#RulerToolOptionValues {border:0px; background: rgb(196,196,196);}");

  m_Xfld->setMaximumWidth(70);
  m_Yfld->setMaximumWidth(70);
  m_Wfld->setMaximumWidth(70);
  m_Hfld->setMaximumWidth(70);
  m_Afld->setMaximumWidth(70);
  m_Lfld->setMaximumWidth(70);

  m_Xfld->setReadOnly(true);
  m_Yfld->setReadOnly(true);
  m_Wfld->setReadOnly(true);
  m_Hfld->setReadOnly(true);
  m_Afld->setReadOnly(true);
  m_Lfld->setReadOnly(true);

  // layout
  QHBoxLayout *lay = new QHBoxLayout();
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(3);
  {
    lay->addWidget(new QLabel(tr("X:", "ruler tool option"), this), 0);
    lay->addWidget(m_Xfld, 0);
    lay->addWidget(m_XpixelFld, 0);

    lay->addSpacing(3);

    lay->addWidget(new QLabel(tr("Y:", "ruler tool option"), this), 0);
    lay->addWidget(m_Yfld, 0);
    lay->addWidget(m_YpixelFld, 0);

    lay->addSpacing(3);
    lay->addWidget(new ToolOptionsBarSeparator(this), 0);
    lay->addSpacing(3);

    lay->addWidget(new QLabel(tr("W:", "ruler tool option"), this), 0);
    lay->addWidget(m_Wfld, 0);
    lay->addWidget(m_WpixelFld, 0);

    lay->addSpacing(3);

    lay->addWidget(new QLabel(tr("H:", "ruler tool option"), this), 0);
    lay->addWidget(m_Hfld, 0);
    lay->addWidget(m_HpixelFld, 0);

    lay->addSpacing(3);
    lay->addWidget(new ToolOptionsBarSeparator(this), 0);
    lay->addSpacing(3);

    lay->addWidget(new QLabel(tr("A:", "ruler tool option"), this), 0);
    lay->addWidget(m_Afld, 0);

    lay->addSpacing(3);

    lay->addWidget(new QLabel(tr("L:", "ruler tool option"), this), 0);
    lay->addWidget(m_Lfld, 0);
  }
  m_layout->addLayout(lay, 0);
  m_layout->addStretch(1);
}

void RulerToolOptionsBox::updateValues(bool isRasterLevelEditing, double X,
                                       double Y, double W, double H, double A,
                                       double L, int Xpix, int Ypix, int Wpix,
                                       int Hpix) {
  m_Xfld->setValue(X);
  m_Yfld->setValue(Y);
  m_Wfld->setValue(W);
  m_Hfld->setValue(H);
  m_Afld->setValue(A);
  m_Lfld->setValue(L);

  m_XpixelFld->setVisible(isRasterLevelEditing);
  m_YpixelFld->setVisible(isRasterLevelEditing);
  m_WpixelFld->setVisible(isRasterLevelEditing);
  m_HpixelFld->setVisible(isRasterLevelEditing);

  if (isRasterLevelEditing) {
    m_XpixelFld->setText(QString("(%1)").arg(Xpix));
    m_YpixelFld->setText(QString("(%1)").arg(Ypix));
    m_WpixelFld->setText(QString("(%1)").arg(Wpix));
    m_HpixelFld->setText(QString("(%1)").arg(Hpix));
  }
  repaint();
}

void RulerToolOptionsBox::resetValues() {
  m_Xfld->setValue(0);
  m_Yfld->setValue(0);
  m_Wfld->setValue(0);
  m_Hfld->setValue(0);
  m_Afld->setValue(0);
  m_Lfld->setValue(0);

  m_XpixelFld->setVisible(false);
  m_YpixelFld->setVisible(false);
  m_WpixelFld->setVisible(false);
  m_HpixelFld->setVisible(false);

  update();
}

//=============================================================================
//
// TapeToolOptionsBox
//
//=============================================================================

TapeToolOptionsBox::TapeToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent)
    , m_smoothMode(0)
    , m_joinStrokesMode(0)
    , m_toolMode(0)
    , m_autocloseLabel(0)
    , m_autocloseField(0) {
  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  hLayout()->addStretch(1);
  if (!(tool->getTargetType() & TTool::Vectors)) return;

  m_smoothMode = dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Smooth"));
  m_joinStrokesMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("JoinStrokes"));
  m_toolMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Mode"));
  m_typeMode = dynamic_cast<ToolOptionCombo *>(m_controls.value("Type"));
  m_autocloseField =
      dynamic_cast<ToolOptionPairSlider *>(m_controls.value("Distance"));
  if (m_autocloseField)
    m_autocloseLabel = m_labels.value(m_autocloseField->propertyName());
  m_multiFrameMode   = dynamic_cast<ToolOptionCombo *>(m_controls.value("Frame Range:"));

  bool isNormalType = m_typeMode->getProperty()->getValue() == L"Normal";
  m_toolMode->setEnabled(isNormalType);
  m_autocloseField->setEnabled(!isNormalType);
  m_autocloseLabel->setEnabled(!isNormalType);
  m_multiFrameMode->setEnabled(!isNormalType);

  bool isLineToLineMode =
      m_toolMode->getProperty()->getValue() == L"Line to Line";
  m_joinStrokesMode->setEnabled(!isLineToLineMode);

  bool isJoinStrokes = m_joinStrokesMode->isChecked();
  m_smoothMode->setEnabled(!isLineToLineMode && isJoinStrokes);

  bool ret = connect(m_typeMode, SIGNAL(currentIndexChanged(int)), this,
                     SLOT(onToolTypeChanged(int)));
  ret = ret && connect(m_toolMode, SIGNAL(currentIndexChanged(int)), this,
                       SLOT(onToolModeChanged(int)));
  ret = ret && connect(m_joinStrokesMode, SIGNAL(toggled(bool)), this,
                       SLOT(onJoinStrokesModeChanged()));
  assert(ret);
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::onToolTypeChanged(int index) {
  const TEnumProperty::Range &range = m_typeMode->getProperty()->getRange();
  bool isNormalType                 = range[index] == L"Normal";
  m_toolMode->setEnabled(isNormalType);
  m_autocloseField->setEnabled(!isNormalType);
  m_autocloseLabel->setEnabled(!isNormalType);
  m_multiFrameMode->setEnabled(!isNormalType);
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::onToolModeChanged(int index) {
  const TEnumProperty::Range &range = m_toolMode->getProperty()->getRange();
  bool isLineToLineMode             = range[index] == L"Line to Line";
  m_joinStrokesMode->setEnabled(!isLineToLineMode);
  bool isJoinStrokes = m_joinStrokesMode->isChecked();
  m_smoothMode->setEnabled(!isLineToLineMode && isJoinStrokes);
}

//-----------------------------------------------------------------------------

void TapeToolOptionsBox::onJoinStrokesModeChanged() {
  bool isLineToLineMode =
      m_toolMode->getProperty()->getValue() == L"Line to Line";
  bool isJoinStrokes = m_joinStrokesMode->isChecked();
  m_smoothMode->setEnabled(!isLineToLineMode && isJoinStrokes);
}

//=============================================================================
// RGBPickerToolOptions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
/*! Label with background color
 */
class RGBLabel final : public QWidget {
  QColor m_color;

public:
  RGBLabel(QColor color, QWidget *parent) : QWidget(parent), m_color(color) {
    setFixedSize(120, 20);
  }

  ~RGBLabel() {}

  void setColorAndUpdate(QColor color) {
    if (m_color == color) return;
    m_color = color;
    update();
  }

protected:
  void paintEvent(QPaintEvent *pe) {
    QPainter p(this);
    p.setPen(Qt::NoPen);

    if (LutManager::instance()->isValid()) {
      QColor convertedColor(m_color);
      LutManager::instance()->convert(convertedColor);
      p.setBrush(convertedColor);
    } else
      p.setBrush(m_color);

    p.drawRect(rect());

    // white text on dark color. black text on light color.
    int val = m_color.red() * 30 + m_color.green() * 59 + m_color.blue() * 11;
    if (val < 12800)
      p.setPen(Qt::white);
    else
      p.setPen(Qt::black);
    p.setBrush(Qt::NoBrush);

    p.drawText(rect(), Qt::AlignCenter, QString("R:%1 G:%2 B:%3")
                                            .arg(m_color.red())
                                            .arg(m_color.green())
                                            .arg(m_color.blue()));
  }
};

//-----------------------------------------------------------------------------

RGBPickerToolOptionsBox::RGBPickerToolOptionsBox(
    QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
    ToolHandle *toolHandle, PaletteController *paletteController)
    : ToolOptionsBox(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_currentRGBLabel = new RGBLabel(QColor(128, 128, 128), this);

  QAction *pickScreenAction =
      CommandManager::instance()->getAction("A_ToolOption_PickScreen");

  QPushButton *button = new QPushButton(tr("Pick Screen"));
  int buttonWidth     = fontMetrics().horizontalAdvance(button->text()) + 10;
  button->setFixedWidth(buttonWidth);
  button->setFixedHeight(20);
  button->addAction(pickScreenAction);
  connect(button, SIGNAL(clicked()), pickScreenAction, SLOT(trigger()));

  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_realTimePickMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Passive Pick"));

  m_layout->addWidget(m_currentRGBLabel, 0);
  m_layout->addStretch(1);
  m_layout->addWidget(button, 0);
  m_layout->addSpacing(5);

  if (m_realTimePickMode) {
    connect(m_realTimePickMode, SIGNAL(toggled(bool)), m_currentRGBLabel,
            SLOT(setVisible(bool)));
    m_currentRGBLabel->setVisible(m_realTimePickMode->isChecked());
  }

  // for passive pick
  connect(paletteController, SIGNAL(colorPassivePicked(const QColor &)), this,
          SLOT(updateRealTimePickLabel(const QColor &)));
}

//-----------------------------------------------------------------------------

void RGBPickerToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void RGBPickerToolOptionsBox::updateRealTimePickLabel(const QColor &pix) {
  if (isVisible()) m_currentRGBLabel->setColorAndUpdate(pix);
}

//=============================================================================
// StylePickerToolOptions
//-----------------------------------------------------------------------------

StylePickerToolOptionsBox::StylePickerToolOptionsBox(
    QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
    ToolHandle *toolHandle, PaletteController *paletteController)
    : ToolOptionsBox(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_currentStyleLabel = new QLabel(" - - - ", this);
  m_currentStyleLabel->setObjectName("SytlePickerValue");
  setStyleSheet(
      "#SytlePickerValue {color: black; border:0px; background: "
      "rgb(196,196,196);}");

  m_currentStyleLabel->setFixedSize(200, 20);
  m_currentStyleLabel->setAlignment(Qt::AlignCenter);

  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  ToolOptionControlBuilder builder(this, tool, pltHandle, toolHandle);
  if (tool && tool->getProperties(0)) tool->getProperties(0)->accept(builder);

  m_realTimePickMode =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Passive Pick"));

  m_layout->addWidget(m_currentStyleLabel, 0);
  m_layout->addStretch(1);
  // retrieve the "organize palette" checkbox from the layout and insert
  // into rightmost of the tool option bar
  ToolOptionCheckbox *organizePaletteCB =
      dynamic_cast<ToolOptionCheckbox *>(m_controls.value("Organize Palette"));
  m_layout->removeWidget(organizePaletteCB);
  // m_layout->addWidget(new ToolOptionsBarSeparator(this), 0);
  m_layout->addWidget(organizePaletteCB);
  m_layout->addSpacing(5);
  organizePaletteCB->setToolTip(
      tr("With this option being activated, the picked style will be\nmoved to "
         "the end of the first page of the palette."));

  if (m_realTimePickMode) {
    connect(m_realTimePickMode, SIGNAL(toggled(bool)), m_currentStyleLabel,
            SLOT(setVisible(bool)));
    m_currentStyleLabel->setVisible(m_realTimePickMode->isChecked());
  }

  // for passive pick
  connect(paletteController,
          SIGNAL(stylePassivePicked(const int, const int, const int)), this,
          SLOT(updateRealTimePickLabel(const int, const int, const int)));
}

//-----------------------------------------------------------------------------

void StylePickerToolOptionsBox::updateStatus() {
  QMap<std::string, ToolOptionControl *>::iterator it;
  for (it = m_controls.begin(); it != m_controls.end(); it++)
    it.value()->updateStatus();
}

//-----------------------------------------------------------------------------

void StylePickerToolOptionsBox::updateRealTimePickLabel(const int ink,
                                                        const int paint,
                                                        const int tone) {
  if (isVisible()) {
    if (ink < 0)
      m_currentStyleLabel->setText(" - - - ");
    else
      m_currentStyleLabel->setText(QString("INK: #%1  PAINT: #%2  TONE: %3")
                                       .arg(ink)
                                       .arg(paint)
                                       .arg(tone));
  }
}

//=============================================================================
// ShiftTraceToolOptionBox
//-----------------------------------------------------------------------------

ShiftTraceToolOptionBox::ShiftTraceToolOptionBox(QWidget *parent, TTool *tool)
    : ToolOptionsBox(parent), m_tool(tool) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  m_prevFrame  = new QFrame(this);
  m_afterFrame = new QFrame(this);

  m_resetPrevGhostBtn  = new QPushButton(tr("Reset Previous"), this);
  m_resetAfterGhostBtn = new QPushButton(tr("Reset Following"), this);
  int buttonWidth      = fontMetrics().horizontalAdvance(m_resetPrevGhostBtn->text()) + 10;
  m_resetPrevGhostBtn->setFixedWidth(buttonWidth);
  buttonWidth = fontMetrics().horizontalAdvance(m_resetAfterGhostBtn->text()) + 10;
  m_resetAfterGhostBtn->setFixedWidth(buttonWidth);

  m_prevRadioBtn  = new QRadioButton(tr("Previous Drawing"), this);
  m_afterRadioBtn = new QRadioButton(tr("Following Drawing"), this);

  m_prevFrame->setFixedSize(10, 21);
  m_afterFrame->setFixedSize(10, 21);

  m_layout->addWidget(m_prevFrame, 0);
  m_layout->addWidget(m_prevRadioBtn, 0);
  m_layout->addWidget(m_resetPrevGhostBtn, 0);

  m_layout->addWidget(new DVGui::Separator("", this, false));

  m_layout->addWidget(m_afterFrame, 0);
  m_layout->addWidget(m_afterRadioBtn, 0);
  m_layout->addWidget(m_resetAfterGhostBtn, 0);

  m_layout->addStretch(1);

  connect(m_resetPrevGhostBtn, SIGNAL(clicked(bool)), this,
          SLOT(onResetPrevGhostBtnPressed()));
  connect(m_resetAfterGhostBtn, SIGNAL(clicked(bool)), this,
          SLOT(onResetAfterGhostBtnPressed()));
  connect(m_prevRadioBtn, SIGNAL(clicked(bool)), this,
          SLOT(onPrevRadioBtnClicked()));
  connect(m_afterRadioBtn, SIGNAL(clicked(bool)), this,
          SLOT(onAfterRadioBtnClicked()));

  updateStatus();
}

void ShiftTraceToolOptionBox::showEvent(QShowEvent *) {
  TTool::Application *app = TTool::getApplication();
  connect(app->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()), this,
          SLOT(updateColors()));
  updateColors();
}

void ShiftTraceToolOptionBox::hideEvent(QShowEvent *) {
  TTool::Application *app = TTool::getApplication();
  disconnect(app->getCurrentOnionSkin(), SIGNAL(onionSkinMaskChanged()), this,
             SLOT(updateColors()));
}

void ShiftTraceToolOptionBox::resetGhost(int index) {
  TTool::Application *app = TTool::getApplication();
  OnionSkinMask osm       = app->getCurrentOnionSkin()->getOnionSkinMask();
  osm.setShiftTraceGhostCenter(index, TPointD());
  osm.setShiftTraceGhostAff(index, TAffine());
  app->getCurrentOnionSkin()->setOnionSkinMask(osm);
  app->getCurrentOnionSkin()->notifyOnionSkinMaskChanged();
  TTool *tool = app->getCurrentTool()->getTool();
  if (tool) tool->reset();

  if (index == 0)
    m_resetPrevGhostBtn->setDisabled(true);
  else  // index == 1
    m_resetAfterGhostBtn->setDisabled(true);
}

void ShiftTraceToolOptionBox::onResetPrevGhostBtnPressed() { resetGhost(0); }

void ShiftTraceToolOptionBox::onResetAfterGhostBtnPressed() { resetGhost(1); }

void ShiftTraceToolOptionBox::updateColors() {
  TPixel front, back;
  bool ink;
  Preferences::instance()->getOnionData(front, back, ink);

  m_prevFrame->setStyleSheet(QString("background:rgb(%1,%2,%3,255);")
                                 .arg((int)back.r)
                                 .arg((int)back.g)
                                 .arg((int)back.b));
  m_afterFrame->setStyleSheet(QString("background:rgb(%1,%2,%3,255);")
                                  .arg((int)front.r)
                                  .arg((int)front.g)
                                  .arg((int)front.b));
}

void ShiftTraceToolOptionBox::updateStatus() {
  TTool::Application *app = TTool::getApplication();
  OnionSkinMask osm       = app->getCurrentOnionSkin()->getOnionSkinMask();
  if (osm.getShiftTraceGhostAff(0).isIdentity() &&
      osm.getShiftTraceGhostCenter(0) == TPointD())
    m_resetPrevGhostBtn->setDisabled(true);
  else
    m_resetPrevGhostBtn->setEnabled(true);

  if (osm.getShiftTraceGhostAff(1).isIdentity() &&
      osm.getShiftTraceGhostCenter(1) == TPointD())
    m_resetAfterGhostBtn->setDisabled(true);
  else
    m_resetAfterGhostBtn->setEnabled(true);

  // Check the ghost index
  ShiftTraceTool *stTool = (ShiftTraceTool *)m_tool;
  if (!stTool) return;
  if (stTool->getCurrentGhostIndex() == 0)
    m_prevRadioBtn->setChecked(true);
  else  // ghostIndex == 1
    m_afterRadioBtn->setChecked(true);
}

void ShiftTraceToolOptionBox::onPrevRadioBtnClicked() {
  ShiftTraceTool *stTool = (ShiftTraceTool *)m_tool;
  if (!stTool) return;
  stTool->setCurrentGhostIndex(0);
}

void ShiftTraceToolOptionBox::onAfterRadioBtnClicked() {
  ShiftTraceTool *stTool = (ShiftTraceTool *)m_tool;
  if (!stTool) return;
  stTool->setCurrentGhostIndex(1);
}

//=============================================================================
//
// PerspectiveGridToolOptionBox classes
//
//=============================================================================

class PerspectiveGridToolOptionBox::PresetNamePopup final
    : public DVGui::Dialog {
  DVGui::LineEdit *m_nameFld;
  DVGui::CheckBox *m_saveInLibrary;

public:
  PresetNamePopup() : Dialog(0, true) {
    setWindowTitle(tr("Preset Name"));
    m_nameFld = new DVGui::LineEdit();
    addWidget(m_nameFld);
    m_saveInLibrary = new DVGui::CheckBox(tr("Save As Library Preset"));
    addWidget(m_saveInLibrary);

    QPushButton *okBtn = new QPushButton(tr("OK"), this);
    okBtn->setDefault(true);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

    addButtonBarWidget(okBtn, cancelBtn);
  }

  QString getName() { return m_nameFld->text(); }
  bool isSaveInLibrary() { return m_saveInLibrary->isChecked(); }
  void resetDialog() {
    m_nameFld->setText(QString(""));
    m_saveInLibrary->setChecked(false);
  }
};

//=============================================================================
// PerspectiveGridToolOptionBox
//-----------------------------------------------------------------------------

PerspectiveGridToolOptionBox::PerspectiveGridToolOptionBox(
    QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
    ToolHandle *toolHandle)
    : ToolOptionsBox(parent), m_tool(tool), m_presetNamePopup(0) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  TEnumProperty *perspectiveType =
      dynamic_cast<TEnumProperty *>(props->getProperty("Type:"));
  m_perspectiveType = new ToolOptionCombo(tool, perspectiveType, toolHandle);

  TDoubleProperty *opacity =
      dynamic_cast<TDoubleProperty *>(props->getProperty("Opacity:"));
  m_opacity = new ToolOptionSlider(tool, opacity, toolHandle);

  int fieldMaxWidth;

  m_spacingLabel = new ClickableLabel(tr("Spacing:"), this);
  m_spacing      = new MeasuredValueField(this);
  m_spacing->setMeasure("");
  m_spacing->setMaximumWidth(getMaximumWidthForMeasuredValueField(m_spacing));

  m_rotationLabel = new ClickableLabel(tr("Rotation:"), this);
  m_rotation      = new MeasuredValueField(this);
  m_rotation->setMeasure("angle");
  m_rotation->setMaximumWidth(getMaximumWidthForMeasuredValueField(m_rotation));

  m_leftRotateButton  = new QPushButton(this);
  m_rightRotateButton = new QPushButton(this);

  m_leftRotateButton->setFixedSize(QSize(20, 20));
  m_rightRotateButton->setFixedSize(QSize(20, 20));

  m_leftRotateButton->setIcon(createQIcon("rotateleft"));
  m_leftRotateButton->setIconSize(QSize(20, 20));
  m_rightRotateButton->setIcon(createQIcon("rotateright"));
  m_rightRotateButton->setIconSize(QSize(20, 20));

  m_leftRotateButton->setToolTip(tr("Rotate Perspective Left"));
  m_rightRotateButton->setToolTip(tr("Rotate Perspective Right"));

  TColorChipProperty *color =
      dynamic_cast<TColorChipProperty *>(props->getProperty("Color:"));
  m_color = new ColorChipCombo(tool, color);

  TBoolProperty *horizon =
      dynamic_cast<TBoolProperty *>(props->getProperty("Horizon"));
  m_horizon = new ToolOptionCheckbox(tool, horizon, toolHandle);

  TBoolProperty *parallel =
      dynamic_cast<TBoolProperty *>(props->getProperty("Parallel"));
  m_parallel = new ToolOptionCheckbox(tool, parallel, toolHandle);

  TBoolProperty *advancedControls =
      dynamic_cast<TBoolProperty *>(props->getProperty("Advanced Controls"));
  m_advancedControls =
      new ToolOptionCheckbox(tool, advancedControls, toolHandle);

  TEnumProperty *preset =
      dynamic_cast<TEnumProperty *>(props->getProperty("Preset:"));
  m_presetCombo = new ToolOptionCombo(tool, preset, toolHandle);

  // Preset +/- buttons
  m_addPresetButton    = new QPushButton(QString("+"));
  m_removePresetButton = new QPushButton(QString("-"));

  m_addPresetButton->setFixedSize(QSize(20, 20));
  m_removePresetButton->setFixedSize(QSize(20, 20));

  /* --- Layout --- */
  QHBoxLayout *mainLay = m_layout;
  {
    mainLay->addWidget(new QLabel(tr("Type:"), this), 0);
    mainLay->addWidget(m_perspectiveType, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Opacity:"), this), 0);
    mainLay->addWidget(m_opacity, 100);

    mainLay->addSpacing(5);

    mainLay->addWidget(m_spacingLabel, 0);
    mainLay->addWidget(m_spacing, 10);

    mainLay->addSpacing(5);

    mainLay->addWidget(m_rotationLabel, 0);
    mainLay->addWidget(m_rotation, 10);
    mainLay->addWidget(m_leftRotateButton, 0);
    mainLay->addWidget(m_rightRotateButton, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Color:"), this), 0);
    mainLay->addWidget(m_color, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(m_horizon, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(m_parallel, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(m_advancedControls, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Preset:"), this), 0);
    mainLay->addWidget(m_presetCombo, 0);
    mainLay->addWidget(m_addPresetButton);
    mainLay->addWidget(m_removePresetButton);
  }

  m_layout->addStretch(1);

  connect(m_perspectiveType, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onPerspectiveTypeChanged(int)));

  connect(m_spacing, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
          SLOT(onSpacingChange(TMeasuredValue *)));
  connect(m_spacingLabel, SIGNAL(onMousePress(QMouseEvent *)), m_spacing,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(m_spacingLabel, SIGNAL(onMouseMove(QMouseEvent *)), m_spacing,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(m_spacingLabel, SIGNAL(onMouseRelease(QMouseEvent *)), m_spacing,
          SLOT(receiveMouseRelease(QMouseEvent *)));

  connect(m_rotation, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
          SLOT(onRotationChange(TMeasuredValue *)));
  connect(m_rotationLabel, SIGNAL(onMousePress(QMouseEvent *)), m_rotation,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(m_rotationLabel, SIGNAL(onMouseMove(QMouseEvent *)), m_rotation,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(m_rotationLabel, SIGNAL(onMouseRelease(QMouseEvent *)), m_rotation,
          SLOT(receiveMouseRelease(QMouseEvent *)));
  connect(m_leftRotateButton, SIGNAL(clicked()), SLOT(onRotateLeft()));
  connect(m_rightRotateButton, SIGNAL(clicked()), SLOT(onRotateRight()));

  connect(m_addPresetButton, SIGNAL(clicked()), this, SLOT(onAddPreset()));
  connect(m_removePresetButton, SIGNAL(clicked()), this,
          SLOT(onRemovePreset()));

  filterControls();
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::filterControls() {
  PerspectiveTool *perspectiveTool = dynamic_cast<PerspectiveTool *>(m_tool);

  PerspectiveObject *mainObj = perspectiveTool->getMainPerspectiveObject();

  std::vector<PerspectiveObject *> objs =
      perspectiveTool->getPerspectiveObjects();
  bool isVanishingSelected =
      m_perspectiveType->getProperty()->getValue() == L"Vanishing Point";
  bool isLineSelected = m_perspectiveType->getProperty()->getValue() == L"Line";
  int selected        = 0;
  for (int i = 0; i < objs.size(); i++) {
    if (!objs[i]->isActive()) continue;
    if (objs[i]->getType() == PerspectiveType::VanishingPoint)
      isVanishingSelected = true;
    else if (objs[i]->getType() == PerspectiveType::Line)
      isLineSelected = true;
    selected++;
  }

  m_parallel->setEnabled(isLineSelected);

  m_horizon->setEnabled(isVanishingSelected || m_parallel->isChecked());

  m_advancedControls->setEnabled(isVanishingSelected);

  m_spacingLabel->setEnabled(selected &&
                             (isVanishingSelected || m_parallel->isChecked()));
  m_spacing->setEnabled(selected &&
                        (isVanishingSelected || m_parallel->isChecked()));
  if (!m_spacing->isEnabled())
    m_spacing->setValue(0);
  else if (mainObj)
    m_spacing->setValue(mainObj->getSpacing());

  m_rotationLabel->setEnabled(selected);
  m_rotation->setEnabled(selected);
  if (!m_rotation->isEnabled())
    m_rotation->setValue(0);
  else if (mainObj)
    m_rotation->setValue(mainObj->getRotation());
  m_leftRotateButton->setEnabled(selected);
  m_rightRotateButton->setEnabled(selected);
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::updateStatus() {
  m_opacity->updateStatus();
  m_color->updateStatus();
  m_horizon->updateStatus();
  m_parallel->updateStatus();
  m_presetCombo->updateStatus();

  filterControls();
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::updateMeasuredValues(double spacing,
                                                        double rotation) {
  filterControls();

  if (m_spacing->isEnabled()) m_spacing->setValue(spacing);
  m_rotation->setValue(rotation);
  repaint();
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::reloadPresetCombo() {
  m_presetCombo->loadEntries();
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::onAddPreset() {
  // Initialize preset name popup
  if (!m_presetNamePopup) m_presetNamePopup = new PresetNamePopup;

  if (!m_presetNamePopup->getName().isEmpty()) m_presetNamePopup->resetDialog();

  // Retrieve the preset name
  bool ret = m_presetNamePopup->exec();
  if (!ret) return;

  QString name(m_presetNamePopup->getName());
  bool saveInLibrary(m_presetNamePopup->isSaveInLibrary());
  m_presetNamePopup->resetDialog();

  static_cast<PerspectiveTool *>(m_tool)->addPreset(name, saveInLibrary);

  m_presetCombo->loadEntries();
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::onRemovePreset() {
  static_cast<PerspectiveTool *>(m_tool)->removePreset();

  m_presetCombo->loadEntries();
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::onPerspectiveTypeChanged(int index) {
  filterControls();
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::onSpacingChange(TMeasuredValue *fld) {
  double value = fld->getValue(TMeasuredValue::CurrentUnit);

  PerspectiveTool *perspectiveTool = dynamic_cast<PerspectiveTool *>(m_tool);
  PerspectiveObject *mainObj = perspectiveTool->getMainPerspectiveObject();
  if (mainObj) {
    value = std::min(value, mainObj->getMaxSpacing());
    value = std::max(value, mainObj->getMinSpacing());
    m_spacing->setValue(value);
    repaint();
  }
  perspectiveTool->updateSpacing(value);
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::onRotationChange(TMeasuredValue *fld) {
  double value = fld->getValue(TMeasuredValue::CurrentUnit);

  PerspectiveTool *perspectiveTool = dynamic_cast<PerspectiveTool *>(m_tool);

  perspectiveTool->updateRotation(value);
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::onRotateLeft() {
  m_rotation->setValue(m_rotation->getValue() + 90);
  emit m_rotation->measuredValueChanged(m_rotation->getMeasuredValue());
}

//-----------------------------------------------------------------------------

void PerspectiveGridToolOptionBox::onRotateRight() {
  m_rotation->setValue(m_rotation->getValue() - 90);
  emit m_rotation->measuredValueChanged(m_rotation->getMeasuredValue());
}

//=============================================================================
// SymmetryToolOptionBox
//-----------------------------------------------------------------------------

class SymmetryToolOptionBox::PresetNamePopup final : public DVGui::Dialog {
  DVGui::LineEdit *m_nameFld;

public:
  PresetNamePopup() : Dialog(0, true) {
    setWindowTitle(tr("Preset Name"));
    m_nameFld = new DVGui::LineEdit();
    addWidget(m_nameFld);

    QPushButton *okBtn = new QPushButton(tr("OK"), this);
    okBtn->setDefault(true);
    QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(okBtn, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

    addButtonBarWidget(okBtn, cancelBtn);
  }

  QString getName() { return m_nameFld->text(); }
  void removeName() { m_nameFld->setText(QString("")); }
};

SymmetryToolOptionBox::SymmetryToolOptionBox(QWidget *parent, TTool *tool,
                                             TPaletteHandle *pltHandle,
                                             ToolHandle *toolHandle)
    : ToolOptionsBox(parent), m_tool(tool), m_presetNamePopup(0) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  TPropertyGroup *props = tool->getProperties(0);
  assert(props->getPropertyCount() > 0);

  TIntProperty *lines =
      dynamic_cast<TIntProperty *>(props->getProperty("Lines:"));
  m_lines = new ToolOptionIntSlider(tool, lines, toolHandle);

  TDoubleProperty *opacity =
      dynamic_cast<TDoubleProperty *>(props->getProperty("Opacity:"));
  m_opacity = new ToolOptionSlider(tool, opacity, toolHandle);

  m_rotationLabel = new ClickableLabel(tr("Rotation:"), this);
  m_rotation      = new MeasuredValueField(this);
  m_rotation->setMeasure("angle");
  m_rotation->setMaximumWidth(getMaximumWidthForMeasuredValueField(m_rotation));

  m_leftRotateButton  = new QPushButton(this);
  m_rightRotateButton = new QPushButton(this);

  m_leftRotateButton->setFixedSize(QSize(20, 20));
  m_rightRotateButton->setFixedSize(QSize(20, 20));

  m_leftRotateButton->setIcon(createQIcon("rotateleft"));
  m_leftRotateButton->setIconSize(QSize(20, 20));
  m_rightRotateButton->setIcon(createQIcon("rotateright"));
  m_rightRotateButton->setIconSize(QSize(20, 20));

  m_leftRotateButton->setToolTip(tr("Rotate Perspective Left"));
  m_rightRotateButton->setToolTip(tr("Rotate Perspective Right"));

  TColorChipProperty *color =
      dynamic_cast<TColorChipProperty *>(props->getProperty("Color:"));
  m_color = new ColorChipCombo(tool, color);

  TBoolProperty *lineSymmetry =
      dynamic_cast<TBoolProperty *>(props->getProperty("Line Symmetry"));
  m_useLineSymmetry = new ToolOptionCheckbox(tool, lineSymmetry, toolHandle);

  QPushButton *resetButton = new QPushButton(tr("Reset Position"));
  int buttonWidth          = fontMetrics().horizontalAdvance(resetButton->text()) + 10;
  resetButton->setFixedWidth(buttonWidth);
  resetButton->setFixedHeight(20);

  TEnumProperty *preset =
      dynamic_cast<TEnumProperty *>(props->getProperty("Preset:"));
  m_presetCombo = new ToolOptionCombo(tool, preset, toolHandle);

  // Preset +/- buttons
  m_addPresetButton    = new QPushButton(QString("+"));
  m_removePresetButton = new QPushButton(QString("-"));

  m_addPresetButton->setFixedSize(QSize(20, 20));
  m_removePresetButton->setFixedSize(QSize(20, 20));

  /* --- Layout --- */
  QHBoxLayout *mainLay = m_layout;
  {
    mainLay->addWidget(new QLabel(tr("Lines:"), this), 0);
    mainLay->addWidget(m_lines, 16);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Opacity:"), this), 0);
    mainLay->addWidget(m_opacity, 100);

    mainLay->addSpacing(5);

    mainLay->addWidget(m_rotationLabel, 0);
    mainLay->addWidget(m_rotation, 10);
    mainLay->addWidget(m_leftRotateButton, 0);
    mainLay->addWidget(m_rightRotateButton, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Color:"), this), 0);
    mainLay->addWidget(m_color, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(m_useLineSymmetry, 0);

    mainLay->addSpacing(5);

    mainLay->addWidget(new QLabel(tr("Preset:"), this), 0);
    mainLay->addWidget(m_presetCombo, 0);
    mainLay->addWidget(m_addPresetButton);
    mainLay->addWidget(m_removePresetButton);
  }

  m_layout->addStretch(1);
  m_layout->addWidget(resetButton, 0);
  m_layout->addSpacing(5);

  connect(m_rotation, SIGNAL(measuredValueChanged(TMeasuredValue *, bool)),
          SLOT(onRotationChange(TMeasuredValue *)));
  connect(m_rotationLabel, SIGNAL(onMousePress(QMouseEvent *)), m_rotation,
          SLOT(receiveMousePress(QMouseEvent *)));
  connect(m_rotationLabel, SIGNAL(onMouseMove(QMouseEvent *)), m_rotation,
          SLOT(receiveMouseMove(QMouseEvent *)));
  connect(m_rotationLabel, SIGNAL(onMouseRelease(QMouseEvent *)), m_rotation,
          SLOT(receiveMouseRelease(QMouseEvent *)));
  connect(m_leftRotateButton, SIGNAL(clicked()), SLOT(onRotateLeft()));
  connect(m_rightRotateButton, SIGNAL(clicked()), SLOT(onRotateRight()));
  connect(resetButton, SIGNAL(clicked()), SLOT(onResetPosition()));
  connect(m_addPresetButton, SIGNAL(clicked()), this, SLOT(onAddPreset()));
  connect(m_removePresetButton, SIGNAL(clicked()), this,
          SLOT(onRemovePreset()));

  filterControls();
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::filterControls() {
  bool hasEvenLines = (m_lines->getValue() % 2) == 0;

  m_useLineSymmetry->setEnabled(hasEvenLines);
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::updateStatus() {
  m_lines->updateStatus();
  m_opacity->updateStatus();
  m_color->updateStatus();
  m_useLineSymmetry->updateStatus();
  m_presetCombo->updateStatus();

  filterControls();
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::updateMeasuredValues(double rotation) {
  m_rotation->setValue(rotation);
  repaint();
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::onLinesChanged() { filterControls(); }

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::onRotationChange(TMeasuredValue *fld) {
  double value = fld->getValue(TMeasuredValue::CurrentUnit);

  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(m_tool);

  symmetryTool->updateRotation(value);
  symmetryTool->onPropertyChanged("Rotation");
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::onRotateLeft() {
  m_rotation->setValue(m_rotation->getValue() + 90);
  emit m_rotation->measuredValueChanged(m_rotation->getMeasuredValue());
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::onRotateRight() {
  m_rotation->setValue(m_rotation->getValue() - 90);
  emit m_rotation->measuredValueChanged(m_rotation->getMeasuredValue());
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::onResetPosition() {
  SymmetryTool *symmetryTool = dynamic_cast<SymmetryTool *>(m_tool);

  symmetryTool->resetPosition();
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::onAddPreset() {
  // Initialize preset name popup
  if (!m_presetNamePopup) m_presetNamePopup = new PresetNamePopup;

  if (!m_presetNamePopup->getName().isEmpty()) m_presetNamePopup->removeName();

  // Retrieve the preset name
  bool ret = m_presetNamePopup->exec();
  if (!ret) return;

  QString name(m_presetNamePopup->getName());
  m_presetNamePopup->removeName();

  static_cast<SymmetryTool *>(m_tool)->addPreset(name);

  m_presetCombo->loadEntries();
}

//-----------------------------------------------------------------------------

void SymmetryToolOptionBox::onRemovePreset() {
  static_cast<SymmetryTool *>(m_tool)->removePreset();

  m_presetCombo->loadEntries();
}

//=============================================================================
// ZoomToolOptionBox
//-----------------------------------------------------------------------------

ZoomToolOptionsBox::ZoomToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  QAction *resetZoomAction =
      CommandManager::instance()->getAction(VB_ZoomReset);

  QPushButton *button = new QPushButton(tr("Reset Zoom"));
  int buttonWidth     = fontMetrics().horizontalAdvance(button->text()) + 10;
  button->setFixedWidth(buttonWidth);
  button->setFixedHeight(20);
  button->addAction(resetZoomAction);

  connect(button, SIGNAL(clicked()), resetZoomAction, SLOT(trigger()));

  m_layout->addStretch(1);
  m_layout->addWidget(button, 0);
  m_layout->addSpacing(5);
}

//=============================================================================
// RotateToolOptionBox
//-----------------------------------------------------------------------------

RotateToolOptionsBox::RotateToolOptionsBox(QWidget *parent, TTool *tool,
                                           TPaletteHandle *pltHandle,
                                           ToolHandle *toolHandle)
    : ToolOptionsBox(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  QAction *resetRotationAction =
      CommandManager::instance()->getAction(VB_RotateReset);

  QPushButton *button = new QPushButton(tr("Reset Rotation"));
  int buttonWidth     = fontMetrics().horizontalAdvance(button->text()) + 10;
  button->setFixedWidth(buttonWidth);
  button->setFixedHeight(20);
  button->addAction(resetRotationAction);

  connect(button, SIGNAL(clicked()), resetRotationAction, SLOT(trigger()));

  m_layout->addStretch(1);
  m_layout->addWidget(button, 0);
  m_layout->addSpacing(5);
}

//=============================================================================
// HandToolOptionBox
//-----------------------------------------------------------------------------

HandToolOptionsBox::HandToolOptionsBox(QWidget *parent, TTool *tool,
                                       TPaletteHandle *pltHandle,
                                       ToolHandle *toolHandle)
    : ToolOptionsBox(parent) {
  setFrameStyle(QFrame::StyledPanel);
  setFixedHeight(26);

  QAction *resetPositionAction =
      CommandManager::instance()->getAction(VB_PositionReset);

  QPushButton *button = new QPushButton(tr("Reset Position"));
  int buttonWidth     = fontMetrics().horizontalAdvance(button->text()) + 20;
  button->setFixedWidth(buttonWidth);
  button->setFixedHeight(20);
  button->addAction(resetPositionAction);

  connect(button, SIGNAL(clicked()), resetPositionAction, SLOT(trigger()));

  m_layout->addStretch(1);
  m_layout->addWidget(button, 0);
  m_layout->addSpacing(5);
}

//=============================================================================
// ToolOptions
//-----------------------------------------------------------------------------

ToolOptions::ToolOptions() : m_panel(0) {
  QHBoxLayout *mainLayout = new QHBoxLayout();
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);
  setLayout(mainLayout);
}

//-----------------------------------------------------------------------------

ToolOptions::~ToolOptions() {}

//-----------------------------------------------------------------------------

void ToolOptions::showEvent(QShowEvent *) {
  TTool::Application *app = TTool::getApplication();
  ToolHandle *currTool    = app->getCurrentTool();
  if (currTool) {
    onToolSwitched();
    connect(currTool, SIGNAL(toolSwitched()), SLOT(onToolSwitched()));
    connect(currTool, SIGNAL(toolChanged()), SLOT(onToolChanged()));
  }

  TObjectHandle *currObject = app->getCurrentObject();
  if (currObject) {
    onStageObjectChange();
    connect(currObject, SIGNAL(objectSwitched()), SLOT(onStageObjectChange()));
    connect(currObject, SIGNAL(objectChanged(bool)),
            SLOT(onStageObjectChange()));
  }

  TXshLevelHandle *currLevel = app->getCurrentLevel();

  if (currLevel)
    connect(currLevel, SIGNAL(xshLevelSwitched(TXshLevel *)), this,
            SLOT(onStageObjectChange()));
}

//-----------------------------------------------------------------------------

void ToolOptions::hideEvent(QShowEvent *) {
  TTool::Application *app = TTool::getApplication();
  ToolHandle *currTool    = app->getCurrentTool();
  if (currTool) currTool->disconnect(this);

  TObjectHandle *currObject = app->getCurrentObject();
  if (currObject) currObject->disconnect(this);

  TXshLevelHandle *currLevel = app->getCurrentLevel();
  if (currLevel) currLevel->disconnect(this);
}

//-----------------------------------------------------------------------------

void ToolOptions::onToolSwitched() {
  if (m_panel) m_panel->hide();
  m_panel = 0;

  TTool::Application *app = TTool::getApplication();

  // Quando i seguenti non serviranno piu', rimuovere gli header corrispondenti
  // in cima, thx
  TFrameHandle *currFrame   = app->getCurrentFrame();
  TObjectHandle *currObject = app->getCurrentObject();
  TXsheetHandle *currXsheet = app->getCurrentXsheet();
  TPaletteHandle *currPalette =
      app->getPaletteController()->getCurrentLevelPalette();
  ToolHandle *currTool = app->getCurrentTool();

  TTool *tool = app->getCurrentTool()->getTool();
  if (tool) {
    // c'e' un tool corrente
    ToolOptionsBox *panel = 0;
    std::map<TTool *, ToolOptionsBox *>::iterator it = m_panels.find(tool);
    if (it == m_panels.end()) {
      // ... senza panel associato
      if (tool->getName() == T_Edit) {
        TPropertyGroup *pg = tool->getProperties(0);
        panel = new ArrowToolOptionsBox(0, tool, pg, currFrame, currObject,
                                        currXsheet, currTool);
      } else if (tool->getName() == T_Selection)
        panel = new SelectionToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Geometric)
        panel = new GeometricToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Type)
        panel = new TypeToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_PaintBrush)
        panel = new PaintbrushToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Fill) {
        if (tool->getTargetType() & TTool::RasterImage)
          panel =
              new FullColorFillToolOptionsBox(0, tool, currPalette, currTool);
        else
          panel = new FillToolOptionsBox(0, tool, currPalette, currTool);
      } else if (tool->getName() == T_Eraser)
        panel = new EraserToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Tape)
        panel = new TapeToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_RGBPicker)
        panel = new RGBPickerToolOptionsBox(0, tool, currPalette, currTool,
                                            app->getPaletteController());
      else if (tool->getName() == T_Ruler) {
        RulerToolOptionsBox *p = new RulerToolOptionsBox(0, tool);
        panel                  = p;
        RulerTool *rt          = dynamic_cast<RulerTool *>(tool);
        if (rt) rt->setToolOptionsBox(p);
      } else if (tool->getName() == T_PerspectiveGrid) {
        PerspectiveGridToolOptionBox *p =
            new PerspectiveGridToolOptionBox(this, tool, currPalette, currTool);
        panel               = p;
        PerspectiveTool *pt = dynamic_cast<PerspectiveTool *>(tool);
        if (pt) pt->setToolOptionsBox(p);
      } else if (tool->getName() == T_Symmetry) {
        SymmetryToolOptionBox *p =
            new SymmetryToolOptionBox(this, tool, currPalette, currTool);
        panel            = p;
        SymmetryTool *st = dynamic_cast<SymmetryTool *>(tool);
        if (st) st->setToolOptionsBox(p);
      } else if (tool->getName() == T_StylePicker)
        panel = new StylePickerToolOptionsBox(0, tool, currPalette, currTool,
                                              app->getPaletteController());
      else if (tool->getName() == "T_ShiftTrace")
        panel = new ShiftTraceToolOptionBox(this, tool);
      else if (tool->getName() == T_Zoom)
        panel = new ZoomToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Rotate)
        panel = new RotateToolOptionsBox(0, tool, currPalette, currTool);
      else if (tool->getName() == T_Hand)
        panel = new HandToolOptionsBox(0, tool, currPalette, currTool);
      else
        panel = tool->createOptionsBox();  // Only this line should remain out
                                           // of that if/else monstrosity

      /* DANIELE: Regola per il futuro - NON FARE PIU' COME SOPRA.
Bisogna cominciare a collegare il metodo virtuale
createOptionsBox() di ogni tool.

Chi ha tempo si adoperi un pochino per normalizzare
la situazione anche per i tool sopra, plz - basta spostare
un po' di codice... */

      m_panels[tool] = panel;
      layout()->addWidget(panel);
      emit newPanelCreated();
    } else {
      // il panel c'e' gia'.
      panel = it->second;
      panel->updateStatus();
    }
    m_panel = panel;
    m_panel->show();
  } else {
    // niente tool
  }
}

//-----------------------------------------------------------------------------

void ToolOptions::onToolChanged() {
  assert(m_panel);
  ToolOptionsBox *optionBox = dynamic_cast<ToolOptionsBox *>(m_panel);
  assert(optionBox);
  optionBox->updateStatus();
}

//-----------------------------------------------------------------------------

void ToolOptions::onStageObjectChange() {
  TTool *tool = TTool::getApplication()->getCurrentTool()->getTool();
  if (!tool) return;

  std::map<TTool *, ToolOptionsBox *>::iterator it = m_panels.find(tool);
  if (it == m_panels.end()) return;

  ToolOptionsBox *panel = it->second;
  panel->onStageObjectChange();
}

//***********************************************************************************
//    Command instantiation
//***********************************************************************************

class FlipHorizontalCommandHandler final : public MenuItemHandler {
public:
  FlipHorizontalCommandHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}
  void execute() override {
    TTool::Application *app = TTool::getApplication();
    TTool *tool             = app->getCurrentTool()->getTool();
    if (!tool) return;
    if (tool->getName() == T_Edit) {
      EditTool *editTool = dynamic_cast<EditTool *>(tool);
      emit editTool->clickFlipHorizontal();
    } else if (tool->getName() == T_Selection) {
      SelectionTool *selectionTool = dynamic_cast<SelectionTool *>(tool);
      emit selectionTool->clickFlipHorizontal();
    }
  }
} flipHorizontalCHInstance("A_ToolOption_FlipHorizontal");

class FlipVerticalCommandHandler final : public MenuItemHandler {
public:
  FlipVerticalCommandHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}
  void execute() override {
    TTool::Application *app = TTool::getApplication();
    TTool *tool             = app->getCurrentTool()->getTool();
    if (!tool) return;
    if (tool->getName() == T_Edit) {
      EditTool *editTool = dynamic_cast<EditTool *>(tool);
      emit editTool->clickFlipVertical();
    } else if (tool->getName() == T_Selection) {
      SelectionTool *selectionTool = dynamic_cast<SelectionTool *>(tool);
      emit selectionTool->clickFlipVertical();
    }
  }
} flipVerticalCHInstance("A_ToolOption_FlipVertical");

class RotateLeftCommandHandler final : public MenuItemHandler {
public:
  RotateLeftCommandHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}
  void execute() override {
    TTool::Application *app = TTool::getApplication();
    TTool *tool             = app->getCurrentTool()->getTool();
    if (!tool) return;
    if (tool->getName() == T_Edit) {
      EditTool *editTool = dynamic_cast<EditTool *>(tool);
      emit editTool->clickRotateLeft();
    } else if (tool->getName() == T_Selection) {
      SelectionTool *selectionTool = dynamic_cast<SelectionTool *>(tool);
      emit selectionTool->clickRotateLeft();
    }
  }
} rotateLeftCHInstance("A_ToolOption_RotateLeft");

class RotateRightCommandHandler final : public MenuItemHandler {
public:
  RotateRightCommandHandler(CommandId cmdId) : MenuItemHandler(cmdId) {}
  void execute() override {
    TTool::Application *app = TTool::getApplication();
    TTool *tool             = app->getCurrentTool()->getTool();
    if (!tool) return;
    if (tool->getName() == T_Edit) {
      EditTool *editTool = dynamic_cast<EditTool *>(tool);
      emit editTool->clickRotateRight();
    } else if (tool->getName() == T_Selection) {
      SelectionTool *selectionTool = dynamic_cast<SelectionTool *>(tool);
      emit selectionTool->clickRotateRight();
    }
  }
} rotateRightCHInstance("A_ToolOption_RotateRight");
