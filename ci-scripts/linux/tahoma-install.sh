#!/bin/bash
sudo apt-get update
sudo apt-get install -y cmake liblzo2-dev liblz4-dev libpng-dev libegl1-mesa-dev libgles2-mesa-dev libglew-dev freeglut3-dev libsuperlu-dev wget libboost-all-dev liblzma-dev libjson-c-dev libjpeg8-dev libjpeg-turbo8-dev libturbojpeg0-dev libglib2.0-dev

sudo apt-get install -y qtscript5-dev libqt5svg5-dev qtmultimedia5-dev libqt5serialport5-dev qttools5-dev libqt5multimedia5-plugins

# Removed: libopenjpeg-dev 
sudo apt-get install -y nasm yasm libgnutls28-dev libunistring-dev libass-dev libbluray-dev libmp3lame-dev libopus-dev libsnappy-dev libtheora-dev libvorbis-dev libvpx-dev libwebp-dev libxml2-dev libfontconfig1-dev libopencore-amrnb-dev libopencore-amrwb-dev libspeex-dev libsoxr-dev libopenjp2-7-dev
sudo apt-get install -y python2 python3-pip
sudo apt-get install -y build-essential libgirepository1.0-dev autotools-dev intltool gettext libtool patchelf autopoint libusb-1.0-0 libusb-1.0-0-dev
sudo apt-get install -y libdeflate-dev
sudo apt-get install -y libfuse2
sudo apt-get install -y protobuf-compiler libprotobuf-dev

pip3 install --upgrade pip
pip3 install numpy

# Leave repo directory for this step
cd ..

##### This is temporary because linuxdeploy won't work on Ubuntu 22.04 until May 1st 2025

sudo apt-get -y install git g++ libgl1-mesa-dev
git clone https://github.com/tahoma2d/linuxdeployqt.git
# Then build in Qt Creator, or use
export PATH=$(readlink -f /tmp/.mount_QtCreator-*-x86_64/*/gcc_64/bin/):$PATH
cd linuxdeployqt
git checkout tahoma2d_version
qmake
make
sudo make install

#wget https://nixos.org/releases/patchelf/patchelf-0.9/patchelf-0.9.tar.bz2
#tar xf patchelf-0.9.tar.bz2
#( cd patchelf-0.9/ && ./configure  && make && sudo make install )

sudo wget -c "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage" -O /usr/local/bin/appimagetool
sudo chmod a+x /usr/local/bin/appimagetool
