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
wget https://github.com/tahoma2d/FFmpeg/releases/download/v5.0.0/ffmpeg-5.0.0-linux64-static-lgpl.zip
unzip ffmpeg-5.0.0-linux64-static-lgpl.zip 
mv ffmpeg-5.0.0-linux64-static-lgpl ffmpeg


echo ">>> Getting Rhubarb Lip Sync"
if [ -d rhubarb ]
then
   rm -rf rhubarb ]
fi
wget https://github.com/tahoma2d/rhubarb-lip-sync/releases/download/v1.10.3/rhubarb-lip-sync-tahoma2d-linux.zip
unzip rhubarb-lip-sync-tahoma2d-linux.zip -d rhubarb

