#!/bin/bash

# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1)) || exit;

pushd thirdparty/tiff-4.2.0 || exit;
CFLAGS="-fPIC" CXXFLAGS="-fPIC" ./configure --disable-jbig --disable-webp && make -j "$parallel" || exit;
popd || exit;

cd toonz || exit;

if [ ! -d build ]
then
   mkdir build
fi
cd build || exit;

source /opt/qt515/bin/qt515-env.sh || exit;

if [ -d ../../thirdparty/canon/Header ]
then
   export CANON_FLAG=-DWITH_CANON=ON
fi

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
cmake ../sources  $CANON_FLAG \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DWITH_GPHOTO2:BOOL=ON \
    -DWITH_SYSTEM_SUPERLU=ON || exit;

make -j "$parallel" || exit;
