#pragma once

#ifndef MENUBAR_H
#define MENUBAR_H

#include <QTabBar>
#include <QToolBar>
#include <QMenuBar>
#include <QFrame>
#include <map>

#include "toonzqt/lineedit.h"

#include <QUrl>
#include <QStackedWidget>
#include <QContextMenuEvent>

// forward declaration
class QMenuBar;
class QAction;
class QMenu;
class TFilePath;
class QPainterEvent;
class QHBoxLayout;
class SubXsheetRoomTabContainer;
class QCheckBox;
class QXmlStreamReader;
class QLabel;

//-----------------------------------------------------------------------------

class RoomTabWidget final : public QTabBar {
  Q_OBJECT

  int m_clickedTabIndex;
  int m_tabToDeleteIndex;
  int m_renameTabIndex;
  DVGui::LineEdit *m_renameTextField;
  bool m_isLocked;

public:
  RoomTabWidget(QWidget *parent);
  ~RoomTabWidget();

  bool isLocked() { return m_isLocked; }

protected:
  void swapIndex(int firstIndex, int secondIndex);

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

protected slots:
  void updateTabName();
  void addNewTab();
  void deleteTab();
  void setIsLocked(bool lock);

signals:
  void indexSwapped(int firstIndex, int secondIndex);
  void insertNewTabRoom();
  void deleteTabRoom(int index);
  void renameTabRoom(int index, const QString name);
  void customizeMenuBar(int index);
};

//-----------------------------------------------------------------------------
/*
class SubSheetBar final : public QFrame
{
  Q_OBJECT

  QColor m_parentBgColor;

  Q_PROPERTY(QColor ParentBGColor READ getParentBGColor WRITE setParentBGColor)

  void setParentBGColor(const QColor& color) {m_parentBgColor = color;}
  QColor getParentBGColor() const { return m_parentBgColor; }

  bool m_mouseOverArrow;
  QPoint m_arrowPosition;
  QSize m_arrowSize;

  // Offset used to draw the subsheet in the bottom of the widget
  int m_pixmapYOffset;
  int m_pixmapXOffset;

public:
  SubSheetBar(QWidget *parent);
  ~SubSheetBar();

protected:

  void paintArrow(QPainter &p, int sceneLevel);
        int getDelta(const QPoint &pos);
  void paintEvent(QPaintEvent *);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

};

//-----------------------------------------------------------------------------

#ifdef MACOSX
class MenuBarWhiteLine final : public QFrame
{
  Q_OBJECT
public:
  MenuBarWhiteLine(QWidget *parent = 0);
protected:
  void paintEvent(QPaintEvent *event);
};
#else
class MenuBarWhiteLine final : public QMenuBar
{
public:
  MenuBarWhiteLine(QWidget *parent = 0);
protected:
  void paintEvent(QPaintEvent *event);
};
#endif
*/
//-----------------------------------------------------------------------------

class UrlOpener final : public QObject {
  Q_OBJECT
  QUrl m_url;

public:
  UrlOpener(const QUrl &url) : m_url(url) {}

public slots:
  void open();
};

//-----------------------------------------------------------------------------

class TopBar final : public QToolBar {
  Q_OBJECT

  QFrame *m_containerFrame;
  RoomTabWidget *m_roomTabBar;
  QMenuBar *m_menuBar;
  QCheckBox *m_lockRoomCB;
  QLabel *m_messageLabel;

public:
  TopBar(QWidget *parent);
  ~TopBar(){};

  QTabBar *getRoomTabWidget() const { return m_roomTabBar; }

  QMenuBar *getMenuBar() const { return m_menuBar; }

  void loadMenubar();

private:
  QMenu *addMenu(const QString &, QMenuBar *);
  void addMenuItem(QMenu *, const char *);

public slots:
  void setMessage(QString message);

protected:
  /*--  右クリックで消えないようにする--*/
  void contextMenuEvent(QContextMenuEvent *event) override { event->accept(); }
};

#endif  // MENUBAR_H
