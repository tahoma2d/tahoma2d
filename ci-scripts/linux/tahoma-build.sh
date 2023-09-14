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

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
cmake ../sources \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DWITH_GPHOTO2:BOOL=ON \
    -DWITH_SYSTEM_SUPERLU=ON

make -j7
