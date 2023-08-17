#pragma once

#ifndef XSHNOTEVIEWER_H
#define XSHNOTEVIEWER_H

#include <memory>

#include "toonz/txsheet.h"
#include "toonzqt/dvdialog.h"
#include "toonzqt/dvtextedit.h"
#include "toonzqt/colorfield.h"

#include <QFrame>
#include <QScrollArea>

#include "layerheaderpanel.h"

//-----------------------------------------------------------------------------

// forward declaration
class XsheetViewer;
class QTextEdit;
class TColorStyle;
class QToolButton;
class QPushButton;
class QComboBox;
class Orientation;

//-----------------------------------------------------------------------------

namespace XsheetGUI {

//=============================================================================
// NotePopup
//-----------------------------------------------------------------------------

class NotePopup final : public DVGui::Dialog {
  Q_OBJECT
  XsheetViewer *m_viewer;
  int m_noteIndex;
  DVGui::DvTextEdit *m_textEditor;
  int m_currentColorIndex;
  QList<DVGui::ColorField *> m_colorFields;
  //! Used to avoid double click in discard or post button.
  bool m_isOneButtonPressed;

public:
  NotePopup(XsheetViewer *viewer, int noteIndex);
  ~NotePopup() {}

  void setCurrentViewer(XsheetViewer *viewer);
  void setCurrentNoteIndex(int index);

  void update();

protected:
  TXshNoteSet *getNotes();
  QList<TPixel32> getColors();

  void onColorFieldEditingChanged(const TPixel32 &color, bool isEditing,
                                  int index);

  DVGui::ColorField *createColorField(int index);
  void updateColorField();

  void showEvent(QShowEvent *) override;
  void hideEvent(QHideEvent *) override;

protected slots:
  void onColor1Switched(const TPixel32 &, bool isDragging);
  void onColor2Switched(const TPixel32 &, bool isDragging);
  void onColor3Switched(const TPixel32 &, bool isDragging);
  void onColor4Switched(const TPixel32 &, bool isDragging);
  void onColor5Switched(const TPixel32 &, bool isDragging);
  void onColor6Switched(const TPixel32 &, bool isDragging);
  void onColor7Switched(const TPixel32 &, bool isDragging);

  void onColorChanged(const TPixel32 &, bool isDragging);
  void onNoteAdded();
  void onNoteDiscarded();
  void onTextEditFocusIn();

  void onXsheetSwitched();
};

static NotePopup *NotePopupWidget;

//=============================================================================
// NoteWidget
//-----------------------------------------------------------------------------

class NoteWidget final : public QWidget {
  Q_OBJECT
  XsheetViewer *m_viewer;
  int m_noteIndex;
  bool m_isHovered;

public:
  NoteWidget(XsheetViewer *parent = 0, int noteIndex = -1);

  int getNoteIndex() const { return m_noteIndex; }
  void setNoteIndex(int index) {
    m_noteIndex = index;
    if (NotePopupWidget) NotePopupWidget->setCurrentNoteIndex(index);
  }

  void paint(QPainter *painter, QPoint pos = QPoint(), bool isCurrent = false);

  void openNotePopup();

protected:
  void paintEvent(QPaintEvent *event) override;
};

//=============================================================================
// NoteArea
//-----------------------------------------------------------------------------

class NoteArea final : public QFrame {
  Q_OBJECT

  XsheetViewer *m_viewer;

  QPushButton *m_flipOrientationButton;

  QToolButton *m_noteButton;
  QToolButton *m_nextNoteButton;
  QToolButton *m_precNoteButton;
  QToolButton *m_newLevelButton;

  QComboBox *m_frameDisplayStyleCombo;

  LayerHeaderPanel *m_layerHeaderPanel;

  QPushButton *m_hamburgerButton;
  QWidget *m_popup;
  QLayout *m_currentLayout;

public:
  NoteArea(XsheetViewer *parent = 0, Qt::WindowFlags flags = Qt::WindowFlags());

  void updateButtons();

protected slots:
  void flipOrientation();
  void toggleNewNote();
  void nextNote();
  void precNote();

  void onFrameDisplayStyleChanged(int id);
  void onXsheetOrientationChanged(const Orientation *orientation);

  void onClickHamburger();

protected:
  void removeLayout();
  void createLayout();
};

//=============================================================================
// NoteArea
//-----------------------------------------------------------------------------

class FooterNoteArea final : public QFrame {
  Q_OBJECT

  XsheetViewer *m_viewer;

  QToolButton *m_noteButton;
  QToolButton *m_nextNoteButton;
  QToolButton *m_precNoteButton;

public:
  FooterNoteArea(QWidget *parent = 0, XsheetViewer *viewer = 0,
                 Qt::WindowFlags flags = Qt::WindowFlags());

  void updateButtons();

protected slots:
  void toggleNewNote();
  void nextNote();
  void precNote();

  void onFrameDisplayStyleChanged(int id);
  void onXsheetOrientationChanged(const Orientation *orientation);

protected:
  void removeLayout();
  void createLayout();
};

}  // namespace XsheetGUI

#endif  // XSHNOTEVIEWER_H
