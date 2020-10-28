#pragma once

#ifndef MOTIONPATHPANEL_H
#define MOTIONPATHPANEL_H

#include "toonz/tstageobjectspline.h"

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QMouseEvent>
#include <QImage>

class TPanelTitleBarButton;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QPushButton;
class QFrame;
class QToolBar;

class ClickablePathLabel : public QLabel {
	Q_OBJECT

protected:
	void mouseReleaseEvent(QMouseEvent*) override;
	void enterEvent(QEvent*) override;
	void leaveEvent(QEvent*) override;

public:
	ClickablePathLabel(const QString& text, QWidget* parent = nullptr,
		Qt::WindowFlags f = Qt::WindowFlags());
	~ClickablePathLabel();

signals:
	void onMouseRelease(QMouseEvent* event);
};

class MotionPathControl : public QWidget {
	Q_OBJECT

	QImage m_timelinePreviewButtonOnImage;
	QImage m_timelinePreviewButtonOffImage;
	Q_PROPERTY(
		QImage TimelinePreviewButtonOnImage READ getTimelinePreviewButtonOnImage
		WRITE setTimelinePreviewButtonOnImage)
		Q_PROPERTY(
			QImage TimelinePreviewButtonOffImage READ getTimelinePreviewButtonOffImage
			WRITE setTimelinePreviewButtonOffImage)

		void setTimelinePreviewButtonOnImage(const QImage& image) {
		m_timelinePreviewButtonOnImage = image;
	}

	void setTimelinePreviewButtonOffImage(const QImage& image) {
		m_timelinePreviewButtonOffImage = image;
	}

	QImage getTimelinePreviewButtonOnImage() const {
		return m_timelinePreviewButtonOnImage;
	}

	QImage getTimelinePreviewButtonOffImage() const {
		return m_timelinePreviewButtonOffImage;
	}

	TStageObjectSpline* m_spline;
	bool m_active;
	QGridLayout* m_controlLayout;
	TPanelTitleBarButton* m_activeButton;
	ClickablePathLabel* m_nameLabel;

public:
	MotionPathControl(QWidget* parent) : QWidget(parent) {};
	~MotionPathControl() {};


	void createControl(TStageObjectSpline* spline);
};

//=============================================================================
// MotionPathPanel
//-----------------------------------------------------------------------------

class MotionPathPanel final : public QWidget {
	Q_OBJECT

    QHBoxLayout* m_toolLayout;
	QHBoxLayout* m_controlsLayout;
	QGridLayout* m_pathsLayout;
	QVBoxLayout* m_outsideLayout;
	QVBoxLayout* m_insideLayout;
	QFrame* m_mainControlsPage;
	QToolBar* m_toolbar;
	std::vector<MotionPathControl*> m_motionPathControls;


public:
  MotionPathPanel(QWidget *parent = 0);
  ~MotionPathPanel();

  protected:
	  void clearPathsLayout();


  protected slots:
	  void refreshPaths();

  // public slots:
};



#endif  // MOTIONPATHPANEL_H
