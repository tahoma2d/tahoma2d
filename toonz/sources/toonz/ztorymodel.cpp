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
#include <QRegularExpression>
#include <QFileInfo>
#include <QUuid>
#include <climits>

// ─── ZtoryNumbering ───────────────────────────────────────────────────────────
// Internal helpers for the SH/SQ/P labelling system.
// orderIndex uses 100× scale: "SH010" ↔ orderIndex 1000, "SH020" ↔ 2000.

namespace ZtoryNumbering {

// Format integer n with given padding, e.g. formatN(10, 3) → "010"
static QString formatN(int n, int pad) {
  return QString("%1").arg(n, pad, 10, QChar('0'));
}

// Extract the numeric value from a label, stripping prefix and optional
// trailing alpha suffix.  Returns -1 on failure.
// E.g. labelNum("SH010A", "SH") → 10,  labelNum("SH020", "SH") → 20
static int labelNum(const QString &label, const QString &prefix) {
  if (!label.startsWith(prefix, Qt::CaseInsensitive)) return -1;
  QString rest = label.mid(prefix.length());
  while (!rest.isEmpty() && rest.back().isLetter()) rest.chop(1);
  bool ok;
  int n = rest.toInt(&ok);
  return ok ? n : -1;
}

// Strip trailing alpha suffix from a label.
// E.g. "SH010A" → "SH010",  "SH010" → "SH010"
static QString stripSuffix(const QString &label, const QString &prefix) {
  if (!label.startsWith(prefix, Qt::CaseInsensitive)) return label;
  QString rest = label.mid(prefix.length());
  while (!rest.isEmpty() && rest.back().isLetter()) rest.chop(1);
  return prefix + rest;
}

// Collect all current shotLabels from a shots vector.
static QStringList allLabels(const std::vector<ShotData> &shots) {
  QStringList res;
  for (const auto &s : shots)
    if (!s.shotLabel.isEmpty()) res << s.shotLabel;
  return res;
}

// Find the next available alpha suffix for a base label.
// E.g. existing = {"SH010", "SH010A", "SH010B"}, base = "SH010" → 'C'
static QChar nextSuffix(const QStringList &existing, const QString &base) {
  for (char c = 'A'; c <= 'Z'; c++) {
    if (!existing.contains(base + QChar(c))) return QChar(c);
  }
  return 'A';  // fallback (exhausted A-Z, shouldn't happen)
}

}  // namespace ZtoryNumbering

// ─── NumberingConfig ──────────────────────────────────────────────────────────

QString NumberingConfig::shotName(int idx) const {
  int number = startNumber + idx * step;
  if (style == Sequence) {
    return QString("%1%2_%3%4")
        .arg(seqPrefix)
        .arg(seqNumber, seqPadding, 10, QChar('0'))
        .arg(shotPrefix)
        .arg(number, padding, 10, QChar('0'));
  }
  return QString("%1%2").arg(shotPrefix).arg(number, padding, 10, QChar('0'));
}

// ─── Singleton ────────────────────────────────────────────────────────────────

ZtoryModel::ZtoryModel() : m_fps(24) {}

// ─── Sequences ────────────────────────────────────────────────────────────────

SequenceData* ZtoryModel::findSequence(const QString &uuid) {
  for (auto &seq : m_sequences)
    if (seq.uuid == uuid) return &seq;
  return nullptr;
}

SequenceData* ZtoryModel::findOrCreateSequence(const QString &label) {
  if (label.isEmpty()) return nullptr;
  // Case-insensitive lookup by label
  for (auto &seq : m_sequences)
    if (seq.label.compare(label, Qt::CaseInsensitive) == 0) return &seq;
  // Not found — create a new sequence
  SequenceData seq;
  seq.uuid  = QUuid::createUuid().toString(QUuid::WithoutBraces);
  seq.label = label;
  // Derive orderIndex from the numeric part of the label
  QString numPart = label;
  while (!numPart.isEmpty() && numPart[0].isLetter()) numPart.remove(0, 1);
  bool ok;
  int n = numPart.toInt(&ok);
  seq.orderIndex = ok ? n : (int)m_sequences.size() + 1;
  m_sequences.push_back(seq);
  return &m_sequences.back();
}

void ZtoryModel::ensureDefaultSequence() {
  if (!m_sequences.empty()) return;
  SequenceData seq;
  seq.uuid  = QUuid::createUuid().toString(QUuid::WithoutBraces);
  seq.label = m_numberingConfig.seqPrefix +
              ZtoryNumbering::formatN(m_numberingConfig.startNumber,
                                     m_numberingConfig.seqPadding);
  seq.orderIndex = m_numberingConfig.startNumber;
  m_sequences.push_back(seq);
}

// ─── Labelling ────────────────────────────────────────────────────────────────

// Static implementation — works on any vector<ShotData>.
// Called by generateShotLabel() and by StoryboardPanel via projected vector.
void ZtoryModel::assignShotLabel(std::vector<ShotData> &shots, int si,
                                  const NumberingConfig &cfg) {
  if (si < 0 || si >= (int)shots.size()) return;
  ShotData &s    = shots[si];
  const QString pfx   = cfg.shotPrefix;
  const int     pad   = cfg.padding;
  const int     step  = cfg.step;
  const int     scale = 100;  // orderIndex = labelNumber * scale

  // Collect existing labels, excluding this shot's own current label
  QStringList existing = ZtoryNumbering::allLabels(shots);
  existing.removeAll(s.shotLabel);

  // Resolve effective orderIndex for a neighbour (fallback: position-based)
  auto effectiveOrder = [&](int idx) -> int {
    int o = shots[idx].orderIndex;
    return (o > 0) ? o : (idx + 1) * step * scale;
  };

  const bool hasPrev = (si > 0);
  const bool hasNext = (si + 1 < (int)shots.size());
  int prevOrder = hasPrev ? effectiveOrder(si - 1) : 0;
  int nextOrder = hasNext ? effectiveOrder(si + 1) : 0;

  if (!hasPrev && !hasNext) {
    // Only shot in the project
    int num = cfg.startNumber;
    s.orderIndex = num * scale;
    s.shotLabel  = pfx + ZtoryNumbering::formatN(num, pad);

  } else if (!hasPrev) {
    // Inserting at the very beginning
    int nextNum = ZtoryNumbering::labelNum(shots[si + 1].label(), pfx);
    if (nextNum <= 0) nextNum = nextOrder / scale;
    int num = qMax(1, nextNum - step);
    QString cand = pfx + ZtoryNumbering::formatN(num, pad);
    if (existing.contains(cand)) {
      QString base = ZtoryNumbering::stripSuffix(cand, pfx);
      s.shotLabel = base + ZtoryNumbering::nextSuffix(existing, base);
    } else {
      s.shotLabel = cand;
    }
    s.orderIndex = nextOrder / 2;

  } else if (!hasNext) {
    // Appending at the end
    int prevNum = ZtoryNumbering::labelNum(shots[si - 1].label(), pfx);
    if (prevNum <= 0) prevNum = prevOrder / scale;
    int num = prevNum + step;
    QString cand = pfx + ZtoryNumbering::formatN(num, pad);
    while (existing.contains(cand)) {
      num += step;
      cand = pfx + ZtoryNumbering::formatN(num, pad);
    }
    s.shotLabel  = cand;
    s.orderIndex = prevOrder + step * scale;

  } else {
    // Inserting between two existing shots
    int prevNum = ZtoryNumbering::labelNum(shots[si - 1].label(), pfx);
    int nextNum = ZtoryNumbering::labelNum(shots[si + 1].label(), pfx);
    if (prevNum <= 0) prevNum = prevOrder / scale;
    if (nextNum <= 0) nextNum = nextOrder / scale;
    int midOrder = (prevOrder + nextOrder) / 2;

    // Prefer midpoint; scan for any free integer in (prevNum, nextNum)
    int midNum = (prevNum + nextNum) / 2;
    int found  = -1;
    if (midNum > prevNum && midNum < nextNum) {
      if (!existing.contains(pfx + ZtoryNumbering::formatN(midNum, pad)))
        found = midNum;
    }
    if (found < 0) {
      for (int n = prevNum + 1; n < nextNum && found < 0; n++) {
        if (!existing.contains(pfx + ZtoryNumbering::formatN(n, pad))) found = n;
      }
    }
    if (found >= 0) {
      s.shotLabel  = pfx + ZtoryNumbering::formatN(found, pad);
      s.orderIndex = midOrder;
    } else {
      // No integer space: alphabetical suffix on the previous label
      QString base = ZtoryNumbering::stripSuffix(shots[si - 1].label(), pfx);
      s.shotLabel  = base + ZtoryNumbering::nextSuffix(existing, base);
      s.orderIndex = midOrder;
    }
  }

  // Keep legacy shotNumber in sync for backward compat
  s.shotNumber = s.shotLabel;
}

void ZtoryModel::generateShotLabel(int si) {
  assignShotLabel(m_shots, si, m_numberingConfig);
}

void ZtoryModel::cleanRenumber() {
  const NumberingConfig &cfg   = m_numberingConfig;
  const QString          pfx   = cfg.shotPrefix;
  const int              pad   = cfg.padding;
  const int              step  = cfg.step;
  const int              scale = 100;

  for (int i = 0; i < (int)m_shots.size(); i++) {
    int num = cfg.startNumber + i * step;
    m_shots[i].shotLabel  = pfx + ZtoryNumbering::formatN(num, pad);
    m_shots[i].shotNumber = m_shots[i].shotLabel;
    m_shots[i].orderIndex = num * scale;
    updateColumnName(i);
  }
}

void ZtoryModel::generatePanelLabels(int si) {
  if (si < 0 || si >= (int)m_shots.size()) return;
  const QString &pfx = m_numberingConfig.panelPrefix;
  auto &panels = m_shots[si].panels;
  for (int pi = 0; pi < (int)panels.size(); pi++) {
    panels[pi].panelLabel = pfx + ZtoryNumbering::formatN(pi + 1, 3);
    panels[pi].orderIndex = pi;
  }
}

QString ZtoryModel::fullLabel(int si) const {
  if (si < 0 || si >= (int)m_shots.size()) return QString();
  const ShotData &s = m_shots[si];
  if (s.sequenceId.isEmpty()) return s.label();
  for (const auto &seq : m_sequences)
    if (seq.uuid == s.sequenceId) return seq.label + "_" + s.label();
  return s.label();
}

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

void ZtoryModel::setWorkflow(ZtoryWorkflow w) {
  if (m_workflow == w) return;
  m_workflow = w;
  emit workflowChanged(w);
}

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
    generateShotLabel((int)m_shots.size() - 1);
    emit shotAdded((int)m_shots.size() - 1);
  } else {
    m_shots.insert(m_shots.begin() + insertAt, s);
    m_previews.insert(m_previews.begin() + insertAt, {QPixmap()});
    generateShotLabel(insertAt);
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
  s.shotLabel    = name;  // keep shotLabel in sync (primary display field)
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
  s.shotNumber = "";   // reset — will be assigned by generateShotLabel
  s.shotLabel  = "";
  s.orderIndex = 0;
  m_shots.insert(m_shots.begin() + si + 1, s);
  std::vector<QPixmap> px = (si < (int)m_previews.size()) ? m_previews[si] : std::vector<QPixmap>();
  m_previews.insert(m_previews.begin() + si + 1, px);
  generateShotLabel(si + 1);
  emit shotAdded(si + 1);
  save();
}

// ─── Numerazione ─────────────────────────────────────────────────────────────

void ZtoryModel::setNumberingConfig(const NumberingConfig &cfg) {
  m_numberingConfig = cfg;
  // Don't call save() here — caller decides when to persist
}

QString ZtoryModel::nextShotName() const {
  const NumberingConfig &cfg = m_numberingConfig;
  // Parse existing shot numbers to find the highest matching number
  QRegularExpression re;
  if (cfg.style == NumberingConfig::Sequence) {
    re.setPattern(
        QString("^%1\\d+_%2(\\d+)$")
            .arg(QRegularExpression::escape(cfg.seqPrefix),
                 QRegularExpression::escape(cfg.shotPrefix)));
  } else {
    re.setPattern(
        QString("^%1(\\d+)$")
            .arg(QRegularExpression::escape(cfg.shotPrefix)));
  }
  int maxNum = cfg.startNumber - cfg.step;
  for (const auto &s : m_shots) {
    auto m = re.match(s.label());
    if (m.hasMatch()) {
      int n = m.captured(1).toInt();
      if (n > maxNum) maxNum = n;
    }
  }
  int next = qMax(cfg.startNumber, maxNum + cfg.step);
  if (cfg.style == NumberingConfig::Sequence) {
    return QString("%1%2_%3%4")
        .arg(cfg.seqPrefix)
        .arg(cfg.seqNumber, cfg.seqPadding, 10, QChar('0'))
        .arg(cfg.shotPrefix)
        .arg(next, cfg.padding, 10, QChar('0'));
  }
  return QString("%1%2").arg(cfg.shotPrefix).arg(next, cfg.padding, 10, QChar('0'));
}

void ZtoryModel::renumberAll() {
  const int scale = 100;
  for (int i = 0; i < (int)m_shots.size(); i++) {
    m_shots[i].shotNumber = m_numberingConfig.shotName(i);
    m_shots[i].shotLabel  = m_shots[i].shotNumber;  // keep shotLabel in sync
    m_shots[i].orderIndex =
        (m_numberingConfig.startNumber + i * m_numberingConfig.step) * scale;
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
  xml.writeAttribute("version", "4");
  // ── Numbering config ──
  xml.writeStartElement("numberingConfig");
  xml.writeAttribute("style",       QString::number((int)m_numberingConfig.style));
  xml.writeAttribute("shotPrefix",  m_numberingConfig.shotPrefix);
  xml.writeAttribute("seqPrefix",   m_numberingConfig.seqPrefix);
  xml.writeAttribute("panelPrefix", m_numberingConfig.panelPrefix);
  xml.writeAttribute("step",        QString::number(m_numberingConfig.step));
  xml.writeAttribute("padding",     QString::number(m_numberingConfig.padding));
  xml.writeAttribute("seqPadding",  QString::number(m_numberingConfig.seqPadding));
  xml.writeAttribute("startNumber", QString::number(m_numberingConfig.startNumber));
  xml.writeAttribute("seqNumber",   QString::number(m_numberingConfig.seqNumber));
  xml.writeEndElement();
  // ── Sequences ──
  if (!m_sequences.empty()) {
    xml.writeStartElement("sequences");
    for (const auto &seq : m_sequences) {
      xml.writeStartElement("sequence");
      xml.writeAttribute("uuid",  seq.uuid);
      xml.writeAttribute("label", seq.label);
      xml.writeAttribute("order", QString::number(seq.orderIndex));
      xml.writeEndElement();
    }
    xml.writeEndElement();
  }
  // ── Shots ──
  for (int si = 0; si < (int)m_shots.size(); si++) {
    const ShotData &s = m_shots[si];
    xml.writeStartElement("shot");
    xml.writeAttribute("index",  QString::number(si));
    xml.writeAttribute("number", s.shotNumber);       // legacy
    xml.writeAttribute("label",  s.shotLabel);        // v4
    xml.writeAttribute("order",  QString::number(s.orderIndex)); // v4
    xml.writeAttribute("seqId",  s.sequenceId);       // v4
    xml.writeAttribute("column", QString::number(s.xsheetColumn));
    for (int pi = 0; pi < (int)s.panels.size(); pi++) {
      const PanelData &pd = s.panels[pi];
      xml.writeStartElement("panel");
      xml.writeAttribute("index",      QString::number(pi));
      xml.writeAttribute("startFrame", QString::number(pd.startFrame));
      xml.writeAttribute("duration",   QString::number(pd.duration));
      if (!pd.panelLabel.isEmpty())
        xml.writeAttribute("panelLabel", pd.panelLabel);
      xml.writeAttribute("panelOrder",  QString::number(pd.orderIndex));
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
  m_sequences.clear();

  QXmlStreamReader xml(&file);
  int si = -1, pi = -1;

  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement()) continue;
    const auto name = xml.name();

    if (name == QLatin1String("numberingConfig")) {
      m_numberingConfig.style =
          (NumberingConfig::Style)xml.attributes().value("style").toInt();
      m_numberingConfig.shotPrefix  = xml.attributes().value("shotPrefix").toString();
      m_numberingConfig.seqPrefix   = xml.attributes().value("seqPrefix").toString();
      QString ppfx = xml.attributes().value("panelPrefix").toString();
      if (!ppfx.isEmpty()) m_numberingConfig.panelPrefix = ppfx;
      m_numberingConfig.step        = xml.attributes().value("step").toInt();
      m_numberingConfig.padding     = xml.attributes().value("padding").toInt();
      m_numberingConfig.seqPadding  = xml.attributes().value("seqPadding").toInt();
      m_numberingConfig.startNumber = xml.attributes().value("startNumber").toInt();
      m_numberingConfig.seqNumber   = xml.attributes().value("seqNumber").toInt();
      // Safety defaults for old files
      if (m_numberingConfig.step <= 0)    m_numberingConfig.step = 10;
      if (m_numberingConfig.padding <= 0) m_numberingConfig.padding = 3;
      if (m_numberingConfig.shotPrefix.isEmpty())  m_numberingConfig.shotPrefix  = "SH";
      if (m_numberingConfig.panelPrefix.isEmpty()) m_numberingConfig.panelPrefix = "P";

    } else if (name == QLatin1String("sequence")) {
      SequenceData seq;
      seq.uuid       = xml.attributes().value("uuid").toString();
      seq.label      = xml.attributes().value("label").toString();
      seq.orderIndex = xml.attributes().value("order").toInt();
      if (!seq.uuid.isEmpty()) m_sequences.push_back(seq);

    } else if (name == QLatin1String("shot")) {
      si = xml.attributes().value("index").toInt();
      while ((int)m_shots.size() <= si) m_shots.push_back(ShotData());
      m_shots[si].shotNumber   = xml.attributes().value("number").toString();
      m_shots[si].shotLabel    = xml.attributes().value("label").toString();
      m_shots[si].orderIndex   = xml.attributes().value("order").toInt();
      m_shots[si].sequenceId   = xml.attributes().value("seqId").toString();
      m_shots[si].xsheetColumn = xml.attributes().value("column").toInt();
      // Backward compat (v1–v3): if shotLabel absent, copy from shotNumber
      if (m_shots[si].shotLabel.isEmpty())
        m_shots[si].shotLabel = m_shots[si].shotNumber;
      m_previews.resize(m_shots.size());

    } else if (name == QLatin1String("panel") && si >= 0) {
      pi = xml.attributes().value("index").toInt();
      PanelData pd;
      pd.startFrame = xml.attributes().value("startFrame").toInt();
      pd.duration   = xml.attributes().value("duration").toInt();
      pd.panelLabel = xml.attributes().value("panelLabel").toString();
      pd.orderIndex = xml.attributes().value("panelOrder").toInt();
      while ((int)m_shots[si].panels.size() <= pi) m_shots[si].panels.push_back(PanelData());
      m_shots[si].panels[pi] = pd;
      m_previews[si].resize(m_shots[si].panels.size());

    } else if (name == QLatin1String("dialog") && si >= 0 && pi >= 0)
      m_shots[si].panels[pi].dialog = xml.readElementText();
    else if (name == QLatin1String("action") && si >= 0 && pi >= 0)
      m_shots[si].panels[pi].action = xml.readElementText();
    else if (name == QLatin1String("notes") && si >= 0 && pi >= 0)
      m_shots[si].panels[pi].notes  = xml.readElementText();
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
  if (obj) obj->setName(m_shots[si].label().toStdString());
}

void ZtoryModel::onXsheetChanged() { updateAllPreviews(); }
void ZtoryModel::onSceneChanged()  { refreshFromScene(); load(); updateAllPreviews(); }
