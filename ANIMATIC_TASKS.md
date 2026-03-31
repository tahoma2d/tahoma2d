# Ztoryc — Animatic Panel: Task List for Claude Code

> Aggiornato 2026-03-29. Task completati ridotti a una riga.
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
| RAZOR-CRASH | SIGABRT razor + AV link (checkColumn assert su ColumnLevel overlap) | 2026-03-29 |
| AUTOFILL | AutoFill flood-fill BFS (savebox-bounded, stile N+1, crash fix) | 2026-03-31 |
| RAZOR-GAP | Audio gap visivo rimosso; splitAudioColumn reimplementata con splitLevelAtFrame | 2026-03-31 |
| RAZOR-HOVER | Linea preview hover gialla tratteggiata cross-track | 2026-03-31 |
| BUILD-DEPLOY | libtnztools aggiunto, libimage/colorfx/stdfx rimossi, libtnzbase aggiunto, RelWithDebInfo | 2026-03-31 |
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

---

~~### BUG — Razor audio gap troppo largo~~ ✅ DONE 2026-03-31

---

### BUG — Camera resize scala anche il disegno

**Sintomo:** ridimensionando la camera dalla sotto-scena (o dall'animatic), il
disegno contenuto viene scalato insieme alla camera invece di rimanere invariato.

**Causa da investigare:** potrebbe essere `TXsheetHandle::notifyCameraChanged`
o `TSceneHandle::notifyCameraChanged` che innesca un resize dei livelli.
Oppure `TCamera::setRes` sta modificando il transform dei livelli.

**Note:** la camera dovrebbe cambiare solo il viewport, non i contenuti.
Verificare se il problema è nella visualizzazione o nei dati salvati.

**File da investigare:** `camerasettingspopup.cpp`, `txsheet.cpp`, `tscenehandle`.

---

### BUG — Ghost onion skin nel viewer animatic

**Sintomo:** il viewer animatic mostra un onion skin non richiesto (nessun
marker FOS/MOS attivato dall'utente), con frame ghost visibili.

**Causa probabile:** `ZtoryAnimaticViewer` eredita da `BaseViewerPanel` e usa
il frame handle globale. Quando si naviga nell'animatic, l'onion skin del viewer
normale potrebbe "filtrare" sul viewer animatic perché condividono lo stesso
`TOnionSkinMask` globale.

**Soluzione:** disabilitare l'onion skin nel viewer animatic esplicitamente:
```cpp
// In ZtoryAnimaticViewer::initialize() o paintEvent
m_sceneViewer->setOnionSkinEnabled(false);
```
Oppure creare un `TOnionSkinMaskHandle` separato per il viewer animatic con
mask sempre vuota.

**File:** `ztoryanimatic.h/.cpp`.

---

### PERF — Rallentamento con file audio pesanti

**Sintomo:** l'UI diventa lenta quando vengono caricati file audio di grandi
dimensioni. Probabilmente dovuto al rendering del waveform su ogni resize/scroll.

**Soluzione:** introdurre caching del waveform pre-renderizzato:
- Calcolare il waveform in un `QImage` cache solo quando cambia la colonna audio
  o le dimensioni del widget.
- In `paintEvent` disegnare solo la `QImage` cache se le dimensioni non sono
  cambiate.
- Invalidare cache su: `audioChanged`, resize, zoom change.

**File:** `ztoryanimatic.cpp` (`ZtoryAudioTrack::paintEvent`, `refreshAudioTracks`).

---

### MOD — Board ↔ Animatic sincronizzazione bidirezionale completa

**Obiettivo:** tutte le operazioni (razor, merge, reorder) devono riflettersi
immediatamente sia nel Board che nell'Animatic, in entrambe le direzioni.

**Regole di sincronizzazione:**
- Durata totale shot: impostabile dal Board (primo panel) O dall'Animatic
  (ripple / match subscene duration). Ogni cambio aggiorna l'altro.
- Durate parziali dei panel: lette automaticamente dalla sotto-scena dello shot.
- Ordine shot: reorder nell'Animatic aggiorna l'ordine nel Board e viceversa.
- Aggiunte/rimozioni: creare/cancellare shot da un panel aggiorna l'altro.

**Problema attuale:** il flag `m_refreshing` può bloccare i refresh successivi
se `refreshFromScene()` esce prematuramente. Vedi anche "BUG Board desync".

**Implementazione:**
1. Assicurare che ogni operazione emetta `ZtoryModel::modelReset()` o signal
   specifici (`shotAdded`, `shotRemoved`, `shotDurationChanged`, `shotReordered`).
2. Sia `StoryboardPanel` che `ZtoryAnimaticPanel` devono ricevere questi signal
   e aggiornarsi senza loop (guard `m_refreshing` con RAII).
3. Le durate parziali dei panel si leggono da
   `ZtoryModel::shotData(i).panelDurations` che a sua volta legge dal sub-xsheet.

**File:** `ztorymodel.h/.cpp`, `storyboardpanel.cpp`, `ztoryanimatic.cpp`.

---

### NEW — Sistema di numerazione Sequence / Shot / Panel

**Gerarchia:** `SQ###_SH###_P###`
**Esempio:** `SQ010_SH020_P003`

#### Struttura dati
Ogni elemento ha:
- `uuid` — identificatore immutabile (non cambia mai)
- `label` — stringa visibile (es. `SQ010`, `SH020A`, `P003`)
- `order_index` — intero per l'ordinamento (non deve essere unico globalmente,
  solo all'interno del livello padre)

Shot e panel sono collegati alla sequenza via `sequence_uuid`, non via label.

#### Numerazione
- Sequenze e shot: step 10 (010, 020, 030…)
- Panel: step 1 (001, 002, 003…)
- Inserimento tra due elementi: calcolare la media degli `order_index`
  - Se c'è spazio numerico: generare label intermedio (es. SH015)
  - Se non c'è spazio: appendere suffisso alfabetico (SH010A, SH010B…)
- Varianti: suffisso separato da `_` (es. `SH020_A`, `P003_B`)
- Ordinamento: SEMPRE per `order_index`, mai per label

#### Modalità numerazione
- **AUTO:** ogni volta che si aggiunge, inserisce o riordina uno shot, tutto
  viene rinumerato automaticamente con step 10.
- **KEEP:** la numerazione non cambia automaticamente. Le label rimangono
  stabili. Rinumerazione solo su richiesta esplicita dell'utente.

#### Funzioni manuali
- **"Renumber shots"**: rinumera tutti (o solo gli shot selezionati) in modo
  pulito con step 10. Azione confermata dall'utente.
- **"Renumber sequences"**: rinumera tutte le sequenze con step 10 e aggiorna
  le label di shot e panel dipendenti. Richiede conferma + warning che i
  riferimenti esterni (export, filenames) cambieranno.
  - NON cambia `uuid` né `order_index`.

#### Gestione sequenze
- Creare una sequenza: nuovo UUID, `order_index` calcolato, label generata.
- **Raggruppare shot in una nuova sequenza:**
  - Creare nuova sequenza
  - Aggiornare `sequence_uuid` degli shot selezionati
  - Rinumerare gli shot nella nuova sequenza da SH010
  - Gli `uuid` degli shot e la struttura dei panel rimangono invariati
- **Spostare uno shot tra sequenze:**
  - Aggiornare `sequence_uuid`
  - Ricalcolare label basandosi sulla sequenza di destinazione
  - Inserire alla posizione corretta (basata su `order_index`)
- **Dividere una sequenza in due:**
  - Creare una nuova sequenza dopo quella corrente
  - Spostare tutti gli shot successivi al punto di split nella nuova sequenza
  - Rinumerare gli shot spostati da SH010

#### Reordering
- Spostare uno shot aggiorna SOLO il suo `order_index`
- La label NON cambia
- L'UI mostra sempre gli elementi ordinati per `order_index`

#### Salvataggio
- Dati salvati nel file `.ztoryc` XML accanto al `.tnz`
- Aggiungere campi: `<Shot uuid="..." label="..." order_index="..." sequence_uuid="..."/>`
- Retrocompatibilità: se mancano i nuovi campi, generare UUID e label al caricamento

**File:** `ztorymodel.h/.cpp` (struttura dati `ShotData`, `SequenceData`),
`storyboardpanel.cpp` (UI numerazione, bottoni renumber), `ztorymodel.cpp`
(save/load `.ztoryc`).

**Nota:** implementazione in 3 fasi:
1. Struttura dati + save/load `.ztoryc` (solo modello)
2. UI Board: visualizza label, menu contestuale "renumber", modalità AUTO/KEEP
3. Integrazione Animatic: label visibili sulle tracce

---

### BUG — Crash SIGSEGV salvataggio TLV

**Sintomo:** crash durante `Cmd+S` / Save All. Backtrace:
`TLevelWriterTzl::TLevelWriterTzl` → `QString::QString(const&)` → SIGSEGV.
Il crash è in `libimage.dylib` (bundle originale), non nel codice Ztoryc.

**Causa sospetta:** incompatibilità ABI tra `libtnzcore` (nostro RelWithDebInfo)
e `libimage` originale che usa layout diversi per alcune strutture Qt.
Il `libimage` originale è patchato con `@executable_path/libtiff` ma potrebbe
avere anche dipendenze da simboli Qt che il nostro `libtnzcore` espone
diversamente in RelWithDebInfo vs Release.

**Opzioni:**
1. Build `libimage` in Release mode con rpath fixati (complesso, dipendenze system)
2. Aggiungere `libimage` al deploy dopo aver fixato il rpath libtiff
3. Investigare quali strutture Qt cambiano layout tra Debug/RelWithDebInfo

**File:** `build_and_deploy.sh`, `image/tzl/tiio_tzl.cpp`, `libimage`

---

### BUG — AutoFill fill colore: opzione picker dalla palette

**Sintomo:** autofill usa sempre lo stile N+1 (hardcoded). Non c'è modo di
scegliere un colore diverso per il fill dall'UI.

**Cosa fare:** aggiungere `TEnumProperty m_autoFillStyle` (o `TIntProperty`)
al brush tool, visibile nelle tool options accanto al checkbox AutoFill.
Il combo si popola dinamicamente con gli stili della palette del livello corrente.
Valore default: "next style" (N+1). Se la palette cambia, il combo si aggiorna.

**File:** `toonzrasterbrushtool.h/.cpp`, `tooloptions.h/.cpp`

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

### Bug immediati (nessun design necessario)
0. **BUG Crash salvataggio TLV** — 🔴 SIGSEGV in libimage durante Save (ABI issue)
1. **BUG Board desync** — 🔴 Board non si aggiorna + sincronizzazione bidirezionale completa
2. ~~**BUG Razor audio gap**~~ ✅ DONE 2026-03-31
3. **BUG Ghost onion skin** — viewer animatic mostra onion skin non richiesto
4. **BUG Camera resize** — ridimensionare camera scala anche il disegno
5. **PERF Audio pesante** — cache waveform pre-renderizzata
6. **BUG AutoFill fill color** — picker colore per autofill nella palette del livello

### Feature in roadmap
5. ~~**Task 16**~~ ✅ DONE (2026-03-28) — Audio-master clock (processedUSecs DAC)
6. ~~**Task B/A**~~ ✅ DONE (2026-03-28) — Audio main xsheet da sotto-scena (play + scrub)
7. ~~**Task C**~~ ✅ DONE (2026-03-28) — Auto-aggiornamento marker Out sub-xsheet
8. ~~**Task 15**~~ ✅ DONE (2026-03-28) — RecentFiles.ini deadlock fix
9. **NEW Numerazione SQ/SH/P** — sistema completo Sequence→Shot→Panel (3 fasi, partire da struttura dati + save/load)
10. **Task 9** — Audio export con shot
11. ~~**Task 10**~~ ✅ DONE — X-Sheet guard audio import
12. **Task 11** — Viewer toggle
13. **Task 13d** — Navigation tags (⚠️ design session prima)

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
