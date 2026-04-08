
#include "ztoryanimatic.h"
#include "ztorymodel.h"
#include "toonz/tcolumnhandle.h"
#include "subscenecommand.h"
#include "toonzqt/menubarcommand.h"
#include "columncommand.h"
#include "tapp.h"
#include "toonz/toonzscene.h"
#include "toonz/childstack.h"
#include "toonz/txsheet.h"
#include "toonz/txshchildlevel.h"
#include "toonz/txshcell.h"
#include "toonz/txshsoundcolumn.h"
#include "toonz/txshsoundlevel.h"
#include "toonz/tframehandle.h"
#include "toonz/txsheethandle.h"
#include "toonz/tscenehandle.h"
#include "toonz/onionskinmask.h"
#include "toonz/tonionskinmaskhandle.h"
#include "iocommand.h"
#include "xsheetdragtool.h"
#include "toonz/sceneproperties.h"
#include "toutputproperties.h"
#include "toonzqt/gutil.h"
#include "toonzqt/icongenerator.h"
#include "toonzqt/dvdialog.h"
#include "orientation.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QSplitter>
#include <QMenu>
#include <QLabel>
#include <QFileDialog>
#include <QContextMenuEvent>
#include "tsound.h"
#include "toonz/tstageobject.h"
#include "toonz/tstageobjecttree.h"
#include "toonz/tstageobjectid.h"
#include "toonz/txshleveltypes.h"
#include "toonz/tcamera.h"

// Shared label column width — must match ZtoryAudioTrack::labelW (80px).
// Used by ZtoryAnimaticRuler and ZtoryAnimaticTrack to align with audio tracks.
static constexpr int kLabelW = 80;


// ---- ZtoryAnimaticController ----

ZtoryAnimaticController::ZtoryAnimaticController() : QObject() {
  m_frameHandle = new TFrameHandle();
  m_frameHandle->setFrame(0);
  // Monitor native-viewer play state so we can stream main-xsheet audio
  // when the user is editing a shot (sub-scene open) and presses play.
  // The signal fires on the main thread, so this is safe to call from here.
  connect(TApp::instance()->getCurrentFrame(),
          &TFrameHandle::isPlayingStatusChanged,
          this, &ZtoryAnimaticController::onNativePlayingStatusChanged);
  connect(TApp::instance()->getCurrentFrame(),
          &TFrameHandle::frameSwitched,
          this, &ZtoryAnimaticController::onNativeFrameSwitched);
}

ZtoryAnimaticController *ZtoryAnimaticController::instance() {
  static ZtoryAnimaticController ctrl;
  return &ctrl;
}

TXsheet *ZtoryAnimaticController::mainXsheet() const {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return nullptr;
  return scene->getChildStack()->getTopXsheet();
}

// Helper: frame count from VIDEO columns only (ignores sound columns).
// mainXsh->getFrameCount() includes audio columns; after a razor cut the
// trailing ColumnLevel keeps endOffset=0 (= full raw file length), inflating
// the count far beyond the actual video range.
static int videoFrameCount(TXsheet *xsh) {
  if (!xsh) return 1;
  int maxFrame = 0;
  for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshColumn *col = xsh->getColumn(c);
    if (!col || col->getSoundColumn()) continue;
    int r0, r1;
    if (col->getRange(r0, r1)) maxFrame = std::max(maxFrame, r1 + 1);
  }
  return maxFrame > 0 ? maxFrame : 1;
}

void ZtoryAnimaticController::setCurrentFrame(int frame) {
  if (frame < 0) frame = 0;
  m_frameHandle->setFrame(frame);
}

int ZtoryAnimaticController::currentFrame() const {
  return m_frameHandle->getFrame();
}

TSoundTrackP ZtoryAnimaticController::requireSoundTrack() {
  if (m_soundTrack) return m_soundTrack;
  TXsheet *xsh = mainXsheet();
  if (!xsh) return TSoundTrackP();
  try {
    TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
    prop->m_isPreview = true;
    // CRITICAL: set explicit frame range using video columns only.
    // Default fromFrame/toFrame = -1,-1 cause mixingTogether() to use
    // xsh->getFrameCount() as toFrame.  After an audio razor cut the trailing
    // ColumnLevel keeps endOffset=0 (raw file length), inflating getFrameCount()
    // to millions of frames.  mixingTogether() then allocates a massive buffer
    // (hundreds of MB) → heap corruption → crash.
    prop->m_fromFrame = 0;
    prop->m_toFrame   = videoFrameCount(xsh) - 1;
    m_soundTrack = TSoundTrackP(xsh->makeSound(prop));
  } catch (...) {
    m_soundTrack = TSoundTrackP();
  }
  return m_soundTrack;
}

// ---- ZtoryAnimaticController::onNativePlayingStatusChanged ----
// Called whenever the NATIVE viewer (ComboViewer / FlipConsole) starts or
// stops playback via TApp::getCurrentFrame()->isPlayingStatusChanged.
//
// Goal: when the user is editing a shot (sub-scene open, ancestorCount == 1)
// and presses play, stream the main-xsheet audio at the correct offset so
// they can hear the soundtrack while animating to picture.
//
// Guard: if the animatic viewer is already playing it handles its own audio
// (continuous-play mode) — we must not start a second concurrent stream.
void ZtoryAnimaticController::onNativePlayingStatusChanged() {
  TFrameHandle *fh     = TApp::instance()->getCurrentFrame();
  TXsheet      *mainXsh = mainXsheet();

  if (!fh->isPlaying()) {
    // Playback stopped — stop background audio if we started it.
    if (m_nativeAudioPlaying && mainXsh) mainXsh->stopScrub();
    m_nativeAudioPlaying = false;
    return;
  }

  // Animatic viewer is already handling audio in continuous-play mode.
  if (m_frameHandle->isPlaying()) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;

  ChildStack *cs = scene->getChildStack();
  // Only handle a single level of nesting (main xsheet → shot sub-scene).
  // If ancestorCount == 0 the user is at the top level; FlipConsole already
  // handles audio via the normal BaseViewerPanel path.
  // If ancestorCount > 1 the user is in a nested sub-scene — skip for now.
  if (!cs || cs->getAncestorCount() != 1) return;

  TSoundTrackP st = requireSoundTrack();
  if (!st || !mainXsh) return;

  // Map the current sub-scene frame to its position in the main xsheet.
  int subFrame = fh->getFrame();
  std::pair<TXsheet *, int> ancestor = cs->getAncestor(subFrame);
  int mainFrame = ancestor.second;

  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  double spf = st->getSampleRate() / fps;

  TINT32 startSample = (TINT32)(mainFrame * spf);
  TINT32 totalSamples = (TINT32)st->getSampleCount();
  int    animFrames   = videoFrameCount(mainXsh);
  TINT32 endSample    = std::min((TINT32)(animFrames * spf), totalSamples - 1);

  if (startSample > endSample) return;

  mainXsh->play(st.getPointer(), startSample, endSample, false);
  m_nativeAudioPlaying = true;
}

// ---- ZtoryAnimaticController::onNativeFrameSwitched ----
// Provides per-frame scrub audio from the main xsheet when the user drags
// the playhead inside a shot sub-scene.  Mirrors the per-frame scrub logic
// in ZtoryAnimaticViewer::playAnimaticAudioFrame but uses the native frame
// handle and maps the sub-scene frame through ChildStack::getAncestor().
void ZtoryAnimaticController::onNativeFrameSwitched() {
  TFrameHandle *fh = TApp::instance()->getCurrentFrame();
  // During continuous play the streaming audio already handles this.
  if (fh->isPlaying()) return;
  // Animatic viewer active — it handles its own scrub audio.
  if (m_frameHandle->isPlaying()) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  ChildStack *cs = scene->getChildStack();
  if (!cs || cs->getAncestorCount() != 1) return;

  TSoundTrackP st = requireSoundTrack();
  TXsheet *mainXsh = mainXsheet();
  if (!st || !mainXsh) return;

  int subFrame = fh->getFrame();
  std::pair<TXsheet *, int> ancestor = cs->getAncestor(subFrame);
  int mainFrame = ancestor.second;

  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  double spf = st->getSampleRate() / fps;

  TINT32 s0 = (TINT32)(mainFrame * spf);
  TINT32 s1 = (TINT32)(s0 + spf);
  TINT32 totalSamples = (TINT32)st->getSampleCount();
  if (s0 >= totalSamples) return;
  if (s1 >= totalSamples) s1 = totalSamples - 1;

  mainXsh->play(st.getPointer(), s0, s1, false);
}

// ---- ZtoryAnimaticRuler ----

ZtoryAnimaticRuler::ZtoryAnimaticRuler(QWidget *parent) : QWidget(parent) {
  setFixedHeight(36);
  setMouseTracking(true);
  m_localMask.setRelativeFrameMode(true); // MOS = relative offsets, FOS = fixed frames
}

void ZtoryAnimaticRuler::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);
  const int h = height();  // 36px

  // Strip heights: 9px top (FOS), 9px bottom (MOS), 18px middle (ruler)
  static const int kFosH = 9;
  static const int kMosH = 9;
  const int rulerY  = kFosH;
  const int rulerH  = h - kFosH - kMosH;
  const int mosY    = h - kMosH;

  // Full background
  p.fillRect(rect(), QColor(40, 40, 40));

  // FOS strip (top) + MOS strip (bottom) — slightly darker
  p.fillRect(kLabelW, 0,    width() - kLabelW, kFosH, QColor(28, 28, 28));
  p.fillRect(kLabelW, mosY, width() - kLabelW, kMosH, QColor(28, 28, 28));
  // Strip labels in label area
  p.fillRect(0, 0, kLabelW, h, QColor(25, 25, 25));
  p.setPen(QColor(100, 100, 100));
  p.setFont(QFont("Arial", 7));
  p.drawText(QRect(0, 0, kLabelW - 2, kFosH), Qt::AlignRight | Qt::AlignVCenter, "F");
  p.drawText(QRect(0, mosY, kLabelW - 2, kMosH), Qt::AlignRight | Qt::AlignVCenter, "R");
  // Strip separator lines
  p.setPen(QColor(55, 55, 55));
  p.drawLine(0, kFosH, width(), kFosH);
  p.drawLine(0, mosY,  width(), mosY);
  // Label-area right border
  p.setPen(QColor(60, 60, 60));
  p.drawLine(kLabelW, 0, kLabelW, h);

  // ---- In/Out range highlight (middle ruler zone only) ----
  int r0 = 0, r1 = 0, step = 1;
  bool rangeEnabled = XsheetGUI::getPlayRange(r0, r1, step);
  if (!rangeEnabled) {
    TXsheet *mainXsh = ZtoryAnimaticController::instance()->mainXsheet();
    r0 = 0;
    r1 = mainXsh ? std::max(0, videoFrameCount(mainXsh) - 1) : 0;
  }
  if (r1 >= r0) {
    int x0 = kLabelW + (int)(r0 * m_ppf);
    int x1 = kLabelW + (int)((r1 + 1) * m_ppf);
    p.fillRect(x0, rulerY, x1 - x0, rulerH,
               QColor(255, 165, 0, rangeEnabled ? 45 : 20));
  }

  // ---- Tick marks (middle zone) ----
  p.setFont(QFont());
  p.setPen(QColor(180, 180, 180));
  int w = width() - kLabelW;
  int tickEvery = 1;
  if (m_ppf < 4)       tickEvery = 24;
  else if (m_ppf < 12) tickEvery = 6;
  for (int f = 0; f * m_ppf < w; f++) {
    int x = kLabelW + (int)(f * m_ppf);
    if (f % 24 == 0) {
      p.drawLine(x, rulerY, x, rulerY + 12);
      p.drawText(x + 2, rulerY + 11, QString::number(f));
    } else if (f % tickEvery == 0) {
      p.drawLine(x, rulerY + 6, x, rulerY + 12);
    }
  }

  // ---- In/Out triangular markers (middle zone top edge) ----
  static const int kM = 8;
  if (r1 >= r0) {
    int x0 = kLabelW + (int)(r0 * m_ppf);
    int x1 = kLabelW + (int)((r1 + 1) * m_ppf);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 200, 0));
    QPolygon inTri;
    inTri << QPoint(x0, rulerY) << QPoint(x0 + kM, rulerY) << QPoint(x0, rulerY + kM);
    p.drawConvexPolygon(inTri);
    QPolygon outTri;
    outTri << QPoint(x1, rulerY) << QPoint(x1 - kM, rulerY) << QPoint(x1, rulerY + kM);
    p.drawConvexPolygon(outTri);
  }

  // ---- Playhead — downward triangle + line (middle zone) ----
  // Centered on the frame column (+ half-frame offset) so the triangle tip
  // points to the middle of the frame, not its left edge.
  static const int kPH = 8;
  int px = kLabelW + (int)(m_currentFrame * m_ppf) + (int)(m_ppf / 2);
  p.setPen(Qt::NoPen);
  p.setBrush(QColor(255, 100, 0));
  QPolygon tri;
  tri << QPoint(px - 5, rulerY) << QPoint(px + 5, rulerY) << QPoint(px, rulerY + kPH);
  p.drawConvexPolygon(tri);
  p.setPen(QPen(QColor(255, 100, 0), 1));
  p.drawLine(px, rulerY + kPH, px, mosY);

  // ---- Onion skin markers ----
  if (m_onionEnabled) {
    p.setPen(Qt::NoPen);

    // FOS strip — fixed-frame dots (orange), top strip
    for (int i = 0; i < m_localMask.getFosCount(); i++) {
      int frame = m_localMask.getFos(i);
      int ox = kLabelW + (int)(frame * m_ppf) + (int)(m_ppf / 2);
      p.setBrush(QColor(255, 165, 0));
      p.drawEllipse(QPoint(ox, kFosH / 2), 4, 4);
    }

    // MOS strip — relative dots (red=past, blue=future), bottom strip.
    // A line connects each dot back to the playhead, matching the native
    // timeline's visual style that shows the relative distance at a glance.
    for (int i = 0; i < m_localMask.getMosCount(); i++) {
      int rel   = m_localMask.getMos(i);
      int frame = m_currentFrame + rel;
      if (frame < 0) continue;
      int ox = kLabelW + (int)(frame * m_ppf) + (int)(m_ppf / 2);
      QColor mc = (rel < 0) ? QColor(255, 100, 100) : QColor(100, 150, 255);
      // Connecting line from playhead to this MOS dot
      p.setPen(QPen(mc.darker(130), 1));
      p.drawLine(px, mosY + kMosH / 2, ox, mosY + kMosH / 2);
      p.setPen(Qt::NoPen);
      p.setBrush(mc);
      p.drawEllipse(QPoint(ox, mosY + kMosH / 2), 4, 4);
    }
  }

  // ---- Hover feedback ----
  if (m_hoverFrame >= 0 && m_hoverZone != HoverNone) {
    int ox = kLabelW + (int)(m_hoverFrame * m_ppf) + (int)(m_ppf / 2);
    p.setPen(Qt::NoPen);
    if (m_hoverZone == HoverFOS) {
      bool exists = m_localMask.isFos(m_hoverFrame);
      // Red hint if removing, orange hint if adding
      p.setBrush(exists ? QColor(255, 60, 60, 160) : QColor(255, 165, 0, 100));
      p.drawEllipse(QPoint(ox, kFosH / 2), 5, 5);
    } else { // HoverMOS
      int rel = m_hoverFrame - m_currentFrame;
      if (rel != 0) {
        bool exists = m_localMask.isMos(rel);
        QColor mc = exists ? QColor(255, 60, 60, 160)
                   : (rel < 0 ? QColor(255, 100, 100, 100)
                              : QColor(100, 150, 255, 100));
        p.setBrush(mc);
        p.drawEllipse(QPoint(ox, mosY + kMosH / 2), 5, 5);
      }
    }
  }
}

void ZtoryAnimaticRuler::mousePressEvent(QMouseEvent *e) {
  if (e->button() != Qt::LeftButton) return;

  const int my = e->y();
  const int h  = height();
  static const int kFosH = 9;
  static const int kMosH = 9;
  const int mosY = h - kMosH;

  int mx    = qMax(0, e->x() - kLabelW);
  int frame = (int)(mx / m_ppf);

  // ── FOS strip (top): click toggles a fixed-frame onion skin point ──────
  if (my < kFosH) {
    if (!m_onionEnabled) { setOnionEnabled(true); emit onionEnabledChanged(true); }
    m_localMask.setFos(frame, !m_localMask.isFos(frame));
    syncOnionToGlobal();
    update();
    return;
  }

  // ── MOS strip (bottom): click toggles a relative onion skin point ───────
  if (my >= mosY) {
    int rel = frame - m_currentFrame;
    if (rel != 0) {
      if (!m_onionEnabled) { setOnionEnabled(true); emit onionEnabledChanged(true); }
      m_localMask.setMos(rel, !m_localMask.isMos(rel));
      syncOnionToGlobal();
      update();
    }
    return;
  }

  // ── Middle ruler zone ────────────────────────────────────────────────────
  // Shift+click = set In, Alt+click = set Out
  if (e->modifiers() & Qt::ShiftModifier) {
    int r0, r1, step;
    if (!XsheetGUI::getPlayRange(r0, r1, step)) { r0 = frame; r1 = frame; step = 1; }
    XsheetGUI::setPlayRange(frame, std::max(frame, r1), step);
    ZtoryAnimaticController::instance()->notifyPlayRangeChanged();
    update();
    return;
  }
  if (e->modifiers() & Qt::AltModifier) {
    int r0, r1, step;
    if (!XsheetGUI::getPlayRange(r0, r1, step)) { r0 = frame; r1 = frame; step = 1; }
    XsheetGUI::setPlayRange(std::min(r0, frame), frame, step);
    ZtoryAnimaticController::instance()->notifyPlayRangeChanged();
    update();
    return;
  }

  // Hit-test In/Out markers for drag (8px tolerance)
  static const int kM = 8;
  m_dragMode = None;
  if (XsheetGUI::isPlayRangeEnabled()) {
    int r0, r1, step;
    XsheetGUI::getPlayRange(r0, r1, step);
    int x0 = (int)(r0 * m_ppf);
    int x1 = (int)((r1 + 1) * m_ppf);  // Out marker is at right edge of frame r1
    if (std::abs(mx - x0) <= kM) { m_dragMode = DragIn;  return; }
    if (std::abs(mx - x1) <= kM) { m_dragMode = DragOut; return; }
  }

  // Plain click: move playhead
  if (e->modifiers() == Qt::NoModifier) {
    m_currentFrame = frame;
    update();
    emit frameChanged(m_currentFrame);
    // Audio scrub — requireSoundTrack() builds the track lazily if not cached
    auto *ctrl = ZtoryAnimaticController::instance();
    TSoundTrackP st = ctrl->requireSoundTrack();
    TXsheet *xsh = ctrl->mainXsheet();
    if (st && xsh) {
      ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
      double fps = (sc && m_fps <= 0)
                   ? sc->getProperties()->getOutputProperties()->getFrameRate()
                   : (m_fps > 0 ? m_fps : 24.0);
      TINT32 sr = st->getSampleRate();
      TINT32 s0 = qBound((TINT32)0, (TINT32)(m_currentFrame * sr / fps),
                         st->getSampleCount() - 1);
      TINT32 s1 = qBound(s0 + 1, (TINT32)((m_currentFrame + 1) * sr / fps),
                         st->getSampleCount());
      if (s0 < s1) xsh->play(st, s0, s1, false);
    }
  }
}

void ZtoryAnimaticRuler::mouseMoveEvent(QMouseEvent *e) {
  // Hover feedback (no button required)
  {
    const int my = e->y();
    const int h  = height();
    static const int kFosH = 9;
    static const int kMosH = 9;
    int newFrame = (int)(qMax(0, e->x() - kLabelW) / m_ppf);
    HoverZone newZone = HoverNone;
    if (my < kFosH)               newZone = HoverFOS;
    else if (my >= h - kMosH)     newZone = HoverMOS;
    if (newFrame != m_hoverFrame || newZone != m_hoverZone) {
      m_hoverFrame = newFrame;
      m_hoverZone  = newZone;
      // Cursor hint: cross when over a strip, default otherwise
      setCursor(newZone != HoverNone ? Qt::CrossCursor : Qt::ArrowCursor);
      update();
    }
  }

  if (!(e->buttons() & Qt::LeftButton)) return;
  int mx = qMax(0, e->x() - kLabelW);
  int frame = (int)(mx / m_ppf);

  if (m_dragMode == DragIn) {
    int r0, r1, step;
    XsheetGUI::getPlayRange(r0, r1, step);
    XsheetGUI::setPlayRange(std::min(frame, r1), r1, step);
    ZtoryAnimaticController::instance()->notifyPlayRangeChanged();
    update();
    return;
  }
  if (m_dragMode == DragOut) {
    int r0, r1, step;
    XsheetGUI::getPlayRange(r0, r1, step);
    XsheetGUI::setPlayRange(r0, std::max(frame, r0), step);
    ZtoryAnimaticController::instance()->notifyPlayRangeChanged();
    update();
    return;
  }

  m_currentFrame = frame;
  update();
  emit frameChanged(m_currentFrame);
  // Audio scrub (12b) — requireSoundTrack() builds lazily if not cached yet
  auto *ctrl = ZtoryAnimaticController::instance();
  TSoundTrackP st = ctrl->requireSoundTrack();
  TXsheet *xsh = ctrl->mainXsheet();
  if (st && xsh) {
    ToonzScene *sc = TApp::instance()->getCurrentScene()->getScene();
    double fps = (sc) ? sc->getProperties()->getOutputProperties()->getFrameRate() : (m_fps > 0 ? m_fps : 24.0);
    TINT32 sr = st->getSampleRate();
    TINT32 s0 = qBound((TINT32)0, (TINT32)(m_currentFrame * sr / fps), st->getSampleCount()-1);
    TINT32 s1 = qBound(s0+1, (TINT32)((m_currentFrame+1) * sr / fps), st->getSampleCount());
    if (s0 < s1) xsh->play(st, s0, s1, false);
  }
}

void ZtoryAnimaticRuler::mouseReleaseEvent(QMouseEvent *) {
  m_dragMode = None;
  ZtoryAnimaticController::instance()->frameHandle()->stopScrubbing();
}

void ZtoryAnimaticRuler::leaveEvent(QEvent *) {
  m_hoverFrame = -1;
  m_hoverZone  = HoverNone;
  setCursor(Qt::ArrowCursor);
  update();
}

void ZtoryAnimaticRuler::setOnionEnabled(bool on) {
  m_onionEnabled = on;
  syncOnionToGlobal();
  update();
}

void ZtoryAnimaticRuler::syncOnionToGlobal() const {
  // Push local animatic onion skin state to the global handle so the
  // viewer renders the correct onion frames. This does NOT touch the
  // native timeline's conceptual state — the local mask is restored
  // every time the animatic panel becomes visible.
  TOnionSkinMaskHandle *osmh = TApp::instance()->getCurrentOnionSkin();
  if (m_onionEnabled) {
    OnionSkinMask mask = m_localMask;
    mask.enable(true);
    osmh->setOnionSkinMask(mask);
  } else {
    OnionSkinMask off;
    off.enable(false);
    osmh->setOnionSkinMask(off);
  }
  osmh->notifyOnionSkinMaskChanged();
}

void ZtoryAnimaticRuler::initPlayRangeIfNeeded() {
  // Initialise In/Out markers to full range on first show, if not yet set.
  if (XsheetGUI::isPlayRangeEnabled()) {
    int r0, r1, step;
    XsheetGUI::getPlayRange(r0, r1, step);
    if (r0 >= 0) return;  // already set
  }
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  int lastFrame = std::max(0, videoFrameCount(xsh) - 1);
  XsheetGUI::setPlayRange(0, lastFrame, 1);
  ZtoryAnimaticController::instance()->notifyPlayRangeChanged();
  update();
}

void ZtoryAnimaticRuler::contextMenuEvent(QContextMenuEvent *e) {
  int mx = qMax(0, e->x() - kLabelW);
  int frame = (int)(mx / m_ppf);  // frame under cursor, NOT playhead

  QMenu menu(this);
  QAction *inAct    = menu.addAction(tr("Mark IN here"));
  QAction *outAct   = menu.addAction(tr("Mark OUT here"));
  menu.addSeparator();
  QAction *autoAct  = menu.addAction(tr("Set OUT to last frame"));
  QAction *resetAct = menu.addAction(tr("Reset IN/OUT to full range"));

  QAction *chosen = menu.exec(e->globalPos());
  if (chosen == inAct) {
    int r0, r1, step;
    if (!XsheetGUI::getPlayRange(r0, r1, step)) { r0 = 0; r1 = frame; step = 1; }
    XsheetGUI::setPlayRange(frame, std::max(frame, r1), step);
  } else if (chosen == outAct) {
    int r0, r1, step;
    if (!XsheetGUI::getPlayRange(r0, r1, step)) { r0 = 0; r1 = frame; step = 1; }
    XsheetGUI::setPlayRange(std::min(r0, frame), frame, step);
  } else if (chosen == autoAct) {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    int last = std::max(0, videoFrameCount(xsh) - 1);
    int r0, r1, step;
    if (!XsheetGUI::getPlayRange(r0, r1, step)) { r0 = 0; step = 1; }
    XsheetGUI::setPlayRange(r0, last, 1);
  } else if (chosen == resetAct) {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    int last = std::max(0, videoFrameCount(xsh) - 1);
    XsheetGUI::setPlayRange(0, last, 1);
  } else {
    return;  // no action chosen — skip notify
  }
  ZtoryAnimaticController::instance()->notifyPlayRangeChanged();
  update();
}

// ---- ZtoryAudioTrack ----

ZtoryAudioTrack::ZtoryAudioTrack(int col, const QString &name, QWidget *parent)
    : QWidget(parent), m_col(col), m_name(name) {
  setFixedHeight(m_trackHeight);
  setMinimumWidth(100);
  setFocusPolicy(Qt::ClickFocus);

  // L/M/S state is driven purely via paintEvent + mousePressEvent hit-test.
  // No child QToolButton widgets — they don't render reliably inside custom-
  // painted QWidgets on macOS.
}

int ZtoryAudioTrack::frameAtX(int x) const {
  return std::max(0, (int)((x - kLabelW) / m_ppf));
}

void ZtoryAudioTrack::setLocked(bool on) {
  if (m_locked == on) return;
  m_locked = on;
  update();
  emit lockedChanged(m_col, on);
}

void ZtoryAudioTrack::setMuted(bool on) {
  if (m_muted == on) return;
  m_muted = on;
  update();
}

void ZtoryAudioTrack::setSolo(bool on) {
  if (m_solo == on) return;
  m_solo = on;
  update();
}

// Static clipboard for audio segments
static struct {
  int col = -1;
  int r0 = -1, r1 = -1;
  std::vector<TXshCell> cells;
  bool isCut = false;
} s_audioClipboard;

std::vector<ZtoryAudioTrack::Segment> ZtoryAudioTrack::findSegments() const {
  std::vector<Segment> segs;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return segs;
  TXshColumn *column = xsh->getColumn(m_col);
  if (!column) return segs;
  TXshSoundColumn *sc = column->getSoundColumn();
  if (!sc) return segs;
  // Iterate ColumnLevels directly so that razor splits created by
  // splitLevelAtFrame (which share audio data but have distinct visible
  // ranges) are each reported as a separate segment — without losing the
  // frame at the cut boundary the way clearCells() would.
  int n = sc->getColumnLevelCount();
  for (int i = 0; i < n; i++) {
    ColumnLevel *cl = sc->getColumnLevel(i);
    if (!cl) continue;
    segs.push_back({cl->getVisibleStartFrame(), cl->getVisibleEndFrame()});
  }
  return segs;
}

void ZtoryAudioTrack::paintEvent(QPaintEvent *) {
  QPainter p(this);
  const int labelW = kLabelW;
  const int trackW = width() - labelW;
  const int trackH = height();
  const int center = trackH / 2;

  // Sfondo
  p.fillRect(rect(), QColor(25, 25, 25));

  // ---- Label area ----
  p.fillRect(0, 0, labelW, trackH, QColor(35, 35, 35));

  // L/M/S buttons — horizontal row at top of label area
  // Layout: 3 buttons each 22px wide, 2px gap, starting at x=2, y=2
  // Coordinates also used in mousePressEvent hit-test — keep in sync!
  static const int kBtnY  = 2;
  static const int kBtnH  = 16;
  static const int kBtnW  = 22;
  static const int kBtnGap = 3;
  struct BtnInfo { const char *txt; QColor offColor; QColor onColor; bool active; };
  BtnInfo btns[3] = {
    {"L", QColor(100, 65, 20), QColor(220, 140, 50), m_locked},
    {"M", QColor(100, 30, 30), QColor(210, 64, 64),  m_muted},
    {"S", QColor(20, 90, 95),  QColor(50, 190, 200), m_solo},
  };
  p.setFont(QFont("Arial", 8, QFont::Bold));
  for (int i = 0; i < 3; i++) {
    QRect r(2 + i * (kBtnW + kBtnGap), kBtnY, kBtnW, kBtnH);
    p.fillRect(r, btns[i].active ? btns[i].onColor : btns[i].offColor);
    p.setPen(btns[i].active ? QColor(255, 255, 255) : btns[i].onColor);
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.drawText(r, Qt::AlignCenter, btns[i].txt);
  }

  // Track name — below the buttons
  p.setPen(QColor(210, 210, 210));
  p.setFont(QFont("Arial", 8));
  int nameY = kBtnY + kBtnH + 2;
  p.drawText(2, nameY, labelW - 4, trackH - nameY, Qt::AlignVCenter | Qt::AlignLeft, m_name);

  p.setPen(QColor(65, 65, 65));
  p.drawLine(labelW, 0, labelW, trackH);

  // Rebuild waveform cache when ppf or size changed
  if (m_waveformDirty || m_waveformCache.size() != QSize(trackW, trackH)) {
    m_waveformCache = QPixmap(trackW, trackH);
    m_waveformCache.fill(QColor(25, 25, 25));
    m_waveformDirty = false;

    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    TXshColumn *column = xsh ? xsh->getColumn(m_col) : nullptr;
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;

    // Draw per-ColumnLevel waveform. Each CL references the same audio file
    // but may have different startOffset/endOffset (from splits/trims).
    // Reading sample positions directly from each CL avoids any merging
    // artefacts that getOverallSoundTrack() could introduce.
    QPainter cp(&m_waveformCache);
    cp.setPen(QColor(60, 60, 60));
    cp.drawLine(0, center, trackW, center);

    if (sc && sc->getColumnLevelCount() > 0) {
      double fps = 24.0;
      ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
      if (scene)
        fps = scene->getProperties()->getOutputProperties()->getFrameRate();

      // Collect one TSoundTrackP per CL (may be null if file not loaded).
      int nCL = sc->getColumnLevelCount();
      struct CLData {
        ColumnLevel *cl;
        TSoundTrackP track;
        TINT32       sampleRate;
      };
      std::vector<CLData> cldata;
      cldata.reserve(nCL);
      for (int i = 0; i < nCL; i++) {
        ColumnLevel *cl = sc->getColumnLevel(i);
        if (!cl) continue;
        TSoundTrackP t;
        try { t = cl->getSoundLevel()->getSoundTrack(); } catch (...) {}
        if (!t) continue;
        cldata.push_back({cl, t, (TINT32)t->getSampleRate()});
      }

      if (!cldata.empty()) {
        // Global peak normalisation across all CLs
        double peak = 1.0;
        for (auto &d : cldata) {
          TINT32 s0 = (TINT32)(d.cl->getStartOffset() * d.sampleRate / fps);
          TINT32 visFrames = d.cl->getVisibleFrameCount();
          TINT32 s1 = s0 + (TINT32)(visFrames * d.sampleRate / fps);
          s0 = qBound((TINT32)0, s0, d.track->getSampleCount() - 1);
          s1 = qBound((TINT32)0, s1, d.track->getSampleCount() - 1);
          if (s1 > s0) {
            double mn = 0, mx = 0;
            d.track->getMinMaxPressure(s0, s1, TSound::MONO, mn, mx);
            peak = std::max(peak,
                            std::max(std::fabs(mn), std::fabs(mx)));
          }
        }

        const int halfH = (trackH - 4) / 2;
        cp.setPen(QColor(80, 200, 120));

        for (auto &d : cldata) {
          double samplesPerPixel = d.sampleRate / fps / m_ppf;
          int vsf  = d.cl->getVisibleStartFrame();
          int pxL  = (int)(vsf * m_ppf);
          int pxR  = (int)((d.cl->getVisibleEndFrame() + 1) * m_ppf);
          pxL = qBound(0, pxL, trackW);
          pxR = qBound(0, pxR, trackW);

          for (int px = pxL; px < pxR; px++) {
            double relFrame = (double)px / m_ppf - vsf;
            TINT32 s0 = (TINT32)((d.cl->getStartOffset() + relFrame)
                                  * d.sampleRate / fps);
            TINT32 s1 = s0 + (TINT32)(samplesPerPixel) + 1;
            TINT32 sc2 = d.track->getSampleCount();
            s0 = qBound((TINT32)0, s0, sc2 - 1);
            s1 = qBound((TINT32)0, s1, sc2 - 1);
            if (s0 > s1) continue;

            double minV = 0, maxV = 0;
            d.track->getMinMaxPressure(s0, s1, TSound::MONO, minV, maxV);

            int yMax = center - (int)(maxV / peak * halfH);
            int yMin = center - (int)(minV / peak * halfH);
            yMax = qBound(2, yMax, trackH - 2);
            yMin = qBound(2, yMin, trackH - 2);
            if (yMax > yMin) std::swap(yMax, yMin);
            cp.drawLine(px, yMax, px, yMin);
          }
        }
      }
    }
  }

  // Draw segments and gaps:
  // - Gaps (empty areas between segments) → gray background, no waveform
  // - Segments → blit only the corresponding slice of the waveform cache
  // - Segment edges → 1px dark border
  {
    auto segs = findSegments();

    if (segs.empty()) {
      // No segments at all — fill entire track area with gap color
      p.fillRect(labelW, 0, trackW, trackH, QColor(42, 42, 42));
    } else {
      // 1. Fill entire area with gap color first (covers gaps before first
      //    segment, between segments, and after last segment)
      p.fillRect(labelW, 0, trackW, trackH, QColor(42, 42, 42));

      // 2. For each segment, blit the corresponding slice from the waveform
      //    cache (cache is position-aware: pixel px = frame px/ppf)
      for (auto &s : segs) {
        int x0 = (int)(s.r0 * m_ppf);          // offset within cache
        int x1 = (int)((s.r1 + 1) * m_ppf);
        int w  = x1 - x0;
        if (w <= 0) continue;
        p.drawPixmap(labelW + x0, 0, m_waveformCache, x0, 0, w, trackH);
      }

      // 3. Draw 1px borders at each segment edge
      p.setPen(QColor(70, 70, 70));
      for (auto &s : segs) {
        int x0 = labelW + (int)(s.r0 * m_ppf);
        int x1 = labelW + (int)((s.r1 + 1) * m_ppf);
        p.drawLine(x0, 0, x0, trackH - 1);
        p.drawLine(x1 - 1, 0, x1 - 1, trackH - 1);
      }
    }
  }

  // Dim overlay whenever the track is silent: either user-muted (M) or
  // silenced by another track's solo (effectiveMuted).
  if (m_muted || m_effectiveMuted)
    p.fillRect(labelW, 0, trackW, trackH, QColor(0, 0, 0, 130));

  // Highlight selected segment
  if (m_selSeg.r0 >= 0 && m_selSeg.r1 >= m_selSeg.r0) {
    int x0 = labelW + (int)(m_selSeg.r0 * m_ppf);
    int x1 = labelW + (int)((m_selSeg.r1 + 1) * m_ppf);
    p.fillRect(x0, 0, x1 - x0, trackH, QColor(100, 180, 255, 50));
    p.setPen(QColor(100, 180, 255, 150));
    p.drawRect(x0, 0, x1 - x0 - 1, trackH - 1);
  }

  // Preview bar (12c) — thin strip at bottom, orange selection
  static const int kScrubBarH = 6;
  p.fillRect(labelW, trackH - kScrubBarH, trackW, kScrubBarH, QColor(55, 55, 55));
  if (m_previewR0 >= 0 && m_previewR1 >= m_previewR0) {
    int x0 = labelW + (int)(m_previewR0 * m_ppf);
    int x1 = labelW + (int)((m_previewR1 + 1) * m_ppf);
    p.fillRect(x0, trackH - kScrubBarH, x1 - x0, kScrubBarH, QColor(255, 165, 0));
  }

  // NOTE: m_cutFrames (video shot boundaries) are intentionally NOT drawn here.
  // Audio segment edges are already visible as the 1px borders around each block.
  // Drawing shot boundaries on the audio track caused ghost lines when segments
  // were moved away from those positions.

  // Razor hover preview line — shown when razor is active and cursor hovers
  if (m_razorHoverFrame >= 0) {
    int rx = labelW + (int)(m_razorHoverFrame * m_ppf);
    p.setPen(QPen(QColor(255, 255, 80, 200), 1, Qt::DashLine));
    p.drawLine(rx, 0, rx, trackH);
  }

  // Playhead (always on top, not cached) — centered on frame like the ruler
  int phx = labelW + (int)(m_currentFrame * m_ppf) + (int)(m_ppf / 2);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(phx, 0, phx, trackH);
}

void ZtoryAudioTrack::setRazorActive(bool on) {
  m_razorActive = on;
  setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

void ZtoryAudioTrack::setCutFrames(const QVector<int> &frames) {
  m_cutFrames = frames;
  update();
}

void ZtoryAudioTrack::setRazorHoverFrame(int frame) {
  if (m_razorHoverFrame == frame) return;
  m_razorHoverFrame = frame;
  update();
}

// ---- ZtoryAudioTrack mouse events (12c: preview bar, razor) ----

void ZtoryAudioTrack::mousePressEvent(QMouseEvent *e) {
  // L/M/S button hit-test — must match coordinates in paintEvent exactly
  // Buttons: horizontal row, each 22px wide, 3px gap, starting at x=2, y=2, h=16
  if (e->button() == Qt::LeftButton && e->y() >= 2 && e->y() < 18) {
    static const int kBtnW = 22, kBtnGap = 3;
    for (int i = 0; i < 3; i++) {
      int bx = 2 + i * (kBtnW + kBtnGap);
      if (e->x() >= bx && e->x() < bx + kBtnW) {
        if (i == 0) {        // Lock
          m_locked = !m_locked;
          update();
          emit lockedChanged(m_col, m_locked);
        } else if (i == 1) { // Mute
          m_muted = !m_muted;
          update();
          // Delegate volume/cache/restart to the panel via applyMuteSolo(),
          // so solo state and live-play restart are always considered.
          emit muteToggleRequested(m_col);
        } else {             // Solo
          m_solo = !m_solo;
          update();
          emit soloToggleRequested(m_col);
        }
        return;
      }
    }
  }
  // Ignore other label area clicks
  if (e->x() < kLabelW) return;

  static const int kScrubBarH = 6;
  // Razor tool: click anywhere on the waveform area (not in the scrub bar)
  if (m_razorActive && !m_locked && e->y() < height() - kScrubBarH) {
    int frame = frameAtX(e->x());
    // Clear selection so the highlight doesn't persist after the cut
    if (m_selSeg.r0 >= 0) { m_selSeg = {-1, -1}; update(); }
    emit razorRequested(m_col, frame);
    return;
  }
  // Scrub bar drag — allowed even when locked (read-only preview)
  if (e->y() >= height() - kScrubBarH) {
    m_draggingPreview  = true;
    int frame = frameAtX(e->x());
    m_previewDragStart = frame;
    m_previewR0 = m_previewR1 = frame;
    update();
    return;
  }
  // Select / drag / trim segment — blocked when locked
  if (e->button() == Qt::LeftButton && !m_razorActive && !m_locked) {
    int frame = frameAtX(e->x());
    int mx = e->x();
    auto segs = findSegments();
    m_selSeg = {-1, -1};
    m_dragMode = NoDrag;
    for (auto &s : segs) {
      if (frame < s.r0 || frame > s.r1) continue;
      m_selSeg = s;
      m_dragOrigR0 = s.r0;
      m_dragOrigR1 = s.r1;
      m_dragStartFrame = frame;
      // Edge detection: 6px zone at each border
      int xLeft  = kLabelW + (int)(s.r0 * m_ppf);
      int xRight = kLabelW + (int)((s.r1 + 1) * m_ppf);
      if (mx - xLeft < 6 && mx - xLeft >= 0) {
        m_dragMode = TrimLeft;
        setCursor(Qt::SizeHorCursor);
      } else if (xRight - mx < 6 && xRight - mx >= 0) {
        m_dragMode = TrimRight;
        setCursor(Qt::SizeHorCursor);
      } else {
        m_dragMode = SegmentDrag;
        setCursor(Qt::ClosedHandCursor);
      }
      break;
    }
    update();
  }
}

void ZtoryAudioTrack::mouseMoveEvent(QMouseEvent *e) {
  if (m_draggingPreview) {
    int frame = frameAtX(e->x());
    m_previewR0 = std::min(m_previewDragStart, frame);
    m_previewR1 = std::max(m_previewDragStart, frame);
    update();
    return;
  }

  // Segment drag with overlap clamping
  if (m_dragMode == SegmentDrag && m_selSeg.r0 >= 0) {
    int frame = frameAtX(e->x());
    int delta = frame - m_dragStartFrame;
    int segLen = m_dragOrigR1 - m_dragOrigR0;
    int newR0 = m_dragOrigR0 + delta;
    if (newR0 < 0) newR0 = 0;
    // Clamp against neighboring segments to prevent overlap
    auto segs = findSegments();
    for (auto &s : segs) {
      if (s.r0 == m_dragOrigR0 && s.r1 == m_dragOrigR1) continue;
      // Neighbor on the left: newR0 must be > s.r1
      if (s.r1 < m_dragOrigR0 && newR0 <= s.r1)
        newR0 = s.r1 + 1;
      // Neighbor on the right: newR0 + segLen must be < s.r0
      if (s.r0 > m_dragOrigR1 && newR0 + segLen >= s.r0)
        newR0 = s.r0 - segLen - 1;
    }
    if (newR0 < 0) newR0 = 0;
    m_selSeg.r0 = newR0;
    m_selSeg.r1 = newR0 + segLen;
    update();
    return;
  }

  // Trim left/right edge: visual feedback
  if ((m_dragMode == TrimLeft || m_dragMode == TrimRight) && m_selSeg.r0 >= 0) {
    int frame = frameAtX(e->x());
    int delta = frame - m_dragStartFrame;
    if (m_dragMode == TrimLeft) {
      int newR0 = m_dragOrigR0 + delta;
      if (newR0 < 0) newR0 = 0;
      if (newR0 > m_dragOrigR1 - 1) newR0 = m_dragOrigR1 - 1;
      m_selSeg.r0 = newR0;
      m_selSeg.r1 = m_dragOrigR1;
    } else {
      int newR1 = m_dragOrigR1 + delta;
      if (newR1 < m_dragOrigR0 + 1) newR1 = m_dragOrigR0 + 1;
      m_selSeg.r0 = m_dragOrigR0;
      m_selSeg.r1 = newR1;
    }
    update();
    return;
  }

  // Razor hover: track cursor position to show the cut preview line
  if (m_razorActive) {
    int frame = frameAtX(e->x());
    if (frame != m_razorHoverFrame) {
      m_razorHoverFrame = frame;
      update();
    }
    return;
  }

  // Hover cursor: show SizeHorCursor near segment edges (pixel-based, no frame rounding)
  {
    int mx = e->x();
    auto segs = findSegments();
    bool nearEdge = false;
    for (auto &s : segs) {
      int xLeft  = kLabelW + (int)(s.r0 * m_ppf);
      int xRight = kLabelW + (int)((s.r1 + 1) * m_ppf);
      if ((mx >= xLeft - 1 && mx < xLeft + 6) ||
          (mx > xRight - 7 && mx <= xRight + 1)) {
        nearEdge = true;
        break;
      }
    }
    setCursor(nearEdge ? Qt::SizeHorCursor : Qt::ArrowCursor);
  }
}

void ZtoryAudioTrack::leaveEvent(QEvent *) {
  if (m_razorHoverFrame >= 0) {
    m_razorHoverFrame = -1;
    update();
  }
}

void ZtoryAudioTrack::mouseReleaseEvent(QMouseEvent *e) {
  DragMode finishedMode = m_dragMode;
  m_dragMode = NoDrag;
  setCursor(m_razorActive ? Qt::CrossCursor : Qt::ArrowCursor);

  // Finish segment drag — commit via shiftLevelInRange on the ColumnLevel
  if (finishedMode == SegmentDrag) {
    int segLen = m_dragOrigR1 - m_dragOrigR0;

    // Cross-track drop: if mouse is outside this widget vertically
    QPoint localPos = e->pos();
    if (localPos.y() < 0 || localPos.y() >= height()) {
      int dragOffset = m_dragStartFrame - m_dragOrigR0;
      emit segmentDroppedOutside(m_col, m_dragOrigR0, m_dragOrigR1,
                                 dragOffset, e->globalPos());
      update();
      return;
    }

    int finalDelta = m_selSeg.r0 - m_dragOrigR0;
    if (finalDelta != 0 && m_dragOrigR0 >= 0) {
      TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
      TXshColumn *column = xsh ? xsh->getColumn(m_col) : nullptr;
      TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
      if (sc) {
        sc->shiftLevelInRange(m_dragOrigR0, m_dragOrigR1, finalDelta);
        invalidateWaveform();
        ZtoryAnimaticController::instance()->invalidateSoundTrack();
        emit segmentMoved();
      } else {
        m_selSeg = {m_dragOrigR0, m_dragOrigR0 + segLen};
      }
    }
    update();
    return;
  }

  // Finish edge trim — commit via modifyCellRange
  if (finishedMode == TrimLeft || finishedMode == TrimRight) {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    TXshColumn *column = xsh ? xsh->getColumn(m_col) : nullptr;
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;
    if (sc) {
      if (finishedMode == TrimLeft) {
        int delta = m_selSeg.r0 - m_dragOrigR0;
        if (delta != 0)
          sc->modifyCellRange(m_dragOrigR0, delta, true);
      } else {
        int delta = m_selSeg.r1 - m_dragOrigR1;
        if (delta != 0)
          sc->modifyCellRange(m_dragOrigR1, delta, false);
      }
      invalidateWaveform();
      ZtoryAnimaticController::instance()->invalidateSoundTrack();
      emit segmentMoved();
    }
    update();
    return;
  }

  // Finish preview bar drag
  if (m_draggingPreview) {
    m_draggingPreview = false;
    if (m_previewR0 < 0 || m_previewR1 < m_previewR0) return;

    auto *ctrl = ZtoryAnimaticController::instance();
    TXsheet *xsh = ctrl->mainXsheet();
    if (!xsh) return;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    double fps = scene ? scene->getProperties()->getOutputProperties()->getFrameRate() : 24.0;

    TSoundTrackP st = ctrl->requireSoundTrack();
    if (!st) return;

    TINT32 sr = st->getSampleRate();
    TINT32 s0 = (TINT32)(m_previewR0 * sr / fps);
    TINT32 s1 = (TINT32)((m_previewR1 + 1) * sr / fps);
    s0 = qBound((TINT32)0, s0, st->getSampleCount() - 1);
    s1 = qBound((TINT32)0, s1, st->getSampleCount() - 1);
    if (s0 >= s1) return;
    xsh->play(st, s0, s1, false);
  }
}

// ---- Clipboard operations ----

void ZtoryAudioTrack::clipboardCopy(ZtoryAudioTrack *src) {
  if (!src || src->m_selSeg.r0 < 0) return;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  int r0 = src->m_selSeg.r0, r1 = src->m_selSeg.r1;
  s_audioClipboard.col = src->m_col;
  s_audioClipboard.r0 = r0;
  s_audioClipboard.r1 = r1;
  s_audioClipboard.isCut = false;
  s_audioClipboard.cells.clear();
  for (int r = r0; r <= r1; r++)
    s_audioClipboard.cells.push_back(xsh->getCell(r, src->m_col));
}

void ZtoryAudioTrack::clipboardCut(ZtoryAudioTrack *src) {
  // TODO(audio-drag): TXshSoundColumn requires ColumnLevel API, not clearCells.
  // For now, cut is a copy-only operation (no delete of the source).
  clipboardCopy(src);
}

void ZtoryAudioTrack::clipboardPaste(ZtoryAudioTrack *dst, int frame) {
  // TODO(audio-drag): TXshSoundColumn requires ColumnLevel API, not setCell.
  // Paste is disabled until the proper API is implemented.
  Q_UNUSED(dst); Q_UNUSED(frame);
}

void ZtoryAudioTrack::keyPressEvent(QKeyEvent *e) {
  bool ctrl = (e->modifiers() & Qt::ControlModifier);
  if (ctrl && e->key() == Qt::Key_X) { clipboardCut(this); return; }
  if (ctrl && e->key() == Qt::Key_C) { clipboardCopy(this); return; }
  if (ctrl && e->key() == Qt::Key_V) {
    // Paste at the start of the selected segment, or at current frame
    int frame = (m_selSeg.r0 >= 0) ? m_selSeg.r0 : m_currentFrame;
    clipboardPaste(this, frame);
    return;
  }
  if (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace) {
    // TODO(audio-drag): TXshSoundColumn requires ColumnLevel API, not clearCells.
    // Delete is disabled until the proper API is implemented.
    return;
  }
  QWidget::keyPressEvent(e);
}

// ---- ZtoryAnimaticTrack ----

ZtoryAnimaticTrack::ZtoryAnimaticTrack(QWidget *parent) : QWidget(parent) {
  setFixedHeight(80);
  setMouseTracking(true);

  // Lock button is painted in paintEvent and activated via mousePressEvent
  // hit-test — no child QToolButton needed.
}

void ZtoryAnimaticTrack::setLocked(bool on) {
  if (m_locked == on) return;
  m_locked = on;
  update();
  emit lockedChanged(on);
}

void ZtoryAnimaticTrack::updateCursor() {
  if (m_tool == RazorTool)
    setCursor(Qt::CrossCursor);
  else
    unsetCursor();
}

void ZtoryAnimaticTrack::setRazorHoverFrame(int frame) {
  if (m_razorHoverFrame == frame) return;
  m_razorHoverFrame = frame;
  update();
}

void ZtoryAnimaticTrack::refreshFromScene() {
  m_blocks.clear();
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  int numCols = mainXsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    // Usa getMinFrame/getMaxFrame per avere durata reale incluse celle vuote
    TXshColumn *column = mainXsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    if (r1 < r0) continue;
    // Cerca child level per identificare lo shot
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) continue;
    int startFrame = r0;
    int duration = r1 - r0 + 1;

    ShotBlock b;
    b.col = col;
    b.startFrameInMain = startFrame;
    b.f0 = 0;
    b.f1 = duration - 1;
    // Legge il numero shot dal nome della colonna (impostato da StoryboardPanel)
    QString colName = QString::fromStdString(
        mainXsh->getStageObject(mainXsh->getColumnObjectId(col))->getName());
    b.shotNumber = colName.isEmpty() ? QString("%1").arg(col + 1, 2, 10, QChar('0')) : colName;

    // Generate thumbnail from the first drawing in the sub-scene
    if (cl) {
      TXsheet *subXsh = cl->getXsheet();
      if (subXsh) {
        bool found = false;
        for (int c = 0; c < subXsh->getColumnCount() && !found; c++) {
          TXshColumn *subCol = subXsh->getColumn(c);
          if (!subCol || subCol->isEmpty()) continue;
          int sr0 = 0, sr1 = 0;
          subCol->getRange(sr0, sr1);
          for (int r = sr0; r <= sr1 && !found; r++) {
            TXshCell cell = subXsh->getCell(r, c);
            if (!cell.isEmpty() && cell.getSimpleLevel()) {
              b.thumbnail = IconGenerator::instance()->getIcon(
                cell.m_level.getPointer(), cell.getFrameId());
              found = !b.thumbnail.isNull();
            }
          }
        }
      }
    }
    m_blocks.push_back(b);
  }
  // Ordina per startFrameInMain per garantire ordine corretto nel ripple
  std::sort(m_blocks.begin(), m_blocks.end(),
    [](const ShotBlock &a, const ShotBlock &b) {
      return a.startFrameInMain < b.startFrameInMain;
    });

  // Calcola larghezza totale
  int totalFrames = 0;
  for (auto &b : m_blocks)
    totalFrames = qMax(totalFrames, b.startFrameInMain + (b.f1 - b.f0 + 1));
  setMinimumWidth(kLabelW + (int)(totalFrames * m_ppf) + 100);
  update();
}

void ZtoryAnimaticTrack::paintEvent(QPaintEvent *) {
  QPainter p(this);

  p.fillRect(rect(), QColor(30, 30, 30));

  // Label column — aligned with audio track label and ruler
  p.fillRect(0, 0, kLabelW, height(), QColor(40, 40, 40));
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  QString sceneName = scene
      ? QString::fromStdWString(scene->getSceneName())
      : tr("Animatic");
  // Track name (left of lock button)
  p.setPen(QColor(210, 210, 210));
  p.setFont(QFont("Arial", 9));
  p.drawText(4, 0, kLabelW - 30, height(),
             Qt::AlignVCenter | Qt::AlignLeft, sceneName);

  // Painted lock button (right of label area)
  {
    const int bw = 22, bh = 20;
    QRect r(kLabelW - bw - 4, (height() - bh) / 2, bw, bh);
    p.fillRect(r, m_locked ? QColor(220, 140, 50) : QColor(45, 45, 45));
    p.setPen(m_locked ? QColor(255, 255, 255) : QColor(220, 140, 50));
    p.drawRect(r.adjusted(0, 0, -1, -1));
    p.setFont(QFont("Arial", 7, QFont::Bold));
    p.drawText(r, Qt::AlignCenter, "L");
  }

  p.setPen(QColor(60, 60, 60));
  p.drawLine(kLabelW, 0, kLabelW, height());

  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = kLabelW + (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    int h = height() - 4;
    bool selected = m_selectedCols.count(b.col) > 0;

    // Sfondo shot
    QColor bg = selected ? QColor(80, 110, 160) : QColor(60, 90, 130);
    p.fillRect(x + 1, 2, w - 2, h, bg);
    p.setPen(selected ? QColor(255, 160, 0) : QColor(100, 140, 200));
    p.drawRect(x + 1, 2, w - 2, h);
    if (selected) {
      p.setPen(QPen(QColor(255, 160, 0), 2));
      p.drawRect(x + 2, 3, w - 4, h - 2);
    }

    // Thumbnail — visible when block is wide enough to show a useful preview
    int thumbW = 0;
    if (!b.thumbnail.isNull() && w > 40) {
      double aspect = b.thumbnail.width() / (double)qMax(b.thumbnail.height(), 1);
      int thumbH = h - 4;
      thumbW = (int)(thumbH * aspect);
      thumbW = qMin(thumbW, w - 6);
      p.setOpacity(0.85);
      p.drawPixmap(x + 2, 4, thumbW, thumbH, b.thumbnail);
      p.setOpacity(1.0);
    }

    // Numero shot — to the right of the thumbnail if present
    p.setPen(Qt::white);
    int textX = x + 4 + (thumbW > 0 ? thumbW + 2 : 0);
    int textW = w - 8 - (thumbW > 0 ? thumbW + 2 : 0);
    if (textW > 10)
      p.drawText(textX, 2, textW, h, Qt::AlignBottom | Qt::AlignLeft, b.shotNumber);
    else
      p.drawText(x + 4, 2, w - 8, h, Qt::AlignBottom | Qt::AlignLeft, b.shotNumber);

    // Handle resize (bordo destro)
    p.fillRect(x + w - 4, 2, 4, h, QColor(180, 180, 80));
  }

  // Playhead — centered on frame like the ruler
  int px = kLabelW + (int)(m_currentFrame * m_ppf) + (int)(m_ppf / 2);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(px, 0, px, height());

  // Razor hover preview — bright vertical line snapped to the frame boundary
  if (m_tool == RazorTool && m_razorHoverFrame >= 0) {
    int rx = kLabelW + (int)(m_razorHoverFrame * m_ppf);
    p.setPen(QPen(QColor(255, 255, 80, 200), 1, Qt::DashLine));
    p.drawLine(rx, 0, rx, height());
  }
}

void ZtoryAnimaticTrack::mousePressEvent(QMouseEvent *e) {
  int mx = e->x() - kLabelW;
  // Lock button hit-test in label area
  if (mx < 0) {
    if (e->button() == Qt::LeftButton) {
      const int bw = 22, bh = 20;
      QRect lockRect(kLabelW - bw - 4, (height() - bh) / 2, bw, bh);
      if (lockRect.contains(e->pos()))
        setLocked(!m_locked);
    }
    return;
  }
  if (m_locked) return; // track is locked — block all editing

  // Razor tool: split shot at click position
  if (m_tool == RazorTool && e->button() == Qt::LeftButton) {
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int x = (int)(b.startFrameInMain * m_ppf);
      int w = (int)(duration * m_ppf);
      if (mx >= x && mx < x + w) {
        int splitFrame = (int)((mx - x) / m_ppf); // relativo all'inizio del blocco
        if (splitFrame > 0 && splitFrame < duration - 1)
          emit razorRequested(b.col, b.startFrameInMain + splitFrame);
        return;
      }
    }
    return;
  }

  // Tasto destro — context menu
  if (e->button() == Qt::RightButton) {
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int x = (int)(b.startFrameInMain * m_ppf);
      int w = (int)(duration * m_ppf);
      if (mx >= x && mx < x + w) {
        QMenu menu(this);
        QAction *matchAct   = menu.addAction(tr("Match Subscene Duration"));
        QAction *mergeAct   = menu.addAction(tr("Merge with Next Shot"));
        QAction *chosen = menu.exec(e->globalPos());
        if (chosen == matchAct)
          emit matchSubsceneDuration(b.col);
        else if (chosen == mergeAct)
          emit mergeWithNextRequested(b.col);
        return;
      }
    }
    return;
  }

  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    // Check handle resize
    if (mx >= x + w - 6 && mx <= x + w + 2) {
      m_draggingCol = b.col;
      m_dragStartX = mx;
      m_dragOrigF1 = b.f1;
      // Refresh prima di salvare originals per avere dati aggiornati
      refreshFromScene();
      m_origStarts.clear();
      m_origDurations.clear();
      for (auto &sb : m_blocks) {
        m_origStarts[sb.col] = sb.startFrameInMain;
        m_origDurations[sb.col] = sb.f1 - sb.f0 + 1;
      }
      return;
    }
    // Click su shot
    if (mx >= x && mx < x + w) {
      Qt::KeyboardModifiers mods = e->modifiers();
      if (mods & Qt::ShiftModifier && m_lastClickedCol >= 0) {
        // Selezione a range: seleziona tutti i blocchi tra m_lastClickedCol e b.col
        bool inRange = false;
        for (auto &sb : m_blocks) {
          if (sb.col == m_lastClickedCol || sb.col == b.col) {
            inRange = !inRange;
            m_selectedCols.insert(sb.col);
          } else if (inRange) {
            m_selectedCols.insert(sb.col);
          }
        }
        // Garantisce che anche il punto finale sia incluso
        m_selectedCols.insert(b.col);
      } else if (mods & Qt::ControlModifier) {
        // Toggle selezione
        if (m_selectedCols.count(b.col)) m_selectedCols.erase(b.col);
        else m_selectedCols.insert(b.col);
      } else {
        // Click semplice: seleziona solo questo
        m_selectedCols.clear();
        m_selectedCols.insert(b.col);
      }
      m_lastClickedCol = b.col;
      update();
      emit selectionChanged(m_selectedCols);
      emit shotClicked(b.col);
      return;
    }
  }
  // Click su area vuota: deseleziona tutto
  if (!(e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
    m_selectedCols.clear();
    m_lastClickedCol = -1;
    update();
    emit selectionChanged(m_selectedCols);
  }
}

void ZtoryAnimaticTrack::mouseMoveEvent(QMouseEvent *e) {
  if (m_draggingCol >= 0) {
    int dx = (e->x() - kLabelW) - m_dragStartX;
    int delta = (int)(dx / m_ppf);
    int newF1 = qMax(m_dragOrigF1 + delta, 0);
    int newDuration = newF1 + 1;
    int durationDelta = newDuration - (m_dragOrigF1 + 1);

    // Aggiorna durata shot draggato e ricalcola posizioni successive
    int nextStart = -1;
    for (int i = 0; i < (int)m_blocks.size(); i++) {
      if (m_blocks[i].col == m_draggingCol) {
        m_blocks[i].f1 = newF1;
        nextStart = m_origStarts[m_blocks[i].col] + newDuration;
      } else if (nextStart >= 0) {
        int origDuration = m_origDurations.value(m_blocks[i].col, m_blocks[i].f1 - m_blocks[i].f0 + 1);
        m_blocks[i].startFrameInMain = nextStart;
        nextStart += origDuration;
      }
    }
    update();
    return;
  }

  // Razor hover: snap the indicator to the nearest frame boundary
  if (m_tool == RazorTool) {
    int mx = e->x() - kLabelW;
    int frame = (mx >= 0) ? (int)(mx / m_ppf) : -1;
    // Only show hover if cursor is over a valid cut position inside a block
    int hoverFrame = -1;
    for (auto &b : m_blocks) {
      int duration = b.f1 - b.f0 + 1;
      int bx0 = (int)(b.startFrameInMain * m_ppf);
      int bx1 = (int)((b.startFrameInMain + duration) * m_ppf);
      if (mx >= bx0 && mx < bx1) {
        int rel = (int)((mx - bx0) / m_ppf);
        if (rel > 0 && rel < duration - 1)
          hoverFrame = b.startFrameInMain + rel;
        break;
      }
    }
    if (hoverFrame != m_razorHoverFrame) {
      m_razorHoverFrame = hoverFrame;
      update();
      emit razorHoverFrameChanged(hoverFrame);
    }
  }
}

void ZtoryAnimaticTrack::leaveEvent(QEvent *) {
  if (m_tool == RazorTool && m_razorHoverFrame >= 0) {
    m_razorHoverFrame = -1;
    update();
    emit razorHoverFrameChanged(-1);
  }
}

void ZtoryAnimaticTrack::mouseReleaseEvent(QMouseEvent *) {
  if (m_draggingCol >= 0) {
    bool found = false;
    for (auto &b : m_blocks) {
      if (b.col == m_draggingCol) {
        emit shotDurationChanged(m_draggingCol, b.f1);
        found = true;
      } else if (found) {
        // Ripple: aggiorna posizione xsheet degli shot successivi
        emit shotMoved(b.col, b.startFrameInMain);
      }
    }
    m_draggingCol = -1;
  }
}

void ZtoryAnimaticTrack::mouseDoubleClickEvent(QMouseEvent *e) {
  int mx = e->x() - kLabelW;
  if (mx < 0) return;
  for (auto &b : m_blocks) {
    int duration = b.f1 - b.f0 + 1;
    int x = (int)(b.startFrameInMain * m_ppf);
    int w = (int)(duration * m_ppf);
    if (mx >= x && mx < x + w) {
      emit shotDoubleClicked(b.col);
      return;
    }
  }
  // Double-click on empty space → return to main xsheet (close any open sub-scene)
  emit returnToMainRequested();
}

void ZtoryAnimaticTrack::wheelEvent(QWheelEvent *e) {
  int delta = e->angleDelta().y();
  double factor = (delta > 0) ? 1.15 : (1.0 / 1.15);
  double newPpf = qBound(2.0, m_ppf * factor, 64.0);
  emit zoomChanged(newPpf);
  e->accept();
}

// ---- ZtoryStoryStrip ----

ZtoryStoryStrip::ZtoryStoryStrip(QWidget *parent) : QWidget(parent) {
  setFixedHeight(kThumbH + 8);
  setMinimumWidth(100);
  setStyleSheet("background:#1a1a1a;");
}

void ZtoryStoryStrip::refreshFromScene() {
  m_entries.clear();
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) { update(); return; }
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) { update(); return; }

  int numCols = xsh->getColumnCount();
  for (int col = 0; col < numCols; col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    TXshChildLevel *cl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        cl = cell.m_level->getChildLevel();
        break;
      }
    }
    if (!cl) continue;
    ThumbEntry e;
    e.col = col;
    e.shotNumber = ZtoryModel::instance()->shotCount() > col
        ? QString::fromStdString("")  // will use column name below
        : QString("%1").arg(col + 1, 2, 10, QChar('0'));
    QString colName = QString::fromStdString(
        xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (!colName.isEmpty()) e.shotNumber = colName;
    // Grab preview from ZtoryModel if available
    for (int si = 0; si < ZtoryModel::instance()->shotCount(); si++) {
      if (ZtoryModel::instance()->shot(si).xsheetColumn == col) {
        e.thumb = ZtoryModel::instance()->preview(si, 0);
        break;
      }
    }
    m_entries.push_back(e);
  }
  update();
}

void ZtoryStoryStrip::setCurrentCol(int col) {
  m_currentCol = col;
  // Ensure the current shot is visible (auto-scroll)
  int idx = -1;
  for (int i = 0; i < (int)m_entries.size(); i++)
    if (m_entries[i].col == col) { idx = i; break; }
  if (idx >= 0) {
    int x = idx * (kThumbW + kSpacing);
    if (x < m_scrollOffset) m_scrollOffset = x;
    else if (x + kThumbW > m_scrollOffset + width()) m_scrollOffset = x + kThumbW - width();
    m_scrollOffset = qMax(0, m_scrollOffset);
  }
  update();
}

void ZtoryStoryStrip::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.fillRect(rect(), QColor(25, 25, 25));

  int x = 4 - m_scrollOffset;
  for (auto &e : m_entries) {
    bool current = (e.col == m_currentCol);
    QRect r(x, 4, kThumbW, kThumbH);

    // Background
    p.fillRect(r, current ? QColor(70, 100, 150) : QColor(45, 45, 45));

    // Thumbnail
    if (!e.thumb.isNull())
      p.drawPixmap(r, e.thumb.scaled(r.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Border
    p.setPen(current ? QColor(255, 160, 0) : QColor(80, 80, 80));
    p.drawRect(r);

    // Shot number overlay
    p.setPen(Qt::white);
    p.setFont(QFont("Arial", 8, QFont::Bold));
    p.drawText(r.adjusted(2, 2, -2, -2), Qt::AlignBottom | Qt::AlignLeft, e.shotNumber);

    x += kThumbW + kSpacing;
  }
}

void ZtoryStoryStrip::mousePressEvent(QMouseEvent *e) {
  int mx = e->x();
  int x = 4 - m_scrollOffset;
  for (auto &entry : m_entries) {
    if (mx >= x && mx < x + kThumbW) {
      setCurrentCol(entry.col);
      emit shotClicked(entry.col);
      return;
    }
    x += kThumbW + kSpacing;
  }
}

void ZtoryStoryStrip::wheelEvent(QWheelEvent *e) {
  m_scrollOffset -= e->angleDelta().y() / 2;
  int maxOffset = qMax(0, (int)m_entries.size() * (kThumbW + kSpacing) - width());
  m_scrollOffset = qBound(0, m_scrollOffset, maxOffset);
  update();
  e->accept();
}

// ---- ZtoryAnimaticViewer (standalone) ----

ZtoryAnimaticViewer::ZtoryAnimaticViewer(QWidget *parent)
    : BaseViewerPanel(parent) {
  m_sceneViewer->setAlwaysMainXsheet(true);

  // --- Dedicated frame handle from the animatic controller ---
  auto *ctrl = ZtoryAnimaticController::instance();
  ctrl->setViewer(this);  // allow panel to call restartAudioIfPlaying()
  // Override FlipConsole so playback uses the animatic frame, not the global
  m_flipConsole->setFrameHandle(ctrl->frameHandle());
  // Override SceneViewer so rendering reads the animatic frame
  m_sceneViewer->setCustomFrameHandle(ctrl->frameHandle());
  // Disconnect playback from global frame handle, connect to ours
  disconnect(m_flipConsole, SIGNAL(playStateChanged(bool)),
             TApp::instance()->getCurrentFrame(), SLOT(setPlaying(bool)));
  connect(m_flipConsole, SIGNAL(playStateChanged(bool)),
          ctrl->frameHandle(), SLOT(setPlaying(bool)));

  // Don't let this viewer become the global active viewer
  disconnect(TApp::instance(), SIGNAL(activeViewerChanged()),
             this, SLOT(onActiveViewerChanged()));

  // Repaint when the animatic frame changes
  connect(ctrl->frameHandle(), &TFrameHandle::frameSwitched, this, [this]() {
    if (m_sceneViewer) m_sceneViewer->update();
  });

  // Repaint when onion skin mask changes so the viewer shows the onion frames.
  // Without this connection, syncOnionToGlobal() pushes the new mask to the
  // global handle but the viewer never receives a repaint request.
  connect(TApp::instance()->getCurrentOnionSkin(),
          SIGNAL(onionSkinMaskChanged()), m_sceneViewer, SLOT(update()));

  // Set up layout: scene viewer + flip console (like SceneViewerPanel)
  m_mainLayout->insertWidget(0, m_fsWidget, 1);
  setLayout(m_mainLayout);
  m_visiblePartsFlag = VPPARTS_ALL;
}

// ---- onDrawFrame override ----
// Called by FlipConsole's internal play timer once per frame.
// The base implementation writes the frame to TApp::getCurrentFrame() (global).
// When a sub-scene is open, that means the sub-scene's frame advances during
// animatic play — wrong behaviour.  We redirect to the dedicated controller
// handle so the animatic stays isolated from the native timeline state.
void ZtoryAnimaticViewer::onDrawFrame(
    int frame, const ImagePainter::VisualSettings &settings,
    QElapsedTimer * /*timer*/, qint64 /*targetInstant*/) {
  m_sceneViewer->setVisual(settings);
  auto *ctrl = ZtoryAnimaticController::instance();
  if (!settings.m_drawBlankFrame) {
    int targetFrame;

    if (m_continuousPlay && m_fps > 0) {
      // Detect loop-back: FlipConsole wrapped its internal counter from
      // mark-out back to mark-in.  When frame < m_prevFlipFrame the loop
      // fired — restart the audio stream from the new start position.
      if (m_prevFlipFrame > 0 && frame < m_prevFlipFrame) {
        TXsheet *mainXsh2 = ctrl->mainXsheet();
        if (mainXsh2) mainXsh2->stopScrub();
        m_continuousPlay = false;
        // Restart audio from FlipConsole's new start frame (0-based).
        int newStart = frame - 1;
        m_playStartFrame = newStart;
        if (m_sound && m_hasSoundtrack && mainXsh2) {
          ToonzScene *sc3 = TApp::instance()->getCurrentScene()->getScene();
          double fps3 = sc3
              ? sc3->getProperties()->getOutputProperties()->getFrameRate()
              : 24.0;
          double spf3 = m_sound->getSampleRate() / fps3;
          TINT32 startSmp = (TINT32)(newStart * spf3);
          TINT32 totalSmp = (TINT32)m_sound->getSampleCount();
          int stopFr = videoFrameCount(mainXsh2);
          if (sc3 && sc3->getChildStack()->getAncestorCount() == 0) {
            int mr0, mr1, mstep;
            if (XsheetGUI::getPlayRange(mr0, mr1, mstep) && mr1 >= 0)
              stopFr = mr1 + 1;
          }
          TINT32 endSmp = std::min((TINT32)(stopFr * spf3), totalSmp - 1);
          if (startSmp <= endSmp) {
            mainXsh2->play(m_sound, startSmp, endSmp, false);
            m_continuousPlay = true;
          }
        }
        targetFrame = newStart;
      } else {
        // Audio-master clock: use QAudioOutput::processedUSecs() as the
        // authoritative time source.  This is the hardware DAC clock and cannot
        // drift relative to the actual audio output.  It eliminates A/V desync
        // caused by timer jitter in FlipConsole or slow frame rendering.
        //
        //   targetFrame = playStartFrame + floor(audioElapsed_s * fps)
        //
        // During the first few milliseconds after play() the device may not have
        // started yet (processedUsecs = 0).  In that case fall back to the
        // FlipConsole frame (1-based → 0-based) so the first frames are still
        // displayed while the DAC initialises.
        TXsheet *mainXsh = ctrl->mainXsheet();
        qint64 audioUsecs = mainXsh ? mainXsh->getAudioPlayedUSecs() : 0;
        if (audioUsecs > 0) {
          targetFrame = m_playStartFrame + (int)(audioUsecs * m_fps / 1000000.0);
          int totalFrames = videoFrameCount(mainXsh);
          // Also clamp to mark-out so that loop respects it.
          // Only apply the mark-out when at the top (main xsheet) level;
          // inside a sub-scene, updateAnimaticFrameMarkers() cleared markers.
          int markOut = totalFrames - 1;
          {
            ToonzScene *sc2 = TApp::instance()->getCurrentScene()->getScene();
            if (sc2 && sc2->getChildStack()->getAncestorCount() == 0) {
              int mr0, mr1, mstep;
              if (XsheetGUI::getPlayRange(mr0, mr1, mstep) && mr1 >= 0)
                markOut = mr1;
            }
          }
          targetFrame = std::max(0, std::min(targetFrame, markOut));
        } else {
          // DAC not yet started — use FlipConsole frame as fallback.
          targetFrame = frame - 1;
        }
      }
    } else {
      // Not playing (scrubbing) — use FlipConsole frame directly.
      targetFrame = frame - 1;  // 1-based → 0-based
    }
    m_prevFlipFrame = frame;

    int oldFrame = ctrl->currentFrame();
    ctrl->setCurrentFrame(targetFrame);

    // Audio scrub when scrubbing (not during play) — mirrors native onDrawFrame.
    if (!ctrl->frameHandle()->isPlaying() && oldFrame != targetFrame) {
      TXsheet *xsh = ctrl->mainXsheet();
      if (xsh) {
        ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
        double fps = scene
            ? scene->getProperties()->getOutputProperties()->getFrameRate()
            : 24.0;
        ctrl->frameHandle()->scrubXsheet(targetFrame, targetFrame, xsh, fps);
      }
    }
  } else if (settings.m_blankColor != TPixel::Transparent) {
    if (m_sceneViewer) m_sceneViewer->update();
  }
}

// ---- updateAnimaticFrameRange ----
// Sets the FlipConsole play range from the MAIN xsheet total frame count.
// The base updateFrameRange() uses TApp::getCurrentFrame()->getMaxFrameIndex()
// which, when inside a sub-scene, returns the sub-scene's (shorter) frame
// count — causing play to stop too early.
void ZtoryAnimaticViewer::updateAnimaticFrameRange() {
  auto *ctrl       = ZtoryAnimaticController::instance();
  TXsheet *mainXsh = ctrl->mainXsheet();
  if (!mainXsh) return;

  // Compute frame count from VIDEO columns only — skip sound columns.
  // mainXsh->getFrameCount() includes audio columns; after a razor cut the
  // trailing ColumnLevel keeps endOffset=0 (= full raw file length, potentially
  // hours), inflating the count and making the ruler/cursor jump to the right.
  int totalFrames = 0;
  for (int c = 0; c < mainXsh->getColumnCount(); c++) {
    TXshColumn *col = mainXsh->getColumn(c);
    if (!col || col->getSoundColumn()) continue; // skip audio
    int r0, r1;
    if (col->getRange(r0, r1)) totalFrames = std::max(totalFrames, r1 + 1);
  }
  if (totalFrames <= 0) totalFrames = mainXsh->getFrameCount();
  if (totalFrames <= 0) totalFrames = 1;

  int currentFrame = ctrl->currentFrame();  // 0-based
  // Clamp: currentFrame can exceed totalFrames if audio was longer than video
  // during the last play session (onDrawFrame advances it via audio clock).
  currentFrame = qBound(0, currentFrame, totalFrames - 1);
  // FlipConsole expects 1-based from/to/current values.
  m_flipConsole->setFrameRange(1, totalFrames, 1, currentFrame + 1);
}

// ---- updateAnimaticFrameMarkers ----
// Sets the FlipConsole in/out markers from the MAIN xsheet play range.
// When inside a sub-scene, ToonzScene::getPreviewProperties() has been swapped
// by subscenecommand.cpp to the sub-scene's play range.  The animatic must
// always use the main xsheet markers (or no markers if none are set on main).
void ZtoryAnimaticViewer::updateAnimaticFrameMarkers() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) {
    m_flipConsole->setMarkers(0, -1);  // no markers → full range
    return;
  }
  if (scene->getChildStack()->getAncestorCount() > 0) {
    // Inside a sub-scene: ignore sub-scene markers, play full animatic range.
    m_flipConsole->setMarkers(0, -1);
  } else {
    // At main level: use whatever play range is set on the main xsheet.
    int r0, r1, step;
    XsheetGUI::getPlayRange(r0, r1, step);
    m_flipConsole->setMarkers(r0, r1);
  }
}

// ---- refreshAnimaticSound ----
// Sets m_sound / m_hasSoundtrack from the controller's CACHED sound track.
//
// IMPORTANT: do NOT call makeSound() here.
// makeSound() for a long audio file can take 1–3 seconds. It is called from
// onAnimaticPlayingStatusChanged() which fires AFTER PlaybackExecutor has
// already started. Because the executor uses BlockingQueuedConnection, it
// blocks waiting for the main thread, accumulating the makeSound() delay.
// When it finally unblocks it skips frames to catch up — but audio starts
// from the original frame, creating an audio-vs-video desync of several
// seconds.  Using the pre-built cache (requireSoundTrack) is instantaneous
// and eliminates the skip entirely.
void ZtoryAnimaticViewer::refreshAnimaticSound() {
  m_sound         = nullptr;
  m_hasSoundtrack = false;
  m_first         = true;
  // requireSoundTrack() builds lazily on first call and caches the result.
  // The cache is invalidated (set to null) whenever audio ColumnLevels change.
  auto *ctrl       = ZtoryAnimaticController::instance();
  m_soundTrackRef  = ctrl->requireSoundTrack(); // keep our own ref alive
  m_sound          = m_soundTrackRef.getPointer(); // safe: ref keeps object alive
  m_hasSoundtrack  = (m_sound != nullptr);
}

// ---- playAnimaticAudioFrame ----
// Used ONLY for per-frame scrub audio (ruler drag, preview bar drag).
// During continuous play the audio streams on its own — this function is a
// no-op so it does not interrupt or replace the streaming buffer.
void ZtoryAnimaticViewer::playAnimaticAudioFrame(int frame) {
  if (m_continuousPlay) return;   // audio already streaming — do nothing
  if (!m_sound) return;
  if (m_first) {
    m_first           = false;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    m_fps             = scene->getProperties()->getOutputProperties()
                            ->getFrameRate();
    m_samplesPerFrame = m_sound->getSampleRate() / std::abs(m_fps);
  }
  TXsheet *mainXsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!mainXsh) return;
  m_viewerFps = m_flipConsole->getCurrentFps();
  double s0   = frame * m_samplesPerFrame;
  double s1   = s0 + m_samplesPerFrame;
  if (m_fps < m_viewerFps) mainXsh->stopScrub();
  mainXsh->play(m_sound, s0, s1, false);
}

// ---- onAnimaticPlayingStatusChanged ----
// Runs AFTER base's onPlayingStatusChanged (both connected to playStateChanged).
// Base calls hasSoundtrack() using the sub-scene xsheet → m_sound = null.
// We immediately override with the correct main-xsheet sound, then start a
// single continuous-play call from the current frame to end-of-track.
// Continuous play avoids per-frame buffer replacement (which causes glitches).
void ZtoryAnimaticViewer::onAnimaticPlayingStatusChanged(bool playing) {
  TXsheet *mainXsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!playing) {
    // Stop the continuous audio stream.
    m_continuousPlay = false;
    m_prevFlipFrame  = 0;   // reset so next play starts fresh
    if (mainXsh) mainXsh->stopScrub();
    // Clamp ctrl frame to valid video range — audio clock can advance it past
    // the video end if audio is longer than video.
    if (mainXsh) {
      auto *ctrl2 = ZtoryAnimaticController::instance();
      int videoFrames = 0;
      for (int c = 0; c < mainXsh->getColumnCount(); c++) {
        TXshColumn *col = mainXsh->getColumn(c);
        if (!col || col->getSoundColumn()) continue;
        int r0, r1;
        if (col->getRange(r0, r1)) videoFrames = std::max(videoFrames, r1 + 1);
      }
      if (videoFrames > 0) {
        int cur = ctrl2->currentFrame();
        if (cur >= videoFrames) ctrl2->setCurrentFrame(videoFrames - 1);
      }
    }
    return;
  }

  // Rebuild m_sound from the main xsheet (base's hasSoundtrack() used the
  // wrong sub-scene xsheet, giving null m_sound and m_hasSoundtrack=false).
  refreshAnimaticSound();

  if (!m_sound || !m_hasSoundtrack || !mainXsh) return;

  // Compute the sample position that corresponds to the current animatic frame.
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  double spf           = m_sound->getSampleRate() / fps;
  int startFrame       = ZtoryAnimaticController::instance()->currentFrame();
  TINT32 startSample  = (TINT32)(startFrame * spf);
  TINT32 totalSamples = (TINT32)m_sound->getSampleCount();
  if (startSample >= totalSamples) return;

  // Cap endSample at the animatic timeline length, NOT the raw audio file
  // length.  After a razor cut the trailing ColumnLevel keeps endOffset=0, so
  // its visibleEndFrame equals the raw file length (potentially hours).
  // makeSound() then produces a track with getSampleCount() = raw file size.
  // play(st, s0, rawEnd) calls m_buffer.resize(rawBytes) + memcpy which can
  // allocate/copy hundreds of MB — causing 1-3 s startup delay while the
  // PlaybackExecutor has already advanced video frames, causing A/V desync.
  int    animFrames      = videoFrameCount(mainXsh);
  // Respect mark-out: if a play range is set at the top level, cap audio there.
  int stopFrame = animFrames;
  if (scene->getChildStack()->getAncestorCount() == 0) {
    int mr0, mr1, mstep;
    if (XsheetGUI::getPlayRange(mr0, mr1, mstep) && mr1 >= 0)
      stopFrame = mr1 + 1;  // exclusive end
  }
  TINT32 animEndSample   = (TINT32)(stopFrame * spf);
  TINT32 endSample       = std::min(animEndSample, totalSamples - 1);
  if (startSample > endSample) return;

  // Also cache fps/samplesPerFrame for any scrub calls that happen before
  // the next refreshAnimaticSound (e.g., if play is cancelled mid-frame).
  m_fps             = fps;
  m_samplesPerFrame = spf;
  m_first           = false;

  // Record the 0-based animatic frame at play-start so that onDrawFrame can
  // compute the audio-master target frame: targetFrame = m_playStartFrame +
  // (int)(audioUsecs * fps / 1e6).  Captured BEFORE play() so that
  // processedUsecs() = 0 at the moment the first onDrawFrame fires.
  m_playStartFrame = startFrame;

  // Start streaming audio from current frame to animatic end in one shot.
  // TSoundOutputDeviceImp uses QAudioOutput with a 100 ms hardware buffer,
  // refilled every 50 ms via QAudioOutput::notify.  One call avoids the
  // per-frame m_buffer replacement that caused glitches in the old approach.
  mainXsh->play(m_sound, startSample, endSample, false);
  m_continuousPlay = true;
}

// ---- restartAudioIfPlaying ----
// Called by the panel after mute/solo changes.  If continuous play is active,
// rebuilds the merged sound track with updated volumes and calls play()
// directly.  m_soundTrackRef keeps the OLD TSoundTrack alive (refcount > 0)
// until refreshAnimaticSound() atomically replaces it — so m_sound is always
// valid and we never hand the audio device a dangling pointer.
void ZtoryAnimaticViewer::restartAudioIfPlaying() {
  if (!m_continuousPlay) return;
  auto *ctrl = ZtoryAnimaticController::instance();
  TXsheet *mainXsh = ctrl->mainXsheet();
  if (!mainXsh) return;

  // Ensure QAudioOutput is fully idle before rebuilding.
  // stopScrub() clears m_buffer so that any pending sendBuffer() callbacks
  // return immediately without writing stale data.
  mainXsh->stopScrub();

  int startFrame = ctrl->currentFrame();

  // Rebuild sound with updated volumes (caches already invalidated by caller).
  // refreshAnimaticSound() swaps m_soundTrackRef atomically: old ref kept alive
  // until the assignment, so m_sound never points to freed memory.
  refreshAnimaticSound();
  if (!m_sound) return;

  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  double fps = scene->getProperties()->getOutputProperties()->getFrameRate();
  if (fps <= 0.0) fps = 24.0;
  double spf = m_sound->getSampleRate() / fps;

  TINT32 startSample  = (TINT32)(startFrame * spf);
  TINT32 totalSamples = (TINT32)m_sound->getSampleCount();
  if (startSample >= totalSamples) return;

  int stopFrame = videoFrameCount(mainXsh);
  if (scene->getChildStack()->getAncestorCount() == 0) {
    int mr0, mr1, mstep;
    if (XsheetGUI::getPlayRange(mr0, mr1, mstep) && mr1 >= 0)
      stopFrame = mr1 + 1;
  }
  TINT32 endSample = std::min((TINT32)(stopFrame * spf), totalSamples - 1);
  if (startSample > endSample) return;

  m_fps             = fps;
  m_samplesPerFrame = spf;
  m_first           = false;
  m_playStartFrame  = startFrame;

  // play() overwrites the QAudioOutput buffer in-place — no stop/restart.
  // QAudioOutput continues streaming; new data replaces the old within ~100ms.
  mainXsh->play(m_sound, startSample, endSample, false);
}

void ZtoryAnimaticViewer::showEvent(QShowEvent *e) {
  // BaseViewerPanel::showEvent connects TApp::getCurrentFrame() signals to
  // slots such as updateFrameRange() and changeWindowTitle().  Those would
  // set the FlipConsole range from the sub-scene's frame count (wrong).
  BaseViewerPanel::showEvent(e);

  TApp *app  = TApp::instance();
  auto *ctrl = ZtoryAnimaticController::instance();

  // Remove every connection from the global frame handle to this viewer.
  // In particular this removes frameSwitched→updateFrameRange which would
  // continuously reset our range to the sub-scene frame count during play.
  disconnect(app->getCurrentFrame(), nullptr, this, nullptr);

  // After base's onSceneChanged() (triggered by sceneSwitched/sceneChanged)
  // calls updateFrameRange() and updateFrameMarkers() with wrong values
  // (sub-scene data), run our corrections right after.
  // hideEvent() (from base) disconnects all sceneHandle→this signals, so
  // these connections are cleaned up automatically on hide.
  connect(app->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameRange);
  connect(app->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);
  connect(app->getCurrentScene(), &TSceneHandle::sceneChanged,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameRange);
  connect(app->getCurrentScene(), &TSceneHandle::sceneChanged,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);

  // ctrl frame handle: re-add the range-update connection, removing any
  // previous one first to avoid accumulation across show/hide cycles.
  if (m_frameRangeConn) disconnect(m_frameRangeConn);
  m_frameRangeConn = connect(ctrl->frameHandle(), &TFrameHandle::frameSwitched,
                             this, &ZtoryAnimaticViewer::updateAnimaticFrameRange);

  // Ruler In/Out marker changes: keep FlipConsole markers in sync.
  // hideEvent() from base disconnects all sceneHandle→this signals but NOT ctrl→this,
  // so disconnect first to avoid accumulating connections across show/hide cycles.
  disconnect(ctrl, &ZtoryAnimaticController::playRangeChanged,
             this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);
  connect(ctrl, &ZtoryAnimaticController::playRangeChanged,
          this, &ZtoryAnimaticViewer::updateAnimaticFrameMarkers);

  // Audio: when play starts, override m_sound with main-xsheet sound.
  // Disconnect first to avoid accumulation; then reconnect.
  disconnect(m_flipConsole, SIGNAL(playStateChanged(bool)),
             this, SLOT(onAnimaticPlayingStatusChanged(bool)));
  connect(m_flipConsole, SIGNAL(playStateChanged(bool)),
          this, SLOT(onAnimaticPlayingStatusChanged(bool)));

  // Audio: play sound from main xsheet on each animatic frame change during play.
  if (m_audioConn) disconnect(m_audioConn);
  m_audioConn = connect(ctrl->frameHandle(), &TFrameHandle::frameSwitched,
                        this, [this]() {
    if (!m_playing || !m_playSound || !m_hasSoundtrack) return;
    int frame = ZtoryAnimaticController::instance()->currentFrame();
    playAnimaticAudioFrame(frame);
  });

  // Set correct range and markers immediately (overrides what base just set).
  updateAnimaticFrameRange();
  updateAnimaticFrameMarkers();

  // Pre-build the merged sound track while the panel is showing — before the
  // user presses play.  requireSoundTrack() is a no-op if the track is already
  // cached.  Building it here means refreshAnimaticSound() (called at
  // play-start) is instant: it just reads the cached raw pointer.
  // This eliminates the 1-3 s makeSound() call that was happening on the
  // main thread while the PlaybackExecutor had already started running,
  // causing the executor to skip frames and creating audio-video desync.
  QTimer::singleShot(0, this, [this]() {
    // Deferred to after the panel fully shows (avoids blocking the show event).
    auto *ctrl = ZtoryAnimaticController::instance();
    TSoundTrackP st = ctrl->requireSoundTrack();
    // Pre-warm the QAudioOutput device so it is already initialised when play
    // starts.  Playing 1 sample initialises the device without audible output;
    // stopScrub() clears the buffer but leaves the device alive (idle state).
    // A subsequent full-track play() call reuses the device with zero startup
    // latency because the format is unchanged.
    TXsheet *mainXsh = ctrl->mainXsheet();
    if (st && mainXsh && st->getSampleCount() > 0)
      mainXsh->play(st, 0, 0, false);  // 1 silent sample → device initialised
  });

  if (m_sceneViewer) m_sceneViewer->update();
}

// ---- ZtoryAnimaticViewerPanel ----

ZtoryAnimaticViewerPanel::ZtoryAnimaticViewerPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle("Ztory Viewer");
  m_viewer = new ZtoryAnimaticViewer(this);
  m_viewer->setMinimumHeight(120);
  setWidget(m_viewer);
}

void ZtoryAnimaticViewerPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  m_viewer->show();
}

// ---- ZtoryStoryStripPanel ----

ZtoryStoryStripPanel::ZtoryStoryStripPanel(QWidget *parent)
    : TPanel(parent) {
  setWindowTitle("Ztory Story Strip");
  m_strip = new ZtoryStoryStrip(this);
  setWidget(m_strip);

  // Refresh when scene or xsheet changes
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, &ZtoryStoryStripPanel::refreshFromScene);
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene && scene->getChildStack()->getAncestorCount() == 0)
      refreshFromScene();
  });
  // Highlight current column when column selection changes
  connect(TApp::instance()->getCurrentColumn(), &TColumnHandle::columnIndexSwitched,
          this, [this]() {
    m_strip->setCurrentCol(TApp::instance()->getCurrentColumn()->getColumnIndex());
  });
  // Clicking a shot in the strip selects its column
  connect(m_strip, &ZtoryStoryStrip::shotClicked,
          this, [](int col) {
    TApp::instance()->getCurrentColumn()->setColumnIndex(col);
  });
}

void ZtoryStoryStripPanel::refreshFromScene() {
  m_strip->refreshFromScene();
}

void ZtoryStoryStripPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  refreshFromScene();
}

// ---- ZtoryAnimaticPanel (timeline only) ----

ZtoryAnimaticPanel::ZtoryAnimaticPanel(QWidget *parent) : TPanel(parent) {
  setWindowTitle("Ztory Animatic");
  setFocusPolicy(Qt::StrongFocus);

  QWidget *container = new QWidget(this);
  QVBoxLayout *lay = new QVBoxLayout(container);
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);

  m_ruler = new ZtoryAnimaticRuler(container);
  m_track = new ZtoryAnimaticTrack(container);

  // Toolbar
  QWidget *toolbar = new QWidget(container);
  toolbar->setFixedHeight(28);
  toolbar->setStyleSheet("background:#2a2a2a;");
  QHBoxLayout *tbLay = new QHBoxLayout(toolbar);
  tbLay->setContentsMargins(6, 2, 6, 2);
  tbLay->setSpacing(4);
  QLabel *zoomLabel = new QLabel("Zoom:", toolbar);
  zoomLabel->setStyleSheet("color:#ccc; font-size:11px;");
  m_zoomSlider = new QSlider(Qt::Horizontal, toolbar);
  m_zoomSlider->setRange(2, 640);   // min 0.2 ppf — allows seeing very long audio tracks
  m_zoomSlider->setValue((int)(m_ppf * 10));
  m_zoomSlider->setMaximumWidth(160);
  m_zoomSlider->setToolTip("Zoom (pixels per frame)");
  tbLay->addWidget(zoomLabel);
  tbLay->addWidget(m_zoomSlider);
  tbLay->addSpacing(12);

  QToolButton *selectBtn = new QToolButton(toolbar);
  selectBtn->setIcon(createQIcon("ztoryc_select"));
  selectBtn->setIconSize(QSize(20, 20));
  selectBtn->setFixedSize(28, 28);
  selectBtn->setToolTip(tr("Select"));
  selectBtn->setCheckable(true);
  selectBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");
  selectBtn->setChecked(true);

  QToolButton *razorBtn = new QToolButton(toolbar);
  razorBtn->setIcon(createQIcon("ztoryc_razor"));
  razorBtn->setIconSize(QSize(20, 20));
  razorBtn->setFixedSize(28, 28);
  razorBtn->setToolTip(tr("Razor"));
  razorBtn->setCheckable(true);
  razorBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");

  QToolButton *linkBtn = new QToolButton(toolbar);
  linkBtn->setIcon(createQIcon("ztoryc_av_link"));
  linkBtn->setIconSize(QSize(20, 20));
  linkBtn->setFixedSize(28, 28);
  linkBtn->setToolTip(tr("A/V Link"));
  linkBtn->setCheckable(true);
  linkBtn->setChecked(true);
  linkBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");
  linkBtn->setToolTip("Link/Unlink audio and video tracks");
  linkBtn->setToolTip("Link/Unlink audio and video tracks");

  QToolButton *mergeBtn = new QToolButton(toolbar);
  mergeBtn->setIcon(createQIcon("ztoryc_merge"));
  mergeBtn->setIconSize(QSize(20, 20));
  mergeBtn->setFixedSize(28, 28);
  mergeBtn->setToolTip(tr("Merge Shots"));
  mergeBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");
  mergeBtn->setToolTip("Merge selected shots into the first selected shot");

  QToolButton *addShotBtn = new QToolButton(toolbar);
  addShotBtn->setIcon(createQIcon("ztoryc_add_shot"));
  addShotBtn->setIconSize(QSize(20, 20));
  addShotBtn->setFixedSize(28, 28);
  addShotBtn->setToolTip(tr("Add Shot"));
  addShotBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");

  tbLay->addWidget(selectBtn);
  tbLay->addWidget(razorBtn);
  tbLay->addSpacing(8);
  tbLay->addWidget(linkBtn);
  tbLay->addSpacing(8);
  connect(addShotBtn, &QToolButton::clicked,
          this, &ZtoryAnimaticPanel::onAddShot);
  tbLay->addWidget(addShotBtn);
  tbLay->addWidget(mergeBtn);
  tbLay->addSpacing(12);

  // Onion skin toggle — controls the animatic's local onion state,
  // independent from the native timeline.
  QToolButton *onionBtn = new QToolButton(toolbar);
  onionBtn->setIcon(createQIcon("ztoryc_onion"));
  onionBtn->setIconSize(QSize(20, 20));
  onionBtn->setFixedSize(28, 28);
  onionBtn->setCheckable(true);
  onionBtn->setChecked(false);
  onionBtn->setToolTip(tr("Toggle Onion Skin"));
  onionBtn->setStyleSheet("QToolButton{background:transparent;border:none;border-radius:4px;}QToolButton:hover{background:#555;}QToolButton:checked{background:#666;}");
  tbLay->addWidget(onionBtn);
  tbLay->addStretch(1);

  // Ruler ↔ button sync (ruler can auto-enable when first marker is placed)
  connect(m_ruler, &ZtoryAnimaticRuler::onionEnabledChanged,
          toolbar, [onionBtn](bool on) {
    onionBtn->blockSignals(true);
    onionBtn->setChecked(on);
    onionBtn->blockSignals(false);
  });
  connect(onionBtn, &QToolButton::toggled, m_ruler,
          &ZtoryAnimaticRuler::setOnionEnabled);

  connect(selectBtn, &QToolButton::clicked, this, [this, selectBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::SelectTool);
    selectBtn->setChecked(true);
    razorBtn->setChecked(false);
    for (auto *at : m_audioTracks) at->setRazorActive(false);
  });
  connect(razorBtn, &QToolButton::clicked, this, [this, selectBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::RazorTool);
    razorBtn->setChecked(true);
    selectBtn->setChecked(false);
    // Always activate razor on audio tracks so the user can cut them directly.
    // m_audioLinked only controls whether a video cut also cuts audio.
    for (auto *at : m_audioTracks) at->setRazorActive(true);
  });
  connect(linkBtn, &QToolButton::toggled, this, [this](bool checked){
    m_audioLinked = checked;
  });
  connect(mergeBtn, &QToolButton::clicked, this, &ZtoryAnimaticPanel::onMergeShots);

  // Scroll area with ruler + track + audio
  QScrollArea *scroll = new QScrollArea(container);
  m_scrollContent = new QWidget();
  m_scrollLay = new QVBoxLayout(m_scrollContent);
  m_scrollLay->setContentsMargins(0, 0, 0, 0);
  m_scrollLay->setSpacing(0);
  m_scrollLay->addWidget(m_ruler);
  m_scrollLay->addWidget(m_track);
  m_scrollLay->addStretch(1);
  scroll->setWidget(m_scrollContent);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
  scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  // Layout: toolbar on top, scroll below
  lay->addWidget(toolbar);
  lay->addWidget(scroll, 1);
  setWidget(container);

  connect(m_ruler, &ZtoryAnimaticRuler::frameChanged,
          this, &ZtoryAnimaticPanel::onFrameChanged);
  connect(m_track, &ZtoryAnimaticTrack::shotClicked,
          this, &ZtoryAnimaticPanel::onShotClicked);
  connect(m_track, &ZtoryAnimaticTrack::shotDoubleClicked,
          this, &ZtoryAnimaticPanel::onShotDoubleClicked);
  connect(m_track, &ZtoryAnimaticTrack::zoomChanged,
          this, &ZtoryAnimaticPanel::onZoomChanged);
  connect(m_zoomSlider, &QSlider::valueChanged, this, [this](int v){
    onZoomChanged(v / 10.0);
  });
  connect(m_track, &ZtoryAnimaticTrack::matchSubsceneDuration,
          this, &ZtoryAnimaticPanel::onMatchSubsceneDuration);
  connect(m_track, &ZtoryAnimaticTrack::razorRequested,
          this, &ZtoryAnimaticPanel::onRazorRequested);
  connect(m_track, &ZtoryAnimaticTrack::mergeWithNextRequested,
          this, &ZtoryAnimaticPanel::onMergeWithNext);
  connect(m_track, &ZtoryAnimaticTrack::returnToMainRequested,
          this, &ZtoryAnimaticPanel::onReturnToMain);
  connect(m_track, &ZtoryAnimaticTrack::shotDurationChanged,
          this, &ZtoryAnimaticPanel::onShotDurationChanged);
  connect(m_track, &ZtoryAnimaticTrack::shotMoved,
          this, &ZtoryAnimaticPanel::onShotMoved);
  // Video lock state — no persistence map needed (single track)
  // The lock state lives directly on m_track and survives refreshFromScene()
  // because m_track itself is not rebuilt (only its blocks are refreshed).

  // BUG-02 fix: sceneSwitched fires when entering a sub-scene too.
  // Without this guard, refreshFromScene() would call getTopXsheet() which
  // returns the sub-scene, causing the timeline to show its columns instead
  // of the main shot-list.  Same guard already used for xsheetChanged below.
  connect(TApp::instance()->getCurrentScene(), &TSceneHandle::sceneSwitched,
          this, [this]() {
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    if (scene->getChildStack()->getAncestorCount() != 0) return;
    refreshFromScene();
  });
  // Animatic panel listens to the controller's dedicated frame handle,
  // NOT the global TApp frame.  This decouples the animatic playhead from
  // the native timeline cursor (BUG-03 fix).
  auto *animCtrl = ZtoryAnimaticController::instance();
  connect(animCtrl->frameHandle(), &TFrameHandle::frameSwitched,
          this, [this](){
    int frame = ZtoryAnimaticController::instance()->currentFrame();
    m_ruler->setCurrentFrame(frame);
    m_track->setCurrentFrame(frame);
    for (auto *at : m_audioTracks)
      at->setCurrentFrame(frame);
  });
  connect(TApp::instance()->getCurrentXsheet(), &TXsheetHandle::xsheetChanged,
          this, [this](){
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (scene && scene->getChildStack()->getAncestorCount() == 0)
      refreshFromScene();
  });
  // 13a: refresh ruler when onion skin changes (markers update)
  connect(TApp::instance()->getCurrentOnionSkin(),
          SIGNAL(onionSkinMaskChanged()), m_ruler, SLOT(update()));
}

void ZtoryAnimaticPanel::refreshFromScene() {
  if (m_refreshing) return;
  m_refreshing = true;
  m_track->refreshFromScene();
  refreshAudioTracks();
  updateTrackWidths();
  m_refreshing = false;
}

void ZtoryAnimaticPanel::updateTrackWidths() {
  // Use video block extents only.  Audio ColumnLevel extents are NOT used here:
  // after a razor cut the right segment's endOffset=0 makes getVisibleEndFrame()
  // return the raw file duration (potentially minutes/hours), inflating totalW
  // and making the ruler/cursor appear to jump to that frame.
  // Animatic length is driven by video shots, not audio placement.
  int maxFrame = 0;
  for (auto &b : m_track->blocks())
    maxFrame = qMax(maxFrame, b.startFrameInMain + (b.f1 - b.f0 + 1));
  if (maxFrame <= 0) maxFrame = 1;
  int totalW = kLabelW + (int)(maxFrame * m_ppf) + 200;
  m_ruler->setMinimumWidth(totalW);
  m_track->setMinimumWidth(totalW);
  for (auto *at : m_audioTracks)
    at->setMinimumWidth(totalW);
}

void ZtoryAnimaticPanel::updateCutFrames() {
  QVector<int> cutFrames;
  for (auto &b : m_track->blocks()) {
    if (b.startFrameInMain > 0)
      cutFrames.append(b.startFrameInMain);
  }
  for (auto *at : m_audioTracks)
    at->setCutFrames(cutFrames);
}

void ZtoryAnimaticPanel::applyMuteSolo() {
  // Visual state is already set on the widgets by the click handler.
  // Apply effective volume to each xsheet sound column.
  // Solo logic: if any track is solo'd, only solo tracks are audible.
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  TXsheet *xsh = scene ? scene->getChildStack()->getTopXsheet() : nullptr;
  if (!xsh) return;

  // Check if any track is solo'd
  bool hasSolo = false;
  for (auto *at : m_audioTracks)
    if (at->isSolo()) { hasSolo = true; break; }

  for (auto *at : m_audioTracks) {
    int col = at->columnIndex();
    // M wins over S: a muted track is silent even if solo'd.
    // A non-solo track is silenced when any other track is solo'd.
    bool effectiveMute = at->isMuted() || (hasSolo && !at->isSolo());
    // Dim waveform for solo-silenced tracks (M already shown via button color)
    at->setEffectiveMuted(hasSolo && !at->isSolo());
    TXshColumn *xshCol = xsh->getColumn(col);
    TXshSoundColumn *sc = xshCol ? xshCol->getSoundColumn() : nullptr;
    if (sc) sc->setVolume(effectiveMute ? 0.0 : 1.0);
  }
  // Invalidate BOTH caches: TXsheet's internal m_mixedSound AND our controller
  // cache. Without xsh->invalidateSound(), makeSound() returns stale data.
  xsh->invalidateSound();
  auto *ctrl = ZtoryAnimaticController::instance();
  ctrl->invalidateSoundTrack();

  // Stop the current audio stream NOW (main thread, safe).
  // This ensures the QAudioOutput is not in Active state when play() is called
  // again: on macOS, QAudioOutput::notify can fire from CoreAudio's internal
  // thread.  If play() runs concurrently with a notify→sendBuffer() callback,
  // m_audioBuffer->write() and m_buffer.resize() race → EXC_BAD_ACCESS.
  // Stopping here drains any in-flight CoreAudio callbacks synchronously.
  TXsheet *mainXsh2 = ctrl->mainXsheet();
  if (mainXsh2) mainXsh2->stopScrub();

  // Defer the restart to the next event-loop iteration so that any queued
  // QAudioOutput signals (notify, stateChanged) already in the queue are
  // delivered BEFORE we call play() with the new buffer.  This eliminates
  // the window where CoreAudio callbacks race against buffer replacement.
  if (ctrl->viewer()) {
    ZtoryAnimaticViewer *viewer = ctrl->viewer();
    QTimer::singleShot(0, viewer, [viewer]() {
      viewer->restartAudioIfPlaying();
    });
  }
}

void ZtoryAnimaticPanel::restoreTrackStates() {
  // Called after refreshAudioTracks() rebuilds widgets — re-apply persisted
  // UI state (button checked/unchecked) only.  Do NOT call applyMuteSolo()
  // here: it triggers invalidateSound() + restartAudioIfPlaying() which can
  // destroy the sound track while the audio device thread is still playing,
  // causing heap corruption / SIGSEGV.  The column volumes were already set
  // before the rebuild and remain valid.
  for (auto *at : m_audioTracks) {
    int col = at->columnIndex();
    if (m_colLocked.value(col, false)) at->setLocked(true);
    if (m_colMuted.value(col, false))  at->setMuted(true);
    if (m_colSolo.contains(col))       at->setSolo(true);
  }
}

void ZtoryAnimaticPanel::refreshAudioTracks() {
  if (m_refreshingAudio) return;

  // Check preconditions BEFORE setting the re-entrancy guard so that an early
  // return never leaves m_refreshingAudio stuck at true.
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  m_refreshingAudio = true;

  // Rimuovi tracce audio esistenti.
  // cancelDrag() prevents spurious shiftLevelInRange commits on stale widgets.
  // deleteLater() (not delete) defers destruction until after the current
  // event finishes — avoids SIGABRT when refreshAudioTracks() is called
  // from inside a ZtoryAudioTrack event handler (e.g. razorRequested).
  for (auto *at : m_audioTracks) {
    at->cancelDrag();
    m_scrollLay->removeWidget(at);
    at->hide();       // hide immediately — deleteLater defers destruction but
    at->deleteLater(); // the widget stays visible until the event loop ticks
  }
  m_audioTracks.clear();

  // Trova tutte le colonne audio nel main xsheet
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    TXshSoundColumn *sc = column->getSoundColumn();
    if (!sc) continue;

    QString name = QString::fromStdString(xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (name.isEmpty()) name = tr("Audio %1").arg(col + 1);

    ZtoryAudioTrack *at = new ZtoryAudioTrack(col, name, m_scrollContent);
    at->setPixelsPerFrame(m_ppf);
    at->setCurrentFrame(m_track->property("currentFrame").toInt());
    // Propagate current razor state to newly created audio track.
    // m_audioLinked only controls whether a video cut also cuts audio;
    // audio tracks are always razerable when the razor tool is active.
    at->setRazorActive(m_track->tool() == ZtoryAnimaticTrack::RazorTool);

    // Cut frames will be set by updateCutFrames() after all tracks are created

    // Sync razor hover across video ↔ audio tracks
    connect(m_track, &ZtoryAnimaticTrack::razorHoverFrameChanged,
            at, &ZtoryAudioTrack::setRazorHoverFrame);

    connect(at, &ZtoryAudioTrack::razorRequested,
            this, &ZtoryAnimaticPanel::onAudioRazorRequested);
    // segmentMoved: audio segment was dragged. Notify the xsheet via a queued
    // (deferred) call so we are NOT inside the widget's event handler when
    // notifyXsheetChanged fires — that would trigger refreshAudioTracks() and
    // delete the widget mid-event.
    connect(at, &ZtoryAudioTrack::segmentMoved, this, [this]() {
      // NOTE: do NOT call xsh->updateFrameCount() here. Audio moves do not
      // change the animatic length (determined by video shots). Calling it
      // would include the long trailing ColumnLevel extent after a razor cut,
      // making getFrameCount() huge and causing the ruler/cursor to jump right.
      TApp *app = TApp::instance();
      app->getCurrentScene()->notifyCastChange();
      updateCutFrames();
      updateTrackWidths();
    }, Qt::QueuedConnection);

    connect(at, &ZtoryAudioTrack::segmentDroppedOutside,
            this, &ZtoryAnimaticPanel::onSegmentDroppedOutside,
            Qt::QueuedConnection);

    // Lock / mute / solo signals
    connect(at, &ZtoryAudioTrack::lockedChanged,
            this, [this](int col, bool on){ m_colLocked[col] = on; });
    connect(at, &ZtoryAudioTrack::muteToggleRequested, this, [this, at](int col){
      // Persist so restoreTrackStates() can re-apply after refreshAudioTracks()
      m_colMuted[col] = at->isMuted();
      applyMuteSolo();
    });
    connect(at, &ZtoryAudioTrack::soloToggleRequested, this, [this, at](int col){
      // Persist so restoreTrackStates() can re-apply after refreshAudioTracks()
      if (at->isSolo()) m_colSolo.insert(col);
      else              m_colSolo.remove(col);
      applyMuteSolo();
    });

    // Inserisci prima dello stretch finale
    int insertIdx = m_scrollLay->count() - 1;
    if (insertIdx < 0) insertIdx = 0;
    m_scrollLay->insertWidget(insertIdx, at);
    m_audioTracks.append(at);
  }
  updateCutFrames();
  restoreTrackStates();
  m_refreshingAudio = false;
}

void ZtoryAnimaticPanel::showEvent(QShowEvent *e) {
  TPanel::showEvent(e);
  refreshFromScene();
  m_ruler->initPlayRangeIfNeeded();
  // Restore animatic-local onion skin state to global handle so the viewer
  // shows the correct onion frames (overrides any native-timeline state).
  m_ruler->syncOnionToGlobal();
}

void ZtoryAnimaticPanel::keyPressEvent(QKeyEvent *e) {
  const Qt::KeyboardModifiers mod = e->modifiers();
  const bool cmd  = mod & Qt::ControlModifier;
  const bool shift = mod & Qt::ShiftModifier;
  const std::set<int> &sel = m_track->selectedCols();

  // Cmd+D — duplicate (clone) selected shots
  if (cmd && !shift && e->key() == Qt::Key_D) {
    if (!ZtoryModel::assertMainXsheet(false)) return;
    // Clone in reverse column order so earlier indices stay valid
    std::vector<int> cols(sel.begin(), sel.end());
    std::sort(cols.rbegin(), cols.rend());
    for (int col : cols) {
      // Find shot index in ZtoryModel by matching xsheet column
      TApp *app = TApp::instance();
      ToonzScene *scene = app->getCurrentScene()->getScene();
      TXsheet *xsh = scene ? scene->getChildStack()->getTopXsheet() : nullptr;
      if (!xsh) continue;
      // Shot index = how many child columns precede this one
      int si = 0;
      for (int c = 0; c < col; c++) {
        TXshColumn *pc = xsh->getColumn(c);
        if (!pc || pc->isEmpty()) continue;
        int r0 = 0, r1 = 0; pc->getRange(r0, r1);
        for (int r = r0; r <= r1; r++) {
          TXshCell cell = xsh->getCell(r, c);
          if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
            { si++; break; }
        }
      }
      ZtoryModel::instance()->cloneShot(si);
    }
    refreshFromScene();
    e->accept();
    return;
  }

  // Delete / Backspace — delete selected shots
  if (!cmd && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
    if (sel.empty()) return;
    if (!ZtoryModel::assertMainXsheet(false)) return;
    TApp *app = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    TXsheet *xsh = scene ? scene->getChildStack()->getTopXsheet() : nullptr;
    if (!xsh) return;
    // Delete in reverse column order so earlier indices stay valid
    std::vector<int> cols(sel.begin(), sel.end());
    std::sort(cols.rbegin(), cols.rend());
    for (int col : cols)
      ColumnCmd::deleteColumn(col);
    xsh->updateFrameCount();
    ZtoryModel::instance()->resequenceXsheet();
    refreshFromScene();
    e->accept();
    return;
  }

  // Cmd+N — add new shot after selection
  if (cmd && e->key() == Qt::Key_N) {
    onAddShot();
    e->accept();
    return;
  }

  TPanel::keyPressEvent(e);
}

void ZtoryAnimaticPanel::onShotClicked(int col) {
  TApp::instance()->getCurrentColumn()->setColumnIndex(col);
}

void ZtoryAnimaticPanel::onShotDoubleClicked(int col) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  // Close any open sub-scene first
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
  app->getCurrentColumn()->setColumnIndex(col);
  TXsheet *xsh = app->getCurrentXsheet()->getXsheet();
  TXshColumn *column = xsh->getColumn(col);
  if (!column || column->isEmpty()) return;
  // BUG-01 fix: shots 2+ start at row r0 > 0; getCell(0,col) returned an
  // empty cell for them, so MI_OpenChild never fired.
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  TXshCell cell = xsh->getCell(r0, col);
  if (cell.m_level && cell.m_level->getChildLevel()) {
    app->getCurrentFrame()->setFrame(r0);
    CommandManager::instance()->execute("MI_OpenChild");
    // Keep the animatic controller's frame at the shot's main-xsheet row
    // so the animatic viewer renders at the correct position.
    ZtoryAnimaticController::instance()->setCurrentFrame(r0);
  }
}

void ZtoryAnimaticPanel::onReturnToMain() {
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  if (!scene) return;
  while (scene->getChildStack()->getAncestorCount() > 0)
    CommandManager::instance()->execute("MI_CloseChild");
}

void ZtoryAnimaticPanel::onShotMoved(int col, int newStartFrame) {
  // NOTE: this signal fires after resequenceXsheet() has already run, so
  // delta computation from getRange() would always be 0.  Audio shifting
  // on ripple is handled directly inside onShotDurationChanged().
  Q_UNUSED(col); Q_UNUSED(newStartFrame);
}

// Split the ColumnLevel covering |splitFrame| into two parts using offset
// trimming — no audio frames are lost at the cut boundary.
// findSegments() iterates ColumnLevels directly, so both halves are
// independently selectable and draggable after the split.
static void splitAudioColumn(TXsheet *xsh, int audioCol, int splitFrame) {
  TXshColumn *ac = xsh ? xsh->getColumn(audioCol) : nullptr;
  TXshSoundColumn *sc = ac ? ac->getSoundColumn() : nullptr;
  if (!sc) return;
  sc->splitLevelAtFrame(splitFrame);
}

// Helper: insert a TStageObject keyframe (without undo) on every column
// and the first camera of the given child xsheet at 0-based |frame|.
static void addRazorKeyframes(TXshChildLevel *cl, int frame) {
  if (!cl) return;
  TXsheet *childXsh = cl->getXsheet();
  if (!childXsh) return;
  TStageObjectTree *tree = childXsh->getStageObjectTree();
  if (!tree) return;
  int numCols = childXsh->getColumnCount();
  for (int c = 0; c < numCols; c++) {
    TStageObject *obj =
        tree->getStageObject(TStageObjectId::ColumnId(c), false);
    if (obj) obj->setKeyframeWithoutUndo(frame);
  }
  TStageObject *cam =
      tree->getStageObject(TStageObjectId::CameraId(0), false);
  if (cam) cam->setKeyframeWithoutUndo(frame);
}

// Helper: materialize "hold" cells in a child xsheet.
// In Tahoma2D, drawing cells are stored only at transition points; intermediate
// frames are "held" (empty in storage but rendered as the previous drawing).
// Before trimming/shifting we must make held frames explicit so that neither
// shot ends up with empty columns at the cut boundary.
// |duration| = total number of frames that the child xsheet must cover.
void materializeCells(TXshChildLevel *cl, int duration) {
  if (!cl) return;
  TXsheet *xsh = cl->getXsheet();
  if (!xsh) return;
  int nCols = xsh->getColumnCount();
  for (int c = 0; c < nCols; c++) {
    // Find the last row that actually has content in this column.
    // We must NOT fill beyond it: in a merged sub-scene, old columns end at
    // dstDuration and new columns start at dstDuration.  Filling old columns
    // past their last content row would create spurious held frames that
    // overlap with the new columns after a subsequent razor cut.
    int lastContent = -1;
    for (int r = std::min(duration, xsh->getFrameCount()) - 1; r >= 0; r--) {
      if (!xsh->getCell(r, c).isEmpty()) { lastContent = r; break; }
    }
    if (lastContent < 0) continue;  // column entirely empty up to duration

    TXshCell last;
    for (int r = 0; r <= lastContent; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (!cell.isEmpty()) {
        last = cell;
      } else if (!last.isEmpty()) {
        // Held frame: write the last explicit cell so it becomes explicit.
        xsh->setCell(r, c, last);
      }
    }
  }
}

// Helper: trim a child xsheet to |keepFrames| frames.
// Removes cells >= keepFrames and removes stage-object keyframes >= keepFrames.
void trimChildXsheetTo(TXshChildLevel *cl, int keepFrames) {
  if (!cl) return;
  TXsheet *xsh = cl->getXsheet();
  if (!xsh) return;
  int maxF  = xsh->getFrameCount();
  int nCols = xsh->getColumnCount();
  for (int c = 0; c < nCols; c++)
    for (int r = keepFrames; r < maxF; r++)
      xsh->clearCells(r, c);
  TStageObjectTree *tree = xsh->getStageObjectTree();
  if (tree) {
    auto trim = [&](TStageObject *obj) {
      if (!obj) return;
      TStageObject::KeyframeMap kfs;
      obj->getKeyframes(kfs);
      for (auto &kv : kfs)
        if (kv.first >= keepFrames)
          obj->removeKeyframeWithoutUndo(kv.first);
    };
    for (int c = 0; c < nCols; c++)
      trim(tree->getStageObject(TStageObjectId::ColumnId(c), false));
    trim(tree->getStageObject(TStageObjectId::CameraId(0), false));
  }
  xsh->updateFrameCount();
}

// Helper: shift a child xsheet left by |offset| frames.
// Cells at [offset .. end] become [0 .. end-offset].
// Stage-object keyframes are shifted by -offset; keyframes < 0 are removed.
static void shiftChildXsheetBy(TXshChildLevel *cl, int offset) {
  if (!cl || offset <= 0) return;
  TXsheet *xsh = cl->getXsheet();
  if (!xsh) return;
  int maxF  = xsh->getFrameCount();
  int nCols = xsh->getColumnCount();
  int keep  = maxF - offset;
  if (keep <= 0) { /* nothing would remain */ return; }
  for (int c = 0; c < nCols; c++) {
    std::vector<TXshCell> buf(keep);
    for (int r = 0; r < keep; r++)
      buf[r] = (offset + r < maxF) ? xsh->getCell(offset + r, c) : TXshCell();
    for (int r = 0; r < maxF; r++) xsh->clearCells(r, c);
    for (int r = 0; r < keep; r++)
      if (!buf[r].isEmpty()) xsh->setCell(r, c, buf[r]);
  }
  TStageObjectTree *tree = xsh->getStageObjectTree();
  if (tree) {
    auto shift = [&](TStageObject *obj) {
      if (!obj) return;
      TStageObject::KeyframeMap kfs;
      obj->getKeyframes(kfs);
      // Remove all existing keyframes first (collect positions, then remove).
      for (auto &kv : kfs) obj->removeKeyframeWithoutUndo(kv.first);
      // Re-add with offset applied; discard those that fall before frame 0.
      for (auto &kv : kfs) {
        int newF = kv.first - offset;
        if (newF >= 0) obj->setKeyframeWithoutUndo(newF, kv.second);
      }
    };
    for (int c = 0; c < nCols; c++)
      shift(tree->getStageObject(TStageObjectId::ColumnId(c), false));
    shift(tree->getStageObject(TStageObjectId::CameraId(0), false));
  }
  xsh->updateFrameCount();
}

// Helper: merge srcCl's content into dstCl starting at |dstOffset|.
//
// Design rules (matching the cut behaviour):
//  - Each drawing column of srcCl is appended as a NEW column in dstCl so
//    that the two shots never share column indices (no cell/keyframe mixing).
//  - The camera track IS shared: srcCl's camera keyframes are copied (with
//    offset) into dstCl's existing camera object.
//  - Boundary keyframes are inserted at the junction to freeze animation:
//      dstOffset-1 : added BEFORE inserting new cols (only existing cols)
//      dstOffset   : added AFTER  inserting new cols (all cols incl. new)
//      dstOffset+srcDuration-1 : added AFTER (end of the new segment, needed
//                                for 3+-shot merges where it becomes a middle).
//  - srcCl's cells are materialized (held cells → explicit) before copying.
void mergeChildXsheetContent(TXshChildLevel *dstCl,
                                    TXshChildLevel *srcCl,
                                    int dstOffset, int srcDuration) {
  if (!dstCl || !srcCl) return;
  TXsheet *dstXsh = dstCl->getXsheet();
  TXsheet *srcXsh = srcCl->getXsheet();
  if (!dstXsh || !srcXsh) return;

  // Step 1: materialize srcCl's held cells for its full duration.
  materializeCells(srcCl, srcDuration);

  // Step 2: keyframe at end of previous content — BEFORE inserting new cols.
  // This pins only the already-existing dst columns at that boundary.
  addRazorKeyframes(dstCl, dstOffset - 1);

  // Step 3: append new columns in dstXsh for each drawing column of srcCl.
  int srcNCols    = srcXsh->getColumnCount();
  int dstColBase  = dstXsh->getColumnCount(); // first new column index
  for (int c = 0; c < srcNCols; c++)
    dstXsh->insertColumn(dstColBase + c);

  // Step 4: copy cells into the new columns at dstOffset.
  for (int c = 0; c < srcNCols; c++) {
    for (int r = 0; r < srcDuration; r++) {
      TXshCell cell = srcXsh->getCell(r, c);
      if (!cell.isEmpty())
        dstXsh->setCell(dstOffset + r, dstColBase + c, cell);
    }
  }

  // Step 5: copy stage-object keyframes for drawing columns (shifted).
  TStageObjectTree *srcTree = srcXsh->getStageObjectTree();
  TStageObjectTree *dstTree = dstXsh->getStageObjectTree();
  if (srcTree && dstTree) {
    for (int c = 0; c < srcNCols; c++) {
      TStageObject *srcObj =
          srcTree->getStageObject(TStageObjectId::ColumnId(c), false);
      TStageObject *dstObj =
          dstTree->getStageObject(TStageObjectId::ColumnId(dstColBase + c), false);
      if (!srcObj || !dstObj) continue;
      TStageObject::KeyframeMap kfs;
      srcObj->getKeyframes(kfs);
      for (auto &kv : kfs)
        dstObj->setKeyframeWithoutUndo(kv.first + dstOffset, kv.second);
    }
  }

  // Step 7a: boundary keyframe at start of new segment — pins all columns
  // (including the new drawing columns) at their current interpolated values.
  // Must run BEFORE copying the src camera keyframes: the single-arg
  // setKeyframeWithoutUndo() materialises the interpolated value at that frame,
  // so it would overwrite the correct second-shot camera value if called after.
  addRazorKeyframes(dstCl, dstOffset);

  // Step 6: merge camera keyframes (shared camera track).
  // Runs AFTER addRazorKeyframes so the correct srcCam value at dstOffset
  // overwrites the interpolated first-shot value written by step 7a.
  if (srcTree && dstTree) {
    TStageObject *srcCam =
        srcTree->getStageObject(TStageObjectId::CameraId(0), false);
    TStageObject *dstCam =
        dstTree->getStageObject(TStageObjectId::CameraId(0), false);
    if (srcCam && dstCam) {
      TStageObject::KeyframeMap kfs;
      srcCam->getKeyframes(kfs);
      for (auto &kv : kfs)
        dstCam->setKeyframeWithoutUndo(kv.first + dstOffset, kv.second);
      // The junction keyframe at dstOffset was set by addRazorKeyframes (step 7a)
      // to the first-shot's interpolated value. Override it with the second shot's
      // first camera keyframe value so the junction is clean.
      // If srcCam has no explicit keyframe at frame 0, we use its first keyframe;
      // if it has no keyframes at all, addRazorKeyframes already set the right hold.
      if (!kfs.empty()) {
        dstCam->setKeyframeWithoutUndo(dstOffset, kfs.begin()->second);
      }
    }
  }

  // Step 7b: boundary keyframe at end of new segment.
  addRazorKeyframes(dstCl, dstOffset + srcDuration - 1);

  dstXsh->updateFrameCount();
}

void ZtoryAnimaticPanel::onMergeShots() {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;
  const std::set<int> &sel = m_track->selectedCols();
  if (sel.size() < 2) return;

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Sort selected cols by their start frame in main xsheet
  std::vector<int> sortedCols(sel.begin(), sel.end());
  std::sort(sortedCols.begin(), sortedCols.end(), [&](int a, int b){
    int r0a = 0, r1a = 0, r0b = 0, r1b = 0;
    if (xsh->getColumn(a)) xsh->getColumn(a)->getRange(r0a, r1a);
    if (xsh->getColumn(b)) xsh->getColumn(b)->getRange(r0b, r1b);
    return r0a < r0b;
  });

  int dstCol = sortedCols[0];
  TXshColumn *dstColumn = xsh->getColumn(dstCol);
  if (!dstColumn) return;
  int dstR0 = 0, dstR1 = 0;
  dstColumn->getRange(dstR0, dstR1);

  // Find child level of destination
  TXshChildLevel *dstCl = nullptr;
  for (int r = dstR0; r <= dstR1; r++) {
    TXshCell cell = xsh->getCell(r, dstCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      dstCl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!dstCl) return;

  int appendAt    = dstR1 + 1;
  int dstDuration = dstR1 - dstR0 + 1;
  int lastFrameNum = dstDuration; // 1-based frame index for continuation

  // Materialize held cells in the first shot before merging, then trim to the
  // timeline duration. Without the trim, any frames in the sub-scene beyond
  // dstDuration (hidden from the main xsheet) would overlap with the incoming
  // src content that is inserted starting at row dstDuration.
  materializeCells(dstCl, dstDuration);
  trimChildXsheetTo(dstCl, dstDuration);

  for (int i = 1; i < (int)sortedCols.size(); i++) {
    int srcCol = sortedCols[i];
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    if (!srcColumn) continue;
    int r0 = 0, r1 = 0;
    srcColumn->getRange(r0, r1);
    int duration = r1 - r0 + 1;

    // Find src child level
    TXshChildLevel *srcCl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, srcCol);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        srcCl = cell.m_level->getChildLevel();
        break;
      }
    }
    // Merge srcCl into dstCl: new columns per shot, shared camera,
    // boundary keyframes at junction and end of segment.
    mergeChildXsheetContent(dstCl, srcCl, lastFrameNum, duration);

    // Extend main xsheet column to cover the merged duration.
    for (int r = 0; r < duration; r++)
      xsh->setCell(appendAt + r, dstCol, TXshCell(dstCl, TFrameId(++lastFrameNum)));
    appendAt += duration;
  }

  // Delete source columns in reverse order (skip first = destination).
  // Reverse order keeps lower column indices stable across iterations.
  // Do NOT emit shotRemovedAt here — the xsheet is in intermediate state
  // (dst column cells overlap with still-present src columns). Emitting here
  // would trigger onShotRemovedAt → renumberAll → updateColumnName →
  // notifyXsheetChanged → Animatic refreshes mid-merge → visual overlap.
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--)
    ColumnCmd::deleteColumn(sortedCols[i]);

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();

  // Notify Board AFTER xsheet is fully stable and track refreshed.
  // Emit in the same reverse-column order so Board's xsheetColumn tracking
  // stays correct across multiple removals.
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--)
    emit ZtoryModel::instance()->shotRemovedAt(sortedCols[i]);
}

// ---- Add Shot ----
void ZtoryAnimaticPanel::onAddShot() {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Insert after the rightmost selected shot, or append at the end
  int insertAt = xsh->getColumnCount();
  const std::set<int> &sel = m_track->selectedCols();
  if (!sel.empty())
    insertAt = *std::max_element(sel.begin(), sel.end()) + 1;

  static const int kDefaultDuration = 24;

  // Create a new sub-scene (child level)
  TXshLevel *xl = scene->createNewLevel(CHILD_XSHLEVEL);
  if (!xl || !xl->getChildLevel()) return;
  TXshChildLevel *cl = xl->getChildLevel();

  xsh->insertColumn(insertAt);
  for (int r = 0; r < kDefaultDuration; r++)
    xsh->setCell(r, insertAt, TXshCell(cl, TFrameId(r + 1)));
  xsh->updateFrameCount();

  // Copy camera resolution/size from parent to sub-scene
  TXsheet *childXsh = cl->getXsheet();
  if (childXsh) {
    TStageObjectTree *parentTree = xsh->getStageObjectTree();
    TStageObjectTree *childTree  = childXsh->getStageObjectTree();
    int tmpCamId = 0;
    for (int cam = 0; cam < parentTree->getCameraCount();) {
      TStageObject *parentCamera = parentTree->getStageObject(
          TStageObjectId::CameraId(tmpCamId), false);
      if (!parentCamera) { tmpCamId++; continue; }
      if (parentCamera->getCamera()) {
        TCamera *childCamera = childTree->getStageObject(
            TStageObjectId::CameraId(tmpCamId))->getCamera();
        if (childCamera) {
          childCamera->setRes(parentCamera->getCamera()->getRes());
          childCamera->setSize(parentCamera->getCamera()->getSize());
        }
      }
      tmpCamId++; cam++;
    }
    childTree->setCurrentCameraId(parentTree->getCurrentCameraId());
  }

  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();

  // Notify Board AFTER xsheet is stable
  emit ZtoryModel::instance()->shotAdded(insertAt);
}

void ZtoryAnimaticPanel::onMergeWithNext(int col) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Find the next non-empty column after 'col'
  TXshColumn *dstColumn = xsh->getColumn(col);
  if (!dstColumn) return;
  int dstR0 = 0, dstR1 = 0;
  dstColumn->getRange(dstR0, dstR1);

  // Find next shot column
  int nextCol = -1;
  for (int c = col + 1; c < xsh->getColumnCount(); c++) {
    TXshColumn *nc = xsh->getColumn(c);
    if (!nc || nc->isEmpty()) continue;
    // Check it's a child level (shot), not audio
    int nr0 = 0, nr1 = 0;
    nc->getRange(nr0, nr1);
    for (int r = nr0; r <= nr1; r++) {
      TXshCell cell = xsh->getCell(r, c);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        nextCol = c;
        break;
      }
    }
    if (nextCol >= 0) break;
  }
  if (nextCol < 0) return;

  // Find child level of destination
  TXshChildLevel *dstCl = nullptr;
  for (int r = dstR0; r <= dstR1; r++) {
    TXshCell cell = xsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      dstCl = cell.m_level->getChildLevel();
      break;
    }
  }
  if (!dstCl) return;

  // Find child level of source (next) shot
  TXshChildLevel *srcCl = nullptr;
  TXshColumn *srcColumn = xsh->getColumn(nextCol);
  int srcR0 = 0, srcR1 = 0;
  srcColumn->getRange(srcR0, srcR1);
  for (int r = srcR0; r <= srcR1; r++) {
    TXshCell cell = xsh->getCell(r, nextCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      srcCl = cell.m_level->getChildLevel();
      break;
    }
  }

  int dstDuration = dstR1 - dstR0 + 1;
  int srcDuration = srcR1 - srcR0 + 1;

  // Materialize held cells then trim to timeline duration — same reasoning as
  // onMergeShots: hidden frames beyond dstDuration must be removed before
  // appending src content, or they overlap with the incoming material.
  materializeCells(dstCl, dstDuration);
  trimChildXsheetTo(dstCl, dstDuration);

  // Merge srcCl into dstCl: new columns per shot, shared camera,
  // boundary keyframes at junction and end of segment.
  mergeChildXsheetContent(dstCl, srcCl, dstDuration, srcDuration);

  // Extend dst column in main xsheet to cover the merged duration.
  int appendAt     = dstR1 + 1;
  int lastFrameNum = dstDuration;
  int duration     = srcDuration;
  for (int r = 0; r < duration; r++)
    xsh->setCell(appendAt + r, col, TXshCell(dstCl, TFrameId(++lastFrameNum)));

  ColumnCmd::deleteColumn(nextCol);

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  m_track->refreshFromScene();

  // Notify Board AFTER xsheet is fully stable (mirrors razor's shotAdded pattern).
  emit ZtoryModel::instance()->shotRemovedAt(nextCol);
}

// Helper: log column range to debug file

void ZtoryAnimaticPanel::onRazorRequested(int col, int splitFrame) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  TXshColumn *srcColumn = mainXsh->getColumn(col);
  if (!srcColumn) return;
  int r0 = 0, r1 = 0;
  srcColumn->getRange(r0, r1);
  // splitRel = # frames that stay in the original shot (rows r0..splitFrame-1)
  int splitRel = splitFrame - r0;
  if (splitRel <= 0 || splitRel >= r1 - r0 + 1) return;

  int totalDuration = r1 - r0 + 1;
  int secondHalf    = totalDuration - splitRel;  // frames for the new shot

  // Grab original child level before cloning.
  TXshChildLevel *origCL = nullptr;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      origCL = cell.m_level->getChildLevel();
      break;
    }
  }

  // ── Step 1: materialize held cells for the full shot duration ──────────────
  // Tahoma2D stores drawing cells only at transition points; intermediate
  // frames are empty in storage ("held"). We must make them explicit so that
  // after trim/shift neither shot has gaps at the cut boundary.
  materializeCells(origCL, totalDuration);

  // ── Step 2: bake boundary keyframes BEFORE cloning ────────────────────────
  // Adding keyframes to origCL now means the clone inherits them automatically.
  // splitRel-1 = last frame of shot 1; splitRel = first frame of shot 2.
  addRazorKeyframes(origCL, splitRel - 1);
  addRazorKeyframes(origCL, splitRel);

  // ── Step 3: Clone ──────────────────────────────────────────────────────────
  ColumnCmd::cloneChild(col);
  int newCol = col + 1;

  // Grab clone's child level.
  TXshChildLevel *cloneCL = nullptr;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, newCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      cloneCL = cell.m_level->getChildLevel();
      break;
    }
  }

  // ── Step 4: Trim original child xsheet to splitRel frames ─────────────────
  // Also removes stage-object keyframes beyond the cut point.
  trimChildXsheetTo(origCL, splitRel);

  // ── Step 5: Shift clone child xsheet left by splitRel ─────────────────────
  // Cells and keyframes at [splitRel..end] become [0..secondHalf-1].
  shiftChildXsheetBy(cloneCL, splitRel);

  // ── Step 6: Fix main-xsheet columns ───────────────────────────────────────
  // Trim original column: remove the tail (secondHalf cells).
  mainXsh->removeCells(r0 + splitRel, col, secondHalf);

  // Rebuild clone column: wipe then repopulate with TFrameId(1..secondHalf).
  mainXsh->clearCells(r0, newCol, totalDuration + 2);
  if (cloneCL) {
    for (int r = 0; r < secondHalf; r++)
      mainXsh->setCell(r0 + r, newCol, TXshCell(cloneCL, TFrameId(r + 1)));
  }

  mainXsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();

  // Linked razor: also split audio tracks at the same absolute frame.
  if (m_audioLinked) {
    // splitFrame is already an absolute frame in the main xsheet.
    // We must iterate a copy because splitAudioColumn may add columns,
    // which would invalidate the audio tracks list.
    QList<int> audioCols;
    for (auto *at : m_audioTracks) audioCols.append(at->columnIndex());
    for (int ac : audioCols) splitAudioColumn(mainXsh, ac, splitFrame);
  }

  m_track->refreshFromScene();
  refreshAudioTracks();

  // Notify Board AFTER xsheet is stable and track refreshed.
  // Emitting before resequenceXsheet would fire onShotInserted → renumberAll
  // → updateColumnName → notifyXsheetChanged → Animatic refreshes while the
  // clone column still starts at the same frame as the original → overlap.
  emit ZtoryModel::instance()->shotAdded(newCol);
}

void ZtoryAnimaticPanel::onAudioRazorRequested(int col, int frame) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/false)) return;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  splitAudioColumn(xsh, col, frame);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  refreshAudioTracks();
}

void ZtoryAnimaticPanel::onSegmentDroppedOutside(int srcCol, int origR0,
                                                  int origR1, int dragOffset,
                                                  QPoint globalPos) {
  // Find the target audio track under the drop position
  ZtoryAudioTrack *target = nullptr;
  for (auto *at : m_audioTracks) {
    if (at->columnIndex() == srcCol) continue;
    QRect r = QRect(at->mapToGlobal(QPoint(0, 0)), at->size());
    if (r.contains(globalPos)) { target = at; break; }
  }
  if (!target) return;

  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;

  // Detach from source
  TXshColumn *srcColumn = xsh->getColumn(srcCol);
  TXshSoundColumn *srcSc = srcColumn ? srcColumn->getSoundColumn() : nullptr;
  if (!srcSc) return;
  int midFrame = (origR0 + origR1) / 2;
  ColumnLevel *cl = srcSc->detachLevelByFrame(midFrame);
  if (!cl) return;

  // Compute target vsf: frame under cursor minus the grab offset inside the segment
  QPoint localPos = target->mapFromGlobal(globalPos);
  int cursorFrame = target->frameAtX(localPos.x());
  int targetVsf = qMax(0, cursorFrame - dragOffset);

  // Clamp against existing segments on destination track to avoid overlap.
  // Use targetVsf (not origR0 from source) to find left/right neighbors.
  int segLen = origR1 - origR0;
  auto dstSegs = target->findSegments();
  for (auto &s : dstSegs) {
    // Neighbor on the left: push targetVsf past s.r1
    if (s.r1 < targetVsf && targetVsf <= s.r1 + 1)
      targetVsf = s.r1 + 1;
    // Neighbor on the right: push targetVsf so we don't overlap s.r0
    if (s.r0 >= targetVsf && targetVsf + segLen >= s.r0)
      targetVsf = s.r0 - segLen - 1;
  }
  if (targetVsf < 0) targetVsf = 0;

  // Adopt into target track
  int dstCol = target->columnIndex();
  TXshColumn *dstColumn = xsh->getColumn(dstCol);
  TXshSoundColumn *dstSc = dstColumn ? dstColumn->getSoundColumn() : nullptr;
  if (!dstSc) { delete cl; return; }
  dstSc->adoptLevel(cl, targetVsf);

  // Refresh — invalidate waveform via a queued call so it fires AFTER
  // all pending Qt paint events from refreshAudioTracks() have settled.
  ZtoryAnimaticController::instance()->invalidateSoundTrack();
  xsh->updateFrameCount();
  TApp::instance()->getCurrentScene()->notifyCastChange();
  refreshAudioTracks();
  updateTrackWidths();
  // Force a second waveform rebuild in the next event-loop iteration:
  // the first paint of the new widgets may happen before Qt finishes
  // processing all pending column-data updates.
  QTimer::singleShot(0, this, [this]() {
    for (auto *at : m_audioTracks) {
      at->invalidateWaveform();
      at->update();
    }
  });
}

void ZtoryAnimaticPanel::onShotDurationChanged(int col, int newF1) {
  int newDuration = newF1 + 1;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  TXshColumn *column = mainXsh->getColumn(col);
  if (!column || column->isEmpty()) return;
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  int currentDuration = r1 - r0 + 1;

  // Trova tipo cella
  TXshCell typeCell;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      typeCell = cell;
      break;
    }
  }
  if (typeCell.isEmpty()) return;

  if (newDuration > currentDuration) {
    // Trova l'ultimo frame index usato
    int lastFrameNum = currentDuration; // frame index dell'ultima cella (0-based -> 1-based)
    for (int r = r1; r >= r0; r--) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty()) {
        lastFrameNum = cell.m_frameId.getNumber();
        break;
      }
    }
    for (int r = r0 + currentDuration; r < r0 + newDuration; r++) {
      TXshCell newCell = typeCell;
      newCell.m_frameId = TFrameId(++lastFrameNum);
      mainXsh->setCell(r, col, newCell);
    }
  } else if (newDuration < currentDuration) {
    mainXsh->removeCells(r0 + newDuration, col, currentDuration - newDuration);
  }

  // Audio shift for subsequent shots is handled entirely by resequenceXsheet()
  // (audio-linked variant records deltas before/after and applies them).
  // Do NOT shift audio here — doing so and then calling resequenceXsheet()
  // causes a double-shift that pushes audio too far and makes it disappear.
  resequenceXsheet();

  // Sync the sub-xsheet's Out marker to the new duration.
  // ztorySetShotRange updates s_frameRangeMap so the correct Out is shown
  // the next time the shot is opened for editing.
  // Range is [0, newDuration-1] (0-based, inclusive).
  ztorySetShotRange(col, 0, newDuration - 1);

  // If this shot's sub-xsheet is currently open, also update the live
  // play range so the FlipConsole markers move immediately.
  ChildStack *cs = scene->getChildStack();
  if (cs && cs->getAncestorCount() == 1) {
    // Find the child xsheet for this column.
    TXsheet *shotXsh = nullptr;
    for (int r = r0; r <= r0 + newDuration - 1 && !shotXsh; r++) {
      TXshCell cell = mainXsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel())
        shotXsh = cell.m_level->getChildLevel()->getXsheet();
    }
    if (shotXsh && cs->getXsheet() == shotXsh)
      XsheetGUI::setPlayRange(0, newDuration - 1, 1, false);
  }

  m_track->refreshFromScene();
}

void ZtoryAnimaticPanel::resequenceXsheet() {
  if (!m_audioLinked) {
    ZtoryModel::instance()->resequenceXsheet();
    return;
  }

  // Audio-video link: shift audio ColumnLevels to follow their associated shots.
  // We shift startFrame directly on each ColumnLevel instead of the broken
  // clear+setCell approach (TXshSoundColumn::setCell is designed for sequential
  // insertion, not for restoring cells read via getCell from different positions).
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) { ZtoryModel::instance()->resequenceXsheet(); return; }
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) { ZtoryModel::instance()->resequenceXsheet(); return; }

  // 1. Record shot positions BEFORE resequence.
  struct ShotPos { int col; int r0; int duration; };
  std::vector<ShotPos> oldPositions;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    bool isChild = false;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        isChild = true; break;
      }
    }
    if (isChild) oldPositions.push_back({col, r0, r1 - r0 + 1});
  }

  // 2. Run resequence on video columns.
  ZtoryModel::instance()->resequenceXsheet();

  // 3. Read new shot positions AFTER resequence → delta per video column.
  std::map<int, int> shotDelta;
  for (auto &op : oldPositions) {
    TXshColumn *column = xsh->getColumn(op.col);
    if (!column || column->isEmpty()) continue;
    int nr0 = 0, nr1 = 0;
    column->getRange(nr0, nr1);
    shotDelta[op.col] = nr0 - op.r0;
  }

  // 4. Shift audio ColumnLevels so they follow their associated shots.
  //
  //    OLD approach (wrong): call shiftLevelInRange(shotR0, shotR1, delta) for
  //    each shot that moved. This only shifts ColumnLevels whose vsf falls
  //    inside exactly that shot's old range. A long (uncut) audio segment
  //    spanning multiple shots has vsf=0 → is never matched. A razor-cut
  //    segment for Shot C has vsf=146 → is not matched when we process Shot B
  //    with range [89,145]. Result: only the first cut segment ever shifts.
  //
  //    NEW approach: find the earliest shot that moved → shiftFrom.
  //    All shots after that point have the same delta (resequence packs
  //    tightly, so a single duration change propagates uniformly).
  //    Shift ALL audio ColumnLevels with vsf >= shiftFrom by that delta.
  //    This correctly handles: uncut tracks spanning all shots, tracks cut
  //    at shot boundaries, and any number of razor-cut segments.
  {
    int shiftFrom   = INT_MAX;
    int commonDelta = 0;
    for (auto &op : oldPositions) {
      int d = shotDelta.count(op.col) ? shotDelta.at(op.col) : 0;
      if (d != 0 && op.r0 < shiftFrom) {
        shiftFrom   = op.r0;
        commonDelta = d;
      }
    }
    if (commonDelta != 0 && shiftFrom != INT_MAX) {
      for (auto *at : m_audioTracks) {
        int audioCol = at->columnIndex();
        TXshColumn *ac = xsh->getColumn(audioCol);
        if (!ac) continue;
        TXshSoundColumn *sc = ac->getSoundColumn();
        if (!sc) continue;
        sc->shiftLevelFromFrame(shiftFrom, commonDelta);
      }
    }
  }

  // Invalidate waveform caches and repaint all audio tracks.
  // xsheetChanged (fired inside ZtoryModel::resequenceXsheet) rebuilds the
  // widgets via refreshAudioTracks() BEFORE the shift, so the cache is stale.
  // A plain update() won't rebuild the cache — we must mark it dirty first.
  for (auto *at : m_audioTracks) at->invalidateWaveform();

  // Invalidate the cached merged sound track so the next scrub/play rebuilds
  // it with the updated ColumnLevel positions.
  ZtoryAnimaticController::instance()->invalidateSoundTrack();

  xsh->updateFrameCount();
}

void ZtoryAnimaticPanel::onMatchSubsceneDuration(int col) {
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  // Trova la prima riga reale della colonna
  TXshColumn *mainCol = mainXsh->getColumn(col);
  if (!mainCol) return;
  int r0 = 0, r1 = 0;
  mainCol->getRange(r0, r1);
  // Entra nella sottoscena per trovare l'ultimo frame non vuoto
  TXshCell cell = mainXsh->getCell(r0, col);
  if (!cell.m_level || !cell.m_level->getChildLevel()) return;
  TXsheet *subXsh = cell.m_level->getChildLevel()->getXsheet();
  if (!subXsh) return;

  // Trova l'ultimo frame non vuoto in tutta la sottoscena
  int lastFrame = 0;
  for (int c = 0; c < subXsh->getColumnCount(); c++) {
    int r0 = 0, r1 = 0;
    TXshColumn *column = subXsh->getColumn(c);
    if (!column) continue;
    column->getRange(r0, r1);
    // Cerca l'ultimo frame con cella non vuota
    for (int r = r1; r >= r0; r--) {
      TXshCell sc = subXsh->getCell(r, c);
      if (!sc.isEmpty()) {
        lastFrame = qMax(lastFrame, r);
        break;
      }
    }
  }

  if (lastFrame <= 0) return;
  int newDuration = lastFrame + 1;

  // Apply new duration. Call directly — the signal connection would cause a
  // double execution since onShotDurationChanged is also connected to
  // m_track::shotDurationChanged.  The track visuals are rebuilt by
  // resequenceXsheet() → refreshFromScene() inside onShotDurationChanged.
  onShotDurationChanged(col, newDuration - 1);
}

void ZtoryAnimaticPanel::onZoomChanged(double ppf) {
  m_ppf = ppf;
  m_ruler->setPixelsPerFrame(ppf);
  m_track->setPixelsPerFrame(ppf);
  for (auto *at : m_audioTracks)
    at->setPixelsPerFrame(ppf);
  // Sync slider without triggering another valueChanged
  if (m_zoomSlider) {
    m_zoomSlider->blockSignals(true);
    m_zoomSlider->setValue((int)(ppf * 10));
    m_zoomSlider->blockSignals(false);
  }
  updateTrackWidths();
}

void ZtoryAnimaticPanel::onFrameChanged(int frame) {
  m_ruler->setCurrentFrame(frame);
  m_track->setCurrentFrame(frame);
  for (auto *at : m_audioTracks)
    at->setCurrentFrame(frame);
  // Write to the dedicated animatic frame handle — this drives the animatic
  // viewer without touching TApp's global frame (BUG-03 fix).
  ZtoryAnimaticController::instance()->setCurrentFrame(frame);
}



void ZtoryAnimaticPanel::contextMenuEvent(QContextMenuEvent *e) {
  QMenu menu(this);
  QAction *loadAudio = menu.addAction(tr("Load Audio..."));
  QAction *addTrack  = menu.addAction(tr("Add Audio Track"));
  QAction *chosen = menu.exec(e->globalPos());
  if (chosen == addTrack) {
    TApp *app = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    if (!scene) return;
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (!xsh) return;
    int insertCol = xsh->getColumnCount();
    xsh->insertColumn(insertCol, TXshColumn::eSoundType);
    TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
    refreshAudioTracks();
    updateTrackWidths();
    app->getCurrentScene()->notifyCastChange();
    return;
  }
  if (chosen == loadAudio) {
    // In storyboard workflow, audio can only be loaded from the main xsheet.
    // Show the warning before opening the file dialog so the user doesn't
    // waste time selecting a file only to be rejected afterward.
    if (ZtoryModel::instance()->isStoryboardWorkflow()) {
      ToonzScene *checkScene = TApp::instance()->getCurrentScene()->getScene();
      if (checkScene && checkScene->getChildStack()->getAncestorCount() > 0) {
        DVGui::warning(tr(
            "In storyboard workflow, audio can only be loaded from the main "
            "xsheet.\nPlease close the current sub-scene first."));
        return;
      }
    }
    // Formati nativi: wav, aiff. mp3/ogg richiedono FFmpeg configurato.
    QString path = QFileDialog::getOpenFileName(
        this, tr("Load Audio"),
        QString(),
        tr("Audio Files (*.wav *.aiff *.aif *.mp3 *.ogg);;WAV (*.wav);;AIFF (*.aiff *.aif);;All Files (*)"));
    if (path.isEmpty()) return;

    TApp *app = TApp::instance();
    ToonzScene *scene = app->getCurrentScene()->getScene();
    if (!scene) return;
    TXsheet *xsh = scene->getChildStack()->getTopXsheet();
    if (!xsh) return;

    // Inserisci nella prima colonna libera dopo le colonne esistenti
    int insertCol = xsh->getColumnCount();

    IoCmd::LoadResourceArguments args(TFilePath(path.toStdWString()));
    args.row0 = 0;
    args.col0 = insertCol;
    args.expose = true;
    IoCmd::loadResources(args);
    refreshAudioTracks();
  }
}

class ZtoryAnimaticPanelFactory final : public TPanelFactory {
public:
  ZtoryAnimaticPanelFactory() : TPanelFactory("ZtoryAnimatic") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryAnimaticPanel(parent);
    panel->setObjectName("ZtoryAnimatic");
    panel->setWindowTitle("Ztoryc Animatic");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryAnimaticPanelFactory;

class ZtoryAnimaticViewerPanelFactory final : public TPanelFactory {
public:
  ZtoryAnimaticViewerPanelFactory() : TPanelFactory("ZtoryAnimaticViewer") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryAnimaticViewerPanel(parent);
    panel->setObjectName("ZtoryAnimaticViewer");
    panel->setWindowTitle("Ztoryc Viewer");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryAnimaticViewerPanelFactory;

class ZtoryStoryStripPanelFactory final : public TPanelFactory {
public:
  ZtoryStoryStripPanelFactory() : TPanelFactory("ZtoryStoryStrip") {}
  TPanel *createPanel(QWidget *parent) override {
    TPanel *panel = new ZtoryStoryStripPanel(parent);
    panel->setObjectName("ZtoryStoryStrip");
    panel->setWindowTitle("Ztoryc Story Strip");
    return panel;
  }
  void initialize(TPanel *panel) override { assert(0); }
} ztoryStoryStripPanelFactory;


