cd thirdparty

echo ">>> Cloning ffmpeg"
git clone https://git.ffmpeg.org/ffmpeg.git ffmpeg

cd ffmpeg
echo "*" >| .gitignore

echo ">>> Configuring to build ffmpeg"
export PKG_CONFIG_PATH=/usr/local/lib/pkconfig

./configure  --prefix=/usr/local \
      --enable-shared \
      --enable-pthreads \
      --enable-version3 \
      --enable-avresample \
      --enable-gnutls \
      --enable-libaom \
      --enable-libbluray \
      --enable-libdav1d \
      --enable-libmp3lame \
      --enable-libopus \
      --enable-librav1e \
      --enable-libsnappy \
      --enable-libtesseract \
      --enable-libtheora \
      --enable-libvorbis \
      --enable-libvpx \
      --enable-libwebp \
      --enable-libxml2 \
      --enable-lzma \
      --enable-libfontconfig \
      --enable-libfreetype \
      --enable-libass \
      --enable-libopencore-amrnb \
      --enable-libopencore-amrwb \
      --enable-libopenjpeg \
      --enable-libspeex \
      --enable-libsoxr \
      --enable-videotoolbox \
      --enable-libopenh264 \
      --disable-libjack \
      --disable-indev=jack

echo ">>> Building ffmpeg"
make -j7 # runs 7 jobs in parallel

echo ">>> Installing ffmpeg"
make install
