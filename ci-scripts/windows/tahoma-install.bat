choco install opencv --version=4.5.1
choco install boost-msvc-14.2

REM Install Qt 5.15
curl -fsSL -o Qt5.15.2_msvc2019_64.zip https://github.com/tahoma2d/qt5/releases/download/v5.15.2/Qt5.15.2_msvc2019_64.zip
7z x Qt5.15.2_msvc2019_64.zip
rename Qt5.15.2_msvc2019_64 5.15
mkdir thirdparty\qt
move 5.15 thirdparty\qt
