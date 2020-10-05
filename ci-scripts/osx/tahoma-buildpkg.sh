#!/bin/bash
export QTDIR=/usr/local/opt/qt
export TOONZDIR=toonz/build/toonz

# If found, use Xcode Release build
if [ -d $TOONZDIR/Release ]
then
   export TOONZDIR=$TOONZDIR/Release
fi

echo ">>> Copying stuff to $TOONZDIR/Tahoma2D.app/tahomastuff"
cp -R stuff $TOONZDIR/Tahoma2D.app/tahomastuff

if [ -d thirdparty/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to $TOONZDIR/Tahoma2D.app/ffmpeg"
   cp -R thirdparty/ffmpeg/bin $TOONZDIR/Tahoma2D.app/ffmpeg
fi

if [ -d thirdparty/canon/Framework ]
then
   echo ">>> Copying canon framework to $TOONZDIR/Tahoma2D.app/Contents/Frameworks/EDSDK.Framework"
   cp -R thirdparty/canon/Framework $TOONZDIR/Tahoma2D.app/Contents/Frameworks
fi

echo ">>> Configuring Tahoma2D.app for deployment"

$QTDIR/bin/macdeployqt $TOONZDIR/Tahoma2D.app -verbose=0 -always-overwrite \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/lzocompress \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/lzodecompress \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tcleanup \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tcomposer \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tconverter \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tfarmcontroller \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tfarmserver 

echo ">>> Correcting library paths"
for X in `find $TOONZDIR/Tahoma2D.app/Contents -type f -name *.dylib -exec otool -l {} \; | grep -e "^toonz" -e"name \/usr\/local" | sed -e"s/://" -e"s/ (.*$//" -e"s/^ *name //"`
do
   Z=`echo $X | cut -c 1-1`
   if [ "$Z" != "/" ]
   then
      LIBFILE=$X
   else
      Y=`basename $X`
      W=`basename $LIBFILE`
      if [ -f $TOONZDIR/Tahoma2D.app/Contents/Frameworks/$Y -a "$Y" != "$W" ]
      then
         echo "Fixing $X in $LIBFILE"
         install_name_tool -change $X @executable_path/../Frameworks/$Y $LIBFILE
      fi
   fi
done
   
echo ">>> Creating Tahoma2D-osx.dmg"

$QTDIR/bin/macdeployqt $TOONZDIR/Tahoma2D.app -dmg -verbose=0

mv $TOONZDIR/Tahoma2D.dmg $TOONZDIR/../Tahoma2D-osx.dmg
