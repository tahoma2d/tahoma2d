#include "motionpathpanel.h"
#include "tapp.h"
#include "toonz/txsheethandle.h"
#include "toonz/txsheet.h"
#include "toonz/tobjecthandle.h"
#include "toonz/tstageobjecttree.h"
#include "toonzqt/menubarcommand.h"
#include "toonz/tscenehandle.h"
#include "toonzqt/gutil.h"
#include "pane.h"

#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QToolBar>
#include <QPixmap>


void MotionPathControl::createControl(TStageObjectSpline* spline) {
	getIconThemePath("actions/20/pane_preview.svg");
		m_spline = spline;
		m_active = spline->getActive();
		m_nameLabel = new ClickablePathLabel(QString::fromStdString(spline->getName()), this);
		m_activeButton = new TPanelTitleBarButton(
		this, getIconThemePath("actions/20/pane_preview.svg"));
		m_controlLayout = new QGridLayout(this);
		m_controlLayout->setMargin(1);
		m_controlLayout->setSpacing(3);
		m_controlLayout->addWidget(m_activeButton, 0, 0, Qt::AlignLeft);
		m_controlLayout->addWidget(m_nameLabel, 0, 1, Qt::AlignLeft);
		m_controlLayout->setColumnStretch(0, 0);
		m_controlLayout->setColumnStretch(1, 500);
		setLayout(m_controlLayout);
		connect(m_nameLabel, &ClickablePathLabel::onMouseRelease, [=]() {
			TApp* app = TApp::instance();
			TStageObjectTree* pegTree = app->getCurrentXsheet()->getXsheet()->getStageObjectTree();
			TStageObject* viewer = pegTree->getMotionPathViewer();
			viewer->setSpline(spline);
			app->getCurrentObject()->setObjectId(pegTree->getMotionPathViewerId());
			app->getCurrentObject()->setIsSpline(true, true);
		});
		m_activeButton->setPressed(m_spline->getActive());
		connect(m_activeButton, &TPanelTitleBarButton::toggled, [=](bool pressed) {
			m_spline->setActive(pressed);
			TApp::instance()->getCurrentScene()->notifySceneChanged();
		});
}

//*****************************************************************************
//    MotionPathPanel  implementation
//*****************************************************************************

MotionPathPanel::MotionPathPanel(QWidget *parent) : QWidget(parent) {
	m_outsideLayout = new QVBoxLayout(this);
	m_outsideLayout->setMargin(0);
	m_outsideLayout->setSpacing(0);
	m_insideLayout = new QVBoxLayout(this);
	m_insideLayout->setMargin(0);
	m_insideLayout->setSpacing(0);
	m_pathsLayout = new QGridLayout(this);
	m_pathsLayout->setMargin(0);
	m_pathsLayout->setSpacing(0);
	m_mainControlsPage = new QFrame(this);
	m_mainControlsPage->setLayout(m_insideLayout);

	QScrollArea* scrollArea = new QScrollArea();
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setWidgetResizable(true);
	scrollArea->setWidget(m_mainControlsPage);
	m_outsideLayout->addWidget(scrollArea);


	// make the toolbar
	m_toolbar = new QToolBar(this);
	m_toolbar->addAction(CommandManager::instance()->getAction("T_Brush"));
	m_toolbar->addAction(CommandManager::instance()->getAction("T_Geometric"));
	m_toolbar->addAction(CommandManager::instance()->getAction("T_ControlPointEditor"));
	m_toolLayout = new QHBoxLayout(this);
	m_toolLayout->addWidget(m_toolbar);

	m_controlsLayout = new QHBoxLayout(this);
	m_controlsLayout->addWidget(new QLabel("teh bottom"));
	
	m_insideLayout->addLayout(m_toolLayout);
	m_insideLayout->addLayout(m_pathsLayout);
	m_insideLayout->addStretch();
	m_insideLayout->addLayout(m_controlsLayout);
	setLayout(m_outsideLayout);
	TXsheetHandle* xsh = TApp::instance()->getCurrentXsheet();
	connect(xsh, &TXsheetHandle::xsheetChanged, [=]() { refreshPaths(); });
}

//-----------------------------------------------------------------------------

MotionPathPanel::~MotionPathPanel() {}

//-----------------------------------------------------------------------------

void MotionPathPanel::refreshPaths() {
	TXsheetHandle* xsh = TApp::instance()->getCurrentXsheet();
	TStageObjectTree *tree = xsh->getXsheet()->getStageObjectTree();
	if (tree->getSplineCount() == m_motionPathControls.size()) return;

	m_motionPathControls.clear();
	QLayoutItem* child;
	while (m_pathsLayout->count() != 0) {
		child = m_pathsLayout->takeAt(0);
		if (child->widget() != 0) {
			delete child->widget();
		}
		delete child;
	}

	int i = 0;
	for (; i < tree->getSplineCount(); i++) {
		MotionPathControl* control = new MotionPathControl(this);
		control->createControl(tree->getSpline(i));
		m_pathsLayout->addWidget(control, i, 0);
		m_pathsLayout->setRowStretch(i, 0);
		m_motionPathControls.push_back(control);

	}
	m_pathsLayout->addWidget(new QLabel(" ", this), ++i, 0);
	m_pathsLayout->setRowStretch(i, 500);
}

//=============================================================================

ClickablePathLabel::ClickablePathLabel(const QString& text,
	QWidget* parent, Qt::WindowFlags f)
	: QLabel(text, parent, f) {
}

//-----------------------------------------------------------------------------

ClickablePathLabel::~ClickablePathLabel() {}

//-----------------------------------------------------------------------------

void ClickablePathLabel::mouseReleaseEvent(QMouseEvent* event) {
	emit onMouseRelease(event);
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::enterEvent(QEvent*) {
	setStyleSheet("text-decoration: underline;");
}

//-----------------------------------------------------------------------------

void ClickablePathLabel::leaveEvent(QEvent*) {
	setStyleSheet("text-decoration: none;");
}

