#pragma once

#ifndef TOOLOPTIONS_H
#define TOOLOPTIONS_H

// TnzQt includes
#include "toonzqt/checkbox.h"

// TnzLib includes
#include "toonz/tstageobject.h"

// TnzCore includes
#include "tcommon.h"
#include "tproperty.h"
#include "tunit.h"

// Qt includes
#include <QFrame>
#include <QAction>
#include <QList>
#include <QToolBar>
#include <QMap>
#include <QLabel>
#include <QRadioButton>

// STD includes
#include <map>

#undef DVAPI
#undef DVVAR
#ifdef TNZTOOLS_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================================================

//    Forward declarations

class TTool;
class ToolOptionToolBar;
class TPropertyGroup;
class TPaletteHandle;
class TFrameHandle;
class TObjectHandle;
class TXsheetHandle;
class ToolHandle;
class SelectionScaleField;
class SelectionRotationField;
class SelectionMoveField;
class ToolOptionSlider;
class ToolOptionIntSlider;
class ThickChangeField;
class ToolOptionCombo;
class ToolOptionCheckbox;
class PegbarChannelField;
class ToolOptionPairSlider;
class ToolOptionControl;
class ToolOptionPopupButton;
class TXshLevelHandle;
class NoScaleField;
class PegbarCenterField;
class RGBLabel;
class MeasuredValueField;
class PaletteController;
class ClickableLabel;
class StyleIndexFieldAndChip;
class ColorChipCombo;

class QLabel;
class QPushButton;
class QPropertyAnimation;
class QFrame;
class QHBoxLayout;
class QComboBox;
class QStackedWidget;

//=============================================================================

// Preprocessor definitions

#define TOOL_OPTIONS_LEFT_MARGIN 5

//=============================================================================

//***********************************************************************************************
//    ToolOptionToolBar  declaration
//***********************************************************************************************

class ToolOptionToolBar final : public QToolBar {
public:
  ToolOptionToolBar(QWidget *parent = 0);

  void addSpacing(int width);
};

//***********************************************************************************************
//    ToolOptionsBox  declaration
//***********************************************************************************************

class ToolOptionsBox : public QFrame {
  Q_OBJECT

protected:
  QMap<std::string, ToolOptionControl *>
      m_controls;  //!< property name -> ToolOptionControl
  QMap<std::string, QLabel *> m_labels;

  QHBoxLayout *m_layout;

public:
  ToolOptionsBox(QWidget *parent, bool isScrollable = true);
  ~ToolOptionsBox();

  virtual void
  updateStatus();  //!< Invokes updateStatus() on all registered controls
  virtual void onStageObjectChange() {}

  QHBoxLayout *hLayout() { return m_layout; }
  void addControl(ToolOptionControl *control);

  ToolOptionControl *control(const std::string &controlName) const;

  QLabel *addLabel(QString name);
  void addLabel(std::string propName, QLabel *label);
  void addSeparator();
};

//***********************************************************************************************
//    ToolOptionControlBuilder  declaration
//***********************************************************************************************

class ToolOptionControlBuilder final : public TProperty::Visitor {
  ToolOptionsBox *m_panel;
  TTool *m_tool;
  TPaletteHandle *m_pltHandle;
  ToolHandle *m_toolHandle;

  int m_singleValueWidgetType;
  int m_enumWidgetType;

public:
  ToolOptionControlBuilder(ToolOptionsBox *panel, TTool *tool,
                           TPaletteHandle *pltHandle,
                           ToolHandle *toolHandle = 0);

  enum SingleValueWidgetType { SLIDER = 0, FIELD };
  void setSingleValueWidgetType(int type) { m_singleValueWidgetType = type; }

  enum EnumWidgetType { COMBOBOX = 0, POPUPBUTTON, FONTCOMBOBOX };
  void setEnumWidgetType(int type) { m_enumWidgetType = type; }

private:
  QHBoxLayout *hLayout() { return m_panel->hLayout(); }
  QLabel *addLabel(TProperty *p);

  void visit(TDoubleProperty *p) override;
  void visit(TDoublePairProperty *p) override;
  void visit(TIntPairProperty *p) override;
  void visit(TIntProperty *p) override;
  void visit(TBoolProperty *p) override;
  void visit(TStringProperty *p) override;
  void visit(TEnumProperty *p) override;
  void visit(TStyleIndexProperty *p) override;
  void visit(TPointerProperty *p) override;
  void visit(TColorChipProperty *p) override;
};

//***********************************************************************************************
//    GenericToolOptionsBox  declaration
//***********************************************************************************************

class GenericToolOptionsBox : public ToolOptionsBox {
public:
  GenericToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                        int propertyGroupIdx = 0, ToolHandle *toolHandle = 0,
                        bool scrollable = true);
};

//=============================================================================
//
// ArrowToolOptionsBox
//
//=============================================================================

class ArrowToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  enum AXIS { Position = 0, Rotation, Scale, Shear, CenterPosition, AllAxis };

  TPropertyGroup *m_pg;
  bool m_splined;
  TTool *m_tool;
  TFrameHandle *m_frameHandle;
  TObjectHandle *m_objHandle;
  TXsheetHandle *m_xshHandle;

  QWidget **m_axisOptionWidgets;
  QWidget *m_pickWidget;

  // General
  ToolOptionCombo *m_chooseActiveAxisCombo;
  ToolOptionCombo *m_pickCombo;
  // enable to choose the target pegbar from the combobox
  QComboBox *m_currentStageObjectCombo;

  // Position
  PegbarChannelField *m_motionPathPosField;
  PegbarChannelField *m_ewPosField;
  PegbarChannelField *m_nsPosField;
  PegbarChannelField *m_zField;
  NoScaleField *m_noScaleZField;
  ClickableLabel *m_motionPathPosLabel;
  ClickableLabel *m_ewPosLabel;
  ClickableLabel *m_nsPosLabel;
  ClickableLabel *m_zLabel;

  ToolOptionCheckbox *m_lockEWPosCheckbox;
  ToolOptionCheckbox *m_lockNSPosCheckbox;

  // SO = Stacked Order
  ClickableLabel *m_soLabel;
  PegbarChannelField *m_soField;

  // Rotation
  ClickableLabel *m_rotationLabel;
  PegbarChannelField *m_rotationField;

  // Scale
  ClickableLabel *m_globalScaleLabel;
  ClickableLabel *m_scaleHLabel;
  ClickableLabel *m_scaleVLabel;
  PegbarChannelField *m_globalScaleField;
  PegbarChannelField *m_scaleHField;
  PegbarChannelField *m_scaleVField;
  ToolOptionCheckbox *m_lockScaleHCheckbox;
  ToolOptionCheckbox *m_lockScaleVCheckbox;
  ToolOptionCombo *m_maintainCombo;

  // Shear
  ClickableLabel *m_shearHLabel;
  ClickableLabel *m_shearVLabel;
  PegbarChannelField *m_shearHField;
  PegbarChannelField *m_shearVField;
  ToolOptionCheckbox *m_lockShearHCheckbox;
  ToolOptionCheckbox *m_lockShearVCheckbox;

  // Center Position
  ClickableLabel *m_ewCenterLabel;
  ClickableLabel *m_nsCenterLabel;
  PegbarCenterField *m_ewCenterField;
  PegbarCenterField *m_nsCenterField;
  ToolOptionCheckbox *m_lockEWCenterCheckbox;
  ToolOptionCheckbox *m_lockNSCenterCheckbox;

  ToolOptionCheckbox *m_globalKey;

  // Flip buttons
  QPushButton *m_hFlipButton, *m_vFlipButton, *m_leftRotateButton,
      *m_rightRotateButton;

  // enables adjusting value by dragging on the label
  void connectLabelAndField(ClickableLabel *label, MeasuredValueField *field);

public:
  ArrowToolOptionsBox(QWidget *parent, TTool *tool, TPropertyGroup *pg,
                      TFrameHandle *frameHandle, TObjectHandle *objHandle,
                      TXsheetHandle *xshHandle, ToolHandle *toolHandle);

  void updateStatus();
  void onStageObjectChange();

protected:
  void showEvent(QShowEvent *);
  void hideEvent(QShowEvent *);

  void setSplined(bool on);
  bool isCurrentObjectSplined() const;

protected slots:
  void onFrameSwitched() { updateStatus(); }
  // update the object list in combobox
  void updateStageObjectComboItems();
  // synchronize the current item in the combobox to the selected stage object
  void syncCurrentStageObjectComboItem();
  // change the current stage object when user changes it via combobox by hand
  void onCurrentStageObjectComboActivated(int index);

  void onCurrentAxisChanged(int);

  void onFlipHorizontal();
  void onFlipVertical();
  void onRotateLeft();
  void onRotateRight();
};

//=============================================================================
//
// SelectionToolOptionsBox
//
//=============================================================================

class DraggableIconView : public QWidget {
  Q_OBJECT
public:
  DraggableIconView(QWidget *parent = 0) : QWidget(parent){};

protected:
  // these are used for dragging on the icon to
  // change the value of the field
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

signals:
  void onMousePress(QMouseEvent *event);
  void onMouseMove(QMouseEvent *event);
  void onMouseRelease(QMouseEvent *event);
};

class IconViewField final : public DraggableIconView {
  Q_OBJECT

public:
  enum IconType {
    Icon_ScalePeg = 0,
    Icon_Rotation,
    Icon_Position,
    Icon_Thickness,
    Icon_Amount
  };

private:
  IconType m_iconType;

protected:
  QPixmap m_pm[Icon_Amount];
  Q_PROPERTY(
      QPixmap ScalePegPixmap READ getScalePegPixmap WRITE setScalePegPixmap);
  Q_PROPERTY(
      QPixmap RotationPixmap READ getRotationPixmap WRITE setRotationPixmap);
  Q_PROPERTY(
      QPixmap PositionPixmap READ getPositionPixmap WRITE setPositionPixmap);

public:
  IconViewField(QWidget *parent = 0, IconType iconType = Icon_ScalePeg);

  QPixmap getScalePegPixmap() const { return m_pm[Icon_ScalePeg]; }
  void setScalePegPixmap(const QPixmap &pixmap) {
    m_pm[Icon_ScalePeg] = pixmap;
  }
  QPixmap getRotationPixmap() const { return m_pm[Icon_Rotation]; }
  void setRotationPixmap(const QPixmap &pixmap) {
    m_pm[Icon_Rotation] = pixmap;
  }
  QPixmap getPositionPixmap() const { return m_pm[Icon_Position]; }
  void setPositionPixmap(const QPixmap &pixmap) {
    m_pm[Icon_Position] = pixmap;
  }

protected:
  void paintEvent(QPaintEvent *e);
};

//-----------------------------------------------------------------------------

class SelectionToolOptionsBox final : public ToolOptionsBox,
                                      public TProperty::Listener {
  Q_OBJECT

  TTool *m_tool;

  ToolOptionCheckbox *m_setSaveboxCheckbox;
  bool m_isVectorSelction;
  QLabel *m_scaleXLabel;
  SelectionScaleField *m_scaleXField;
  QLabel *m_scaleYLabel;
  SelectionScaleField *m_scaleYField;
  DVGui::CheckBox *m_scaleLink;
  SelectionRotationField *m_rotationField;
  QLabel *m_moveXLabel;
  SelectionMoveField *m_moveXField;
  QLabel *m_moveYLabel;
  SelectionMoveField *m_moveYField;
  ThickChangeField *m_thickChangeField;

  ToolOptionPopupButton *m_capStyle;
  ToolOptionPopupButton *m_joinStyle;
  ToolOptionIntSlider *m_miterField;

  QPushButton *m_hFlipButton, *m_vFlipButton, *m_leftRotateButton,
      *m_rightRotateButton;

public:
  SelectionToolOptionsBox(QWidget *parent, TTool *tool,
                          TPaletteHandle *pltHandle, ToolHandle *toolHandle);

  void updateStatus();
  void onPropertyChanged();

protected slots:
  // addToUndo is only set to false when dragging with the mouse
  // to set the value.  It is set to true on mouse release.
  void onScaleXValueChanged(bool addToUndo = true);
  void onScaleYValueChanged(bool addToUndo = true);
  void onSetSaveboxCheckboxChanged(bool);
  void onFlipHorizontal();
  void onFlipVertical();
  void onRotateLeft();
  void onRotateRight();
};

//=============================================================================
//
// GeometricToolOptionsBox
//
//=============================================================================

class GeometricToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  int m_targetType;

  QLabel *m_poligonSideLabel, *m_hardnessLabel;
  ToolOptionSlider *m_hardnessField;
  ToolOptionIntSlider *m_poligonSideField;
  ToolOptionCombo *m_shapeField;
  ToolOptionCheckbox *m_pencilMode;
  ToolOptionIntSlider *m_miterField;
  ToolOptionCheckbox *m_snapCheckbox, *m_smoothCheckbox, *m_drawOrderCheckbox;
  ToolOptionCombo *m_snapSensitivityCombo;
  TTool *m_tool;

private:
  void filterControls();

public:
  GeometricToolOptionsBox(QWidget *parent, TTool *tool,
                          TPaletteHandle *pltHandle, ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onShapeValueChanged(int);
  void onPencilModeToggled(bool);
  void onJoinStyleChanged(int);
};

//=============================================================================
//
// TypeToolOptionsBox
//
//=============================================================================

class TypeToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  TTool *m_tool;

public:
  TypeToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onFieldChanged();
};

//=============================================================================
//
// PaintbrushToolOptionsBox
//
//=============================================================================

class PaintbrushToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  ToolOptionCombo *m_colorMode;
  ToolOptionCheckbox *m_selectiveMode;
  ToolOptionCheckbox *m_lockAlphaMode;

public:
  PaintbrushToolOptionsBox(QWidget *parent, TTool *tool,
                           TPaletteHandle *pltHandle, ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onColorModeChanged(int);
};

//=============================================================================
//
// FullColorFillToolOptionsBox
//
//=============================================================================

class FullColorFillToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  ToolOptionCombo *m_rasterGapSettings;
  ToolOptionSlider *m_rasterGapSlider;
  StyleIndexFieldAndChip *m_styleIndex;

  QLabel *m_gapSliderLabel, *m_styleIndexLabel, *m_rasterGapLabel;

public:
  FullColorFillToolOptionsBox(QWidget *parent, TTool *tool,
                              TPaletteHandle *pltHandle,
                              ToolHandle *toolHandle);

  void checkGapSettingsVisibility();

protected slots:
  void onGapSettingChanged(int);
};

//=============================================================================
//
// FillToolOptionsBox
//
//=============================================================================

class FillToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  int m_targetType;
  QLabel *m_fillDepthLabel;
  ToolOptionCombo *m_colorMode, *m_toolType, *m_rasterGapSettings,
      *m_multiFrameMode;
  ToolOptionCheckbox *m_selectiveMode, *m_segmentMode, *m_onionMode, *m_autopaintMode;
  ToolOptionPairSlider *m_fillDepthField;
  ToolOptionSlider *m_rasterGapSlider;
  StyleIndexFieldAndChip *m_styleIndex;
  QLabel *m_gapSliderLabel, *m_styleIndexLabel, *m_rasterGapLabel;
  ToolOptionCheckbox *m_referenced;

public:
  FillToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);

  void updateStatus();
  void checkGapSettingsVisibility();

protected slots:
  void onColorModeChanged(int);
  void onToolTypeChanged(int);
  void onOnionModeToggled(bool);
  void onMultiFrameModeChanged(int);
  void onGapSettingChanged(int);
};

//=============================================================================
//
// BrushToolOptionsBox
//
//=============================================================================

class BrushToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  TTool *m_tool;

  ToolOptionCheckbox *m_pencilMode;
  QLabel *m_hardnessLabel;
  ToolOptionSlider *m_hardnessField;
  ToolOptionPopupButton *m_joinStyleCombo;
  ToolOptionIntSlider *m_miterField;
  ToolOptionCombo *m_presetCombo;
  ToolOptionCheckbox *m_snapCheckbox, *m_drawOrderCheckbox,
      *m_autoCloseCheckbox, *m_autoGroupCheckbox, *m_autoFillCheckbox;
  ToolOptionCombo *m_snapSensitivityCombo;
  QPushButton *m_addPresetButton;
  QPushButton *m_removePresetButton;

private:
  class PresetNamePopup;
  PresetNamePopup *m_presetNamePopup;
  void filterControls();

public:
  BrushToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                      ToolHandle *toolHandle);

  void updateStatus();

protected slots:

  void onPencilModeToggled(bool);
  void onAddPreset();
  void onRemovePreset();
};

//=============================================================================
//
// EraserToolOptionsBox
//
//=============================================================================

class EraserToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  ToolOptionCheckbox *m_pencilMode, *m_invertMode, *m_eraseOnlySavebox,
      *m_pressure;
  ToolOptionCombo *m_toolType, *m_colorMode, *m_multiFrameMode;
  QLabel *m_hardnessLabel, *m_colorModeLabel;
  ToolOptionSlider *m_hardnessField;

public:
  EraserToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                       ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onPencilModeToggled(bool);
  void onToolTypeChanged(int);
  void onColorModeChanged(int);
};

//=============================================================================
//
// RulerToolOptionsBox
//
//=============================================================================

class RulerToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  MeasuredValueField *m_Xfld;
  MeasuredValueField *m_Yfld;
  MeasuredValueField *m_Wfld;
  MeasuredValueField *m_Hfld;
  MeasuredValueField *m_Afld;
  MeasuredValueField *m_Lfld;

  QLabel *m_XpixelFld;
  QLabel *m_YpixelFld;
  QLabel *m_WpixelFld;
  QLabel *m_HpixelFld;

  TTool *m_tool;

public:
  RulerToolOptionsBox(QWidget *parent, TTool *tool);

  void updateValues(bool isRasterLevelEditing, double X, double Y, double W,
                    double H, double A, double L, int Xpix = 0, int Ypix = 0,
                    int Wpix = 0, int Hpix = 0);

  void resetValues();
};

//=============================================================================
//
// TapeToolOptionsBox
//
//=============================================================================

class TapeToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

  ToolOptionCheckbox *m_smoothMode, *m_joinStrokesMode;
  ToolOptionCombo *m_toolMode, *m_typeMode, *m_multiFrameMode;
  QLabel *m_autocloseLabel;
  ToolOptionPairSlider *m_autocloseField;

public:
  TapeToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);

  void updateStatus();

protected slots:
  void onToolTypeChanged(int);
  void onToolModeChanged(int);
  void onJoinStrokesModeChanged();
};

//=============================================================================
//
// RGBPickerToolOptionsBox
//
//=============================================================================

class RGBPickerToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT
  ToolOptionCheckbox *m_realTimePickMode;
  // label with background color
  RGBLabel *m_currentRGBLabel;

public:
  RGBPickerToolOptionsBox(QWidget *parent, TTool *tool,
                          TPaletteHandle *pltHandle, ToolHandle *toolHandle,
                          PaletteController *paletteController);
  void updateStatus();
protected slots:
  void updateRealTimePickLabel(const QColor &);
};

//=============================================================================
//
// StylePickerToolOptionsBox
//
//=============================================================================

class StylePickerToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT
  ToolOptionCheckbox *m_realTimePickMode;

  QLabel *m_currentStyleLabel;

public:
  StylePickerToolOptionsBox(QWidget *parent, TTool *tool,
                            TPaletteHandle *pltHandle, ToolHandle *toolHandle,
                            PaletteController *paletteController);
  void updateStatus();
protected slots:
  void updateRealTimePickLabel(const int, const int, const int);
};

//=============================================================================
//
// ShiftTraceToolOptionBox
// shown only when "Edit Shift" mode is active
//
//=============================================================================

class ShiftTraceToolOptionBox final : public ToolOptionsBox {
  Q_OBJECT
  QFrame *m_prevFrame, *m_afterFrame;
  QRadioButton *m_prevRadioBtn, *m_afterRadioBtn;
  QPushButton *m_resetPrevGhostBtn, *m_resetAfterGhostBtn;
  TTool *m_tool;
  void resetGhost(int index);

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QShowEvent *);

public:
  ShiftTraceToolOptionBox(QWidget *parent = 0, TTool *tool = 0);
  void updateStatus() override;
protected slots:
  void onResetPrevGhostBtnPressed();
  void onResetAfterGhostBtnPressed();
  void onPrevRadioBtnClicked();
  void onAfterRadioBtnClicked();
  void updateColors();
};

//=============================================================================
//
// PerspectiveToolOptionBox
// shown only when "Edit Perspective" mode is active
//
//=============================================================================

class PerspectiveGridToolOptionBox final : public ToolOptionsBox {
  Q_OBJECT

  TTool *m_tool;

  ToolOptionCombo *m_perspectiveType;
  ToolOptionSlider *m_opacity;
  ColorChipCombo *m_color;
  ToolOptionCheckbox *m_horizon, *m_parallel, *m_advancedControls;
  ClickableLabel *m_rotationLabel, *m_spacingLabel;
  MeasuredValueField *m_rotation, *m_spacing;
  ToolOptionCombo *m_presetCombo;
  QPushButton *m_addPresetButton, *m_removePresetButton, *m_leftRotateButton,
      *m_rightRotateButton;

private:
  class PresetNamePopup;
  PresetNamePopup *m_presetNamePopup;
  void filterControls();

public:
  PerspectiveGridToolOptionBox(QWidget *parent, TTool *tool,
                               TPaletteHandle *pltHandle,
                               ToolHandle *toolHandle);
  void updateStatus();
  void updateMeasuredValues(double spacing, double rotation);

  void reloadPresetCombo();

protected slots:

  void onAddPreset();
  void onRemovePreset();
  void onPerspectiveTypeChanged(int);
  void onSpacingChange(TMeasuredValue *fld);
  void onRotationChange(TMeasuredValue *fld);
  void onRotateLeft();
  void onRotateRight();
};

//=============================================================================
//
// SymmetryToolOptionBox
// shown only when "Edit Perspective" mode is active
//
//=============================================================================

class SymmetryToolOptionBox final : public ToolOptionsBox {
  Q_OBJECT

  TTool *m_tool;

  ToolOptionIntSlider *m_lines;
  ToolOptionSlider *m_opacity;
  ColorChipCombo *m_color;
  ToolOptionCheckbox *m_useLineSymmetry;
  ClickableLabel *m_rotationLabel;
  MeasuredValueField *m_rotation;
  QPushButton *m_leftRotateButton, *m_rightRotateButton;
  ToolOptionCombo *m_presetCombo;
  QPushButton *m_addPresetButton;
  QPushButton *m_removePresetButton;

private:
  class PresetNamePopup;
  PresetNamePopup *m_presetNamePopup;
  void filterControls();

public:
  SymmetryToolOptionBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                        ToolHandle *toolHandle);
  void updateStatus();
  void updateMeasuredValues(double rotation);

protected slots:

  void onLinesChanged();
  void onRotationChange(TMeasuredValue *fld);
  void onRotateLeft();
  void onRotateRight();
  void onResetPosition();
  void onAddPreset();
  void onRemovePreset();
};

//=============================================================================
//
// ZoomToolOptionsBox
//
//=============================================================================

class ZoomToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

public:
  ZoomToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);
};

//=============================================================================
//
// RotateToolOptionsBox
//
//=============================================================================

class RotateToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

public:
  RotateToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                       ToolHandle *toolHandle);
};

//=============================================================================
//
// HandToolOptionsBox
//
//=============================================================================

class HandToolOptionsBox final : public ToolOptionsBox {
  Q_OBJECT

public:
  HandToolOptionsBox(QWidget *parent, TTool *tool, TPaletteHandle *pltHandle,
                     ToolHandle *toolHandle);
};

//-----------------------------------------------------------------------------

class DVAPI ToolOptions final : public QFrame {
  Q_OBJECT

  int m_width, m_height;
  std::map<TTool *, ToolOptionsBox *> m_panels;
  QWidget *m_panel;

public:
  ToolOptions();
  ~ToolOptions();

  QWidget *getPanel() const { return m_panel; }

protected:
  void showEvent(QShowEvent *);
  void hideEvent(QShowEvent *);

public slots:

  void onToolSwitched();
  void onToolChanged();
  void onStageObjectChange();

signals:
  // used in ComboViewer to handle Tab focus
  void newPanelCreated();

  //  void toolOptionChange();
};

#endif  // PANE_H
