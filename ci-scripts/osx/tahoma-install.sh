#!/bin/bash
brew update
# from Homebrew 1.6.0 the old formula for obtaining Qt5.9.2 becomes invalid.
# so we start to use the latest version of Qt. (#1910)
brew install qt clang-format glew lz4 lzo libmypaint jpeg-turbo nasm yasm aom dav1d fontconfig freetype gnutls lame libass libbluray libsoxr libvorbis libvpx opencore-amr openh264 openjpeg opus rav1e sdl2 snappy speex tesseract theora webp xvid xz
# delete older qt versions and make sure to have only the latest
brew cleanup qt
# temp workaround to brew installed glew cmake info overriding glew lib detection
# which causes compiling issues later
if [ -L /usr/local/lib/cmake/glew ]
then
   echo "Symbolic link '/usr/local/lib/cmake/glew' detected. Removing to avoid glew lib detection issue!"
   ls -l /usr/local/lib/cmake/glew
   rm /usr/local/lib/cmake/glew
fi
