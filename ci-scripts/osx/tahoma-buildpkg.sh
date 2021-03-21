#!/bin/bash
if [ -d /usr/local/Cellar/qt@5 ]
then
   export QTDIR=/usr/local/opt/qt@5
else
   export QTDIR=/usr/local/opt/qt
fi
export TOONZDIR=toonz/build/toonz

# If found, use Xcode Release build
if [ -d $TOONZDIR/Release ]
then
   export TOONZDIR=$TOONZDIR/Release
fi

echo ">>> Copying stuff to $TOONZDIR/Tahoma2D.app/tahomastuff"
if [ -d $TOONZDIR/Tahoma2D.app/tahomastuff ]
then
   # In case of prior builds, replace stuff folder
   rm -rf $TOONZDIR/Tahoma2D.app/tahomastuff
fi
cp -R stuff $TOONZDIR/Tahoma2D.app/tahomastuff
chmod -R 777 $TOONZDIR/Tahoma2D.app/tahomastuff

if [ -d thirdparty/apps/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to $TOONZDIR/Tahoma2D.app/ffmpeg"
   if [ -d $TOONZDIR/Tahoma2D.app/ffmpeg ]
   then
      # In case of prior builds, replace ffmpeg folder
      rm -rf $TOONZDIR/Tahoma2D.app/ffmpeg
   fi
   mkdir $TOONZDIR/Tahoma2D.app/ffmpeg
   cp -R thirdparty/apps/ffmpeg/bin/ffmpeg thirdparty/apps/ffmpeg/bin/ffprobe $TOONZDIR/Tahoma2D.app/ffmpeg
fi

if [ -d thirdparty/apps/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to $TOONZDIR/Tahoma2D.app/rhubarb"
   if [ -d $TOONZDIR/Tahoma2D.app/rhubarb ]
   then
      # In case of prior builds, replace rhubarb folder
      rm -rf $TOONZDIR/Tahoma2D.app/rhubarb
   fi
   mkdir $TOONZDIR/Tahoma2D.app/rhubarb
   cp -R thirdparty/apps/rhubarb/rhubarb thirdparty/apps/rhubarb/res $TOONZDIR/Tahoma2D.app/rhubarb
fi

if [ -d thirdparty/canon/Framework ]
then
   echo ">>> Copying canon framework to $TOONZDIR/Tahoma2D.app/Contents/Frameworks/EDSDK.Framework"
   if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks ]
   then
      mkdir $TOONZDIR/Tahoma2D.app/Contents/Frameworks
   fi
   cp -R thirdparty/canon/Framework/ $TOONZDIR/Tahoma2D.app/Contents/Frameworks
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

