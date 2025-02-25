#!/bin/bash
cd thirdparty || exit;

echo ">>> Cloning libgphoto2"
git clone https://github.com/tahoma2d/libgphoto2.git libgphoto2_src || exit;

cd libgphoto2_src || exit;

git checkout tahoma2d-version || exit;

echo ">>> Configuring libgphoto2"
autoreconf --install --symlink || exit;

./configure --prefix=/usr/local || exit;

# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1)) || exit;
echo ">>> Making libgphoto2"
make -j "$parallel" || exit;

echo ">>> Installing libgphoto2"
sudo make -j "$parallel" install || exit;

