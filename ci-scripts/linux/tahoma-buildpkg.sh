#!/bin/bash
source /opt/qt515/bin/qt515-env.sh || exit;

echo ">>> Temporary install of Tahoma2D"
SCRIPTPATH=`dirname "$0"`
export BUILDDIR=$SCRIPTPATH/../../toonz/build
cd $BUILDDIR || exit;
sudo make install || exit;

sudo ldconfig || exit;

echo ">>> Creating appDir"
if [ -d appdir ]
then
   rm -rf appdir
fi
mkdir -p appdir/usr || exit;

echo ">>> Copy and configure Tahoma2D installation in appDir"
cp -r /opt/tahoma2d/* appdir/usr || exit;
cp appdir/usr/share/applications/*.desktop appdir || exit;
cp appdir/usr/share/icons/hicolor/*/apps/*.png appdir || exit;
mv appdir/usr/lib/tahoma2d/* appdir/usr/lib || exit;
rmdir appdir/usr/lib/tahoma2d || exit;

echo ">>> Creating Tahoma2D directory"
if [ -d Tahoma2D ]
then
   rm -rf Tahoma2D
fi
mkdir Tahoma2D || exit;

echo ">>> Copying stuff to Tahoma2D/tahomastuff"

mv appdir/usr/share/tahoma2d/stuff Tahoma2D/tahomastuff || exit;
chmod -R 777 Tahoma2D/tahomastuff || exit;
rmdir appdir/usr/share/tahoma2d || exit;

find Tahoma2D/tahomastuff -name .gitkeep -exec rm -f {} \; || exit;

if [ -d ../../thirdparty/apps/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to Tahoma2D/ffmpeg"
   if [ -d Tahoma2D/ffmpeg ]
   then
      rm -rf Tahoma2D/ffmpeg
   fi
   mkdir -p Tahoma2D/ffmpeg || exit;
   cp -R ../../thirdparty/apps/ffmpeg/bin/ffmpeg ../../thirdparty/apps/ffmpeg/bin/ffprobe Tahoma2D/ffmpeg || exit;
   chmod -R 755 Tahoma2D/ffmpeg || exit;
fi

if [ -d ../../thirdparty/apps/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to Tahoma2D/rhubarb"
   if [ -d Tahoma2D/rhubarb ]
   then
      rm -rf Tahoma2D/rhubarb
   fi
   mkdir -p Tahoma2D/rhubarb || exit;
   cp -R ../../thirdparty/apps/rhubarb/rhubarb ../../thirdparty/apps/rhubarb/res Tahoma2D/rhubarb || exit;
   chmod 755 -R Tahoma2D/rhubarb || exit;
fi

if [ -d ../../thirdparty/canon/Library ]
then
   echo ">>> Copying canon libraries"
   cp -R ../../thirdparty/canon/Library/x86_64/* appdir/usr/lib || exit;
fi

echo ">>> Copying libghoto2 supporting directories"
cp -r /usr/local/lib/libgphoto2 appdir/usr/lib || exit;
cp -r /usr/local/lib/libgphoto2_port appdir/usr/lib || exit;

rm appdir/usr/lib/libgphoto2/print-camera-list || exit;
find appdir/usr/lib/libgphoto2* -name *.la -exec rm -f {} \; || exit;
find appdir/usr/lib/libgphoto2* -name *.so -exec patchelf --set-rpath '$ORIGIN/../..' {} \; || exit;

echo ">>> Creating Tahoma2D/Tahoma2D.AppImage"

if [ ! -f linuxdeployqt*.AppImage ]
then
   wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" || exit;
   chmod a+x linuxdeployqt*.AppImage || exit;
fi

export LD_LIBRARY_PATH=appdir/usr/lib/tahoma2d
./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma2D -bundle-non-qt-libs -verbose=0 -always-overwrite -no-strip \
   -executable=appdir/usr/bin/lzocompress \
   -executable=appdir/usr/bin/lzodecompress \
   -executable=appdir/usr/bin/tcleanup \
   -executable=appdir/usr/bin/tcomposer \
   -executable=appdir/usr/bin/tconverter \
   -executable=appdir/usr/bin/tfarmcontroller \
   -executable=appdir/usr/bin/tfarmserver  || exit;

rm appdir/AppRun || exit;
cp ../sources/scripts/AppRun appdir || exit;
chmod 775 appdir/AppRun || exit;

./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma2D -appimage -no-strip  || exit;

mv Tahoma2D*.AppImage Tahoma2D/Tahoma2D.AppImage || exit;

echo ">>> Creating Tahoma2D Linux package"

tar zcf Tahoma2D-linux.tar.gz Tahoma2D || exit;

