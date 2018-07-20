#pragma once

#ifndef SCHEMATICVIEWER_H
#define SCHEMATICVIEWER_H

// TnzLib includes
#include "toonz/tstageobjectid.h"

// TnzBase includes
#include "tfx.h"

// Qt includes
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTouchDevice>

// STD includes
#include <set>

#undef DVAPI
#undef DVVAR
#ifdef TOONZQT_EXPORTS
#define DVAPI DV_EXPORT_API
#define DVVAR DV_EXPORT_VAR
#else
#define DVAPI DV_IMPORT_API
#define DVVAR DV_IMPORT_VAR
#endif

//====================================================

//    Forward declarations

class SchematicNode;
class SchematicPort;
class SchematicLink;
class ToonzScene;
class StageSchematicScene;
class FxSchematicScene;
class TXsheetHandle;
class TObjectHandle;
class TColumnHandle;
class TFxHandle;
class TSceneHandle;
class TFrameHandle;
class TFx;
class TLevel;
class TSelection;
class TApplication;
class QToolBar;
class QToolButton;
class QAction;
class QTouchEvent;
class QGestureEvent;

//====================================================
namespace {
enum CursorMode { Select, Zoom, Hand };
}

//==================================================================
//
// SchematicScene
//
//==================================================================

class DVAPI SchematicScene : public QGraphicsScene {
  Q_OBJECT

public:
  SchematicScene(QWidget *parent);
  ~SchematicScene();

  void clearAllItems();

  virtual QGraphicsItem *getCurrentNode() { return 0; }
  virtual void reorderScene() = 0;
  virtual void updateScene()  = 0;

protected:
  QList<SchematicLink *> m_highlightedLinks;
  enum GridDimension { eLarge, eSmall };

protected:
  //! Returns \b true if no nodes intersects \b rect.
  bool isAnEmptyZone(const QRectF &rect);
  //! Returns a vector containing all nodes which had their bounding rects
  //! conatined in \b node bounding
  //! rect enlarged of 10.
  QVector<SchematicNode *> getPlacedNode(SchematicNode *node);

  void showEvent(QShowEvent *se);
  void hideEvent(QHideEvent *se);

protected slots:

  virtual void onSelectionSwitched(TSelection *, TSelection *) {}
};

//==================================================================
//
// SchematicSceneViewer
//
//==================================================================

class DVAPI SchematicSceneViewer final : public QGraphicsView {
  Q_OBJECT

  bool m_tabletEvent, m_tabletMove;
  enum TabletState {
    None = 0,
    Touched,
    StartStroke,  // this state is to detect the first call
    // of TabletMove just after TabletPress
    OnStroke,
    Released
  } m_tabletState = None;

  bool m_touchActive = false;

  bool m_gestureActive                   = false;
  QTouchDevice::DeviceType m_touchDevice = QTouchDevice::TouchScreen;
  bool m_zooming                         = false;
  bool m_panning                         = false;
  double m_scaleFactor;  // used for zoom gesture

  CursorMode m_cursorMode;

public:
  SchematicSceneViewer(QWidget *parent);
  ~SchematicSceneViewer();

  void zoomQt(bool zoomin, bool resetZoom);
  void panQt(QPoint currWinPos);

  QPointF getOldScenePos() { return m_oldScenePos; }

  void setCursorMode(CursorMode mode);

protected:
  void mousePressEvent(QMouseEvent *me) override;
  void mouseMoveEvent(QMouseEvent *me) override;
  void mouseReleaseEvent(QMouseEvent *me) override;
  void keyPressEvent(QKeyEvent *ke) override;
  void wheelEvent(QWheelEvent *me) override;
  void showEvent(QShowEvent *se) override;
  void mouseDoubleClickEvent(QMouseEvent *event);

  void touchEvent(QTouchEvent *e, int type);
  void gestureEvent(QGestureEvent *e);

  bool event(QEvent *event) override;

protected slots:

  void fitScene();
  void centerOnCurrent();
  void reorderScene();
  void normalizeScene();

private:
  Qt::MouseButton m_buttonState;
  QPoint m_oldWinPos;
  QPointF m_oldScenePos;
  QPoint m_prevWinPos;
  bool m_firstShowing;

private:
  void changeScale(const QPoint &winPos, qreal scaleFactor);
};

//==================================================================
//
// SchematicViewer
//
//==================================================================

class DVAPI SchematicViewer final : public QWidget {
  Q_OBJECT

public:
  SchematicViewer(QWidget *parent);
  ~SchematicViewer();

  void setSchematicScene(SchematicScene *scene);
  void setApplication(TApplication *app);
  bool isStageSchematicViewed();
  void setStageSchematicViewed(bool isStageSchematic);

  void setCursorMode(CursorMode mode);

public slots:

  void updateSchematic();

signals:

  void showPreview(TFxP);
  void doCollapse(const QList<TFxP> &);
  void doCollapse(QList<TStageObjectId>);
  void doExplodeChild(const QList<TFxP> &);
  void doExplodeChild(QList<TStageObjectId>);
  void editObject();

protected slots:

  void onSceneChanged();
  void onSceneSwitched();
  void updateScenes();
  void changeNodeSize();

  void selectModeEnabled();
  void zoomModeEnabled();
  void handModeEnabled();

private:
  SchematicSceneViewer *m_viewer;
  StageSchematicScene *m_stageScene;
  FxSchematicScene *m_fxScene;

  TSceneHandle *m_sceneHandle;

  QToolBar *m_stageToolbar, *m_commonToolbar, *m_fxToolbar, *m_swapToolbar;

  QAction *m_fitSchematic, *m_centerOn, *m_reorder, *m_normalize, *m_nodeSize,
      *m_changeScene, *m_selectMode, *m_zoomMode, *m_handMode;

  bool m_fullSchematic, m_maximizedNode;

  SchematicViewer::CursorMode m_cursorMode;

private:
  void createToolbars();
  void createActions();

  void setStageSchematic();
  void setFxSchematic();
};

#endif  // SCHEMATICVIEWER_H
