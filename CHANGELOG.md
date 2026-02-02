# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.6.0] - 2026-02-02

### Added

- Brush Tips (Raster/Smart Raster) [#1915]
- Stylus Settings Popup (Raster Brush & MyPaint styles) [#1888, #1923, #2018]
- Stylus Settings Popup (Smart Raster - Standard Brush, Vector) [#1898, #2018]
- Column Alpha Lock [#1666, #1736]
- Loop Frames [#1821, #1970]
- Add Raster Tool Option - Paint Behind [#1793, #1881, #1802]
- Smart Raster Selection - Selective mode [#1797, #1817]
- Add marker and soft round vector styles. [#1983]
- Contiguous cell selection shortcut [#1812]
- Show image in cell tooltip [#1783]
- Special Modifier Key: Viewer Scrub [#1722]
- Canon SDK for Linux [#1704]

### Changed

- MyPaint Brush Tilt Support [#1836]
- New and updated shortcuts [#1805]
- Refactored and Improved Preproduction Board [#1953]
- Reverse Clipping Mask Stacking Placement [#1667]
- Rhubarb Recognizer option [#1835]
- Shift markers with Cell Insert/Delete [#1970]
- Add color and fill support to raster pattern brush. [#1931]
- Allow Touch Screen zoom and rotate combo [#1834]
- List All GPhoto config button and critical error handling [#1962]
- Enhance input validation and clamping in DoubleValueField (OpenToonz port) [#1946, #1961]
- Existing scenes list in Startup Popup (OpenToonz Port) [#1930, #1964, #1954]

### Fixed

- SVG load fixes [#1995]
- Add missing Hook Tool status bar description [#1926, #1933]
- Autoclose: Limit Two Points Spot Research Angle to 0°~36° [#1901]
- Capitalize Spanish Fx doc directory [#1984]
- Compress high frequency MyPaint drawing events [#1921]
- Fix backing up of scene icon [#2026]
- Fix Capture Image using Canon EDSDK Linux [#1977]
- Fix changing geometry opacity back to 100 [#1919]
- Fix column status toggling and display of folder items [#2008]
- Fix Export Vector Level Bad Resolution Crash [#1920]
- Fix File Browser New Folder creation [#1948]
- Fix File Browser Rename logic and UI [#1947]
- Fix flipped vector color picker [#2022]
- Fix image capture default folder [#1978]
- Fix key as last scene frame [#1976, #1999]
- Fix last Stop Frame Hold [#2006]
- Fix load svg crash [#1925]
- Fix loading unknown style id [#2012]
- Fix missing Checkboard Fx settings (Linux) [#1922]
- Fix missing MSVC DLLs [#1938]
- Fix missing QString arg [#2000]
- Fix missing viewer title bar level info [#1949]
- Fix reading corrupted PNG crash [#1982]
- Fix saving escaped apostrophe (linux) [#1928]
- Fix Show Column Parent's Color context menu option [#1924]
- Fix some Vector Inspector issues + some refactoring [#1996]
- Fix sound cells extender controls and move selection [#2014]
- Fix starting in unicode path [#2009]
- Fix stop frame placement on OCA import [#2010]
- Fix styleeditor not updating on colorswatch switch [#2020]
- Fix SVG stroke-width at lower resolutions [#2004]
- Fix to stop writing unfilled regions to SVG [#2007]
- Fix vector fill loss due to ghost grouping changes [#1997]
- Fix Vector Ungroup/Group undo issue [#2002]
- Fix Zerary Fx input preview status in Schematic [#1927]
- Fix(autoclose): Add intersection check for spotRearchTwoPoints [#1934]
- Fixes for Preproduction Board and File Browser [#1950, #1964]
- Force startup tips window to the top [#1932]
- Restore disabled file writer types [#1936]
- Fix bounds issue [#1941]
- Fix brush size input for non-English locales (OpenToonz port) [#1946]
- Fix color wheel pointer and replace layout margins (OpenToonz port) [#1946[^11]]
- Fix dummy ink pixel (Tone =255) ink texture crash (OpenToonz port) [#1946]
- Fix memory leaks by replacing 'delete' with 'delete[] (OpenToonz port) [#1946]
- Fix Mesh Memory Leak (OpenToonz port) [#1946[^8]]
- Fix New Folder refresh issue in File Browser (OpenToonz port) [#1946]
- Fix order of beginInsertRows/beginRemoveRows in ExportSceneDvDirModel (OpenToonz port) [#1946]
- Fix registering multiple system var paths with command line qualifiers (OpenToonz port) [#1946]
- Fix SVG import crash with non-ASCII file paths on Windows (OpenToonz port) [#1946]
- Fix unreachable code and duplicate VEC array initialization (OpenToonz port) [#1946]
- Fix -Wreturn-type warnings in iocommand and igs_rotate_blur (OpenToonz port) [#1946[^9] ]
- Fix(OCA): correct endTime (OpenToonz port) [#1946]
- Fix(Scene Depth/Quicktoolbar): Invisible after startup (OpenToonz port) [#1942]
- Fix: Add missing fclose calls for proper file handling (OpenToonz port) [#1946]
- fix: handle MSVC warnings C4723 and C4552 (OpenToonz port) [#1946[^10]]
- fix: initialize popup, firstW, and lastW (OpenToonz port) [#1946]
- fix: ODR violations (# 241) by adding flipbooksettings.h for toggle vars (OpenToonz port) [#1946]
- Fix: Premultiply background color for Export Level Commands (OpenToonz port) [#1946[^9] ]
- fix: Preview not Updating for New Toonz Raster Levels editted with Fill Tool (OpenToonz port) [#1946]
- fix: remove duplicate and unreachable 'stylepages' case (OpenToonz port) [#1946]
- fix: remove std::iterator to resolve deprecation warning (OpenToonz port) [#1946]
- fix: resolve C4018 signed/unsigned mismatch warnings (OpenToonz port) [#1946]
- fix: typo (OpenToonz port) [#1946]
- fix: unreachable code in tscannerepson.cpp and Naa2TlvConverter.cpp (OpenToonz port) [#1946[^10]]
- Fix: Use sig_atomic_t for signal-safe shutdown flag (OpenToonz port) [#1946]
- Prevent OS/theme background mismatch (OpenToonz port) [#1946[^8]]
- Set proper window flag on preferences pop-up for Mac OSX (OpenToonz port) [#1946]

### Other

- Update translation files for v1.6 [#1980, #1992, #2005, #2023, #2032]
- Fix macOS build CMake version [#1909]
- Fix removing unnecessary files in Windows builds [#1968]
- Github build on macos-15-intel (Sequoia)/Fix Linux build [#1917]
- Feature: Report missing/unknown XML tags during scene loading (OpenToonz port) [#1946]

## [1.5.4] - 2025-08-01

### Fixed

- Fix raster autoclose [#1861]
- Fix Control Point Editor Pen Lag [#1869]
- Fix temp tool switching [#1882]

### Other

- Spanish translation updates by @gab3d [#1870]
- Build on MSVC 2022 [#1859]
- postinst-fix [#1880]
- Fix wget redirect auth fail [#1884]

## [1.5.3] - 2025-06-01

### Added

- GPhoto Event Log Viewer + Help Menu Update [#1838]

### Fixed

- Fix on-canvas widget/indicator visibility [#1782]
- Fix Fill gap - Savebox fill image corruption [#1792]
- Correct typo Frane -> Frame [#1800]
- Fix double-clicking 1st folder item not working [#1807]
- Fix Plastic Tool crash and history messages [#1808]
- Fix canceling selection when modifiers used [#1814]
- Fix startup console warnings and errors [#1815]
- Fix Relative Drawing Mode Tweening [#1819]
- Percentage spacing on pattern brushes. [#1829]
- Fix rasterization of vector [#1843]
- Block shortcuts while tool is busy [#1844]
- Fix Implicit Clear Frames [#1845]
- Fix Mesh Animate crash of SmartVector Level [#1784[^7]]
- Remove rasterized vector pink box [#1806[^7]]
- Show 0 thickness strokes in Visualize Vector as Raster [#1775[^6][^7]]
- Fix Visualize Vector As Raster (OpenToonz port) [#1775[^6]]
- [VectorSelectionTool] Fix Rasterized Vector Image update failure (OpenToonz port) [#1775[^6]]

### Other

- Update how_to_build_linux.md to include missing dependency to build on arch linux [#1785]
- Fix github cmake version for Linux builds [#1788]
- Cleanup post Debian package creation [#1790]
- Build on Ubuntu 22.04 [#1791]
- Remove unncessary folders from ccache [#1796]
- Fix Linux packaging space usage [#1798]
- Switch from Git LFS to regular files [#1816]
- Fix missing Linux Action ccache directory [#1818]
- Fix windows scripting build path [#1840]

## [1.5.2] - 2025-03-31

### Fixed

- Fix canceling cell/column selection [#1721]
- Fix Task Render/Cleanup failure (Linux) [#1709]
- Fix startup crash due to incompatible DLLs [#1711]
- Fix MOV transparency [#1713]
- Fix camera loading crash + improve Sony configuration [#1733]
- Fix Excessive Frame Range Tape lag [#1735]
- Increase Size of Step Lines in Motion Path [#1750]
- Fix Delete Style Color [#1754]
- Disable mouse wheel in stopmotion control panel [#1763]
- Fix Linux Desktop Path Detection Failure [#1777]
- Fix project reload fail (OpenToonz port) [#1775[^6]]
- Fix Failure when updating the folder node (OpenToonz port) [#1775[^6]]
- Optimize Adjust Current Level to This Palette (OpenToonz port) [#1775[^6]]
- Avoid endless loop for some small fonts (OpenToonz port) [#1775[^6]]
- Fix File Browser Model Refreshing (OpenToonz port) [#1775[^6]]
- Fix selection in File Browser (OpenToonz port) [#1775[^6]]
- Fix Gif Export for newer FFmpeg versions (OpenToonz port) [#1775[^6]]
- Fix Clean Up Palette Related Crash and cleanup parameters related adjustment (OpenToonz port) [#1775[^6]]

### Other

- Update Debian dependences doc [#1731]
- Fix Testing Pull Requests (PRs) instructions [#1741]
- Update Build Cache Management [#1742, #1744, #1746, #1748]
- New deb-creator from Appimage script [#1760]
- Build concurrently to reduce overall build time [#1761]
- .desktop file comments (with translations) and keywords [#1762]
- Create Debian Package for Tahoma2D [#1768, #1773]
- Fix macOS Installer creation [#1787]
- Include functional in place of removed xstddef (OpenToonz port) [#1775[^6]]
- Remove any_iterator (OpenToonz port) [#1775[^6]]
- Cleanup: Replace deprecated endl with Qt::endl and fix operator prece… (OpenToonz port) [#1775[^6]]
- Update Webm Export (OpenToonz port) [#1775[^6]]

## [1.5.1] - 2025-02-03

### Added

- High DPI Scaling preference [#1660, #1694, #1705]
- Added CHANGELOG.md [#1708]

### Changed

- Fix Gap Closure results (Tape Tool) [#1673]

### Fixed

- Fix relative drawing onion skin guided drawing [#1661]
- Fix update folder delay (OpenToonz port) [#1697]

### Other

- Brazilian Portuguese Translation - Review + Update to 1.5 [#1671]
- Update Canon EDSDK to v13.18.40 [#1700, #1701, #1702, #1703]

## [1.5.0] - 2024-12-01

### Added

- Timeline/Xsheet Folders [#1385, #1398, #1571]
- Clipping Mask Column (Raster/Smart Raster) [#1307]
- Relative Drawing Onion Skin Mode [#1556]
- Ping Pong command/button [#1363]
- Inbetween Flip [#1372]
- Vector Control Point/Stroke Alignment Commands [#1275]
- Undo/Redo Touch Gestures [#1584]
- OCA Import [#1392, #1396, #1397, #1409, #1520]
- Tips popup [#1654]
- Clip with Vector Strokes [#1549]
- Clip ZeraryFx Columns [#1541]
- Dockable Locator panel [#1422]
- Dockable Output/Preview settings panels [#1423]
- Eraser Tool Pressure and Frame Range Interpolation settings [#1416]
- Frame Range tool option enhancements [#1427, #1445]
- Add option to increase the size of the Edit & Control Point Editor tool widgets [#1469]
- Inbetween from Timeline/Xsheet and Hook movement tweening [#1428]
- Onion Skin Opacity control [#1471, #1504]
- Raster/MyPaint style brush Smooth option [#1465]
- Vector Inspector Initial Release [#1486, #1516]
- Zoom in/out and fit floating panel geometry commands (OpenToonz port) [#1535]

### Changed

- Change eraser tools resize binding to CTRL+ALT in Normal Mode only. [#1547]
- Control Point Editor Tool enhancements [#1305]
- Fx Browser enhancement and Fx Room update [#1419, #1557, #1580]
- Move Columns and Cells without Drag Bars + selection/drag fixes [#1330, #1342, #1440, #1458, #1534]
- Reenable arrow keys as shortcuts and fixes [#1462]
- saveLevelsOnSaveAs + UHD capture cam + clean debug asserts [#1359]
- Shift and Trace - Hide original position [#1364]
- Show Symmetry and Perspective Grids commands and buttons [#1401]
- Update default layout and command bars [#1400]
- [ui] Decouple x-sheet text colors (OpenToonz port) [#1535]
- [ui] Extra x-sheet colors (OpenToonz port) [#1535]
- Studio Palette Tweaks (OpenToonz port) [#1535]

### Fixed

- Fix Cleanup Palette switching issues [#1605]
- Fix Create Blank Drawing making new level after Stop Frame [#1589]
- Fix displaying plastic deformed level in Linux [#1628]
- Fix edit missing controlpoint crash [#1656]
- Fix enabling tool options on Vector Select All [#1626]
- Fix missing undo with temp tool release [#1645]
- Fix multi-screen detection issues [#1542]
- Fix Note Cell Extending [#1637]
- Fix palette style animation keys management [#1607]
- Fix palette viewer's Save Palette/Save Palette As options [#1608]
- Fix pasting external clipboard image [#1630]
- Fix pinching at end of line [#1625]
- Fix preview undo collapse crash [#1650]
- Fix raster eraser crash [#1624]
- Fix SHIFT + Cell Dragging clearing cells [#1611]
- Fix Smart Raster Freehand/Polyline Fill [#1606]
- Fix Smart Raster Freehand/Polyline Fill Redo and gap closure lines [#1649]
- Fix Vector Stroke Group/Ungroup issues [#1393, #1399]
- Fix zoom out with small movements [#1646]
- Ignore Auto-Stretch while in Implicit Mode [#1591]
- Correct Minor Typos (OpenToonz port) [#1535]
- Fix the dialogs to be closed when clicking the buttons, instead of pressing. (OpenToonz port) [#1535]
- Fix typo (OpenToonz port) [#1535]
- Fix update sound after editing cells (OpenToonz port) [#1535]
- Macos undo freeze (OpenToonz port) [#1535]

### Other

- Qt 5.15 [#1456, #1479, #1487, #1492]
- Allow to run packaging script from any dir [#1622]
- Appimage fix: Allow to pass filename as argument for Tahoma binary [#1633]
- Fix hdiutil resource busy [#1620]
- Install and link libdeflate (macOS) [#1632]
- Russian translation updates by beeheemooth [#1657]
- Update macOS build environment to Ventura [#1613]
- Update to custom rhubarb v1.13.0 [#1464]
- Update translation files for v1.5 [#1599]
- Refactor FFmpeg format check on launch (OpenToonz port) [#1535]
- Remove TSmartObject from TProject (OpenToonz port) [#1535, #1546]
- Update cleanup_default.tpl to be empty (OpenToonz port) [#1535]
- windows configure ThirdParty (OpenToonz port) [#1535]

## [1.4.5] - 2024-10-01

### Fixed

- Fix sound column issues [#1537]
- Fix pane dimensions for new floating toolbar [#1539]
- Fix ZoomIn Viewer shortcut processing [#1543, #1574]
- Fix unexposed clipping mask frame hiding exposed level frame [#1550]
- Fix display of mesh deformed clipping mask [#1551]
- Increase initial 3D Viewer scale [#1554]
- Fix implicit getCells in column range [#1558]
- Fix extend cells creating explicit holds in implicit mode [#1563]
- Fix pasting raster selection in xsheet/timeline cell [#1575[^4]]
- Fix ffmpeg MP4/MOV color space [#1576]
- Add MSVC DLLs to Windows Package [#1579]
- Skip Stop Holds with Next/Previous Drawing [#1582]
- Fix update sound after editing cells (OpenToonz Port) [#1535[^5]]
- Macos undo freeze (OpenToonz Port) [#1535[^5]]
- Refactor FFmpeg format check on launch (OpenToonz Port) [#1535[^5]]
- Fix typo (OpenToonz Port) [#1535[^5]]
- Correct Minor Typos (OpenToonz Port) [#1535[^5]]

## [1.4.4] - 2024-08-01

### Fixed

- Fix Flow Paint Brush Iwa Fx Swatch Crash [#1501]
- Fix max fill depth calculations [#1506, #1515]
- Link libdeflate for Linux builds [#1517]
- Fix Stylus event processing [#1519, #1527]
- Fix crash opening PLI (Vector) files and folders [#1530]
- Fix used style detection for Fill and Style Picker tools [#1532]

## [1.4.3] - 2024-06-02

### Fixed

- Updated Chinese translations [#1438]
- Add horizontal scrollbar to file browser [#1446]
- Fix brush and eraser resizing [#1450]
- Fix empty Export Level popup [#1455]
- Fix missing undo Raster Create Frame [#1459, #1480]
- Fix level strip empty space selection and commands [#1472]
- Fix Windows key description for macOS [#1481]
- Fix Export Current Scene popup folder tree [#1482]
- Fix explicit holds created in implicit mode [#1484]
- Fix invisible drawing guides [#1488]
- Timeline duplicate created after original in Level Strip [#1491]
- Fix context menu in rename fields [#1494]

## [1.4.2] - 2024-04-02

### Fixed

- Fix Minimum Xsheet layout UI issues [#1387]
- Fix saving files in scene sub-folders [#1388, #1394, #1359[^3]]
- Fixes to SVG Import and Export [#1395, #1408]
- Fix symmetry join vectors [#1404]
- Fix Erase and Fill Shift+Frame Range operation [#1410]
- Fix Fx not working with Implicit Holds [#1411]
- Fix slow preview generation of particle fx [#1417, #1430]
- Fix popups hiding behind main window [#1425]

## [1.4.1] - 2024-02-01

### Changed

- Skip version check option [#1326]

### Fixed

- Warn when unable to load/read file [#1285]
- Add missing exception catch-all [#1306]
- Fix Timeline/Xsheet Frame Range processing [#1309]
- Fix undo selection paste into new frame + undo renumbering [#1316]
- Fix saving renumbered mesh error + potential file image loss [#1317]
- Add Flatpak bin directory and fix ffmpeg & rhubarb detection [#1322]
- Fix system env variables [#1324]
- Fix sub-scene navigation bar issues [#1325]
- Fix loading unexpected image filetype [#1331]
- Fix drawing timeline/xsheet cells [#1334, #1342[^1]]
- Fix targeting timeline keys and ease handles [#1336]
- Fix position of parent dropdown menu [#1337]
- Fix actions that shouldn't happen while autosaving [#1341]
- Fix palette loss when caching images [#1343]
- Fix insert column crash without a column selected [#1344]
- UHD capture cam [#1359[^2] ]
- Fix segment erase + image change crash [#1368]
- Fixed crash when holding Ctrl+Z while drawing. (OpenToonz port) [#1361]
- Fixed deselect of hexlineedit after right click (OpenToonz port) [#1361]
- Known vector fill glitch fix (OpenToonz port) [#1361]

## [1.4.0] - 2023-12-01

Some features, when used, are saved to the scene file and will prevent the scene from opening in prior versions:
- Scene Overlay Image
- Floating point & Linear color space rendering (OpenToonz port)
- Customizable column color filters (OpenToonz port)

### Added

- Clipping Mask Column (Vector based) [#1142, #1269]
- Symmetry Tool + some Tool bug fixes [#1131]
- Sub-scene Navigation Bar [#1251]
- Add Clear Frames and Remove Cells commands [#1191]
- Current Drawing on Top [#1218]
- Light Table [#1217]
- Scene Overlay Image [#1222, #1262, #1235]
- StopMotion - libgphoto2 integration [#1027, #1287]
- Viewer Event Log [#1237]
- Control file history information [#1216]
- Brazilian Portuguese Translation [#1144]
- Custom Panel (OpenToonz port) [#1139, #1185, #1286]
- Customizable column color filters (OpenToonz port) [#1185]
- Export Camera Track feature (OpenToonz port) [#1185]
- Floating point & Linear color space rendering (OpenToonz port) [#1139, #1273]
- New Fx: Floor Bump Fx Iwa (OpenToonz port) [#1139]
- New Fxs: Flow Fx series (OpenToonz port) [#1139]
- OCA Export (OpenToonz port) [#1139, #1185]
- OpenEXR I/O (OpenToonz port) [#1139]
- Particles Fx Motion Blur (OpenToonz port) [#1139]
- Preproduction Board feature 1.6 (OpenToonz port) [#1139, #1185, #1286]
- Toggle Blank Frames menu command (OpenToonz port) [#1139, #1002]
- Triangle Brush Cursors (OpenToonz port) [#1139]

### Changed

- Show Advanced Preferences and Options [#1114, #1166]
- Save/Restore tool settings + Import environment preference fix [#1220]
- Onion Skin All Frames or New Exposures Toggle [#1271]
- Add Default Projects Folder to Browser [#998]
- Allow Decrease Step without selection [#1119]
- Allow tag actions on non-Current frame [#1117]
- Gaps: Autoclose/Autofill enhancement [#1141, #1260]
- Individualized command Bar definitions [#1135, #1140]
- Magnet Tool slider - non-Linear preference [#1221, #1227]
- Move Cell selection with Current frame Navigation [#1116, #1165]
- Render Key Drawings Only & Render to Folders [#1223]
- Replace DEL with Backspace for OSX [#994]
- Save/Restore palette swatch size setting for popups [#1224]
- Set Start/Stop Markers enhancement [#995]
- Stop Function editor Folder auto-expansion [#1192]
- Toggle Toolbar Orientation [#1134, #1138]
- Update displaying of Column/Pegbar Number [#1125]
- Add icons [#1219, #1277]
- 2D room update [#1265]
- Update translation files for v1.4 [#1255]
- Function Editor Curve Graph: Bigger Handles & Improved Contrast (OpenToonz port) [#1139]
- Make style editor textfield expandable (OpenToonz port) [#1002]
- Refined Icons (OpenToonz port) [#1139]
- Update Icons: Types (OpenToonz port) [#1139, #1190, #1196, #1195, #1203]
- 3rd party interface adjustments + more (OpenToonz port) [#1139, #1146]
- Add Paint Brush Mode Actions (OpenToonz port) [#1139]
- Audio fixes and enhancements (OpenToonz port) [#1139, #1150]
- Display Blank Frames in Camera stand view (OpenToonz port) [#1002]
- Expand xsheet frame area (OpenToonz port) [#1139]
- Export Xsheet to PDF: Frame Length Field (OpenToonz port) [#1139]
- Extend the coverage of the "Default TLV Caching Behavior" option to all raster levels on any loading (OpenToonz port) [#1185]
- Fill Tool - Picker+FH (OpenToonz port) [#1139]
- fix column right-click action (OpenToonz port) [#1244]
- Highlight xsheet line every second (OpenToonz port) [#1002]
- Histogram popup enhancements (OpenToonz port) [#1139]
- HSLBlend images alpha fix (OpenToonz port) [#1139]
- Misc. updates to style editor brush pages (OpenToonz port) [#1139]
- Modify Convert TZP in Folder and register TZP icon (OpenToonz port) [#1185]
- Modify hiding and showing controls in fx settings (OpenToonz port) [#1139]
- Modify menubar commands tree by Contrail Co.,Ltd. (OpenToonz port) [#1185]
- Modify reframe behavior by Contrail Co., Ltd. (OpenToonz port) [#1185]
- Palette Level Toolbar: Menu to hide buttons, Palette Gizmo button, Name Editor button and missing icons (OpenToonz port) [#1139]
- Paste cells command behavior option by Contrail Co.,Ltd. (OpenToonz port) [#1185]
- refactor: Icon stuff (Tahoma2D) [#1183]
- Revise Radial Blur Fx Ino (OpenToonz port) [#1002]
- Revise Spin Blur Fx Ino (OpenToonz port) [#1002]
- Sampling Size for Gradient Warp Iwa Fx (OpenToonz port) [#1139]
- Selection aware command name by Contrail Co.,ltd. (OpenToonz port) [#1185]
- Show autopaint style color in Transparency Check (OpenToonz port) [#1185]
- Snap Schematic Node (OpenToonz port) [#1139]
- Spin Blur & Radial Blur Fx gadgets: Lock AR / angle with modifier keys (OpenToonz port) [#1002]
- Spin Blur Fx Ino : Spin gadget alternated coloring (OpenToonz port) [#1139]
- Stretch function curve keys with Alt+drag (OpenToonz port) [#1185]
- Switch current fx on switching columns (OpenToonz port) [#1185]
- Thicken Xsheet marker lines (OpenToonz port) [#1139]
- Update Bokeh Advanced Iwa Fx (OpenToonz port) [#1139]
- Viewer preview feature enhancement (OpenToonz port) [#1139]

### Fixed

- Fix broken Column Toggle shortcuts [#1246]
- Fix column settings menu below screen [#1263]
- Fix crash exploding subscene with Only Audio [#1231]
- Fix crash rendering with MOV QuickTime properties [#1178]
- Fix crashrpt initialization creating bad directories [#1149]
- Fix dragging on Smart Raster reference Fill [#1239]
- Fix empty source switching crash [#1241]
- Fix more implicit issues [#1167]
- Fix plastic Render crash [#1179]
- Fix plastic Tooloption shortcuts [#1226]
- Fix potential pen-drag actions when tapping [#1240]
- Fix reopening config button when closing [#1173]
- Fix Smart Raster Brush Lock alpha Tone [#1156]
- Fix SVG import from Browser [#1230]
- Fix SVG sequence file loading [#1212]
- Fix Toggle and Edit tag shortcuts [#1172]
- Fix wrong Raster project palette [#1252]
- Invert Raster Fill depth value calculation [#1242]
- Issue 1151 residual gap on fill gaps [#1215]
- Range intermediate status bar and tooltip [#1184]
- Restore Current Column indicator [#1174]
- fix untranslatable text [#1147]
- Reword "Reverse sheet symbol" to "Inbetween symbol 2" (OpenToonz port) [#1002]
- Patch Stylesheet (Tabs, Light Theme) (OpenToonz port) [#1002]
- Animation tool option: select camera after switching it (OpenToonz port) [#1139]
- Fix "define sub camera" with stylus (OpenToonz port) [#1185]
- Fix 32bit preview (OpenToonz port) [#1244]
- Fix batch rendering via task panel (OpenToonz port) [#1185]
- Fix column transparency to work on rendering (OpenToonz port) [#1185]
- Fix crash on loading png in subbox (OpenToonz port) [#1185]
- Fix cropped input fields (OpenToonz port) [#1139]
- Fix deleting output node (OpenToonz port) [#1185]
- Fix enabling unit preferences (OpenToonz port) [#1185]
- Fix File Browser to open network path (OpenToonz port) [#1185]
- Fix MacOS color model picking with stylus (OpenToonz port) [#1185]
- Fix notifyClick when mouse button is released rather than pressed (OpenToonz port) [#1185]
- Fix Output Settings not to set the dirty flag when unchanged (OpenToonz port) [#1139]
- Fix Particles Fx column placement bug (OpenToonz port) [#1139]
- Fix Radial and Spin Blur Ino offset problem (OpenToonz port) [#1139]
- Fix raster caching not to set the modified flag (OpenToonz port) [#1185]
- Fix RGB picker notification (OpenToonz port) [#1185]
- Fix Soapbubble fx (OpenToonz port) [#1185]
- Fix Starting frame Multi-thread FFMPEG (OpenToonz port) [#1139]
- Fix studio palette drawtoggle for linux (OpenToonz port) [#1182]
- Fix studio palette paths caching (OpenToonz port) [#1185]
- Fix Style Editor HSV rounding (OpenToonz port) [#1185]
- fix Stylesheet variables (OpenToonz port) [#1185]
- Fix tablet tools (OpenToonz port) [#1185]
- Fix Text Iwa Fx input field (OpenToonz port) [#1185]
- Fix unsigned warning (OpenToonz port) [#1181]
- Fix Use default constructor instead (OpenToonz port) [#1244]
- Fix viewer parts visibility (OpenToonz port) [#1139]
- Fix viewer preview fails to restart after canceling while rendering (OpenToonz port) [#1185]
- Keep selection valid when cutting frames (OpenToonz port) [#1139]
- Remove Vector Tape Tool "m_draw" Flag (OpenToonz port) [#1185]
- Replace fx fixed sized buttons with minimum size hint (OpenToonz port) [#1244]

### Other

- Update Canon EDSDK to v13.17.10 [#1261]
- Build Installer [#990, #1292]
- fix MacOS lib path Issue [#1198, #1208, #1209]
- fix rpath in MacOS packaged libraries [#1155]
- Revert "Revert Linux builds to Ubuntu 18.04" [#1153]
- Update MacOS Build env to Monterey [#1234]
- Update T2D/OT licenses to 2023 [#1243]
- Various bug fixes [#1235]
- Add Haiku support (OpenToonz port) [#1185]
- Clarify size_t origin for tgc::hash::BucketNode (OpenToonz port) [#1185]
- Convert TZP to TLV in folder (OpenToonz port) [#1139]
- Fix build on FreeBSD 13+ (OpenToonz port) [#1244]
- refactor: Use strcmp and restore addSpecifiedSizedImageToIcon function (OpenToonz port) [#1244]
- Refactoring: Replace obsolete Qt functions (OpenToonz port) [#1139]
- Refactoring: Unify Viewer and Comboviewer base classes (OpenToonz port) [#1139]
- Remove boost::bind (OpenToonz port) [#1244]
- Replace 3rd party crashrpt (OpenToonz port) [#1187, #1214, #1210, #1201]
- Simplify uneditable check code (OpenToonz port) [#1139]
- Update sceneviewerevents.cpp remove identical include (OpenToonz port) [#1244]
- Various typo and translation of source comments + whitespace tweaks (OpenToonz port) [#1002]
- Fix xsheet pdf export tick-marking bug (OpenToonz port) [#1002]
- Fix QTabWidget Pane Border in Stylesheets (OpenToonz port) [#1002]

### Removed

- Removed CrashRpt (Windows) [#1187]

## [1.3.1] - 2022-12-08

### Fixes

- Fix software version check 
- Fix Canon Import Image crash
- Fix parent column indicators
- Update thirdparty library binaries
 - Includes updating libtiff to v4.2.0 to address security vulnerabilities
- Fix pen issues
 - Fixes Drawing delay on vector and smart raster levels (Linux issue)
 - Pen doesn't work in Schematic Viewer. (macOS/Linux issue)
- Fix Implicit Hold issues
 - Fixes Crash when cloning implicitly held cell
 - Fixes Dragging drawings in cells causes issues
 - Fixes Crash when merging Smart Raster Levels when the first column has a Stop-frame
 - Fixes issue where New Level creation with Step > 1 creating explicit cells
 - Fixes issue creating explicit cells for Implicit cells when collapsing to Subscene
- Fix Autofill toggle command crash
- Fix Particle Control Port handling
- Fix Linux plastic tool crash
- Fix MeshImage compression crash
- Fix blank line showing in Macro fx list
- Fix using favorite Vector Brush not drawing properly
- Fix moving styles in palette
- Fix output filename changing to dot
- Fix loading TZP (OpenToonz port)
- Fix Mov Settings Updating (OpenToonz port)
- Fix SoapBubble Iwa fx (OpenToonz port)
- Fix memory corruption caused by rapid project/scene change (OpenToonz port)
- Fix preview range not to be negative (OpenToonz port)
- Style Selection .isEmpty logic fix (OpenToonz port)
- Fix crash on replacing level's parent directory while opening preview (OpenToonz port)
- Fix Bokeh Fx White Artifacts (OpenToonz port)
- Fix Fx obsolete parameter handling (OpenToonz port)
- Fix Custom File Path Rule Bugs (OpenToonz port)

## [1.3.0] - 2022-05-06

Existing Users - Please note the following 3 changes
- Implicit Frame Hold mode is defaulted ON so you do not need to manually expose held frames.
- Frames will auto hold implicitly until a new frame or a Stop Frame is encountered
- For older scenes, recommend turning this mode off or use Convert to Implicit Holds
- Default level type is Raster.

Some features, when used, are saved to the scene file and will prevent the scene from opening in prior versions
- Timeline/Xsheet navigaion tags
- Cell Mark feature (OpenToonz port)
- Note level column enhancement (OpenToonz port)

### Added

- Implicit Frame Hold
- Timeline/Xsheet navigation tags
- Autorenumber column
- Add referenced fills
- Perspective Grid Tool
- Add Raster Lock Alpha tool setting
- Type Tool for Raster levels
- Add polyline point handle Shift/Alt modifiers
- Add Flip and Rotate Animate tool option buttons + shortcuts
- Add Flip and Rotate Selection tool option buttons
- Add Auto creation option commands/buttons
- Current Column/Cell Indicator color
- Stop Motion Camera Calibration
- Style set management
- Add View Check indicators to Viewer
- Allow renaming of revamped Pass Through Fx nodes
- Add Tahoma2D Stuff and Downloads folder access from Browser
- Cell Mark feature (OpenToonz port)
- Xsheet Minimum Layout (OpenToonz port)
- Xsheet Zoom Control (OpenToonz port)
- Add rotate option to geometry tool (OpenToonz port)
- Additional Style Sheet (OpenToonz port)
- Hex color names editor (OpenToonz port)
- Style Editor: Move "toggle orientation" button in the menu (OpenToonz port)
- 30 bit display feature (OpenToonz port)
- Add Fx Global Controls (OpenToonz port)
- Compass Gadget for Radial and Spin Blur Fxs (OpenToonz port)
- New Fx : Rainbow Fx Iwa (OpenToonz port)
- New Fx: Bokeh Advanced Iwa (and Bokeh Fxs overhaul) (OpenToonz port)
- A new option for the Fractal Noise Fx Iwa : Conical Transform (OpenToonz port)
- Add the Linear color space option to all other Layer Blending Ino fxs (OpenToonz port)
- Export Xsheet to PDF (OpenToonz port)
- TVPaint JSON export (Experimental feature) (OpenToonz port)

### Changed

- Default column names to 1st level name
- Paint Brush Tool enhancements
- Update MyPaint style usage on various tools
- Move savebox fill limit preference to tool option
- Switch to Build mode on add skeleton or no skeleton
- Enable single click expansion on Quicktoolbar categories
- Change Default Level Type to Raster
- Allow multiple key stroke shortcuts
- Enhanced WAV support
- Changed default sound column volume to 100%.
- Change Level Editor background for Vector levels
- Change UI highlight color
- Changed Style edit tab order
- Mypaint icon display improvement
- Stop motion room changes
- Tahoma branding design update
- Update labels to Guided Tweening
- "Implicit hold" move by Shift+dragging cells (OpenToonz port)
- Modify sync scroll between Xsheet and Function editor panels (OpenToonz port)
- Note level column enhancement (OpenToonz port)
- Raster level frame number format & PNG for new Raster level (OpenToonz port)
- Project Settings-aware Frame Number Field Width (OpenToonz port)
- Stop motion play sound on capture and deciseconds resolution (OpenToonz port)
- Enable to Paste Style's Color Into Color Field (OpenToonz port)
- Enhance Studio Palette Searching (OpenToonz port)
- RGB sliders behaving like CSP/Krita, Hex editbox and various bugfixes (OpenToonz port)
- Add additional zoom levels to the zoom in/out commands.
- raylitfx enahancement: radius parameter and gadget (OpenToonz port)
- Bloom Fx Iwa Revised (OpenToonz port)
- Change behavior of the Layer Blending Ino fxs when the background layer does not exist (OpenToonz port)
- pnperspectivefx normalize option (OpenToonz port)
- Particles Fx : Use 16bpc Control Image for Gradient (OpenToonz port)
- Redesign the Pass Through Fx Node (OpenToonz port)
- Tile Fx Iwa : a new "Image Size" option for the Input Size parameter (OpenToonz port)
- Update Shader Fx: HSL Blend GPU (OpenToonz port)
- Enable To View Palette Files From the File Browser (OpenToonz port)
- Enhance Flipbook playback (OpenToonz port)
- Modify Flipbook Histogram (OpenToonz port)
- Improvements to Audio Recording (OpenToonz port)
- FFMPEG GIF Export enhancements (OpenToonz port)
- Modify Export Xsheet PDF feature (OpenToonz port)

### Fixed

- Allow Level Extender to shift Sound cells
- Change default minimum fill depth
- Fix Autopaint for Pencil lines
- Fix blurred brush crash after canvas resize
- Fix brower from startup not reopening
- Fix Canvas Size availability
- Fix cell selection issues
- Fix cell selection resizing with Ctrl+Arrows
- Fix column switching when clicking on column drag bar
- Fix crash adding Fx while on Camera column
- Fix crash when adding to task list from browser
- Fix fx swatch not displaying initially
- Fix Ignore Gaps typo in Fill Tool
- Fix keyframe icon positions
- Fix loading plugins on Linux
- Fix loading scenes from Story Planner
- Fix macOS shortcut display
- Fix memo style editor color change crash
- Fix missing Motion Path data causing crash
- Fix missing PSD Layer Id
- Fix Normal-Selective-Area fills filling partially painted objects incorrectly
- Fix Note Text cell issues
- Fix paintbrush modifier actions
- Fix palette converting to Smart Raster
- Fix palette saving issues
- Fix popups appearing behind other popups
- Fix reappearing deleted style
- Fix reloading stop motion scene crashing application
- Fix rendered sound starting too soon
- Fix Replace Level for Implicit Holds
- Fix Scene Cast Level drag/drop indicator
- Fix scene switching flipbook crash
- Fix segment erase crash at borders
- Fix selection tool for motion path editing
- Fix small text on high res monitors
- Fix starting panel resizing issues
- Fix temp fillgap indicators not disappearing
- Fix timeline column lock indicator refresh
- Fix timeline/xsheet cell dragbar display issues
- Fix undo delete level strip mesh frame
- Fix xsheet/timeline starting with wrong orientation
- Fix/Improve style chip indicators
- Fixed undoing a New Level creation in an empty frame in middle of existing level shifting incorrectly
- Fixed undoing column loaded PSD file leaving empty columns behind
- Fixed undoing Cut of selection of only empty cells shifting incorrectly
- Fixed undoing Each 2/3/4 when you select frames less than the Each # adding new cell incorrectly.
- Fixes Opacity slider lost due to text translation is too long
- Improve keyframe icon targeting
- Set directory permissions on macOS/Linux builds
- Stop switching columns on column button press
- Update scene sounds on save.
- Change the Paste Color command behavior not to overwrite link information (OpenToonz port)
- Deactivate Edit Shift mode when deactivating Shift and Trace (OpenToonz port)
- Fix "Show Folder Contents" command for UNC path (OpenToonz port)
- Fix and refactor Line Blur Fx Ino (OpenToonz port)
- Fix Autorenumber of Raster Level (OpenToonz port)
- Fix backtab key to work for moving styles shortcut scope (OpenToonz port)
- Fix Bokeh Fxs Artifacts (OpenToonz port)
- Fix Brush Smoothing (OpenToonz port)
- Fix channel clamping behavior in Layer Blending Fxs (OpenToonz port)
- Fix conical fractal noise with offset (OpenToonz port)
- Fix Convert to TLV Palette Saving (OpenToonz port)
- Fix crash on cleaning up zerary column (OpenToonz port)
- Fix crash on clicking shortcut tree (OpenToonz port)
- Fix crash on creating Macro Fx from the Fx Browser (OpenToonz port)
- Fix crash on deleting control point while dragging (OpenToonz port)
- Fix crash on Fx Schematic (OpenToonz port)
- Fix crash on rendering ArtContour fx (OpenToonz port)
- Fix crash on rendering PaletteFilter Fx (and other fxs) with hyphen (OpenToonz port)
- Fix crash on saving scene containing missing palette level (OpenToonz port)
- Fix crash on switching scenes while selecting raster level frame (OpenToonz port)
- Fix crash on undoing tool (OpenToonz port)
- Fix crashes when using Type tool (OpenToonz port)
- Fix crashes with preview in Fx Settings popup (OpenToonz port)
- Fix Crush On Inserting Fx (OpenToonz port)
- Fix crush on showing snapshot in flipbook (OpenToonz port)
- Fix dynamic conical fractal noise (OpenToonz port)
- Fix file browser assertion failure (OpenToonz port)
- Fix Flipbook wobbling when dragging while playing (OpenToonz port)
- Fix FrameIds Inconsistency in Level Strip Commands (OpenToonz port)
- Fix Friction parameter behavior of the Particles Fx (OpenToonz port)
- Fix Fx Gadget Offset (OpenToonz port)
- Fix fx gadget picking (OpenToonz port)
- Fix Fx intensity malfunction (OpenToonz port)
- Fix Fx placement (OpenToonz port)
- Fix Fx Schematic links disappear (OpenToonz port)
- Fix FxId and expression handling (OpenToonz port)
- Fix GIF rendering on older versions (OpenToonz port)
- Fix Hide Icons in Windows with Scale 125 % (OpenToonz port)
- Fix high dpi multimonitor glitch (OpenToonz port)
- Fix margin bottom slider-v-handle (OpenToonz port)
- Fix margin of Directional Blur Fx Iwa (OpenToonz port)
- fix memo popup background color (OpenToonz port)
- Fix mesh file naming (OpenToonz port)
- Fix missing mouse release (OpenToonz port)
- Fix output file name for movie format (OpenToonz port)
- Fix parent object field in the xsheet column header (OpenToonz port)
- Fix Particles Fx using plastic-deformed source (OpenToonz port)
- Fix Paste Color command not to set the "edited flag" to normal styles (OpenToonz port)
- Fix PN Clouds Ino Fx (OpenToonz port)
- Fix Preferences popup combobox size adjustment (OpenToonz port)
- Fix pressing Shift key loses focus when renaming cell (OpenToonz port)
- Fix PSD loading and level format preferences (OpenToonz port)
- Fix Raster Deformation Slowness (OpenToonz port)
- Fix Rendering Fx After Xsheet Node (OpenToonz port)
- Fix RGB key fx affine transformation handling (OpenToonz port)
- Fix saving tlv (OpenToonz port)
- Fix shortcut loading from file (OpenToonz port)
- Fix Spectrum (OpenToonz port)
- Fix Stage Schematic Column (Right click) now w/Context Menu (OpenToonz port)
- Fix Style Editor bottom menu inaccessible (OpenToonz port)
- Fix text color style inside some windows (OpenToonz port)
- Fix the parent object label in xsheet column header translatable (OpenToonz port)
- Fix tile fxs & bounding box handling (OpenToonz port)
- Fix Undo Malfunction (OpenToonz port)
- Fix undo of moving cells with Shift+Ctrl (OpenToonz port)
- Fix undo pasting raster selection to cell (OpenToonz port)
- Fix various typos (OpenToonz port)
- Fix vector levels to render aliased line (OpenToonz port)
- Fix viewer playback (OpenToonz port)
- Fix wheel scrolling on the output settings popup (OpenToonz port)
- Fix Zerary Fx bugs (OpenToonz port)
- Fix zoom scale display of ComboViewer's titlebar (OpenToonz port)
- Fixes toward Xsheet parenting + enhancements (OpenToonz port)
- Fx schematic node placement fix (OpenToonz port)
- revert changing pinned center (OpenToonz port)
- Xsheet PDF Export: Tickmark breaks continuous line (OpenToonz port)

### Other

- Update ffmpeg to v5.0
- Add FreeBSD support (OpenToonz port)
- Auto Cleaning Assets Feature (OpenToonz port)
- Enable to Cleanup Without Line Processing (OpenToonz port)
- File Path Processing Using Regular Expression (OpenToonz port)
- Multi-Thread FFMPEG and responsive finalizing window (OpenToonz port)
- Improve Tablet Response (Windows Only) (OpenToonz port)

## [1.2.0] - 2021-05-03

### Added

- Motion Path Panel
- Add Import Preferences option
- Add temp tool hold switch time as preference
- Support MOV formats through ffmpeg
- Add Shader Fx: HSL Blend GPU
- Incorporate Rhubarb Lip Sync
- Recent Projects and Default Project Location
- Add Recent Projects to Export Scene Popup
- Add individual reset room layout options
- Added Command Bar to 2D room. Added History Room.
- Option to show the color of the parent column in the header
- Add start Short Play from Live View
- Segment Eraser for Smart Raster Levels
- Fill with gaps on Smart Raster levels
- Add CrashRpt (Windows only)
- New Fx : Bloom Iwa Fx (OpenToonz port)
- Expression Reference Manager (OpenToonz port)
- feat: FxParticles preset simulating the Flock effect (OpenToonz port)
- New Color Keys Global Palette (OpenToonz port)

### Changed

- Update external app handling
- Export Scene enhancements
- Restore and update project management
- Replace coded default Studio Ghibli rooms with Tahoma2D rooms
- Make setting markers easier
- Modify Output Settings Layout
- Statusbar enhancements
- Display file paths in Scene Cast
- Motion Blur Fx Iwa - default value change (OpenToonz port)
- Convert To Vector : Align Boundary Strokes Direction Option (OpenToonz port)
- Icons for all commands (OpenToonz port)
- Stylesheet Fixes (OpenToonz port)
- Add Tooltype Icons (OpenToonz port)

### Fixed

- Fix loading brush presets or last brush used
- Fix default permissions on tahomastuff
- Fix username room layout directory path
- Fix translation of menubar
- Fix crash moving cell with shaderfx
- Fix motion path issues
- Fix guidedstroke in sceneviewer
- Fix seconds working after 1 minute in timecode.
- Fix resize handles not showing on hover
- Fix vector skeleton picking
- Snap Center Point to Motion Path
- xdg-data: rename files
- Fix multiple levels using the same file
- Hide global set keys when no skeleton created
- Fix pegbar reparenting when column empties
- Fix saving degree symbol
- Update tahoma2d links to new path
- Re-allow space shortcut
- Fix macOS crash on startup
- Change shortcut usage and navigation while renaming cell
- Breakout frame creation options from Autocreate preference
- Fix crashing on exit with floating level strip
- Allow ending underscore in output filename
- Stop renaming OpenToonz project files
- Block new frames in Single Frame levels
- Fix crash when quitting with floating levelstrip
- Fix static macOS ffmpeg builds
- Stop panels from resizing on startup
- Fix outdated Tahoma2D links
- Fix Lazy Nezumi Pro hooking incorrectly
- Fix console warning messages
- Fix saving memos on macOS
- Fix typos
- Block saving scene outside current project
- Fix missing Linux event mouseMove with tabletMove
- Fix loading files with multiple dots
- Fix Raster Rectangular Fill Tool (OpenToonz port)
- Fix Crash on Switching Rooms (OpenToonz port)
- Fix incorrect xml in tnz file (OpenToonz port)
- Glare Fx Iwa Revised (OpenToonz port)
- Fix Camera Settings response (OpenToonz port)
- Fix the onion skin fill and picking range for vector levels (OpenToonz port)
- Fix crash on deleting stage schematic node (OpenToonz port)
- Fix crash on saving palette level (OpenToonz port)
- Cut Level Strip Selection Behavior Fix (OpenToonz port)
- Fix redundant preview invocation (OpenToonz port)
- Fix Color Tab on Style Editor (OpenToonz port)
- Fix Deleting Fx Link (OpenToonz port)
- Fix new level undo (OpenToonz port)
- Fix crash in Type tool on linux (OpenToonz port)
- Fix for Full Screen Mode on X11 with Multiple Monitors (OpenToonz port)
- Fix Viewer Window Disappearing on Undock (OpenToonz port)
- Fix Icon Rendering for Hdpi (OpenToonz port)
- Fix Temp Icon Over State in Menu (OpenToonz port)
- Fix for Full Screen Mode on Windows with Multiple Monitors (OpenToonz port)
- Fix Scene Cast to Update on Pasting (OpenToonz port)
- Fix mac record audio crash (OpenToonz port)
- Fix Level Strip Inbetween Text (OpenToonz port)
- Fix Batch Rendering on Mac (OpenToonz port)
- Premultiply on Loading PNG Images (OpenToonz port)
- Fix full screen feature (OpenToonz port)
- Fix Main Window Full Screen (OpenToonz port)
- Fix Viewer to refresh on drawing with raster brush (OpenToonz port)
- Fix undo when removing entire cells in the column (OpenToonz port)
- Fix XDTS popup duplicative fields (OpenToonz port)
- Fix Palette Glitches (OpenToonz port)
- Fix Alt-drag Fx Node Insertion (OpenToonz port)
- Fix Crash on Inserting New Frame (OpenToonz port)
- Fix OpenGL Line Smoothing (OpenToonz port)
- Fix pasting raster to cell (OpenToonz port)
- Fix wrong blending (OpenToonz port)
- Fix Cleanup to Set Image Data (OpenToonz port)
- Fix Crash on Launch with Shrinked ImagViewer (OpenToonz port)
- Fix Style Shortcut When Full Screen Mode (OpenToonz port)
- Fix color model pick mode (OpenToonz port)
- Fix Hdpi Icons (OpenToonz port)
- Fix Crash on Recording Audio on Windows (OpenToonz port)
- Fix Geometric tool / Type tool first point offset (OpenToonz port)
- Fix Shift and Trace scaling (OpenToonz port)
- Fix PSD Loading (OpenToonz port)
- Fix Cell Selection on Click (OpenToonz port)
- Fix tool options' shortcuts to work without opening tool options bar (OpenToonz port)
- Fix Shortcut Input (OpenToonz port)

### Removed

- Remove Camera Capture Popup Window
- Remove all things Quicktime
- Remove libusb from scripts and fix some name references
- Remove some using declaration of std (OpenToonz port)
- Remove unused directory and some unused files (OpenToonz port)

## [1.1.0] - 2020-10-30

### Added
- Vanishing points and advanced straight line drawing on Vector levels.
- You can now snap straight lines to 15 degree intervals for greater precision.
- The vector brush can now draw behind, auto close and auto fill strokes.
- There is a massive new grid and guides overlay system. You can add the rule of thirds, vertical and horizontal grids, isometric grids, and a horizon. These are customizable as well.
- We now have Linux builds thanks to @manongjohn
- You can now paste images into Tahoma2D copied from other programs.
- You can now make the main window transparent for reference drawing.
- You can now temporarily toggle snapping with the vector Geometric Tool using Control + Shift. This still works on the vector brush too.

### Changed
- The name is now Tahoma2D
- The Windows menu is now the Panels menu
- In many places, we used to refer to the Xsheet when we were really referring to the scene as a whole. So in some parts of the interface, the word scene is now used instead of Xsheet.
- Sub-Xsheets are now Sub-Scenes.
- The camera view mode has an adjustable transparency slider.
- The ruler marks are now based on pixels.

### Fixed
- Webcams on Linux should now work.
- Over 100 bug fixes, tweaks and new features.

## [1.0.1] - 2020-09-04

- A few bugs were discovered in 1.0. This fixes them. Enjoy.

## [1.0.0] - 2020-09-01

- Initial release forked from OpenToonz with numerous changes

[^1]: Only the cell mark changes were taken from this PR. Rest will be in v1.5
[^2]: Only the UHD changes were taken from this PR. Rest will be in v1.5
[^3]: Only changes related to scene saving were taken from this PR. Rest will be in v1.5
[^4]: Only fixes applicable for this release were applied. Rest will be in v1.5
[^5]: Only fixes from this OpenToonz port PR were applied to this release. Rest will be in v1.5
[^6]: A few fixes from this OpenToonz port PR were moved to the v1.5.3 fix release
[^7]: Followup fixes made directly in Tahoma2D related to OpenToonz ports
[^8]: Modified to work with T2D changes
[^9]: Partial. Some changes on unported OT changes not applied
[^10]: Partial. Changes not relevant to T2D were not applied
[^11]: Partial. Similar changes already in T2D were not applied

[1.6.0]: https://github.com/tahoma2d/tahoma2d/compare/v1.5.4...v1.6
[1.5.4]: https://github.com/tahoma2d/tahoma2d/compare/v1.5.3...v1.5.4
[1.5.3]: https://github.com/tahoma2d/tahoma2d/compare/v1.5.2...v1.5.3
[1.5.2]: https://github.com/tahoma2d/tahoma2d/compare/v1.5.1...v1.5.2
[1.5.1]: https://github.com/tahoma2d/tahoma2d/compare/v1.5...v1.5.1
[1.5.0]: https://github.com/tahoma2d/tahoma2d/compare/v1.4.5...v1.5
[1.4.5]: https://github.com/tahoma2d/tahoma2d/compare/v1.4.4...v1.4.5
[1.4.4]: https://github.com/tahoma2d/tahoma2d/compare/v1.4.3...v1.4.4
[1.4.3]: https://github.com/tahoma2d/tahoma2d/compare/v1.4.2...v1.4.3
[1.4.2]: https://github.com/tahoma2d/tahoma2d/compare/v1.4.1...v1.4.2
[1.4.1]: https://github.com/tahoma2d/tahoma2d/compare/v1.4...v1.4.1
[1.4.0]: https://github.com/tahoma2d/tahoma2d/compare/v1.3.1...v1.4
[1.3.1]: https://github.com/tahoma2d/tahoma2d/compare/v1.3...v1.3.1
[1.3.0]: https://github.com/tahoma2d/tahoma2d/compare/v1.2...v1.3
[1.2.0]: https://github.com/tahoma2d/tahoma2d/compare/v1.1...v1.2
[1.1.0]: https://github.com/tahoma2d/tahoma2d/compare/v1.0.1...v1.1
[1.0.1]: https://github.com/tahoma2d/tahoma2d/compare/v1.0...v1.0.1
[1.0.0]: https://github.com/tahoma2d/tahoma2d/releases/tag/v1.0
