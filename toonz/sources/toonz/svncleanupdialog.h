

#ifndef SVN_CLEANUP_DIALOG_H
#define SVN_CLEANUP_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"

using namespace DVGui;

class QPushButton;
class QLabel;

//-----------------------------------------------------------------------------

class SVNCleanupDialog : public Dialog
{
	Q_OBJECT

	QPushButton *m_closeButton;
	QLabel *m_waitingLabel;
	QLabel *m_textLabel;

	VersionControlThread m_thread;

public:
	SVNCleanupDialog(QWidget *parent, const QString &workingDir);

protected slots:
	void onCleanupDone();
	void onError(const QString &);
};

using namespace DVGui;

#endif // SVN_CLEANUP_DIALOG_H
