cd thirdparty

IF NOT EXIST apps mkdir apps

cd apps
echo * > .gitignore

echo ">>> Getting CrashRpt"

IF EXIST crashrpt rmdir /S /Q crashrpt
curl -fsSL -o crashrpt-tahoma2d-win.zip https://github.com/tahoma2d/crashrpt2/releases/download/v1.5.0.0/crashrpt-tahoma2d-win.zip
7z x crashrpt-tahoma2d-win.zip
rename crashrpt-tahoma2d-win crashrpt
IF EXIST ..\crashrpt\include rmdir /S /Q ..\crashrpt\include
IF EXIST ..\crashrpt\CrashRpt1500.lib del ..\crashrpt\CrashRpt1500.lib
move crashrpt\include ..\crashrpt
move crashrpt\CrashRpt1500.lib ..\crashrpt

echo ">>> Getting FFmpeg"

IF EXIST ffmpeg rmdir /S /Q ffmpeg
curl -fsSL -o ffmpeg-5.0.0-win64-static-lgpl.zip https://github.com/tahoma2d/FFmpeg/releases/download/v5.0.0/ffmpeg-5.0.0-win64-static-lgpl.zip
7z x ffmpeg-5.0.0-win64-static-lgpl.zip
rename ffmpeg-5.0.0-win64-static-lgpl ffmpeg

echo ">>> Getting Rhubarb Lip Sync"

IF EXIST rhubarb rmdir /S /Q rhubarb
curl -fsSL -o rhubarb-lip-sync-tahoma2d-win.zip https://github.com/tahoma2d/rhubarb-lip-sync/releases/download/v1.10.3/rhubarb-lip-sync-tahoma2d-win.zip
7z x rhubarb-lip-sync-tahoma2d-win.zip
rename rhubarb-lip-sync-tahoma2d-win rhubarb

cd ../..
