cd thirdparty

echo ">>> Cloning opencv"
git clone https://github.com/opencv/opencv.git

cd opencv
echo "*" >| .gitignore

mkdir build
cd build

echo ">>> Cmaking openv"
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
      -DWITH_GSTREAMER=ON \
      -DWITH_JASPER=OFF \
      -DWITH_OPENEXR=ON \
      -DWITH_OPENGL=OFF \
      -DWITH_QT=OFF \
      -DWITH_TBB=ON \
      -DWITH_VTK=ON \
      -DBUILD_opencv_python2=OFF \
      -DBUILD_opencv_python3=ON \
      -DCMAKE_INSTALL_NAME_DIR=/usr/local/lib \
      ..

echo ">>> Building opencv"
make

echo ">>> Installing opencv"
sudo make install
