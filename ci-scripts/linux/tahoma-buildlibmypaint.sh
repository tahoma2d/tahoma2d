#!/bin/bash
cd thirdparty/libmypaint

echo ">>> Cloning libmypaint"
git clone https://github.com/tahoma2d/libmypaint src

cd src
echo "*" >| .gitignore

export CFLAGS='-Ofast -ftree-vectorize -fopt-info-vec-optimized -march=native -mtune=native -funsafe-math-optimizations -funsafe-loop-optimizations'

echo ">>> Generating libmypaint environment"
./autogen.sh

echo ">>> Configuring libmypaint build"
sudo ./configure

# Leave one processor available for other processing if possible
parallel=$(($(nproc) < 2 ? 1 : $(nproc) - 1))
echo ">>> Building libmypaint"
sudo make -j "$parallel"

echo ">>> Installing libmypaint"
sudo make -j "$parallel" install

sudo ldconfig
