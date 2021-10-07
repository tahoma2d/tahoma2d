#!/bin/bash
brew update
# Remove symlink to bin/2to3 in order for latest python to install
rm -f '/usr/local/bin/2to3'
# Remove synlink to nghttp2 in order fpr latest curl to install
brew unlink nghttp2
brew install boost qt@5 clang-format glew lz4 lzo libmypaint jpeg-turbo nasm yasm dav1d fontconfig freetype gnutls lame libass libbluray libsoxr libvorbis libvpx opencore-amr openh264 openjpeg opus rav1e sdl2 snappy speex tesseract theora webp xvid xz
