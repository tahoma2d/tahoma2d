#!/bin/bash
export TAHOMA2DVERSION=1.5.4
#source /opt/qt515/bin/qt515-env.sh

echo ">>> Temporary install of Tahoma2D"
SCRIPTPATH=`dirname "$0"`
export BUILDDIR=$SCRIPTPATH/../../toonz/build
cd $BUILDDIR
# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1))
sudo make -j "$parallel" install

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
cp appdir/usr/share/icons/hicolor/128x128/apps/*.png appdir
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

if [ -d ../../thirdparty/canon/Library ]
then
   echo ">>> Copying canon libraries"
   cp -R ../../thirdparty/canon/Library/x86_64/* appdir/usr/lib
fi

echo ">>> Copying libghoto2 supporting directories"
cp -r /usr/local/lib/libgphoto2 appdir/usr/lib
cp -r /usr/local/lib/libgphoto2_port appdir/usr/lib

rm appdir/usr/lib/libgphoto2/print-camera-list
find appdir/usr/lib/libgphoto2* -name *.la -exec rm -f {} \;
find appdir/usr/lib/libgphoto2* -name *.so -exec patchelf --set-rpath '$ORIGIN/../..' {} \;

echo ">>> Creating Tahoma2D/Tahoma2D.AppImage"

export EXTRA_QT_PLUGINS="waylandcompositor"
export EXTRA_PLATFORM_PLUGINS="libqwayland-egl.so;libqwayland-generic.so"

wget "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
wget "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"

chmod +x ./linuxdeploy-x86_64.AppImage
chmod +x ./linuxdeploy-plugin-qt-x86_64.AppImage

export LDAI_OUTPUT="Tahoma2D.AppImage"

./linuxdeploy-x86_64.AppImage --appdir appdir --plugin qt --output appimage

mv Tahoma2D*.AppImage Tahoma2D/Tahoma2D.AppImage

echo ">>> Creating Tahoma2D Linux package"

tar zcf Tahoma2D-linux.tar.gz Tahoma2D

echo ">>> Creating Tahoma2D Debian Package"

chmod +x ../installer/linux/deb-creator/debcreator.sh 

../installer/linux/deb-creator/debcreator.sh \
 -p $TAHOMA2DVERSION \
 -v $TAHOMA2DVERSION \
 -t ../installer/linux/deb-creator/deb-template \
 -x ./appdir \
 -f ./Tahoma2D/ffmpeg \
 -r ./Tahoma2D/rhubarb \
 -s ../../stuff

 mv tahoma2d_*_amd64.deb Tahoma2D-linux.deb
