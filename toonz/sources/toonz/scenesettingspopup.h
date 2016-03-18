

#ifndef SCENESETTINGSPOPUP_H
#define SCENESETTINGSPOPUP_H

#include "toonzqt/dvdialog.h"
#include "tpixel.h"
#include "toonzqt/intfield.h"
#include "toonzqt/doublefield.h"
#include "toonzqt/colorfield.h"

// forward declaration
class TSceneProperties;
class QComboBox;

using namespace DVGui;

//=============================================================================
// SceneSettingsPopup
//-----------------------------------------------------------------------------

class SceneSettingsPopup : public QDialog
{
	Q_OBJECT

	DoubleLineEdit *m_frameRateFld;
	ColorField *m_bgColorFld;

	IntLineEdit *m_fieldGuideFld;
	DoubleLineEdit *m_aspectRatioFld;

	IntLineEdit *m_fullcolorSubsamplingFld;
	IntLineEdit *m_tlvSubsamplingFld;

	IntLineEdit *m_markerIntervalFld;
	IntLineEdit *m_startFrameFld;

	TSceneProperties *getProperties() const;

public:
	SceneSettingsPopup();
	void configureNotify();

protected:
	void showEvent(QShowEvent *);
	void hideEvent(QHideEvent *);

public slots:

	void update();

	void onFrameRateEditingFinished();
	void onFieldGuideSizeEditingFinished();
	void onFieldGuideAspectRatioEditingFinished();

	void onFullColorSubsampEditingFinished();
	void onTlvSubsampEditingFinished();
	void onMakerIntervalEditingFinished();
	void onStartFrameEditingFinished();

	void setBgColor(const TPixel32 &value, bool isDragging);
};

#endif // SCENESETTINGSPOPUP_H
