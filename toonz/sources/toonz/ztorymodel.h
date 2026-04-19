#pragma once
#include <QObject>
#include <QPixmap>
#include <QString>
#include <vector>
#include <set>
#include "toonz/txshchildlevel.h"  // for TXshLevelP

// ─── NumberingConfig ─────────────────────────────────────────────────────────
// Persistent numbering scheme used both at startup and during Board editing.

struct NumberingConfig {
  enum Style { Simple, Sequence } style = Simple;
  QString shotPrefix  = "sh";
  QString seqPrefix   = "sq";
  int     step        = 10;
  int     padding     = 3;
  int     seqPadding  = 2;
  int     startNumber = 10;
  int     seqNumber   = 1;   // active sequence number (for Sequence style)

  // Returns the shot name for a 0-based index, e.g. shotName(0)="sh010"
  QString shotName(int idx) const;
};

// ─── Shared clipboard ─────────────────────────────────────────────────────────
// Used by both StoryboardPanel and ZtoryAnimaticPanel so that copy/cut in one
// panel is immediately available for paste in the other.

struct ZtoryClipEntry {
  int        srcCol   = 0;    // xsheet column at copy/clone time; -1 for cut
  int        duration = 24;   // cell count of original column
  bool       isCut    = false;
  bool       isClone  = false;
  TXshLevelP cutLevel;        // keeps sub-scene alive after immediate cut
};

// ─── Strutture dati ───────────────────────────────────────────────────────────

struct PanelData {
  int     startFrame;
  int     duration;
  QString dialog;
  QString action;
  QString notes;
  PanelData() : startFrame(0), duration(24) {}
};

struct ShotData {
  int                    xsheetColumn;
  QString                shotNumber;
  std::vector<PanelData> panels;
  ShotData() : xsheetColumn(0) {}
  int totalDuration() const {
    int tot = 0;
    for (const auto &p : panels) tot += p.duration;
    return tot;
  }
};

// ─── Workflow ─────────────────────────────────────────────────────────────────
// Authoritative global workflow state.  Set via ZtoryModel::setWorkflow() at
// every transition point; query via ZtoryModel::currentWorkflow().

enum class ZtoryWorkflow {
  Storyboard,   // Storyboard Mode
  Tradigital,   // 2D Tradigital Mode
  CutoutDigital, // Cutout Digital Mode
  StopMotion,   // Stop-Motion Mode
};

// ─── ZtoryModel ───────────────────────────────────────────────────────────────

class ZtoryModel : public QObject {
  Q_OBJECT

  std::vector<ShotData>         m_shots;
  std::vector<std::vector<QPixmap>> m_previews; // [shotIdx][panelIdx]
  int                           m_fps;
  QString                       m_ztoryPath;
  ZtoryWorkflow                 m_workflow = ZtoryWorkflow::Tradigital;
  std::vector<ZtoryClipEntry>   m_sharedClip;      // shared across Board & Animatic
  std::set<int>                 m_sharedSelection; // xsheet columns, last-panel-wins

  ZtoryModel();
  NumberingConfig m_numberingConfig;

public:
  static ZtoryModel *instance();

  // ── Accesso dati ──────────────────────────────────────────────────────────
  int  shotCount() const { return (int)m_shots.size(); }
  ShotData       &shot(int i)       { return m_shots[i]; }
  const ShotData &shot(int i) const { return m_shots[i]; }
  std::vector<ShotData>       &shots()       { return m_shots; }
  const std::vector<ShotData> &shots() const { return m_shots; }
  int  fps() const { return m_fps; }

  // ── Preview ───────────────────────────────────────────────────────────────
  QPixmap preview(int shotIdx, int panelIdx) const;
  void    updatePreview(int shotIdx, int panelIdx);
  void    updateAllPreviews();

  // ── Operazioni su shot ────────────────────────────────────────────────────
  void addShot(int insertAt = -1);
  // Creates a fully-wired shot (xsheet column + sub-scene) with a given name.
  // Used by ZtoryStartupDialog to pre-populate a new project.
  void addShotNamed(const QString &name);
  // Clears model data only (no xsheet changes). Call before re-populating.
  void clearShots() { m_shots.clear(); m_previews.clear(); }
  void removeShot(int shotIdx);
  void moveShot(int fromIdx, int toIdx);
  void cloneShot(int shotIdx);

  // ── Numerazione ───────────────────────────────────────────────────────────
  void renumberAll();                       // full renumber using m_numberingConfig
  void assignKeepNumbers(int insertAt);     // letter-suffix for Keep-# mode
  QString nextShotName() const;             // next auto name after existing shots
  void setNumberingConfig(const NumberingConfig &cfg);
  NumberingConfig       &numberingConfig()       { return m_numberingConfig; }
  const NumberingConfig &numberingConfig() const { return m_numberingConfig; }

  // ── Panel automatici ──────────────────────────────────────────────────────
  void detectAndUpdatePanels(int shotIdx);
  void refreshFromScene();

  // ── Persistenza ───────────────────────────────────────────────────────────
  void save();
  void load();
  void setZtoryPath(const QString &path) { m_ztoryPath = path; }

  // ── Shared clipboard (Board ↔ Animatic) ──────────────────────────────────
  const std::vector<ZtoryClipEntry>& sharedClip() const { return m_sharedClip; }
  void setSharedClip(std::vector<ZtoryClipEntry> v)     { m_sharedClip = std::move(v); }

  // ── Shared selection (Board ↔ Animatic) — xsheet column indices ─────────
  // Written by whichever panel last had user interaction.
  // Used by merge buttons as fallback when own selection is < 2 shots.
  const std::set<int>& sharedSelection() const { return m_sharedSelection; }
  void setSharedSelection(std::set<int> s)      { m_sharedSelection = std::move(s); }

  // ── Resequencing ──────────────────────────────────────────────────────────
  void resequenceXsheet();

  // Returns true if at main xsheet level; optionally shows a warning dialog.
  static bool assertMainXsheet(bool showWarning = true);

  // ── Workflow state ──────────────────────────────────────────────────────────
  ZtoryWorkflow currentWorkflow() const { return m_workflow; }
  void setWorkflow(ZtoryWorkflow w);

  // Convenience: true when currentWorkflow() == ZtoryWorkflow::Storyboard.
  bool isStoryboardWorkflow() const {
    return m_workflow == ZtoryWorkflow::Storyboard;
  }

  // ── Sincronizzazione scena ────────────────────────────────────────────────
  void onXsheetChanged();
  void onSceneChanged();
  void updateColumnName(int shotIdx);

signals:
  void workflowChanged(ZtoryWorkflow workflow);
  void modelReset();                          // tutto cambiato
  void shotAdded(int shotIdx);
  void shotRemovedAt(int col);  // col = xsheet column deleted (symmetric with shotAdded)
  void shotRemoved(int shotIdx);
  void shotMoved(int fromIdx, int toIdx);
  void shotDataChanged(int shotIdx);
  void previewUpdated(int shotIdx, int panelIdx);
};
