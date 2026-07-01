#!/bin/bash
export TAHOMA2DVERSION=1.6.2

if [ -d /usr/local/Cellar/qt@5 ]
then
   export QTDIR=/usr/local/opt/qt@5
elif [ -d /opt/homebrew/opt/qt@5 ]
then
   export QTDIR=/opt/homebrew/opt/qt@5
else
   export QTDIR=/usr/local/opt/qt
fi
export TOONZDIR=toonz/build/toonz

# If found, use Xcode Release build
if [ -d $TOONZDIR/Release ]
then
   export TOONZDIR=$TOONZDIR/Release
fi

###
### Cleanup prior runs just in case
###
if [ -d $TOONZDIR/DSYM ]
then
   rm -rf $TOONZDIR/DSYM
fi

if [ -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks ]
then
   rm -rf $TOONZDIR/Tahoma2D.app/Contents/Frameworks
fi

if [ -d $TOONZDIR/Tahoma2D.app/Contents/Plugins ]
then
   rm -rf $TOONZDIR/Tahoma2D.app/Contents/Plugins
fi

if [ -d $TOONZDIR/Tahoma2D.app/Contents/Resources/DWARF ]
then
   rm -rf $TOONZDIR/Tahoma2D.app/Contents/Resources/DWARF
fi

if [ -d $TOONZDIR/Tahoma2D.app/Contents/Resources/tahomastuff ]
then
   rm -rf $TOONZDIR/Tahoma2D.app/Contents/Resources/tahomastuff
fi

if [ -d $TOONZDIR/Tahoma2D.app/Contents/Resources/ffmpeg ]
then
   rm -rf $TOONZDIR/Tahoma2D.app/Contents/Resources/ffmpeg
fi

if [ -d $TOONZDIR/Tahoma2D.app/Contents/Resources/rhubarb ]
then
   rm -rf $TOONZDIR/Tahoma2D.app/Contents/Resources/rhubarb
fi

echo ">>> Creating DSYM files"
for X in `find $TOONZDIR/Tahoma2D.app/Contents/MacOS -type f`
do
   dsymutil -o $TOONZDIR/DSYM $X
   strip -S $X
done

echo ">>> Configuring Tahoma2D.app for deployment"

$QTDIR/bin/macdeployqt $TOONZDIR/Tahoma2D.app -verbose=0 -always-overwrite \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/lzocompress \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/lzodecompress \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tcleanup \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tcomposer \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tconverter \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tfarmcontroller \
   -executable=$TOONZDIR/Tahoma2D.app/Contents/MacOS/tfarmserver 

###
### Add additional frameworks
###
for FW in `echo "QtDBus QtPdf QtQml QtQmlModels QtQuick QtVirtualKeyboard"`
do
   if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks/$FW.framework ]
   then
      echo ">>> Copying missing $FW.framework to Tahoma2D.app/Contents/Frameworks"
      cp -r $QTDIR/Frameworks/$FW.framework $TOONZDIR/Tahoma2D.app/Contents/Frameworks
   fi
done

if [ -d thirdparty/canon/Framework ]
then
   if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Frameworks/EDSDK.framework ]
   then
      echo ">>> Copying canon EDSDK.Framework to Tahoma2D.app/Contents/Frameworks"
      cp -R thirdparty/canon/Framework/ $TOONZDIR/Tahoma2D.app/Contents/Frameworks
      chmod -R 755 $TOONZDIR/Tahoma2D.app/Contents/Frameworks/EDSDK.framework
   fi
fi

###
### Install additional resources
###
if [ -d thirdparty/apps/ffmpeg/bin ]
then
   echo ">>> Copying FFmpeg to Tahoma2D.app/Contents/Resources/ffmpeg"
   mkdir $TOONZDIR/Tahoma2D.app/Contents/Resources/ffmpeg
   cp -R thirdparty/apps/ffmpeg/bin/ffmpeg thirdparty/apps/ffmpeg/bin/ffprobe $TOONZDIR/Tahoma2D.app/Contents/Resources/ffmpeg
   chmod -R 755 $TOONZDIR/Tahoma2D.app/Contents/Resources/ffmpeg
fi

if [ -d thirdparty/apps/rhubarb ]
then
   echo ">>> Copying Rhubarb Lip Sync to Tahoma2D.app/Contents/Resources/rhubarb"
   mkdir $TOONZDIR/Tahoma2D.app/Contents/Resources/rhubarb
   cp -R thirdparty/apps/rhubarb/rhubarb thirdparty/apps/rhubarb/res $TOONZDIR/Tahoma2D.app/Contents/Resources/rhubarb
   chmod -R 755 $TOONZDIR/Tahoma2D.app/Contents/Resources/rhubarb
fi

if [ ! -d $TOONZDIR/Tahoma2D.app/Contents/Resources/libgphoto2 ]
then
   echo ">>> Copying libghoto2 supporting directories to Tahoma2D.app/Contents/Resources"
   cp -R /usr/local/lib/libgphoto2 $TOONZDIR/Tahoma2D.app/Contents/Resources
   cp -R /usr/local/lib/libgphoto2_port $TOONZDIR/Tahoma2D.app/Contents/Resources

   rm $TOONZDIR/Tahoma2D.app/Contents/Resources/libgphoto2/print-camera-list
   find $TOONZDIR/Tahoma2D.app/Contents/Resources/libgphoto2* -name *.la -exec rm -f {} \;
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
            echo ">>>Checking for $SRC"
            if [ ! -f $SRC ]
            then
               echo ">>>NOT Found...Searching /usr"
               local SRC=`find /usr -name $Y | head -1`
               if [ "$SRC" = "" ]
               then
                  echo ">>>NOT Found...Searching /opt"
                  local SRC=`find /opt -name $Y | head -1`
                  if [ "$SRC" = "" ]
                  then
                     echo "Error: Unable to find dependency $Y"
                     exit 1
                  fi
               fi
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
            if [ "$X" != "" ]
            then
               local Y=`echo $DEPFILE | sed -e"s/^.*\/\.\.\///" -e"s/@rpath.//"`
            fi
            if [ "$DEPFILE" != "@rpath/$Y" ]
            then
               echo "Fixing $DEPFILE in $LIBFILE"
               install_name_tool -change $DEPFILE @rpath/$Y $LIBFILE
            fi
         fi
         FIXCHECK=`otool -D $LIBFILE | grep -v ":" | grep -e"\/usr\/local"`
         if [ "$FIXCHECK" == "$DEPFILE" ]
         then
            echo "   Fixed ID!"
            install_name_tool -id @rpath/$Y $LIBFILE
         fi
      fi
   done
}

for FILE in `find $TOONZDIR/Tahoma2D.app/Contents -type f | grep -v -e"\.h" -e"\.prl" -e"\.plist" -e"\.conf" -e"\.icns" -e"EDSDK" -e"\/Headers\/" -e"sphinx"`
do
   checkLibFile $FILE
done

###
### Add DSYM files
###
echo ">>> Moving DSYM/Contents/Resources to Tahoma2D.app/Contents/Resources"
mv $TOONZDIR/DSYM/Contents/Resources/* $TOONZDIR/Tahoma2D.app/Contents/Resources

###
### Cleanup unnecessary files
###
find $TOONZDIR/Tahoma2D.app -name .DS_Store -exec rm {} \;

###
### Build Packages
###
echo ">>> Codesign Tahoma2D.app (installed)"
codesign --deep --force --sign - $TOONZDIR/Tahoma2D.app

echo ">>> Creating Tahoma2D-install-osx.pkg"

toonz/installer/osx/app.rb $TOONZDIR stuff toonz/installer/osx/scripts $TAHOMA2DVERSION

mv $TOONZDIR/Tahoma2D-install-osx.pkg $TOONZDIR/..

echo ">>> Creating Tahoma2D-portable-osx.dmg"

cp -R stuff $TOONZDIR/Tahoma2D.app/Contents/Resources/tahomastuff
chmod -R 777 $TOONZDIR/Tahoma2D.app/Contents/Resources/tahomastuff

find $TOONZDIR/Tahoma2D.app/Contents/Resources/tahomastuff -name .gitkeep -exec rm -f {} \;

echo ">>> Codesign Tahoma2D.app (portable)"
codesign --deep --force --sign - $TOONZDIR/Tahoma2D.app

cd $TOONZDIR

# Due to random ERROR: Bundle creation error: "hdiutil: create failed - Resource busy\n"
# We'll try to create the DMG a few times

let MAXTRY=10

for TRY in $(seq 1 $MAXTRY)
do
   if [ $TRY -gt  1 ]
   then
      echo ">>> DMG file creation failed.  Retrying $TRY/$MAXTRY..."
   fi

    $QTDIR/bin/macdeployqt Tahoma2D.app -dmg -verbose=0
    if [ -f Tahoma2D.dmg ]
    then
       echo ">>> DMG file created successfully"
       mv Tahoma2D.dmg ../Tahoma2D-portable-osx.dmg
       exit 0
    fi
done

echo ">>> DMG file creation failed after too many attempts. Aborting!"
exit 1

