

// Tnz6 includes
#include "menubarcommandids.h"
#include "tapp.h"
#include "sceneviewer.h"
#include "stopmotion.h"
#include "cellselection.h"

// TnzQt includes
#include "toonzqt/menubarcommand.h"
#include "toonzqt/flipconsole.h"
#include "toonzqt/tselectionhandle.h"

// TnzLib includes
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txsheethandle.h"
#include "toonz/tframehandle.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/preferences.h"

#include <QApplication>

//**********************************************************************************
//    Commands  definition
//**********************************************************************************

class LinkToggleCommand final : public MenuItemHandler {
public:
  LinkToggleCommand() : MenuItemHandler("MI_Link") {}
  void execute() override { FlipConsole::toggleLinked(); }
} linkToggleCommand;

//-----------------------------------------------------------------------------

class VcrCommand final : public MenuItemHandler {
  FlipConsole::EGadget m_buttonId;

public:
  VcrCommand(const char *cmdId, FlipConsole::EGadget buttonId)
      : MenuItemHandler(cmdId), m_buttonId(buttonId) {}
  void execute() override {
    FlipConsole *console = FlipConsole::getCurrent();
    if (console) {
      console->pressButton(m_buttonId);

      if (m_buttonId != FlipConsole::eFirst &&
          m_buttonId != FlipConsole::eLast &&
          m_buttonId != FlipConsole::eNext && m_buttonId != FlipConsole::ePrev)
        return;

      int row = TApp::instance()->getCurrentFrame()->getFrame();
      TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
          TApp::instance()->getCurrentSelection()->getSelection());
      if (cellSelection) {
        int r0, r1, c0, c1;
        cellSelection->getSelectedCells(r0, c0, r1, c1);
        cellSelection->selectCells(row, c0, r1 + (row - r0), c1);
      }
    }
  }
};

//-----------------------------------------------------------------------------

class NextDrawingCommand final : public MenuItemHandler {
public:
  NextDrawingCommand() : MenuItemHandler(MI_NextDrawing) {}

  void execute() override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    int row = TApp::instance()->getCurrentFrame()->getFrame();
    int col = TApp::instance()->getCurrentColumn()->getColumnIndex();

    const TXshCell &cell = xsh->getCell(row, col);

    int frameCount = xsh->getFrameCount();
    while (row < frameCount) {
      row++;
      if (xsh->getCell(row, col).isEmpty()) continue;
      if (xsh->getCell(row, col) != cell) {
        TApp::instance()->getCurrentFrame()->setFrame(row);

        TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
            TApp::instance()->getCurrentSelection()->getSelection());
        if (cellSelection) {
          int r0, r1, c0, c1;
          cellSelection->getSelectedCells(r0, c0, r1, c1);
          cellSelection->selectCells(row, c0, r1 + (row - r0), c1);
        }
        break;
      }
    }
  }
};

//-----------------------------------------------------------------------------

class PrevDrawingCommand final : public MenuItemHandler {
public:
  PrevDrawingCommand() : MenuItemHandler(MI_PrevDrawing) {}

  void execute() override {
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();

    int row = TApp::instance()->getCurrentFrame()->getFrame();
    int col = TApp::instance()->getCurrentColumn()->getColumnIndex();

    TXshCell cell = xsh->getCell(row, col);

    // Get *last* cell in previous uniform cell block
    while (row >= 0) {
      row--;
      if (xsh->getCell(row, col).isEmpty()) continue;
      if (xsh->getCell(row, col) != cell) {
        cell = xsh->getCell(row, col);
        break;
      }
    }

    if (row >= 0) {
      cell = xsh->getCell(row, col);
      // Get *first* cell in current uniform cell block
      while (row > 0 && xsh->getCell(row - 1, col) == cell) --row;
      TApp::instance()->getCurrentFrame()->setFrame(row);

      TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
          TApp::instance()->getCurrentSelection()->getSelection());
      if (cellSelection) {
        int r0, r1, c0, c1;
        cellSelection->getSelectedCells(r0, c0, r1, c1);
        cellSelection->selectCells(row, c0, r1 + (row - r0), c1);
      }
    }
  }
};

//-----------------------------------------------------------------------------

class NextStepCommand final : public MenuItemHandler {
public:
  NextStepCommand() : MenuItemHandler(MI_NextStep) {}

  void execute() override {
    int row  = TApp::instance()->getCurrentFrame()->getFrame();
    int step = Preferences::instance()->getXsheetStep();

    TApp::instance()->getCurrentFrame()->setFrame(row + step);

    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    if (cellSelection) {
      int r0, r1, c0, c1;
      cellSelection->getSelectedCells(r0, c0, r1, c1);
      cellSelection->selectCells(r0 + step, c0, r1 + step, c1);
    }
  }
};

//-----------------------------------------------------------------------------

class PrevStepCommand final : public MenuItemHandler {
public:
  PrevStepCommand() : MenuItemHandler(MI_PrevStep) {}

  void execute() override {
    int row  = TApp::instance()->getCurrentFrame()->getFrame();
    int step = Preferences::instance()->getXsheetStep();

    TApp::instance()->getCurrentFrame()->setFrame(std::max(row - step, 0));

    TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
        TApp::instance()->getCurrentSelection()->getSelection());
    if (cellSelection) {
      int r0, r1, c0, c1;
      cellSelection->getSelectedCells(r0, c0, r1, c1);
      cellSelection->selectCells(r0 - step, c0, r1 - step, c1);
    }
  }
};

//-----------------------------------------------------------------------------

class ShortPlayCommand final : public MenuItemHandler {
public:
  ShortPlayCommand() : MenuItemHandler(MI_ShortPlay) {}

  void execute() override {
    int currentFrame        = TApp::instance()->getCurrentFrame()->getFrame();
    int shortPlayFrameCount = Preferences::instance()->getShortPlayFrameCount();
    int maxFrame =
        TApp::instance()->getCurrentXsheet()->getXsheet()->getFrameCount();

    int stopFrame = std::min(currentFrame, maxFrame);

    StopMotion *stopMotion = StopMotion::instance();
    if (stopMotion->getPlaceOnXSheet() && stopMotion->m_liveViewStatus > 0)
      stopFrame = StopMotion::instance()->getXSheetFrameNumber() - 1;

    int startFrame = std::max(0, stopFrame - shortPlayFrameCount);

    TApp::instance()->getCurrentFrame()->setFrame(startFrame);
    FlipConsole *console = FlipConsole::getCurrent();
    console->setStopAt(stopFrame + 1);
    CommandManager::instance()->execute(MI_Play);
  }
};

//-----------------------------------------------------------------------------

class NextKeyframeCommand final : public MenuItemHandler {
public:
  NextKeyframeCommand() : MenuItemHandler(MI_NextKeyframe) {}

  void execute() override {
    QString navControlList[6] = {"LevelPalette",   "StudioPalette",
                                 "FunctionEditor", "FxSettings",
                                 "ComboViewer",    "SceneViewer"};

    QWidget *panel    = QApplication::focusWidget();
    if (!panel) panel = TApp::instance()->getActiveViewer();
    while (panel) {
      QString pane = panel->objectName();
      if (panel->windowType() == Qt::WindowType::SubWindow ||
          panel->windowType() == Qt::WindowType::Tool) {
        if (std::find(navControlList, navControlList + 6, pane) !=
            (navControlList + 6)) {
          TApp::instance()->getCurrentFrame()->emitTriggerNextKeyframe(panel);

          int row = TApp::instance()->getCurrentFrame()->getFrame();
          TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
              TApp::instance()->getCurrentSelection()->getSelection());
          if (cellSelection) {
            int r0, r1, c0, c1;
            cellSelection->getSelectedCells(r0, c0, r1, c1);
            cellSelection->selectCells(row, c0, r1 + (row - r0), c1);
          }
          break;
        } else
          panel = TApp::instance()->getActiveViewer()->parentWidget();
      } else
        panel = panel->parentWidget();
    }
  }
};

//-----------------------------------------------------------------------------

class PrevKeyframeCommand final : public MenuItemHandler {
public:
  PrevKeyframeCommand() : MenuItemHandler(MI_PrevKeyframe) {}

  void execute() override {
    QString navControlList[6] = {"LevelPalette",   "StudioPalette",
                                 "FunctionEditor", "FxSettings",
                                 "ComboViewer",    "SceneViewer"};

    QWidget *panel    = QApplication::focusWidget();
    if (!panel) panel = TApp::instance()->getActiveViewer();
    while (panel) {
      QString pane = panel->objectName();
      if (panel->windowType() == Qt::WindowType::SubWindow ||
          panel->windowType() == Qt::WindowType::Tool) {
        if (std::find(navControlList, navControlList + 6, pane) !=
            (navControlList + 6)) {
          TApp::instance()->getCurrentFrame()->emitTriggerPrevKeyframe(panel);

          int row = TApp::instance()->getCurrentFrame()->getFrame();
          TCellSelection *cellSelection = dynamic_cast<TCellSelection *>(
              TApp::instance()->getCurrentSelection()->getSelection());
          if (cellSelection) {
            int r0, r1, c0, c1;
            cellSelection->getSelectedCells(r0, c0, r1, c1);
            cellSelection->selectCells(row, c0, r1 + (row - r0), c1);
          }
          break;
        } else
          panel = TApp::instance()->getActiveViewer()->parentWidget();
      } else
        panel = panel->parentWidget();
    }
  }
};

//**********************************************************************************
//    Commands  instantiation
//**********************************************************************************

VcrCommand playCommand(MI_Play, FlipConsole::ePlay),
    pauseCommand(MI_Pause, FlipConsole::ePause),
    loopCommand(MI_Loop, FlipConsole::eLoop),
    firstFrameCommand(MI_FirstFrame, FlipConsole::eFirst),
    lastFrameCommand(MI_LastFrame, FlipConsole::eLast),
    nextFrameCommand(MI_NextFrame, FlipConsole::eNext),
    prevFrameCommand(MI_PrevFrame, FlipConsole::ePrev),

    redChannelCommand(MI_RedChannel, FlipConsole::eRed),
    greenChannelCommand(MI_GreenChannel, FlipConsole::eGreen),
    blueChannelCommand(MI_BlueChannel, FlipConsole::eBlue),
    matteChannelCommand(MI_MatteChannel, FlipConsole::eMatte),

    redChannelGCommand(MI_RedChannelGreyscale, FlipConsole::eGRed),
    greenChannelGComman(MI_GreenChannelGreyscale, FlipConsole::eGGreen),
    blueChannelGCommand(MI_BlueChannelGreyscale, FlipConsole::eGBlue),

    compareCommand(MI_CompareToSnapshot, FlipConsole::eCompare),
    blankFramesCommand(MI_ToggleBlankFrames, FlipConsole::eBlankFrames);

NextDrawingCommand nextDrawingCommand;
PrevDrawingCommand prevDrawingCommand;
NextStepCommand nextStepCommand;
PrevStepCommand prevStepCommand;
ShortPlayCommand shortPlayCommand;

NextKeyframeCommand nextKeyframeCommand;
PrevKeyframeCommand prevKeyframeCommand;
