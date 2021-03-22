#!/bin/bash
cd thirdparty

echo ">>> Cloning opencv"
git clone https://github.com/tahoma2d/opencv

cd opencv
echo "*" >| .gitignore

if [ ! -d build ]
then
   mkdir build
fi
cd build

echo ">>> Cmaking opencv"
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_JASPER=OFF \
      -DBUILD_JPEG=OFF \
      -DBUILD_OPENEXR=OFF \
      -DBUILD_PERF_TESTS=OFF \
      -DBUILD_PNG=OFF \
      -DBUILD_PROTOBUF=OFF \
      -DBUILD_TESTS=OFF \
      -DBUILD_TIFF=OFF \
      -DBUILD_WEBP=OFF \
      -DBUILD_ZLIB=OFF \
      -DBUILD_opencv_hdf=OFF \
      -DBUILD_opencv_java=OFF \
      -DBUILD_opencv_text=ON \
      -DOPENCV_ENABLE_NONFREE=ON \
      -DOPENCV_GENERATE_PKGCONFIG=ON \
      -DPROTOBUF_UPDATE_FILES=ON \
      -DWITH_1394=OFF \
      -DWITH_CUDA=OFF \
      -DWITH_EIGEN=ON \
      -DWITH_FFMPEG=ON \
      -DWITH_GPHOTO2=OFF \
      -DWITH_GSTREAMER=OFF \
      -DWITH_JASPER=OFF \
      -DWITH_OPENEXR=ON \
      -DWITH_OPENGL=OFF \
      -DWITH_QT=OFF \
      -DWITH_TBB=ON \
      -DWITH_VTK=ON \
      -DBUILD_opencv_python2=OFF \
      -DBUILD_opencv_python3=ON \
      -DCMAKE_MACOS_RPATH=FALSE \
      -DCMAKE_INSTALL_NAME_DIR=/usr/local/lib \
      ..

echo ">>> Building opencv"
make -j7 # runs 7 jobs in parallel

echo ">>> Installing opencv"
sudo make install
