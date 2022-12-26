#!/bin/bash
cd thirdparty

echo ">>> Cloning libgphoto2"
git clone https://github.com/tahoma2d/libgphoto2.git libgphoto2_src

cd libgphoto2_src

git checkout tahoma2d-version

echo ">>> Configuring libgphoto2"
autoreconf --install --symlink

./configure --prefix=/usr/local

echo ">>> Making libgphoto2"
make

echo ">>> Installing libgphoto2"
sudo make install

cd ..
