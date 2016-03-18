

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

//-----------------------------------------------------------------------------
/*
class RoomTabWidget : public QTabBar
{
  Q_OBJECT

  int m_clickedTabIndex;
	int m_tabToDeleteIndex;
	int m_renameTabIndex;
	DVGui::LineEdit* m_renameTextField;

public:
  RoomTabWidget(QWidget *parent);
  ~RoomTabWidget();
	
	void drawContextMenu(QContextMenuEvent *event);

protected:
	void swapIndex(int firstIndex, int secondIndex);

  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);

protected slots:
	void updateTabName();
	void addNewTab();
	void deleteTab();

signals:
	void indexSwapped(int firstIndex, int secondIndex);
	void insertNewTabRoom();
	void deleteTabRoom(int index);
	void renameTabRoom(int index, const QString name);
};

//-----------------------------------------------------------------------------

class SubSheetBar : public QFrame
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
class MenuBarWhiteLine : public QFrame
{
  Q_OBJECT
public:
  MenuBarWhiteLine(QWidget *parent = 0);
protected:
  void paintEvent(QPaintEvent *event);
};
#else
class MenuBarWhiteLine : public QMenuBar
{
public:
  MenuBarWhiteLine(QWidget *parent = 0);
protected:
  void paintEvent(QPaintEvent *event);
};
#endif
*/
//-----------------------------------------------------------------------------

class UrlOpener : public QObject
{
	Q_OBJECT
	QUrl m_url;

public:
	UrlOpener(const QUrl &url) : m_url(url) {}

public slots:
	void open();
};

//-----------------------------------------------------------------------------
/*-- モジュールごとにMenubarの内容を切り替える --*/
class StackedMenuBar : public QStackedWidget
{
	Q_OBJECT

	void createCleanupMenuBar();
	void createPltEditMenuBar();
	void createInknPaintMenuBar();
	void createXsheetMenuBar();
	void createBatchesMenuBar();
	void createBrowserMenuBar();
	void createFullMenuBar();

public:
	StackedMenuBar(QWidget *parent);
	~StackedMenuBar(){};

	void createMenuBarByName(const QString &roomName);

	QMenu *addMenu(const QString &, QMenuBar *);
	void addMenuItem(QMenu *, const char *);
};

//-----------------------------------------------------------------------------

class TopBar : public QToolBar
{
	Q_OBJECT

	QFrame *m_containerFrame;
	QTabBar *m_roomTabBar;
	StackedMenuBar *m_stackedMenuBar;

public:
	TopBar(QWidget *parent);
	~TopBar(){};

	QTabBar *getRoomTabWidget() const
	{
		return m_roomTabBar;
	}

	StackedMenuBar *getStackedMenuBar() const
	{
		return m_stackedMenuBar;
	}

protected:
	/*--  右クリックで消えないようにする--*/
	void contextMenuEvent(QContextMenuEvent *event)
	{
		event->accept();
	}
};

#endif // MENUBAR_H
