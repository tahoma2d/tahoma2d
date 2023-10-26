cd toonz\build

echo ">>> Creating Tahoma2D directory"

IF EXIST Tahoma2D rmdir /S /Q Tahoma2D

mkdir Tahoma2D

echo ">>> Copy and configure Tahoma2D installation"

copy /y RelWithDebInfo\*.* Tahoma2D

copy /Y ..\..\thirdparty\freeglut\bin\x64\freeglut.dll Tahoma2D
copy /Y ..\..\thirdparty\glew\glew-1.9.0\bin\64bit\glew32.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libiconv-2.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libintl-8.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libjson-c-2.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libmypaint-1-4-0.dll Tahoma2D

IF EXIST C:\tools\opencv (
   copy /Y "C:\tools\opencv\build\x64\vc14\bin\opencv_world451.dll" Tahoma2D
) ELSE (
   copy /Y "C:\opencv\451\build\x64\vc14\bin\opencv_world451.dll" Tahoma2D
)

IF EXIST ..\..\thirdparty\canon\Header (
   copy /Y ..\..\thirdparty\canon\Dll\EDSDK.dll Tahoma2D
   copy /Y ..\..\thirdparty\canon\Dll\EdsImage.dll Tahoma2D
)

IF EXIST ..\..\thirdparty\libgphoto2\include (
   xcopy /Y /E ..\..\thirdparty\libgphoto2\bin Tahoma2D
)

REM Remove ILK files
del Tahoma2D\*.ilk

echo ">>> Configuring Tahoma2D.exe for deployment"

REM Setup for local builds
set QT_PATH=C:\Qt\5.9.7\msvc2019_64

REM These are effective when running from Actions/Appveyor
IF EXIST D:\a\tahoma2d\tahoma2d\thirdparty\qt\5.9\msvc2019_64 set QT_PATH=D:\a\tahoma2d\tahoma2d\thirdparty\qt\5.9\msvc2019_64

set VCINSTALLDIR="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC"
IF EXIST "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC" set VCINSTALLDIR="C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC"

%QT_PATH%\bin\windeployqt.exe Tahoma2D\Tahoma2D.exe

del /A- /S Tahoma2D\tahomastuff\*.gitkeep

IF EXIST ..\..\thirdparty\apps\ffmpeg\bin (
   echo ">>> Copying FFmpeg to Tahoma2D\ffmpeg"
   IF EXIST Tahoma2D\ffmpeg rmdir /S /Q Tahoma2D\ffmpeg
   mkdir Tahoma2D\ffmpeg
   copy /Y ..\..\thirdparty\apps\ffmpeg\bin\ffmpeg.exe Tahoma2D\ffmpeg
   copy /Y ..\..\thirdparty\apps\ffmpeg\bin\ffprobe.exe Tahoma2D\ffmpeg
)

IF EXIST ..\..\thirdparty\apps\rhubarb (
   echo ">>> Copying Rhubarb Lip Sync to Tahoma2D\rhubarb"
   IF EXIST Tahoma2D\rhubarb rmdir /S /Q Tahoma2D\rhubarb
   mkdir Tahoma2D\rhubarb
   copy /Y ..\..\thirdparty\apps\rhubarb\rhubarb.exe Tahoma2D\rhubarb
   xcopy /Y /E /I ..\..\thirdparty\apps\rhubarb\res "Tahoma2D\rhubarb\res"
)

echo ">>> Creating Tahoma2D Windows Installer"
IF NOT EXIST installer mkdir installer
cd installer

rmdir /S /Q program
rmdir /S /Q stuff

xcopy /Y /E /I ..\Tahoma2D program

xcopy /Y /E /I ..\..\..\stuff stuff

python ..\..\installer\windows\filelist_python3.py %cd%
ISCC.exe /I. /O.. ..\..\installer\windows\setup.iss

cd ..

echo ">>> Creating Tahoma2D Windows Portable package"

xcopy /Y /E /I ..\..\stuff Tahoma2D\tahomastuff

IF EXIST Tahoma2D-portable-win.zip del Tahoma2D-portable-win.zip
7z a Tahoma2D-portable-win.zip Tahoma2D


cd ../..
