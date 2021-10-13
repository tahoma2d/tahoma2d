#pragma once

#ifndef STYLEEDITOR_H
#define STYLEEDITOR_H

// TnzCore includes
#include "tcommon.h"
#include "tfilepath.h"
#include "tpixel.h"
#include "tpalette.h"
#include "saveloadqsettings.h"
#include "../toonz/tapplication.h"

// TnzLib includes
#include "toonz/tpalettehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/txshlevel.h"

// TnzQt includes
#include "toonzqt/checkbox.h"
#include "toonzqt/intfield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"
#include "toonzqt/tabbar.h"
#include "toonzqt/glwidget_for_highdpi.h"
#include "toonzqt/dvdialog.h"

// Qt includes
#include <QWidget>
#include <QFrame>
#include <QTabBar>
#include <QSlider>
#include <QToolButton>
#include <QScrollArea>
#include <QMouseEvent>
#include <QPointF>
#include <QSettings>
#include <QSplitter>
#include <QRadioButton>
#include <QLabel>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//=============================================

//    Forward declarations

class TColorStyle;
class TPalette;

class TXshLevelHandle;
class PaletteController;

class QGridLayout;
class QLabel;
class QStackedWidget;
class QSlider;
class QRadioButton;
class QButtonGroup;
class QPushButton;
class QTabWidget;
class QToolBar;
class QOpenGLFramebufferObject;
class QWidgetAction;

class ColorSquaredWheel;
class TabBarContainter;
class StyleChooser;
class StyleEditor;
class LutCalibrator;

//=============================================

class HexLineEdit : public QLineEdit {
  Q_OBJECT

public:
  HexLineEdit(const QString &contents, QWidget *parent)
      : QLineEdit(contents, parent) {}
  ~HexLineEdit() {}

protected:
  void focusInEvent(QFocusEvent *event) override;
  void showEvent(QShowEvent *event) override;
};

//=============================================================================
namespace StyleEditorGUI {
//=============================================================================

enum ColorChannel {
  eRed = 0,
  eGreen,
  eBlue,
  eAlpha,
  eHue,
  eSaturation,
  eValue
};

//=============================================================================
/*! \brief The ColorModel provides an object to manage color change and
                its transformation from rgb value to hsv value and vice versa.

                This object change color using its rgb channel or its hsv
   channel;
                if you change a color channel class assure you that other
   channel not change.
*/
class DVAPI ColorModel {
  int m_channels[7];
  void rgb2hsv();
  void hsv2rgb();

public:
  ColorModel();

  void setTPixel(const TPixel32 &color);
  TPixel32 getTPixel() const;

  void setValue(ColorChannel channel, int value);
  void setValues(ColorChannel channel, int u, int v);
  int getValue(ColorChannel channel) const;
  void getValues(ColorChannel channel, int &u, int &v);

  inline int r() const { return m_channels[0]; }
  inline int g() const { return m_channels[1]; }
  inline int b() const { return m_channels[2]; }
  inline int a() const { return m_channels[3]; }
  inline int h() const { return m_channels[4]; }
  inline int s() const { return m_channels[5]; }
  inline int v() const { return m_channels[6]; }

  bool operator==(const ColorModel &cm) {
    int i;
    for (i = 0; i < 7; i++)
      if (m_channels[i] != cm.getValue(ColorChannel(i))) return false;
    return true;
  }
};

//=============================================

enum CurrentWheel { none, leftWheel, rightTriangle };

class DVAPI HexagonalColorWheel final : public GLWidgetForHighDpi {
  Q_OBJECT

  // backgoround color (R160, G160, B160)
  QColor m_bgColor;
  Q_PROPERTY(QColor BGColor READ getBGColor WRITE setBGColor)

  ColorModel m_color;
  QPointF m_wheelPosition;
  float m_triEdgeLen;
  float m_triHeight;
  QPointF m_wp[7], m_leftp[3];

  CurrentWheel m_currentWheel;

  // used for color calibration with 3DLUT
  QOpenGLFramebufferObject *m_fbo = NULL;
  LutCalibrator *m_lutCalibrator  = NULL;

  bool m_firstInitialized      = true;
  bool m_cuedCalibrationUpdate = false;

private:
  void drawCurrentColorMark();
  void clickLeftWheel(const QPoint &pos);
  void clickRightTriangle(const QPoint &pos);

public:
  HexagonalColorWheel(QWidget *parent);
  void setColor(const ColorModel &color) { m_color = color; };

  ~HexagonalColorWheel();

  void setBGColor(const QColor &color) { m_bgColor = color; }
  QColor getBGColor() const { return m_bgColor; }

  void updateColorCalibration();
  void cueCalibrationUpdate() { m_cuedCalibrationUpdate = true; }

protected:
  void initializeGL() override;
  void resizeGL(int width, int height) override;
  void paintGL() override;
  QSize SizeHint() const { return QSize(300, 200); };

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  void showEvent(QShowEvent *) override;

signals:
  void colorChanged(const ColorModel &color, bool isDragging);

public slots:
  void onContextAboutToBeDestroyed();
};

//=============================================================================
/*! \brief The SquaredColorWheel is a squared color to change color.

                Inherits \b QWidget.

                This object show a square faded from one color channel to
   another color channel,
                the two channel represent x and y axis of square.
                It's possible to choose viewed shade using \b setChannel().
                Click in square change current SquaredColorWheel.
*/
class DVAPI SquaredColorWheel final : public QWidget {
  Q_OBJECT
  ColorChannel m_channel;
  ColorModel m_color;

public:
  SquaredColorWheel(QWidget *parent);

  /*! Doesn't call update(). */
  void setColor(const ColorModel &color);

protected:
  void paintEvent(QPaintEvent *event) override;

  void click(const QPoint &pos);
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

public slots:
  /*! Connect channels to the two square axes:
                  \li eRed : connect x-axis to eGreen and y-axis to eBlue;
                  \li eGreen : connect x-axis to eRed and y-axis to eBlue;
                  \li eBlue : connect x-axis to eRed and y-axis to eGreen;
                  \li eHue : connect x-axis to eSaturation and y-axis to eValue;
                  \li eSaturation : connect x-axis to eHue and y-axis to eValue;
                  \li eValue : connect x-axis to eHue and y-axis to eSaturation;
     */
  void setChannel(int channel);

signals:
  void colorChanged(const ColorModel &color, bool isDragging);
};

//=============================================================================
/*! \brief The ColorSlider is used to set a color channel.

                Inherits \b QSlider.

                This object show a bar which colors differ from minimum to
   maximum channel color
                value.
*/
class DVAPI ColorSlider final : public QSlider {
  Q_OBJECT
public:
  ColorSlider(Qt::Orientation orientation, QWidget *parent = 0);

  /*! set channel and color. doesn't call update(). */
  void setChannel(ColorChannel channel);
  void setColor(const ColorModel &color);

  ColorChannel getChannel() const { return m_channel; }

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

  //	QIcon getFirstArrowIcon();
  //	QIcon getLastArrowIcon();
  //	QRect getFirstArrowRect();
  //	QRect getLastArrowRect();

private:
  ColorChannel m_channel;
  ColorModel m_color;
};

//=============================================================================
// ArrowButton

class ArrowButton final : public QToolButton {
  Q_OBJECT

  Qt::Orientation m_orientaion;
  bool m_isFirstArrow;

  int m_timerId;
  int m_firstTimerId;

public:
  ArrowButton(QWidget *parent = 0, Qt::Orientation orientation = Qt::Horizontal,
              bool m_isFirstArrow = true);

protected:
  void stopTime(int timerId);
  void timerEvent(QTimerEvent *event) override;
  void notifyChanged();

protected slots:
  void onPressed();
  void onRelease();

signals:
  void add();
  void remove();
};

//=============================================================================
/*! \brief The ColorSliderBar is a colorSlider with  two arrow to add or remove
   one to current value.

                Inherits \b QToolBar.
*/
class DVAPI ColorSliderBar final : public QWidget {
  Q_OBJECT

  ColorSlider *m_colorSlider;

public:
  ColorSliderBar(QWidget *parent             = 0,
                 Qt::Orientation orientation = Qt::Vertical);

  void setValue(int value) { m_colorSlider->setValue(value); }
  void setRange(int minValue, int maxValue) {
    m_colorSlider->setRange(minValue, maxValue);
  }

  void setChannel(ColorChannel channel) {
    return m_colorSlider->setChannel(channel);
  }
  void setColor(const ColorModel &color) {
    return m_colorSlider->setColor(color);
  }

  ColorChannel getChannel() const { return m_colorSlider->getChannel(); }

protected slots:
  void onRemove();
  void onAdd();

signals:
  void valueChanged(int);
  void valueChanged();
};

//=============================================================================
/*! \brief The ChannelLineEdit is a cutomized version of IntLineEdit for channel
   value.
    It calls selectAll() at the moment of the first click.
*/
class ChannelLineEdit final : public DVGui::IntLineEdit {
  Q_OBJECT

  bool m_isEditing;

public:
  ChannelLineEdit(QWidget *parent, int value, int minValue, int maxValue)
      : IntLineEdit(parent, value, minValue, maxValue), m_isEditing(false) {}

protected:
  void mousePressEvent(QMouseEvent *) override;
  void focusOutEvent(QFocusEvent *) override;
  void paintEvent(QPaintEvent *) override;
};

//=============================================================================
/*! \brief ColorChannelControl is the widget used to show/edit a channel

                Inherits \b QWidget.

                The ColorChannelControl is composed of three object: a label \b
   QLabel
                to show the channel name, and an \b IntLineEdit and a
   ColorSlider to show/edit the
                channel value.
*/
class DVAPI ColorChannelControl final : public QWidget {
  Q_OBJECT
  QLabel *m_label;
  ChannelLineEdit *m_field;
  ColorSlider *m_slider;

  ColorChannel m_channel;
  ColorModel m_color;

  int m_value;
  bool m_signalEnabled;

public:
  ColorChannelControl(ColorChannel channel, QWidget *parent = 0);
  void setColor(const ColorModel &color);

protected slots:
  void onFieldChanged();
  void onSliderChanged(int value);
  void onSliderReleased();

  void onAddButtonClicked();
  void onSubButtonClicked();

signals:
  void colorChanged(const ColorModel &color, bool isDragging);
};

//=============================================================================
/*! \brief The StyleEditorPage is the base class of StyleEditor pages.

                Inherits \b QFrame.
                Inherited by \b PlainColorPage and \b StyleChooserPage.
*/
class StyleEditorPage : public QFrame {
public:
  StyleEditor *m_editor;

  std::vector<int> m_selection;

  StyleEditorPage(QWidget *parent);
};

//=============================================================================
/*! \brief The ColorParameterSelector is used for styles having more
    than one color parameter to select the current one.

                Inherits \b QWidget.
*/
class ColorParameterSelector final : public QWidget {
  Q_OBJECT

  std::vector<QColor> m_colors;
  int m_index;
  const QSize m_chipSize;
  const QPoint m_chipOrigin, m_chipDelta;

public:
  ColorParameterSelector(QWidget *parent);
  int getSelected() const { return m_index; }
  void setStyle(const TColorStyle &style);
  void clear();

signals:
  void colorParamChanged();

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;

  QSize sizeHint() const override;
};

//=============================================================================
/*! \brief The PlainColorPage is used to control the color parameter.

                Inherits \b StyleEditorPage.

                The PlainColorPage is made of a \b SquaredColorWheel and a \b
   ColorSlider,
                a collection of \b ColorChannelControl, and a number of radio
   button (to control
                the ColorWheel behaviour).
*/
class PlainColorPage final : public StyleEditorPage {
  Q_OBJECT

  // ColorSliderBar *m_verticalSlider;
  // QRadioButton *m_modeButtons[7];
  ColorChannelControl *m_channelControls[7];
  // SquaredColorWheel *m_squaredColorWheel; //iwsw not used

  HexagonalColorWheel *m_hexagonalColorWheel;

  ColorModel m_color;
  bool m_signalEnabled;
  bool m_isVertical = true;
  int m_visibleParts;
  void updateControls();

  // QGridLayout *m_mainLayout;
  QFrame *m_slidersContainer;
  QSplitter *m_vSplitter;

public:
  PlainColorPage(QWidget *parent = 0);
  ~PlainColorPage() {}

  QFrame *m_wheelFrame;
  QFrame *m_hsvFrame;
  QFrame *m_alphaFrame;
  QFrame *m_rgbFrame;
  void setColor(const TColorStyle &style, int colorParameterIndex);

  void setIsVertical(bool isVertical);
  bool getIsVertical() { return m_isVertical; }
  QByteArray getSplitterState();
  void setSplitterState(QByteArray state);

  void updateColorCalibration();

protected:
  void resizeEvent(QResizeEvent *) override;

signals:
  void colorChanged(const ColorModel &, bool isDragging);

protected slots:
  void onWheelChanged(const ColorModel &color, bool isDragging);
  // void onWheelSliderChanged(int value);
  // void onWheelSliderReleased();

public slots:
  // void setWheelChannel(int channel);
  void onControlChanged(const ColorModel &color, bool isDragging);
  void toggleOrientation();
};

//=============================================================================
/*! \brief The StyleChooserPage is the base class of pages with texture,
    special style and custom style. It features a collection of selectable
   'chips'.

                Inherits \b StyleEditorPage.
*/
enum StylePageType {
  Unknown = 0,
  Texture,
  VectorGenerated,
  VectorCustom,
  VectorBrush,
  Raster
};

class StyleChooserPage : public StyleEditorPage {
  Q_OBJECT

protected:
  QPoint m_chipOrigin;
  QSize m_chipSize;
  int m_chipPerRow;
  StylePageType m_pageType = StylePageType::Unknown;
  TFilePath m_stylesFolder;
  int m_folderDepth;
  QString m_styleSetName;
  bool m_allowPageDelete;

  bool m_favorite      = false;
  bool m_myFavoriteSet = false;
  bool m_allowFavorite = false;
  bool m_external      = false;

public:
  StyleChooserPage(TFilePath styleFolder, QWidget *parent = 0);

  QSize getChipSize() const { return m_chipSize; }

  TFilePath getStylesFolder() { return m_stylesFolder; }

  void setPageType(StylePageType pageType) { m_pageType = pageType; }
  StylePageType getPageType() { return m_pageType; }

  virtual void setFavorite(bool favorite) { m_favorite = favorite; }
  bool isFavorite() { return m_favorite; }

  void setMyFavoriteSet(bool myFavorite) { m_myFavoriteSet = myFavorite; }
  bool isMyFavoriteSet() { return m_myFavoriteSet; }

  void setAllowFavorite(bool allow) { m_allowFavorite = allow; }
  bool allowFavorite() { return m_allowFavorite; }

  virtual void setExternal(bool external) { m_external = external; }
  bool isExternal() { return m_external; }

  void setFolderDepth(int depth) { m_folderDepth = depth; }
  bool isRootFolder() { return !m_folderDepth; }

  void clearSelection() { m_selection.clear(); }
  std::vector<int> getSelection() { return m_selection; }

  virtual void loadItems() {}
  virtual bool loadIfNeeded() = 0;
  virtual bool isLoading() { return false; }
  virtual int getChipCount() const = 0;

  virtual void drawChip(QPainter &p, QRect rect, int index) = 0;
  virtual void onSelect(int index) {}

  virtual void removeSelectedStylesFromSet(std::vector<int> selection){};
  virtual void addSelectedStylesToSet(std::vector<int> selection,
                                      TFilePath setPath){};
  virtual void updateFavorite(){};
  virtual void addSelectedStylesToPalette(std::vector<int> selection){};

  virtual void changeStyleSetFolder(TFilePath newPath) {
    m_stylesFolder = newPath;
  }

  bool copyFilesToStyleFolder(TFilePathSet srcFiles, TFilePath destDir);
  bool deleteFilesFromStyleFolder(TFilePathSet targetFiles);

  void processContextMenuEvent(QContextMenuEvent *event) {
    contextMenuEvent(event);
  }

  void setStyleSetName(QString name) { m_styleSetName = name; }
  QString getStyleSetName() { return m_styleSetName; }

  void setAllowPageDelete(bool allowDelete) { m_allowPageDelete = allowDelete; }
  bool canDeletePage() { return m_allowPageDelete; }

protected:
  int m_currentIndex;

  int posToIndex(const QPoint &pos) const;

  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override { computeSize(); }

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override {}
  void mouseReleaseEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void enterEvent(QEvent *event) override;

protected slots:
  void computeSize();
  void onTogglePage(bool toggled);
  void onRemoveStyleFromSet();
  void onEmptySet();
  void onAddStyleToFavorite();
  void onAddStyleToPalette();
  void onCopyStyleToSet();
  void onMoveStyleToSet();
  void onAddSetToPalette();
  void onUpdateFavorite();
  void onRemoveStyleSet();
  void onReloadStyleSet();
  void onRenameStyleSet();
  void onLabelContextMenu(const QPoint &pos);
signals:
  void styleSelected(const TColorStyle &style);
  void refreshFavorites();
  void customStyleSelected();
};

//=============================================================================

/*!
  \brief    The SettingsPage is used to show/edit style parameters.

  \details  This class stores the GUI for editing a \a copy of the
            current color style. Updates of the actual current color
            style are \a not performed directly by this class.
*/

class SettingsPage final : public QScrollArea {
  Q_OBJECT

  QGridLayout *m_paramsLayout;

  QCheckBox *m_autoFillCheckBox;

  TColorStyleP m_editedStyle;  //!< A copy of the current style being edited by
                               //! the Style Editor.

  bool
      m_updating;  //!< Whether the page is copying style content to its widget,
                   //!  to be displayed.
private:
  int getParamIndex(const QWidget *widget);

public:
  SettingsPage(QWidget *parent);

  void setStyle(const TColorStyleP &editedStyle);
  void updateValues();

  void enableAutopaintToggle(bool enabled);

signals:

  void paramStyleChanged(
      bool isDragging);  //!< Signals that the edited style has changed.

private slots:

  void onAutofillChanged();
  void onValueChanged(bool isDragging = false);
  void onValueReset();
};

//=============================================================================
}  // namespace StyleEditorGUI
//=============================================================================

using namespace StyleEditorGUI;

//=============================================================================
// Clickable Label
//-----------------------------------------------------------------------------

class ClickableLabel : public QLabel {
  Q_OBJECT

public:
  ClickableLabel(const QString &text, QWidget *parent = nullptr,
                 Qt::WindowFlags f = Qt::WindowFlags());
  ~ClickableLabel();

protected:
  void mousePressEvent(QMouseEvent *event);

signals:
  void click();
};

//=============================================================================
// RenameStyleSet
//-----------------------------------------------------------------------------

class RenameStyleSet final : public QLineEdit {
  Q_OBJECT

protected:
  StyleChooserPage *m_page;
  StyleEditor *m_editor;

  bool m_validatingName;

public:
  RenameStyleSet(QWidget *parent);
  ~RenameStyleSet() {}

  void show(const QRect &rect);

  void setStyleSetPage(StyleChooserPage *page) { m_page = page; }
  void setEditor(StyleEditor *editor) { m_editor = editor; }

protected:
  void focusOutEvent(QFocusEvent *) override;

protected slots:
  void renameSet();
};

//=============================================================================
// New Style Set Popup
//-----------------------------------------------------------------------------

class NewStyleSetPopup : public DVGui::Dialog {
  Q_OBJECT

protected:
  DVGui::LineEdit *m_nameFld;
  DVGui::CheckBox *m_isFavorite;
  QButtonGroup *m_styleSetType;
  QRadioButton *m_texture, *m_vectorCustom, *m_vectorBrush, *m_raster;
  StyleEditor *m_editor;

  StylePageType m_pageType;

public:
  NewStyleSetPopup(StylePageType pageType, QWidget *parent);

protected:
  void showEvent(QShowEvent *event) override { m_nameFld->setFocus(); }

public slots:
  void createStyleSet();

private slots:
  void onFavoriteToggled();
};

//=============================================================================
// StyleEditor
//-----------------------------------------------------------------------------

class DVAPI StyleEditor final : public QWidget, public SaveLoadQSettings {
  Q_OBJECT
  TApplication *m_app;

  PaletteController *m_paletteController;
  TPaletteHandle *m_paletteHandle;
  TPaletteHandle *m_cleanupPaletteHandle;
  HexLineEdit *m_hexLineEdit;
  QWidgetAction *m_hexAction;
  QWidget *m_parent;
  TXshLevelHandle
      *m_levelHandle;  //!< for clearing the level cache when the color changed

  DVGui::TabBar *m_styleBar;
  QStackedWidget *m_styleChooser;

  DVGui::StyleSample
      *m_newColor;  //!< New style viewer (lower-right panel side).
  DVGui::StyleSample
      *m_oldColor;  //!< Old style viewer (lower-right panel side).
  QFrame *m_fillColorWidget;
  QAction *m_toggleOrientationAction;
  QPushButton
      *m_autoButton;  //!< "Auto Apply" checkbox on the right panel side.
  QPushButton *m_applyButton;  //!< "Apply" button on the right panel side.
  QToolButton *m_styleSetsButton;

  QToolBar *m_toolBar;                               //!< Lower toolbar.
  ColorParameterSelector *m_colorParameterSelector;  //!< Secondary color
                                                     //! parameter selector in
  //! the lower toolbar.

  TabBarContainter *m_tabBarContainer;  //!< Tabs container for style types.

  // QLabel *m_statusLabel;  //!< showing the information of the current palette
  //! and style.

  PlainColorPage *m_plainColorPage;
  SettingsPage *m_settingsPage;
  QScrollArea *m_textureArea;
  QScrollArea *m_vectorArea;
  QScrollArea *m_rasterArea;
  QAction *m_wheelAction;
  QAction *m_hsvAction;
  QAction *m_alphaAction;
  QAction *m_rgbAction;

  TColorStyleP
      m_oldStyle;  //!< A copy of current style \a before the last change.
  TColorStyleP m_editedStyle;  //!< The currently edited style. Please observe
                               //! that this is
  //!  a \b copy of currently selected style, since style edits
  //!  may be not <I>automatically applied</I>.
  bool m_enabled;
  bool m_enabledOnlyFirstTab;
  bool m_enabledFirstAndLastTab;
  bool m_colorPageIsVertical = true;

  QScrollArea *m_textureOutsideArea;
  QScrollArea *m_rasterOutsideArea;
  QScrollArea *m_vectorOutsideArea;

  std::vector<QPushButton *> m_textureButtons;
  std::vector<QPushButton *> m_vectorButtons;
  std::vector<QPushButton *> m_rasterButtons;

  std::vector<ClickableLabel *> m_textureLabels;
  std::vector<ClickableLabel *> m_vectorLabels;
  std::vector<ClickableLabel *> m_rasterLabels;

  std::vector<StyleChooserPage *> m_texturePages;
  std::vector<StyleChooserPage *> m_vectorPages;
  std::vector<StyleChooserPage *> m_rasterPages;

  QMenu *m_textureMenu;
  QMenu *m_vectorMenu;
  QMenu *m_rasterMenu;

  bool m_isAltPressed  = false;
  bool m_isCtrlPressed = false;

  RenameStyleSet *m_renameStyleSet;

public:
  StyleEditor(PaletteController *, QWidget *parent = 0);
  ~StyleEditor();

  void setApplication(TApplication *app) { m_app = app; }
  TApplication *getApplication() { return m_app; }

  void setPaletteHandle(TPaletteHandle *paletteHandle);
  TPaletteHandle *getPaletteHandle() const { return m_paletteHandle; }

  void setLevelHandle(TXshLevelHandle *levelHandle) {
    m_levelHandle = levelHandle;
  }

  TPalette *getPalette() { return m_paletteHandle->getPalette(); }
  int getStyleIndex() { return m_paletteHandle->getStyleIndex(); }

  void enableAutopaintToggle(bool enabled) {
    m_settingsPage->enableAutopaintToggle(enabled);
  }

  // SaveLoadQSettings
  virtual void save(QSettings &settings) const override;
  virtual void load(QSettings &settings) override;

  void updateColorCalibration();

  void createStylePage(StylePageType pageType, TFilePath styleFolder,
                       QString filters = QString("*"), bool isFavorite = false,
                       int dirDepth = 0);

  void initializeStyleMenus();

  bool isAltPressed() { return m_isAltPressed; }
  bool isCtrlPressed() { return m_isCtrlPressed; }

  void clearSelection();

  bool isSelecting();
  bool isSelectingFavorites();
  bool isSelectingFavoritesOnly();
  bool isSelectingNonFavoritesOnly();

  void addToPalette(const TColorStyle &style);

  QStringList savePageStates(StylePageType pageType) const;
  void loadPageStates(StylePageType pageType, QStringList pageStateData);

  void createNewStyleSet(StylePageType pageType, TFilePath pagePath,
                         bool isFavorite);
  void removeStyleSet(StyleChooserPage *styleSetPage);
  void removeStyleSetAtIndex(int index, int pageIndex);
  void editStyleSetName(StyleChooserPage *styleSetPage);
  void renameStyleSet(StyleChooserPage *styleSetPage, QString newName);

  std::vector<StyleChooserPage *> getStyleSetList(StylePageType pageType);

  void setUpdated(TFilePath setPath);
  TFilePath getSetStyleFolder(QString setName);

  void updatePage(int pageIndex);

  QString getStylePageFilter(StylePageType pageType);

  bool isStyleNameValid(QString name, StylePageType pageType, bool isFavorite);

protected:
  /*! Return false if style is linked and style must be set to null.*/
  bool setStyle(TColorStyle *currentStyle);

  void setEditedStyleToStyle(const TColorStyle *style);  //!< Clones the
                                                         //! supplied style and
  //! considers that as
  //! the edited one.
  void setOldStyleToStyle(const TColorStyle *style);  //!< Clones the supplied
                                                      //! style and considers
  //! that as the previously
  //! current one.
  //!  \todo  Why is this not assimilated to setCurrentStyleToStyle()?

  /*! Return style parameter index selected in \b ColorParameterSelector. */
  int getColorParam() const { return m_colorParameterSelector->getSelected(); }

  /*! Set StyleEditor view to \b enabled. If \b enabledOnlyFirstTab or if \b
     enabledFirstAndLastTab
                  is true hide other tab, pay attention \b enabled must be true
     or StyleEditor is disabled. */
  void enable(bool enabled, bool enabledOnlyFirstTab = false,
              bool enabledFirstAndLastTab = false);

protected:
  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void enterEvent(QEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;

protected slots:

  void onStyleSwitched();
  void onStyleChanged(bool isDragging);
  void onCleanupStyleChanged(bool isDragging);
  void onOldStyleClicked(const TColorStyle &);
  void updateOrientationButton();
  void checkPaletteLock();
  // called (e.g.) by PaletteController when an other StyleEditor change the
  // toggle
  void enableColorAutoApply(bool enabled);

  // when colorAutoApply==false this slot is called when the current color
  // changes
  void setColorSample(const TPixel32 &color);

  // chiamato quando viene modificato uno slider o la color wheel
  void onColorChanged(const ColorModel &, bool isDragging);

  void selectStyle(const TColorStyle &style);

  void applyButtonClicked();
  void autoCheckChanged(bool value);

  void setPage(int index);

  void onColorParamChanged();

  void onParamStyleChanged(bool isDragging);

  void onHexChanged();
  void onHexEdited(const QString &text);
  void onHideMenu();
  void onPageChanged(int index);

  void onToggleTextureSet(int checkedState);
  void onToggleVectorSet(int checkedState);
  void onToggleRasterSet(int checkedState);

  void onShowAllTextureSet();
  void onShowAllVectorSet();
  void onShowAllRasterSet();

  void onHideAllTextureSet();
  void onHideAllVectorSet();
  void onHideAllRasterSet();

  void onCollapseAllTextureSet();
  void onCollapseAllVectorSet();
  void onCollapseAllRasterSet();

  void onExpandAllTextureSet();
  void onExpandAllVectorSet();
  void onExpandAllRasterSet();

  void onUpdateFavorites();

  void onRemoveSelectedStylesFromFavorites();
  void onAddSelectedStylesToFavorites();
  void onAddSelectedStylesToPalette();
  void onCopySelectedStylesToSet();
  void onMoveSelectedStylesToSet();
  void onRemoveSelectedStyleFromSet();

  void onAddNewStyleSet();
  void onScanStyleSetChanges();
  void onSwitchToSettings();

private:
  QFrame *createBottomWidget();
  QFrame *createTexturePage();
  QFrame *createVectorPage();
  QFrame *createRasterPage();
  void updateTabBar();

  void copyEditedStyleToPalette(bool isDragging);
};

#endif  // STYLEEDITOR_H
