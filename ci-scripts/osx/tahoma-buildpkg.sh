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

echo ">>> Copying stuff to Tahoma2D.app/tahomastuff"
if [ -d $TOONZDIR/Tahoma2D.app/tahomastuff ]
then
   # In case of prior builds, replace stuff folder
   rm -rf $TOONZDIR/Tahoma2D.app/tahomastuff
fi
cp -R stuff $TOONZDIR/Tahoma2D.app/tahomastuff
chmod -R 777 $TOONZDIR/Tahoma2D.app/tahomastuff

find $TOONZDIR/Tahoma2D.app/tahomastuff -name .gitkeep -exec rm -f {} \;

if [ -d thirdparty/apps/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to Tahoma2D.app/ffmpeg"
   if [ -d $TOONZDIR/Tahoma2D.app/ffmpeg ]
   then
      # In case of prior builds, replace ffmpeg folder
      rm -rf $TOONZDIR/Tahoma2D.app/ffmpeg
   fi
   mkdir $TOONZDIR/Tahoma2D.app/ffmpeg
   cp -R thirdparty/apps/ffmpeg/bin/ffmpeg thirdparty/apps/ffmpeg/bin/ffprobe $TOONZDIR/Tahoma2D.app/ffmpeg
   chmod -R 755 $TOONZDIR/Tahoma2D.app/ffmpeg
fi

if [ -d thirdparty/apps/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to Tahoma2D.app/rhubarb"
   if [ -d $TOONZDIR/Tahoma2D.app/rhubarb ]
   then
      # In case of prior builds, replace rhubarb folder
      rm -rf $TOONZDIR/Tahoma2D.app/rhubarb
   fi
   mkdir $TOONZDIR/Tahoma2D.app/rhubarb
   cp -R thirdparty/apps/rhubarb/rhubarb thirdparty/apps/rhubarb/res $TOONZDIR/Tahoma2D.app/rhubarb
   chmod -R 755 $TOONZDIR/Tahoma2D.app/rhubarb
fi

if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks ]
then
   mkdir $TOONZDIR/Tahoma2D.app/Contents/Frameworks
fi

if [ -d thirdparty/canon/Framework ]
then
   if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks/EDSDK.framework ]
   then
      echo ">>> Copying canon framework to Tahoma2D.app/Contents/Frameworks/EDSDK.Framework"
      cp -R thirdparty/canon/Framework/ $TOONZDIR/Tahoma2D.app/Contents/Frameworks
      chmod -R 755 $TOONZDIR/Tahoma2D.app/Contents/Frameworks/EDSDK.framework
   fi
fi

if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks/libgphoto2 ]
then
   echo ">>> Copying libghoto2 supporting directories to Tahoma2D.app/Contents/Frameworks"
   cp -R /usr/local/lib/libgphoto2 $TOONZDIR/Tahoma2D.app/Contents/Frameworks
   cp -R /usr/local/lib/libgphoto2_port $TOONZDIR/Tahoma2D.app/Contents/Frameworks

   rm $TOONZDIR/Tahoma2D.app/Contents/Frameworks/libgphoto2/print-camera-list
   find $TOONZDIR/Tahoma2D.app/Contents/Frameworks/libgphoto2* -name *.la -exec rm -f {} \;
fi

echo ">>> Creating DSYM files"
if [ -d $TOONZDIR/DSYM ]
then
   rm -rf $TOONZDIR/DSYM
fi

for X in `find $TOONZDIR/Tahoma2D.app/Contents/MacOS -type f`
do
   dsymutil -o $TOONZDIR/DSYM $X
   strip -S $X
done

if [ -d $TOONZDIR/Tahoma2D.app/DSYM ]
then
   rm -rf $TOONZDIR/Tahoma2D.app/DSYM
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

for FW in `echo "QtDBus QtPdf QtQml QtQmlModels QtQuick QtVirtualKeyboard"`
do
   if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks/$FW.framework ]
   then
      echo ">>> Copying missing $FW.framework to Tahoma2D.app/Contents/Frameworks"
      cp -r $QTDIR/Frameworks/$FW.framework $TOONZDIR/Tahoma2D.app/Contents/Frameworks
   fi
done

if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/lib ]
then
   echo ">>> Adding Contents/lib symbolic link to Tahoma2D.app/Contents/Frameworks"
   ln -s Frameworks $TOONZDIR/Tahoma2D.app/Contents/lib
fi

echo ">>> Correcting library paths"
function checkLibFile() {
   local LIBFILE=$1   
   for DEPFILE in `otool -L $LIBFILE | sed -e "s/ (.*$//" | grep -e"\/usr\/local" -e"@rpath" -e"\.\./\.\./\.\." | grep -v "/qt"`
   do
      local Z=`echo $DEPFILE | cut -c 1-1`
      if [ "$Z" = "/" -o "$Z" = "@" ]
      then
         local Y=`basename $DEPFILE`
         local W=`basename $LIBFILE`
         local X=`echo $DEPFILE | grep "\.framework\/"`
         if [ "$X" = "" -a ! -f $TOONZDIR/Tahoma2D.app/Contents/Frameworks/$Y ]
         then
            local SRC=$DEPFILE
            local Z=`echo $DEPFILE | cut -c 1-16`
            local Z2=`echo $DEPFILE | cut -c 1-6`
            if [ "$Z" = "@loader_path/../" ]
            then
               local V=`echo $DEPFILE | sed -e"s/^.*\/\.\.\///"`
               local SRC=/usr/local/$V
            elif [ "$Z2" = "@rpath" ]
            then
                local SRC=/usr/local/lib/$Y
            fi
            echo "Copying $SRC to Frameworks"
            cp $SRC $TOONZDIR/Tahoma2D.app/Contents/Frameworks
            chmod 644 $TOONZDIR/Tahoma2D.app/Contents/Frameworks/$Y
            local ORIGDEPFILE=$DEPFILE
            checkLibFile $TOONZDIR/Tahoma2D.app/Contents/Frameworks/$Y
            DEPFILE=$ORIGDEPFILE
         fi
         if [ "$Y" != "$W" ]
         then
            echo "Fixing $DEPFILE in $LIBFILE"
            if [ "$X" != "" ]
            then
               local Y=`echo $DEPFILE | sed -e"s/^.*\/\.\.\///"`
               install_name_tool -change $DEPFILE @executable_path/../Frameworks/$Y $LIBFILE
            else
               install_name_tool -change $DEPFILE @executable_path/../Frameworks/$Y $LIBFILE
            fi
         fi
         FIXCHECK=`otool -D $LIBFILE | grep -v ":" | grep -e"\/usr\/local"`
         if [ "$FIXCHECK" == "$DEPFILE" ]
         then
            echo "   Fixed ID!"
            install_name_tool -id @executable_path/../Frameworks/$Y $LIBFILE
         fi
      fi
   done
}

for FILE in `find $TOONZDIR/Tahoma2D.app/Contents -type f | grep -v -e"\.h" -e"\.prl" -e"\.plist" -e"\.conf" -e"\.icns" -e"EDSDK" -e"\/Headers\/"`
do
   checkLibFile $FILE
done

echo ">>> Moving DYSM to Tahoma2D.app"
mv $TOONZDIR/DSYM $TOONZDIR/Tahoma2D.app

echo ">>> Creating Tahoma2D-osx.dmg"

$QTDIR/bin/macdeployqt $TOONZDIR/Tahoma2D.app -dmg -verbose=0

mv $TOONZDIR/Tahoma2D.dmg $TOONZDIR/../Tahoma2D-osx.dmg

