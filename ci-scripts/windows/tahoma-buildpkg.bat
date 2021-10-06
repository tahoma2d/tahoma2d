cd toonz\build

echo ">>> Creating Tahoma2D directory"

IF EXIST Tahoma2D rmdir /S /Q Tahoma2D

mkdir Tahoma2D

echo ">>> Copy and configure Tahoma2D installation"

copy /y RelWithDebInfo\*.* Tahoma2D

REM Remove PDB and ILK files
del Tahoma2D\*.pdb
del Tahoma2D\*.ilk

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

IF EXIST ..\..\thirdparty\crashrpt\include (
   copy /Y ..\..\thirdparty\apps\crashrpt\CrashRpt1500.dll Tahoma2D
   copy /Y ..\..\thirdparty\apps\crashrpt\CrashSender1500.exe Tahoma2D
   copy /Y ..\..\thirdparty\apps\crashrpt\crashrpt_lang.ini Tahoma2D
)

echo ">>> Copying stuff to Tahoma2D\tahomastuff"

mkdir Tahoma2D\tahomastuff
xcopy /Y /E ..\..\stuff Tahoma2D\tahomastuff

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

echo ">>> Configuring Tahoma2D.exe for deployment"

REM Setup for local builds
set QT_PATH=C:\Qt\5.9.7\msvc2017_64

REM These are effective when running from Actions/Appveyor
IF EXIST D:\a\tahoma2d\tahoma2d\thirdparty\qt\5.9\msvc2017_64 set QT_PATH=D:\a\tahoma2d\tahoma2d\thirdparty\qt\5.9\msvc2017_64

%QT_PATH%\bin\windeployqt.exe Tahoma2D\Tahoma2D.exe

echo ">>> Creating Tahoma2D Windows package"

IF EXIST Tahoma2D-win.zip del Tahoma2D-win.zip
7z a Tahoma2D-win.zip Tahoma2D

IF EXIST ..\..\..\tahoma2d_symbols (
   echo ">>> Saving debugging symbols"
   mkdir ..\..\..\tahoma2d_symbols\%date:~10,4%-%date:~4,2%-%date:~7,2%
   copy /y RelWithDebInfo\*.* ..\..\..\tahoma2d_symbols\%date:~10,4%-%date:~4,2%-%date:~7,2%
) else (
   echo ">>> Creating debugging symbols package"
   IF EXIST debug-symbols.zip del debug-symbols.zip
   7z a debug-symbols.zip RelWithDebInfo\*.*
)

cd ../..
