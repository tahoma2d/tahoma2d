cd thirdparty

IF NOT EXIST apps mkdir apps

cd apps
echo * > .gitignore

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
