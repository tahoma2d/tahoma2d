

#ifndef SAVEPRESETPOPUP_H
#define SAVEPRESETPOPUP_H

#include "toonzqt/dvdialog.h"
#include "toonzqt/lineedit.h"

using namespace DVGui;

//=============================================================================
// SavePresetPopup
//-----------------------------------------------------------------------------

class SavePresetPopup : public Dialog
{
	Q_OBJECT

	LineEdit *m_nameFld;

public:
	SavePresetPopup();
	~SavePresetPopup();
	bool apply();

protected slots:
	void onOkBtn();
};

#endif // SAVEPRESETPOPUP_H
