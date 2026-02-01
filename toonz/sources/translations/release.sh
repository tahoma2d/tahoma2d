#!/bin/bash
#
# Usage: release.sh LANGUAGE DESTINATION

BASE_DIR=$(cd `dirname "$0"`; pwd)
TOONZLANG=$1
DESTINATION=$2


[ -d "${BASE_DIR}/${TOONZLANG}" ] || mkdir -p ../../../stuff/config/loc/${DESTINATION}
cd "${BASE_DIR}/${TOONZLANG}"

lrelease colorfx.ts -qm colorfx.qm
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lrelease tnzcore.ts -qm tnzcore.qm
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lrelease toonz.ts -qm toonz.qm
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lrelease toonzqt.ts -qm toonzqt.qm
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lrelease image.ts -qm image.qm
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lrelease tnztools.ts -qm tnztools.qm
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lrelease toonzlib.ts -qm toonzlib.qm
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

mv *.qm ../../../../stuff/config/loc/${DESTINATION}