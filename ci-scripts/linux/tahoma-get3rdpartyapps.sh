#!/bin/bash
cd thirdparty || exit;

if [ ! -d apps ]
then
   mkdir apps
fi
cd apps || exit;
echo "*" >| .gitignore

echo ">>> Getting FFmpeg"
if [ -d ffmpeg ]
then
   rm -rf ffmpeg
fi
wget https://github.com/tahoma2d/FFmpeg/releases/download/v5.0.0/ffmpeg-5.0.0-linux64-static-lgpl.zip || exit;
unzip ffmpeg-5.0.0-linux64-static-lgpl.zip || exit;
mv ffmpeg-5.0.0-linux64-static-lgpl ffmpeg || exit;


echo ">>> Getting Rhubarb Lip Sync"
if [ -d rhubarb ]
then
   rm -rf rhubarb ]
fi
wget https://github.com/tahoma2d/rhubarb-lip-sync/releases/download/v1.13.0/rhubarb-lip-sync-tahoma2d-linux.zip || exit;
unzip rhubarb-lip-sync-tahoma2d-linux.zip -d rhubarb || exit;

