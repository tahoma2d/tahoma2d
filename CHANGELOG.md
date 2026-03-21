# Ztoryc — Changelog

> **Come aggiornare (istruzioni per Claude Code):** dopo ogni sessione aggiungi
> una voce in cima con: data, `### Fixed`, `### Added`, `### Modified`,
> `### Upstream candidates`, `### Notes`. Poi esegui rsync (vedi AGENTS.md).

---
## [2026-03-21] — Task 13a + Task 14: onion skin markers + startup dialog

### Added

- **Task 13a** — Onion skin markers on `ZtoryAnimaticRuler`:
  - Semi-transparent downward triangles at `currentFrame + rel` for each ROS offset.
  - Red (255,100,100,120) for frames before current; blue (100,100,255,120) for after.
  - Includes: `onionskinmask.h`, `tonionskinmaskhandle.h`.
  - Connected `onionSkinMaskChanged` → `m_ruler->update()` in `ZtoryAnimaticPanel`.

- **Task 14** — Startup dialog (`ZtoryStartupDialog`) — new files `ztorystartup.h/cpp`:
  - 4 sections: Project (name + location), Camera (preset, W×H, fps, duration),
    Workflow (STORYBOARD / ANIMATIC / STOPMOTION radio), Shot Numbering (Simple / Sequence).
  - Preferences persisted via `QSettings("Ztoryc", "StartupDefaults")`.
  - `ZtoryStartupDialog::Config::shotName(sq, idx)` generates e.g. `sh010` or `sq01_sh010`.
  - Integrated in `iocommand.cpp::newScene()`: replaces `SaveSceneAsPopup`.
    - Applies camera/fps from dialog.
    - Creates project directory via `QDir::mkpath`.
    - Sets scene path before any content is drawn.
    - Switches to selected room via `mw->switchToRoom()`.
    - Pre-creates initial shots with correct names via `ZtoryModel::addShotNamed()`.
    - Saves scene immediately to disk.
  - New `ZtoryModel::addShotNamed(const QString &name)`: creates xsheet column +
    sub-scene + model entry in one call; called by startup dialog.
  - New files registered in `CMakeLists.txt`.

### Notes

- Tasks 4, 3, 2, 13c were already implemented in previous sessions — no changes needed.
- Build: `ninja -j4` clean, no errors.

---
## [2026-03-21] — Design session (Cowork): marker In/Out, scrubbing, preview bar, guard sub-scene

### Added (planning — non ancora implementato in codice)

- **Task 1b** — Guard `ZtoryModel::assertMainXsheet()`: tutte le operazioni
  sugli shot devono verificare `ancestorCount == 0` prima di toccare il main
  xsheet. Silent return per drag, warning dialog per azioni esplicite.
  Aggiunto come priorità assoluta #1.

- **Task 12c** — Barra di preview audio in `ZtoryAudioTrack`: strip da 6px
  in fondo alla traccia, click+drag seleziona range arancione, release suona
  il range. Codice completo nel task.

- **Task 13b aggiornato** — Tre bug specifici sui marker In/Out:
  1. Marker invisibili all'apertura: `initPlayRangeIfNeeded()` inizializza
     r0=0, r1=lastFrame quando `getPlayRange()` ritorna r0<0. Fix anche alla
     paint condition (`r1 >= r0` invece di `r1 > r0`).
  2. Il playhead non si deve spostare con tasto destro né con Shift/Alt+click.
  3. `contextMenuEvent` con "Mark IN", "Mark OUT", "Reset to full range" —
     NLE-style, frame dalla posizione cursore, nessun `setCurrentFrame()`.

- **Task 12b diagnostica** — 4 cause ordinate per il bug scrubbing non
  funzionante. Soluzione preferita: esporre `soundTrack()` da
  `ZtoryAnimaticController` invece di chiamare `makeSound()` nel ruler.

### Notes

- Task 12a (waveform) confermato visibile dall'utente ✅
- Task 1 (ZtoryAnimaticController) confermato completato ✅
- Prossima sessione Code: Task 1b → Task 13b fix → Task 12b → Task 12c

---
## [2026-03-19] — Design: startup window + shot numbering system (Cowork session)

### Added (planning / design only — not yet implemented in code)

- **Task 14 in `ANIMATIC_TASKS.md`** — Startup window / New Project dialog:
  full spec with 4 sections (Project, Camera, Workflow, Shot numbering).
- **Shot numbering system spec**: two styles — Simple (`sh010`) and Sequence
  (`sq01_sh010`). Configurable step (default 10), padding, prefix, start number.
  Rationale: step-10 leaves room to insert shots between existing ones;
  sequence prefix groups shots by act/sequence for ep/feature productions.
- **New class spec `ZtoryStartupDialog`** in `ztorystartup.h/cpp` with inner
  `Config` struct and `shotName(seq, idx)` generator.
- **Integration plan** for `iocommand.cpp::newScene()`: replace current
  `SaveSceneAsPopup` with the new dialog, apply camera settings, switch room,
  pre-create initial shots with correct names, save scene immediately.
- **New `ZtoryModel::addShot(QString)` method spec** for programmatic shot
  creation from the startup dialog.
- Task 14 added as **top priority** in the priority order.

### Notes

- This is a planning session (Cowork). Implementation by Claude Code.
- The "new scene forced save" fix in `iocommand.cpp` must be replaced/adapted
  when Task 14 is implemented — do not regress the forced-save behavior.
- Sequence tags (Task 13c) and startup sequences are related: navigation tags
  on the ruler will eventually map to sequences defined at project creation.

---
## [2026-03-19] — FlipConsole play fix + audio animatic + BUG-05 allineamento tracce

### Fixed

- **Play FlipConsole animatic usa sub-scene invece del main xsheet**
  - `ZtoryAnimaticViewer::onDrawFrame()` — override del metodo virtuale: scrive il frame
    su `ctrl->frameHandle()` invece di `TApp::getCurrentFrame()` (globale).
    Il drag del ruler funzionava già; il play button no perché passava per `onDrawFrame`.
  - `ZtoryAnimaticViewer::updateAnimaticFrameRange()` — nuovo slot: legge il frame count
    da `ctrl->mainXsheet()->getFrameCount()` invece di `TApp::getCurrentFrame()->getMaxFrameIndex()`
    (che restituiva la durata della sub-scene aperta). Collegato a `sceneSwitched`,
    `sceneChanged` e `ctrl->frameHandle()::frameSwitched`.
  - `ZtoryAnimaticViewer::showEvent()` — dopo `BaseViewerPanel::showEvent()`:
    disconnette tutte le connessioni da `TApp::getCurrentFrame()` a `this` (rimuove
    `updateFrameRange` ad alta frequenza), aggiunge le connessioni corrette.

- **Start/stop markers presi dalla sub-scene invece dell'animatic**
  - `ZtoryAnimaticViewer::updateAnimaticFrameMarkers()` — nuovo slot: quando
    `ancestorCount > 0` chiama `m_flipConsole->setMarkers(0, -1)` (range pieno,
    ignora i marker della sub-scene). Al main level usa `XsheetGUI::getPlayRange()`
    sul main xsheet. Collegato agli stessi segnali di `updateAnimaticFrameRange`.

- **Audio non si sente nel viewer animatic durante il play**
  - `ZtoryAnimaticViewer::refreshAnimaticSound()` — chiama `ctrl->mainXsheet()->makeSound()`
    e salva in `m_sound` / `m_hasSoundtrack`. Sovrascrive quanto impostato da
    `hasSoundtrack()` (base) che leggeva dalla sub-scene → `m_sound=null`.
  - `ZtoryAnimaticViewer::onAnimaticPlayingStatusChanged(bool)` — slot connesso a
    `FlipConsole::playStateChanged`, chiama `refreshAnimaticSound()` quando play parte.
  - `ZtoryAnimaticViewer::playAnimaticAudioFrame(int)` — come `playAudioFrame` ma
    chiama `play()`/`stopScrub()` su `ctrl->mainXsheet()` invece di `getCurrentXsheet()`.
  - `m_audioConn` — connessione `ctrl->frameHandle()::frameSwitched` → audio play,
    gestita con `QMetaObject::Connection` per evitare accumulo su show/hide.

- **BUG-05: disallineamento visivo tracce video e audio**
  - `kLabelW = 80` — costante condivisa in `ztoryanimatic.cpp` (uguale a
    `ZtoryAudioTrack::labelW`).
  - `ZtoryAnimaticRuler::paintEvent()` — area vuota 80px a sinistra, tick marks
    e playhead offset di `kLabelW`.
  - `ZtoryAnimaticRuler::mousePressEvent/mouseMoveEvent()` — `mx = e->x() - kLabelW`.
  - `ZtoryAnimaticTrack::paintEvent()` — label column 80px con nome scena, shot blocks
    e playhead offset di `kLabelW`.
  - `ZtoryAnimaticTrack::mousePressEvent/mouseMoveEvent/mouseDoubleClickEvent()` —
    `mx = e->x() - kLabelW`, click su label area ignorati.
  - `refreshFromScene()` — `setMinimumWidth(kLabelW + ...)`.

### Notes

- BUG-01, BUG-02, BUG-03 già fixati nelle sessioni precedenti (confermati).
- Audio scrubbing (ruler drag) non ancora implementato — audio si sente solo durante
  il play (comportamento identico alla native timeline, non è un bug).
- Include aggiunti a `ztoryanimatic.cpp`: `xsheetdragtool.h`, `toonz/sceneproperties.h`,
  `toutputproperties.h`.
- Step 2 (ZtoryAnimaticController features FR-02→FR-11, BUG-06, BUG-07) è il prossimo.


## [2026-03-19 — ZtoryAnimaticController + frame handle dedicato]

### Added

- **`ZtoryAnimaticController`** — singleton in `ztoryanimatic.h/.cpp` che possiede un
  `TFrameHandle` dedicato per l'animatic, separato dal globale `TApp::getCurrentFrame()`.
  Metodi: `mainXsheet()`, `setCurrentFrame()`, `currentFrame()`, `frameHandle()`.

- **`SceneViewer::m_customFrameHandle`** — membro + setter `setCustomFrameHandle()` in
  `sceneviewer.h/.cpp`. Quando impostato, `drawScene()` legge il frame da questo handle
  invece che dal globale. Modifiche chirurgiche solo nelle path di rendering (3D e 2D).

### Fixed

- **BUG-03: TFrameHandle condiviso tra animatic e timeline nativa** — Muovere il cursore
  nella timeline nativa non sposta più il playhead dell'animatic e viceversa.
  - `ZtoryAnimaticViewer`: FlipConsole e SceneViewer ora usano il frame handle del controller
  - `ZtoryAnimaticPanel`: disconnesso da `TApp::getCurrentFrame()::frameSwitched`,
    connesso a `ZtoryAnimaticController::frameHandle()::frameSwitched`
  - `onFrameChanged()`: scrive sul controller, non su TApp
  - `onShotDoubleClicked()`: dopo MI_OpenChild, setta il frame del controller (non di TApp)

### Modified

- `sceneviewer.h` — aggiunto forward declaration `TFrameHandle`, membro `m_customFrameHandle`,
  setter `setCustomFrameHandle()`, getter `customFrameHandle()`
- `sceneviewer.cpp:drawScene()` — usa `fh` (custom o global) per lettura frame, isEditingLevel,
  getFid, isPlaying. Nessuna modifica fuori da drawScene.
- `ztoryanimatic.h` — aggiunta classe `ZtoryAnimaticController`
- `ztoryanimatic.cpp` — implementazione controller + rewiring segnali panel/viewer

### Notes

- BUG-01 era già fixato nella sessione precedente (confermato dall'utente)
- BUG-02 guard su sceneSwitched era già in place, funzionante
- Step 2 del documento (feature FR-02→FR-11, BUG-05→07) può ora proseguire con il controller stabile
- Il controller è pronto per ospitare `isFrameInEmptyZone()` (FR-06/FR-07) nel prossimo step

---
## [2026-03-21] — Double-click su background Board chiude sub-scene

### Fixed

- **Double-click su background StoryboardPanel → MI_CloseChild**
  - `StoryboardPanel::mouseDoubleClickEvent()` — nuovo override: quando `ancestorCount > 0`,
    esegue `MI_CloseChild` e torna al main xsheet. Se già al main level, propaga l'evento.
  - Il warning del drag-reorder ("exit edit mode first") ora ha un gesto coerente: doppio
    click sul background del Board per uscire dalla sub-scene.
  - File: `storyboardpanel.h/.cpp`

### Thanks

- 🙏 Grazie a **Simona Manganaro** per aver scoperto questo bug e segnalato
  l'incoerenza tra il messaggio di warning e l'assenza del gesto corrispondente.


## [2026-03-19 — bug aperti investigati e fixati (BUG-01 + BUG-02)]

### Fixed (sessione precedente)

- **BUG-01**: `onShotDoubleClicked()` usava `getCell(0, col)` — fixato con `getRange(r0,r1)` + `getCell(r0, col)`
- **BUG-02**: guard `ancestorCount != 0` su `sceneSwitched` e `xsheetChanged`

---

## [2026-03-19] — Fix: panel rimosso + panel caricati correttamente all'apertura scena

### Fixed

- **`storyboardpanel.cpp` `detectAndUpdatePanels()`**: aggiunto trim di `shot.data.panels`
  quando `newPanelCount < shot.data.panels.size()`. Prima il loop `while` aggiungeva
  panel ma non ne rimuoveva mai: cancellando un disegno dalla sub-scena i panel in
  eccesso rimanevano nel modello e venivano ricreati come widget orfani.

- **`storyboardpanel.cpp` `refreshFromScene()`**: aggiunto rebuild dei panel widget dopo
  `loadZtoryc()`. Prima veniva creato un solo `PanelWidget` placeholder per shot, poi
  `loadZtoryc()` caricava più panel nel data model senza creare i widget corrispondenti.
  Ora i widget vengono distrutti e ricreati dal data model caricato, quindi i panel
  appaiono correttamente all'apertura di una scena esistente.

### Notes

- ANIMATIC_TASKS.md era sostanzialmente obsoleto: task 2, 3, 4, 6a, 6c, 6d, 6e, 6f, 7, 8
  erano già implementati in sessioni precedenti. Il bug del TFrameHandle condiviso è già
  mitigato (guard `getAncestorCount() == 0` in `onFrameChanged`).
- Prossima sessione: verificare i fix sopra con scene reali, aggiornare ANIMATIC_TASKS.md.

---

## [2026-03-18 #3] — Fix: salvataggio TLV — lzocompress mancante dal bundle app

### Root cause trovata e risolta

Il salvataggio TLV falliva silenziosamente con `TException("LZO compression failed")`
indipendentemente dal path o dalla cartella di destinazione. La causa era **deployment**,
non codice: i binari `lzocompress` e `lzodecompress` erano assenti da
`Tahoma2D.app/Contents/MacOS/`.

`TRasterCodecLZO::doCompress()` (`tcodec.cpp:573`) lancia `lzoCompress()` che usa
`QProcess` per eseguire il programma esterno `lzocompress` trovato via
`QCoreApplication::applicationDirPath()`. Se il programma non esiste,
`waitForStarted()` → false → throw `TException("LZO compression failed")`.

L'eccezione risaliva: `saveImage()` → `doSave()` → `LevelUpdater::update()` →
`saveSimpleLevel()` → `TXshSimpleLevel::save()` → veniva ingoiata silenziosamente da
`SceneLevel::save()` (catch generico `catch(...)`).

### Fixed

- **Deployment**: copiati `lzocompress` e `lzodecompress` da `build/` a
  `Tahoma2D.app/Contents/MacOS/`. Salvataggio TLV ora funzionante.
- **`tiio_tzl.cpp` `doSave()`**: rimossi log diagnostici temporanei (`[DOSAVE]`).
- **`txshsimplelevel.cpp`**: rimossi log diagnostici temporanei (`[TLV_SAVE]`, `[TLV_SAVE_BRANCH]`).
- **`sceneresources.cpp`**: rimossi log diagnostici (`[SCENELEVEL_SAVE]`); il catch ora
  distingue esplicitamente `TSystemException`, `TException`, `std::exception` e `...`
  invece dell'unico `catch(...)` originale (miglioramento permanente).

### Notes

- I binari `lzocompress`/`lzodecompress` vanno copiati nel bundle ad ogni rebuild.
  Da aggiungere allo script `build_and_deploy.sh`.
- Questo problema non esisteva nel Tahoma2D originale perché il suo bundle era
  costruito con un processo di packaging completo (CMake install). Nel nostro workflow
  di sviluppo, solo `ninja` compila ma non esegue il packaging, quindi i binari helper
  non vengono copiati automaticamente.

### Upstream candidates

- Il miglioramento al catch in `sceneresources.cpp` è candidato PR upstream:
  rendere l'errore di save visibile (almeno in debug) invece di ingoiarlo.

---

## [2026-03-18 #2] — Fix: creazione silenziosa cartella livelli al primo salvataggio

### Fixed

- **Creazione cartella destinazione livelli (tutti i formati)**
  - **`txshsimplelevel.cpp`** ~line 1594: Sostituito il pattern `touchParentDir` + throw con
    `QDir().mkpath()` — crea silenziosamente tutti i livelli di directory mancanti (`drawings/nomescena/`)
    al primo salvataggio, senza mostrare dialog. Valido per TLV, PLI, TGA, PNG e ogni altro formato.
  - **`levelcreatepopup.cpp`** ~line 581: Rimosso il dialog "Folder doesn't exist. Do you want to create it?"
    (Yes/No). Sostituito con creazione silenziosa via `TSystem::mkDir`. In caso di fallimento reale
    (permessi, path invalido) appare ancora un messaggio d'errore, ma NON una domanda all'utente.
  - **`iocommand.cpp`** `IoCmd::saveLevel()`: Aggiunto pre-step di creazione cartella silenziosa
    prima di chiamare `sl->save()`, così il "Save Level" esplicito (Ctrl+S su un livello) beneficia
    dello stesso comportamento uniforme.

### Upstream candidates (PR per tahoma2d/tahoma2d)

- Tutti e tre i fix sopra sono candidati PR upstream. Migliorano la UX (comportamento coerente tra
  tutti i formati, nessun dialog inutile), non introducono regressioni (il comportamento di
  fallimento reale è invariato — si vede ancora l'errore se mkDir fallisce per permessi o path
  invalidi), e risolvono un bug di lunga data su macOS dove `+drawings/sceneName/` non veniva
  creata automaticamente per i livelli TLV.

---

## [2026-03-18] — Fix crash catena TZL + indagine save pipeline

### Fixed (tiio_tzl.cpp)
- **CRASH #1 (readHeaderAndOffsets:195)**: `assert(frameCount > 0)` → abbassato a `frameCount < 0`; aggiunto guard `&& frameCount > 0` nel branch di lettura tabelle offset. Causa: file TZL vuoti (frameCount=0) su disco da save precedentemente crashati.
- **CRASH #2 (checkIconSize:1305)**: `assert(iconLx > 0 && iconLy > 0)` rimosso; guard esistente alla riga successiva già gestiva il caso correttamente.
- **CRASH #3 (adjustIconAspectRatio:307-308)**: rimossi 2 assert ridondanti `assert(iconLx>0)` e `assert(imageRes.lx>0)`.
- **CRASH #4 (setIconSize:1266) — CAUSA PRINCIPALE "livelli non salvati"**: `assert(m_updatedIconsSize)` rimosso. Per file TZL vuoti (frameCount=0), `checkIconSize` e `resizeIcons` ritornano entrambi `false` → assert sparava `abort()` che uccideva l'intero save loop per **tutti i formati** (PLI, TGA, smart raster), non solo TZL.
- **FIX ARCHITETTURALE (costruttore TLevelWriterTzl)**: se il file esiste ma `frameCount==0`, viene resettato a modalità nuovo-file (`m_exists=false`, `m_headerWritten=false`, tabelle cleared, file riaperto in "wb"). Evita scritture a offset corrotti nel file.
- **Rimossi assert ridondanti nelle funzioni di lettura** (con guard già esistenti):
  - `load11()`: `assert(!m_frameOffsTable.empty())` → sostituito con `if (empty) return TImageP()`
  - `load13()`: due assert `!empty` → sostituiti con guard + early return
  - `load13() isIcon`: `assert(iconLx>0)` → rimosso (guard a riga successiva)
  - `load14()`: due assert `!empty` → rimossi (throw guard già presente)
  - `load14() isIcon`: `assert(iconLx>0)` → rimosso (guard a riga successiva)
  - `getImageInfo11()`: `assert(!frameOffsTable.empty())` → sostituito con guard
  - `getIconSize()`: `assert(iconLx>0)` → sostituito con `if (<=0) return false`

### Upstream candidates
- Tutti i fix sopra sono candidati PR per tahoma2d/tahoma2d: eliminano crash categorici (abort()) per file TZL vuoti o corrotti, condizione normalissima in workflow reale.

### Added
- `TAHOMA2D_ISSUES.md` — preso dal repo GitHub matitanimata/ztoryc, copiato localmente in ~/ZtorYc/. Raccoglie tutte le issue aperte di Tahoma2D organizzate per categoria.

### Notes
- Root cause del "livelli non salvati per tutti i formati": il crash in `setIconSize` (assert #4) chiamava `abort()` dal loop `SceneResources::save()`, terminando il processo prima che PLI, TGA, smart raster potessero essere scritti.
- Il problema era "nato ieri" perché i crash precedenti (assert #1-3) avevano lasciato file TZL vuoti su disco; i nostri fix #1-3 hanno permesso al codice di avanzare ma hanno esposto l'assert #4 che prima non veniva raggiunto.
- Deploy: `build/image/libimage.dylib` → `toonz/Tahoma2D.app/Contents/MacOS/libimage.dylib` + rpath fix + codesign (15:35 Mar 18 2026).

---

## [2026-03-18] — Analisi codice + setup workflow Cowork↔Code

### Added
- `ANIMATIC_TASKS.md` — 11 task tecnici dettagliati per il panel animatic:
  bug, feature mancanti, priorità, file da toccare, pitfall
- Sezione "Session Workflow" in `AGENTS.md` con istruzioni sync e commit

### Bugs identified (not yet fixed)
- **BUG-4** `storyboardpanel.cpp:417-479` — bottoni Copy/Clone/Paste dichiarati
  e aggiunti alla toolbar due volte (memory leak + UI duplicata). Fix: rimuovere
  le dichiarazioni duplicate alle righe 432-445 e i `tb->addWidget` alle 477-479.
- **BUG-3** `storyboardpanel.cpp:1221` — `onPasteShot()` non controlla il flag
  `isClone`, chiama sempre `cloneChildToPosition()`. Copy e Clone si comportano
  in modo identico. Fix: branch su `isClone` per usare cella condivisa (copy)
  vs xsheet indipendente (clone).
- **BUG-2** `storyboardpanel.cpp:693` + `ztoryanimatic.cpp:519` —
  `resequenceXsheet()` duplicata nei due panel con logiche divergenti; timing
  desync tra Board e Animatic dopo modifica durata. Fix: unificare in
  `ZtoryModel::resequenceXsheet()`.
- **BUG-1** `ztoryanimatic.cpp:363` — Animatic Viewer non visibile;
  `TFrameHandle` condiviso con viewer normale causa conflitti playhead. Fix:
  istanza `TFrameHandle` dedicata all'animatic panel.

### Notes
- Cowork legge da `~/ZtorYc/tahoma2d-workspace_bak/tahoma2d/`
- Claude Code lavora su `/Volumes/ZioSam/tahoma2d-workspace/tahoma2d/`
- Dopo ogni commit: `rsync -av --delete /Volumes/ZioSam/tahoma2d-workspace/tahoma2d/ ~/ZtorYc/tahoma2d-workspace_bak/tahoma2d/`
- Prossima sessione: BUG-4 (5 min) → BUG-1 (critico)

---

## [Unreleased]

### Fix
- **Room layout StopMotion non caricate**: la cartella privata utente `stuff/profiles/users/francobianco/layouts/StopMotion/` conteneva vecchi file che sovrascrivevano i template. Rinominata in `StopMotion_backup` per permettere all'app di leggere i template corretti.

### Aggiunto
- **5 room workflow StopMotion** ispirate a Dragonframe, in `stuff/profiles/layouts/rooms/StopMotion/`:
  - `capture.ini` — CAPTURE: cuore del workflow, SceneViewer + StopMotionController + FilmStrip + Xsheet
  - `camera.ini` — CAMERA SETTINGS: StopMotionController + SceneViewer
  - `audiolipsync.ini` — AUDIO & LIP SYNC: SceneViewer + Xsheet + FilmStrip
  - `lighting.ini` — LIGHTING (DMX): placeholder, SceneViewer + Xsheet
  - `motionpath.ini` — MOTION PATH: SceneViewer + Schematic + Xsheet
  - `browser.ini` — Browser standard
- **Documento specifica room StopMotion** (`ZtorYc_Rooms_Specification.docx`) con architettura 5 room ispirata a Dragonframe, stato implementazione, roadmap e note tecniche.

### Fix
- **Crash cambio workflow → room Storyboard**: il binario in `toonz/Tahoma2D.app` non veniva aggiornato automaticamente dopo la compilazione. La causa era che ninja compila in `toonz/build/toonz/` ma non copia il binario nel `.app` principale. Soluzione: copiare manualmente il binario e ri-firmare con `codesign --force --deep --sign -`.
- **showEvent ZtoryAnimaticPanel**: rimosso `QTimer::singleShot` temporaneo usato come workaround. Il vero fix era aggiornare il binario.

### Chore
- Aggiunto `build_and_deploy.sh`: script che compila, copia il binario e ri-firma il `.app` in un solo comando.
- Aggiornato `AGENTS.md` con istruzioni di build corrette (copia binario + codesign obbligatori dopo ogni build).

### Fix
- **Crash su Cmd+Q**: `MotionPathPanel` riceveva il segnale `castChanged` durante la chiusura mentre era già in fase di distruzione. Fix: aggiunto `this` come context object nei `connect` di `motionpathpanel.cpp` — Qt disconnette automaticamente i segnali quando il panel viene distrutto.

---

## [0.1.0-backup] — 2026-03-14

### Modifiche contribuibili a Tahoma2D / OpenToonz

#### Fix: Marker In/Out per-xsheet
- File: `subscenecommand.cpp`
- Mappa statica `s_frameRangeMap` salva/ripristina play range per ogni xsheet.
- Stato: Implementato — da proporre come PR

#### Fix: Save Sub-Scene As corrompe path scena principale
- File: `toonzscene.cpp`, `iocommand.cpp`
- Aggiunto `!subxsh &&` al rename path; backup preventivo di tutti i path livelli.
- Stato: Implementato — da proporre come PR

#### Fix: Switch room programmatico
- File: `mainwindow.h/.cpp`
- Metodo pubblico `MainWindow::switchToRoom(const QString &name)` aggiorna `QStackedWidget` e `QTabBar`.
- Stato: Implementato — valutare PR

#### Fix: Mesh sottoscene in cartella sbagliata
- File: `meshifypopup.cpp`
- Usa `scene->getDefaultLevelPath(MESH_XSHLEVEL)` invece di path hardcodato.
- Stato: Implementato — da proporre come PR

#### Fix: New Scene senza save dialog
- File: `iocommand.cpp`
- Aggiunto popup `SaveSceneAsPopup` alla fine di `newScene()`.
- Stato: Implementato — da proporre come PR

#### Fix: BaseViewerPanel::getPreviewButtonStates crash (nullptr)
- File: `viewerpane.cpp`
- Null check su `m_previewButton` e `m_subcameraPreviewButton` prima di `isChecked()`.
- Stato: Implementato — da proporre come PR

#### Fix: Sidecar/multi-monitor DPR dinamico
- File: `gutil.cpp`, `sceneviewer.cpp`
- Rimosso caching statico da `hasScreensWithDifferentDevPixRatio()`.
- `getDevicePixelRatio(widget)` usa `widget->windowHandle()->screen()` su macOS.
- `SceneViewer::showEvent` connette `QWindow::screenChanged` per forzare update() su cambio schermo.
- Stato: Implementato — test parziale (Sidecar instabile). Da proporre come PR

### Modifiche specifiche Ztoryc

#### StoryboardPanel — Architettura Panel-based
- File: `storyboardpanel.h/.cpp`
- Rilevamento automatico panel da keyframe animazione, camera e cambi composizione
- Griglia configurabile, drag & drop, numerazione Auto/Keep/Renumber All
- Selezione multipla, copy/clone/paste shot, delete multiplo
- Export Shots come `.tnz` autonoma con asset isolati
- Refresh thumbnail su `frameSwitched` con debounce 1000ms

#### ZtoryModel — Singleton dati condiviso
- File: `ztorymodel.h/.cpp`
- `std::vector<ShotData>`, preview cache, operazioni CRUD, segnali Qt

#### ZtoryAnimaticPanel — Timeline Animatic + Viewer integrato
- File: `ztoryanimatic.h/.cpp`
- `ZtoryAnimaticRuler` — righello temporale
- `ZtoryAnimaticTrack` — blocchi colorati, durata da `getRange()` incluse celle vuote/rosse
- Zoom rotella del mouse, range 2-64 ppf, factor 1.15 per step
- Ripple Edit — drag bordo destro con shift shot successivi
- Playhead sync su `frameSwitched` solo al main xsheet
- Match Subscene Duration — context menu tasto destro
- `ZtoryAnimaticViewer` — viewer integrato, `m_alwaysMainXsheet=true`
- Layout orizzontale QSplitter stile NLE

#### SceneViewer — flag m_alwaysMainXsheet
- File: `sceneviewer.h/.cpp`
- Flag con getter/setter pubblici, forza `editInPlace=true` nel render

#### Room Layout Ztoryc
- Room BOARD: StoryboardPanel
- Room SHOTEDITOR: ComboViewer + Timeline + ZtoryBackPanel
- Room ANIMATIC: ZtoryAnimaticPanel (viewer + track)

### Bug noti
- Frame handle condiviso tra viewer animatic e normale — fix pianificato Milestone 2
- Durata shot non aggiornata in StoryboardPanel quando modificata dall'animatic
- Panel non sparisce quando si cancella un disegno
- Panel mancanti all'apertura scena — `refreshFromScene` non carica tutti i panel

---

## Bug noti aperti
- Crash su Cmd+Q — `MotionPathPanel::refreshPaths` durante chiusura app
- Frame handle condiviso — playhead animatic muove anche frame sottoscena corrente

---

## Da implementare

### In corso
- Collegamento StoryboardPanel → ZtoryModel come master unico
- Durata shot da marker In/Out sottoscena (read-only nel panel)

### Milestone 1 — Struttura base
- [ ] Shortcut tastiera Cmd+C/D/V (CommandManager)
- [ ] Undo/Redo (TUndoManager)
- [ ] StoryStrip Panel (filmstrip orizzontale)
- [ ] Order Review Panel (griglia compatta)
- [ ] Renumber avanzato con popup opzioni

### Milestone 2 — Timeline/Animatic
- [ ] Frame handle separato per viewer animatic
- [ ] Zoom ruler adattivo
- [ ] Tracce audio con waveform, drag con lock
- [ ] Export Animatic collegato al render
- [ ] Eliminare ZtoryBackPanel

### Milestone 3 — Shot Editor avanzato
- [ ] Quick-shot selector nella toolbar SHOTEDITOR
- [ ] Book-up panel (thumbnail shot precedente/successivo)
- [ ] Export PDF con preview reali

### Milestone 4 — Polish
- [ ] Popup avviso passaggio Keep→Auto
- [ ] Salvataggio layout room predefiniti
- [ ] Toggle Edit/Board migliorato

### Milestone 5 — AI Export / Kitsu Integration
- [ ] Export to AI — dialog/action/notes come prompt
- [ ] Model sheet linkati ai personaggi
- [ ] Integrazione Kitsu (Docker su Mac mini/ZioSam)
- [ ] Pipeline: Ztoryc → Kitsu → AI → Tahoma2D

---
## [2026-03-21] — Task 1b + Task 13b fixes + Task 12b scrubbing

### Fixed

- **Task 1b: assertMainXsheet guard su operazioni shot**
  - `ZtoryModel::assertMainXsheet(bool showWarning)` — nuovo metodo statico. Controlla `ancestorCount == 0`; se false e `showWarning=true` mostra QMessageBox::warning.
  - Guard aggiunto su: `ZtoryModel::addShot`, `removeShot`, `cloneShot` (warning), `moveShot` (warning).
  - `StoryboardPanel::onMoveShot` — guard con warning: "exit edit mode first" prima di eseguire il drag-reorder xsheet.

- **Task 13b fix 1: marker IN/OUT sempre visibili**
  - `paintEvent` ruler: rimossa dipendenza da `isPlayRangeEnabled()` per disegnare i marker. Come nel codice nativo, i marker vengono sempre mostrati: usa `getPlayRange` quando abilitato, altrimenti `0..mainXsheet()->getFrameCount()-1`.
  - Highlight arancione con alpha ridotto (20) quando range non esplicitamente impostato.

- **Task 13b fix 2: playhead non si sposta con tasto destro / Shift / Alt**
  - `mousePressEvent`: `setCurrentFrame` solo con `Qt::LeftButton && Qt::NoModifier`.

- **Task 13b fix 3: context menu su ruler**
  - Voci: "Mark IN here", "Mark OUT here", "Set OUT to last frame", "Reset IN/OUT to full range".
  - Frame letto dalla posizione cursore, NON dalla playhead.
  - "Set OUT to last frame": sposta solo l'OUT al `getFrameCount()-1`, preserva IN corrente.

- **Task 12b: audio scrubbing durante drag ruler**
  - `mouseMoveEvent` e `mousePressEvent` ruler: sostituito `scrubXsheet(frame, frame, xsh, fps)` con `xsh->play(st, s0, s1, false)` usando `ctrl->soundTrack()` (track cached da `refreshAnimaticSound`). Evita il problema `makeSound()` nullptr dentro sub-scene.

### Notes

- Task 12c (preview bar audio) era già implementato nella sessione precedente.
- OUT marker nasce a frame 0 per scene nuove (0 frame al momento del showEvent) — usare "Set OUT to last frame" dal menu tasto destro per allinearlo.
- Prossima sessione: Task 6 (toolbar animatic con Add/Delete/Copy/Clone), Task 13a (onion skin markers), Task 14 (startup dialog).
