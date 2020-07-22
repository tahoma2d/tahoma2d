#!/bin/bash
export QTDIR=/usr/local/Cellar/qt/5.15.0
export TOONZDIR=toonz/build/toonz/Release

echo ">>> Copying stuff to $TOONZDIR/Tahoma.app/portablestuff"
cp -R stuff $TOONZDIR/Tahoma.app/tahomastuff

echo ">>> Configuring Tahoma.app for deployment"

$QTDIR/bin/macdeployqt $TOONZDIR/Tahoma.app -verbose=0 -always-overwrite \
   -executable=$TOONZDIR/Tahoma.app/Contents/MacOS/lzocompress \
   -executable=$TOONZDIR/Tahoma.app/Contents/MacOS/lzodecompress \
   -executable=$TOONZDIR/Tahoma.app/Contents/MacOS/tcleanup \
   -executable=$TOONZDIR/Tahoma.app/Contents/MacOS/tcomposer \
   -executable=$TOONZDIR/Tahoma.app/Contents/MacOS/tconverter \
   -executable=$TOONZDIR/Tahoma.app/Contents/MacOS/tfarmcontroller \
   -executable=$TOONZDIR/Tahoma.app/Contents/MacOS/tfarmserver 

echo ">>> Correcting library paths"
for X in `find $TOONZDIR/Tahoma.app/Contents -type f -name *.dylib -exec otool -l {} \; | grep -e "^toonz" -e"name \/usr\/local" | sed -e"s/://" -e"s/ (.*$//" -e"s/^ *name //"`
do
   Z=`echo $X | cut -c 1-1`
   if [ "$Z" != "/" ]
   then
      LIBFILE=$X
   else
      Y=`basename $X`
      W=`basename $LIBFILE`
      if [ -f $TOONZDIR/Tahoma.app/Contents/Frameworks/$Y -a "$Y" != "$W" ]
      then
         echo "Fixing $X in $LIBFILE"
         install_name_tool -change $X @executable_path/../Frameworks/$Y $LIBFILE
      fi
   fi
done
   
echo ">>> Creating Tahoma-OTX-osx.dmg"

$QTDIR/bin/macdeployqt $TOONZDIR/Tahoma.app -dmg -verbose=0

mv $TOONZDIR/Tahoma.dmg $TOONZDIR/../Tahoma-osx.dmg