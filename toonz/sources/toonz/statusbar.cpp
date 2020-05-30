#include "statusbar.h"

#include "tapp.h"

#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/txshlevelhandle.h"
#include "toonz/tframehandle.h"
#include "toonz/txshlevel.h"
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonz/tcolumnhandle.h"
#include "toonz/txshlevel.h"
#include "toonz/txshleveltypes.h"

#include "toonzqt/tselectionhandle.h"
#include "toonzqt/selection.h"

#include "tools/tool.h"


#include "tools/toolhandle.h"

#include <QLayout>
#include <QLabel>


StatusBar::StatusBar(QWidget* parent) : QStatusBar(parent) {
  setObjectName("StatusBar");
	m_currentFrameLabel = new QLabel(tr("Level: 1   Frame: 1"), this);
	m_currentFrameLabel->setObjectName("MainWindowPlainLabel");

	m_infoLabel = new QLabel(tr("Info goes here."), this);
	m_infoLabel->setObjectName("MainWindowLabel");

	m_messageLabel = new QLabel(tr("Messages here?"), this);
	m_messageLabel->setObjectName("MainWindowLabel");	
	
	addWidget(m_currentFrameLabel, 0);
	addWidget(m_infoLabel, 0);
	addPermanentWidget(m_messageLabel, 0);

    TApp* app = TApp::instance();
    TFrameHandle* frameHandle = app->getCurrentFrame();
    TSceneHandle* sceneHandle = app->getCurrentScene();
    TXshLevelHandle* levelHandle = app->getCurrentLevel();
    TObjectHandle* objectHandle = app->getCurrentObject();
    TXsheetHandle* xshHandle = app->getCurrentXsheet();

    bool ret = true;

    ret = ret && connect(xshHandle, SIGNAL(xsheetChanged()), this,
        SLOT(updateFrameText()));
    ret = ret && connect(sceneHandle, SIGNAL(sceneSwitched()), this,
        SLOT(updateFrameText()));
    ret = ret && connect(sceneHandle, SIGNAL(sceneChanged()), this,
        SLOT(updateFrameText()));
    ret = ret && connect(sceneHandle, SIGNAL(nameSceneChanged()), this,
        SLOT(updateFrameText()));


    ret = ret && connect(levelHandle, SIGNAL(xshLevelSwitched(TXshLevel*)), this,
        SLOT(onXshLevelSwitched(TXshLevel*)));
    ret = ret && connect(levelHandle, SIGNAL(xshLevelTitleChanged()), this,
        SLOT(updateFrameText()));
    ret = ret && connect(levelHandle, SIGNAL(xshLevelChanged()), this,
        SLOT(updateFrameText()));
    ret = ret && connect(frameHandle, SIGNAL(frameSwitched()), this,
        SLOT(updateFrameText()));


    ret = ret && connect(frameHandle, SIGNAL(frameTypeChanged()), this,
        SLOT(updateInfoText()));
    ret = ret && connect(app->getCurrentTool(), SIGNAL(toolSwitched()),
        this, SLOT(updateInfoText()));

    assert(ret);

    updateInfoText();
    updateFrameText();

}

//-----------------------------------------------------------------------------

StatusBar::~StatusBar() {}

//-----------------------------------------------------------------------------

void StatusBar::updateFrameText() {
    TApp* app = TApp::instance();
    // zoom = sqrt(m_sceneViewer->getViewMatrix().det());
    ToonzScene* scene = app->getCurrentScene()->getScene();
    if (!scene) return;
    if (!parentWidget()) return;
    int frame = app->getCurrentFrame()->getFrame();
    QString name;
    if (app->getCurrentFrame()->isEditingScene()) {
        TProject* project = scene->getProject();
        QString sceneName = QString::fromStdWString(scene->getSceneName());
        if (sceneName.isEmpty()) sceneName = tr("Untitled");
        if (app->getCurrentScene()->getDirtyFlag()) sceneName += QString("*");
        name = tr("[SCENE]: ") + sceneName;
        if (frame >= 0)
            name =
            name + tr("   ::   Frame: ") + tr(std::to_string(frame + 1).c_str());
        int col = app->getCurrentColumn()->getColumnIndex();
        if (col < 0) {
            m_currentFrameLabel->setText(name);
            return;
        }
        TXsheet* xsh = app->getCurrentXsheet()->getXsheet();
        TXshCell cell = xsh->getCell(frame, col);
        if (cell.isEmpty()) {
            m_currentFrameLabel->setText(name);
            return;
        }
        assert(cell.m_level.getPointer());
        TFilePath fp(cell.m_level->getName());
        QString imageName =
            QString::fromStdWString(fp.withFrame(cell.m_frameId).getWideString());
        name = name + tr("   ::   Level: ") + imageName;
    }
    else {
        TXshLevel* level = app->getCurrentLevel()->getLevel();
        if (level) {
            TFilePath fp(level->getName());
            QString imageName = QString::fromStdWString(
                fp.withFrame(app->getCurrentFrame()->getFid()).getWideString());
            name = name + tr("[LEVEL]: ") + imageName;
        }
    }
    m_currentFrameLabel->setText(name);
}

//-----------------------------------------------------------------------------

void StatusBar::updateInfoText() {
    TApp *app = TApp::instance();
    ToolHandle* toolHandle = app->getCurrentTool();
    TTool* tool = toolHandle->getTool();
    std::string name = tool->getName();
    tool->getToolType();
    int target = tool->getTargetType();

    int type = -1;
    TXshLevelHandle* levelHandle = app->getCurrentLevel();
    TXshLevel* level = levelHandle->getLevel();
    if (level) {
        type = level->getType();
    }
    bool isRaster = false;
    bool isVector = false;
    bool isToonzRaster = false;
    bool isEmpty = false;
    if (type >= 0) {

        if (type == TXshLevelType::PLI_XSHLEVEL) isVector = true;
        else if (type == TXshLevelType::TZP_XSHLEVEL) isToonzRaster = true;
        else if (type == TXshLevelType::OVL_XSHLEVEL) isRaster = true;
        else if (type == NO_XSHLEVEL) isEmpty = true;
    }
    QString text = "";
    if (name == "T_Hand") {
        text += "Hand Tool: Drag to see different areas in thew viewer.";
    } else if (name == "T_Selection") {
        text += "Selection Tool: Select parts of your image to transform it.";
    } else if (name == "T_Dummy" && !isEmpty) {
        text += "This tool doesn't work on this layer type.";
    }


    m_infoLabel->setText(text);
}

//-----------------------------------------------------------------------------

void StatusBar::updateMessageText() {

}

//-----------------------------------------------------------------------------

void StatusBar::onXshLevelSwitched(TXshLevel*) {

}