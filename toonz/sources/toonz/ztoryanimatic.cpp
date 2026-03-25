
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
#include "orientation.h"
#include <QPainter>
#include <QMouseEvent>
#include <QPushButton>
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

// Shared label column width — must match ZtoryAudioTrack::labelW (80px).
// Used by ZtoryAnimaticRuler and ZtoryAnimaticTrack to align with audio tracks.
static constexpr int kLabelW = 80;

// ---- ZtoryAnimaticController ----

ZtoryAnimaticController::ZtoryAnimaticController() : QObject() {
  m_frameHandle = new TFrameHandle();
  m_frameHandle->setFrame(0);
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

void ZtoryAnimaticController::setCurrentFrame(int frame) {
  if (frame < 0) frame = 0;
  m_frameHandle->setFrame(frame);
}

int ZtoryAnimaticController::currentFrame() const {
  return m_frameHandle->getFrame();
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
    r1 = mainXsh ? std::max(0, mainXsh->getFrameCount() - 1) : 0;
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
    update();
    return;
  }
  if (e->modifiers() & Qt::AltModifier) {
    int r0, r1, step;
    if (!XsheetGUI::getPlayRange(r0, r1, step)) { r0 = frame; r1 = frame; step = 1; }
    XsheetGUI::setPlayRange(std::min(r0, frame), frame, step);
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
    // Audio scrub
    auto *ctrl = ZtoryAnimaticController::instance();
    TSoundTrackP st = ctrl->soundTrack();
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
    update();
    return;
  }
  if (m_dragMode == DragOut) {
    int r0, r1, step;
    XsheetGUI::getPlayRange(r0, r1, step);
    XsheetGUI::setPlayRange(r0, std::max(frame, r0), step);
    update();
    return;
  }

  m_currentFrame = frame;
  update();
  emit frameChanged(m_currentFrame);
  // Audio scrub (12b: use cached sound track instead of makeSound per frame)
  auto *ctrl = ZtoryAnimaticController::instance();
  TSoundTrackP st = ctrl->soundTrack();
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
  int lastFrame = std::max(0, xsh->getFrameCount() - 1);
  XsheetGUI::setPlayRange(0, lastFrame, 1);
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
    int last = xsh ? std::max(0, xsh->getFrameCount() - 1) : 0;
    int r0, r1, step;
    if (!XsheetGUI::getPlayRange(r0, r1, step)) { r0 = 0; step = 1; }
    XsheetGUI::setPlayRange(r0, last, 1);
  } else if (chosen == resetAct) {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    int last = xsh ? std::max(0, xsh->getFrameCount() - 1) : 0;
    XsheetGUI::setPlayRange(0, last, 1);
  }
  update();
}

// ---- ZtoryAudioTrack ----

ZtoryAudioTrack::ZtoryAudioTrack(int col, const QString &name, QWidget *parent)
    : QWidget(parent), m_col(col), m_name(name) {
  setFixedHeight(m_trackHeight);
  setMinimumWidth(100);
  setFocusPolicy(Qt::ClickFocus);  // needed for keyPressEvent
}

int ZtoryAudioTrack::frameAtX(int x) const {
  return std::max(0, (int)((x - kLabelW) / m_ppf));
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
  int r0 = 0, r1 = 0;
  column->getRange(r0, r1);
  if (r1 < r0) return segs;
  bool inSeg = false;
  int segStart = 0;
  for (int r = r0; r <= r1 + 1; r++) {
    bool empty = (r > r1) || xsh->getCell(r, m_col).isEmpty();
    if (!empty && !inSeg) { segStart = r; inSeg = true; }
    else if (empty && inSeg) { segs.push_back({segStart, r - 1}); inSeg = false; }
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

  // Label colonna
  p.fillRect(0, 0, labelW, trackH, QColor(40, 40, 40));
  p.setPen(QColor(200, 200, 200));
  p.setFont(QFont("Arial", 9));
  p.drawText(4, 0, labelW - 4, trackH, Qt::AlignVCenter | Qt::AlignLeft, m_name);
  p.setPen(QColor(60, 60, 60));
  p.drawLine(labelW, 0, labelW, trackH);

  // Rebuild waveform cache when ppf or size changed
  if (m_waveformDirty || m_waveformCache.size() != QSize(trackW, trackH)) {
    m_waveformCache = QPixmap(trackW, trackH);
    m_waveformCache.fill(QColor(25, 25, 25));
    m_waveformDirty = false;

    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    TXshColumn *column = xsh ? xsh->getColumn(m_col) : nullptr;
    TXshSoundColumn *sc = column ? column->getSoundColumn() : nullptr;

    if (sc) {
      // Get merged sound track for the full xsheet duration
      int totalFrames = xsh->getFrameCount();
      TSoundTrackP st = sc->getOverallSoundTrack(0, totalFrames - 1);

      if (st) {
        double fps = 24.0;
        ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
        if (scene)
          fps = scene->getProperties()->getOutputProperties()->getFrameRate();

        TINT32 sampleRate  = st->getSampleRate();
        TINT32 sampleCount = st->getSampleCount();
        double samplesPerPixel = sampleRate / fps / m_ppf;

        // Normalise: find peak amplitude across entire track
        double globalMin = 0, globalMax = 0;
        st->getMinMaxPressure(0, sampleCount - 1, TSound::MONO,
                              globalMin, globalMax);
        double peak = std::max(std::fabs(globalMin), std::fabs(globalMax));
        if (peak <= 0) peak = 1.0;

        const int halfH = (trackH - 4) / 2;
        QPainter cp(&m_waveformCache);

        // Centre line
        cp.setPen(QColor(60, 60, 60));
        cp.drawLine(0, center, trackW, center);

        cp.setPen(QColor(80, 200, 120));
        for (int px = 0; px < trackW; px++) {
          double frame = (double)px / m_ppf;
          TINT32 s0 = (TINT32)(frame * sampleRate / fps);
          TINT32 s1 = s0 + (TINT32)(samplesPerPixel) + 1;
          s0 = qBound((TINT32)0, s0, sampleCount - 1);
          s1 = qBound((TINT32)0, s1, sampleCount - 1);
          if (s0 > s1) continue;

          double minV = 0, maxV = 0;
          st->getMinMaxPressure(s0, s1, TSound::MONO, minV, maxV);

          int yMax = center - (int)(maxV / peak * halfH);
          int yMin = center - (int)(minV / peak * halfH);
          yMax = qBound(2, yMax, trackH - 2);
          yMin = qBound(2, yMin, trackH - 2);
          if (yMax > yMin) std::swap(yMax, yMin);
          cp.drawLine(px, yMax, px, yMin);
        }
      } else {
        // No sound: draw centre line only
        QPainter cp(&m_waveformCache);
        cp.setPen(QColor(60, 60, 60));
        cp.drawLine(0, center, trackW, center);
      }
    } else {
      QPainter cp(&m_waveformCache);
      cp.setPen(QColor(60, 60, 60));
      cp.drawLine(0, center, trackW, center);
    }
  }

  // Blit cached waveform
  p.drawPixmap(labelW, 0, m_waveformCache);

  // Draw separators at gaps between audio segments
  {
    auto segs = findSegments();
    for (int i = 0; i + 1 < (int)segs.size(); i++) {
      // Draw a separator at the gap between segment i and i+1
      int gapStart = segs[i].r1 + 1;
      int gapEnd = segs[i + 1].r0;
      int x0 = labelW + (int)(gapStart * m_ppf);
      int x1 = labelW + (int)(gapEnd * m_ppf);
      if (x1 - x0 < 2) x1 = x0 + 2;  // minimum visible width
      p.fillRect(x0, 0, x1 - x0, trackH, QColor(255, 80, 80, 120));
    }
  }

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

  // Playhead (always on top, not cached) — centered on frame like the ruler
  int phx = labelW + (int)(m_currentFrame * m_ppf) + (int)(m_ppf / 2);
  p.setPen(QColor(255, 100, 0));
  p.drawLine(phx, 0, phx, trackH);
}

void ZtoryAudioTrack::setRazorActive(bool on) {
  m_razorActive = on;
  setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
}

// ---- ZtoryAudioTrack mouse events (12c: preview bar, razor) ----

void ZtoryAudioTrack::mousePressEvent(QMouseEvent *e) {
  static const int kScrubBarH = 6;
  // Razor tool: click anywhere on the waveform area (not in the scrub bar)
  if (m_razorActive && e->y() < height() - kScrubBarH) {
    int frame = frameAtX(e->x());
    emit razorRequested(m_col, frame);
    return;
  }
  // Scrub bar drag
  if (e->y() >= height() - kScrubBarH) {
    m_draggingPreview  = true;
    int frame = frameAtX(e->x());
    m_previewDragStart = frame;
    m_previewR0 = m_previewR1 = frame;
    update();
    return;
  }
  // Select / drag segment
  if (e->button() == Qt::LeftButton && !m_razorActive) {
    int frame = frameAtX(e->x());
    auto segs = findSegments();
    m_selSeg = {-1, -1};
    for (auto &s : segs) {
      if (frame >= s.r0 && frame <= s.r1) {
        m_selSeg = s;
        m_draggingSeg = true;
        m_dragStartFrame = frame;
        m_dragOrigR0 = s.r0;
        setCursor(Qt::ClosedHandCursor);
        break;
      }
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
  if (m_draggingSeg && m_selSeg.r0 >= 0) {
    int frame = frameAtX(e->x());
    int delta = frame - m_dragStartFrame;
    int newR0 = m_dragOrigR0 + delta;
    if (newR0 < 0) newR0 = 0;
    int segLen = m_selSeg.r1 - m_selSeg.r0;
    // Update visual selection to show where the segment would land
    m_selSeg.r0 = newR0;
    m_selSeg.r1 = newR0 + segLen;
    update();
    return;
  }
}

void ZtoryAudioTrack::mouseReleaseEvent(QMouseEvent *e) {
  // Finish segment drag — move cells in xsheet
  if (m_draggingSeg) {
    m_draggingSeg = false;
    setCursor(m_razorActive ? Qt::CrossCursor : Qt::ArrowCursor);
    int frame = frameAtX(e->x());
    int delta = frame - m_dragStartFrame;
    if (delta != 0 && m_dragOrigR0 >= 0) {
      int origR0 = m_dragOrigR0;
      int segLen = m_selSeg.r1 - m_selSeg.r0;
      int origR1 = origR0 + segLen;
      int newR0 = origR0 + delta;
      if (newR0 < 0) { delta = -origR0; newR0 = 0; }
      TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
      if (xsh) {
        // Collect cells from original position
        int count = segLen + 1;
        std::vector<TXshCell> cells(count);
        for (int i = 0; i < count; i++)
          cells[i] = xsh->getCell(origR0 + i, m_col);
        // Clear original
        for (int i = 0; i < count; i++)
          xsh->clearCells(origR0 + i, m_col);
        // Place at new position
        for (int i = 0; i < count; i++)
          xsh->setCell(newR0 + i, m_col, cells[i]);
        xsh->updateFrameCount();
        m_selSeg = {newR0, newR0 + segLen};
        m_waveformDirty = true;
        TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      }
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

    TSoundTrackP st = ctrl->soundTrack();
    if (!st) {
      TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
      prop->m_isPreview = true;
      st = TSoundTrackP(xsh->makeSound(prop));
      if (st) ctrl->setSoundTrack(st);
    }
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
  clipboardCopy(src);
  if (s_audioClipboard.cells.empty()) return;
  s_audioClipboard.isCut = true;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  int r0 = src->m_selSeg.r0, r1 = src->m_selSeg.r1;
  for (int r = r0; r <= r1; r++)
    xsh->clearCells(r, src->m_col);
  xsh->updateFrameCount();
  src->m_selSeg = {-1, -1};
  src->m_waveformDirty = true;
  src->update();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
}

void ZtoryAudioTrack::clipboardPaste(ZtoryAudioTrack *dst, int frame) {
  if (s_audioClipboard.cells.empty()) return;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  int count = (int)s_audioClipboard.cells.size();
  for (int i = 0; i < count; i++)
    xsh->setCell(frame + i, dst->m_col, s_audioClipboard.cells[i]);
  xsh->updateFrameCount();
  dst->m_selSeg = {frame, frame + count - 1};
  dst->m_waveformDirty = true;
  dst->update();
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
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
    if (m_selSeg.r0 >= 0) {
      TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
      if (xsh) {
        for (int r = m_selSeg.r0; r <= m_selSeg.r1; r++)
          xsh->clearCells(r, m_col);
        xsh->updateFrameCount();
        m_selSeg = {-1, -1};
        m_waveformDirty = true;
        update();
        TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
      }
    }
    return;
  }
  QWidget::keyPressEvent(e);
}

// ---- ZtoryAnimaticTrack ----

ZtoryAnimaticTrack::ZtoryAnimaticTrack(QWidget *parent) : QWidget(parent) {
  setFixedHeight(80);
  setMouseTracking(true);
}

void ZtoryAnimaticTrack::updateCursor() {
  if (m_tool == RazorTool)
    setCursor(Qt::CrossCursor);
  else
    unsetCursor();
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
  p.setPen(QColor(200, 200, 200));
  p.setFont(QFont("Arial", 9));
  ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
  QString sceneName = scene
      ? QString::fromStdWString(scene->getSceneName())
      : tr("Animatic");
  p.drawText(4, 0, kLabelW - 4, height(),
             Qt::AlignVCenter | Qt::AlignLeft, sceneName);
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
}

void ZtoryAnimaticTrack::mousePressEvent(QMouseEvent *e) {
  int mx = e->x() - kLabelW;
  if (mx < 0) return; // click on label area — ignore

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
    // FlipConsole uses 1-based frame numbers; controller uses 0-based.
    int newFrame = frame - 1;  // 0-based
    int oldFrame = ctrl->currentFrame();
    ctrl->setCurrentFrame(newFrame);
    // Audio scrub when scrubbing (not during play) — mirrors native onDrawFrame.
    if (!ctrl->frameHandle()->isPlaying() && oldFrame != newFrame) {
      TXsheet *xsh = ctrl->mainXsheet();
      if (xsh) {
        ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
        double fps = scene
            ? scene->getProperties()->getOutputProperties()->getFrameRate()
            : 24.0;
        ctrl->frameHandle()->scrubXsheet(newFrame, newFrame, xsh, fps);
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
  int totalFrames  = mainXsh->getFrameCount();
  if (totalFrames <= 0) totalFrames = 1;
  int currentFrame = ctrl->currentFrame();  // 0-based
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
// Builds m_sound from the MAIN xsheet's audio columns.
// The base hasSoundtrack() uses TApp::getCurrentXsheet() which, when inside a
// sub-scene, returns the sub-scene (no audio) → m_sound null → no playback.
void ZtoryAnimaticViewer::refreshAnimaticSound() {
  m_sound         = nullptr;
  m_hasSoundtrack = false;
  m_first         = true;
  TXsheet *mainXsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!mainXsh) return;
  TXsheet::SoundProperties *prop = new TXsheet::SoundProperties();
  if (m_sceneViewer && !m_sceneViewer->isPreviewEnabled())
    prop->m_isPreview = true;
  try {
    m_sound = mainXsh->makeSound(prop);
    // Cache merged track in controller for ruler scrubbing and preview bar
    ZtoryAnimaticController::instance()->setSoundTrack(
        TSoundTrackP(m_sound));
  } catch (...) {}
  m_hasSoundtrack = (m_sound != nullptr);
}

// ---- playAnimaticAudioFrame ----
// Like BaseViewerPanel::playAudioFrame but calls play()/stopScrub() on
// ctrl->mainXsheet() instead of TApp::getCurrentXsheet()->getXsheet().
void ZtoryAnimaticViewer::playAnimaticAudioFrame(int frame) {
  if (!m_sound) return;
  if (m_first) {
    m_first         = false;
    ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
    if (!scene) return;
    m_fps             = scene->getProperties()->getOutputProperties()
                            ->getFrameRate();
    m_samplesPerFrame = m_sound->getSampleRate() / std::abs(m_fps);
  }
  TXsheet *mainXsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!mainXsh) return;
  m_viewerFps      = m_flipConsole->getCurrentFps();
  double s0        = frame * m_samplesPerFrame;
  double s1        = s0 + m_samplesPerFrame;
  if (m_fps < m_viewerFps) mainXsh->stopScrub();
  mainXsh->play(m_sound, s0, s1, false);
}

// ---- onAnimaticPlayingStatusChanged ----
// Runs AFTER base's onPlayingStatusChanged (both connected to playStateChanged).
// Base calls hasSoundtrack() using the sub-scene xsheet → m_sound = null.
// We immediately override with the correct main-xsheet sound.
void ZtoryAnimaticViewer::onAnimaticPlayingStatusChanged(bool playing) {
  if (playing) refreshAnimaticSound();
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
  m_zoomSlider->setRange(20, 640);
  m_zoomSlider->setValue((int)(m_ppf * 10));
  m_zoomSlider->setMaximumWidth(160);
  m_zoomSlider->setToolTip("Zoom (pixels per frame)");
  tbLay->addWidget(zoomLabel);
  tbLay->addWidget(m_zoomSlider);
  tbLay->addSpacing(12);

  QPushButton *selectBtn = new QPushButton("Select", toolbar);
  selectBtn->setCheckable(true);
  selectBtn->setChecked(true);
  selectBtn->setMaximumWidth(60);
  selectBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:checked{background:#555;color:white;}"
    "QPushButton:hover{background:#4a4a4a;}");

  QPushButton *razorBtn = new QPushButton("Razor", toolbar);
  razorBtn->setCheckable(true);
  razorBtn->setMaximumWidth(60);
  razorBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:checked{background:#6a3a2a;color:white;}"
    "QPushButton:hover{background:#4a4a4a;}");

  QPushButton *linkBtn = new QPushButton("A|V Link", toolbar);
  linkBtn->setCheckable(true);
  linkBtn->setChecked(true);
  linkBtn->setMaximumWidth(70);
  linkBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:checked{background:#2a5a3a;color:white;}"
    "QPushButton:hover{background:#4a4a4a;}");
  linkBtn->setToolTip("Link/Unlink audio and video tracks");

  QPushButton *mergeBtn = new QPushButton("Merge", toolbar);
  mergeBtn->setMaximumWidth(60);
  mergeBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:hover{background:#5a4a2a;color:white;}");
  mergeBtn->setToolTip("Merge selected shots into the first selected shot");

  tbLay->addWidget(selectBtn);
  tbLay->addWidget(razorBtn);
  tbLay->addSpacing(8);
  tbLay->addWidget(linkBtn);
  tbLay->addSpacing(8);
  tbLay->addWidget(mergeBtn);
  tbLay->addSpacing(12);

  // Onion skin toggle — controls the animatic's local onion state,
  // independent from the native timeline.
  QPushButton *onionBtn = new QPushButton("Onion", toolbar);
  onionBtn->setCheckable(true);
  onionBtn->setChecked(false);
  onionBtn->setMaximumWidth(60);
  onionBtn->setToolTip(tr(
    "Toggle onion skin.\n"
    "Click the F strip (top ruler) to pin fixed-frame markers.\n"
    "Click the R strip (bottom ruler) to add relative markers.\n"
    "Click an existing marker to remove it."));
  onionBtn->setStyleSheet(
    "QPushButton{background:#3a3a3a;color:#ccc;border-radius:3px;padding:2px 6px;font-size:11px;}"
    "QPushButton:checked{background:#5a3a6a;color:white;}"
    "QPushButton:hover{background:#4a4a4a;}");
  tbLay->addWidget(onionBtn);
  tbLay->addStretch(1);

  // Ruler ↔ button sync (ruler can auto-enable when first marker is placed)
  connect(m_ruler, &ZtoryAnimaticRuler::onionEnabledChanged,
          toolbar, [onionBtn](bool on) {
    onionBtn->blockSignals(true);
    onionBtn->setChecked(on);
    onionBtn->blockSignals(false);
  });
  connect(onionBtn, &QPushButton::toggled, m_ruler,
          &ZtoryAnimaticRuler::setOnionEnabled);

  connect(selectBtn, &QPushButton::clicked, this, [this, selectBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::SelectTool);
    selectBtn->setChecked(true);
    razorBtn->setChecked(false);
    for (auto *at : m_audioTracks) at->setRazorActive(false);
  });
  connect(razorBtn, &QPushButton::clicked, this, [this, selectBtn, razorBtn](){
    m_track->setTool(ZtoryAnimaticTrack::RazorTool);
    razorBtn->setChecked(true);
    selectBtn->setChecked(false);
    // Independent audio razor only when link is OFF; linked razor is handled
    // automatically in onRazorRequested when a video shot is cut.
    for (auto *at : m_audioTracks) at->setRazorActive(!m_audioLinked);
  });
  connect(linkBtn, &QPushButton::toggled, this, [this](bool checked){
    m_audioLinked = checked;
  });
  connect(mergeBtn, &QPushButton::clicked, this, &ZtoryAnimaticPanel::onMergeShots);

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
  m_refreshing = false;
}

void ZtoryAnimaticPanel::refreshAudioTracks() {
  if (m_refreshingAudio) return;
  m_refreshingAudio = true;

  // Rimuovi tracce audio esistenti
  for (auto *at : m_audioTracks) {
    m_scrollLay->removeWidget(at);
    delete at;
  }
  m_audioTracks.clear();

  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) return;

  // Trova tutte le colonne audio nel main xsheet
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column) continue;
    TXshSoundColumn *sc = column->getSoundColumn();
    if (!sc || sc->isEmpty()) continue;

    QString name = QString::fromStdString(xsh->getStageObject(xsh->getColumnObjectId(col))->getName());
    if (name.isEmpty()) name = tr("Audio %1").arg(col + 1);

    ZtoryAudioTrack *at = new ZtoryAudioTrack(col, name, m_scrollContent);
    at->setPixelsPerFrame(m_ppf);
    at->setCurrentFrame(m_track->property("currentFrame").toInt());
    // Propagate current razor state (independent mode = razor ON + link OFF)
    at->setRazorActive(m_track->tool() == ZtoryAnimaticTrack::RazorTool && !m_audioLinked);
    connect(at, &ZtoryAudioTrack::razorRequested,
            this, &ZtoryAnimaticPanel::onAudioRazorRequested);
    // segmentMoved just invalidates the waveform (no full refresh needed)

    // Inserisci prima dello stretch finale
    int insertIdx = m_scrollLay->count() - 1;
    if (insertIdx < 0) insertIdx = 0;
    m_scrollLay->insertWidget(insertIdx, at);
    m_audioTracks.append(at);
  }
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

// Helper: split a sound column at |splitFrame| in the main xsheet.
// Cells from splitFrame..ar1 are moved to a new column appended at the end.
// If splitFrame is outside the column range, nothing happens.
// Split audio in-place: clear the cell at the split frame so the waveform
// shows two distinct segments in the same column (no extra track created).
static void splitAudioColumn(TXsheet *xsh, int audioCol, int splitFrame) {
  TXshColumn *col = xsh->getColumn(audioCol);
  if (!col) return;
  int ar0 = 0, ar1 = 0;
  col->getRange(ar0, ar1);
  if (splitFrame <= ar0 || splitFrame > ar1) return;
  // Clear a single cell at the split point to create a visible gap
  xsh->clearCells(splitFrame, audioCol);
  xsh->updateFrameCount();
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

// Helper: copy all cell content from srcCl's child xsheet into dstCl's
// child xsheet starting at frame offset |dstOffset| (0-based).
// Used by merge operations so that the destination sub-scene contains
// the actual drawings/animation of the absorbed shot, not just empty frames.
static void copyChildXsheetFrames(TXshChildLevel *dstCl, TXshChildLevel *srcCl,
                                   int dstOffset) {
  if (!dstCl || !srcCl) return;
  TXsheet *dstXsh = dstCl->getXsheet();
  TXsheet *srcXsh = srcCl->getXsheet();
  if (!dstXsh || !srcXsh) return;

  // Copy cells (drawings)
  int srcMaxF = srcXsh->getFrameCount();
  int srcNCols = srcXsh->getColumnCount();
  for (int c = 0; c < srcNCols; c++) {
    for (int r = 0; r < srcMaxF; r++) {
      TXshCell cell = srcXsh->getCell(r, c);
      if (!cell.isEmpty())
        dstXsh->setCell(dstOffset + r, c, cell);
    }
  }

  // Copy stage object keyframes (column transforms + camera) with offset
  TStageObjectTree *srcTree = srcXsh->getStageObjectTree();
  TStageObjectTree *dstTree = dstXsh->getStageObjectTree();
  if (srcTree && dstTree) {
    // Copy column keyframes
    for (int c = 0; c < srcNCols; c++) {
      TStageObject *srcObj =
          srcTree->getStageObject(TStageObjectId::ColumnId(c), false);
      TStageObject *dstObj =
          dstTree->getStageObject(TStageObjectId::ColumnId(c), false);
      if (!srcObj || !dstObj) continue;
      TStageObject::KeyframeMap kfs;
      srcObj->getKeyframes(kfs);
      for (auto &kv : kfs)
        dstObj->setKeyframeWithoutUndo(kv.first + dstOffset, kv.second);
    }
    // Copy camera keyframes
    TStageObject *srcCam =
        srcTree->getStageObject(TStageObjectId::CameraId(0), false);
    TStageObject *dstCam =
        dstTree->getStageObject(TStageObjectId::CameraId(0), false);
    if (srcCam && dstCam) {
      TStageObject::KeyframeMap kfs;
      srcCam->getKeyframes(kfs);
      for (auto &kv : kfs)
        dstCam->setKeyframeWithoutUndo(kv.first + dstOffset, kv.second);
    }
  }

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

  // Append frames from each subsequent shot into dstCol.
  // Insert keyframes at merge boundaries to preserve camera/level movement:
  // - last frame of the first shot
  // - first and last frame of each subsequent shot segment
  int appendAt = dstR1 + 1;
  int dstDuration = dstR1 - dstR0 + 1;
  int lastFrameNum = dstDuration; // 1-based frame index for continuation

  // Keyframe on last frame of first shot (0-based in child xsheet)
  addRazorKeyframes(dstCl, dstDuration - 1);

  for (int i = 1; i < (int)sortedCols.size(); i++) {
    int srcCol = sortedCols[i];
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    if (!srcColumn) continue;
    int r0 = 0, r1 = 0;
    srcColumn->getRange(r0, r1);
    int duration = r1 - r0 + 1;

    // Find src child level and copy its sub-scene content into dstCl
    TXshChildLevel *srcCl = nullptr;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, srcCol);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        srcCl = cell.m_level->getChildLevel();
        break;
      }
    }
    int segmentStart = lastFrameNum; // 0-based offset in dst child xsheet
    copyChildXsheetFrames(dstCl, srcCl, lastFrameNum);

    // Keyframe at first frame of this segment
    addRazorKeyframes(dstCl, segmentStart);
    // Keyframe at last frame of this segment
    addRazorKeyframes(dstCl, segmentStart + duration - 1);

    // Extend main xsheet column to cover the merged duration
    for (int r = 0; r < duration; r++)
      xsh->setCell(appendAt + r, dstCol, TXshCell(dstCl, TFrameId(++lastFrameNum)));
    appendAt += duration;
  }

  // Delete source columns in reverse order (skip first = destination)
  for (int i = (int)sortedCols.size() - 1; i >= 1; i--)
    ColumnCmd::deleteColumn(sortedCols[i]);

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  emit ZtoryModel::instance()->modelReset();
  m_track->refreshFromScene();
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

  // Copy src sub-scene content into dst sub-scene at the correct offset.
  // Insert keyframes at merge boundary to preserve camera/level movement.
  int dstDuration = dstR1 - dstR0 + 1;
  addRazorKeyframes(dstCl, dstDuration - 1); // last frame of first shot
  copyChildXsheetFrames(dstCl, srcCl, dstDuration);
  int srcDuration = srcR1 - srcR0 + 1;
  addRazorKeyframes(dstCl, dstDuration);                    // first frame of second shot
  addRazorKeyframes(dstCl, dstDuration + srcDuration - 1);  // last frame of second shot

  // Extend dst column in main xsheet to cover the merged duration
  int appendAt = dstR1 + 1;
  int lastFrameNum = dstDuration;
  int duration = srcDuration;
  for (int r = 0; r < duration; r++)
    xsh->setCell(appendAt + r, col, TXshCell(dstCl, TFrameId(++lastFrameNum)));

  ColumnCmd::deleteColumn(nextCol);

  xsh->updateFrameCount();
  app->getCurrentXsheet()->notifyXsheetChanged();
  ZtoryModel::instance()->resequenceXsheet();
  emit ZtoryModel::instance()->modelReset();
  m_track->refreshFromScene();
}

// Helper: log column range to debug file
static void razorLog(FILE *f, TXsheet *xsh, int col, const char *label) {
  TXshColumn *c = xsh->getColumn(col);
  if (!c) { fprintf(f, "  %s: col %d NULL\n", label, col); return; }
  int r0 = 0, r1 = -1;
  c->getRange(r0, r1);
  bool locked = c->isLocked();
  fprintf(f, "  %s: col %d range %d..%d (dur %d) locked=%d empty=%d\n",
          label, col, r0, r1, r1 >= r0 ? r1 - r0 + 1 : 0, locked, c->isEmpty());
}

void ZtoryAnimaticPanel::onRazorRequested(int col, int splitFrame) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) return;
  TXsheet *mainXsh = scene->getChildStack()->getTopXsheet();
  if (!mainXsh) return;

  FILE *dbg = fopen("/tmp/razor_debug.log", "a");
  if (dbg) fprintf(dbg, "\n=== RAZOR col=%d splitFrame=%d ===\n", col, splitFrame);

  TXshColumn *srcColumn = mainXsh->getColumn(col);
  if (!srcColumn) { if (dbg) { fprintf(dbg, "  srcColumn NULL\n"); fclose(dbg); } return; }
  int r0 = 0, r1 = 0;
  srcColumn->getRange(r0, r1);
  // splitRel = # frames that stay in the original shot (rows r0..splitFrame-1)
  int splitRel = splitFrame - r0;
  if (splitRel <= 0 || splitRel >= r1 - r0 + 1) {
    if (dbg) { fprintf(dbg, "  ABORT: splitRel=%d out of range (r0=%d r1=%d)\n", splitRel, r0, r1); fclose(dbg); }
    return;
  }

  int totalDuration = r1 - r0 + 1;
  int secondHalf    = totalDuration - splitRel;  // frames for the new shot

  if (dbg) fprintf(dbg, "  r0=%d r1=%d totalDur=%d splitRel=%d secondHalf=%d\n",
                   r0, r1, totalDuration, splitRel, secondHalf);

  // Grab original child level before cloning.
  TXshChildLevel *origCL = nullptr;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, col);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      origCL = cell.m_level->getChildLevel();
      break;
    }
  }
  if (dbg) fprintf(dbg, "  origCL=%p\n", (void *)origCL);

  // Clone → creates a fully independent sub-scene at col+1 with cells r0..r1.
  if (dbg) { fprintf(dbg, "  before cloneChild:\n"); razorLog(dbg, mainXsh, col, "orig"); }
  ColumnCmd::cloneChild(col);
  int newCol = col + 1;
  if (dbg) { fprintf(dbg, "  after cloneChild:\n"); razorLog(dbg, mainXsh, col, "orig"); razorLog(dbg, mainXsh, newCol, "clone"); }

  // Grab clone's child level.
  TXshChildLevel *cloneCL = nullptr;
  for (int r = r0; r <= r1; r++) {
    TXshCell cell = mainXsh->getCell(r, newCol);
    if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
      cloneCL = cell.m_level->getChildLevel();
      break;
    }
  }
  if (dbg) fprintf(dbg, "  cloneCL=%p\n", (void *)cloneCL);

  // Trim original's child xsheet: remove frames after splitRel so that
  // the sub-scene only contains the first part of the shot.
  if (origCL) {
    TXsheet *childXsh = origCL->getXsheet();
    if (childXsh) {
      int maxF  = childXsh->getFrameCount();
      int nCols = childXsh->getColumnCount();
      if (dbg) fprintf(dbg, "  orig childXsh: maxF=%d nCols=%d, trimming to %d\n", maxF, nCols, splitRel);
      for (int c = 0; c < nCols; c++) {
        for (int r = splitRel; r < maxF; r++)
          childXsh->clearCells(r, c);
      }
      childXsh->updateFrameCount();
      if (dbg) fprintf(dbg, "  orig childXsh after trim: frameCount=%d\n", childXsh->getFrameCount());
    }
  }

  // Fix clone's child xsheet: remove the first splitRel frames so that
  // frame 1 of the new sub-scene corresponds to the cut point.
  if (cloneCL) {
    TXsheet *childXsh = cloneCL->getXsheet();
    if (childXsh) {
      int maxF  = childXsh->getFrameCount();
      int nCols = childXsh->getColumnCount();
      if (dbg) fprintf(dbg, "  clone childXsh: maxF=%d nCols=%d\n", maxF, nCols);
      for (int c = 0; c < nCols; c++) {
        // Collect source cells into a temp buffer first — writing to the
        // same column we read from can trigger vector reallocation inside
        // TXshCellColumn::setCell, invalidating the source data.
        std::vector<TXshCell> buf(secondHalf);
        for (int r = 0; r < secondHalf && (splitRel + r) < maxF; r++)
          buf[r] = childXsh->getCell(splitRel + r, c);
        // Clear entire column range, then write back from buffer.
        for (int r = 0; r < maxF; r++)
          childXsh->clearCells(r, c);
        for (int r = 0; r < secondHalf; r++) {
          if (!buf[r].isEmpty())
            childXsh->setCell(r, c, buf[r]);
        }
      }
      childXsh->updateFrameCount();
      if (dbg) fprintf(dbg, "  clone childXsh after shift: frameCount=%d\n", childXsh->getFrameCount());
    }
  }

  // Trim original column: remove the tail (secondHalf cells).
  if (dbg) { fprintf(dbg, "  before removeCells(%d, %d, %d):\n", r0 + splitRel, col, secondHalf); razorLog(dbg, mainXsh, col, "orig"); }
  mainXsh->removeCells(r0 + splitRel, col, secondHalf);
  if (dbg) { fprintf(dbg, "  after removeCells:\n"); razorLog(dbg, mainXsh, col, "orig"); }

  // Rebuild clone column: wipe then repopulate with TFrameId(1..secondHalf).
  mainXsh->clearCells(r0, newCol, totalDuration + 2);
  if (dbg) { fprintf(dbg, "  after clearCells clone:\n"); razorLog(dbg, mainXsh, newCol, "clone"); }
  if (cloneCL) {
    for (int r = 0; r < secondHalf; r++)
      mainXsh->setCell(r0 + r, newCol, TXshCell(cloneCL, TFrameId(r + 1)));
  }
  if (dbg) { fprintf(dbg, "  after repopulate clone:\n"); razorLog(dbg, mainXsh, newCol, "clone"); }

  // Insert keyframes at the cut boundary.
  addRazorKeyframes(origCL, splitRel - 1);
  addRazorKeyframes(cloneCL, 0);

  // Notify the board FIRST (before resequenceXsheet emits modelReset).
  emit ZtoryModel::instance()->shotAdded(newCol);

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

  // Log final state of ALL columns.
  if (dbg) {
    fprintf(dbg, "  === FINAL STATE ===\n");
    for (int c = 0; c < mainXsh->getColumnCount(); c++)
      razorLog(dbg, mainXsh, c, "final");
    fclose(dbg);
  }

  m_track->refreshFromScene();
  refreshAudioTracks();
}

void ZtoryAnimaticPanel::onAudioRazorRequested(int col, int frame) {
  if (!ZtoryModel::assertMainXsheet(/*showWarning=*/false)) return;
  TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
  if (!xsh) return;
  splitAudioColumn(xsh, col, frame);
  TApp::instance()->getCurrentXsheet()->notifyXsheetChanged();
  refreshAudioTracks();
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

  // Audio link: if shot duration changed, shift audio tracks that start
  // after the shot's (shorter) endpoint by the same durationDelta.
  // Must happen BEFORE resequenceXsheet() moves video columns, otherwise
  // we'd need to re-read positions afterward.
  int durationDelta = newDuration - currentDuration;
  if (m_audioLinked && durationDelta != 0) {
    // Boundary: first frame after the shorter of old/new shot end.
    // Audio starting at or after this point belongs to subsequent shots.
    int boundary = r0 + qMin(currentDuration, newDuration);
    for (auto *at : m_audioTracks) {
      int audioCol = at->columnIndex();
      TXshColumn *ac = mainXsh->getColumn(audioCol);
      if (!ac) continue;
      int ar0 = 0, ar1 = 0;
      ac->getRange(ar0, ar1);
      if (ar0 < boundary) continue; // audio starts inside or before this shot
      int dur = ar1 - ar0 + 1;
      std::vector<TXshCell> cells(dur);
      for (int r = ar0; r <= ar1; r++)
        cells[r - ar0] = mainXsh->getCell(r, audioCol);
      for (int r = ar0; r <= ar1; r++)
        mainXsh->clearCells(r, audioCol);
      int newAr0 = qMax(0, ar0 + durationDelta);
      for (int r = 0; r < dur; r++) {
        if (!cells[r].isEmpty())
          mainXsh->setCell(newAr0 + r, audioCol, cells[r]);
      }
    }
    mainXsh->updateFrameCount();
  }

  resequenceXsheet();
  m_track->refreshFromScene();
}

void ZtoryAnimaticPanel::resequenceXsheet() {
  if (!m_audioLinked) {
    ZtoryModel::instance()->resequenceXsheet();
    return;
  }

  // Audio-video link: shift audio segments to follow their associated shots.
  TApp *app = TApp::instance();
  ToonzScene *scene = app->getCurrentScene()->getScene();
  if (!scene) { ZtoryModel::instance()->resequenceXsheet(); return; }
  TXsheet *xsh = scene->getChildStack()->getTopXsheet();
  if (!xsh) { ZtoryModel::instance()->resequenceXsheet(); return; }

  // 1. Record shot positions BEFORE resequence
  struct ShotPos { int col; int r0; int duration; };
  std::vector<ShotPos> oldPositions;
  for (int col = 0; col < xsh->getColumnCount(); col++) {
    TXshColumn *column = xsh->getColumn(col);
    if (!column || column->isEmpty()) continue;
    int r0 = 0, r1 = 0;
    column->getRange(r0, r1);
    // Only child-level columns (video shots)
    bool isChild = false;
    for (int r = r0; r <= r1; r++) {
      TXshCell cell = xsh->getCell(r, col);
      if (!cell.isEmpty() && cell.m_level && cell.m_level->getChildLevel()) {
        isChild = true; break;
      }
    }
    if (isChild) oldPositions.push_back({col, r0, r1 - r0 + 1});
  }

  // 2. Record audio segment positions BEFORE resequence
  struct AudioSeg { int col; int r0; int r1; std::vector<TXshCell> cells; };
  std::vector<AudioSeg> audioSegs;
  for (auto *at : m_audioTracks) {
    auto segs = at->findSegments();
    for (auto &s : segs) {
      AudioSeg as;
      as.col = at->columnIndex();
      as.r0 = s.r0;
      as.r1 = s.r1;
      for (int r = s.r0; r <= s.r1; r++)
        as.cells.push_back(xsh->getCell(r, as.col));
      audioSegs.push_back(as);
    }
  }

  // 3. Clear all audio cells
  for (auto &as : audioSegs)
    for (int r = as.r0; r <= as.r1; r++)
      xsh->clearCells(r, as.col);

  // 4. Run resequence on video columns
  ZtoryModel::instance()->resequenceXsheet();

  // 5. Read new shot positions AFTER resequence
  std::map<int, int> shotDelta; // col -> delta
  for (auto &op : oldPositions) {
    TXshColumn *column = xsh->getColumn(op.col);
    if (!column || column->isEmpty()) continue;
    int nr0 = 0, nr1 = 0;
    column->getRange(nr0, nr1);
    shotDelta[op.col] = nr0 - op.r0;
  }

  // 6. For each audio segment, find which shot it overlapped, apply same delta
  for (auto &as : audioSegs) {
    int audioMid = (as.r0 + as.r1) / 2;
    int bestDelta = 0;
    // Find the shot whose old range contains the audio segment's midpoint
    for (auto &op : oldPositions) {
      if (audioMid >= op.r0 && audioMid < op.r0 + op.duration) {
        auto it = shotDelta.find(op.col);
        if (it != shotDelta.end()) bestDelta = it->second;
        break;
      }
    }
    int newR0 = qMax(0, as.r0 + bestDelta);
    int count = (int)as.cells.size();
    for (int i = 0; i < count; i++) {
      if (!as.cells[i].isEmpty())
        xsh->setCell(newR0 + i, as.col, as.cells[i]);
    }
  }
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

  // Applica la nuova durata
  emit m_track->shotDurationChanged(col, newDuration - 1);
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
  QAction *chosen = menu.exec(e->globalPos());
  if (chosen == loadAudio) {
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


