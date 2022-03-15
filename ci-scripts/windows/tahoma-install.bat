choco install opencv --version=4.5.1
choco install boost-msvc-14.2

REM Install Qt 5.9
curl -fsSL -o Qt5.9.7_msvc2019_64.zip https://github.com/tahoma2d/qt5/releases/download/v5.9.7/Qt5.9.7_msvc2019_64.zip
7z x Qt5.9.7_msvc2019_64.zip
rename Qt5.9.7_msvc2019_64 5.9
mkdir thirdparty\qt
move 5.9 thirdparty\qt
