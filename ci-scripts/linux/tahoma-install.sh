#!/bin/bash

sudo add-apt-repository --yes ppa:beineri/opt-qt-5.15.2-focal || exit;
sudo apt-get update || exit;
sudo apt-get install -y cmake liblzo2-dev liblz4-dev libfreetype6-dev libpng-dev libegl1-mesa-dev libgles2-mesa-dev libglew-dev freeglut3-dev qt515script libsuperlu-dev qt515svg qt515tools qt515multimedia wget libboost-all-dev liblzma-dev libjson-c-dev libjpeg-turbo8-dev libturbojpeg0-dev libglib2.0-dev qt515serialport || exit;
# Removed: libopenjpeg-dev 
sudo apt-get install -y nasm yasm libgnutls28-dev libunistring-dev libass-dev libbluray-dev libmp3lame-dev libopus-dev libsnappy-dev libtheora-dev libvorbis-dev libvpx-dev libwebp-dev libxml2-dev libfontconfig1-dev libfreetype6-dev libopencore-amrnb-dev libopencore-amrwb-dev libspeex-dev libsoxr-dev libopenjp2-7-dev || exit;
sudo apt-get install -y python3-pip || exit;
sudo apt-get install -y build-essential libgirepository1.0-dev autotools-dev intltool gettext libtool patchelf autopoint libusb-1.0-0 libusb-1.0-0-dev || exit;
sudo apt-get install -y libdeflate-dev || exit;

pip3 install --upgrade pip || exit;
pip3 install numpy || exit;

# Leave repo directory for this step
cd .. || exit;

# someone forgot to include liblz4.pc with the package, use the version from xenial, as it only depends on libc
#if [ ! -f liblz4.deb ]
#then
#   wget http://mirrors.kernel.org/ubuntu/pool/main/l/lz4/liblz4-1_0.0~r131-2ubuntu2_amd64.deb -O liblz4.deb
#fi
#
#if [ ! -f liblz4-dev.db ]
#then
#   wget http://mirrors.kernel.org/ubuntu/pool/main/l/lz4/liblz4-dev_0.0~r131-2ubuntu2_amd64.deb -O liblz4-dev.deb
#fi
#
#sudo dpkg -i liblz4.deb liblz4-dev.deb

# Remove this as the version that is there is old and causes issues compiling opencv
sudo apt-get remove libprotobuf-dev || exit;

# Need protoc3 for some compiles.  don't use the apt-get version as it's too old.

if [ ! -d protoc3 ]
then
   wget https://github.com/google/protobuf/releases/download/v3.6.1/protoc-3.6.1-linux-x86_64.zip || exit;
   # Unzip
   unzip protoc-3.6.1-linux-x86_64.zip -d protoc3 || exit;
fi

# Move protoc to /usr/local/bin/
sudo cp -pr protoc3/bin/* /usr/local/bin/ || exit;

# Move protoc3/include to /usr/local/include/
sudo cp -pr protoc3/include/* /usr/local/include/ || exit;

sudo ldconfig || exit;
