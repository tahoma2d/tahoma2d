cd thirdparty

echo ">>> Cloning openH264"
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
cp -R ffmpeg ffmpeg_shared

cd ffmpeg_shared
echo "*" >| .gitignore

echo ">>> Configuring to build ffmpeg (shared)"
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

echo ">>> Building ffmpeg (shared)"
make

echo ">>> Installing ffmpeg (shared)"
sudo make install

sudo ldconfig

echo ">>> Configuring to build ffmpeg (static)"
cd ../ffmpeg
echo "*" >| .gitignore

export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig

./configure  --prefix=/usr/local \
      --pkg-config-flags="--static" \
      --extra-cflags="-I/usr/local/include -static" \
      --extra-ldflags="-L/usr/local/lib -static" \
      --extra-ldexeflags="-static" \
      --enable-static \
      --enable-cross-compile \
      --enable-pic \
      --disable-shared \
      --enable-libopenh264 \
      --enable-pthreads \
      --enable-version3 \
      --enable-avresample \
      --enable-libmp3lame \
      --enable-libopus \
      --enable-libsnappy \
      --enable-libtheora \
      --enable-libvorbis \
      --enable-libvpx \
      --enable-libwebp \
      --enable-lzma \
      --enable-libfreetype \
      --enable-libopencore-amrnb \
      --enable-libopencore-amrwb \
      --enable-libspeex \
      --disable-ffplay \
      --disable-htmlpages \
      --disable-manpages \
      --disable-podpages \
      --disable-txtpages \
      --disable-libjack \
      --disable-indev=jack

echo ">>> Building ffmpeg (static)"
make

echo ">>> Installing to tahoma2d/thirdparty/ffmpeg/bin"
if [ ! -d bin ]
then
   mkdir bin
fi

cp ffmpeg ffprobe bin/

