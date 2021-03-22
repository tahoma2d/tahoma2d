#!/bin/bash
cd thirdparty

if [ ! -d apps ]
then
   mkdir apps
fi
cd apps
echo "*" >| .gitignore

echo ">>> Getting FFmpeg"
if [ -d ffmpeg ]
then
   rm -rf ffmpeg
fi
wget https://github.com/tahoma2d/FFmpeg/releases/download/v4.3.1/ffmpeg-4.3.1-macos64-static-lgpl.zip
unzip ffmpeg-4.3.1-macos64-static-lgpl.zip 
mv ffmpeg-4.3.1-macos64-static-lgpl ffmpeg


echo ">>> Getting Rhubarb Lip Sync"
if [ -d rhubarb ]
then
   rm -rf rhubarb
fi
wget https://github.com/tahoma2d/rhubarb-lip-sync/releases/download/v1.10.2/rhubarb-lip-sync-tahoma2d-osx.zip
unzip rhubarb-lip-sync-tahoma2d-osx.zip -d rhubarb

