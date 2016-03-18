

#ifndef BATCHSERVERSVIEWER_H
#define BATCHSERVERSVIEWER_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/lineedit.h"

#include <QFrame>
#include <QListWidget>

class QComboBox;
class FarmServerListView;
class QListWidgetItem;
using namespace DVGui;

//=============================================================================
// BatchServersViewer

class FarmServerListView : public QListWidget
{
	Q_OBJECT
public:
	FarmServerListView(QWidget *parent);

	~FarmServerListView(){};

	void update();

protected slots:

	void activate();
	void deactivate();

private:
	void openContextMenu(const QPoint &p);
	void mousePressEvent(QMouseEvent *event);
	QMenu *m_menu;
};

class BatchServersViewer : public QFrame
{
	Q_OBJECT

public:
#if QT_VERSION >= 0x050500
	BatchServersViewer(QWidget *parent = 0, Qt::WindowFlags flags = 0);
#else
	BatchServersViewer(QWidget *parent = 0, Qt::WFlags flags = 0);
#endif
	~BatchServersViewer();

	void updateSelected();

protected slots:
	void setGRoot();
	void onProcessWith(int index);
	void onCurrentItemChanged(QListWidgetItem *);

private:
	QString m_serverId;
	LineEdit *m_farmRootField;
	QComboBox *m_processWith;
	FarmServerListView *m_serverList;

	LineEdit *m_name;
	LineEdit *m_ip;
	LineEdit *m_port;
	LineEdit *m_tasks;
	LineEdit *m_state;
	LineEdit *m_cpu;
	LineEdit *m_mem;
	void updateServerInfo(const QString &id);
};

#endif //BATCHSERVERSVIEWER_H
