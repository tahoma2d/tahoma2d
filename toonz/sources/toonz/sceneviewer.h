#pragma once

#ifndef SCENEVIEWER_H
#define SCENEVIEWER_H

// TnzCore includes
#include "tgeometry.h"
#include "tgl.h"

// TnzLib includes
#include "toonz/imagepainter.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/flipconsole.h"

// TnzTools includes
#include "tools/tool.h"

// Toonz includes
#include "pane.h"
#include "previewer.h"

#include <array>
#include <QMatrix4x4>
#include <QTouchDevice>

//=====================================================================

//  Forward declarations

class Ruler;
class QMenu;
class SceneViewer;
class LocatorPopup;
class QGestureEvent;
class QTouchEvent;
class QOpenGLFramebufferObject;
class LutCalibrator;
class StopMotion;
class TStopWatch;
class QElapsedTimer;

namespace ImageUtils {
class FullScreenWidget;
}

//=====================================================================

class ToggleCommandHandler final : public MenuItemHandler {
  int m_status;

public:
  ToggleCommandHandler(CommandId id, bool startStatus);

  bool getStatus() const { return m_status; }
  // For reproducing the UI toggle when launch
  void setStatus(bool status) { m_status = status; }
  void execute() override;
};

//=============================================================================
// SceneViewer
//-----------------------------------------------------------------------------

class SceneViewer final : public TTool::Viewer,
                          public Previewer::Listener {
  Q_OBJECT

  Q_PROPERTY(QColor BGColor READ getBGColor WRITE setBGColor)
  QColor m_bgColor;

  Q_PROPERTY(
      QColor PreviewBGColor READ getPreviewBGColor WRITE setPreviewBGColor)
  QColor m_previewBgColor;

  double m_pressure;
  QPointF m_lastMousePos;
  QPointF m_pos;
  Qt::MouseButton m_mouseButton;
  bool m_foregroundDrawing;
  bool m_tabletEvent, m_tabletMove;
  enum TabletState {
    None = 0,
    Touched,      // Pressed for mouse
    StartStroke,  // this state is to detect the first call
                  // of TabletMove just after TabletPress
    OnStroke,
    Released
  } m_tabletState = None,
    m_mouseState  = None;
  // used to handle wrong mouse drag events!
  bool m_buttonClicked, m_toolSwitched;
  bool m_shownOnce                       = false;
  bool m_gestureActive                   = false;
  bool m_touchActive                     = false;
  QTouchDevice::DeviceType m_touchDevice = QTouchDevice::TouchScreen;
  bool m_rotating                        = false;
  bool m_zooming                         = false;
  bool m_panning                         = false;
  int m_touchPoints                      = 0;

  // These are for mouse based zoom, rotate, pan
  int m_mouseRotating   = 0;
  int m_mouseZooming    = 0;
  int m_mousePanning    = 0;
  bool m_resetOnRelease = false;
  // shared variables
  TStopWatch m_sw;
  TPointD m_oldPos;
  TPointD m_center;
  bool m_dragging;
  // zoom variables
  double m_oldY;
  double m_factor;
  // rotate variables
  double m_angle;
  TPointD m_oldMousePos;

  QPointF m_firstPanPoint;
  QPointF m_undoPoint;
  double m_scaleFactor;    // used for zoom gesture
  double m_rotationDelta;  // used for rotate gesture
  int m_referenceMode;
  int m_previewMode;
  bool m_isMouseEntered, m_forceGlFlush;
  bool m_isFlippedX = false, m_isFlippedY = false;
  /*!  FreezedStatus:
   *  \li NO_FREEZED freezed is not active;
   *  \li NORMAL_FREEZED freezed is active: show grab image;
   *  \li UPDATE_FREEZED freezed is active: draw last unfreezed image and grab
   * view;
   */
  enum FreezedStatus {
    NO_FREEZED     = 0,
    NORMAL_FREEZED = 1,
    UPDATE_FREEZED = 2,
  } m_freezedStatus;
  TRaster32P m_viewGrabImage;

  int m_FPS;

  ImagePainter::CompareSettings m_compareSettings;
  Ruler *m_hRuler;
  Ruler *m_vRuler;

  TPointD m_pan3D;
  double m_zoomScale3D;
  double m_phi3D;
  double m_theta3D;
  double m_minZ;

  // current pan/zoom matrix (two different matrices are used for editing scenes
  // and leves)
  std::array<TAffine, 2> m_viewAff;
  int m_viewMode;

  TPointD m_dpiScale;

  int m_tableDLId;  // To compute table DisplayList only if necessary.

  int m_groupIndexToBeEntered;

  double m_pixelSize;
  bool m_eraserPointerOn;
  QString m_backupTool;
  TRectD m_clipRect;

  bool m_isPicking;
  bool m_canShowPerspectiveGrids = false;
  bool m_showViewerIndicators    = true;

  TRaster32P m_3DSideL;
  TRaster32P m_3DSideR;
  TRaster32P m_3DTop;
  TRasterImageP m_stopMotionImage, m_stopMotionLineUpImage;
  StopMotion *m_stopMotion        = NULL;
  bool m_hasStopMotionImage       = false;
  bool m_hasStopMotionLineUpImage = false;

  TPointD m_sideRasterPos;
  TPointD m_topRasterPos;
  QString m_toolDisableReason;

  bool m_editPreviewSubCamera;

  // used for color calibration with 3DLUT
  QOpenGLFramebufferObject *m_fbo = NULL;
  LutCalibrator *m_lutCalibrator  = NULL;

  enum Device3D {
    NONE,
    SIDE_LEFT_3D,
    SIDE_RIGHT_3D,
    TOP_3D,
  } m_current3DDevice;

  bool m_isLocator;
  bool m_isStyleShortcutSwitchable;

  bool m_isBusyOnTabletMove;

  QMatrix4x4 m_projectionMatrix;

  // Used for texture management.
  // Changing dock / float state of the panel will alter the context.
  // So discarding the resources in old context in initializeGL.
  TGlContext m_currentContext;

  // used for updating viewer where the animated guide appears
  // updated in drawScene() and used in GLInvalidateRect()
  TRectD m_guidedDrawingBBox;

  double m_rotationAngle[2];

  bool m_firstInitialized = true;

  QAction *m_keyAction;

public:
  enum ReferenceMode {
    NORMAL_REFERENCE   = 1,
    CAMERA3D_REFERENCE = 2,
    CAMERA_REFERENCE   = 3,
    LEVEL_MODE         = 128,
  };

  // Zoom/Pan matrices are selected by ViewMode
  enum ViewMode { SCENE_VIEWMODE = 0, LEVEL_VIEWMODE = 1 };

  enum PreviewMode { NO_PREVIEW = 0, FULL_PREVIEW = 1, SUBCAMERA_PREVIEW = 2 };

  SceneViewer(ImageUtils::FullScreenWidget *parent);
  ~SceneViewer();

  double getPixelSize() const override { return m_pixelSize; }

  // Previewer::Listener
  TRectD getPreviewRect() const override;
  void onRenderStarted(int frame) override;
  void onRenderCompleted(int frame) override;
  void onPreviewUpdate() override;

  bool isPreviewEnabled() const { return m_previewMode != NO_PREVIEW; }
  int getPreviewMode() const { return m_previewMode; }

  void setVisual(const ImagePainter::VisualSettings &settings);

  TRect getActualClipRect(const TAffine &aff);

  //! Return the view matrix.
  //! The view matrix is a matrix contained in \b m_viewAff; if the SceneViewer
  //! in showing images
  //! in Camera view Mode (m_referenceMode = CAMERA_REFERENCE) the returned
  //! affine is composed with camera
  //! transformation.
  TAffine getViewMatrix() const override;
  //! Return the view matrix.
  //! The view matrix is a matrix contained in \b m_viewAff
  TAffine getSceneMatrix() const;

  void setViewMatrix(const TAffine &aff, int viewMode);

  int getFPS() { return m_FPS; }

  void setRulers(Ruler *v, Ruler *h) {
    m_vRuler = v;
    m_hRuler = h;
  }

  bool is3DView() const override;
  TDimension getViewportSize() const { return TDimension(width(), height()); }

  void invalidateAll() override;
  void GLInvalidateAll() override;
  void GLInvalidateRect(const TRectD &rect) override;
  void invalidateToolStatus() override;

  TPointD getPan3D() const { return m_pan3D; }
  double getZoomScale3D() const { return m_zoomScale3D; }

  double projectToZ(const TPointD &delta) override;

  TPointD getDpiScale() const override { return m_dpiScale; }
  void zoomQt(bool forward, bool reset);
  TAffine getNormalZoomScale();

  bool canSwapCompared() const;

  bool isEditPreviewSubcamera() const { return m_editPreviewSubCamera; }
  bool getIsFlippedX() const override { return m_isFlippedX; }
  bool getIsFlippedY() const override { return m_isFlippedY; }
  void setEditPreviewSubcamera(bool enabled) {
    m_editPreviewSubCamera = enabled;
  }

  // panning by dragging the navigator in the levelstrip
  void navigatorPan(const QPoint &delta);
  // a factor for getting pixel-based zoom ratio
  double getDpiFactor();
  // when showing the viewer with full-screen mode,
  // add a zoom factor which can show image fitting with the screen size
  double getZoomScaleFittingWithScreen();
  // return the viewer geometry in order to avoid picking the style outside of
  // the viewer
  // when using the stylepicker and the finger tools
  TRectD getGeometry() const override;
  TRectD getCameraRect() const override;

  void setFocus(Qt::FocusReason reason) { QWidget::setFocus(reason); };

  void setIsLocator() { m_isLocator = true; }
  void setIsStyleShortcutSwitchable() { m_isStyleShortcutSwitchable = true; }
  int getVGuideCount() override;
  int getHGuideCount() override;
  double getVGuide(int index) override;
  double getHGuide(int index) override;

  void bindFBO() override;
  void releaseFBO() override;

  void resetNavigation();

  void setBGColor(const QColor &color) { m_bgColor = color; }
  QColor getBGColor() const { return m_bgColor; }
  void setPreviewBGColor(const QColor &color) { m_previewBgColor = color; }
  QColor getPreviewBGColor() const { return m_previewBgColor; }

public:
  // SceneViewer's gadget public functions
  TPointD winToWorld(const QPointF &pos) const;
  TPointD winToWorld(const TPointD &winPos) const override;

  TPointD worldToPos(const TPointD &worldPos) const override;

protected:
  // Paint vars
  TAffine m_drawCameraAff;
  TAffine m_drawTableAff;
  bool m_draw3DMode;
  bool m_drawCameraTest;
  bool m_drawIsTableVisible;
  bool m_drawEditingLevel;
  TRect m_actualClipRect;

  // Paint methods
  void drawBuildVars();
  void drawEnableScissor();
  void drawDisableScissor();
  void drawBackground();
  void drawCameraStand();
  void drawPreview();
  void drawOverlay();
  void drawViewerIndicators();

  void drawScene();
  void drawSceneOverlay();
  void drawToolGadgets();

protected:
  void mult3DMatrix();

  void initializeGL() override;
  void resizeGL(int width, int height) override;

  void paintGL() override;

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

  void gestureEvent(QGestureEvent *e);
  void touchEvent(QTouchEvent *e, int type);
  void tabletEvent(QTabletEvent *) override;
  void leaveEvent(QEvent *) override;
  void enterEvent(QEvent *) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

  void onPress(const TMouseEvent &event);
  void onMove(const TMouseEvent &event);
  void onRelease(const TMouseEvent &event);
  void onContextMenu(const QPoint &pos, const QPoint &globalPos);
  void onEnter();
  void onLeave();

  void wheelEvent(QWheelEvent *) override;
  void keyPressEvent(QKeyEvent *event) override;
  void keyReleaseEvent(QKeyEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void inputMethodEvent(QInputMethodEvent *) override;
  void drawCompared();
  bool event(QEvent *event) override;

  // delta.x: right panning, pixel; delta.y: down panning, pixel
  void panQt(const QPointF &delta);

  // center: window coordinate, pixels, topleft origin
  void zoomQt(const QPoint &center, double scaleFactor);

  void mousePan(const TMouseEvent &e);
  void mouseZoom(const TMouseEvent &e);
  void mouseRotate(const TMouseEvent &e);

  // overridden from TTool::Viewer
  void pan(const TPointD &delta) override { panQt(QPointF(delta.x, delta.y)); }

  // overridden from TTool::Viewer
  void zoom(const TPointD &center, double factor) override;
  void rotate(const TPointD &center, double angle) override;
  void rotate3D(double dPhi, double dTheta) override;

  TAffine getToolMatrix() const;

  //! return the column index of the drawing intersecting point \b p
  //! (window coordinate, pixels, bottom-left origin)
  int pick(const TPointD &point) override;

  //! return the column indexes of the drawings intersecting point \b p
  //! (window coordinate, pixels, bottom-left origin)
  int posToColumnIndex(const TPointD &p, double distance,
                       bool includeInvisible = true) const override;
  void posToColumnIndexes(const TPointD &p, std::vector<int> &indexes,
                          double distance,
                          bool includeInvisible = true) const override;

  //! return the row of the drawings intersecting point \b p (used with onion
  //! skins)
  //! (window coordinate, pixels, bottom-left origin)
  int posToRow(const TPointD &p, double distance, bool includeInvisible = true,
               bool currentColumnOnly = false) const override;

  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

  void resetInputMethod() override;

  void set3DLeftSideView();
  void set3DRightSideView();
  void set3DTopView();

  void setFocus() override { QWidget::setFocus(); };

  void registerContext();

private:
  void doQuit();

public slots:

  void resetSceneViewer();
  void resetZoom();
  void resetRotation();
  void resetPosition();
  void setActualPixelSize();
  void flipX();
  void flipY();
  void zoomIn();
  void zoomOut();
  void onXsheetChanged();
  void onObjectSwitched();
  // when tool options are changed, update tooltip immediately
  void onToolChanged();
  void onToolSwitched();
  void onSceneChanged();
  void onLevelChanged();
  // when level is switched, update m_dpiScale in order to show white background
  // for Ink&Paint work properly
  void onLevelSwitched();
  void onFrameSwitched();
  void onOnionSkinMaskChanged() { GLInvalidateAll(); }

  void setReferenceMode(int referenceMode);
  void enablePreview(int previewMode);
  void freeze(bool on);

  void onButtonPressed(FlipConsole::EGadget button);
  void fitToCamera();
  void fitToCameraOutline();
  void swapCompared();
  void regeneratePreviewFrame();
  void regeneratePreview();

  // delete preview-subcamera executed from context menu
  void doDeleteSubCamera();

  void resetTabletStatus();

  void releaseBusyOnTabletMove() { m_isBusyOnTabletMove = false; }

  void onContextAboutToBeDestroyed();
  void onNewStopMotionImageReady();
  void onStopMotionLiveViewStopped();
  void onPreferenceChanged(const QString &prefName);

signals:

  void onZoomChanged();
  void onFlipHChanged(bool);
  void onFlipVChanged(bool);
  void freezeStateChanged(bool);
  void previewStatusChanged();
  // when pan/zoom on the viewer, notify to level strip in order to update the
  // navigator
  void refreshNavi();
  // for updating the titlebar
  void previewToggled();
  void viewerDestructing();
};

// Functions

void invalidateIcons();

#endif  // SCENEVIEWER_H
