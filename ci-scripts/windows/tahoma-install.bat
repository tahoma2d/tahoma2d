choco install opencv --version=4.5.1
choco install boost-msvc-14.1

REM Install Qt 5.11
curl -fsSL -o Qt5.11.3_msvc2017_64.zip https://github.com/tahoma2d/qt5/releases/download/v5.11.3/Qt5.11.3_msvc2017_64.zip
7z x Qt5.11.3_msvc2017_64.zip
rename Qt5.11.3_msvc2017_64 5.11
mkdir thirdparty\qt
move 5.11 thirdparty\qt
