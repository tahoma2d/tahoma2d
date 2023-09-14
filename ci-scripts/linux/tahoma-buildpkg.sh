#!/bin/bash
source /opt/qt515/bin/qt515-env.sh

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

find Tahoma2D/tahomastuff -name .gitkeep -exec rm -f {} \;

if [ -d ../../thirdparty/apps/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to Tahoma2D/ffmpeg"
   if [ -d Tahoma2D/ffmpeg ]
   then
      rm -rf Tahoma2D/ffmpeg
   fi
   mkdir -p Tahoma2D/ffmpeg
   cp -R ../../thirdparty/apps/ffmpeg/bin/ffmpeg ../../thirdparty/apps/ffmpeg/bin/ffprobe Tahoma2D/ffmpeg
   chmod -R 755 Tahoma2D/ffmpeg
fi

if [ -d ../../thirdparty/apps/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to Tahoma2D/rhubarb"
   if [ -d Tahoma2D/rhubarb ]
   then
      rm -rf Tahoma2D/rhubarb
   fi
   mkdir -p Tahoma2D/rhubarb
   cp -R ../../thirdparty/apps/rhubarb/rhubarb ../../thirdparty/apps/rhubarb/res Tahoma2D/rhubarb
   chmod 755 -R Tahoma2D/rhubarb
fi

echo ">>> Copying libghoto2 supporting directories"
cp -r /usr/local/lib/libgphoto2 appdir/usr/lib
cp -r /usr/local/lib/libgphoto2_port appdir/usr/lib

rm appdir/usr/lib/libgphoto2/print-camera-list
find appdir/usr/lib/libgphoto2* -name *.la -exec rm -f {} \;
find appdir/usr/lib/libgphoto2* -name *.so -exec patchelf --set-rpath '$ORIGIN/../..' {} \;

echo ">>> Creating Tahoma2D/Tahoma2D.AppImage"

if [ ! -f linuxdeployqt*.AppImage ]
then
   wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
   chmod a+x linuxdeployqt*.AppImage
fi

export LD_LIBRARY_PATH=appdir/usr/lib/tahoma2d
./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma2D -bundle-non-qt-libs -verbose=0 -always-overwrite -no-strip \
   -executable=appdir/usr/bin/lzocompress \
   -executable=appdir/usr/bin/lzodecompress \
   -executable=appdir/usr/bin/tcleanup \
   -executable=appdir/usr/bin/tcomposer \
   -executable=appdir/usr/bin/tconverter \
   -executable=appdir/usr/bin/tfarmcontroller \
   -executable=appdir/usr/bin/tfarmserver 

rm appdir/AppRun
cp ../sources/scripts/AppRun appdir
chmod 775 appdir/AppRun

./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma2D -appimage -no-strip 

mv Tahoma2D*.AppImage Tahoma2D/Tahoma2D.AppImage

echo ">>> Creating Tahoma2D Linux package"

tar zcf Tahoma2D-linux.tar.gz Tahoma2D

