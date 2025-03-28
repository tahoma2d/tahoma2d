#!/bin/bash
cd thirdparty

echo ">>> Cloning openH264"
git clone https://github.com/cisco/openh264.git openh264

cd openh264
echo "*" >| .gitignore

# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1))
echo ">>> Making openh264"
make -j "$parallel"

echo ">>> Installing openh264"
sudo make -j "$parallel" install

cd ..

echo ">>> Cloning ffmpeg"
git clone -b v4.3.1 https://github.com/tahoma2d/FFmpeg ffmpeg

cd ffmpeg
echo "*" >| .gitignore

echo ">>> Configuring to build ffmpeg (shared)"
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

./configure  --prefix=/usr/local \
      --cc="$CC" \
      --cxx="$CXX" \
      --toolchain=hardened \
      --pkg-config-flags="--static" \
      --extra-cflags="-I/usr/local/include" \
      --extra-ldflags="-L/usr/local/lib" \
      --enable-pthreads \
      --enable-version3 \
      --enable-avresample \
      --enable-gnutls \
      --enable-libbluray \
      --enable-libmp3lame \
      --enable-libopus \
      --enable-libsnappy \
      --enable-libtheora \
      --enable-libvorbis \
      --enable-libvpx \
      --enable-libwebp \
      --enable-libxml2 \
      --enable-lzma \
      --enable-libfreetype \
      --enable-libass \
      --enable-libopencore-amrnb \
      --enable-libopencore-amrwb \
      --enable-libopenjpeg \
      --enable-libspeex \
      --enable-libsoxr \
      --enable-libopenh264 \
      --enable-shared \
      --disable-static \
      --disable-libjack \
      --disable-indev=jack

echo ">>> Building ffmpeg (shared)"
make -j "$parallel"

echo ">>> Installing ffmpeg (shared)"
sudo make -j "$parallel" install

sudo ldconfig
