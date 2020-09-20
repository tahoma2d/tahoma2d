cd thirdparty

echo ">>> Cloning openH254"
git clone https://github.com/cisco/openh264.git openh264

cd openh264
echo "*" >| .gitignore

echo ">>> Making openh264"
make

echo ">>> Installing openh264"
sudo make install

cd ..

echo ">>> Cloning ffmpeg"
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg

cd ffmpeg
echo "*" >| .gitignore

echo ">>> Configuring to build ffmpeg"
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

./configure  --prefix=/usr/local \
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

echo ">>> Building ffmpeg"
make

echo ">>> Installing ffmpeg"
sudo make install

sudo ldconfig

