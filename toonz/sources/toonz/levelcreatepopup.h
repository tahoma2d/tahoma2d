

#ifndef LEVELCREATEPOPUP_H
#define LEVELCREATEPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/intfield.h"
#include "toonzqt/filefield.h"

// forward declaration
class QLabel;
class QComboBox;
//class DVGui::MeasuredDoubleLineEdit;

using namespace DVGui;

//=============================================================================
// LevelCreatePopup
//-----------------------------------------------------------------------------

class LevelCreatePopup : public Dialog
{
	Q_OBJECT

	LineEdit *m_nameFld;
	IntLineEdit *m_fromFld;
	IntLineEdit *m_toFld;
	QComboBox *m_levelTypeOm;
	IntLineEdit *m_stepFld;
	IntLineEdit *m_incFld;
	FileField *m_pathFld;
	QLabel *m_widthLabel;
	QLabel *m_heightLabel;
	QLabel *m_dpiLabel;
	DVGui::MeasuredDoubleLineEdit *m_widthFld;
	DVGui::MeasuredDoubleLineEdit *m_heightFld;
	DoubleLineEdit *m_dpiFld;

public:
	LevelCreatePopup();

	void setSizeWidgetEnable(bool isEnable);
	int getLevelType() const;

	void update();
	bool apply();

protected:
	// set m_pathFld to the default path
	void updatePath();

	void showEvent(QShowEvent *);

public slots:
	void onLevelTypeChanged(const QString &text);
	void onOkBtn();

	void onApplyButton();
};

#endif // LEVELCREATEPOPUP_H
