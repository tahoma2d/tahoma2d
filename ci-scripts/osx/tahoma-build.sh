#!/bin/bash
pushd thirdparty/tiff-4.0.3
./configure --disable-lzma --without-x && make
popd

cd toonz

mkdir build
cd build

QTVERSION=`ls /usr/local/Cellar/qt`
echo "QT Version detected: $QTVERSION"

if [ -d ../../thirdparty/canon/Header ]
then
   export CANON_FLAG=-DWITH_CANON=ON
fi

export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/opt/jpeg-turbo/lib/pkgconfig"
cmake ../sources  $CANON_FLAG \
      -DQT_PATH=/usr/local/opt/qt/lib/ \
      -DTIFF_INCLUDE_DIR=../../thirdparty/tiff-4.0.3/libtiff/ \
      -DSUPERLU_INCLUDE_DIR=../../thirdparty/superlu/SuperLU_4.1/include/

make -j7 # runs 7 jobs in parallel
