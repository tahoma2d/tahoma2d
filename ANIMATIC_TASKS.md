# Ztoryc — Animatic Panel: Task List for Claude Code

> Aggiornato 2026-03-28. Task completati ridotti a una riga.
> Per le spec storiche dei task DONE vedere git history.

---

## LEGEND

- **BUG** = existing code that is broken
- **NEW** = feature that does not exist yet
- **MOD** = existing code that needs modification

---

## DONE — Task completati (riepilogo)

| Task | Descrizione | Data |
|------|-------------|------|
| 1 | Animatic Viewer visibile + ZtoryAnimaticController | 2026-03-19 |
| 1b | assertMainXsheet guard su tutte le operazioni shot | 2026-03-21 |
| 2 | resequenceXsheet unificato in ZtoryModel | 2026-03-21 |
| 3 | Fix Copy vs Clone nel Board (onPasteShot) | 2026-03-21 |
| 4 | Rimosse dichiarazioni bottoni duplicate nel costruttore | 2026-03-21 |
| 5 | Story-strip thumbnail orizzontale | 2026-03-25 |
| 6a | Zoom slider toolbar animatic | 2026-03-24 |
| 6c | Razor tool (SIGSEGV fix, trim child xsheet, audio linked) | 2026-03-26c |
| 6d | Link/Unlink audio-video (shiftLevelFromFrame) | 2026-03-26c |
| 6f | Merge shots con camera keyframe | 2026-03-26b |
| 7 | Double-click entra in edit mode | 2026-03-25 |
| 8 | Multi-selection in track | 2026-03-25 |
| 12a | Audio waveform visibile | 2026-03-19 |
| 12b | Audio scrubbing durante drag ruler | 2026-03-21 |
| 12c | Sound preview bar in ZtoryAudioTrack | 2026-03-25 |
| 13a | Onion skin markers sul ruler (FOS + MOS strip) | 2026-03-21 |
| 13b | In/Out markers sempre visibili + context menu NLE | 2026-03-21 |
| 13c | Playhead triangle style nativo | 2026-03-21 |
| 14 | Startup dialog (ZtoryStartupDialog, 4 sezioni) | 2026-03-21 |
| 15 | RecentFiles.ini cap a 50 entry | 2026-03-26b |

---

## Task aperti

---

### BUG — Board non si aggiorna (timeline mostra 1 shot, Board ne mostra 4)

**Sintomo:** dopo operazioni (merge, razor, o altro), il panel Board mostra
ancora i vecchi shot mentre la timeline animatic ne mostra uno solo (o un
numero diverso). I due panel sono desynced.

**Causa probabile:** il flag `m_refreshing` introdotto in 2026-03-25 come
guard di rientranza può rimanere `true` se `refreshFromScene()` esce
prematuramente (eccezione, early return, o crash parziale). Tutti i refresh
successivi vengono bloccati silenziosamente.

**Cosa fare:**
1. Verificare che `m_refreshing` venga sempre resettato a `false` anche nei
   path di uscita anticipata (aggiungere RAII guard o `QScopeGuard`).
2. Verificare che `ZtoryModel::modelReset()` venga emesso dopo merge/razor.
3. Verificare che `StoryboardPanel` sia connesso a `ZtoryModel::modelReset`.

**File:** `ztoryanimatic.cpp` (`refreshFromScene`), `storyboardpanel.cpp`,
`ztorymodel.cpp`.

---

### Task 16 — 🔴 Audio sync play (PRIORITARIO)

**Sintomo:** desync A/V residuo su animatici lunghi — "molto meglio" ma non
perfetto. Causa: video timer (`PlaybackExecutor`) e `QAudioOutput` usano
clock indipendenti → drift nel tempo.

**Soluzione preferita (master clock audio):**
Esporre `QAudioOutput::processedUSecs()` da `TSoundOutputDevice`, poi in
`ZtoryAnimaticViewer::onDrawFrame` usarlo come master:
```cpp
qint64 audioUs = ctrl->mainXsheet()->getAudioPlayedUSecs();
int audioFrame = m_playStartFrame + (int)(audioUs * fps / 1e6);
if (audioFrame != currentFrame) ctrl->setAnimaticFrame(audioFrame);
```
**File da toccare:** `tsound.h`, `tsound_qt.cpp`, `txsheet.h/.cpp`,
`ztoryanimatic.h/.cpp`.

**Alternativa più semplice** (solo ztoryanimatic, nessuna modifica core):
`QElapsedTimer` avviato al momento di `mainXsh->play()` come proxy del
clock audio. Meno preciso ma non tocca file Tahoma2D core.

---

### Task 9 — Audio export con lo shot

**Cosa:** all'export di uno shot come `.tnz` autonomo, inserire
automaticamente la porzione di audio corrispondente nella sub-scene.

**Implementazione:**
In `StoryboardPanel::onExportShots()`, prima di `IoCmd::saveScene(...)`:
1. Trovare le colonne audio nel main xsheet.
2. Estrarre le celle nel range `[shotStart, shotStart + duration - 1]`.
3. Inserirle nella sub-scene a frame 0 come nuova sound column.
4. Nome colonna: prefisso `_audio_main_` per indicarla come riservata.
5. Dopo il save, rimuovere la colonna temporanea dalla sub-scene.

**File:** `storyboardpanel.cpp` (`onExportShots`).
**Pitfall:** usare `TXshSoundColumn` API specifiche, non `clearCells`/`setCell`.

---

### Task 10 — X-Sheet guard: bloccare import audio nelle sub-scene

**Cosa:** quando il workflow è Storyboard, impedire l'import di file audio
nelle sub-scene (l'audio vive solo nel main xsheet).

**Implementazione:**
In `IoCmd::loadResources()` (o nel comando di import audio), aggiungere:
```cpp
if (TApp::instance()->getCurrentScene()->getScene()
        ->getChildStack()->getAncestorCount() > 0
    && ZtoryModel::isStoryboardWorkflow()) {
    // mostra warning e return
}
```
**Nota:** `isStoryboardWorkflow()` attuale controlla il file `.ztoryc` —
verificare che non dia falsi positivi quando si cambia workflow.

**File:** `iocommand.cpp`, `ztorymodel.h/.cpp`.

---

### Task 11 — Animatic Viewer toggle con ComboViewer

**Cosa:** toggle button per condividere lo stesso spazio tra viewer animatic
e viewer normale (risparmio schermo).

**Implementazione:**
- `QStackedWidget` con index 0 = `m_animViewer` (main xsheet), index 1 =
  normale `SceneViewerPanel` (sub-scene).
- Bottone "Animatic / Shot" in toolbar commuta tra i due.

**Alternativa:** collapse button sul panel animatic viewer (più semplice).

**File:** `ztoryanimatic.h/.cpp`.

---

### Task 13d — Navigation tags sul ruler

**Cosa:** tag colorati sul ruler (come nella timeline nativa) con label
testuale. Solo visual + rename/drag per ora — nessuna logica sequence.

**Implementazione:**
- Copiare rendering da `RowArea::paintEvent` in `xsheetviewer.cpp`
  (cerca `NavigationTag`, `navTag`).
- Context menu: "Add tag", "Rename tag", "Delete tag".
- Drag tag su nuovo frame.
- Salvare sul main xsheet via `TXsheet::addNavigationTag(frame, name)`.
- **NON** implementare sequence grouping — richiede design session separata.

**File:** `ztoryanimatic.h/.cpp`.
**Riferimento:** `xsheetviewer.cpp` `RowArea`.

⚠️ **Design session necessaria prima** per definire come i tag si relazionano
alle sequence del progetto (sq01, sq02…).

---

## Priority Order

0. **BUG Board desync** — 🔴 Board non si aggiorna (vedi sopra)
1. **Task 16** — 🔴 Audio sync play (master clock audio)
2. **Task 9** — Audio export con shot
3. **Task 10** — X-Sheet guard audio import
4. **Task 11** — Viewer toggle
5. **Task 13d** — Navigation tags (⚠️ design session prima)

---

## Reference: Current File Structure

```
toonz/sources/toonz/storyboardpanel.h/.cpp   — Board room, shot grid UI
toonz/sources/toonz/ztorymodel.h/.cpp        — Singleton data model
toonz/sources/toonz/ztoryanimatic.h/.cpp     — Animatic panel + viewer
toonz/sources/toonz/ztorystartup.h/.cpp      — Startup dialog (NEW 2026-03-21)
toonz/sources/toonz/ztorybackpanel.h/.cpp    — Back to storyboard button
toonz/sources/stopmotion/webcam.h/.cpp       — Webcam + AVCapture
toonz/sources/toonzqt/txshsoundcolumn.h/.cpp — Audio column (shiftLevelFromFrame)
toonz/sources/image/tzl/tiio_tzl.cpp         — TLV save (assert fixes)
toonz/sources/toonz/mainwindow.cpp           — Workflow switch + RecentFiles cap
```
