# Ztoryc — Animatic Panel: Task List for Claude Code

> Generated from code review + design document (March 2026).
> Each task has: context, what to do, which files to touch, pitfalls.

---

## LEGEND

- **BUG** = existing code that is broken
- **NEW** = feature that does not exist yet
- **MOD** = existing code that needs modification

---

## 1. BUG — Animatic Viewer not visible / not independent

**Problem:** `ZtoryAnimaticViewer` is created inside `ZtoryAnimaticPanel` (line 363 of
`ztoryanimatic.cpp`) and placed in a splitter (viewer above, timeline below). However
the user reports it is not showing or not working. The viewer inherits
`BaseViewerPanel` with `m_sceneViewer->setAlwaysMainXsheet(true)` and disconnects
`activeViewerChanged`. Despite this, the viewer still shares `TFrameHandle` with the
normal viewer (documented as known bug in AGENTS.md).

**Root cause to investigate:**

1. The viewer may need an explicit `show()` call or a minimum size to appear.
2. `BaseViewerPanel` relies on being registered as the "active viewer" to receive
   paint updates. Since we disconnect `activeViewerChanged`, the viewer may never
   get repainted. Check if `onSceneChanged` / `updateFrameRange` are triggered.
3. The `TFrameHandle` sharing means that when the user navigates the animatic
   ruler, it moves the frame in whatever sub-scene is open in the normal viewer
   — this is confusing and must be fixed with a separate `TFrameHandle` instance.

**What to do:**

- Ensure the `ZtoryAnimaticViewer` widget is visible: check splitter sizes,
  add `m_animViewer->show()` in `showEvent`, verify minimum height.
- Add a dedicated `TFrameHandle *m_animaticFrameHandle` in `ZtoryAnimaticPanel`
  and wire it to `ZtoryAnimaticViewer` instead of `TApp::instance()->getCurrentFrame()`.
  This requires subclassing or patching `BaseViewerPanel` to accept an external
  frame handle (add a `setFrameHandle(TFrameHandle*)` method to
  `BaseViewerPanel` or `SceneViewer`).
- When the ruler/track changes frame, update only `m_animaticFrameHandle`.
- When TApp's frame changes from external sources, sync back to the animatic
  ruler/track only if we're at the main xsheet.

**Files:** `ztoryanimatic.h`, `ztoryanimatic.cpp`, possibly `viewerpane.h/cpp`,
`sceneviewer.h/cpp`.

**Pitfall:** Do NOT modify `TFrameHandle` globally. Create a new instance local
to the animatic panel.

---

## 1b. BUG — Shot operations must be guarded to main xsheet only ⚠️ CRITICAL

**Problem:** Any operation that modifies the main xsheet structure (reordering
shots, inserting/deleting columns, resequencing) will **silently corrupt the
scene** if the user is currently inside a sub-scene (`ancestorCount > 0`).
In that state, `TApp::instance()->getCurrentXsheet()` returns the **sub-scene**
xsheet, not the main one. Calling `insertColumn`, `removeColumn`, or
`resequenceXsheet` on it destroys the sub-scene's content.

**Affected operations (every single one needs the guard):**

- `resequenceXsheet()` — both in `storyboardpanel.cpp` and `ztoryanimatic.cpp`
- `onAddShot()`, `onDeleteShot()` — in `StoryboardPanel`
- `onCopyShot()`, `onCloneShot()`, `onPasteShot()` — in `StoryboardPanel`
- Shot drag-reorder in `ZtoryAnimaticTrack::mouseReleaseEvent`
- Duration drag (`onShotDurationChanged`) in `ZtoryAnimaticTrack`
- Any future shot operation added to either panel

**The guard (add to the top of every affected function):**

```cpp
// GUARD: shot operations must run on the main xsheet only.
ToonzScene *scene = TApp::instance()->getCurrentScene()->getScene();
if (scene->getChildStack()->getAncestorCount() > 0) {
    // Option A — silent no-op (preferred for drag operations):
    return;
    // Option B — warn the user (preferred for explicit button actions):
    DVGui::warning(tr("Cannot modify shots while editing a sub-scene.\n"
                      "Close the sub-scene first (double-click outside it)."));
    return;
}
```

Use **Option A** (silent return) for continuous operations like drag-resize
or drag-reorder, to avoid flooding the user with dialogs.
Use **Option B** (warning) for discrete actions like Add Shot, Delete Shot,
Paste Shot, so the user knows why nothing happened.

**Centralise in a helper (recommended):**

Add a static helper in `ztorymodel.h`:
```cpp
// Returns true if at main xsheet level; shows optional warning if not.
static bool assertMainXsheet(bool showWarning = false) {
    ToonzScene *s = TApp::instance()->getCurrentScene()->getScene();
    if (s->getChildStack()->getAncestorCount() == 0) return true;
    if (showWarning)
        DVGui::warning(QObject::tr(
            "Cannot modify shots while editing a sub-scene."));
    return false;
}
```

Then every affected function starts with:
```cpp
if (!ZtoryModel::assertMainXsheet(/*showWarning=*/true)) return;
```

**Files:** `ztorymodel.h`, `storyboardpanel.cpp`, `ztoryanimatic.cpp`.

**Pitfall:** Do NOT just use `ZtoryAnimaticController::instance()->mainXsheet()`
as the xsheet to operate on — that resolves the *data target* correctly but the
underlying Qt undo commands and `TApp` signals still reference
`getCurrentXsheet()`. The correct fix is to **block the operation** if not at
main level, rather than trying to redirect it.

---

## 2. BUG — Timing desync between Board and Animatic timeline

**Problem:** When the user modifies shot duration by dragging the border in the
animatic timeline (`onShotDurationChanged`), the Board panel does not always
reflect the updated duration. The Board's `onXsheetChanged` handler (line 1051
of `storyboardpanel.cpp`) updates `panels[0].duration` from `column->getRange()`,
but only if `getAncestorCount() == 0` (at main xsheet level). If the user is
inside a sub-scene while the animatic timeline changes the main xsheet, the
board won't update.

Also, `resequenceXsheet()` exists in BOTH `storyboardpanel.cpp` and
`ztoryanimatic.cpp` — two separate implementations that can diverge. The board
version uses `m_shots[i].data.totalDuration()` (from ZtoryModel data) while
the animatic version scans actual xsheet columns. This is a recipe for conflicts.

**What to do:**

- **Unify `resequenceXsheet()`** into `ZtoryModel` as a single source of truth.
  Both panels should call `ZtoryModel::instance()->resequenceXsheet()`.
- After any duration change in the animatic, emit a signal that the board
  can listen to. Use `ZtoryModel::shotDataChanged(int shotIdx)` for this.
- Make sure both panels connect to `ZtoryModel::modelReset` and
  `ZtoryModel::shotDataChanged` to refresh their UI.

**Files:** `ztorymodel.h/cpp`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`.

**Pitfall:** Avoid infinite signal loops (panel A changes data -> model emits
signal -> panel B updates -> panel B notifies model -> model emits again).
Use a guard flag or `blockSignals()`.

---

## 3. BUG — Copy does not work in Board (Clone works)

**Problem:** The user reports "Clone funziona ma Copy no." Looking at the code:

- `onCopyShot()` (line 1131) copies shot data to `m_clipboard` with `isClone=false`.
- `onCloneShot()` (line 1148) copies shot data to `m_clipboard` with `isClone=true`.
- `onPasteShot()` (line 1221) always calls `cloneChildToPosition()` regardless
  of `isClone` flag — the `isClone` flag is never checked!

For a **copy** (shared instance), the paste should reuse the same `TXshChildLevel`
(point to the same sub-scene), not create an independent clone. The current code
always creates an independent clone via `StageObjectsData::eDoClone`.

**What to do:**

- In `onPasteShot()`, check `m_clipboard[ci].isClone`:
  - If `true` (clone): call `cloneChildToPosition(srcCol, pos)` as now.
  - If `false` (copy/shared): insert a new column at `pos`, then copy the
    original column's cells (same `TXshChildLevel` pointer) into it. Something
    like:
    ```cpp
    xsh->insertColumn(pos);
    TXshColumn *srcColumn = xsh->getColumn(srcCol);
    int r0, r1;
    srcColumn->getRange(r0, r1);
    for (int r = r0; r <= r1; r++) {
        TXshCell cell = xsh->getCell(r, srcCol);
        if (!cell.isEmpty()) xsh->setCell(r, pos, cell);
    }
    ```
    This makes the new column point to the SAME sub-scene. Edits in one will
    appear in all copies (shared child level).

**Files:** `storyboardpanel.cpp` (around line 1221-1260).

**Pitfall:** After inserting a column, all column indices shift. The existing
correction loop (lines 1230-1234) handles this for `srcCol`, but verify it
works for the copy case too.

---

## 4. BUG — Duplicate button declarations in StoryboardPanel constructor

**Problem:** In `StoryboardPanel::StoryboardPanel()` (lines 417-445), the buttons
`m_copyButton`, `m_cloneButton`, and `m_pasteButton` are each created TWICE:

```cpp
m_copyButton = new QPushButton("Copy");       // line 417
...
m_copyButton = new QPushButton("Copy");       // line 432 — overwrites!
```

This means the first three button instances leak (never deleted) and the
toolbar adds both the first and second instances (`tb->addWidget` at lines
473-479 adds them twice).

**What to do:**

- Remove the duplicate declarations (lines 432-445).
- Remove the duplicate `tb->addWidget` calls (lines 477-479).

**Files:** `storyboardpanel.cpp` (constructor, around lines 417-479).

---

## 5. NEW — Story-strip (horizontal thumbnail strip)

**What:** A thin horizontal strip showing all shots as small thumbnails (no text
fields). Two arrow buttons on the sides for scrolling, plus a scroll bar.
Clicking a thumbnail navigates to that shot. Should be placed above the
animatic timeline (between viewer and ruler).

**Design (from user document):**

- Based on the board panel concept, but much smaller thumbnails
- No dialog/action/notes fields
- Shot number overlaid on each thumbnail
- Scroll arrows on left/right edges
- Current shot highlighted

**Where to put it:**

- Create a new class `ZtoryStoryStrip : public QWidget` in `ztoryanimatic.h/cpp`
  (or in its own file if preferred).
- Add it to `ZtoryAnimaticPanel` layout, between `m_animViewer` and the scroll
  area containing ruler+track.

**Implementation notes:**

- Reuse thumbnail generation from `StoryboardPanel::updatePreview()` or
  `ZtoryModel::preview()`.
- Each thumbnail: fixed height ~60px, width proportional to aspect ratio.
- Use a `QScrollArea` with horizontal scroll.
- Connect to `ZtoryModel::shotDataChanged` and `modelReset` for updates.

**Files:** `ztoryanimatic.h/cpp` (new class + integration in panel layout).

---

## 6. NEW — Animatic Timeline Toolbar

**What:** A toolbar above the timeline ruler with these tools:

### 6a. Zoom slider
- A `QSlider` (horizontal) that controls `m_ppf` (pixels per frame).
- Range: 2.0 to 64.0. Default: 8.0.
- Syncs with wheel zoom that already exists.

### 6b. Select tool
- Default tool. Single click selects a shot; double-click enters edit mode
  (calls `MI_OpenChild` on that column — same as board's `onEditShot`).
- Shift+click: add to selection. Ctrl/Cmd+click: toggle selection.
- Currently: single click selects (`shotClicked` signal), but:
  - No double-click handling
  - No multi-selection
  - No visual indication of selection in the track

### 6c. Razor tool
- Click on a shot at position X to split it into two shots at that frame.
- Implementation: given shot at column `col` with range `[r0, r1]` and click
  at frame `splitFrame`:
  1. Create a new column at `col+1`
  2. Move cells from `splitFrame` to `r1` into the new column
  3. The new column gets a new sub-scene containing only the frames after split
  4. Or simpler: just split the cell range and create a new child level with
     `cloneChildToPosition` trimmed to the second half
- Also supports splitting an audio track at the click position (razor on audio).

### 6d. Link/Unlink tool
- Toggles link between audio and video tracks.
- When linked: moving a shot also moves the corresponding audio region.
- When unlinked: audio stays fixed, video can be repositioned independently.
- Implementation: add a `bool m_audioLinked` flag per shot or as a global
  toggle. When dragging a shot in linked mode, also shift the audio columns.

### 6e. Add / Delete / Copy / Clone / Paste buttons
- Same operations as the Board toolbar, but operating on the timeline selection.
- Can reuse the same logic from `StoryboardPanel` or (better) move the logic
  into `ZtoryModel` and call it from both panels.

### 6f. Merge button
- Merges two or more selected shots into one.
- Use case: two shots with the same framing separated by an insert shot; when
  the insert is deleted, the two matching shots should be reunited.
- Implementation: take all selected shots, combine their sub-scene content into
  the first shot's sub-scene (append frames), delete the other shots' columns.

**Files:** `ztoryanimatic.h/cpp`, potentially `ztorymodel.h/cpp` for shared logic.

---

## 7. NEW — Double-click shot to enter edit mode

**What:** In the animatic track, double-clicking a shot block should enter
that shot's sub-scene (same as the Board's "Edit" button behavior).

**What to do:**

- Add `mouseDoubleClickEvent` to `ZtoryAnimaticTrack`.
- Detect which shot block was clicked.
- Emit a new signal `shotDoubleClicked(int col)`.
- In `ZtoryAnimaticPanel`, connect to this signal and call the same logic as
  `StoryboardPanel::onEditShot()`:
  ```cpp
  void ZtoryAnimaticPanel::onShotDoubleClicked(int col) {
      TApp *app = TApp::instance();
      // First close any open sub-scene
      ToonzScene *scene = app->getCurrentScene()->getScene();
      while (scene->getChildStack()->getAncestorCount() > 0)
          CommandManager::instance()->execute("MI_CloseChild");
      // Then open the target sub-scene
      app->getCurrentColumn()->setColumnIndex(col);
      TXshCell cell = app->getCurrentXsheet()->getXsheet()->getCell(0, col);
      if (cell.m_level && cell.m_level->getChildLevel()) {
          app->getCurrentFrame()->setFrame(0);
          CommandManager::instance()->execute("MI_OpenChild");
      }
  }
  ```

**Files:** `ztoryanimatic.h/cpp`.

---

## 8. NEW — Multi-selection in Animatic Track

**What:** Allow selecting multiple shots with Shift+click (range) and
Ctrl/Cmd+click (toggle), same as the board.

**What to do:**

- Add `std::set<int> m_selectedCols` to `ZtoryAnimaticTrack`.
- Modify `mousePressEvent` to handle modifiers.
- In `paintEvent`, draw selected shots with a highlight border (orange, same
  as the board's selection style).
- Emit `selectionChanged(std::set<int> selectedCols)` signal.

**Files:** `ztoryanimatic.h/cpp`.

---

## 9. NEW — Audio export with shot (reserved column)

**What (from user document — N.B. section):** When exporting shots as individual
`.tnz` scenes, the portion of audio from the main xsheet that corresponds to
each shot's frame range must be automatically inserted into the sub-scene.
There should be a reserved, non-editable column in the sub-scene to hold this
audio.

**Implementation:**

- In `StoryboardPanel::onExportShots()`, before calling
  `IoCmd::saveScene(outPath, IoCmd::SAVE_SUBXSHEET)`:
  1. Find all audio columns in the main xsheet.
  2. For each audio column, extract the audio data in the frame range
     `[shotStartFrame, shotStartFrame + shotDuration - 1]`.
  3. Insert this audio into the sub-scene's xsheet as a new sound column.
  4. Mark this column as "reserved" (read-only). Tahoma2D doesn't have a
     built-in column lock, so this might need a metadata flag or a special
     column name prefix like `_audio_main_`.
  5. After saving, remove the temporary audio column from the sub-scene.

**Files:** `storyboardpanel.cpp` (`onExportShots`), possibly new utility function.

**Pitfall:** The audio must be trimmed to the exact shot range. Use
`TXshSoundColumn::getSoundCell()` to read cells in the range, and insert
them at frame 0 in the sub-scene.

---

## 10. MOD — X-Sheet Panel in storyboard workflow

**What (from user document):** The native X-Sheet/Timeline panel, when used in
the storyboard workflow, should:

- Show only the sub-scene content (current shot being edited)
- NOT display the main xsheet level
- NOT import audio files (audio only in main xsheet)
- Double-click on the thumbnail in the storyboard enters edit mode

**This is largely about room configuration** rather than code changes:

- The BOARD and SHOTEDITOR rooms should configure the timeline panel to show
  only the current xsheet (which is already the default behavior when inside
  a sub-scene via `MI_OpenChild`).
- To prevent audio import in sub-scenes, add a check in the audio import
  command: if `getAncestorCount() > 0`, show a warning and refuse.

**Files:** Room layout configuration, possibly `iocommand.cpp` (audio import
guard).

---

## 11. MOD — Animatic Viewer toggle with ComboViewer

**What (from user document):** The Animatic Viewer could share the same space
as the ComboViewer via a toggle button, instead of always being a separate
widget. This saves screen space.

**Implementation option:**

- Add a toggle button (e.g., "Animatic View / Shot View") in the viewer area.
- Use a `QStackedWidget` containing both `m_animViewer` and the normal
  `SceneViewerPanel`.
- Toggle switches between index 0 (animatic = main xsheet playback) and
  index 1 (shot editor = sub-scene).

**Alternative:** Keep them separate but allow the user to hide/show the
animatic viewer via the panel's context menu or a collapse button.

**Files:** `ztoryanimatic.h/cpp` (panel layout).

---

## 12. NEW — Audio waveform display + scrubbing in Animatic Timeline ⭐ PRIORITY

**What:** Waveform display and audio scrubbing in `ZtoryAudioTrack` and
`ZtoryAnimaticRuler`, replicating exactly the native Tahoma2D timeline.
Do NOT reimplement — copy/adapt from native source.

**Status:**
- ✅ Play audio: works (refreshAnimaticSound + ZtoryAnimaticController)
- ✅ 12a Waveform: visible (confirmed by user)
- ❌ 12b Scrubbing on ruler drag: not working (audio only on play button)
- ❌ 12c Sound preview bar: missing

---

### 12a. Fix waveform rendering in `ZtoryAudioTrack`

**Root cause of broken pixel mapping:** Current code has `soundPixel = px`,
which maps screen pixel directly to sample index, ignoring fps and m_ppf.

**Where the native waveform is rendered:**
Search `xsheetviewer.cpp` for `drawSoundColumnHead` or `paintSoundBar` or
grep for `getSoundTrack` in `ColumnArea`. The native `CellArea::paintEvent`
handles sound cells — look specifically for a block that calls
`column->getSoundColumn()` then iterates over a sample range to draw
vertical lines.

**Correct pixel → sample mapping formula:**
```cpp
// Given: m_ppf (pixels per frame), fps from scene properties
double fps = scene->getProperties()->getOutputProperties()->getFrameRate();

// In ZtoryAudioTrack::paintEvent, for each pixel column px:
double frame    = (px - kLabelW) / m_ppf;          // fractional frame
TINT32 s0       = (TINT32)(frame * sampleRate / fps);
TINT32 s1       = s0 + (TINT32)(sampleRate / fps / m_ppf) + 1;
// s0..s1 = sample range covered by this pixel; clamp to [0, sampleCount)
```

**How to get the sound track and samples:**
```cpp
TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
// Find the first sound column in the main xsheet:
for (int c = 0; c < xsh->getColumnCount(); c++) {
    TXshSoundColumn *sc = xsh->getColumn(c)->getSoundColumn();
    if (!sc) continue;
    // Get the full merged sound track for the entire xsheet duration:
    TSoundTrack *st = sc->getSoundTrack(0, xsh->getFrameCount(), false);
    if (!st) continue;
    TINT32 sampleRate = st->getSampleRate();
    TINT32 sampleCount = st->getSampleCount();
    // For each pixel, compute s0..s1 as above, then:
    double minVal = 0, maxVal = 0;
    for (TINT32 s = s0; s < s1 && s < sampleCount; s++) {
        // TSoundTrack stores samples as 16-bit signed in stereo or mono:
        // Use st->getValue(s) or cast sample buffer directly.
        // Native code uses: TXshSoundColumn::getSoundBuffer() or
        // iterates via TSoundTrack::getSample(s, channel)
    }
    // Draw vertical line from mid-height-minVal to mid-height+maxVal
}
```

**Practical approach (copy from native):**
1. Open `xsheetviewer.cpp`, find `CellArea::drawSoundCell` or equivalent.
2. Copy the waveform-drawing block verbatim into
   `ZtoryAudioTrack::paintEvent`, replacing the native `x`-coordinate
   computation with `px = kLabelW + frame * m_ppf` (our coordinate system).
3. Call `update()` from `ZtoryAnimaticPanel` whenever `m_ppf` changes
   (already done via `onZoomChanged`).

**Caching:** The native code caches the waveform QPixmap when zoom/size
changes. Add a `QPixmap m_waveformCache` + `bool m_waveformDirty` flag;
regenerate only when `m_ppf` or xsheet changes.

---

### 12b. Audio scrubbing on ruler drag

**Where the native scrubbing is:**
Search `xsheetviewer.cpp` in `RowArea::mouseMoveEvent` (or `mousePressEvent`)
for a block that calls `TApp::instance()->getCurrentXsheet()` and then
`TSoundOutputDevice`. The key call is approximately:

```cpp
// Native (from RowArea):
TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
double fps = ...; // from scene
TSoundTrack *st = xsh->makeSound(0);   // or makeSoundTrack(r0, r1)
if (st) {
    TINT32 sRate = st->getSampleRate();
    double s      = frame * sRate / fps;
    TINT32 s0     = std::max(0, (TINT32)(s - sRate / (2 * fps)));
    TINT32 s1     = (TINT32)(s + sRate / (2 * fps));
    TSoundOutputDevice::instance()->play(st, s0, s1, false, true); // scrub=true
}
```

**Adaptation for `ZtoryAnimaticRuler::mouseMoveEvent`:**
```cpp
void ZtoryAnimaticRuler::mouseMoveEvent(QMouseEvent *e) {
    if (!(e->buttons() & Qt::LeftButton)) return;
    int frame = frameAtX(e->x());           // already exists
    ZtoryAnimaticController::instance()->setCurrentFrame(frame);

    // --- ADD: audio scrubbing ---
    TXsheet *xsh = TApp::instance()->getCurrentXsheet()->getXsheet();
    // Walk up to main xsheet (in case user is inside a sub-scene):
    while (TApp::instance()->getCurrentScene()->getScene()
               ->getChildStack()->getAncestorCount() > 0)
        xsh = xsh->getScene()->getTopXsheet(); // or use ctrl->mainXsheet()
    // Use ZtoryAnimaticController::instance()->mainXsheet() directly:
    xsh = ZtoryAnimaticController::instance()->mainXsheet();

    double fps = TApp::instance()->getCurrentScene()->getScene()
                     ->getProperties()->getOutputProperties()->getFrameRate();
    TSoundTrack *st = xsh->makeSound(0);
    if (st && TSoundOutputDevice::instance()->isDeviceAvailable()) {
        TINT32 sr = st->getSampleRate();
        TINT32 s0 = std::max(0, (TINT32)(frame * sr / fps - sr / fps / 2));
        TINT32 s1 = (TINT32)(frame * sr / fps + sr / fps / 2);
        TSoundOutputDevice::instance()->play(st, s0, s1, false, true);
    }
    // ---------------------
    update();
}
```

**Stop scrub on mouse release:**
```cpp
void ZtoryAnimaticRuler::mouseReleaseEvent(QMouseEvent *) {
    if (TSoundOutputDevice::instance()->isPlaying())
        TSoundOutputDevice::instance()->stop();
}
```

**Includes needed:** `#include "tsound.h"`, `#include "toonz/txsheet.h"`
(check if already present; add to `ztoryanimatic.cpp` includes).

**Files:** `ztoryanimatic.h/cpp`, reference: `xsheetviewer.cpp` (`RowArea`).

**Pitfall:** `xsh->makeSound(0)` may return nullptr if no sound column exists
— always null-check before using. Also, do NOT call `play()` while the
animatic FlipConsole is already playing (check `ctrl->isPlaying()` and skip
scrub if true, to avoid audio conflicts).

---

### 12b — DIAGNOSTIC: why scrubbing is still not working

The spec above is correct in principle, but `makeSound(0)` is the likely
culprit. Here are the four causes to check **in this order:**

**Cause 1 — `makeSound()` returns nullptr or a stale track.**
`TXsheet::makeSound()` rebuilds the merged soundtrack from all sound columns,
which may fail if no sound column is at the main-xsheet level when the user
is inside a sub-scene. Always verify with a qDebug before the `if (st)` block:
```cpp
qDebug() << "scrub: st=" << (void*)st << "frame=" << frame;
```
If nullptr → use the soundtrack already cached by `ZtoryAnimaticViewer`.

**Preferred fix — use the cached sound from ZtoryAnimaticController:**
Add a `TSoundTrackP m_soundTrack` member to `ZtoryAnimaticController` that
`ZtoryAnimaticViewer::refreshAnimaticSound()` fills when loading audio.
Then the ruler uses:
```cpp
TSoundTrackP st = ZtoryAnimaticController::instance()->soundTrack();
```
instead of calling `makeSound()` again. This avoids the rebuild cost and the
nullptr problem.

**Cause 2 — `TSoundOutputDevice::instance()->isDeviceAvailable()` returns false.**
Add a qDebug to check. On some macOS/Linux builds the device is only available
after first playback. Try removing the `isDeviceAvailable()` guard or calling
`TSoundOutputDevice::instance()->open()` once at startup.

**Cause 3 — `play()` is being called but immediately interrupted.**
`makeSound(0)` or `play()` might be triggering an internal stop signal that
the FlipConsole audio path then catches and kills. Check if any slot is
connected to `TSoundOutputDevice::soundStopped` that calls `stop()` or
resets state.

**Cause 4 — Mouse events not reaching the ruler.**
If the ruler is inside a `QScrollArea` or another widget that grabs mouse
events, `mouseMoveEvent` may not fire during drag. Verify with a qDebug in
`ZtoryAnimaticRuler::mouseMoveEvent`. If it never fires, add
`setMouseTracking(false)` (not needed) but check `grabMouse()` conflicts.
Also ensure the ruler has `Qt::LeftButton` tracked: use
`installEventFilter` on the scroll area if needed.

---

### 12c. NEW — Sound preview bar in `ZtoryAudioTrack`

**What:** A thin strip (≈6 px) at the bottom edge of `ZtoryAudioTrack`.
Click+drag selects a frame range; on mouse release the selected range is
played back. Identical to the native Tahoma2D sound column preview bar.

**Where the native one is:**
In `xsheetviewer.cpp`, search `ColumnArea` (or `SoundColumnArea`) for a
block handling clicks near the bottom of the sound column header. May be
under the name "playbar", "scrubbar" or just an `if (y > h - kScrubBarH)`
branch in `mousePressEvent`. Also look in `columnarea.cpp` if it exists as
a separate file.

**Rendering in `ZtoryAudioTrack::paintEvent`:**
```cpp
// Constants:
static const int kScrubBarH = 6;

// At the bottom of paintEvent, after the waveform:
QRect scrubBar(kLabelW, height() - kScrubBarH,
               width() - kLabelW, kScrubBarH);
painter.fillRect(scrubBar, QColor(60, 60, 60));

// If a preview range is selected (m_previewR0 <= m_previewR1):
if (m_previewR0 >= 0) {
    int x0 = kLabelW + (int)(m_previewR0 * m_ppf);
    int x1 = kLabelW + (int)((m_previewR1 + 1) * m_ppf);
    painter.fillRect(x0, height() - kScrubBarH,
                     x1 - x0, kScrubBarH, QColor(255,165,0));
}
```

**New members in `ZtoryAudioTrack`:**
```cpp
int m_previewR0 = -1;   // In frame of preview selection (-1 = none)
int m_previewR1 = -1;   // Out frame
bool m_draggingPreview = false;
int m_previewDragStart = -1;
```

**Mouse interaction:**
```cpp
void ZtoryAudioTrack::mousePressEvent(QMouseEvent *e) {
    if (e->y() >= height() - kScrubBarH) {
        m_draggingPreview = true;
        int frame = std::max(0, (int)((e->x() - kLabelW) / m_ppf));
        m_previewDragStart = frame;
        m_previewR0 = m_previewR1 = frame;
        update();
    }
}

void ZtoryAudioTrack::mouseMoveEvent(QMouseEvent *e) {
    if (!m_draggingPreview) return;
    int frame = std::max(0, (int)((e->x() - kLabelW) / m_ppf));
    m_previewR0 = std::min(m_previewDragStart, frame);
    m_previewR1 = std::max(m_previewDragStart, frame);
    update();
}

void ZtoryAudioTrack::mouseReleaseEvent(QMouseEvent *e) {
    if (!m_draggingPreview) return;
    m_draggingPreview = false;
    if (m_previewR0 < 0 || m_previewR1 < m_previewR0) return;

    // Play selected range:
    TSoundTrackP st = ZtoryAnimaticController::instance()->soundTrack();
    // fallback if controller has no cache:
    if (!st) st = ZtoryAnimaticController::instance()->mainXsheet()->makeSound(0);
    if (!st) return;

    double fps = TApp::instance()->getCurrentScene()->getScene()
                     ->getProperties()->getOutputProperties()->getFrameRate();
    TINT32 sr = st->getSampleRate();
    TINT32 s0 = (TINT32)(m_previewR0 * sr / fps);
    TINT32 s1 = (TINT32)((m_previewR1 + 1) * sr / fps);
    s0 = std::max(0, std::min(s0, (TINT32)st->getSampleCount() - 1));
    s1 = std::max(s0, std::min(s1, (TINT32)st->getSampleCount()));
    TSoundOutputDevice::instance()->play(st, s0, s1, false, false); // not scrub, plays to end
    update();
}
```

**Cursor:** Change cursor to `Qt::SizeHorCursor` when hovering over the
scrub bar strip (use `mouseMoveEvent` + `unsetCursor()`/`setCursor()`).

**Files:** `ztoryanimatic.h/cpp` (ZtoryAudioTrack).

---

## 13. NEW — Playhead, Onion skin, In/Out markers, Navigation tags in Animatic Ruler

**What:** Four visual/interactive features of the native timeline ruler to
replicate in `ZtoryAnimaticRuler`. In all cases: copy/adapt from native source,
do NOT reimplement. Primary reference: `XsheetGUI::RowArea` in `xsheetviewer.cpp`.

---

### 13a. Onion skin markers on the ruler

**Where the native onion skin is:**
In `xsheetviewer.cpp`, `RowArea::paintEvent` — search for `OnionSkinMask`
or `getCurrentOnionSkin`. The native code reads:
```cpp
OnionSkinMask mask =
    TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
// Relative offsets (frames before/after current):
QList<int> rel = mask.getRelativeOnionSkin();
// Absolute frames:
QList<int> abs = mask.getFixedOnionSkin();
```
For each frame in `rel`, it draws a small filled dot or secondary triangle
at `currentFrame + offset` in the ruler.

**Adaptation for `ZtoryAnimaticRuler::paintEvent`:**
```cpp
// After drawing the main playhead, add onion skin ghosts:
OnionSkinMask mask =
    TApp::instance()->getCurrentOnionSkin()->getOnionSkinMask();
if (mask.isEnabled()) {
    for (int rel : mask.getRelativeOnionSkin()) {
        int ghostFrame = currentFrame + rel;
        if (ghostFrame < 0) continue;
        int gx = kLabelW + (int)(ghostFrame * m_ppf);
        // Draw small semi-transparent triangle at gx:
        QColor ghostColor = rel < 0 ? QColor(255,100,100,120)
                                    : QColor(100,100,255,120);
        // ... paint small downward triangle at gx, ruler bottom half
    }
}
```

**Connect:** In `ZtoryAnimaticPanel` constructor, connect
`TApp::instance()->getCurrentOnionSkin()->onionSkinMaskChanged`
→ `m_ruler->update()`.

---

### 13b. In/Out markers on the ruler ⭐ PRIORITY

**Status / bugs reported by user:**
- ❌ Markers not visible on panel open — require Cmd+Shift+Click to appear
- ❌ No "Mark IN" / "Mark OUT" right-click menu
- ❌ Right-click moves the playhead (must not)
- ✅ Marker position is recorded correctly once set

---

#### Bug 1 — Markers invisible on open: auto-initialize play range

**Root cause:** `getPlayRange()` returns `r0 = -1` (unset) when the xsheet
has never had a play range set. The paint guard `if (r0 >= 0 && r1 > r0)`
then skips drawing entirely. On first open the markers simply don't exist.

**Fix — initialize on panel show:**
In `ZtoryAnimaticRuler::showEvent()` (or in
`ZtoryAnimaticPanel::updateAnimaticFrameRange()`), after computing
`totalFrames` from the main xsheet:

```cpp
void ZtoryAnimaticRuler::initPlayRangeIfNeeded() {
    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    if (!xsh) return;
    int r0, r1, step;
    xsh->getPlayRange(r0, r1, step);
    // -1 means "never set" — initialize to full range
    if (r0 < 0) {
        int lastFrame = std::max(0, xsh->getFrameCount() - 1);
        xsh->setPlayRange(0, lastFrame, 1);
        // Sync FlipConsole immediately:
        ZtoryAnimaticController::instance()->flipConsole()
            ->setMarkers(0, lastFrame);
    }
}
```

Call `initPlayRangeIfNeeded()` from:
- `ZtoryAnimaticRuler::showEvent()`
- `ZtoryAnimaticPanel::updateAnimaticFrameRange()` — already fires on
  `sceneSwitched` and `sceneChanged`

Also call it whenever a new shot is added or removed (the total frame count
changes, so the Out marker should extend to the new last frame if it was
already at the previous last frame — i.e. "sticky to end" behaviour).

**Paint guard fix:** Change the condition so markers are ALWAYS drawn if the
range is valid (r0 >= 0 and r1 >= r0, allowing r0 == r1 == 0 as a valid
single-frame range):

```cpp
// BEFORE (wrong — hides markers when r0==0, r1==0):
if (r0 >= 0 && r1 > r0) { ... }

// AFTER (correct):
if (r0 >= 0 && r1 >= r0) { ... }
```

---

#### Bug 2 — Right-click must NOT move the playhead

**Root cause:** `ZtoryAnimaticRuler::mousePressEvent` calls
`setCurrentFrame(frame)` regardless of which mouse button was pressed.

**Fix:** Guard the playhead move on left button only:

```cpp
void ZtoryAnimaticRuler::mousePressEvent(QMouseEvent *e) {
    int frame = frameAtX(e->x());

    if (e->button() == Qt::LeftButton) {
        // Handle Shift/Alt modifiers for In/Out drag (see below)
        // Move playhead only for plain left-click:
        if (!(e->modifiers() & (Qt::ShiftModifier | Qt::AltModifier)))
            ZtoryAnimaticController::instance()->setCurrentFrame(frame);
        // ... marker drag detection ...
    }
    // Right-click: do NOT move playhead — context menu will handle it
    // (Qt calls contextMenuEvent automatically for right-click)
}
```

---

#### Bug 3 — "Mark IN / Mark OUT" right-click context menu

**Add `contextMenuEvent` to `ZtoryAnimaticRuler`:**

```cpp
void ZtoryAnimaticRuler::contextMenuEvent(QContextMenuEvent *e) {
    // Frame under cursor — this is the mark position, NOT the playhead
    int frame = frameAtX(e->x());

    QMenu menu(this);

    QAction *actIn  = menu.addAction(tr("Mark IN"));
    QAction *actOut = menu.addAction(tr("Mark OUT"));
    menu.addSeparator();
    QAction *actReset = menu.addAction(tr("Reset IN/OUT to full range"));

    QAction *chosen = menu.exec(e->globalPos());
    if (!chosen) return;

    TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
    int r0, r1, step;
    xsh->getPlayRange(r0, r1, step);
    if (r0 < 0) { r0 = 0; r1 = xsh->getFrameCount() - 1; step = 1; }

    if (chosen == actIn) {
        r0 = frame;
        r1 = std::max(r1, r0);   // Out must be >= In
    } else if (chosen == actOut) {
        r1 = frame;
        r0 = std::min(r0, r1);   // In must be <= Out
    } else if (chosen == actReset) {
        r0 = 0;
        r1 = std::max(0, xsh->getFrameCount() - 1);
    }

    xsh->setPlayRange(r0, r1, step);
    ZtoryAnimaticController::instance()->flipConsole()->setMarkers(r0, r1);
    update();

    // Do NOT call setCurrentFrame() — playhead must not move on right-click
}
```

**Note on naming — "Mark IN/OUT" vs sub-scene play range:**
The native timeline's Shift+click / Alt+click shortcuts set the **xsheet
play range** (same data). The right-click labels "Mark IN" / "Mark OUT"
are an NLE-style alias for the same operation, identical to Premiere/Resolve.
No conflict with sub-scene markers: sub-scene markers are only visible inside
a sub-scene; the animatic ruler always operates on the main xsheet range.

---

#### Existing shortcuts — keep but fix playhead behaviour

- `Shift+Left-click` → set In at clicked frame (no playhead move)
- `Alt+Left-click` → set Out at clicked frame (no playhead move)
- Drag In/Out triangle → move marker (no playhead move during drag)

In `mousePressEvent`, when `ShiftModifier` or `AltModifier` is detected,
**skip** the `setCurrentFrame()` call entirely. Only update the marker:

```cpp
if (e->button() == Qt::LeftButton) {
    int frame = frameAtX(e->x());
    if (e->modifiers() & Qt::ShiftModifier) {
        setInFrame(frame);   // helper: clamp, setPlayRange, setMarkers, update
        return;
    }
    if (e->modifiers() & Qt::AltModifier) {
        setOutFrame(frame);
        return;
    }
    // Plain left-click → move playhead + check for marker drag
    ...
}
```

**Reading/writing the play range always via main xsheet:**
```cpp
TXsheet *xsh = ZtoryAnimaticController::instance()->mainXsheet();
int r0, r1, step;
xsh->getPlayRange(r0, r1, step);
// ... modify r0 or r1 ...
xsh->setPlayRange(r0, r1, step);
ZtoryAnimaticController::instance()->flipConsole()->setMarkers(r0, r1);
update();
```

**Files:** `ztoryanimatic.h/cpp` (`ZtoryAnimaticRuler`).
**Reference:** `xsheetviewer.cpp` `RowArea::mousePressEvent`,
`RowArea::contextMenuEvent`, `RowArea::paintEvent`.

---

### 13c. Playhead visual style — match native triangle

The current `ZtoryAnimaticRuler` likely draws a simple line or small cursor.
Replace with the **native downward filled triangle** style:

**Where the native playhead is:**
In `RowArea::paintEvent`, search for `currentFrame` and a triangle draw
call: `QPolygon`, `drawPolygon`, or `drawConvexPolygon`.

**Typical native pattern:**
```cpp
int cx = kLabelW + (int)(currentFrame * m_ppf);
QPolygon tri;
tri << QPoint(cx-5, 0) << QPoint(cx+5, 0) << QPoint(cx, 8);
painter.setBrush(Qt::white);   // or orange depending on play state
painter.drawConvexPolygon(tri);
// Draw vertical line below triangle:
painter.drawLine(cx, 8, cx, height());
```
Copy and adapt the exact shape, color and size from `RowArea::paintEvent`
to ensure visual consistency.

---

### 13d. Navigation tags ⚠️ brainstorming needed

**Where the native tags are:**
Search `xsheetviewer.cpp` or `xshrowviewer.cpp` for `NavigationTag`,
`navTag`, or the context menu on the ruler that says "Add marker" /
"Rename marker". Tags are stored in `TXsheet` via methods like
`getNavigationTags()` / `addNavigationTag(frame, name)`.

**For now — implement only visual + rename/drag, no Ztoryc sequence logic:**
- Copy tag rendering from `RowArea::paintEvent` (small colored flag above
  ruler, text label).
- Copy right-click context menu: "Add tag", "Rename tag", "Delete tag".
- Copy drag interaction: drag a tag to a new frame.
- Store tags on the main xsheet using the same `TXsheet` API as native.
- Do NOT implement sequence grouping logic yet — that requires a separate
  design session (see pending item in AGENTS.md).

**Files:** `ztoryanimatic.h/cpp`.
**Key files to read for all sub-tasks:** `xsheetviewer.cpp` (`RowArea` class,
`paintEvent`, `mousePressEvent`, `mouseMoveEvent`, `contextMenuEvent`).

---

## 14. NEW — Startup Window (New Project Dialog) ⭐ PRIORITY

**What:** A modal dialog that appears when the user creates a new project (or
when Ztoryc is launched with no scene). Forces the user to name and configure
the project *before* any content is created, avoiding the "untitled" path
problem where levels end up with broken relative paths.

**Why this is needed:**
- Currently the app opens on an "Untitled" scene. Any levels drawn before
  saving get `+drawings/Untitled/level.tlv` paths that break when the scene
  is later saved under a real name.
- A startup dialog forces the save-path decision upfront, identical to how
  ToonBoom Storyboard Pro works.
- The existing "New Scene forced save" fix in `iocommand.cpp` already adds a
  `SaveSceneAsPopup` at the end of `newScene()`, but this is the wrong moment
  (after the empty scene is already open). The startup dialog replaces this with
  a richer pre-configured dialog shown *before* the scene is opened.

---

### Dialog sections

#### Section 1 — Project
- **Project name** (text field): becomes the `.tnz` file name and the
  `+drawings/projectname/` folder. Validated: no spaces, no special chars
  (same rules as Tahoma2D scene names).
- **Project location** (path picker): default `$HOME/Ztoryc Projects/`.
- **Template** (dropdown, optional): list of `.tnz` templates from
  `stuff/templates/ztoryc/`. Default: "Blank storyboard".

#### Section 2 — Camera / Resolution
- **Format preset** (dropdown): same list as Tahoma2D's output settings
  (`TOutputSettings` presets). Default: "HDTV 1080p (1920×1080)".
- **Width / Height** (spin boxes, px): auto-filled from preset, editable.
- **Frame rate** (spin box, fps): default 24. Common presets: 12, 24, 25, 30.
- **Duration** (spin box, frames or hh:mm:ss): total project duration (optional;
  can be left at 0 = "undefined, grow as shots are added").

#### Section 3 — Workflow
- **Mode** (radio buttons or icon selector):
  - `STORYBOARD` — Opens in BOARD room. Creates empty shots.
  - `ANIMATIC` — Opens in ANIMATIC room. For timing-first workflows.
  - `STOPMOTION` — Opens in CAPTURE room (Stop Motion workflow).
- Default: `STORYBOARD`.

#### Section 4 — Shot numbering
- **Style** (dropdown):
  - `Simple` — e.g. `sh010`, `sh020`, `sh030`
  - `Sequence` — e.g. `sq01_sh010`, `sq01_sh020`, `sq02_sh010`
- **Step** (spin box): default `10`. Sets the increment between shot numbers.
  Use 10 so there is room to insert shots between existing ones.
  Common choices: 1, 5, 10.
- **Sequence prefix** (text field, visible only when Style=Sequence):
  default `sq`. Example: `sq` → `sq01`, `sq02`...
- **Shot prefix** (text field): default `sh`. Example: `sh` → `sh010`, `sh020`.
- **Padding** (spin box): number of digits in the shot number. Default `3`
  → `010`, `020`. Sequence padding also `2` → `01`, `02`.
- **Start number** (spin box): default `10` (first shot = step × 1).
- **Initial shot count** (spin box): how many empty shots to pre-create.
  Default `1`. Set to 0 = no shots, just an empty scene.

**Examples with Style=Sequence, step=10, padding=3, sq-padding=2:**
```
Sequence 1: sq01_sh010, sq01_sh020, sq01_sh030 ...
Sequence 2: sq02_sh010, sq02_sh020 ...
```
**Example with Style=Simple, step=10, padding=3:**
```
sh010, sh020, sh030 ...
```

Shot names are stored in `ShotData::shotName` (already in `ZtoryModel`).

---

### Implementation

**New files:**
```
toonz/sources/toonz/ztorystartup.h
toonz/sources/toonz/ztorystartup.cpp
```

**New class `ZtoryStartupDialog : public QDialog`:**

```cpp
class ZtoryStartupDialog : public QDialog {
    Q_OBJECT

    // Section 1 — Project
    QLineEdit *m_projectName;
    FileField  *m_projectPath;  // reuse Tahoma2D's FileField widget
    QComboBox  *m_templateCombo;

    // Section 2 — Camera
    QComboBox  *m_formatPreset;
    QSpinBox   *m_width, *m_height;
    QSpinBox   *m_fps;
    QSpinBox   *m_totalFrames;

    // Section 3 — Workflow
    QButtonGroup *m_workflowGroup;  // STORYBOARD / ANIMATIC / STOPMOTION

    // Section 4 — Numbering
    QComboBox  *m_numberingStyle;   // Simple / Sequence
    QSpinBox   *m_numberingStep;
    QLineEdit  *m_seqPrefix;        // hidden when Style=Simple
    QLineEdit  *m_shotPrefix;
    QSpinBox   *m_numPadding;
    QSpinBox   *m_startNumber;
    QSpinBox   *m_initialShotCount;

public:
    explicit ZtoryStartupDialog(QWidget *parent = nullptr);

    // Returns configured data after exec() == Accepted
    struct Config {
        QString projectName;
        QString projectPath;
        QString templatePath;

        int     width, height, fps, totalFrames;

        enum Workflow { Storyboard, Animatic, StopMotion } workflow;

        enum NumberingStyle { Simple, Sequence } numberingStyle;
        int     step, padding, seqPadding, startNumber, initialShotCount;
        QString seqPrefix, shotPrefix;

        // Generates shot name for sequence sq, shot index idx (0-based)
        QString shotName(int sq, int idx) const;
    };
    Config config() const;
};
```

**Integration in `iocommand.cpp` — `newScene()` function:**

Replace the current `SaveSceneAsPopup` call at the end of `newScene()` with:

```cpp
// 1. Show startup dialog
ZtoryStartupDialog dlg(QApplication::activeWindow());
if (dlg.exec() != QDialog::Accepted) return;
ZtoryStartupDialog::Config cfg = dlg.config();

// 2. Set scene path (forces a named save upfront)
TFilePath scenePath(cfg.projectPath + "/" + cfg.projectName + "/" +
                    cfg.projectName + ".tnz");
TSystem::mkDir(scenePath.getParentDir());
IoCmd::saveSceneIfNeeded(tr("New Scene"));
scene->setScenePath(scenePath);

// 3. Apply camera settings
TCamera *cam = scene->getCurrentCamera();
cam->setRes(TDimension(cfg.width, cfg.height));
// (fps is set on xsheet level output settings)

// 4. Switch to chosen workflow room
MainWindow::instance()->switchToRoom(
    cfg.workflow == ZtoryStartupDialog::Config::Storyboard ? "BOARD" :
    cfg.workflow == ZtoryStartupDialog::Config::Animatic   ? "ANIMATIC" :
                                                             "CAPTURE");

// 5. Create initial shots with correct names
ZtoryModel *model = ZtoryModel::instance();
for (int i = 0; i < cfg.initialShotCount; i++) {
    QString name = cfg.shotName(1, i);  // sequence 1 for initial shots
    model->addShot(name);  // new method: insert column + sub-scene
}

// 6. Save scene to disk immediately
IoCmd::saveScene(scene->getScenePath(), IoCmd::SAVE_SCENE);
```

**New `ZtoryModel::addShot(const QString &name)` method:**
- Inserts one new column at the end of the main xsheet.
- Creates a new empty sub-scene (`cloneChildToPosition` with an empty level).
- Sets `ShotData::shotName = name`.
- Calls `resequenceXsheet()`.
- Emits `modelReset()`.

**Preferences persistence (`ZtoryModel` or `QSettings`):**
- Remember last-used values per-user in `QSettings("Ztoryc", "StartupDefaults")`.
- Restore them as defaults on the next dialog open.

---

### Shot name generation logic

```cpp
QString ZtoryStartupDialog::Config::shotName(int seq, int idx) const {
    int number = startNumber + idx * step;
    if (numberingStyle == Sequence) {
        return QString("%1%2_%3%4")
            .arg(seqPrefix)
            .arg(seq, seqPadding, 10, QChar('0'))
            .arg(shotPrefix)
            .arg(number, padding, 10, QChar('0'));
        // e.g. "sq01_sh010"
    } else {
        return QString("%1%2")
            .arg(shotPrefix)
            .arg(number, padding, 10, QChar('0'));
        // e.g. "sh010"
    }
}
```

**Files:** `ztorystartup.h` (new), `ztorystartup.cpp` (new),
`iocommand.cpp` (hook into `newScene()`),
`ztorymodel.h/cpp` (add `addShot(QString)`),
`CMakeLists.txt` (add new files).

**Pitfall:** Do not call `SaveSceneAsPopup` anymore after adding this dialog —
remove or guard the existing call in `iocommand.cpp::newScene()`.

**Pitfall:** `TSystem::mkDir` creates parent dir only. Use a loop or
`QDir().mkpath()` to create nested paths.

---

## Priority Order (suggested)

1. **Task 1b** — ~~Guard: all shot ops must check ancestorCount == 0~~ ✅ DONE
2. **Task 14** — ~~Startup window / new-project dialog~~ ✅ DONE (2026-03-21)
3. **Task 4** — ~~Fix duplicate buttons~~ ✅ DONE (no duplicates found)
4. **Task 1** — ~~Fix animatic viewer visibility + TFrameHandle~~ ✅ DONE (ZtoryAnimaticController)
4. **Task 12a** — ~~Audio waveform~~ ✅ DONE (confirmed visible)
5. **Task 12b** — ~~Audio scrubbing on ruler drag~~ ✅ DONE (2026-03-21)
6. **Task 12c** — Sound preview bar in ZtoryAudioTrack (click+drag range → play on release)
7. **Task 13c** — ~~Playhead triangle style~~ ✅ DONE (2026-03-21)
7. **Task 13b** — ~~In/Out markers on animatic ruler~~ ✅ DONE (2026-03-21)
8. **Task 13a** — ~~Onion skin markers on ruler~~ ✅ DONE (2026-03-21)
9. **Task 3** — ~~Fix Copy vs Clone in Board~~ ✅ DONE
10. **Task 2** — ~~Unify resequenceXsheet into ZtoryModel~~ ✅ DONE
11. **Task 7** — Double-click to enter edit mode (quick win)
12. **Task 8** — Multi-selection in track (prerequisite for merge/razor)
13. **Task 6a** — Zoom slider (quick)
14. **Task 6c** — Razor tool
15. **Task 6d** — Link/Unlink audio-video
16. **Task 6f** — Merge shots
17. **Task 5** — Story-strip
18. **Task 9** — Audio export with shot
19. **Task 10** — X-Sheet panel guard for audio import
20. **Task 11** — Viewer toggle
21. **Task 13d** — Navigation tags (⚠️ design session needed first)

---

## Reference: Current File Structure

```
ztoryanimatic.h     — classes: Ruler, Track, AudioTrack, Viewer, Panel
ztoryanimatic.cpp
storyboardpanel.h   — classes: PanelWidget, StoryboardPanel
storyboardpanel.cpp
ztorymodel.h        — singleton data model
ztorymodel.cpp
ztorystartup.h      — ZtoryStartupDialog (NEW 2026-03-21)
ztorystartup.cpp    — (NEW 2026-03-21)
ztorybackpanel.h
AGENTS.md           — Claude Code rules
DESIGN.md           — functional spec
```
