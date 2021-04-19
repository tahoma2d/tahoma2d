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

echo ">>> Building libmypaint"
sudo make

echo ">>> Installing libmypaint"
sudo make install

sudo ldconfig
