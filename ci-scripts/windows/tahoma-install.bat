choco install opencv --version=4.11.0
choco install boost-msvc-14.3 --version=1.87.0

mkdir thirdparty\qt

REM Install Custom Qt 5.15.2 with WinTab support
curl -fsSL -o Qt5.15.2_wintab.zip https://github.com/tahoma2d/qt5/releases/download/v5.15.2/Qt5.15.2_wintab.zip
7z x Qt5.15.2_wintab.zip
move Qt5.15.2_wintab\5.15.2_wintab thirdparty\qt
