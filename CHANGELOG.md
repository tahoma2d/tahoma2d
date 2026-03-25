# Ztoryc — Changelog

> **Come aggiornare (istruzioni per Claude Code):** dopo ogni sessione aggiungi
> una voce in cima con: data, `### Fixed`, `### Added`, `### Modified`,
> `### Upstream candidates`, `### Notes`. Poi esegui rsync (vedi AGENTS.md).
> Voci più vecchie di ~2 settimane → spostarle in `CHANGELOG_ARCHIVE.md`.

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

1. **Webcam feed perso**: il two-step switch ha introdotto una regressione.
   - Ipotesi: `changeCameras(0, None, 0)` rilascia risorse che non vengono
     riacquisite correttamente dalla seconda chiamata.
   - Alternativa da provare: bypass del two-step, tornare al comportamento originale
     ma aumentare il timeout del semaforo a 3-4s, oppure usare `dispatch_sync`
     sulla `sampleBufferCallbackQueue` prima di `stopRunning`.

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
