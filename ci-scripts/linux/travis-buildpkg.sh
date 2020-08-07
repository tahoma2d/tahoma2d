#!/bin/bash
source /opt/qt59/bin/qt59-env.sh

echo ">>> Temporary install of Tahoma"
export BUILDDIR=$(pwd)/toonz/build
cd $BUILDDIR
sudo make install

echo ">>> Creating appDir"
mkdir -p appdir/usr

echo ">>> Copy and configure Tahoma installation in appDir"
cp -r /opt/tahoma/* appdir/usr
cp appdir/usr/share/applications/*.desktop appdir
cp appdir/usr/share/icons/hicolor/*/apps/*.png appdir
mv appdir/usr/lib/tahoma/* appdir/usr/lib
rmdir appdir/usr/lib/tahoma

echo ">>> Creating Tahoma directory"
mkdir Tahoma

echo ">>> Copying stuff to Tahoma/tahomastuff"

mv appdir/usr/share/tahoma/stuff Tahoma/tahomastuff
chmod -R 777 Tahoma/tahomastuff
rmdir appdir/usr/share/tahoma

echo ">>> Creating Tahoma/Tahoma.AppImage"

wget -c "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage"
chmod a+x linuxdeployqt*.AppImage

export LD_LIBRARY_PATH=appdir/usr/lib/tahoma
./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma -bundle-non-qt-libs -verbose=0 -always-overwrite \
   -executable=appdir/usr/bin/lzocompress \
   -executable=appdir/usr/bin/lzodecompress \
   -executable=appdir/usr/bin/tcleanup \
   -executable=appdir/usr/bin/tcomposer \
   -executable=appdir/usr/bin/tconverter \
   -executable=appdir/usr/bin/tfarmcontroller \
   -executable=appdir/usr/bin/tfarmserver 
./linuxdeployqt*.AppImage appdir/usr/bin/Tahoma -appimage

mv Tahoma*.AppImage Tahoma/Tahoma.AppImage

echo ">>> Creating Tahoma Linux package"

tar zcf Tahoma-linux.tar.gz Tahoma
