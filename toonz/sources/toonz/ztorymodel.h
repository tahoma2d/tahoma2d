#pragma once
#include <QObject>
#include <QPixmap>
#include <QString>
#include <vector>

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

// ─── ZtoryModel ───────────────────────────────────────────────────────────────

class ZtoryModel : public QObject {
  Q_OBJECT

  std::vector<ShotData>         m_shots;
  std::vector<std::vector<QPixmap>> m_previews; // [shotIdx][panelIdx]
  int                           m_fps;
  QString                       m_ztoryPath;

  ZtoryModel();

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
  void renumberAll();
  void assignKeepNumbers(int insertAt);

  // ── Panel automatici ──────────────────────────────────────────────────────
  void detectAndUpdatePanels(int shotIdx);
  void refreshFromScene();

  // ── Persistenza ───────────────────────────────────────────────────────────
  void save();
  void load();
  void setZtoryPath(const QString &path) { m_ztoryPath = path; }

  // ── Resequencing ──────────────────────────────────────────────────────────
  void resequenceXsheet();

  // Returns true if at main xsheet level; optionally shows a warning dialog.
  static bool assertMainXsheet(bool showWarning = true);

  // ── Sincronizzazione scena ────────────────────────────────────────────────
  void onXsheetChanged();
  void onSceneChanged();
  void updateColumnName(int shotIdx);

signals:
  void modelReset();                          // tutto cambiato
  void shotAdded(int shotIdx);
  void shotRemoved(int shotIdx);
  void shotMoved(int fromIdx, int toIdx);
  void shotDataChanged(int shotIdx);
  void previewUpdated(int shotIdx, int panelIdx);
};
