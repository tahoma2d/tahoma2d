#!/bin/bash

OLDFULLVERSION=$1
NEWFULLVERSION=$2
RELEASEDATE=$3

if [ "$OLDFULLVERSION" = "" -o "$NEWFULLVERSION" = "" -o "$RELEASEDATE" = "" ]
then
   echo "$0 <OLDFULLVERSION> <NEWFULLVERSION> <releasedate>"
   echo "    version format: #.#.#"
   echo "    date format   : yyyy-mm-dd"
   exit
fi

OLDMAJOR=0
OLDMINOR=0
OLDFIX=0
read OLDMAJOR OLDMINOR OLDFIX <<XXX
$(echo $OLDFULLVERSION | sed -e"s/\./ /g")
XXX
if [ "$OLDFIX" = "" ]
then
   OLDFIX="0"
   OLDFULLVERSION=$OLDFULLVERSION".0"
fi

NEWMAJOR=0
NEWMINOR=0
NEWFIX=0
read NEWMAJOR NEWMINOR NEWFIX <<XXX
$(echo $NEWFULLVERSION | sed -e"s/\./ /g")
XXX
if [ "$NEWFIX" = "" ]
then
   NEWFIX="0"
   NEWFULLVERSION=$NEWFULLVERSION".0"
fi

OLDSHORTVERSION=`echo $OLDMAJOR.$OLDMINOR`
NEWSHORTVERSION=`echo $NEWMAJOR.$NEWMINOR`

USEOLDVERSION=$OLDFULLVERSION
if [ "$OLDFIX" = "0" ]
then
   USEOLDVERSION=$OLDSHORTVERSION
fi
USENEWVERSION=$NEWFULLVERSION
if [ "$NEWFIX" = "0" ]
then
   USENEWVERSION=$NEWSHORTVERSION
fi

# .github/ISSUE_TEMPLATE/bug_report.yml
dos2unix .github/ISSUE_TEMPLATE/bug_report.yml
sed -e"s/        - $USEOLDVERSION/        - $USENEWVERSION/" -e "s/        - Nightly.*$/&\n        - $OLDFULLVERSION/" .github/ISSUE_TEMPLATE/bug_report.yml >| tmp.txt
unix2dos tmp.txt
mv tmp.txt .github/ISSUE_TEMPLATE/bug_report.yml

# appveyor.yml
dos2unix appveyor.yml
sed -e"s/$OLDFULLVERSION/$NEWFULLVERSION/" appveyor.yml >| tmp.txt
unix2dos tmp.txt
mv tmp.txt appveyor.yml

# ci-scripts/osx/tahoma-buildpkg.sh
dos2unix ci-scripts/osx/tahoma-buildpkg.sh
sed -e"s/$USEOLDVERSION/$USENEWVERSION/" ci-scripts/osx/tahoma-buildpkg.sh >| tmp.txt
unix2dos tmp.txt
mv tmp.txt ci-scripts/osx/tahoma-buildpkg.sh

# toonz/cmake/BundleInfo.plist.in
dos2unix toonz/cmake/BundleInfo.plist.in 
sed -e"s/$OLDFULLVERSION/$NEWFULLVERSION/" toonz/cmake/BundleInfo.plist.in >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/cmake/BundleInfo.plist.in

# toonz/installer/windows/setup.iss
dos2unix toonz/installer/windows/setup.iss
sed -e"s/$USEOLDVERSION/$USENEWVERSION/" toonz/installer/windows/setup.iss >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/installer/windows/setup.iss

# toonz/sources/include/tversion.h
dos2unix toonz/sources/include/tversion.h
sed -e"s/applicationVersion  = $OLDSHORTVERSION/applicationVersion  = $NEWSHORTVERSION/" -e"s/applicationRevision = $OLDFIX/applicationRevision = $NEWFIX/" toonz/sources/include/tversion.h >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/sources/include/tversion.h

# toonz/sources/toonz/CMakeLists.txt
dos2unix toonz/sources/toonz/CMakeLists.txt
sed -e"s/$OLDFULLVERSION/$NEWFULLVERSION/" toonz/sources/toonz/CMakeLists.txt >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/sources/toonz/CMakeLists.txt

# toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml
dos2unix toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml
sed -e "s/    <release version=\"$OLDFULLVERSION\"/    <release version=\"$NEWFULLVERSION\" date=\"$RELEASEDATE\"\/>\n&/" toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml
