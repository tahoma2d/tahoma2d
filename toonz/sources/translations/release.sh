#!/bin/bash
#
# Usage: release.sh LANGUAGE DESTINATION

BASE_DIR=$(cd `dirname "$0"`; pwd)
TOONZLANG=$1
DESTINATION=$2


[ -d "${BASE_DIR}/${TOONZLANG}" ] || mkdir -p ../../../stuff/config/loc/${DESTINATION}
cd "${BASE_DIR}/${TOONZLANG}"
lrelease colorfx.ts -qm colorfx.qm
lrelease tnzcore.ts -qm tnzcore.qm
lrelease toonz.ts -qm toonz.qm
lrelease toonzqt.ts -qm toonzqt.qm
lrelease image.ts -qm image.qm
lrelease tnztools.ts -qm tnztools.qm
lrelease toonzlib.ts -qm toonzlib.qm

mv *.qm ../../../../stuff/config/loc/${DESTINATION}