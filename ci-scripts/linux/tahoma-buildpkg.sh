#!/bin/bash
source /opt/qt59/bin/qt59-env.sh

echo ">>> Temporary install of Tahoma2D"
export BUILDDIR=$(pwd)/toonz/build
cd $BUILDDIR
sudo make install

sudo ldconfig

echo ">>> Creating appDir"
if [ -d appdir ]
then
   rm -rf appdir
fi
mkdir -p appdir/usr

echo ">>> Copy and configure Tahoma2D installation in appDir"
cp -r /opt/tahoma2d/* appdir/usr
cp appdir/usr/share/applications/*.desktop appdir
cp appdir/usr/share/icons/hicolor/*/apps/*.png appdir
mv appdir/usr/lib/tahoma2d/* appdir/usr/lib
rmdir appdir/usr/lib/tahoma2d

echo ">>> Creating Tahoma2D directory"
if [ -d Tahoma2D ]
then
   rm -rf Tahoma2D
fi
mkdir Tahoma2D

echo ">>> Copying stuff to Tahoma2D/tahomastuff"

mv appdir/usr/share/tahoma2d/stuff Tahoma2D/tahomastuff
chmod -R 777 Tahoma2D/tahomastuff
rmdir appdir/usr/share/tahoma2d

if [ -d ../../thirdparty/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to Tahoma2D/ffmpeg"
   if [ -d Tahoma2D/ffmpeg ]
   then
      rm -rf Tahoma2D/ffmpeg
   fi
   cp -R ../../thirdparty/ffmpeg/bin Tahoma2D/ffmpeg
fi

if [ -d ../../thirdparty/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to Tahoma2D/rhubarb"
   if [ -d Tahoma2D/rhubarb ]
   then
      rm -rf Tahoma2D/rhubarb
   fi
   cp -R ../../thirdparty/rhubarb Tahoma2D/rhubarb
fi

echo ">>> Creating Tahoma2D/Tahoma2D.AppImage"

if [ ! -f linuxdeployqt*.AppImage ]
then
   wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
   chmod a+x linuxdeployqt*.AppImage
fi

export LD_LIBRARY_PATH=appdir/usr/lib/tahoma2d
./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma2D -bundle-non-qt-libs -verbose=0 -always-overwrite \
   -executable=appdir/usr/bin/lzocompress \
   -executable=appdir/usr/bin/lzodecompress \
   -executable=appdir/usr/bin/tcleanup \
   -executable=appdir/usr/bin/tcomposer \
   -executable=appdir/usr/bin/tconverter \
   -executable=appdir/usr/bin/tfarmcontroller \
   -executable=appdir/usr/bin/tfarmserver 
./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma2D -appimage

mv Tahoma2D*.AppImage Tahoma2D/Tahoma2D.AppImage

echo ">>> Creating Tahoma2D Linux package"

tar zcf Tahoma2D-linux.tar.gz Tahoma2D

