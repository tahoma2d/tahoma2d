#!/bin/bash
export QTDIR=/usr/local/opt/qt
export TOONZDIR=toonz/build/toonz

# If found, use Xcode Release build
if [ -d $TOONZDIR/Release ]
then
   export TOONZDIR=$TOONZDIR/Release
fi

echo ">>> Copying stuff to $TOONZDIR/Tahoma.app/tahomastuff"
cp -R stuff $TOONZDIR/Tahoma.app/tahomastuff

if [ -d thirdparty/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to $TOONZDIR/Tahoma.app/ffmpeg"
   cp -R thirdparty/ffmpeg/bin $TOONZDIR/Tahoma.app/ffmpeg
fi

if [ -d thirdparty/canon/Framework ]
then
   echo ">>> Copying canon framework to $TOONZDIR/Tahoma.app/Contents/Frameworks/EDSDK.Framework"
   cp -R thirdparty/canon/Framework $TOONZDIR/Tahoma.app/Contents/Frameworks
fi

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
   
echo ">>> Creating Tahoma-osx.dmg"

$QTDIR/bin/macdeployqt $TOONZDIR/Tahoma.app -dmg -verbose=0

mv $TOONZDIR/Tahoma.dmg $TOONZDIR/../Tahoma-osx.dmg
