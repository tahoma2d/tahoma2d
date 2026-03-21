#include "ztorymodel.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/txsheet.h"
#include "toonz/txshcell.h"
#include "toonz/txshsimplelevel.h"
#include "tparamcontainer.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshleveltypes.h"
#include "toonz/childstack.h"
#include "toonz/tscenehandle.h"
#include "toonz/txsheethandle.h"
#include "toonzqt/icongenerator.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/toonzscene.h"

#include <QFile>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include <QDir>
#include <QMessageBox>

// ─── Singleton ────────────────────────────────────────────────────────────────

ZtoryModel::ZtoryModel() : m_fps(24) {}

ZtoryModel *ZtoryModel::instance() {
  static ZtoryModel inst;
  return &inst;
}

// ─── Preview ──────────────────────────────────────────────────────────────────

QPixmap ZtoryModel::preview(int si, int pi) const {
  if (si < 0 || si >= (int)m_previews.size()) return QPixmap();
  if (pi < 0 || pi >= (int)m_previews[si].size()) return QPixmap();
  return m_previews[si][pi];
}

void ZtoryModel::updatePreview(int si, int pi) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  const ShotData &s = m_shots[si];
  if (pi < 0 || pi >= (int)s.panels.size()) return;

  while ((int)m_previews.size() <= si)
    m_previews.push_back({});
  while ((int)m_previews[si].size() <= pi)
    m_previews[si].push_back(QPixmap());

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;

  int col = s.xsheetColumn;
  TXshCell cell = xsh->getCell(s.panels[pi].startFrame, col);
  if (cell.isEmpty()) { emit previewUpdated(si, pi); return; }

  TXshSimpleLevel *sl = cell.getSimpleLevel();
  if (!sl) { emit previewUpdated(si, pi); return; }

  QPixmap px = IconGenerator::instance()->getIcon(sl, cell.getFrameId());
  if (!px.isNull()) {
    m_previews[si][pi] = px;
    emit previewUpdated(si, pi);
  }
}

void ZtoryModel::updateAllPreviews() {
  for (int si = 0; si < (int)m_shots.size(); si++)
    for (int pi = 0; pi < (int)m_shots[si].panels.size(); pi++)
      updatePreview(si, pi);
}

// ─── Operazioni su shot ───────────────────────────────────────────────────────

bool ZtoryModel::assertMainXsheet(bool showWarning) {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return false;
  if (scene->getChildStack()->getAncestorCount() == 0) return true;
  if (showWarning)
    QMessageBox::warning(nullptr, QObject::tr("Ztoryc"),
        QObject::tr("This operation is only available at the main xsheet level.\n"
                    "Please close the current sub-scene first (double-click outside)."));
  return false;
}

void ZtoryModel::addShot(int insertAt) {
  if (!assertMainXsheet(true)) return;
  ShotData s;
  PanelData pd;
  s.panels.push_back(pd);
  if (insertAt < 0 || insertAt >= (int)m_shots.size()) {
    m_shots.push_back(s);
    m_previews.push_back({QPixmap()});
    assignKeepNumbers((int)m_shots.size() - 1);
    emit shotAdded((int)m_shots.size() - 1);
  } else {
    m_shots.insert(m_shots.begin() + insertAt, s);
    m_previews.insert(m_previews.begin() + insertAt, {QPixmap()});
    assignKeepNumbers(insertAt);
    emit shotAdded(insertAt);
  }
  save();
}

void ZtoryModel::addShotNamed(const QString &name) {
  // Creates a fully-wired shot: xsheet column + sub-scene + model entry.
  // Used by ZtoryStartupDialog to pre-populate new projects.
  if (!assertMainXsheet(false)) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  if (!scene || !xsh) return;

  static const int kDefaultDuration = 24;
  int col = xsh->getColumnCount();  // append at end

  // Create a new sub-scene (child level)
  TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
  if (!xl || !xl->getChildLevel()) return;
  TXshChildLevel *cl = xl->getChildLevel();

  xsh->insertColumn(col);
  for (int r = 0; r < kDefaultDuration; r++)
    xsh->setCell(r, col, TXshCell(cl, TFrameId(r + 1)));
  xsh->updateFrameCount();

  // Build model entry
  ShotData s;
  s.xsheetColumn = col;
  s.shotNumber   = name;
  PanelData pd;
  pd.duration = kDefaultDuration;
  s.panels.push_back(pd);
  m_shots.push_back(s);
  m_previews.push_back({QPixmap()});

  app->getCurrentXsheet()->notifyXsheetChanged();
  resequenceXsheet();
  emit modelReset();
}

void ZtoryModel::removeShot(int si) {
  if (!assertMainXsheet(true)) return;
  if (si < 0 || si >= (int)m_shots.size()) return;
  m_shots.erase(m_shots.begin() + si);
  if (si < (int)m_previews.size())
    m_previews.erase(m_previews.begin() + si);
  emit shotRemoved(si);
  save();
}

void ZtoryModel::moveShot(int from, int to) {
  if (!assertMainXsheet(false)) return;
  if (from == to) return;
  if (from < 0 || from >= (int)m_shots.size()) return;
  if (to   < 0 || to   >= (int)m_shots.size()) return;
  ShotData s = m_shots[from];
  std::vector<QPixmap> px = (from < (int)m_previews.size()) ? m_previews[from] : std::vector<QPixmap>();
  m_shots.erase(m_shots.begin() + from);
  m_shots.insert(m_shots.begin() + to, s);
  if (!m_previews.empty()) {
    m_previews.erase(m_previews.begin() + from);
    m_previews.insert(m_previews.begin() + to, px);
  }
  emit shotMoved(from, to);
  save();
}

void ZtoryModel::cloneShot(int si) {
  if (!assertMainXsheet(true)) return;
  if (si < 0 || si >= (int)m_shots.size()) return;
  ShotData s = m_shots[si];
  s.shotNumber = "";
  m_shots.insert(m_shots.begin() + si + 1, s);
  std::vector<QPixmap> px = (si < (int)m_previews.size()) ? m_previews[si] : std::vector<QPixmap>();
  m_previews.insert(m_previews.begin() + si + 1, px);
  assignKeepNumbers(si + 1);
  emit shotAdded(si + 1);
  save();
}

// ─── Numerazione ─────────────────────────────────────────────────────────────

void ZtoryModel::renumberAll() {
  for (int i = 0; i < (int)m_shots.size(); i++) {
    m_shots[i].shotNumber = QString("%1").arg(i + 1, 2, 10, QChar('0'));
    updateColumnName(i);
  }
}

void ZtoryModel::assignKeepNumbers(int insertAt) {
  int total = (int)m_shots.size();
  if (total == 0) return;
  if (m_shots[insertAt].shotNumber.isEmpty()) {
    if (insertAt == 0) {
      m_shots[0].shotNumber = "01";
      return;
    }
    if (insertAt >= total - 1) {
      int n = 0; bool ok = false;
      for (int j = insertAt - 1; j >= 0 && !ok; j--) {
        QString prev = m_shots[j].shotNumber;
        int i = prev.length() - 1;
        while (i >= 0 && prev[i].isLetter()) i--;
        n = prev.left(i + 1).toInt(&ok);
      }
      if (!ok) n = insertAt;
      m_shots[insertAt].shotNumber = QString("%1").arg(n + 1, 2, 10, QChar('0'));
      return;
    }
    QString prev = m_shots[insertAt - 1].shotNumber;
    int i = prev.length() - 1;
    while (i >= 0 && prev[i].isLetter()) i--;
    QString base = prev.left(i + 1);
    QChar nextLetter = 'A';
    for (int j = 0; j < total; j++) {
      if (j == insertAt) continue;
      if (m_shots[j].shotNumber.startsWith(base)) {
        QString suffix = m_shots[j].shotNumber.mid(base.length());
        if (suffix.length() == 1 && suffix[0].isLetter())
          if (suffix[0] >= nextLetter) nextLetter = QChar(suffix[0].unicode() + 1);
      }
    }
    m_shots[insertAt].shotNumber = base + nextLetter;
  }
}

// ─── Panel automatici ─────────────────────────────────────────────────────────

void ZtoryModel::detectAndUpdatePanels(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  ShotData &s = m_shots[si];
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;

  int col = s.xsheetColumn;
  int frameCount = xsh->getFrameCount();
  std::set<int> keyframes;
  keyframes.insert(0);

  TStageObjectTree *tree = xsh->getStageObjectTree();
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TStageObject *obj = tree->getStageObject(TStageObjectId::ColumnId(c), false);
    if (obj) {
      for (int f = 0; f < frameCount; f++)
        if (obj->isKeyframe(f)) keyframes.insert(f);
    }
  }
  TStageObject *cam = tree->getStageObject(TStageObjectId::CameraId(0), false);
  if (cam)
    for (int f = 0; f < frameCount; f++)
      if (cam->isKeyframe(f)) keyframes.insert(f);

  std::vector<PanelData> newPanels;
  std::vector<int> kfList(keyframes.begin(), keyframes.end());
  for (int k = 0; k < (int)kfList.size(); k++) {
    PanelData pd;
    pd.startFrame = kfList[k];
    pd.duration   = (k + 1 < (int)kfList.size()) ? (kfList[k+1] - kfList[k]) : qMax(1, frameCount - kfList[k]);
    if (k < (int)s.panels.size()) {
      pd.dialog = s.panels[k].dialog;
      pd.action = s.panels[k].action;
      pd.notes  = s.panels[k].notes;
    }
    newPanels.push_back(pd);
  }
  if (newPanels.empty()) { PanelData pd; pd.duration = qMax(1, frameCount); newPanels.push_back(pd); }
  s.panels = newPanels;
  emit shotDataChanged(si);
  save();
}

void ZtoryModel::refreshFromScene() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;

  int colCount = xsh->getColumnCount();
  while ((int)m_shots.size() < colCount) {
    ShotData s; s.xsheetColumn = (int)m_shots.size();
    PanelData pd; s.panels.push_back(pd);
    m_shots.push_back(s);
  }
  emit modelReset();
}

// ─── Persistenza ─────────────────────────────────────────────────────────────

void ZtoryModel::save() {
  if (m_ztoryPath.isEmpty()) return;
  QFile file(m_ztoryPath);
  if (!file.open(QIODevice::WriteOnly)) return;
  QXmlStreamWriter xml(&file);
  xml.setAutoFormatting(true);
  xml.writeStartDocument();
  xml.writeStartElement("ztoryc");
  xml.writeAttribute("version", "2");
  for (int si = 0; si < (int)m_shots.size(); si++) {
    const ShotData &s = m_shots[si];
    xml.writeStartElement("shot");
    xml.writeAttribute("index",  QString::number(si));
    xml.writeAttribute("number", s.shotNumber);
    xml.writeAttribute("column", QString::number(s.xsheetColumn));
    for (int pi = 0; pi < (int)s.panels.size(); pi++) {
      const PanelData &pd = s.panels[pi];
      xml.writeStartElement("panel");
      xml.writeAttribute("index",      QString::number(pi));
      xml.writeAttribute("startFrame", QString::number(pd.startFrame));
      xml.writeAttribute("duration",   QString::number(pd.duration));
      xml.writeTextElement("dialog", pd.dialog);
      xml.writeTextElement("action", pd.action);
      xml.writeTextElement("notes",  pd.notes);
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  xml.writeEndElement();
  xml.writeEndDocument();
}

void ZtoryModel::load() {
  if (m_ztoryPath.isEmpty()) return;
  QFile file(m_ztoryPath);
  if (!file.open(QIODevice::ReadOnly)) return;
  m_shots.clear();
  m_previews.clear();
  QXmlStreamReader xml(&file);
  int si = -1, pi = -1;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isStartElement()) {
      if (xml.name() == QLatin1String("shot")) {
        si = xml.attributes().value("index").toInt();
        while ((int)m_shots.size() <= si) m_shots.push_back(ShotData());
        m_shots[si].shotNumber   = xml.attributes().value("number").toString();
        m_shots[si].xsheetColumn = xml.attributes().value("column").toInt();
        m_previews.resize(m_shots.size());
      } else if (xml.name() == QLatin1String("panel") && si >= 0) {
        pi = xml.attributes().value("index").toInt();
        PanelData pd;
        pd.startFrame = xml.attributes().value("startFrame").toInt();
        pd.duration   = xml.attributes().value("duration").toInt();
        while ((int)m_shots[si].panels.size() <= pi) m_shots[si].panels.push_back(PanelData());
        m_shots[si].panels[pi] = pd;
        m_previews[si].resize(m_shots[si].panels.size());
      } else if (xml.name() == QLatin1String("dialog") && si >= 0 && pi >= 0)
        m_shots[si].panels[pi].dialog = xml.readElementText();
      else if (xml.name() == QLatin1String("action") && si >= 0 && pi >= 0)
        m_shots[si].panels[pi].action = xml.readElementText();
      else if (xml.name() == QLatin1String("notes") && si >= 0 && pi >= 0)
        m_shots[si].panels[pi].notes  = xml.readElementText();
    }
  }
  emit modelReset();
}

void ZtoryModel::resequenceXsheet() {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;
  int numCols = xsh->getColumnCount();
  int maxFrames = xsh->getFrameCount() + 200;
  int startFrame = 0;
  for (int col = 0; col < numCols; col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    int duration = r1 - r0 + 1;
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) { startFrame += duration; continue; }
    for (int r = 0; r <= maxFrames; r++) xsh->clearCells(r, col);
    for (int r = 0; r < duration; r++)
      xsh->setCell(startFrame + r, col, TXshCell(cl, TFrameId(r + 1)));
    startFrame += duration;
  }
  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  emit modelReset();
}

void ZtoryModel::updateColumnName(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getXsheet();
  if (!xsh) return;
  int col = m_shots[si].xsheetColumn;
  TStageObject *obj = xsh->getStageObjectTree()->getStageObject(TStageObjectId::ColumnId(col), false);
  if (obj) obj->setName(m_shots[si].shotNumber.toStdString());
}

void ZtoryModel::onXsheetChanged() { updateAllPreviews(); }
void ZtoryModel::onSceneChanged()  { refreshFromScene(); load(); updateAllPreviews(); }
