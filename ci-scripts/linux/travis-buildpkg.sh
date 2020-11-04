#!/bin/bash
source /opt/qt59/bin/qt59-env.sh

echo ">>> Temporary install of Tahoma2D"
export BUILDDIR=$(pwd)/toonz/build
cd $BUILDDIR
sudo make install

echo ">>> Creating appDir"
mkdir -p appdir/usr

echo ">>> Copy and configure Tahoma2D installation in appDir"
cp -r /opt/tahoma2d/* appdir/usr
cp appdir/usr/share/applications/*.desktop appdir
cp appdir/usr/share/icons/hicolor/*/apps/*.png appdir
mv appdir/usr/lib/tahoma2d/* appdir/usr/lib
rmdir appdir/usr/lib/tahoma2d

echo ">>> Creating Tahoma2D directory"
mkdir Tahoma2D

echo ">>> Copying stuff to Tahoma2D/tahomastuff"

mv appdir/usr/share/tahoma2d/stuff Tahoma2D/tahomastuff
chmod -R 777 Tahoma2D/tahomastuff
rmdir appdir/usr/share/tahoma2d

echo ">>> Creating Tahoma2D/Tahoma2D.AppImage"

wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt*.AppImage

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
