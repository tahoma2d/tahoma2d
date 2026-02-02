#!/bin/bash
#
# Usage: update.sh LANGUAGE

BASE_DIR=$(cd `dirname "$0"`; pwd)
TOONZLANG=$1

[ -d "${BASE_DIR}/${TOONZLANG}" ] || mkdir -p "${BASE_DIR}/${TOONZLANG}"
cd "${BASE_DIR}/${TOONZLANG}"

lupdate ../../colorfx/ -ts colorfx.ts
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lupdate ../../common/ -ts tnzcore.ts
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lupdate ../../tnztools/ -ts tnztools.ts
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lupdate ../../toonz/ ../../stopmotion/ ../../stdfx/ -ts toonz.ts
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lupdate ../../toonzlib/ -ts toonzlib.ts
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lupdate ../../toonzqt/ -ts toonzqt.ts
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

lupdate ../../image/ -ts image.ts
if [ "$?" != "0" ]
then
   echo "ERROR ENCOUNTERED!!!!"
   exit 1
fi

if [ "$OSTYPE" = "msys" ]
then
   unix2dos *.ts
fi