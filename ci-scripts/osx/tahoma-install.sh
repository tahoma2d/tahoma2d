#!/bin/bash
brew update
# Remove symlink to bin/2to3 in order for latest python to install
rm -f '/usr/local/bin/2to3'
brew install boost qt clang-format glew lz4 lzo libmypaint jpeg-turbo nasm yasm aom dav1d fontconfig freetype gnutls lame libass libbluray libsoxr libvorbis libvpx opencore-amr openh264 openjpeg opus rav1e sdl2 snappy speex tesseract theora webp xvid xz
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
