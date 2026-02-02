@echo off

cd toonz\build

echo ">>> Creating Tahoma2D directory"

IF EXIST Tahoma2D rmdir /S /Q Tahoma2D

mkdir Tahoma2D

echo ">>> Copy Tahoma2D DLLs"

copy /y RelWithDebInfo\*.* Tahoma2D

echo ">>> Copy ThirdParty DLLs"
copy /Y ..\..\thirdparty\freeglut\bin\x64\freeglut.dll Tahoma2D
copy /Y ..\..\thirdparty\glew\glew-1.9.0\bin\64bit\glew32.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libiconv-2.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libintl-8.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libjson-c-2.dll Tahoma2D
copy /Y ..\..\thirdparty\libmypaint\dist\64\libmypaint-1-4-0.dll Tahoma2D

echo ">>> Copy OpenCV DLLs"
IF EXIST C:\tools\opencv (
   copy /Y "C:\tools\opencv\build\x64\vc16\bin\opencv_world4110.dll" Tahoma2D
) ELSE (
   copy /Y "C:\opencv\4110\build\x64\vc16\bin\opencv_world4110.dll" Tahoma2D
)

IF EXIST ..\..\thirdparty\canon\Header (
   echo ">>> Copy Canon EDSDK DLLs"
   copy /Y ..\..\thirdparty\canon\Dll\EDSDK.dll Tahoma2D
   copy /Y ..\..\thirdparty\canon\Dll\EdsImage.dll Tahoma2D
)

IF EXIST ..\..\thirdparty\libgphoto2\include (
   echo ">>> Copy Libgphoto2 DLLs"
   xcopy /Y /E ..\..\thirdparty\libgphoto2\bin Tahoma2D
)

echo ">>> Copy MSVC DLLs"
set VCINSTALLDIR="C:\Program Files\Microsoft Visual Studio\2022\Community\VC"
IF EXIST "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC" set VCINSTALLDIR="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC"
echo "VCINSTALLDIR=%VCINSTALLDIR%"

set VCRUNTIME_PATH=
for /d /r ""%VCINSTALLDIR%"" %%a in (14.*) do (
    if exist "%%a\x64" (
        for /d %%b in ("%%a\x64\*") do (
            if exist "%%b\vcruntime*.dll" (
                set "VCRUNTIME_PATH=%%b"
                goto :done
            )
        )
    )
)
:done
echo "VCRUNTIME_PATH=%VCRUNTIME_PATH%"

copy /Y "%VCRUNTIME_PATH%\vcruntime140.dll" Tahoma2D
copy /Y "%VCRUNTIME_PATH%\vcruntime140_1.dll" Tahoma2D
copy /Y "%VCRUNTIME_PATH%\msvcp140.dll" Tahoma2D
copy /Y "%VCRUNTIME_PATH%\msvcp140_1.dll" Tahoma2D
copy /Y "%VCRUNTIME_PATH%\msvcp140_2.dll" Tahoma2D

echo ">>> Configuring Tahoma2D.exe for deployment"

REM Setup for local builds
set QT_PATH=C:\Qt\5.15.2_wintab\msvc2019_64

REM These are effective when running from Actions/Appveyor
IF EXIST ..\..\thirdparty\qt\5.15.2_wintab\msvc2019_64 set QT_PATH=..\..\thirdparty\qt\5.15.2_wintab\msvc2019_64
echo "QT_PATH=%QT_PATH%"


%QT_PATH%\bin\windeployqt.exe Tahoma2D\Tahoma2D.exe --opengl


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

echo ">>> Remove unnecessary files"
REM Remove github keep files
del /A- /S ..\..\stuff\*.gitkeep

echo ">>> Creating Tahoma2D Windows Installer"
IF NOT EXIST installer mkdir installer
cd installer

IF EXIST program rmdir /S /Q program
xcopy /Y /E /I ..\Tahoma2D program

IF EXIST stuff rmdir /S /Q stuff
xcopy /Y /E /I ..\..\..\stuff stuff

python ..\..\installer\windows\filelist_python3.py %cd%
ISCC.exe /I. /O.. ..\..\installer\windows\setup.iss

cd ..

echo ">>> Creating Tahoma2D Windows Portable package"

xcopy /Y /E /I ..\..\stuff Tahoma2D\tahomastuff

IF EXIST Tahoma2D-portable-win.zip del Tahoma2D-portable-win.zip
7z a Tahoma2D-portable-win.zip Tahoma2D


cd ../..
