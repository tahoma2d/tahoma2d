#pragma once

#ifndef TAPP_H
#define TAPP_H

#include <QObject>

#include "tools/tool.h"

// forward declaration
class TSceneHandle;
class TXsheetHandle;
class TXshLevelHandle;
class TFrameHandle;
class TColumnHandle;
class ToolHandle;
class TObjectHandle;
class TSelectionHandle;
class TOnionSkinMaskHandle;
class TFxHandle;
class PaletteController;
class QTimer;
class TXshLevel;
class QMainWindow;
class StatusBar;

class TMainWindow;
class ComboViewerPanel;
class SceneViewer;
class XsheetViewer;
class LocatorPopup;

//=============================================================================
// TXsheeHandle
//-----------------------------------------------------------------------------
//! This is the instance of the main application.

/*! This class is a singleton and is used to initialize the main application
window.
                It defines the basic signals used in the application, as the
status changing of
                the scene and connects them to the basic handlers.
        \n	It is also used to retrieve a pointer to the current state of
the application,
                as the pointer to the main window, a pointer to the current
scene or a pointer
                to the current selection, etc...
        \n	For example to get the pointer to the main window in any part
of the code one has to write:

        \code
// A pointer to the main windows is

QMainWindow * mainwindow = TApp::instance()->getMainWindow();

// To get the current object id

TStageObjectId currentObjectId =
TApp::instance()->getCurrentObject()->getObjectId();

                \endcode
                This class is used to take care of changes on the state of the
application and to
                notify through event handling to the rest of the code.
        */
class TApp final : public QObject,
                   public TTool::Application  // Singleton
{
  Q_OBJECT

  TSceneHandle *m_currentScene;
  TXsheetHandle *m_currentXsheet;
  TFrameHandle *m_currentFrame;
  TColumnHandle *m_currentColumn;
  TXshLevelHandle *m_currentLevel;
  ToolHandle *m_currentTool;
  TObjectHandle *m_currentObject;
  TSelectionHandle *m_currentSelection;
  TOnionSkinMaskHandle *m_currentOnionSkinMask;
  TFxHandle *m_currentFx;

  PaletteController *m_paletteController;

  QMainWindow *m_mainWindow;

  SceneViewer *m_activeViewer;
  XsheetViewer *m_xsheetViewer;
  LocatorPopup *m_activeLocator;

  int m_autosavePeriod;  // minutes
  bool m_autosaveSuspended;
  bool m_saveInProgress;
  QTimer *m_autosaveTimer;
  StatusBar *m_statusBar;
  TApp();

  bool m_isStarting;
  bool m_isPenCloseToTablet;

  bool m_metaPressed;

public:
  /*!
          A static pointer to the main application.
  */
  static TApp *instance();

  ~TApp();
  /*!
          Returns a pointer to the current scene.
  */
  TSceneHandle *getCurrentScene() const override { return m_currentScene; }
  /*!
          Returns a pointer to the current Xsheet.
  */
  TXsheetHandle *getCurrentXsheet() const override { return m_currentXsheet; }
  /*!
          Returns a pointer to the current frame.
  */
  TFrameHandle *getCurrentFrame() const override { return m_currentFrame; }
  /*!
          Returns a pointer to the current column.
  */
  TColumnHandle *getCurrentColumn() const override { return m_currentColumn; }
  /*!
          Returns a pointer to the current level.
  */
  TXshLevelHandle *getCurrentLevel() const override { return m_currentLevel; }
  /*!
          Returns a pointer to the current tool used.
  */
  ToolHandle *getCurrentTool() const override { return m_currentTool; }
  /*!
          Returns a pointer to the current object in use.
  */
  TObjectHandle *getCurrentObject() const override { return m_currentObject; }
  /*!
          Returns a pointer to the current selection.
  */
  TSelectionHandle *getCurrentSelection() const override {
    return m_currentSelection;
  }
  /*!
          Returns a pointer to the current layer's mask.
  */
  TOnionSkinMaskHandle *getCurrentOnionSkin() const override {
    return m_currentOnionSkinMask;
  }
  /*!
          Returns a pointer to the current effect.
  */
  TFxHandle *getCurrentFx() const override { return m_currentFx; }

  PaletteController *getPaletteController() const override {
    return m_paletteController;
  }
  /*!
          Sets a pointer to the main window..
  */

  // Current Palette (PaletteController) methods

  TPaletteHandle *getCurrentPalette() const override;

  TColorStyle *getCurrentLevelStyle() const override;

  int getCurrentLevelStyleIndex() const override;

  void setCurrentLevelStyleIndex(int index, bool forceUpdate = false) override;

  void setMainWindow(QMainWindow *mainWindow) { m_mainWindow = mainWindow; }
  /*!
          Returns a pointer to the main window.
  */
  QMainWindow *getMainWindow() const { return m_mainWindow; }
  /*!
          Returns a pointer to the current room. The current room is the window
     environment in use,
          i.e. the drawing tab, the animation tab, the pltedit tab etc...
  */
  TMainWindow *getCurrentRoom() const;
  /*!
          Returns the current image type that can be a raster or a vector image.
     \sa TImage::Type.
  */
  int getCurrentImageType();
  /*!
          Initializes the main window application. It is called in the main
     function
          after the environment initialization.
  */
  void init();

  QString getCurrentRoomName() const;

  void setActiveViewer(SceneViewer *viewer) {
    if (m_activeViewer == viewer) return;
    m_activeViewer = viewer;
    emit activeViewerChanged();
  }

  SceneViewer *getActiveViewer() const { return m_activeViewer; }

  bool isApplicationStarting() { return m_isStarting; }

  bool isPenCloseToTablet() const { return m_isPenCloseToTablet; }

  void writeSettings();

  void setCurrentXsheetViewer(XsheetViewer *viewer) { m_xsheetViewer = viewer; }

  XsheetViewer *getCurrentXsheetViewer() const { return m_xsheetViewer; }

  void showMessage(QString message, int duration = 2000) override;
  void sendShowTitleBars(bool on, bool force = false);
  bool getShowTitleBars() { return m_showTitleBars; }
  void setShowTitleBars(bool on);
  bool getCanHideTitleBars() { return m_canHideTitleBars; }
  void setCanHideTitleBars(bool on);

  void setStatusBar(StatusBar *statusBar);
  void setStatusBarFrameInfo(QString text);

  void refreshStatusBar() override;

  bool isSaveInProgress() { return m_saveInProgress; }
  void setSaveInProgress(bool inProgress) { m_saveInProgress = inProgress; }

  void setActiveLocator(LocatorPopup *locator) {
    if (m_activeLocator == locator) return;
    m_activeLocator = locator;
    emit activeLocatorChanged();
  }

  LocatorPopup *getActiveLocator() const { return m_activeLocator; }

  bool isMetaPressed() { return m_metaPressed; }
  void setMetaPressed(bool pressed) { m_metaPressed = pressed; }

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;
  bool m_showTitleBars    = true;
  bool m_canHideTitleBars = false;

private:
  void updateXshLevel();
  void updateCurrentFrame();

protected slots:
  void onXsheetChanged();
  void onSceneSwitched();
  void onXsheetSwitched();
  void onXsheetSoundChanged();
  void onFrameSwitched();
  void onFxSwitched();
  void onColumnIndexSwitched();
  void onXshLevelSwitched(TXshLevel *);
  void onXshLevelChanged();
  void onObjectSwitched();
  void onSplineChanged();
  void onSceneChanged();

  void onImageChanged();

  void onPaletteChanged();
  void onLevelColorStyleChanged();
  void onLevelColorStyleSwitched();

  void autosave();
  void onToolEditingFinished();
  void onStartAutoSave();
  void onStopAutoSave();

signals:
  // on OSX, there is a critical bug that SceneViewer::mousePressEvent is called
  // when leaving the stylus and it causes unwanted stroke drawn while
  // hover-moving of the pen.
  // This signal is to detect tablet leave and force initializing such irregular
  // mouse press.
  // NOTE: For now QEvent::TabletLeaveProximity is NOT detected on Windows. See
  // QTBUG-53628.
  void tabletLeft();
  void showTitleBars(bool);

  void
  activeViewerChanged();  // TODO: put widgets-related stuffs in some new handle
  void activeLocatorChanged();
};

#endif  // TAPP_H
