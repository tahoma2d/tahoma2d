#ifndef PREFERENCESITEMIDS_H
#define PREFERENCESITEMIDS_H

enum PreferencesItemId {
  // General
  defaultViewerEnabled,
  rasterOptimizedMemory,
  autosaveEnabled,
  autosavePeriod,
  autosaveSceneEnabled,
  autosaveOtherFilesEnabled,
  startupPopupEnabled,
  undoMemorySize,
  taskchunksize,
  replaceAfterSaveLevelAs,
  backupEnabled,
  backupKeepCount,
  sceneNumberingEnabled,
  watchFileSystemEnabled,
  projectRoot,
  customProjectRoot,
  pathAliasPriority,
  showAdvancedOptions,
  tipsPopupEnabled,

  //----------
  // Interface
  CurrentStyleSheetName,
  additionalStyleSheet,
  iconTheme,
  pixelsOnly,
  oldUnits,
  oldCameraUnits,
  linearUnits,
  cameraUnits,
  CurrentRoomChoice,
  functionEditorToggle,
  moveCurrentFrameByClickCellArea,
  actualPixelViewOnSceneEditingMode,
  showRasterImagesDarkenBlendedInViewer,
  iconSize,
  viewShrink,
  viewStep,
  viewerZoomCenter,
  CurrentLanguageName,
  interfaceFont,
  interfaceFontStyle,
  colorCalibrationEnabled,
  colorCalibrationLutPaths,
  showIconsInMenu,
  displayIn30bit,
  viewerIndicatorEnabled,
  highDpiScalingEnabled,

  //----------
  // Visualization
  show0ThickLines,
  regionAntialias,

  //----------
  // Loading
  importPolicy,
  autoExposeEnabled,
  subsceneFolderEnabled,
  removeSceneNumberFromLoadedLevelName,
  IgnoreImageDpi,
  rasterLevelCachingBehavior,
  // initialLoadTlvCachingBehavior, // deprecated
  columnIconLoadingPolicy,
  levelFormats,  // need to be handle separately
  autoRemoveUnusedLevels,

  //----------
  // Saving
  rasterBackgroundColor,
  resetUndoOnSavingLevel,
  defaultProjectPath,
  recordFileHistory,
  recordAsUsername,

  //----------
  // Import / Export
  ffmpegPath,
  ffmpegTimeout,
  fastRenderPath,
  ffmpegMultiThread,
  rhubarbPath,
  rhubarbTimeout,

  //----------
  // Drawing
  DefRasterFormat,
  // scanLevelType,// deprecated
  DefLevelType,
  newLevelSizeToCameraSizeEnabled,
  DefLevelWidth,
  DefLevelHeight,
  DefLevelDpi,
  // AutocreationType,// deprecated
  EnableAutocreation,
  NumberingSystem,
  EnableAutoStretch,
  EnableImplicitHold,
  EnableCreationInHoldCells,
  EnableAutoRenumber,
  vectorSnappingTarget,
  saveUnpaintedInCleanup,
  minimizeSaveboxAfterEditing,
  useNumpadForSwitchingStyles,
  downArrowInLevelStripCreatesNewFrame,
  keepFillOnVectorSimplify,
  useHigherDpiOnVectorSimplify,

  //----------
  // Tools
  // dropdownShortcutsCycleOptions, // removed
  FillOnlysavebox,
  multiLayerStylePickerEnabled,
  cursorBrushType,
  cursorBrushStyle,
  cursorOutlineEnabled,
  levelBasedToolsDisplay,
  useCtrlAltToResizeBrush,
  temptoolswitchtimer,
  magnetNonLinearSliderEnabled,
  toolScale,

  //----------
  // Xsheet
  xsheetLayoutPreference,
  xsheetStep,
  xsheetAutopanEnabled,
  DragCellsBehaviour,
  pasteCellsBehavior,
  ignoreAlphaonColumn1Enabled,
  showKeyframesOnXsheetCellArea,
  showXsheetCameraColumn,
  useArrowKeyToShiftCellSelection,
  inputCellsWithoutDoubleClickingEnabled,
  shortcutCommandsWhileRenamingCellEnabled,
  showQuickToolbar,
  showXsheetBreadcrumbs,
  expandFunctionHeader,
  showColumnNumbers,
  unifyColumnVisibilityToggles,
  parentColorsInXsheetColumn,
  highlightLineEverySecond,
  syncLevelRenumberWithXsheet,
  currentTimelineEnabled,
  currentColumnColor,
  currentCellColor,
  levelNameDisplayType,
  showFrameNumberWithLetters,
  showDragBars,
  timelineLayoutPreference,

  //----------
  // Animation
  keyframeType,
  animationStep,
  modifyExpressionOnMovingReferences,

  //----------
  // Preview
  blanksCount,
  blankColor,
  rewindAfterPlayback,
  previewAlwaysOpenNewFlip,
  fitToFlipbook,
  generatedMovieViewEnabled,
  shortPlayFrameCount,
  inbetweenFlipDrawingCount,
  inbetweenFlipSpeed,

  //----------
  // Onion Skin
  onionSkinEnabled,
  onionPaperThickness,
  backOnionColor,
  frontOnionColor,
  onionInksOnly,
  onionSkinDuringPlayback,
  useOnionColorsForShiftAndTraceGhosts,
  animatedGuidedDrawing,

  //----------
  // Colors
  viewerBGColor,
  previewBGColor,
  useThemeViewerColors,
  levelEditorBoxColor,
  chessboardColor1,
  chessboardColor2,
  transpCheckInkOnWhite,
  transpCheckInkOnBlack,
  transpCheckPaint,

  //----------
  // Version Control
  SVNEnabled,
  automaticSVNFolderRefreshEnabled,
  latestVersionCheckEnabled,

  //----------
  // Touch / Tablet Settings
  // TounchGestureControl // Touch Gesture is a checkable command and not in
  // preferences.ini
  gestureUndoMethod,
  gestureRedoMethod,
  winInkEnabled,
  // This option will be shown & available only when WITH_WINTAB is defined
  useQtNativeWinInk,

  //----------
  // Others (not appeared in the popup)
  // Shortcut popup settings
  shortcutPreset,
  // Viewer context menu
  guidedDrawingType,
  guidedAutoInbetween,
  guidedInterpolationType,
  // OSX shared memory settings
  shmmax,
  shmseg,
  shmall,
  shmmni,
  //- obsoleted / unused members
  // interfaceFontWeight,
  // autoCreateEnabled,
  // animationSheetEnabled,
  // askForOverrideRender,
  // textureSize, // set to 0
  // LineTestFpsCapture,
  // guidedDrawingType,

  doNotShowPopupSaveScene,

  PreferencesItemCount
};

#endif
