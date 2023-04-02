#!/bin/bash
#
# Usage: updateAll.sh

BASE_DIR=$(cd `dirname "$0"`; pwd)

for TOONZLANG in `ls`
do

if [ -d "${BASE_DIR}/${TOONZLANG}" ]
then
   echo "./update.sh $TOONZLANG"
   ./update.sh $TOONZLANG
fi

done
