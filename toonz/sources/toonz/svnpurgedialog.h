

#ifndef SVN_PURGE_DIALOG_H
#define SVN_PURGE_DIALOG_H

#include "toonzqt/dvdialog.h"
#include "versioncontrol.h"
#include <QList>

using namespace DVGui;

class QPushButton;
class QLabel;

//-----------------------------------------------------------------------------

class SVNPurgeDialog : public Dialog
{
	Q_OBJECT

	QString m_workingDir;

	QPushButton *m_purgeButton;
	QPushButton *m_cancelButton;
	QLabel *m_waitingLabel;
	QLabel *m_textLabel;

	QList<SVNStatus> m_status;

	VersionControlThread m_thread;

	QStringList m_filesToPurge;

public:
	SVNPurgeDialog(QWidget *parent, const QString &workingDir);

private:
	void switchToCloseButton();
	void checkFiles();
	void updateFileBrowser();

protected slots:

	void onPurgeClicked();
	void onStatusRetrieved(const QString &);
	void onError(const QString &);

signals:
	void done(const QStringList &);
};

using namespace DVGui;

#endif // SVN_PURGE_DIALOG_H
