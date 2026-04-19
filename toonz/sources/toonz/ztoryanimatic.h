#pragma once
#ifndef ZTORYANIMATIC_H
#define ZTORYANIMATIC_H

#include "viewerpane.h"
#include "tapp.h"
#include "pane.h"
#include "ztorymodel.h"
#include "toonz/onionskinmask.h"
#include <QWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QLabel>
#include <QVBoxLayout>
#include <QSlider>
#include <QHBoxLayout>
#include <set>
#include <QKeyEvent>
#include <QMap>
#include <QSet>

// ---- ZtoryAnimaticController ----
// Singleton that owns the dedicated frame state for the animatic timeline
// and viewer, isolating them from TApp's global TFrameHandle.
class TFrameHandle;
class ZtoryAnimaticViewer;
#include "tsound.h"

class ZtoryAnimaticController : public QObject {
  Q_OBJECT
public:
  static ZtoryAnimaticController *instance();
  TFrameHandle *frameHandle() const { return m_frameHandle; }
  TXsheet *mainXsheet() const;
  void setCurrentFrame(int frame);
  int currentFrame() const;
  // Cached merged sound track — filled lazily on first scrub / on play start.
  // Call invalidateSoundTrack() after any ColumnLevel shift to force rebuild.
  TSoundTrackP soundTrack() const { return m_soundTrack; }
  void setSoundTrack(TSoundTrackP st) { m_soundTrack = st; }
  void invalidateSoundTrack() {
    m_soundTrack        = TSoundTrackP();
    m_soundBuildPending = false;  // Allow a fresh async build
  }
  // Viewer registers itself so the panel can call restartAudioIfPlaying().
  void setViewer(ZtoryAnimaticViewer *v) { m_viewer = v; }
  ZtoryAnimaticViewer *viewer() const { return m_viewer; }
  // Returns true when the animatic viewer is active at the main level,
  // meaning it owns audio and the native ComboViewer must not compete.
  bool ownsAudioAtMainLevel() const;

  // Build (or return cached) merged track from the main xsheet.
  // Safe to call from any scrub handler — returns null if no audio.
  TSoundTrackP requireSoundTrack();

  // Start an async background build of the merged sound track.
  // Result is delivered to m_soundTrack via QueuedConnection (main thread).
  // No-op if a build is already in progress or the track is already cached.
  void preBuildSoundTrackAsync();

  // Animatic-owned play range — independent from scene->getPreviewProperties()
  // which is shared with (and overwritten by) the native xsheet viewer when
  // entering/leaving sub-scenes.  The ruler and animatic marker logic read
  // this instead of XsheetGUI::getPlayRange().
  // Sets animatic play range AND mirrors to XsheetGUI (native viewer sync).
  void setAnimaticPlayRange(int r0, int r1);
  void getAnimaticPlayRange(int &r0, int &r1) const { r0 = m_animaticR0; r1 = m_animaticR1; }
  bool isAnimaticPlayRangeEnabled() const { return m_animaticR0 <= m_animaticR1; }

public slots:
  // Called by the ruler whenever the In/Out play range changes, so the
  // animatic viewer's FlipConsole markers stay in sync.
  void notifyPlayRangeChanged() { emit playRangeChanged(); }

private slots:
  // Fired by TApp::getCurrentFrame()->isPlayingStatusChanged.
  // When the native viewer starts playing inside a shot sub-scene, this
  // starts the main-xsheet audio at the correct time offset so the animator
  // can hear the soundtrack while working on animation to picture.
  void onNativePlayingStatusChanged();
  // Fired by TApp::getCurrentFrame()->frameSwitched.
  // Provides per-frame scrub audio from the main xsheet while the user
  // drags the playhead inside a shot sub-scene (not during continuous play).
  void onNativeFrameSwitched();

signals:
  void playRangeChanged();

private:
  ZtoryAnimaticController();
  TFrameHandle         *m_frameHandle;
  TSoundTrackP          m_soundTrack;
  int m_animaticR0 = 0;
  int m_animaticR1 = -1;  // -1 = no range set (full range)
  ZtoryAnimaticViewer  *m_viewer = nullptr;
  // True while we are streaming main-xsheet audio on behalf of the native viewer.
  bool m_nativeAudioPlaying  = false;
  // Guards against launching a second async build while one is already running.
  bool m_soundBuildPending   = false;
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
  // Razor hover: set the absolute frame under the cursor (or -1 to clear).
  // Also called by the panel to sync the hover position across tracks.
  void setRazorHoverFrame(int frame);
  // Returns the blocks vector (for panel to read cut frame positions).
  const std::vector<ShotBlock> &blocks() const { return m_blocks; }
  const std::set<int> &selectedCols() const { return m_selectedCols; }
  // Lock — blocks drag/resize on the video track
  bool isLocked() const { return m_locked; }
  void setLocked(bool on);

protected:
  void paintEvent(QPaintEvent *) override;
  void wheelEvent(QWheelEvent *e) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void mouseDoubleClickEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;

signals:
  void shotClicked(int col);
  void shotDoubleClicked(int col);
  void selectionChanged(std::set<int> selectedCols);
  void shotDurationChanged(int col, int newF1);
  void shotMoved(int col, int newStartFrame);
  void zoomChanged(double ppf);
  void matchSubsceneDuration(int col);
  void razorRequested(int col, int splitFrame);
  void mergeWithNextRequested(int col);
  void returnToMainRequested();
  // Emitted on mouse move when razor is active — absolute frame under cursor,
  // or -1 when the mouse leaves. Panel forwards this to audio tracks.
  void razorHoverFrameChanged(int frame);
  void lockedChanged(bool on);

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
  int m_razorHoverFrame = -1;
  // Lock button painted in paintEvent, toggled via mousePressEvent hit-test
  bool m_locked = false;

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
  // Abort any in-progress drag without committing the move.
  // Call before deleting the widget to prevent stale drag state from firing.
  void cancelDrag() { m_dragMode = NoDrag; m_draggingPreview = false; }
  int trackHeight() const { return m_trackHeight; }
  void setTrackHeight(int h) { m_trackHeight = h; m_waveformDirty = true; setFixedHeight(h); update(); }
  int columnIndex() const { return m_col; }
  void setRazorActive(bool on);
  // Cut frame markers — drawn as bright separator lines in the waveform.
  // Set by the panel after every shot change; frame indices are absolute
  // (same coordinate space as the audio column).
  void setCutFrames(const QVector<int> &frames);
  // Razor hover: absolute frame under the razor cursor, or -1 to clear.
  void setRazorHoverFrame(int frame);

  // Audio segment: a contiguous range of non-empty cells in this column
  struct Segment { int r0; int r1; };  // inclusive frame range
  std::vector<Segment> findSegments() const;

  // Selection
  bool hasSelection() const { return m_selSeg.r0 >= 0; }
  Segment selectedSegment() const { return m_selSeg; }

  // Clipboard (shared across all audio tracks)
  static void clipboardCut(ZtoryAudioTrack *src);
  static void clipboardCopy(ZtoryAudioTrack *src);
  static void clipboardPaste(ZtoryAudioTrack *dst, int frame);

  int frameAtX(int x) const;

  // Lock / Mute / Solo — driven by real QToolButton children
  bool isLocked() const { return m_locked; }
  bool isMuted()  const { return m_muted; }
  bool isSolo()   const { return m_solo; }
  void setLocked(bool on);
  void setMuted(bool on);
  void setSolo(bool on);
  // Called by the panel after computing effective mute (solo logic).
  // Dims waveform visually without corrupting the user's m_muted state.
  void setEffectiveMuted(bool on) { m_effectiveMuted = on; update(); }

signals:
  void razorRequested(int col, int frame);
  void segmentMoved();
  void segmentDroppedOutside(int srcCol, int origR0, int origR1, int dragOffset, QPoint globalPos);
  void lockedChanged(int col, bool on);
  void muteToggleRequested(int col);
  void soloToggleRequested(int col);

protected:
  void paintEvent(QPaintEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void leaveEvent(QEvent *) override;
  void keyPressEvent(QKeyEvent *) override;

private:
  // L/M/S buttons are drawn in paintEvent and clicked via hit-test in mousePressEvent

  int m_col;
  QString m_name;
  double m_ppf = 8.0;
  int m_currentFrame = 0;
  int m_trackHeight = 50;
  QPixmap m_waveformCache;
  bool m_waveformDirty = true;
  // Preview bar
  int  m_previewR0        = -1;
  int  m_previewR1        = -1;
  bool m_draggingPreview  = false;
  int  m_previewDragStart = -1;
  bool m_razorActive      = false;
  int  m_razorHoverFrame  = -1;
  QVector<int> m_cutFrames;
  // Segment selection & drag
  Segment m_selSeg{-1, -1};
  enum DragMode { NoDrag, SegmentDrag, TrimLeft, TrimRight };
  DragMode m_dragMode     = NoDrag;
  int  m_dragStartFrame   = -1;
  int  m_dragOrigR0       = -1;
  int  m_dragOrigR1       = -1;
  // L/M/S state — toggled via mousePressEvent, rendered in paintEvent
  bool m_locked = false;
  bool m_muted  = false;
  bool m_solo   = false;
  // Set by applyMuteSolo() to dim the waveform when another track is solo'd.
  // Separate from m_muted so user state is never corrupted.
  bool m_effectiveMuted = false;
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

  // Set up minimal title-bar buttons for the animatic viewer:
  // Camera Stand View, Camera View, and Preview.
  // Called by ZtoryAnimaticViewerPanel after construction.
  void initializeAnimaticTitleBar(TPanelTitleBar *titleBar);

  // Called by the panel after applyMuteSolo() — if playback is in progress,
  // stops and restarts audio so the new mix takes effect immediately.
  void restartAudioIfPlaying();

  // Schedule an audio restart on the next onDrawFrame tick.  Safe to call
  // from any slot (including button-click handlers) because onDrawFrame runs
  // between CoreAudio XPC callbacks.
  void requestAudioRestart() { m_pendingAudioRestart = true; }

  // Override: write frame to controller's handle, NOT TApp::getCurrentFrame().
  // Base implementation always uses the global handle → during play it would
  // advance the sub-scene's frame instead of the animatic frame.
  void onDrawFrame(int frame,
                   const ImagePainter::VisualSettings &settings,
                   QElapsedTimer *timer     = nullptr,
                   qint64 targetInstant     = 0) override;

  // Override: always read markers from the main xsheet play range.
  // Base implementation reads from scene->getProperties() but the base's
  // onSceneChanged() calls this after xsheetChanged — which fires when entering
  // a sub-scene — and would overwrite whatever updateAnimaticFrameMarkers set.
  void updateFrameMarkers() override;

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
  // Per-frame audio for scrubbing only (not during play — play uses continuous
  // streaming started in onAnimaticPlayingStatusChanged).
  void playAnimaticAudioFrame(int frame);

  // Keeps the merged sound track alive as long as the viewer may use m_sound.
  // m_sound (base class raw ptr) must not outlive the TSoundTrack object.
  // By holding our own ref here, we guarantee the object stays alive until
  // the next refreshAnimaticSound() call replaces it — even if the controller
  // invalidates its own cache in the meantime.
  TSoundTrackP m_soundTrackRef;

  // True while the full-track continuous play is active.
  // When true, playAnimaticAudioFrame is a no-op (audio already streaming).
  bool m_continuousPlay = false;

  // 0-based animatic frame at which the current play session started.
  // Used by onDrawFrame to compute the audio-master target frame.
  int m_playStartFrame = 0;

  // Previous FlipConsole frame (1-based) seen by onDrawFrame.
  // Used to detect loop-back (frame drops below previous value).
  int m_prevFlipFrame = 0;

  // Set by applyMuteSolo() when audio needs to be restarted (mute/solo changed
  // during playback).  Checked at the top of onDrawFrame, which runs on the
  // main thread between CoreAudio XPC callbacks — safe to call stopScrub/play.
  bool m_pendingAudioRestart = false;

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
  void keyPressEvent(QKeyEvent *e) override;
  bool eventFilter(QObject *obj, QEvent *e) override;

private slots:
  void onCopyShots();   // Cmd+C: store selected shots to animatic clipboard
  void onCutShots();    // Cmd+X: store + mark for deletion on paste
  void onPasteShots();  // Cmd+V: clone clipboard shots, delete cut originals
  void onDeleteShots(); // Delete/Backspace: remove selected shots
  void onCloneShots();  // Cmd+D: duplicate (clone) selected shots
  void onShotClicked(int col);
  void onShotDoubleClicked(int col);
  void onReturnToMain();
  void onShotDurationChanged(int col, int newF1);
  void onRazorRequested(int col, int splitFrame);
  void onShotMoved(int col, int newStartFrame);
  void onMergeShots();
  void onMergeWithNext(int col);
  void onAddShot();
  void resequenceXsheet();
  void onZoomChanged(double ppf);
  void onMatchSubsceneDuration(int col);
  void onFrameChanged(int frame);
  void onAudioRazorRequested(int col, int frame);
  void onSegmentDroppedOutside(int srcCol, int origR0, int origR1, int dragOffset, QPoint globalPos);

public:
  void refreshAudioTracks();
  void updateTrackWidths();
  void updateCutFrames();

protected:
  void contextMenuEvent(QContextMenuEvent *e) override;

private:
  // ── Clipboard per Cmd+C/X/V ──────────────────────────────────────────────
  // Clipboard is now shared with StoryboardPanel via ZtoryModel::sharedClip().

  ZtoryAnimaticRuler *m_ruler;
  ZtoryAnimaticTrack *m_track;
  QWidget *m_scrollContent = nullptr;
  QVBoxLayout *m_scrollLay = nullptr;
  QList<ZtoryAudioTrack *> m_audioTracks;
  QSlider *m_zoomSlider = nullptr;
  bool m_audioLinked = true;
  double m_ppf = 8.0;
  bool m_refreshing = false;      // re-entrancy guard for refreshFromScene
  bool m_refreshingAudio = false; // re-entrancy guard for refreshAudioTracks
  // Per-column mute/solo/lock state — persists across refreshAudioTracks() rebuilds
  QMap<int, bool> m_colMuted;
  QSet<int>       m_colSolo;
  QMap<int, bool> m_colLocked;
  // Apply effective mute/solo volumes to all audio columns and rebuild soundtrack
  void applyMuteSolo();
  // Restore muted/solo/locked state onto freshly-created track widgets
  void restoreTrackStates();
};

#endif
