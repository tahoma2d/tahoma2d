# Ztoryc — Changelog

> **Come aggiornare (istruzioni per Claude Code):** dopo ogni sessione aggiungi
> una voce in cima con: data, `### Fixed`, `### Added`, `### Modified`,
> `### Upstream candidates`, `### Notes`. Poi esegui rsync (vedi AGENTS.md).
> Voci più vecchie di ~2 settimane → spostarle in `CHANGELOG_ARCHIVE.md`.

---
## [2026-03-31] — AutoFill flood-fill + Razor UX + splitAudioColumn + build/deploy fixes

### Fixed
- **Build deploy script** (`build_and_deploy.sh`): `libtnztools.dylib` mancante → tutte le modifiche a tooloptions.cpp e brush tool erano invisibili. Aggiunto al deploy. Rimossi `libimage`, `libcolorfx`, `libtnzstdfx` (rpath di sistema incompatibili). Aggiunto `libtnzbase.dylib` per ABI `TSmartObject::m_unknownClassCode`.
- **Build mode** → passato da Debug a RelWithDebInfo (`-O2 -DNDEBUG`): eliminati SIGABRT da `assert()` in tcomputeregions.cpp (vettoriale) e altri assert attivi solo in debug.
- **AutoFill crash con onion skin** (`autofilltlv.cpp`): guard `if (tot_pix == 0) return;` prima della divisione `pbx/tot_pix` in `autofill_learn`.
- **AutoFill save crash** (`toonzrasterbrushtool.cpp`): rimosso `return` errato che saltava `m_workingFrameId = TFrameId()` e `m_strokeRect.empty()` lasciando stato corrotto al salvataggio successivo.
- **Razor audio gap** (`ztoryanimatic.cpp`): `splitAudioColumn` era no-op per design (separatore solo grafico). Ora **reimplementata** come split reale via `TXshSoundColumn::splitLevelAtFrame` — i due segmenti sono fisicamente separati e possono essere shiftati indipendentemente dal ripple.
- **Audio drag/ripple** (`txshsoundcolumn.cpp`): aggiunto `splitLevelAtFrame()` come metodo pubblico: taglia un `ColumnLevel` in due parti (`startOffset`/`endOffset` ajustati) senza creare gap audio né colonne extra. Dopo il taglio `shiftLevelFromFrame` shifta solo la parte destra.

### Added
- **AutoFill flood-fill** (`toonzrasterbrushtool.cpp`): rimosso approccio onion-skin (`autofill_learn/apply`). Nuova implementazione BFS inverso: dal bordo del savebox+1px si esplorano tutti i pixel non-ink raggiungibili dall'esterno; i pixel chiusi (non raggiungibili e non-ink) vengono riempiti con lo stile N+1 della palette (convenzione ink=N, fill=N+1).
- **Razor hover preview** (`ztoryanimatic.cpp/.h`): linea gialla tratteggiata sincronizzata su tutte le tracce audio e video durante hover con razor attivo.
- **Separator lines audio track** (`ztoryanimatic.cpp/.h`): `ZtoryAudioTrack::setCutFrames()` disegna linee bianche ai boundary dei blocchi video; `setRazorHoverFrame()` per sync hover cross-track.

### Modified
- **AutoFill BFS perf** (`toonzrasterbrushtool.cpp`): `std::vector<bool>` → `std::vector<uint8_t>` + indici flat + BFS limitato al savebox → esecuzione sub-millisecondo su canvas normali.
- **Fill color** (`toonzrasterbrushtool.cpp`): autofill usa stile palette corrente+1 (ink=N → fill=N+1), fallback a N se N+1 non esiste.

### Notes
- **Crash SIGSEGV salvataggio TLV** (`libimage.dylib`): crash in `TLevelWriterTzl::TLevelWriterTzl` → `QString copy` con puntatore corrotto. Non è nel codice Ztoryc — sospetta incompatibilità ABI tra `libtnzcore` (nostro RelWithDebInfo) e `libimage` originale bundle. Da investigare: deploy `libimage` con rpath fixato, oppure identificare quale struttura viene corrotta.
- **AutoFill comportamento**: ancora non "istantaneo al chiudersi della forma" — il fill avviene al `leftButtonUp`, non live durante il drag. Questo è by design attuale; fill live richiederebbe BFS ad ogni `mouseDrag` (costoso).

---
## [2026-03-28] — Audio-master clock + audio in sotto-scena + auto-marker Out + RecentFiles fix

### Added
- **Audio-master clock** (`tsound_qt.cpp`, `tsound.h`, `tsound_nt.cpp`, `txsheet.h/.cpp`, `ztoryanimatic.h/.cpp`): `QAudioOutput::processedUSecs()` esposto come `TSoundOutputDevice::processedUsecs()` → `TXsheet::getAudioPlayedUSecs()`. In `ZtoryAnimaticViewer::onDrawFrame` il frame video è calcolato dal clock DAC hardware invece che dal timer FlipConsole: `targetFrame = m_playStartFrame + floor(audioUsecs * fps / 1e6)`. Fallback a frame FlipConsole durante warmup DAC. Qualità sync pari a software editing professionale.
- **Audio main xsheet da dentro la sotto-scena** (`ztoryanimatic.h/.cpp`): `ZtoryAnimaticController` monitora `TApp::getCurrentFrame()->isPlayingStatusChanged` e `frameSwitched`. Quando il native viewer è in playback o scrub dentro un sub-xsheet (`ancestorCount == 1`), calcola `mainFrame = childStack->getAncestor(subFrame).second` e fa partire l'audio del main xsheet alla posizione corretta. Guard: nessuna interferenza se l'animatic viewer sta già gestendo l'audio.
- **Auto-aggiornamento marker Out nel sub-xsheet** (`ztoryanimatic.cpp`): in `onShotDurationChanged`, dopo `resequenceXsheet()`, chiama `ztorySetShotRange(col, 0, newDuration-1)` per aggiornare `s_frameRangeMap`. Se il sub-xsheet è aperto in quel momento aggiorna anche la play range live via `XsheetGUI::setPlayRange`.

### Fixed
- **SIGABRT al lancio** (`build_and_deploy.sh`): `tsound_qt.cpp` è compilato in `libtnzcore` (non `libsound`), quindi il simbolo `processedUsecs()` era assente a runtime. Fix: aggiunto `cp tnzcore/libtnzcore.dylib` nel deploy script. Corretta anche annotazione `libsound` nel commento.
- **`QMutexLocker` in metodo const** (`tsound_qt.cpp`): `m_mutex` dichiarato `mutable` per consentire lock in `processedUsecs() const`.
- **RecentFiles.ini deadlock Cocoa/Qt** (`mainwindow.cpp`): `loadRecentFiles()` ora chiama `saveRecentFiles()` al termine. Il file gonfio viene riscritto compatto al primo avvio, eliminando il deadlock su aperture successive.

### Modified
- `build_and_deploy.sh`: aggiunto `cp tnzcore/libtnzcore.dylib` e `cp sound/libsound.dylib`; aggiornati commenti.

### Notes
- Task 16 (audio-master clock) ✅ completato
- Task B/A (audio in sotto-scena + scrub) ✅ completato
- Task C (auto-marker Out sub-xsheet) ✅ completato
- Task 15 (RecentFiles deadlock) ✅ completato

---
## [2026-03-26c] — AV-link multi-segmento + audio play sync

### Fixed

- **AV-link multi-segmento non funzionava** (`txshsoundcolumn.h/.cpp`, `ztoryanimatic.cpp`):
  `shiftLevelInRange(r0, r1, delta)` cercava ColumnLevel con vsf ∈ [r0, r1] — falliva per:
  1. Tracce audio non tagliate (vsf=0, non in nessun intervallo shot)
  2. Tagli multipli: il secondo segmento (es. vsf=89 per Shot B) non veniva trovato
     quando si processava Shot C con range [146,200]
  Fix: aggiunto `shiftLevelFromFrame(fromFrame, delta)` che sposta tutti i ColumnLevel
  con vsf ≥ fromFrame in un'unica chiamata. `resequenceXsheet()` ora trova il primo
  shot con delta≠0 e chiama `shiftLevelFromFrame` una volta sola per tutto l'audio.

- **Audio-video desync al play (3 cause, risolte tutte)**:
  1. **Buffer enorme dalla razor cut**: il ColumnLevel tail aveva `endOffset=0` →
     `visibleEndFrame` = intera lunghezza del file audio grezzo (potenzialmente ore).
     `makeSound()` creava un track di quella lunghezza. `play(st, s0, rawEnd)` faceva
     `m_buffer.resize(centinaia_MB) + memcpy` bloccando il main thread 1-3 s mentre
     il `PlaybackExecutor` girava già su un altro thread → video skippava frame →
     desync. **Fix**: cap `endSample = min(animaticFrames × spf, totalSamples-1)`.
  2. **makeSound() ad ogni play-start**: `refreshAnimaticSound()` chiamava `makeSound()`
     ogni volta che si premeva play. Ora usa `ctrl->requireSoundTrack()` (cached).
  3. **Per-frame buffer replacement**: ogni frame chiamava `play(st, s0, s1)` che
     sovrascriveva `m_buffer` con 1 frame di audio → micro-glitch continui.
     Fix: streaming continuo — al play-start si chiama `play(st, startSample, animEnd)`
     una volta sola; per-frame `playAnimaticAudioFrame` è no-op durante il play.

- **SIGABRT razor tool audio** (sessione precedente, confermato risolto):
  `delete at` dentro `refreshAudioTracks()` durante evento del widget → `deleteLater()`.

### Added

- `TXshSoundColumn::shiftLevelFromFrame(int fromFrame, int delta)` — nuovo metodo
  pubblico in `txshsoundcolumn.h/.cpp`.
- Pre-warmup `QAudioOutput` in `ZtoryAnimaticViewer::showEvent` via `QTimer::singleShot(0)`:
  inizializza il device prima che l'utente prema play → zero startup latency.
- `bool m_continuousPlay` in `ZtoryAnimaticViewer` per coordinare play/scrub.

### Notes

- Il sync A/V è sensibilmente migliorato ("molto meglio" dall'utente). Residui di drift
  su animatici molto lunghi sono intrinseci all'architettura (video timer indipendente
  dall'audio clock). Fix definitivo richiederebbe esporre `QAudioOutput::processedUSecs`
  da `TSoundOutputDevice` e usarlo come master clock in `onDrawFrame`.

---
## [2026-03-26b] — RecentFiles cap, merge camera fix, workflow path bug

### Fixed

- **RecentFiles hang (Task 15)** (`mainwindow.cpp`): `loadRecentFiles()` ora cappa
  ogni lista a `maxSize=10` tramite `qMin()` — previene deadlock Cocoa/Qt se il file
  su disco è cresciuto oltre il limite. `addFilePath()` usa `while` invece di singolo
  `removeAt` — trim corretto anche se la lista era già gonfia in memoria.

- **Merge camera keyframe giunzione (Task 6f)** (`ztoryanimatic.cpp`):
  `mergeChildXsheetContent` eseguiva `addRazorKeyframes(dstOffset)` DOPO la copia
  delle srcCam keyframes, sovrascrivendo il valore corretto con quello interpolato
  del primo shot. Fix doppio:
  1. `addRazorKeyframes(dstOffset)` spostato PRIMA della copia camera.
  2. Dopo la copia, inserito esplicitamente `kfs.begin()->second` a `dstOffset` —
     garantisce che la key di giunzione abbia i valori della prima key del secondo shot
     anche se questa non è a frame 0.

- **`ensureStoryboardRoomsTemplate` path bug** (`mainwindow.cpp`): la funzione usava
  `getMyRoomsDir()` (basato sul workflow corrente, non ancora aggiornato) invece di
  costruire il path direttamente da `choice`. Poteva controllare la directory del
  workflow sbagliato (es. Tradigital) e triggerare una migrazione errata.

### Modified

- Debug prints rimossi: `[ZTORY]`, `[RS]`, backtrace, `#include <execinfo.h>`
  da `mainwindow.cpp` (aggiunti nella sessione precedente per diagnostica).

### Notes

- Crash intermittente su workflow switch (empty room → SIGSEGV) non riprodotto
  dopo i fix. Causa root non identificata — da monitorare.
- Task 6f camera confermato funzionante dall'utente.
- Task 15 RecentFiles confermato (file .bak2 da 2.5 MB era la causa del hang precedente).

---
## [2026-03-26] — Fix hang su workflow switch (RecentFiles.ini + guard rientranza)

### Fixed

- **Hang ~1 min su ogni workflow switch** (mainwindow.cpp): causa doppia:
  1. `RecentFiles.ini` ricresciuto a 2.5 MB — causa deadlock nel plugin Cocoa Qt
     durante `readSettings()` → `loadRecentFiles()`. Fix immediato: reset del file
     (backup in `.bak2`). **Fix permanente da implementare (PRIORITARIO)**: limitare
     le entry in `RecentFiles.ini` a max ~50 voci (era già in TODO da sessione 2026-03-24).
  2. `MI_Workflow2D` QAction postata (queued via `sendPostedEvents`) durante caricamento
     Storyboard rooms, processata dopo il ritorno di `onWorkflowStoryboard` →
     secondo `switchRoomChoice("Tradigital")` → secondo `readSettings` lento.
     Fix: `m_isHandlingWorkflow` flag con reset differito via `QTimer::singleShot(0)`
     in tutti e 4 i workflow handler (`onWorkflowStoryboard`, `onWorkflow2D`,
     `onWorkflowCutout`, `onWorkflowStopMotion`).

### Added

- `m_isHandlingWorkflow` flag in `MainWindow` — guard rientranza per workflow switch.
- `#include <execinfo.h>` + backtrace debug in `onWorkflow2D` (da rimuovere).
- Debug prints `[RS]` in `readSettings` loop room-by-room (da rimuovere).

### Notes

- Il backtrace ha rivelato: `DVAction::onTriggered` → `QAction::activate` via
  `sendPostedEvents` — probabilmente un panel nelle Storyboard rooms ha una
  `QAction`/`QToolButton` per `MI_Workflow2D` che viene attivata al primo show.
  Root cause non rimosso — il guard è il fix definitivo.
- Debug prints da rimuovere nella prossima sessione dopo conferma stabilità.

---
## [2026-03-25b] — Merge keyframes, audio track interactions, audio import guard

### Added
- **Merge with keyframes (6f)**: `onMergeShots()` e `onMergeWithNext()` ora inseriscono
  keyframes su tutte le colonne + camera ai bordi dei segmenti uniti via `addRazorKeyframes`.
  `copyChildXsheetFrames` copia anche le keyframes degli stage objects dal source al
  destination con offset.
- **Audio import guard**: In `IoCmd::loadResources()`, file audio bloccati nelle sottoscene
  quando `ZtoryModel::isStoryboardWorkflow()` è true. Nuovo metodo che verifica sia gli
  shot in memoria sia l'esistenza del file `.ztoryc`.
- **Audio track interattiva**: `findSegments()` identifica segmenti contigui. Click per
  selezionare (highlight blu), drag per spostare, Ctrl+X/C/V per clipboard, Delete per
  cancellare. Separatori rossi semitrasparenti tra segmenti dopo split.
- **Razor AV-linked in-place**: `splitAudioColumn()` ora cancella solo la cella al punto
  di taglio invece di creare una nuova colonna/traccia.
- **Audio-video link su resequence**: `resequenceXsheet()` salva posizioni prima/dopo e
  applica lo stesso delta ai segmenti audio associati (per midpoint overlap).

### Known bugs (da risolvere nella prossima sessione)
- **Merge keyframes confuse**: `copyChildXsheetFrames` copia nelle stesse colonne del dst
  xsheet, ma gli shot uniti dovrebbero avere colonne separate nella child xsheet di
  destinazione per evitare che le keyframes si confondano e i movimenti vengano sfalsati.
  **Fix necessario**: copiare il contenuto dei source shot in NUOVE colonne nel dst xsheet
  (appendere colonne, non sovrascrivere le stesse).
- **Audio guard falso positivo**: `isStoryboardWorkflow()` controlla l'esistenza del file
  `.ztoryc` ma non distingue quando l'utente cambia workflow (es. da storyboard a
  tradigital). Serve un flag esplicito sul workflow attivo, non basato sul file.
- **Audio drag crash (segfault)**: `TXshSoundColumn` gestisce l'audio con `ColumnLevel`
  (startFrame, startOffset, endOffset) — le operazioni generiche `clearCells`/`setCell`
  non sono sicure sulle sound columns. Il drag audio deve usare le API specifiche di
  `TXshSoundColumn` e `ColumnLevel` per spostare i segmenti.
- **Audio sparisce su resize con AV link**: `onShotDurationChanged` shifta l'audio PRIMA
  del resequence, poi `resequenceXsheet()` (versione audio-linked) lo shifta DI NUOVO.
  **Fix**: rimuovere il doppio shift — fare lo shift solo in uno dei due posti.

---
## [2026-03-25] — Stability fixes: re-entrancy guards, razor crash, duplicate crash, webcam UX

### Fixed

- **Razor SIGSEGV** (ztoryanimatic.cpp:1957): in-place cell-shift sul clone child xsheet
  causava crash per realloc del vettore interno. Fix: buffer temporaneo per raccogliere
  le celle sorgente prima di svuotare e riscrivere la colonna.
- **Stack overflow / hang** (ztoryanimatic.cpp): `refreshFromScene` e `refreshAudioTracks`
  potevano rientrare durante `insertWidget` (Qt processa eventi queued). Aggiunto
  `m_refreshing` e `m_refreshingAudio` come guard di rientranza.
- **DuplicatePopup SIGABRT** (txsheet.cpp:829): `assert(upTo >= r1 + 1)` sostituito con
  `if (upTo < r1 + 1) return;` — crash quando upTo era dentro il range selezionato.
- **Webcam switch two-step UX** (stopmotioncontroller.cpp): eliminato il reset a
  "Select Camera"; ora mantiene la selezione dell'utente e riconnette con QTimer(300ms).
- **Razor child xsheet trim** (ztoryanimatic.cpp): il sotto-xscene del primo shot
  ora viene correttamente trimmato dopo il taglio (i frame in eccesso venivano mantenuti).
- **Workflow StopMotion room** (mainwindow.cpp): ripristinato `switchRoomChoice("StopMotion")`
  e corrette le room case-sensitive ("Capture" non "CAPTURE").

### Notes

- Bug vettoriale (crash disegno vettoriale) confermato come problema preesistente
  Tahoma2D (issues #1545, #1712, #1715, #1826) — non collegato a Ztoryc.
- Nuovi task aperti per prossima sessione: merge con keyframe camera/colonne,
  load audio solo da main xsheet, razor audio linked/unlinked, traccia audio
  selezionabile/draggabile con cut-copy-paste.

---
## [2026-03-24] — Razor fix (parziale), webcam two-step switch (regressione), RecentFiles bug

### Fixed

- **App non si avviava** (`RecentFiles.ini` 10.4 MB): il plugin Cocoa di Qt 5.15 su macOS 26.3.1
  si bloccava con deadlock `_os_lock_handoff_lock_slow` nel costruire il menu File → Recenti.
  Fix: reset di `stuff/profiles/users/francobianco/RecentFiles.ini` (backup in `.bak`).
  Causa strutturale: il file accumula entry illimitatamente. Soluzione permanente: limitare
  `DVMenuAction` a max 20 item o svuotare periodicamente.

### Modified (parziale — bugs aperti)

- **Razor `onRazorRequested`** (`ztoryanimatic.cpp`):
  - Aggiunto helper `addRazorKeyframes(TXshChildLevel*, int frame)` che chiama
    `setKeyframeWithoutUndo` su tutti i column stage object + camera(0) della child xsheet.
  - Aggiunto fix della child xsheet del clone: shift frame `splitRel..N → 0..secondHalf`.
  - **BUG ANCORA APERTO:** la prima parte dello shot mantiene la lunghezza originale
    (i frame in coda dopo il taglio non vengono rimossi correttamente).
  - **Keyframe da rifare:** l'utente richiede "Insert Multiple Keys" (su TUTTE le colonne
    della sub-scene) sia nel frame PRECEDENTE al taglio che nel frame DEL taglio nel nuovo shot,
    PIÙ una chiave specifica sulla colonna camera in entrambi i punti.

- **Webcam two-step switch** (`stopmotioncontroller.cpp`):
  - Tentativo: quando si switcha direttamente da camera A → B, prima chiamare
    `changeCameras(0, None, 0)` poi con `QTimer::singleShot(800ms)` chiamare la nuova camera.
  - **REGRESSIONE:** il feed video non arriva più dopo il fix. Da investigare se il problema
    è nel timing (800ms troppo poco?) o se `changeCameras(0, None)` rompe lo stato interno.

### Bug aperti da risolvere nella prossima sessione

1. **Webcam switch**: il two-step ha introdotto regressione (feed perso).
   - **Soluzione approvata:** forzare il passaggio obbligato attraverso `selectCamera()`
     prima di ogni switch. Osservazione: se l'utente clicca manualmente "Select Camera"
     tra uno switch e l'altro, il cambio funziona perfettamente. Quindi invece di
     replicare il cleanup in modo asincrono, chiamare automaticamente `selectCamera()`
     (o il codepath equivalente) come primo step nel handler di cambio camera.
     Comportamento risultante: ogni switch fa sempre SELECT → INIT, mai INIT diretto.
   - Revertire il two-step e il QTimer. Non usare dispatch_async né semafori.

2. **Razor prima parte troppo lunga**: le celle in coda all'originale non vengono
   cancellate. Verificare che `clearCells(splitFrame..r1, col)` venga eseguito
   DOPO `cloneChild` e che `r1` sia aggiornato dopo il clone.

3. **Razor keyframe corretti**: usare `ColumnCmd` o API equivalente per
   "Insert Multiple Keys" (non solo `setKeyframeWithoutUndo` sul singolo stage object
   ma su TUTTE le colonne della sub-scene). Keyframe nei due punti:
   - Frame `splitRel - 1` (ultimo frame originale): Insert Multiple Keys + chiave camera
   - Frame `0` del clone (primo frame nuovo shot): Insert Multiple Keys + chiave camera

---
## [2026-03-24] — Webcam AVCapture: deadlock durante switch camera (WIP)

### Added (sessione separata — modulo cattura Stop Motion)

- **AVCaptureSession live view su macOS** in `webcam.cpp/.h`:
  - Classe `ZtoryAVDelegate`, metodi `initWebcamAVCapture`, `releaseWebcamAVCapture`,
    `getWebcamImageAVCapture`. La cattura live funziona.

### Bug aperto — DEADLOCK switch camera

- **Sintomo:** app si pianta switchando camera (es. Iriun → Insta360) durante il live view.
- **Causa:** `releaseWebcamAVCapture` chiama `[session stopRunning]` (sincrono) che aspetta
  che il delegate finisca. Il delegate usa `QMutex::tryLock(5)` su `m_avFrameMutex`. Se il
  main thread tiene il mutex → deadlock.
- **Tentativo fallito:** flag `active` (BOOL property) sul delegate, impostato a NO prima
  di `stopRunning` — non basta perché `stopRunning` blocca comunque.
- **Soluzioni da provare:**
  1. `dispatch_async` su queue secondaria per `[session stopRunning]` (non bloccante)
  2. Oppure `std::atomic<bool>` per il flag active + `stopRunning` su thread separato
  3. Il delegate deve usare SOLO `tryLock`, mai `lock`
- **Test:** aprire Stop Motion, selezionare Iriun, start live view, switchare a Insta360
  ripetutamente — non deve piantarsi, deve mostrare il feed corretto.
- **File:** `toonz/sources/stopmotion/webcam.cpp` e `webcam.h`
- **Build:** `./build_and_deploy.sh toonz/sources/stopmotion/webcam.cpp`

---
## [2026-03-24] — Fix deployment: TLV crash riapparso e risolto

### Fixed

- **TLV salvataggio crash riapparso** — causa: `lzocompress`/`lzodecompress` mancanti
  dal bundle dopo rebuild eseguito con ninja diretto invece di `build_and_deploy.sh`.
  Fix: ripristinati i binari in `Tahoma2D.app/Contents/MacOS/`.

### Modified

- **`AGENTS.md` sezione Build & Run** — aggiunto avviso obbligatorio:
  usare SEMPRE `./build_and_deploy.sh`, mai ninja diretto prima di aprire l'app.

### Notes

- Bug ricorrente: ogni rebuild pulito sovrascrive il bundle senza copiare i binari helper.
  `build_and_deploy.sh` risolve automaticamente — usarlo sistematicamente.

---
## [2026-03-21] — Onion skin ruler: due strip indipendenti (FOS + MOS)

### Modified

- **`ZtoryAnimaticRuler`**: altezza 36px; suddivisa in tre zone:
  - Striscia **F** (top 9px) — FOS (fixed-frame, punto arancione)
  - Zona centrale — ticks, numeri, playhead, In/Out
  - Striscia **R** (bottom 9px) — MOS (relativi alla playhead)
- Stato locale `m_localMask` indipendente dalla timeline nativa
- Bottone Onion wired a `m_ruler->setOnionEnabled()`

---
## [2026-03-21] — Startup popup fixes + animatic onion skin

### Fixed

- StartupPopup: QComboBox per workflow (4 voci), shot numbering visibile solo per Storyboard
- Shot di default rimosso al create; Out-marker posizione `(r1+1)*ppf`
- `closeEvent` blocca chiusura se scena untitled; rimosso "Save Untitled?" spurio
- **TAHOMA2DROOT crash**: `SystemVar.ini` mancante da `Contents/Resources/` — ripristinato

---
## [2026-03-21] — Task 13a + Task 14: onion skin markers + startup dialog

### Added

- **Task 13a** — Onion skin markers su `ZtoryAnimaticRuler` (triangolini semi-trasparenti)
- **Task 14** — `ZtoryStartupDialog` in `ztorystartup.h/cpp`:
  - 4 sezioni: Project, Camera, Workflow, Shot Numbering
  - Preferences in `QSettings("Ztoryc", "StartupDefaults")`
  - Integrato in `iocommand.cpp::newScene()`
  - `ZtoryModel::addShotNamed(QString)` — crea colonna + sub-scene + model entry
  - Registrato in `CMakeLists.txt`

### Notes

- Tasks 4, 3, 2, 13c già implementati in sessioni precedenti.

---
## [2026-03-21] — Double-click su background Board chiude sub-scene

### Fixed

- `StoryboardPanel::mouseDoubleClickEvent()`: quando `ancestorCount > 0` esegue
  `MI_CloseChild`. Gesto coerente con il warning "exit edit mode first".

---
## [2026-03-21] — Task 1b + Task 13b fixes + Task 12b scrubbing

### Fixed

- **Task 1b:** `ZtoryModel::assertMainXsheet(bool showWarning)` — guard su `addShot`,
  `removeShot`, `cloneShot`, `moveShot`. Guard anche in `StoryboardPanel::onMoveShot`.

- **Task 13b fix 1:** marker IN/OUT sempre visibili (rimossa dipendenza da `isPlayRangeEnabled`)
- **Task 13b fix 2:** playhead non si sposta con tasto destro / Shift / Alt
- **Task 13b fix 3:** context menu ruler — "Mark IN here", "Mark OUT here",
  "Set OUT to last frame", "Reset IN/OUT to full range"

- **Task 12b:** audio scrubbing con `xsh->play(st, s0, s1)` usando `ctrl->soundTrack()`

### Notes

- Task 12c (preview bar audio) già implementato in sessione precedente.
- OUT marker nasce a frame 0 per scene nuove — usare "Set OUT to last frame" dal menu.
