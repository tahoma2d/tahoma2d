# Ztoryc — Changelog

> **Come aggiornare (istruzioni per Claude Code):** dopo ogni sessione aggiungi
> una voce in cima con: data, `### Fixed`, `### Added`, `### Modified`,
> `### Upstream candidates`, `### Notes`. Poi esegui rsync (vedi AGENTS.md).
> Voci più vecchie di ~2 settimane → spostarle in `CHANGELOG_ARCHIVE.md`.

---
## [2026-04-24] — resetOnSeqChange: riavvio contatore SH per sequenza

### Added
- **`NumberingConfig::resetOnSeqChange`** — nuovo campo bool (default `false`).
  Quando `true` (solo Sequence style): il contatore SH si azzera a `startNumber`
  ad ogni cambio di sequenza (SQ01→SH010, SQ02→SH010…). Quando `false`:
  numerazione globale continua tra tutte le sequenze.
- **`m_resetOnSeqChangeCB`** in `StartupPopup` — checkbox "Restart shot # at each
  new sequence", visibile solo in Sequence style; stato salvato in `NumberingConfig`.
- **`resetOnSeqCB`** in `StoryboardPanel::onNumberingConfig()` — stessa checkbox
  nel dialogo di configurazione numerazione del Board.

### Modified
- **`StoryboardPanel::renumberAll()`** — in Auto mode, le sequenze sopravvivono
  al renumber (solo SH cambia). I nuovi shot senza `sequenceId` ereditano la
  sequenza dello shot precedente. Con `resetOnSeqChange`, `shotIdx` è relativo
  alla sequenza (non globale).
- **`StartupPopup::onCreateButton()`** — in Sequence mode, crea una sequenza
  default "sq01" e vi assegna tutti gli shot iniziali (campo SQ pre-popolato).

### Fixed
- **Crash SIGABRT su import scena con Plastic Deformer** — `ZtoryAnimaticTrack::
  refreshFromScene()` e `ZtoryStoryStripPanel::refreshFromScene()` chiamavano
  `IconGenerator::getIcon()` sincronicamente durante `xsheetChanged`. Durante
  l'import di una scena, la xsheet non è ancora stabilizzata: il rendering
  triggerava `PlasticDeformerStorage::process()` → `PlasticDeformer::initialize()`
  → `tlin::factorize()` → `StatFree()` su SuperLU Matrix non inizializzata → crash.
  Fix: entrambi gli handler `xsheetChanged` wrappati con `QTimer::singleShot(0)`
  per differire l'esecuzione all'iterazione successiva dell'event loop.
  Rimossa anche la chiamata ridondante `updateAllPreviews()` da
  `ZtoryModel::onXsheetChanged()` (violava regola AGENTS.md).

---
## [2026-04-23] — Numerazione SQ/SH, rename app Ztoryc, fix firma bundle

### Added
- **Sequenze editabili nel Board** — campo SQ separato e editabile per ogni shot.
  Digitando un numero di sequenza (es. "020") viene assegnata la sequenza a quello
  shot e a tutti i seguenti fino al prossimo cambio manuale (`seqLabelEdited` cascade).
- **`ZtoryModel::findOrCreateSequence()`** — trova o crea `SequenceData` by label,
  usata sia dal cascade handler che dal renumber automatico.
- **`ZtoryModel::assignShotLabel()` (static)** — algoritmo midpoint condiviso tra
  `ZtoryModel` e `StoryboardPanel` per generare label senza duplicati al momento
  dell'inserimento (Keep mode → SH015 tra SH010 e SH020; Auto mode → rinumera tutto).

### Fixed
- **Doppio click entra e ritorna subito** — `PanelWidget::mouseDoubleClickEvent`
  chiamava `QFrame::mouseDoubleClickEvent(e)` che propagava l'evento a
  `StoryboardPanel::mouseDoubleClickEvent` il quale eseguiva `MI_CloseChild`.
  Fix: sostituito con `e->accept()`.
- **Shot duplicato al momento dell'inserimento** — in modalità Auto, `renumberAll()`
  usava `ZtoryModel::m_shots` come sorgente invece della lista locale del Board,
  ottenendo l'indice errato. Fix: algoritmo statico opera sulla lista locale del Board.
- **Campo SH mostrava "SH - sq01_sh010"** — `setShotNumber()` ora separa SQ e SH
  sul separatore `_`, mostra solo la parte numerica in ciascun campo e salva il
  prefisso in `m_storedShotPrefix`/`m_storedSeqPrefix` per la ricostruzione.
- **`renumberAll()` Auto + Sequence style** — `cfg.shotName(i)` restituisce
  "SQ001_SH010"; ora viene splittato correttamente: SH → `shotLabel`, SQ → `sequenceId`.

### Modified
- **Rename app: Tahoma2D → Ztoryc** — bundle ID `io.github.ztoryc.Ztoryc`,
  `CFBundleName/ExecutableName = Ztoryc`, versione 1.0.0.
  File cambiati: `CMakeLists.txt` (target), `BundleInfo.plist.in`, `main.cpp`,
  `Ztoryc.entitlements`, `build_and_deploy.sh`.
- **`build_and_deploy.sh`** — firma corretta senza `--deep` (dylib firmate
  singolarmente prima del bundle); `xattr -cr` prima della firma; `rm -rf profiles/`
  per evitare "unsealed contents in bundle root"; copia automatica `SystemVar.ini`
  se mancante; copia dylib secondarie dal build tree.

### Notes
- `Ztoryc.app/profiles/` viene ricreata dall'app ad ogni avvio — è normale,
  non invalida la firma al lancio (il seal è valido al momento di `open`).
- `SystemVar.ini` punta a `/Volumes/ZioSam/.../stuff` — path assoluto,
  non portabile; da parametrizzare per distribuzione.
- Per permessi TCC stabili: aggiungere Ztoryc.app al Full Disk Access in
  System Settings → Privacy & Security.

---
## [2026-04-23b] — Branding Ztoryc completato

### Modified
- **`tversion.h`** — `applicationName = "Ztoryc"`, versione 1.0 (era Tahoma2D 1.6).
  Propaga su titolo finestra, startup popup, about dialog, tutti i log.
- **`tahoma2d_splash.svg`** — icona Ztoryc (PNG embedded base64) + wordmark +
  tagline "STORYBOARD · ANIMATIC · ANIMATION" su sfondo scuro.
- **`tahoma2d_startup.svg`** — banner orizzontale: icona + "ZTORYC" in giallo `#F5B800`.
- **`tipspopup.cpp`** — titolo "Ztoryc Tips".
- **`mainwindow.cpp`** — update checker punta a github.com/matitanimata/ztoryc.
- **`main.cpp`** — tips popup disabilitato; update check automatico disabilitato
  (contenuti ancora riferiti a Tahoma2D).
- **`Ztoryc.icns`** — generato da `ztoryc_icon.png` con tutte le risoluzioni macOS
  (16×16 → 1024×1024).

### Notes
- `toonz.qrc` va touchato prima di ogni modifica SVG per forzare la ricompilazione
  delle risorse Qt: `touch toonz/sources/toonz/toonz.qrc && ./build_and_deploy.sh`

---
## [2026-04-20] — Fix: 7 crash + audio export + workflow switch lento

### Fixed
- **Crash FlipConsole::doButtonPressed (QThread::isRunning SIGSEGV)** — durante
  `clearRooms()` i widget venivano nascosti e `hideEvent` → `setActive(false)` →
  `pressButton(ePause)` → `doButtonPressed` iterava `m_visibleConsoles` con pointer
  potenzialmente stale. Fix: `setActive(false)` ora abortisce direttamente il
  `PlaybackExecutor` inline invece di passare per click→signal→slot chain.
  (`flipconsole.cpp`)

- **Crash ~FlipConsole dangling pointer** — `m_visibleConsoles` non veniva pulita
  nel distruttore. Aggiunto `~FlipConsole()` che rimuove `this` dalla lista.
  (`flipconsole.cpp`, `flipconsole.h`)

- **Crash SceneViewer/FxGadgetController (TTool::m_viewer dangling)** — al load di
  una scena il `SceneViewer` veniva distrutto ma `TTool::m_viewer` non veniva
  azzerato → crash in `onFxSwitched`. Fix: `SceneViewer::~SceneViewer()` chiama
  `TTool::onViewerDestroyed(this)` che azzera tutti i tool che puntano a quel viewer.
  (`sceneviewer.cpp`, `tool.cpp`, `tool.h`)

- **Crash PlasticDeformer SuperLU (dgstrf NaN)** — triangoli degeneri in una mesh
  producevano NaN/Inf da `ortCoords()` che venivano passati a SuperLU → crash.
  Fix: guard `isfinite()` in `initializeStep2()` salta la fattorizzazione per facce
  degeneri; `deformStep2()` usa posizione invariata quando `m_invF[f]` è null.
  (`plasticdeformer.cpp`)

- **Crash Room::save() da switchRoomChoice re-entrante** — `Room::load()` chiama
  `qApp->processEvents()` che fa scattare il `QTimer::singleShot(0)` che resettava
  `m_isHandlingWorkflow=false`, permettendo un secondo `switchRoomChoice` annidato
  che settava poi `m_isSwitchingRooms=false`. L'outer `readSettings` entrava in
  `makePrivate(rooms)` con pointer dangling → SIGSEGV. Fix: guard
  `if (m_isSwitchingRooms) return;` all'inizio di `switchRoomChoice`.
  (`mainwindow.cpp`)

- **Audio export oltre lunghezza shot** — `vsf - shotR0` usava `getVisibleStartFrame()`
  invece di `getStartFrame()` per calcolare la posizione nella colonna destinazione.
  Fix: usa `cl->getStartFrame() - shotR0`.
  (`storyboardpanel.cpp`)

- **"Load Audio" non apriva il dialog su macOS** — parent `this` invece di `nullptr`
  rendeva il dialog invisibile dietro la finestra principale. (`ztoryanimatic.cpp`)

- **Audio stale tra scene diverse** — `requireSoundTrack()` usava la cache della
  scena precedente al cambio scena. Fix: `invalidateSoundTrack()` chiamato nel
  handler `sceneSwitched`. (`ztoryanimatic.cpp`)

### Performance
- **Workflow switch verso Storyboard lento (1–3 s)** — `makeSound()` bloccava il
  main thread perché veniva chiamato da `singleShot(0)` che scattava dentro
  `qApp->processEvents()` di `Room::load()`. Fix: `preBuildSoundTrackAsync()` esegue
  `makeSound()` in un `std::thread` detached; il risultato è consegnato al main
  thread via `QMetaObject::invokeMethod(QueuedConnection)`. Zero blocking.
  (`ztoryanimatic.cpp`, `ztoryanimatic.h`)

### Modified
- `flipconsole.cpp` — `~FlipConsole()`, `setActive(false)` riscritta
- `flipconsole.h` — aggiunto `~FlipConsole()`
- `sceneviewer.cpp` — `~SceneViewer()` chiama `TTool::onViewerDestroyed`
- `tool.cpp` / `tool.h` — aggiunto `TTool::onViewerDestroyed(Viewer*)`
- `plasticdeformer.cpp` — guard triangoli degeneri in step2
- `mainwindow.cpp` — re-entrancy guard in `switchRoomChoice`
- `storyboardpanel.cpp` — fix audio export frame offset
- `ztoryanimatic.cpp` / `.h` — Load Audio fix, sceneSwitched invalidate, async sound build

### Notes
- Il ritardo al primo switch verso Storyboard è fisiologico: il Board carica le
  anteprime (500ms timer) e l'audio viene costruito in background. Non è un bug.

---
## [2026-04-19b] — Fix: double-update Board dopo operazioni Animatic

### Fixed
- **Razor, AddShot, MergeWithNext dall'Animatic aggiungevano uno shot vuoto extra nel Board**
  - Root cause: stessa classe di bug del merge double-removal. Dopo `resequenceXsheet()`
    → `modelReset()` → `onModelResequenced()` → `refreshFromScene()` il Board era già
    corretto (4 shot dopo razor), poi arrivava `emit shotAdded(newCol)` →
    `onShotInserted()` inseriva un altro shot vuoto (senza sub-scene) → Board a 5 shot.
  - Fix: rimossi `emit shotAdded()`/`emit shotRemovedAt()` da `onRazorRequested()`,
    `onAddShot()`, `onMergeWithNext()`. Il Board si sincronizza esclusivamente via
    `resequenceXsheet()` → `modelReset()` → `onModelResequenced()` (xsheet count check).

### Modified
- `ztoryanimatic.cpp` — rimossi 3 emit ridondanti post-resequenceXsheet

---
## [2026-04-19] — Shared clipboard e shared selection Board ↔ Animatic + fix merge double-removal

### Added
- **Shared clipboard Board ↔ Animatic** (`ztorymodel.h`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`)
  - `ZtoryClipEntry` struct e `m_sharedClip` in `ZtoryModel` — unica source of truth per clipboard
  - Board (`onCopyShot`, `onCutShot`, `onCloneShot`): scrive sempre su `ZtoryModel::setSharedClip()`
  - Animatic (`onCopyShots`, `onCutShots`, `onCloneShots`): usa già `ZtoryModel::sharedClip()`
  - `pasteSharedClipToBoard()` — helper statico in `storyboardpanel.cpp` che replica
    la logica di `pasteFromClip()` usando il `cloneChildToPosition()` locale
  - Board `onPasteShot()`: shared clip ha sempre priorità su `m_clipboard` locale
    (fix bug: `m_clipboard` stale con 3 shot causava incolla 3 invece di 1 dopo copy da Animatic)

- **Shared selection Board ↔ Animatic** (`ztorymodel.h`, `ztoryanimatic.cpp`, `storyboardpanel.cpp`)
  - `m_sharedSelection` (set di xsheet columns) in `ZtoryModel` con getter/setter
  - Animatic: `selectionChanged` signal → `ZtoryModel::setSharedSelection()`
  - Board `onPanelClicked()`: converte `m_selectedIndices` → xsheet columns → `setSharedSelection()`
  - Merge cross-panel: seleziona in Animatic → merge button Board funziona (e viceversa)
  - Fallback "last panel wins": vince sempre l'ultima interazione utente

### Fixed
- **Bug merge cross-panel: double-removal nel Board** (`storyboardpanel.cpp`, `ztoryanimatic.cpp`)
  - Root cause: `onModelResequenced()` usava `ZtoryModel::m_shots.size()` come riferimento
    ma quella dimensione è stale dopo operazioni copy/paste/clone che bypassano
    `ZtoryModel::addShot()/removeShot()`. Se stale ≠ Board count → `refreshFromScene()` (Board → 5 shot)
    poi arrivava anche `emit shotRemovedAt(4)` → `onShotRemovedAt()` → rimozione extra (Board → 4 shot)
  - Fix 1: `onModelResequenced()` conta le colonne child-level direttamente dall'xsheet (ground truth),
    non da `ZtoryModel::m_shots.size()`
  - Fix 2: Animatic `onMergeShots()`: rimosso `emit shotRemovedAt()` — il Board si sincronizza già
    via `resequenceXsheet()` → `modelReset()` → `onModelResequenced()`
  - Fix 3: Board `onMergeShots()`: `m_updating=true` attorno all'emit di `shotRemovedAt()` per
    prevenire self-processing (double-removal anche per merge nativo del Board)

- **Bug clipboard priorità**: Board usava `m_clipboard` locale (stale) invece dello shared clip
  - Fix: in `onPasteShot()` lo shared clip ha sempre la precedenza; `m_clipboard` è solo fallback

### Modified
- `ztorymodel.h` — aggiunti `ZtoryClipEntry`, `m_sharedClip`, `m_sharedSelection` + `#include <set>`
- `ztoryanimatic.h` — rimossi `AnimClipEntry`/`m_animClip`; commento shared clipboard
- `ztoryanimatic.cpp` — riscritta gestione clipboard; merge fix; connect selectionChanged
- `storyboardpanel.cpp` — shared clipboard write in copy/cut/clone; paste fallback; merge fix;
  shared selection write in onPanelClicked; pasteSharedClipToBoard() helper

---
## [2026-04-17] — Fix: crash BrushToolOptionsBox + AutoFill restore

### Fixed
- **`tooloptions.cpp` — crash on sub-xsheet entry and app close (`BrushToolOptionsBox::updateStatus`)**
  - Root cause: `updateStatus()` era chiamata sincronamente durante la signal chain
    dell'xsheet switch (`openSubXsheet` / `saveSceneIfNeeded`); in quel momento
    `m_pltHandle->getPalette()` può restituire un puntatore temporaneamente invalido
    → SIGSEGV in `rebuildAutoFillStyleCombo`.
  - Fix: entrambe le chiamate critiche (`rebuildAutoFillStyleCombo` +
    `notifyToolComboBoxListChanged`) deferite con `QTimer::singleShot(0, this, lambda)`,
    così vengono eseguite solo dopo che la signal chain si è completamente disfatta.
  - Aggiunto change-detection (`m_lastPalette`, `m_lastPaletteStyles`) per evitare
    rebuild superflui.
  - `try-catch(...)` non era sufficiente: SIGSEGV è un segnale Unix, non un'eccezione C++.

### Modified
- **`tooloptions.cpp`** — `BrushToolOptionsBox::updateStatus()` con QTimer deferred rebuild
- **`tooloptions.h`** — aggiunti `m_lastPalette` / `m_lastPaletteStyles` a `BrushToolOptionsBox`
- **`toonzrasterbrushtool.cpp`** — `rebuildAutoFillStyleCombo` ripristinato con lista completa
  palette; fill code ripristinato al comportamento originale (`getPaint() == 0`)
- **`toonzrasterbrushtool.h`** — `rebuildAutoFillStyleCombo(TPaletteP pal)` dichiarazione ripristinata

### Notes
- AutoFill "Fill Style" combo ora mostra di nuovo tutti i colori della palette (non solo "+1")
- Fill con antialias ripristinato al comportamento originale (era stato rimosso per errore)
- Savebox fix mantenuto: `sb = sb + m_strokeRect` per evitare scan area 1×1 al primo stroke

---
## [2026-04-16] — Fix: render preview frame bianco/trasparente

### Fixed
- **`toonz/sources/tnzbase/trasterfx.cpp` — `enlargeToI()` UB con `TConsts::infiniteRectD`**
  - Root cause definitivo identificato e corretto.
  - `enlargeToI(TRectD &r)` applica `tfloor`/`tceil` (che fanno `(int)(x)`) a `TConsts::infiniteRectD = TRectD(-DBL_MAX,-DBL_MAX,DBL_MAX,DBL_MAX)`. Cast `(int)(±DBL_MAX)` è undefined behavior; su questo Mac produce `(int)(DBL_MAX)=-1` e `(int)(-DBL_MAX)=0`, corrompendo il rect a `(-1,-1)-(0,0)`.
  - `ColorCardFx::doGetBBox` ritorna `infiniteRectD` → dopo `enlargeToI` il bbox di `overFx` diventa `(-1,-1)-(0,0)` → `interestingRect` = 1×1 pixel → tutto il render è 1 pixel trasparente.
  - **Fix**: guard in `enlargeToI` che skippa la conversione se qualsiasi coordinata supera `INT_MAX/2`:
    ```cpp
    const double kMaxSafeInt = static_cast<double>(std::numeric_limits<int>::max() / 2);
    if (r.x0 < -kMaxSafeInt || r.x1 > kMaxSafeInt || r.y0 < -kMaxSafeInt || r.y1 > kMaxSafeInt)
        return;
    ```

### Modified
- Rimossi tutti i log diagnostici `std::cerr` aggiunti nelle sessioni precedenti da:
  - `trasterfx.cpp` (logger rimosso dall'agent)
  - `tcolumnfx.cpp`, `scenefx.cpp`, `previewer.cpp`, `sceneviewer.cpp` (rimossi con Python script)

### Notes — Diagnosi completa (path del bug)
```
ColorCardFx::doGetBBox → restituisce TConsts::infiniteRectD
  → TRasterFx::getBBox chiama enlargeToI(infiniteRectD)
    → (int)(DBL_MAX) = -1 [UB sul Mac]
    → temp = TRectD(-1,-1,0,0)
    → myIsEmpty(-1,-1,0,0) = false (getLx()=1 ≥ 1)
    → r corrotto a (-1,-1)-(0,0)
  → overFx.compute: interestingRect = tileRect * (-1,-1,0,0) = 1×1 pixel
  → tutta la chain renderizza 1 pixel trasparente → frame bianco in output
```
Confirmato con log14: `[compute_extract] fx=overFx tile=1920x1080 bbox=(-1,-1)-(0,0) interesting_tile=1x1`

### Nuovo bug da investigare (sessione successiva)
- Con 2+ livelli il render a volte produce frame **nero** (intermittente)
- Con 3+ livelli il terzo livello quasi mai viene renderizzato
- In visualizzazione normale il 3° livello appare **sotto** il 2° (z-order invertito)
- Probabile causa: `TImageCombinationFx::doCompute` gestisce il livello più alto come
  "background" (render diretto sulla tile) e quelli sotto con `allocateAndCompute`.
  Se l'ordering dei port è invertito rispetto all'atteso, l'ordine di compositing
  è sbagliato. Da verificare in `binaryFx.cpp` e `scenefx.cpp` (`makePF`).

### Upstream candidate
- Il fix di `enlargeToI` è pulito e applicabile a Tahoma2D upstream: il commento
  originale diceva "the rect may become empty" ma non lo proteggeva. Fix corretto
  e backward-compatible.

---
## [2026-04-15b] — Diagnosi: render preview produce raster TRASPARENTE (bug Ztoryc-specifico)

### Modified
- `toonz/sources/toonz/sceneviewer.cpp` — `drawPreview()`:
  - Camera usata per `rasterToStageRef` cambiata da `scene->getCurrentCamera()`
    → `scene->getTopXsheet()->getStageObjectTree()->getCurrentCamera()` per
    allineare la camera a quella usata dal Previewer (in test erano già
    equivalenti 1920x1080, ma fix coerente con `Previewer::updateCamera()`).
  - Aggiunto logging diagnostico (ogni 60 frame): row, dimensioni camera
    root/sub, validità raster, pixel sample TL/Center/BR.
- `toonz/sources/toonz/previewer.cpp` — logging diagnostico in:
  - `updateCamera()`: cameraRes, renderArea, flag subcamera
  - `refreshFrame()`: previewRect, renderArea, motivi abort
  - callback render completed: dimensione raster + pixel centrale

### Notes — Scoperta chiave
Il raster **NON è bianco, è totalmente TRASPARENTE**:
```
[Previewer::renderCompleted] frame=0 rasSize=1920x1080 centerPix=(0,0,0,0)
[drawPreview] ras=valid rasSize=1920x1080 TL=(0,0,0,0) C=(0,0,0,0) BR=(0,0,0,0)
```

Tutti i pixel sono `RGBA=(0,0,0,0)` — alpha zero. Il viewer compone
il trasparente sopra `m_visualSettings.m_blankColor` (bianco di default),
**facendoci vedere bianco**.

Quindi il bug NON è:
- ❌ camera mismatch (root e sub entrambe 1920x1080)
- ❌ scheduling/trasporto (`refreshFrame` parte, `renderCompleted` firma,
  raster arriva valido al viewer con dim corretta)
- ❌ legato alle sub-scene (confermato dall'utente: succede anche
  renderizzando un disegno direttamente nel main xsheet)

**Il bug è Ztoryc-specifico**: la stessa scena aperta in Tahoma2D vanilla
renderizza correttamente. Una modifica di fork introdotta da Ztoryc rompe
il render preview → da bisettare rispetto a upstream `tahoma2d/tahoma2d`.

### Prossima sessione — piano concreto
1. **Diff con upstream Tahoma2D** — `git diff upstream/master -- toonz/sources/toonz/previewer.cpp toonz/sources/toonz/sceneviewer.cpp toonz/sources/common/tfx/` per vedere cosa Ztoryc ha toccato nel path render preview.
2. **Ricerca aree sospette**: `scenefx.cpp`, `trop.cpp`, `trasterfx.cpp`,
   qualsiasi modifica alla composizione `makeOver(bgCard, fx)`.
3. **Bisect**: se il diff è grande, `git bisect` partendo da un commit
   pre-animatic che funzionava. Candidati iniziali:
   - commit `ac5e46ca8` "Add storyboard/ztory sources" (potrebbe essere OK)
   - commit `35577720e` "ZtoryAnimaticController + dedicated TFrameHandle"
     (tocca TFrameHandle, area a rischio)
4. **Opus per analisi** — dato che il codice di rendering è denso, usare
   Claude Opus per leggere il diff upstream vs Ztoryc e identificare
   subito l'area rotta.

---
## [2026-04-15] — Indagine render preview bianco (bug ancora aperto)

### Modified
- `toonz/sources/toonz/previewer.cpp`:
  - `Previewer::Imp::buildSceneFx()`: cambiato `scene->getXsheet()` →
    `scene->getTopXsheet()` — il Previewer ora renderizza sempre dalla root
    xsheet anziché dalla sub-scene aperta. Fix corretto ma non sufficiente.
  - `Previewer::Imp::updateCamera()`: cambiato `scene->getCurrentCamera()`
    (che usava `getXsheet()`, tornando la camera della sub-scene aperta) →
    `scene->getTopXsheet()->getStageObjectTree()->getCurrentCamera()`.
    Camera del Previewer ora sempre allineata alla root xsheet.
  - Aggiunti include: `toonz/fxdag.h`, `toonz/tcolumnfxset.h`,
    `toonz/tstageobjecttree.h` (necessari per i fix).

### Notes — Bug render preview ancora aperto
Il render preview mostra bianco sia nel viewer animatico che in quello nativo.

**Investigazione effettuata:**
- Debug confermato: il FX tree è valido end-to-end:
  - Root xsheet: `cols=6, frameCount=214, termFxs=4` → `fxA=non-null`
  - Sub-scene: `subCols=1, subTermFxs=1, outputConnected=1` → `buildFx=non-null`
- Il render completa e `ras=VALID` (raster non-null restituito al viewer).
- `buildSceneFx()` in `scenefx.cpp` fa sempre `makeOver(bgCard, fx)` — quindi
  `fxA=non-null` non garantisce contenuto visivo (potrebbe essere solo bgCard).
- GL error 1286 (`GL_INVALID_FRAMEBUFFER_OPERATION`) pre-esistente, non causa
  del bianco (LUT non attiva, `lutValid=0`).

**Ipotesi ancora da verificare:**
1. La `drawPreview()` in `sceneviewer.cpp` usa ancora
   `scene->getCurrentCamera()` per calcolare `rasterToStageRef` — se la camera
   della sub-scene ha dimensioni diverse dalla root, l'immagine potrebbe essere
   mappata fuori dal viewport.
2. Il raster renderizzato potrebbe contenere effettivamente solo il colore
   sfondo (bianco) perché le sub-scene, pur avendo `termFxs=1`, non producono
   pixel visibili per qualche ragione ancora ignota (palette? DPI? blend mode?).
3. Il Previewer singleton potrebbe condividere cache tra viewer diversi in modo
   conflittuale.

**Prossima sessione — cosa fare:**
- Fixare `drawPreview()` in `sceneviewer.cpp` per usare la root xsheet camera
  nel calcolo di `rasterToStageRef`.
- Aggiungere debug mirato al valore dei pixel del raster renderizzato (es.
  `ras->pixels(0)[0]`) per capire se il contenuto è bianco o trasparente.
- Considerare di usare Opus per analisi più profonda.

---
## [2026-04-09] — Camera mismatch parziale fix + design room unificata

### Fixed
- **`getViewMatrix()` rimossa logica errata `getTopXsheet()`** (`sceneviewer.cpp`): il branch `if (m_alwaysMainXsheet)` in `getViewMatrix()` usava `getTopXsheet()` (camera root = identity) rendendo il viewer animatic cieco alle camera delle sottoscene. Rimosso: ora usa sempre `getCurrentXsheet()` + `TApp::getCurrentFrame()` (comportamento originale Tahoma2D). Il `m_customFrameHandle` resta solo per `drawScene()` dove serve il frame animatic per renderizzare la root xsheet al frame corretto.

### Notes
- **Bug aperto — Camera mismatch inside shot**: il mismatch tra viewer animatic e ComboViewer quando si è dentro uno shot persiste. La causa root è che Stage NON applica la camera della sottoscena quando renderizza dalla root xsheet (la camera sub-scene è applicata solo quando si è *dentro* la sottoscena). Il viewer animatic renderizza sempre la root xsheet, quindi non può applicare la camera delle singole sottoscene via `getViewMatrix()`. Richiede investigazione approfondita su Stage::visit() o una soluzione alternativa (e.g. quando si è dentro uno shot, il viewer animatic usa getCurrentXsheet() come shot viewer).
- **Design room unificata discussa**: proposta utente di room SHOT+ANIMATIC con toggle (QStackedWidget Left: Board↔XSheet, Center: AnimaticViewer↔ComboViewer, Right: Script+Inspector↔Palette+SmallViewer). Fasi: sprint 1 = toggle Left+Center + highlight giallo shot corrente.

---
## [2026-04-08] — Animatic viewer: marker indipendenti, camera view, real-time update

### Fixed
- **TSoundTrackP dangling pointer** (`viewerpane.h`): `m_sound` era `TSoundTrack*` raw — diventava dangling quando `m_mixedSound` veniva liberato da `invalidateSound()`. Fix: cambiato a `TSoundTrackP` (smart pointer). Null check aggiornati da `!= NULL` a `if (m_sound)`.
- **AutoFill combo non si popolava** (`tooloptions.cpp`): `m_controls` è indicizzato per `getName()` = `"Fill Style:"`, non per `getId()` = `"AutoFillStyle"`. Fix: corretti 3 punti in `tooloptions.cpp` (lookup, filter set, notifyToolComboBoxListChanged).
- **Mute/solo interferisce con ComboViewer nativo** (`viewerpane.cpp`, `ztoryanimatic.cpp`): quando sia il ComboViewer nativo che l'animatic viewer erano aperti, i rispettivi `play()` competevano per lo stesso `TSoundOutputDevice`. Fix quickfix: `ownsAudioAtMainLevel()` in `ZtoryAnimaticController` — il viewer nativo cede il controllo audio quando siamo a main level e l'animatic è aperto (gated su `isStoryboardWorkflow()`).
- **Marker animatic si spostavano entrando in uno shot** (`ztoryanimatic.cpp`, `ztoryanimatic.h`): `openSubXsheet()` sovrascriveva `XsheetGUI::setPlayRange()` con il range della sottoscena — storage singolo globale. Fix: range indipendente `m_animaticR0/m_animaticR1` in `ZtoryAnimaticController`; `updateFrameMarkers()` virtuale overridato in `ZtoryAnimaticViewer` per leggere sempre dallo storage proprio.
- **Camera animatic viewer non si aggiornava in real-time** (`ztoryanimatic.cpp`): aggiunto `objectChanged → m_sceneViewer->update()` in `showEvent()` — `objectChanged` si emette durante il drag interattivo di camera/peg, che era già connesso in `SceneViewer::showEvent()` ma non sopravviveva ai cicli disconnect delle sottoscene.

### Added
- **CAMERA_REFERENCE come default** (`ztoryanimatic.cpp`): l'animatic viewer si avvia in camera view — mostra l'inquadratura della sottoscena corrente senza che l'utente debba cambiare modalità manualmente.
- **`getViewMatrix()` usa root xsheet per animatic** (`sceneviewer.cpp`): quando `m_alwaysMainXsheet` è true, `getViewMatrix()` usa `getTopXsheet()` (camera del main) invece di `getCurrentXsheet()` (camera della sottoscena), evitando doppia applicazione della camera. Usa `m_customFrameHandle` per il frame animatic invece di `getCurrentFrame()` (che punterebbe alla frame della sottoscena).

### Notes
- **Bug aperto**: real-time update della camera mentre si edita all'interno di uno shot ancora da verificare dopo l'aggiunta di `objectChanged`. Il build `4f48e4da5` include il fix.
- **Design session in sospeso**: toggle Animatic↔ComboViewer rooms (architettura rooms definitiva).

---
## [2026-04-08] — Fix crash mute scene vecchie + antialias autofill + palette picker

### Fixed
- **Crash SIGABRT mute su scene vecchie** (`txshsoundcolumn.cpp`): `mixingTogether()` aveva `assert(soundLevel)` attivo in RelWithDebInfo — se l'audio file di una scena vecchia ha un riferimento rotto, `l->getSoundLevel()` ritorna null → assert → crash. Fix: sostituito con `if (!soundLevel) return mix`. Stesso fix in `getOverallSoundTrack()`: `overallSoundTrack->blank()` crashava se `TSoundTrack::create()` aveva lanciato un'eccezione (overallSoundTrack null). Fix: guard `if (!overallSoundTrack) return`. Aggiunto anche null check per `soundLevel` nel loop degli sound levels. Upstream candidate fix per Tahoma2D.
- **Bordino bianco tra linea e fill (antialias autofill)**: il BFS usava `getInk() != 0` come barriera (corretto), ma la fill condition richiedeva `getInk() == 0` — i pixel antialiased interni (ink>0, tone>0) venivano esclusi → gap bianco. Fix: rimossa condizione `getInk() == 0` → fill su tutti i pixel interni con `getPaint() == 0`. Per pixel puramente inchiostrati (tone=0) il paint viene settato ma il canale ink domina visivamente — nessun impatto.

### Added
- **AutoFill palette picker** (`toonzrasterbrushtool.h/.cpp`, `tooloptions.h/.cpp`): il combo "Fill Style" ora si popola dinamicamente con tutti gli stili della palette corrente (oltre a "Next Style (N+1)" e "Current Style"). Rebuild automatico quando cambia la palette o il livello. Ogni stile appare come `[N] NomeStile`. Selezione persistente tra un refresh e l'altro.

---
## [2026-04-08] — Fix crash audio mute + mute immediato durante play + AutoFill picker

### Fixed
- **Crash heap corruption su mute (scena con audio lungo)**: `makeSound()` con `fromFrame=-1, toFrame=-1` → `mixingTogether()` usava `getFrameCount()` inflato (durata file raw, potenzialmente ore) → buffer centinaia di MB → corruzione heap. Fix in `viewerpane.cpp`: bounded `prop->m_toFrame` al frame count delle sole colonne video (`maxFrame` da `col->getRange()`).
- **Crash heap corruption durante `refreshAudioTracks()`**: `restoreTrackStates()` chiamava `applyMuteSolo()` → `invalidateSound()` + restart audio device mentre ancora in play → corruzione. Fix: `restoreTrackStates()` ripristina solo stato UI (checked/unchecked), non tocca il device audio.
- **Null dereference in viewerpane.cpp**: `m_sound->getSampleRate()` chiamato prima del null check → spostato `if (!m_sound) return` prima del dereference.
- **Mute non ha effetto immediato durante play**: `applyMuteSolo()` chiamava `stopScrub()`/`play()` dal click handler, in race con i callback CoreAudio XPC → EXC_BAD_ACCESS. Fix: flag `m_pendingAudioRestart` settato da `applyMuteSolo()`, consumato in `onDrawFrame()` che viene chiamato dal Qt timer tra i callback XPC — contesto sicuro per `stopScrub()`/`play()`. Il mute ora è effettivo entro il prossimo frame (~40ms).
- **Mute/solo non persistente dopo `refreshAudioTracks()`**: stato salvato in `m_colMuted`/`m_colSolo`, ripristinato in `restoreTrackStates()`.

### Added
- **AutoFill fill style picker** (`toonzrasterbrushtool.h/.cpp`, `tooloptions.cpp`): nuovo `TEnumProperty m_autoFillStyle` con valori "Next Style (N+1)" (default, comportamento precedente) e "Current Style" (riempie con lo stile attualmente selezionato in palette). Il combo appare nella toolbar del brush tool accanto al checkbox AutoFill. Aggiunto anche `invalidate()` dopo autofill per aggiornare il canvas subito dopo mouseUp senza aspettare il prossimo hover.

### Notes
- Pattern sicuro per restart audio durante play: flag `m_pendingAudioRestart` → `restartAudioIfPlaying()` da `onDrawFrame()`.
- `stopScrub()`/`play()` sono sicuri solo se chiamati tra i callback CoreAudio XPC (Qt timer) — non dai click handler UI.

---
## [2026-04-06] — Board desync fix (merge/cut/delete), edit shot fix, durate panel, match button

### Fixed

- **3-shot merge lascia uno shot in più nel Board** (`storyboardpanel.cpp`, `onShotRemovedAt`):
  quando il secondo `shotRemovedAt` non trova lo shot per `data.xsheetColumn` (tracking
  desynced da operazioni precedenti), ora cade back su `refreshFromScene()` invece di
  tornare silenziosamente.

- **Edit shot button non selezionava lo shot** (`storyboardpanel.cpp`, `onEditShot`):
  aggiunto `selectShot(shotIdx)` prima di aprire la sottoscena.

- **Edit shot button usava board index come colonna xsheet** (`storyboardpanel.cpp`, `onEditShot`):
  ora usa `m_shots[shotIdx].data.xsheetColumn` — fix critico dopo merge/cut che desincronizzano
  indice Board dall'indice xsheet.

- **T: (durata totale) aggiornava panels[0].duration invece del display** (`onXsheetChanged`):
  per shot multi-panel questo sovrascriveva la durata parziale del panel 0 con la durata
  totale. Ora `onXsheetChanged` aggiorna solo il display T: per tutti i panel; D: (parziale)
  viene aggiornata solo per shot a panel singolo (dove D: == T:).

- **D: (durata parziale) includeva frame nascosti** (`detectAndUpdatePanels`):
  l'ultimo panel usava `numFrames` (frame count completo della sottoscena, inclusi frame
  oltre la durata visibile in timeline). Ora legge la durata visibile dalla colonna del
  main xsheet ancestor e cappa l'ultimo panel al limite timeline.

- **Panel oltre l'area visibile in timeline venivano mostrati nel Board**
  (`detectAndUpdatePanels`): aggiunto filtro — i panel con `startFrame >= timelineDuration`
  vengono esclusi dal Board.

### Added

- **Bottone ⇔ (Match Duration)** (`storyboardpanel.h/.cpp`, `PanelWidget`):
  ogni shot nel Board ha un piccolo bottone ⇔ accanto al campo T:. Quando cliccato,
  legge il `getFrameCount()` reale della sottoscena e ridimensiona la colonna nel main
  xsheet di conseguenza, poi chiama `resequenceXsheet()`. Consente di allineare la durata
  timeline alla durata effettiva della sottoscena.

### Notes

- `detectAndUpdatePanels` è chiamato dal `m_panelDetectTimer` (1000ms debounce) mentre
  si è dentro una sottoscena. Ora richiede un AncestorNode valido per calcolare
  `timelineDuration`; se l'ancestor non è disponibile, usa `numFrames` come fallback.
- Il bottone ⇔ è visibile in tutti i panel dello shot ma opera sempre sulla colonna
  dell'intero shot nel main xsheet.

---
## [2026-04-05] — Icone toolbar QToolButton, SVG Ztoryc, camera init sottoscene

### Modified

- **QPushButton → QToolButton in toolbar** (`storyboardpanel.h/.cpp`, `ztoryanimatic.cpp`):
  tutti i bottoni toolbar convertiti da QPushButton con testo a QToolButton con icone SVG
  via `createQIcon()`. Stile uniforme: `setFixedSize(28,28)`, `setIconSize(20,20)`,
  background trasparente, hover `#555`, checked `#666`.
  Connect aggiornati da `&QPushButton::clicked` a `&QToolButton::clicked`.

### Added

- **21 icone SVG Ztoryc** (`toonz/sources/toonz/icons/dark/ztoryc/`, `toonz.qrc`):
  `ztoryc_add_shot`, `ztoryc_delete_shot`, `ztoryc_merge`, `ztoryc_edit_shot`,
  `ztoryc_numbering`, `ztoryc_export_pdf`, `ztoryc_export_animatic`, `ztoryc_export_shots`,
  `ztoryc_select`, `ztoryc_razor`, `ztoryc_av_link`, `ztoryc_av_link_on`, `ztoryc_onion`,
  `ztoryc_onion_on`, `ztoryc_lock`, `ztoryc_lock_on`, `ztoryc_copy`, `ztoryc_clone`,
  `ztoryc_paste`, `ztoryc_shotedit`, `ztoryc_shotedit_on`, `ztoryc_refresh_preview`.
  Embedded nel binario via qrc. Toggle on/off gestiti automaticamente da `createQIcon`.

- **Camera init sottoscene** (`storyboardpanel.cpp`, `onAddShot()`): copia res e size
  dalla camera del main xsheet alla nuova sottoscena, stesso comportamento di
  `subscenecommand.cpp`. Risolve la piccola differenza di inquadratura tra sottoscena
  e main su scene create con Ztoryc.

### Removed

- `m_refreshButton` — rimosso da header, cpp e layout Board (refresh automatico
  con debounce già attivo).
- `m_backButton` — rimosso da header, cpp e layout Board (doppio click per tornare
  al Board già implementato).

### Notes

- Per aggiornare un'icona: sostituire il file SVG in `icons/dark/ztoryc/` e ricompilare.
  Se l'icona non cambia dopo la modifica al qrc: `ninja -C toonz/build -t clean` poi rebuild.
- Il bottone merge nel Board (`m_mergeButton`) è presente ma disabilitato
  (`setEnabled(false)`) — implementazione pendente come task aperto.
- Edit In Place deve essere **spento** quando si lavora sulla camera dentro uno shot.
  Con Edit In Place spento la camera locale funziona correttamente.
  L'audio del main si sente anche con Edit In Place spento — comportamento corretto.

---
## [2026-04-03] — Audio track L/M/S buttons, mute/solo fix, crash fix, cursor jump fix

### Fixed
- **Crash on mute (memory corruption of free block)**: `m_sound` (raw ptr in base
  `viewerpane.h`) was dangling after controller released `m_soundTrack` ref. Fixed by
  giving `ZtoryAnimaticViewer` its own `TSoundTrackP m_soundTrackRef` to keep the
  object alive until `refreshAnimaticSound()` replaces it. Removed the fragile
  `soundTrackInvalidating` signal approach.
- **Mute/Solo not updating during playback**: Mute handler was calling `setVolume()`
  directly without going through `applyMuteSolo()`, so solo state was ignored and
  `restartAudioIfPlaying()` was never called. Now both M and S delegate entirely to
  `applyMuteSolo()` via signals. `applyMuteSolo()` invalidates both TXsheet internal
  cache (`xsh->invalidateSound()`) and controller cache, then calls
  `restartAudioIfPlaying()` synchronously.
- **Solo logic**: Fixed `effectiveMute = muted || (hasSolo && !solo)` — M wins over S.
  Previously used `hasSolo ? !solo : muted` which gave wrong result when M+S both active.
- **`applyMuteSolo()` corrupting m_muted state**: Was calling `at->setMuted()` to
  apply solo overrides, which destroyed the user's own mute flag. Now uses separate
  `m_effectiveMuted` bool (set by `setEffectiveMuted()`) for visual dim only.
- **Cursor jumps right after audio cut/move**: `segmentMoved` lambda was calling
  `xsh->updateFrameCount()` which included long audio columns (trailing ColumnLevel
  with `endOffset=0` after razor cut = raw file length). Removed the call; animatic
  length is driven by video shots, not audio.
- **Selection not clearing on razor cut**: `m_selSeg` was never reset when razor was
  active (selection logic gated on `!m_razorActive`). Now cleared when razor fires.

### Added
- **L/M/S painted buttons** on audio track headers (horizontal row, 22×16px each).
  Pure paint approach — no QToolButton children (they don't render in custom-painted
  QWidgets on macOS).
- **Lock painted button** on video track header.
- **Waveform dim overlay** when track is muted (M) or solo-silenced — semi-transparent
  black rect over waveform area.
- **`m_effectiveMuted` flag** on `ZtoryAudioTrack`: tracks solo-silenced state
  separately from user's `m_muted`, so applyMuteSolo never corrupts user state.
- **`restartAudioIfPlaying()`** on `ZtoryAnimaticViewer`: rebuilds merged track and
  calls `mainXsh->play()` in-place (no stopScrub) so QAudioOutput hot-swaps data.
- **`ZtoryAnimaticController::setViewer/viewer()`**: lets the panel call
  `restartAudioIfPlaying()` on the viewer without a direct reference.

### Notes
- Audio update during play has ~100ms latency (QAudioOutput hardware buffer drain
  time) — same as DaVinci Resolve. Acceptable.
- M + S both active on same track: M wins (track is muted). Both S active: both play.

---
## [2026-04-01] — NLE audio track: zoom, edge trim, overlap, add track, cross-track

### Fixed
- **Razor audio split**: `splitAudioColumn` ripristinata a `splitLevelAtFrame` (nessun frame perso). `findSegments()` ora itera `ColumnLevel` direttamente (non celle xsheet) → segmenti razor indipendentemente selezionabili e trascinabili
- **Zoom/scroll audio lungo**: `updateTrackWidths()` calcola la larghezza totale includendo sia i blocchi video che i range audio — i file audio lunghi non vengono più tagliati
- **Cut lines fantasma**: cut lines ora mostrate solo dove c'è audio nel punto di taglio; aggiornate dopo ogni `segmentMoved` e `shotDurationChanged`
- **Cursore hover edge**: `SizeHorCursor` su hover pixel-based ai bordi segmento (non solo al click)

### Added
- **Edge trim segmenti audio**: drag bordo sinistro/destro per accorciare o allungare il segmento; commit via `modifyCellRange` (nessun frame audio perso all'interno del ColumnLevel)
- **Overlap prevention**: durante `SegmentDrag` il movimento è clampato contro i segmenti adiacenti per evitare sovrapposizioni nella stessa traccia
- **Add Audio Track**: context menu panel → inserisce nuova colonna sound vuota nell'xsheet
- **Cross-track segment move**: drag segmento fuori dalla traccia → drop su altra traccia; posizionamento preciso con `dragOffset`; clamp anti-overlap sulla traccia destinazione

### Modified
- `TXshSoundColumn`: `getColumnLevel`/`getColumnLevelCount` spostati da `protected` a `public`; aggiunti `detachLevelByFrame` e `adoptLevel` come API pubbliche
- `refreshAudioTracks`: rimosso check `sc->isEmpty()` per mostrare tracce audio vuote (necessario per Add Audio Track)

### Notes
- Cross-track drop: se la traccia destinazione ha segmenti sovrapposti, il clamp li evita ma può posizionare il segmento in modo non intuitivo — da migliorare in sessione futura con feedback visivo durante il drag

---
## [2026-04-01] — Fix SIGSEGV salvataggio TLV (libimage ABI mismatch)

### Fixed
- **Crash SIGSEGV salvataggio TLV** (`build_and_deploy.sh`): `libimage.dylib` nel bundle era un residuo di una build Debug precedente. `TLevelWriterTzl` (in `libimage`) leggeva `m_creator` a `this+0x48`, ma il nuovo `libtnzcore` RelWithDebInfo lo scrive a `this+0x50` (8 byte di differenza per layout di `TSmartObject`). Fix: aggiunto deploy di `libimage` con `install_name_tool` che patcha il rpath `libtiff` da `/usr/local/lib/libtiff.5.dylib` → `@executable_path/libtiff.5.dylib` (il path `/usr/local/lib` non esiste su questo Mac).

### Notes
- Root cause: `libimage` e `libtnzcore` devono essere sempre della stessa build. Qualsiasi cambio di build type (Debug/RelWithDebInfo/Release) richiede di ri-deployare `libimage`.
- `libpng` e `libjpeg` linkati via `/opt/homebrew` — risolvono correttamente a runtime.
- `libcolorfx` e `libtnzstdfx` NON deployate: dipendono da `libimage` ma non cambiano → usano quella nel bundle già aggiornata.

---
