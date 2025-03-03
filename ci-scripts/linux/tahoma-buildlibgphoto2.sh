#!/bin/bash
cd thirdparty

echo ">>> Cloning libgphoto2"
git clone https://github.com/tahoma2d/libgphoto2.git libgphoto2_src

cd libgphoto2_src

git checkout tahoma2d-version

echo ">>> Configuring libgphoto2"
autoreconf --install --symlink

./configure --prefix=/usr/local

# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1))
echo ">>> Making libgphoto2"
make -j "$parallel"

echo ">>> Installing libgphoto2"
sudo make -j "$parallel" install

cd ..
