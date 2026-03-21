#pragma once
#ifndef ZTORYANIMATIC_H
#define ZTORYANIMATIC_H

#include "viewerpane.h"
#include "tapp.h"
#include "pane.h"
#include "toonz/onionskinmask.h"
#include <QWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QHBoxLayout>
#include <set>

// ---- ZtoryAnimaticController ----
// Singleton that owns the dedicated frame state for the animatic timeline
// and viewer, isolating them from TApp's global TFrameHandle.
class TFrameHandle;
#include "tsound.h"

class ZtoryAnimaticController : public QObject {
  Q_OBJECT
public:
  static ZtoryAnimaticController *instance();
  TFrameHandle *frameHandle() const { return m_frameHandle; }
  TXsheet *mainXsheet() const;
  void setCurrentFrame(int frame);
  int currentFrame() const;
  // Cached merged sound track — filled by ZtoryAnimaticViewer::refreshAnimaticSound
  TSoundTrackP soundTrack() const { return m_soundTrack; }
  void setSoundTrack(TSoundTrackP st) { m_soundTrack = st; }

private:
  ZtoryAnimaticController();
  TFrameHandle  *m_frameHandle;
  TSoundTrackP   m_soundTrack;
};

class ZtoryAnimaticRuler : public QWidget {
  Q_OBJECT
public:
  ZtoryAnimaticRuler(QWidget *parent = nullptr);
  void setFps(double fps) { m_fps = fps; update(); }
  void setPixelsPerFrame(double ppf) { m_ppf = ppf; update(); }
  void setCurrentFrame(int f) { m_currentFrame = f; update(); }
  int currentFrame() const { return m_currentFrame; }
  void initPlayRangeIfNeeded();

  // Local (independent) onion skin — does NOT share state with native timeline
  void setOnionEnabled(bool on);
  bool onionEnabled() const { return m_onionEnabled; }
  void syncOnionToGlobal() const; // push local mask to global handle

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;
  void contextMenuEvent(QContextMenuEvent *) override;
signals:
  void frameChanged(int frame);
  void onionEnabledChanged(bool on);
private:
  double m_fps = 24.0;
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  // In/Out marker drag state (13b)
  enum DragMode { None, DragIn, DragOut };
  DragMode m_dragMode = None;
  // Onion skin — local state, independent from native timeline
  OnionSkinMask m_localMask;  // relative frame mode; FOS=fixed, MOS=relative
  bool m_onionEnabled = false;
  // Hover feedback
  int m_hoverFrame = -1;
  enum HoverZone { HoverNone, HoverFOS, HoverMOS };
  HoverZone m_hoverZone = HoverNone;
};

class ZtoryAnimaticTrack : public QWidget {
  Q_OBJECT
public:
  enum Tool { SelectTool, RazorTool };

  struct ShotBlock {
    int col;
    int startFrameInMain; // frame di inizio nel main xsheet
    int f0, f1;           // marker In/Out della sottoscena
    QString shotNumber;
    QPixmap thumbnail;
  };

  ZtoryAnimaticTrack(QWidget *parent = nullptr);
  void setPixelsPerFrame(double ppf) { m_ppf = ppf; update(); }
  void setCurrentFrame(int f) { m_currentFrame = f; update(); }
  void setTool(Tool t) { m_tool = t; updateCursor(); }
  Tool tool() const { return m_tool; }
  void refreshFromScene();
  const std::set<int> &selectedCols() const { return m_selectedCols; }

protected:
  void paintEvent(QPaintEvent *) override;
  void wheelEvent(QWheelEvent *e) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;

signals:
  void shotClicked(int col);
  void shotDoubleClicked(int col);
  void selectionChanged(std::set<int> selectedCols);
  void shotDurationChanged(int col, int newF1);
  void shotMoved(int col, int newStartFrame);
  void zoomChanged(double ppf);
  void matchSubsceneDuration(int col);
  void razorRequested(int col, int splitFrame);

private:
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  std::vector<ShotBlock> m_blocks;
  int m_draggingCol = -1;
  int m_dragStartX = 0;
  int m_dragOrigF1 = 0;
  QMap<int, int> m_origStarts;
  QMap<int, int> m_origDurations;
  std::set<int> m_selectedCols;
  int m_lastClickedCol = -1; // for Shift+click range selection
  Tool m_tool = SelectTool;

  void updateCursor();
};

// ---- ZtoryAudioTrack ----
// Una traccia audio orizzontale con waveform, nome colonna e altezza regolabile
class ZtoryAudioTrack : public QWidget {
  Q_OBJECT
public:
  ZtoryAudioTrack(int col, const QString &name, QWidget *parent = nullptr);
  void setPixelsPerFrame(double ppf) {
    if (m_ppf != ppf) { m_ppf = ppf; m_waveformDirty = true; update(); }
  }
  void setCurrentFrame(int f) { m_currentFrame = f; update(); }
  void invalidateWaveform() { m_waveformDirty = true; update(); }
  int trackHeight() const { return m_trackHeight; }
  void setTrackHeight(int h) { m_trackHeight = h; m_waveformDirty = true; setFixedHeight(h); update(); }
  int columnIndex() const { return m_col; }

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;

private:
  int m_col;
  QString m_name;
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  int m_trackHeight = 50;
  QPixmap m_waveformCache;
  bool m_waveformDirty = true;
  // Preview bar (12c)
  int  m_previewR0        = -1;
  int  m_previewR1        = -1;
  bool m_draggingPreview  = false;
  int  m_previewDragStart = -1;
};

// ---- ZtoryStoryStrip ----
// Horizontal thumbnail strip showing all shots; clicking navigates to a shot.
class ZtoryStoryStrip : public QWidget {
  Q_OBJECT
public:
  explicit ZtoryStoryStrip(QWidget *parent = nullptr);
  void refreshFromScene();
  void setCurrentCol(int col);

signals:
  void shotClicked(int col);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void wheelEvent(QWheelEvent *) override;

private:
  struct ThumbEntry {
    int col;
    QString shotNumber;
    QPixmap thumb;
  };
  std::vector<ThumbEntry> m_entries;
  int m_currentCol = -1;
  int m_scrollOffset = 0; // horizontal pixel offset
  static constexpr int kThumbH = 54;
  static constexpr int kThumbW = 80;
  static constexpr int kSpacing = 4;
};

// ---- ZtoryStoryStripPanel ----
// Standalone TPanel wrapper for ZtoryStoryStrip
class ZtoryStoryStripPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryStoryStripPanel(QWidget *parent = nullptr);
  void refreshFromScene();

protected:
  void showEvent(QShowEvent *e) override;

private:
  ZtoryStoryStrip *m_strip;
};

// ---- ZtoryAnimaticViewer ----
// Standalone scene viewer that always shows the main xsheet.
// Overrides onDrawFrame and updateFrameRange so that playback is driven by the
// dedicated ZtoryAnimaticController::frameHandle() instead of the global
// TApp::getCurrentFrame().  This decouples the animatic play button from the
// native timeline / sub-scene currently open in the editor (BUG-03 play fix).
class ZtoryAnimaticViewer : public BaseViewerPanel {
  Q_OBJECT
public:
  ZtoryAnimaticViewer(QWidget *parent = nullptr);
  void updateShowHide() override {}
  void addShowHideContextMenu(QMenu *) override {}
  void checkOldVersionVisblePartsFlags(QSettings &) override {}

  // Override: write frame to controller's handle, NOT TApp::getCurrentFrame().
  // Base implementation always uses the global handle → during play it would
  // advance the sub-scene's frame instead of the animatic frame.
  void onDrawFrame(int frame,
                   const ImagePainter::VisualSettings &settings,
                   QElapsedTimer *timer     = nullptr,
                   qint64 targetInstant     = 0) override;

protected:
  void showEvent(QShowEvent *e) override;

private slots:
  // Sets FlipConsole frame range from the main xsheet (top-level), not from
  // TApp::getCurrentFrame() which would return the sub-scene's frame count.
  void updateAnimaticFrameRange();
  // Sets FlipConsole in/out markers from the main xsheet play range.
  // When in a sub-scene, clears markers so the full animatic range is used.
  void updateAnimaticFrameMarkers();
  // Called when FlipConsole play starts: overrides m_sound built by base
  // onPlayingStatusChanged() (which reads from the wrong sub-scene xsheet).
  void onAnimaticPlayingStatusChanged(bool playing);

private:
  // Rebuilds m_sound from the main xsheet (not the current/sub-scene xsheet).
  void refreshAnimaticSound();
  // Like BaseViewerPanel::playAudioFrame but uses ctrl->mainXsheet()->play()
  // instead of TApp::getCurrentXsheet()->getXsheet()->play().
  void playAnimaticAudioFrame(int frame);

  // Tracks ctrl-handle connections so they aren't duplicated across show/hide.
  QMetaObject::Connection m_frameRangeConn;
  QMetaObject::Connection m_audioConn;
};

// ---- ZtoryAnimaticViewerPanel ----
// Standalone TPanel wrapper for ZtoryAnimaticViewer
class ZtoryAnimaticViewerPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryAnimaticViewerPanel(QWidget *parent = nullptr);

protected:
  void showEvent(QShowEvent *e) override;

private:
  ZtoryAnimaticViewer *m_viewer;
};

// ---- ZtoryAnimaticPanel ----
// Timeline panel: toolbar + ruler + track + audio tracks
class ZtoryAnimaticPanel : public TPanel {
  Q_OBJECT
public:
  ZtoryAnimaticPanel(QWidget *parent = nullptr);
  void refreshFromScene();
protected:
  void showEvent(QShowEvent *e) override;

private slots:
  void onShotClicked(int col);
  void onShotDoubleClicked(int col);
  void onShotDurationChanged(int col, int newF1);
  void onRazorRequested(int col, int splitFrame);
  void onShotMoved(int col, int newStartFrame);
  void onMergeShots();
  void resequenceXsheet();
  void onZoomChanged(double ppf);
  void onMatchSubsceneDuration(int col);
  void onFrameChanged(int frame);

public:
  void refreshAudioTracks();

protected:
  void contextMenuEvent(QContextMenuEvent *e) override;

private:
  ZtoryAnimaticRuler *m_ruler;
  ZtoryAnimaticTrack *m_track;
  QWidget *m_scrollContent = nullptr;
  QVBoxLayout *m_scrollLay = nullptr;
  QList<ZtoryAudioTrack *> m_audioTracks;
  QSlider *m_zoomSlider = nullptr;
  bool m_audioLinked = true;
  double m_ppf = 8.0;
};

#endif
