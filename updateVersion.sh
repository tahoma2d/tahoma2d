#!/bin/bash

OLDVERSION=$1
NEWVERSION=$2
RELEASEDATE=$3

# .github/ISSUE_TEMPLATE/bug_report.yml
dos2unix .github/ISSUE_TEMPLATE/bug_report.yml
sed -e"s/        - $OLDVERSION/        - $NEWVERSION/" -e "s/        - Nightly.*$/&\n        - $OLDVERSION/" .github/ISSUE_TEMPLATE/bug_report.yml >| tmp.txt
unix2dos tmp.txt
mv tmp.txt .github/ISSUE_TEMPLATE/bug_report.yml

# appveyor.yml
dos2unix appveyor.yml
sed -e"s/$OLDVERSION/$NEWVERSION/" appveyor.yml >| tmp.txt
unix2dos tmp.txt
mv tmp.txt appveyor.yml

# ci-scripts/osx/tahoma-buildpkg.sh
dos2unix ci-scripts/osx/tahoma-buildpkg.sh
sed -e"s/$OLDVERSION/$NEWVERSION/" ci-scripts/osx/tahoma-buildpkg.sh >| tmp.txt
unix2dos tmp.txt
mv tmp.txt ci-scripts/osx/tahoma-buildpkg.sh

# toonz/cmake/BundleInfo.plist.in
dos2unix toonz/cmake/BundleInfo.plist.in 
sed -e"s/$OLDVERSION/$NEWVERSION/" toonz/cmake/BundleInfo.plist.in >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/cmake/BundleInfo.plist.in

# toonz/installer/windows/setup.iss
dos2unix toonz/installer/windows/setup.iss
sed -e"s/$OLDVERSION/$NEWVERSION/" toonz/installer/windows/setup.iss >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/installer/windows/setup.iss

# toonz/sources/include/tversion.h
OLDMINOR=`echo $OLDVERSION | cut -c 3`
NEWMINOR=`echo $NEWVERSION | cut -c 3`
OLDFIX=`echo $OLDVERSION | cut -c 5`
if [ "$OLDFIX" = "" ]
then
   OLDFIX="0"
fi
NEWFIX=`echo $NEWVERSION | cut -c 5`
if [ "$NEWFIX" = "" ]
then
   NEWFIX="0"
fi
dos2unix toonz/sources/include/tversion.h
sed -e"s/applicationVersion  = 1.$OLDMINOR/applicationVersion  = 1.$NEWMINOR/" -e"s/applicationRevision = $OLDFIX/applicationRevision = $NEWFIX/" toonz/sources/include/tversion.h >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/sources/include/tversion.h

# toonz/sources/toonz/CMakeLists.txt
dos2unix toonz/sources/toonz/CMakeLists.txt
sed -e"s/$OLDVERSION/$NEWVERSION/" toonz/sources/toonz/CMakeLists.txt >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/sources/toonz/CMakeLists.txt

# toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml
dos2unix toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml
sed -e "s/    <release version=\"$OLDVERSION\"/    <release version=\"$NEWVERSION\" date=\"$RELEASEDATE\"\/>\n&/" toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml >| tmp.txt
unix2dos tmp.txt
mv tmp.txt toonz/sources/xdg-data/org.tahoma2d.Tahoma2D.metainfo.xml
