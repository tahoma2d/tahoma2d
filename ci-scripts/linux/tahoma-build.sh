#!/bin/bash

cd toonz

if [ ! -d build ]
then
   mkdir build
fi
cd build

#source /opt/qt515/bin/qt515-env.sh

if [ -d ../../thirdparty/canon/Header ]
then
   export CANON_FLAG=-DWITH_CANON=ON
fi

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
CC="gcc" cmake ../sources  $CANON_FLAG \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DWITH_GPHOTO2:BOOL=ON \
    -DWITH_SYSTEM_SUPERLU=ON

make
