#!/bin/bash
cd thirdparty/libmypaint || exit;

echo ">>> Cloning libmypaint"
git clone https://github.com/tahoma2d/libmypaint src || exit;

cd src || exit;
echo "*" >| .gitignore

export CFLAGS='-Ofast -ftree-vectorize -fopt-info-vec-optimized -march=native -mtune=native -funsafe-math-optimizations -funsafe-loop-optimizations'

echo ">>> Generating libmypaint environment"
./autogen.sh || exit;

echo ">>> Configuring libmypaint build"
sudo ./configure || exit;

echo ">>> Building libmypaint"
# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1)) || exit;
sudo make -j "$parallel" || exit;

echo ">>> Installing libmypaint"
sudo make -j "$parallel" install || exit;

sudo ldconfig || exit;
