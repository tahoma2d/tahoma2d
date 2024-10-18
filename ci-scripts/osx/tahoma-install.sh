#!/bin/bash
brew update
# Remove symlink in order for latest python to install
rm -f '/usr/local/bin/python3'
rm -f '/usr/local/bin/python3.12'
rm -f '/usr/local/bin/2to3'
rm -f '/usr/local/bin/2to3-3.12'
rm -f '/usr/local/bin/idle3'
rm -f '/usr/local/bin/idle3.12'
rm -f '/usr/local/bin/pydoc3'
rm -f '/usr/local/bin/pydoc3.12'
rm -f '/usr/local/bin/python3-config'
rm -f '/usr/local/bin/python3.12-config'
# Remove synlink to nghttp2 in order for latest curl to install
#brew unlink nghttp2
brew install boost qt@5 clang-format glew lz4 lzo libmypaint jpeg-turbo nasm yasm fontconfig freetype gnutls lame libass libbluray libsoxr libvorbis libvpx opencore-amr openh264 openjpeg opus rav1e sdl2 snappy speex tesseract theora webp xvid xz gsed
#brew install dav1d
brew install meson ninja
brew install automake autoconf gettext pkg-config libtool libusb-compat gd libexif