#!/bin/bash
pushd thirdparty/tiff-4.2.0
./configure --disable-lzma --without-x && make
popd

cd toonz

if [ ! -d build ]
then
   mkdir build
fi
cd build

if [ -d /usr/local/Cellar/qt@5 ]
then
   QTVERSION=`ls /usr/local/Cellar/qt@5`
   USEQTLIB="/usr/local/opt/qt@5/lib/"
else
   QTVERSION=`ls /usr/local/Cellar/qt`
   USEQTLIB="/usr/local/opt/qt/lib/"
fi

echo "QT Version detected: $QTVERSION"

if [ -d ../../thirdparty/canon/Header ]
then
   export CANON_FLAG=-DWITH_CANON=ON
fi

export MACOSX_DEPLOYMENT_TARGET=10.13
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/opt/jpeg-turbo/lib/pkgconfig"
cmake ../sources  $CANON_FLAG \
      -DQT_PATH=$USEQTLIB \
      -DTIFF_INCLUDE_DIR=../../thirdparty/tiff-4.2.0/libtiff/ \
      -DSUPERLU_INCLUDE_DIR=../../thirdparty/superlu/SuperLU_4.1/include/

make -j7 # runs 7 jobs in parallel
