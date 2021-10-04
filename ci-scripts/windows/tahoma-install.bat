choco install opencv --version=4.5.1
choco install boost-msvc-14.2

mkdir thirdparty\qt

REM Install Qt 5.15
REM curl -fsSL -o Qt5.15.2_msvc2019_64.zip https://github.com/tahoma2d/qt5/releases/download/v5.15.2/Qt5.15.2_msvc2019_64.zip
REM 7z x Qt5.15.2_msvc2019_64.zip
REM rename Qt5.15.2_msvc2019_64 5.15
REM move 5.15 thirdparty\qt

REM Install Custom Qt 5.15.2 with WinTab support
curl -fsSL -o Qt5.15.2_wintab.zip https://github.com/tahoma2d/qt5/releases/download/v5.15.2/Qt5.15.2_wintab.zip
7z x Qt5.15.2_wintab.zip
move Qt5.15.2_wintab\5.15.2_wintab thirdparty\qt
