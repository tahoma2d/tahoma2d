#!/bin/bash
pushd thirdparty/tiff-4.2.0
CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --disable-jbig --disable-webp && make
popd

cd toonz

if [ ! -d build ]
then
   mkdir build
fi
cd build

source /opt/qt515/bin/qt515-env.sh

cmake ../sources \
    -DWITH_SYSTEM_SUPERLU:BOOL=OFF

make -j7

